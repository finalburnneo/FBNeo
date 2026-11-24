// FB Neo SI Electronics System Board Y2 driver module
// Based on MAME driver by Peter Wilhelmsen, R. Belmont, David Haywood

// The two NAND dumps (with their spare areas) are disks, nand_u101 and nand_u102 in <hdd path>/<romset>/:
// MAME's dumps renamed to .img, or CHDs made with
//   chdman createraw -i nand08gw3b2cn6.u101 -o nand_u101.chd -us 4224 -hs 16896
// They are read and decrypted on demand, one 4KB page (two NAND pages) at a time.
//
// The power-on boot is unidentified (as in MAME): the NAND loader is copied to RAM and run directly.
//
// TODO: JVS I/O (no device answers on the bridge, as in MAME: use the JAMMA setting)

#include <vector>
#include "tiles_generic.h"
#include "mips3_intf.h"
#include "burn_hdd.h"
#include "rpp324.h"
#define THREADY_NO_GLOBAL
#include "thready.h"
#include <math.h>

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvSndROM;
static UINT8 *DrvRAM;
static UINT8 *DrvBackup;
static UINT8 *DrvJvsRAM;
static UINT8 *DrvRamWritten;
static UINT8 *TrackMap;
static UINT32 *LoggedGen;
static UINT8 *DrvBoot;
static UINT32 *DrvGfxList;

static UINT8 DrvRecalc;

// host colors of the 16-bit scanout modes (0: ARGB1555, 1: RGB565), rebuilt when the frontend's format changes
static UINT32 *DrawLut = NULL;
static INT32 nDrawLutBpp = 0;
static UINT8 DrvReset;
static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[1];
static UINT16 DrvInputs[3];
static UINT8 DrvCoinPrev[2];

static INT32 nSndROMLen;

#define RAM_SIZE		0x10000000
#define BACKUP_SIZE		0x19000
#define GFX_LIST_WORDS	0x40000		// longest decoded GE stream

#define CPU_CLOCK		266666666
#define PIXEL_CLOCK		25011000	// unverified: gives 60Hz with the totals programmed by YG_Reset
#define H_TOTAL			794
#define V_TOTAL			525

static const double CyclesPerLine = (double)CPU_CLOCK * H_TOTAL / PIXEL_CLOCK;

// device registers
static UINT32 voc[0x1c4 / 4];
static UINT32 rdc[0x1100 / 4];
static UINT32 *RdcR = rdc;		// the registers the renderer uses: a copy while a list renders in the background
static UINT32 gec[0x80 / 4];
static UINT32 int_select[5];
static UINT32 irq_sources;
static UINT32 dma[0x80 / 4];
static UINT32 jvs_status[2];
static UINT32 jvs_config[2];
static UINT32 gfx_base[2];
static UINT32 gfx_words[2];
static UINT8 gfx_slot;
static UINT32 mvc[0x60 / 4];
static UINT32 mvc_sram[0x940 / 4];
static UINT8 mvc_command_ready[3];
static UINT8 mvc_block_ready[18];
static UINT8 mvc_command_pos;
static UINT8 mvc_block_pos;
static UINT8 mvc_retry;		// a register changed: a blocked command may proceed on the next SRAM write
static UINT32 yuv[0x200 / 4];
static UINT32 timer[2][4];		// timers 6 and 7: count, compare, control, status
static UINT64 timer_epoch[2];
static UINT16 rtc[0x20];
static UINT8 coin_pending[2];
static UINT16 coin_control;
static UINT32 scc_sys_data;
static UINT32 ioc[4];

// events, in cpu cycles (Mips3TotalCycles)
#define NEVER			(~(UINT64)0)
static INT32 vint_line;
static INT32 vint_event;
static UINT64 timer7_fire;
static UINT64 jvs_fire[2];

// scanout state, latched at the start of vblank (MAME draws the screen there)
#define V_VISIBLE		480
enum { DISP_CONTROL = 0, DISP_BACK, DISP_ENABLE, DISP_BASE, DISP_SIZE, DISP_REGS };
static UINT32 display[DISP_REGS];

// position of the beam, for the VOC counter
static INT32 nCurrentLine;
static UINT64 nLineStart;

// NAND: decryption and decrypted page cache
static BurnHDD *Nand[2];
static UINT16 *NandDataLut;
static UINT16 *NandKeyLut;
static UINT16 NandMasks[4][0x800];
static UINT16 nNandSeed;
static const UINT8 *pNandDataBits;
static const UINT8 *pNandKeyBits;

#define NAND_RECORD		0x1080		// two 0x840-byte pages: data + spare
#define NAND_RECORDS	0x40000
#define NAND_CACHE		512			// decrypted 4KB pages, direct mapped

struct NandCacheLine {
	INT32 tag;
	UINT8 data[0x1000];
};
static NandCacheLine *NandCache;

static double MvcBasis[8][8];

#if defined (FBNEO_DEBUG)
// activity per second, to follow the boot while the renderer is missing
static UINT32 nStatGe, nStatLists, nStatTriangles, nStatClears, nStatDma, nStatMvc, nStatYuv, nStatSnd, nStatFrames, nStatFlips, nStatEvent[2];
#define Y2_STAT(x)	(x)++
// every 600 frames, the next 8 are traced: lists, flips, vblank events and what is latched for display
static UINT32 nTraceFrame;
#define Y2_TRACING	((nTraceFrame % 600) < 8)

// previous rendered list, to count what changed from one frame to the next
static UINT32 *TracePrevList = NULL;
static UINT32 nTracePrevWords = 0;

static void TraceListDiff(const UINT32 *list, UINT32 words)
{
	if (TracePrevList == NULL) TracePrevList = (UINT32*)calloc(GFX_LIST_WORDS, sizeof(UINT32));
	if (TracePrevList == NULL) return;

	UINT32 common = (words < nTracePrevWords) ? words : nTracePrevWords;
	UINT32 diff = 0, first[6], nfirst = 0;
	for (UINT32 i = 0; i < common; i++) {
		if (list[i] != TracePrevList[i]) {
			if (nfirst < 6) first[nfirst++] = i;
			diff++;
		}
	}
	bprintf(PRINT_NORMAL, _T("Y2T %d line %3d: list differs from the previous one in %d of %d words (sizes %d/%d), first at"),
		nTraceFrame, nCurrentLine, diff, common, words, nTracePrevWords);
	for (UINT32 i = 0; i < nfirst; i++) bprintf(PRINT_NORMAL, _T(" %d:%08x->%08x"), first[i], TracePrevList[first[i]], list[first[i]]);
	bprintf(PRINT_NORMAL, _T("\n"));

	memcpy(TracePrevList, list, words * sizeof(UINT32));
	nTracePrevWords = words;
}

static UINT32 ListHash(const UINT32 *list, UINT32 words)
{
	UINT32 hash = 2166136261U;
	for (UINT32 i = 0; i < words; i++) hash = (hash ^ list[i]) * 16777619U;
	return hash;
}
// wall time spent per second, in microseconds (process cpu time would mix the threads up)
#if defined (_WIN32)
static INT64 Y2Now()
{
	LARGE_INTEGER count, freq;
	QueryPerformanceCounter(&count);
	QueryPerformanceFrequency(&freq);
	return (INT64)(count.QuadPart * 1000000.0 / freq.QuadPart);
}
#else
#include <sys/time.h>
static INT64 Y2Now()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (INT64)tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif
static INT64 nTimeCpu, nTimeRender, nTimeMvc, nTimeYuv, nTimeFrame, nTimeWait;
#define Y2_TIME_START()	INT64 y2_time_start = Y2Now()
#define Y2_TIME_END(x)	(x) += Y2Now() - y2_time_start
#else
#define Y2_STAT(x)		do { } while (0)
#define Y2_TIME_START()	do { } while (0)
#define Y2_TIME_END(x)	do { } while (0)
#endif

// ---------------------------------------------------------------------------
// inputs

