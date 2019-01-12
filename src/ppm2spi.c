
//
//#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h> 

#include "common.h"
#include "interface.h"
#include "spi.h"
#include "mpx_serial.h"
#include "misc.h"

// Prototypes.
s8 bind_button_duration(void);
const void * select_protocol_cmds(enum Protocols idx);
static int get_module(enum Protocols idx);
void change_protocol(void);
void set_proto_leds(enum Protocols idx);
void initialise_hardware(void);


// Global Variables.
const void * (*PROTO_Cmds)(enum ProtoCmds);
struct Transmitter Transmitter;
struct Model Model;
volatile s16 Channels[16];
volatile u8 g_sync_count = 0;
volatile u16 g_entropy;
volatile u32 bind_press_time = 0;
volatile enum PROTO_MODE proto_mode = NORMAL_MODE; // static u8 proto_state was similar in function.

static u8 mcusr_mirror;




int main(void)
{
initialise_hardware();

Model.num_channels = NUM_OUT_CHANNELS;
Model.tx_power = TXPOWER_7;

	// Check to see if a TX ID has been generated before.
	if(eeprom_read_byte((u8 *) 0) == 0x36 && eeprom_read_byte((u8 *) 1) == 0xA4)
	{
	// Retrieve TX TD from EEPROM
	Model.fixed_id = eeprom_read_dword((u32 *) 2);
	Model.protocol = eeprom_read_word((u16 *) 6);
	}
	else
	{
	Model.fixed_id = 0UL; // Temporary value.
	Model.protocol = PROTOCOL_NONE;
//	Model.protocol = PROTOCOL_FRSKY2WAY;
	eeprom_write_word ((u16 *)6, Model.protocol);
	while (! eeprom_is_ready()); //Make sure EEPROM write finishes
	}

/* Hardcode protocol options for now */
#ifdef PROTO_HAS_A7105
	if(PROTOCOL_FLYSKY == Model.protocol)
	{
//	Model.proto_opts[0] = 1; // Format = "Sky Wlkr"
//	Model.proto_opts[1] = 1; // Telemetry = "On"
	}
	if(PROTOCOL_HUBSAN == Model.protocol)
	{
   	Model.proto_opts[0] = 1; // vTX MHz = 0
   	//    	Model.proto_opts[1] = 0; // Telemetry = "Off"
	}
#endif
#ifdef PROTO_HAS_CC2500
	if(PROTOCOL_FRSKY1WAY || PROTOCOL_FRSKY2WAY == Model.protocol)
	{
	Model.proto_opts[0] = -8; // Freq-Fine
	// Frsky rf deck = 0, Skyartec rf module = -17.
	if(PROTOCOL_FRSKY2WAY == Model.protocol) Model.proto_opts[1] = 1; // Telemetry on.
	}
#endif

PROTO_Cmds = select_protocol_cmds(Model.protocol);
set_proto_leds(Model.protocol);

#if UNIMOD == 1
// Disable Input Capture
TCCR1B &= ~(1<<ICNC1);
mpx_start(); // Start communication with Multiplex Royal Pro V3.46
#else
if(read_bind_sw()) proto_mode = BIND_MODE;
// Enable ICP interrupt
TIMSK1 = (1<<ICIE1);
// Enable Input Capture
TCCR1B |= (1<<ICNC1);
#endif

if(Model.fixed_id == 0)
{
//Wait until we have received 50 frames.
//while (g_sync_count < 51);
//u8 temp_reg = SREG;
//cli();
//g_entropy = TCNT1;
//if(temp_reg & 0x80) sei(); // Global (I)nterrupt Enable bit.

// Get some much needed entropy for rand()
//srand(g_entropy);
Model.fixed_id = 0x1111UL; // rand() + ((u32) rand() << 16);
	eeprom_write_byte ((u8 *)0, 0x36); // Just two different numbers to denote a locked in tx id.
	eeprom_write_byte ((u8 *)1, 0xA4);
	eeprom_write_dword ((u32 *)2, Model.fixed_id);
	while (!eeprom_is_ready()); //Make sure EEPROM write finishes

	// Flash all LEDs.
	while(1)
	{
	set_proto_leds(PROTOCOL_NONE);
	CLOCK_delayms(1000);
	LED_ON(LED_G); LED_ON(LED_R); LED_ON(LED_Y);
	CLOCK_delayms(1000);
	}
}



	if(NULL != PROTO_Cmds)
	{
		if(BIND_MODE == proto_mode)	PROTO_Cmds(PROTOCMD_BIND);
		else if(NORMAL_MODE == proto_mode) PROTO_Cmds(PROTOCMD_INIT);
		else // RANGE_MODE == proto_mode
		{
		// TODO Set output power to reduced value  -30 dB.
		PROTO_Cmds(PROTOCMD_INIT);
		}
	}

	while(1) // main loopy.
	{
		if( bind_button_duration() > 5) // 6 seconds.
			change_protocol();

#if UNIMOD == 99
    	// If trainer port is used, suspend the RF protocol.
    	if(! RF_EN_STATE() && (msecs > 4000))
    	{
    	MPX_SERIAL_Cmds(PROTOCMD_DEINIT);
    	if(NULL != PROTO_Cmds) PROTO_Cmds(PROTOCMD_DEINIT);
    	proto_mode = RANGE_MODE;
    	set_proto_leds(PROTOCOL_NONE);
    	}
#endif // UNIMOD
	}
}

