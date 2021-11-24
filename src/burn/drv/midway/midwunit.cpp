// midway wolf unit

#include "tiles_generic.h"
#include "midwunit.h"
#include "midwayic.h"
#include "dcs2k.h"
#include "tms34_intf.h"
#include <stddef.h>

static UINT8 *AllMem;
static UINT8 *RamEnd;
static UINT8 *AllRam;
static UINT8 *MemEnd;
static UINT8 *DrvBootROM;
static UINT8 *DrvSoundROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvRAM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvPalette;
static UINT32 *DrvPaletteB;
static UINT8 *DrvVRAM;
static UINT16 *DrvVRAM16;

UINT8 nWolfUnitRecalc;
UINT8 nWolfUnitJoy1[32];
UINT8 nWolfUnitJoy2[32];
UINT8 nWolfUnitJoy3[32];
UINT8 nWolfUnitDSW[2];
UINT8 nWolfReset = 0;
static UINT32 DrvInputs[4];

static bool bGfxRomLarge = true;
static UINT32 nGfxBankOffset[2] = { 0x000000, 0x400000 };

static bool bCMOSWriteEnable = false;
static UINT32 nVideoBank = 1;
static UINT16 *nDMA;
static UINT16 nWolfUnitCtrl = 0;
static INT32 nIOShuffle[16];

static INT32 wwfmania = 0;
static INT32 is_umk3 = 0;

static INT32 nExtraCycles = 0;

static INT32 vb_start = 0;


#define RGB888(r,g,b)   ((r) | ((g) << 8) | ((b) << 16))
#define RGB888_r(x) ((x) & 0xFF)
#define RGB888_g(x) (((x) >>  8) & 0xFF)
#define RGB888_b(x) (((x) >> 16) & 0xFF)

#define RGB555_2_888(x)     \
    RGB888((x >> 7) & 0xF8, \
           (x >> 2) & 0xF8, \
           (x << 3) & 0xF8)

#define RGB888_2_565(x)  (          \
    ((RGB888_r(x) << 8) & 0xF800) | \
    ((RGB888_g(x) << 3) & 0x07E0) | \
    ((RGB888_b(x) >> 3)))

#include "midtunit_dma.h"

static INT32 MemIndex()
{
    UINT8 *Next; Next = AllMem;

    DrvBootROM 	= Next;             Next += 0x800000 * sizeof(UINT8);
	DrvSoundROM	= Next;				Next += 0x1000000 * sizeof(UINT8);
	DrvGfxROM 	= Next;				Next += 0x2000000 * sizeof(UINT8);

	DrvNVRAM	= Next;             Next += 0x8000 * sizeof(UINT16);

	AllRam		= Next;
	DrvRAM		= Next;				Next += TOBYTE(0x400000) * sizeof(UINT16);
	DrvPalette	= Next;				Next += 0x20000 * sizeof(UINT8);
	DrvPaletteB	= (UINT32*)Next;	Next += 0x8000 * sizeof(UINT32);
	DrvVRAM		= Next;				Next += 0x80000 * sizeof(UINT16);
	DrvVRAM16	= (UINT16*)DrvVRAM;

	nDMA        = (UINT16*)Next;    Next += 0x0020 * sizeof(UINT16);
	dma_state   = (dma_state_s*)Next; Next += sizeof(dma_state_s);

	RamEnd		= Next;

	MemEnd		= Next;
    return 0;
}

static void sound_sync()
{
	INT32 cyc = ((double)TMS34010TotalCycles() / 63 * 100) - Dcs2kTotalCycles();
	if (cyc > 0) {
		Dcs2kRun(cyc);
	}
}

static void sound_sync_end()
{
	INT32 cyc = ((double)10000000 * 100 / nBurnFPS) - Dcs2kTotalCycles();
	if (cyc > 0) {
		Dcs2kRun(cyc);
	}
}


static UINT16 WolfUnitIoRead(UINT32 address)
{
    UINT32 offset = (address >> 4);
    offset = nIOShuffle[offset % 16];
    switch (offset) {
		case 0: return ~DrvInputs[0];
		case 1: return ~DrvInputs[1];
		case 2: return nWolfUnitDSW[0] | (nWolfUnitDSW[1] << 8);
		case 3: return ~DrvInputs[3];
		case 4: {
			sound_sync();
			return ((MidwaySerialPicStatus() << 12) | (Dcs2kControlRead() & 0xfff));
		}
	default:
		return ~0;
	}
}

