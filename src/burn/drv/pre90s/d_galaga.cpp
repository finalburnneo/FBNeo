// Galaga & Dig-Dug driver for FB Alpha, based on the MAME driver by Nicola Salmoria & previous work by Martin Scragg, Mirko Buffoni, Aaron Giles
// Dig Dug added July 27, 2015 - dink
// Xevious added April 22, 2019 - CupcakeFan

#include "tiles_generic.h"
#include "z80_intf.h"
#include "namco_snd.h"
#include "samples.h"
#include "earom.h"

enum
{
	CPU1 = 0,
	CPU2,
	CPU3,
	NAMCO_BRD_CPU_COUNT
};

struct CPU_Control_Def
{
	UINT8 fireIRQ;
	UINT8 halt;
};

struct CPU_Def
{
	struct CPU_Control_Def CPU[NAMCO_BRD_CPU_COUNT];
};

static struct CPU_Def cpus = { 0 };

struct CPU_Memory_Map_Def
{
	UINT8    **byteArray;
	UINT32   startAddress;
	UINT32   endAddress;
	UINT32   type;
};

struct CPU_Config_Def
{
	UINT32   id;
	UINT8 (__fastcall *z80ProgRead)(UINT16 addr);
	void (__fastcall *z80ProgWrite)(UINT16 addr, UINT8 dta);
	void (*z80MemMap)(void);
};

struct Memory_Def
{
	struct
	{
		UINT8  *start;
		UINT32 size;
	} all;
	struct
	{
		UINT8 *start;
		UINT32 size;
		UINT8 *video;
		UINT8 *shared1;
		UINT8 *shared2;
		UINT8 *shared3;
	} RAM;
	struct
	{
		UINT8 *rom1;
		UINT8 *rom2;
		UINT8 *rom3;
		UINT8 *rom4;
	} Z80;
	struct
	{
		UINT8 *palette;
		UINT8 *charLookup;
		UINT8 *spriteLookup;
	} PROM;
};

static struct Memory_Def memory;

enum
{
	MEM_PGM = 0,
	MEM_RAM,
	MEM_ROM,
	MEM_DATA,
	MEM_DATA32,
	MEM_TYPES
};

struct Memory_Map_Def
{
	union
	{
		UINT8    **uint8;
		UINT32   **uint32;
	} region;
	UINT32   size;
	UINT32   type;
};

struct ROM_Load_Def
{
	UINT8    **address;
	UINT32   offset;
	INT32    (*postProcessing)(void);
};

static UINT8 *tempRom = NULL;
static UINT8 *gameData; // digdug playfield data

struct Graphics_Def
{
	UINT8 *fgChars;
	UINT8 *sprites;
	UINT8 *bgTiles;
	UINT32 *palette;
};

static struct Graphics_Def graphics;

/* Weird video definitions...
 *  +---+
 *  |   |
 *  |   |
 *  |   | screen_height, x, tilemap_width
 *  |   |
 *  +---+
 *  screen_width, y, tilemap_height
 */
#define NAMCO_SCREEN_WIDTH    224
#define NAMCO_SCREEN_HEIGHT   288

#define NAMCO_TMAP_WIDTH      36
#define NAMCO_TMAP_HEIGHT     28


static const INT32 Colour2Bit[4] =
{
	0x00, 0x47, 0x97, 0xde
};

static const INT32 Colour3Bit[8] =
{
	0x00, 0x21, 0x47, 0x68,
	0x97, 0xb8, 0xde, 0xff
};

static const INT32 Colour4Bit[16] =
{
	0x00, 0x0e, 0x1f, 0x2d,
	0x43, 0x51, 0x62, 0x70,
	0x8f, 0x9d, 0xae, 0xbc,
	0xd2, 0xe0, 0xf1, 0xff
};

#define NAMCO_BRD_INP_COUNT      3

struct InputSignalBits_Def
{
	UINT8 bit[8];
};

struct InputSignal_Def
{
	struct InputSignalBits_Def bits;
	UINT8 byte;
};

struct Port_Def
{
	struct InputSignal_Def previous;
	struct InputSignal_Def current;
};

struct Input_Def
{
	struct Port_Def ports[NAMCO_BRD_INP_COUNT];
	struct InputSignal_Def dip[2];
	UINT8 reset;
};

static struct Input_Def input;

/* check directions, according to the following 8-position rule */
/*         0          */
/*        7 1         */
/*       6 8 2        */
/*        5 3         */
/*         4          */
static const UINT8 namcoControls[16] = {
	/* 0000, 0001, 0010, 0011, 0100, 0101, 0110, 0111, 1000, 1001, 1010, 1011, 1100, 1101, 1110, 1111  */
	/* LDRU, LDRu, LDrU, LDru, LdRU, LdRu, LdrU, Ldru, lDRU, lDRu, lDrU, lDru, ldRU, ldRu, ldrU, ldru  */
	8,    8,    8,    5,    8,    8,    7,    6,    8,    3,    8,    4,    1,    2,    0,    8
};

struct CPU_Rd_Table
{
	UINT16 startAddr;
	UINT16 endAddr;
	UINT8 (*readFunc)(UINT16 offset);
};

struct CPU_Wr_Table
{
	UINT16 startAddr;
	UINT16 endAddr;
	void (*writeFunc)(UINT16 offset, UINT8 dta);
};

struct Namco_Custom_RW_Entry
{
	UINT8 n06xxCmd;
	UINT8 (*customRWFunc)(UINT8 offset, UINT8 writeDta);
};

enum SpriteFlags
{
	X_FLIP = 0,
	Y_FLIP,
	X_SIZE,
	Y_SIZE
};

#define xFlip (1 << X_FLIP)
#define yFlip (1 << Y_FLIP)
#define xSize (1 << X_SIZE)
#define ySize (1 << Y_SIZE)
#define orient (xFlip | yFlip)

struct Namco_Sprite_Params
{
	INT32 sprite;
	INT32 colour;
	INT32 xStart;
	INT32 yStart;
	INT32 xStep;
	INT32 yStep;
	INT32 flags;
	INT32 paletteBits;
	INT32 paletteOffset;
	UINT8 transparent_mask;
};

#define N06XX_BUF_SIZE       16

struct N06XX_Def
{
	UINT8 customCommand;
	UINT8 CPU1FireNMI;
	UINT8 buffer[N06XX_BUF_SIZE];
};

struct N50XX_Def
{
	UINT8 input;
};

struct N51XX_Def
{
	UINT8 mode;
	UINT8 leftCoinPerCredit;
	UINT8 leftCreditPerCoins;
	UINT8 rightCoinPerCredit;
	UINT8 rightCreditPerCoins;
	UINT8 auxCoinPerCredit;
	UINT8 auxCreditPerCoins;
	UINT8 leftCoinsInserted;
	UINT8 rightCoinsInserted;
	UINT8 credits;
	UINT8 startEnable;
	UINT8 remapJoystick;
	UINT8 coinCreditDataCount;
	UINT8 coinCreditDataIndex;
};

#define NAMCO54XX_CFG1_SIZE   4
#define NAMCO54XX_CFG2_SIZE   4
#define NAMCO54XX_CFG3_SIZE   5

struct N54XX_Def
{
	INT32 fetch;
	UINT8 *fetchDestination;
	UINT8 config1[NAMCO54XX_CFG1_SIZE];
	UINT8 config2[NAMCO54XX_CFG2_SIZE];
	UINT8 config3[NAMCO54XX_CFG3_SIZE];
};

struct N54XX_Sample_Info_Def
{
	INT32 sampleNo;
	UINT8 sampleTrigger[8];
};

static struct NCustom_Def
{
	struct N06XX_Def  n06xx;
	struct N50XX_Def  n50xx;
	struct N51XX_Def  n51xx;
	struct N54XX_Def  n54xx;
} namcoCustomIC;

struct Machine_Config_Def
{
	struct CPU_Config_Def         *cpus;
	struct CPU_Wr_Table           *wrAddrList;
	struct CPU_Rd_Table           *rdAddrList;
	struct Memory_Map_Def         *memMapTable;
	UINT32                        sizeOfMemMapTable;
	struct ROM_Load_Def           *romLayoutTable;
	UINT32                        sizeOfRomLayoutTable;
	UINT32                        tempRomSize;
	INT32                         (*tilemapsConfig)(void);
	void                          (**drawLayerTable)(void);
	UINT32                        drawTableSize;
	UINT32                        (*getSpriteParams)(struct Namco_Sprite_Params *spriteParams, UINT32 offset);
	INT32                         (*reset)(void);
	struct Namco_Custom_RW_Entry  *customRWTable;
	struct N54XX_Sample_Info_Def  *n54xxSampleList;
};

enum GAMES_ON_MACHINE
{
	NAMCO_GALAGA = 0,
	NAMCO_DIGDUG,
	NAMCO_XEVIOUS,
	NAMCO_TOTAL_GAMES
};

struct Machine_Def
{
	struct Machine_Config_Def *config;
	enum GAMES_ON_MACHINE game;
	UINT8 starsInitted;
	UINT8 flipScreen;
	UINT32 numOfDips;
};

static struct Machine_Def machine = { 0 };

enum
{
	NAMCO_1BIT_PALETTE_BITS = 1,
	NAMCO_2BIT_PALETTE_BITS
};

static const INT32 planeOffsets1Bit[NAMCO_1BIT_PALETTE_BITS] =
{ 0 };
static const INT32 planeOffsets2Bit[NAMCO_2BIT_PALETTE_BITS] =
{ 0, 4 };

static const INT32 xOffsets8x8Tiles1Bit[8]      = { STEP8(7,-1) };
static const INT32 yOffsets8x8Tiles1Bit[8]      = { STEP8(0,8) };
static const INT32 xOffsets8x8Tiles2Bit[8]      = { 64, 65, 66, 67, 0, 1, 2, 3 };
static const INT32 yOffsets8x8Tiles2Bit[8]      = { 0, 8, 16, 24, 32, 40, 48, 56 };
static const INT32 xOffsets16x16Tiles2Bit[16]   = { 0,   1,   2,   3,   64,  65,  66,  67,
128, 129, 130, 131, 192, 193, 194, 195 };
static const INT32 yOffsets16x16Tiles2Bit[16]   = { 0,   8,   16,  24,  32,  40,  48,  56,
256, 264, 272, 280, 288, 296, 304, 312 };

typedef void (*DrawFunc_t)(void);

static INT32 namcoInitBoard(void);
static INT32 namcoMachineInit(void);
static void machineReset(void);
static INT32 DrvDoReset(void);

static INT32 namcoMemIndex(void);
static INT32 namcoLoadGameROMS(void);

static void namcoCustomReset(void);
static void namco51xxReset(void);

static UINT8 updateJoyAndButtons(UINT16 offset, UINT8 jp);
static UINT8 namco51xxRead(UINT8 offset, UINT8 dummyDta);
static UINT8 namco50xxRead(UINT8 offset, UINT8 dummyDta);
static UINT8 namco53xxRead(UINT8 offset, UINT8 dummyDta);
static UINT8 namcoCustomICsReadDta(UINT16 offset);

static UINT8 namco51xxWrite(UINT8 offset, UINT8 dta);
static INT32 n54xxCheckBuffer(UINT8 *n54xxBuffer, UINT32 bufferSize);
static UINT8 namco50xxWrite(UINT8 offset, UINT8 dta);
static UINT8 namco54xxWrite(UINT8 offset, UINT8 dta);
static void namcoCustomICsWriteDta(UINT16 offset, UINT8 dta);

static UINT8 namcoCustomICsReadCmd(UINT16 offset);
static void namcoCustomICsWriteCmd(UINT16 offset, UINT8 dta);

static UINT8 namcoZ80ReadDip(UINT16 offset);

static UINT8 __fastcall namcoZ80ProgRead(UINT16 addr);

static void namcoZ80WriteSound(UINT16 offset, UINT8 dta);
static void namcoZ80WriteCPU1Irq(UINT16 offset, UINT8 dta);
static void namcoZ80WriteCPU2Irq(UINT16 offset, UINT8 dta);
static void namcoZ80WriteCPU3Irq(UINT16 offset, UINT8 dta);
static void namcoZ80WriteCPUReset(UINT16 offset, UINT8 dta);
static void namcoZ80WriteFlipScreen(UINT16 offset, UINT8 dta);
static void __fastcall namcoZ80ProgWrite(UINT16 addr, UINT8 dta);

static tilemap_scan ( namco );
static void namcoRenderSprites(void);

static INT32 DrvExit(void);
static void DrvMakeInputs(void);
static INT32 DrvFrame(void);

static INT32 DrvScan(INT32 nAction, INT32 *pnMin);

/* === Common === */

static INT32 namcoInitBoard(void)
{
	// Allocate and Blank all required memory
	memset(&memory, 0, sizeof(memory));
	namcoMemIndex();

	memory.all.start = (UINT8 *)BurnMalloc(memory.all.size);
	if (NULL == memory.all.start)
		return 1;
	memset(memory.all.start, 0, memory.all.size);

	namcoMemIndex();

	return namcoLoadGameROMS();
}

static INT32 namcoMachineInit(void)
{
	INT32 retVal = 0;

	for (INT32 cpuCount = CPU1; cpuCount < NAMCO_BRD_CPU_COUNT; cpuCount ++)
	{
		struct CPU_Config_Def *currentCPU = &machine.config->cpus[cpuCount];
		ZetInit(currentCPU->id);
		ZetOpen(currentCPU->id);
		ZetSetReadHandler(currentCPU->z80ProgRead);
		ZetSetWriteHandler(currentCPU->z80ProgWrite);
		currentCPU->z80MemMap();
		ZetClose();
	}

	NamcoSoundInit(18432000 / 6 / 32, 3, 0);
	NamcoSoundSetAllRoutes(0.90 * 10.0 / 16.0, BURN_SND_ROUTE_BOTH);
	NamcoSoundSetBuffered(ZetTotalCycles, 3072000);
	BurnSampleInit(1);
	BurnSampleSetAllRoutesAllSamples(0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	if (machine.config->tilemapsConfig)
	{
		retVal = machine.config->tilemapsConfig();
	}

	if (0 == retVal)
	{
		// Reset the driver
		machine.config->reset();
	}

	return retVal;
}

static void machineReset()
{
	cpus.CPU[CPU1].fireIRQ = 0;
	cpus.CPU[CPU2].fireIRQ = 0;
	cpus.CPU[CPU3].fireIRQ = 0;
	cpus.CPU[CPU2].halt = 0;
	cpus.CPU[CPU3].halt = 0;

	machine.flipScreen = 0;

	namcoCustomReset();
	namco51xxReset();

}

static INT32 DrvDoReset(void)
{
	for (INT32 i = 0; i < NAMCO_BRD_CPU_COUNT; i ++)
	{
		ZetReset(i);
	}

	BurnSampleReset();
	NamcoSoundReset();

	machineReset();

	HiscoreReset();

	return 0;
}

static INT32 namcoMemIndex(void)
{
	struct Memory_Map_Def *memoryMapEntry = machine.config->memMapTable;
	if (NULL == memoryMapEntry) return 1;

	UINT8 *next = memory.all.start;

	UINT32 i = 0;
	while (i < machine.config->sizeOfMemMapTable)
	{
		if (next)
		{
			if ((MEM_RAM == memoryMapEntry->type) && (0 == memory.RAM.start))
			{
				memory.RAM.start = next;
			}
			switch (memoryMapEntry->type)
			{
				case MEM_DATA32:
					*(memoryMapEntry->region.uint32) = (UINT32 *)next;
					break;

				default:
					*(memoryMapEntry->region.uint8) = next;
					break;
			}
			next += memoryMapEntry->size;
			if ( (MEM_RAM == memoryMapEntry->type) &&
				(memory.RAM.size < (next - memory.RAM.start)) )
			{
				memory.RAM.size = next - memory.RAM.start;
			}
		}
		else
		{
			memory.all.size += memoryMapEntry->size;
		}

		i ++;
		memoryMapEntry ++;
	}

	return 0;
}

static INT32 namcoLoadGameROMS(void)
{
	struct ROM_Load_Def *romEntry = machine.config->romLayoutTable;
	UINT32 tableSize = machine.config->sizeOfRomLayoutTable;
	UINT32 tempSize = machine.config->tempRomSize;
	INT32 retVal = 1;

	if (tempSize) tempRom = (UINT8 *)BurnMalloc(tempSize);

	if ((NULL != tempRom) && (NULL != romEntry))
	{
		memset(tempRom, 0, tempSize);

		retVal = 0;

		for (UINT32 idx = 0; ((idx < tableSize) && (0 == retVal)); idx ++)
		{
			retVal = BurnLoadRom(*(romEntry->address) + romEntry->offset, idx, 1);
			if ((0 == retVal) && (NULL != romEntry->postProcessing))
				retVal = romEntry->postProcessing();

			romEntry ++;
		}

		BurnFree(tempRom);
	}

	return retVal;
}

/* derived from the latest emulation contained in MAME
 */
static void namcoCustomReset(void)
{
	memset(&namcoCustomIC, 0, sizeof(struct NCustom_Def));
}

static void namco51xxReset(void)
{
	namcoCustomIC.n51xx.coinCreditDataCount = 0;

	namcoCustomIC.n51xx.leftCoinPerCredit = 0;
	namcoCustomIC.n51xx.leftCreditPerCoins = 0;
	namcoCustomIC.n51xx.rightCoinPerCredit = 0;
	namcoCustomIC.n51xx.rightCreditPerCoins = 0;
	namcoCustomIC.n51xx.auxCoinPerCredit = 0;
	namcoCustomIC.n51xx.auxCreditPerCoins = 0;

	namcoCustomIC.n51xx.credits = 0;
	namcoCustomIC.n51xx.leftCoinsInserted = 0;
	namcoCustomIC.n51xx.rightCoinsInserted = 0;

	namcoCustomIC.n51xx.startEnable = 0;

	namcoCustomIC.n51xx.remapJoystick = 0;

	input.ports[0].current.byte = 0xff;
	input.ports[1].current.byte = 0xff;
	input.ports[2].current.byte = 0xff;

	input.ports[0].previous.byte = input.ports[0].current.byte;
	input.ports[1].previous.byte = input.ports[1].current.byte;
	input.ports[2].previous.byte = input.ports[2].current.byte;

}

/*
 * joy.5 | joy.4 | toggle | in.0 | last.0
 *   1   |   1   |    0   |   0  |  0     (released)
 *   0   |   0   |    1   |   1  |  0     (just pressed)
 *   0   |   1   |    0   |   1  |  1     (held)
 *   1   |   1   |    1   |   0  |  1     (just released)
 */
static UINT8 updateJoyAndButtons(UINT16 offset, UINT8 jp)
{
	UINT8 portValue = input.ports[1].current.byte;

	UINT8 joy = portValue & 0x0f;
	if (namcoCustomIC.n51xx.remapJoystick) joy = namcoControls[joy];

	UINT8 portLast = input.ports[1].previous.byte;

	UINT8 buttonsValue = 0;

	UINT8 buttonsDown = ~(portValue & 0xf0);
	UINT8 buttonsLast = ~(portLast  & 0xf0);

	UINT8 toggle = buttonsDown ^ buttonsLast;

	switch (offset)
	{
		case 1:
			{
				/* fire */
				buttonsValue  =  ((toggle & buttonsDown & 0x10)^0x10);
				buttonsValue |= (((         buttonsDown & 0x10)^0x10) << 1);
			}
			break;

		case 2:
			{
				buttonsDown <<= 1;
				buttonsLast <<= 1;

				toggle <<= 1;

				/* fire */
				buttonsValue  = (((toggle & buttonsDown & 0x20)^0x20)>>1);
				buttonsValue |=  ((         buttonsDown & 0x20)^0x20);
			}
			break;

		default:
			break;
	}

	input.ports[offset].previous.byte = input.ports[offset].current.byte;

	return (joy | buttonsValue);
}

static UINT8 namco51xxRead(UINT8 offset, UINT8 dummyDta)
{
	UINT8 retVal = 0xff;

	switch (offset)
	{
		case 0:
			{
				if (0 == namcoCustomIC.n51xx.mode)
				{
					retVal = input.ports[0].current.byte;
				}
				else
				{
					UINT8 in = input.ports[0].current.byte;
					UINT8 toggle = in ^ input.ports[0].previous.byte;

					if (0 < namcoCustomIC.n51xx.leftCoinPerCredit)
					{
						if (99 >= namcoCustomIC.n51xx.credits)
						{
							if (in & toggle & 0x10)
							{
								namcoCustomIC.n51xx.leftCoinsInserted ++;
								if (namcoCustomIC.n51xx.leftCoinsInserted >= namcoCustomIC.n51xx.leftCoinPerCredit)
								{
									namcoCustomIC.n51xx.credits += namcoCustomIC.n51xx.leftCreditPerCoins;
									namcoCustomIC.n51xx.leftCoinsInserted -= namcoCustomIC.n51xx.leftCoinPerCredit;
								}
							}
							if (in & toggle & 0x20)
							{
								namcoCustomIC.n51xx.rightCoinsInserted ++;
								if (namcoCustomIC.n51xx.rightCoinsInserted >= namcoCustomIC.n51xx.rightCoinPerCredit)
								{
									namcoCustomIC.n51xx.credits += namcoCustomIC.n51xx.rightCreditPerCoins;
									namcoCustomIC.n51xx.rightCoinsInserted -= namcoCustomIC.n51xx.rightCoinPerCredit;
								}
							}
							if (in & toggle & 0x40)
							{
								namcoCustomIC.n51xx.credits ++;
							}
						}
					}
#ifndef FORCE_FREEPLAY
					else
#endif
					{
						namcoCustomIC.n51xx.credits = 100;
					}

					if (namcoCustomIC.n51xx.startEnable)
					{
						if (toggle & in & 0x04)
						{
							if (1 <= namcoCustomIC.n51xx.credits)
							{
								namcoCustomIC.n51xx.credits --;
							}
						}

						else if (toggle & in & 0x08)
						{
							if (2 <= namcoCustomIC.n51xx.credits)
							{
								namcoCustomIC.n51xx.credits -= 2;
							}
						}
					}

					retVal = (namcoCustomIC.n51xx.credits / 10) * 16 + namcoCustomIC.n51xx.credits % 10;

					if (~in & 0x80)
					{
						retVal = 0xbb;
					}
				}
				input.ports[0].previous.byte = input.ports[0].current.byte;
			}
			break;

		case 1:
		case 2:
			{
				retVal = updateJoyAndButtons(offset, input.ports[offset].current.byte);
			}
			break;

		default:
			{
			}
			break;
	}

	return retVal;
}

static UINT8 namco50xxRead(UINT8 offset, UINT8 dummyDta)
{
	UINT8 retVal = 0;

	if (3 == offset)
	{
		if ((0x80 == namcoCustomIC.n50xx.input) ||
			(0x10 == namcoCustomIC.n50xx.input) )
			retVal = 0x05;
		else
			retVal = 0x95;
	}

	return retVal;
}


static UINT8 namco53xxRead(UINT8 offset, UINT8 dummyDta)
{
	UINT8 retVal = 0xff;

	if ( (0 == offset) || (1 == offset) )
		retVal = input.dip[offset].byte;

	return retVal;
}

static UINT8 namcoCustomICsReadDta(UINT16 offset)
{
	UINT8 retVal = 0xff;

	struct Namco_Custom_RW_Entry *customRdEntry = machine.config->customRWTable;
	if (NULL != customRdEntry)
	{
		while (NULL != customRdEntry->customRWFunc)
		{
			if (namcoCustomIC.n06xx.customCommand == customRdEntry->n06xxCmd)
			{
				retVal = customRdEntry->customRWFunc((UINT8)(offset & 0xff), 0);
			}
			customRdEntry ++;
		}
	}

	return retVal;
}

static UINT8 namco50xxWrite(UINT8 offset, UINT8 dta)
{
	if (0 == offset)
		namcoCustomIC.n50xx.input = dta;

	return 0;
}

static UINT8 namco51xxWrite(UINT8 offset, UINT8 dta)
{
	dta &= 0x07;

	if (namcoCustomIC.n51xx.coinCreditDataCount)
	{
		namcoCustomIC.n51xx.coinCreditDataIndex ++;

		if (namcoCustomIC.n51xx.coinCreditDataIndex >= namcoCustomIC.n51xx.coinCreditDataCount)
		{
			namcoCustomIC.n51xx.coinCreditDataCount = 0;
		}

		switch (namcoCustomIC.n51xx.coinCreditDataIndex)
		{
			case 1:
				namcoCustomIC.n51xx.leftCoinPerCredit = dta;
				break;

			case 2:
				namcoCustomIC.n51xx.leftCreditPerCoins = dta;
				break;

			case 3:
				namcoCustomIC.n51xx.rightCoinPerCredit = dta;
				break;

			case 4:
				namcoCustomIC.n51xx.rightCreditPerCoins = dta;
				break;

			case 5:
				namcoCustomIC.n51xx.auxCoinPerCredit = dta;
				break;

			case 6:
				namcoCustomIC.n51xx.auxCreditPerCoins = dta;
				break;

			default:
				break;
		}
	}
	else
	{
		switch (dta)
		{
			case 0: // nop
				break;

			case 1:
				switch (machine.game)
				{
					case NAMCO_XEVIOUS:
						{
							namcoCustomIC.n51xx.coinCreditDataCount = 6;
							namcoCustomIC.n51xx.remapJoystick = 1;
						}
						break;

					default:
						{
							namcoCustomIC.n51xx.coinCreditDataCount = 4;
						}
						break;
				}
				namcoCustomIC.n51xx.coinCreditDataIndex = 0;
				break;

			case 2:
				namcoCustomIC.n51xx.mode = 1;
				namcoCustomIC.n51xx.startEnable = 1;
				break;

			case 3:
				namcoCustomIC.n51xx.remapJoystick = 0;
				break;

			case 4:
				namcoCustomIC.n51xx.remapJoystick = 1;
				break;

			case 5:
				memset(&namcoCustomIC.n51xx, 0, sizeof(struct N51XX_Def));
				input.ports[0].previous.byte = input.ports[0].current.byte;
				break;

			default:
				bprintf(PRINT_ERROR, _T("unknown 51XX command %02x\n"), dta);
				break;
		}
	}

	return 0;
}

#define n54xxDebug 0

static INT32 n54xxCheckBuffer(UINT8 *n54xxBuffer, UINT32 bufferSize)
{
	INT32 retVal = -1;

#if n54xxDebug
	char bufCheck[32];
#endif

	struct N54XX_Sample_Info_Def *sampleEntry = machine.config->n54xxSampleList;

	if (NULL != sampleEntry)
	{
		while (0 <= sampleEntry->sampleNo)
		{
			if (0 == memcmp(n54xxBuffer, sampleEntry->sampleTrigger, bufferSize) )
			{
				retVal = sampleEntry->sampleNo;
			}
#if n54xxDebug
			sprintf(bufCheck, "%d, %d %02x,%02x,%02x,%02x : %02x,%02x,%02x,%02x",
					retVal,
					sampleEntry->sampleNo,
					n54xxBuffer[0],
					n54xxBuffer[1],
					n54xxBuffer[2],
					n54xxBuffer[3],
					sampleEntry->sampleTrigger[0],
					sampleEntry->sampleTrigger[1],
					sampleEntry->sampleTrigger[2],
					sampleEntry->sampleTrigger[3]
				   );
			bprintf(PRINT_NORMAL, _T("%S\n"), bufCheck);
#endif
			sampleEntry ++;
		}
#if n54xxDebug
		bprintf(PRINT_NORMAL, _T("  %d (%d)\n"), retVal, sampleEntry->sampleNo);
#endif
	}

	return retVal;
}

static UINT8 namco54xxWrite(UINT8 offset, UINT8 dta)
{
	if (namcoCustomIC.n54xx.fetch)
	{
		if (NULL != namcoCustomIC.n54xx.fetchDestination)
			*(namcoCustomIC.n54xx.fetchDestination ++) = dta;

		namcoCustomIC.n54xx.fetch --;
	}
	else
	{
		switch (dta & 0xf0)
		{
			case 0x00:
				break;

			case 0x10:	// output sound on pins 4-7 only
				{
					INT32 sampleNo = n54xxCheckBuffer(namcoCustomIC.n54xx.config1, NAMCO54XX_CFG1_SIZE);
					if (0 <= sampleNo) BurnSamplePlay(sampleNo);
				}
				break;

			case 0x20:	// output sound on pins 8-11 only
				{
					INT32 sampleNo = n54xxCheckBuffer(namcoCustomIC.n54xx.config2, NAMCO54XX_CFG2_SIZE);
					if (0 <= sampleNo) BurnSamplePlay(sampleNo);
				}
				break;

			case 0x30:
				{
					namcoCustomIC.n54xx.fetch = NAMCO54XX_CFG1_SIZE;
					namcoCustomIC.n54xx.fetchDestination = namcoCustomIC.n54xx.config1;
				}
				break;

			case 0x40:
				{
					namcoCustomIC.n54xx.fetch = NAMCO54XX_CFG2_SIZE;
					namcoCustomIC.n54xx.fetchDestination = namcoCustomIC.n54xx.config2;
				}
				break;

			case 0x50:	// output sound on pins 17-20 only
				{
					INT32 sampleNo = n54xxCheckBuffer(namcoCustomIC.n54xx.config3, NAMCO54XX_CFG3_SIZE);
					if (0 <= sampleNo) BurnSamplePlay(sampleNo);
				}
				break;

			case 0x60:
				{
					namcoCustomIC.n54xx.fetch = NAMCO54XX_CFG3_SIZE;
					namcoCustomIC.n54xx.fetchDestination = namcoCustomIC.n54xx.config3;
				}
				break;

			case 0x70:
				{
					// polepos
					/* 0x7n = Screech sound. n = pitch (if 0 then no sound) */
					/* followed by 0x60 command? */
					if (0 == ( dta & 0x0f ))
					{
						//					if (sample_playing(1))
						//						sample_stop(1);
					}
					else
					{
						//					INT32 freq = (INT32)( ( 44100.0f / 10.0f ) * (float)(Data & 0x0f) );

						//					if (!sample_playing(1))
						//						sample_start(1, 1, 1);
						//					sample_set_freq(1, freq);
					}
				}
				break;

			default:
				break;
		}
	}

	return 0;
}

static void namcoCustomICsWriteDta(UINT16 offset, UINT8 dta)
{
	namcoCustomIC.n06xx.buffer[offset & 0x0f] = dta;

	UINT8 retVal = 0x0;
	struct Namco_Custom_RW_Entry *customWrEntry = machine.config->customRWTable;
	if (NULL != customWrEntry)
	{
		while (NULL != customWrEntry->customRWFunc)
		{
			if (namcoCustomIC.n06xx.customCommand == customWrEntry->n06xxCmd)
			{
				retVal += customWrEntry->customRWFunc((UINT8)(offset & 0xff), dta);
			}
			customWrEntry ++;
		}
	}
}

static UINT8 namcoCustomICsReadCmd(UINT16 offset)
{
	return namcoCustomIC.n06xx.customCommand;
}

static void namcoCustomICsWriteCmd(UINT16 offset, UINT8 dta)
{
	namcoCustomIC.n06xx.customCommand = dta;
	namcoCustomIC.n06xx.CPU1FireNMI = 1;

	switch (namcoCustomIC.n06xx.customCommand)
	{
		case 0x10:
			{
				namcoCustomIC.n06xx.CPU1FireNMI = 0;
			}
			break;

		default:
			break;
	}
}

static UINT8 namcoZ80ReadDip(UINT16 offset)
{
	UINT8 retVal = input.dip[1].bits.bit[offset] | input.dip[0].bits.bit[offset];

	return retVal;
}

static UINT8 __fastcall namcoZ80ProgRead(UINT16 addr)
{
	struct CPU_Rd_Table *rdEntry = machine.config->rdAddrList;
	UINT8 dta = 0;

	if (NULL != rdEntry)
	{
		while (NULL != rdEntry->readFunc)
		{
			if ( (addr >= rdEntry->startAddr) &&
				(addr <= rdEntry->endAddr)      )
			{
				dta = rdEntry->readFunc(addr - rdEntry->startAddr);
			}
			rdEntry ++;
		}
	}

	return dta;
}

static void namcoZ80WriteSound(UINT16 Offset, UINT8 dta)
{
	NamcoSoundWrite(Offset, dta);
}

static void namcoZ80WriteCPU1Irq(UINT16 Offset, UINT8 dta)
{
	cpus.CPU[CPU1].fireIRQ = dta & 0x01;

	if (!cpus.CPU[CPU1].fireIRQ)
	{
		ZetSetIRQLine(CPU1, 0, CPU_IRQSTATUS_NONE);
	}

}

static void namcoZ80WriteCPU2Irq(UINT16 Offset, UINT8 dta)
{
	cpus.CPU[CPU2].fireIRQ = dta & 0x01;
	if (!cpus.CPU[CPU2].fireIRQ)
	{
		ZetSetIRQLine(CPU2, 0, CPU_IRQSTATUS_NONE);
	}

}

static void namcoZ80WriteCPU3Irq(UINT16 Offset, UINT8 dta)
{
	cpus.CPU[CPU3].fireIRQ = !(dta & 0x01);

}

static void namcoZ80WriteCPUReset(UINT16 Offset, UINT8 dta)
{
	if (!(dta & 0x01))
	{
		ZetReset(CPU2);
		ZetReset(CPU3);
		cpus.CPU[CPU2].halt = 1;
		cpus.CPU[CPU3].halt = 1;
		namcoCustomReset();
		namco51xxReset();
		return;
	}
	else
	{
		cpus.CPU[CPU2].halt = 0;
		cpus.CPU[CPU3].halt = 0;
	}
}

static void namcoZ80WriteFlipScreen(UINT16 offset, UINT8 dta)
{
	machine.flipScreen = dta & 0x01;
}

static void __fastcall namcoZ80ProgWrite(UINT16 addr, UINT8 dta)
{
	struct CPU_Wr_Table *wrEntry = machine.config->wrAddrList;

	if (NULL != wrEntry)
	{
		while (NULL != wrEntry->writeFunc)
		{
			if ( (addr >= wrEntry->startAddr) &&
				(addr <= wrEntry->endAddr)      )
			{
				wrEntry->writeFunc(addr - wrEntry->startAddr, dta);
			}

			wrEntry ++;
		}
	}
}

static tilemap_scan ( namco )
{
	if ((col - 2) & 0x20)
	{
		return (row + 2 + (((col - 2) & 0x1f) << 5));
	}

	return col - 2 + ((row + 2) << 5);
}

static void namcoRenderSprites(void)
{
	struct Namco_Sprite_Params spriteParams;

	for (INT32 offset = 0; offset < 0x80; offset += 2)
	{
		if (machine.config->getSpriteParams(&spriteParams, offset))
		{
			INT32 spriteRows = ((spriteParams.flags & ySize) != 0);
			INT32 spriteCols = ((spriteParams.flags & xSize) != 0);

			for (INT32 y = 0; y <= spriteRows; y ++)
			{
				for (INT32 x = 0; x <= spriteCols; x ++)
				{
					INT32 code = spriteParams.sprite;
					if (spriteRows || spriteCols)
						code += ((y * 2 + x) ^ (spriteParams.flags & orient));
					INT32 xPos = spriteParams.xStart + spriteParams.xStep * x;
					INT32 yPos = spriteParams.yStart + spriteParams.yStep * y;

					if ((xPos < -15) || (xPos >= nScreenWidth) ) continue;
					if ((yPos < -15) || (yPos >= nScreenHeight)) continue;

					RenderTileTranstabOffset(pTransDraw, graphics.sprites, code,
											 spriteParams.colour << spriteParams.paletteBits,
											 spriteParams.transparent_mask, xPos, yPos,
											 spriteParams.flags & xFlip,
											 spriteParams.flags & yFlip, 16, 16,
											 memory.PROM.spriteLookup, spriteParams.paletteOffset);

				}
			}
		}
	}
}

static INT32 DrvExit(void)
{
	GenericTilesExit();

	NamcoSoundExit();
	BurnSampleExit();

	ZetExit();

	earom_exit();

	machineReset();

	BurnFree(memory.all.start);

	machine.game = NAMCO_GALAGA;
	machine.starsInitted = 0;

	return 0;
}

static void DrvMakeInputs(void)
{
	struct InputSignal_Def *currentPort0 = &input.ports[0].current;
	struct InputSignal_Def *currentPort1 = &input.ports[1].current;
	struct InputSignal_Def *currentPort2 = &input.ports[2].current;

	// Reset Inputs
	currentPort0->byte = 0xff;
	currentPort1->byte = 0xff;
	currentPort2->byte = 0xff;

	switch (machine.game)
	{
		case NAMCO_XEVIOUS:
			// map blaster inputs from ports to dip switches
			input.dip[0].byte |= 0x11;
			if (currentPort1->bits.bit[6]) input.dip[0].byte &= 0xFE;
			if (currentPort2->bits.bit[6]) input.dip[0].byte &= 0xEF;
			break;

		default:
			break;
	}

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i ++)
	{
		currentPort0->byte -= (currentPort0->bits.bit[i] & 1) << i;
		currentPort1->byte -= (currentPort1->bits.bit[i] & 1) << i;
		currentPort2->byte -= (currentPort2->bits.bit[i] & 1) << i;

		input.dip[0].bits.bit[i] = ((input.dip[0].byte >> i) & 1) << 0;
		input.dip[1].bits.bit[i] = ((input.dip[1].byte >> i) & 1) << 1;
	}
}

