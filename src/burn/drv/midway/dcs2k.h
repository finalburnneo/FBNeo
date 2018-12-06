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
void Dcs2kRun(int cycles);
void Dcs2kMapSoundROM(void *ptr, int size);
void Dcs2kSetVolume(double vol);
void Dcs2kBoot();
void Dcs2kDataWrite(int data);
int Dcs2kDataRead();
int Dcs2kControlRead();
void Dcs2kResetWrite(int data);
void Dcs2kRender(INT16 *pSoundBuf, INT32 nSegmentLength);
void Dcs2kReset();
void Dcs2kNewFrame();
UINT32 Dcs2kTotalCycles();
void DcsIRQ();
