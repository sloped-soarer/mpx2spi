

//
 
#include <stddef.h>
#include <stdint.h>
#include <avr/io.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;




#define PROTO_TELEM_UNSUPPORTED -1
#define MODULE_CALLTYPE
//#define UNIMOD 1 // MPX Serial input.
//#define UNIMOD 2 // PPM input capture.

enum PROTO_MODE {
BIND_MODE,
NORMAL_MODE,
RANGE_MODE,
};


enum TxPower {
    TXPOWER_0,
    TXPOWER_1,
    TXPOWER_2,
    TXPOWER_3,
    TXPOWER_4,
    TXPOWER_5,
    TXPOWER_6,
    TXPOWER_7,
};

enum {
    CYRF6936,
    A7105,
    CC2500,
    NRF24L01,
    MULTIMOD,
    TX_MODULE_LAST,
};

/* Protocol */
#define PROTODEF(proto, module, map, init, name) proto,
enum Protocols {
    PROTOCOL_NONE,
    #include "protocol.h"
    PROTOCOL_COUNT,
};
#undef PROTODEF

//extern const u8 *ProtocolChannelMap[PROTOCOL_COUNT];
//extern const char * const ProtocolNames[PROTOCOL_COUNT];

struct Model {
//    char name[24];
//    char icon[24];
//    enum ModelType type;
	enum Protocols protocol;
#define NUM_PROTO_OPTS 4
	s16 proto_opts[NUM_PROTO_OPTS];
    u8 num_channels;
//    u8 num_ppmin;
//   u16 ppmin_centerpw;
//    u16 ppmin_deltapw;
//    u8 train_sw;
//    s8 ppm_map[MAX_PPM_IN_CHANNELS];
    u32 fixed_id;
    enum TxPower tx_power;
//    enum SwashType swash_type;
//    u8 swash_invert;
//    u8 swashmix[3];
//    struct Trim trims[NUM_TRIMS];
//    struct Mixer mixers[NUM_MIXERS];
//    struct Limit limits[NUM_OUT_CHANNELS];
//    char virtname[NUM_VIRT_CHANNELS][VIRT_NAME_LEN];
//    struct Timer timer[NUM_TIMERS];
//    u8 templates[NUM_CHANNELS];
//    struct PageCfg2 pagecfg2;
//    u8 safety[NUM_SOURCES+1];
//    u8 telem_alarm[TELEM_NUM_ALARMS];
//    u16 telem_alarm_val[TELEM_NUM_ALARMS];
//    u8 telem_flags;
//    MixerMode mixer_mode;
//    u32 permanent_timer;
//#if HAS_DATALOG
//    struct datalog datalog;
//#endif
};


extern struct Model Model;

extern volatile uint8_t g_initializing;

struct Transmitter {
    u8 dummy;
};

extern struct Transmitter Transmitter;

// #define NULL ((void*) 0)

#define NUM_OUT_CHANNELS 7

//MAX = +10000
//MIN = -10000
// #define CHAN_MULTIPLIER 100

// For Multiplex M-Link & PPM
#define CHAN_MAX_VALUE (1520L)
#define CHAN_MIN_VALUE (-CHAN_MAX_VALUE)
#define DELTA_PPM_IN 550 // 500 for Most systems or 550 for Multiplex. e.g. 1500 +/- 550 us
//#define CHAN_MAX_VALUE (+1520 * 7)
//#define CHAN_MIN_VALUE (-1520 * 7) // Range of -1521 to +1520.

extern volatile s16 Channels[16];

/* Temproary definition until we have real translation */
#define _tr_noop(x) x
#ifdef NO_LANGUAGE_SUPPORT
#define _tr(x) x
#else
const char *_tr(const char *str);
#endif

#define MICRO_SEC_CONVERT(uS) (((F_CPU/800)*(uS))/10000)
#define COUNTS_PER_MILLI_SEC (F_CPU/128000L)-1 // value should == 1ms based on F_CPU.

#define gpio_set(sfr, bit)   (sfr) |=   1<<(bit)
#define gpio_clear(sfr, bit) (sfr) &= ~(1<<(bit))

//Port C
#define SPI_CS			3
#define CH_ORD          4
#define	BIND_SW			5

//Port B
#define PPM_IN			0
#define DEBUG_1		    1
#define DEBUG_2     	2

#define CS_HI() 		gpio_set(PORTC, SPI_CS)
#define CS_LO() 		gpio_clear(PORTC, SPI_CS) 

//Port D
#define LED_G 2  // LED green.
#define LED_R 3  // LED red.
#define LED_Y 4  // LED yellow.
#define LED_O 5  // LED orange.
#define RF_EN 7  // MPX RF Enable signal.

#define LED_ON(colour)	gpio_set(PORTD, colour)
#define LED_OFF(colour)	gpio_clear(PORTD, colour)
#define LED_TOGGLE(colour) gpio_set(PIND, colour) //toggle LED.
#define RF_EN_STATE()	gpio_get(GPIOD, RF_EN)

