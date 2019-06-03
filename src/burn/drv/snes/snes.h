#pragma once

#include "tiles_generic.h"
#include "bitswap.h"

/* Useful definitions */
#define SNES_SCR_WIDTH        256		/* 32 characters 8 pixels wide */
#define SNES_SCR_HEIGHT_NTSC  224		/* Can be 224 or 240 height */
#define SNES_SCR_HEIGHT_PAL   274		/* ??? */
#define SNES_VTOTAL_NTSC      262		/* Maximum number of lines for NTSC systems */
#define SNES_VTOTAL_PAL       312		/* Maximum number of lines for PAL systems */
#define SNES_HTOTAL           341		/* Maximum number pixels per line (incl. hblank) */
#define SNES_DMA_BASE         0x4300	/* Base DMA register address */
#define SNES_MODE_20          0x1		/* Lo-ROM cart */
#define SNES_MODE_21          0x2		/* Hi-ROM cart */
#define SNES_MODE_22          0x4		/* Extended Lo-ROM cart - SDD-1 */
#define SNES_MODE_25          0x8		/* Extended Hi-ROM cart */
#define SNES_NTSC             0x00
#define SNES_PAL              0x10
#define SNES_VRAM_SIZE        0x20000	/* 128kb of video ram */
#define SNES_CGRAM_SIZE       0x202		/* 256 16-bit colours + 1 tacked on 16-bit colour for fixed colour */
#define SNES_OAM_SIZE         0x440		/* 1088 bytes of Object Attribute Memory */
#define SNES_SPCRAM_SIZE      0x10000	/* 64kb of spc700 ram */
#define SNES_EXROM_START      0x1000000
#define FIXED_COLOUR          256		/* Position in cgram for fixed colour */
/* Definitions for PPU Memory-Mapped registers */
#define INIDISP        0x2100
#define OBSEL          0x2101
#define OAMADDL        0x2102
#define OAMADDH        0x2103
#define OAMDATA        0x2104
#define BGMODE         0x2105	/* abcdefff = abcd: bg4-1 tile size | e: BG3 high priority | f: mode */
#define MOSAIC         0x2106	/* xxxxabcd = x: pixel size | abcd: affects bg 1-4 */
#define BG1SC          0x2107
#define BG2SC          0x2108
#define BG3SC          0x2109
#define BG4SC          0x210A
#define BG12NBA        0x210B
#define BG34NBA        0x210C
#define BG1HOFS        0x210D
#define BG1VOFS        0x210E
#define BG2HOFS        0x210F
#define BG2VOFS        0x2110
#define BG3HOFS        0x2111
#define BG3VOFS        0x2112
#define BG4HOFS        0x2113
#define BG4VOFS        0x2114
#define VMAIN          0x2115	/* i---ffrr = i: Increment timing | f: Full graphic | r: increment rate */
#define VMADDL         0x2116	/* aaaaaaaa = a: LSB of vram address */
#define VMADDH         0x2117	/* aaaaaaaa = a: MSB of vram address */
#define VMDATAL        0x2118	/* dddddddd = d: data to be written */
#define VMDATAH        0x2119	/* dddddddd = d: data to be written */
#define M7SEL          0x211A	/* ab----yx = a: screen over | y: vertical flip | x: horizontal flip */
#define M7A            0x211B	/* aaaaaaaa = a: COSINE rotate angle / X expansion */
#define M7B            0x211C	/* aaaaaaaa = a: SINE rotate angle / X expansion */
#define M7C            0x211D	/* aaaaaaaa = a: SINE rotate angle / Y expansion */
#define M7D            0x211E	/* aaaaaaaa = a: COSINE rotate angle / Y expansion */
#define M7X            0x211F
#define M7Y            0x2120
#define CGADD          0x2121
#define CGDATA         0x2122
#define W12SEL         0x2123
#define W34SEL         0x2124
#define WOBJSEL        0x2125
#define WH0            0x2126	/* pppppppp = p: Left position of window 1 */
#define WH1            0x2127	/* pppppppp = p: Right position of window 1 */
#define WH2            0x2128	/* pppppppp = p: Left position of window 2 */
#define WH3            0x2129	/* pppppppp = p: Right position of window 2 */
#define WBGLOG         0x212A	/* aabbccdd = a: BG4 params | b: BG3 params | c: BG2 params | d: BG1 params */
#define WOBJLOG        0x212B	/* ----ccoo = c: Colour window params | o: Object window params */
#define TM             0x212C
#define TS             0x212D
#define TMW            0x212E
#define TSW            0x212F
#define CGWSEL         0x2130
#define CGADSUB        0x2131
#define COLDATA        0x2132
#define SETINI         0x2133
#define MPYL           0x2134
#define MPYM           0x2135
#define MPYH           0x2136
#define SLHV           0x2137
#define ROAMDATA       0x2138
#define RVMDATAL       0x2139
#define RVMDATAH       0x213A
#define RCGDATA        0x213B
#define OPHCT          0x213C
#define OPVCT          0x213D
#define STAT77         0x213E
#define STAT78         0x213F
#define APU00          0x2140
#define APU01          0x2141
#define APU02          0x2142
#define APU03          0x2143
#define WMDATA         0x2180
#define WMADDL         0x2181
#define WMADDM         0x2182
#define WMADDH         0x2183
/* Definitions for CPU Memory-Mapped registers */