#if 0
	if(proto_type == SET_ID)
	{
	  	wdt_enable(WDTO_250MS); // enable watchdog
	  	while(1); // wait for watchdog to reset processor 
	}
#endif



u8 read_bind_sw(void)
{
// Change to +ve logic.
	return (PINC & (1<<BIND_SW)) ? 0 : 1;
}

s8 bind_button_duration(void)
{
s8 temp;

	if(bind_press_time)
	{
	// return as seconds.
	temp = bind_press_time / 1000;
	if(temp > 10) temp = 10;
	bind_press_time = 0; // Reset variable when acted upon.
	return temp;
	}
	else return -1;
}


const void * select_protocol_cmds(enum Protocols idx)
{
const void * (*Sel_PROTO_Cmds)(enum ProtoCmds);
	#define PROTODEF(proto, module, map, cmd, name) case proto: Sel_PROTO_Cmds = cmd; break;
	switch(idx) {
		#include "protocol.h"
	#undef PROTODEF
	default: Sel_PROTO_Cmds = NULL; break;
	}
return Sel_PROTO_Cmds;
}


static int get_module(enum Protocols idx)
{
	int m;
	#define PROTODEF(proto, module, map, cmd, name) case proto: m = module; break;
    switch(idx) {
        #include "protocol.h"
	#undef PROTODEF
	default: m = TX_MODULE_LAST; break;
    }
    return m;
}


void change_protocol(void)
{
// Protocol selection.
enum Protocols temp_protocol = Model.protocol;

	// TODO Add TX stick / slider selection method ?.

//	printf_u3("Press Bind button to step through protocols\n\r");
//	printf_u3("Press Bind button for more than 3 seconds to confirm selection\n\r");
	while(1)
	{
	s8 time = bind_button_duration();
		if(time == 0)
		{
			// short press < 1 sec.
			temp_protocol ++;
				if(TX_MODULE_LAST == get_module(temp_protocol) || temp_protocol >= PROTOCOL_COUNT)
	    			temp_protocol = PROTOCOL_NONE;

//			printf_u3("Use Protocol %s ?\n\r",ProtocolNames[temp_protocol]);
	    	set_proto_leds(temp_protocol);	// Update LED's to indicate selection.
		}

		if(time > 2)
		{
//			printf_u3("Selected Protocol is %s \n\r",ProtocolNames[temp_protocol]);
//			display_proto_options(temp_protocol);
			// Store temp_protocol as Model.protocol in EEPROM.
			set_proto_leds(PROTOCOL_NONE);
			eeprom_write_word ((u16 *)6, temp_protocol);
			CLOCK_delayms(250);
			while (!eeprom_is_ready()); //Make sure EEPROM write finishes
			for(u8 i=1; i<4; i++)
			{
			set_proto_leds(temp_protocol);
			CLOCK_delayms(250);
			set_proto_leds(PROTOCOL_NONE);
			CLOCK_delayms(250);
			}
			break;
		}
	}
}


