// todo:
//   NBAJAM & NBA JAM TE: resets in intro - CPU core?
//   JDREDDP - sound

#include "tiles_generic.h"
#include "midtunit.h"
#include "tms34010_intf.h"
#include "m6809_intf.h"
#include "burn_ym2151.h"
#include "dcs2k.h"
#include "msm6295.h"
#include "dac.h"
#include <stddef.h>

#define LOG_UNMAPPED    0

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
UINT8 nTUnitDSW[2];
UINT8 nTUnitReset = 0;
static UINT32 DrvInputs[4];

static bool bGfxRomLarge = false;
static UINT32 nGfxBankOffset[2] = { 0x000000, 0x400000 };

static bool bCMOSWriteEnable = false;
static UINT32 nVideoBank = 1;
static UINT16 *nDMA;
static UINT16 nTUnitCtrl = 0;
static UINT8 DrvFakeSound = 0;

static UINT8 MKProtIndex = 0;
static UINT16 MK2ProtData = 0xffff;

static UINT16 NbajamProtQueue[5] = { 0, 0, 0, 0, 0 };
static UINT8 NbajamProtIndex = 0;

static UINT8 JdreddpProtIndex = 0;
static UINT8 JdreddpProtMax = 0;
static const UINT8 *JdreddpProtTable = NULL;

static UINT16 SoundProtAddressStart = 0;
static UINT16 SoundProtAddressEnd = 0;

UINT8 TUnitIsMK = 0;
UINT8 TUnitIsMKTurbo = 0;
UINT8 TUnitIsMK2 = 0;
UINT8 TUnitIsNbajam = 0;
UINT8 TUnitIsNbajamTe = 0;
UINT8 TUnitIsJdreddp = 0;

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
	DrvRAM		= Next;				Next += TOBYTE(0x400000) * sizeof(UINT16);
	DrvSoundProgRAM = Next;			Next += 0x002000 * sizeof(UINT8);
	DrvSoundProgRAMProt = Next;		Next += 0x000100 * sizeof(UINT8);
	DrvPalette	= Next;				Next += 0x20000 * sizeof(UINT8);
	DrvPaletteB	= (UINT32*)Next;	Next += 0x8000 * sizeof(UINT32);
	DrvVRAM		= Next;				Next += TOBYTE(0x400000) * sizeof(UINT16);
	DrvVRAM16	= (UINT16*)DrvVRAM;

	nDMA        = (UINT16*)Next;    Next += 0x0020 * sizeof(UINT16);
	dma_state   = (dma_state_s*)Next; Next += sizeof(dma_state_s);

	RamEnd		= Next;

	MemEnd		= Next;
    return 0;
}

// dcs sound helpers
static void dcs_sound_sync()
{
	INT32 cyc = ((double)TMS34010TotalCycles() / 63 * 100) - Dcs2kTotalCycles();
	if (cyc > 0) {
		Dcs2kRun(cyc);
	}
}

static void dcs_sound_sync_end()
{
	INT32 cyc = ((double)10000000 * 100 / nBurnFPS) - Dcs2kTotalCycles();
	if (cyc > 0) {
		Dcs2kRun(cyc);
	}
}

// williams adpcm-compat. sound section (MK)
static INT32 sound_latch;
static INT32 sound_talkback;
static INT32 sound_irqstate;
static INT32 sound_bank;
static INT32 sound_msm6295bank;
static INT32 sound_inreset;

static void MKsound_bankswitch(INT32 bank)
{
	sound_bank = bank & 7;
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
	BurnYM2151Reset();
	if (!local) M6809Close();

	DACReset();
	MSM6295Reset();
}

static void MKsound_reset_write(INT32 val)
{
	if (val) {
		MKsound_reset(1);
		sound_inreset = 1;
	} else {
		sound_inreset = 0;
	}
}

static void MKsound_main2soundwrite(INT32 data)
{
	sound_latch = data & 0xff;
	if (~data & 0x200) {
		M6809SetIRQLine(M6809_IRQ_LINE, CPU_IRQSTATUS_ACK);
		sound_irqstate = 1;
	}
}

