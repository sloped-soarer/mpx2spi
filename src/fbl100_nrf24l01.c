
//
#ifdef PROTO_HAS_NRF24L01

#include <stdlib.h>
#include <string.h>
#include <avr/interrupt.h>   // Edit by cam - Added
#include <util/delay.h>

#include "mpx.h"
#include "common.h"
#include "misc.h"
#include "iface_nrf24l01.h"


#define  FREQUENCE_NUM  20

// Available frequency must be in between 2402 and 2477.
static u8 channel_table[FREQUENCE_NUM]=
		{
		// Base channel numbers mostly spaced apart by 4 (some are 3) to give a good spread.
		2,6,10,14,18,21,25,29,33,36,
        40,44,48,51,55,59,63,66,70,74
		};

static u8 channel_index;

static const u8  binding_adr_rf[5]={0x12,0x23,0x23,0x45,0x78}; // Sync word for binding.
static u8 rf_adr_buf[5]={0x00,0x00,0x00,0x00,0x98}; // Use 9 as the first nibble ... see datasheet on sync words.


static u8 bind_data[4][10];
// volatile u8 transmit_started =0;
volatile u8 bind;
static unsigned int ch_data[8];
static u8 payload[10];


void fbl100_reg_init()
{

    NRF24L01_WriteReg(NRF24L01_02_EN_RXADDR, 0x00);  // Disable all RX data pipes. // FOR SOME REASON THIS HAS TO BE FIRST.

    // Disable all interrupts, Enable 2 bytes CRC, PTX.
	NRF24L01_WriteReg(NRF24L01_00_CONFIG,(
	(1<<NRF24L01_00_MASK_RX_DR) | (1<< NRF24L01_00_MASK_TX_DS) | (1<<NRF24L01_00_MASK_MAX_RT) |
	(1<<NRF24L01_00_EN_CRC) | (1<<NRF24L01_00_CRCO) ) | (0<<NRF24L01_00_PRIM_RX) );	
 
    NRF24L01_WriteReg(NRF24L01_01_EN_AA, 0x00);      // No Auto Acknowledgement.
    NRF24L01_WriteReg(NRF24L01_03_SETUP_AW, 0x03);   // 5-byte RX/TX address (byte -2)
    NRF24L01_WriteReg(NRF24L01_04_SETUP_RETR, 0x00); // No retransmission.

    NRF24L01_SetBitrate(0);                          // 1 Mbps

    NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x70);     // Clear data ready, data sent, and retransmit

    // Power up, Enable TX_DS IRQ (Enabled IRQ for test signal only).
	u8 x = NRF24L01_ReadReg(NRF24L01_00_CONFIG);
	x &= ~(1 << NRF24L01_00_MASK_TX_DS);
	NRF24L01_WriteReg(NRF24L01_00_CONFIG, x);
	x |=  (1<<NRF24L01_00_PWR_UP);
	NRF24L01_WriteReg(NRF24L01_00_CONFIG, x);

}


void build_binding_array(void)
{

    u8 i;
    unsigned int  sum = 0;
    u8 sum_l,sum_h;

    channel_index = 0;

    for(i=0;i<5;i++) sum += rf_adr_buf[i];
    sum_l = (u8)sum;
    sum >>= 8;
    sum_h = (u8)sum;
    bind_data[0][0] = 0xFF;
    bind_data[0][1] = 0xAA;
    bind_data[0][2] = 0x55;
    for(i=3;i<8;i++)
      bind_data[0][i] = rf_adr_buf[i-3];

    for(i=1;i<4;i++)
    {
      bind_data[i][0] = sum_l;
      bind_data[i][1] = sum_h;
      bind_data[i][2] = i-1;
    }
    for(i=0;i<7;i++)
    bind_data[1][i+3] = channel_table[i];
    for(i=0;i<7;i++)
    bind_data[2][i+3] = channel_table[i+7];
    for(i=0;i<6;i++)
    bind_data[3][i+3] = channel_table[i+14];

}


