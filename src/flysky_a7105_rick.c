// Rick

#ifdef MODULAR
  //Allows the linker to properly relocate
  #define FLYSKY_Cmds PROTO_Cmds
  #pragma long_calls
#endif

#include "common.h"
#include "interface.h"
#include "misc.h"

//#include "mixer.h"
//#include "config/model.h"
//#include "config/tx.h"


#ifdef MODULAR
  #pragma long_calls_off
  extern unsigned _data_loadaddr;
  const unsigned long protocol_type = (unsigned long)&_data_loadaddr;
#endif

#ifdef PROTO_HAS_A7105

//Fewer bind packets in the emulator so we can get right to the important bits
#ifdef EMULATOR
#define BIND_COUNT 3
#else
#define BIND_COUNT 2500
#endif

static const char * const flysky_opts[] = {
  "WLToys V9x9",  _tr_noop("Off"), _tr_noop("On"), NULL,
  NULL
};
enum {
    PROTOOPTS_WLTOYS = 0,
    LAST_PROTO_OPT,
};
//ctassert(LAST_PROTO_OPT <= NUM_PROTO_OPTS, too_many_protocol_opts);

#define WLTOYS_ON 1
#define WLTOYS_OFF 0

// Following checked & correct with FS-TM002 module.
// Added 0x24 == 0x13 // VCO Current calibration.
// Added 0x25 == 0x09 // VCO Bank calibration.
/*
static const u8 A7105_regs[] =
{
     -1,  0x42, 0x00, 0x14, 0x00,  -1 ,  -1 , 0x00, 0x00, 0x00, 0x00, 0x01, 0x21, 0x05, 0x00, 0x50,
    0x9e, 0x4b, 0x00, 0x02, 0x16, 0x2b, 0x12, 0x00, 0x62, 0x80, 0x80, 0x00, 0x0a, 0x32, 0xc3, 0x0f,
    0x13, 0xc3, 0x00,  -1,  0x13, 0x09, 0x3b, 0x00, 0x17, 0x47, 0x80, 0x03, 0x01, 0x45, 0x18, 0x00,
    0x01, 0x0F,  -1,
};
*/

static const u8 tx_channels[16][16] =
{
  { 10, 90, 20,100, 30,110, 40,120, 50,130, 60,140, 70,150, 80,160},
  {160, 80,150, 70,140, 60,130, 50,120, 40,110, 30,100, 20, 90, 10},
  { 10, 90, 80,160, 20,100, 70,150, 30,110, 60,140, 40,120, 50,130},
  {130, 50,120, 40,140, 60,110, 30,150, 70,100, 20,160, 80, 90, 10},
  { 40,120, 10, 90, 80,160, 20,100, 30,110, 60,140, 50,130, 70,150},
  {150, 70,130, 50,140, 60,110, 30,100, 20,160, 80, 90, 10,120, 40},
  { 80,160, 40,120, 10, 90, 30,110, 60,140, 50,130, 70,150, 20,100},
  {100, 20,150, 70,130, 50,140, 60,110, 30, 90, 10,120, 40,160, 80},
  { 80,160, 70,150, 60,140, 40,120, 10, 90, 50,130, 30,110, 20,100},
  {100, 20,110, 30,130, 50, 90, 10,120, 40,140, 60,150, 70,160, 80},
  { 70,150, 60,140, 80,160, 40,120, 10, 90, 30,110, 50,130, 20,100},
  {100, 20,130, 50,110, 30, 90, 10,120, 40,160, 80,140, 60,150, 70},
  { 70,150, 10, 90, 60,140, 20,100, 80,160, 40,120, 30,110, 50,130},
  {130, 50,110, 30,120, 40,160, 80,100, 20,140, 60, 90, 10,150, 70},
  { 70,150, 10, 90, 80,160, 60,140, 40,120, 30,110, 50,130, 20,100},
  {100, 20,130, 50,110, 30,120, 40,140, 60,160, 80, 90, 10,150, 70},
};

static u32 id;
//static const u8 id[] = { 0x02, 0x00, 0x00, 0x70 };
static u8 chanrow;
static u8 chancol;
static u8 chanoffset;
static u8 packet[21];
static u16 counter;


