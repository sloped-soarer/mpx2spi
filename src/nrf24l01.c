/*
#ifdef MODULAR
  //Allows the linker to properly relocate
  #define DEVO_Cmds PROTO_Cmds
  #pragma long_calls
#endif
*/


#ifdef PROTO_HAS_NRF24L01
#include <util/delay.h>
#include "common.h"
#include "spi.h"
#include "iface_nrf24l01.h"

/* Instruction Mnemonics */
#define R_REGISTER    0x00
#define W_REGISTER    0x20
#define REGISTER_MASK 0x1F
#define ACTIVATE      0x50
#define R_RX_PL_WID   0x60
#define R_RX_PAYLOAD  0x61
#define W_TX_PAYLOAD  0xA0
#define W_ACK_PAYLOAD 0xA8
#define FLUSH_TX      0xE1
#define FLUSH_RX      0xE2
#define REUSE_TX_PL   0xE3
#define NOP           0xFF


static u8 rf_setup; // bug fix: don't initialized static var here, otherwise it will break devo7e build when this protocol is enabled

void NRF24L01_Initialize()
{
	rf_setup = 0x0f;
}    

u8 NRF24L01_WriteReg(u8 reg, u8 data)
{
    CS_LO();
    u8 res = spi_xfer(W_REGISTER | (REGISTER_MASK & reg));
    spi_xfer(data);
    CS_HI();
    return res;
}

u8 NRF24L01_WriteRegisterMulti(u8 reg, u8 data[], u8 length)
{
    CS_LO();
    u8 res = spi_xfer(W_REGISTER | ( REGISTER_MASK & reg));
    for (u8 i = 0; i < length; i++)
    {
        spi_xfer(data[i]);
    }
    CS_HI();
    return res;
}

u8 NRF24L01_WritePayload(u8 *data, u8 length)
{
    CS_LO();
    u8 res = spi_xfer(W_TX_PAYLOAD);
    for (u8 i = 0; i < length; i++)
    {
        spi_xfer(data[i]);
    }
    CS_HI();
    return res;
}

u8 NRF24L01_ReadReg(u8 reg)
{
    CS_LO();
    spi_xfer(R_REGISTER | (REGISTER_MASK & reg));
    u8 data = spi_xfer(0);
    CS_HI();
    return data;
}

u8 NRF24L01_ReadRegisterMulti(u8 reg, u8 data[], u8 length)
{
    CS_LO();
    u8 res = spi_xfer(R_REGISTER | (REGISTER_MASK & reg));
    for(u8 i = 0; i < length; i++)
    {
        data[i] = spi_xfer(0);
    }
    CS_HI();
    return res;
}

u8 NRF24L01_ReadPayload(u8 *data, u8 length)
{
    CS_LO();
    u8 res = spi_xfer(R_RX_PAYLOAD);
    for(u8 i = 0; i < length; i++)
    {
        data[i] = spi_xfer(0);
    }
    CS_HI();
    return res;
}

static u8 Strobe(u8 state)
{
    CS_LO();
    u8 res = spi_xfer(state);
    CS_HI();
    return res;
}

u8 NRF24L01_FlushTx()
{
    return Strobe(FLUSH_TX);
}

u8 NRF24L01_FlushRx()
{
    return Strobe(FLUSH_RX);
}

u8 NRF24L01_NOP()
{
    return Strobe(NOP);
}

u8 NRF24L01_Activate(u8 code)
{
    CS_LO();
    u8 res = spi_xfer(ACTIVATE);
    spi_xfer(code);
    CS_HI();
    return res;
}

u8 NRF24L01_SetBitrate(u8 bitrate)
{

	u8 temp = NRF24L01_ReadReg(NRF24L01_06_RF_SETUP);
	temp = (temp & 0xF7) | ((bitrate & 0x01) << 3);
    return NRF24L01_WriteReg(NRF24L01_06_RF_SETUP, temp);
}



u8 NRF24L01_SetPower(u8 power)
{
/*
// nRF24L01+ Power Output
     Raw       * 20dBm PA
0 : -18dBm    2dBm (1.6mW)
1 : -12dBm    8dBm   (6mW)
2 :  -6dBm   14dBm  (25mW)
3 :   0dBm   20dBm (100mW)
*/

// Claimed power amp for nRF24L01 from eBay is 20dBm ?.

u8 nrf_power = 0;

    switch(power)
	{
        case 0: nrf_power = 0; break; //
        case 1: nrf_power = 0; break; //
        case 2: nrf_power = 1; break; //
        case 3: nrf_power = 1; break; //
        case 4: nrf_power = 2; break; //
        case 5: nrf_power = 2; break; //
        case 6: nrf_power = 3; break; //
        case 7: nrf_power = 3; break; //
        default: nrf_power = 0; break;
    };
    // Power is in range 0..3 for nRF24L01

	u8 temp = NRF24L01_ReadReg(NRF24L01_06_RF_SETUP);
	temp = (temp & 0xF9) | ((nrf_power & 0x03) << 1);
	return NRF24L01_WriteReg(NRF24L01_06_RF_SETUP, temp);
}

static void CE_lo()
{
#if HAS_MULTIMOD_SUPPORT
    SPI_ConfigSwitch(0x0f, 0x0b);
#endif
}
static void CE_hi()
{
#if HAS_MULTIMOD_SUPPORT
    SPI_ConfigSwitch(0x1f, 0x1b);
#endif
}

void NRF24L01_SetTxRxMode(enum TXRX_State mode)
{
    if(mode == TX_EN) {
        CE_lo();
        NRF24L01_WriteReg(NRF24L01_07_STATUS, (1 << NRF24L01_07_RX_DR)    //reset the flag(s)
                                            | (1 << NRF24L01_07_TX_DS)
                                            | (1 << NRF24L01_07_MAX_RT));
        NRF24L01_WriteReg(NRF24L01_00_CONFIG, (1 << NRF24L01_00_EN_CRC)   // switch to TX mode
                                            | (1 << NRF24L01_00_CRCO)
                                            | (1 << NRF24L01_00_PWR_UP));
//        usleep(130);
        CE_hi();
    } else if (mode == RX_EN) {
        CE_lo();
        NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);        // reset the flag(s)
        NRF24L01_WriteReg(NRF24L01_00_CONFIG, 0x0F);        // switch to RX mode
        NRF24L01_WriteReg(NRF24L01_07_STATUS, (1 << NRF24L01_07_RX_DR)    //reset the flag(s)
                                            | (1 << NRF24L01_07_TX_DS)
                                            | (1 << NRF24L01_07_MAX_RT));
        NRF24L01_WriteReg(NRF24L01_00_CONFIG, (1 << NRF24L01_00_EN_CRC)   // switch to RX mode
                                            | (1 << NRF24L01_00_CRCO)
                                            | (1 << NRF24L01_00_PWR_UP)
                                            | (1 << NRF24L01_00_PRIM_RX));
//        usleep(130);
        CE_hi();
    } else {
        NRF24L01_WriteReg(NRF24L01_00_CONFIG, (1 << NRF24L01_00_EN_CRC)); //PowerDown
        CE_lo();
    }
}

int NRF24L01_Reset()
{
    NRF24L01_FlushTx();
    NRF24L01_FlushRx();
    u8 status1 = Strobe(NOP);
    u8 status2 = NRF24L01_ReadReg(0x07);
    NRF24L01_SetTxRxMode(TXRX_OFF);
    return (status1 == status2 && (status1 & 0x0f) == 0x0e);
}

#endif // PROTO_HAS_NRF24L01

