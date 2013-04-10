
UINT16 pic16c5x_read_op(UINT16 address);
UINT8 pic16c5x_read_byte(UINT16 address);
void pic16c5x_write_byte(UINT16 address, UINT8 data);
UINT8 pic16c5x_read_port(UINT16 port);
void pic16c5x_write_port(UINT16 port, UINT8 data);

extern UINT8 (*pPic16c5xReadPort)(UINT16 port);
extern void (*pPic16c5xWritePort)(UINT16 port, UINT8 data);

extern INT32 pic16c5xRun(INT32 cycles);
void pic16c5xReset();
void pic16c5xExit();
void pic16c5xInit(INT32 type, UINT8 *mem);

extern INT32 pic16c5xScan(INT32 nAction, INT32* pnMin);
extern void pic16c5xRunEnd();

INT32 BurnLoadPicROM(UINT8 *src, INT32 offset, INT32 len);
