#include "driver.h"
#include "burnint.h"
#include "midwunit.h"
#include "midwayic.h"
#include "dcs2k.h"
#include "tms34010_intf.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *DrvBootROM;
static UINT8 *DrvSoundROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvRAM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvPalette;
static UINT16 *DrvPaletteB;
static UINT8 *DrvVRAM;
static UINT16 *DrvVRAM16;

UINT8 nWolfUnitRecalc;
UINT8 nWolfUnitJoy1[32];
UINT8 nWolfUnitJoy2[32];
UINT8 nWolfUnitJoy3[32];
UINT8 nWolfUnitDSW[8];
static UINT32 DrvInputs[4];
int nIOShuffle[16];


static bool bCMOSWriteEnable = false;

static bool bGfxRomLarge = true;
static UINT32 nGfxBankOffset[2] = { 0x000000, 0x400000 };
static UINT32 nVideoBank = 1;

static UINT16 nDMA[32];
static UINT16 nWolfUnitCtrl = 0;



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

    DrvBootROM 	= Next;             Next += 0x80000 * 2;
	DrvSoundROM	= Next;				Next += 0x1000000;
	DrvGfxROM 	= Next;				Next += 0x2000000;
	DrvRAM		= Next;				Next += 0x80000;
	DrvNVRAM	= Next;				Next += 0xC000;
	DrvPalette	= Next;				Next += 0x10000;
	DrvPaletteB	= (UINT16*)Next;	Next += 0x10000;
	DrvVRAM		= Next;				Next += 0x400000;
	DrvVRAM16	= (UINT16*)DrvVRAM;
    MemEnd		= Next;
    return 0;
}


static UINT16 WolfUnitIoRead(UINT32 address)
{
    UINT32 offset = (address >> 4);
    offset = nIOShuffle[offset % 16];
    switch (offset) {
    case 0: return ~DrvInputs[0];
    case 1: return ~DrvInputs[1];
    case 2: return DrvInputs[2];
    case 3: return ~DrvInputs[3];
    case 4: return ((MidwaySerialPicStatus() << 12) | (Dcs2kDataRead()));
    default:
    	return ~0;
    }
}