static void MKsound_statescan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		M6809Scan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		MSM6295Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

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
		if (address >= SoundProtAddressStart && address <= SoundProtAddressEnd) { // MK sound protection ram.
			return DrvSoundProgRAMProt[address - SoundProtAddressStart];
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
	if (address >= SoundProtAddressStart && address <= SoundProtAddressEnd) { // MK sound protection ram.
		DrvSoundProgRAMProt[address - SoundProtAddressStart] = value;
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

// general emulation helpers, etc.
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
	if (BurnLoadRom(DrvSoundProgROM, 2, 1)) return 1;
	
	return 0;
}

static INT32 LoadSoundProgNbajamRom()
{
	if (BurnLoadRom(DrvSoundProgROM, 2, 1)) return 1;
	if (BurnLoadRom(DrvSoundProgROM + 0x20000, 2, 1)) return 1;
	
	return 0;
}

static INT32 LoadSoundBanksMK()
{
	if (BurnLoadRom(DrvSoundROM + 0x000000, 3, 1)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0x040000, 3, 1)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0x080000, 4, 1)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0x0c0000, 4, 1)) return 1;
	
    return 0;
}

static INT32 LoadSoundBanksMk2()
{
   memset(DrvSoundROM, 0xFF, 0x1000000);
    if (BurnLoadRom(DrvSoundROM + 0x000000, 2, 2)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0x100000, 2, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0x200000, 3, 2)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0x300000, 3, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0x400000, 4, 2)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0x500000, 4, 2)) return 1;
    if (BurnLoadRom(DrvSoundROM + 0x600000, 5, 2)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0x700000, 5, 2)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0x800000, 6, 2)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0x900000, 6, 2)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0xa00000, 7, 2)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0xb00000, 7, 2)) return 1;
	
	return 0;
}

static INT32 LoadSoundBanksNbajam()
{
	if (BurnLoadRom(DrvSoundROM + 0x000000, 3, 1)) return 1;
	if (BurnLoadRom(DrvSoundROM + 0x080000, 4, 1)) return 1;
	
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
		case SOUND_ADPCM: {
			MKsound_reset(0);
			break;
		}

		case SOUND_DCS: {
			Dcs2kReset();
			break;
		}
	}

	memset (AllRam, 0, RamEnd - AllRam);

	nGfxBankOffset[0] = 0x000000;
	nGfxBankOffset[1] = 0x400000;

	nVideoBank = 1;
	nTUnitCtrl = 0;

	MKProtIndex = 0;
	MK2ProtData = 0xffff;
	NbajamProtIndex = 0;
	memset(NbajamProtQueue, 0, 5 * sizeof(UINT16));
	JdreddpProtIndex = 0;
	JdreddpProtMax = 0;
	JdreddpProtTable = NULL;
	
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

#if LOG_UNMAPPED
static UINT16 TUnitRead(UINT32 address)
{
	if (address == 0x01600040) return 0xff; // ???
	if (address == 0x01d81070) return 0xff; // watchdog

	if (address == 0x01b00000) return 0xff; // ?
	if (address == 0x01c00060) return 0xff; // ?
	if (address == 0x01f00000) return 0xff; // ?

	bprintf(PRINT_NORMAL, _T("Read %x\n"), address);

	return ~0;
}

static void TUnitWrite(UINT32 address, UINT16 value)
{
	if (address == 0x01d81070) return; // watchdog
	if (address == 0x01c00060) return; // ?
	
	bprintf(PRINT_NORMAL, _T("Write %x, %x\n"), address, value);
}
#endif

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
		if (nSoundType == SOUND_DCS) {
			dcs_sound_sync();
			return Dcs2kControlRead() >> 4;
		}

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
		if (nSoundType == SOUND_DCS) {
			dcs_sound_sync();
			UINT16 dr = Dcs2kDataRead();
			Dcs2kRun(20); // "sync" in frame will even things out.
			return dr & 0xff;
		}
	}
	//bprintf(PRINT_NORMAL, _T("Sound Read %x\n"), address);

	return ~0;
}

