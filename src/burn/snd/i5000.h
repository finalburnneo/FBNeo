void i5000sndInit(UINT8 *rom, INT32 clock, INT32 length);
void i5000sndExit();
void i5000sndReset();
void i5000sndUpdate(INT16 *output, INT32 length);
void i5000sndWrite(INT32 offset, UINT16 data);
UINT16 i5000sndRead(INT32 offset);
void i5000sndScan(INT32 nAction, INT32 *pnMin);
