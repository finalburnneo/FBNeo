#include "toaplan.h"
// Enma Daio

static UINT8 DrvButton[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvDips[1] = { 0 };
static UINT8 DrvInput[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static UINT8 *Mem = NULL, *MemEnd = NULL;
static UINT8 *RamStart, *RamEnd;
static UINT8 *Rom01;
static UINT8 *Ram01, *RamPal;

static const INT32 nColCount = 0x0800;

static UINT8 DrvReset = 0;

static INT32 bankaddress = 0;

static UINT8 nIRQPending;

static struct BurnInputInfo EnmadaioInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvButton + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvButton + 5,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvButton + 0,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvButton + 1,	"tilt"		},
	{"Fake",  			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Enmadaio)

static struct BurnDIPInfo EnmadaioDIPList[] = {
	// Defaults
	{0x07,	0xFF,  0xFF,	0x00,	  NULL},
};

STDDIPINFO(Enmadaio)

static UINT8 __fastcall enmadaioReadByte(UINT32 sekAddress)
{
	switch (sekAddress) {

		case 0x20000d:
			return ToaVBlankRegister();

		case 0x700001:
			return ToaScanlineRegister();

		case 0x700004:
		case 0x700005:								// dswa
			return 0; //DrvInput[0];

		case 0x70000c:
		case 0x70000d:								// MISC2
			return 0; //DrvInput[1];

		case 0x700010:
		case 0x700011:								// MISC3
			return DrvInput[1];

		case 0x700014:
		case 0x700015:								// MISC4
			return 0; //DrvInput[3];

		case 0x700018:
		case 0x700019:								// SYS
			return DrvInput[2];

		case 0x70001c:// return 0x01;
		case 0x70001d:// return 0x01; 							// UNK
			return 0; //DrvInput[5];

		case 0x400001:
			return BurnYM2151Read();

		case 0x400003:
			return BurnYM2151Read();

		case 0x500001:
			return MSM6295Read(0);

		default: {
			bprintf(0, _T("Attempt to read byte value of location %x\n"), sekAddress);
		}
	}
	return 0;
}

static UINT16 __fastcall enmadaioReadWord(UINT32 sekAddress)
{
	switch (sekAddress) {

		case 0x200004:
			return ToaGP9001ReadRAM_Hi(0);

		case 0x200006:
			return ToaGP9001ReadRAM_Lo(0);

		case 0x700000:
			return ToaScanlineRegister();

		case 0x700004:
			return 0; //DrvInput[0];

		case 0x70000c:
			return 0; //DrvInput[1];

		case 0x700010:
			return DrvInput[1];

		case 0x700014:
			return 0; //DrvInput[3];

		case 0x700018:
			return DrvInput[2];

		case 0x70001c: return 0x0100;  // 0x0100 = unfreeze! -dink  // UNK

		case 0x400000:
			return BurnYM2151Read();

		case 0x500000:
			return MSM6295Read(0);

		default: {
			bprintf(0, _T("Attempt to read word value of location %x\n"), sekAddress);
		}
	}
	return 0;
}

static void oki_bankswitch(INT32 bank)
{
	if (bank >= 0x60) return;

	bankaddress = bank;

	MSM6295SetBank(0, MSM6295ROM + bankaddress * 0x20000, 0, 0x3ffff);
}

static void __fastcall enmadaioWriteByte(UINT32 sekAddress, UINT8 byteValue)
{
	switch (sekAddress) {
		case 0x400001:
			BurnYM2151SelectRegister(byteValue);
			break;
		case 0x400003:
			BurnYM2151WriteRegister(byteValue);
			break;

		case 0x500001:
			MSM6295Write(0, byteValue);
			break;

		case 0x700021:
			oki_bankswitch(byteValue);
			break;

		default: {
			bprintf(0, _T("Attempt to write byte value %x to location %x\n"), byteValue, sekAddress);
		}
	}
}

static void __fastcall enmadaioWriteWord(UINT32 sekAddress, UINT16 wordValue)
{
	switch (sekAddress) {
		case 0x200000:								// Set GP9001 VRAM address-pointer
			ToaGP9001SetRAMPointer(wordValue);
			break;

		case 0x200004:
		case 0x200006:
			ToaGP9001WriteRAM(wordValue, 0);
			break;

		case 0x200008:
			ToaGP9001SelectRegister(wordValue);
			break;

		case 0x20000C:
			ToaGP9001WriteRegister(wordValue);
			break;

		case 0x400000:
			BurnYM2151SelectRegister(wordValue);
			break;

		case 0x400002:
			BurnYM2151WriteRegister(wordValue);
			break;

		case 0x500000:
			MSM6295Write(0, wordValue & 0xFF);
			break;

		case 0x700020:
			oki_bankswitch(wordValue);
			break;

		case 0x700028:
		case 0x70003c:
			return; // NOP

		default: {
			bprintf(0, _T("Attempt to write word value %x to location %x\n"), wordValue, sekAddress);
		}
	}
}

static INT32 DrvExit()
{
	MSM6295Exit(0);
	BurnYM2151Exit();

	ToaPalExit();

	ToaExtraTextExit();
	ToaExitGP9001();
	SekExit();				// Deallocate 68000s

	BurnFree(Mem);

	return 0;
}

static INT32 DrvDoReset()
{
	SekOpen(0);
	nIRQPending = 0;
	SekSetIRQLine(0, CPU_IRQSTATUS_NONE);
	SekReset();
	SekClose();

	MSM6295Reset(0);
	oki_bankswitch(0);
	BurnYM2151Reset();

	HiscoreReset();

	return 0;
}

static INT32 DrvDraw()
{
	ToaClearScreen(0);

	ToaGetBitmap();
	ToaRenderGP9001();						// Render GP9001 graphics

	ToaPalUpdate();							// Update the palette

	return 0;
}

static INT32 DrvFrame()
{
	INT32 nInterleave = 8;

	if (DrvReset) {														// Reset machine
		DrvDoReset();
	}

	// Compile digital inputs
	DrvInput[0] = 0x00;													// Buttons
	DrvInput[1] = 0x00;													// Player 1
	DrvInput[2] = 0x00;													// Player 2
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy1[i] & 1) << i;
		DrvInput[1] |= (DrvJoy2[i] & 1) << i;
		DrvInput[2] |= (DrvButton[i] & 1) << i;
	}
	ToaClearOpposites(&DrvInput[0]);
	ToaClearOpposites(&DrvInput[1]);

	SekNewFrame();

	nCyclesTotal[0] = (INT32)((INT64)10000000 * nBurnCPUSpeedAdjust / (0x0100 * 60));
	nCyclesDone[0] = 0;
	
	SekOpen(0);

	SekSetCyclesScanline(nCyclesTotal[0] / 262);
	nToaCyclesDisplayStart = nCyclesTotal[0] - ((nCyclesTotal[0] * (TOA_VBLANK_LINES + 240)) / 262);
	nToaCyclesVBlankStart = nCyclesTotal[0] - ((nCyclesTotal[0] * TOA_VBLANK_LINES) / 262);
	bool bVBlank = false;

	INT32 nSoundBufferPos = 0;

	for (INT32 i = 0; i < nInterleave; i++) {
    	INT32 nCurrentCPU;
		INT32 nNext;

		// Run 68000

		nCurrentCPU = 0;
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;

		// Trigger VBlank interrupt
		if (!bVBlank && nNext > nToaCyclesVBlankStart) {
			if (nCyclesDone[nCurrentCPU] < nToaCyclesVBlankStart) {
				nCyclesSegment = nToaCyclesVBlankStart - nCyclesDone[nCurrentCPU];
				nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
			}

			nIRQPending  = 1;
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

			ToaBufferGP9001Sprites();

			bVBlank = true;
		}

		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nIRQPending = 0;
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);

		if ((i & 1) == 0) {
			// Render sound segment
			if (pBurnSoundOut) {
				INT32 nSegmentLength = (nBurnSoundLen * i / nInterleave) - nSoundBufferPos;
				INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
				BurnYM2151Render(pSoundBuf, nSegmentLength);
				MSM6295Render(0, pSoundBuf, nSegmentLength);
				nSoundBufferPos += nSegmentLength;
			}
		}
	}

	{
		// Make sure the buffer is entirely filled.
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			if (nSegmentLength) {
				BurnYM2151Render(pSoundBuf, nSegmentLength);
				MSM6295Render(0, pSoundBuf, nSegmentLength);
			}
		}
	}
	
	SekClose();

	if (pBurnDraw != NULL) {
		DrvDraw();												// Draw screen if needed
	}

	return 0;
}

