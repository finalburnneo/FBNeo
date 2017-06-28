// NMK004 device
// Based on MAME device by David Haywood and trap15

#include "burnint.h"
#include "m68000_intf.h"
#include "tlcs90_intf.h"
#include "nmk004.h"
#include "burn_ym2203.h"
#include "msm6295.h"

UINT8 *NMK004OKIROM0;
UINT8 *NMK004OKIROM1;
UINT8 *NMK004PROGROM;

static UINT8 *ram;

static UINT8 to_nmk004 = 0;
static UINT8 to_main = 0;
static INT32 bankdata[2] = { 0, 0 };

static INT32 nmk004_initted = 0;

static UINT8 nmk004_tlcs90_read(UINT32 address)
{
	if (address >= 0xfec0 && address < 0xffc0) {	// internal ram
		return ram[0x800 + (address - 0xfec0)];
	}

	switch (address)
	{
		case 0xf800:
		case 0xf801:
			return BurnYM2203Read(0, address & 1);

		case 0xf900:
			return MSM6295ReadStatus(0);

		case 0xfa00:
			return MSM6295ReadStatus(1);

		case 0xfb00:
			return to_nmk004;
	}

	return 0;
}

static void oki_bankswitch(INT32 chip, INT32 bank)
{
	UINT8 *rom = (chip) ? NMK004OKIROM1 : NMK004OKIROM0;

	bankdata[chip] = bank;
	bank = (bank + 1) & 3;

	memcpy (rom + 0x20000, rom + 0x20000 + bank * 0x20000, 0x20000);
}

static void nmk004_tlcs90_write(UINT32 address, UINT8 data)
{
	if (address >= 0xfec0 && address < 0xffc0) {	// internal ram
		ram[0x800 + (address - 0xfec0)] = data;
		return;
	}

	switch (address)
	{
		case 0xf800:
		case 0xf801:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0xf900:
			MSM6295Command(0, data);
		return;

		case 0xfa00:
			MSM6295Command(1, data);
		return;

		case 0xfc00:
			to_main = data;
		return;

		case 0xfc01:
			oki_bankswitch(0, data);
		return;

		case 0xfc02:
			oki_bankswitch(1, data);
		return;
	}
}

static void nmk004_tlcs90_write_port(UINT16 port, UINT8 data)
{
	switch (port)
	{
		case 0xffc8: { // hack - disable watchdog function
			if (data & 1) {
		//		SekReset();
		//		bprintf (0, _T("Resetting 68k!\n"));
			} else {
		//		bprintf (0, _T("Clear 68k reset!\n"));
			}			
		}
	}
}

void NMK004_reset()
{
	memset (ram, 0, 0x900);

	tlcs90Open(0);
	tlcs90Reset();
	BurnYM2203Reset();
	tlcs90Close();

	MSM6295Reset(0);
	MSM6295Reset(1);

	oki_bankswitch(0,0);
	oki_bankswitch(1,0);
	to_main = 0;
	to_nmk004 = 0;
}

static void NMK004YM2203IrqHandler(INT32, INT32 nStatus)
{
	tlcs90SetIRQLine(0, (nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

inline static double NMK004GetTime()
{
	return (double)tlcs90TotalCycles() / 8000000;
}

inline static INT32 NMK004SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(tlcs90TotalCycles() * nSoundRate / 8000000);
}

void NMK004_init()
{
	nmk004_initted = 1;
	ram = (UINT8*)BurnMalloc(0x900);

	tlcs90Init(0, 8000000);
	tlcs90Open(0);
	tlcs90MapMemory(NMK004PROGROM, 0x0000, 0xefff, MAP_ROM);
	tlcs90MapMemory(ram,	       0xf000, 0xf7ff, MAP_RAM);
	tlcs90SetReadHandler(nmk004_tlcs90_read);
	tlcs90SetWriteHandler(nmk004_tlcs90_write);
	tlcs90SetWritePortHandler(nmk004_tlcs90_write_port);
	tlcs90Close();

	BurnYM2203Init(1, 1500000, &NMK004YM2203IrqHandler, NMK004SynchroniseStream, NMK004GetTime, 0);
	BurnTimerAttachTlcs90(8000000);
	/* reminder: these route#s are opposite of MAME, for example:
	MCFG_SOUND_ROUTE(0, "mono", 0.50)
	MCFG_SOUND_ROUTE(1, "mono", 0.50)
	MCFG_SOUND_ROUTE(2, "mono", 0.50)
	MCFG_SOUND_ROUTE(3, "mono", 1.20)
	Is equal to the 4 route lines below. -dink
	*/
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   1.20, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.50, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 4000000 / 165, 1);
	MSM6295Init(1, 4000000 / 165, 1);
	MSM6295SetRoute(0, 0.10, BURN_SND_ROUTE_BOTH);
	MSM6295SetRoute(1, 0.10, BURN_SND_ROUTE_BOTH);
}

void NMK004_exit()
{
	if (!nmk004_initted) return;

	nmk004_initted = 0;

	BurnFree(ram);
	ram = NULL;

	tlcs90Exit();
	BurnYM2203Exit();
	MSM6295Exit(0);
	MSM6295Exit(1);
}

INT32 NMK004Scan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = ram;
		ba.nLen	  = 0x900;
		ba.szName = "tlcs90 ram";
		BurnAcb(&ba);

		tlcs90Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		MSM6295Scan(0, nAction);
		MSM6295Scan(1, nAction);

		SCAN_VAR(to_nmk004);
		SCAN_VAR(to_main);
		SCAN_VAR(bankdata[0]);
		SCAN_VAR(bankdata[1]);
	}

	if (nAction & ACB_WRITE)
	{
		oki_bankswitch(0,bankdata[0]);
		oki_bankswitch(1,bankdata[1]);
	}

	return 0;
}

void NMK004NmiWrite(INT32 data)
{
	data ^= 0xff; // hack - no game works properly without this being inverted.

	tlcs90SetIRQLine(0x20 /*nmi*/, (data & 1) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

void NMK004Write(INT32, INT32 data)
{
	to_nmk004 = data & 0xff;
}

UINT8 NMK004Read()
{
	return to_main;
}
