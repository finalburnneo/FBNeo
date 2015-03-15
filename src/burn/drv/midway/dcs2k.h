#pragma once


void Dcs2kInit();
void Dcs2kExit();
void Dcs2kRun(int cycles);
void Dcs2kMapSoundROM(void *ptr, int size);
void Dcs2kBoot();
void Dcs2kDataWrite(int data);
int Dcs2kDataRead();
void Dcs2kResetWrite(int data);
void Dcs2kRender(INT16 *pSoundBuf, INT32 nSegmentLenght);
void Dcs2kReset();
