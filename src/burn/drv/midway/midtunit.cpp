#include "tiles_generic.h"
#include "midtunit.h"
#include "tms34010_intf.h"
#include "m6809_intf.h"
#include "burn_ym2151.h"
#include "dcs2k.h"
#include "msm6295.h"
#include "dac.h"

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
static UINT8 *DrvSoundProgROM;
static UINT8 *DrvSoundProgRAM;
static UINT8 *DrvSoundProgRAMProt;

UINT8 nTUnitRecalc;
UINT8 nTUnitJoy1[32];
UINT8 nTUnitJoy2[32];
UINT8 nTUnitJoy3[32];
UINT8 nTUnitDSW[8];
UINT8 nTUnitReset = 0;
static UINT32 DrvInputs[4];

static bool bGfxRomLarge = false;
static UINT32 nGfxBankOffset[2] = { 0x000000, 0x400000 };

static bool bCMOSWriteEnable = false;
static UINT32 nVideoBank = 1;
static UINT16 nDMA[32];
static UINT16 nTUnitCtrl = 0;
static UINT8 DrvFakeSound = 0;

static UINT8 MKProtIndex = 0;
static UINT16 MK2ProtData = 0;

UINT8 TUnitIsMK = 0;
UINT8 TUnitIsMK2 = 0;

static INT32 nSoundType = 0;

enum {
	SOUND_ADPCM = 0,
    SOUND_DCS
};

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
	DrvSoundProgROM = Next;			Next += 0x0040000 * sizeof(UINT8);

	DrvNVRAM	= Next;				Next += 0x002000 * sizeof(UINT16);

	AllRam		= Next;
	DrvRAM		= Next;				Next += 0x400000 * sizeof(UINT16);
	DrvSoundProgRAM = Next;			Next += 0x002000 * sizeof(UINT8);
	DrvSoundProgRAMProt = Next;		Next += 0x0000ff * sizeof(UINT8);
	DrvPalette	= Next;				Next += 0x20000 * sizeof(UINT8);
	DrvPaletteB	= (UINT32*)Next;	Next += 0x8000 * sizeof(UINT32);
	DrvVRAM		= Next;				Next += 0x400000 * sizeof(UINT16);
	DrvVRAM16	= (UINT16*)DrvVRAM;
	RamEnd		= Next;

	MemEnd		= Next;
    return 0;
}

// forwards
static void MKsound_bankswitch(INT32 bank);
static void MKsound_reset(INT32 local);
static void MKsound_reset_write(INT32 val);
static void MKsound_main2soundwrite(INT32 data);
static void MKsound_msm6295bankswitch(INT32 bank);


