/*
 RICK
*/

#ifdef MODULAR
  //Allows the linker to properly relocate
  #define FRSKY2WAY_Cmds PROTO_Cmds
  #pragma long_calls
#endif
#include "common.h"
#include "interface.h"
#include "misc.h"

//#include "interface.h"
//#include "mixer.h"
//#include "config/model.h"
//#include "telemetry.h"

#ifdef UNIMOD
#include "mpx_serial.h"
#endif

#ifdef MODULAR
  //Some versions of gcc apply this to definitions, others to calls
  //So just use long_calls everywhere
  //#pragma long_calls_off
  extern unsigned _data_loadaddr;
  const unsigned long protocol_type = (unsigned long)&_data_loadaddr;
#endif

#ifdef PROTO_HAS_CC2500
#include "iface_cc2500.h"

static const char * const frsky_opts[] = {
    _tr_noop("Freq-Fine"),  "-127", "+127", NULL,
    _tr_noop("Telemetry"),  _tr_noop("Off"), _tr_noop("On"), NULL,
    NULL
};
enum {
    PROTO_OPTS_FREQFINE =0,
	PROTO_OPTS_TELEM,
    LAST_PROTO_OPT,
};
//ctassert(LAST_PROTO_OPT <= NUM_PROTO_OPTS, too_many_protocol_opts);

#define TELEM_ON 1

#ifdef UNIMOD
extern volatile struct MPX_Telemetry mpx_telemetry[15];
extern volatile enum PROTO_MODE proto_mode;
#endif //UNIMOD

static u8 packet[25]; // should only be 20 (telemetry receieve)
static u16 frsky_id;
static u8 channels_used[50];
static u8 channel_offset;
static u8 packet_number = 0;


static void frsky2way_init(u8 bind)
{
        CC2500_SetTxRxMode(TX_EN);

        CC2500_WriteReg(CC2500_17_MCSM1, 0x0C); // Stay in receive after packet reception, Idle state after transmission
        CC2500_WriteReg(CC2500_18_MCSM0, 0x08); // Manual calibration using SCAL. Was 0x18.
        CC2500_WriteReg(CC2500_06_PKTLEN, 0x19);
        CC2500_WriteReg(CC2500_07_PKTCTRL1, 0x05); // Address checking no broadcasts, Append status bytes to payload.
        CC2500_WriteReg(CC2500_08_PKTCTRL0, 0x05); // No data whitening, FIFO for tx/rx, CRC generation and checking,
									// Variable packet length mode set by first byte after sync word (0x11).
        CC2500_WriteReg(CC2500_3E_PATABLE, bind ? 0x50 : 0xFE);

        CC2500_WriteReg(CC2500_0B_FSCTRL1, 0x08);

        // static const s8 fine = 0; // Frsky rf deck = 0, Skyartec = -17.
        CC2500_WriteReg(CC2500_0C_FSCTRL0, (s8) Model.proto_opts[PROTO_OPTS_FREQFINE]);

        CC2500_WriteReg(CC2500_0D_FREQ2, 0x5c);
        CC2500_WriteReg(CC2500_0E_FREQ1, 0x76); // V8 was 0x58
        CC2500_WriteReg(CC2500_0F_FREQ0, 0x27); // V8 was 0x9d
        CC2500_WriteReg(CC2500_10_MDMCFG4, 0xaa);
        CC2500_WriteReg(CC2500_11_MDMCFG3, 0x39); // 31044 baud.
        CC2500_WriteReg(CC2500_12_MDMCFG2, 0x11); // V8 was 0x93

        CC2500_WriteReg(CC2500_13_MDMCFG1, 0x23);
        CC2500_WriteReg(CC2500_14_MDMCFG0, 0x7a);
        CC2500_WriteReg(CC2500_15_DEVIATN, 0x42);

        CC2500_WriteReg(CC2500_19_FOCCFG, 0x16);
        CC2500_WriteReg(CC2500_1A_BSCFG, 0x6c);
        CC2500_WriteReg(CC2500_1B_AGCCTRL2, 0x03); // 0x03 for bind and normal.


        CC2500_WriteReg(CC2500_1C_AGCCTRL1, 0x40);
        CC2500_WriteReg(CC2500_1D_AGCCTRL0, 0x91);
        CC2500_WriteReg(CC2500_21_FREND1, 0x56);
        CC2500_WriteReg(CC2500_22_FREND0, 0x10);
        
        CC2500_WriteReg(CC2500_23_FSCAL3, 0xA9); // Enable charge pump calibration, calibrate for each hop.
        CC2500_WriteReg(CC2500_24_FSCAL2, 0x0a);
        CC2500_WriteReg(CC2500_25_FSCAL1, 0x00);
        CC2500_WriteReg(CC2500_26_FSCAL0, 0x11);
        CC2500_WriteReg(CC2500_29_FSTEST, 0x59);
        CC2500_WriteReg(CC2500_2C_TEST2, 0x88);
        CC2500_WriteReg(CC2500_2D_TEST1, 0x31);
        CC2500_WriteReg(CC2500_2E_TEST0, 0x0b);
        CC2500_WriteReg(CC2500_03_FIFOTHR, 0x07);
        CC2500_WriteReg(CC2500_09_ADDR, bind ? 0x03 : (frsky_id & 0xff));

        CC2500_Strobe(CC2500_SIDLE);    // Go to idle...
        CC2500_Strobe(CC2500_SFTX); // 3b
        CC2500_Strobe(CC2500_SFRX); // 3a

#ifdef UNIMOD
        if(proto_mode == NORMAL_MODE) CC2500_WriteReg(CC2500_3E_PATABLE, 0xFE); // D8 uses PATABLE = 0xFE for normal transmission.
        else CC2500_WriteReg(CC2500_3E_PATABLE, 0x50); // D8 uses PATABLE = 0x50 for range testing and binding.
#endif

        CC2500_WriteReg(CC2500_0A_CHANNR, 0x00);
        CC2500_Strobe(CC2500_SCAL);    // Manual calibration
}