static int flysky_init()
{
    u8 if_calibration1;
//    u8 vco_calibration0;
//    u8 vco_calibration1;

    A7105_WriteID(0x5475c52a);
/*
    for (int i = 0; i < 0x33; i++)
        if((s8)A7105_regs[i] != -1)
            A7105_WriteReg(i, A7105_regs[i]);
*/
    //0
    A7105_WriteReg(A7105_01_MODE_CONTROL, 0x42);
    A7105_WriteReg(A7105_02_CALC,         0x00);
    A7105_WriteReg(A7105_03_FIFOI,        0x14);
    A7105_WriteReg(A7105_04_FIFOII,       0x00);
    //5
    //6
    A7105_WriteReg(A7105_07_RC_OSC_I,     0x00);
    A7105_WriteReg(A7105_08_RC_OSC_II,    0x00);
    A7105_WriteReg(A7105_09_RC_OSC_III,   0x00);
    A7105_WriteReg(A7105_0A_CK0_PIN,      0x00);
    A7105_WriteReg(A7105_0B_GPIO1_PIN1,   0x01);
    A7105_WriteReg(A7105_0C_GPIO2_PIN_II, 0x21);
    A7105_WriteReg(A7105_0D_CLOCK,        0x05);
    A7105_WriteReg(A7105_0E_DATA_RATE,    0x00);
    A7105_WriteReg(A7105_0F_PLL_I,        0x50); // Channel Number.

    A7105_WriteReg(A7105_10_PLL_II,       0x9E);
    A7105_WriteReg(A7105_11_PLL_III,      0x4B);
    A7105_WriteReg(A7105_12_PLL_IV,       0x00);
    A7105_WriteReg(A7105_13_PLL_V,        0x02);
    A7105_WriteReg(A7105_14_TX_I,         0x16);
    A7105_WriteReg(A7105_15_TX_II,        0x2B);
    A7105_WriteReg(A7105_16_DELAY_I,      0x12);
    A7105_WriteReg(A7105_17_DELAY_II,     0x00);
    A7105_WriteReg(A7105_18_RX,           0x62);
    A7105_WriteReg(A7105_19_RX_GAIN_I,    0x80);
    A7105_WriteReg(A7105_1A_RX_GAIN_II,   0x80);
    A7105_WriteReg(A7105_1B_RX_GAIN_III,  0x00);
    A7105_WriteReg(A7105_1C_RX_GAIN_IV,   0x0A);
    A7105_WriteReg(A7105_1D_RSSI_THOLD,   0x32);
    A7105_WriteReg(A7105_1E_ADC,          0xC3);
    A7105_WriteReg(A7105_1F_CODE_I,       0x0F);

    A7105_WriteReg(A7105_20_CODE_II,      0x13);
    A7105_WriteReg(A7105_21_CODE_III,     0xC3);
    A7105_WriteReg(A7105_22_IF_CALIB_I,   0x00);
    //23
    A7105_WriteReg(A7105_24_VCO_CURCAL,     0x13);
    A7105_WriteReg(A7105_25_VCO_SBCAL_I,    0x09);
    A7105_WriteReg(A7105_26_VCO_SBCAL_II,   0x3B);
    A7105_WriteReg(A7105_27_BATTERY_DET,    0x00);
    A7105_WriteReg(A7105_28_TX_TEST,        0x17);
    A7105_WriteReg(A7105_29_RX_DEM_TEST_I,  0x47);
    A7105_WriteReg(A7105_2A_RX_DEM_TEST_II, 0x80);
    A7105_WriteReg(A7105_2B_CPC,            0x03);
    A7105_WriteReg(A7105_2C_XTAL_TEST,      0x01);
    A7105_WriteReg(A7105_2D_PLL_TEST,       0x45);
    A7105_WriteReg(A7105_2E_VCO_TEST_I,     0x18);
    A7105_WriteReg(A7105_2F_VCO_TEST_II,    0x00);

    A7105_WriteReg(A7105_30_IFAT,         0x01);
    A7105_WriteReg(A7105_31_RSCALE,       0x0F);
    //32

//  A7105_Strobe(A7105_STANDBY); //rick
    A7105_Strobe(A7105_PLL); // VCO Bank Calibration must be done in PLL mode. rick.

	// IF Filter Bank Calibration
#define FBC_FLAG 0x01
    A7105_WriteReg(0x02, FBC_FLAG); // Set FBC.
    u32 ms = CLOCK_getms();
//	CLOCK_ResetWatchdog();
    while(CLOCK_getms()  - ms < 500)
    {
    if(! (A7105_ReadReg(0x02) & FBC_FLAG)) break; // Zero when calibration is complete.
#undef FBC_FLAG
    }
    if (CLOCK_getms() - ms >= 500) return 0;

    if_calibration1 = A7105_ReadReg(0x22);
    if(if_calibration1 & A7105_MASK_FBCF) return 0;
    //Calibration failed...what do we do?


    // VCO Current Calibration
//	A7105_WriteReg(0x24, 0x13); //Recommended calibration from A7105 Datasheet - in startup regs.

    // VCO Bank Calibration
//	A7105_WriteReg(0x26, 0x3b); //Recommended limits from A7105 Datasheet - in startup regs.

    // Manual VCO Bank calibration
//    A7105_WriteReg(0x25, 0x09); // - in startup regs.

    A7105_SetTxRxMode(TX_EN);
    A7105_SetPower(Model.tx_power);

    A7105_Strobe(A7105_STANDBY);
    return 1;
}