static INT32 DrvDraw(void)
{
	BurnTransferClear();

	if (NULL != machine.config->drawLayerTable)
	{
		for (UINT32 drawLayer = 0; drawLayer < machine.config->drawTableSize; drawLayer ++)
		{
			machine.config->drawLayerTable[drawLayer]();
		}

		BurnTransferCopy(graphics.palette);

		return 0;
	}
	return 1;
}

static INT32 DrvFrame(void)
{
	if (input.reset)
	{
		machine.config->reset();
	}

	DrvMakeInputs();

	INT32 nInterleave = 400;
	INT32 nCyclesTotal[3];
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	nCyclesTotal[0] = (18432000 / 6) / 60;
	nCyclesTotal[1] = (18432000 / 6) / 60;
	nCyclesTotal[2] = (18432000 / 6) / 60;

	ZetNewFrame();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(CPU1);
		CPU_RUN(0, Zet);
		if (i == (nInterleave-1) && cpus.CPU[CPU1].fireIRQ)
		{
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		if ( (9 == (i % 10)) && namcoCustomIC.n06xx.CPU1FireNMI )
		{
			ZetNmi();
		}
		ZetClose();

		if (!cpus.CPU[CPU2].halt)
		{
			ZetOpen(CPU2);
			CPU_RUN(1, Zet);
			if (i == (nInterleave-1) && cpus.CPU[CPU2].fireIRQ)
			{
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			}
			ZetClose();
		}

		if (!cpus.CPU[CPU3].halt)
		{
			ZetOpen(CPU3);
			CPU_RUN(2, Zet);
			if ( ((i == ((64 + 000) * nInterleave) / 272) ||
				  (i == ((64 + 128) * nInterleave) / 272))   && cpus.CPU[CPU3].fireIRQ)
			{
				ZetNmi();
			}
			ZetClose();
		}
	}

	if (pBurnSoundOut)
	{
		NamcoSoundUpdate(pBurnSoundOut, nBurnSoundLen);
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw)
		BurnDrvRedraw();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	// Return minimum compatible version
	if (pnMin != NULL)
	{
		*pnMin = 0x029737;
	}

	if (nAction & ACB_MEMORY_RAM)
	{
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = memory.RAM.start;
		ba.nLen	  = memory.RAM.size;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);			// Scan Z80

		NamcoSoundScan(nAction, pnMin);
		BurnSampleScan(nAction, pnMin);

		// Scan critical driver variables
		SCAN_VAR(cpus.CPU[CPU1].fireIRQ);
		SCAN_VAR(cpus.CPU[CPU2].fireIRQ);
		SCAN_VAR(cpus.CPU[CPU3].fireIRQ);
		SCAN_VAR(cpus.CPU[CPU2].halt);
		SCAN_VAR(cpus.CPU[CPU3].halt);
		SCAN_VAR(machine.flipScreen);
		SCAN_VAR(namcoCustomIC.n06xx.customCommand);
		SCAN_VAR(namcoCustomIC.n06xx.CPU1FireNMI);
		SCAN_VAR(namcoCustomIC.n51xx.mode);
		SCAN_VAR(namcoCustomIC.n51xx.credits);
		SCAN_VAR(namcoCustomIC.n51xx.leftCoinPerCredit);
		SCAN_VAR(namcoCustomIC.n51xx.leftCreditPerCoins);
		SCAN_VAR(namcoCustomIC.n51xx.rightCoinPerCredit);
		SCAN_VAR(namcoCustomIC.n51xx.rightCreditPerCoins);
		SCAN_VAR(namcoCustomIC.n51xx.auxCoinPerCredit);
		SCAN_VAR(namcoCustomIC.n51xx.auxCreditPerCoins);
		SCAN_VAR(namcoCustomIC.n51xx.coinCreditDataIndex);
		SCAN_VAR(namcoCustomIC.n51xx.coinCreditDataCount);
		SCAN_VAR(namcoCustomIC.n06xx.buffer);
		SCAN_VAR(input.ports);

		SCAN_VAR(namcoCustomIC.n54xx.fetch);
		SCAN_VAR(namcoCustomIC.n54xx.fetchDestination);
		SCAN_VAR(namcoCustomIC.n54xx.config1);
		SCAN_VAR(namcoCustomIC.n54xx.config2);
		SCAN_VAR(namcoCustomIC.n54xx.config3);
	}

	return 0;
}

/* === Galaga === */

static struct BurnInputInfo GalagaInputList[] =
{
	{"Start 1"           , BIT_DIGITAL  , &input.ports[0].current.bits.bit[2], "p1 start"  },
	{"Start 2"           , BIT_DIGITAL  , &input.ports[0].current.bits.bit[3], "p2 start"  },
	{"Coin 1"            , BIT_DIGITAL  , &input.ports[0].current.bits.bit[4], "p1 coin"   },
	{"Coin 2"            , BIT_DIGITAL  , &input.ports[0].current.bits.bit[5], "p2 coin"   },

	{"Left"              , BIT_DIGITAL  , &input.ports[1].current.bits.bit[3], "p1 left"   },
	{"Right"             , BIT_DIGITAL  , &input.ports[1].current.bits.bit[1], "p1 right"  },
	{"Fire 1"            , BIT_DIGITAL  , &input.ports[1].current.bits.bit[4], "p1 fire 1" },

	{"Left (Cocktail)"   , BIT_DIGITAL  , &input.ports[2].current.bits.bit[3], "p2 left"   },
	{"Right (Cocktail)"  , BIT_DIGITAL  , &input.ports[2].current.bits.bit[1], "p2 right"  },
	{"Fire 1 (Cocktail)" , BIT_DIGITAL  , &input.ports[2].current.bits.bit[4], "p2 fire 1" },

	{"Reset"             , BIT_DIGITAL  , &input.reset,                        "reset"     },
	{"Service"           , BIT_DIGITAL  , &input.ports[0].current.bits.bit[6], "service"   },
	{"Dip 1"             , BIT_DIPSWITCH, &input.dip[0].byte,                  "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH, &input.dip[1].byte,                  "dip"       },
};

STDINPUTINFO(Galaga)

#define GALAGA_NUM_OF_DIPSWITCHES      2

static struct BurnDIPInfo GalagaDIPList[]=
{
	// offset
	{  0x0b,    0xf0,    0x01,    0x01,       NULL                     },

	// Default Values
	// nOffset, nID,     nMask,   nDefault,   NULL
	{  0x00,    0xff,    0x01,    0x01,       NULL                     },
	{  0x01,    0xff,    0xff,    0x97,       NULL                     },
	{  0x02,    0xff,    0xbf,    0xb7,       NULL                     },

	// Service Switch
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Service Mode"           },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x01,    0x01,       "Off"                    },
	{  0x00,    0x01,    0x01,    0x00,       "On"                     },

	// Dip 1
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Coinage"                },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x07,    0x04,       "4 Coins 1 Play"         },
	{  0x01,    0x01,    0x07,    0x02,       "3 Coins 1 Play"         },
	{  0x01,    0x01,    0x07,    0x06,       "2 Coins 1 Play"         },
	{  0x01,    0x01,    0x07,    0x07,       "1 Coin  1 Play"         },
	{  0x01,    0x01,    0x07,    0x01,       "2 Coins 3 Plays"        },
	{  0x01,    0x01,    0x07,    0x03,       "1 Coin  2 Plays"        },
	{  0x01,    0x01,    0x07,    0x05,       "1 Coin  3 Plays"        },
	{  0x01,    0x01,    0x07,    0x00,       "Freeplay"               },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Bonus Life"             },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x38,    0x20,       "20k  60k  60k"          },
	{  0x01,    0x01,    0x38,    0x18,       "20k  60k"               },
	{  0x01,    0x01,    0x38,    0x10,       "20k  70k  70k"          },
	{  0x01,    0x01,    0x38,    0x30,       "20k  80k  80k"          },
	{  0x01,    0x01,    0x38,    0x38,       "30k  80k"               },
	{  0x01,    0x01,    0x38,    0x08,       "30k 100k 100k"          },
	{  0x01,    0x01,    0x38,    0x28,       "30k 120k 120k"          },
	{  0x01,    0x01,    0x38,    0x00,       "None"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Lives"                  },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0xc0,    0x00,       "2"                      },
	{  0x01,    0x01,    0xc0,    0x80,       "3"                      },
	{  0x01,    0x01,    0xc0,    0x40,       "4"                      },
	{  0x01,    0x01,    0xc0,    0xc0,       "5"                      },

	// Dip 2
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Difficulty"             },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x03,    0x03,       "Easy"                   },
	{  0x02,    0x01,    0x03,    0x00,       "Medium"                 },
	{  0x02,    0x01,    0x03,    0x01,       "Hard"                   },
	{  0x02,    0x01,    0x03,    0x02,       "Hardest"                },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Demo Sounds"            },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x08,    0x08,       "Off"                    },
	{  0x02,    0x01,    0x08,    0x00,       "On"                     },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Freeze"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x10,    0x10,       "Off"                    },
	{  0x02,    0x01,    0x10,    0x00,       "On"                     },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Rack Test"              },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x20,    0x20,       "Off"                    },
	{  0x02,    0x01,    0x20,    0x00,       "On"                     },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Cabinet"                },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x80,    0x80,       "Upright"                },
	{  0x02,    0x01,    0x80,    0x00,       "Cocktail"               },
};

STDDIPINFO(Galaga)

static struct BurnDIPInfo GalagamwDIPList[]=
{
	// Offset
	{  0x0b,    0xf0,    0x01,    0x01,       NULL                     },

	// Default Values
	// nOffset, nID,     nMask,   nDefault,   NULL
	{  0x00,    0xff,    0x01,    0x01,       NULL                     },
	{  0x01,    0xff,    0xff,    0x97,       NULL                     },
	{  0x02,    0xff,    0xff,    0xf7,       NULL                     },

	// Service Switch
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Service Mode"           },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x01,    0x01,       "Off"                    },
	{  0x00,    0x01,    0x01,    0x00,       "On"                     },

	// Dip 1
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Coinage"                },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x07,    0x04,       "4 Coins 1 Play"         },
	{  0x01,    0x01,    0x07,    0x02,       "3 Coins 1 Play"         },
	{  0x01,    0x01,    0x07,    0x06,       "2 Coins 1 Play"         },
	{  0x01,    0x01,    0x07,    0x07,       "1 Coin  1 Play"         },
	{  0x01,    0x01,    0x07,    0x01,       "2 Coins 3 Plays"        },
	{  0x01,    0x01,    0x07,    0x03,       "1 Coin  2 Plays"        },
	{  0x01,    0x01,    0x07,    0x05,       "1 Coin  3 Plays"        },
	{  0x01,    0x01,    0x07,    0x00,       "Freeplay"               },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Bonus Life"             },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x38,    0x20,       "20k  60k  60k"          },
	{  0x01,    0x01,    0x38,    0x18,       "20k  60k"               },
	{  0x01,    0x01,    0x38,    0x10,       "20k  70k  70k"          },
	{  0x01,    0x01,    0x38,    0x30,       "20k  80k  80k"          },
	{  0x01,    0x01,    0x38,    0x38,       "30k  80k"               },
	{  0x01,    0x01,    0x38,    0x08,       "30k 100k 100k"          },
	{  0x01,    0x01,    0x38,    0x28,       "30k 120k 120k"          },
	{  0x01,    0x01,    0x38,    0x00,       "None"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Lives"                  },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0xc0,    0x00,       "2"                      },
	{  0x01,    0x01,    0xc0,    0x80,       "3"                      },
	{  0x01,    0x01,    0xc0,    0x40,       "4"                      },
	{  0x01,    0x01,    0xc0,    0xc0,       "5"                      },

	// Dip 2
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "2 Credits Game"         },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x01,    0x00,       "1 Player"               },
	{  0x02,    0x01,    0x01,    0x01,       "2 Players"              },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Difficulty"             },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x06,    0x06,       "Easy"                   },
	{  0x02,    0x01,    0x06,    0x00,       "Medium"                 },
	{  0x02,    0x01,    0x06,    0x02,       "Hard"                   },
	{  0x02,    0x01,    0x06,    0x04,       "Hardest"                },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Demo Sounds"            },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x08,    0x08,       "Off"                    },
	{  0x02,    0x01,    0x08,    0x00,       "On"                     },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Freeze"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x10,    0x10,       "Off"                    },
	{  0x02,    0x01,    0x10,    0x00,       "On"                     },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Rack Test"              },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x20,    0x20,       "Off"                    },
	{  0x02,    0x01,    0x20,    0x00,       "On"                     },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Cabinet"                },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x02,    0x01,    0x80,    0x80,       "Upright"                },
	{  0x02,    0x01,    0x80,    0x00,       "Cocktail"               },
};

STDDIPINFO(Galagamw)

static struct BurnRomInfo GalagaRomDesc[] = {
	{ "gg1_1b.3p",     0x01000, 0xab036c9f, BRF_ESS | BRF_PRG   }, //  0	Z80 #1 Program Code
	{ "gg1_2b.3m",     0x01000, 0xd9232240, BRF_ESS | BRF_PRG   }, //	 1
	{ "gg1_3.2m",      0x01000, 0x753ce503, BRF_ESS | BRF_PRG   }, //	 2
	{ "gg1_4b.2l",     0x01000, 0x499fcc76, BRF_ESS | BRF_PRG   }, //	 3