static u8 get_chan_num(u8 idx)
{
const u8 multiplier = 0x23;
/*
The multiplier appears to vary with Txid.
e.g.
Txid	multiplier
0x0766	0x2d
0x101c	0x9b
0x1ee9	0x0a
0x2dd7	0x1e
0x25c2	0x23
0xadac	0x7d
*/

	unsigned int ret = ((idx * multiplier) % 235) + channel_offset;
 //   if(idx == 3 || idx == 23 ret++; rick
 //   if(ret ==0x5a || ret == 0xdc) ret++; rick
    if(idx == 47) return 1; // rick
    if(idx > 47) return 0;
    return (u8) ret;
}


static void frsky2way_build_bind_packet()
{
static u8 bind_idx =0;

	packet[0] = 0x11;                //Length (17)
	packet[1] = 0x03;                //Packet type
	packet[2] = 0x01;                //Packet type
	packet[3] = frsky_id & 0xff;
	packet[4] = frsky_id >> 8;
	packet[5] = bind_idx *5; // Index into channels_used array.
	packet[6] =  channels_used[ (packet[5]) +0];
	packet[7] =  channels_used[ (packet[5]) +1];
	packet[8] =  channels_used[ (packet[5]) +2];
	packet[9] =  channels_used[ (packet[5]) +3];
	packet[10] = channels_used[ (packet[5]) +4];
	packet[11] = 0x00;
	packet[12] = 0x00;
	packet[13] = 0x00;
	packet[14] = 0x00;
	packet[15] = 0x00;
	packet[16] = 0x00;
	packet[17] = 0x01;

    bind_idx ++;
    if(bind_idx > 9) bind_idx = 0;
}


