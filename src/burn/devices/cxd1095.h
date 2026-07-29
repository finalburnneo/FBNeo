

UINT8 cxd1095_read(INT32 chip, UINT8 offset);
void cxd1095_write(INT32 chip, UINT8 offset, UINT8 data);

void cxd1095Reset();
void cxd1095Init(INT32 chip, void (*write_cb)(UINT8,UINT8), UINT8 (*read_cb)(UINT8));
INT32 cxd1095Scan(INT32 nAction);

