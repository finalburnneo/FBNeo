// FB Alpha Taito En / F3 sound core
// Based on MAME sources by Bryan McPhail, Aaron Giles, R. Belmont, hap, Philip Bennett

#include "burnint.h"
#include "m68000_intf.h"
#include "es5506.h"
#include "mb87078.h"
#include "taito.h"

// Allocate these externally!
UINT8 *TaitoF3SoundRom = NULL;
UINT8 *TaitoF3SoundRam = NULL;
UINT8 *TaitoF3ES5506Rom = NULL;
INT32  TaitoF3ES5506RomSize = 0;
UINT8 *TaitoF3SharedRam = NULL;
UINT8 *TaitoES5510DSPRam = NULL;
UINT32 *TaitoES5510GPR = NULL;
UINT16 *TaitoES5510DRAM = NULL;
double TaitoF3VolumeOffset = 0.0;

static INT32 TaitoF3Counter;
static INT32 TaitoF3VectorReg;
static INT32 M68681IMR;
static INT32 IMRStatus;
static UINT32 TaitoF3SoundTriggerIRQCycles;
static UINT32 TaitoF3SoundTriggerIRQPulseCycles;
static UINT32 TaitoF3SoundTriggerIRQCycleCounter;
static UINT32 TaitoF3SoundTriggerIRQPulseCycleCounter;
static INT32  TaitoF3SoundTriggerIRQCyclesMode = 0;

static UINT32 TaitoES5510GPRLatch;
static UINT32 TaitoES5510DOLLatch;
static UINT32 TaitoES5510DILLatch;
static UINT32 TaitoES5510DADRLatch;
static UINT8  TaitoES5510RAMSelect;

#define IRQ_TRIGGER_OFF		0
#define IRQ_TRIGGER_ONCE	1
#define IRQ_TRIGGER_PULSE	2

static INT32 TaitoF3CpuNum = 2;

static INT32 __fastcall TaitoF3SoundIRQCallback(INT32 /*irq*/)
{
	return TaitoF3VectorReg;
}

static UINT8 __fastcall TaitoF3Sound68KReadByte(UINT32 a)
{
	if (a >= 0x140000 && a <= 0x140fff) {
		INT32 Offset = (a & 0xfff) >> 1;
		UINT8 *Ram = (UINT8*)TaitoF3SharedRam;
		return Ram[Offset^1];
	}

	if (a >= 0x260000 && a <= 0x2601ff) {
		INT32 Offset = (a & 0x1ff);

		switch (Offset>>1)
		{
			case (0x09): return (TaitoES5510DILLatch >> 16) & 0xff;
			case (0x0a): return (TaitoES5510DILLatch >> 8) & 0xff;
			case (0x0b): return (TaitoES5510DILLatch >> 0) & 0xff;
			case (0x12): return 0;
			case (0x16): return 0x27;
		    default: break;
		}

		return TaitoES5510DSPRam[Offset];
	}

	if (a >= 0x280000 && a <= 0x28001f) {
		INT32 Offset = (a & 0x1f) >> 1;
		if (Offset == 0x05) {
			INT32 Ret = IMRStatus;
			IMRStatus = 0;
			return Ret;
		}

		if (Offset == 0x0e) return 0x01;

		if (Offset == 0x0f) {
			SekSetIRQLine(6, CPU_IRQSTATUS_NONE);
			return 0x00;
		}

		return 0xff;
	}

	if (a >= 0x200000 && a <= 0x20001f) {
		INT32 Offset = ((a & 0x1f) >> 1);
		INT16 rc = ES5505Read(Offset);
		if (Offset&1) rc >>=8;

		return rc&0xff;
	}

	bprintf(PRINT_NORMAL, _T("Sound 68K Read byte => %06X\n"), a);

	return 0;
}