extern UINT16 snes_cgram[SNES_CGRAM_SIZE];
extern UINT32 snesPal[0x20000];
/*SPC700*/
extern double spccycles;
extern double spctotal2;
extern double spctotal3;
void execspc();

extern int intthisline;
extern int framenum;
extern int oldnmi;


static inline void clockspc(int cyc)
{
	spccycles += cyc;
	if (spccycles > 0) execspc();
}

/*65816*/
/*Registers*/
typedef union
{
	UINT16 w;
	struct
	{
#ifndef BIG_ENDIAN
		UINT8 l, h;
#else
		UINT8 h, l;
#endif
	} b;
} reg;



struct CPU_65816
{
	INT16 b; // the break flag
	INT16 e; // the emulation mode flag

	/*CPU modes : 0 = X1M1
1 = X1M0
2 = X0M1
3 = X0M0
4 = emulation*/
	INT32 cpumode;
	INT8 inwai; // Set when WAI is called

	reg regA; // The Accumulator(16 bits wide)
	reg regX; // The X index register (16 bits wide)
	reg regY; // The Y index register (16 bits wide)
	reg regS; // The Stack pointer(16 bits wide)
	reg regP; // The Processor Status

	UINT32 pbr; // program banK register (8 bits wide) 
	UINT32 dbr; //Data Bank Register(8 bits wide)
	UINT16 pc;  // program counter
	UINT16 dp; // Direct register

	/*Global cycles count*/
	INT32 cycles;

	/*Temporary variables*/
	UINT32 tempAddr;


};

extern CPU_65816 snes_cpu;

#define REG_AL() (snes_cpu.regA.b.l)
#define REG_AH() (snes_cpu.regA.b.h)
#define REG_AW() (snes_cpu.regA.w)

#define REG_XL() (snes_cpu.regX.b.l)
#define REG_XH() (snes_cpu.regX.b.h)
#define REG_XW() (snes_cpu.regX.w)

#define REG_YL() (snes_cpu.regY.b.l)
#define REG_YH() (snes_cpu.regY.b.h)
#define REG_YW() (snes_cpu.regY.w)

#define REG_SL() (snes_cpu.regS.b.l)
#define REG_SH() (snes_cpu.regS.b.h)
#define REG_SW() (snes_cpu.regS.w)

#define REG_PL() (snes_cpu.regP.b.l)
#define REG_PH() (snes_cpu.regP.b.h)
#define REG_PW() (snes_cpu.regP.w)

#define CARRY_FLAG   0x01
#define ZERO_FLAG	 0x02
#define IRQ_FLAG     0x04
#define DECIMAL_FLAG 0x08
#define INDEX_FLAG   0x10
#define MEMORY_FLAG  0x20
#define OVERFLOW_FLAG 0x40
#define NEGATIVE_FLAG 0x80


#define CLEAR_CARRY() (snes_cpu.regP.b.l &= ~CARRY_FLAG)
#define SET_CARRY() (snes_cpu.regP.b.l |= CARRY_FLAG)
#define CHECK_CARRY() (snes_cpu.regP.b.l & CARRY_FLAG)

#define CLEAR_ZERO() (snes_cpu.regP.b.l &= ~ZERO_FLAG)
#define SET_ZERO() (snes_cpu.regP.b.l |= ZERO_FLAG)
#define CHECK_ZERO() (snes_cpu.regP.b.l & ZERO_FLAG)

#define CLEAR_IRQ() (snes_cpu.regP.b.l &= ~IRQ_FLAG)
#define SET_IRQ() (snes_cpu.regP.b.l |= IRQ_FLAG)
#define CHECK_IRQ() (snes_cpu.regP.b.l & IRQ_FLAG)