void WolfUnitIoWrite(UINT32 address, UINT16 value)
{
	if (wwfmania && address <= 0x180000f) {
		for (INT32 i = 0; i < 16; i++) nIOShuffle[i] = i % 8;

		switch (value) {
			case 1: {
				nIOShuffle[0x04] = 0;
				nIOShuffle[0x08] = 1;
				nIOShuffle[0x01] = 2;
				nIOShuffle[0x09] = 3;
				nIOShuffle[0x02] = 4;
				break;
			}
			case 2: {
				nIOShuffle[0x08] = 0;
				nIOShuffle[0x02] = 1;
				nIOShuffle[0x04] = 2;
				nIOShuffle[0x06] = 3;
				nIOShuffle[0x01] = 4;
				break;
			}
			case 3: {
				nIOShuffle[0x01] = 0;
				nIOShuffle[0x08] = 1;
				nIOShuffle[0x02] = 2;
				nIOShuffle[0x0A] = 3;
				nIOShuffle[0x05] = 4;
				break;
			}
			case 4: {
				nIOShuffle[0x02] = 0;
				nIOShuffle[0x04] = 1;
				nIOShuffle[0x01] = 2;
				nIOShuffle[0x07] = 3;
				nIOShuffle[0x08] = 4;
				break;
			}
		}

		return;
	}

	UINT32 offset = nIOShuffle[(address >> 4) % 16] % 8;

	switch(offset) {
		case 1:
			sound_sync();
			Dcs2kResetWrite(value & 0x10);
			if (value & 0x20) MidwaySerialPicReset();
			break;
    }
}

UINT16 WolfUnitCtrlRead(UINT32 address)
{
    return nWolfUnitCtrl;
}

void WolfUnitCtrlWrite(UINT32 address, UINT16 value)
{
    nWolfUnitCtrl = value;
    nGfxBankOffset[0] = 0x800000 * ((nWolfUnitCtrl >> 8) & 3);
    nVideoBank = (nWolfUnitCtrl >> 11) & 1;
}

UINT16 WolfUnitSecurityRead(UINT32 address)
{
    return MidwaySerialPicRead();
}

void WolfUnitSecurityWrite(UINT32 address, UINT16 value)
{
	if (address == 0x1600000) {
		MidwaySerialPicWrite(value);
    }
}

UINT16 WolfUnitCMOSRead(UINT32 address)
{
    UINT16 *wn = (UINT16*)DrvNVRAM;
	UINT32 offset = (address & 0x07ffff) >> 4;
    return wn[offset];
}

void WolfUnitCMOSWrite(UINT32 address, UINT16 value)
{
    if (bCMOSWriteEnable) {
		UINT16 *wn = (UINT16*)DrvNVRAM;
		UINT32 offset = (address & 0x07ffff) >> 4;
		wn[offset] = value;
		bCMOSWriteEnable = false;
    }
}

void WolfUnitCMOSWriteEnable(UINT32 address, UINT16 value)
{
	bCMOSWriteEnable = true;
}

void WolfUnitUMK3PaletteHack(UINT32 address, UINT16 value)
{
	if (address >= 0x0106a060 && address <= 0x0106a09f) {
		tms34010_modify_timeslice(-100);
	}
	address &= 0xFFF;
	*(UINT16*)(&DrvRAM[TOBYTE(0x6a000 + address)]) = value;
}

UINT16 WolfUnitPalRead(UINT32 address)
{
    address &= 0x7FFFF;
    return BURN_ENDIAN_SWAP_INT16(*(UINT16*)(&DrvPalette[TOBYTE(address)]));
}

void WolfUnitPalWrite(UINT32 address, UINT16 value)
{
    address &= 0x7FFFF;
    *(UINT16*)(&DrvPalette[TOBYTE(address)]) = BURN_ENDIAN_SWAP_INT16(value);

    UINT32 col = RGB555_2_888(value);
    DrvPaletteB[address>>4] = BurnHighCol(RGB888_r(col),RGB888_g(col),RGB888_b(col),0);
}

void WolfUnitPalRecalc()
{
	for (INT32 i = 0; i < 0x10000; i += 2) {
		UINT16 value = *(UINT16*)(&DrvPalette[i]);

		UINT32 col = RGB555_2_888(BURN_ENDIAN_SWAP_INT16(value));
		DrvPaletteB[i>>1] = BurnHighCol(RGB888_r(col),RGB888_g(col),RGB888_b(col),0);
	}
}