// This routine is called first to determine how much memory is needed (MemEnd-(UINT8 *)0),
// and then afterwards to set up all the pointers
static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	Rom01		= Next; Next += 0x080000;		//
	GP9001ROM[0]	= Next; Next += nGP9001ROMSize[0];	// GP9001 tile data
	MSM6295ROM	= Next; Next += 0x1800000;

	RamStart	= Next;
	Ram01		= Next; Next += 0x010000;		// CPU #0 work RAM
	RamPal		= Next; Next += 0x001000;		// palette
	GP9001RAM[0]	= Next; Next += 0x004000;
	GP9001Reg[0]	= (UINT16*)Next; Next += 0x01000 * sizeof(UINT16);
	RamEnd		= Next;

	ToaPalette	= (UINT32 *)Next; Next += nColCount * sizeof(UINT32);

	MemEnd		= Next;

	return 0;
}

static INT32 LoadRoms()
{
	// Load 68000 ROM
	BurnLoadRom(Rom01, 0, 1);
	//BurnByteswap(Rom01, 0x80000);
	
	// Load GP9001 tile data
	ToaLoadGP9001Tiles(GP9001ROM[0], 1, 2, nGP9001ROMSize[0]);

	// Load MSM6295 ADPCM data
	BurnLoadRom(MSM6295ROM + 0x0000000,  3, 1);
	BurnLoadRom(MSM6295ROM + 0x0200000,  4, 1);
	BurnLoadRom(MSM6295ROM + 0x0400000,  5, 1);
	BurnLoadRom(MSM6295ROM + 0x0600000,  6, 1);
	BurnLoadRom(MSM6295ROM + 0x0800000,  7, 1);
	BurnLoadRom(MSM6295ROM + 0x0a00000,  8, 1);
	BurnLoadRom(MSM6295ROM + 0x0c00000,  9, 1);
	BurnLoadRom(MSM6295ROM + 0x0e00000, 10, 1);
	BurnLoadRom(MSM6295ROM + 0x1000000, 11, 1);
	BurnLoadRom(MSM6295ROM + 0x1200000, 12, 1);
	BurnLoadRom(MSM6295ROM + 0x1400000, 13, 1);
	BurnLoadRom(MSM6295ROM + 0x1600000, 14, 1);

	return 0;
}

