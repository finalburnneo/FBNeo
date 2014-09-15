
UINT16 pic16c5xFetch(UINT16 address);
UINT8 pic16c5xRead(UINT16 address);
void pic16c5xWrite(UINT16 address, UINT8 data);
UINT8 pic16c5xReadPort(UINT16 port);
void pic16c5xWritePort(UINT16 port, UINT8 data);

void pic16c5xSetWritePortHandler(void (*write)(UINT16 port, UINT8 data));
void pic16c5xSetReadPortHandler(UINT8 (*read)(UINT16 port));

void pic16c5xInit(INT32 type, UINT8 *mem);
void pic16c5xReset();
void pic16c5xOpen(INT32);
INT32 pic16c5xRun(INT32 cycles);
void pic16c5xClose();
void pic16c5xExit();

INT32 pic16c5xScan(INT32 nAction);

void pic16c5xRunEnd();

// load & convert pic rom
INT32 BurnLoadPicROM(UINT8 *src, INT32 offset, INT32 len);
