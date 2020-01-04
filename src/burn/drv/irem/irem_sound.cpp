// FB Alpha Irem Sound system device
// Based on MAME sources by Couriersud and FBA port by Barry Harris (Treble Winner)

#include "burnint.h"
#include "z80_intf.h"
#include "m6800_intf.h"
#include "msm5205.h"
#include "ay8910.h"

static UINT8 *IremM6803Rom;
static UINT8 IremM6803Ram[0x80];

static UINT8 IremSoundLatch;
static UINT8 IremPort1;
static UINT8 IremPort2;
UINT8 IremSlaveMSM5205VClckReset;

static INT32 IremM6803Clock;
static INT32 IremZ80Clock;
static INT32 bHasMSM5205_1 = 0;
static INT32 nSoundType = 0;

static void (*pAY8910P1ACb)(UINT8 data) = NULL;

void IremSoundWrite(UINT8 d)
{
	if ((d & 0x80) == 0) {
		IremSoundLatch = d;
	} else {
		M6803SetIRQLine(M6803_IRQ_LINE, CPU_IRQSTATUS_ACK);
	}

}

INT32 IremSoundReset()
{
	memset (IremM6803Ram, 0, 0x80);

	M6803Open(0);
	M6803Reset();
	M6803SetIRQLine(M6803_IRQ_LINE, CPU_IRQSTATUS_ACK);
	M6803Close();

	AY8910Reset(0);
	AY8910Reset(1);

	MSM5205Reset();

	IremSoundLatch = 0;
	IremPort1 = 0;
	IremPort2 = 0;
	IremSlaveMSM5205VClckReset = 0;

	return 0;
}

static UINT8 IremM6803ReadByte(UINT16 a)
{
	if (a <= 0x001f) {
		return m6803_internal_registers_r(a);
	}

	if (a >= 0x0080 && a <= 0x00ff) {
		return IremM6803Ram[a - 0x0080];
	}

	if (a == 0x7f) return 0; // NOP

	bprintf(PRINT_NORMAL, _T("M6803 Read Byte -> %04X\n"), a);

	return 0;
}

static void IremM6803WriteByte(UINT16 a, UINT8 d)
{
	int subhit = 0;
	if (a <= 0x001f) {
		m6803_internal_registers_w(a, d);
		return;
	}

	if (a >= 0x0080 && a <= 0x00ff) {
		IremM6803Ram[a - 0x0080] = d;
		return;
	}

	if (nSoundType == 0)
	{
		a &= 0x7fff;

		if (a < 0x1000) {
			if (a & 1) MSM5205DataWrite(0, d);
			if ((a & 2) && bHasMSM5205_1) MSM5205DataWrite(0, d);
			return;
		}

		if (a < 0x2000) {
			M6803SetIRQLine(M6803_IRQ_LINE, CPU_IRQSTATUS_NONE);
			return;
		}
	}
	else if (nSoundType == 1)
	{
		a &= 0x0803;
		subhit = 1;

		if (a == 0x0800) {
			M6803SetIRQLine(M6803_IRQ_LINE, CPU_IRQSTATUS_NONE);
			return;
		}

		if (a == 0x0801) {
			MSM5205DataWrite(0, d);
			return;
		}

		if (a == 0x0802) {
			if (bHasMSM5205_1) MSM5205DataWrite(1, d);
			return;
		}
		
		if (a == 0x0803) {
			return; // nop?
		}
		
		if (a < 0x800) {
			IremM6803WriteByte(a, d);
			return;
		}
	}
	else if (nSoundType == 2)
	{
		if (a < 0x2000) {
			if (a & 1) MSM5205DataWrite(0, d);
			if ((a & 2) && bHasMSM5205_1) MSM5205DataWrite(1, d);
			return;
		}

		if (a < 0x4000) {
			M6803SetIRQLine(M6803_IRQ_LINE, CPU_IRQSTATUS_NONE);
			return;
		}
	}

	bprintf(PRINT_NORMAL, _T("M6803 Write Byte -> %04X, %02X Type-> %d, %d\n"), a, d, nSoundType, subhit);
}

static UINT8 IremM6803ReadPort(UINT16 a)
{
	switch (a) {
		case M6803_PORT1: {
  			if (IremPort2 & 0x08) return AY8910Read(0);
			if (IremPort2 & 0x10) return AY8910Read(1);
			return 0xff;
		}

		case M6803_PORT2: {
			return 0;
		}
	}

	bprintf(PRINT_NORMAL, _T("M6803 Read Port -> %04X\n"), a);

	return 0;
}