	{ "gg1_5b.3f",     0x01000, 0xbb5caae3, BRF_ESS | BRF_PRG   }, //  4	Z80 #2 Program Code

	{ "gg1_7b.2c",     0x01000, 0xd016686b, BRF_ESS | BRF_PRG   }, //  5	Z80 #3 Program Code

	{ "gg1_9.4l",      0x01000, 0x58b2f47c, BRF_GRA             },	//  6	Characters

	{ "gg1_11.4d",     0x01000, 0xad447c80, BRF_GRA             },	//  7	Sprites
	{ "gg1_10.4f",     0x01000, 0xdd6f1afc, BRF_GRA             },	//  8

	{ "prom-5.5n",     0x00020, 0x54603c6b, BRF_GRA             },	//  9	PROMs
	{ "prom-4.2n",     0x00100, 0x59b6edab, BRF_GRA             },	// 10
	{ "prom-3.1c",     0x00100, 0x4a04bb6b, BRF_GRA             },	// 11
	{ "prom-1.1d",     0x00100, 0x7a2815b4, BRF_GRA             },	// 12
	{ "prom-2.5c",     0x00100, 0x77245b66, BRF_GRA             },	// 13
};

STD_ROM_PICK(Galaga)
STD_ROM_FN(Galaga)

static struct BurnRomInfo GalagaoRomDesc[] = {
	{ "gg1-1.3p",      0x01000, 0xa3a0f743, BRF_ESS | BRF_PRG   }, //  0	Z80 #1 Program Code
	{ "gg1-2.3m",      0x01000, 0x43bb0d5c, BRF_ESS | BRF_PRG   }, //	 1
	{ "gg1-3.2m",      0x01000, 0x753ce503, BRF_ESS | BRF_PRG   }, //	 2
	{ "gg1-4.2l",      0x01000, 0x83874442, BRF_ESS | BRF_PRG   }, //	 3

	{ "gg1-5.3f",      0x01000, 0x3102fccd, BRF_ESS | BRF_PRG   }, //  4	Z80 #2 Program Code

	{ "gg1-7.2c",      0x01000, 0x8995088d, BRF_ESS | BRF_PRG   }, //  5	Z80 #3 Program Code

	{ "gg1-9.4l",      0x01000, 0x58b2f47c, BRF_GRA             },	//  6	Characters

	{ "gg1-11.4d",     0x01000, 0xad447c80, BRF_GRA             },	//  7	Sprites
	{ "gg1-10.4f",     0x01000, 0xdd6f1afc, BRF_GRA             }, //  8

	{ "prom-5.5n",     0x00020, 0x54603c6b, BRF_GRA             },	//  9	PROMs
	{ "prom-4.2n",     0x00100, 0x59b6edab, BRF_GRA             },	// 10
	{ "prom-3.1c",     0x00100, 0x4a04bb6b, BRF_GRA             },	// 11
	{ "prom-1.1d",     0x00100, 0x7a2815b4, BRF_GRA             },	// 12
	{ "prom-2.5c",     0x00100, 0x77245b66, BRF_GRA             },	// 13
};

STD_ROM_PICK(Galagao)
STD_ROM_FN(Galagao)

static struct BurnRomInfo GalagamwRomDesc[] = {
	{ "3200a.bin",     0x01000, 0x3ef0b053, BRF_ESS | BRF_PRG   }, //  0	Z80 #1 Program Code
	{ "3300b.bin",     0x01000, 0x1b280831, BRF_ESS | BRF_PRG   }, //	 1
	{ "3400c.bin",     0x01000, 0x16233d33, BRF_ESS | BRF_PRG   }, //	 2
	{ "3500d.bin",     0x01000, 0x0aaf5c23, BRF_ESS | BRF_PRG   }, //	 3

	{ "3600e.bin",     0x01000, 0xbc556e76, BRF_ESS | BRF_PRG   }, //  4	Z80 #2 Program Code

	{ "3700g.bin",     0x01000, 0xb07f0aa4, BRF_ESS | BRF_PRG   }, //  5	Z80 #3 Program Code

	{ "2600j.bin",     0x01000, 0x58b2f47c, BRF_GRA             },	//  6	Characters

	{ "2800l.bin",     0x01000, 0xad447c80, BRF_GRA             },	//  7	Sprites
	{ "2700k.bin",     0x01000, 0xdd6f1afc, BRF_GRA             },	//  8

	{ "prom-5.5n",     0x00020, 0x54603c6b, BRF_GRA             },	//  9	PROMs
	{ "prom-4.2n",     0x00100, 0x59b6edab, BRF_GRA             },	// 10
	{ "prom-3.1c",     0x00100, 0x4a04bb6b, BRF_GRA             },	// 11
	{ "prom-1.1d",     0x00100, 0x7a2815b4, BRF_GRA             },	// 12
	{ "prom-2.5c",     0x00100, 0x77245b66, BRF_GRA             },	// 13
};

STD_ROM_PICK(Galagamw)
STD_ROM_FN(Galagamw)

static struct BurnRomInfo GalagamfRomDesc[] = {
	{ "3200a.bin",     0x01000, 0x3ef0b053, BRF_ESS | BRF_PRG   }, //  0	Z80 #1 Program Code
	{ "3300b.bin",     0x01000, 0x1b280831, BRF_ESS | BRF_PRG   }, //	 1
	{ "3400c.bin",     0x01000, 0x16233d33, BRF_ESS | BRF_PRG   }, //	 2
	{ "3500d.bin",     0x01000, 0x0aaf5c23, BRF_ESS | BRF_PRG   }, //	 3

	{ "3600fast.bin",  0x01000, 0x23d586e5, BRF_ESS | BRF_PRG   }, //  4	Z80 #2 Program Code

	{ "3700g.bin",     0x01000, 0xb07f0aa4, BRF_ESS | BRF_PRG   }, //  5	Z80 #3 Program Code

	{ "2600j.bin",     0x01000, 0x58b2f47c, BRF_GRA             },	//  6	Characters

	{ "2800l.bin",     0x01000, 0xad447c80, BRF_GRA             },	//  7	Sprites
	{ "2700k.bin",     0x01000, 0xdd6f1afc, BRF_GRA             },	//  8

	{ "prom-5.5n",     0x00020, 0x54603c6b, BRF_GRA             },	//  9	PROMs
	{ "prom-4.2n",     0x00100, 0x59b6edab, BRF_GRA             },	// 10
	{ "prom-3.1c",     0x00100, 0x4a04bb6b, BRF_GRA             },	// 11
	{ "prom-1.1d",     0x00100, 0x7a2815b4, BRF_GRA             },	// 12
	{ "prom-2.5c",     0x00100, 0x77245b66, BRF_GRA             },	// 13
};

STD_ROM_PICK(Galagamf)
STD_ROM_FN(Galagamf)

static struct BurnRomInfo GalagamkRomDesc[] = {
	{ "mk2-1",         0x01000, 0x23cea1e2, BRF_ESS | BRF_PRG   }, //  0	Z80 #1 Program Code
	{ "mk2-2",         0x01000, 0x89695b1a, BRF_ESS | BRF_PRG   }, //	 1
	{ "3400c.bin",     0x01000, 0x16233d33, BRF_ESS | BRF_PRG   }, //	 2
	{ "mk2-4",         0x01000, 0x24b767f5, BRF_ESS | BRF_PRG   }, //	 3

	{ "gg1-5.3f",      0x01000, 0x3102fccd, BRF_ESS | BRF_PRG   }, //  4	Z80 #2 Program Code

	{ "gg1-7b.2c",     0x01000, 0xd016686b, BRF_ESS | BRF_PRG   }, //  5	Z80 #3 Program Code

	{ "gg1-9.4l",      0x01000, 0x58b2f47c, BRF_GRA             },	//  6	Characters

	{ "gg1-11.4d",     0x01000, 0xad447c80, BRF_GRA             },	//  7	Sprites
	{ "gg1-10.4f",     0x01000, 0xdd6f1afc, BRF_GRA             },	//  8

	{ "prom-5.5n",     0x00020, 0x54603c6b, BRF_GRA             },	//  9	PROMs
	{ "prom-4.2n",     0x00100, 0x59b6edab, BRF_GRA             },	// 10
	{ "prom-3.1c",     0x00100, 0x4a04bb6b, BRF_GRA             },	// 11
	{ "prom-1.1d",     0x00100, 0x7a2815b4, BRF_GRA             },	// 12
	{ "prom-2.5c",     0x00100, 0x77245b66, BRF_GRA             },	// 13
};

STD_ROM_PICK(Galagamk)
STD_ROM_FN(Galagamk)

static struct BurnRomInfo GallagRomDesc[] = {
	{ "gallag.1",      0x01000, 0xa3a0f743, BRF_ESS | BRF_PRG   }, //  0	Z80 #1 Program Code
	{ "gallag.2",      0x01000, 0x5eda60a7, BRF_ESS | BRF_PRG   }, //	 1
	{ "gallag.3",      0x01000, 0x753ce503, BRF_ESS | BRF_PRG   }, //	 2
	{ "gallag.4",      0x01000, 0x83874442, BRF_ESS | BRF_PRG   }, //	 3

	{ "gallag.5",      0x01000, 0x3102fccd, BRF_ESS | BRF_PRG   }, //  4	Z80 #2 Program Code

	{ "gallag.7",      0x01000, 0x8995088d, BRF_ESS | BRF_PRG   }, //  5	Z80 #3 Program Code

	{ "gallag.6",      0x01000, 0x001b70bc, BRF_ESS | BRF_PRG   }, //  6	Z80 #4 Program Code

	{ "gallag.8",      0x01000, 0x169a98a4, BRF_GRA             },	//  7	Characters

	{ "gallag.a",      0x01000, 0xad447c80, BRF_GRA             },	//  8	Sprites
	{ "gallag.9",      0x01000, 0xdd6f1afc, BRF_GRA             },	//  9

	{ "prom-5.5n",     0x00020, 0x54603c6b, BRF_GRA             },	// 10	PROMs
	{ "prom-4.2n",     0x00100, 0x59b6edab, BRF_GRA             },	// 11
	{ "prom-3.1c",     0x00100, 0x4a04bb6b, BRF_GRA             },	// 12
	{ "prom-1.1d",     0x00100, 0x7a2815b4, BRF_GRA             },	// 13
	{ "prom-2.5c",     0x00100, 0x77245b66, BRF_GRA             },	// 14
};

STD_ROM_PICK(Gallag)
STD_ROM_FN(Gallag)

static struct BurnRomInfo NebulbeeRomDesc[] = {
	{ "nebulbee.01",   0x01000, 0xf405f2c4, BRF_ESS | BRF_PRG   }, //  0	Z80 #1 Program Code
	{ "nebulbee.02",   0x01000, 0x31022b60, BRF_ESS | BRF_PRG   }, //	 1
	{ "gg1_3.2m",      0x01000, 0x753ce503, BRF_ESS | BRF_PRG   }, //	 2
	{ "nebulbee.04",   0x01000, 0xd76788a5, BRF_ESS | BRF_PRG   }, //	 3

	{ "gg1-5",         0x01000, 0x3102fccd, BRF_ESS | BRF_PRG   }, //  4	Z80 #2 Program Code

	{ "gg1-7",         0x01000, 0x8995088d, BRF_ESS | BRF_PRG   }, //  5	Z80 #3 Program Code

	{ "nebulbee.07",   0x01000, 0x035e300c, BRF_ESS | BRF_PRG   }, //  6	Z80 #4 Program Code

	{ "gg1_9.4l",      0x01000, 0x58b2f47c, BRF_GRA             },	//  7	Characters

	{ "gg1_11.4d",     0x01000, 0xad447c80, BRF_GRA             },	//  8	Sprites
	{ "gg1_10.4f",     0x01000, 0xdd6f1afc, BRF_GRA             },	//  9

	{ "prom-5.5n",     0x00020, 0x54603c6b, BRF_GRA             },	// 10	PROMs
	{ "2n.bin",        0x00100, 0xa547d33b, BRF_GRA             },	// 11
	{ "1c.bin",        0x00100, 0xb6f585fb, BRF_GRA             },	// 12
	{ "1d.bin",        0x00100, 0x86d92b24, BRF_GRA             },	// 14
	{ "5c.bin",        0x00100, 0x8bd565f6, BRF_GRA             },	// 13
};

STD_ROM_PICK(Nebulbee)
STD_ROM_FN(Nebulbee)

static struct BurnRomInfo GalagawmRomDesc[] = {
	{ "wmgg1_1b.3p",   0x01000, 0xd7dffd9c, BRF_ESS | BRF_PRG   }, //  0	Z80 #1 Program Code
	{ "wmgg1_2b.3m",   0x01000, 0xab7cbd28, BRF_ESS | BRF_PRG   }, //	 1
	{ "wmgg1_3.2m",    0x01000, 0x75bcd999, BRF_ESS | BRF_PRG   }, //	 2
	{ "wmgg1_4b.2l",   0x01000, 0x114f2ae5, BRF_ESS | BRF_PRG   }, //	 3

	{ "gg1_5b.3f",     0x01000, 0xbb5caae3, BRF_ESS | BRF_PRG   }, //  4	Z80 #2 Program Code

	{ "gg1_7b.2c",     0x01000, 0xd016686b, BRF_ESS | BRF_PRG   }, //  5	Z80 #3 Program Code

	{ "gg1_9.4l",      0x01000, 0x58b2f47c, BRF_GRA             },	//  6	Characters

	{ "gg1_11.4d",     0x01000, 0xad447c80, BRF_GRA             },	//  7	Sprites
	{ "gg1_10.4f",     0x01000, 0xdd6f1afc, BRF_GRA             },	//  8

	{ "prom-5.5n",     0x00020, 0x54603c6b, BRF_GRA             },	//  9	PROMs
	{ "prom-4.2n",     0x00100, 0x59b6edab, BRF_GRA             },	// 10
	{ "prom-3.1c",     0x00100, 0x4a04bb6b, BRF_GRA             },	// 11
	{ "prom-1.1d",     0x00100, 0x7a2815b4, BRF_GRA             },	// 12
	{ "prom-2.5c",     0x00100, 0x77245b66, BRF_GRA             },	// 13
};

STD_ROM_PICK(Galagawm)
STD_ROM_FN(Galagawm)

static struct BurnSampleInfo GalagaSampleDesc[] = {
#if !defined (ROM_VERIFY)
	{ "bang", SAMPLE_NOLOOP },
	{ "init", SAMPLE_NOLOOP },
#endif
	{ "", 0 }
};

STD_SAMPLE_PICK(Galaga)
STD_SAMPLE_FN(Galaga)

static INT32 galagaInit(void);
static INT32 gallagInit(void);
static INT32 galagaReset(void);

static void galagaMemoryMap1(void);
static void galagaMemoryMap2(void);
static void galagaMemoryMap3(void);
static tilemap_callback(galaga_fg);
static INT32 galagaCharDecode(void);
static INT32 galagaSpriteDecode(void);
static INT32 galagaTilemapConfig(void);

static void galagaZ80WriteStars(UINT16 offset, UINT8 dta);

static void galagaCalcPalette(void);
static void galagaInitStars(void);
static void galagaRenderStars(void);
static void galagaStarScroll(void);
static void galagaRenderChars(void);

static UINT32 galagaGetSpriteParams(struct Namco_Sprite_Params *spriteParams, UINT32 offset);

static INT32 galagaScan(INT32 nAction, INT32 *pnMin);

#define GALAGA_PALETTE_SIZE_CHARS         0x100
#define GALAGA_PALETTE_SIZE_SPRITES       0x100
#define GALAGA_PALETTE_SIZE_BGSTARS       0x040
#define GALAGA_PALETTE_SIZE (GALAGA_PALETTE_SIZE_CHARS + \
	GALAGA_PALETTE_SIZE_SPRITES + \
	GALAGA_PALETTE_SIZE_BGSTARS)

#define GALAGA_PALETTE_OFFSET_CHARS       0
#define GALAGA_PALETTE_OFFSET_SPRITE      (GALAGA_PALETTE_OFFSET_CHARS + GALAGA_PALETTE_SIZE_CHARS)
#define GALAGA_PALETTE_OFFSET_BGSTARS     (GALAGA_PALETTE_OFFSET_SPRITE + GALAGA_PALETTE_SIZE_SPRITES)

#define GALAGA_NUM_OF_CHAR                0x100
#define GALAGA_SIZE_OF_CHAR_IN_BYTES      0x80

#define GALAGA_NUM_OF_SPRITE              0x80
#define GALAGA_SIZE_OF_SPRITE_IN_BYTES    0x200

#define STARS_CTRL_NUM     6

struct Stars_Def
{
	UINT32 scrollX;
	UINT32 scrollY;
	UINT8 control[STARS_CTRL_NUM];
};

static struct Stars_Def stars = { 0 };

struct Star_Def
{
	UINT16 x;
	UINT16 y;
	UINT8 colour;
	UINT8 set;
};

#define MAX_STARS 252
static struct Star_Def *starSeedTable;

static struct CPU_Config_Def galagaCPU[NAMCO_BRD_CPU_COUNT] =
{
	{
		/* CPU ID = */          CPU1,
		/* CPU Read Func = */   namcoZ80ProgRead,
		/* CPU Write Func = */  namcoZ80ProgWrite,
		/* Memory Mapping = */  galagaMemoryMap1
	},
	{
		/* CPU ID = */          CPU2,
		/* CPU Read Func = */   namcoZ80ProgRead,
		/* CPU Write Func = */  namcoZ80ProgWrite,
		/* Memory Mapping = */  galagaMemoryMap2
	},
	{
		/* CPU ID = */          CPU3,
		/* CPU Read Func = */   namcoZ80ProgRead,
		/* CPU Write Func = */  namcoZ80ProgWrite,
		/* Memory Mapping = */  galagaMemoryMap3
	},
};

static struct CPU_Rd_Table galagaReadTable[] =
{
	{ 0x6800, 0x6807, namcoZ80ReadDip         },
	{ 0x7000, 0x7002, namcoCustomICsReadDta   },
	{ 0x7100, 0x7100, namcoCustomICsReadCmd   },
	{ 0x0000, 0x0000, NULL                    }, // End of Table marker
};

static struct CPU_Wr_Table galagaWriteTable[] =
{
	{ 0x6800, 0x681f, namcoZ80WriteSound      },
	{ 0x6820, 0x6820, namcoZ80WriteCPU1Irq    },
	{ 0x6821, 0x6821, namcoZ80WriteCPU2Irq    },
	{ 0x6822, 0x6822, namcoZ80WriteCPU3Irq    },
	{ 0x6823, 0x6823, namcoZ80WriteCPUReset   },
	//	{ 0x6830, 0x6830, WatchDogWriteNotImplemented },
	{ 0x7000, 0x700f, namcoCustomICsWriteDta  },
	{ 0x7100, 0x7100, namcoCustomICsWriteCmd  },
	{ 0xa000, 0xa005, galagaZ80WriteStars     },
	{ 0xa007, 0xa007, namcoZ80WriteFlipScreen },
	{ 0x0000, 0x0000, NULL                    }, // End of Table marker
};

static struct Memory_Map_Def galagaMemTable[] =
{
	{  &memory.Z80.rom1,           0x04000,                               MEM_PGM        },
	{  &memory.Z80.rom2,           0x04000,                               MEM_PGM        },
	{  &memory.Z80.rom3,           0x04000,                               MEM_PGM        },
	{  &memory.PROM.palette,       0x00020,                               MEM_ROM        },
	{  &memory.PROM.charLookup,    0x00100,                               MEM_ROM        },
	{  &memory.PROM.spriteLookup,  0x00100,                               MEM_ROM        },
	{  &NamcoSoundProm,            0x00200,                               MEM_ROM        },

	{  &memory.RAM.video,          0x00800,                               MEM_RAM        },
	{  &memory.RAM.shared1,        0x00400,                               MEM_RAM        },
	{  &memory.RAM.shared2,        0x00400,                               MEM_RAM        },
	{  &memory.RAM.shared3,        0x00400,                               MEM_RAM        },

	{  (UINT8 **)&starSeedTable,   sizeof(struct Star_Def) * MAX_STARS,   MEM_DATA       },
	{  &graphics.fgChars,          GALAGA_NUM_OF_CHAR * 8 * 8,            MEM_DATA       },
	{  &graphics.sprites,          GALAGA_NUM_OF_SPRITE * 16 * 16,        MEM_DATA       },
	{  (UINT8 **)&graphics.palette, GALAGA_PALETTE_SIZE * sizeof(UINT32),  MEM_DATA32     },
};

#define GALAGA_MEM_TBL_SIZE      (sizeof(galagaMemTable) / sizeof(struct Memory_Map_Def))

static struct ROM_Load_Def galagaROMTable[] =
{
	{  &memory.Z80.rom1,             0x00000,    NULL                          },
	{  &memory.Z80.rom1,             0x01000,    NULL                          },
	{  &memory.Z80.rom1,             0x02000,    NULL                          },
	{  &memory.Z80.rom1,             0x03000,    NULL                          },
	{  &memory.Z80.rom2,             0x00000,    NULL                          },
	{  &memory.Z80.rom3,             0x00000,    NULL                          },
	{  &tempRom,                     0x00000,    galagaCharDecode              },
	{  &tempRom,                     0x00000,    NULL                          },
	{  &tempRom,                     0x01000,    galagaSpriteDecode            },
	{  &memory.PROM.palette,         0x00000,    NULL                          },
	{  &memory.PROM.charLookup,      0x00000,    NULL                          },
	{  &memory.PROM.spriteLookup,    0x00000,    NULL                          },
	{  &NamcoSoundProm,              0x00000,    namcoMachineInit              }

};

#define GALAGA_ROM_TBL_SIZE      (sizeof(galagaROMTable) / sizeof(struct ROM_Load_Def))

static struct ROM_Load_Def gallagROMTable[] =
{
	{  &memory.Z80.rom1,             0x00000,    NULL                          },
	{  &memory.Z80.rom1,             0x01000,    NULL                          },
	{  &memory.Z80.rom1,             0x02000,    NULL                          },
	{  &memory.Z80.rom1,             0x03000,    NULL                          },
	{  &memory.Z80.rom2,             0x00000,    NULL                          },
	{  &memory.Z80.rom3,             0x00000,    NULL                          },
	{  &memory.Z80.rom4,             0x00000,    NULL                          },
	{  &tempRom,                     0x00000,    galagaCharDecode              },
	{  &tempRom,                     0x00000,    NULL                          },
	{  &tempRom,                     0x01000,    galagaSpriteDecode            },
	{  &memory.PROM.palette,         0x00000,    NULL                          },
	{  &memory.PROM.charLookup,      0x00000,    NULL                          },
	{  &memory.PROM.spriteLookup,    0x00000,    NULL                          },
	{  &NamcoSoundProm,              0x00000,    namcoMachineInit              },
};

#define GALLAG_ROM_TBL_SIZE      (sizeof(gallagROMTable) / sizeof(struct ROM_Load_Def))

static DrawFunc_t galagaDrawFuncs[] =
{
	galagaCalcPalette,
	galagaRenderChars,
	galagaRenderStars,
	namcoRenderSprites,
	galagaStarScroll,
};

#define GALAGA_DRAW_TBL_SIZE  (sizeof(galagaDrawFuncs) / sizeof(galagaDrawFuncs[0]))

static struct Namco_Custom_RW_Entry galagaCustomICRW[] =
{
	{  0x71,    namco51xxRead     },
	{  0xa1,    namco51xxWrite    },
	{  0xb1,    namco51xxRead     },
	{  0xe1,    namco51xxWrite    },
	{  0xA8,    namco54xxWrite    },
	{  0x00,    NULL              }
};

static struct N54XX_Sample_Info_Def galagaN54xxSampleList[] =
{
	{  0,    "\x40\x00\x02\xdf"   },
	{  1,    "\x30\x30\x03\xdf"   },
	{  -1,   ""                   },
};

static struct Machine_Config_Def galagaMachineConfig =
{
	/*cpus                   */ galagaCPU,
	/*wrAddrList             */ galagaWriteTable,
	/*rdAddrList             */ galagaReadTable,
	/*memMapTable            */ galagaMemTable,
	/*sizeOfMemMapTable      */ GALAGA_MEM_TBL_SIZE,
	/*romLayoutTable         */ galagaROMTable,
	/*sizeOfRomLayoutTable   */ GALAGA_ROM_TBL_SIZE,
	/*tempRomSize            */ 0x2000,
	/*tilemapsConfig         */ galagaTilemapConfig,
	/*drawLayerTable         */ galagaDrawFuncs,
	/*drawTableSize          */ GALAGA_DRAW_TBL_SIZE,
	/*getSpriteParams        */ galagaGetSpriteParams,
	/*reset                  */ galagaReset,
	/*customRWTable          */ galagaCustomICRW,
	/*n54xxSampleList        */ galagaN54xxSampleList
};

static INT32 galagaInit(void)
{
	machine.game = NAMCO_GALAGA;
	machine.numOfDips = GALAGA_NUM_OF_DIPSWITCHES;

	machine.config = &galagaMachineConfig;

	return namcoInitBoard();
}

