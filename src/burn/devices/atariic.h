void AtariEEPROMUnlockWrite();
void AtariEEPROMReset();
void AtariEEPROMInit(INT32 size);
void AtariEEPROMInstallMap(INT32 map_handler, UINT32 address_start, UINT32 address_end);
void AtariEEPROMLoad(UINT8 *src);
void AtariEEPROMExit();
INT32 AtariEEPROMScan(INT32 nAction, INT32 *);


void AtariSlapsticInit(UINT8 *rom, INT32 slapsticnum);
void AtariSlapsticReset();
void AtariSlapsticInstallMap(INT32 map_handler, UINT32 address_start);
void AtariSlapsticExit();
INT32 AtariSlapsticScan(INT32 nAction, INT32 *);


void AtariPaletteUpdateIRGB(UINT8 *ram, UINT32 *palette, INT32 ramsize);
void AtariPaletteWriteIRGB(INT32 offset, UINT8 *ram, UINT32 *palette);
void AtariPaletteUpdate4IRGB(UINT8 *ram, UINT32 *palette, INT32 ramsize);