static void IremM6803WritePort(UINT16 a, UINT8 d)
{
	switch (a) {
		case M6803_PORT1: {
			IremPort1 = d;
			return;
		}

		case M6803_PORT2: {
			if ((IremPort2 & 0x01) && !(d & 0x01)) {
				if (IremPort2 & 0x04) {
					if (IremPort2 & 0x08) {
						AY8910Write(0, 0, IremPort1);
					}
					if (IremPort2 & 0x10) {
						AY8910Write(1, 0, IremPort1);
					}
				} else {
					if (IremPort2 & 0x08) {
						AY8910Write(0, 1, IremPort1);
					}
					if (IremPort2 & 0x10) {
						AY8910Write(1, 1, IremPort1);
					}
				}
			}

			IremPort2 = d;
			return;
		}
	}

	bprintf(PRINT_NORMAL, _T("M6803 Write Port -> %04X, %02X\n"), a, d);
}

static UINT8 IremSoundLatchRead(UINT32)
{
	return IremSoundLatch;
}

static void AY8910_0PortBWrite(UINT32, UINT32 d)
{
	MSM5205PlaymodeWrite(0, (d >> 2) & 0x07);
	MSM5205ResetWrite(0, d & 0x01);

	if (bHasMSM5205_1) {
		MSM5205PlaymodeWrite(1, ((d >> 2) & 0x04) | 0x03);
		MSM5205ResetWrite(1, d & 0x02);
	}
}

static void AY8910_1PortAWrite(UINT32, UINT32 data)
{
	if (pAY8910P1ACb) {
		pAY8910P1ACb(data);
	}
}

inline static INT32 IremSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)ZetTotalCycles() * nSoundRate / IremZ80Clock);
}

static void IremMSM5205Vck0()
{
	M6803SetIRQLine(M6803_INPUT_LINE_NMI, CPU_IRQSTATUS_AUTO);
	IremSlaveMSM5205VClckReset = 1;
	// second chip is clocked in driver under MSM5205Update(); with IremSoundClockSlave();
}

void IremSoundClockSlave()
{
	if (bHasMSM5205_1 && IremSlaveMSM5205VClckReset) {
		MSM5205VCLKWrite(1, 1);
		MSM5205VCLKWrite(1, 0);
		IremSlaveMSM5205VClckReset = 0;
	}
}

// type 2 = 10 yard fight
void IremSoundInit(UINT8 *pZ80ROM, INT32 nType, INT32 nZ80Clock)
{
	IremM6803Rom = pZ80ROM;

	M6803Init(0);
	M6803Open(0);
	if (nType == 0) {
		M6803MapMemory(IremM6803Rom + 0x2000, 0x2000, 0x7fff, MAP_ROM);
		M6803MapMemory(IremM6803Rom + 0x2000, 0xa000, 0xffff, MAP_ROM);
	} else {
		M6803MapMemory(IremM6803Rom + 0x4000, 0x4000, 0xffff, MAP_ROM);
	}
	M6803SetReadHandler(IremM6803ReadByte);
	M6803SetWriteHandler(IremM6803WriteByte);
	M6803SetReadPortHandler(IremM6803ReadPort);
	M6803SetWritePortHandler(IremM6803WritePort);
	M6803Close();

	AY8910Init(0, 894886, 0);
	AY8910Init(1, 894886, 1);
	AY8910SetPorts(0, &IremSoundLatchRead, NULL, NULL, &AY8910_0PortBWrite);
	AY8910SetPorts(1, NULL, NULL, &AY8910_1PortAWrite, NULL);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, IremSynchroniseStream, 384000, IremMSM5205Vck0, MSM5205_S96_4B, 1);
	MSM5205Init(1, IremSynchroniseStream, 384000, NULL, MSM5205_SEX_4B, 1);
	MSM5205SetRoute(0, (nType == 2) ? 0.80 : 0.20, BURN_SND_ROUTE_BOTH);
	MSM5205SetRoute(1, (nType == 2) ? 0.80 : 0.20, BURN_SND_ROUTE_BOTH);

	IremZ80Clock = nZ80Clock;
	IremM6803Clock = 894886;
	bHasMSM5205_1 = (nType != 0) ? 1 : 0;
	nSoundType = nType;
}

void IremSoundInit(UINT8 *pZ80ROM, INT32 nType, INT32 nZ80Clock, void (*ay8910cb)(UINT8))
{
	pAY8910P1ACb = ay8910cb;
	
	IremSoundInit(pZ80ROM, nType, nZ80Clock);
}

INT32 IremSoundExit()
{
	M6800Exit();
	AY8910Exit(0);
	AY8910Exit(1);
	MSM5205Exit();

	IremSoundLatch = 0;
	IremPort1 = 0;
	IremPort2 = 0;
	IremSlaveMSM5205VClckReset = 0;
	IremM6803Clock = 0;

	bHasMSM5205_1 = 0;
	nSoundType = 0;
	pAY8910P1ACb = NULL;

	return 0;
}

INT32 IremSoundScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = IremM6803Ram;
		ba.nLen	  = 0x80;
		ba.szName = "Irem Sound RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		M6803Scan(nAction);
		AY8910Scan(nAction, pnMin);
		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(IremSoundLatch);
		SCAN_VAR(IremPort1);
		SCAN_VAR(IremPort2);
		SCAN_VAR(IremSlaveMSM5205VClckReset);
	}

	return 0;
}
