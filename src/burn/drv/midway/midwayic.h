#ifndef SERIALPIC
#define SERIALPIC

void MidwaySerialPicInit(int upper);
void MidwaySerialPicReset();
UINT32 MidwaySerialPicStatus();
UINT8 MidwaySerialPicRead();
void MidwaySerialPicWrite(UINT8 data);
void MidwaySerialPicScan(INT32 nAction, INT32 *pnMin);

#endif // SERIALPIC

