void mb88xxSetSerialReadWriteFunction(void (*write)(INT32 line), INT32 (*read)());
void mb88xxSetReadWriteFunction(void (*write)(UINT8), UINT8 (*read)(), char select, INT32 which);

void mb88xxInit(INT32 nCPU, INT32 nType, UINT8 *rom);
void mb88xxExit();
void mb88xxReset();
void mb88xxSetRESETLine(INT32 status);
void mb88xxSetIRQLine(int /*inputnum*/, int state);
INT32 mb88xxRun(INT32 nCycles);
INT32 mb88xxIdle(INT32 cycles);

void mb88xxScan(INT32 nAction);
void mb88xxNewFrame();
INT32 mb88xxTotalCycles();

void mb88xx_clock_write(INT32 state);
void mb88xx_set_pla(UINT8 *pla);