// Scan ram
static INT32 DrvScan(INT32 nAction, INT32* pnMin)
{
	if (pnMin) {						// Return minimum compatible version
		*pnMin = 0x029497;
	}

	if (nAction & ACB_VOLATILE) {		// Scan volatile data
		struct BurnArea ba;

		memset(&ba, 0, sizeof(ba));
		ba.Data		= RamStart;
		ba.nLen		= RamEnd - RamStart;
		ba.szName	= "RAM";
		BurnAcb(&ba);

		SekScan(nAction);				// scan 68000 states

		MSM6295Scan(nAction, pnMin);
		BurnYM2151Scan(nAction, pnMin);

		ToaScanGP9001(nAction, pnMin);

		SCAN_VAR(DrvInput);
		SCAN_VAR(nIRQPending);
		SCAN_VAR(bankaddress);
	}

	if (nAction & ACB_WRITE) {
		oki_bankswitch(bankaddress);
	}

	return 0;
}

static INT32 DrvInit()
{
	INT32 nLen;

#ifdef DRIVER_ROTATION
	bToaRotateScreen = false;
#endif

	nGP9001ROMSize[0] = 0x200000;

	// Find out how much memory is needed
	Mem = NULL;
	MemIndex();
	nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) {
		return 1;
	}
	memset(Mem, 0, nLen);										// blank all memory
	MemIndex();													// Index the allocated memory

	// Load the roms into memory
	if (LoadRoms()) {
		return 1;
	}

	{
		SekInit(0, 0x68000);										// Allocate 68000
		SekOpen(0);

		// Map 68000 memory:
		SekMapMemory(Rom01,		0x000000, 0x07FFFF, MAP_ROM);	// CPU 0 ROM
		SekMapMemory(Ram01,		0x100000, 0x10FFFF, MAP_RAM);
		SekMapMemory(RamPal,	0x300000, 0x300FFF, MAP_RAM);	// Palette RAM
		SekSetReadWordHandler(0, enmadaioReadWord);
		SekSetReadByteHandler(0, enmadaioReadByte);
		SekSetWriteWordHandler(0, enmadaioWriteWord);
		SekSetWriteByteHandler(0, enmadaioWriteByte);

		SekClose();
	}

	nLayer0XOffset = -0x01D6;
	nLayer1XOffset = -0x01D8;
	nLayer2XOffset = -0x01DA;

	nSpriteYOffset = 0x0001;
	ToaInitGP9001();

	//ToaExtraTextInit();

	nToaPalLen = nColCount;
	ToaPalSrc = RamPal;
	ToaPalInit();

	BurnYM2151Init(27000000 / 8);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	MSM6295Init(0, 4000000 / 132, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	DrvDoReset(); // Reset machine

	return 0;
}