static void flysky_build_packet(u8 init)
{
    packet[0] = init ? 0xaa : 0x55; // packet type 0x55 is normal, 0xAA is bind packet.
    packet[1] = (id >>  0) & 0xff;
    packet[2] = (id >>  8) & 0xff;
    packet[3] = (id >> 16) & 0xff;
    packet[4] = (id >> 24) & 0xff;

    // Value = pulse length in uS
    for (int i = 0; i < 8; i++)
    {
    	if(i < Model.num_channels)
    	{
    		s32 value = (s32) Channels[i];
   	    // value = servo pulse length in uS

    	#define PPM_SCALING 500L // 500 for Most systems or 550 for Multiplex. e.g. 1500 + 500 us
    		value = (s32) ((PPM_SCALING * value) / CHAN_MAX_VALUE) + 1500L;
		#undef PPM_SCALING

		if(value < 900) value = 900;
		else if(value > 2100) value = 2100;
		packet[5 + i*2] = value & 0xff;
		packet[6 + i*2] = (value >> 8) & 0xff;
    	}
    	else
    	{
    		packet[5 + i*2] = 0xDC;
    		packet[6 + i*2] = 0x05;
    	}
    }

    if (Model.proto_opts[PROTOOPTS_WLTOYS] == WLTOYS_ON)
    {
        if(Channels[4] > 0)
            packet[12] |= 0x20; // Bit 13 of the rudder channel - JP Twister Quad LEDS
        if(Channels[5] > 0)
            packet[10] |= 0x40;  // Bit 14 of the throttle channel will toggle the accessory (video camera)
        if(Channels[6] > 0)
            packet[10] |= 0x80;  // Bit 15 of the throttle channel will toggle the accessory (still camera)
        if(Channels[7] > 0)
            packet[12] |= 0x10;

    }
}


MODULE_CALLTYPE
static u16 flysky_cb()
{
    if (counter) {
        flysky_build_packet(1);
        A7105_WriteData(packet, 21, 1);
        counter--;
        if (! counter)
            PROTOCOL_SetBindState(0);
    }
    else {
        flysky_build_packet(0);
        A7105_WriteData(packet, 21, tx_channels[chanrow][chancol]-chanoffset);
        chancol = (chancol + 1) % 16;
        if (! chancol) //Keep transmit power updated
            A7105_SetPower(Model.tx_power);
    }
    return 1500;
}


static void initialize(u8 bind)
{
    CLOCK_StopTimer();
    while(1) {
        A7105_Reset();
//        CLOCK_ResetWatchdog();
        if (flysky_init())
            break;
    }
    if (Model.fixed_id) {
        id = Model.fixed_id;
    } else {
        id = (Crc(&Model, sizeof(Model)) + Crc(&Transmitter, sizeof(Transmitter))) % 999999;
    }
    chanrow = id % 16;
    chancol = 0;
    chanoffset = (id & 0xff) / 16;
    if(chanoffset > 9) chanoffset = 9; //rick
    if (bind || ! Model.fixed_id) {
        counter = BIND_COUNT;
        PROTOCOL_SetBindState((u32) BIND_COUNT * 1500 / 1000); //msec
    } else {
        counter = 0;
    }
    CLOCK_StartTimer(25000, &flysky_cb);
}


const void * FLYSKY_Cmds(enum ProtoCmds cmd)
{
    switch(cmd) {
        case PROTOCMD_INIT:  initialize(0); return 0;
//        case PROTOCMD_DEINIT:
//        case PROTOCMD_RESET:
//            CLOCK_StopTimer();
//            return (void *)(A7105_Reset() ? 1L : -1L);
        case PROTOCMD_CHECK_AUTOBIND: return Model.fixed_id ? 0 : (void *)1L;
        case PROTOCMD_BIND:  initialize(1); return 0;
        case PROTOCMD_NUMCHAN: return (void *)8L;
        case PROTOCMD_DEFAULT_NUMCHAN: return (void *)8L;
//      case PROTOCMD_CURRENT_ID: return (void *)((unsigned long)id);
        case PROTOCMD_GETOPTIONS:
            return flysky_opts;
        case PROTOCMD_TELEMETRYSTATE: return (void *)(long)PROTO_TELEM_UNSUPPORTED;
        default: break;
    }
    return 0;
}
#endif
