extern INT32 atarivad_scanline_timer;
extern INT32 atarivad_scanline_timer_enabled;
extern INT32 atarivad_scanline; // external

void AtariVADReset();
void AtariVADInit(INT32 tmap_num0, INT32 tmap_num1, INT32 bgtype, void (*sl_timer_cb)(INT32), void (*palette_write)(INT32 offset, UINT16 data));
void AtariVADExit();
void AtariVADMap(INT32 startaddress, INT32 endaddress, INT32 shuuz);
INT32 AtariVADScan(INT32 nAction, INT32 *pnMin);
void AtariVADSetPartialCB(void (*partial_cb)(INT32));

void AtariVADEOFUpdate(UINT16 *eof_data); // call after last scanline
void AtariVADTimerUpdate(); // call after each scanline
void AtariVADTileRowUpdate(INT32 scanline, UINT16 *alphamap_ram); // each scanline, only if alpha map is present!
void AtariVADDraw(UINT16 *pDestDraw, INT32 use_categories);
void AtariVADRecalcPalette();
