extern UINT8 *st0020BlitRAM;	// 0x100 bytes
extern UINT8 *st0020SprRAM;	// 0x80000 bytes
extern UINT8 *st0020GfxRAM;	// 0x400000 bytes
extern UINT8 *st0020GfxROM;
extern INT32 st0020GfxROMLen;

UINT16 st0020GfxramReadWord(UINT32 offset);
UINT8 st0020GfxramReadByte(UINT32 offset);
void st0020GfxramWriteByte(UINT32 offset, UINT8 data);
void st0020GfxramWriteWord(UINT32 offset, UINT16 data);

void st0020_blitram_write_word(UINT32 offset, UINT16 data);
void st0020_blitram_write_byte(UINT32 offset, UINT8 data);
UINT16 st0020_blitram_read_word(UINT32 offset);
void st0020Draw();
