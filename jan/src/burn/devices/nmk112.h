void NMK112_okibank_write(INT32 offset, UINT8 data);
void NMK112Reset();
void NMK112_init(UINT8 disable_page_mask, UINT8 *region0, UINT8 *region1, INT32 len0, INT32 len1);
INT32 NMK112_Scan(INT32 nAction);