static struct Machine_Config_Def gallagMachineConfig =
{
	/*cpus                   */ galagaCPU,
	/*wrAddrList             */ galagaWriteTable,
	/*rdAddrList             */ galagaReadTable,
	/*memMapTable            */ galagaMemTable,
	/*sizeOfMemMapTable      */ GALAGA_MEM_TBL_SIZE,
	/*romLayoutTable         */ gallagROMTable,
	/*sizeOfRomLayoutTable   */ GALLAG_ROM_TBL_SIZE,
	/*tempRomSize            */ 0x2000,
	/*tilemapsConfig         */ galagaTilemapConfig,
	/*drawLayerTable         */ galagaDrawFuncs,
	/*drawTableSize          */ GALAGA_DRAW_TBL_SIZE,
	/*getSpriteParams        */ galagaGetSpriteParams,
	/*reset                  */ galagaReset,
	/*customRWTable          */ galagaCustomICRW,
	/*n54xxSampleList        */ galagaN54xxSampleList
};

static INT32 gallagInit(void)
{
	machine.game = NAMCO_GALAGA;
	machine.numOfDips = GALAGA_NUM_OF_DIPSWITCHES;

	machine.config = &gallagMachineConfig;

	return namcoInitBoard();
}

static INT32 galagaReset(void)
{
	for (INT32 i = 0; i < STARS_CTRL_NUM; i ++)
	{
		stars.control[i] = 0;
	}
	stars.scrollX = 0;
	stars.scrollY = 0;

	return DrvDoReset();
}

static void galagaMemoryMap1(void)
{
	ZetMapMemory(memory.Z80.rom1,    0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(memory.RAM.video,   0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared1, 0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared2, 0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared3, 0x9800, 0x9bff, MAP_RAM);
}

static void galagaMemoryMap2(void)
{
	ZetMapMemory(memory.Z80.rom2,    0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(memory.RAM.video,   0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared1, 0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared2, 0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared3, 0x9800, 0x9bff, MAP_RAM);
}

static void galagaMemoryMap3(void)
{
	ZetMapMemory(memory.Z80.rom3,    0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(memory.RAM.video,   0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared1, 0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared2, 0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared3, 0x9800, 0x9bff, MAP_RAM);
}

static tilemap_callback ( galaga_fg )
{
	INT32 code   = memory.RAM.video[offs + 0x000] & 0x7f;
	INT32 colour = memory.RAM.video[offs + 0x400] & 0x3f;

	TILE_SET_INFO(0, code, colour, 0);
}

static INT32 galagaCharDecode(void)
{
	GfxDecode(
			  GALAGA_NUM_OF_CHAR,
			  NAMCO_2BIT_PALETTE_BITS,
			  8, 8,
			  (INT32*)planeOffsets2Bit,
			  (INT32*)xOffsets8x8Tiles2Bit,
			  (INT32*)yOffsets8x8Tiles2Bit,
			  GALAGA_SIZE_OF_CHAR_IN_BYTES,
			  tempRom,
			  graphics.fgChars
			 );

	return 0;
}

static INT32 galagaSpriteDecode(void)
{
	GfxDecode(
			  GALAGA_NUM_OF_SPRITE,
			  NAMCO_2BIT_PALETTE_BITS,
			  16, 16,
			  (INT32*)planeOffsets2Bit,
			  (INT32*)xOffsets16x16Tiles2Bit,
			  (INT32*)yOffsets16x16Tiles2Bit,
			  GALAGA_SIZE_OF_SPRITE_IN_BYTES,
			  tempRom,
			  graphics.sprites
			 );

	return 0;
}

static INT32 galagaTilemapConfig(void)
{
	GenericTilemapInit(
					   0, //TILEMAP_FG,
					   namco_map_scan,
					   galaga_fg_map_callback,
					   8, 8,
					   NAMCO_TMAP_WIDTH, NAMCO_TMAP_HEIGHT
					  );

	GenericTilemapSetGfx(
						 0, //TILEMAP_FG,
						 graphics.fgChars,
						 NAMCO_2BIT_PALETTE_BITS,
						 8, 8,
						 (GALAGA_NUM_OF_CHAR * 8 * 8),
						 0x0,
						 (GALAGA_PALETTE_SIZE_CHARS - 1)
						);

	GenericTilemapSetTransparent(0, 0);

	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, 0);

	return 0;
}

static void galagaZ80WriteStars(UINT16 offset, UINT8 dta)
{
	stars.control[offset] = dta & 0x01;
}

#define GALAGA_3BIT_PALETTE_SIZE    32
#define GALAGA_2BIT_PALETTE_SIZE    64

static void galagaCalcPalette(void)
{
	UINT32 palette3Bit[GALAGA_3BIT_PALETTE_SIZE];

	for (INT32 i = 0; i < GALAGA_3BIT_PALETTE_SIZE; i ++)
	{
		INT32 r = Colour3Bit[(memory.PROM.palette[i] >> 0) & 0x07];
		INT32 g = Colour3Bit[(memory.PROM.palette[i] >> 3) & 0x07];
		INT32 b = Colour3Bit[(memory.PROM.palette[i] >> 5) & 0x06];

		palette3Bit[i] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 i = 0; i < GALAGA_PALETTE_SIZE_CHARS; i ++)
	{
		graphics.palette[GALAGA_PALETTE_OFFSET_CHARS + i] =
			palette3Bit[((memory.PROM.charLookup[i]) & 0x0f) + 0x10];
	}

	for (INT32 i = 0; i < GALAGA_PALETTE_SIZE_SPRITES; i ++)
	{
		graphics.palette[GALAGA_PALETTE_OFFSET_SPRITE + i] =
			palette3Bit[memory.PROM.spriteLookup[i] & 0x0f];
	}

	UINT32 palette2Bit[GALAGA_2BIT_PALETTE_SIZE];

	for (INT32 i = 0; i < GALAGA_2BIT_PALETTE_SIZE; i ++)
	{
		INT32 r = Colour2Bit[(i >> 0) & 0x03];
		INT32 g = Colour2Bit[(i >> 2) & 0x03];
		INT32 b = Colour2Bit[(i >> 4) & 0x03];

		palette2Bit[i] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 i = 0; i < GALAGA_PALETTE_SIZE_BGSTARS; i ++)
	{
		graphics.palette[GALAGA_PALETTE_OFFSET_BGSTARS + i] =
			palette2Bit[i];
	}

	galagaInitStars();
}

static void galagaInitStars(void)
{
	/*
	 Galaga star line and pixel locations pulled directly from
	 a clocked stepping of the 05 starfield. The chip was clocked
	 on a test rig with hblank and vblank simulated, each X & Y
	 location of a star being recorded along with it's color value.

	 The lookup table is generated using a reverse engineered
	 linear feedback shift register + XOR boolean expression.

	 Because the starfield begins generating stars at the point
	 in time it's enabled the exact horiz location of the stars
	 on Galaga depends on the length of time of the POST for the
	 original board.

	 Two control bits determine which of two sets are displayed
	 set 0 or 1 and simultaneously 2 or 3.

	 There are 63 stars in each set, 126 displayed at any one time
	 Code: jmakovicka, based on info from http://www.pin4.at/pro_custom_05xx.php
	 */

	if (machine.starsInitted) return;
	machine.starsInitted = 1;
	bprintf(0, _T("init stars!\n"));

	const UINT16 feed = 0x9420;

	INT32 idx = 0;
	for (UINT32 sf = 0; sf < 4; ++ sf)
	{
		// starfield select flags
		UINT16 sf1 = (sf >> 1) & 1;
		UINT16 sf2 = sf & 1;

		UINT16 i = 0x70cc;
		for (UINT32 cnt = 0; cnt < 65535; ++ cnt)
		{
			// output enable lookup
			UINT16 xor1 = i    ^ (i >> 3);
			UINT16 xor2 = xor1 ^ (i >> 2);
			UINT16 oe = (sf1 ? 0 : 0x4000) | ((sf1 ^ sf2) ? 0 : 0x1000);
			if ( ( 0x8007             == ( i   & 0x8007) ) &&
				( 0x2008             == (~i   & 0x2008) ) &&
				( (sf1 ? 0 : 0x0100) == (xor1 & 0x0100) ) &&
				( (sf2 ? 0 : 0x0040) == (xor2 & 0x0040) ) &&
				( oe                 == (i    & 0x5000) ) &&
				( cnt                >= (256 * 4)       ) )
			{
				// color lookup
				UINT16 xor3 = (i >> 1) ^ (i >> 6);
				UINT16 clr  = (
							   (          (i >> 9)             & 0x07)
							   | ( ( xor3 ^ (i >> 4) ^ (i >> 7)) & 0x08)
							   |   (~xor3                        & 0x10)
							   | (        ( (i >> 2) ^ (i >> 5)) & 0x20) )
					^ ( (i & 0x4000) ? 0 : 0x24)
					^ ( ( ((i >> 2) ^ i) & 0x1000) ? 0x21 : 0);

				starSeedTable[idx].x = cnt % 256;
				starSeedTable[idx].y = cnt / 256;
				starSeedTable[idx].colour = clr;
				starSeedTable[idx].set = sf;
				++ idx;
			}

			// update the LFSR
			if (i & 1) i = (i >> 1) ^ feed;
			else       i = (i >> 1);
		}
	}
}

static void galagaRenderStars(void)
{
	if (1 == stars.control[5])
	{
		INT32 setA = stars.control[3];
		INT32 setB = stars.control[4] | 0x02;

		for (INT32 starCounter = 0; starCounter < MAX_STARS; starCounter ++)
		{
			if ( (setA == starSeedTable[starCounter].set) ||
				(setB == starSeedTable[starCounter].set) )
			{
				INT32 x = (                      starSeedTable[starCounter].x + stars.scrollX) % 256 + 16;
				INT32 y = ((nScreenHeight / 2) + starSeedTable[starCounter].y + stars.scrollY) % 256;

				if ( (x >= 0) && (x < nScreenWidth)  &&
					(y >= 0) && (y < nScreenHeight) )
				{
					pTransDraw[(y * nScreenWidth) + x] = starSeedTable[starCounter].colour + GALAGA_PALETTE_OFFSET_BGSTARS;
				}
			}

		}
	}
}

static void galagaStarScroll(void)
{
	static const INT32 speeds[8] = { -1, -2, -3, 0, 3, 2, 1, 0 };

	stars.scrollX += speeds[stars.control[0] + (stars.control[1] * 2) + (stars.control[2] * 4)];
}

static void galagaRenderChars(void)
{
	GenericTilemapSetScrollX(0, 0);
	GenericTilemapSetScrollY(0, 0);
	GenericTilemapSetEnable(0, 1);
	GenericTilemapDraw(0, pTransDraw, 0 | TMAP_TRANSPARENT);
}

static UINT32 galagaGetSpriteParams(struct Namco_Sprite_Params *spriteParams, UINT32 offset)
{
	UINT8 *spriteRam1 = memory.RAM.shared1 + 0x380;
	UINT8 *spriteRam2 = memory.RAM.shared2 + 0x380;
	UINT8 *spriteRam3 = memory.RAM.shared3 + 0x380;

	spriteParams->sprite =    spriteRam1[offset + 0] & 0x7f;
	spriteParams->colour =    spriteRam1[offset + 1] & 0x3f;

	spriteParams->xStart =    spriteRam2[offset + 1] - 40 + (0x100 * (spriteRam3[offset + 1] & 0x03));
	spriteParams->yStart =    NAMCO_SCREEN_WIDTH - spriteRam2[offset + 0] + 1;
	spriteParams->xStep =     16;
	spriteParams->yStep =     16;

	spriteParams->flags =     spriteRam3[offset + 0] & 0x0f;
	spriteParams->transparent_mask = 0x0f;

	if (spriteParams->flags & ySize)
	{
		if (spriteParams->flags & yFlip)
		{
			spriteParams->yStep = -16;
		}
		else
		{
			spriteParams->yStart -= 16;
		}
	}

	if (spriteParams->flags & xSize)
	{
		if (spriteParams->flags & xFlip)
		{
			spriteParams->xStart += 16;
			spriteParams->xStep  = -16;
		}
	}

	spriteParams->paletteBits   = NAMCO_2BIT_PALETTE_BITS;
	spriteParams->paletteOffset = GALAGA_PALETTE_OFFSET_SPRITE;

	return 1;
}

static INT32 galagaScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(stars.scrollX);
		SCAN_VAR(stars.scrollY);
		SCAN_VAR(stars.control);
	}

	return DrvScan(nAction, pnMin);
}

