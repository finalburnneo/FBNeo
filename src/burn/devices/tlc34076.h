void tlc34076_reset(INT32 dacwidth);
UINT8 tlc34076_read(UINT32 offset);
void tlc34076_write(UINT32 offset, UINT8 data);
UINT8 tlc34076_read16(UINT32 address);
void tlc34076_write16(UINT32 address, UINT16 data);

void tlc34076_recalc_palette();

INT32 tlc34076_Scan(INT32 nAction);
