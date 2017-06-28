// FB Alpha t5182 core
// Based on MAME sources by Jonathan Gevaryahu

#include "burnint.h"
#include "t5182.h"
#include "z80_intf.h"
#include "burn_ym2151.h"

// only used in t5182.c
static INT32 irqstate;
static UINT8 *t5182RAM	= NULL;

static INT32 nCPU;
static INT32 coin_frame;

// use externally
UINT8 *t5182SharedRAM = NULL;	// allocate externally
UINT8 *t5182ROM = NULL;		// allocate externally

UINT8 t5182_semaphore_snd = 0;
UINT8 t5182_semaphore_main = 0;
UINT8 t5182_coin_input;

void t5182_setirq_callback(INT32 param)
{
#if defined FBA_DEBUG
	if (!DebugDev_T5182Initted) bprintf(PRINT_ERROR, _T("t5182_setirq_callback called without init\n"));
#endif

	switch (param)
	{
		case YM2151_ASSERT:
			irqstate |= 1|4;
			break;

		case YM2151_CLEAR:
			irqstate &= ~1;
			break;

		case YM2151_ACK:
			irqstate &= ~4;
			break;

		case CPU_ASSERT:
			irqstate |= 2;
			break;

		case CPU_CLEAR:
			irqstate &= ~2;
			break;
	}

	ZetSetIRQLine(0, (irqstate) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}


static void __fastcall t5182_port_write(UINT16 p, UINT8 d)
{
	switch (p & 0xff)
	{
		case 0x00:
			BurnYM2151SelectRegister(d);
		return;

		case 0x01:
			BurnYM2151WriteRegister(d);
		return;

		case 0x10:
		case 0x11:
			t5182_semaphore_snd = ~p & 1;
		return;

		case 0x12:
			t5182_setirq_callback(YM2151_ACK);
		return;

		case 0x13:
			t5182_setirq_callback(CPU_CLEAR);
		return;
	}
}

static UINT8 __fastcall t5182_port_read(UINT16 p)
{
	switch (p & 0xff)
	{
		case 0x00:
		case 0x01:
			return BurnYM2151ReadStatus();

		case 0x20:
			return t5182_semaphore_main | (irqstate & 2);

		case 0x30: {
			// hack to make coins pulse
			if (t5182_coin_input == 0) {
				coin_frame = 0;
			} else {
				if (coin_frame == 0) {
					coin_frame = GetCurrentFrame();
				} else {
					if ((GetCurrentFrame() - coin_frame) >= 2) {
						return 0; // no coins
					}
				}
			}
			return t5182_coin_input;
		}
	}

	return 0;
}

static void t5182YM2151IrqHandler(INT32 Irq)
{
	t5182_setirq_callback((Irq) ? YM2151_ASSERT : YM2151_CLEAR);
}

void t5182Reset()
{
#if defined FBA_DEBUG
	if (!DebugDev_T5182Initted) bprintf(PRINT_ERROR, _T("t5182Reset called without init\n"));
#endif

	ZetOpen(nCPU);
	ZetReset();
	ZetClose();

	BurnYM2151Reset();

	t5182_semaphore_snd = 0;
	t5182_semaphore_main = 0;

	coin_frame = 0;

	irqstate = 0;
}

void t5182Init(INT32 nZ80CPU, INT32 clock)
{
	DebugDev_T5182Initted = 1;
	
	nCPU = nZ80CPU;

	t5182RAM	= (UINT8*)BurnMalloc(0x800);

	ZetInit(nCPU);
	ZetOpen(nCPU);
	ZetMapMemory(t5182ROM + 0x0000, 0x0000, 0x1fff, MAP_ROM);

	for (INT32 i = 0x2000; i < 0x4000; i += 0x800) {
		ZetMapMemory(t5182RAM,       i, i + 0x7ff, MAP_RAM); // internal ram
	}

	for (INT32 i = 0x4000; i < 0x8000; i += 0x100) {
		ZetMapMemory(t5182SharedRAM, i, i + 0x0ff, MAP_RAM); // shared ram
	}

	ZetMapMemory(t5182ROM + 0x8000, 0x8000, 0xffff, MAP_ROM); // external rom

	ZetSetOutHandler(t5182_port_write);
	ZetSetInHandler(t5182_port_read);
	ZetClose();

	BurnYM2151Init(clock);
	BurnYM2151SetIrqHandler(&t5182YM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
}

void t5182Exit()
{
#if defined FBA_DEBUG
	if (!DebugDev_T5182Initted) bprintf(PRINT_ERROR, _T("t5182Exit called without init\n"));
#endif

	if (!DebugDev_T5182Initted) return;

	BurnYM2151Exit();

	if (nHasZet > 0) {
		ZetExit();
	}

	BurnFree (t5182RAM);

	t5182SharedRAM = NULL;
	t5182RAM = NULL;
	t5182ROM = NULL;
	
	DebugDev_T5182Initted = 0;
}

INT32 t5182Scan(INT32 nAction)
{
#if defined FBA_DEBUG
	if (!DebugDev_T5182Initted) bprintf(PRINT_ERROR, _T("t5182Scan called without init\n"));
#endif

	struct BurnArea ba;

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = t5182RAM;
		ba.nLen	  = 0x00800;
		ba.szName = "t5182 RAM";
		BurnAcb(&ba);

		if (nCPU == 0) {	// if this is z80 #0, it is likely the only one
			ZetScan(nAction);
		}

		BurnYM2151Scan(nAction);

		SCAN_VAR(t5182_semaphore_snd);
		SCAN_VAR(t5182_semaphore_main);
		SCAN_VAR(irqstate);
	}

	return 0;
}