static void __fastcall TaitoF3Sound68KWriteByte(UINT32 a, UINT8 d)
{
	if (a >= 0x140000 && a <= 0x140fff) {
		INT32 Offset = (a & 0xfff) >> 1;
		UINT8 *Ram = (UINT8*)TaitoF3SharedRam;
		Ram[Offset^1] = d;
		return;
	}

	if (a >= 0x260000 && a <= 0x2601ff) {
		INT32 Offset = (a & 0x1ff);

		TaitoES5510DSPRam[Offset] = d;

		switch (Offset>>1) {
			case 0x00: TaitoES5510GPRLatch = (TaitoES5510GPRLatch & 0x00ffff) | ((d & 0xff) <<16); return;
			case 0x01: TaitoES5510GPRLatch = (TaitoES5510GPRLatch & 0xff00ff) | ((d & 0xff) << 8); return;
			case 0x02: TaitoES5510GPRLatch = (TaitoES5510GPRLatch & 0xffff00) | ((d & 0xff) << 0); return;

			case 0x0c: TaitoES5510DOLLatch = (TaitoES5510DOLLatch & 0x00ffff) | ((d & 0xff) <<16); return;
			case 0x0d: TaitoES5510DOLLatch = (TaitoES5510DOLLatch & 0xff00ff) | ((d & 0xff) << 8); return;
			case 0x0e: TaitoES5510DOLLatch = (TaitoES5510DOLLatch & 0xffff00) | ((d & 0xff) << 0); return;
			case 0x0f: {
				TaitoES5510DADRLatch = (TaitoES5510DADRLatch & 0x00ffff) | ((d & 0xff) << 16);
				if (TaitoES5510DADRLatch > 0x1fffff) {
					bprintf(0, _T("Taito F3SND-error: DRAM OVERFLOW! Addr = %X\n"), TaitoES5510DADRLatch);
					TaitoES5510DADRLatch &= 0x1fffff;
				}
				if(TaitoES5510RAMSelect)
					TaitoES5510DILLatch = ((UINT32)TaitoES5510DRAM[TaitoES5510DADRLatch] << 8);
				else
					TaitoES5510DRAM[TaitoES5510DADRLatch] = (TaitoES5510DOLLatch >> 8);
				return;
			}
			case 0x10: TaitoES5510DADRLatch = (TaitoES5510DADRLatch & 0xff00ff) | ((d & 0xff) << 8); return;
			case 0x11: TaitoES5510DADRLatch = (TaitoES5510DADRLatch & 0xffff00) | ((d & 0xff) << 0); return;
			case 0x14: TaitoES5510RAMSelect = d & 0x80; return;

			case 0x80: if (d < 0xc0) { TaitoES5510GPRLatch = TaitoES5510GPR[d]; return; }

			case 0xa0: if (d < 0xc0) { TaitoES5510GPR[d] = TaitoF3ES5506Rom[(TaitoES5510GPRLatch >> 8) & (TaitoF3ES5506RomSize - 1)]; return; }

		    default: return;
		}
		return;
	}

	if (a >= 0x280000 && a <= 0x28001f) {
		INT32 Offset = (a & 0x1f) >> 1;

		switch (Offset) {
			case 0x04: {
				switch ((d >> 4) & 0x07) {
					case 0x0: {
						return;
					}

					case 0x01: {
						return;
					}

					case 0x02: {
						return;
					}

					case 0x03: {
						//bprintf(PRINT_NORMAL, _T("counter is %04x (/16), so interrupt once in %d cycles\n"), TaitoF3Counter, (16000000 / 2000000) * TaitoF3Counter * 16);
						TaitoF3SoundTriggerIRQCyclesMode = IRQ_TRIGGER_ONCE;
						TaitoF3SoundTriggerIRQCycleCounter = 0;
						TaitoF3SoundTriggerIRQCycles = (16000000 / 2000000) * TaitoF3Counter * 16;
						return;
					}

					case 0x04: {
						return;
					}

					case 0x05: {
						return;
					}

					case 0x06: {
						//bprintf(PRINT_NORMAL, _T("counter is %04x, so interrupt every %d cycles\n"), TaitoF3Counter, (16000000 / 2000000) * TaitoF3Counter);
						TaitoF3SoundTriggerIRQCyclesMode = IRQ_TRIGGER_PULSE;
						TaitoF3SoundTriggerIRQPulseCycleCounter = 0;
						TaitoF3SoundTriggerIRQPulseCycles = (16000000 / 2000000) * TaitoF3Counter;
						return;
					}

					case 0x07: {
						return;
					}
				}
			}

			case 0x05: {
				M68681IMR = d & 0xff;
				return;
			}

			case 0x06: {
				TaitoF3Counter = ((d & 0xff) << 8) | (TaitoF3Counter & 0xff);
				return;
			}

			case 0x07: {
				TaitoF3Counter = (TaitoF3Counter & 0xff00) | (d & 0xff);
				return;
			}

			case 0x08: return;
			case 0x09: return;
			case 0x0a: return;
			case 0x0b: return;

			case 0x0c: {
				TaitoF3VectorReg = d & 0xff;
				return;
			}

			default: {
//				bprintf(PRINT_NORMAL,_T("f3_68681_w byte %x -> %x\n"), Offset, d);
				return;
			}
		}
	}

	if (a >= 0x300000 && a <= 0x30003f) {
		UINT32 MaxBanks = (TaitoF3ES5506RomSize / 0x200000) - 1;
		INT32 Offset = (a & 0x3f) >> 1;

		d &= MaxBanks;
		es5505_voice_bank_w(Offset, d << 20);
		return;
	}

	if (a >= 0x340000 && a <= 0x340003) {
		INT32 Offset = (a & 3) >> 1;

		mb87078_write(Offset^1, d);
		return;
	}

	bprintf(PRINT_NORMAL, _T("Sound 68K Write byte => %06X, %02X\n"), a, d);
}

