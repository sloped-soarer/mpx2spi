

#include "common.h"
#include "mpx_serial.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>


extern volatile u8 g_sync_count;
// extern volatile uint16_t g_entropy;
extern volatile enum PROTO_MODE proto_mode;

volatile struct MPX_Telemetry mpx_telemetry[15];

// define macro for UBRR value at double speed eg U2X0=1
#define UART_BAUD_VALUE(baudrate) ((F_CPU + (baudrate) * 4L) / (8L * (baudrate))) -1
// Check compiled code for correct UBRR0 values.

// Linear buffer for received characters.
#define BUFFER_SIZE 50 // Frame is 35 octets.
static volatile u8 l_buffer[BUFFER_SIZE];


void mpx_telem_send()
{
/*
Telemetry frame consists of :
5 Octets.
1st Octet is a bitfield of
bit 7=? 6=range test active 5=bind active 4=no rx detected
bit 3=? 2=normally set 1=rf enabled 0=?
2nd Octet = 0xFF
3rd Octet high nibble = telemetry channel (0-14), low nibble=telemetry units e.g.
Volts, Amps, m/s Km/h rpm degC degF metres %fuel %LQI mAh mL
4th & 5th Bytes = telemetry value (15 bit signed  << 1), bit 0 = alarm condition. LSB first.
*/

static u8 mpx_telem_pkt[5];
static u8 telem_channel =0;

struct MPX_Telem_Flags {
unsigned not_known_0  :1;
unsigned rf_enabled   :1;
unsigned normally_set :1;
unsigned not_known_3  :1;
unsigned no_rx        :1;
unsigned bind_active  :1;
unsigned range_active :1;
unsigned not_known_7  :1;
};

#if 0
union{
struct MPX_Telem_Flags flags_bits;
unsigned char flags;
}mpx_telem;
#endif

	if(proto_mode == BIND_MODE)
	{
		uart_putc(0x26);
		uart_putc(0xFF);
		uart_putc(0x00);
		uart_putc(0x00);
		uart_putc(0x00);
	}
	else // NORMAL and RANGE
	{
		if(proto_mode == NORMAL_MODE) mpx_telem_pkt[0] = 0x06; // RF enabled (Can be used to return the state of the signal on the Royal Module port)
		if(proto_mode == RANGE_MODE) mpx_telem_pkt[0] = 0x46; // RF enabled (Can be used to return the state of the signal on the Royal Module port)
		mpx_telem_pkt[1] = 0xFF;
		mpx_telem_pkt[2] = (telem_channel << 4) | mpx_telemetry[telem_channel].units;
		mpx_telem_pkt[3] = mpx_telemetry[telem_channel].value << 1 | mpx_telemetry[telem_channel].alarm;
		mpx_telem_pkt[4] = mpx_telemetry[telem_channel].value >> 7;
		for(u8 i=0; i<5; i++) uart_putc(mpx_telem_pkt[i]);
	}

	telem_channel ++;
	if(telem_channel > 14) telem_channel =0;
}


ISR(USART_RX_vect)
{
static unsigned char read_ptr = 1;
static unsigned char write_ptr = 0; // next free position
static unsigned int previous_cnt =0;
static u8 servo_count = 0;


u8 temp_octet = UDR0; // Read received char.
u16 current_cnt = TCNT1;

/*
Frame seems to be comprised of the following :
One header byte usually 0x8n, but i have seen 0x9? when a model memory was corrupted.
nibble n appears to be a bitfield containing bit 2=set failsafe, 1=fast response off

Two bytes for each channel 1-16, LSB first (16 bit signed value) Range of -1521 to +1520.
Two termination bytes of 0x00.
*/

unsigned int cnt_diff;
if(current_cnt >= previous_cnt) cnt_diff = current_cnt - previous_cnt;
else cnt_diff = (0xffff - previous_cnt) + current_cnt + 1 ;

	if(cnt_diff > MICRO_SEC_CONVERT(500) )
	{
	write_ptr = 0;
	read_ptr = 1;
	g_sync_count ++;
	servo_count = 0;
	}

	l_buffer[write_ptr] = temp_octet;
	if(write_ptr < BUFFER_SIZE -1) write_ptr ++;


// Read out channel data, little and often is good.

		if(write_ptr > read_ptr+1)
		{
			if(servo_count < NUM_OUT_CHANNELS)
			{		
			Channels[servo_count] = l_buffer[read_ptr] | (l_buffer[read_ptr+1]<<8);
			read_ptr +=2;
 			servo_count++;
			}
		}

	previous_cnt = current_cnt;

//	if(g_entropy==0) g_entropy = TCNT1;

//if(write_ptr == 35) gpio_clear(PORTB,DEBUG_2);
}


