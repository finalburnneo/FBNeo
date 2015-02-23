
#include "driver.h"
#include "burnint.h"
#include "mips3_intf.h"
#include "ide.h"
#include <stdio.h>

#define IDE_IRQ     1
#define VBLANK_IRQ  0

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvBootROM;
static UINT8 *DrvRAM0;
static UINT8 *DrvRAM1;
static UINT8 DrvRecalc;
static UINT8 DrvJoy1[32];
static UINT8 DrvJoy2[32];
static UINT32 DrvVRAMBase;
static UINT32 DrvInputs[3];
static ide::ide_disk *DrvDisk;

// Fast conversion from BGR555 to RGB565
static UINT16 *DrvColorLUT;

static struct BurnInputInfo kinstInputList[] = {
    {"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 coin"	},
    {"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 10,	"p1 start"	},
    {"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 up"		},
    {"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 down"	},
    {"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 left"	},
    {"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 9,	"p1 right"	},
    {"P1 Button A",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
    {"P1 Button B",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
    {"P1 Button C",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
    {"P1 Button X",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},
    {"P1 Button Y",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 5"	},
    {"P1 Button Z",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 6"	},
    {"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 coin"	},
    {"P2 Start",	BIT_DIGITAL,	DrvJoy2 + 10,	"p2 start"	},
    {"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 up"		},
    {"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 down"	},
    {"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 left"	},
    {"P2 Right",	BIT_DIGITAL,	DrvJoy2 + 9,	"p2 right"	},
    {"P2 Button A",	BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
    {"P2 Button B",	BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
    {"P2 Button C",	BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},
    {"P2 Button X",	BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 4"	},
    {"P2 Button Y",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 5"	},
    {"P2 Button Z",	BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 6"	},
};

STDINPUTINFO(kinst)

static struct BurnDIPInfo kinstDIPList[1]=
{
};

STDDIPINFO(kinst)

static INT32 MemIndex()
{
    UINT8 *Next; Next = AllMem;

    DrvBootROM 	= Next;             Next += 0x80000;
    AllRam      = Next;
    DrvRAM0     = Next;             Next += 0x80000;
    DrvRAM1     = Next;             Next += 0x800000;
    DrvColorLUT = (UINT16*) Next;   Next += 0x8000 * 2;

    RamEnd		= Next;
    MemEnd		= Next;
    return 0;
}


#define RGB888(b,g,r)   ((r) | ((g) << 8) | ((b) << 16))
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

static void GenerateColorLUT()
{
    for (int i = 0; i < 0x8000; i++) {
        UINT16 x = i;
        DrvColorLUT[i] = RGB888_2_565(RGB555_2_888(i));
    }
}

static void IDESetIRQState(int state)
{
    Mips3SetIRQLine(IDE_IRQ, state);
}

static UINT32 ideRead(UINT32 address)
{
    if (address >= 0x10000100 && address <= 0x1000013f)
        return DrvDisk->read((address - 0x10000100) / 8);

    if (address >= 0x10000170 && address <= 0x10000173)
        return DrvDisk->read_alternate(6);
    return 0;
}

static void ideWrite(UINT32 address, UINT32 value)
{
    if (address >= 0x10000100 && address <= 0x1000013f) {
        DrvDisk->write((address - 0x10000100) / 8, value);
        return;
    }

    if (address >= 0x10000170 && address <= 0x10000173) {
        DrvDisk->write_alternate(6, value);
        return;
    }
}

static UINT32 kinstRead(UINT32 address)
{
    if (address >= 0x10000080 && address <= 0x100000ff) {
        switch (address & 0xFF) {
        case 0x90:
            return 2;
        case 0x80:
            return ~DrvInputs[0];
        case 0x88:
            return ~DrvInputs[1];
        case 0xA0:
            return (~0) & ~0x00003e00;
        }
        return ~0;
    }

    if (address >= 0x10000100 && address <= 0x10000173) {
        return ideRead(address);
    }


    printf("Invalid read %08X\n", address);
    return ~0;
}

static void kinstWrite(UINT32 address, UINT64 value)
{
    if (address >= 0x10000080 && address <= 0x100000ff) {
        switch (address & 0xFF) {
        case 0x80:
            DrvVRAMBase = (value & 4) ? 0x58000 : 0x30000;
            break;
        }
        return;
    }

    if (address >= 0x10000100 && address <= 0x10000173) {
        ideWrite(address, value);
        return;
    }

}

static void kinstWriteByte(UINT32 address, UINT8 value) { kinstWrite(address, value); }
static void kinstWriteHalf(UINT32 address, UINT16 value) { kinstWrite(address, value); }
static void kinstWriteWord(UINT32 address, UINT32 value) { kinstWrite(address, value); }
static void kinstWriteDouble(UINT32 address, UINT64 value) { kinstWrite(address, value); }

static UINT8 kinstReadByte(UINT32 address) { return kinstRead(address); }
static UINT16 kinstReadHalf(UINT32 address) { return kinstRead(address); }
static UINT32 kinstReadWord(UINT32 address) { return kinstRead(address); }
static UINT64 kinstReadDouble(UINT32 address) { return kinstRead(address); }


static void MakeInputs()
{
    DrvInputs[0] = 0;
    DrvInputs[1] = 0;

    for (int i = 0; i < 12; i++) {
        if (DrvJoy1[i] & 1)
            DrvInputs[0] |= (1 << i);
        if (DrvJoy2[i] & 1)
            DrvInputs[1] |= (1 << i);
    }
}

static INT32 DrvInit()
{
    printf("kinst: DrvInit\n");
    MemIndex();
    INT32 nLen = MemEnd - (UINT8 *)0;

    if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL)
        return 1;

    DrvDisk = new ide::ide_disk();
    DrvDisk->set_irq_callback(IDESetIRQState);

    printf("kinst: loading image at kinst.img\n");
    // FIXME:
    if (!DrvDisk->load_disk_image("kinst.img")) {
        printf("kinst: harddisk image not found!");
        return 1;
    }

    MemIndex();

    GenerateColorLUT();

    UINT32 nRet = BurnLoadRom(DrvBootROM, 0, 0);
    if (nRet != 0)
        return 1;

#ifdef MIPS3_X64_DRC
    Mips3UseRecompiler(true);
#endif
    Mips3Init();
    Mips3Reset();

    DrvVRAMBase = 0x30000;

    Mips3MapMemory(DrvBootROM,  0x1FC00000, 0x1FC7FFFF, MAP_READ);
    Mips3MapMemory(DrvRAM0,     0x00000000, 0x0007FFFF, MAP_RAM);
    Mips3MapMemory(DrvRAM1,     0x08000000, 0x087FFFFF, MAP_RAM);

    Mips3SetReadByteHandler(1, kinstReadByte);
    Mips3SetReadHalfHandler(1, kinstReadHalf);
    Mips3SetReadWordHandler(1, kinstReadWord);
    Mips3SetReadDoubleHandler(1, kinstReadDouble);

    Mips3SetWriteByteHandler(1, kinstWriteByte);
    Mips3SetWriteHalfHandler(1, kinstWriteHalf);
    Mips3SetWriteWordHandler(1, kinstWriteWord);
    Mips3SetWriteDoubleHandler(1, kinstWriteDouble);

    Mips3MapHandler(1, 0x10000000, 0x100001FF, MAP_READ | MAP_WRITE);
    return 0;
}

static INT32 DrvExit()
{
    delete DrvDisk;
    printf("kinst: DrvExit\n");
    Mips3Exit();
    BurnFree(AllMem);
    return 0;
}

static INT32 DrvDraw()
{
    UINT16 *src = (UINT16*) &DrvRAM0[DrvVRAMBase];

    for (int y = 0; y < 240; y++) {
        UINT16 *dst = (UINT16*) pBurnDraw + (y * 320);

        for (int x = 0; x < 320; x++) {
            *dst = DrvColorLUT[*src & 0x7FFF];
            dst++;
            src++;
        }
    }
    return 0;
}

#define MHz(x)  (x * 1000000)
#define kHz(x)  (x * 1000)

// R4600: 100 MHz
// VIDEO:  60  Hz
// VBLANK: 20 kHz, 50us (from MAME))
//                 50us = 20kHz
//
static INT32 DrvFrame()
{
    MakeInputs();

    const long fps = 60;
    const long vblankCycles = kHz(20) * fps;
    const long cycles = (MHz(100) - vblankCycles) / fps ;

    Mips3SetIRQLine(VBLANK_IRQ, 0);
    Mips3Run(cycles);

    if (pBurnDraw) {
        DrvDraw();
    }

    Mips3SetIRQLine(VBLANK_IRQ, 1);
    Mips3Run(kHz(20));

    return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
    return 0;
}

static struct BurnRomInfo kinstRomDesc[] = {
    { "ki-l15d.u98",		0x80000, 0x7b65ca3d, 1 | BRF_PRG | BRF_ESS }, //  0 MIPS R4600 Code
};

STD_ROM_PICK(kinst)
STD_ROM_FN(kinst)

struct BurnDriver BurnDrvKinst = {
    "kinst", "kinst", NULL, NULL, "1994/1995",
    "Killer Instinct (ver. 1.5)\0", NULL, "Rare/Nintendo", "MIDWAY",
    NULL, NULL, NULL, NULL,
    BDF_GAME_WORKING, 2, HARDWARE_PREFIX_MIDWAY, GBF_VSFIGHT, 0,
    NULL, kinstRomInfo, kinstRomName, NULL, NULL, kinstInputInfo, kinstDIPInfo,
    DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x8000,
    320, 240, 4, 3
};