void WolfUnitIoWrite(UINT32 address, UINT16 value)
{
    UINT32 offset = (address >> 4) % 8;
    switch(offset) {
    case 1:
    	Dcs2kResetWrite(value & 0x10);
        MidwaySerialPicReset();
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
    if (address == 0x01600000) {
        MidwaySerialPicWrite(value);
    }
}

UINT16 WolfUnitCMOSRead(UINT32 address)
{
    UINT16 *wn = (UINT16*)DrvNVRAM;
    UINT32 offset = (address - 0x01400000) >> 1;
    return wn[offset];
}


void WolfUnitCMOSWrite(UINT32 address, UINT16 value)
{
    if (bCMOSWriteEnable) {
        UINT16 *wn = (UINT16*) DrvNVRAM;
        UINT32 offset = (address - 0x01400000) >> 1;
        wn[offset] = value;
        bCMOSWriteEnable = false;
    }
}

void WolfUnitCMOSWriteEnable(UINT32 address, UINT16 value)
{
    bCMOSWriteEnable = true;
}


UINT16 WolfUnitPalRead(UINT32 address)
{
    address &= 0x7FFFF;
    return *(UINT16*)(&DrvPalette[TOBYTE(address)]);
}

void WolfUnitPalWrite(UINT32 address, UINT16 value)
{
    address &= 0x7FFFF;
    *(UINT16*)(&DrvPalette[TOBYTE(address)]) = value;

    UINT32 col = RGB555_2_888(BURN_ENDIAN_SWAP_INT16(value));
    DrvPaletteB[address>>4] = BurnHighCol(RGB888_r(col),RGB888_g(col),RGB888_b(col),0);
}



UINT16 WolfUnitVramRead(UINT32 address)
{
    UINT32 offset = TOBYTE(address & 0x3fffff);
    if (nVideoBank)
        return (DrvVRAM16[offset] & 0x00ff) | (DrvVRAM16[offset + 1] << 8);
    else
        return (DrvVRAM16[offset] >> 8) | (DrvVRAM16[offset + 1] & 0xff00);
}

void WolfUnitVramWrite(UINT32 address, UINT16 data)
{
    UINT32 offset = TOBYTE(address & 0x3fffff);
    if (nVideoBank)
    {
        DrvVRAM16[offset] = (data & 0xff) | ((nDMA[DMA_PALETTE] & 0xff) << 8);
        DrvVRAM16[offset + 1] = ((data >> 8) & 0xff) | (nDMA[DMA_PALETTE] & 0xff00);
    }
    else
    {
        DrvVRAM16[offset] = (DrvVRAM16[offset] & 0xff) | ((data & 0xff) << 8);
        DrvVRAM16[offset + 1] = (DrvVRAM16[offset + 1] & 0xff) | (data & 0xff00);
    }
}

UINT16 WolfUnitGfxRead(UINT32 address)
{
    UINT8 *base = DrvGfxROM + nGfxBankOffset[0];
    UINT32 offset = TOBYTE(address - 0x02000000);
    return base[offset] | (base[offset + 1] << 8);
}


static void WolfUnitToShift(UINT32 address, void *dst)
{
    memcpy(dst, &DrvVRAM16[(address >> 3)], 4096/2);
}

static void WolfUnitFromShift(UINT32 address, void *src)
{
    memcpy(&DrvVRAM16[(address >> 3)], src, 4096/2);
}


static int ScanlineRender(int line, TMS34010Display *info)
{
    if (!pBurnDraw)
        return 0;

    UINT16 *src = &DrvVRAM16[(info->rowaddr << 9) & 0x3FE00];
    if (info->rowaddr >= 254)
        return 0;

    int col = info->coladdr << 1;
    UINT16 *dest = (UINT16*) pBurnDraw + (info->rowaddr * 512);

    const int heblnk = info->heblnk;
    const int hsblnk = info->hsblnk;
    for (int x = heblnk; x < hsblnk; x++) {
        dest[x] = DrvPaletteB[src[col++ & 0x1FF] & 0x7FFF];
    }

    // blank
    for (int x = 0; x < heblnk; x++) {
        dest[x] = 0;
    }
    for (int x = hsblnk; x < info->htotal; x++) {
        dest[x] = 0;
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

    for (int i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
        bprintf(PRINT_NORMAL, _T("ROM %d\n"), i);
        BurnDrvGetRomInfo(&pri, i);
        if ((pri.nType & 7) == 3) {

            UINT32 addr = WUNIT_GFX_ADR(pri.nType) << 20;
            UINT32 offs = WUNIT_GFX_OFF(pri.nType);

            bprintf(PRINT_NORMAL, _T("ROM %d - %X\n"), i, addr+offs);

            if (BurnLoadRom(DrvGfxROM + addr + offs, i, 4) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

INT32 WolfUnitInit()
{

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

    for (int i = 0; i < 16; i++)
        nIOShuffle[i] = i % 8;


    Dcs2kInit();
    Dcs2kMapSoundROM(DrvSoundROM, 0x1000000);

    MidwaySerialPicInit(528);
    MidwaySerialPicReset();

    TMS34010MapReset();
    TMS34010Init();

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
    TMS34010MapHandler(4, 0x01400000, 0x0145ffff, MAP_READ | MAP_WRITE);

    TMS34010SetWriteHandler(5, WolfUnitCMOSWriteEnable);
    TMS34010MapHandler(5, 0x01480000, 0x014fffff, MAP_READ | MAP_WRITE);

    TMS34010SetHandlers(6, WolfUnitPalRead, WolfUnitPalWrite);
    TMS34010MapHandler(6, 0x01880000, 0x018fffff, MAP_READ | MAP_WRITE);

    TMS34010SetHandlers(7, TUnitDmaRead, TUnitDmaWrite);
    TMS34010MapHandler(7, 0x01a00000, 0x01a000ff, MAP_READ | MAP_WRITE);
    TMS34010MapHandler(7, 0x01a80000, 0x01a800ff, MAP_READ | MAP_WRITE);


    TMS34010SetReadHandler(8, WolfUnitGfxRead);
    TMS34010MapHandler(8, 0x02000000, 0x06ffffff, MAP_READ);

    TMS34010SetHandlers(11, WolfUnitVramRead, WolfUnitVramWrite);
    TMS34010MapHandler(11, 0x00000000, 0x003fffff, MAP_READ | MAP_WRITE);

    
    Dcs2kBoot();

    Dcs2kResetWrite(1);
    Dcs2kResetWrite(0);


    memset(DrvVRAM, 0, 0x400000);
    DrvInputs[2] = 0;

	TMS34010Reset();
    return 0;
}

static void MakeInputs()
{
    DrvInputs[0] = 0;
    DrvInputs[1] = 0;
    DrvInputs[2] = 0xFD7D | 0x0200;
    DrvInputs[3] = 0;

    for (int i = 0; i < 16; i++) {
        if (nWolfUnitJoy1[i] & 1)
            DrvInputs[0] |= (1 << i);
        if (nWolfUnitJoy2[i] & 1)
            DrvInputs[1] |= (1 << i);
        if (nWolfUnitJoy3[i] & 1)
            DrvInputs[3] |= (1 << i);
    }

    if (nWolfUnitDSW[0] & 1)
        DrvInputs[2] ^= 0x08000;
}

INT32 WolfUnitFrame()
{
	MakeInputs();
	static int line = 0;
    for (int i = 0; i < 288; i++) {
    	TMS34010Run(2893);//50000000/60/288
    	line = TMS34010GenerateScanline(line);
    }

    return 0;
}

INT32 WolfUnitExit()
{
	Dcs2kExit();
	BurnFree(AllMem);
    return 0;
}

INT32 WolfUnitDraw()
{
	// TMS34010 renders scanlines direct to pBurnDraw
	return 0;
}



INT32 WolfUnitScan(INT32 nAction, INT32 *pnMin)
{
	return 0;
}