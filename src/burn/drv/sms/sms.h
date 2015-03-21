
#ifndef _SMS_H_
#define _SMS_H_

enum {
    SLOT_BIOS   = 0,
    SLOT_CARD   = 1,
    SLOT_CART   = 2,
    SLOT_EXP    = 3
};

enum {
	MAPPER_NONE         = 0,
	MAPPER_SEGA         = 1,
	MAPPER_CODIES       = 2,
	MAPPER_MSX          = 3,
	MAPPER_MSX_NEMESIS  = 4,
	MAPPER_KOREA        = 5,
	MAPPER_KOREA8K      = 6,
	MAPPER_4PAK         = 7,
	MAPPER_XIN1         = 8
};

enum {
    DISPLAY_NTSC        = 0,
    DISPLAY_PAL         = 1
};

enum {
    FPS_NTSC        = 60,
    FPS_PAL         = 50
};

enum {
    CLOCK_NTSC        = 3579545,
    CLOCK_PAL         = 3579545
};

enum {
    CONSOLE_SMS         = 0x20,
    CONSOLE_SMSJ        = 0x21,
    CONSOLE_SMS2        = 0x22,

    CONSOLE_GG          = 0x40,
    CONSOLE_GGMS        = 0x41,

    CONSOLE_MD          = 0x80,
    CONSOLE_MDPBC       = 0x81,
    CONSOLE_GEN         = 0x82,
    CONSOLE_GENPBC      = 0x83
};

#define HWTYPE_SMS  CONSOLE_SMS
#define HWTYPE_GG   CONSOLE_GG
#define HWTYPE_MD   CONSOLE_MD

#define IS_SMS      (sms.console & HWTYPE_SMS)
#define IS_GG       (sms.console & HWTYPE_GG)
#define IS_MD       (sms.console & HWTYPE_MD)

enum {
    TERRITORY_DOMESTIC  = 0,
    TERRITORY_EXPORT    = 1
};

/* SMS context */
typedef struct
{
    UINT8 wram[0x2000];
    UINT8 paused;
    UINT8 save;
    UINT8 territory;
    UINT8 console;
    UINT8 display;
    UINT8 fm_detect;
    UINT8 use_fm;
    UINT8 memctrl;
    UINT8 ioctrl;
    struct {
        UINT8 pdr;      /* Parallel data register */
        UINT8 ddr;      /* Data direction register */
        UINT8 txdata;   /* Transmit data buffer */
        UINT8 rxdata;   /* Receive data buffer */
        UINT8 sctrl;    /* Serial mode control and status */
    } sio;
    struct {
        INT32 type;
    } device[2];
} sms_t;

/* Global data */
extern sms_t sms;

/* Function prototypes */
void sms_init(void);
void sms_reset(void);
void sms_shutdown(void);
void sms_mapper_w(INT32 address, UINT8 data);
void sms_mapper8k_w(INT32 address, UINT8 data);
void sms_mapper8kvirt_w(INT32 address, UINT8 data);

/* port-map Function prototypes */
UINT8 z80_read_unmapped(void);
void __fastcall gg_port_w(UINT16 port, UINT8 data);
UINT8 __fastcall gg_port_r(UINT16 port);
void __fastcall ggms_port_w(UINT16 port, UINT8 data);
UINT8 __fastcall ggms_port_r(UINT16 port);
void __fastcall sms_port_w(UINT16 port, UINT8 data);
UINT8 __fastcall sms_port_r(UINT16 port);
#endif /* _SMS_H_ */
