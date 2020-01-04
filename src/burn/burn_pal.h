// Common RAM-based palette decoding functions

// point these to destination palette and to palette ram

extern UINT32 *BurnPalette;
extern UINT8 *BurnPalRAM;

// palette update functions are called to recalculate the entire palette

void BurnPaletteUpdate_xxxxBBBBRRRRGGGG();
void BurnPaletteUpdate_xxxxBBBBGGGGRRRR();
void BurnPaletteUpdate_xxxxRRRRGGGGBBBB();
void BurnPaletteUpdate_xRRRRRGGGGGBBBBB();
void BurnPaletteUpdate_xBBBBBGGGGGRRRRR();
void BurnPaletteUpdate_xGGGGGBBBBBRRRRR();
void BurnPaletteUpdate_xGGGGGRRRRRBBBBB();
void BurnPaletteUpdate_GGGGGRRRRRBBBBBx();
void BurnPaletteUpdate_RRRRGGGGBBBBRGBx();
void BurnPaletteUpdate_BBGGGRRR();
void BurnPaletteUpdate_RRRGGGBB();
void BurnPaletteUpdate_BBGGGRRR_inverted();
void BurnPaletteUpdate_RRRGGGBB_inverted();

// palette write functions called to write single palette entry
// note that the offset should not be shifted, only masked for palette size

void BurnPaletteWrite_xxxxBBBBRRRRGGGG(INT32 offset);
void BurnPaletteWrite_xxxxBBBBGGGGRRRR(INT32 offset);
void BurnPaletteWrite_xxxxRRRRGGGGBBBB(INT32 offset);
void BurnPaletteWrite_xRRRRRGGGGGBBBBB(INT32 offset);
void BurnPaletteWrite_xBBBBBGGGGGRRRRR(INT32 offset);
void BurnPaletteWrite_xGGGGGBBBBBRRRRR(INT32 offset);
void BurnPaletteWrite_xGGGGGRRRRRBBBBB(INT32 offset);
void BurnPaletteWrite_GGGGGRRRRRBBBBBx(INT32 offset);
void BurnPaletteWrite_RRRRGGGGBBBBRGBx(INT32 offset);
void BurnPaletteWrite_BBGGGRRR(INT32 offset);
void BurnPaletteWrite_RRRGGGBB(INT32 offset);
void BurnPaletteWrite_BBGGGRRR_inverted(INT32 offset);
void BurnPaletteWrite_RRRGGGBB_inverted(INT32 offset);

// palette expansion macros

#define pal5bit(x)	((((x) & 0x1f)<<3)|(((x) & 0x1f) >> 2))
#define pal4bit(x)	((((x) & 0x0f)<<4)|(((x) & 0x0f) << 4))
#define pal3bit(x)	((((x) & 0x07)<<5)|(((x) & 0x07) << 2)|(((x) & 0x07) >> 1))
#define pal2bit(x)	((((x) & 0x03)<<6)|(((x) & 0x03) << 4)|(((x) & 0x03) << 2) | ((x) & 0x03))
#define pal1bit(x)	(((x) & 1) ? 0xff : 0)