// FBL100 channel sequence: swash_right(starboard), swash_rear, throttle, tail_rotor, not_used, swash_left(port), not_used, not_used.
static void build_data_packet()
{
    for(u8 i = 0; i< 8; i++)
	{
        if(i < Model.num_channels)
		{
s32 value;
//u8 temp_reg = SREG;
//cli();
value = (s32) Channels[i];
//if(temp_reg & 0x80) sei(); // Global (I)nterrupt Enable bit.

		value = (s32) value * 500 / CHAN_MAX_VALUE + 500;
            if (value < 0) value = 0;
            else if (value > 1000) value = 1000;
		ch_data[i] = (unsigned int) value;
		}
        else ch_data[i] = 500; // any data between 0 to 1000 is ok


	payload[i] = (u8) ch_data[i];
    }

    payload[8]  = (u8)((ch_data[0]>>8)&0x0003);
    payload[8] |= (u8)((ch_data[1]>>6)&0x000c);
    payload[8] |= (u8)((ch_data[2]>>4)&0x0030);
    payload[8] |= (u8)((ch_data[3]>>2)&0x00c0);

    payload[9]  = (u8)((ch_data[4]>>8)&0x0003);
    payload[9] |= (u8)((ch_data[5]>>6)&0x000c);
    payload[9] |= (u8)((ch_data[6]>>4)&0x0030);
    payload[9] |= (u8)((ch_data[7]>>2)&0x00c0);
}


static u16 fbl100_cb()
{
static u8 counter = 0;
static u8 channel_index = 0;

// Tx sequence split into two phases.
// Bind or data packet every 9ms.
// mpx comms every 18ms.


	if(! (counter & 0x01))
	{
	// **** Tx prep ****
		if(bind)
		{
			NRF24L01_WriteReg(NRF24L01_05_RF_CH, 81); // Binding packet sent in channel 81
			NRF24L01_SetPower(TXPOWER_0); // Lowest power level for binding.
		}
		else
		{
			NRF24L01_WriteReg(NRF24L01_05_RF_CH, channel_table[channel_index]);
			NRF24L01_SetPower(Model.tx_power); // Highest power level
		}

		NRF24L01_FlushTx(); //0xE1
		counter ++;
		return 1180;
	}
	// **** Tx data ****
	else
	{
	NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x20); // Write 1 to clear TX_DS.

		// No TX Strobe ?
		if(bind)
		{
			NRF24L01_WritePayload(bind_data[(counter >> 1) & 0x03], 10);
		}
		else
		{
			NRF24L01_WritePayload(payload, 10);
			channel_index++;
			if(channel_index >= FREQUENCE_NUM) channel_index = 0;
			build_data_packet(); // 11 Mhz AVR takes about 0.51ms
		}
		// Transmission of RF packet takes about 320uS.
		// Measuring the TX_DS_IRQ signal from previous NRF24L01_WriteReg(NRF24L01_07_STATUS, 0x20) command.

	// mpx comms
	// if((counter & 0x03) == 0x03) {send_mpx_telem(); gpio_set(PIND,LED_G);} //toggle LED.

	counter ++;
	return 7820;
	}
}


static void initialize(u8 state)
{
bind = state;

	CLOCK_StopTimer();

	// Copy transmitter id (4 bytes) into our 5 byte transmitter id. high byte is unaffected.
	memcpy(rf_adr_buf, &Model.fixed_id, 4);

	// Modify channel_table base frequencies based on bit shifting model.fixed_id.
	// This could result in two repeated channels as the spacing is not always 4.
	// So we add a check to prevent this happening. DONE !.
	for(channel_index = 0; channel_index < FREQUENCE_NUM; channel_index ++)
	{
	channel_table[channel_index] += (Model.fixed_id >> channel_index) & 0x03;
		if(channel_index > 0 && channel_table[channel_index - 1] == channel_table[channel_index])
			channel_table[channel_index] ++;
	}

	// No chip reset ?
	fbl100_reg_init();
 
    if(bind)
	{
    build_binding_array();

    NRF24L01_WriteRegisterMulti(NRF24L01_10_TX_ADDR, (u8 *) binding_adr_rf, 5);
	PROTOCOL_SetBindState(0xFFFFFFFF);
	}
	else
	{
		build_data_packet(); // 11 Mhz AVR takes about 0.51ms

		NRF24L01_WriteRegisterMulti(NRF24L01_10_TX_ADDR, rf_adr_buf, 5);
		PROTOCOL_SetBindState(0);
	}
	
	CLOCK_StartTimer(25000, &fbl100_cb);
}


const void * FBL100_Cmds(enum ProtoCmds cmd)
{
    switch(cmd)
	{
        case PROTOCMD_INIT:  initialize(0); return 0;
        case PROTOCMD_DEINIT: return 0;
        case PROTOCMD_CHECK_AUTOBIND: return (void *) 0L; // NEVER autobind
        case PROTOCMD_BIND:  initialize(1); return 0;
        case PROTOCMD_NUMCHAN: return (void *)8L;
        case PROTOCMD_DEFAULT_NUMCHAN: return (void *)6L;
//        case PROTOCMD_CURRENT_ID: return Model.fixed_id ? (void *)((unsigned long)Model.fixed_id) : 0;
        case PROTOCMD_TELEMETRYSTATE: return (void *)(long)-1;
        default: break;
    }
    return 0;
}

#endif