UINT16 WolfUnitVramRead(UINT32 address)
{
    UINT32 offset = TOBYTE(address & 0x3fffff);
    if (nVideoBank)
        return (BURN_ENDIAN_SWAP_INT16(DrvVRAM16[offset]) & 0x00ff) | (BURN_ENDIAN_SWAP_INT16(DrvVRAM16[offset + 1]) << 8);
    else
        return (BURN_ENDIAN_SWAP_INT16(DrvVRAM16[offset]) >> 8) | (BURN_ENDIAN_SWAP_INT16(DrvVRAM16[offset + 1]) & 0xff00);
}

void WolfUnitVramWrite(UINT32 address, UINT16 data)
{
    UINT32 offset = TOBYTE(address & 0x3fffff);
    if (nVideoBank)
    {
        DrvVRAM16[offset] = BURN_ENDIAN_SWAP_INT16((data & 0xff) | ((nDMA[DMA_PALETTE] & 0xff) << 8));
        DrvVRAM16[offset + 1] = BURN_ENDIAN_SWAP_INT16(((data >> 8) & 0xff) | (nDMA[DMA_PALETTE] & 0xff00));
    }
    else
    {
        DrvVRAM16[offset] = BURN_ENDIAN_SWAP_INT16((BURN_ENDIAN_SWAP_INT16(DrvVRAM16[offset]) & 0xff) | ((data & 0xff) << 8));
        DrvVRAM16[offset + 1] = BURN_ENDIAN_SWAP_INT16((BURN_ENDIAN_SWAP_INT16(DrvVRAM16[offset + 1]) & 0xff) | (data & 0xff00));
    }
}

UINT16 WolfUnitGfxRead(UINT32 address)
{
    UINT8 *base = DrvGfxROM + nGfxBankOffset[0];
    UINT32 offset = TOBYTE(address - 0x02000000);
    return base[offset] | (base[offset + 1] << 8);
}

UINT16 WolfSoundRead(UINT32 address)
{
	sound_sync();
	UINT16 dr = Dcs2kDataRead();
	Dcs2kRun(20); // "sync" in frame will even things out.
	return dr & 0xff;
}

void WolfSoundWrite(UINT32 address, UINT16 value)
{
	if (address & 0x1f) return; // ignore bad writes
	sound_sync();
	Dcs2kDataWrite(value & 0xff);
	Dcs2kRun(20);
}

static void WolfUnitToShift(UINT32 address, UINT16 *dst)
{
	memcpy(dst, &DrvVRAM16[(address >> 3)], 4096/2);
}

static void WolfUnitFromShift(UINT32 address, UINT16 *src)
{
	memcpy(&DrvVRAM16[(address >> 3)], src, 4096/2);
}


static INT32 ScanlineRender(INT32 line, TMS34010Display *info)
{
	if (!pBurnDraw)
		return 0;

	vb_start = info->vsblnk;

#if 0
	if (line == 0x15) {
		bprintf(0, _T("ENAB %d\n"), info->enabled);
		bprintf(0, _T("he %d\n"), info->heblnk);
		bprintf(0, _T("hs %d\n"), info->hsblnk);
		bprintf(0, _T("ve %d\n"), info->veblnk);
		bprintf(0, _T("vs %d\n"), info->vsblnk);
		bprintf(0, _T("vt %d\n"), info->vtotal);
		bprintf(0, _T("ht %d\n"), info->htotal);
	}
#endif

	line -= 0x14; // offset

	INT32 nHeight = nScreenHeight;
	if (nHeight > 254) nHeight = 254;

	if (line < 0 || line >= nHeight)
		return 0;

	UINT16 *src = &DrvVRAM16[(info->rowaddr << 9) & 0x3FE00];
	INT32 col = info->coladdr << 1;
	UINT16 *dest = (UINT16*) pTransDraw + (line * nScreenWidth);

	INT32 heblnk = info->heblnk;
	INT32 hsblnk = info->hsblnk;

	if (!info->enabled) heblnk = hsblnk; // blank line!

	if ((hsblnk - heblnk) < nScreenWidth) {
		for (INT32 x = 0; x < nScreenWidth; x++) {
			dest[x] = 0;
		}
	}

	for (INT32 x = heblnk; x < hsblnk; x++) {
		if ((x - heblnk) >= nScreenWidth) break;
		dest[x - heblnk] = BURN_ENDIAN_SWAP_INT16(src[col++ & 0x1FF] & BURN_ENDIAN_SWAP_INT16(0x7FFF));
	}

	return 0;
}


