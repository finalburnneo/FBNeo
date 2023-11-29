#include "cps.h"
#include "burn_ym2151.h"

// PSound - Z80
static INT32 nPsndZBank = 0;
static UINT8* PsndZRam = nullptr;

INT32 Kodb = 0;

// Map in the memory for the current 0zx8000-0xc000 bank
static INT32 PsndZBankMap()
{
	UINT8* Bank;
	UINT32 nOff = (nPsndZBank << 14) + 0x8000;

	if (nOff + 0x4000 > nCpsZRomLen)
	{
		// End of bank in out of range
		nOff = 0;
	}

	Bank = CpsZRom + nOff;

	// Read and fetch the bank
	ZetMapArea(0x8000, 0xBFFF, 0, Bank);
	ZetMapArea(0x8000, 0xBFFF, 2, Bank);

	return 0;
}

// PSound Z80 memory write
void __fastcall PsndZWrite(UINT16 a, UINT8 d)
{
	switch (a)
	{
	case 0xF000:
		BurnYM2151SelectRegister(d);
	//			bprintf(PRINT_NORMAL, "YM2151 select -> %02X\n", d);
		break;
	case 0xF001:
		BurnYM2151WriteRegister(d);
	//			bprintf(PRINT_NORMAL, "YM2151 write  -> %02X\n", d);
		break;
	case 0xF002:
		MSM6295Write(0, d);
		break;
	case 0xF004:
		{
			INT32 nNewBank = d & 0x0f;
			if (nPsndZBank != nNewBank)
			{
				nPsndZBank = nNewBank;
				PsndZBankMap();
			}
			break;
		}
	case 0xF006: // ??? Enable interrupt ???
		break;

#ifdef FBNEO_DEBUG
	//		default:
	//			bprintf(PRINT_NORMAL, _T("Z80 address %04X -> %02X.\n"), a, d);
#endif
	}
}

void __fastcall kodbZWrite(UINT16 a, UINT8 d)
{
	switch (a)
	{
	case 0xE000:
		BurnYM2151SelectRegister(d);
		break;
	case 0xE001:
		BurnYM2151WriteRegister(d);
		break;
	case 0xE400:
		MSM6295Write(0, d);
		break;

#ifdef FBNEO_DEBUG
	//		default:
	//			bprintf(PRINT_NORMAL, _T("Z80 address %04X -> %02X.\n"), a, d);
#endif
	}
}

// PSound Z80 memory read
UINT8 __fastcall PsndZRead(UINT16 a)
{
	switch (a)
	{
	case 0xF001:
		return BurnYM2151Read();
	case 0xF002:
		return MSM6295Read(0);
	case 0xF008:
		//			bprintf(PRINT_NORMAL, " -- Sound latch read (%i).\n", PsndCode);
		return PsndCode;
	case 0xF00A:
		return PsndFade;

#ifdef FBNEO_DEBUG
	//		default:
	//			bprintf(PRINT_NORMAL, _T("Z80 address %04X read.\n"), a);
#endif
	}
	return 0;
}

UINT8 __fastcall kodbZRead(UINT16 a)
{
	switch (a)
	{
	case 0xE001:
		return BurnYM2151Read();
	case 0xE400:
		return MSM6295Read(0);
	case 0xE800:
		return PsndCode;

#ifdef FBNEO_DEBUG
	//		default:
	//			bprintf(PRINT_NORMAL, _T("Z80 address %04X read.\n"), a);
#endif
	}
	return 0;
}

INT32 PsndZInit()
{
	if (nCpsZRomLen < 0x8000)
	{
		// Not enough Z80 Data
		return 1;
	}
	if (CpsZRom == nullptr)
	{
		return 1;
	}

	PsndZRam = BurnMalloc(0x800);
	if (PsndZRam == nullptr)
	{
		return 1;
	}

	ZetInit(0);
	ZetOpen(0);

	if (Kodb)
	{
		ZetSetReadHandler(kodbZRead);
		ZetSetWriteHandler(kodbZWrite);
	}
	else
	{
		ZetSetReadHandler(PsndZRead);
		ZetSetWriteHandler(PsndZWrite);
	}

	// Read and fetch first 0x8000 of Rom
	ZetMapArea(0x0000, 0x7fff, 0, CpsZRom);
	ZetMapArea(0x0000, 0x7fff, 2, CpsZRom);

	// Map first Bank of Rom to 0x8000-0xc000
	nPsndZBank = 0;
	PsndZBankMap();

	// Ram
	ZetMapArea(0xd000, 0xd7ff, 0, PsndZRam);
	ZetMapArea(0xd000, 0xd7ff, 1, PsndZRam);
	ZetMapArea(0xd000, 0xd7ff, 2, PsndZRam);

	// Sound chip interfaces
	ZetMemCallback(0xf000, 0xffff, 0);
	ZetMemCallback(0xf000, 0xffff, 1);

	// In case it tries to fetch other areas
	ZetMapArea(0xc000, 0xcfff, 2, CpsZRom);
	ZetMapArea(0xd800, 0xffff, 2, CpsZRom);
	ZetClose();

	return 0;
}

INT32 PsndZExit()
{
	BurnFree(PsndZRam);

	ZetExit();
	return 0;
}

// Scan the current PSound z80 state
INT32 PsndZScan(INT32 nAction, INT32* pnMin)
{
	struct BurnArea ba;
	ZetScan(nAction);

	MSM6295Scan(nAction, pnMin);
	BurnYM2151Scan(nAction, pnMin);

	SCAN_VAR(nPsndZBank);

	// Scan Ram
	memset(&ba, 0, sizeof(ba));
	ba.szName = "Z80 RAM";
	ba.Data = PsndZRam;
	ba.nLen = 0x800;
	BurnAcb(&ba);

	if (nAction & ACB_WRITE)
	{
		// restore bank -dink
		ZetOpen(0);
		PsndZBankMap();
		ZetClose();
	}

	return 0;
}