static void frsky2way_build_data_packet()
{
    packet[0] = 0x11; // Length
    packet[1] = frsky_id & 0xff;
    packet[2] = frsky_id >> 8;
//    packet[3] = packet_number;
    packet[4] = 0x00;
    packet[5] = 0x01;
    // packet 6 to 9 contain LS byte of channels 1 to 4.
    packet[10] = 0; // Low nibble = channel 1, High nibble = channel 2.
    packet[11] = 0;
    // packet 12 to 16 contain LS byte of channels 5 to 8.
    packet[16] = 0;
    packet[17] = 0;

	for(u8 i = 0; i < 8; i++) 
	{
        s32 value;
       	if(i < Model.num_channels)
    	{
    		value = (s32) Channels[i];

#define PPM_SCALING 500L // 500 for Most systems or 550 for Multiplex. e.g. 1500 + 500 us
    		value = (s32)((PPM_SCALING * 15L * value) / (CHAN_MAX_VALUE * 10L)) + 0x08CA;
#undef PPM_SCALING

   		if(value < 0x546) value = 0x546; // 900 uS
   		else if(value > 0xC4E ) value = 0xC4E; // 2100 uS

    	}
    	else value = 0x8ca;
		
		
        if(i < 4) 
		{
            packet[6+i] = value & 0xff;
            packet[10+(i>>1)] |= ((value >> 8) & 0x0f) << (4 *(i & 0x01));
        }
		else
		{
            packet[8+i] = value & 0xff;
            packet[16+((i-4)>>1)] |= ((value >> 8) & 0x0f) << (4 * ((i-4) & 0x01));
        }
	}
}


static u16 frsky2waybind_cb()
{
	CC2500_Strobe(CC2500_SIDLE);
	CC2500_WriteReg(CC2500_0A_CHANNR, 0);
// CC2500_Strobe(CC2500_SCAL); // Manual calibration
	frsky2way_build_bind_packet();
	CC2500_Strobe(CC2500_SFTX); // Flush Tx FIFO
	CC2500_WriteData(packet, 0x12);
	CC2500_Strobe(CC2500_STX); // Tx
	return 18000;
}


static u16 frsky2waydata_cb()
{
static u8 start_tx_rx = 0;
static u8 len;
u8 rx_packet[25]; // should only be 20 (telemetry receive)

	if(! start_tx_rx)
	{
		CC2500_Strobe(CC2500_SIDLE);

		switch(packet_number & 0x03)
		{
		case 0: // Tx prep
			CC2500_SetTxRxMode(TX_EN);
		break;

		case 1: // Tx prep
		case 2: // Tx prep
		break;

		case 3: // Rx prep
			CC2500_SetTxRxMode(RX_EN);
		break;
		}

		CC2500_WriteReg(CC2500_0A_CHANNR, channels_used[packet_number %47]);
		CC2500_Strobe(CC2500_SCAL);    // Manual calibration
		start_tx_rx =1;
		return 1180;
	}
	else
	{
	switch(packet_number & 0x03)
		{
		case 0: // Tx data
		case 2: // Tx data
			CC2500_Strobe(CC2500_SFTX);
			packet[3] = packet_number;
			CC2500_WriteData(packet, 0x12);
			CC2500_Strobe(CC2500_STX);

			frsky2way_build_data_packet();
		break;

		case 1: // Tx data
			CC2500_Strobe(CC2500_SFTX);
			packet[3] = packet_number;
			CC2500_WriteData(packet, 0x12);
			CC2500_Strobe(CC2500_STX);

			// process previous telemetry packet
			len = CC2500_ReadReg(CC2500_3B_RXBYTES | CC2500_READ_BURST);
			if(len != 20) break;
			CC2500_ReadData(rx_packet, len);
			// parse telemetry packet here
			// frsky2way_parse_telem(packet, len);

			// verify packet number txid etc
			if(rx_packet[0] != 0x11) break;
			else if(rx_packet[1] != (frsky_id & 0xff)) break;
			else if(rx_packet[2] != frsky_id >>8) break;

			// Get voltage A1 (52mv/count)
mpx_telemetry[TELEM_MPX_CHANNEL_1].value = (u32) rx_packet[3] * 52 / 100; //In 1/10 of Volts
//			Telemetry.p.frsky.volt[0] = (u32) rx_packet[3] * 52 / 100; //In 1/10 of Volts
//			TELEMETRY_SetUpdated(TELEM_FRSKY_VOLT1);

			// Get voltage A2 (~13.2mv/count) (Docs say 1/4 of A1)
mpx_telemetry[TELEM_MPX_CHANNEL_2].value = (u32) rx_packet[4] * 132 / 1000; //In 1/10 of Volts
//			Telemetry.p.frsky.volt[1] = (u32) rx_packet[4] * 132 / 1000; //In 1/10 of Volts
//			TELEMETRY_SetUpdated(TELEM_FRSKY_VOLT2);

#ifdef UNIMOD
			//Telemetry.p.frsky.rssi = packet[5];
			mpx_telemetry[TELEM_MPX_CHANNEL_0].value = rx_packet[5]; // Appears to be a LQI % value.
			// May flag up a RANGE alarm or LQI alarm if under 50 % - To Do.
			if(rx_packet[5] < 50) mpx_telemetry[TELEM_MPX_CHANNEL_0].alarm = 1;
			else mpx_telemetry[TELEM_MPX_CHANNEL_0].alarm = 0;
#endif //UNIMOD


			break;

		case 3: // Rx data
			CC2500_Strobe(CC2500_SFRX);
			CC2500_Strobe(CC2500_SRX);
			frsky2way_build_data_packet();
			// CC2500_Strobe(CC2500_SNOP); // just shows how long to build packet. AVR = 1.3ms
		break;
		}

	packet_number ++;
	if(packet_number > 187) packet_number =0;
	start_tx_rx =0;
	return 7820;
	}
}


