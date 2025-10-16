#define SEGA_MD_ROM_LOAD_NORMAL										0x10
#define SEGA_MD_ROM_LOAD16_WORD_SWAP								0x20
#define SEGA_MD_ROM_LOAD16_BYTE										0x30
#define SEGA_MD_ROM_LOAD16_WORD_SWAP_CONTINUE_040000_100000			0x40
#define SEGA_MD_ROM_LOAD_NORMAL_CONTINUE_020000_080000				0x50
#define SEGA_MD_ROM_OFFS_000000										0x01
#define SEGA_MD_ROM_OFFS_000001										0x02
#define SEGA_MD_ROM_OFFS_020000										0x03
#define SEGA_MD_ROM_OFFS_080000										0x04
#define SEGA_MD_ROM_OFFS_100000										0x05
#define SEGA_MD_ROM_OFFS_100001										0x06
#define SEGA_MD_ROM_OFFS_200000										0x07
#define SEGA_MD_ROM_OFFS_300000										0x08
#define SEGA_MD_ROM_RELOAD_200000_200000							0x09
#define SEGA_MD_ROM_RELOAD_100000_300000							0x0a
#define SEGA_MD_ARCADE_SUNMIXING									(0x4000)

extern UINT8 MegadriveUnmappedRom;
extern UINT8 MegadriveReset;
extern UINT8 bMegadriveRecalcPalette;
extern UINT8 MegadriveJoy1[12];
extern UINT8 MegadriveJoy2[12];
extern UINT8 MegadriveJoy3[12];
extern UINT8 MegadriveJoy4[12];
extern UINT8 MegadriveJoy5[12];
extern UINT8 MegadriveDIP[3];
extern INT16 MegadriveAnalog[4];

INT32 MegadriveInit();
INT32 MegadriveInitNoDebug();
INT32 MegadriveInitPaprium();
INT32 MegadriveInitPsolar();
INT32 MegadriveInitSot4w();
INT32 MegadriveInitColocodx();

INT32 MegadriveExit();
INT32 MegadriveFrame();
INT32 MegadriveScan(INT32 nAction, INT32 *pnMin);
INT32 MegadriveDraw();

void MegadriveLightGunOffsets(INT32 x, INT32 y, bool reticle = true);

// pier solar
void md_eeprom_stm95_reset();
void md_eeprom_stm95_init(UINT8 *rom);
void md_eeprom_stm95_scan(INT32 nAction);
UINT16 md_psolar_rw(UINT32 offset);