#define CLEAR_DECIMAL() (snes_cpu.regP.b.l &= ~DECIMAL_FLAG)
#define SET_DECIMAL() (snes_cpu.regP.b.l |= DECIMAL_FLAG)
#define CHECK_DECIMAL() (snes_cpu.regP.b.l & DECIMAL_FLAG)

#define CLEAR_INDEX() (snes_cpu.regP.b.l &= ~INDEX_FLAG)
#define SET_INDEX() (snes_cpu.regP.b.l |= INDEX_FLAG)
#define CHECK_INDEX() (snes_cpu.regP.b.l & INDEX_FLAG)

#define CLEAR_OVERFLOW() (snes_cpu.regP.b.l &= ~OVERFLOW_FLAG)
#define SET_OVERFLOW() (snes_cpu.regP.b.l |= OVERFLOW_FLAG)
#define CHECK_OVERFLOW() (snes_cpu.regP.b.l & OVERFLOW_FLAG)

#define CLEAR_MEMORYACC() (snes_cpu.regP.b.l &= ~MEMORY_FLAG)
#define SET_MEMORYACC() (snes_cpu.regP.b.l |= MEMORY_FLAG)
#define CHECK_MEMORYACC() (snes_cpu.regP.b.l & MEMORY_FLAG)

#define CLEAR_NEGATIVE() (snes_cpu.regP.b.l &= ~NEGATIVE_FLAG)
#define SET_NEGATIVE() (snes_cpu.regP.b.l |= NEGATIVE_FLAG)
#define CHECK_NEGATIVE() (snes_cpu.regP.b.l & NEGATIVE_FLAG)


extern void (*opcodes[256][5])();
// cpu stuff
void irq65816();
void nmi65816();
void reset65816();
void makeopcodetable();

//DSP

void polldsp();
void writedsp(UINT16 a, UINT8 v);
void resetdsp();
UINT8 readdsp(UINT16 a);

/*Memory*/
extern UINT8* SNES_ram;
extern UINT8* SNES_rom;
extern UINT8* memlookup[2048];
extern UINT8* memread;
extern UINT8* memwrite;
extern UINT8* accessspeed;

extern INT32 lorom;


UINT8 snes_readmem(UINT32 adress);
void snes_writemem(UINT32 ad, UINT8 v);

#define readmemw(a) (snes_readmem(a))|((snes_readmem((a)+1))<<8)
#define writememw(a,v)  snes_writemem(a,(v)&0xFF); snes_writemem((a)+1,(v)>>8)
#define writememw2(a,v) snes_writemem((a)+1,(v)>>8); snes_writemem(a,(v)&0xFF)

/*Video*/
extern INT32 nmi, vbl, joyscan;
extern INT32 nmienable;

extern INT32 ppumask;

extern INT32 yirq, xirq, irqenable, irq;
extern INT32 lines;



extern int global_pal;

/*DMA registers*/
extern UINT16 dmadest[8], dmasrc[8], dmalen[8];
extern UINT32 hdmaaddr[8], hdmaaddr2[8];
extern UINT8 dmabank[8], dmaibank[8], dmactrl[8], hdmastat[8], hdmadat[8];
extern int hdmacount[8];
extern UINT8 hdmaena;


// ppu stuff
void resetppu();
void initppu();
void initspc();
void exitspc();

UINT8 readppu(UINT16 addr);
void writeppu(UINT16 addr, UINT8 val);
void drawline(int line);

// io stuff
void readjoy();
UINT8 readio(UINT16 addr);
void writeio(UINT16 addr, UINT8 val);
void writejoyold(UINT16 addr, UINT8 val);
UINT8 readjoyold(UINT16 addr);
// spc stuff
void resetspc();
UINT8 readfromspc(UINT16 addr);
void writetospc(UINT16 addr, UINT8 val);
extern UINT8 voiceon;

//snes.cpp stuff



//mem stuff
void snes_mapmem();

extern UINT16 srammask;
extern UINT8* SNES_sram;
extern INT32 spctotal;
extern UINT8* spcram;
// snes_main.cpp
INT32 SnesInit();
INT32 SnesExit();
INT32 SnesFrame();
INT32 SnesScan(INT32 nAction, INT32* pnMin);
extern UINT8 DoSnesReset;

extern UINT8 SnesJoy1[12];
void pollsound();
