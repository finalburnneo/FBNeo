#include "cps.h"
#include "burn_ym2203.h"
#include "msm5205.h"

static UINT8 *FcrashZ80Ram = NULL;
static INT32 FcrashZ80BankAddress = 0;
static INT32 FcrashSoundLatch = 0;
static INT32 FcrashMSM5205Interleave = 0;
static INT32 FcrashSampleBuffer1 = 0;
static INT32 FcrashSampleBuffer2 = 0;
static INT32 FcrashSampleSelect1 = 0;
static INT32 FcrashSampleSelect2 = 0;

void FrcashSoundCommand(UINT16 d)
{
	if (d & 0xff) {
		INT32 nCyclesToDo = (INT64)SekTotalCycles() * nCpsZ80Cycles / nCpsCycles;
		INT32 nFramePortion = (INT64)FcrashMSM5205Interleave * nCyclesToDo / nCpsZ80Cycles;
		INT32 nCyclesPerPortion = nCyclesToDo / nFramePortion;
		
		for (INT32 i = 0; i < nFramePortion; i++) {
			BurnTimerUpdate((i + 1) * nCyclesPerPortion);
			MSM5205Update();
		}

		FcrashSoundLatch = d & 0xff;
		ZetSetIRQLine(0, ZET_IRQSTATUS_ACK);
	}
}

UINT8 __fastcall FcrashZ80Read(UINT16 a)
{
	switch (a) {
		case 0xd800: {
			return BurnYM2203Read(0, 0);
		}
		
		case 0xdc00: {
			return BurnYM2203Read(1, 0);
		}
		
		case 0xe400: {
			ZetSetIRQLine(0, ZET_IRQSTATUS_NONE);
			return FcrashSoundLatch;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Read => %04X\n"), a);
		}
	}

	return 0;
}

void __fastcall FcrashZ80Write(UINT16 a, UINT8 d)
{
	switch (a) {
		case 0xd800: {
			BurnYM2203Write(0, 0, d);
			return;
		}
		
		case 0xd801: {
			BurnYM2203Write(0, 1, d);
			return;
		}
		
		case 0xdc00: {
			BurnYM2203Write(1, 0, d);
			return;
		}
		
		case 0xdc01: {
			BurnYM2203Write(1, 1, d);
			return;
		}
		
		case 0xe000: {
			MSM5205SetVolume(0, (d & 0x08) ? 0 : 30);
			MSM5205SetVolume(1, (d & 0x10) ? 0 : 30);

			FcrashZ80BankAddress = (d & 0x07) * 0x4000;
			ZetMapArea(0x8000, 0xbfff, 0, CpsZRom + FcrashZ80BankAddress);
			ZetMapArea(0x8000, 0xbfff, 2, CpsZRom + FcrashZ80BankAddress);
			return;
		}
		
		case 0xe800: {
			FcrashSampleBuffer1 = d;
			return;
		}
		
		case 0xec00: {
			FcrashSampleBuffer2 = d;
			return;
		}
		
		case 0xf002:
		case 0xf004:
		case 0xf006: {
			// ???
			return;
		}
		
		default: {
			bprintf(PRINT_NORMAL, _T("Z80 #1 Write => %04X, %02X\n"), a, d);
		}
	}
}

inline static INT32 FcrashSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(ZetTotalCycles() * nSoundRate / (24000000 / 6));
}

inline static double FcrashGetTime()
{
	return (double)ZetTotalCycles() / (24000000 / 6);
}

static void FcrashMSM5205Vck0()
{
	MSM5205DataWrite(0, FcrashSampleBuffer1 & 0x0f);
	FcrashSampleBuffer1 >>= 4;
	FcrashSampleSelect1 ^= 1;
	if (FcrashSampleSelect1 == 0) {
		ZetNmi();
	}
}

static void FcrashMSM5205Vck1()
{
	MSM5205DataWrite(1, FcrashSampleBuffer2 & 0x0f);
	FcrashSampleBuffer2 >>= 4;
	FcrashSampleSelect2 ^= 1;
}