struct BurnDriver BurnDrvGalaga = {
	"galaga", NULL, NULL, "galaga", "1981",
	"Galaga (Namco rev. B)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, GalagaRomInfo, GalagaRomName, NULL, NULL, GalagaSampleInfo, GalagaSampleName, GalagaInputInfo, GalagaDIPInfo,
	galagaInit, DrvExit, DrvFrame, DrvDraw, galagaScan, NULL,
	GALAGA_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvGalagao = {
	"galagao", "galaga", NULL, "galaga", "1981",
	"Galaga (Namco)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, GalagaoRomInfo, GalagaoRomName, NULL, NULL, GalagaSampleInfo, GalagaSampleName, GalagaInputInfo, GalagaDIPInfo,
	galagaInit, DrvExit, DrvFrame, DrvDraw, galagaScan, NULL,
	GALAGA_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvGalagamw = {
	"galagamw", "galaga", NULL, "galaga", "1981",
	"Galaga (Midway set 1)\0", NULL, "Namco (Midway license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, GalagamwRomInfo, GalagamwRomName, NULL, NULL, GalagaSampleInfo, GalagaSampleName, GalagaInputInfo, GalagamwDIPInfo,
	galagaInit, DrvExit, DrvFrame, DrvDraw, galagaScan, NULL,
	GALAGA_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvGalagamk = {
	"galagamk", "galaga", NULL, "galaga", "1981",
	"Galaga (Midway set 2)\0", NULL, "Namco (Midway license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, GalagamkRomInfo, GalagamkRomName, NULL, NULL, GalagaSampleInfo, GalagaSampleName, GalagaInputInfo, GalagaDIPInfo,
	galagaInit, DrvExit, DrvFrame, DrvDraw, galagaScan, NULL,
	GALAGA_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvGalagamf = {
	"galagamf", "galaga", NULL, "galaga", "1981",
	"Galaga (Midway set 1 with fast shoot hack)\0", NULL, "Namco (Midway license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, GalagamfRomInfo, GalagamfRomName, NULL, NULL, GalagaSampleInfo, GalagaSampleName, GalagaInputInfo, GalagamwDIPInfo,
	galagaInit, DrvExit, DrvFrame, DrvDraw, galagaScan, NULL,
	GALAGA_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvGallag = {
	"gallag", "galaga", NULL, "galaga", "1982",
	"Gallag\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, GallagRomInfo, GallagRomName, NULL, NULL, GalagaSampleInfo, GalagaSampleName, GalagaInputInfo, GalagaDIPInfo,
	gallagInit, DrvExit, DrvFrame, DrvDraw, galagaScan, NULL,
	GALAGA_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvNebulbee = {
	"nebulbee", "galaga", NULL, "galaga", "1981",
	"Nebulous Bee\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_BOOTLEG | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, NebulbeeRomInfo, NebulbeeRomName, NULL, NULL, GalagaSampleInfo, GalagaSampleName, GalagaInputInfo, GalagaDIPInfo,
	gallagInit, DrvExit, DrvFrame, DrvDraw, galagaScan, NULL,
	GALAGA_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

// https://www.donkeykonghacks.net/page_other.html
struct BurnDriver BurnDrvGalagawm = {
	"galagawm", "galaga", NULL, "galaga", "2024",
	"Galaga Wave Mixer (Hack)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HACK | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, GalagawmRomInfo, GalagawmRomName, NULL, NULL, GalagaSampleInfo, GalagaSampleName, GalagaInputInfo, GalagaDIPInfo,
	galagaInit, DrvExit, DrvFrame, DrvDraw, galagaScan, NULL,
	GALAGA_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

/* === Dig Dug === */

static struct BurnInputInfo DigdugInputList[] =
{
	{"P1 Start"             , BIT_DIGITAL  , &input.ports[0].current.bits.bit[2], "p1 start"  },
	{"P2 Start"             , BIT_DIGITAL  , &input.ports[0].current.bits.bit[3], "p2 start"  },
	{"P1 Coin"              , BIT_DIGITAL  , &input.ports[0].current.bits.bit[4], "p1 coin"   },
	{"P2 Coin"              , BIT_DIGITAL  , &input.ports[0].current.bits.bit[5], "p2 coin"   },

	{"P1 Up"                , BIT_DIGITAL  , &input.ports[1].current.bits.bit[0], "p1 up"     },
	{"P1 Down"              , BIT_DIGITAL  , &input.ports[1].current.bits.bit[2], "p1 down"   },
	{"P1 Left"              , BIT_DIGITAL  , &input.ports[1].current.bits.bit[3], "p1 left"   },
	{"P1 Right"             , BIT_DIGITAL  , &input.ports[1].current.bits.bit[1], "p1 right"  },
	{"P1 Fire 1"            , BIT_DIGITAL  , &input.ports[1].current.bits.bit[4], "p1 fire 1" },

	{"P2 Up (cocktail)"     , BIT_DIGITAL  , &input.ports[2].current.bits.bit[0], "p2 up"     },
	{"P2 Down (Cocktail)"   , BIT_DIGITAL  , &input.ports[2].current.bits.bit[2], "p2 down"   },
	{"P2 Left (Cocktail)"   , BIT_DIGITAL  , &input.ports[2].current.bits.bit[3], "p2 left"   },
	{"P2 Right (Cocktail)"  , BIT_DIGITAL  , &input.ports[2].current.bits.bit[1], "p2 right"  },
	{"P2 Fire 1 (Cocktail)" , BIT_DIGITAL  , &input.ports[2].current.bits.bit[4], "p2 fire 1" },

	{"Service"              , BIT_DIGITAL  , &input.ports[0].current.bits.bit[7], "service"   },
	{"Reset"                , BIT_DIGITAL  , &input.reset,                        "reset"     },
	{"Dip 1"                , BIT_DIPSWITCH, &input.dip[0].byte,                  "dip"       },
	{"Dip 2"                , BIT_DIPSWITCH, &input.dip[1].byte,                  "dip"       },
};

STDINPUTINFO(Digdug)

#define DIGDUG_NUM_OF_DIPSWITCHES      2

static struct BurnDIPInfo DigdugDIPList[]=
{
	{  0x10,    0xf0,    0xff,    0xa1,       NULL		               },

	// nOffset, nID,     nMask,   nDefault,   NULL
	{  0x00,    0xff,    0xff,    0xa1,       NULL		               },
	{  0x01,    0xff,    0xff,    0x24,       NULL		               },

	// Dip 1
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Coin B"		            },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x07,    0x07,       "3 Coins 1 Credits"		},
	{  0x00,    0x01,    0x07,    0x03,       "2 Coins 1 Credits"		},
	{  0x00,    0x01,    0x07,    0x05,       "2 Coins 3 Credits"		},
	{  0x00,    0x01,    0x07,    0x01,       "1 Coin  1 Credits"		},
	{  0x00,    0x01,    0x07,    0x06,       "1 Coin  2 Credits"		},
	{  0x00,    0x01,    0x07,    0x02,       "1 Coin  3 Credits"		},
	{  0x00,    0x01,    0x07,    0x04,       "1 Coin  6 Credits"		},
	{  0x00,    0x01,    0x07,    0x00,       "1 Coin  7 Credits"		},

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Bonus Life (1,2,3) / (5)"		      },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x38,    0x20,       "10K & Every 40K / 20K & Every 60K"	},
	{  0x00,    0x01,    0x38,    0x10,       "10K & Every 50K / 30K & Every 80K"	},
	{  0x00,    0x01,    0x38,    0x30,       "20K & Every 60K / 20K & 50K Only"	},
	{  0x00,    0x01,    0x38,    0x08,       "20K & Every 70K / 20K & 60K Only"	},
	{  0x00,    0x01,    0x38,    0x28,       "10K & 40K Only  / 30K & 70K Only"	},
	{  0x00,    0x01,    0x38,    0x18,       "20K & 60K Only  / 20K Only"  		},
	{  0x00,    0x01,    0x38,    0x38,       "10K Only        / 30K Only"        },
	{  0x00,    0x01,    0x38,    0x00,       "None            / None"		      },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Lives"		            },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0xc0,    0x00,       "1"		               },
	{  0x00,    0x01,    0xc0,    0x40,       "2"		               },
	{  0x00,    0x01,    0xc0,    0x80,       "3"		               },
	{  0x00,    0x01,    0xc0,    0xc0,       "5"		               },

	// DIP 2
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Coin A"		            },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0xc0,    0x40,       "2 Coins 1 Credits"		},
	{  0x01,    0x01,    0xc0,    0x00,       "1 Coin  1 Credits"		},
	{  0x01,    0x01,    0xc0,    0xc0,       "2 Coins 3 Credits"		},
	{  0x01,    0x01,    0xc0,    0x80,       "1 Coin  2 Credits"		},

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Freeze"		            },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x20,    0x20,       "Off"		               },
	{  0x01,    0x01,    0x20,    0x00,       "On"		               },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Demo Sounds"		      },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x10,    0x10,       "Off"		               },
	{  0x01,    0x01,    0x10,    0x00,       "On"		               },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Allow Continue"		   },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x08,    0x08,       "No"		               },
	{  0x01,    0x01,    0x08,    0x00,       "Yes"		               },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Cabinet"		         },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x04,    0x04,       "Upright"		         },
	{  0x01,    0x01,    0x04,    0x00,       "Cocktail"		         },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Difficulty"		      },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x03,    0x00,       "Easy"		            },
	{  0x01,    0x01,    0x03,    0x02,       "Medium"		            },
	{  0x01,    0x01,    0x03,    0x01,       "Hard"		            },
	{  0x01,    0x01,    0x03,    0x03,       "Hardest"		         },
};

STDDIPINFO(Digdug)

// Dig Dug (rev 2)

static struct BurnRomInfo digdugRomDesc[] = {
	{ "dd1a.1",	      0x1000, 0xa80ec984, BRF_ESS | BRF_PRG  }, //  0 Z80 #1 Program Code
	{ "dd1a.2",	      0x1000, 0x559f00bd, BRF_ESS | BRF_PRG  }, //  1
	{ "dd1a.3",	      0x1000, 0x8cbc6fe1, BRF_ESS | BRF_PRG  }, //  2
	{ "dd1a.4",	      0x1000, 0xd066f830, BRF_ESS | BRF_PRG  }, //  3

	{ "dd1a.5",	      0x1000, 0x6687933b, BRF_ESS | BRF_PRG  }, //  4	Z80 #2 Program Code
	{ "dd1a.6",	      0x1000, 0x843d857f, BRF_ESS | BRF_PRG  }, //  5

	{ "dd1.7",	      0x1000, 0xa41bce72, BRF_ESS | BRF_PRG  }, //  6	Z80 #3 Program Code

	{ "dd1.9",	      0x0800, 0xf14a6fe1, BRF_GRA            }, //  7	Characters

	{ "dd1.15",	      0x1000, 0xe22957c8, BRF_GRA            }, //  8	Sprites
	{ "dd1.14",	      0x1000, 0x2829ec99, BRF_GRA            }, //  9
	{ "dd1.13",	      0x1000, 0x458499e9, BRF_GRA            }, // 10
	{ "dd1.12",	      0x1000, 0xc58252a0, BRF_GRA            }, // 11

	{ "dd1.11",	      0x1000, 0x7b383983, BRF_GRA            }, // 12	Characters 8x8 2bpp

	{ "dd1.10b",      0x1000, 0x2cf399c2, BRF_GRA            }, // 13 Playfield Data

	{ "136007.113",   0x0020, 0x4cb9da99, BRF_GRA            }, // 14 Palette Prom
	{ "136007.111",   0x0100, 0x00c7c419, BRF_GRA            }, // 15 Sprite Color Prom
	{ "136007.112",   0x0100, 0xe9b3e08e, BRF_GRA            }, // 16 Character Color Prom

	{ "136007.110",   0x0100, 0x7a2815b4, BRF_GRA            }, // 17 Namco Sound Proms
	{ "136007.109",   0x0100, 0x77245b66, BRF_GRA            }, // 18
};

STD_ROM_PICK(digdug)
STD_ROM_FN(digdug)

// Dig Dug (rev 1)

static struct BurnRomInfo digdug1RomDesc[] = {
	{ "dd1.1",	      0x1000, 0xb9198079, BRF_ESS | BRF_PRG  }, //  0 Z80 #1 Program Code
	{ "dd1.2",	      0x1000, 0xb2acbe49, BRF_ESS | BRF_PRG  }, //  1
	{ "dd1.3",	      0x1000, 0xd6407b49, BRF_ESS | BRF_PRG  }, //  2
	{ "dd1.4b",	      0x1000, 0xf4cebc16, BRF_ESS | BRF_PRG  }, //  3

	{ "dd1.5b",	      0x1000, 0x370ef9b4, BRF_ESS | BRF_PRG  }, //  4	Z80 #2 Program Code
	{ "dd1.6b",	      0x1000, 0x361eeb71, BRF_ESS | BRF_PRG  }, //  5

	{ "dd1.7",	      0x1000, 0xa41bce72, BRF_ESS | BRF_PRG  }, //  6	Z80 #3 Program Code

	{ "dd1.9",	      0x0800, 0xf14a6fe1, BRF_GRA            }, //  7	Characters

	{ "dd1.15",	      0x1000, 0xe22957c8, BRF_GRA            }, //  8	Sprites
	{ "dd1.14",	      0x1000, 0x2829ec99, BRF_GRA            }, //  9
	{ "dd1.13",	      0x1000, 0x458499e9, BRF_GRA            }, // 10
	{ "dd1.12",	      0x1000, 0xc58252a0, BRF_GRA            }, // 11

	{ "dd1.11",	      0x1000, 0x7b383983, BRF_GRA            }, // 12	Characters 8x8 2bpp

	{ "dd1.10b",      0x1000, 0x2cf399c2, BRF_GRA            }, // 13 Playfield Data

	{ "136007.113",   0x0020, 0x4cb9da99, BRF_GRA            }, // 14 Palette Prom
	{ "136007.111",   0x0100, 0x00c7c419, BRF_GRA            }, // 15 Sprite Color Prom
	{ "136007.112",   0x0100, 0xe9b3e08e, BRF_GRA            }, // 16 Character Color Prom

	{ "136007.110",   0x0100, 0x7a2815b4, BRF_GRA            }, // 17 Namco Sound Proms
	{ "136007.109",   0x0100, 0x77245b66, BRF_GRA            }, // 18
};

STD_ROM_PICK(digdug1)
STD_ROM_FN(digdug1)

// Dig Dug (Atari, rev 2)

static struct BurnRomInfo digdugatRomDesc[] = {
	{ "136007.201",	  0x1000, 0x23d0b1a4, BRF_ESS | BRF_PRG  }, //  0 Z80 #1 Program Code
	{ "136007.202",	  0x1000, 0x5453dc1f, BRF_ESS | BRF_PRG  }, //  1
	{ "136007.203",	  0x1000, 0xc9077dfa, BRF_ESS | BRF_PRG  }, //  2
	{ "136007.204",	  0x1000, 0xa8fc8eac, BRF_ESS | BRF_PRG  }, //  3

	{ "136007.205",	  0x1000, 0x5ba385c5, BRF_ESS | BRF_PRG  }, //  4	Z80 #2 Program Code
	{ "136007.206",	  0x1000, 0x382b4011, BRF_ESS | BRF_PRG  }, //  5

	{ "136007.107",	  0x1000, 0xa41bce72, BRF_ESS | BRF_PRG  }, //  6	Z80 #3 Program Code

	{ "136007.108",	  0x0800, 0x3d24a3af, BRF_GRA            }, //  7	Characters

	{ "136007.116",	  0x1000, 0xe22957c8, BRF_GRA            }, //  8	Sprites
	{ "136007.117",	  0x1000, 0xa3bbfd85, BRF_GRA            }, //  9
	{ "136007.118",	  0x1000, 0x458499e9, BRF_GRA            }, // 10
	{ "136007.119",	  0x1000, 0xc58252a0, BRF_GRA            }, // 11

	{ "136007.115",	  0x1000, 0x754539be, BRF_GRA            }, // 12	Characters 8x8 2bpp

	{ "136007.114",   0x1000, 0xd6822397, BRF_GRA            }, // 13 Playfield Data

	{ "136007.113",   0x0020, 0x4cb9da99, BRF_GRA            }, // 14 Palette Prom
	{ "136007.111",   0x0100, 0x00c7c419, BRF_GRA            }, // 15 Sprite Color Prom
	{ "136007.112",   0x0100, 0xe9b3e08e, BRF_GRA            }, // 16 Character Color Prom

	{ "136007.110",   0x0100, 0x7a2815b4, BRF_GRA            }, // 17 Namco Sound Proms
	{ "136007.109",   0x0100, 0x77245b66, BRF_GRA            }, // 18
};

STD_ROM_PICK(digdugat)
STD_ROM_FN(digdugat)

static struct DigDug_PlayField_Params
{
	// Dig Dug playfield stuff
	INT32 playField;
	INT32 alphaColor;
	INT32 playEnable;
	INT32 playColor;
} playFieldParams;

#define DIGDUG_NUM_OF_CHAR_PALETTE_BITS   1
#define DIGDUG_NUM_OF_SPRITE_PALETTE_BITS 2
#define DIGDUG_NUM_OF_BGTILE_PALETTE_BITS 2

#define DIGDUG_PALETTE_SIZE_BGTILES       0x100
#define DIGDUG_PALETTE_SIZE_SPRITES       0x100
#define DIGDUG_PALETTE_SIZE_CHARS         0x20
#define DIGDUG_PALETTE_SIZE (DIGDUG_PALETTE_SIZE_CHARS + \
	DIGDUG_PALETTE_SIZE_SPRITES + \
	DIGDUG_PALETTE_SIZE_BGTILES)

#define DIGDUG_PALETTE_OFFSET_BGTILES     0x0
#define DIGDUG_PALETTE_OFFSET_SPRITE      (DIGDUG_PALETTE_OFFSET_BGTILES + \
	DIGDUG_PALETTE_SIZE_BGTILES)
#define DIGDUG_PALETTE_OFFSET_CHARS       (DIGDUG_PALETTE_OFFSET_SPRITE + \
	DIGDUG_PALETTE_SIZE_SPRITES)

#define DIGDUG_NUM_OF_CHAR                0x80
#define DIGDUG_SIZE_OF_CHAR_IN_BYTES      0x40

#define DIGDUG_NUM_OF_SPRITE              0x100
#define DIGDUG_SIZE_OF_SPRITE_IN_BYTES    0x200

#define DIGDUG_NUM_OF_BGTILE              0x100
#define DIGDUG_SIZE_OF_BGTILE_IN_BYTES    0x80

static INT32 digdugInit(void);
static INT32 digdugReset(void);

static void digdugMemoryMap1(void);
static void digdugMemoryMap2(void);
static void digdugMemoryMap3(void);

static INT32 digdugCharDecode(void);
static INT32 digdugBGTilesDecode(void);
static INT32 digdugSpriteDecode(void);
static tilemap_callback(digdug_bg);
static tilemap_callback(digdug_fg);
static INT32 digdugTilemapConfig(void);

static void digdug_pf_latch_w(UINT16 offset, UINT8 dta);
static void digdugZ80Writeb840(UINT16 offset, UINT8 dta);

static void digdugCalcPalette(void);
static void digdugRenderTiles(void);
static UINT32 digdugGetSpriteParams(struct Namco_Sprite_Params *spriteParams, UINT32 offset);

static INT32 digdugScan(INT32 nAction, INT32 *pnMin);

static struct CPU_Config_Def digdugCPU[NAMCO_BRD_CPU_COUNT] =
{
	{
		/* CPU ID = */          CPU1,
		/* CPU Read Func = */   namcoZ80ProgRead,
		/* CPU Write Func = */  namcoZ80ProgWrite,
		/* Memory Mapping = */  digdugMemoryMap1
	},
	{
		/* CPU ID = */          CPU2,
		/* CPU Read Func = */   namcoZ80ProgRead,
		/* CPU Write Func = */  namcoZ80ProgWrite,
		/* Memory Mapping = */  digdugMemoryMap2
	},
	{
		/* CPU ID = */          CPU3,
		/* CPU Read Func = */   namcoZ80ProgRead,
		/* CPU Write Func = */  namcoZ80ProgWrite,
		/* Memory Mapping = */  digdugMemoryMap3
	},
};

static struct CPU_Rd_Table digdugReadTable[] =
{
	{ 0x6800, 0x6807, namcoZ80ReadDip         },
	{ 0x7000, 0x700f, namcoCustomICsReadDta   },
	{ 0x7100, 0x7100, namcoCustomICsReadCmd   },
	// EAROM Read
	{ 0xb800, 0xb83f, earom_read              },
	{ 0x0000, 0x0000, NULL                    },
};

static struct CPU_Wr_Table digdugWriteTable[] =
{
	// EAROM Write
	{ 0xb800, 0xb83f, earom_write             },
	{ 0x6800, 0x681f, namcoZ80WriteSound      },
	{ 0xb840, 0xb840, digdugZ80Writeb840      },
	{ 0x6820, 0x6820, namcoZ80WriteCPU1Irq    },
	{ 0x6821, 0x6821, namcoZ80WriteCPU2Irq    },
	{ 0x6822, 0x6822, namcoZ80WriteCPU3Irq    },
	{ 0x6823, 0x6823, namcoZ80WriteCPUReset   },
	//	{ 0x6830, 0x6830, WatchDogWriteNotImplemented },
	{ 0x7000, 0x700f, namcoCustomICsWriteDta  },
	{ 0x7100, 0x7100, namcoCustomICsWriteCmd  },
	{ 0xa000, 0xa006, digdug_pf_latch_w       },
	{ 0xa007, 0xa007, namcoZ80WriteFlipScreen },
	{ 0x0000, 0x0000, NULL                    },

};

static struct Memory_Map_Def digdugMemTable[] =
{
	{  &memory.Z80.rom1,             0x04000,                               MEM_PGM        },
	{  &memory.Z80.rom2,             0x04000,                               MEM_PGM        },
	{  &memory.Z80.rom3,             0x04000,                               MEM_PGM        },
	{  &memory.PROM.palette,         0x00020,                               MEM_ROM        },
	{  &memory.PROM.charLookup,      0x00100,                               MEM_ROM        },
	{  &memory.PROM.spriteLookup,    0x00100,                               MEM_ROM        },
	{  &NamcoSoundProm,              0x00200,                               MEM_ROM        },

	{  &memory.RAM.video,            0x00800,                               MEM_RAM        },
	{  &memory.RAM.shared1,          0x00400,                               MEM_RAM        },
	{  &memory.RAM.shared2,          0x00400,                               MEM_RAM        },
	{  &memory.RAM.shared3,          0x00400,                               MEM_RAM        },

	{  (UINT8 **)&gameData,          0x1000,                                MEM_DATA       },
	{  &graphics.fgChars,            DIGDUG_NUM_OF_CHAR * 8 * 8,            MEM_DATA       },
	{  &graphics.bgTiles,            DIGDUG_NUM_OF_BGTILE * 8 * 8,          MEM_DATA       },
	{  &graphics.sprites,            DIGDUG_NUM_OF_SPRITE * 16 * 16,        MEM_DATA       },
	{  (UINT8 **)&graphics.palette,  DIGDUG_PALETTE_SIZE * sizeof(UINT32),  MEM_DATA32     },
};

#define DIGDUG_MEM_TBL_SIZE      (sizeof(digdugMemTable) / sizeof(struct Memory_Map_Def))

static struct ROM_Load_Def digdugROMTable[] =
{
	{  &memory.Z80.rom1,             0x00000, NULL                 },
	{  &memory.Z80.rom1,             0x01000, NULL                 },
	{  &memory.Z80.rom1,             0x02000, NULL                 },
	{  &memory.Z80.rom1,             0x03000, NULL                 },
	{  &memory.Z80.rom2,             0x00000, NULL                 },
	{  &memory.Z80.rom2,             0x01000, NULL                 },
	{  &memory.Z80.rom3,             0x00000, NULL                 },
	{  &tempRom,                     0x00000, digdugCharDecode     },
	{  &tempRom,                     0x00000, NULL                 },
	{  &tempRom,                     0x01000, NULL                 },
	{  &tempRom,                     0x02000, NULL                 },
	{  &tempRom,                     0x03000, digdugSpriteDecode   },
	{  &tempRom,                     0x00000, digdugBGTilesDecode  },
	{  &gameData,                    0x00000, NULL                 },
	{  &memory.PROM.palette,         0x00000, NULL                 },
	{  &memory.PROM.spriteLookup,    0x00000, NULL                 },
	{  &memory.PROM.charLookup,      0x00000, NULL                 },
	{  &NamcoSoundProm,              0x00000, NULL                 },
	{  &NamcoSoundProm,              0x00100, namcoMachineInit     },
};

#define DIGDUG_ROM_TBL_SIZE      (sizeof(digdugROMTable) / sizeof(struct ROM_Load_Def))

typedef void (*DrawFunc_t)(void);

static DrawFunc_t digdugDrawFuncs[] =
{
	digdugCalcPalette,
	digdugRenderTiles,
	namcoRenderSprites,
};

#define DIGDUG_DRAW_TBL_SIZE  (sizeof(digdugDrawFuncs) / sizeof(digdugDrawFuncs[0]))

static struct Namco_Custom_RW_Entry digdugCustomRWTable[] =
{
	{  0x71,    namco51xxRead  },
	{  0xa1,    namco51xxWrite },
	{  0xb1,    namco51xxRead  },
	{  0xc1,    namco51xxWrite },
	{  0xd2,    namco53xxRead  },
	{  0x00,    NULL           }
};

static struct Machine_Config_Def digdugMachineConfig =
{
	/*cpus                   */ digdugCPU,
	/*wrAddrList             */ digdugWriteTable,
	/*rdAddrList             */ digdugReadTable,
	/*memMapTable            */ digdugMemTable,
	/*sizeOfMemMapTable      */ DIGDUG_MEM_TBL_SIZE,
	/*romLayoutTable         */ digdugROMTable,
	/*sizeOfRomLayoutTable   */ DIGDUG_ROM_TBL_SIZE,
	/*tempRomSize            */ 0x4000,
	/*tilemapsConfig         */ digdugTilemapConfig,
	/*drawLayerTable         */ digdugDrawFuncs,
	/*drawTableSize          */ DIGDUG_DRAW_TBL_SIZE,
	/*getSpriteParams        */ digdugGetSpriteParams,
	/*reset                  */ digdugReset,
	/*customRWTable          */ digdugCustomRWTable,
	/*n54xxSampleList        */ NULL
};

static INT32 digdugInit(void)
{
	machine.game = NAMCO_DIGDUG;
	machine.numOfDips = DIGDUG_NUM_OF_DIPSWITCHES;

	machine.config = &digdugMachineConfig;

	INT32 retVal = namcoInitBoard();

	if (0 == retVal)
		earom_init();

	return retVal;
}

static INT32 digdugReset(void)
{
	playFieldParams.playField = 0;
	playFieldParams.alphaColor = 0;
	playFieldParams.playEnable = 0;
	playFieldParams.playColor = 0;

	earom_reset();

	return DrvDoReset();
}

static void digdugMemoryMap1(void)
{
	ZetMapMemory(memory.Z80.rom1,    0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(memory.RAM.video,   0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared1, 0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared2, 0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared3, 0x9800, 0x9bff, MAP_RAM);
}

static void digdugMemoryMap2(void)
{
	ZetMapMemory(memory.Z80.rom2,    0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(memory.RAM.video,   0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared1, 0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared2, 0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared3, 0x9800, 0x9bff, MAP_RAM);
}

static void digdugMemoryMap3(void)
{
	ZetMapMemory(memory.Z80.rom3,    0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(memory.RAM.video,   0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared1, 0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared2, 0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared3, 0x9800, 0x9bff, MAP_RAM);
}

static INT32 digdugCharDecode(void)
{
	GfxDecode(
			  DIGDUG_NUM_OF_CHAR,
			  DIGDUG_NUM_OF_CHAR_PALETTE_BITS,
			  8, 8,
			  (INT32*)planeOffsets1Bit,
			  (INT32*)xOffsets8x8Tiles1Bit,
			  (INT32*)yOffsets8x8Tiles1Bit,
			  DIGDUG_SIZE_OF_CHAR_IN_BYTES,
			  tempRom,
			  graphics.fgChars
			 );

	return 0;
}

static INT32 digdugBGTilesDecode(void)
{
	GfxDecode(
			  DIGDUG_NUM_OF_BGTILE,
			  DIGDUG_NUM_OF_BGTILE_PALETTE_BITS,
			  8, 8,
			  (INT32*)planeOffsets2Bit,
			  (INT32*)xOffsets8x8Tiles2Bit,
			  (INT32*)yOffsets8x8Tiles2Bit,
			  DIGDUG_SIZE_OF_BGTILE_IN_BYTES,
			  tempRom,
			  graphics.bgTiles
			 );

	return 0;
}

static INT32 digdugSpriteDecode(void)
{
	GfxDecode(
			  DIGDUG_NUM_OF_SPRITE,
			  DIGDUG_NUM_OF_SPRITE_PALETTE_BITS,
			  16, 16,
			  (INT32*)planeOffsets2Bit,
			  (INT32*)xOffsets16x16Tiles2Bit,
			  (INT32*)yOffsets16x16Tiles2Bit,
			  DIGDUG_SIZE_OF_SPRITE_IN_BYTES,
			  tempRom,
			  graphics.sprites
			 );

	return 0;
}

static tilemap_callback ( digdug_fg )
{
	INT32 code = memory.RAM.video[offs];
	INT32 colour = ((code >> 4) & 0x0e) | ((code >> 3) & 2);
	code &= 0x7f;

	TILE_SET_INFO(1, code, colour, 0);
}

static tilemap_callback ( digdug_bg )
{
	UINT8 *pf = gameData + (playFieldParams.playField << 10);
	INT8 pfval = pf[offs & 0xfff];
	INT32 pfColour = (pfval >> 4) + (playFieldParams.playColor << 4);

	TILE_SET_INFO(0, pfval, pfColour, 0);
}

static INT32 digdugTilemapConfig(void)
{
	GenericTilemapInit(
					   0,
					   namco_map_scan, digdug_bg_map_callback,
					   8, 8,
					   NAMCO_TMAP_WIDTH, NAMCO_TMAP_HEIGHT
					  );
	GenericTilemapSetGfx(
						 0,
						 graphics.bgTiles,
						 DIGDUG_NUM_OF_BGTILE_PALETTE_BITS,
						 8, 8,
						 DIGDUG_NUM_OF_BGTILE * 8 * 8,
						 DIGDUG_PALETTE_OFFSET_BGTILES,
						 DIGDUG_PALETTE_SIZE_BGTILES - 1
						);
	GenericTilemapSetTransparent(0, 0);

	GenericTilemapInit(
					   1,
					   namco_map_scan, digdug_fg_map_callback,
					   8, 8,
					   NAMCO_TMAP_WIDTH, NAMCO_TMAP_HEIGHT
					  );
	GenericTilemapSetGfx(
						 1,
						 graphics.fgChars,
						 DIGDUG_NUM_OF_CHAR_PALETTE_BITS,
						 8, 8,
						 DIGDUG_NUM_OF_CHAR * 8 * 8,
						 DIGDUG_PALETTE_OFFSET_CHARS,
						 DIGDUG_PALETTE_SIZE_CHARS - 1
						);
	GenericTilemapSetTransparent(1, 0);

	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, 0);

	return 0;
}

static void digdug_pf_latch_w(UINT16 offset, UINT8 dta)
{
	switch (offset)
	{
		case 0:
			playFieldParams.playField = (playFieldParams.playField & ~1) | (dta & 1);
			break;

		case 1:
			playFieldParams.playField = (playFieldParams.playField & ~2) | ((dta << 1) & 2);
			break;

		case 2:
			playFieldParams.alphaColor = dta & 1;
			break;

		case 3:
			playFieldParams.playEnable = dta & 1;
			break;

		case 4:
			playFieldParams.playColor = (playFieldParams.playColor & ~1) | (dta & 1);
			break;

		case 5:
			playFieldParams.playColor = (playFieldParams.playColor & ~2) | ((dta << 1) & 2);
			break;
	}
}

static void digdugZ80Writeb840(UINT16 offset, UINT8 dta)
{
	earom_ctrl_write(0xb840, dta);
}

#define DIGDUG_3BIT_PALETTE_SIZE    32

static void digdugCalcPalette(void)
{
	UINT32 palette[DIGDUG_3BIT_PALETTE_SIZE];

	for (INT32 i = 0; i < DIGDUG_3BIT_PALETTE_SIZE; i ++)
	{
		INT32 r = Colour3Bit[(memory.PROM.palette[i] >> 0) & 0x07];
		INT32 g = Colour3Bit[(memory.PROM.palette[i] >> 3) & 0x07];
		INT32 b = Colour3Bit[(memory.PROM.palette[i] >> 5) & 0x06];

		palette[i] = BurnHighCol(r, g, b, 0);
	}

	/* bg_select */
	for (INT32 i = 0; i < DIGDUG_PALETTE_SIZE_BGTILES; i ++)
	{
		graphics.palette[DIGDUG_PALETTE_OFFSET_BGTILES + i] =
			palette[memory.PROM.charLookup[i] & 0x0f];
	}

	/* sprites */
	for (INT32 i = 0; i < DIGDUG_PALETTE_SIZE_SPRITES; i ++)
	{
		graphics.palette[DIGDUG_PALETTE_OFFSET_SPRITE + i] =
			palette[(memory.PROM.spriteLookup[i] & 0x0f) + 0x10];
	}

	/* characters - direct mapping */
	for (INT32 i = 0; i < DIGDUG_PALETTE_SIZE_CHARS; i += 2)
	{
		graphics.palette[DIGDUG_PALETTE_OFFSET_CHARS + i + 0] = palette[0];
		graphics.palette[DIGDUG_PALETTE_OFFSET_CHARS + i + 1] = palette[i/2];
	}
}

static void digdugRenderTiles(void)
{
	GenericTilemapSetScrollX(0, 0);
	GenericTilemapSetScrollY(0, 0);
	GenericTilemapSetOffsets(0, 0, 0);
	GenericTilemapSetEnable(0, (0 == playFieldParams.playEnable));
	GenericTilemapDraw(0, pTransDraw, 0 | TMAP_DRAWOPAQUE);
	GenericTilemapSetEnable(1, 1);
	GenericTilemapDraw(1, pTransDraw, 0 | TMAP_TRANSPARENT);
}

static UINT32 digdugGetSpriteParams(struct Namco_Sprite_Params *spriteParams, UINT32 offset)
{
	UINT8 *spriteRam1 = memory.RAM.shared1 + 0x380;
	UINT8 *spriteRam2 = memory.RAM.shared2 + 0x380;
	UINT8 *spriteRam3 = memory.RAM.shared3 + 0x380;

	INT32 sprite = spriteRam1[offset + 0];
	if (sprite & 0x80) spriteParams->sprite = (sprite & 0xc0) | ((sprite & ~0xc0) << 2);
	else               spriteParams->sprite = sprite;
	spriteParams->colour = spriteRam1[offset + 1] & 0x3f;

	spriteParams->xStart = spriteRam2[offset + 1] - 40 + 1;
	if (8 > spriteParams->xStart) spriteParams->xStart += 0x100;
	spriteParams->yStart = NAMCO_SCREEN_WIDTH - spriteRam2[offset + 0] + 1;
	spriteParams->xStep = 16;
	spriteParams->yStep = 16;

	spriteParams->flags = spriteRam3[offset + 0] & 0x03;
	spriteParams->flags |= ((sprite & 0x80) >> 4) | ((sprite & 0x80) >> 5);

	spriteParams->transparent_mask = 0x0f;

	if (spriteParams->flags & ySize)
	{
		spriteParams->yStart -= 16;
	}

	if (spriteParams->flags & xSize)
	{
		if (spriteParams->flags & xFlip)
		{
			spriteParams->xStart += 16;
			spriteParams->xStep  = -16;
		}
	}

	spriteParams->paletteBits = DIGDUG_NUM_OF_SPRITE_PALETTE_BITS;
	spriteParams->paletteOffset = DIGDUG_PALETTE_OFFSET_SPRITE;

	return 1;
}

static INT32 digdugScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(playFieldParams.playField);
		SCAN_VAR(playFieldParams.alphaColor);
		SCAN_VAR(playFieldParams.playEnable);
		SCAN_VAR(playFieldParams.playColor);

	}

	if (nAction & ACB_NVRAM) {
		earom_scan(nAction, pnMin);
	}

	return DrvScan(nAction, pnMin);
}

struct BurnDriver BurnDrvDigdug = {
	"digdug", NULL, NULL, NULL, "1982",
	"Dig Dug (rev 2)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, digdugRomInfo, digdugRomName, NULL, NULL, NULL, NULL, DigdugInputInfo, DigdugDIPInfo,
	digdugInit, DrvExit, DrvFrame, DrvDraw, digdugScan, NULL,
	DIGDUG_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvDigdug1 = {
	"digdug1", "digdug", NULL, NULL, "1982",
	"Dig Dug (rev 1)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, digdug1RomInfo, digdug1RomName, NULL, NULL, NULL, NULL, DigdugInputInfo, DigdugDIPInfo,
	digdugInit, DrvExit, DrvFrame, DrvDraw, digdugScan, NULL,
	DIGDUG_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvDigdugat = {
	"digdugat", "digdug", NULL, NULL, "1982",
	"Dig Dug (Atari, rev 2)\0", NULL, "Namco (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, digdugatRomInfo, digdugatRomName, NULL, NULL, NULL, NULL, DigdugInputInfo, DigdugDIPInfo,
	digdugInit, DrvExit, DrvFrame, DrvDraw, digdugScan, NULL,
	DIGDUG_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

/* === XEVIOUS === */

static struct BurnInputInfo XeviousInputList[] =
{
	{"Dip 1"             , BIT_DIPSWITCH,  &input.dip[0].byte,                  "dip"       },
	{"Dip 2"             , BIT_DIPSWITCH,  &input.dip[1].byte,                  "dip"       },

	{"Reset"             , BIT_DIGITAL,    &input.reset,                        "reset"     },

	{"Up"                , BIT_DIGITAL,    &input.ports[1].current.bits.bit[0], "p1 up"     },
	{"Right"             , BIT_DIGITAL,    &input.ports[1].current.bits.bit[1], "p1 right"  },
	{"Down"              , BIT_DIGITAL,    &input.ports[1].current.bits.bit[2], "p1 down"   },
	{"Left"              , BIT_DIGITAL,    &input.ports[1].current.bits.bit[3], "p1 left"   },
	{"P1 Button 1"       , BIT_DIGITAL,    &input.ports[1].current.bits.bit[4], "p1 fire 1" },
	// hack! CUF - must remap this input to DIP1.0
	{"P1 Button 2"       , BIT_DIGITAL,    &input.ports[1].current.bits.bit[6], "p1 fire 2" },

	{"Up (Cocktail)"     , BIT_DIGITAL,    &input.ports[2].current.bits.bit[0], "p2 up"     },
	{"Right (Cocktail)"  , BIT_DIGITAL,    &input.ports[2].current.bits.bit[1], "p2 right"  },
	{"Down (Cocktail)"   , BIT_DIGITAL,    &input.ports[2].current.bits.bit[2], "p2 down"   },
	{"Left (Cocktail)"   , BIT_DIGITAL,    &input.ports[2].current.bits.bit[3], "p2 left"   },
	{"Fire 1 (Cocktail)" , BIT_DIGITAL,    &input.ports[2].current.bits.bit[4], "p2 fire 1" },
	// hack! CUF - must remap this input to DIP1.4
	{"Fire 2 (Cocktail)" , BIT_DIGITAL,    &input.ports[2].current.bits.bit[6], "p2 fire 2" },

	{"Start 1"           , BIT_DIGITAL,    &input.ports[0].current.bits.bit[2], "p1 start"  },
	{"Start 2"           , BIT_DIGITAL,    &input.ports[0].current.bits.bit[3], "p2 start"  },
	{"Coin 1"            , BIT_DIGITAL,    &input.ports[0].current.bits.bit[4], "p1 coin"   },
	{"Coin 2"            , BIT_DIGITAL,    &input.ports[0].current.bits.bit[5], "p2 coin"   },
	{"Service"           , BIT_DIGITAL,    &input.ports[0].current.bits.bit[7], "service"   },

};

STDINPUTINFO(Xevious)

#define XEVIOUS_NUM_OF_DIPSWITCHES     2

static struct BurnDIPInfo XeviousDIPList[]=
{
	// Default Values
	// nOffset, nID,     nMask,   nDefault,   NULL
	{  0x00,    0xff,    0xff,    0xFF,       NULL                     },
	{  0x01,    0xff,    0xff,    0xFF,       NULL                     },

	// Dip 1
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       0,          "Button 2 (Not a DIP)"   },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x01,    0x01,       "Released"               },
	{  0x00,    0x01,    0x01,    0x00,       "Held"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Flags Award Bonus Life" },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x02,    0x02,       "Yes"                    },
	{  0x00,    0x01,    0x02,    0x00,       "No"                     },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Coin B"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x0C,    0x04,       "2 Coins 1 Play"         },
	{  0x00,    0x01,    0x0C,    0x0C,       "1 Coin  1 Play"         },
	{  0x00,    0x01,    0x0C,    0x00,       "2 Coins 3 Plays"        },
	{  0x00,    0x01,    0x0C,    0x08,       "1 Coin  2 Plays"        },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       0,          "Button 2 (Cocktail) (Not a DIP)" },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x10,    0x10,       "Released"               },
	{  0x00,    0x01,    0x10,    0x00,       "Held"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Difficulty"             },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x60,    0x40,       "Easy"                   },
	{  0x00,    0x01,    0x60,    0x60,       "Normal"                 },
	{  0x00,    0x01,    0x60,    0x20,       "Hard"                   },
	{  0x00,    0x01,    0x60,    0x00,       "Hardest"                },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Freeze"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x80,    0x80,       "Off"                    },
	{  0x00,    0x01,    0x80,    0x00,       "On"                     },

	// Dip 2
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Coin A"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x03,    0x01,       "2 Coins 1 Play"         },
	{  0x01,    0x01,    0x03,    0x03,       "1 Coin  1 Play"         },
	{  0x01,    0x01,    0x03,    0x00,       "2 Coins 3 Plays"        },
	{  0x01,    0x01,    0x03,    0x02,       "1 Coin  2 Plays"        },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Bonus Life"             },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x1C,    0x18,       "10k  40k  40k"          },
	{  0x01,    0x01,    0x1C,    0x14,       "10k  50k  50k"          },
	{  0x01,    0x01,    0x1C,    0x10,       "20k  50k  50k"          },
	{  0x01,    0x01,    0x1C,    0x1C,       "20k  60k  60k"          },
	{  0x01,    0x01,    0x1C,    0x0C,       "20k  70k  70k"          },
	{  0x01,    0x01,    0x1C,    0x08,       "20k  80k  80k"          },
	{  0x01,    0x01,    0x1C,    0x04,       "20k  60k"               },
	{  0x01,    0x01,    0x1C,    0x00,       "None"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Lives"                  },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x60,    0x40,       "1"                      },
	{  0x01,    0x01,    0x60,    0x20,       "2"                      },
	{  0x01,    0x01,    0x60,    0x60,       "3"                      },
	{  0x01,    0x01,    0x60,    0x00,       "5"                      },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Cabinet"                },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x80,    0x80,       "Upright"                },
	{  0x01,    0x01,    0x80,    0x00,       "Cocktail"               },

};

STDDIPINFO(Xevious)

static struct BurnDIPInfo XeviousaDIPList[]=
{
	// Default Values
	// nOffset, nID,     nMask,   nDefault,   NULL
	{  0x00,    0xff,    0xff,    0xFF,       NULL                     },
	{  0x01,    0xff,    0xff,    0xFF,       NULL                     },

	// Dip 1
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       0,          "Button 2 (Not a DIP)"   },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x01,    0x01,       "Released"               },
	{  0x00,    0x01,    0x01,    0x00,       "Held"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Flags Award Bonus Life" },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x02,    0x02,       "Yes"                    },
	{  0x00,    0x01,    0x02,    0x00,       "No"                     },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Coin B"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x0C,    0x0c,       "1 Coin 1 Play"          },
	{  0x00,    0x01,    0x0C,    0x08,       "1 Coin 2 Plays"         },
	{  0x00,    0x01,    0x0C,    0x04,       "1 Coin 3 Plays"         },
	{  0x00,    0x01,    0x0C,    0x00,       "1 Coin 6 Plays"         },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       0,          "Button 2 (Cocktail) (Not a DIP)" },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x10,    0x10,       "Released"               },
	{  0x00,    0x01,    0x10,    0x00,       "Held"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Difficulty"             },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x60,    0x40,       "Easy"                   },
	{  0x00,    0x01,    0x60,    0x60,       "Normal"                 },
	{  0x00,    0x01,    0x60,    0x20,       "Hard"                   },
	{  0x00,    0x01,    0x60,    0x00,       "Hardest"                },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Copyright"              },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x80,    0x00,       "Namco"                  },
	{  0x00,    0x01,    0x80,    0x80,       "Atari/Namco"            },

	// Dip 2
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Coin A"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x03,    0x01,       "2 Coins 1 Play"         },
	{  0x01,    0x01,    0x03,    0x03,       "1 Coin  1 Play"         },
	{  0x01,    0x01,    0x03,    0x00,       "2 Coins 3 Plays"        },
	{  0x01,    0x01,    0x03,    0x02,       "1 Coin  2 Plays"        },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Bonus Life"             },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x1C,    0x18,       "10k  40k  40k"          },
	{  0x01,    0x01,    0x1C,    0x14,       "10k  50k  50k"          },
	{  0x01,    0x01,    0x1C,    0x10,       "20k  50k  50k"          },
	{  0x01,    0x01,    0x1C,    0x1C,       "20k  60k  60k"          },
	{  0x01,    0x01,    0x1C,    0x0C,       "20k  70k  70k"          },
	{  0x01,    0x01,    0x1C,    0x08,       "20k  80k  80k"          },
	{  0x01,    0x01,    0x1C,    0x04,       "20k  60k"               },
	{  0x01,    0x01,    0x1C,    0x00,       "None"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Lives"                  },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x60,    0x40,       "1"                      },
	{  0x01,    0x01,    0x60,    0x20,       "2"                      },
	{  0x01,    0x01,    0x60,    0x60,       "3"                      },
	{  0x01,    0x01,    0x60,    0x00,       "5"                      },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Cabinet"                },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x80,    0x80,       "Upright"                },
	{  0x01,    0x01,    0x80,    0x00,       "Cocktail"               },

};

STDDIPINFO(Xeviousa)

static struct BurnDIPInfo XeviousbDIPList[]=
{
	// Default Values
	// nOffset, nID,     nMask,   nDefault,   NULL
	{  0x00,    0xff,    0xff,    0xFF,       NULL                     },
	{  0x01,    0xff,    0xff,    0xFF,       NULL                     },

	// Dip 1
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       0,          "Button 2 (Not a DIP)"   },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x01,    0x01,       "Released"               },
	{  0x00,    0x01,    0x01,    0x00,       "Held"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Flags Award Bonus Life" },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x02,    0x02,       "Yes"                    },
	{  0x00,    0x01,    0x02,    0x00,       "No"                     },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Coin B"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x0C,    0x04,       "2 Coins 1 Play"         },
	{  0x00,    0x01,    0x0C,    0x0C,       "1 Coin  1 Play"         },
	{  0x00,    0x01,    0x0C,    0x00,       "2 Coins 3 Plays"        },
	{  0x00,    0x01,    0x0C,    0x08,       "1 Coin  2 Plays"        },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       0,          "Button 2 (Cocktail) (Not a DIP)" },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x10,    0x10,       "Released"               },
	{  0x00,    0x01,    0x10,    0x00,       "Held"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Difficulty"             },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x60,    0x40,       "Easy"                   },
	{  0x00,    0x01,    0x60,    0x60,       "Normal"                 },
	{  0x00,    0x01,    0x60,    0x20,       "Hard"                   },
	{  0x00,    0x01,    0x60,    0x00,       "Hardest"                },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Copyright"              },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x80,    0x00,       "Namco"                  },
	{  0x00,    0x01,    0x80,    0x80,       "Atari/Namco"            },

	// Dip 2
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Coin A"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x03,    0x01,       "2 Coins 1 Play"         },
	{  0x01,    0x01,    0x03,    0x03,       "1 Coin  1 Play"         },
	{  0x01,    0x01,    0x03,    0x00,       "2 Coins 3 Plays"        },
	{  0x01,    0x01,    0x03,    0x02,       "1 Coin  2 Plays"        },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Bonus Life"             },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x1C,    0x18,       "10k  40k  40k"          },
	{  0x01,    0x01,    0x1C,    0x14,       "10k  50k  50k"          },
	{  0x01,    0x01,    0x1C,    0x10,       "20k  50k  50k"          },
	{  0x01,    0x01,    0x1C,    0x1C,       "20k  60k  60k"          },
	{  0x01,    0x01,    0x1C,    0x0C,       "20k  70k  70k"          },
	{  0x01,    0x01,    0x1C,    0x08,       "20k  80k  80k"          },
	{  0x01,    0x01,    0x1C,    0x04,       "20k  60k"               },
	{  0x01,    0x01,    0x1C,    0x00,       "None"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Lives"                  },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x60,    0x40,       "1"                      },
	{  0x01,    0x01,    0x60,    0x20,       "2"                      },
	{  0x01,    0x01,    0x60,    0x60,       "3"                      },
	{  0x01,    0x01,    0x60,    0x00,       "5"                      },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Cabinet"                },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x80,    0x80,       "Upright"                },
	{  0x01,    0x01,    0x80,    0x00,       "Cocktail"               },

};

STDDIPINFO(Xeviousb)

static struct BurnDIPInfo SxeviousDIPList[]=
{
	// Default Values
	// nOffset, nID,     nMask,   nDefault,   NULL
	{  0x00,    0xff,    0xff,    0x7F,       NULL                     },
	{  0x01,    0xff,    0xff,    0xFF,       NULL                     },

	// Dip 1
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       0,          "Button 2 (Not a DIP)"   },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x01,    0x01,       "Released"               },
	{  0x00,    0x01,    0x01,    0x00,       "Held"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Flags Award Bonus Life" },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x02,    0x02,       "Yes"                    },
	{  0x00,    0x01,    0x02,    0x00,       "No"                     },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Coin B"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x0C,    0x04,       "1 Coins 3 Plays"         },
	{  0x00,    0x01,    0x0C,    0x0C,       "1 Coin  1 Play"         },
	{  0x00,    0x01,    0x0C,    0x00,       "1 Coins 6 Plays"        },
	{  0x00,    0x01,    0x0C,    0x08,       "1 Coin  2 Plays"        },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       0,          "Button 2 (Cocktail) (Not a DIP)" },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x10,    0x10,       "Released"               },
	{  0x00,    0x01,    0x10,    0x00,       "Held"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Difficulty"             },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x60,    0x40,       "Easy"                   },
	{  0x00,    0x01,    0x60,    0x60,       "Normal"                 },
	{  0x00,    0x01,    0x60,    0x20,       "Hard"                   },
	{  0x00,    0x01,    0x60,    0x00,       "Hardest"                },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Freeze"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x00,    0x01,    0x80,    0x00,       "Off"                    },
	{  0x00,    0x01,    0x80,    0x80,       "On"                     },

	// Dip 2
	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Coin A"                 },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x03,    0x01,       "2 Coins 1 Play"         },
	{  0x01,    0x01,    0x03,    0x03,       "1 Coin  1 Play"         },
	{  0x01,    0x01,    0x03,    0x00,       "2 Coins 3 Plays"        },
	{  0x01,    0x01,    0x03,    0x02,       "1 Coin  2 Plays"        },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       8,          "Bonus Life"             },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x1C,    0x18,       "10k  40k  40k"          },
	{  0x01,    0x01,    0x1C,    0x14,       "10k  50k  50k"          },
	{  0x01,    0x01,    0x1C,    0x10,       "20k  50k  50k"          },
	{  0x01,    0x01,    0x1C,    0x1C,       "20k  60k  60k"          },
	{  0x01,    0x01,    0x1C,    0x0C,       "20k  70k  70k"          },
	{  0x01,    0x01,    0x1C,    0x08,       "20k  80k  80k"          },
	{  0x01,    0x01,    0x1C,    0x04,       "20k  60k"               },
	{  0x01,    0x01,    0x1C,    0x00,       "None"                   },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       4,          "Lives"                  },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x60,    0x40,       "1"                      },
	{  0x01,    0x01,    0x60,    0x20,       "2"                      },
	{  0x01,    0x01,    0x60,    0x60,       "3"                      },
	{  0x01,    0x01,    0x60,    0x00,       "5"                      },

	// x,       DIP_GRP, x,       OptionCnt,  szTitle
	{  0,       0xfe,    0,       2,          "Cabinet"                },
	// nInput,  nFlags,  nMask,   nSetting,   szText
	{  0x01,    0x01,    0x80,    0x80,       "Upright"                },
	{  0x01,    0x01,    0x80,    0x00,       "Cocktail"               },

};