static INT32 LoadGfxBanks()
{
    char *pRomName;
    struct BurnRomInfo pri;

    for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
        BurnDrvGetRomInfo(&pri, i);
        if ((pri.nType & 7) == 3) {
			if (pri.nLen > 0x80000) bGfxRomLarge = true;
		
            UINT32 addr = TUNIT_GFX_ADR(pri.nType) << 20;
            UINT32 offs = TUNIT_GFX_OFF(pri.nType);

            if (BurnLoadRom(DrvGfxROM + addr + offs, i, 4) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static INT32 LoadSoundProgRom()
{
    char *pRomName;
    struct BurnRomInfo pri;

    for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
        BurnDrvGetRomInfo(&pri, i);
        if ((pri.nType & 7) == 4) {
            if (BurnLoadRom(DrvSoundProgROM, i, 1) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static INT32 LoadSoundBanksMK()
{
    char *pRomName;
    struct BurnRomInfo pri;
	INT32 offs = 0;
	INT32 bank = 0;

    for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
        BurnDrvGetRomInfo(&pri, i);
		if ((pri.nType & 7) == 2) {
			bprintf(0, _T("MKsound: loading soundbank %d @ %x\n"), bank, offs);
            if (BurnLoadRom(DrvSoundROM + offs, i, 1) != 0) {
                return 1;
			}
			offs += pri.nLen;
			bprintf(0, _T("MKsound: loading soundbank %d @ %x\n"), bank, offs);
            if (BurnLoadRom(DrvSoundROM + offs, i, 1) != 0) {
                return 1;
            }
			offs += pri.nLen;
			bank++;
        }
    }
    return 0;
}

static void TUnitToShift(UINT32 address, void *dst)
{
    memcpy(dst, &DrvVRAM16[(address >> 3)], 4096/2);
}

static void TUnitFromShift(UINT32 address, void *src)
{
    memcpy(&DrvVRAM16[(address >> 3)], src, 4096/2);
}

static void TUnitDoReset()
{
	TMS34010Reset();
	
	switch (nSoundType)	{
		case SOUND_ADPCM:
			MKsound_reset(0);
			break;

		case SOUND_DCS:
			Dcs2kResetWrite(1);
			Dcs2kResetWrite(0);
			break;
	}
	
	MKProtIndex = 0;
	MK2ProtData = 0;
	DrvFakeSound = 0;
}

static UINT16 TUnitVramRead(UINT32 address)
{
    UINT32 offset = TOBYTE(address & 0x3fffff);
    if (nVideoBank)
        return (DrvVRAM16[offset] & 0x00ff) | (DrvVRAM16[offset + 1] << 8);
    else
        return (DrvVRAM16[offset] >> 8) | (DrvVRAM16[offset + 1] & 0xff00);
}

static void TUnitVramWrite(UINT32 address, UINT16 data)
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

static UINT16 TUnitPalRead(UINT32 address)
{
    address &= 0x7FFFF;
    return *(UINT16*)(&DrvPalette[TOBYTE(address)]);
}

static void TUnitPalWrite(UINT32 address, UINT16 value)
{
    address &= 0x7FFFF;
    *(UINT16*)(&DrvPalette[TOBYTE(address)]) = value;

    UINT32 col = RGB555_2_888(BURN_ENDIAN_SWAP_INT16(value));
    DrvPaletteB[address>>4] = BurnHighCol(RGB888_r(col),RGB888_g(col),RGB888_b(col),0);
}

static void TUnitPalRecalc()
{
	for (INT32 i = 0; i < 0x10000; i += 2) {
		UINT16 value = *(UINT16*)(&DrvPalette[i]);

		UINT32 col = RGB555_2_888(BURN_ENDIAN_SWAP_INT16(value));
		DrvPaletteB[i>>1] = BurnHighCol(RGB888_r(col),RGB888_g(col),RGB888_b(col),0);
	}
}

static INT32 ScanlineRender(INT32 line, TMS34010Display *info)
{
    if (!pBurnDraw) return 0;

	UINT16 *src = &DrvVRAM16[(info->rowaddr << 9) & 0x3FE00];

    if (info->rowaddr >= nScreenHeight) return 0;

    INT32 col = info->coladdr << 1;
    UINT16 *dest = (UINT16*) pTransDraw + (info->rowaddr * nScreenWidth);

    const INT32 heblnk = info->heblnk;
    const INT32 hsblnk = info->hsblnk * 2; // T-Unit is 2 pixels per clock
    for (INT32 x = heblnk; x < hsblnk; x++) {
        dest[x - heblnk] = src[col++ & 0x1FF] & 0x7FFF;
    }

    return 0;
}

static UINT16 TUnitRead(UINT32 address)
{
	if (address == 0x01600040) return 0xff; // ???
	if (address == 0x01d81070) return 0xff; // watchdog

	if (address == 0x01f00000) return 0; // ?

	bprintf(PRINT_NORMAL, _T("Read %x\n"), address);

	return ~0;
}

static void TUnitWrite(UINT32 address, UINT16 value)
{
	if (address == 0x01d81070) return; // watchdog
	
	bprintf(PRINT_NORMAL, _T("Write %x, %x\n"), address, value);
}

static UINT16 TUnitInputRead(UINT32 address)
{
	INT32 offset = (address & 0xff) >> 4;
	
	switch (offset) {
		case 0x00: return ~DrvInputs[0];
		case 0x01: return ~DrvInputs[1];
		case 0x02: return ~DrvInputs[2];
		case 0x03: return nTUnitDSW[0] | (nTUnitDSW[1] << 8);
	}

	return ~0;
}

static UINT16 TUnitGfxRead(UINT32 address)
{
	//uint8_t *base = m_gfxrom->base() + gfxbank_offset[(offset >> 21) & 1];
	UINT8 *base = DrvGfxROM + nGfxBankOffset[0];
    UINT32 offset = TOBYTE(address - 0x02000000);
    return base[offset] | (base[offset + 1] << 8);
}

static UINT16 TUnitSoundStateRead(UINT32 address)
{
	//bprintf(PRINT_NORMAL, _T("Sound State Read %x\n"), address);
	if (address >= 0x1d00000 && address <= 0x1d0001f) {
		if (nSoundType == SOUND_DCS)
			return Dcs2kControlRead() >> 4;

		if (DrvFakeSound) {
			DrvFakeSound--;
			return 0;
		}
	}

	return ~0;
}

static UINT16 TUnitSoundRead(UINT32 address)
{
	if (address >= 0x01d01020 && address <= 0x01d0103f) {
		if (nSoundType == SOUND_DCS)
			return Dcs2kDataRead() & 0xff;
	}
	//bprintf(PRINT_NORMAL, _T("Sound Read %x\n"), address);

	return ~0;
}

static void TUnitSoundWrite(UINT32 address, UINT16 value)
{
	//bprintf(PRINT_NORMAL, _T("Sound Write %x, %x\n"), address, value);
	if (address >= 0x01d01020 && address <= 0x01d0103f) {
		switch (nSoundType)	{
			case SOUND_ADPCM:
				MKsound_reset_write(~value & 0x100);
				MKsound_main2soundwrite(value & 0xff);

				DrvFakeSound = 128;
				break;

			case SOUND_DCS:
				Dcs2kResetWrite(~value & 0x100);
				Dcs2kDataWrite(value & 0xff);

				DrvFakeSound = 128;
				break;
		}
	}
}

static void TUnitCtrlWrite(UINT32 address, UINT16 value)
{
	nTUnitCtrl = value;
    
	if (!(nTUnitCtrl & 0x0080) || !bGfxRomLarge) {
		nGfxBankOffset[0] = 0x000000;
	} else {
		nGfxBankOffset[0] = 0x800000;
	}
	
    nVideoBank = (nTUnitCtrl >> 5) & 1;
}

static void TUnitCMOSWriteEnable(UINT32 address, UINT16 value)
{
    bCMOSWriteEnable = true;
}

static UINT16 TUnitCMOSRead(UINT32 address)
{
    UINT16 *wn = (UINT16*)DrvNVRAM;
	UINT32 offset = (address & 0x01ffff) >> 4;
    return wn[offset];
}


static void TUnitCMOSWrite(UINT32 address, UINT16 value)
{
	UINT16 *wn = (UINT16*)DrvNVRAM;
	UINT32 offset = (address & 0x01ffff) >> 4;
	wn[offset] = value;
}

static const UINT8 mk_prot_values[] =
{
	0x13, 0x27, 0x0f, 0x1f, 0x3e, 0x3d, 0x3b, 0x37,
	0x2e, 0x1c, 0x38, 0x31, 0x22, 0x05, 0x0a, 0x15,
	0x2b, 0x16, 0x2d, 0x1a, 0x34, 0x28, 0x10, 0x21,
	0x03, 0x06, 0x0c, 0x19, 0x32, 0x24, 0x09, 0x13,
	0x27, 0x0f, 0x1f, 0x3e, 0x3d, 0x3b, 0x37, 0x2e,
	0x1c, 0x38, 0x31, 0x22, 0x05, 0x0a, 0x15, 0x2b,
	0x16, 0x2d, 0x1a, 0x34, 0x28, 0x10, 0x21, 0x03,
	0xff
};

static UINT16 MKProtRead(UINT32 /*address*/)
{
	if (MKProtIndex >= sizeof(mk_prot_values)) {
		MKProtIndex = 0;
	}

	return mk_prot_values[MKProtIndex++] << 9;
}


static void MKProtWrite(UINT32 /*address*/, UINT16 value)
{
	INT32 first_val = (value >> 9) & 0x3f;
	INT32 i;

	for (i = 0; i < sizeof(mk_prot_values); i++) {
		if (mk_prot_values[i] == first_val) {
			MKProtIndex = i;
			break;
		}
	}

	if (i == sizeof(mk_prot_values)) {
		MKProtIndex = 0;
	}
}


// williams adpcm-compat. sound section (MK)
static INT32 sound_latch;
static INT32 sound_talkback;
static INT32 sound_irqstate;
static INT32 sound_bank;
static INT32 sound_msm6295bank;
static INT32 sound_inreset;

static void MKsound_reset_write(INT32 val)
{
	if (val) {
		MKsound_reset(1);
		sound_inreset = 1;
	} else {
		sound_inreset = 0;
	}
}

static void MKsound_reset(INT32 local)
{
	sound_latch = 0;
	sound_irqstate = 0;
	sound_talkback = 0;
	sound_inreset = 0;

	if (!local) M6809Open(0);
	MKsound_bankswitch(0);
	MKsound_msm6295bankswitch(0);
	M6809Reset();
	if (!local) M6809Close();

	BurnYM2151Reset();
	DACReset();
	MSM6295Reset();
}

static void MKsound_main2soundwrite(INT32 data)
{
	sound_latch = data & 0xff;
	if (~data & 0x200) {
		M6809SetIRQLine(M6809_IRQ_LINE, CPU_IRQSTATUS_ACK);
		sound_irqstate = 1;
	}
}

static void MKsound_msm6295bankswitch(INT32 bank)
{
	bank &= 7;
	sound_msm6295bank = bank;

	INT32 bank_offs = 0;
	switch (bank) {
		case 0:
		case 1: bank_offs = 0x40000; break;
		case 2: bank_offs = 0x20000; break;
		case 3: bank_offs = 0x00000; break;
		case 4: bank_offs = 0xe0000; break;
		case 5: bank_offs = 0xc0000; break;
		case 6: bank_offs = 0xa0000; break;
		case 7: bank_offs = 0x80000; break;
	}

	MSM6295SetBank(0, DrvSoundROM + 0x60000, 0x20000, 0x3ffff); // static bank
	MSM6295SetBank(0, DrvSoundROM + bank_offs, 0x00000, 0x1ffff); // dynamic bank
}

static void MKsound_bankswitch(INT32 bank)
{
	sound_bank = bank & 7;
}

static void MKsound_statescan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);
		SCAN_VAR(sound_latch);
		SCAN_VAR(sound_talkback);
		SCAN_VAR(sound_irqstate);
		SCAN_VAR(sound_bank);
		SCAN_VAR(sound_msm6295bank);
	}

	if (nAction & ACB_WRITE) {
		M6809Open(0);
		MKsound_bankswitch(sound_bank);
		M6809Close();
		MKsound_msm6295bankswitch(sound_msm6295bank);
	}
}

static void MKYM2151IrqHandler(INT32 Irq)
{
	M6809SetIRQLine(M6809_FIRQ_LINE, (Irq) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static UINT8 MKSoundRead(UINT16 address)
{

	if (address >= 0x4000 && address <= 0xbfff) {
		return DrvSoundProgROM[(sound_bank * 0x8000) + (address - 0x4000)];
	}

	if (address >= 0xc000 && address <= 0xffff) {
		if (address >= 0xfb9c && address <= 0xfbc6) { // MK sound protection ram.
			return DrvSoundProgRAMProt[address - 0xfb9c];
		}
		return DrvSoundProgROM[(0x4000 + 7 * 0x8000) + (address - 0xc000)];
	}

	address &= ~0x3ff; // mirroring

	switch (address) {
		case 0x2000: {
			return 0; // NOP?
		}
		case 0x2400: {
			return BurnYM2151Read();
		}
		case 0x2c00: {
			return MSM6295Read(0);
		}
		case 0x3000: {
			M6809SetIRQLine(M6809_IRQ_LINE, CPU_IRQSTATUS_NONE);
			sound_irqstate = 0; // if self-test fails, delay this! (not on this hw, though)
			return sound_latch;
		}
	}

	bprintf(PRINT_NORMAL, _T("M6809 Read Byte -> %04X\n"), address);

	return 0;
}


static void MKSoundWrite(UINT16 address, UINT8 value)
{
	if (address >= 0xfb9c && address <= 0xfbc6) { // MK sound protection ram.
		DrvSoundProgRAMProt[address - 0xfb9c] = value;
	}

	if (address >= 0x4000) return; // ROM

	if ((address & ~0x3ff) == 0x2400) // mirroring
		address &= ~0x3fe; // ym
	else
		address &= ~0x3ff; // everything else


	switch (address) {
		case 0x2000: {
			MKsound_bankswitch(value);
			return;
		}

		case 0x2400:
		case 0x2401: {
			BurnYM2151Write(address&1, value);
			return;
		}

		case 0x2800: {
			DACSignedWrite(0, value);
			return;
		}

		case 0x2c00: {
			MSM6295Write(0, value);
			return;
		}

		case 0x3400: {
			MKsound_msm6295bankswitch(value);
			return;
		}

		case 0x3c00: {
			sound_talkback = value;
			return;
		}
	}

	bprintf(PRINT_NORMAL, _T("M6809 Write Byte -> %04X, %02X\n"), address, value);
}

static UINT16 MK2ProtConstRead(UINT32 /*address*/)
{
	return 2;
}

static UINT16 MK2ProtRead(UINT32 /*address*/)
{
	return MK2ProtData;
}

static UINT16 MK2ProtShiftRead(UINT32 /*address*/)
{
	return MK2ProtData >> 1;
}

static void MK2ProtWrite(UINT32 /*address*/, UINT16 value)
{
	MK2ProtData = value;
}

INT32 TUnitInit()
{
    MemIndex();
    INT32 nLen = MemEnd - (UINT8 *)0;

    if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL)
        return 1;

    MemIndex();

    UINT32 nRet;
    nRet = BurnLoadRom(DrvBootROM + 0, 0, 2);
    if (nRet != 0) return 1;

    nRet = BurnLoadRom(DrvBootROM + 1, 1, 2);
    if (nRet != 0) return 1;
	
	if (TUnitIsMK) {
		nRet = LoadSoundProgRom();
		if (nRet != 0) return 1;

		nRet = LoadSoundBanksMK();
		if (nRet != 0) return 1;
	}

/*    nRet = LoadSoundBanks();
    if (nRet != 0)
        return 1;*/

    nRet = LoadGfxBanks();
    if (nRet != 0) return 1;
	
	BurnSetRefreshRate(54.71);

    TMS34010MapReset();
    TMS34010Init();

    TMS34010SetScanlineRender(ScanlineRender);
    TMS34010SetToShift(TUnitToShift);
    TMS34010SetFromShift(TUnitFromShift);
	
	// this will be removed - but putting all unmapped memory through generic handlers to enable logging unmapped reads/writes
	TMS34010SetHandlers(1, TUnitRead, TUnitWrite);
	TMS34010MapHandler(1, 0x00000000, 0x1FFFFFFF, MAP_READ | MAP_WRITE);
	
	TMS34010MapMemory(DrvBootROM, 0xFF800000, 0xFFFFFFFF, MAP_READ);
	TMS34010MapMemory(DrvBootROM, 0x1F800000, 0x1FFFFFFF, MAP_READ); // mirror
	TMS34010MapMemory(DrvRAM,     0x01000000, 0x013FFFFF, MAP_READ | MAP_WRITE);
	
	TMS34010SetHandlers(2, TUnitVramRead, TUnitVramWrite);
    TMS34010MapHandler(2, 0x00000000, 0x003fffff, MAP_READ | MAP_WRITE);
	
	TMS34010SetHandlers(3, TUnitCMOSRead, TUnitCMOSWrite);
    TMS34010MapHandler(3, 0x01400000, 0x0141ffff, MAP_READ | MAP_WRITE);
	
	TMS34010SetWriteHandler(4, TUnitCMOSWriteEnable);
    TMS34010MapHandler(4, 0x01480000, 0x014fffff, MAP_READ | MAP_WRITE);
	
	TMS34010SetReadHandler(5, TUnitInputRead);
    TMS34010MapHandler(5, 0x01600000, 0x0160003f, MAP_READ);
	
	TMS34010SetHandlers(6, TUnitPalRead, TUnitPalWrite);
    TMS34010MapHandler(6, 0x01800000, 0x0187ffff, MAP_READ | MAP_WRITE);
	
	TMS34010SetHandlers(7, TUnitDmaRead, TUnitDmaWrite);
    TMS34010MapHandler(7, 0x01a80000, 0x01a800ff, MAP_READ | MAP_WRITE);
	
	if (TUnitIsMK) {
		TMS34010SetHandlers(8, MKProtRead, MKProtWrite);
		TMS34010MapHandler(8, 0x01b00000, 0x1b6ffff, MAP_READ | MAP_WRITE);
	} else {
		TMS34010SetWriteHandler(8, TUnitCtrlWrite);
		TMS34010MapHandler(8, 0x01b00000, 0x01b0001f, MAP_WRITE);
	}
	
	TMS34010SetReadHandler(9, TUnitSoundStateRead);
    TMS34010MapHandler(9, 0x01d00000, 0x01d0001f, MAP_READ);
	
	TMS34010SetHandlers(10, TUnitSoundRead, TUnitSoundWrite);
    TMS34010MapHandler(10, 0x01d01020, 0x01d0103f, MAP_READ | MAP_WRITE);
	
	TMS34010SetWriteHandler(11, TUnitCtrlWrite);
    TMS34010MapHandler(11, 0x01f00000, 0x01f0001f, MAP_WRITE);
	
	TMS34010SetReadHandler(12, TUnitGfxRead);
    TMS34010MapHandler(12, 0x02000000, 0x07ffffff, MAP_READ);
	
	if (TUnitIsMK2) {
		TMS34010SetWriteHandler(13, MK2ProtWrite);
		TMS34010MapHandler(13, 0x00f20c60, 0x00f20c7f, MAP_WRITE);
		
		TMS34010SetWriteHandler(14, MK2ProtWrite);
		TMS34010MapHandler(14, 0x00f42820, 0x00f4283f, MAP_WRITE);
		
		TMS34010SetReadHandler(15, MK2ProtRead);
		TMS34010MapHandler(15, 0x01a190e0, 0x01a190ff, MAP_READ);
		
		TMS34010SetReadHandler(16, MK2ProtShiftRead);
		TMS34010MapHandler(16, 0x01a191c0, 0x01a191df, MAP_READ);
		
		TMS34010SetReadHandler(17, MK2ProtRead);
		TMS34010MapHandler(17, 0x01a3d0c0, 0x01a3d0ff, MAP_READ);
		
		TMS34010SetReadHandler(18, MK2ProtConstRead);
		TMS34010MapHandler(18, 0x01d9d1e0, 0x01d9d1ff, MAP_READ);
		
		TMS34010SetReadHandler(19, MK2ProtConstRead);
		TMS34010MapHandler(19, 0x01def920, 0x01def93f, MAP_READ);
	}

    memset(DrvVRAM, 0, 0x400000);
	
	if (TUnitIsMK) {
		nSoundType = SOUND_ADPCM;
		M6809Init(0);
		M6809Open(0);
		M6809MapMemory(DrvSoundProgRAM, 0x0000, 0x1fff, MAP_RAM);
		MKsound_bankswitch(0);
		// _all_ ROM is dished out in the handler, had to do it this way
		// because of the hidden prot ram. (and the separate op/oparg/read handlers..)
		M6809SetReadHandler(MKSoundRead);
		M6809SetReadOpHandler(MKSoundRead);
		M6809SetReadOpArgHandler(MKSoundRead);
		M6809SetWriteHandler(MKSoundWrite);
		M6809Close();
		
		BurnYM2151Init(3579545);
		BurnYM2151SetIrqHandler(&MKYM2151IrqHandler);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

		DACInit(0, 0, 1, M6809TotalCycles, 2000000);
		DACSetRoute(0, 0.35, BURN_SND_ROUTE_BOTH);

		MSM6295Init(0, 1000000 / MSM6295_PIN7_HIGH, 1);
		MSM6295SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	}
	
	GenericTilesInit();
	
	TUnitDoReset();
	
    return 0;
}

static void MakeInputs()
{
    DrvInputs[0] = 0;
    DrvInputs[1] = 0;
    DrvInputs[2] = 0;
    DrvInputs[3] = 0;

    for (INT32 i = 0; i < 16; i++) {
        if (nTUnitJoy1[i] & 1) DrvInputs[0] |= (1 << i);
        if (nTUnitJoy2[i] & 1) DrvInputs[1] |= (1 << i);
        if (nTUnitJoy3[i] & 1) DrvInputs[2] |= (1 << i);
    }
}

INT32 TUnitExit()
{
	BurnFree(AllMem);

	if (nSoundType == SOUND_ADPCM) {
		M6809Exit();
		BurnYM2151Exit();
		DACExit();
		MSM6295Exit();
	}

	if (nSoundType == SOUND_DCS) {
		Dcs2kExit();
	}

	GenericTilesExit();
	
	TUnitIsMK = 0;
	TUnitIsMK2 = 0;

    return 0;
}

INT32 TUnitDraw()
{
	if (nTUnitRecalc) {
		TUnitPalRecalc();
		nTUnitRecalc = 0;
	}

	// TMS34010 renders scanlines direct to pTransDraw
	BurnTransferCopy(DrvPaletteB);

	return 0;
}

INT32 TUnitFrame()
{
	if (nTUnitReset) TUnitDoReset();
	
	MakeInputs();

	TMS34010NewFrame();

	if (TUnitIsMK) M6809NewFrame();

	INT32 nInterleave = 288;
	INT32 nCyclesTotal[2] = { (INT32)(50000000/8/54.71), (INT32)(2000000 / 54.71) };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;
	
	if (TUnitIsMK) M6809Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		nCyclesDone[0] += TMS34010Run((nCyclesTotal[0] * (i + 1) / nInterleave) - nCyclesDone[0]);
		
		if (TUnitIsMK && !sound_inreset) nCyclesDone[1] += M6809Run((nCyclesTotal[1] * (i + 1) / nInterleave) - nCyclesDone[1]);

		TMS34010GenerateScanline(i);
		
		if (pBurnSoundOut) {
			if (TUnitIsMK && i&1) {
				INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 2);
				INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
				BurnYM2151Render(pSoundBuf, nSegmentLength);
				nSoundBufferPos += nSegmentLength;
			}
		}
    }
	
	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		if (TUnitIsMK) {
			INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

			if (nSegmentLength) {
				BurnYM2151Render(pSoundBuf, nSegmentLength);
			}
			DACUpdate(pBurnSoundOut, nBurnSoundLen);
			MSM6295Render(pBurnSoundOut, nBurnSoundLen);
		}
	}
	
	if (TUnitIsMK) M6809Close();

	if (pBurnDraw) {
		TUnitDraw();
	}

    return 0;
}

INT32 TUnitScan(INT32 nAction, INT32 *pnMin)
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

		if (nSoundType == SOUND_ADPCM) {
			MKsound_statescan(nAction);
		}

		if (nSoundType == SOUND_DCS) {
			Dcs2kScan(nAction, pnMin);
		}

		SCAN_VAR(nVideoBank);
		SCAN_VAR(nDMA);
		SCAN_VAR(nTUnitCtrl);
		SCAN_VAR(bCMOSWriteEnable);
		// Might need to scan the dma_state struct in midtunit_dma.h
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x2000;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_WRITE) {
	}

	return 0;
}