static UINT16 __fastcall TaitoF3Sound68KReadWord(UINT32 a)
{
	if (a >= 0x200000 && a <= 0x20001f) {
		INT32 Offset = (a & 0x1f) >> 1;

		return ES5505Read(Offset);
	}

	bprintf(PRINT_NORMAL, _T("Sound 68K Read word => %06X\n"), a);

	return 0;
}

static void __fastcall TaitoF3Sound68KWriteWord(UINT32 a, UINT16 d)
{
	if (a >= 0x200000 && a <= 0x20001f) {
		INT32 Offset = (a & 0x1f) >> 1;
		ES5505Write(Offset, d);
		return;
	}

	bprintf(PRINT_NORMAL, _T("Sound 68K Write word => %06X, %04X\n"), a, d);
}

void TaitoF3VolumeCallback(INT32 offset, INT32 data)
{
	if (offset > 1) {
		offset = (offset & 1) ? BURN_SND_ES5506_ROUTE_RIGHT : BURN_SND_ES5506_ROUTE_LEFT;
		ES5506SetRoute(0, (double)(data / 100.00) + TaitoF3VolumeOffset, offset);
	}
}

void TaitoF3SoundReset()
{
	memcpy(TaitoF3SoundRam, TaitoF3SoundRom, 8); // copy vectors

//	taito system resets these, but SekReset() is needed to latch the vectors.
	{
		SekOpen(TaitoF3CpuNum);
		SekReset();
		ES5506Reset();
		SekClose();
	}

	TaitoF3Counter = 0;
	TaitoF3VectorReg = 0;
	TaitoES5510GPRLatch = 0;
	TaitoES5510DOLLatch = 0;
	TaitoES5510DILLatch = 0;
	TaitoES5510DADRLatch = 0;
	TaitoES5510RAMSelect = 0;

	M68681IMR = 0;
	IMRStatus = 0;
	TaitoF3SoundTriggerIRQCycles = 0;
	TaitoF3SoundTriggerIRQPulseCycles = 0;
	TaitoF3SoundTriggerIRQCycleCounter = 0;
	TaitoF3SoundTriggerIRQPulseCycleCounter = 0;
	TaitoF3SoundTriggerIRQCyclesMode = 0;
	mb87078_reset();
}

void TaitoF3SoundExit()
{
	// do not exit Sek here! All known F3 sound drivers have m68ec020 as main!

	TaitoF3SoundRom = NULL;
	TaitoF3SoundRam = NULL;
	TaitoF3ES5506Rom = NULL;
	TaitoF3SharedRam = NULL;
	TaitoES5510DSPRam = NULL;
	TaitoES5510GPR = NULL;
	TaitoF3VolumeOffset = 0.0;

	TaitoF3ES5506RomSize = 0;

	ES5506Exit();

	mb87078_exit();
}