STDDIPINFO(Sxevious)

static struct BurnRomInfo XeviousRomDesc[] = {
	{ "xvi_1.3p",      0x01000, 0x09964dda, BRF_ESS | BRF_PRG   }, //  0 Z80 #1 Program Code
	{ "xvi_2.3m",      0x01000, 0x60ecce84, BRF_ESS | BRF_PRG   }, //  1
	{ "xvi_3.2m",      0x01000, 0x79754b7d, BRF_ESS | BRF_PRG   }, //  2
	{ "xvi_4.2l",      0x01000, 0xc7d4bbf0, BRF_ESS | BRF_PRG   }, //  3

	{ "xvi_5.3f",      0x01000, 0xc85b703f, BRF_ESS | BRF_PRG   }, //  4 Z80 #2 Program Code
	{ "xvi_6.3j",      0x01000, 0xe18cdaad, BRF_ESS | BRF_PRG   }, //  5

	{ "xvi_7.2c",      0x01000, 0xdd35cf1c, BRF_ESS | BRF_PRG   }, //  6 Z80 #3 Program Code

	{ "xvi_12.3b",     0x01000, 0x088c8b26, BRF_GRA             }, //  7 background characters
	{ "xvi_13.3c",     0x01000, 0xde60ba25, BRF_GRA             }, //  8 bg pattern B0
	{ "xvi_14.3d",     0x01000, 0x535cdbbc, BRF_GRA             }, //  9 bg pattern B1

	{ "xvi_15.4m",     0x02000, 0xdc2c0ecb, BRF_GRA             }, // 10 sprite set #1, planes 0/1
	{ "xvi_18.4r",     0x02000, 0x02417d19, BRF_GRA             }, // 11 sprite set #1, plane 2, set #2, plane 0
	{ "xvi_17.4p",     0x02000, 0xdfb587ce, BRF_GRA             }, // 12 sprite set #2, planes 1/2
	{ "xvi_16.4n",     0x01000, 0x605ca889, BRF_GRA             }, // 13 sprite set #3, planes 0/1

	{ "xvi_9.2a",      0x01000, 0x57ed9879, BRF_GRA             }, // 14
	{ "xvi_10.2b",     0x02000, 0xae3ba9e5, BRF_GRA             }, // 15
	{ "xvi_11.2c",     0x01000, 0x31e244dd, BRF_GRA             }, // 16

	{ "xvi-8.6a",      0x00100, 0x5cc2727f, BRF_GRA             }, // 17 palette red component
	{ "xvi-9.6d",      0x00100, 0x5c8796cc, BRF_GRA             }, // 18 palette green component
	{ "xvi-10.6e",     0x00100, 0x3cb60975, BRF_GRA             }, // 19 palette blue component
	{ "xvi-7.4h",      0x00200, 0x22d98032, BRF_GRA             }, // 20 bg tiles lookup table low bits
	{ "xvi-6.4f",      0x00200, 0x3a7599f0, BRF_GRA             }, // 21 bg tiles lookup table high bits
	{ "xvi-4.3l",      0x00200, 0xfd8b9d91, BRF_GRA             }, // 22 sprite lookup table low bits
	{ "xvi-5.3m",      0x00200, 0xbf906d82, BRF_GRA             }, // 23 sprite lookup table high bits

	{ "xvi-2.7n",      0x00100, 0x550f06bc, BRF_GRA             }, // 24
	{ "xvi-1.5n",      0x00100, 0x77245b66, BRF_GRA             }, // 25 timing - not used

	{ "xvi-3.1f",      0x00117, 0x9192d57a, BRF_OPT             }, // N82S153N
};

STD_ROM_PICK(Xevious)
STD_ROM_FN(Xevious)

static struct BurnRomInfo XeviousaRomDesc[] = {
	{ "xea-1m-a.bin",  0x02000, 0x8c2b50ec, BRF_ESS | BRF_PRG   }, //  0 Z80 #1 Program Code
	{ "xea-1l-a.bin",  0x02000, 0x0821642b, BRF_ESS | BRF_PRG   }, //  1

	{ "xea-4c-a.bin",  0x02000, 0x14d8fa03, BRF_ESS | BRF_PRG   }, //  2 Z80 #2 Program Code

	{ "xvi_7.2c",      0x01000, 0xdd35cf1c, BRF_ESS | BRF_PRG   }, //  3 Z80 #3 Program Code

	{ "xvi_12.3b",     0x01000, 0x088c8b26, BRF_GRA             }, //  4 background characters
	{ "xvi_13.3c",     0x01000, 0xde60ba25, BRF_GRA             }, //  5 bg pattern B0
	{ "xvi_14.3d",     0x01000, 0x535cdbbc, BRF_GRA             }, //  6 bg pattern B1

	{ "xvi_15.4m",     0x02000, 0xdc2c0ecb, BRF_GRA             }, //  7 sprite set #1, planes 0/1
	{ "xvi_18.4r",     0x02000, 0x02417d19, BRF_GRA             }, //  8 sprite set #1, plane 2, set #2, plane 0
	{ "xvi_17.4p",     0x02000, 0xdfb587ce, BRF_GRA             }, //  9 sprite set #2, planes 1/2
	{ "xvi_16.4n",     0x01000, 0x605ca889, BRF_GRA             }, // 10 sprite set #3, planes 0/1

	{ "xvi_9.2a",      0x01000, 0x57ed9879, BRF_GRA             }, // 11
	{ "xvi_10.2b",     0x02000, 0xae3ba9e5, BRF_GRA             }, // 12
	{ "xvi_11.2c",     0x01000, 0x31e244dd, BRF_GRA             }, // 13

	{ "xvi-8.6a",      0x00100, 0x5cc2727f, BRF_GRA             }, // 14 palette red component
	{ "xvi-9.6d",      0x00100, 0x5c8796cc, BRF_GRA             }, // 15 palette green component
	{ "xvi-10.6e",     0x00100, 0x3cb60975, BRF_GRA             }, // 16 palette blue component
	{ "xvi-7.4h",      0x00200, 0x22d98032, BRF_GRA             }, // 17 bg tiles lookup table low bits
	{ "xvi-6.4f",      0x00200, 0x3a7599f0, BRF_GRA             }, // 18 bg tiles lookup table high bits
	{ "xvi-4.3l",      0x00200, 0xfd8b9d91, BRF_GRA             }, // 19 sprite lookup table low bits
	{ "xvi-5.3m",      0x00200, 0xbf906d82, BRF_GRA             }, // 20 sprite lookup table high bits

	{ "xvi-2.7n",      0x00100, 0x550f06bc, BRF_GRA             }, // 21
	{ "xvi-1.5n",      0x00100, 0x77245b66, BRF_GRA             }, // 22 timing - not used

	{ "xvi-3.1f",      0x00117, 0x9192d57a, BRF_OPT             }, // N82S153N
};

STD_ROM_PICK(Xeviousa)
STD_ROM_FN(Xeviousa)

static struct BurnRomInfo XeviousbRomDesc[] = {
	{ "1m.bin",        0x02000, 0xe82a22f6, BRF_ESS | BRF_PRG   }, //  0 Z80 #1 Program Code
	{ "1l.bin",        0x02000, 0x13831df9, BRF_ESS | BRF_PRG   }, //  1

	{ "4c.bin",        0x02000, 0x827e7747, BRF_ESS | BRF_PRG   }, //  2 Z80 #2 Program Code

	{ "xvi_7.2c",      0x01000, 0xdd35cf1c, BRF_ESS | BRF_PRG   }, //  3 Z80 #3 Program Code

	{ "xvi_12.3b",     0x01000, 0x088c8b26, BRF_GRA             }, //  4 background characters
	{ "xvi_13.3c",     0x01000, 0xde60ba25, BRF_GRA             }, //  5 bg pattern B0
	{ "xvi_14.3d",     0x01000, 0x535cdbbc, BRF_GRA             }, //  6 bg pattern B1

	{ "xvi_15.4m",     0x02000, 0xdc2c0ecb, BRF_GRA             }, //  7 sprite set #1, planes 0/1
	{ "xvi_18.4r",     0x02000, 0x02417d19, BRF_GRA             }, //  8 sprite set #1, plane 2, set #2, plane 0
	{ "xvi_17.4p",     0x02000, 0xdfb587ce, BRF_GRA             }, //  9 sprite set #2, planes 1/2
	{ "xvi_16.4n",     0x01000, 0x605ca889, BRF_GRA             }, // 10 sprite set #3, planes 0/1

	{ "xvi_9.2a",      0x01000, 0x57ed9879, BRF_GRA             }, // 11
	{ "xvi_10.2b",     0x02000, 0xae3ba9e5, BRF_GRA             }, // 12
	{ "xvi_11.2c",     0x01000, 0x31e244dd, BRF_GRA             }, // 13

	{ "xvi-8.6a",      0x00100, 0x5cc2727f, BRF_GRA             }, // 14 palette red component
	{ "xvi-9.6d",      0x00100, 0x5c8796cc, BRF_GRA             }, // 15 palette green component
	{ "xvi-10.6e",     0x00100, 0x3cb60975, BRF_GRA             }, // 16 palette blue component
	{ "xvi-7.4h",      0x00200, 0x22d98032, BRF_GRA             }, // 17 bg tiles lookup table low bits
	{ "xvi-6.4f",      0x00200, 0x3a7599f0, BRF_GRA             }, // 18 bg tiles lookup table high bits
	{ "xvi-4.3l",      0x00200, 0xfd8b9d91, BRF_GRA             }, // 19 sprite lookup table low bits
	{ "xvi-5.3m",      0x00200, 0xbf906d82, BRF_GRA             }, // 20 sprite lookup table high bits

	{ "xvi-2.7n",      0x00100, 0x550f06bc, BRF_GRA             }, // 21
	{ "xvi-1.5n",      0x00100, 0x77245b66, BRF_GRA             }, // 22 timing - not used

	{ "xvi-3.1f",      0x00117, 0x9192d57a, BRF_OPT             }, // N82S153N
};

STD_ROM_PICK(Xeviousb)
STD_ROM_FN(Xeviousb)