void mpx_start(void)
{
u8 c;
u8 timeout =0;


for(u8 i=0; i<15; i++)
{
	mpx_telemetry[i].alarm = 0;
	mpx_telemetry[i].value = 0;
	mpx_telemetry[i].units = TELEM_MPX_NOT_USED;
}
mpx_telemetry[0].units = TELEM_MPX_P_CENT_LQI;
mpx_telemetry[1].units = TELEM_MPX_VOLTS_TENTHS;
mpx_telemetry[2].units = TELEM_MPX_VOLTS_TENTHS;


	// Initialisation of USART.
	// Clear USART transmit complete, Disable double speed.
	UCSR0A = (1<<TXC0) | (0<<U2X0);
	// Enable TX + RX + **NO**  RX interrupts.
	UCSR0B = (0<<RXCIE0) | (1<<RXEN0) | (1<<TXEN0) | (0<<UCSZ02);

    // Asynchronous mode, Parity disabled, 1 stop bit, 8 bit character.
	UCSR0C = 6;
	UBRR0 = UART_BAUD_VALUE(19200) ; // 19200
	UBRR0 = UBRR0 >>1; // Divide by 2 for U2X0=0.


	// Start communication using polling.
	while(1)
	{
	if(UCSR0A & (1<<RXC0)) break; // Wait for receive complete flag.
	_delay_ms(20);
	timeout++;
	if(timeout > 100) return;
	}
	c = UDR0;
	if(c !='v') return; // Transmitter asks for version of module ... only sends this twice before giving up.

	_delay_ms(12);
	uart_puts("M-LINK   V3200"); // HFM-4 M-LINK Identifier.
	uart_putc(0x0D);
	uart_putc(0x0A);
	loop_until_bit_is_set (UCSR0A,TXC0); // Wait for transmit complete.

	do
	{
	loop_until_bit_is_set (UCSR0A,RXC0); // Wait for receive complete.
	c = UDR0;
	}
	while( (c !='b') && (c !='r') && (c !='a') ); // (b)ind(en), (r)ange, (a)ctivate ?.

	UBRR0 = UART_BAUD_VALUE(115200);

#if F_CPU == 16000000
// Get least baud error @ 16MHz.
	UCSR0A |= (1<<U2X0); // Double speed UART.
#else
	UBRR0 = UBRR0 >>1; // Divide by 2 for U2X0=0.
#endif

UCSR0B |= (1<<RXCIE0); // Enable receive interrupts.

_delay_ms(10);
if(c=='b') proto_mode = BIND_MODE;
else if(c=='r') proto_mode = RANGE_MODE;
else proto_mode = NORMAL_MODE;
}


void uart_putc(char c)
{
   loop_until_bit_is_set (UCSR0A,UDRE0); // Wait for USART Tx buffer (UDR0) to empty.
	UCSR0A |= (1<<TXC0); // Reset USART transmit complete flag. Should be checked before switching baudrates.
   UDR0 = c;
}

void uart_puts(char * str)
{
	while (str[0]!=0)
	{
	    uart_putc(str[0]);
	    str++;
	}
}

