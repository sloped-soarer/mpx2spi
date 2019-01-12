/*
#ifdef MODULAR
//Allows the linker to properly relocate
#define DEVO_Cmds PROTO_Cmds
#pragma long_calls
#endif
*/


#ifdef PROTO_HAS_A7105
#include <util/delay.h>
#include "common.h"
#include "spi.h"
#include "interface.h"

#define spi_tx(value)  (void)spi_xfer(value)
#define spi_rx()  spi_xfer(0)

void A7105_WriteReg(u8 address, u8 data)
{
// Set MOSI as output.
DDRB |= (1<<DDB3); // MOSI.
CS_LO();
spi_tx(address);
spi_tx(data);
CS_HI();
}

u8 A7105_ReadReg(u8 address)
{
u8 data;
address = address | 0x40; // Bit 6 indicates read.
// Set MOSI as output.
DDRB |= (1<<DDB3); // MOSI.
CS_LO();
spi_tx(address);
// Set MOSI as input.
DDRB &= ~(1<<DDB3); // MOSI.
data = spi_rx();
CS_HI();
// Set MOSI as output.
DDRB |= (1<<DDB3); // MOSI.
return data;
}


void A7105_WriteData(u8 *dpbuffer, u8 len, u8 channel)
{

A7105_Strobe(A7105_RST_WRPTR);

// Set MOSI as output.
DDRB |= (1<<DDB3); // MOSI.
CS_LO();
spi_tx(0x05);
for (u8 i = 0; i < len; i++) spi_tx(dpbuffer[i]);
CS_HI();

A7105_WriteReg(A7105_0F_CHANNEL, channel);
A7105_Strobe(A7105_TX);
}


void A7105_FIFO_Write_Packet(u8 *dpbuffer, u8 len)
{
DDRB |= (1<<DDB3); // MOSI.
CS_LO();
spi_tx(0x05);
for (u8 i = 0; i < len; i++) spi_tx(dpbuffer[i]);
CS_HI();
}



void A7105_ReadData(u8 *dpbuffer, u8 len)
{
A7105_Strobe(0xF0); //A7105_RST_RDPTR
    for(int i = 0; i < len; i++)
        dpbuffer[i] = A7105_ReadReg(0x05);
/*
    CS_LO();
    spi_xfer(SPI2, 0x40 | 0x05);

    for(i = 0; i < len; i++)
        dpbuffer[i] = spi_read(SPI2);
    CS_HI();
*/
return;
}


/*
 * 1 - Tx else Rx
 */
void A7105_SetTxRxMode(enum TXRX_State mode)
{
    if(mode == TX_EN) {
        A7105_WriteReg(A7105_0B_GPIO1_PIN1, 0x33);
        A7105_WriteReg(A7105_0C_GPIO2_PIN_II, 0x31);
    } else if (mode == RX_EN) {
        A7105_WriteReg(A7105_0B_GPIO1_PIN1, 0x31);
        A7105_WriteReg(A7105_0C_GPIO2_PIN_II, 0x33);
    } else {
        //The A7105 seems to some with a cross-wired power-amp (A7700)
        //On the XL7105-D03, TX_EN -> RXSW and RX_EN -> TXSW
        //This means that sleep mode is wired as RX_EN = 1 and TX_EN = 1
        //If there are other amps in use, we'll need to fix this
        A7105_WriteReg(A7105_0B_GPIO1_PIN1, 0x33);
        A7105_WriteReg(A7105_0C_GPIO2_PIN_II, 0x33);
    }
}

void A7105_Reset()
{
    A7105_WriteReg(0x00, 0x00);
	_delay_us(100);
}

void A7105_WriteID(u32 id)
{
 // Set MOSI as output.
DDRB |= (1<<DDB3); // MOSI.
CS_LO();
spi_tx(0x06); // ID register write address 
spi_tx((id >> 24) & 0xFF);
spi_tx((id >> 16) & 0xFF);
spi_tx((id >> 8) & 0xFF);
spi_tx((id >> 0) & 0xFF);
CS_HI();
}

void A7105_Strobe(enum A7105_State state)
{
// Set MOSI as output.
DDRB |= (1<<DDB3); // MOSI.
CS_LO();
spi_tx(state);
CS_HI();
}

void A7105_SetPower(u8 power)
{
/*
// A7105 Power Output
-23dBm	== PAC=0 TBG=0 
-19dBm	== PAC=2 TBG=0
-15dBm	== PAC=2 TBG=1
-11dBm	== PAC=0 TBG=4
-7.2dBm	== PAC=3 TBG=3
-3.4dBm == PAC=1 TBG=6
+0.1dBm	== PAC=2 TBG=7 // FlySky TM002 module uses this.
+1.3dBm	== PAC=3 TBG=7
*/

// 1110 Power Amp is ~14dB so:

    u8 pac, tbg;

    switch(power)
	{
        case 0: pac = 0; tbg = 0; break; // -9 dBm
        case 1: pac = 2; tbg = 0; break; // -5
        case 2: pac = 2; tbg = 1; break; // -1
        case 3: pac = 0; tbg = 4; break; // +3
        case 4: pac = 3; tbg = 3; break; // +6.8
        case 5: pac = 1; tbg = 6; break; // +10.6
        case 6: pac = 2; tbg = 7; break; // +14.1 dBm 26mW
		case 7: pac = 3; tbg = 7; break; // +15.3 dBm 34mW
        default: pac = 0; tbg = 0; break;
	};

    A7105_WriteReg(0x28, (pac << 3) | tbg);
}

//#pragma long_calls_off   Commented out by cam
#endif // PROTO_HAS_A7105