INT32 FcrashSoundInit()
{
	FcrashZ80Ram = (UINT8*)BurnMalloc(0x800);
	
	ZetInit(0);
	ZetOpen(0);
	ZetSetReadHandler(FcrashZ80Read);
	ZetSetWriteHandler(FcrashZ80Write);
	ZetMapArea(0x0000, 0x7fff, 0, CpsZRom + 0x00000);
	ZetMapArea(0x0000, 0x7fff, 2, CpsZRom + 0x00000);
	ZetMapArea(0x8000, 0xbfff, 0, CpsZRom + 0x08000);
	ZetMapArea(0x8000, 0xbfff, 2, CpsZRom + 0x08000);
	ZetMapArea(0xd000, 0xd7ff, 0, FcrashZ80Ram     );
	ZetMapArea(0xd000, 0xd7ff, 1, FcrashZ80Ram     );
	ZetMapArea(0xd000, 0xd7ff, 2, FcrashZ80Ram     );
	ZetMemEnd();
	ZetClose();
	
	BurnYM2203Init(2, 24000000 / 6, NULL, FcrashSynchroniseStream, FcrashGetTime, 0);
	BurnTimerAttachZet(24000000 / 6);
	BurnYM2203SetVolumeShift(2);
	
	MSM5205Init(0, FcrashSynchroniseStream, 24000000 / 64, FcrashMSM5205Vck0, MSM5205_S96_4B, 30, 1);
	MSM5205Init(1, FcrashSynchroniseStream, 24000000 / 64, FcrashMSM5205Vck1, MSM5205_S96_4B, 30, 1);
	
	nCpsZ80Cycles = (24000000 / 6) * 100 / nBurnFPS;
	
	return 0;
}

INT32 FcrashSoundReset()
{
	ZetOpen(0);
	ZetReset();
	BurnYM2203Reset();
	MSM5205Reset();
	FcrashZ80BankAddress = 2;
	ZetMapArea(0x8000, 0xbfff, 0, CpsZRom + FcrashZ80BankAddress);
	ZetMapArea(0x8000, 0xbfff, 2, CpsZRom + FcrashZ80BankAddress);
	ZetClose();
	
	FcrashSoundLatch = 0;
	FcrashSampleBuffer1 = 0;
	FcrashSampleBuffer2 = 0;
	FcrashSampleSelect1 = 0;
	FcrashSampleSelect2 = 0;
	
	return 0;
}

INT32 FcrashSoundExit()
{
	ZetExit();
	BurnYM2203Exit();
	MSM5205Exit();
	
	BurnFree(FcrashZ80Ram);
	
	FcrashZ80BankAddress = 0;
	FcrashSoundLatch = 0;
	FcrashMSM5205Interleave = 0;
	FcrashSampleBuffer1 = 0;
	FcrashSampleBuffer2 = 0;
	FcrashSampleSelect1 = 0;
	FcrashSampleSelect2 = 0;
	
	nCpsZ80Cycles = 0;

	return 0;
}

void FcrashSoundFrameStart()
{
	FcrashMSM5205Interleave = MSM5205CalcInterleave(0, 24000000 / 6);
	
	ZetNewFrame();
	ZetOpen(0);	
}

void FcrashSoundFrameEnd()
{
	INT32 nStartCycles = ZetTotalCycles();
	INT32 nCyclesToDo = nCpsZ80Cycles - nStartCycles;
	INT32 nFramePortion = (INT64)FcrashMSM5205Interleave * nCyclesToDo / nCpsZ80Cycles;
	INT32 nCyclesPerPortion = nCyclesToDo / nFramePortion;
	
	for (INT32 i = 0; i < nFramePortion; i++) {
		BurnTimerUpdate(nStartCycles + ((i + 1) * nCyclesPerPortion));
		MSM5205Update();
	}
	BurnTimerEndFrame(nCpsZ80Cycles);
	
	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(1, pBurnSoundOut, nBurnSoundLen);
	}
	ZetClose();
}