static void TUnitSoundWrite(UINT32 address, UINT16 value)
{
	if (address >= 0x01d01021 && address <= 0x01d0103f) {
		//bprintf(PRINT_NORMAL, _T("Sound Write %x, %x\n"), address, value);

		switch (nSoundType)	{
			case SOUND_ADPCM: {
				MKsound_reset_write(~value & 0x100);
				MKsound_main2soundwrite(value & 0xff);

				DrvFakeSound = 128;
				break;
			}

			case SOUND_DCS: {

				Dcs2kResetWrite(~value & 0x100);
				dcs_sound_sync();
				Dcs2kDataWrite(value & 0xff);
				Dcs2kRun(20);

				DrvFakeSound = 128;
				break;
			}
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

// mortal kombat protection
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

// mortal kombat turbo protection
static UINT16 MKTurboProtRead(UINT32 address)
{
	if (address >= 0xfffff400 && address <= 0xfffff40f) {
		return BurnRandom();
	}
	
    UINT32 offset = TOBYTE(address & 0x7fffff);
    return DrvBootROM[offset] | (DrvBootROM[offset + 1] << 8);
}

// mortal kombat 2 protection
static UINT16 MK2ProtRead(UINT32 address)
{
	if (address >= 0x01a190e0 && address <= 0x01a190ff) {
		return MK2ProtData;
	}
	
	if (address >= 0x01a191c0 && address <= 0x01a191df) {
		return MK2ProtData >> 1;
	}
	
	if (address >= 0x01a3d0c0 && address <= 0x01a3d0ff) {
		return MK2ProtData;
	}
	
	if (address >= 0x01d9d1e0 && address <= 0x01d9d1ff) {
		return 2;
	}
	
	if (address >= 0x01def920 && address <= 0x01def93f) {
		return 2;
	}
	
	return ~0;
}

static void MK2ProtWrite(UINT32 address, UINT16 value)
{
	if (address >= 0x00f20c60 && address <= 0x00f20c7f) {
		MK2ProtData = value;
	}
	
	if (address >= 0x00f42820 && address <= 0x00f4283f) {
		MK2ProtData = value;
	}
}

// nba jam protection
static const UINT32 nbajam_prot_values[128] =
{
	0x21283b3b, 0x2439383b, 0x31283b3b, 0x302b3938, 0x31283b3b, 0x302b3938, 0x232f2f2f, 0x26383b3b,
	0x21283b3b, 0x2439383b, 0x312a1224, 0x302b1120, 0x312a1224, 0x302b1120, 0x232d283b, 0x26383b3b,
	0x2b3b3b3b, 0x2e2e2e2e, 0x39383b1b, 0x383b3b1b, 0x3b3b3b1b, 0x3a3a3a1a, 0x2b3b3b3b, 0x2e2e2e2e,
	0x2b39383b, 0x2e2e2e2e, 0x393a1a18, 0x383b1b1b, 0x3b3b1b1b, 0x3a3a1a18, 0x2b39383b, 0x2e2e2e2e,
	0x01202b3b, 0x0431283b, 0x11202b3b, 0x1021283b, 0x11202b3b, 0x1021283b, 0x03273b3b, 0x06302b39,
	0x09302b39, 0x0c232f2f, 0x19322e06, 0x18312a12, 0x19322e06, 0x18312a12, 0x0b31283b, 0x0e26383b,
	0x03273b3b, 0x06302b39, 0x11202b3b, 0x1021283b, 0x13273938, 0x12243938, 0x03273b3b, 0x06302b39,
	0x0b31283b, 0x0e26383b, 0x19322e06, 0x18312a12, 0x1b332f05, 0x1a302b11, 0x0b31283b, 0x0e26383b,
	0x21283b3b, 0x2439383b, 0x31283b3b, 0x302b3938, 0x31283b3b, 0x302b3938, 0x232f2f2f, 0x26383b3b,
	0x21283b3b, 0x2439383b, 0x312a1224, 0x302b1120, 0x312a1224, 0x302b1120, 0x232d283b, 0x26383b3b,
	0x2b3b3b3b, 0x2e2e2e2e, 0x39383b1b, 0x383b3b1b, 0x3b3b3b1b, 0x3a3a3a1a, 0x2b3b3b3b, 0x2e2e2e2e,
	0x2b39383b, 0x2e2e2e2e, 0x393a1a18, 0x383b1b1b, 0x3b3b1b1b, 0x3a3a1a18, 0x2b39383b, 0x2e2e2e2e,
	0x01202b3b, 0x0431283b, 0x11202b3b, 0x1021283b, 0x11202b3b, 0x1021283b, 0x03273b3b, 0x06302b39,
	0x09302b39, 0x0c232f2f, 0x19322e06, 0x18312a12, 0x19322e06, 0x18312a12, 0x0b31283b, 0x0e26383b,
	0x03273b3b, 0x06302b39, 0x11202b3b, 0x1021283b, 0x13273938, 0x12243938, 0x03273b3b, 0x06302b39,
	0x0b31283b, 0x0e26383b, 0x19322e06, 0x18312a12, 0x1b332f05, 0x1a302b11, 0x0b31283b, 0x0e26383b
};

static UINT16 NbajamProtRead(UINT32 address)
{
	if (address >= 0x1b14020 && address <= 0x1b2503f) {
		UINT16 result = NbajamProtQueue[NbajamProtIndex];
		if (NbajamProtIndex < 4) NbajamProtIndex++;
		return result;
	}
	
	return ~0;
}

static void NbajamProtWrite(UINT32 address, UINT16 value)
{
	if (address >= 0x1b14020 && address <= 0x1b2503f) {
		UINT32 offset = (address - 0x1b14020) >> 4;
		INT32 table_index = (offset >> 6) & 0x7f;
		UINT32 protval = nbajam_prot_values[table_index];
		
		NbajamProtQueue[0] = value;
		NbajamProtQueue[1] = ((protval >> 24) & 0xff) << 9;
		NbajamProtQueue[2] = ((protval >> 16) & 0xff) << 9;
		NbajamProtQueue[3] = ((protval >> 8) & 0xff) << 9;
		NbajamProtQueue[4] = ((protval >> 0) & 0xff) << 9;
		NbajamProtIndex = 0;
	}
}

// nba jam te protection
static const UINT32 nbajamte_prot_values[128] =
{
	0x00000000, 0x04081020, 0x08102000, 0x0c183122, 0x10200000, 0x14281020, 0x18312204, 0x1c393326,
	0x20000001, 0x24081021, 0x28102000, 0x2c183122, 0x30200001, 0x34281021, 0x38312204, 0x3c393326,
	0x00000102, 0x04081122, 0x08102102, 0x0c183122, 0x10200000, 0x14281020, 0x18312204, 0x1c393326,
	0x20000103, 0x24081123, 0x28102102, 0x2c183122, 0x30200001, 0x34281021, 0x38312204, 0x3c393326,
	0x00010204, 0x04091224, 0x08112204, 0x0c193326, 0x10210204, 0x14291224, 0x18312204, 0x1c393326,
	0x20000001, 0x24081021, 0x28102000, 0x2c183122, 0x30200001, 0x34281021, 0x38312204, 0x3c393326,
	0x00010306, 0x04091326, 0x08112306, 0x0c193326, 0x10210204, 0x14291224, 0x18312204, 0x1c393326,
	0x20000103, 0x24081123, 0x28102102, 0x2c183122, 0x30200001, 0x34281021, 0x38312204, 0x3c393326,
	0x00000000, 0x01201028, 0x02213018, 0x03012030, 0x04223138, 0x05022110, 0x06030120, 0x07231108,
	0x08042231, 0x09243219, 0x0a251229, 0x0b050201, 0x0c261309, 0x0d060321, 0x0e072311, 0x0f273339,
	0x10080422, 0x1128140a, 0x1229343a, 0x13092412, 0x142a351a, 0x150a2532, 0x160b0502, 0x172b152a,
	0x180c2613, 0x192c363b, 0x1a2d160b, 0x1b0d0623, 0x1c2e172b, 0x1d0e0703, 0x1e0f2733, 0x1f2f371b,
	0x20100804, 0x2130182c, 0x2231381c, 0x23112834, 0x2432393c, 0x25122914, 0x26130924, 0x2733190c,
	0x28142a35, 0x29343a1d, 0x2a351a2d, 0x2b150a05, 0x2c361b0d, 0x2d160b25, 0x2e172b15, 0x2f373b3d,
	0x30180c26, 0x31381c0e, 0x32393c3e, 0x33192c16, 0x343a3d1e, 0x351a2d36, 0x361b0d06, 0x373b1d2e,
	0x381c2e17, 0x393c3e3f, 0x3a3d1e0f, 0x3b1d0e27, 0x3c3e1f2f, 0x3d1e0f07, 0x3e1f2f37, 0x3f3f3f1f
};

static UINT16 NbajamteProtRead(UINT32 address)
{
	if ((address >= 0x1b15f40 && address <= 0x1b37f5f) || (address >= 0x1b95f40 && address <= 0x1bb7f5f)) {
		UINT16 result = NbajamProtQueue[NbajamProtIndex];
		if (NbajamProtIndex < 4) NbajamProtIndex++;
		return result;
	}
	
	return ~0;
}

static void NbajamteProtWrite(UINT32 address, UINT16 value)
{
	UINT32 offset = 0;
	
	if (address >= 0x1b15f40 && address <= 0x1b37f5f) offset = address - 0x1b15f40;
	if (address >= 0x1b95f40 && address <= 0x1bb7f5f) offset = address - 0x1b95f40;
	
	if (offset > 0) {
		offset = offset >> 4;
		INT32 table_index = (offset >> 6) & 0x7f;
		UINT32 protval = nbajamte_prot_values[table_index];
		
		NbajamProtQueue[0] = value;
		NbajamProtQueue[1] = ((protval >> 24) & 0xff) << 9;
		NbajamProtQueue[2] = ((protval >> 16) & 0xff) << 9;
		NbajamProtQueue[3] = ((protval >> 8) & 0xff) << 9;
		NbajamProtQueue[4] = ((protval >> 0) & 0xff) << 9;
		NbajamProtIndex = 0;
	}
}

// judge dredd protection
static const UINT8 jdredd_prot_values_10740[] =
{
	0x14,0x2A,0x15,0x0A,0x25,0x32,0x39,0x1C,
	0x2E,0x37,0x3B,0x1D,0x2E,0x37,0x1B,0x0D,
	0x26,0x33,0x39,0x3C,0x1E,0x2F,0x37,0x3B,
	0x3D,0x3E,0x3F,0x1F,0x2F,0x17,0x0B,0x25,
	0x32,0x19,0x0C,0x26,0x33,0x19,0x2C,0x16,
	0x2B,0x15,0x0A,0x05,0x22,0x00
};

static const UINT8 jdredd_prot_values_13240[] =
{
	0x28
};

static const UINT8 jdredd_prot_values_76540[] =
{
	0x04,0x08
};

static const UINT8 jdredd_prot_values_77760[] =
{
	0x14,0x2A,0x14,0x2A,0x35,0x2A,0x35,0x1A,
	0x35,0x1A,0x2D,0x1A,0x2D,0x36,0x2D,0x36,
	0x1B,0x36,0x1B,0x36,0x2C,0x36,0x2C,0x18,
	0x2C,0x18,0x31,0x18,0x31,0x22,0x31,0x22,
	0x04,0x22,0x04,0x08,0x04,0x08,0x10,0x08,
	0x10,0x20,0x10,0x20,0x00,0x20,0x00,0x00,
	0x00,0x00,0x01,0x00,0x01,0x02,0x01,0x02,
	0x05,0x02,0x05,0x0B,0x05,0x0B,0x16,0x0B,
	0x16,0x2C,0x16,0x2C,0x18,0x2C,0x18,0x31,
	0x18,0x31,0x22,0x31,0x22,0x04,0x22,0x04,
	0x08,0x04,0x08,0x10,0x08,0x10,0x20,0x10,
	0x20,0x00,0x00
};

static const UINT8 jdredd_prot_values_80020[] =
{
	0x3A,0x1D,0x2E,0x37,0x00,0x00,0x2C,0x1C,
	0x39,0x33,0x00,0x00,0x00,0x00,0x00,0x00
};

static UINT16 JdreddpProtRead(UINT32 address)
{
	UINT16 result = 0xffff;

	if (JdreddpProtTable && JdreddpProtIndex < JdreddpProtMax) {
		result = JdreddpProtTable[JdreddpProtIndex++] << 9;
	}

	return result;
}

static void JdreddpProtWrite(UINT32 address, UINT16 value)
{
	UINT32 offset = (address - 0x1b00000) >> 4;
	
	switch (offset) {
		case 0x1074: {
			JdreddpProtIndex = 0;
			JdreddpProtTable = jdredd_prot_values_10740;
			JdreddpProtMax = sizeof(jdredd_prot_values_10740);
			break;
		}

		case 0x1324: {
			JdreddpProtIndex = 0;
			JdreddpProtTable = jdredd_prot_values_13240;
			JdreddpProtMax = sizeof(jdredd_prot_values_13240);
			break;
		}

		case 0x7654: {
			JdreddpProtIndex = 0;
			JdreddpProtTable = jdredd_prot_values_76540;
			JdreddpProtMax = sizeof(jdredd_prot_values_76540);
			break;
		}

		case 0x7776: {
			JdreddpProtIndex = 0;
			JdreddpProtTable = jdredd_prot_values_77760;
			JdreddpProtMax = sizeof(jdredd_prot_values_77760);
			break;
		}

		case 0x8002: {
			JdreddpProtIndex = 0;
			JdreddpProtTable = jdredd_prot_values_80020;
			JdreddpProtMax = sizeof(jdredd_prot_values_80020);
			break;
		}
	}
}

// init, exit, etc.
INT32 TUnitInit()
{
	BurnSetRefreshRate(54.71);

	nSoundType = SOUND_ADPCM;
	bGfxRomLarge = false;

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
	
	if (TUnitIsMK2) {
		nRet = LoadSoundBanksMk2();
		if (nRet != 0) return 1;
	}
	
	if (TUnitIsNbajam || TUnitIsNbajamTe || TUnitIsJdreddp) {
		nRet = LoadSoundProgNbajamRom();
		if (nRet != 0) return 1;

		nRet = LoadSoundBanksNbajam();
		if (nRet != 0) return 1;
	}

    nRet = LoadGfxBanks();
    if (nRet != 0) return 1;

    TMS34010MapReset();
    TMS34010Init();

    TMS34010SetScanlineRender(ScanlineRender);
    TMS34010SetToShift(TUnitToShift);
    TMS34010SetFromShift(TUnitFromShift);
	
#if LOG_UNMAPPED
	// this will be removed - but putting all unmapped memory through generic handlers to enable logging unmapped reads/writes
	TMS34010SetHandlers(1, TUnitRead, TUnitWrite);
	TMS34010MapHandler(1, 0x00000000, 0x1FFFFFFF, MAP_READ | MAP_WRITE);
#endif

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
	
	if (TUnitIsMKTurbo) {
		TMS34010SetReadHandler(13, MKTurboProtRead);
		TMS34010MapHandler(13, 0xFF800000, 0xffffffff, MAP_READ);
	}
	
	if (TUnitIsMK2) {
		TMS34010SetWriteHandler(13, MK2ProtWrite);
		TMS34010MapHandler(13, 0x00f20000, 0x00f4ffff, MAP_WRITE);
		
		TMS34010SetReadHandler(14, MK2ProtRead);
		TMS34010MapHandler(14, 0x01a10000, 0x01a3ffff, MAP_READ);
		
		TMS34010SetReadHandler(15, MK2ProtRead);
		TMS34010MapHandler(15, 0x01d90000, 0x01dfffff, MAP_READ);
	}
	
	if (TUnitIsNbajam) {
		TMS34010SetHandlers(13, NbajamProtRead, NbajamProtWrite);
		TMS34010MapHandler(13, 0x1b14000, 0x1b25fff, MAP_READ | MAP_WRITE);
	}
	
	if (TUnitIsNbajamTe) {
		TMS34010SetHandlers(13, NbajamteProtRead, NbajamteProtWrite);
		TMS34010MapHandler(13, 0x1b15000, 0x1bbffff, MAP_READ | MAP_WRITE);
	}
	
	if (TUnitIsJdreddp) {
		TMS34010SetHandlers(13, JdreddpProtRead, JdreddpProtWrite);
		TMS34010MapHandler(13, 0x1b00000, 0x1bfffff, MAP_READ | MAP_WRITE);
	}

	if (TUnitIsMK || TUnitIsNbajam || TUnitIsNbajamTe || TUnitIsJdreddp) {
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

		BurnYM2151Init(3579545, 1);
		BurnTimerAttachM6809(2000000);
		BurnYM2151SetIrqHandler(&MKYM2151IrqHandler);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
		BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

		DACInit(0, 0, 1, M6809TotalCycles, 2000000);
		DACSetRoute(0, 0.25, BURN_SND_ROUTE_BOTH);

		MSM6295Init(0, /*1000000 / MSM6295_PIN7_HIGH*/8000, 1);
		MSM6295SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
		
		SoundProtAddressStart = 0xfb9c;
		SoundProtAddressEnd = 0xfbc6;
		
		if (TUnitIsNbajam) {
			SoundProtAddressStart = 0xfbaa;
			SoundProtAddressEnd = 0xfbd4;
		}
		
		if (TUnitIsNbajamTe) {
			SoundProtAddressStart = 0xfbec;
			SoundProtAddressEnd = 0xfc16;
		}
	}
	
	if (TUnitIsMK2) {
		nSoundType = SOUND_DCS;
		
		Dcs2kInit(DCS_2K, MHz(10));
		Dcs2kMapSoundROM(DrvSoundROM, 0x1000000);
		Dcs2kSetVolume(10.00);
		
		Dcs2kBoot();

		Dcs2kResetWrite(1);
		Dcs2kResetWrite(0);
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
	TUnitIsMKTurbo = 0;
	TUnitIsNbajam = 0;
	TUnitIsNbajamTe = 0;
	TUnitIsJdreddp = 0;
	
	SoundProtAddressStart = 0;
	SoundProtAddressEnd = 0;

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

static void HandleDCSIRQ(INT32 line)
{
	if (nBurnFPS == 6000) {
		// 60hz needs 2 irq's/frame (this is here for "force 60hz"/etc)
		if (line == 0 || line == 144) DcsIRQ(); // 2x per frame
	} else {
		// 54.71hz needs 5 irq's every 2 frames
		if (nCurrentFrame & 1) {
			if (line == 0 || line == 144) DcsIRQ(); // 2x per frame
		} else {
			if (line == 0 || line == 96 || line == 192) DcsIRQ(); // 3x
		}
	}
}

INT32 TUnitFrame()
{
	if (nTUnitReset) TUnitDoReset();
	
	MakeInputs();

	TMS34010NewFrame();

	if (nSoundType == SOUND_ADPCM) M6809NewFrame();

	INT32 nInterleave = 288;
	INT32 nCyclesTotal[2] = { (INT32)(50000000/8/54.71), (INT32)(2000000 / 54.71) };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;
	
	if (nSoundType == SOUND_DCS) {
		nCyclesTotal[1] = (INT32)(10000000 / 54.71);
		Dcs2kNewFrame();
	}
	
	if (nSoundType == SOUND_ADPCM) M6809Open(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		nCyclesDone[0] += TMS34010Run((nCyclesTotal[0] * (i + 1) / nInterleave) - nCyclesDone[0]);

		TMS34010GenerateScanline(i);

		if (nSoundType == SOUND_DCS) {
			HandleDCSIRQ(i);

			dcs_sound_sync(); // sync to main cpu
			if (i == nInterleave - 1) dcs_sound_sync_end();
		}

		if (nSoundType == SOUND_ADPCM && !sound_inreset) {
			BurnTimerUpdate((i + 1) * nCyclesTotal[1] / nInterleave);
			if (i == nInterleave - 1) BurnTimerEndFrame(nCyclesTotal[1]);
		}

		if (pBurnSoundOut) {
			if (nSoundType == SOUND_ADPCM && i&1) {
				INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 2);
				INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
				BurnYM2151Render(pSoundBuf, nSegmentLength);
				nSoundBufferPos += nSegmentLength;
			}
		}
    }

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		if (nSoundType == SOUND_ADPCM) {
			INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

			if (nSegmentLength) {
				BurnYM2151Render(pSoundBuf, nSegmentLength);
			}
			DACUpdate(pBurnSoundOut, nBurnSoundLen);
			MSM6295Render(pBurnSoundOut, nBurnSoundLen);
		}
		
		if (nSoundType == SOUND_DCS) {
			Dcs2kRender(pBurnSoundOut, nBurnSoundLen);
		}
	}
	
	if (nSoundType == SOUND_ADPCM) M6809Close();

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
			MKsound_statescan(nAction, pnMin);
		}

		if (nSoundType == SOUND_DCS) {
			Dcs2kScan(nAction, pnMin);
		}

		SCAN_VAR(nVideoBank);
		SCAN_VAR(nTUnitCtrl);
		SCAN_VAR(nGfxBankOffset);
		SCAN_VAR(bCMOSWriteEnable);
		
		SCAN_VAR(MKProtIndex);
		SCAN_VAR(MK2ProtData);
		SCAN_VAR(NbajamProtQueue);
		SCAN_VAR(NbajamProtIndex);
		SCAN_VAR(JdreddpProtIndex);
		SCAN_VAR(JdreddpProtMax);
		SCAN_VAR(JdreddpProtTable);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x2000 * sizeof(UINT16);
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_WRITE) {
	}

	return 0;
}