static struct BurnRomInfo XeviouscRomDesc[] = {
	{ "xvi_u_.3p",     0x01000, 0x7b203868, BRF_ESS | BRF_PRG   }, //  0 Z80 #1 Program Code
	{ "xv_2-2.3m",     0x01000, 0xb6fe738e, BRF_ESS | BRF_PRG   }, //  1
	{ "xv_2-3.2m",     0x01000, 0xdbd52ff5, BRF_ESS | BRF_PRG   }, //  2
	{ "xvi_u_.2l",     0x01000, 0xad12af53, BRF_ESS | BRF_PRG   }, //  3

	{ "xv2_5.3f",      0x01000, 0xf8cc2861, BRF_ESS | BRF_PRG   }, //  4 Z80 #2 Program Code
	{ "xvi_6.3j",      0x01000, 0xe18cdaad, BRF_ESS | BRF_PRG   }, //  5

	{ "xvi_7.2c",      0x01000, 0xdd35cf1c, BRF_ESS | BRF_PRG   }, //  6 Z80 #3 Program Code

	{ "xvi_12.3b",     0x01000, 0x088c8b26, BRF_GRA             }, //  7 background characters
	{ "xvi_13.3c",     0x01000, 0xde60ba25, BRF_GRA             }, //  8 bg pattern B0
	{ "xvi_14.3d",     0x01000, 0x535cdbbc, BRF_GRA             }, //  9 bg pattern B1

	{ "xvi_15.4m",     0x02000, 0xdc2c0ecb, BRF_GRA             }, // 10 sprite set #1, planes 0/1
	{ "xvi_18.4r",     0x02000, 0x02417d19, BRF_GRA             }, // 11 sprite set #1, plane 2, set #2, plane 0
	{ "xvi_17.4p",     0x02000, 0xdfb587ce, BRF_GRA             }, // 12 sprite set #2, planes 1/2
	{ "xvi_16.4n",     0x01000, 0x605ca889, BRF_GRA             }, // 13 sprite set #3, planes 0/1

	{ "xvi_9.2a",      0x01000, 0x57ed9879, BRF_GRA             }, // 14
	{ "xvi_10.2b",     0x02000, 0xae3ba9e5, BRF_GRA             }, // 15
	{ "xvi_11.2c",     0x01000, 0x31e244dd, BRF_GRA             }, // 16

	{ "xvi-8.6a",      0x00100, 0x5cc2727f, BRF_GRA             }, // 17 palette red component
	{ "xvi-9.6d",      0x00100, 0x5c8796cc, BRF_GRA             }, // 18 palette green component
	{ "xvi-10.6e",     0x00100, 0x3cb60975, BRF_GRA             }, // 19 palette blue component
	{ "xvi-7.4h",      0x00200, 0x22d98032, BRF_GRA             }, // 20 bg tiles lookup table low bits
	{ "xvi-6.4f",      0x00200, 0x3a7599f0, BRF_GRA             }, // 21 bg tiles lookup table high bits
	{ "xvi-4.3l",      0x00200, 0xfd8b9d91, BRF_GRA             }, // 22 sprite lookup table low bits
	{ "xvi-5.3m",      0x00200, 0xbf906d82, BRF_GRA             }, // 23 sprite lookup table high bits

	{ "xvi-2.7n",      0x00100, 0x550f06bc, BRF_GRA             }, // 24
	{ "xvi-1.5n",      0x00100, 0x77245b66, BRF_GRA             }, // 25 timing - not used

	{ "xvi-3.1f",      0x00117, 0x9192d57a, BRF_OPT             }, // N82S153N
};

STD_ROM_PICK(Xeviousc)
STD_ROM_FN(Xeviousc)

static struct BurnRomInfo SxeviousRomDesc[] = {
	{ "cpu_3p.rom",    0x01000, 0x1c8d27d5, BRF_ESS | BRF_PRG   }, //  0 Z80 #1 Program Code
	{ "cpu_3m.rom",    0x01000, 0xfd04e615, BRF_ESS | BRF_PRG   }, //  1
	{ "xv3_3.2m",      0x01000, 0x294d5404, BRF_ESS | BRF_PRG   }, //  2
	{ "xv3_4.2l",      0x01000, 0x6a44bf92, BRF_ESS | BRF_PRG   }, //  3

	{ "xv3_5.3f",      0x01000, 0xd4bd3d81, BRF_ESS | BRF_PRG   }, //  4 Z80 #2 Program Code
	{ "xv3_6.3j",      0x01000, 0xaf06be5f, BRF_ESS | BRF_PRG   }, //  5

	{ "xvi_7.2c",      0x01000, 0xdd35cf1c, BRF_ESS | BRF_PRG   }, //  6 Z80 #3 Program Code

	{ "xvi_12.3b",     0x01000, 0x088c8b26, BRF_GRA             }, //  7 background characters
	{ "xvi_13.3c",     0x01000, 0xde60ba25, BRF_GRA             }, //  8 bg pattern B0
	{ "xvi_14.3d",     0x01000, 0x535cdbbc, BRF_GRA             }, //  9 bg pattern B1

	{ "xvi_15.4m",     0x02000, 0xdc2c0ecb, BRF_GRA             }, // 10 sprite set #1, planes 0/1
	{ "xvi_18.4r",     0x02000, 0x02417d19, BRF_GRA             }, // 11 sprite set #1, plane 2, set #2, plane 0
	{ "xvi_17.4p",     0x02000, 0xdfb587ce, BRF_GRA             }, // 12 sprite set #2, planes 1/2
	{ "xvi_16.4n",     0x01000, 0x605ca889, BRF_GRA             }, // 13 sprite set #3, planes 0/1

	{ "xvi_9.2a",      0x01000, 0x57ed9879, BRF_GRA             }, // 14
	{ "xvi_10.2b",     0x02000, 0xae3ba9e5, BRF_GRA             }, // 15
	{ "xvi_11.2c",     0x01000, 0x31e244dd, BRF_GRA             }, // 16

	{ "xvi-8.6a",      0x00100, 0x5cc2727f, BRF_GRA             }, // 17 palette red component
	{ "xvi-9.6d",      0x00100, 0x5c8796cc, BRF_GRA             }, // 18 palette green component
	{ "xvi-10.6e",     0x00100, 0x3cb60975, BRF_GRA             }, // 19 palette blue component
	{ "xvi-7.4h",      0x00200, 0x22d98032, BRF_GRA             }, // 20 bg tiles lookup table low bits
	{ "xvi-6.4f",      0x00200, 0x3a7599f0, BRF_GRA             }, // 21 bg tiles lookup table high bits
	{ "xvi-4.3l",      0x00200, 0xfd8b9d91, BRF_GRA             }, // 22 sprite lookup table low bits
	{ "xvi-5.3m",      0x00200, 0xbf906d82, BRF_GRA             }, // 23 sprite lookup table high bits

	{ "xvi-2.7n",      0x00100, 0x550f06bc, BRF_GRA             }, // 24
	{ "xvi-1.5n",      0x00100, 0x77245b66, BRF_GRA             }, // 25 timing - not used
};

STD_ROM_PICK(Sxevious)
STD_ROM_FN(Sxevious)

static struct BurnRomInfo SxeviousjRomDesc[] = {
	{ "xv3_1.3p",      0x01000, 0xafbc3372, BRF_ESS | BRF_PRG   }, //  0 Z80 #1 Program Code
	{ "xv3_2.3m",      0x01000, 0x1854a5ee, BRF_ESS | BRF_PRG   }, //  1
	{ "xv3_3.2m",      0x01000, 0x294d5404, BRF_ESS | BRF_PRG   }, //  2
	{ "xv3_4.2l",      0x01000, 0x6a44bf92, BRF_ESS | BRF_PRG   }, //  3

	{ "xv3_5.3f",      0x01000, 0xd4bd3d81, BRF_ESS | BRF_PRG   }, //  4 Z80 #2 Program Code
	{ "xv3_6.3j",      0x01000, 0xaf06be5f, BRF_ESS | BRF_PRG   }, //  5

	{ "xvi_7.2c",      0x01000, 0xdd35cf1c, BRF_ESS | BRF_PRG   }, //  6 Z80 #3 Program Code

	{ "xvi_12.3b",     0x01000, 0x088c8b26, BRF_GRA             }, //  7 background characters
	{ "xvi_13.3c",     0x01000, 0xde60ba25, BRF_GRA             }, //  8 bg pattern B0
	{ "xvi_14.3d",     0x01000, 0x535cdbbc, BRF_GRA             }, //  9 bg pattern B1

	{ "xvi_15.4m",     0x02000, 0xdc2c0ecb, BRF_GRA             }, // 10 sprite set #1, planes 0/1
	{ "xvi_18.4r",     0x02000, 0x02417d19, BRF_GRA             }, // 11 sprite set #1, plane 2, set #2, plane 0
	{ "xvi_17.4p",     0x02000, 0xdfb587ce, BRF_GRA             }, // 12 sprite set #2, planes 1/2
	{ "xvi_16.4n",     0x01000, 0x605ca889, BRF_GRA             }, // 13 sprite set #3, planes 0/1

	{ "xvi_9.2a",      0x01000, 0x57ed9879, BRF_GRA             }, // 14
	{ "xvi_10.2b",     0x02000, 0xae3ba9e5, BRF_GRA             }, // 15
	{ "xvi_11.2c",     0x01000, 0x31e244dd, BRF_GRA             }, // 16

	{ "xvi-8.6a",      0x00100, 0x5cc2727f, BRF_GRA             }, // 17 palette red component
	{ "xvi-9.6d",      0x00100, 0x5c8796cc, BRF_GRA             }, // 18 palette green component
	{ "xvi-10.6e",     0x00100, 0x3cb60975, BRF_GRA             }, // 19 palette blue component
	{ "xvi-7.4h",      0x00200, 0x22d98032, BRF_GRA             }, // 20 bg tiles lookup table low bits
	{ "xvi-6.4f",      0x00200, 0x3a7599f0, BRF_GRA             }, // 21 bg tiles lookup table high bits
	{ "xvi-4.3l",      0x00200, 0xfd8b9d91, BRF_GRA             }, // 22 sprite lookup table low bits
	{ "xvi-5.3m",      0x00200, 0xbf906d82, BRF_GRA             }, // 23 sprite lookup table high bits

	{ "xvi-2.7n",      0x00100, 0x550f06bc, BRF_GRA             }, // 24
	{ "xvi-1.5n",      0x00100, 0x77245b66, BRF_GRA             }, // 25 timing - not used
};

STD_ROM_PICK(Sxeviousj)
STD_ROM_FN(Sxeviousj)


static struct BurnSampleInfo XeviousSampleDesc[] = {
#if !defined (ROM_VERIFY)
	{ "explo1", SAMPLE_NOLOOP },	// ground target explosion
	{ "explo2", SAMPLE_NOLOOP },	// Solvalou explosion
	{ "explo3", SAMPLE_NOLOOP },	// credit
	{ "explo4", SAMPLE_NOLOOP },	// Garu Zakato explosion
#endif
	{ "",           0 }
};

STD_SAMPLE_PICK(Xevious)
STD_SAMPLE_FN(Xevious)

#define XEVIOUS_NO_OF_COLS                   64
#define XEVIOUS_NO_OF_ROWS                   32

#define XEVIOUS_NUM_OF_CHAR_PALETTE_BITS     1
#define XEVIOUS_NUM_OF_SPRITE_PALETTE_BITS   3
#define XEVIOUS_NUM_OF_BGTILE_PALETTE_BITS   2

#define XEVIOUS_PALETTE_OFFSET_BGTILES       0x0
#define XEVIOUS_PALETTE_SIZE_BGTILES         (0x80 * 4)
#define XEVIOUS_PALETTE_OFFSET_SPRITE        (XEVIOUS_PALETTE_OFFSET_BGTILES + \
	XEVIOUS_PALETTE_SIZE_BGTILES)
#define XEVIOUS_PALETTE_SIZE_SPRITES         (0x40 * 8)
#define XEVIOUS_PALETTE_OFFSET_CHARS         (XEVIOUS_PALETTE_OFFSET_SPRITE + \
	XEVIOUS_PALETTE_SIZE_SPRITES)
#define XEVIOUS_PALETTE_SIZE_CHARS           (0x40 * 2)
#define XEVIOUS_PALETTE_SIZE (XEVIOUS_PALETTE_SIZE_CHARS + \
	XEVIOUS_PALETTE_SIZE_SPRITES + \
	XEVIOUS_PALETTE_SIZE_BGTILES)
#define XEVIOUS_PALETTE_MEM_SIZE_IN_BYTES    (XEVIOUS_PALETTE_SIZE * \
	sizeof(UINT32))

#define XEVIOUS_NUM_OF_CHAR                  0x200
#define XEVIOUS_SIZE_OF_CHAR_IN_BYTES        (8 * 8)
#define XEVIOUS_CHAR_MEM_SIZE_IN_BYTES       (XEVIOUS_NUM_OF_CHAR * \
	XEVIOUS_SIZE_OF_CHAR_IN_BYTES)

#define XEVIOUS_NUM_OF_SPRITE1               0x080
#define XEVIOUS_NUM_OF_SPRITE2               0x080
#define XEVIOUS_NUM_OF_SPRITE3               0x040
#define XEVIOUS_NUM_OF_SPRITE                (XEVIOUS_NUM_OF_SPRITE1 + \
	XEVIOUS_NUM_OF_SPRITE2 + \
	XEVIOUS_NUM_OF_SPRITE3)
#define XEVIOUS_SIZE_OF_SPRITE_IN_BYTES      0x200
#define XEVIOUS_SPRITE_MEM_SIZE_IN_BYTES     (XEVIOUS_NUM_OF_SPRITE * \
	XEVIOUS_SIZE_OF_SPRITE_IN_BYTES)

#define XEVIOUS_NUM_OF_BGTILE                0x200
#define XEVIOUS_SIZE_OF_BGTILE_IN_BYTES      (8 * 8)
#define XEVIOUS_TILES_MEM_SIZE_IN_BYTES      (XEVIOUS_NUM_OF_BGTILE * \
	XEVIOUS_SIZE_OF_BGTILE_IN_BYTES)

static INT32 XeviousCharXOffsets[8] = 	{ 0, 1, 2, 3, 4, 5, 6, 7 };
static INT32 XeviousCharYOffsets[8] = 	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 };

static struct PlaneOffsets
{
	INT32 fgChars[XEVIOUS_NUM_OF_CHAR_PALETTE_BITS];
	INT32 bgChars[XEVIOUS_NUM_OF_BGTILE_PALETTE_BITS];
	INT32 sprites1[XEVIOUS_NUM_OF_SPRITE_PALETTE_BITS];
	INT32 sprites2[XEVIOUS_NUM_OF_SPRITE_PALETTE_BITS];
	INT32 sprites3[XEVIOUS_NUM_OF_SPRITE_PALETTE_BITS];
} xeviousOffsets = {
	{ 0 },   /* foreground characters */

	/* background tiles */
	/* 512 characters */
	/* 2 bits per pixel */
	/* 8 x 8 characters */
	/* every char takes 8 consecutive bytes */
	{ 0, 512 * 8 * 8 },

	/* sprite set #1 */
	/* 128 sprites */
	/* 3 bits per pixel */
	/* 16 x 16 sprites */
	/* every sprite takes 64 consecutive bytes */
	{ 0x10004, 0x00000, 0x00004 }, // 0x0000 + { 128*64*8+4, 0, 4 }

	/* sprite set #2 */
	{ 0x00000, 0x10000, 0x10004 }, // 0x2000 + { 0, 128*64*8, 128*64*8+4 }

	/* sprite set #3 */
	{ 0x08000, 0x00000, 0x00004 }, // 0x6000 + { 64*64*8, 0, 4 }

};

static INT32 xeviousInit(void);
static void xeviousMemoryMap1(void);
static void xeviousMemoryMap2(void);
static void xeviousMemoryMap3(void);
static INT32 xeviousCharDecode(void);
static INT32 xeviousTilesDecode(void);
static INT32 xeviousSpriteDecode(void);
static INT32 xeviousSpriteLUTDec(void);
static tilemap_scan(xevious);
static tilemap_callback(xevious_bg);
static tilemap_callback(xevious_fg);
static INT32 xeviousTilemapConfig(void);

static UINT8 xeviousPlayFieldRead(UINT16 offset);
static UINT8 xeviousWorkRAMRead(UINT16 offset);
static UINT8 xeviousSharedRAM1Read(UINT16 offset);
static UINT8 xeviousSharedRAM2Read(UINT16 offset);
static UINT8 xeviousSharedRAM3Read(UINT16 offset);

static void xevious_bs_wr(UINT16 offset, UINT8 dta);
static void xevious_vh_latch_w(UINT16 offset, UINT8 dta);
static void xeviousBGColorRAMWrite(UINT16 offset, UINT8 dta);
static void xeviousBGCharRAMWrite(UINT16 offset, UINT8 dta);
static void xeviousFGColorRAMWrite(UINT16 offset, UINT8 dta);
static void xeviousFGCharRAMWrite(UINT16 offset, UINT8 dta);
static void xeviousWorkRAMWrite(UINT16 offset, UINT8 dta);
static void xeviousSharedRAM1Write(UINT16 offset, UINT8 dta);
static void xeviousSharedRAM2Write(UINT16 offset, UINT8 dta);
static void xeviousSharedRAM3Write(UINT16 offset, UINT8 dta);

static void xeviousCalcPalette(void);
static void xeviousRenderTiles0(void);
static void xeviousRenderTiles1(void);
static UINT32 xeviousGetSpriteParams(struct Namco_Sprite_Params *spriteParams, UINT32 offset);

struct Xevious_RAM
{
	UINT8 bs[2];

	UINT8 *workram;
	UINT8 *fg_videoram;
	UINT8 *fg_colorram;
	UINT8 *bg_videoram;
	UINT8 *bg_colorram;
};

struct Xevious_ROM
{
	UINT8 *rom2a;
	UINT8 *rom2b;
	UINT8 *rom2c;
};

static struct Xevious_RAM xeviousRAM;
static struct Xevious_ROM xeviousROM;

static struct CPU_Config_Def xeviousCPU[NAMCO_BRD_CPU_COUNT] =
{
	{
		/* CPU ID = */          CPU1,
		/* CPU Read Func = */   namcoZ80ProgRead,
		/* CPU Write Func = */  namcoZ80ProgWrite,
		/* Memory Mapping = */  xeviousMemoryMap1
	},
	{
		/* CPU ID = */          CPU2,
		/* CPU Read Func = */   namcoZ80ProgRead,
		/* CPU Write Func = */  namcoZ80ProgWrite,
		/* Memory Mapping = */  xeviousMemoryMap2
	},
	{
		/* CPU ID = */          CPU3,
		/* CPU Read Func = */   namcoZ80ProgRead,
		/* CPU Write Func = */  namcoZ80ProgWrite,
		/* Memory Mapping = */  xeviousMemoryMap3
	},
};

static struct CPU_Rd_Table xeviousZ80ReadTable[] =
{
	{ 0x6800, 0x6807, namcoZ80ReadDip            },
	{ 0x7000, 0x700f, namcoCustomICsReadDta      },
	{ 0x7100, 0x7100, namcoCustomICsReadCmd      },
	{ 0x7800, 0x7fff, xeviousWorkRAMRead         },
	{ 0x8000, 0x8fff, xeviousSharedRAM1Read      },
	{ 0x9000, 0x9fff, xeviousSharedRAM2Read      },
	{ 0xa000, 0xafff, xeviousSharedRAM3Read      },
	{ 0xf000, 0xffff, xeviousPlayFieldRead       },
	{ 0x0000, 0x0000, NULL                       },
};

static struct CPU_Wr_Table xeviousZ80WriteTable[] =
{
	{ 0x6800, 0x681f, namcoZ80WriteSound         },
	{ 0x6820, 0x6820, namcoZ80WriteCPU1Irq       },
	{ 0x6821, 0x6821, namcoZ80WriteCPU2Irq       },
	{ 0x6822, 0x6822, namcoZ80WriteCPU3Irq       },
	{ 0x6823, 0x6823, namcoZ80WriteCPUReset      },
	//	{ 0x6830, 0x6830, WatchDogWriteNotImplemented },
	{ 0x7000, 0x700f, namcoCustomICsWriteDta     },
	{ 0x7100, 0x7100, namcoCustomICsWriteCmd     },
	{ 0x7800, 0x7fff, xeviousWorkRAMWrite        },
	{ 0x8000, 0x8fff, xeviousSharedRAM1Write     },
	{ 0x9000, 0x9fff, xeviousSharedRAM2Write     },
	{ 0xa000, 0xafff, xeviousSharedRAM3Write     },
	{ 0xb000, 0xb7ff, xeviousFGColorRAMWrite     },
	{ 0xb800, 0xbfff, xeviousBGColorRAMWrite     },
	{ 0xc000, 0xc7ff, xeviousFGCharRAMWrite      },
	{ 0xc800, 0xcfff, xeviousBGCharRAMWrite      },
	{ 0xd000, 0xd07f, xevious_vh_latch_w         },
	{ 0xf000, 0xffff, xevious_bs_wr              },
	{ 0x0000, 0x0000, NULL                       },
};

static struct Memory_Map_Def xeviousMemTable[] =
{
	{  &memory.Z80.rom1,           0x04000,                           MEM_PGM  },
	{  &memory.Z80.rom2,           0x04000,                           MEM_PGM  },
	{  &memory.Z80.rom3,           0x04000,                           MEM_PGM  },
	{  &memory.PROM.palette,       0x00300,                           MEM_ROM  },
	{  &memory.PROM.charLookup,    0x00400,                           MEM_ROM  },
	{  &memory.PROM.spriteLookup,  0x00400,                           MEM_ROM  },
	{  &NamcoSoundProm,            0x00200,                           MEM_ROM  },

	{  &xeviousRAM.workram,        0x00800,                           MEM_RAM  },
	{  &memory.RAM.shared1,        0x01000,                           MEM_RAM  },
	{  &memory.RAM.shared2,        0x01000,                           MEM_RAM  },
	{  &memory.RAM.shared3,        0x01000,                           MEM_RAM  },
	{  &memory.RAM.video,          0x02000,                           MEM_RAM  },

	{  &graphics.bgTiles,          XEVIOUS_TILES_MEM_SIZE_IN_BYTES,   MEM_DATA },
	{  &xeviousROM.rom2a,          0x01000,                           MEM_DATA },
	{  &xeviousROM.rom2b,          0x02000,                           MEM_DATA },
	{  &xeviousROM.rom2c,          0x01000,                           MEM_DATA },
	{  &graphics.fgChars,          XEVIOUS_CHAR_MEM_SIZE_IN_BYTES,    MEM_DATA },
	{  &graphics.sprites,          XEVIOUS_SPRITE_MEM_SIZE_IN_BYTES,  MEM_DATA },
	{  (UINT8 **)&graphics.palette, XEVIOUS_PALETTE_MEM_SIZE_IN_BYTES,MEM_DATA32},
};

#define XEVIOUS_MEM_TBL_SIZE      (sizeof(xeviousMemTable) / sizeof(struct Memory_Map_Def))

static struct ROM_Load_Def xeviousROMTable[] =
{
	{  &memory.Z80.rom1,             0x00000, NULL                 },
	{  &memory.Z80.rom1,             0x01000, NULL                 },
	{  &memory.Z80.rom1,             0x02000, NULL                 },
	{  &memory.Z80.rom1,             0x03000, NULL                 },
	{  &memory.Z80.rom2,             0x00000, NULL                 },
	{  &memory.Z80.rom2,             0x01000, NULL                 },
	{  &memory.Z80.rom3,             0x00000, NULL                 },

	{  &tempRom,                     0x00000, xeviousCharDecode    },

	{  &tempRom,                     0x00000, NULL                 },
	{  &tempRom,                     0x01000, xeviousTilesDecode   },

	{  &tempRom,                     0x00000, NULL                 },
	{  &tempRom,                     0x02000, NULL                 },
	{  &tempRom,                     0x04000, NULL                 },
	{  &tempRom,                     0x06000, xeviousSpriteDecode  },

	{  &xeviousROM.rom2a,            0x00000, NULL                 },
	{  &xeviousROM.rom2b,            0x00000, NULL                 },
	{  &xeviousROM.rom2c,            0x00000, NULL                 },

	{  &memory.PROM.palette,         0x00000, NULL                 },
	{  &memory.PROM.palette,         0x00100, NULL                 },
	{  &memory.PROM.palette,         0x00200, NULL                 },
	{  &memory.PROM.charLookup,      0x00000, NULL                 },
	{  &memory.PROM.charLookup,      0x00200, NULL                 },
	{  &memory.PROM.spriteLookup,    0x00000, NULL                 },
	{  &memory.PROM.spriteLookup,    0x00200, xeviousSpriteLUTDec  },
	{  &NamcoSoundProm,              0x00000, NULL                 },
	{  &NamcoSoundProm,              0x00100, namcoMachineInit     }
};

#define XEVIOUS_ROM_TBL_SIZE      (sizeof(xeviousROMTable) / sizeof(struct ROM_Load_Def))

static struct ROM_Load_Def xeviousaROMTable[] =
{
	{  &memory.Z80.rom1,             0x00000, NULL                 },
	{  &memory.Z80.rom1,             0x02000, NULL                 },
	{  &memory.Z80.rom2,             0x00000, NULL                 },
	{  &memory.Z80.rom3,             0x00000, NULL                 },

