#pragma once

enum {
	DCS_2K = 0,
	DCS_2KU = 1, // UART
	DCS_8K = 2
};

#define MHz(x)  (x * 1000000)
#define kHz(x)  (x * 1000)

void Dcs2kInit(INT32 dtype, INT32 dmhz);
void Dcs2kExit();
void Dcs2kRun(INT32 cycles);
INT32 Dcs2kScan(INT32 nAction, INT32 *pnMin);
void Dcs2kMapSoundROM(void *ptr, INT32 size);
void Dcs2kSetVolume(double vol);
void Dcs2kBoot();
void Dcs2kDataWrite(INT32 data);
INT32 Dcs2kDataRead();
INT32 Dcs2kControlRead();
void Dcs2kResetWrite(INT32 data);
void Dcs2kRender(INT16 *pSoundBuf, INT32 nSegmentLength);
void Dcs2kReset();
void Dcs2kNewFrame();
UINT32 Dcs2kTotalCycles();
void DcsIRQ();
