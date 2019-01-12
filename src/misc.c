
//

#include "common.h"
#include "misc.h"
#include "mpx_serial.h"
#include <avr/interrupt.h>

extern volatile u8 g_sync_count;
//extern volatile u16 g_entropy;
extern volatile enum PROTO_MODE proto_mode;
extern volatile u32 bind_press_time;

u16 (*timer_callback)(void);

static volatile u32 timer_counts;
static volatile u16 full_loops;
static volatile u32 msecs = 0;



void CLOCK_StartTimer(u16 us, u16 (*cb)(void))
{
    if(! cb) return;
	if(! us) return;
    timer_callback = cb; // timer_callback = pointer to function.
	timer_counts = MICRO_SEC_CONVERT(us);
	full_loops = timer_counts >> 16;

	u8 temp_reg = SREG;
	cli();
	OCR1A = TCNT1 + (timer_counts & 0xFFFF);
	if(temp_reg & 0x80) sei(); // Global (I)nterrupt Enable bit.

	TIFR1  |= 1<<OCF1A; // Reset Flag.
	// Enable Output-Compare interrrupts
	TIMSK1 |= 1<<OCIE1A; 
}



ISR(TIMER1_COMPA_vect) // Protocol callback ISR.
{
static u16 mpx_timer = 0;

	if(full_loops) full_loops --;
	else
	{
	u16 us = timer_callback(); // e.g. flysky_cb()
		if(! us)  { CLOCK_StopTimer(); return; }
	timer_counts = MICRO_SEC_CONVERT(us);
	full_loops = timer_counts >> 16;
	OCR1A += (timer_counts & 0xFFFF);

#if UNIMOD == 1
	// Check to see if > 14ms has elapsed since last mpx communication.
	// If we have > 5ms until next callback then send mpx packet and process returned packet.
		if( mpx_timer > 14000 && (OCR1A - TCNT1) > MICRO_SEC_CONVERT(5000) )
		{
		//gpio_set(PORTB,DEBUG_2);
			mpx_telem_send();
			mpx_timer = 0;
		}
#endif
	mpx_timer += us;
	}
}



ISR(TIMER1_CAPT_vect)
{
// One timer count ~723 nS @ 11.0592MHz / 8.
u16 icr1_diff;
u16 icr1_current;

static u16 icr1_previous = 0;
static u8 servo_count = 0;
static u8 need_to_sync = 1;

icr1_current = ICR1;

//icr1_diff = icr1_current - icr1_previous;
if(icr1_current >= icr1_previous) icr1_diff = icr1_current - icr1_previous;
else icr1_diff = (0xffff - icr1_previous) + icr1_current + 1 ;

icr1_previous = icr1_current;

	if (icr1_diff > MICRO_SEC_CONVERT(2300)) // > 2.3ms pulse seen as frame sync.
	{
	need_to_sync =0;
	g_sync_count ++;
	servo_count =0;
   	}
	else if (icr1_diff < MICRO_SEC_CONVERT(700)) // < 0.7ms pulse seen as glitch.
	{
	// Do nothing with glitch.
	}
	else if (! need_to_sync) // Pulse within limits and we don't need to sync.
	{
		if (servo_count < NUM_OUT_CHANNELS)
		{
		if (icr1_diff > MICRO_SEC_CONVERT(1500 + DELTA_PPM_IN)) icr1_diff = MICRO_SEC_CONVERT(1500 + DELTA_PPM_IN);
   		else if (icr1_diff < MICRO_SEC_CONVERT(1500 - DELTA_PPM_IN)) icr1_diff = MICRO_SEC_CONVERT(1500 - DELTA_PPM_IN);
		
		// Subtract 1.5 ms centre offset.
		// Multiply by 2 to get max-min counter value difference to be +-1520
		// (same scaling as M-Link Packet for MPX ppm (+-550us) range !).

   		Channels[servo_count] = (icr1_diff - MICRO_SEC_CONVERT(1500)) * 2;
   		servo_count++;
		}	
		else need_to_sync = 1; // More servo pulses than we can handle ... need to sync.
	}
// if (g_entropy==0) g_entropy = TCNT1;
}


void CLOCK_StopTimer()
{
	// Disable Output-Compare interrrupts
	TIMSK1 &= ~(1<<OCIE1A);
    timer_callback = NULL;
}


ISR(TIMER2_COMPA_vect)
{
msecs ++;
OCR2A += COUNTS_PER_MILLI_SEC;

// Read bind button with de-bounce.
static u8 prev_state =0;
u8 curr_state;
static u32 press_t =0;
u32 release_t;

curr_state = read_bind_sw();

	if( curr_state && ! prev_state) press_t = msecs;
	else if( ! curr_state && prev_state )
	{
	release_t = msecs;
		if((release_t - press_t) > 50) // De-bounce time 50ms.
			bind_press_time = release_t - press_t;
	}

	prev_state = curr_state;


// Update bind led.
if(proto_mode == BIND_MODE) gpio_set(PORTD,LED_O);
    else gpio_clear(PORTD,LED_O);
}


u32 CLOCK_getms()
{
u32 ms;

	u8 temp_reg = SREG;
	// disable interrupts 
	cli();
	ms = msecs;
	//re-enable interrupts
	if(temp_reg & 0x80) sei(); // Global (I)nterrupt Enable bit.

return ms;
}


void CLOCK_delayms(u32 delay_ms)
{
u32 start_ms;

start_ms = msecs;
while(msecs < (start_ms + delay_ms));
}


void PROTOCOL_SetBindState(u32 msec)
{
u32 bind_time;

	if(msec)
    {
      proto_mode = BIND_MODE; // unimod rick added proto_mode.
        if (msec == 0xFFFFFFFF) bind_time = msec;
        else bind_time = CLOCK_getms() + msec;
    }
    else proto_mode = NORMAL_MODE; // unimod rick added. Can't go from bind to range test.
}


//The folloiwng code came from: http://notabs.org/winzipcrc/winzipcrc.c
// C99 winzip crc function, by Scott Duplichan
//We could use the internal CRC implementation in the STM32, but this is really small
//and perfomrance isn't really an issue
u32 Crc(const void *buffer, u32 size)
{
   u32 crc = ~0;
   const u8  *position = buffer;

   while (size--)
      {
      int bit;
      crc ^= *position++;
      for (bit = 0; bit < 8; bit++)
         {
         s32 out = crc & 1;
         crc >>= 1;
         crc ^= -out & 0xEDB88320;
         }
      }
   return ~crc;
}