void TaitoF3SoundInit(INT32 cpunum)
{
	TaitoF3CpuNum = cpunum;
	
	SekInit(TaitoF3CpuNum, 0x68000);
	SekOpen(TaitoF3CpuNum);
	SekMapMemory(TaitoF3SoundRam          , 0x000000, 0x00ffff, MAP_RAM);
	SekMapMemory(TaitoF3SoundRam          , 0x010000, 0x01ffff, MAP_RAM);
	SekMapMemory(TaitoF3SoundRam          , 0x020000, 0x02ffff, MAP_RAM);
	SekMapMemory(TaitoF3SoundRam          , 0x030000, 0x03ffff, MAP_RAM);
	SekMapMemory(TaitoF3SoundRom          , 0xc00000, 0xcfffff, MAP_ROM);
	SekMapMemory(TaitoF3SoundRam          , 0xff0000, 0xffffff, MAP_RAM);
	SekSetReadByteHandler(0, TaitoF3Sound68KReadByte);
	SekSetWriteByteHandler(0, TaitoF3Sound68KWriteByte);
	SekSetReadWordHandler(0, TaitoF3Sound68KReadWord);
	SekSetWriteWordHandler(0, TaitoF3Sound68KWriteWord);
	SekSetIrqCallback(TaitoF3SoundIRQCallback);
	SekClose();

	ES5505Init(30476100/2, TaitoF3ES5506Rom, TaitoF3ES5506Rom, NULL);

	mb87078_init(TaitoF3VolumeCallback);
}

void TaitoF3CpuUpdate(INT32 nInterleave, INT32 nCurrentSlice)
{
	static INT32 nCyclesDone = 0;

	if (nCurrentSlice == 0) {
		nCyclesDone = 0;
	}

	INT32 nTotalCycles = (30476100 / 2) / (nBurnFPS / 100);

	SekOpen(TaitoF3CpuNum);

	INT32 nNext = (nCurrentSlice + 1) * nTotalCycles / nInterleave;
	INT32 nSegment = nNext - nCyclesDone;

	nCyclesDone += SekRun(nSegment);

	if (TaitoF3SoundTriggerIRQCyclesMode == IRQ_TRIGGER_ONCE) {
		TaitoF3SoundTriggerIRQCycleCounter += nSegment;
		if (TaitoF3SoundTriggerIRQCycleCounter >= TaitoF3SoundTriggerIRQCycles) {
			TaitoF3SoundTriggerIRQCyclesMode = IRQ_TRIGGER_OFF;
			if (M68681IMR & 0x08) {
				SekSetIRQLine(6, CPU_IRQSTATUS_ACK);
				IMRStatus |= 0x08;
			}
		}
	}

	if (TaitoF3SoundTriggerIRQCyclesMode == IRQ_TRIGGER_PULSE) {
		TaitoF3SoundTriggerIRQPulseCycleCounter += nSegment;
		if (TaitoF3SoundTriggerIRQPulseCycleCounter >= TaitoF3SoundTriggerIRQPulseCycles) {
			if (M68681IMR & 0x08) {
				SekSetIRQLine(6, CPU_IRQSTATUS_ACK);
				IMRStatus |= 0x08;
			}
			TaitoF3SoundTriggerIRQPulseCycleCounter = 0;
		}
	}

	SekClose();
}

void TaitoF3SoundUpdate(INT16 *pDest, INT32 nLen)
{
	if (pBurnSoundOut) {
		ES5505Update(pDest, nLen);
	}
}

INT32 TaitoF3SoundScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
	//	SekScan(nAction);		// call in driver!
		ES5506ScanRoutes(nAction, pnMin); // F3 games change the volume levels in-game

		mb87078_scan();

		SCAN_VAR(TaitoF3Counter);
		SCAN_VAR(TaitoF3VectorReg);
		SCAN_VAR(TaitoES5510GPRLatch);
		SCAN_VAR(M68681IMR);
		SCAN_VAR(IMRStatus);
		SCAN_VAR(TaitoF3SoundTriggerIRQCycles);
		SCAN_VAR(TaitoF3SoundTriggerIRQPulseCycles);
		SCAN_VAR(TaitoF3SoundTriggerIRQCycleCounter);
		SCAN_VAR(TaitoF3SoundTriggerIRQPulseCycleCounter);
		SCAN_VAR(TaitoF3SoundTriggerIRQCyclesMode);
	}

	return 0;
}