static struct BurnInputInfo Y2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 13,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 12,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 11,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 6"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 8,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 15,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 13,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 12,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 11,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 4"	},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 5"	},
	{"P2 Button 6",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 6"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 14,	"service2"	},
	{"Test",			BIT_DIGITAL,	DrvJoy2 + 0,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Y2)

static struct BurnDIPInfo Y2DIPList[] =
{
	DIP_OFFSET(0x1c)
	{0x00, 0xff, 0xff, 0x3f, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x01, 0x01, "Off"				},
	{0x00, 0x01, 0x01, 0x00, "On"				},

#if 0
	// no JVS I/O board is emulated: the games hang on a black screen with JVS
	{0   , 0xfe, 0   ,    2, "Cabinet Interface"},
	{0x00, 0x01, 0x02, 0x02, "JAMMA"			},
	{0x00, 0x01, 0x02, 0x00, "JVS"				},
#endif

	{0   , 0xfe, 0   ,    2, "VGA Sync"			},
	{0x00, 0x01, 0x04, 0x04, "Separate"			},
	{0x00, 0x01, 0x04, 0x00, "Composite"		},

	{0   , 0xfe, 0   ,    2, "Monitor"			},
	{0x00, 0x01, 0x08, 0x08, "31 kHz"			},
	{0x00, 0x01, 0x08, 0x00, "15 kHz"			},

	{0   , 0xfe, 0   ,    2, "JAMMA Harness"	},
	{0x00, 0x01, 0x10, 0x10, "Standard"			},
	{0x00, 0x01, 0x10, 0x00, "Mahjong"			},
};

STDDIPINFO(Y2)

// ---------------------------------------------------------------------------
// guest RAM (little-endian)

static inline UINT32 Ram32(UINT32 a)
{
	return BURN_ENDIAN_SWAP_INT32(*(UINT32*)(DrvRAM + (a & ~3)));
}

static inline UINT32 Ram16(UINT32 a)
{
	return BURN_ENDIAN_SWAP_INT16(*(UINT16*)(DrvRAM + (a & ~1)));
}

static inline void RamW16(UINT32 a, UINT32 d)
{
	*(UINT16*)(DrvRAM + (a & ~1)) = BURN_ENDIAN_SWAP_INT16((UINT16)d);
}

static inline void RamW32(UINT32 a, UINT32 d)
{
	*(UINT32*)(DrvRAM + (a & ~3)) = BURN_ENDIAN_SWAP_INT32(d);
}

static inline UINT32 RamRead8(UINT32 a)
{
	return (a < RAM_SIZE) ? DrvRAM[a] : 0;
}

static inline void RamW8(UINT32 a, UINT8 d)
{
	DrvRAM[a] = d;
}

// one byte per 4KB page of RAM written since reset, by the cpu or the devices: the other pages are
// still zero and stay out of savestates
//
// single instance runahead (and preemptive frames) loads a state it saved a few frames before: after
// each save, a page is copied the first time it is written, and the load puts back the copies made
// since the save it was given (such states hold no RAM)
#define CHECKPOINTS		32

struct Checkpoint {
	UINT32 id;
	UINT32 n, max;
	UINT32 *page;
	UINT8 *written;		// DrvRamWritten of the page before the copy
	UINT8 *data;
};

static Checkpoint Checkpoints[CHECKPOINTS];
static UINT32 nCheckpointId;		// id of the last save, 0: none
static UINT32 nCheckpointFirst;		// oldest id still held
static INT32 nCheckpointAge;

static void PageTouch(UINT32 page)
{
	if (nCheckpointId && LoggedGen[page] != nCheckpointId) {
		Checkpoint *c = &Checkpoints[nCheckpointId % CHECKPOINTS];
		if (c->n == c->max) {
			c->max = c->max ? c->max * 2 : 256;
			c->page = (UINT32*)realloc(c->page, c->max * sizeof(UINT32));
			c->written = (UINT8*)realloc(c->written, c->max);
			c->data = (UINT8*)realloc(c->data, (size_t)c->max << 12);
		}
		c->page[c->n] = page;
		c->written[c->n] = DrvRamWritten[page];
		memcpy(c->data + ((size_t)c->n << 12), DrvRAM + (page << 12), 0x1000);
		c->n++;
		LoggedGen[page] = nCheckpointId;
	}
	DrvRamWritten[page] = 1;
}

static UINT32 WrittenMemo[4][2];	// last ranges marked (start, bytes): lists mark the same targets over and over
static INT32 nWrittenMemo;

// before any write to RAM that is not done by the cpu
static void RamWritten(UINT32 start, UINT64 bytes)
{
	if (bytes == 0 || start >= RAM_SIZE) return;
	if (bytes > RAM_SIZE - start) bytes = RAM_SIZE - start;
	for (INT32 i = 0; i < 4; i++) {
		if (WrittenMemo[i][0] == start && WrittenMemo[i][1] == (UINT32)bytes) return;
	}

	const UINT32 last = (UINT32)((start + bytes - 1) >> 12);
	for (UINT32 page = start >> 12; page <= last; page++) {
		if (!TrackMap[page]) { PageTouch(page); TrackMap[page] = 1; }
	}

	WrittenMemo[nWrittenMemo][0] = start;
	WrittenMemo[nWrittenMemo][1] = (UINT32)bytes;
	nWrittenMemo = (nWrittenMemo + 1) & 3;
}

// after bytes of TrackMap were cleared
static void RamWrittenRestart()
{
	memset(WrittenMemo, 0, sizeof(WrittenMemo));
	Mips3SetWriteTrack(TrackMap, RAM_SIZE, PageTouch);
}

static void CheckpointDrop()
{
	for (INT32 i = 0; i < CHECKPOINTS; i++) Checkpoints[i].n = 0;
	nCheckpointId = nCheckpointFirst = 0;
	memset(LoggedGen, 0, (RAM_SIZE >> 12) * sizeof(UINT32));
	memcpy(TrackMap, DrvRamWritten, RAM_SIZE >> 12);
}

static UINT32 nCheckpointNext = 1;	// ids keep growing, so stale LoggedGen entries never match

static UINT32 CheckpointSet()
{
	const UINT32 id = nCheckpointNext++;
	if (nCheckpointId == 0) nCheckpointFirst = id;
	if (id - nCheckpointFirst >= CHECKPOINTS) nCheckpointFirst = id - CHECKPOINTS + 1;

	Checkpoint *c = &Checkpoints[id % CHECKPOINTS];
	c->id = id;
	c->n = 0;
	nCheckpointId = id;
	nCheckpointAge = 0;

	memset(TrackMap, 0, RAM_SIZE >> 12);
	RamWrittenRestart();
	return id;
}

static void CheckpointRestore(UINT32 id)
{
	if (nCheckpointId == 0 || id < nCheckpointFirst || id > nCheckpointId) {
		bprintf(PRINT_ERROR, _T("Y2: runahead state too old, RAM left as is!\n"));
		return;
	}

	// newest copies first: the oldest ones, from the save asked for, are what stays
	for (UINT32 g = nCheckpointId; g >= id; g--) {
		Checkpoint *c = &Checkpoints[g % CHECKPOINTS];
		for (UINT32 i = 0; i < c->n; i++) {
			const UINT32 page = c->page[i];
			memcpy(DrvRAM + (page << 12), c->data + ((size_t)i << 12), 0x1000);
			DrvRamWritten[page] = c->written[i];
			TrackMap[page] = 0;
			Mips3InvalidateRange(page << 12, (page << 12) | 0xfff);
		}
		if (g > id) c->n = 0;
	}

	// later saves are gone, and the copies of this one stay valid
	nCheckpointId = id;
	nCheckpointAge = 0;
	RamWrittenRestart();
}

static void CheckpointExit()
{
	for (INT32 i = 0; i < CHECKPOINTS; i++) {
		free(Checkpoints[i].page);
		free(Checkpoints[i].written);
		free(Checkpoints[i].data);
	}
	memset(Checkpoints, 0, sizeof(Checkpoints));
	nCheckpointId = nCheckpointFirst = 0;
}

// savestates must keep the same size for a whole session (runahead, rewind, netplay): main RAM goes
// in a fixed number of page slots per game, the map of the stored pages first, the unused slots last
static UINT32 nStatePages;
static UINT8 *StoredMap;
static UINT8 *StatePadding;
static INT32 bStateOverflow;

#define STATE_PADDING	0x100000

static void ScanMainRamInit(UINT32 megabytes)
{
	nStatePages = megabytes << 8;
	StoredMap = (UINT8*)calloc(RAM_SIZE >> 12, 1);
	StatePadding = (UINT8*)calloc(STATE_PADDING, 1);
	bStateOverflow = 0;
}

static void ScanMainRamExit()
{
	free(StoredMap);
	StoredMap = NULL;
	free(StatePadding);
	StatePadding = NULL;
}

static UINT32 CountWritten()
{
	UINT32 n = 0;
	for (UINT32 page = 0; page < (RAM_SIZE >> 12); page++) n += DrvRamWritten[page];
	return n;
}

static void ScanMainRam(INT32 nAction)
{
	if (nAction & ACB_RUNAHEAD) {
		UINT32 id = 0;
		if (nAction & ACB_READ) id = CheckpointSet();
		SCAN_VAR(id);
		if (nAction & ACB_WRITE) CheckpointRestore(id);
		return;
	}

	struct BurnArea ba;
	memset(&ba, 0, sizeof(ba));
	const UINT32 pages = RAM_SIZE >> 12;

	if (nAction & ACB_READ) {
		UINT32 count = CountWritten();
		if (count > nStatePages) {
			// pages written back to zero need no slot
			for (UINT32 page = 0; page < pages; page++) {
				if (!DrvRamWritten[page]) continue;
				const UINT64 *q = (const UINT64*)(DrvRAM + (page << 12));
				INT32 k = 0;
				while (k < 512 && q[k] == 0) k++;
				if (k == 512) DrvRamWritten[page] = TrackMap[page] = 0;
			}
			RamWrittenRestart();
			count = CountWritten();
		}
		if (count > nStatePages && !bStateOverflow) {
			bprintf(PRINT_ERROR, _T("Y2: %d KB of RAM in use, savestates only hold %d KB: they will be incomplete!\n"), count * 4, nStatePages * 4);
			bStateOverflow = 1;
		}

		UINT32 slots = 0;
		for (UINT32 page = 0; page < pages; page++) {
			StoredMap[page] = (DrvRamWritten[page] && slots < nStatePages) ? 1 : 0;
			slots += StoredMap[page];
		}
	}

	ba.Data	  = StoredMap;
	ba.nLen	  = pages;
	ba.szName = "Main RAM map";
	BurnAcb(&ba);

	UINT32 slots = 0;
	for (UINT32 page = 0; page < pages; ) {
		if (!StoredMap[page]) { page++; continue; }
		const UINT32 first = page;
		while (page < pages && StoredMap[page] && slots < nStatePages) { page++; slots++; }
		ba.Data	  = DrvRAM + (first << 12);
		ba.nLen	  = (page - first) << 12;
		ba.szName = "Main RAM";
		BurnAcb(&ba);
		if (slots == nStatePages) break;
	}

	for (UINT32 left = (nStatePages - slots) << 12; left; ) {
		ba.Data	  = StatePadding;
		ba.nLen	  = (left < STATE_PADDING) ? left : STATE_PADDING;
		ba.szName = "Main RAM unused slots";
		BurnAcb(&ba);
		left -= ba.nLen;
	}

	if (nAction & ACB_WRITE) {
		// the pages left out of the state are zero
		for (UINT32 page = 0; page < pages; page++) {
			if (DrvRamWritten[page] && !StoredMap[page]) memset(DrvRAM + (page << 12), 0, 0x1000);
			DrvRamWritten[page] = StoredMap[page];
		}
		CheckpointDrop();
		RamWrittenRestart();
	}
}

static inline UINT32 u2f_bits(float f) { UINT32 v; memcpy(&v, &f, 4); return v; }
static inline float u2f(UINT32 v) { float f; memcpy(&f, &v, 4); return f; }

static inline INT32 RoundInt(double v)
{
	return (v >= 0) ? (INT32)(v + 0.5) : -(INT32)(-v + 0.5);
}

static inline INT32 Clamp(INT32 v, INT32 lo, INT32 hi)
{
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

#define COMBINE(x, d, m)	((x) = ((x) & ~(m)) | ((d) & (m)))

// ---------------------------------------------------------------------------
// NAND

static UINT16 Permute(UINT16 value, const UINT8 *bits)
{
	UINT16 result = 0;
	for (INT32 i = 0; i < 16; i++) result |= ((value >> bits[i]) & 1) << i;
	return result;
}

static const UINT8 LfsrBits[4][16] = {
	{ 1, 6, 10, 5, 7, 9, 12, 3, 13, 8, 0, 15, 14, 2, 11, 4 },
	{ 6, 8, 15, 2, 12, 5, 3, 14, 10, 1, 9, 4, 7, 11, 0, 13 },
	{ 2, 9, 5, 0, 8, 6, 15, 10, 14, 7, 3, 12, 13, 11, 4, 1 },
	{ 12, 3, 2, 14, 15, 0, 5, 9, 7, 10, 4, 1, 8, 13, 11, 6 },
};

static void NandInitDecrypt()
{
	for (INT32 v = 0; v < 0x10000; v++) {
		NandDataLut[v] = Permute(v, pNandDataBits);
		NandKeyLut[v] = Permute(v, pNandKeyBits);
	}

	for (INT32 sel = 0; sel < 4; sel++) {
		UINT16 state = nNandSeed;
		for (INT32 i = 0; i < 0x800; i++) {
			NandMasks[sel][i] = Permute(state, LfsrBits[sel]);
			state = (state >> 1) ^ ((state & 1) ? 0xb400 : 0);
		}
	}

	for (INT32 i = 0; i < NAND_CACHE; i++) NandCache[i].tag = -1;
}

// each record decrypts on its own: the key is in its spare area, the mask depends on the position
static void NandFill(NandCacheLine *line, INT32 chip, UINT32 record)
{
	UINT8 src[NAND_RECORD];
	BurnHDDReadBytes(Nand[chip], (UINT64)record * NAND_RECORD, NAND_RECORD, src);

	INT32 erased = 1;
	for (INT32 i = 0; i < NAND_RECORD && erased; i++) erased = (src[i] == 0xff);

	if (erased) {
		memset(line->data, 0xff, 0x1000);
		return;
	}

	UINT16 key = src[0x1070] | (src[0x1071] << 8);
	const UINT16 *mask = NandMasks[((key >> 5) & 1) | (((key >> 12) & 1) << 1)];
	UINT16 kl = NandKeyLut[key];

	for (INT32 i = 0; i < 0x800; i++) {
		INT32 pos = i * 2 + ((i >= 0x400) ? 0x40 : 0);
		UINT16 plain = NandDataLut[(src[pos] | (src[pos + 1] << 8)) ^ mask[i] ^ kl];
		line->data[i * 2 + 0] = plain & 0xff;
		line->data[i * 2 + 1] = plain >> 8;
	}
}

static UINT8 *NandPage(INT32 chip, UINT32 offset)
{
	UINT32 record = (offset >> 12) & (NAND_RECORDS - 1);
	INT32 tag = (chip << 18) | record;
	NandCacheLine *line = &NandCache[(record ^ (chip * (NAND_CACHE / 2))) & (NAND_CACHE - 1)];

	if (line->tag != tag) {
		NandFill(line, chip, record);
		line->tag = tag;
	}

	return line->data;
}

static UINT32 NandRead32(UINT32 a)
{
	INT32 chip = (a >= 0xc0000000);
	UINT32 offset = a - (chip ? 0xc0000000 : 0x80000000);
	UINT8 *p = NandPage(chip, offset) + (offset & 0xffc);
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((UINT32)p[3] << 24);
}

static UINT8 NandReadByte(UINT32 a) { return NandRead32(a) >> ((a & 3) * 8); }
static UINT16 NandReadHalf(UINT32 a) { return NandRead32(a) >> ((a & 2) * 8); }
static UINT32 NandReadWord(UINT32 a) { return NandRead32(a); }
static UINT64 NandReadDouble(UINT32 a) { return NandRead32(a) | ((UINT64)NandRead32(a + 4) << 32); }

// physical bus reads, for the DMA
static UINT32 BusRead32(UINT32 a)
{
	if (a < RAM_SIZE) return Ram32(a);
	if (a >= 0x80000000 && a < 0xf0000000) return NandRead32(a);
	if (a >= 0xf0200000 && a < 0xf0200000 + BACKUP_SIZE) return BURN_ENDIAN_SWAP_INT32(*(UINT32*)(DrvBackup + (a & 0x1fffc)));
	return 0;
}

// ---------------------------------------------------------------------------
// interrupts, timers, beam

static UINT64 CpuCycles()
{
	return (UINT64)Mips3TotalCycles();
}

static void update_irqs()
{
	// YG_Add_Intr routes RDC, GEC and VOC to SCC source bits 3, 4 and 5
	irq_sources = ((rdc[0x10 / 4] & rdc[0x14 / 4] & 0x0f77) ? 8 : 0)
		| ((gec[0x10 / 4] & gec[0x14 / 4] & 3) ? 16 : 0)
		| (((voc[0x14 / 4] & 0x80000000) && (voc[0x10 / 4] & voc[0x14 / 4] & 0x307)) ? 32 : 0);

	if (mvc[0x10 / 4] & mvc[0x14 / 4] & 15) irq_sources |= 1U << 27;
	if (yuv[0x10 / 4] & yuv[0x14 / 4] & 3) irq_sources |= 1U << 28;

	// timers 3..7 share SCC interrupt 30
	if (timer[1][3] & 1) irq_sources |= 1U << 30;

	// YTFDMA_Intr on SCC source 11, shared by all channels
	if ((dma[0x1c / 4] | dma[0x2c / 4] | dma[0x5c / 4] | dma[0x6c / 4]) & 0x80000000) irq_sources |= 1U << 11;

	for (INT32 i = 0; i < 5; i++) {
		Mips3SetIRQLine(i, (irq_sources & int_select[i]) ? 1 : 0);
	}
}

// timers 6 and 7 count up at the cpu clock / 32 (8.33MHz) while control bit 0 is set
static UINT64 timer_ticks()
{
	return CpuCycles() / 32;
}

static UINT32 timer_r(INT32 n, INT32 offset)
{
	const UINT32 *regs = timer[n];
	if (offset == 0 && (regs[2] & 1)) return regs[0] + (UINT32)(timer_ticks() - timer_epoch[n]);
	return regs[offset];
}

static void timer_latch(INT32 n)
{
	timer[n][0] = timer_r(n, 0);
	timer_epoch[n] = timer_ticks();
}

static void timer6_w(INT32 offset, UINT32 data, UINT32 mask)
{
	// TODO: compare interrupts, only free-running operation is used
	timer_latch(0);
	COMBINE(timer[0][offset], data, mask);
}

static void timer7_w(INT32 offset, UINT32 data, UINT32 mask)
{
	timer_latch(1);
	UINT32 *regs = timer[1];

	if (offset == 3) {
		regs[3] &= ~(data & mask);	// YG_TimInit / YG_TimHandr3_7 acknowledge
	} else {
		COMBINE(regs[offset], data, mask);
	}

	if (regs[2] & 1) {
		UINT32 ticks = regs[1] - regs[0];
		timer7_fire = CpuCycles() + (ticks ? (UINT64)ticks : ((UINT64)1 << 32)) * 32;
	} else {
		timer7_fire = NEVER;
	}

	update_irqs();
}

static void timer7_compare()
{
	timer[1][3] |= 1;
	timer7_fire = CpuCycles() + ((UINT64)1 << 32) * 32;
	update_irqs();
}

static void schedule_vint(INT32 event)
{
	vint_line = ((voc[0x1c / 4] >> (event * 16)) & 0x7ff) % V_TOTAL;
	vint_event = event;
}

static void vint()
{
	INT32 event = vint_event;
	vint_line = -1;

	if (voc[0] & 1) {
		Y2_STAT(nStatEvent[event]);
#if defined (FBNEO_DEBUG)
		if (Y2_TRACING) bprintf(PRINT_NORMAL, _T("Y2T %d line %3d: event %d\n"), nTraceFrame, nCurrentLine, event);
#endif
		voc[0x10 / 4] |= 1U << event;
		update_irqs();
		schedule_vint(event ^ 1);
	}
}

static void jvs_timeout(INT32 n)
{
	// completed + error + timeout: how BDJVS_ReceiveBuffer sees an empty bus
	jvs_status[n] |= 0x23;
	jvs_fire[n] = NEVER;
}

// ---------------------------------------------------------------------------
// graphics: GE lists, render target clears

static UINT32 pack_pixel(UINT32 color, INT32 mode)
{
	switch (mode) {
		case 0: return ((color >> 16) & 0x8000) | ((color >> 9) & 0x7c00) | ((color >> 6) & 0x3e0) | ((color >> 3) & 0x1f);
		case 1: return ((color >> 8) & 0xf800) | ((color >> 5) & 0x7e0) | ((color >> 3) & 0x1f);
		case 2: return ((color >> 16) & 0xf000) | ((color >> 12) & 0xf00) | ((color >> 8) & 0xf0) | ((color >> 4) & 0xf);
	}
	return color;
}

static void pixel_write(UINT32 address, INT32 mode, UINT32 color)
{
	if (address >= RAM_SIZE) return;
	INT32 shift = (mode == 3) ? 0 : ((address & 2) * 8);
	UINT32 mask = (mode == 3) ? ~0U : (0xffffU << shift);
	RamW32(address, (Ram32(address) & ~mask) | (pack_pixel(color, mode) << shift));
}

// 8x8 tiles, shared by render targets and scanout (TODO: the physical interleaving is unverified)
static UINT32 buffer_offset(UINT32 x, UINT32 y, UINT32 width, INT32 linear)
{
	return linear ? (y * width + x) : (((y / 8) * (width / 8) + x / 8) * 64 + (y & 7) * 8 + (x & 7));
}

static UINT16 TextureMorton[64];

// rank of each point among the occupied Morton positions of a w x h edge tile, per shape (sizes are
// multiples of 8), built on first use
static UINT16 *EdgeTileRanks[64];

static UINT32 edge_tile_rank(UINT32 x, UINT32 y, UINT32 w, UINT32 h)
{
	UINT32 index = 0, ox = 0, oy = 0;
	for (INT32 bit = 5; bit >= 0; bit--) {
		UINT32 size = 1U << bit;
		UINT32 q = ((x >> bit) & 1) | (((y >> bit) & 1) << 1);
		for (UINT32 n = 0; n < q; n++) {
			UINT32 xx = ox + (n & 1) * size;
			UINT32 yy = oy + (n >> 1) * size;
			UINT32 cw = (xx < w) ? (((w - xx) < size) ? (w - xx) : size) : 0;
			UINT32 ch = (yy < h) ? (((h - yy) < size) ? (h - yy) : size) : 0;
			index += cw * ch;
		}
		ox += (q & 1) * size;
		oy += (q >> 1) * size;
	}
	return index;
}

// main thread only: the render workers just read the tables
static void EdgeTileRanksBuild(UINT32 w, UINT32 h)
{
	if (!w || !h || w > 64 || h > 64 || (w & 7) || (h & 7)) return;

	UINT32 shape = ((w >> 3) - 1) * 8 + ((h >> 3) - 1);
	if (EdgeTileRanks[shape]) return;

	UINT16 *ranks = (UINT16*)malloc(64 * 64 * sizeof(UINT16));
	if (ranks == NULL) return;
	for (UINT32 y = 0; y < 64; y++) {
		for (UINT32 x = 0; x < 64; x++) ranks[y * 64 + x] = edge_tile_rank(x, y, w, h);
	}
	EdgeTileRanks[shape] = ranks;
}

// the edge tile shapes of a width x height texture
static void EdgeTileRanksPrepare(UINT32 width, UINT32 height)
{
	UINT32 we = width & 63, he = height & 63;
	if (we) EdgeTileRanksBuild(we, 64);
	if (he) EdgeTileRanksBuild(64, he);
	if (we && he) EdgeTileRanksBuild(we, he);
}

static void EdgeTileRanksExit()
{
	for (INT32 i = 0; i < 64; i++) {
		free(EdgeTileRanks[i]);
		EdgeTileRanks[i] = NULL;
	}
}

// Morton order in 64x64 blocks with compactly packed edge blocks (TODO: edge packing is unverified)
static inline UINT32 texture_offset(UINT32 x, UINT32 y, UINT32 width, UINT32 height)
{
	UINT32 tx = x & ~63U;
	UINT32 ty = y & ~63U;
	UINT32 w = (width - tx < 64) ? (width - tx) : 64;
	UINT32 h = (height - ty < 64) ? (height - ty) : 64;
	UINT32 index = ty * width + tx * h;
	x &= 63;
	y &= 63;

	if (w == 64 && h == 64) return index + (TextureMorton[x] | (TextureMorton[y] << 1));

	if (!(w & 7) && !(h & 7)) {
		const UINT16 *ranks = EdgeTileRanks[((w >> 3) - 1) * 8 + ((h >> 3) - 1)];
		if (ranks) return index + ranks[y * 64 + x];
	}

	return index + edge_tile_rank(x, y, w, h);
}

// the color and depth ranges a clear writes; 0 when the command is refused
static INT32 clear_ranges(UINT32 control, UINT32 *base, UINT32 *bytes, UINT32 *db, UINT32 *depth_bytes, INT32 *clear_color, INT32 *clear_depth)
{
	UINT32 count = control & 0x007ffff8;
	INT32 mode = (RdcR[0xa0 / 4] >> 16) & 3;
	*base = RdcR[0x9c / 4] & 0x0ffffff0;
	*bytes = count * ((mode == 3) ? 4 : 2);
	*clear_color = (control & 0x40000000) != 0;
	*clear_depth = (control & 0x80000000) && (RdcR[0xa0 / 4] & (1 << 23));
	*db = RdcR[0x98 / 4] & 0x0fffff00;
	*depth_bytes = count * (((RdcR[0xa0 / 4] >> 22) & 1) ? 4 : 2);

	return !((*clear_color && *bytes > RAM_SIZE - *base) || (*clear_depth && *depth_bytes > RAM_SIZE - *db));
}

static INT32 buffer_clear(UINT32 control, UINT32 depth, UINT32 color)
{
	INT32 mode = (RdcR[0xa0 / 4] >> 16) & 3;
	INT32 depth_wide = (RdcR[0xa0 / 4] >> 22) & 1;
	UINT32 base, bytes, db, depth_bytes;
	INT32 clear_color, clear_depth;
	if (!clear_ranges(control, &base, &bytes, &db, &depth_bytes, &clear_color, &clear_depth)) return 0;
	UINT32 count = control & 0x007ffff8;

	if (clear_depth) {
		// positive reciprocal depth: 32-bit mode stores the float, 16-bit mode its upper half
		float z = u2f(depth);
		UINT32 value = u2f_bits((z > 0) ? z : 0.0f);
		if (depth_wide) {
			for (UINT32 i = 0; i < count; i++) RamW32(db + i * 4, value);
		} else {
			for (UINT32 i = 0; i < count; i++) RamW16(db + i * 2, value >> 16);
		}
	}

	if (control & 0x40000000) {
		Y2_STAT(nStatClears);
		UINT32 packed = pack_pixel(color, mode);
		if (mode != 3) packed |= packed << 16;
		for (UINT32 i = 0; i < bytes; i += 4) RamW32(base + i, packed);
	}

	return 1;
}

// ---------------------------------------------------------------------------
// renderer: screen-space triangles, drawn as soon as they are decoded
// TODO: GE microcode/T&L, SA depth sorting, fog, logic ops and exact subpixel rules

struct Y2Vertex {
	float x, y, z, u, v;
	UINT32 color;
};

struct Y2Poly {
	Y2Vertex a, b, c;		// wound so that the area is positive
	float area;				// twice the area
	UINT32 control;			// raster control (attribute word 1)
	UINT32 color;			// constant color when there is no texture (attribute word 2)
	INT32 textured, perspective, constant_color;

	UINT32 fb, db;			// color and depth buffers
	UINT32 stride;
	INT32 mode, linear, depth_enabled, depth_wide;

	UINT32 texbase, palette, tw, th;
	INT32 texmode, pmode, bits;
	INT32 clamp_u, clamp_v, power2_u, power2_v, swizzled, complete_tiles;
	INT32 flat_z;			// the vertices share their depth: it is not interpolated, and the perspective divide cancels out
	INT32 alpha_weighted;	// filtering weights the color of a texel by its alpha
	const UINT32 *decoded_palette;	// ARGB8888, or NULL to read it as it is used
	const UINT32 *pixels;	// texture decoded as linear ARGB8888, or NULL to read it as it is used
	UINT32 linear_argb;		// linear ARGB8888 texture: read the texels in place
};

static UINT32 DecodedPalette[256];

// textures decoded once for the bilinear sampler, dropped whenever the target could write to them
// (a triangle whose target overlaps a texture, target changes, clears, end of list)
struct Y2Snapshot {
	UINT32 key[7];
	UINT32 *pixels;
};

#define SNAPSHOT_BYTES	0x1000000

static std::vector<Y2Snapshot> *Snapshots = NULL;
static UINT32 nSnapshotBytes = 0;

static void RenderFlush();

static void SnapshotFlush()
{
	RenderFlush();
	if (Snapshots == NULL) return;
	for (size_t i = 0; i < Snapshots->size(); i++) free((*Snapshots)[i].pixels);
	Snapshots->clear();
	nSnapshotBytes = 0;
}

#define COORD_LIMIT		524288.0f	// 2^19: a coordinate times the widest texture stays below 2^30

// pixel modes shared by the VOC, RC targets, palettes and texture modes 4..7, as ARGB8888
static inline UINT32 pixel_read(UINT32 address, INT32 mode)
{
	if (address >= RAM_SIZE) return 0;
	UINT32 p = Ram32(address) >> ((mode == 3) ? 0 : ((address & 2) * 8));
	UINT32 r, g, b;

	switch (mode) {
		case 0:
			r = (p >> 10) & 0x1f; g = (p >> 5) & 0x1f; b = p & 0x1f;
			return ((p & 0x8000) ? 0xff000000 : 0) | (((r << 3) | (r >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2));
		case 1:
			r = (p >> 11) & 0x1f; g = (p >> 5) & 0x3f; b = p & 0x1f;
			return 0xff000000 | (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
		case 2:
			return (((p >> 12) & 0xf) * 0x11 << 24) | (((p >> 8) & 0xf) * 0x11 << 16) | (((p >> 4) & 0xf) * 0x11 << 8) | ((p & 0xf) * 0x11);
	}
	return p;
}

static inline UINT32 depth_encode(float z, INT32 wide)
{
	UINT32 value = u2f_bits((z > 0) ? z : 0.0f);
	return wide ? value : (value >> 16);
}

static inline UINT32 depth_read(UINT32 base, INT32 wide, UINT32 index)
{
	UINT32 address = base + index * (wide ? 4 : 2);
	if (address >= RAM_SIZE) return 0;
	return wide ? Ram32(address) : ((Ram32(address) >> ((address & 2) * 8)) & 0xffff);
}

static inline void depth_write(UINT32 base, INT32 wide, UINT32 index, UINT32 value)
{
	UINT32 address = base + index * (wide ? 4 : 2);
	if (address >= RAM_SIZE) return;
	INT32 shift = wide ? 0 : ((address & 2) * 8);
	UINT32 mask = wide ? ~0U : (0xffffU << shift);
	RamW32(address, (Ram32(address) & ~mask) | (value << shift));
}

static inline float Edge(const Y2Vertex &p, const Y2Vertex &q, float x, float y)
{
	return (q.x - p.x) * (y - p.y) - (q.y - p.y) * (x - p.x);
}

// wrap or clamp one texel coordinate, powers of two need no division
static inline UINT32 Coord(INT32 i, UINT32 size, INT32 clamp, INT32 power2)
{
	if ((UINT32)i < size) return i;
	if (clamp) return Clamp(i, 0, (INT32)size - 1);
	if (power2) return (UINT32)i & (size - 1);
	INT32 remainder = i % (INT32)size;
	return (remainder < 0) ? (remainder + size) : remainder;
}

// floor for the bounded coordinate range, where (INT32)value is exact
static inline INT32 FloorInt(float value)
{
	INT32 truncated = (INT32)value;
	if (value >= 0) return truncated;
	return (value < (float)truncated) ? (truncated - 1) : truncated;
}

// a perspective divide over depths of mixed sign can take a coordinate far outside the vertices' range
static inline float BoundCoord(float value)
{
	if (value <= COORD_LIMIT && value >= -COORD_LIMIT) return value;
	return (value > 0) ? COORD_LIMIT : -COORD_LIMIT;	// NaN goes to the lower bound
}

// one texel at integer coordinates, as ARGB8888
static inline UINT32 Fetch(const Y2Poly &poly, UINT32 x, UINT32 y)
{
	if (poly.linear_argb) return Ram32(poly.texbase + (y * poly.tw + x) * 4);

	UINT32 index = y * poly.tw + x;
	if (poly.swizzled) {
		index = poly.complete_tiles
			? (y & ~63U) * poly.tw + ((x & ~63U) << 6) + TextureMorton[x & 63] + (TextureMorton[y & 63] << 1)
			: texture_offset(x, y, poly.tw, poly.th);
	}

	if (poly.texmode >= 4) return pixel_read(poly.texbase + index * ((poly.texmode == 7) ? 4 : 2), poly.texmode - 4);

	UINT32 address = poly.texbase + index * poly.bits / 8;
	if (address >= RAM_SIZE) return 0;

	UINT8 packed = DrvRAM[address];
	UINT32 entry = (poly.bits == 4) ? ((packed >> ((index & 1) * 4)) & 15) : packed;
	return poly.decoded_palette ? poly.decoded_palette[entry] : pixel_read(poly.palette + entry * ((poly.pmode == 3) ? 4 : 2), poly.pmode);
}

static inline UINT32 ClampByte(float v)
{
	return (UINT32)Clamp((INT32)(v + 0.5f), 0, 255);
}

// texture filtering uses normalized coordinates and texel centers
template <bool Decoded>
static UINT32 TexelBilinear(const Y2Poly &poly, float u, float v)
{
	UINT32 tw = poly.tw, th = poly.th;
	float uu = u * tw - 0.5f, vv = v * th - 0.5f;
	INT32 x = FloorInt(uu), y = FloorInt(vv);
	float fx = uu - x, fy = vv - y;
	UINT32 tx0 = Coord(x, tw, poly.clamp_u, poly.power2_u), tx1 = Coord(x + 1, tw, poly.clamp_u, poly.power2_u);
	UINT32 ty0 = Coord(y, th, poly.clamp_v, poly.power2_v), ty1 = Coord(y + 1, th, poly.clamp_v, poly.power2_v);
	UINT32 p[4];
	if (Decoded) {
		p[0] = poly.pixels[ty0 * tw + tx0];
		p[1] = poly.pixels[ty0 * tw + tx1];
		p[2] = poly.pixels[ty1 * tw + tx0];
		p[3] = poly.pixels[ty1 * tw + tx1];
	} else {
		p[0] = Fetch(poly, tx0, ty0);
		p[1] = Fetch(poly, tx1, ty0);
		p[2] = Fetch(poly, tx0, ty1);
		p[3] = Fetch(poly, tx1, ty1);
	}

	// four equal texels filter to themselves (the sums below are within rounding of the value)
	if (p[0] == p[1] && p[0] == p[2] && p[0] == p[3]) return (poly.alpha_weighted && !(p[0] >> 24)) ? 0 : p[0];

	// transparent texels contribute no color, so they cannot darken a cell's edge
	if (poly.alpha_weighted) {
		UINT32 a0 = p[0] >> 24, a1 = p[1] >> 24, a2 = p[2] >> 24, a3 = p[3] >> 24;
		if (!(a0 | a1 | a2 | a3)) return 0;

		if (a0 != a1 || a0 != a2 || a0 != a3) {
			float top_alpha = a0 * (1 - fx) + a1 * fx;
			float bottom_alpha = a2 * (1 - fx) + a3 * fx;
			float alpha = top_alpha * (1 - fy) + bottom_alpha * fy;
			if (alpha <= 0) return 0;

			UINT32 result = ClampByte(alpha) << 24;
			for (INT32 shift = 0; shift < 24; shift += 8) {
				float top = ((p[0] >> shift) & 255) * a0 * (1 - fx) + ((p[1] >> shift) & 255) * a1 * fx;
				float bottom = ((p[2] >> shift) & 255) * a2 * (1 - fx) + ((p[3] >> shift) & 255) * a3 * fx;
				result |= ClampByte((top * (1 - fy) + bottom * fy) / alpha) << shift;
			}
			return result;
		}
	}

	UINT32 result = 0;
	for (INT32 shift = 0; shift < 32; shift += 8) {
		float top = ((p[0] >> shift) & 255) * (1 - fx) + ((p[1] >> shift) & 255) * fx;
		float bottom = ((p[2] >> shift) & 255) * (1 - fx) + ((p[3] >> shift) & 255) * fx;
		result |= ClampByte(top * (1 - fy) + bottom * fy) << shift;
	}
	return result;
}

static inline UINT32 BlendFactor(UINT32 which, UINT32 src, UINT32 dst, INT32 shift)
{
	switch (which) {
		case 0: return 0;
		case 1: return 255;
		case 2: return (dst >> shift) & 255;
		case 3: return 255 - ((dst >> shift) & 255);
		case 4: return src >> 24;
		case 5: return 255 - (src >> 24);
		case 6: return dst >> 24;
	}
	return 255 - (dst >> 24);
}

template <bool Filter, bool Decoded>
static void RenderSpan(INT32 y, INT32 startx, INT32 stopx, const Y2Poly &poly)
{
	const Y2Vertex &a = poly.a, &b = poly.b, &c = poly.c;
	const UINT32 control = poly.control;
	const float fy = y + 0.5f;

	// Edge(p, q, x, y) = (q.x - p.x) * (y - p.y) - (q.y - p.y) * (x - p.x): the first product is the same along the row
	const float ra = (c.x - b.x) * (fy - b.y), sa = c.y - b.y;
	const float rb = (a.x - c.x) * (fy - c.y), sb = a.y - c.y;
	const float rc = (b.x - a.x) * (fy - a.y), sc = b.y - a.y;

	for (INT32 x = startx; x < stopx; x++) {
		const float fx = x + 0.5f;
		const float wa = (ra - sa * (fx - b.x)) / poly.area;
		const float wb = (rb - sb * (fx - c.x)) / poly.area;
		const float wc = (rc - sc * (fx - a.x)) / poly.area;
		const float iz = wa * a.z + wb * b.z + wc * c.z;
		const UINT32 index = buffer_offset(x, y, poly.stride, poly.linear);

		UINT32 z = 0;
		if (poly.depth_enabled) {
			z = depth_encode(iz, poly.depth_wide);
			UINT32 oldz = depth_read(poly.db, poly.depth_wide, index);
			UINT32 result = (z < oldz) ? 1 : (z == oldz) ? 2 : 4;
			if (!(((control >> 12) & 7) & result)) continue;
		}

		// without a texture, attribute word 2 is the constant color; its alpha does full-screen fades
		UINT32 tex = poly.color;
		if (poly.textured) {
			float u, v;
			if (poly.perspective && iz > 0) {
				u = (wa * a.u * a.z + wb * b.u * b.z + wc * c.u * c.z) / iz;
				v = (wa * a.v * a.z + wb * b.v * b.z + wc * c.v * c.z) / iz;
			} else {
				u = wa * a.u + wb * b.u + wc * c.u;
				v = wa * a.v + wb * b.v + wc * c.v;
			}
			u = BoundCoord(u);
			v = BoundCoord(v);

			if (Filter) {
				tex = TexelBilinear<Decoded>(poly, u, v);
			} else {
				tex = Fetch(poly, Coord(FloorInt(u * poly.tw), poly.tw, poly.clamp_u, poly.power2_u), Coord(FloorInt(v * poly.th), poly.th, poly.clamp_v, poly.power2_v));
			}

			// opaque textured draws keep cutout texels (KOF's paletted font) (TODO: alpha threshold, TEX_ADIS)
			if (((control >> 16) & 7) == 1 && !(control & (1 << 19)) && !(tex >> 24)) continue;
		}

		if (poly.depth_enabled && (control & (1 << 15))) depth_write(poly.db, poly.depth_wide, index, z);

		if (!(control & (1 << 11))) continue;

		UINT32 src = 0;

		// with weights this close to normalised, interpolating equal colors is exact
		const float sum = wa + wb + wc - 1.0f;
		if (poly.constant_color && sum < (1.0f / 1024.0f) && sum > -(1.0f / 1024.0f)) {
			if (a.color == 0xffffffff) {
				src = tex;
			} else {
				for (INT32 shift = 0; shift < 32; shift += 8) {
					src |= (((a.color >> shift) & 255) * ((tex >> shift) & 255) / 255) << shift;
				}
			}
		} else {
			for (INT32 shift = 0; shift < 32; shift += 8) {
				UINT32 col = Clamp((INT32)(wa * ((a.color >> shift) & 255) + wb * ((b.color >> shift) & 255) + wc * ((c.color >> shift) & 255) + 0.5f), 0, 255);
				src |= (col * ((tex >> shift) & 255) / 255) << shift;
			}
		}

		const UINT32 address = poly.fb + index * ((poly.mode == 3) ? 4 : 2);

		// SRC=1,DST=0 and opaque source-over pixels do not read the destination
		if ((control & 0x3f) == 8 || ((control & 0x3f) == 0x25 && (src >> 24) == 255)) {
			pixel_write(address, poly.mode, (((control >> 6) & 3) == 3) ? 0xff000000 : (src | 0xff000000));
			continue;
		}

		// zero source alpha leaves the destination RGB alone for these factors, only the alpha bit changes
		if (!(src >> 24) && ((control & 0x3f) == 0x25 || (control & 0x3f) == 0x21)) {
			static const UINT32 AlphaMask[4] = { 0x8000, 0, 0xf000, 0xff000000 };
			if (poly.mode != 1 && address < RAM_SIZE) {
				RamW32(address, Ram32(address) | (AlphaMask[poly.mode] << ((poly.mode == 3) ? 0 : ((address & 2) * 8))));
			}
			continue;
		}

		// blend factors follow the SDK's SRC_BLEND/DST_BLEND fields
		const UINT32 dst = pixel_read(address, poly.mode);
		UINT32 color = 0xff000000;
		for (INT32 shift = 0; shift < 24; shift += 8) {
			INT32 sv = ((src >> shift) & 255) * BlendFactor((control >> 3) & 7, src, dst, shift);
			INT32 dv = ((dst >> shift) & 255) * BlendFactor(control & 7, src, dst, shift);

			// mode 3 subtracts (Higurashi DispSpriteGs flag 4000), mode 1 adds (flag 8000)
			INT32 value = (((control >> 6) & 3) == 3) ? (dv - sv) : (dv + sv);
			color |= (UINT32)Clamp((value + 127) / 255, 0, 255) << shift;
		}
		pixel_write(address, poly.mode, color);
	}
}

// RenderSpan for the common cases, on an RGB565 target with a 16-bit depth buffer or none: Kind 4/8 is an
// unfiltered texture through a decoded 4/8-bit palette (KOF's stages), 16 an unfiltered 16-bit pixel mode
// (movies), 1 and 2 a bilinear texture fetched or from its snapshot (Higurashi); same results as the
// generic span
#define SPAN_BILINEAR		1
#define SPAN_SNAPSHOT		2

template <bool Perspective, INT32 Kind, bool Depth>
static void RenderSpanFast(INT32 y, INT32 startx, INT32 stopx, const Y2Poly &poly)
{
	const Y2Vertex &a = poly.a, &b = poly.b, &c = poly.c;
	const UINT32 control = poly.control;
	const float fy = y + 0.5f;

	const float ra = (c.x - b.x) * (fy - b.y), sa = c.y - b.y;
	const float rb = (a.x - c.x) * (fy - c.y), sb = a.y - c.y;
	const float rc = (b.x - a.x) * (fy - a.y), sc = b.y - a.y;

	const UINT32 depth_func = (control >> 12) & 7;
	const INT32 depth_write = control & (1 << 15);
	const INT32 color_write = control & (1 << 11);
	const INT32 cutout = ((control >> 16) & 7) == 1 && !(control & (1 << 19));
	const UINT32 blend = control & 0x3f;
	const INT32 subtract = ((control >> 6) & 3) == 3;
	const UINT32 src_factor = (control >> 3) & 7, dst_factor = control & 7;
	const INT32 alpha_factors = (src_factor < 2 || src_factor >= 4) && (dst_factor < 2 || dst_factor >= 4);
	const UINT32 tw = poly.tw, th = poly.th;
	const float ftw = (float)tw, fth = (float)th;
	const UINT32 *palette = poly.decoded_palette;
	const UINT32 row = poly.linear ? y * poly.stride : (y / 8) * (poly.stride / 8) * 64 + (y & 7) * 8;
	const INT32 white = a.color == 0xffffffff;

	// the weights are linear along the row
	const float start = startx + 0.5f;
	float wa = (ra - sa * (start - b.x)) / poly.area;
	float wb = (rb - sb * (start - c.x)) / poly.area;
	float wc = (rc - sc * (start - a.x)) / poly.area;
	const float dwa = -sa / poly.area, dwb = -sb / poly.area, dwc = -sc / poly.area;

	for (INT32 x = startx; x < stopx; x++, wa += dwa, wb += dwb, wc += dwc) {
		const float iz = poly.flat_z ? a.z : (wa * a.z + wb * b.z + wc * c.z);
		const UINT32 index = poly.linear ? (row + x) : (row + (x / 8) * 64 + (x & 7));

		UINT32 z = 0, depth_address = 0;
		if (Depth) {
			z = depth_encode(iz, 0);
			depth_address = poly.db + index * 2;
			const UINT32 oldz = (depth_address < RAM_SIZE) ? Ram16(depth_address) : 0;
			if (!(depth_func & ((z < oldz) ? 1 : (z == oldz) ? 2 : 4))) continue;
		}

		float u, v;
		if (Perspective && iz > 0 && !poly.flat_z) {
			const float inv_z = 1.0f / iz;
			u = (wa * a.u * a.z + wb * b.u * b.z + wc * c.u * c.z) * inv_z;
			v = (wa * a.v * a.z + wb * b.v * b.z + wc * c.v * c.z) * inv_z;
		} else {
			u = wa * a.u + wb * b.u + wc * c.u;
			v = wa * a.v + wb * b.v + wc * c.v;
		}
		u = BoundCoord(u);
		v = BoundCoord(v);

		UINT32 tex = 0;
		if (Kind == SPAN_BILINEAR) {
			tex = TexelBilinear<false>(poly, u, v);
		} else if (Kind == SPAN_SNAPSHOT) {
			tex = TexelBilinear<true>(poly, u, v);
		} else {
			const UINT32 tx = Coord(FloorInt(u * ftw), tw, poly.clamp_u, poly.power2_u);
			const UINT32 ty = Coord(FloorInt(v * fth), th, poly.clamp_v, poly.power2_v);
			UINT32 texel = ty * tw + tx;
			if (poly.swizzled) {
				texel = poly.complete_tiles
					? (ty & ~63U) * tw + ((tx & ~63U) << 6) + TextureMorton[tx & 63] + (TextureMorton[ty & 63] << 1)
					: texture_offset(tx, ty, tw, th);
			}

			if (Kind == 16) {
				tex = pixel_read(poly.texbase + texel * 2, poly.texmode - 4);
			} else {
				const UINT32 texel_address = poly.texbase + texel * Kind / 8;
				if (texel_address < RAM_SIZE) {
					const UINT8 packed = DrvRAM[texel_address];
					tex = palette[(Kind == 4) ? ((packed >> ((texel & 1) * 4)) & 15) : packed];
				}
			}
		}

		if (cutout && !(tex >> 24)) continue;

		if (Depth && depth_write && depth_address < RAM_SIZE) RamW16(depth_address, z);

		if (!color_write) continue;

		UINT32 src = 0;
		const float sum = wa + wb + wc - 1.0f;
		if (poly.constant_color && sum < (1.0f / 1024.0f) && sum > -(1.0f / 1024.0f)) {
			if (white) {
				src = tex;
			} else {
				for (INT32 shift = 0; shift < 32; shift += 8) {
					src |= (((a.color >> shift) & 255) * ((tex >> shift) & 255) / 255) << shift;
				}
			}
		} else {
			for (INT32 shift = 0; shift < 32; shift += 8) {
				UINT32 col = Clamp((INT32)(wa * ((a.color >> shift) & 255) + wb * ((b.color >> shift) & 255) + wc * ((c.color >> shift) & 255) + 0.5f), 0, 255);
				src |= (col * ((tex >> shift) & 255) / 255) << shift;
			}
		}

		const UINT32 address = poly.fb + index * 2;
		if (address >= RAM_SIZE) continue;

		if (blend == 8 || (blend == 0x25 && (src >> 24) == 255)) {
			RamW16(address, pack_pixel(subtract ? 0xff000000 : (src | 0xff000000), 1));
			continue;
		}

		// RGB565 has no alpha bit to update
		if (!(src >> 24) && (blend == 0x25 || blend == 0x21)) continue;

		const UINT32 dst = pixel_read(address, 1);
		UINT32 color = 0xff000000;
		if (alpha_factors) {
			// factors that only depend on the alphas are the same for the three channels
			const INT32 sf = BlendFactor(src_factor, src, dst, 0), df = BlendFactor(dst_factor, src, dst, 0);
			for (INT32 shift = 0; shift < 24; shift += 8) {
				INT32 sv = ((src >> shift) & 255) * sf;
				INT32 dv = ((dst >> shift) & 255) * df;
				INT32 value = subtract ? (dv - sv) : (dv + sv);
				color |= (UINT32)Clamp((value + 127) / 255, 0, 255) << shift;
			}
		} else {
			for (INT32 shift = 0; shift < 24; shift += 8) {
				INT32 sv = ((src >> shift) & 255) * BlendFactor(src_factor, src, dst, shift);
				INT32 dv = ((dst >> shift) & 255) * BlendFactor(dst_factor, src, dst, shift);
				INT32 value = subtract ? (dv - sv) : (dv + sv);
				color |= (UINT32)Clamp((value + 127) / 255, 0, 255) << shift;
			}
		}
		RamW16(address, pack_pixel(color, 1));
	}
}

typedef void (*Y2SpanFn)(INT32 y, INT32 startx, INT32 stopx, const Y2Poly &poly);

static Y2SpanFn SpanFastPick(INT32 kind, INT32 perspective, INT32 depth)
{
#define SPAN_PICK(k)	(perspective ? (depth ? RenderSpanFast<true, k, true> : RenderSpanFast<true, k, false>) : (depth ? RenderSpanFast<false, k, true> : RenderSpanFast<false, k, false>))
	switch (kind) {
		case 4: return SPAN_PICK(4);
		case 8: return SPAN_PICK(8);
		case 16: return SPAN_PICK(16);
		case SPAN_BILINEAR: return SPAN_PICK(SPAN_BILINEAR);
		default: return SPAN_PICK(SPAN_SNAPSHOT);
	}
#undef SPAN_PICK
}

static INT32 InsideEdge(const Y2Vertex &p, const Y2Vertex &q, INT32 top, INT32 x, INT32 y)
{
	float value = Edge(p, q, x + 0.5f, y + 0.5f);
	return value > 0 || (value == 0 && top);
}

// each edge predicate is monotonic in x along a row: binary search its boundary with the span's edge expression
static INT32 ClipEdge(const Y2Vertex &p, const Y2Vertex &q, INT32 top, INT32 y, INT32 &row_min, INT32 &row_max)
{
	if (q.y == p.y) return InsideEdge(p, q, top, row_min, y);

	INT32 increasing = q.y < p.y;
	if (!InsideEdge(p, q, top, increasing ? row_max : row_min, y)) return 0;
	if (InsideEdge(p, q, top, increasing ? row_min : row_max, y)) return 1;

	INT32 low = row_min, high = row_max;
	while (low + 1 < high) {
		INT32 middle = low + (high - low) / 2;
		if (InsideEdge(p, q, top, middle, y) == increasing) high = middle; else low = middle;
	}

	if (increasing) row_min = high; else row_max = low;
	return 1;
}

// triangles are queued and drawn in batches, split by interleaved rows between the main thread and the
// workers: every pixel still sees the triangles in list order, and rows never share a 32-bit word of the
// color or depth buffer. Anything that could read what the batch draws flushes it first (SnapshotFlush).
#define Y2_MAX_WORKERS		7
#define Y2_QUEUE			512
#define Y2_THREAD_PIXELS	8192	// smaller batches are not worth waking the workers

struct Y2Queued {
	Y2Poly poly;
	INT32 x0, x1, y0, y1;
	INT32 ab, bc, ca;
	Y2SpanFn span;
	UINT32 palette[256];
};

static threadystruct Y2Workers[Y2_MAX_WORKERS];
static INT32 nY2Workers = 0;
static Y2Queued *RenderQueue = NULL;
static INT32 nQueued = 0;
static INT32 nQueuedPixels = 0;
static INT32 nRenderParts = 1;

static void RenderRows(INT32 part)
{
	const INT32 parts = nRenderParts;

	for (INT32 i = 0; i < nQueued; i++) {
		const Y2Queued &q = RenderQueue[i];
		const Y2Poly &poly = q.poly;

		for (INT32 y = q.y0 + ((part - q.y0 % parts) + parts) % parts; y <= q.y1; y += parts) {
			INT32 row_min = q.x0, row_max = q.x1;
			if (!ClipEdge(poly.b, poly.c, q.bc, y, row_min, row_max)
				|| !ClipEdge(poly.c, poly.a, q.ca, y, row_min, row_max)
				|| !ClipEdge(poly.a, poly.b, q.ab, y, row_min, row_max)) {
				continue;
			}

			q.span(y, row_min, row_max + 1, poly);
		}
	}
}

static void RenderFlush()
{
	if (nQueued == 0) return;

	INT32 workers = (nQueuedPixels >= Y2_THREAD_PIXELS) ? nY2Workers : 0;
	nRenderParts = workers + 1;

	for (INT32 i = 0; i < workers; i++) Y2Workers[i].notify();
	RenderRows(0);
	for (INT32 i = 0; i < workers; i++) Y2Workers[i].notify_wait();

	nQueued = 0;
	nQueuedPixels = 0;
}

static void (*WorkerRows)(INT32 part) = RenderRows;	// what the workers share out: a render batch or a YUV conversion

template <INT32 N>
static void RenderWorker()
{
	WorkerRows(N + 1);
}

static void (*const RenderWorkers[Y2_MAX_WORKERS])() = {
	RenderWorker<0>, RenderWorker<1>, RenderWorker<2>, RenderWorker<3>, RenderWorker<4>, RenderWorker<5>, RenderWorker<6>
};

static INT32 bRenderAsync = 0;		// lists render on their own thread, beside the cpu

static void RenderInit()
{
	// with two processors, rendering on both beats overlapping a one-thread render with the cpu
	const INT32 cpus = thready_cpu_count();
	bRenderAsync = cpus >= 3;
	nY2Workers = cpus - (bRenderAsync ? 2 : 1);
	if (nY2Workers > Y2_MAX_WORKERS) nY2Workers = Y2_MAX_WORKERS;
	if (nY2Workers < 0) nY2Workers = 0;

	for (INT32 i = 0; i < nY2Workers; i++) Y2Workers[i].init(RenderWorkers[i]);

	RenderQueue = (Y2Queued*)malloc(Y2_QUEUE * sizeof(Y2Queued));
	nQueued = 0;
	nQueuedPixels = 0;
}

static void RenderExit()
{
	for (INT32 i = 0; i < nY2Workers; i++) Y2Workers[i].exit();
	nY2Workers = 0;

	free(RenderQueue);
	RenderQueue = NULL;
	nQueued = 0;
}

static inline INT32 TopLeft(const Y2Vertex &p, const Y2Vertex &q)
{
	return q.y < p.y || (q.y == p.y && q.x > p.x);
}

static inline float Min3(float a, float b, float c) { float m = (a < b) ? a : b; return (m < c) ? m : c; }
static inline float Max3(float a, float b, float c) { float m = (a > b) ? a : b; return (m > c) ? m : c; }

static INT32 triangle_supported(const UINT32 *attr)
{
	const UINT32 control = attr[1];
	const INT32 texmode = (attr[2] >> 28) & 7;

	return !((control & 0x00f00300) || ((control >> 6) & 3) == 2
		|| (((control >> 16) & 7) != 1 && ((control >> 16) & 7) != 3)
		|| ((attr[0] & 1) && (texmode == 1 || texmode == 3)));
}

static INT32 draw_triangle(const UINT32 *attr, Y2Vertex a, Y2Vertex b, Y2Vertex c)
{
	const UINT32 control = attr[1];
	const INT32 textured = attr[0] & 1;
	const INT32 texmode = (attr[2] >> 28) & 7;

	if (!triangle_supported(attr)) return 0;	// unsupported raster state

	if (!(control & (1 << 11)) && !(control & (1 << 15))) return 1;

	float area = Edge(a, b, c.x, c.y);
	if (area == 0) return 1;
	if (area < 0) {
		Y2Vertex t = b; b = c; c = t;
		area = -area;
	}

	const INT32 ab = TopLeft(a, b), bc = TopLeft(b, c), ca = TopLeft(c, a);
	const UINT32 stride = (RdcR[0xa0 / 4] & 0x7f8) + 8;
	const INT32 mode = (RdcR[0xa0 / 4] >> 16) & 3;
	const INT32 xmax = (((RdcR[0xa4 / 4] & 0x7fe) + 1) < stride - 1) ? ((RdcR[0xa4 / 4] & 0x7fe) + 1) : (stride - 1);
	const INT32 ymax = (RdcR[0xa4 / 4] >> 16) & 0x7ff;

	INT32 x0 = (INT32)ceil(Min3(a.x, b.x, c.x) - 0.5f);
	INT32 y0 = (INT32)ceil(Min3(a.y, b.y, c.y) - 0.5f);
	INT32 x1 = (INT32)floor(Max3(a.x, b.x, c.x) - 0.5f);
	INT32 y1 = (INT32)floor(Max3(a.y, b.y, c.y) - 0.5f);
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > xmax) x1 = xmax;
	if (y1 > ymax) y1 = ymax;
	if (x0 > x1 || y0 > y1) return 1;

	Y2Poly poly;
	poly.a = a;
	poly.b = b;
	poly.c = c;
	poly.area = area;
	poly.control = control;
	poly.color = attr[2];
	poly.textured = textured;
	poly.perspective = (attr[0] >> 3) & 1;
	poly.constant_color = (a.color == b.color && b.color == c.color);
	poly.flat_z = (a.z == b.z && b.z == c.z);
	poly.fb = RdcR[0x9c / 4] & 0x0fffff00;
	poly.db = RdcR[0x98 / 4] & 0x0fffff00;
	poly.stride = stride;
	poly.mode = mode;
	poly.linear = (RdcR[0xa0 / 4] >> 19) & 1;
	poly.depth_wide = (RdcR[0xa0 / 4] >> 22) & 1;
	poly.depth_enabled = (RdcR[0xa0 / 4] >> 23) & 1;
	poly.tw = ((attr[2] & 255) + 1) * 8;
	poly.th = (((attr[2] >> 8) & 255) + 1) * 8;
	poly.texbase = (attr[3] & 0x7fffff) << 5;
	poly.texmode = texmode;
	poly.pmode = (RdcR[0xb0 / 4] >> 16) & 3;
	poly.bits = (texmode == 0) ? 4 : 8;
	poly.palette = (RdcR[((texmode == 0) ? 0xb4 : 0xbc) / 4] & 0x0ffffe00) + (((attr[2] >> 16) & 255) << poly.bits) * ((poly.pmode == 3) ? 4 : 2);
	poly.clamp_u = (attr[2] >> 26) & 1;
	poly.clamp_v = (attr[2] >> 27) & 1;
	poly.power2_u = !(poly.tw & (poly.tw - 1));
	poly.power2_v = !(poly.th & (poly.th - 1));
	poly.swizzled = (attr[2] >> 31) & 1;
	poly.complete_tiles = !(poly.tw & 63) && !(poly.th & 63);
	// alpha-weighted filtering keeps black fringes off Higurashi's animation cells (TODO: a hypothesis for this blend state only)
	poly.alpha_weighted = ((control >> 16) & 7) == 3 && ((control >> 3) & 7) == 4;
	poly.decoded_palette = NULL;
	poly.pixels = NULL;
	poly.linear_argb = 0;

	// feedback check against the whole clipped target, since earlier triangles of the list write it too
	const UINT32 cbytes = (mode == 3) ? 4 : 2, dbytes = poly.depth_wide ? 4 : 2;
	const UINT32 target_pixels = buffer_offset(xmax, ymax, stride, poly.linear) + 1;
	const UINT32 color_start = poly.fb & ~3U, color_end = (poly.fb + target_pixels * cbytes + 3) & ~3U;
	const UINT32 depth_start = poly.db & ~3U, depth_end = (poly.db + target_pixels * dbytes + 3) & ~3U;
	const UINT32 texture_bits = (texmode >= 4) ? ((texmode == 7) ? 32 : 16) : poly.bits;
	const UINT32 tex_start = poly.texbase & ~3U, tex_end = (poly.texbase + poly.tw * poly.th * texture_bits / 8 + 3) & ~3U;
	const UINT32 pal_start = poly.palette & ~3U, pal_end = (poly.palette + (1U << poly.bits) * ((poly.pmode == 3) ? 4 : 2) + 3) & ~3U;
	const INT32 tex_hit = (tex_start < color_end && color_start < tex_end) || (poly.depth_enabled && tex_start < depth_end && depth_start < tex_end);
	const INT32 pal_hit = (pal_start < color_end && color_start < pal_end) || (poly.depth_enabled && pal_start < depth_end && depth_start < pal_end);
	const INT32 independent = !(poly.depth_enabled && color_start < depth_end && depth_start < color_end)
		&& (!textured || !tex_hit) && (!textured || texmode >= 4 || !pal_hit);

	if (!independent) SnapshotFlush();

	const INT32 bilinear = (attr[3] >> 28) & 1;

	if (textured && poly.swizzled && !poly.complete_tiles) EdgeTileRanksPrepare(poly.tw, poly.th);

	if (textured && texmode < 4 && area >= (float)(1U << poly.bits)) {
		// decode the palette once unless this triangle's own writes can reach it
		UINT32 first = buffer_offset(x0, y0, stride, poly.linear);
		UINT32 last = buffer_offset(x1, y1, stride, poly.linear) + 1;
		UINT32 palette_end = poly.palette + (1U << poly.bits) * ((poly.pmode == 3) ? 4 : 2);
		INT32 color_hit = poly.palette < poly.fb + last * cbytes && palette_end > poly.fb + first * cbytes;
		INT32 depth_hit = poly.palette < poly.db + last * dbytes && palette_end > poly.db + first * dbytes;

		if ((!(control & (1 << 11)) || !color_hit) && (!poly.depth_enabled || !(control & (1 << 15)) || !depth_hit)) {
			for (UINT32 entry = 0; entry < (1U << poly.bits); entry++) {
				DecodedPalette[entry] = pixel_read(poly.palette + entry * ((poly.pmode == 3) ? 4 : 2), poly.pmode);
			}
			poly.decoded_palette = DecodedPalette;
		}
	}

	// a linear ARGB8888 texture is already in the sampler's format
	if (textured && texmode == 7 && !poly.swizzled && poly.texbase + (UINT64)poly.tw * poly.th * 4 <= RAM_SIZE) poly.linear_argb = 1;

	if (textured && bilinear && !poly.linear_argb && independent) {
		UINT32 key[7] = { poly.texbase, (texmode < 4) ? poly.palette : 0, poly.tw, poly.th, (UINT32)texmode, (texmode < 4) ? (UINT32)poly.pmode : 0, (UINT32)poly.swizzled };

		for (size_t i = 0; i < Snapshots->size(); i++) {
			if (!memcmp((*Snapshots)[i].key, key, sizeof(key))) {
				poly.pixels = (*Snapshots)[i].pixels;
				break;
			}
		}

		// only worth it when the texture is not much larger than what the triangle samples
		UINT32 texels = poly.tw * poly.th;
		UINT64 samples = (UINT64)(area * 0.5f);
		UINT64 box = (UINT64)(x1 - x0 + 1) * (y1 - y0 + 1);
		if (box < samples) samples = box;

		if (poly.pixels == NULL && texels <= samples * 4 && texels <= (SNAPSHOT_BYTES - nSnapshotBytes) / 4) {
			Y2Snapshot snap;
			memcpy(snap.key, key, sizeof(key));
			snap.pixels = (UINT32*)malloc(texels * sizeof(UINT32));
			if (snap.pixels) {
				for (UINT32 ty = 0; ty < poly.th; ty++) {
					for (UINT32 tx = 0; tx < poly.tw; tx++) snap.pixels[ty * poly.tw + tx] = Fetch(poly, tx, ty);
				}
				Snapshots->push_back(snap);
				nSnapshotBytes += texels * 4;
				poly.pixels = snap.pixels;
			}
		}
	}

	if (nQueued == Y2_QUEUE) RenderFlush();

	Y2Queued &q = RenderQueue[nQueued++];
	q.poly = poly;
	if (poly.decoded_palette) {
		memcpy(q.palette, poly.decoded_palette, (1U << poly.bits) * sizeof(UINT32));
		q.poly.decoded_palette = q.palette;
	}
	q.x0 = x0;
	q.x1 = x1;
	q.y0 = y0;
	q.y1 = y1;
	q.ab = ab;
	q.bc = bc;
	q.ca = ca;
	q.span = !bilinear ? RenderSpan<false, false> : (!poly.pixels ? RenderSpan<true, false> : RenderSpan<true, true>);
	if (textured && mode == 1 && !(poly.depth_enabled && poly.depth_wide)) {
		INT32 kind = 0;
		if (bilinear) kind = poly.pixels ? SPAN_SNAPSHOT : SPAN_BILINEAR;
		else if (texmode < 4 && poly.decoded_palette) kind = poly.bits;
		else if (texmode >= 4 && texmode <= 6) kind = 16;
		if (kind) q.span = SpanFastPick(kind, poly.perspective, poly.depth_enabled);
	}
	nQueuedPixels += (x1 - x0 + 1) * (y1 - y0 + 1);

	// a triangle that can read its own target is drawn on its own
	if (!independent) RenderFlush();

	Y2_STAT(nStatTriangles);
	return 1;
}

// ---------------------------------------------------------------------------
// background work: lists render, and movie frames decode and convert, on another thread while the cpu
// runs on. Everything the cpu can see is done at once on its thread: a first pass applies a list's
// register writes and checks it, the movie decoder and converter update their command rings and status.
// The pixel work is queued as jobs, run in order on the background thread, and the RAM pages they read
// (textures, references) and write (targets, depth, decoded frames) are protected: cpu accesses that
// could see or change them wait for all the work, and so does everything else that touches RAM.

#define PROTECT_READ	1	// background work reads the page: the cpu may not write it
#define PROTECT_WRITE	2	// background work writes the page: the cpu may not touch it

// per 4KB page of RAM: what the queued and the running jobs need, and what the cpu's map has
static UINT8 *PendingMap = NULL, *RunningMap = NULL, *AppliedMap = NULL;
static UINT32 nMapMin = ~0U, nMapMax = 0;
static UINT32 nDirtyMin = ~0U, nDirtyMax = 0;	// pages marked since the map was last brought up to date
static UINT32 nPendingGen = 0;				// changes whenever the queued jobs' map is emptied

enum { JOB_RENDER = 0, JOB_MVC, JOB_YUV };

struct Y2Job {
	INT32 type;
	const UINT32 *list;		// render
	UINT32 words;
	UINT32 cmd[4];			// movie decoder: the command, its registers and coefficient blocks
	UINT32 width, height, out, ref[2];
	UINT32 control, planes[3];	// converter
	UINT32 coef[6][32];
};

static std::vector<Y2Job> *JobsPending = NULL, *JobsRunning = NULL;
static INT32 bRenderBusy = 0;			// jobs are queued or running
static INT32 bBgRunning = 0;			// JobsRunning is on the background thread
static volatile INT32 bBgDone = 0;		// ... and it has finished them
static const UINT32 *BgListPending = NULL, *BgListRunning = NULL;
static threadystruct RenderThread;
static UINT32 RenderRdc[0x1100 / 4];

static inline UINT8 PageLevel(UINT32 page)
{
	return (PendingMap[page] > RunningMap[page]) ? PendingMap[page] : RunningMap[page];
}

static void FootprintMark(UINT32 start, UINT64 bytes, UINT8 level)
{
	if (bytes == 0 || start >= RAM_SIZE) return;
	UINT64 end = (UINT64)start + bytes - 1;
	if (end >= RAM_SIZE) end = RAM_SIZE - 1;

	UINT32 first = start >> 12, last = (UINT32)(end >> 12);
	for (UINT32 page = first; page <= last; page++) {
		if (PendingMap[page] < level) PendingMap[page] = level;
	}
	if (level == PROTECT_WRITE) RamWritten(start, end - start + 1);
	if (first < nMapMin) nMapMin = first;
	if (last > nMapMax) nMapMax = last;
	if (first < nDirtyMin) nDirtyMin = first;
	if (last > nDirtyMax) nDirtyMax = last;
}

// colour and depth targets draw_triangle can write, in whole 8-line tile rows (no depth: bytes[1] = 0)
static void TriangleTargets(UINT32 *start, UINT64 *bytes)
{
	const UINT32 stride = (RdcR[0xa0 / 4] & 0x7f8) + 8;
	const INT32 mode = (RdcR[0xa0 / 4] >> 16) & 3;
	const UINT32 ymax = (RdcR[0xa4 / 4] >> 16) & 0x7ff;
	const UINT64 pixels = (UINT64)((ymax / 8) + 1) * 8 * stride;
	const INT32 depth_enabled = (RdcR[0xa0 / 4] >> 23) & 1;

	start[0] = RdcR[0x9c / 4] & 0x0fffff00;
	bytes[0] = pixels * ((mode == 3) ? 4 : 2);
	start[1] = RdcR[0x98 / 4] & 0x0fffff00;
	bytes[1] = depth_enabled ? pixels * (((RdcR[0xa0 / 4] >> 22) & 1) ? 4 : 2) : 0;
}

// what draw_triangle can touch
static void FootprintTriangle(const UINT32 *attr)
{
	UINT32 start[2];
	UINT64 bytes[2];
	TriangleTargets(start, bytes);
	FootprintMark(start[0], bytes[0], PROTECT_WRITE);
	FootprintMark(start[1], bytes[1], PROTECT_WRITE);

	if (attr[0] & 1) {
		const INT32 texmode = (attr[2] >> 28) & 7;
		const UINT32 tw = ((attr[2] & 255) + 1) * 8, th = (((attr[2] >> 8) & 255) + 1) * 8;
		const UINT32 bits = (texmode == 0) ? 4 : 8;
		const UINT32 texture_bits = (texmode >= 4) ? ((texmode == 7) ? 32 : 16) : bits;
		FootprintMark((attr[3] & 0x7fffff) << 5, (UINT64)tw * th * texture_bits / 8, PROTECT_READ);

		if (texmode < 4) {
			const INT32 pmode = (RdcR[0xb0 / 4] >> 16) & 3;
			const UINT32 palette = (RdcR[((texmode == 0) ? 0xb4 : 0xbc) / 4] & 0x0ffffe00) + (((attr[2] >> 16) & 255) << bits) * ((pmode == 3) ? 4 : 2);
			FootprintMark(palette, (UINT64)(1U << bits) * ((pmode == 3) ? 4 : 2), PROTECT_READ);
		}
	}
}

static INT32 FootprintClear(UINT32 control)
{
	UINT32 base, bytes, db, depth_bytes;
	INT32 clear_color, clear_depth;
	if (!clear_ranges(control, &base, &bytes, &db, &depth_bytes, &clear_color, &clear_depth)) return 0;

	if (clear_color) FootprintMark(base, bytes, PROTECT_WRITE);
	if (clear_depth) FootprintMark(db, depth_bytes, PROTECT_WRITE);
	return 1;
}

static inline INT32 Finite(float f)
{
	return (f == f) && (f - f == 0);
}

// command lengths from the YG_Add*/YG_*Regset routines in the SDK
static const UINT8 GeParamWords[13] = { 2, 6, 10, 0, 4, 4, 2, 0, 4, 4, 4, 4, 8 };

// render 0: check the list only, 1: draw it, 2: first pass of a background render (see above)
static INT32 execute_list(const UINT32 *list, UINT32 words, INT32 render)
{
	UINT32 attr[4] = { 0, 0, 0, 0 };
	Y2Vertex strip[3];
	UINT32 vertices = 0;
	memset(strip, 0, sizeof(strip));

	for (UINT32 pos = 0; pos + 1 < words; ) {
		const UINT32 *p = list + pos;
		UINT32 command = p[0];
		UINT32 count = 0;

		switch (command >> 28) {
			case 0: count = 2; break;	// NOP
			case 1: count = (command == 0x10000001) ? 8 : (command == 0x10000002) ? 16 : 0; break;
			case 2: count = 4; break;
			case 6: count = ((command & 0xfffffff) < 13) ? GeParamWords[command & 0xfffffff] : 0; break;
			case 7: count = (command & 7) ? ((command & 8) ? 18 : 14) : 2; break;
			case 11: count = 4; break;	// attribute
			case 12: count = attr[0] ? (4 + 2 * ((attr[0] >> 17) & 1) + 2 * ((attr[0] >> 16) & 1)) : 0; break;
			case 15: return 1;
		}

		if (!count || count > words - pos) return 0;

		if ((command >> 28) == 11) {
			memcpy(attr, p, sizeof(attr));
			vertices = 0;
		}

		// only pre-transformed strips with diffuse/specular colors are decoded
		if ((command >> 28) == 12) {
			if ((attr[0] & 0x0ffcff20) || !(attr[0] & (1 << 17))) return 0;
			if (command & 1) vertices = 0;

			Y2Vertex v;
			v.x = u2f(p[1]);
			v.y = u2f(p[2]);
			v.z = u2f(p[3]);
			v.u = (attr[0] & (1 << 16)) ? u2f(p[6]) : 0.0f;
			v.v = (attr[0] & (1 << 16)) ? u2f(p[7]) : 0.0f;
			v.color = p[4];

			if (!Finite(v.x) || !Finite(v.y) || !Finite(v.z) || !Finite(v.u) || !Finite(v.v)
				|| fabsf(v.x) > 65536 || fabsf(v.y) > 65536 || fabsf(v.u) > 65536 || fabsf(v.v) > 65536) {
				return 0;
			}

			strip[vertices % 3] = v;
			if (++vertices >= 3 && render) {
				if (render == 2) {
					if (!triangle_supported(attr)) return 0;
					FootprintTriangle(attr);
				} else {
					if (!bRenderAsync) {
						UINT32 start[2];
						UINT64 bytes[2];
						TriangleTargets(start, bytes);
						RamWritten(start[0], bytes[0]);
						RamWritten(start[1], bytes[1]);
					}
					if (!draw_triangle(attr, strip[0], strip[1], strip[2])) return 0;
				}
			}
		}

		if (render) {
			if (render == 1 && ((command >> 28) == 2 || (command == 0x10000002 && memcmp(p + 2, &RdcR[0x98 / 4], 4 * 4)))) SnapshotFlush();

			if (command == 0x10000001) {
				memcpy(&RdcR[0x48 / 4], p + 2, 6 * 4);
			} else if (command == 0x10000002) {
				memcpy(&RdcR[0x98 / 4], p + 2, 14 * 4);
			} else if ((command >> 28) == 2) {
				if (render == 1 && !bRenderAsync) {
					UINT32 base, bytes, db, depth_bytes;
					INT32 clear_color, clear_depth;
					if (clear_ranges(p[1], &base, &bytes, &db, &depth_bytes, &clear_color, &clear_depth)) {
						if (clear_color) RamWritten(base, bytes);
						if (clear_depth) RamWritten(db, depth_bytes);
					}
				}
				if (!((render == 2) ? FootprintClear(p[1]) : buffer_clear(p[1], p[2], p[3]))) return 0;
			}
		}

		pos += count;
	}

	return 0;	// missing end command
}

// brings the cpu's map in line with the jobs' needs: the newly marked pages, or all of them when jobs ended
static void ProtectUpdate(INT32 full)
{
	UINT32 from = full ? nMapMin : nDirtyMin, to = full ? nMapMax : nDirtyMax;
	nDirtyMin = ~0U;
	nDirtyMax = 0;
	if (from > to) return;

	INT32 any = 0;
	for (UINT32 page = from; page <= to; ) {
		// most of the range is pages nothing ever protected: skip them 8 at a time
		if (!(page & 7) && page + 7 <= to) {
			UINT64 p, r, a;
			memcpy(&p, PendingMap + page, 8);
			memcpy(&r, RunningMap + page, 8);
			memcpy(&a, AppliedMap + page, 8);
			if ((p | r | a) == 0) { page += 8; continue; }
		}

		const UINT8 level = PageLevel(page);
		if (level) any = 1;
		if (level == AppliedMap[page]) {
			page++;
			continue;
		}

		const UINT32 first = page;
		while (page <= to && PageLevel(page) == level && AppliedMap[page] != level) AppliedMap[page++] = level;
		const UINT32 start = first << 12, end = ((page - 1) << 12) | 0xfff;

		if (level == PROTECT_WRITE) {
			Mips3MapHandler(3, start, end, MAP_READ | MAP_WRITE);
		} else {
			Mips3MapMemory(DrvRAM + start, start, end, MAP_READ | MAP_WRITE);
			if (level == PROTECT_READ) Mips3MapHandler(3, start, end, MAP_WRITE);
		}
	}

	if (full && !any) {
		nMapMin = ~0U;
		nMapMax = 0;
	}
}

static void ClearMap(UINT8 *map)
{
	if (nMapMin <= nMapMax) memset(map + nMapMin, 0, nMapMax - nMapMin + 1);
}

static void MvcExec(const Y2Job &job);
static void YuvExec(const Y2Job &job);

static void ExecJob(const Y2Job &job)
{
	switch (job.type) {
		case JOB_RENDER: {
			Y2_TIME_START();
			RdcR = RenderRdc;
			execute_list(job.list, job.words, 1);
			SnapshotFlush();
			RdcR = rdc;
			Y2_TIME_END(nTimeRender);
			break;
		}
		case JOB_MVC: {
			Y2_TIME_START();
			MvcExec(job);
			Y2_TIME_END(nTimeMvc);
			break;
		}
		case JOB_YUV: {
			Y2_TIME_START();
			YuvExec(job);
			Y2_TIME_END(nTimeYuv);
			break;
		}
	}
}

static void RenderThreadProc()
{
	for (size_t i = 0; i < JobsRunning->size(); i++) ExecJob((*JobsRunning)[i]);
	bBgDone = 1;
}

// waits for the running jobs, the queued ones stay queued
static void BgWait()
{
	if (!bBgRunning) return;

	Y2_TIME_START();
	RenderThread.notify_wait();
	Y2_TIME_END(nTimeWait);

	bBgRunning = 0;
	JobsRunning->clear();
	BgListRunning = NULL;
	ClearMap(RunningMap);
	ProtectUpdate(1);
	bRenderBusy = !JobsPending->empty();
}

// hands the queued jobs to the background thread, if it is free
static void BgLaunch()
{
	if (bBgRunning || JobsPending->empty()) return;

	std::vector<Y2Job> *jobs = JobsRunning;
	JobsRunning = JobsPending;
	JobsPending = jobs;

	UINT8 *map = RunningMap;
	RunningMap = PendingMap;
	PendingMap = map;
	nPendingGen++;

	BgListRunning = BgListPending;
	BgListPending = NULL;

	bBgDone = 0;
	bBgRunning = 1;
	RenderThread.notify();
}

static void BgQueue(const Y2Job &job)
{
	JobsPending->push_back(job);
	bRenderBusy = 1;
	ProtectUpdate(0);

	// a long stretch without lists: keep the queue short
	if (JobsPending->size() >= 1024) {
		BgWait();
		BgLaunch();
	}
}

// waits for all the background work: after this its results are in RAM
static void RenderSync()
{
	if (!bRenderBusy) return;

	BgWait();

	for (size_t i = 0; i < JobsPending->size(); i++) ExecJob((*JobsPending)[i]);
	JobsPending->clear();
	BgListPending = NULL;
	ClearMap(PendingMap);
	nPendingGen++;
	ProtectUpdate(1);
	bRenderBusy = 0;
}

// devices: wait only when the RAM they read (write) is written (used) by background work
static void RenderSyncRange(UINT32 start, UINT64 bytes, INT32 write)
{
	if (!bRenderBusy || bytes == 0 || start >= RAM_SIZE) return;
	UINT64 end = (UINT64)start + bytes - 1;
	if (end >= RAM_SIZE) end = RAM_SIZE - 1;

	for (UINT32 page = start >> 12; page <= (UINT32)(end >> 12); page++) {
		const UINT8 level = PageLevel(page);
		if (write ? level : (level == PROTECT_WRITE)) {
			RenderSync();
			return;
		}
	}
}

static inline UINT32 RamReadSynced(UINT32 a)
{
	if (bRenderBusy && PageLevel((a >> 12) & ((RAM_SIZE >> 12) - 1)) == PROTECT_WRITE) RenderSync();
	return Ram32(a);
}

// protected pages: the access waits for the background work, which gives the page back to RAM
static UINT8 ProtReadByte(UINT32 a) { RenderSync(); return DrvRAM[a]; }
static UINT16 ProtReadHalf(UINT32 a) { RenderSync(); return Ram16(a); }
static UINT32 ProtReadWord(UINT32 a) { RenderSync(); return Ram32(a); }
static UINT64 ProtReadDouble(UINT32 a) { RenderSync(); return Ram32(a) | ((UINT64)Ram32(a + 4) << 32); }
static void ProtWriteByte(UINT32 a, UINT8 d) { RenderSync(); RamWritten(a & ~7, 8); DrvRAM[a] = d; Mips3InvalidateRange(a, a); }
static void ProtWriteHalf(UINT32 a, UINT16 d) { RenderSync(); RamWritten(a & ~7, 8); RamW16(a, d); Mips3InvalidateRange(a, a + 1); }
static void ProtWriteWord(UINT32 a, UINT32 d) { RenderSync(); RamWritten(a & ~7, 8); RamW32(a, d); Mips3InvalidateRange(a, a + 3); }
static void ProtWriteDouble(UINT32 a, UINT64 d) { RenderSync(); RamWritten(a & ~7, 8); RamW32(a, (UINT32)d); RamW32(a + 4, (UINT32)(d >> 32)); Mips3InvalidateRange(a, a + 7); }

static void RenderAsyncInit()
{
	PendingMap = (UINT8*)calloc(RAM_SIZE >> 12, 1);
	RunningMap = (UINT8*)calloc(RAM_SIZE >> 12, 1);
	AppliedMap = (UINT8*)calloc(RAM_SIZE >> 12, 1);
	nMapMin = ~0U;
	nMapMax = 0;
	JobsPending = new std::vector<Y2Job>();
	JobsRunning = new std::vector<Y2Job>();
	bRenderBusy = bBgRunning = 0;
	BgListPending = BgListRunning = NULL;
	RdcR = rdc;

	Mips3SetReadByteHandler(3, ProtReadByte);
	Mips3SetReadHalfHandler(3, ProtReadHalf);
	Mips3SetReadWordHandler(3, ProtReadWord);
	Mips3SetReadDoubleHandler(3, ProtReadDouble);
	Mips3SetWriteByteHandler(3, ProtWriteByte);
	Mips3SetWriteHalfHandler(3, ProtWriteHalf);
	Mips3SetWriteWordHandler(3, ProtWriteWord);
	Mips3SetWriteDoubleHandler(3, ProtWriteDouble);
	Mips3SetCacheSyncCallback(RenderSync);

	if (bRenderAsync) RenderThread.init(RenderThreadProc);
}

static void RenderAsyncExit()
{
	RenderSync();
	if (bRenderAsync) RenderThread.exit();

	free(PendingMap);
	free(RunningMap);
	free(AppliedMap);
	PendingMap = RunningMap = AppliedMap = NULL;
	delete JobsPending;
	delete JobsRunning;
	JobsPending = JobsRunning = NULL;
}

static void render_start()
{
#if defined (FBNEO_DEBUG)
	if (Y2_TRACING) bprintf(PRINT_NORMAL, _T("Y2T %d line %3d: RC start, wants %08x, slots %08x/%d %08x/%d\n"), nTraceFrame, nCurrentLine, rdc[0x90 / 4], gfx_base[0], gfx_words[0], gfx_base[1], gfx_words[1]);
#endif
	for (INT32 slot = 0; slot < 2; slot++) {
		if (gfx_words[slot] && gfx_base[slot] == rdc[0x90 / 4]) {
			Y2_STAT(nStatLists);
#if defined (FBNEO_DEBUG)
			UINT32 trace_hash = Y2_TRACING ? ListHash(DrvGfxList + slot * GFX_LIST_WORDS, gfx_words[slot]) : 0;
			UINT32 trace_words = gfx_words[slot];
			if ((nTraceFrame % 600) < 9) {
				if ((nTraceFrame % 600) == 0) nTracePrevWords = 0;
				TraceListDiff(DrvGfxList + slot * GFX_LIST_WORDS, gfx_words[slot]);
			}
#endif
			INT32 done;
			if (bRenderAsync) {
				// the running jobs may still use the register copy; the queued ones (movie frames) go with the list
				BgWait();
				memcpy(RenderRdc, rdc, sizeof(rdc));
				done = execute_list(DrvGfxList + slot * GFX_LIST_WORDS, gfx_words[slot], 2);

				Y2Job job;
				job.type = JOB_RENDER;
				job.list = DrvGfxList + slot * GFX_LIST_WORDS;
				job.words = gfx_words[slot];
				BgListPending = job.list;
				JobsPending->push_back(job);
				bRenderBusy = 1;
				ProtectUpdate(0);
				BgLaunch();
			} else {
				Y2_TIME_START();
				done = execute_list(DrvGfxList + slot * GFX_LIST_WORDS, gfx_words[slot], 1);
				SnapshotFlush();
				Y2_TIME_END(nTimeRender);
			}
#if defined (FBNEO_DEBUG)
			if (Y2_TRACING) {
				bprintf(PRINT_NORMAL, _T("Y2T %d line %3d: RC wants %08x, slots %08x/%d %08x/%d, renders slot %d (%d words, hash %08x) into %08x\n"),
					nTraceFrame, nCurrentLine, rdc[0x90 / 4], gfx_base[0], gfx_words[0], gfx_base[1], gfx_words[1], slot, trace_words, trace_hash, rdc[0x9c / 4]);
			}
#endif
			if (done) {
				gfx_words[slot] = 0;
				rdc[0x10 / 4] |= 1;
			}
			return;
		}
	}
}

static void geometry_start()
{
	INT32 slot = gfx_slot;
	UINT32 *list = DrvGfxList + slot * GFX_LIST_WORDS;
	if (bRenderBusy && (list == BgListPending || list == BgListRunning)) RenderSync();
	gfx_words[slot] = 0;
	UINT32 words = 0;
	UINT32 glt = gec[0x34 / 4] & 0x0ffffff8;

	Y2_STAT(nStatGe);
	if (gec[0x30 / 4] & 0x10) return;	// memory-output mode not decoded

	for (INT32 entry = 0; entry < 4096 && glt < RAM_SIZE - 7; entry++, glt += 8) {
		UINT32 base = RamReadSynced(glt);
		UINT32 length = RamReadSynced(glt + 4);

		if (base == 0x80000000) {
			if (!execute_list(list, words, 0)) return;

			// a resubmission replaces whatever was decoded for this RC list address
			for (INT32 other = 0; other < 2; other++) {
				if (gfx_base[other] == rdc[0x30 / 4]) gfx_words[other] = 0;
			}

			gfx_base[slot] = rdc[0x30 / 4];
			gfx_words[slot] = words;
			gfx_slot ^= 1;
#if defined (FBNEO_DEBUG)
			if (Y2_TRACING) {
				bprintf(PRINT_NORMAL, _T("Y2T %d line %3d: GE decodes list for %08x into slot %d (%d words, hash %08x), auto render %d\n"),
					nTraceFrame, nCurrentLine, gfx_base[slot], slot, words, ListHash(list, words), rdc[0x2c / 4] & 1);
			}
#endif
			gec[0x18 / 4] = glt + 8;
			gec[0x10 / 4] |= 1;		// GE complete
			rdc[0x10 / 4] |= 0x10;	// SA complete, no overflow
			if (rdc[0x2c / 4] & 1) render_start();
			return;
		}

		UINT32 count = (length + 8) / 4;
		if ((base & 0xf0000007) || (length & 7) || length >= 0x100000 || base >= RAM_SIZE
			|| length + 8 > RAM_SIZE - base || count > GFX_LIST_WORDS - words) {
			break;
		}

		for (UINT32 i = 0; i < count; i++) list[words + i] = RamReadSynced(base + i * 4);
		words += count;
	}
}

// ---------------------------------------------------------------------------
// movie decoder (MVC) and YUV converter

static INT32 MvcSample(UINT32 base, UINT32 plane, UINT32 width, INT32 pw, INT32 ph, INT32 xx, INT32 yy)
{
	return RamRead8((base & 0x0fffffe0) + plane + Clamp(yy, 0, ph - 1) * width + Clamp(xx, 0, pw - 1));
}

// half-pel motion compensated prediction of one 8x8 block
static void MvcPredictBlock(UINT32 base, UINT32 vector, INT32 chroma, UINT32 bx, UINT32 by, UINT32 plane, UINT32 width, INT32 pw, INT32 ph, INT32 *pred)
{
	INT32 vx = (INT16)vector, vy = (INT16)(vector >> 16);
	if (chroma) {
		vx /= 2;
		vy /= 2;
	}
	INT32 hx = vx & 1, hy = vy & 1;
	INT32 sx0 = (INT32)bx + (vx >> 1), sy0 = (INT32)by + (vy >> 1);
	UINT32 origin = (base & 0x0fffffe0) + plane;

	// the whole reference block is inside the plane: no clamping
	if (sx0 >= 0 && sy0 >= 0 && sx0 + 8 + hx <= pw && sy0 + 8 + hy <= ph
		&& (UINT64)origin + (UINT64)(sy0 + 7 + hy) * width + sx0 + 8 + hx <= RAM_SIZE) {
		const UINT8 *src = DrvRAM + origin + sy0 * width + sx0;
		for (INT32 y = 0; y < 8; y++, src += width) {
			const UINT8 *s0 = src, *s1 = src + hy * width;
			if (!hx && !hy) {
				for (INT32 x = 0; x < 8; x++) pred[y * 8 + x] = s0[x];
			} else {
				for (INT32 x = 0; x < 8; x++) pred[y * 8 + x] = (s0[x] + s0[x + hx] + s1[x] + s1[x + hx] + 2) / 4;
			}
		}
		return;
	}

	for (INT32 y = 0; y < 8; y++) {
		for (INT32 x = 0; x < 8; x++) {
			INT32 sx = sx0 + x, sy = sy0 + y;
			INT32 a = MvcSample(base, plane, width, pw, ph, sx, sy);
			INT32 b = MvcSample(base, plane, width, pw, ph, sx + hx, sy);
			INT32 c = MvcSample(base, plane, width, pw, ph, sx, sy + hy);
			INT32 d = MvcSample(base, plane, width, pw, ph, sx + hx, sy + hy);
			pred[y * 8 + x] = (a + b + c + d + 2) / 4;
		}
	}
}

// one macroblock's pixels: residuals, prediction from the reference frames, output
static void MvcExec(const Y2Job &job)
{
	const UINT32 *cmd = job.cmd;
	const UINT32 type = cmd[0] >> 28;
	const UINT32 width = job.width, height = job.height;
	const UINT32 mx = ((cmd[0] >> 16) & 63) * 16, my = ((cmd[0] >> 8) & 63) * 16;
	INT32 next = 0;

	for (INT32 block = 0; block < 6; block++) {
		INT32 residual[64];
		memset(residual, 0, sizeof(residual));

		if (cmd[0] & (1 << (5 - block))) {
			// standard separable MPEG IDCT (TODO: hardware rounding and precision)
			const UINT32 *packed = job.coef[next++];
			INT32 coef[64];
			INT32 dc_only = 1;
			for (INT32 i = 0; i < 64; i++) {
				coef[i] = (INT16)(packed[i / 2] >> ((i & 1) * 16));
				if (i && coef[i]) dc_only = 0;
			}

			if (dc_only) {
				INT32 dc = RoundInt(coef[0] / 8.0);
				for (INT32 i = 0; i < 64; i++) residual[i] = dc;
			} else {
				// zero coefficients and rows are skipped: adding zeros leaves the sums unchanged, and the
				// remaining terms are added in the same order
				double tmp[8][8];
				INT32 rows[8], nrows = 0;
				for (INT32 v = 0; v < 8; v++) {
					INT32 cols[8], ncols = 0;
					double values[8];
					for (INT32 u = 0; u < 8; u++) {
						if (coef[v * 8 + u]) {
							cols[ncols] = u;
							values[ncols++] = coef[v * 8 + u];
						}
					}
					if (!ncols) continue;
					rows[nrows++] = v;
					for (INT32 x = 0; x < 8; x++) {
						const double *basis = MvcBasis[x];
						double sum = 0;
						for (INT32 i = 0; i < ncols; i++) sum += values[i] * basis[cols[i]];
						tmp[v][x] = sum;
					}
				}
				for (INT32 y = 0; y < 8; y++) {
					double basis[8];
					for (INT32 r = 0; r < nrows; r++) basis[r] = MvcBasis[y][rows[r]];
					for (INT32 x = 0; x < 8; x++) {
						double sum = 0;
						for (INT32 r = 0; r < nrows; r++) sum += tmp[rows[r]][x] * basis[r];
						residual[y * 8 + x] = RoundInt(sum);
					}
				}
			}
		}

		INT32 chroma = (block >= 4);
		INT32 pw = chroma ? (width / 2) : width;
		INT32 ph = chroma ? (height / 2) : height;
		UINT32 bx = chroma ? (mx / 2) : (mx + (block & 1) * 8);
		UINT32 by = chroma ? (my / 2) : (my + (block >> 1) * 8);
		UINT32 plane = chroma ? (width * height + (block - 4) * width / 2) : 0;

		INT32 pred[64], pred2[64];
		if (type == 1 || type == 3) MvcPredictBlock(job.ref[0], cmd[2], chroma, bx, by, plane, width, pw, ph, pred);
		if (type == 2) MvcPredictBlock(job.ref[1], cmd[3], chroma, bx, by, plane, width, pw, ph, pred);
		if (type == 3) {
			MvcPredictBlock(job.ref[1], cmd[3], chroma, bx, by, plane, width, pw, ph, pred2);
			for (INT32 i = 0; i < 64; i++) pred[i] = (pred[i] + pred2[i] + 1) / 2;
		}

		for (UINT32 y = 0; y < 8; y++) {
			UINT8 *dst = DrvRAM + job.out + plane + (by + y) * width + bx;
			for (UINT32 x = 0; x < 8; x++) {
				INT32 value = residual[y * 8 + x];
				if (type) value += pred[y * 8 + x];
				dst[x] = Clamp(value, 0, 255);
			}
		}
	}
}

// the command and block rings and the status: what the cpu sees, at once; the pixels are a job
static void mvc_process()
{
	if (!(mvc[0x28 / 4] & 1)) return;

	while (mvc_command_ready[mvc_command_pos]) {
		const UINT32 *cmd = &mvc_sram[mvc_command_pos * 4];
		UINT32 type = cmd[0] >> 28;

		if (type > 4 || mvc[0x50 / 4] != 0x211) return;

		if (type == 4) {
			mvc_command_ready[mvc_command_pos] = 0;
			mvc[0x28 / 4] = 0;
			mvc[0x10 / 4] |= 1;
			update_irqs();
			return;
		}

		INT32 needed = 0;
		for (INT32 b = 0; b < 6; b++) {
			if (cmd[0] & (1 << b)) {
				if (!mvc_block_ready[(mvc_block_pos + needed) % 18]) return;
				needed++;
			}
		}

		UINT32 width = (mvc[0x30 / 4] & 0x3f0) + 16;
		UINT32 height = ((mvc[0x30 / 4] >> 16) & 0x3f0) + 16;
		UINT32 mx = ((cmd[0] >> 16) & 63) * 16, my = ((cmd[0] >> 8) & 63) * 16;
		UINT32 out = mvc[0x34 / 4] & 0x0fffffe0;

		if (mx >= width || my >= height || out + width * height * 3 / 2 > RAM_SIZE) return;

		Y2Job job;
		job.type = JOB_MVC;
		memcpy(job.cmd, cmd, sizeof(job.cmd));
		job.width = width;
		job.height = height;
		job.out = out;
		job.ref[0] = mvc[0x38 / 4];
		job.ref[1] = mvc[0x3c / 4];

		INT32 blocks = 0;
		for (INT32 block = 0; block < 6; block++) {
			if (cmd[0] & (1 << (5 - block))) {
				memcpy(job.coef[blocks++], &mvc_sram[0x40 / 4 + mvc_block_pos * 32], 32 * sizeof(UINT32));
				mvc_block_ready[mvc_block_pos] = 0;
				mvc_block_pos = (mvc_block_pos + 1) % 18;
			}
		}

		Y2_STAT(nStatMvc);
		mvc_command_ready[mvc_command_pos] = 0;
		mvc_command_pos = (mvc_command_pos + 1) % 3;

		RamWritten(out, (UINT64)width * height * 3 / 2);

		if (bRenderAsync) {
			// the same frames for a whole picture: mark them once per queue
			static UINT32 last_gen = ~0U, last[4];
			const UINT32 refs = (type == 1 || type == 3) | ((type >= 2) << 1);
			const UINT64 bytes = (UINT64)width * height * 3 / 2;
			if (last_gen != nPendingGen || last[0] != out || last[1] != (job.ref[0] & 0x0fffffe0) || last[2] != (job.ref[1] & 0x0fffffe0) || last[3] != ((UINT32)bytes | (refs << 30))) {
				FootprintMark(out, bytes, PROTECT_WRITE);
				if (refs & 1) FootprintMark(job.ref[0] & 0x0fffffe0, bytes, PROTECT_READ);
				if (refs & 2) FootprintMark(job.ref[1] & 0x0fffffe0, bytes, PROTECT_READ);
				last_gen = nPendingGen;
				last[0] = out;
				last[1] = job.ref[0] & 0x0fffffe0;
				last[2] = job.ref[1] & 0x0fffffe0;
				last[3] = (UINT32)bytes | (refs << 30);
			}
			BgQueue(job);
		} else {
			ExecJob(job);
		}
	}
}

static void mvc_w(INT32 offset, UINT32 data, UINT32 mask)
{
	if (offset == 0x10 / 4) {
		mvc[offset] = (mvc[offset] & ~(data & mask & 15)) | (data & mask & 0x1f30);
	} else {
		COMBINE(mvc[offset], data, mask);
	}

	if (offset == 0x20 / 4 || (offset == 0 && (data & mask & 1))) {
		memset(mvc_command_ready, 0, sizeof(mvc_command_ready));
		memset(mvc_block_ready, 0, sizeof(mvc_block_ready));
		mvc_command_pos = mvc_block_pos = 0;
		mvc[0x28 / 4] = offset ? 1 : 0;
		mvc[0x10 / 4] &= ~15U;
	}

	if (offset == 0x24 / 4) {
		mvc[0x28 / 4] = 0;
		mvc[0x10 / 4] |= 2;
	}

	mvc_retry = 1;
	update_irqs();
}

static void mvc_sram_w(INT32 offset, UINT32 data, UINT32 mask)
{
	COMBINE(mvc_sram[offset], data, mask);

	// YG_MVC_SetCommand writes +0/+8/+c, SetBlock writes 64 coefficients; three commands share an 18-block ring.
	// Nothing new can progress until a command or a block is complete, or a register changed.
	INT32 process = mvc_retry;
	if (offset < 0x30 / 4 && (offset & 3) == 3) {
		mvc_command_ready[offset / 4] = 1;
		process = 1;
	}
	if (offset >= 0x40 / 4 && ((offset - 0x40 / 4) & 31) == 31) {
		mvc_block_ready[(offset - 0x40 / 4) / 32] = 1;
		process = 1;
	}
	if (process) {
		mvc_retry = 0;
		mvc_process();
	}
}

static const Y2Job *YuvJob = NULL;

// CFT_Yge: planar 4:2:0 input with interleaved U/V rows, the format bits select the framebuffer packing
static void YuvRows(INT32 part)
{
	const Y2Job &job = *YuvJob;
	const UINT32 width = job.width, height = job.height;
	const INT32 mode = (job.control >> 12) & 3;
	// ARGB8888 output (CONTROL 0xb100) is always tiled, 16-bit output (0xd100) only with bit 14 set
	const INT32 tiled = (mode == 3) || (job.control & 0x4000);

	for (UINT32 y = part; y < height; y += nRenderParts) {
		for (UINT32 x = 0; x < width; x++) {
			INT32 yy = RamRead8(job.planes[0] + y * width + x) - 16;
			INT32 u = RamRead8(job.planes[1] + (y / 2) * width + x / 2) - 128;
			INT32 v = RamRead8(job.planes[2] + (y / 2) * width + x / 2) - 128;
			INT32 r = Clamp((298 * yy + 409 * v + 128) >> 8, 0, 255);
			INT32 g = Clamp((298 * yy - 100 * u - 208 * v + 128) >> 8, 0, 255);
			INT32 b = Clamp((298 * yy + 516 * u + 128) >> 8, 0, 255);
			UINT32 index = tiled ? texture_offset(x, y, width, height) : (y * width + x);
			UINT32 argb = 0xff000000 | (r << 16) | (g << 8) | b;
			if (mode == 3) {
				RamW32(job.out + index * 4, argb);
			} else {
				pixel_write(job.out + index * 2, mode, argb);
			}
		}
	}
}

// rows are shared out: the two pixels of a 32-bit word are always on the same row
static void YuvExec(const Y2Job &job)
{
	const INT32 mode = (job.control >> 12) & 3;
	if ((mode == 3) || (job.control & 0x4000)) EdgeTileRanksPrepare(job.width, job.height);

	YuvJob = &job;
	WorkerRows = YuvRows;
	nRenderParts = nY2Workers + 1;
	for (INT32 i = 0; i < nY2Workers; i++) Y2Workers[i].notify();
	YuvRows(0);
	for (INT32 i = 0; i < nY2Workers; i++) Y2Workers[i].notify_wait();
	WorkerRows = RenderRows;
	YuvJob = NULL;
}

static void yuv_w(INT32 offset, UINT32 data, UINT32 mask)
{
	if (offset == 0x10 / 4) {
		yuv[offset] &= ~(data & mask & 3);
	} else {
		COMBINE(yuv[offset], data, mask);
	}

	if (offset == 0x20 / 4) {
		UINT32 control = yuv[0x30 / 4];
		UINT32 width = (yuv[0x38 / 4] & 0x3f0) + 16;
		UINT32 height = ((yuv[0x38 / 4] >> 16) & 0x3f0) + 16;
		INT32 mode = (control >> 12) & 3;
		UINT32 out = yuv[0x3c / 4] & 0x0fffffe0;

		if ((control & ~0x7000U) == 0x8100 && out + width * height * ((mode == 3) ? 4 : 2) <= RAM_SIZE) {
			Y2Job job;
			job.type = JOB_YUV;
			job.control = control;
			job.width = width;
			job.height = height;
			job.out = out;
			for (INT32 i = 0; i < 3; i++) job.planes[i] = yuv[(0x40 / 4) + i] & 0x0fffffff;
			RamWritten(out, (UINT64)width * height * ((mode == 3) ? 4 : 2));

			if (bRenderAsync) {
				for (INT32 i = 0; i < 3; i++) FootprintMark(job.planes[i], (UINT64)width * height, PROTECT_READ);
				FootprintMark(out, (UINT64)width * height * ((mode == 3) ? 4 : 2), PROTECT_WRITE);
				BgQueue(job);
			} else {
				ExecJob(job);
			}

			Y2_STAT(nStatYuv);
			yuv[0x10 / 4] = (yuv[0x10 / 4] & ~0x100U) | 1;
		}
	}

	update_irqs();
}

// ---------------------------------------------------------------------------
// device registers

static void dma_w(INT32 offset, UINT32 data, UINT32 mask)
{
	UINT32 reg = offset * 4;

	if (reg == 0x1c || reg == 0x2c || reg == 0x5c || reg == 0x6c) {
		// STT bit 31 acknowledges completion, bit 0 starts a transfer
		dma[offset] &= ~(data & mask & 0x80000000);

		if (data & mask & 1) {
			// YTFDMA_Start: physical addresses and a byte count, only linear bus-to-RAM transfers (TODO: timing)
			UINT32 source = dma[offset - 3];
			UINT32 dest = dma[offset - 2];
			UINT32 size = dma[offset - 1];

			RenderSyncRange(dest, size, 1);
			RenderSyncRange(source, size, 0);

			if (dest < RAM_SIZE && size <= RAM_SIZE - dest && !(size & 3) && !(dest & 3) && size <= 0x2000000) {
				RamWritten(dest, size);
				UINT32 pos = 0;
				while (pos < size) {
					UINT32 src = source + pos;
					// NAND pages are copied whole, their bytes are already in the guest's order
					if (src >= 0x80000000 && src < 0xf0000000 && !(src & 3)) {
						INT32 chip = (src >= 0xc0000000);
						UINT32 nand = src - (chip ? 0xc0000000 : 0x80000000);
						UINT32 chunk = 0x1000 - (nand & 0xfff);
						if (chunk > size - pos) chunk = size - pos;
						memcpy(DrvRAM + dest + pos, NandPage(chip, nand) + (nand & 0xfff), chunk);
						pos += chunk;
					} else {
						RamW32(dest + pos, BusRead32(src));
						pos += 4;
					}
				}
				if (size) Mips3InvalidateRange(dest, dest + size - 1);
				Y2_STAT(nStatDma);
				dma[offset] = 0x80000000;
			}
		}
	} else {
		COMBINE(dma[offset], data, mask);
	}

	update_irqs();
}

static void voc_w(INT32 offset, UINT32 data, UINT32 mask)
{
	if (offset == 0x10 / 4) {
		voc[offset] &= ~(data & mask);	// YG_BeginFrame/EndFrame acknowledge with ones
	} else {
		COMBINE(voc[offset], data, mask);
	}

	if (offset == 0x70 / 4) {
		Y2_STAT(nStatFlips);
#if defined (FBNEO_DEBUG)
		if (Y2_TRACING) bprintf(PRINT_NORMAL, _T("Y2T %d line %3d: flip to %08x\n"), nTraceFrame, nCurrentLine, voc[0x70 / 4]);
#endif
	}

	if (offset == 0 && (voc[0] & 0xffff0000) == 0x59470000) {
		vint_line = -1;
		if (voc[0] & 1) {
			schedule_vint(0);
		} else {
			voc[0x10 / 4] = 0;
		}
	}

	update_irqs();
}

static UINT32 voc_r(INT32 offset)
{
	if (offset == 0x0c / 4) {
		INT32 hpos = (INT32)((double)(CpuCycles() - nLineStart) * H_TOTAL / CyclesPerLine);
		if (hpos >= H_TOTAL) hpos = H_TOTAL - 1;
		return (nCurrentLine << 16) | hpos;
	}

	return voc[offset] | ((offset == 0x10 / 4 && (voc[0x14 / 4] & 0x80000000) && (voc[offset] & voc[0x14 / 4] & 0x307)) ? 0x80000000 : 0);
}

static void rdc_w(INT32 offset, UINT32 data, UINT32 mask)
{
	if (offset == 0x10 / 4) {
		rdc[offset] &= ~(data & mask);
	} else {
		COMBINE(rdc[offset], data, mask);
	}

	if (offset == 0x80 / 4) render_start();

	update_irqs();
}

static void gec_w(INT32 offset, UINT32 data, UINT32 mask)
{
	if (offset == 0x10 / 4) {
		gec[offset] &= ~(data & mask);
	} else {
		COMBINE(gec[offset], data, mask);
	}

	if (offset == 0x20 / 4) geometry_start();

	update_irqs();
}

static UINT32 irq_r(INT32 offset)
{
	if (offset == 0) return irq_sources;
	if (offset == 9) return (timer[1][3] & 1) ? (1U << 20) : 0;
	if (offset >= 4 && offset < 9) return irq_sources & int_select[offset - 4];
	if (offset >= 12 && offset < 17) return int_select[offset - 12];
	return 0;
}

static void irq_w(INT32 offset, UINT32 data, UINT32 mask)
{
	if (offset >= 12 && offset < 17) COMBINE(int_select[offset - 12], data, mask);
	update_irqs();
}

static UINT32 coin_r()
{
	// per chute: active-low contact at bit 0, pulse count at bits 2..4
	return (DrvInputs[2] & 0x0101) | (coin_pending[0] << 2) | (coin_pending[1] << 10);
}

static void coin_w(UINT32 data, UINT32 mask)
{
	// _BDLib_JAMMA_InputCoin: bits 0/4 acknowledge one pulse, bits 2..3/6..7 drive the coin meters
	UINT16 command = (data & mask) >> 16;

	for (INT32 chute = 0; chute < 2; chute++) {
		if ((command & (1 << (chute * 4))) && coin_pending[chute]) coin_pending[chute]--;
	}
}

static void coin_control_w(UINT32 data, UINT32 mask)
{
	// upper byte 0xbd is the write key, bits 0/4 reset the input queues, bits 2/6 are the lockouts
	UINT16 value = (data & mask) >> 16;
	if ((mask & 0xff000000) != 0xff000000 || (value & 0xff00) != 0xbd00) return;

	coin_control = (coin_control & ~(mask >> 16)) | (value & 0x0044);

	for (INT32 chute = 0; chute < 2; chute++) {
		if (value & (1 << (chute * 4))) coin_pending[chute] = 0;
	}
}

static UINT16 rtc_r(INT32 offset)
{
	return (offset == 0x1f) ? 2 : (offset == 0x1e) ? 0 : rtc[offset];
}

static void rtc_w(INT32 offset, UINT16 data, UINT16 mask)
{
	// FPGA RTC bridge: 0xbd is the write key, the low byte is the register data (TODO: clock ticking)
	if ((data & mask & 0xff00) == 0xbd00 && offset < 0x1e) rtc[offset] = data & 0xff;
}

static UINT32 nand_status_r(INT32 offset)
{
	// BDFNAND probes both NAND08GW3B2CN6 IDs at +10/+18; two populated sockets select the 0x70000000 window
	switch (offset * 4) {
		case 0x10: case 0x18: return 0x9551d320;
		case 0x14: case 0x1c: return 0x00000058;
	}
	return 0;	// +30: no uncorrectable/bad-block event, ECC not modelled
}

static void jvs_w(INT32 offset, UINT32 data, UINT32 mask)
{
	// FPGA JVS bridge: upper halfword is W1C status, bit 0 transmits the SRAM packet (TODO: JVS devices)
	jvs_status[offset] &= ~((data & mask) >> 16);
	if (data & mask & 1) jvs_fire[offset] = CpuCycles() + CPU_CLOCK / 500;	// approximate bridge timeout (2ms)
}

static UINT32 Y2Read32(UINT32 a)
{
	if (a >= 0x17000000 && a < 0x17001100) {
		UINT32 off = (a - 0x17000000) / 4;
		return rdc[off] | ((off == 0x10 / 4 && (rdc[off] & rdc[0x14 / 4] & 0x0f77)) ? 0x80000000 : 0);
	}
	if (a >= 0x17002000 && a < 0x17002080) return gec[(a - 0x17002000) / 4];
	if (a >= 0x17003000 && a < 0x170031c4) return voc_r((a - 0x17003000) / 4);
	if (a >= 0x17006000 && a < 0x17006060) return mvc[(a - 0x17006000) / 4];
	if (a >= 0x17007000 && a < 0x17007200) return yuv[(a - 0x17007000) / 4];
	if (a >= 0x17008000 && a < 0x17008940) return mvc_sram[(a - 0x17008000) / 4];
	if (a >= 0x18000000 && a < 0x18000044) return irq_r((a - 0x18000000) / 4);
	if (a == 0x18000050) return 4;	// SYS_MODE 4: full ELF header at +0x100 and the direct NAND path
	if (a == 0x18000058) return scc_sys_data;
	if (a == 0x18001000) return ioc[0];
	if (a >= 0x18001010 && a < 0x1800101c) return ioc[1 + (a - 0x18001010) / 4];
	if (a >= 0x18002070 && a < 0x18002080) return timer_r(0, (a - 0x18002070) / 4);
	if (a >= 0x18002080 && a < 0x18002090) return timer_r(1, (a - 0x18002080) / 4);
	if (a >= 0x18003000 && a < 0x18003080) return dma[(a - 0x18003000) / 4];

	switch (a) {
		case 0xf0000010: return RPP324HostRead(0) | (RPP324HostRead(1) << 16);
		case 0xf0000020: return RPP324ControlRead();
		case 0xf0000024: return RPP324StatusRead();
		case 0xf0400010: return DrvInputs[0] | (DrvInputs[1] << 16);
		case 0xf0400030: return coin_r();
		case 0xf0400034: return (UINT32)coin_control << 16;
		case 0xf0540000: return 0xff;		// volume potentiometer at 100%
		case 0xf05c0010: return 0xffffffc0 | DrvDips[0];
		case 0xf0600018: return jvs_status[0] << 16;
		case 0xf060001c: return jvs_status[1] << 16;
		case 0xf0600020: return jvs_config[0];
		case 0xf0600024: return jvs_config[1];
	}

	if (a >= 0xf0500000 && a < 0xf0500040) {
		INT32 off = (a - 0xf0500000) / 2;
		return rtc_r(off) | (rtc_r(off + 1) << 16);
	}
	if (a >= 0xf0800000 && a < 0xf0800034) return nand_status_r((a - 0xf0800000) / 4);

	return 0;
}

static void Y2Write32(UINT32 a, UINT32 data, UINT32 mask)
{
	if (a >= 0x17000000 && a < 0x17001100) { rdc_w((a - 0x17000000) / 4, data, mask); return; }
	if (a >= 0x17002000 && a < 0x17002080) { gec_w((a - 0x17002000) / 4, data, mask); return; }
	if (a >= 0x17003000 && a < 0x170031c4) { voc_w((a - 0x17003000) / 4, data, mask); return; }
	if (a >= 0x17006000 && a < 0x17006060) { mvc_w((a - 0x17006000) / 4, data, mask); return; }
	if (a >= 0x17007000 && a < 0x17007200) { yuv_w((a - 0x17007000) / 4, data, mask); return; }
	if (a >= 0x17008000 && a < 0x17008940) { mvc_sram_w((a - 0x17008000) / 4, data, mask); return; }
	if (a >= 0x18000000 && a < 0x18000044) { irq_w((a - 0x18000000) / 4, data, mask); return; }
	if (a == 0x18000058) { COMBINE(scc_sys_data, data, mask); return; }	// firmware stores its NMI hook here
	if (a == 0x18001000) { COMBINE(ioc[0], data, mask); return; }
	if (a >= 0x18001010 && a < 0x1800101c) { COMBINE(ioc[1 + (a - 0x18001010) / 4], data, mask); return; }
	if (a >= 0x18002070 && a < 0x18002080) { timer6_w((a - 0x18002070) / 4, data, mask); return; }
	if (a >= 0x18002080 && a < 0x18002090) { timer7_w((a - 0x18002080) / 4, data, mask); return; }
	if (a >= 0x18003000 && a < 0x18003080) { dma_w((a - 0x18003000) / 4, data, mask); return; }

	switch (a) {
		case 0xf0000010:
			if (mask & 0x000000ff) {
				Y2_STAT(nStatSnd);
				RPP324HostWrite(0, data & 0xffff);
			}
			if (mask & 0x00ff0000) RPP324HostWrite(1, data >> 16);
			return;
		case 0xf0000020: RPP324ControlWrite((UINT16)data, (UINT16)mask); return;
		case 0xf0400030: coin_w(data, mask); return;
		case 0xf0400034: coin_control_w(data, mask); return;
		case 0xf0600018: jvs_w(0, data, mask); return;
		case 0xf060001c: jvs_w(1, data, mask); return;
		case 0xf0600020: COMBINE(jvs_config[0], data, mask); return;
		case 0xf0600024: COMBINE(jvs_config[1], data, mask); return;
	}

	if (a >= 0xf0500000 && a < 0xf0500040) {
		INT32 off = (a - 0xf0500000) / 2;
		if (mask & 0xffff) rtc_w(off, data & 0xffff, mask & 0xffff);
		if (mask >> 16) rtc_w(off + 1, data >> 16, mask >> 16);
	}
}

static UINT8 Y2ReadByte(UINT32 a) { return Y2Read32(a & ~3) >> ((a & 3) * 8); }
static UINT16 Y2ReadHalf(UINT32 a) { return Y2Read32(a & ~3) >> ((a & 2) * 8); }
static UINT32 Y2ReadWord(UINT32 a) { return Y2Read32(a & ~3); }
static UINT64 Y2ReadDouble(UINT32 a) { return Y2Read32(a & ~7) | ((UINT64)Y2Read32((a & ~7) + 4) << 32); }
static void Y2WriteByte(UINT32 a, UINT8 d) { INT32 s = (a & 3) * 8; Y2Write32(a & ~3, (UINT32)d << s, 0xffU << s); }
static void Y2WriteHalf(UINT32 a, UINT16 d) { INT32 s = (a & 2) * 8; Y2Write32(a & ~3, (UINT32)d << s, 0xffffU << s); }
static void Y2WriteWord(UINT32 a, UINT32 d) { Y2Write32(a & ~3, d, ~0U); }
static void Y2WriteDouble(UINT32 a, UINT64 d) { Y2Write32(a & ~7, (UINT32)d, ~0U); Y2Write32((a & ~7) + 4, (UINT32)(d >> 32), ~0U); }

// ---------------------------------------------------------------------------

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvSndROM		= Next; Next += nSndROMLen;
	DrvBoot			= Next; Next += 0x1000;

	NandDataLut		= (UINT16*)Next; Next += 0x10000 * sizeof(UINT16);
	NandKeyLut		= (UINT16*)Next; Next += 0x10000 * sizeof(UINT16);
	NandCache		= (NandCacheLine*)Next; Next += NAND_CACHE * sizeof(NandCacheLine);

	DrvGfxList		= (UINT32*)Next; Next += 2 * GFX_LIST_WORDS * sizeof(UINT32);

	AllRam			= Next;

	DrvRAM			= Next; Next += RAM_SIZE;
	DrvBackup		= Next; Next += BACKUP_SIZE;
	DrvJvsRAM		= Next; Next += 0x4000;
	DrvRamWritten	= Next; Next += RAM_SIZE >> 12;
	TrackMap		= Next; Next += RAM_SIZE >> 12;
	LoggedGen		= (UINT32*)Next; Next += (RAM_SIZE >> 12) * sizeof(UINT32);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvDoReset()
{
	RenderSync();

	// only the written pages can be non-zero
	for (UINT32 page = 0; page < (RAM_SIZE >> 12); page++) {
		if (DrvRamWritten[page]) memset(DrvRAM + (page << 12), 0, 0x1000);
	}
	memset(DrvRamWritten, 0, RAM_SIZE >> 12);
	CheckpointDrop();
	RamWrittenRestart();
	memset(DrvJvsRAM, 0, 0x4000);

	memset(voc, 0, sizeof(voc));
	memset(rdc, 0, sizeof(rdc));
	memset(gec, 0, sizeof(gec));
	memset(int_select, 0, sizeof(int_select));
	memset(dma, 0, sizeof(dma));
	memset(jvs_status, 0, sizeof(jvs_status));
	memset(jvs_config, 0, sizeof(jvs_config));
	memset(gfx_base, 0, sizeof(gfx_base));
	memset(gfx_words, 0, sizeof(gfx_words));
	gfx_slot = 0;
	memset(mvc, 0, sizeof(mvc));
	memset(mvc_sram, 0, sizeof(mvc_sram));
	memset(mvc_command_ready, 0, sizeof(mvc_command_ready));
	memset(mvc_block_ready, 0, sizeof(mvc_block_ready));
	mvc_command_pos = mvc_block_pos = 0;
	mvc_retry = 0;
	memset(yuv, 0, sizeof(yuv));
	memset(timer, 0, sizeof(timer));
	memset(timer_epoch, 0, sizeof(timer_epoch));
	memset(coin_pending, 0, sizeof(coin_pending));
	coin_control = 0;
	scc_sys_data = 0;
	memset(ioc, 0, sizeof(ioc));
	vint_line = -1;
	vint_event = 0;
	memset(display, 0, sizeof(display));
	timer7_fire = NEVER;
	jvs_fire[0] = jvs_fire[1] = NEVER;

	// unidentified first-stage boot (as MAME): the NAND loader runs from RAM 0x00100000
	RamWritten(0x100000, 0x80000);
	for (UINT32 i = 0; i < 0x80000; i += 4) RamW32(0x100000 + i, NandRead32(0x80000200 + i));

	Mips3Reset();
	Mips3InvalidateRange(0x100000, 0x17ffff);

	RPP324Reset();

	update_irqs();
}

static INT32 DrvInit(UINT16 seed, const UINT8 *data_bits, const UINT8 *key_bits, INT32 snd_roms, UINT32 state_megabytes)
{
	ScanMainRamInit(state_megabytes);

	nSndROMLen = snd_roms * 0x8000000;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	for (INT32 i = 0; i < snd_roms; i++) {
		if (BurnLoadRom(DrvSndROM + i * 0x8000000, i, 1)) return 1;
	}

	for (INT32 i = 0; i < 2; i++) {
		Nand[i] = BurnHDDOpen(i);
		if (Nand[i] == NULL) return 1;
		if (BurnHDDGetSize(Nand[i]) != (UINT64)NAND_RECORD * NAND_RECORDS) {
			bprintf(PRINT_ERROR, _T("Y2: NAND %d is not a 0x42000000 byte dump\n"), i);
			return 1;
		}
	}

	nNandSeed = seed;
	pNandDataBits = data_bits;
	pNandKeyBits = key_bits;
	NandInitDecrypt();

	if (memcmp(NandPage(0, 0), "YATA_BOOT_HEADER", 16)) {
		bprintf(PRINT_ERROR, _T("Y2: no boot header in NAND U101\n"));
		return 1;
	}

	for (INT32 n = 0; n < 64; n++) {
		TextureMorton[n] = 0;
		for (INT32 bit = 0; bit < 6; bit++) TextureMorton[n] |= ((n >> bit) & 1) << (bit * 2);
	}

	for (INT32 x = 0; x < 8; x++) {
		for (INT32 u = 0; u < 8; u++) {
			MvcBasis[x][u] = (u ? 0.5 : sqrt(0.125)) * cos((2 * x + 1) * u * 3.14159265358979323846 / 16);
		}
	}

	// reset vector: jump to the loader (lui t0, 0x8010; jr t0; nop)
	DrvBoot[0] = 0x10; DrvBoot[1] = 0x80; DrvBoot[2] = 0x08; DrvBoot[3] = 0x3c;
	DrvBoot[4] = 0x08; DrvBoot[5] = 0x00; DrvBoot[6] = 0x00; DrvBoot[7] = 0x01;

	Mips3Init(MIPS3_TYPE_VR5500);
	Mips3MapMemory(DrvRAM,		0x00000000, 0x0fffffff, MAP_RAM);
	Mips3MapMemory(DrvBoot,		0x1fc00000, 0x1fc00fff, MAP_ROM);

	Mips3SetReadByteHandler(1, NandReadByte);
	Mips3SetReadHalfHandler(1, NandReadHalf);
	Mips3SetReadWordHandler(1, NandReadWord);
	Mips3SetReadDoubleHandler(1, NandReadDouble);
	Mips3MapHandler(1, 0x80000000, 0xefffffff, MAP_READ);

	Mips3SetReadByteHandler(2, Y2ReadByte);
	Mips3SetReadHalfHandler(2, Y2ReadHalf);
	Mips3SetReadWordHandler(2, Y2ReadWord);
	Mips3SetReadDoubleHandler(2, Y2ReadDouble);
	Mips3SetWriteByteHandler(2, Y2WriteByte);
	Mips3SetWriteHalfHandler(2, Y2WriteHalf);
	Mips3SetWriteWordHandler(2, Y2WriteWord);
	Mips3SetWriteDoubleHandler(2, Y2WriteDouble);
	Mips3MapHandler(2, 0x17000000, 0x18ffffff, MAP_READ | MAP_WRITE);
	Mips3MapHandler(2, 0xf0000000, 0xffffffff, MAP_READ | MAP_WRITE);

	// BDLib backup memory: three 0x500-byte records, then 0x18000 bytes of user data at +0x1000 (TODO: SRAM size)
	Mips3MapMemory(DrvBackup,	0xf0200000, 0xf0218fff, MAP_RAM);
	Mips3MapMemory(DrvJvsRAM,	0xf0601000, 0xf0604fff, MAP_RAM);

	memset(rtc, 0, sizeof(rtc));
	rtc[2] = 0x80;	// lost backup: the firmware initializes the calendar itself

	BurnSetRefreshRate((double)PIXEL_CLOCK / (H_TOTAL * V_TOTAL));

	Snapshots = new std::vector<Y2Snapshot>();
	RenderInit();
	RenderAsyncInit();

	RPP324Init(DrvSndROM, nSndROMLen, CpuCycles, CPU_CLOCK);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	ScanMainRamExit();
	CheckpointExit();
	RenderAsyncExit();
	Mips3Exit();
	RPP324Exit();

	RenderExit();
	EdgeTileRanksExit();
	SnapshotFlush();
	delete Snapshots;
	Snapshots = NULL;

	for (INT32 i = 0; i < 2; i++) {
		BurnHDDClose(Nand[i]);
		Nand[i] = NULL;
	}

	BurnFree(AllMem);
	BurnFree(DrawLut);
	DrawLut = NULL;

	return 0;
}

static void DrawLutUpdate()
{
	if (DrawLut && !DrvRecalc && nDrawLutBpp == nBurnBpp) return;
	if (DrawLut == NULL) DrawLut = (UINT32*)BurnMalloc(2 * 0x10000 * sizeof(UINT32));

	for (INT32 p = 0; p < 0x10000; p++) {
		INT32 r = (p >> 10) & 0x1f, g = (p >> 5) & 0x1f, b = p & 0x1f;
		DrawLut[p] = BurnHighCol((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2), 0);
		r = (p >> 11) & 0x1f; g = (p >> 5) & 0x3f; b = p & 0x1f;
		DrawLut[0x10000 + p] = BurnHighCol((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2), 0);
	}

	nDrawLutBpp = nBurnBpp;
	DrvRecalc = 0;
}

// 16-bit scanout straight from RAM: 8 contiguous pixels per tile row in tiled mode
template <typename T>
static void DrawFast16(const UINT32 *lut, UINT32 base, INT32 width, INT32 height, INT32 linear, UINT32 back)
{
	for (INT32 y = 0; y < 480; y++) {
		T *dst = (T*)(pBurnDraw + y * nBurnPitch);
		INT32 x = 0;

		if (y < height) {
			const UINT32 row = linear ? y * width : (y / 8) * (width / 8) * 64 + (y & 7) * 8;
			const INT32 visible = (width < 640) ? width : 640;

			if (linear) {
				const UINT16 *src = (const UINT16*)(DrvRAM + base + row * 2);
				for (; x < visible; x++) dst[x] = (T)lut[BURN_ENDIAN_SWAP_INT16(src[x])];
			} else {
				for (; x < visible; x += 8) {
					const UINT16 *src = (const UINT16*)(DrvRAM + base + (row + (x / 8) * 64) * 2);
					for (INT32 i = 0; i < 8; i++) dst[x + i] = (T)lut[BURN_ENDIAN_SWAP_INT16(src[i])];
				}
			}
		}

		for (; x < 640; x++) dst[x] = (T)back;
	}
}

static INT32 DrvDraw()
{
	if (pBurnDraw == NULL) return 0;

	DrawLutUpdate();

	// primary output, packing from YG_Flip/pixtype2regmode (TODO: second plane, gamma)
	UINT32 base = display[DISP_BASE];
	UINT32 size = display[DISP_SIZE];
	INT32 width = (size & 0x7f8) + 8;
	INT32 height = ((size >> 16) & 0x7ff) + 1;
	INT32 mode = (base >> 28) & 7;
	INT32 enabled = (display[DISP_CONTROL] & 1) && (display[DISP_ENABLE] & 1) && mode <= 3;
	UINT32 back = BurnHighCol((display[DISP_BACK] >> 16) & 0xff, (display[DISP_BACK] >> 8) & 0xff, display[DISP_BACK] & 0xff, 0);
	const INT32 linear = base >> 31;
	const UINT32 *lut = (mode <= 1) ? (DrawLut + mode * 0x10000) : NULL;

	// normally the render draws into another buffer and runs on
	if (enabled) RenderSyncRange(base & 0x0ffffff0, (UINT64)width * 480 * ((mode == 3) ? 4 : 2), 0);

	// the whole 16-bit picture is in RAM, in whole tiles: no checks per pixel
	const INT32 rows = (height < 480) ? height : 480;
	if (enabled && lut && (linear || !(width & 7)) && (nBurnBpp == 2 || nBurnBpp == 4)
		&& (UINT64)(base & 0x0ffffff0) + (UINT64)width * ((rows + 7) & ~7) * 2 <= RAM_SIZE) {
		if (nBurnBpp == 4) DrawFast16<UINT32>(lut, base & 0x0ffffff0, width, height, linear, back);
		else DrawFast16<UINT16>(lut, base & 0x0ffffff0, width, height, linear, back);
		return 0;
	}

	for (INT32 y = 0; y < 480; y++) {
		UINT8 *dst = pBurnDraw + y * nBurnPitch;
		const INT32 visible = enabled && y < height;
		const UINT32 row = linear ? y * width : (y / 8) * (width / 8) * 64 + (y & 7) * 8;

		for (INT32 x = 0; x < 640; x++, dst += nBurnBpp) {
			UINT32 col = back;

			if (visible && x < width) {
				const UINT32 offset = linear ? (row + x) : (row + (x / 8) * 64 + (x & 7));
				const UINT32 address = (base & 0x0ffffff0) + offset * ((mode == 3) ? 4 : 2);
				if (address < RAM_SIZE) {
					if (lut) {
						col = lut[Ram16(address)];
					} else {
						UINT32 p = Ram32(address) >> ((mode == 3) ? 0 : ((address & 2) * 8));
						INT32 r, g, b;
						if (mode == 2) {
							r = ((p >> 8) & 0xf) * 0x11; g = ((p >> 4) & 0xf) * 0x11; b = (p & 0xf) * 0x11;
						} else {
							r = (p >> 16) & 0xff; g = (p >> 8) & 0xff; b = p & 0xff;
						}
						col = BurnHighCol(r, g, b, 0);
					}
				}
			}

			PutPix(dst, col);
		}
	}

	return 0;
}

static void MakeInputs()
{
	DrvInputs[0] = 0xffff;
	DrvInputs[1] = 0xffff;
	DrvInputs[2] = 0xffff;

	for (INT32 i = 0; i < 16; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
	}

	// the FPGA holds coin pulses until the board library consumes them
	for (INT32 chute = 0; chute < 2; chute++) {
		UINT8 pressed = DrvJoy3[chute * 8] & 1;
		if (pressed && !DrvCoinPrev[chute] && !(coin_control & (4 << (chute * 4))) && coin_pending[chute] < 7) {
			coin_pending[chute]++;
		}
		DrvCoinPrev[chute] = pressed;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) DrvDoReset();

	// runahead was turned off: stop copying pages
	if (nCheckpointId && ++nCheckpointAge > 30) CheckpointDrop();

	Y2_TIME_START();

	MakeInputs();

	UINT64 nFrameStart = CpuCycles();

	for (INT32 line = 0; line < V_TOTAL; line++) {
		nCurrentLine = line;
		nLineStart = CpuCycles();

		if (line == vint_line) vint();

		if (line == V_VISIBLE) {
			display[DISP_CONTROL] = voc[0];
			display[DISP_BACK] = voc[0x48 / 4];
			display[DISP_ENABLE] = voc[0x50 / 4];
			display[DISP_BASE] = voc[0x70 / 4];
			display[DISP_SIZE] = voc[0x74 / 4];
#if defined (FBNEO_DEBUG)
			if (Y2_TRACING) {
				RenderSync();
				UINT32 base = display[DISP_BASE] & 0x0ffffff0, hash = 2166136261U;
				for (UINT32 i = 0; i < 640 * 480 * 2 && base + i < RAM_SIZE; i++) hash = (hash ^ DrvRAM[base + i]) * 16777619U;
				bprintf(PRINT_NORMAL, _T("Y2T %d line %3d: latched %08x, buffer hash %08x\n"), nTraceFrame, line, display[DISP_BASE], hash);
			}
#endif
		}

		UINT64 nLineEnd = nFrameStart + (UINT64)((line + 1) * CyclesPerLine);

		for (;;) {
			UINT64 now = CpuCycles();
			if (now >= nLineEnd) break;

			UINT64 target = nLineEnd;
			if (timer7_fire < target) target = timer7_fire;
			if (jvs_fire[0] < target) target = jvs_fire[0];
			if (jvs_fire[1] < target) target = jvs_fire[1];

			if (target > now) {
#if defined (FBNEO_DEBUG)
				// the devices the cpu drives are timed on their own, take them out
				INT64 before = Y2Now(), devices = bRenderAsync ? nTimeWait : (nTimeRender + nTimeMvc + nTimeYuv);
				Mips3Run((INT32)(target - now));
				nTimeCpu += Y2Now() - before - ((bRenderAsync ? nTimeWait : (nTimeRender + nTimeMvc + nTimeYuv)) - devices);
#else
				Mips3Run((INT32)(target - now));
#endif
			}

			now = CpuCycles();
			if (now >= timer7_fire) timer7_compare();
			if (now >= jvs_fire[0]) jvs_timeout(0);
			if (now >= jvs_fire[1]) jvs_timeout(1);
		}
	}

	// a background render may run on into the next frame: what can see its results waits for it
	// (Draw for the displayed buffer, savestates in DrvScan, the cpu and devices through the protection)

	RPP324Update(pBurnSoundOut, nBurnSoundLen);

	Y2_TIME_END(nTimeFrame);

#if defined (FBNEO_DEBUG)
	nTraceFrame++;
#endif

#if defined (FBNEO_DEBUG)
	if (++nStatFrames == 60) {
		bprintf(PRINT_NORMAL, _T("Y2: GE %d, lists %d, triangles %d, clears %d, DMA %d, MVC %d, YUV %d, sound %d, fb %08x %08x, voc %08x\n"),
			nStatGe, nStatLists, nStatTriangles, nStatClears, nStatDma, nStatMvc, nStatYuv, nStatSnd, voc[0x70 / 4], voc[0x74 / 4], voc[0]);
		if (bRenderAsync) {
			bprintf(PRINT_NORMAL, _T("Y2: ms for 60 frames: total %d = cpu %d + waiting for background work %d + other; background thread busy: render %d, MVC %d, YUV %d\n"),
				(INT32)(nTimeFrame / 1000), (INT32)(nTimeCpu / 1000), (INT32)(nTimeWait / 1000), (INT32)(nTimeRender / 1000), (INT32)(nTimeMvc / 1000), (INT32)(nTimeYuv / 1000));
		} else {
			bprintf(PRINT_NORMAL, _T("Y2: ms for 60 frames: total %d = cpu %d + render %d + MVC %d + YUV %d + other\n"),
				(INT32)(nTimeFrame / 1000), (INT32)(nTimeCpu / 1000), (INT32)(nTimeRender / 1000), (INT32)(nTimeMvc / 1000), (INT32)(nTimeYuv / 1000));
		}
		// kof2002um only: YG_Process frame status (vblank count, flip period, main loop period, last main vblank, queued flips)
		UINT32 status = RamReadSynced(0x00df816c) & 0x0fffffff;
		if (status && status < RAM_SIZE - 0x50) {
			bprintf(PRINT_NORMAL, _T("Y2: flips %d, events %d/%d, frame status: count %d, flip period %d, vcount %d, last %d, queued %d\n"),
				nStatFlips, nStatEvent[0], nStatEvent[1], RamReadSynced(status), RamReadSynced(status + 8), RamReadSynced(status + 0xc), RamReadSynced(status + 0x24), RamReadSynced(status + 0x34));
		} else {
			bprintf(PRINT_NORMAL, _T("Y2: flips %d, events %d/%d\n"), nStatFlips, nStatEvent[0], nStatEvent[1]);
		}
		nStatFlips = nStatEvent[0] = nStatEvent[1] = 0;
		nStatGe = nStatLists = nStatTriangles = nStatClears = nStatDma = nStatMvc = nStatYuv = nStatSnd = nStatFrames = 0;
		nTimeCpu = nTimeRender = nTimeMvc = nTimeYuv = nTimeFrame = nTimeWait = 0;
	}
#endif

	if (pBurnDraw) DrvDraw();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	RenderSync();

	struct BurnArea ba;

	if (pnMin) *pnMin = 0x029702;

	if (nAction & ACB_VOLATILE) {
		// main RAM: the map of the pages written since reset, then those pages (the others are zero)
		ScanMainRam(nAction);

		ba.Data	  = DrvJvsRAM;
		ba.nLen	  = 0x4000;
		ba.szName = "JVS RAM";
		BurnAcb(&ba);

		ba.Data	  = DrvGfxList;
		ba.nLen	  = 2 * GFX_LIST_WORDS * sizeof(UINT32);
		ba.szName = "GE lists";
		BurnAcb(&ba);

		Mips3Scan(nAction);
		RPP324Scan(nAction, pnMin);

		SCAN_VAR(voc);
		SCAN_VAR(rdc);
		SCAN_VAR(gec);
		SCAN_VAR(int_select);
		SCAN_VAR(irq_sources);
		SCAN_VAR(dma);
		SCAN_VAR(jvs_status);
		SCAN_VAR(jvs_config);
		SCAN_VAR(gfx_base);
		SCAN_VAR(gfx_words);
		SCAN_VAR(gfx_slot);
		SCAN_VAR(mvc);
		SCAN_VAR(mvc_sram);
		SCAN_VAR(mvc_command_ready);
		SCAN_VAR(mvc_block_ready);
		SCAN_VAR(mvc_command_pos);
		SCAN_VAR(mvc_block_pos);
		SCAN_VAR(mvc_retry);
		SCAN_VAR(yuv);
		SCAN_VAR(timer);
		SCAN_VAR(timer_epoch);
		SCAN_VAR(rtc);
		SCAN_VAR(coin_pending);
		SCAN_VAR(coin_control);
		SCAN_VAR(scc_sys_data);
		SCAN_VAR(ioc);
		SCAN_VAR(vint_line);
		SCAN_VAR(vint_event);
		SCAN_VAR(timer7_fire);
		SCAN_VAR(jvs_fire);
		SCAN_VAR(DrvCoinPrev);
		SCAN_VAR(display);
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvBackup;
		ba.nLen	  = BACKUP_SIZE;
		ba.szName = "Backup RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// System Board Y2 (BIOS)

static struct BurnRomInfo emptyRomDesc[] = { { "", 0, 0, 0 }, }; // for bios handling

static struct BurnRomInfo systemy2RomDesc[] = {
	{ "system_y2_m25p40.u801",	0x080000, 0x2b19f490, BRF_BIOS },	// Xilinx Spartan 3A FPGA bitstream (not used)
};

STD_ROM_PICK(systemy2)
STD_ROM_FN(systemy2)

static INT32 systemy2Init()
{
	return 1;
}

struct BurnDriver BurnDrvSystemy2 = {
	"systemy2", NULL, NULL, NULL, "2009",
	"System Board Y2 (BIOS)\0", "BIOS only", "SI Electronics", "System Board Y2",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_SEGA_MISC, GBF_BIOS, 0,
	NULL, systemy2RomInfo, systemy2RomName, NULL, NULL, NULL, NULL, Y2InputInfo, Y2DIPInfo,
	systemy2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	640, 480, 4, 3
};


static const UINT8 KofDataBits[16] = { 3, 12, 8, 11, 9, 2, 6, 5, 14, 0, 13, 7, 4, 15, 1, 10 };
static const UINT8 KofKeyBits[16]  = { 3, 5, 4, 10, 9, 0, 15, 6, 13, 2, 14, 1, 7, 12, 8, 11 };


// The King of Fighters - Fuchou Zhi Lu (Chinese/English)

static struct BurnRomInfo kof2002umRomDesc[] = {
	{ "s29gl01gp11fcr2.u103",	0x8000000, 0x722cbad1, BRF_SND | BRF_ESS },	//  0 Roland RPP-324 phrases and sequences
};

STDROMPICKEXT(kof2002um, kof2002um, systemy2)
STD_ROM_FN(kof2002um)

static struct BurnHDDInfo kof2002umHDDDesc[] = {
	{ "nand_u101", "86344541e71aa1e8a0423b97c7b3b826bdbdfbba" },
	{ "nand_u102", "9316ae2e729b93cb5db48b23fe812e5b3c1d6980" },
};

STD_HDD_PICK(kof2002um)
STD_HDD_FN(kof2002um)

static INT32 kof2002umInit()
{
	return DrvInit(0x41e5, KofDataBits, KofKeyBits, 1, 96);
}

struct BurnDriver BurnDrvKof2002um = {
	"kof2002um", NULL, "systemy2", NULL, "2009",
	"The King of Fighters - Fuchou Zhi Lu (Chinese/English)\0", NULL, "SNK Playmore / New Channel", "System Board Y2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_VSFIGHT, 0,
	NULL, kof2002umRomInfo, kof2002umRomName, kof2002umHDDInfo, kof2002umHDDName, NULL, NULL, Y2InputInfo, Y2DIPInfo,
	kof2002umInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	640, 480, 4, 3
};


// The King of Fighters 2002 - Unlimited Match (Japanese/English)

static struct BurnRomInfo kof2002umjRomDesc[] = {
	{ "s29gl01gp11fcr2.u103",	0x8000000, 0x916c9d68, BRF_SND | BRF_ESS },	//  0 Roland RPP-324 phrases and sequences
};

STDROMPICKEXT(kof2002umj, kof2002umj, systemy2)
STD_ROM_FN(kof2002umj)

static struct BurnHDDInfo kof2002umjHDDDesc[] = {
	{ "nand_u101", "5fd38fe816b22ec0e4459f1fc076c148a3f11652" },
	{ "nand_u102", "52b6093b658baf1dd81f883a2c52b6aac688f703" },
};

STD_HDD_PICK(kof2002umj)
STD_HDD_FN(kof2002umj)

static INT32 kof2002umjInit()
{
	return DrvInit(0xe9d8, KofDataBits, KofKeyBits, 1, 96);
}

struct BurnDriver BurnDrvKof2002umj = {
	"kof2002umj", "kof2002um", "systemy2", NULL, "2009",
	"The King of Fighters 2002 - Unlimited Match (Japanese/English)\0", NULL, "SNK Playmore", "System Board Y2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_VSFIGHT, 0,
	NULL, kof2002umjRomInfo, kof2002umjRomName, kof2002umjHDDInfo, kof2002umjHDDName, NULL, NULL, Y2InputInfo, Y2DIPInfo,
	kof2002umjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	640, 480, 4, 3
};


// Higurashi no Naku Koro ni Jong (Japan)

static struct BurnRomInfo higurashiRomDesc[] = {
	{ "s29gl01gp11tfcr2.u103",	0x8000000, 0x1a20bb8b, BRF_SND | BRF_ESS },	//  0 Roland RPP-324 phrases and sequences
	{ "s29gl01gp11tfcr2.u104",	0x8000000, 0xea7a4ba4, BRF_SND | BRF_ESS },	//  1
};

STDROMPICKEXT(higurashi, higurashi, systemy2)
STD_ROM_FN(higurashi)

static struct BurnHDDInfo higurashiHDDDesc[] = {
	{ "nand_u101", "9c50573bd6fb703cd1dd744e9fd13bc81509be01" },
	{ "nand_u102", "93c7c7ac212d217c8b1417045a9cbdb2b39174d5" },
};

STD_HDD_PICK(higurashi)
STD_HDD_FN(higurashi)

static const UINT8 HigurashiDataBits[16] = { 2, 14, 5, 9, 8, 1, 11, 6, 13, 7, 0, 10, 3, 4, 12, 15 };
static const UINT8 HigurashiKeyBits[16]  = { 9, 14, 0, 13, 15, 3, 5, 8, 6, 11, 10, 7, 1, 4, 12, 2 };

static INT32 higurashiInit()
{
	return DrvInit(0x1650, HigurashiDataBits, HigurashiKeyBits, 2, 80);
}

struct BurnDriverD BurnDrvHigurashi = {
	"higurashi", NULL, "systemy2", NULL, "2009",
	"Higurashi no Naku Koro ni Jong (Japan)\0", "Mahjong controls missing", "AQ Interactive", "System Board Y2",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MAHJONG, 0,
	NULL, higurashiRomInfo, higurashiRomName, higurashiHDDInfo, higurashiHDDName, NULL, NULL, Y2InputInfo, Y2DIPInfo,
	higurashiInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0,
	640, 480, 4, 3
};