	{  &tempRom,                     0x00000, xeviousCharDecode    },

	{  &tempRom,                     0x00000, NULL                 },
	{  &tempRom,                     0x01000, xeviousTilesDecode   },

	{  &tempRom,                     0x00000, NULL                 },
	{  &tempRom,                     0x02000, NULL                 },
	{  &tempRom,                     0x04000, NULL                 },
	{  &tempRom,                     0x06000, xeviousSpriteDecode  },

	{  &xeviousROM.rom2a,            0x00000, NULL                 },
	{  &xeviousROM.rom2b,            0x00000, NULL                 },
	{  &xeviousROM.rom2c,            0x00000, NULL                 },

	{  &memory.PROM.palette,         0x00000, NULL                 },
	{  &memory.PROM.palette,         0x00100, NULL                 },
	{  &memory.PROM.palette,         0x00200, NULL                 },
	{  &memory.PROM.charLookup,      0x00000, NULL                 },
	{  &memory.PROM.charLookup,      0x00200, NULL                 },
	{  &memory.PROM.spriteLookup,    0x00000, NULL                 },
	{  &memory.PROM.spriteLookup,    0x00200, NULL                 },
	{  &NamcoSoundProm,              0x00000, NULL                 },
	{  &NamcoSoundProm,              0x00100, namcoMachineInit     }
};

#define XEVIOUSA_ROM_TBL_SIZE     (sizeof(xeviousaROMTable) / sizeof(struct ROM_Load_Def))

static DrawFunc_t xeviousDrawFuncs[] =
{
	xeviousCalcPalette,
	xeviousRenderTiles0,
	namcoRenderSprites,
	xeviousRenderTiles1,
};

#define XEVIOUS_DRAW_TBL_SIZE  (sizeof(xeviousDrawFuncs) / sizeof(xeviousDrawFuncs[0]))

static struct Namco_Custom_RW_Entry xeviousCustomRWTable[] =
{
	{  0x71,    namco51xxRead     },
	{  0xa1,    namco51xxWrite    },
	{  0x61,    namco51xxWrite    },
	{  0x74,    namco50xxRead     },
	{  0x64,    namco50xxWrite    },
	{  0x68,    namco54xxWrite    },
	{  0x00,    NULL              }
};

static struct N54XX_Sample_Info_Def xeviousN54xxSampleList[] =
{
	{  0,    "\x40\x40\x01\xff"   }, // ground target explosion
	{  1,    "\x40\x00\x02\xdf"   }, // Solvalou explosion
	{  2,    "\x10\x00\x80\xff"   },	// credit
	{  3,    "\x30\x30\x03\xdf"   },	// Garu Zakato explosion
	{  -1,   ""                   }
};

static struct Machine_Config_Def xeviousMachineConfig =
{
	/*cpus                   */ xeviousCPU,
	/*wrAddrList             */ xeviousZ80WriteTable,
	/*rdAddrList             */ xeviousZ80ReadTable,
	/*memMapTable            */ xeviousMemTable,
	/*sizeOfMemMapTable      */ XEVIOUS_MEM_TBL_SIZE,
	/*romLayoutTable         */ xeviousROMTable,
	/*sizeOfRomLayoutTable   */ XEVIOUS_ROM_TBL_SIZE,
	/*tempRomSize            */ 0x8000,
	/*tilemapsConfig         */ xeviousTilemapConfig,
	/*drawLayerTable         */ xeviousDrawFuncs,
	/*drawTableSize          */ XEVIOUS_DRAW_TBL_SIZE,
	/*getSpriteParams        */ xeviousGetSpriteParams,
	/*reset                  */ DrvDoReset,
	/*customRWTable          */ xeviousCustomRWTable,
	/*n54xxSampleList        */ xeviousN54xxSampleList
};

static struct Machine_Config_Def xeviousaMachineConfig =
{
	/*cpus                   */ xeviousCPU,
	/*wrAddrList             */ xeviousZ80WriteTable,
	/*rdAddrList             */ xeviousZ80ReadTable,
	/*memMapTable            */ xeviousMemTable,
	/*sizeOfMemMapTable      */ XEVIOUS_MEM_TBL_SIZE,
	/*romLayoutTable         */ xeviousaROMTable,
	/*sizeOfRomLayoutTable   */ XEVIOUSA_ROM_TBL_SIZE,
	/*tempRomSize            */ 0x8000,
	/*tilemapsConfig         */ xeviousTilemapConfig,
	/*drawLayerTable         */ xeviousDrawFuncs,
	/*drawTableSize          */ XEVIOUS_DRAW_TBL_SIZE,
	/*getSpriteParams        */ xeviousGetSpriteParams,
	/*reset                  */ DrvDoReset,
	/*customRWTable          */ xeviousCustomRWTable,
	/*n54xxSampleList        */ xeviousN54xxSampleList
};

static INT32 xeviousInit(void)
{
	machine.game = NAMCO_XEVIOUS;
	machine.numOfDips = XEVIOUS_NUM_OF_DIPSWITCHES;

	machine.config = &xeviousMachineConfig;

	return namcoInitBoard();
}

static INT32 xeviousaInit(void)
{
	machine.game = NAMCO_XEVIOUS;
	machine.numOfDips = XEVIOUS_NUM_OF_DIPSWITCHES;

	machine.config = &xeviousaMachineConfig;

	return namcoInitBoard();
}

static void xeviousMemoryMap1(void)
{
	ZetMapMemory(memory.Z80.rom1,    0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(xeviousRAM.workram, 0x7800, 0x7fff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared1, 0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared2, 0x9000, 0x9fff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared3, 0xa000, 0xafff, MAP_RAM);
	ZetMapMemory(memory.RAM.video,   0xb000, 0xcfff, MAP_RAM);
}

static void xeviousMemoryMap2(void)
{
	ZetMapMemory(memory.Z80.rom2,    0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(xeviousRAM.workram, 0x7800, 0x7fff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared1, 0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared2, 0x9000, 0x9fff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared3, 0xa000, 0xafff, MAP_RAM);
	ZetMapMemory(memory.RAM.video,   0xb000, 0xcfff, MAP_RAM);
}

static void xeviousMemoryMap3(void)
{
	ZetMapMemory(memory.Z80.rom3,    0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(xeviousRAM.workram, 0x7800, 0x7fff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared1, 0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared2, 0x9000, 0x9fff, MAP_RAM);
	ZetMapMemory(memory.RAM.shared3, 0xa000, 0xafff, MAP_RAM);
	ZetMapMemory(memory.RAM.video,   0xb000, 0xcfff, MAP_RAM);
}

static INT32 xeviousCharDecode(void)
{
	// Load and decode the chars
	/* foreground characters: */
	/* 512 characters */
	/* 1 bit per pixel */
	/* 8 x 8 characters */
	/* every char takes 8 consecutive bytes */
	GfxDecode(
			  XEVIOUS_NUM_OF_CHAR,
			  XEVIOUS_NUM_OF_CHAR_PALETTE_BITS,
			  8, 8,
			  xeviousOffsets.fgChars,
			  XeviousCharXOffsets,
			  XeviousCharYOffsets,
			  XEVIOUS_SIZE_OF_CHAR_IN_BYTES,
			  tempRom,
			  graphics.fgChars
			 );

	memset(tempRom, 0, machine.config->tempRomSize);

	return 0;
}

static INT32 xeviousTilesDecode(void)
{
	/* background tiles */
	/* 512 characters */
	/* 2 bits per pixel */
	/* 8 x 8 characters */
	/* every char takes 8 consecutive bytes */
	GfxDecode(
			  XEVIOUS_NUM_OF_BGTILE,
			  XEVIOUS_NUM_OF_BGTILE_PALETTE_BITS,
			  8, 8,
			  xeviousOffsets.bgChars,
			  XeviousCharXOffsets,
			  XeviousCharYOffsets,
			  XEVIOUS_SIZE_OF_BGTILE_IN_BYTES,
			  tempRom,
			  graphics.bgTiles
			 );

	memset(tempRom, 0, machine.config->tempRomSize);

	return 0;
}

static INT32 xeviousSpriteLUTDec(void)
{
	for (INT32 i = 0; i < XEVIOUS_PALETTE_SIZE_SPRITES; i ++)
	{
		UINT8 code = ( (memory.PROM.spriteLookup[i                               ] & 0x0f)       |
				((memory.PROM.spriteLookup[XEVIOUS_PALETTE_SIZE_SPRITES + i] & 0x0f) << 4) );

		memory.PROM.spriteLookup[i] = (code & 0x80) ? (code & 0x7f) : 0x80;
	}

	return 0;
}


static INT32 xeviousSpriteDecode(void)
{
	/* sprite set #1 */
	/* 128 sprites */
	/* 3 bits per pixel */
	/* 16 x 16 sprites */
	/* every sprite takes 128 (64?) consecutive bytes */
	GfxDecode(
			  XEVIOUS_NUM_OF_SPRITE1,
			  XEVIOUS_NUM_OF_SPRITE_PALETTE_BITS,
			  16, 16,
			  xeviousOffsets.sprites1,
			  (INT32*)xOffsets16x16Tiles2Bit,
			  (INT32*)yOffsets16x16Tiles2Bit,
			  XEVIOUS_SIZE_OF_SPRITE_IN_BYTES,
			  tempRom + (0x0000),
			  graphics.sprites
			 );

	/* sprite set #2 */
	/* 128 sprites */
	/* 3 bits per pixel */
	/* 16 x 16 sprites */
	/* every sprite takes 128 (64?) consecutive bytes */
	GfxDecode(
			  XEVIOUS_NUM_OF_SPRITE2,
			  XEVIOUS_NUM_OF_SPRITE_PALETTE_BITS,
			  16, 16,
			  xeviousOffsets.sprites2,
			  (INT32*)xOffsets16x16Tiles2Bit,
			  (INT32*)yOffsets16x16Tiles2Bit,
			  XEVIOUS_SIZE_OF_SPRITE_IN_BYTES,
			  tempRom + (0x2000),
			  graphics.sprites + (XEVIOUS_NUM_OF_SPRITE1 * (16 * 16))
			 );

	/* sprite set #3 */
	/* 64 sprites */
	/* 3 bits per pixel (one is always 0) */
	/* 16 x 16 sprites */
	/* every sprite takes 64 consecutive bytes */
	GfxDecode(
			  XEVIOUS_NUM_OF_SPRITE3,
			  XEVIOUS_NUM_OF_SPRITE_PALETTE_BITS,
			  16, 16,
			  xeviousOffsets.sprites3,
			  (INT32*)xOffsets16x16Tiles2Bit,
			  (INT32*)yOffsets16x16Tiles2Bit,
			  XEVIOUS_SIZE_OF_SPRITE_IN_BYTES,
			  tempRom + (0x6000),
			  graphics.sprites + ((XEVIOUS_NUM_OF_SPRITE1 + XEVIOUS_NUM_OF_SPRITE2) * (16 * 16))
			 );

	return 0;
}

static tilemap_scan ( xevious )
{
	return (row) * XEVIOUS_NO_OF_COLS + col;
}

static tilemap_callback ( xevious_bg )
{
	UINT8 code = xeviousRAM.bg_videoram[offs];
	UINT8 attr = xeviousRAM.bg_colorram[offs];

	TILE_SET_INFO(
				  0,
				  (UINT16)(code + ((attr & 0x01) << 8)),
				  ((attr & 0x3c) >> 2) | ((code & 0x80) >> 3) | ((attr & 0x03) << 5),
				  ((attr & 0xc0) >> 6)
				 );
}

static tilemap_callback ( xevious_fg )
{
	UINT8 code = xeviousRAM.fg_videoram[offs];
	UINT8 attr = xeviousRAM.fg_colorram[offs];
	TILE_SET_INFO(
				  1,
				  code,
				  ((attr & 0x03) << 4) | ((attr & 0x3c) >> 2),
				  ((attr & 0xc0) >> 6)
				 );

}

static INT32 xeviousTilemapConfig(void)
{
	xeviousRAM.fg_colorram = memory.RAM.video;            // 0xb000 - 0xb7ff
	xeviousRAM.bg_colorram = memory.RAM.video + 0x0800;   // 0xb800 - 0xbfff
	xeviousRAM.fg_videoram = memory.RAM.video + 0x1000;   // 0xc000 - 0xc7ff
	xeviousRAM.bg_videoram = memory.RAM.video + 0x1800;   // 0xc800 - 0xcfff

	GenericTilemapInit(
					   0,
					   xevious_map_scan, xevious_bg_map_callback,
					   8, 8,
					   XEVIOUS_NO_OF_COLS, XEVIOUS_NO_OF_ROWS
					  );
	GenericTilemapSetGfx(
						 0,
						 graphics.bgTiles,
						 XEVIOUS_NUM_OF_BGTILE_PALETTE_BITS,
						 8, 8,
						 XEVIOUS_TILES_MEM_SIZE_IN_BYTES,
						 XEVIOUS_PALETTE_OFFSET_BGTILES,
						 0x7f //XEVIOUS_PALETTE_SIZE_BGTILES - 1
						);

	GenericTilemapInit(
					   1,
					   xevious_map_scan, xevious_fg_map_callback,
					   8, 8,
					   XEVIOUS_NO_OF_COLS, XEVIOUS_NO_OF_ROWS
					  );
	GenericTilemapSetGfx(
						 1,
						 graphics.fgChars,
						 XEVIOUS_NUM_OF_CHAR_PALETTE_BITS,
						 8, 8,
						 XEVIOUS_CHAR_MEM_SIZE_IN_BYTES,
						 XEVIOUS_PALETTE_OFFSET_CHARS,
						 0x3f // XEVIOUS_PALETTE_SIZE_CHARS - 1
						);
	GenericTilemapSetTransparent(1, 0);

	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, 0);

	return 0;
}

static UINT8 xeviousPlayFieldRead(UINT16 offset)
{
	UINT16 addr_2b = ( ((xeviousRAM.bs[1] & 0x7e) << 6) |
					  ((xeviousRAM.bs[0] & 0xfe) >> 1) );

	UINT8 dat_2b = xeviousROM.rom2b[addr_2b];

	UINT16 addr_2a = addr_2b >> 1;

	UINT8 dat_2a = xeviousROM.rom2a[addr_2a];
	if (addr_2b & 1)
	{
		dat_2a >>= 4;
	}
	else
	{
		dat_2a &=  0x0f;
	}

	UINT16 addr_2c = (UINT16)dat_2b << 2;
	if (dat_2a & 1)
	{
		addr_2c += 0x0400;
	}
	if ((xeviousRAM.bs[0] & 1) ^ ((dat_2a >> 2) & 1) )
	{
		addr_2c |= 1;
	}
	if ((xeviousRAM.bs[1] & 1) ^ ((dat_2a >> 1) & 1) )
	{
		addr_2c |= 2;
	}

	UINT8 dat_2c = 0;
	if (offset & 1)
	{
		dat_2c = xeviousROM.rom2c[addr_2c + 0x0800];
	}
	else
	{
		dat_2c = xeviousROM.rom2c[addr_2c];
		dat_2c = (dat_2c & 0x3f) | ((dat_2c & 0x80) >> 1) | ((dat_2c & 0x40) << 1);
		dat_2c ^= ((dat_2a << 4) & 0x40);
		dat_2c ^= ((dat_2a << 6) & 0x80);
	}

	return dat_2c;
}

static UINT8 xeviousWorkRAMRead(UINT16 offset)
{
	return xeviousRAM.workram[offset];
}

static UINT8 xeviousSharedRAM1Read(UINT16 offset)
{
	return memory.RAM.shared1[offset & 0x07ff];
}

static UINT8 xeviousSharedRAM2Read(UINT16 offset)
{
	return memory.RAM.shared2[offset & 0x07ff];
}

static UINT8 xeviousSharedRAM3Read(UINT16 offset)
{
	return memory.RAM.shared3[offset & 0x07ff];
}

static void xevious_bs_wr(UINT16 offset, UINT8 dta)
{
	xeviousRAM.bs[offset & 0x01] = dta;
}

static void xevious_vh_latch_w(UINT16 offset, UINT8 dta)
{
	UINT16 dta16 = dta + ((offset & 1) << 8);
	UINT16 reg = (offset & 0xf0) >> 4;

	switch (reg)
	{
		case 0:
			{
				// set bg tilemap x
				GenericTilemapSetScrollX(0, dta16 + 20);
				break;
			}
		case 1:
			{
				// set fg tilemap x
				GenericTilemapSetScrollX(1, dta16 + 32);
				break;
			}
		case 2:
			{
				// set bg tilemap y
				GenericTilemapSetScrollY(0, dta16 + 16);
				break;
			}
		case 3:
			{
				// set fg tilemap y
				GenericTilemapSetScrollY(1, dta16 + 18);
				break;
			}
		case 7:
			{
				// flipscreen
				machine.flipScreen = dta & 1;
				break;
			}
		default:
			{
				break;
			}
	}

}

static void xeviousBGColorRAMWrite(UINT16 offset, UINT8 dta)
{
	*(xeviousRAM.bg_colorram + (offset & 0x7ff)) = dta;
}

static void xeviousBGCharRAMWrite(UINT16 offset, UINT8 dta)
{
	*(xeviousRAM.bg_videoram + (offset & 0x7ff)) = dta;
}

static void xeviousFGColorRAMWrite(UINT16 offset, UINT8 dta)
{
	*(xeviousRAM.fg_colorram + (offset & 0x7ff)) = dta;
}

static void xeviousFGCharRAMWrite(UINT16 offset, UINT8 dta)
{
	*(xeviousRAM.fg_videoram + (offset & 0x7ff)) = dta;
}

static void xeviousWorkRAMWrite(UINT16 offset, UINT8 dta)
{
	xeviousRAM.workram[offset & 0x7ff] = dta;
}

static void xeviousSharedRAM1Write(UINT16 offset, UINT8 dta)
{
	memory.RAM.shared1[offset & 0x07ff] = dta;
}

static void xeviousSharedRAM2Write(UINT16 offset, UINT8 dta)
{
	memory.RAM.shared2[offset & 0x07ff] = dta;
}

static void xeviousSharedRAM3Write(UINT16 offset, UINT8 dta)
{
	memory.RAM.shared3[offset & 0x07ff] = dta;
}

#define XEVIOUS_BASE_PALETTE_SIZE   128

static void xeviousCalcPalette(void)
{
	UINT32 palette[XEVIOUS_BASE_PALETTE_SIZE + 1];
	UINT32 code = 0;

	for (INT32 i = 0; i < XEVIOUS_BASE_PALETTE_SIZE; i ++)
	{
		INT32 r = Colour4Bit[(memory.PROM.palette[0x0000 + i]) & 0x0f];
		INT32 g = Colour4Bit[(memory.PROM.palette[0x0100 + i]) & 0x0f];
		INT32 b = Colour4Bit[(memory.PROM.palette[0x0200 + i]) & 0x0f];

		palette[i] = BurnHighCol(r, g, b, 0);
	}

	palette[XEVIOUS_BASE_PALETTE_SIZE] = BurnHighCol(0, 0, 0, 0); // Transparency Colour for Sprites

	/* bg_select */
	for (INT32 i = 0; i < XEVIOUS_PALETTE_SIZE_BGTILES; i ++)
	{
		code = ( (memory.PROM.charLookup[                               i] & 0x0f)       |
				((memory.PROM.charLookup[XEVIOUS_PALETTE_SIZE_BGTILES + i] & 0x0f) << 4) );
		graphics.palette[XEVIOUS_PALETTE_OFFSET_BGTILES + i] = palette[code];
	}

	/* sprites */
	for (INT32 i = 0; i < XEVIOUS_PALETTE_SIZE_SPRITES; i ++)
	{
		graphics.palette[XEVIOUS_PALETTE_OFFSET_SPRITE + i] = palette[memory.PROM.spriteLookup[i]];

#if 0
		code = ( (memory.PROM.spriteLookup[i                               ] & 0x0f)       |
				((memory.PROM.spriteLookup[XEVIOUS_PALETTE_SIZE_SPRITES + i] & 0x0f) << 4) );

		spriteLookup_converted[i] = (code & 0x80) ? code & 0x7f : 0x80;

		if (code & 0x80)
			graphics.palette[XEVIOUS_PALETTE_OFFSET_SPRITE + i] = palette[code & 0x7f];
		else
			graphics.palette[XEVIOUS_PALETTE_OFFSET_SPRITE + i] = palette[XEVIOUS_BASE_PALETTE_SIZE];
#endif
	}

	/* characters - direct mapping */
	for (INT32 i = 0; i < XEVIOUS_PALETTE_SIZE_CHARS; i += 2)
	{
		graphics.palette[XEVIOUS_PALETTE_OFFSET_CHARS + i + 0] = palette[XEVIOUS_BASE_PALETTE_SIZE];
		graphics.palette[XEVIOUS_PALETTE_OFFSET_CHARS + i + 1] = palette[i / 2];
	}

}

static void xeviousRenderTiles0(void)
{
	GenericTilemapSetEnable(0, 1);
	GenericTilemapDraw(0, pTransDraw, 0 | TMAP_DRAWOPAQUE);
}

static void xeviousRenderTiles1(void)
{
	GenericTilemapSetEnable(1, 1);
	GenericTilemapDraw(1, pTransDraw, 0 | TMAP_TRANSPARENT);
}

static UINT32 xeviousGetSpriteParams(struct Namco_Sprite_Params *spriteParams, UINT32 offset)
{
	UINT8 *spriteRam2 = memory.RAM.shared1 + 0x780;
	UINT8 *spriteRam3 = memory.RAM.shared2 + 0x780;
	UINT8 *spriteRam1 = memory.RAM.shared3 + 0x780;

	spriteParams->transparent_mask = 0x80;

	if (0 == (spriteRam1[offset + 1] & 0x40))
	{
		INT32 sprite =      spriteRam1[offset + 0];

		if (spriteRam3[offset + 0] & 0x80)
		{
			sprite &= 0x3f;
			sprite += 0x100;
		}
		spriteParams->sprite = sprite;
		spriteParams->colour = spriteRam1[offset + 1] & 0x7f;

		spriteParams->xStart = ((spriteRam2[offset + 1] - 40) + (spriteRam3[offset + 1] & 1 ) * 0x100);
		spriteParams->yStart = NAMCO_SCREEN_WIDTH - (spriteRam2[offset + 0] - 1);
		spriteParams->xStep = 16;
		spriteParams->yStep = 16;

		spriteParams->flags = ((spriteRam3[offset + 0] & 0x03) << 2) |
			((spriteRam3[offset + 0] & 0x0c) >> 2);

		if (spriteParams->flags & ySize)
		{
			spriteParams->yStart -= 16;
		}

		spriteParams->paletteBits = XEVIOUS_NUM_OF_SPRITE_PALETTE_BITS;
		spriteParams->paletteOffset = XEVIOUS_PALETTE_OFFSET_SPRITE;

		return 1;
	}

	return 0;
}

struct BurnDriver BurnDrvXevious = {
	"xevious", NULL, NULL, "xevious", "1982",
	"Xevious (Namco)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, XeviousRomInfo, XeviousRomName, NULL, NULL, XeviousSampleInfo, XeviousSampleName, XeviousInputInfo, XeviousDIPInfo,
	xeviousInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	XEVIOUS_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvXeviousa = {
	"xeviousa", "xevious", NULL, "xevious", "1982",
	"Xevious (Atari, harder)\0", NULL, "Namco (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, XeviousaRomInfo, XeviousaRomName, NULL, NULL, XeviousSampleInfo, XeviousSampleName, XeviousInputInfo, XeviousaDIPInfo,
	xeviousaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	XEVIOUS_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvXeviousb = {
	"xeviousb", "xevious", NULL, "xevious", "1982",
	"Xevious (Atari)\0", NULL, "Namco (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, XeviousbRomInfo, XeviousbRomName, NULL, NULL, XeviousSampleInfo, XeviousSampleName, XeviousInputInfo, XeviousbDIPInfo,
	xeviousaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	XEVIOUS_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvXeviousc = {
	"xeviousc", "xevious", NULL, "xevious", "1982",
	"Xevious (Atari, Namco PCB)\0", NULL, "Namco (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, XeviouscRomInfo, XeviouscRomName, NULL, NULL, XeviousSampleInfo, XeviousSampleName, XeviousInputInfo, XeviousaDIPInfo,
	xeviousInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	XEVIOUS_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvSxevious = {
	"sxevious", NULL, NULL, "xevious", "1984",
	"Super Xevious\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, SxeviousRomInfo, SxeviousRomName, NULL, NULL, XeviousSampleInfo, XeviousSampleName, XeviousInputInfo, SxeviousDIPInfo,
	xeviousInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	XEVIOUS_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};

struct BurnDriver BurnDrvSxeviousj = {
	"sxeviousj", "sxevious", NULL, "xevious", "1984",
	"Super Xevious (Japan)\0", NULL, "Namco", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, SxeviousjRomInfo, SxeviousjRomName, NULL, NULL, XeviousSampleInfo, XeviousSampleName, XeviousInputInfo, SxeviousDIPInfo,
	xeviousInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	XEVIOUS_PALETTE_SIZE, NAMCO_SCREEN_WIDTH, NAMCO_SCREEN_HEIGHT, 3, 4
};