// Enma Daio (Japan)

static struct BurnRomInfo enmadaioRomDesc[] = {
	{ "03n_u53.c8",		0x080000, 0x1a6ca2ee, BRF_PRG | BRF_ESS }, //  0 CPU #0 code

	{ "rom4_u30.c19",	0x100000, 0x7a012d8b, BRF_GRA },           //  1 GP9001 Tile data
	{ "rom5_u31.c18",	0x100000, 0x60b127ab, BRF_GRA },           //  2

	{ "rom6_u65.a1",	0x200000, 0xf33c6c0b, BRF_SND },           //  3 MSM6295 ADPCM data
	{ "rom7_u66.a3",	0x200000, 0x1306f8b3, BRF_SND },           //  4
	{ "rom8_u61.a4",	0x200000, 0x4f211c00, BRF_SND },           //  5
	{ "rom9_u62.a6",	0x200000, 0x292d3ef6, BRF_SND },           //  6
	{ "rom10_u67.a8",	0x200000, 0x5219bf86, BRF_SND },           //  7
	{ "rom11_u68.a10",	0x200000, 0x56fe4b1d, BRF_SND },           //  8
	{ "rom12_u63.a11",	0x200000, 0xcc48ff18, BRF_SND },           //  9
	{ "rom13_u64.a13",	0x200000, 0xa3cd181a, BRF_SND },           // 10
	{ "rom14_u69.a14",	0x200000, 0x5d8cddec, BRF_SND },           // 11
	{ "rom15_u70.a16",	0x200000, 0xc75012f5, BRF_SND },           // 12
	{ "rom16_u71.a18",	0x200000, 0xefd02b0d, BRF_SND },           // 13
	{ "rom17_u72.a19",	0x200000, 0x6b8717c3, BRF_SND },           // 14
};

STD_ROM_PICK(enmadaio)
STD_ROM_FN(enmadaio)

struct BurnDriver BurnDrvEnmadaio = {
	"enmadaio", NULL, NULL, NULL, "1993",
	"Enma Daio (Japan)\0", "Game unplayable.", "Toaplan / Taito", "Toaplan GP9001 based",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_TOAPLAN_68K_ONLY, GBF_MISC, 0,
	NULL, enmadaioRomInfo, enmadaioRomName, NULL, NULL, NULL, NULL, EnmadaioInputInfo, EnmadaioDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &ToaRecalcPalette, 0x800,
	320, 240, 4, 3
};