void set_proto_leds(enum Protocols idx)
{
LED_OFF(LED_G); LED_OFF(LED_R); LED_OFF(LED_Y);

#ifdef PROTO_HAS_CC2500
	if(PROTOCOL_FRSKY1WAY == idx) LED_ON(LED_G);
	else if(PROTOCOL_FRSKY2WAY == idx) LED_ON(LED_R);
#endif
#ifdef PROTO_HAS_A7105
	if(PROTOCOL_HUBSAN == idx) LED_ON(LED_G);
	else if(PROTOCOL_FLYSKY == idx) LED_ON(LED_R);
#endif
	else if(PROTOCOL_PPM == idx) LED_ON(LED_Y);
}


void initialise_hardware(void)
{
	cli();

	//The correct sequence of events to disable the watchdog timer.
	mcusr_mirror = MCUSR;
	MCUSR = 0;
	wdt_disable();

	gpio_clear (DDRB, PPM_IN);  // Input  pin0
	gpio_set   (PORTB, PPM_IN); // Pull up resistor on pin0 //rick
	gpio_set   (DDRB, DEBUG_1); // Output pin1
	gpio_set   (DDRB, DEBUG_2); // Output pin2 (SS input - General purpose output pin)
	gpio_set   (DDRB,3);        // MOSI output
	gpio_clear (DDRB,4);        // MISO input
	gpio_set   (DDRB,5);        // SCK output

	gpio_set   (DDRC, SPI_CS);   // Output pin3
	gpio_clear (DDRC, CH_ORD);   // Input pin4
	gpio_set   (PORTC,CH_ORD);   // Pull up resistor on pin4
	gpio_clear (DDRC, BIND_SW);  // Input pin5
	gpio_set   (PORTC,BIND_SW);  // Pull up resistor on pin5

	gpio_clear (DDRD, RF_EN);  // Input pin
	gpio_set   (PORTD, RF_EN); // Pull up resistor on pin

	// gpio_clear (DDRD, 0);  // Serial Rx input // USART Overrides
	// gpio_set   (DDRD, 1);  // Serial Tx output // USART Overrides

	gpio_set   (DDRD, LED_G);  // LED green  pin2
	gpio_set   (DDRD, LED_R);  // LED red    pin3
	gpio_set   (DDRD, LED_Y);  // LED yellow pin4
	gpio_set   (DDRD, LED_O);  // LED orange pin5

	spi_enable_master_mode();

	// Setup Timer 1.
	// Normal mode (0), OVF @ TOP (0xFFFF), F_CPU/8, Noise canceler on ICP, Falling edge trigger.
	// Set up for input capture on PB0 (ICP1).
	TCCR1B = (0<<ICES1) | (2<<CS10); // Falling edge trigger, ICP1 has pull-up enabled.
	// Should work better when connected to open collector stages and when left unconnected.
	// Disable Input Capture
	TCCR1B &= ~(1<<ICNC1);
	TCCR1A=0;
	TCCR1C=0;


	// Setup timer 2 as a 1 milli-second timer.
	// Mode 0 (Normal) 1/128 prescaler.
	TCCR2A = 0<<WGM20;
	TCCR2B = (5<<CS20) | (0<<WGM22);
	TIFR2 = 1<<OCF2A; // reset flag.
	OCR2A = COUNTS_PER_MILLI_SEC;
	TCNT2 = 0;
	TIMSK2 =  1<<OCIE2A;

	// Global interrupt enable
	sei();
}

