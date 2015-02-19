
#include "driver.h"
#include "burnint.h"
#include "mips3_intf.h"
#include <stdio.h>

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvBootROM;
static UINT8 *DrvRAM0;
static UINT8 *DrvRAM1;
static UINT8 DrvRecalc;
static UINT8 DrvJoy1[32];

static struct BurnInputInfo kinstInputList[] = {
    {"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
    {"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
    {"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
    {"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
    {"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 left"	},
    {"P1 Right",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
    {"P1 Button 1",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
    {"P1 Button 2",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},
    {"P1 Button 3",	BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 3"	},
    {"P1 Button 4",	BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 1"	},
    {"P1 Button 5",	BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 2"	},
    {"P1 Button 6",	BIT_DIGITAL,	DrvJoy1 + 11,	"p1 fire 3"	},
};

STDINPUTINFO(kinst)

static struct BurnDIPInfo kinstDIPList[]=
{
};

STDDIPINFO(kinst)

static INT32 MemIndex()
{
    UINT8 *Next; Next = AllMem;

    DrvBootROM 	= Next; Next += 0x80000;
    AllRam      = Next;
    DrvRAM0     = Next; Next += 0x80000;
    DrvRAM1     = Next; Next += 0x800000;

    RamEnd		= Next;
    MemEnd		= Next;
    return 0;
}


static INT32 DrvInit()
{
    printf("kinst: DrvInit\n");
    MemIndex();
    INT32 nLen = MemEnd - (UINT8 *)0;

    if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL)
        return 1;

    MemIndex();

    UINT32 nRet = BurnLoadRom(DrvBootROM, 0, 0);
    if (nRet != 0)
        return 1;

    Mips3Init();
    Mips3Reset();

    Mips3MapMemory(DrvBootROM, 0x1FC00000, 0x1FC080000, MAP_READ);
    Mips3MapMemory(DrvRAM0, 0x00000000, 0x00080000, MAP_RAM);
    Mips3MapMemory(DrvRAM1, 0x08000000, 0x08800000, MAP_RAM);

    return 0;
}

static INT32 DrvExit()
{
    printf("kinst: DrvExit\n");
    Mips3Exit();
    BurnFree(AllMem);
    return 0;
}

static INT32 DrvDraw()
{
    UINT16 *src = (UINT16*) &DrvRAM0[0x30000];

    for (int y = 0; y < 240; y++) {
        UINT16 *dst = (UINT16*) pBurnDraw + (y * 320);

        for (int x = 0; x < 320; x++) {
            UINT16 col = *src;
            *dst = (col & 0x3E0) | ((col >> 10) & 0x1F) | ((col & 0x1F) << 10);
            dst++;
            src++;
        }
    }
    return 0;
}

static INT32 DrvFrame()
{
    Mips3SetIRQLine(0, 0);
    Mips3Run(1000000/4);

    Mips3SetIRQLine(0, 1);
    Mips3Run(1000000/4);

    if (pBurnDraw) {
        DrvDraw();
    }
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
    DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
    320, 240, 4, 3
};