static void initialize(u8 bind)
{
	CLOCK_StopTimer();

	frsky_id = Model.fixed_id % 0x4000; // 0x3210 pour moi
//	frsky_id = 0x25c2;

	// Build channel array.
	channel_offset = frsky_id % 5;
	for(u8 x=0; x<50; x++)	channels_used[x] = get_chan_num(x);

	CC2500_Reset(); // 0x30

	if(bind)
	{
		frsky2way_init(1);
		PROTOCOL_SetBindState(0xFFFFFFFF);
		CLOCK_StartTimer(25000, frsky2waybind_cb);
	}
	else
	{
		frsky2way_init(0);
		frsky2way_build_data_packet();
		// TELEMETRY_SetType(TELEM_FRSKY);
		CLOCK_StartTimer(25000, frsky2waydata_cb);
	}
}


const void * FRSKY2WAY_Cmds(enum ProtoCmds cmd)
{
    switch(cmd) {
        case PROTOCMD_INIT:  initialize(0); return 0;
        case PROTOCMD_CHECK_AUTOBIND: return 0; //Never Autobind
        case PROTOCMD_BIND:  initialize(1); return 0;
        case PROTOCMD_NUMCHAN: return (void *)8L;
        case PROTOCMD_DEFAULT_NUMCHAN: return (void *)8L;
//        case PROTOCMD_CURRENT_ID: return Model.fixed_id ? (void *)((unsigned long)Model.fixed_id) : 0;
        case PROTOCMD_GETOPTIONS:
            return frsky_opts;
//        case PROTOCMD_TELEMETRYSTATE:
//            return (void *)(long)(Model.proto_opts[PROTO_OPTS_TELEM] == TELEM_ON ? PROTO_TELEM_ON : PROTO_TELEM_OFF);
//        case PROTOCMD_RESET:
//        case PROTOCMD_DEINIT:
//            CLOCK_StopTimer();
//            return (void *)(CC2500_Reset() ? 1L : -1L);
        default: break;
    }
    return 0;
}

#endif // PROTO_HAS_CC2500