static INT32 LoadSoundBanks()
{
    memset(DrvSoundROM, 0xFF, 0x1000000);
    if (BurnLoadRom(DrvSoundROM + 0x000000, 2, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0x200000, 3, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0x400000, 4, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0x600000, 5, 2)) return 1;

    return 0;
}

static INT32 LoadGfxBanks()
{
    char *pRomName;
    struct BurnRomInfo pri;

    for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
        //bprintf(PRINT_NORMAL, _T("ROM %d\n"), i);
        BurnDrvGetRomInfo(&pri, i);
        if ((pri.nType & 7) == 3) {

            UINT32 addr = WUNIT_GFX_ADR(pri.nType) << 20;
            UINT32 offs = WUNIT_GFX_OFF(pri.nType);

            //bprintf(PRINT_NORMAL, _T("ROM %d - %X\n"), i, addr+offs);

            if (BurnLoadRom(DrvGfxROM + addr + offs, i, 4) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static void WolfDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	MidwaySerialPicReset();

	bCMOSWriteEnable = false;
	nVideoBank = 1;
	nWolfUnitCtrl = 0;

	nGfxBankOffset[0] = 0x000000;
	nGfxBankOffset[1] = 0x400000;

	TMS34010Open(0);
	TMS34010Reset();
	TMS34010Close();

	Dcs2kReset();

	nExtraCycles = 0;
}

INT32 WolfUnitInit()
{
	BurnSetRefreshRate(54.71);

    MemIndex();
    INT32 nLen = MemEnd - (UINT8 *)0;

    if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL)
        return 1;

    MemIndex();

    UINT32 nRet;
    nRet = BurnLoadRom(DrvBootROM + 0, 0, 2);
    if (nRet != 0)
        return 1;

    nRet = BurnLoadRom(DrvBootROM + 1, 1, 2);
    if (nRet != 0)
        return 1;

    nRet = LoadSoundBanks();
    if (nRet != 0)
        return 1;

    nRet = LoadGfxBanks();
    if (nRet != 0)
        return 1;

    for (INT32 i = 0; i < 16; i++) nIOShuffle[i] = i % 8;

	wwfmania = (strstr(BurnDrvGetTextA(DRV_NAME), "wwfmania") ? 1 : 0);
	is_umk3 = (strstr(BurnDrvGetTextA(DRV_NAME), "umk3") ? 1 : 0);

    Dcs2kInit(DCS_8K, MHz(10));
    Dcs2kMapSoundROM(DrvSoundROM, 0x1000000);
	Dcs2kSetVolume(5.25);

    MidwaySerialPicInit(528);
	MidwaySerialPicReset();

	midtunit_cpurate = 50000000/8; // midtunit_dma.h

	TMS34010Init(0);
	TMS34010Open(0);
	TMS34010SetPixClock(8000000, 1);
	TMS34010SetCpuCyclesPerFrame((INT32)(midtunit_cpurate/54.71));
	TMS34010TimerSetCB(TUnitDmaCallback);

    TMS34010SetScanlineRender(ScanlineRender);
    TMS34010SetToShift(WolfUnitToShift);
    TMS34010SetFromShift(WolfUnitFromShift);

    TMS34010MapMemory(DrvBootROM, 0xFF800000, 0xFFFFFFFF, MAP_READ);
    TMS34010MapMemory(DrvRAM,     0x01000000, 0x013FFFFF, MAP_READ | MAP_WRITE);

    TMS34010SetHandlers(1, WolfUnitIoRead, WolfUnitIoWrite);
    TMS34010MapHandler(1, 0x01800000, 0x0187ffff, MAP_READ | MAP_WRITE);

    TMS34010SetHandlers(2, WolfUnitCtrlRead, WolfUnitCtrlWrite);
    TMS34010MapHandler(2, 0x01b00000, 0x01b0001f, MAP_READ | MAP_WRITE);

    TMS34010SetHandlers(3, WolfUnitSecurityRead, WolfUnitSecurityWrite);
    TMS34010MapHandler(3, 0x01600000, 0x0160001f, MAP_READ | MAP_WRITE);

    TMS34010SetHandlers(4, WolfUnitCMOSRead, WolfUnitCMOSWrite);
    TMS34010MapHandler(4, 0x01400000, 0x0147ffff, MAP_READ | MAP_WRITE);

    TMS34010SetWriteHandler(5, WolfUnitCMOSWriteEnable);
    TMS34010MapHandler(5, 0x01480000, 0x014fffff, MAP_READ | MAP_WRITE);

    TMS34010SetHandlers(6, WolfUnitPalRead, WolfUnitPalWrite);
    TMS34010MapHandler(6, 0x01880000, 0x018fffff, MAP_READ | MAP_WRITE);

    TMS34010SetHandlers(7, TUnitDmaRead, TUnitDmaWrite);
    TMS34010MapHandler(7, 0x01a00000, 0x01a000ff, MAP_READ | MAP_WRITE);
    TMS34010MapHandler(7, 0x01a80000, 0x01a800ff, MAP_READ | MAP_WRITE);

    TMS34010SetReadHandler(8, WolfUnitGfxRead);
    TMS34010MapHandler(8, 0x02000000, 0x06ffffff, MAP_READ);
	
	TMS34010SetHandlers(9, WolfSoundRead, WolfSoundWrite);
    TMS34010MapHandler(9, 0x01680000, 0x0168001f, MAP_READ | MAP_WRITE);

    TMS34010SetHandlers(11, WolfUnitVramRead, WolfUnitVramWrite);
    TMS34010MapHandler(11, 0x00000000, 0x003fffff, MAP_READ | MAP_WRITE);

	if (is_umk3) {
		bprintf(0, _T("*** UMK3 Palette Fix active.\n"));
		TMS34010SetWriteHandler(12, WolfUnitUMK3PaletteHack);
		TMS34010MapHandler(12, 0x0106a000, 0x0106afff, MAP_WRITE);
	}

	TMS34010Close();

	GenericTilesInit();

	WolfDoReset();

    return 0;
}

static void MakeInputs()
{
    DrvInputs[0] = 0;
    DrvInputs[1] = 0;
    DrvInputs[2] = 0; // not used
    DrvInputs[3] = 0;

    for (INT32 i = 0; i < 16; i++) {
        if (nWolfUnitJoy1[i] & 1) DrvInputs[0] |= (1 << i);
        if (nWolfUnitJoy2[i] & 1) DrvInputs[1] |= (1 << i);
        if (nWolfUnitJoy3[i] & 1) DrvInputs[3] |= (1 << i);
    }
}

static void HandleDCSIRQ(INT32 line)
{
	if (line == 0 || line == 96 || line == 192) DcsCheckIRQ();
}

INT32 WolfUnitFrame()
{
	if (nWolfReset) WolfDoReset();

	MakeInputs();

	TMS34010NewFrame();
	Dcs2kNewFrame();

	INT32 nInterleave = 288;
	INT32 nCyclesTotal[2] = { (INT32)(50000000/8/54.706840), (INT32)(10000000 / 54.706840) };
	INT32 nCyclesDone[2] = { nExtraCycles, 0 };

	TMS34010Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 our_line = (i + 274) % 289; // start at vblank

		CPU_RUN(0, TMS34010);

		TMS34010GenerateScanline(our_line);

		HandleDCSIRQ(i);

		sound_sync(); // sync to main cpu
		if (i == nInterleave - 1)
			sound_sync_end();
	}

	if (pBurnDraw) {
		WolfUnitDraw();
	}

	nExtraCycles = nCyclesDone[0] - nCyclesTotal[0];

	TMS34010Close();

	if (pBurnSoundOut) {
		Dcs2kRender(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

INT32 WolfUnitExit()
{
	Dcs2kExit();
	BurnFree(AllMem);

	TMS34010Exit();

	GenericTilesExit();

	wwfmania = 0;
	is_umk3 = 0;

    return 0;
}

INT32 WolfUnitDraw()
{
	if (nWolfUnitRecalc) {
		WolfUnitPalRecalc();
		nWolfUnitRecalc = 0;
	}

	// TMS34010 renders scanlines direct to pTransDraw
	BurnTransferCopy(DrvPaletteB);

	return 0;
}

INT32 WolfUnitScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		TMS34010Scan(nAction);

		Dcs2kScan(nAction, pnMin);
		MidwaySerialPicScan(nAction, pnMin);

		SCAN_VAR(nVideoBank);
		SCAN_VAR(nWolfUnitCtrl);
		SCAN_VAR(bCMOSWriteEnable);
		SCAN_VAR(nGfxBankOffset);
		SCAN_VAR(nIOShuffle);
		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x8000;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE) {
		//
	}

	return 0;
}
