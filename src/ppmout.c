


#include "common.h"
#include "interface.h"
#include <avr/interrupt.h>
#include "misc.h"


#define PPM_SHIFT -1 // +1 for positive, -1 for negative shift.
#define SERVO_SYNC_PULSE 300L // 300 micro seconds.
#define FRAME_SYNC_PULSE 6000L // Fixed frame sync pulse of 6000 micro seconds.
#define CENTRE_PW 1500L // Servo centre pulse width.



ISR(TIMER1_COMPB_vect)
{
static u8 state = 0;

	if(! state) // Output fixed duration frame sync pulse.
	// For very old hardware using integrator detection circuits).
	{
		// Set idle level.
		if(PPM_SHIFT == +1) TCCR1A = (TCCR1A | (1<<COM1B1)) & ~(1<<COM1B0);
		else TCCR1A |= 3<<COM1B0;

		TCCR1C = 1<<FOC1B; // Strobe FOC1B.
		TCCR1A = (TCCR1A | (1<<COM1B0)) & ~(1<<COM1B1); // Toggle OC1B on Compare Match.


	OCR1B += MICRO_SEC_CONVERT(FRAME_SYNC_PULSE); // Frame sync.
	}
	else if(state & 0x01) // Odd number states - Servo sync pulse.
	{
	OCR1B += MICRO_SEC_CONVERT(SERVO_SYNC_PULSE);
	}
	else // Even number states output servo pulse - sync value.
	{
#define PPM_SCALING 500L // 500 for Most systems or 550 for Multiplex. e.g. 1500 + 500 us
	s32 value = (s32) (( PPM_SCALING * Channels[(state>>1)-1] ) / CHAN_MAX_VALUE) + CENTRE_PW;
	OCR1B += MICRO_SEC_CONVERT(value - SERVO_SYNC_PULSE);
#undef PPM_SCALING
	}

state++;
if(state > (NUM_OUT_CHANNELS*2) +1) state = 0;
}


static void initialize()
{
// Setup timer 1.
TIFR1  |= 1<<OCF1B; // Reset Flag.
TIMSK1 |= 1<<OCIE1B; // Timer interrupt enable for match to OCR1B
}


void PWM_Stop()
{
TIMSK1 &= ~(1<<OCIE1B); // Disable timer 1 OCR1B interrupts.
}


const void * PPMOUT_Cmds(enum ProtoCmds cmd)
{
    switch(cmd) {
        case PROTOCMD_INIT: initialize(); return 0;
        case PROTOCMD_DEINIT: PWM_Stop(); return 0;
        case PROTOCMD_CHECK_AUTOBIND: return 0;
        case PROTOCMD_BIND:  initialize(); return 0;
        case PROTOCMD_NUMCHAN: return (void *)((unsigned long) NUM_OUT_CHANNELS);
        case PROTOCMD_DEFAULT_NUMCHAN: return (void *) 6L;
/*        case PROTOCMD_GETOPTIONS:
            if (Model.proto_opts[CENTER_PW] == 0) {
                Model.proto_opts[CENTER_PW] = 1100;
                Model.proto_opts[DELTA_PW] = 500;
                Model.proto_opts[NOTCH_PW] = 400;
                Model.proto_opts[PERIOD_PW] = 22500;
            }
            return ppm_opts;
*/
        case PROTOCMD_TELEMETRYSTATE: return (void *)(long) PROTO_TELEM_UNSUPPORTED;
        default: break;
    }
    return 0;
}

