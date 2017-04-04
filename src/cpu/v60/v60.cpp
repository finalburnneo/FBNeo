// V60.C
// Undiscover the beast!
// Main hacking and coding by Farfetch'd
// Portability fixes by Richter Belmont

#include "burnint.h"
#include "bitswap.h" // ...xor_le
#include "driver.h"
#include "v60_intf.h"

#define offs_t			UINT32
#define INPUT_LINE_NMI		CPU_IRQLINE_NMI

#define change_pc(x)		v60.reg[32] = (x)

// convert a series of 32 bits into a float
inline float u2f(UINT32 v)
{
	union {
		float ff;
		UINT32 vv;
	} u;
	u.vv = v;
	return u.ff;
}


// convert a float into a series of 32 bits
inline UINT32 f2u(float f)
{
	union {
		float ff;
		UINT32 vv;
	} u;
	u.ff = f;
	return u.vv;
}


//#define LOG_MEM

#define address_range	0x1000000
#define address_mask	0xffffff
#define page_size	0x800
#define page_mask	0x7ff

static UINT8 *mem[3][(address_mask + 1) / page_size];

static UINT8  (*v60_read8)(UINT32) = NULL;
static UINT16 (*v60_read16)(UINT32) = NULL;
static UINT32 (*v60_read32)(UINT32) = NULL;
static void (*v60_write8)(UINT32,UINT8) = NULL;
static void (*v60_write16)(UINT32,UINT16) = NULL;
static void (*v60_write32)(UINT32,UINT32) = NULL;

static UINT8  (*v60_ioread8)(UINT32) = NULL;
static UINT16 (*v60_ioread16)(UINT32) = NULL;
static UINT32 (*v60_ioread32)(UINT32) = NULL;
static void (*v60_iowrite8)(UINT32,UINT8) = NULL;
static void (*v60_iowrite16)(UINT32,UINT16) = NULL;
static void (*v60_iowrite32)(UINT32,UINT32) = NULL;


void v60SetWriteByteHandler(void (*write)(UINT32,UINT8))
{
	v60_write8 = write;
}

void v60SetWriteWordHandler(void (*write)(UINT32,UINT16))
{
	v60_write16 = write;
}

void v60SetReadByteHandler(UINT8  (*read)(UINT32))
{
	v60_read8 = read;
}

void v60SetReadWordHandler(UINT16 (*read)(UINT32))
{
	v60_read16 = read;
}


void v60MapMemory(UINT8 *ptr, UINT32 start, UINT32 end, UINT32 flags)
{
	// error check here!

	for (UINT32 i = start; i < end; i+= page_size)
	{
		if (flags & 1) mem[0][i/page_size] = ptr + (i - start);
		if (flags & 2) mem[1][i/page_size] = ptr + (i - start);
		if (flags & 4) mem[2][i/page_size] = ptr + (i - start);
	}
}

static UINT8 cpu_readop(UINT32 a)
{
	a &= address_mask;

	if (mem[2][a / page_size]) {
#ifdef LOG_MEM
//		bprintf (0, _T("OP8: %6.6x %2.2x\n"), a, mem[2][a / page_size][a & page_mask]);
#endif
		return mem[2][a / page_size][a & page_mask];
	}

	if (v60_read8) {
#ifdef LOG_MEM
		bprintf (0, _T("OP8: %6.6x %2.2x\n"), a, v60_read8(a));
#endif
		return v60_read8(a);
	}

	return 0;
}

static UINT16 cpu_readop16(UINT32 a)
{
	a &= address_mask;

	UINT8 *p = mem[2][a / page_size];

	if (p) {
		UINT16 *z = (UINT16*)(p + (a & page_mask));
#ifdef LOG_MEM
//		bprintf (0, _T("OP16: %6.6x %4.4x\n"), a, *z);
#endif
		return *z;
	}

	if (v60_read16) {
#ifdef LOG_MEM
		bprintf (0, _T("OP16: %6.6x %4.4x\n"), a, v60_read16(a));
#endif

		return v60_read16(a);
	}

	return 0;
}

static UINT32 cpu_readop32(UINT32 a)
{
	a &= address_mask;

	UINT8 *p = mem[2][a / page_size];

	if (p) {
		UINT32 *z = (UINT32*)(p + (a & page_mask));

#ifdef LOG_MEM
//		bprintf (0, _T("OP32: %6.6x %8.8x\n"), a, *z);
#endif

		return *z;
	}

	if (v60_read32) {
#ifdef LOG_MEM
		bprintf (0, _T("OP32: %6.6x %8.8x\n"), a, v60_read32(a));
#endif

		return v60_read32(a);
	}

	return 0;
}

static UINT16 program_read_word_16le(UINT32 a)
{
	a &= address_mask;

	UINT8 *p = mem[0][a / page_size];

	if (p) {
		UINT16 *z = (UINT16*)(p + (a & page_mask));
#ifdef LOG_MEM
		bprintf (0, _T("PRW: %6.6x %4.4x\n"), a, *z);
#endif

		return *z;
	}

	if (v60_read16) {
#ifdef LOG_MEM
		bprintf (0, _T("PRW: %6.6x %4.4x\n"), a, v60_read16(a));
#endif
		return v60_read16(a);
	}

	return 0;
}

static UINT8 program_read_byte_16le(UINT32 a)
{
	a &= address_mask;

#ifdef LOG_MEM
	bprintf (0, _T("PRB: %6.6x\n"), a);
#endif

	if (mem[0][a / page_size]) {
		return mem[0][a / page_size][a & page_mask];
	}

	if (v60_read8) {
		return v60_read8(a);
	}

	return 0;
}

static void program_write_word_16le(UINT32 a, UINT16 d)
{
	a &= address_mask;
#ifdef LOG_MEM
	bprintf (0, _T("PWW: %6.6x %4.4x\n"), a,d);
#endif

	UINT8 *p = mem[1][a / page_size];

	if (p) {
		UINT16 *z = (UINT16*)(p + (a & page_mask));
		*z = d;
		return;
	}

	if (v60_write16) {
		v60_write16(a,d);
		return;
	}
}

static void program_write_byte_16le(UINT32 a, UINT8 d)
{
	a &= address_mask;
#ifdef LOG_MEM
	bprintf (0, _T("PWB: %6.6x %2.2x\n"), a,d);
#endif

	if (mem[1][a / page_size]) {
		mem[1][a / page_size][a & page_mask] = d;
		return;
	}

	if (v60_write8) {
		return v60_write8(a,d);
	}
}

static UINT16 io_read_word_16le(UINT32 a)
{
	if (v60_ioread16) {
		return v60_ioread16(a);
	}

	return 0;
}

static UINT8 io_read_byte_16le(UINT32 a)
{
	if (v60_ioread8) {
		return v60_ioread8(a);
	}

	return 0;
}

static void io_write_word_16le(UINT32 a, UINT16 d)
{
	if (v60_iowrite16) {
		return v60_iowrite16(a,d);
	}
}

static void io_write_byte_16le(UINT32 a, UINT8 d)
{
	if (v60_iowrite8) {
		return v60_iowrite8(a,d);
	}
}

static UINT32 io_read_dword_32le(UINT32 a)
{
	if (v60_ioread32) {
		return v60_ioread32(a);
	}

	return 0;
}

static UINT16 io_read_word_32le(UINT32 a)
{
	if (v60_ioread16) {
		return v60_ioread16(a);
	}

	return 0;
}

static UINT8 io_read_byte_32le(UINT32 a)
{
	if (v60_ioread8) {
		return v60_ioread8(a);
	}

	return 0;
}

static void io_write_dword_32le(UINT32 a, UINT32 d)
{
	if (v60_iowrite32) {
		return v60_iowrite32(a,d);
	}
}

static void io_write_word_32le(UINT32 a, UINT16 d)
{
	if (v60_iowrite16) {
		return v60_iowrite16(a,d);
	}
}

static void io_write_byte_32le(UINT32 a, UINT8 d)
{
	if (v60_iowrite8) {
		return v60_iowrite8(a,d);
	}
}

static UINT32 program_read_dword_32le(UINT32 a)
{
	UINT32 *p = (UINT32*)mem[0][a / page_size];

	if (p) {
		return p[(a & page_mask)/4];
	}

	if (v60_read32) {
		return v60_read32(a);
	}

	return 0;
}

static UINT16 program_read_word_32le(UINT32 a)
{
	UINT16 *p = (UINT16*)mem[0][a / page_size];

	if (p) {
		return p[(a & page_mask)/2];
	}

	if (v60_read16) {
		return v60_read16(a);
	}

	return 0;
}

static UINT8 program_read_byte_32le(UINT32 a)
{
	if (mem[0][a / page_size]) {
		return mem[0][a / page_size][a & page_mask];
	}

	if (v60_read8) {
		return v60_read8(a);
	}

	return 0;
}

static void program_write_dword_32le(UINT32 a, UINT32 d)
{
	UINT32 *p = (UINT32*)mem[1][a / page_size];

	if (p) {
		p[(a & page_mask)/4] = d;
		return;
	}

	if (v60_write32) {
		v60_write32(a,d);
		return;
	}
}

static void program_write_word_32le(UINT32 a, UINT16 d)
{
	UINT16 *p = (UINT16*)mem[1][a / page_size];

	if (p) {
		p[(a & page_mask)/2] = d;
		return;
	}

	if (v60_write16) {
		v60_write16(a,d);
		return;
	}
}

static void program_write_byte_32le(UINT32 a, UINT8 d)
{
	if (mem[1][a / page_size]) {
		mem[1][a / page_size][a & page_mask] = d;
		return;
	}

	if (v60_write8) {
		return v60_write8(a,d);
	}
}


struct cpu_info {
	UINT8  (*mr8) (offs_t address);
	void   (*mw8) (offs_t address, UINT8  data);
	UINT16 (*mr16)(offs_t address);
	void   (*mw16)(offs_t address, UINT16 data);
	UINT32 (*mr32)(offs_t address);
	void   (*mw32)(offs_t address, UINT32 data);
	UINT8  (*pr8) (offs_t address);
	void   (*pw8) (offs_t address, UINT8  data);
	UINT16 (*pr16)(offs_t address);
	void   (*pw16)(offs_t address, UINT16 data);
	UINT32 (*pr32)(offs_t address);
	void   (*pw32)(offs_t address, UINT32 data);
	UINT8  (*or8) (offs_t address);
	UINT16 (*or16)(offs_t address);
	UINT32 (*or32)(offs_t address);
	void   (*chpc)(offs_t newpc);
	UINT32 start_pc;
};

typedef struct
{
	UINT8 CY;
	UINT8 OV;
	UINT8 S;
	UINT8 Z;
} Flags;

// v60 Register Inside (Hm... It's not a pentium inside :-))) )
static struct v60info {
	struct cpu_info info;
	UINT32 reg[68];
	Flags flags;
	UINT8 irq_line;
	UINT8 nmi_line;
	ALIGN_VAR(8) int (*irq_cb)(int irqline);
	UINT32 PPC;
	UINT32 current_cycles;
	UINT32 cycles;
} v60;


static int v60_ICount;


// memory accessors
#include "v60mem.c"


// macros stolen from MAME for flags calc
// note that these types are in x86 naming:
// byte = 8 bit, word = 16 bit, long = 32 bit

// parameter x = result, y = source 1, z = source 2

#define SetOFL_Add(x,y,z)	(_OV = (((x) ^ (y)) & ((x) ^ (z)) & 0x80000000) ? 1: 0)
#define SetOFW_Add(x,y,z)	(_OV = (((x) ^ (y)) & ((x) ^ (z)) & 0x8000) ? 1 : 0)
#define SetOFB_Add(x,y,z)	(_OV = (((x) ^ (y)) & ((x) ^ (z)) & 0x80) ? 1 : 0)

#define SetOFL_Sub(x,y,z)	(_OV = (((z) ^ (y)) & ((z) ^ (x)) & 0x80000000) ? 1 : 0)
#define SetOFW_Sub(x,y,z)	(_OV = (((z) ^ (y)) & ((z) ^ (x)) & 0x8000) ? 1 : 0)
#define SetOFB_Sub(x,y,z)	(_OV = (((z) ^ (y)) & ((z) ^ (x)) & 0x80) ? 1 : 0)

#define SetCFB(x)		{_CY = ((x) & 0x100) ? 1 : 0; }
#define SetCFW(x)		{_CY = ((x) & 0x10000) ? 1 : 0; }
#define SetCFL(x)		{_CY = ((x) & (((UINT64)1) << 32)) ? 1 : 0; }

#define SetSF(x)		(_S = (x))
#define SetZF(x)		(_Z = (x))

#define SetSZPF_Byte(x) 	{_Z = ((UINT8)(x)==0);  _S = ((x)&0x80) ? 1 : 0; }
#define SetSZPF_Word(x) 	{_Z = ((UINT16)(x)==0);  _S = ((x)&0x8000) ? 1 : 0; }
#define SetSZPF_Long(x) 	{_Z = ((UINT32)(x)==0);  _S = ((x)&0x80000000) ? 1 : 0; }

#define ORB(dst,src)		{ (dst) |= (src); _CY = _OV = 0; SetSZPF_Byte(dst); }
#define ORW(dst,src)		{ (dst) |= (src); _CY = _OV = 0; SetSZPF_Word(dst); }
#define ORL(dst,src)		{ (dst) |= (src); _CY = _OV = 0; SetSZPF_Long(dst); }

#define ANDB(dst,src)		{ (dst) &= (src); _CY = _OV = 0; SetSZPF_Byte(dst); }
#define ANDW(dst,src)		{ (dst) &= (src); _CY = _OV = 0; SetSZPF_Word(dst); }
#define ANDL(dst,src)		{ (dst) &= (src); _CY = _OV = 0; SetSZPF_Long(dst); }

#define XORB(dst,src)		{ (dst) ^= (src); _CY = _OV = 0; SetSZPF_Byte(dst); }
#define XORW(dst,src)		{ (dst) ^= (src); _CY = _OV = 0; SetSZPF_Word(dst); }
#define XORL(dst,src)		{ (dst) ^= (src); _CY = _OV = 0; SetSZPF_Long(dst); }

#define SUBB(dst, src)		{ unsigned res=(dst)-(src); SetCFB(res); SetOFB_Sub(res,src,dst); SetSZPF_Byte(res); dst=(UINT8)res; }
#define SUBW(dst, src)		{ unsigned res=(dst)-(src); SetCFW(res); SetOFW_Sub(res,src,dst); SetSZPF_Word(res); dst=(UINT16)res; }
#define SUBL(dst, src)		{ UINT64 res=(UINT64)(dst)-(INT64)(src); SetCFL(res); SetOFL_Sub(res,src,dst); SetSZPF_Long(res); dst=(UINT32)res; }

#define ADDB(dst, src)		{ unsigned res=(dst)+(src); SetCFB(res); SetOFB_Add(res,src,dst); SetSZPF_Byte(res); dst=(UINT8)res; }
#define ADDW(dst, src)		{ unsigned res=(dst)+(src); SetCFW(res); SetOFW_Add(res,src,dst); SetSZPF_Word(res); dst=(UINT16)res; }
#define ADDL(dst, src)		{ UINT64 res=(UINT64)(dst)+(UINT64)(src); SetCFL(res); SetOFL_Add(res,src,dst); SetSZPF_Long(res); dst=(UINT32)res; }

#define SETREG8(a, b)  (a) = ((a) & ~0xff) | ((b) & 0xff)
#define SETREG16(a, b) (a) = ((a) & ~0xffff) | ((b) & 0xffff)


/*
 * Prevent warnings on NetBSD.  All identifiers beginning with an underscore
 * followed by an uppercase letter are reserved by the C standard (ISO/IEC
 * 9899:1999, 7.1.3) to be used by the implementation.  It'd be best to rename
 * all such instances, but this is less intrusive and error-prone.
 */
#undef _S

#define _CY v60.flags.CY
#define _OV v60.flags.OV
#define _S v60.flags.S
#define _Z v60.flags.Z


// Defines of all v60 register...
#define R0 v60.reg[0]
#define R1 v60.reg[1]
#define R2 v60.reg[2]
#define R3 v60.reg[3]
#define R4 v60.reg[4]
#define R5 v60.reg[5]
#define R6 v60.reg[6]
#define R7 v60.reg[7]
#define R8 v60.reg[8]
#define R9 v60.reg[9]
#define R10 v60.reg[10]
#define R11 v60.reg[11]
#define R12 v60.reg[12]
#define R13 v60.reg[13]
#define R14 v60.reg[14]
#define R15 v60.reg[15]
#define R16 v60.reg[16]
#define R17 v60.reg[17]
#define R18 v60.reg[18]
#define R19 v60.reg[19]
#define R20 v60.reg[20]
#define R21 v60.reg[21]
#define R22 v60.reg[22]
#define R23 v60.reg[23]
#define R24 v60.reg[24]
#define R25 v60.reg[25]
#define R26 v60.reg[26]
#define R27 v60.reg[27]
#define R28 v60.reg[28]
#define AP v60.reg[29]
#define FP v60.reg[30]
#define SP v60.reg[31]

#define PC		v60.reg[32]
#define PSW		v60.reg[33]

// Privileged registers
#define ISP		v60.reg[36]
#define L0SP	v60.reg[37]
#define L1SP	v60.reg[38]
#define L2SP	v60.reg[39]
#define L3SP	v60.reg[40]
#define SBR		v60.reg[41]
#define TR		v60.reg[42]
#define SYCW	v60.reg[43]
#define TKCW	v60.reg[44]
#define PIR		v60.reg[45]
//10-14 reserved
#define PSW2	v60.reg[51]
#define ATBR0	v60.reg[52]
#define ATLR0	v60.reg[53]
#define ATBR1	v60.reg[54]
#define ATLR1	v60.reg[55]
#define ATBR2	v60.reg[56]
#define ATLR2	v60.reg[57]
#define ATBR3	v60.reg[58]
#define ATLR3	v60.reg[59]
#define TRMODE v60.reg[60]
#define ADTR0	v60.reg[61]
#define ADTR1	v60.reg[62]
#define ADTMR0	v60.reg[63]
#define ADTMR1	v60.reg[64]
//29-31 reserved

// Register names
const char *v60_reg_names[69] = {
	"R0", "R1", "R2", "R3",
	"R4", "R5", "R6", "R7",
	"R8", "R9", "R10", "R11",
	"R12", "R13", "R14", "R15",
	"R16", "R17", "R18", "R19",
	"R20", "R21", "R22", "R23",
	"R24", "R25", "R26", "R27",
	"R28", "AP", "FP", "SP",
	"PC", "PSW","Unk","Unk",
	"ISP", "L0SP", "L1SP", "L2SP",
	"L3SP", "SBR","TR","SYCW",
	"TKCW", "PIR", "Reserved","Reserved",
	"Reserved","Reserved","Reserved","PSW2",
	"ATBR0", "ATLR0", "ATBR1", "ATLR1",
	"ATBR2", "ATLR2", "ATBR3", "ATLR3",
	"TRMODE", "ADTR0", "ADTR1","ADTMR0",
	"ADTMR1","Reserved","Reserved","Reserved"
};

// Defines...
#define NORMALIZEFLAGS() \
{ \
	_S	= _S  ? 1 : 0; \
	_OV	= _OV ? 1 : 0; \
	_Z	= _Z  ? 1 : 0; \
	_CY	= _CY ? 1 : 0; \
}

static void v60_try_irq(void);


INLINE void v60SaveStack(void)
{
	if (PSW & 0x10000000)
		ISP = SP;
	else
		v60.reg[37 + ((PSW >> 24) & 3)] = SP;
}

INLINE void v60ReloadStack(void)
{
	if (PSW & 0x10000000)
		SP = ISP;
	else
		SP = v60.reg[37 + ((PSW >> 24) & 3)];
}

INLINE UINT32 v60ReadPSW(void)
{
	PSW &= 0xfffffff0;
	PSW |= (_Z?1:0) | (_S?2:0) | (_OV?4:0) | (_CY?8:0);
	return PSW;
}

INLINE void v60WritePSW(UINT32 newval)
{
	/* determine if we need to save/restore the stacks */
	int updateStack = 0;

	/* if the interrupt state is changing, we definitely need to update */
	if ((newval ^ PSW) & 0x10000000)
		updateStack = 1;

	/* if we are not in interrupt mode and the level is changing, we also must update */
	else if (!(PSW & 0x10000000) && ((newval ^ PSW) & 0x03000000))
		updateStack = 1;

	/* save the previous stack value */
	if (updateStack)
		v60SaveStack();

	/* set the new value and update the flags */
	PSW = newval;
	_Z =  (UINT8)(PSW & 1);
	_S =  (UINT8)(PSW & 2);
	_OV = (UINT8)(PSW & 4);
	_CY = (UINT8)(PSW & 8);

	/* fetch the new stack value */
	if (updateStack)
		v60ReloadStack();
}


INLINE UINT32 v60_update_psw_for_exception(int is_interrupt, int target_level)
{
	UINT32 oldPSW = v60ReadPSW();
	UINT32 newPSW = oldPSW;

	// Change to interrupt context
	newPSW &= ~(3 << 24);  // PSW.EL = 0
	newPSW |= target_level << 24; // set target level
	newPSW &= ~(1 << 18);  // PSW.IE = 0
	newPSW &= ~(1 << 16);  // PSW.TE = 0
	newPSW &= ~(1 << 27);  // PSW.TP = 0
	newPSW &= ~(1 << 17);  // PSW.AE = 0
	newPSW &= ~(1 << 29);  // PSW.EM = 0
	if (is_interrupt)
		newPSW |=  (1 << 28);// PSW.IS = 1
	newPSW |=  (1 << 31);  // PSW.ASA = 1
	v60WritePSW(newPSW);

	return oldPSW;
}


#define GETINTVECT(nint)	MemRead32((SBR & ~0xfff) + (nint)*4)
#define EXCEPTION_CODE_AND_SIZE(code, size)	(((code) << 16) | (size))


// Addressing mode decoding functions
#include "am.c"

// Opcode functions
#include "op12.c"
#include "op2.c"
#include "op3.c"
#include "op4.c"
#include "op5.c"
#include "op6.c"
#include "op7a.c"

static UINT32 opUNHANDLED(void)
{
	//fatalerror("Unhandled OpCode found : %02x at %08x", OpRead16(PC), PC);
	return 0; /* never reached, fatalerror won't return */
}

// Opcode jump table
#include "optable.c"

static int v60_default_irq_cb(int )
{
	return 0;
}

INT32 v60GetActive()
{
	return 0;
}

static void cheat_write_byte(UINT32 a, UINT8 d)
{
	if (mem[0][a / page_size]) {
		mem[0][a / page_size][a & page_mask] = d;
		return;
	}

	if (mem[1][a / page_size]) {
		mem[1][a / page_size][a & page_mask] = d;
		return;
	}

	if (mem[2][a / page_size]) {
		mem[2][a / page_size][a & page_mask] = d;
		return;
	}

	if (v60_write8) {
		return v60_write8(a,d);
	}
}

static cpu_core_config v60CheatCpuConfig =
{
	v60Open,
	v60Close,
	program_read_byte_16le,
	cheat_write_byte,
	v60GetActive,
	v60TotalCycles,
	v60NewFrame,
	v60Run,
	v60RunEnd,
	v60Reset,
	1<<24,
	0
};

static void base_init()
{
	v60.irq_cb = v60_default_irq_cb;
	v60.irq_line = CLEAR_LINE;
	v60.nmi_line = CLEAR_LINE;

#if 0
	state_save_register_item_array(type, index, v60.reg);
	state_save_register_item(type, index, v60.irq_line);
	state_save_register_item(type, index, v60.nmi_line);
	state_save_register_item(type, index, v60.PPC);
	state_save_register_item(type, index, _CY);
	state_save_register_item(type, index, _OV);
	state_save_register_item(type, index, _S);
	state_save_register_item(type, index, _Z);
#endif
}

void v60Init()
{
	memset (mem, 0, 3 * ((address_mask + 1) / page_size) * sizeof(UINT8*));

	base_init();
	// Set PIR (Processor ID) for NEC v60. LSB is reserved to NEC,
	// so I don't know what it contains.
	PIR = 0x00006000;
	v60.info = v60_i;

	CpuCheatRegister(0, &v60CheatCpuConfig);
}

void v70Init()
{
	base_init();
	// Set PIR (Processor ID) for NEC v70. LSB is reserved to NEC,
	// so I don't know what it contains.
	PIR = 0x00007000;
	v60.info = v70_i;
}

INT32 v60Scan(INT32 nAction)
{
	struct BurnArea ba;
	
	if ((nAction & ACB_DRIVER_DATA) == 0) {
		return 1;
	}

	ba.Data = &v60.reg;
	ba.nLen = sizeof(v60.reg);
	ba.szName = "V60 Regs";
	BurnAcb(&ba);

	SCAN_VAR(v60.flags.CY);
	SCAN_VAR(v60.flags.OV);
	SCAN_VAR(v60.flags.S);
	SCAN_VAR(v60.flags.Z);
	SCAN_VAR(v60.irq_line);
	SCAN_VAR(v60.nmi_line);
	SCAN_VAR(v60.PPC);
	SCAN_VAR(v60.current_cycles);
	SCAN_VAR(v60.cycles);
	
	return 0;
}

void v60SetIRQCallback(int (*callback)(int irqline))
{
	v60.irq_cb = callback;
}

void v60Reset()
{
	v60.current_cycles = 0;
	PSW		= 0x10000000;
	PC		= v60.info.start_pc;
	SBR		= 0x00000000;
	SYCW	= 0x00000070;
	TKCW	= 0x0000e000;
	PSW2	= 0x0000f002;
	ChangePC(PC);

	_CY	= 0;
	_OV	= 0;
	_S	= 0;
	_Z	= 0;
}

void v60Open(int)
{

}

void v60Close()
{

}

void v60Exit()
{

}


static void v60_do_irq(int vector)
{
	UINT32 oldPSW = v60_update_psw_for_exception(1, 0);

	// Push PC and PSW onto the stack
	SP-=4;
	MemWrite32(SP, oldPSW);
	SP-=4;
	MemWrite32(SP, PC);

	// Jump to vector for user interrupt
	PC = GETINTVECT(vector);
}

static void v60_try_irq(void)
{
	if(v60.irq_line == CLEAR_LINE)
		return;
	if((PSW & (1<<18)) != 0) {
		int vector;
		if(v60.irq_line != ASSERT_LINE)
			v60.irq_line = CLEAR_LINE;

		vector = v60.irq_cb(0);

		v60_do_irq(vector + 0x40);
	} else if(v60.irq_line == PULSE_LINE)
		v60.irq_line = CLEAR_LINE;
}

static void set_irq_line(int irqline, int state)
{
	if(irqline == INPUT_LINE_NMI) {
		switch(state) {
		case ASSERT_LINE:
			if(v60.nmi_line == CLEAR_LINE) {
				v60.nmi_line = ASSERT_LINE;
				v60_do_irq(2);
			}
			break;
		case CLEAR_LINE:
			v60.nmi_line = CLEAR_LINE;
			break;
		case HOLD_LINE:
		case PULSE_LINE:
			v60.nmi_line = CLEAR_LINE;
			v60_do_irq(2);
			break;
		}
	} else {
		v60.irq_line = state;
		v60_try_irq();
	}
}

// Actual cycles/instruction is unknown

INT32 v60Run(int cycles)
{
	UINT32 inc;

	v60.cycles = cycles;

	v60_ICount = cycles;
	if(v60.irq_line != CLEAR_LINE)
		v60_try_irq();
	while(v60_ICount >= 0) {
		v60.PPC = PC;
	//	CALL_MAME_DEBUG;
		v60_ICount -= 8;	/* fix me -- this is just an average */
		inc = OpCodeTable[OpRead8(PC)]();
		PC += inc;
		if(v60.irq_line != CLEAR_LINE)
			v60_try_irq();
	}

	v60.current_cycles += cycles - v60_ICount;

	return cycles - v60_ICount;
}

void v60SetIRQLine(INT32 irqline, INT32 state)
{
	if (state == CPU_IRQSTATUS_AUTO) {
	//	INT32 tmp0 = v60.current_cycles;
	//	INT32 tmp1 = v60.cycles;
		set_irq_line(irqline,1);
		v60Run(100);
	//	if (tmp1) tmp1 -= 100;
		set_irq_line(irqline,0);
		v60Run(100);
	}
	else
	{
		set_irq_line(irqline,state);
	}
}

INT32 v60TotalCycles()
{
	return v60.current_cycles + (v60.cycles - v60_ICount);
}

void v60RunEnd()
{
	v60_ICount = 0;
}

void v60NewFrame()
{
	v60.current_cycles = 0;
}

#if 0
static void v60_get_context(void *dst)
{
	if(dst)
		*(struct v60info *)dst = v60;
}

static void v60_set_context(void *src)
{
	if(src)
	{
		v60 = *(struct v60info *)src;
		ChangePC(PC);
	}
}
#endif

#ifdef MAME_DEBUG
offs_t v60_dasm(char *buffer, offs_t pc, const UINT8 *oprom, const UINT8 *opram);
offs_t v70_dasm(char *buffer, offs_t pc, const UINT8 *oprom, const UINT8 *opram);
#endif /* MAME_DEBUG */


/**************************************************************************
 * Generic set_info
 **************************************************************************/

#if 0
static void v60_set_info(UINT32 state, union cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are set as 64-bit signed integers --- */
		case CPUINFO_INT_INPUT_STATE + 0:				set_irq_line(0, info->i);				break;
		case CPUINFO_INT_INPUT_STATE + INPUT_LINE_NMI:	set_irq_line(INPUT_LINE_NMI, info->i);	break;

		case CPUINFO_INT_PC:							PC = info->i; ChangePC(PC);				break;
		case CPUINFO_INT_SP:							SP = info->i;							break;

		case CPUINFO_INT_REGISTER + V60_R0:				R0 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R1:				R1 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R2:				R2 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R3:				R3 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R4:				R4 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R5:				R5 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R6:				R6 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R7:				R7 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R8:				R8 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R9:				R9 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R10:			R10 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R11:			R11 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R12:			R12 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R13:			R13 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R14:			R14 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R15:			R15 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R16:			R16 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R17:			R17 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R18:			R18 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R19:			R19 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R20:			R20 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R21:			R21 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R22:			R22 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R23:			R23 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R24:			R24 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R25:			R25 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R26:			R26 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R27:			R27 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_R28:			R28 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_AP:				AP = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_FP:				FP = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_SP:				SP = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_PC:				PC = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_PSW:			v60WritePSW(info->i);					break;
		case CPUINFO_INT_REGISTER + V60_ISP:			ISP = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_L0SP:			L0SP = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_L1SP:			L1SP = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_L2SP:			L2SP = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_L3SP:			L3SP = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_SBR:			SBR = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_TR:				TR = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_SYCW:			SYCW = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_TKCW:			TKCW = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_PIR:			PIR = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_PSW2:			PSW2 = info->i;							break;
		case CPUINFO_INT_REGISTER + V60_ATBR0:			ATBR0 = info->i;						break;
		case CPUINFO_INT_REGISTER + V60_ATLR0:			ATLR0 = info->i;						break;
		case CPUINFO_INT_REGISTER + V60_ATBR1:			ATBR1 = info->i;						break;
		case CPUINFO_INT_REGISTER + V60_ATLR1:			ATLR1 = info->i;						break;
		case CPUINFO_INT_REGISTER + V60_ATBR2:			ATBR2 = info->i;						break;
		case CPUINFO_INT_REGISTER + V60_ATLR2:			ATLR2 = info->i;						break;
		case CPUINFO_INT_REGISTER + V60_ATBR3:			ATBR3 = info->i;						break;
		case CPUINFO_INT_REGISTER + V60_ATLR3:			ATLR3 = info->i;						break;
		case CPUINFO_INT_REGISTER + V60_TRMODE:			TRMODE = info->i;						break;
		case CPUINFO_INT_REGISTER + V60_ADTR0:			ADTR0 = info->i;						break;
		case CPUINFO_INT_REGISTER + V60_ADTR1:			ADTR1 = info->i;						break;
		case CPUINFO_INT_REGISTER + V60_ADTMR0:			ADTMR0 = info->i;						break;
		case CPUINFO_INT_REGISTER + V60_ADTMR1:			ADTMR1 = info->i;						break;
	}
}



/**************************************************************************
 * Generic get_info
 **************************************************************************/

void v60_get_info(UINT32 state, union cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_CONTEXT_SIZE:					info->i = sizeof(v60);					break;
		case CPUINFO_INT_INPUT_LINES:					info->i = 1;							break;
		case CPUINFO_INT_DEFAULT_IRQ_VECTOR:			info->i = 0;							break;
		case CPUINFO_INT_ENDIANNESS:					info->i = CPU_IS_LE;					break;
		case CPUINFO_INT_CLOCK_DIVIDER:					info->i = 1;							break;
		case CPUINFO_INT_MIN_INSTRUCTION_BYTES:			info->i = 1;							break;
		case CPUINFO_INT_MAX_INSTRUCTION_BYTES:			info->i = 22;							break;
		case CPUINFO_INT_MIN_CYCLES:					info->i = 1;							break;
		case CPUINFO_INT_MAX_CYCLES:					info->i = 1;							break;

		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_PROGRAM:	info->i = 16;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_PROGRAM: info->i = 24;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_PROGRAM: info->i = 0;					break;
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_DATA:	info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_DATA: 	info->i = 0;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_DATA: 	info->i = 0;					break;
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_IO:		info->i = 16;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_IO: 		info->i = 24;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_IO: 		info->i = 0;					break;

		case CPUINFO_INT_INPUT_STATE + 0:				info->i = v60.irq_line;					break;
		case CPUINFO_INT_INPUT_STATE + INPUT_LINE_NMI:	info->i = v60.nmi_line;					break;

		case CPUINFO_INT_PREVIOUSPC:					info->i = v60.PPC;							break;

		case CPUINFO_INT_REGISTER + V60_R0:				info->i = R0;							break;
		case CPUINFO_INT_REGISTER + V60_R1:				info->i = R1;							break;
		case CPUINFO_INT_REGISTER + V60_R2:				info->i = R2;							break;
		case CPUINFO_INT_REGISTER + V60_R3:				info->i = R3;							break;
		case CPUINFO_INT_REGISTER + V60_R4:				info->i = R4;							break;
		case CPUINFO_INT_REGISTER + V60_R5:				info->i = R5;							break;
		case CPUINFO_INT_REGISTER + V60_R6:				info->i = R6;							break;
		case CPUINFO_INT_REGISTER + V60_R7:				info->i = R7;							break;
		case CPUINFO_INT_REGISTER + V60_R8:				info->i = R8;							break;
		case CPUINFO_INT_REGISTER + V60_R9:				info->i = R9;							break;
		case CPUINFO_INT_REGISTER + V60_R10:			info->i = R10;							break;
		case CPUINFO_INT_REGISTER + V60_R11:			info->i = R11;							break;
		case CPUINFO_INT_REGISTER + V60_R12:			info->i = R12;							break;
		case CPUINFO_INT_REGISTER + V60_R13:			info->i = R13;							break;
		case CPUINFO_INT_REGISTER + V60_R14:			info->i = R14;							break;
		case CPUINFO_INT_REGISTER + V60_R15:			info->i = R15;							break;
		case CPUINFO_INT_REGISTER + V60_R16:			info->i = R16;							break;
		case CPUINFO_INT_REGISTER + V60_R17:			info->i = R17;							break;
		case CPUINFO_INT_REGISTER + V60_R18:			info->i = R18;							break;
		case CPUINFO_INT_REGISTER + V60_R19:			info->i = R19;							break;
		case CPUINFO_INT_REGISTER + V60_R20:			info->i = R20;							break;
		case CPUINFO_INT_REGISTER + V60_R21:			info->i = R21;							break;
		case CPUINFO_INT_REGISTER + V60_R22:			info->i = R22;							break;
		case CPUINFO_INT_REGISTER + V60_R23:			info->i = R23;							break;
		case CPUINFO_INT_REGISTER + V60_R24:			info->i = R24;							break;
		case CPUINFO_INT_REGISTER + V60_R25:			info->i = R25;							break;
		case CPUINFO_INT_REGISTER + V60_R26:			info->i = R26;							break;
		case CPUINFO_INT_REGISTER + V60_R27:			info->i = R27;							break;
		case CPUINFO_INT_REGISTER + V60_R28:			info->i = R28;							break;
		case CPUINFO_INT_REGISTER + V60_AP:				info->i = AP;							break;
		case CPUINFO_INT_REGISTER + V60_FP:				info->i = FP;							break;
		case CPUINFO_INT_SP:
		case CPUINFO_INT_REGISTER + V60_SP:				info->i = SP;							break;
		case CPUINFO_INT_PC:
		case CPUINFO_INT_REGISTER + V60_PC:				info->i = PC;							break;
		case CPUINFO_INT_REGISTER + V60_PSW:			info->i = v60ReadPSW();					break;
		case CPUINFO_INT_REGISTER + V60_ISP:			info->i = ISP;							break;
		case CPUINFO_INT_REGISTER + V60_L0SP:			info->i = L0SP;							break;
		case CPUINFO_INT_REGISTER + V60_L1SP:			info->i = L1SP;							break;
		case CPUINFO_INT_REGISTER + V60_L2SP:			info->i = L2SP;							break;
		case CPUINFO_INT_REGISTER + V60_L3SP:			info->i = L3SP;							break;
		case CPUINFO_INT_REGISTER + V60_SBR:			info->i = SBR;							break;
		case CPUINFO_INT_REGISTER + V60_TR:				info->i = TR;							break;
		case CPUINFO_INT_REGISTER + V60_SYCW:			info->i = SYCW;							break;
		case CPUINFO_INT_REGISTER + V60_TKCW:			info->i = TKCW;							break;
		case CPUINFO_INT_REGISTER + V60_PIR:			info->i = PIR;							break;
		case CPUINFO_INT_REGISTER + V60_PSW2:			info->i = PSW2;							break;
		case CPUINFO_INT_REGISTER + V60_ATBR0:			info->i = ATBR0;						break;
		case CPUINFO_INT_REGISTER + V60_ATLR0:			info->i = ATLR0;						break;
		case CPUINFO_INT_REGISTER + V60_ATBR1:			info->i = ATBR1;						break;
		case CPUINFO_INT_REGISTER + V60_ATLR1:			info->i = ATLR1;						break;
		case CPUINFO_INT_REGISTER + V60_ATBR2:			info->i = ATBR2;						break;
		case CPUINFO_INT_REGISTER + V60_ATLR2:			info->i = ATLR2;						break;
		case CPUINFO_INT_REGISTER + V60_ATBR3:			info->i = ATBR3;						break;
		case CPUINFO_INT_REGISTER + V60_ATLR3:			info->i = ATLR3;						break;
		case CPUINFO_INT_REGISTER + V60_TRMODE:			info->i = TRMODE;						break;
		case CPUINFO_INT_REGISTER + V60_ADTR0:			info->i = ADTR0;						break;
		case CPUINFO_INT_REGISTER + V60_ADTR1:			info->i = ADTR1;						break;
		case CPUINFO_INT_REGISTER + V60_ADTMR0:			info->i = ADTMR0;						break;
		case CPUINFO_INT_REGISTER + V60_ADTMR1:			info->i = ADTMR1;						break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_SET_INFO:						info->setinfo = v60_set_info;			break;
		case CPUINFO_PTR_GET_CONTEXT:					info->getcontext = v60_get_context;		break;
		case CPUINFO_PTR_SET_CONTEXT:					info->setcontext = v60_set_context;		break;
		case CPUINFO_PTR_INIT:							info->init = v60_init;					break;
		case CPUINFO_PTR_RESET:							info->reset = v60_reset;				break;
		case CPUINFO_PTR_EXIT:							info->exit = v60_exit;					break;
		case CPUINFO_PTR_EXECUTE:						info->execute = v60_execute;			break;
		case CPUINFO_PTR_BURN:							info->burn = NULL;						break;
#ifdef MAME_DEBUG
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = v60_dasm;			break;
#endif /* MAME_DEBUG */
		case CPUINFO_PTR_INSTRUCTION_COUNTER:			info->icount = &v60_ICount;				break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s = cpuintrf_temp_str(), "V60"); break;
		case CPUINFO_STR_CORE_FAMILY:					strcpy(info->s = cpuintrf_temp_str(), "NEC V60"); break;
		case CPUINFO_STR_CORE_VERSION:					strcpy(info->s = cpuintrf_temp_str(), "1.0"); break;
		case CPUINFO_STR_CORE_FILE:						strcpy(info->s = cpuintrf_temp_str(), __FILE__); break;
		case CPUINFO_STR_CORE_CREDITS:					strcpy(info->s = cpuintrf_temp_str(), "Farfetch'd and R.Belmont"); break;

		case CPUINFO_STR_FLAGS:							strcpy(info->s = cpuintrf_temp_str(), " "); break;

		case CPUINFO_STR_REGISTER + V60_R0:				sprintf(info->s = cpuintrf_temp_str(), "R0:%08X", R0);	break;
		case CPUINFO_STR_REGISTER + V60_R1:				sprintf(info->s = cpuintrf_temp_str(), "R1:%08X", R1);	break;
		case CPUINFO_STR_REGISTER + V60_R2:				sprintf(info->s = cpuintrf_temp_str(), "R2:%08X", R2);	break;
		case CPUINFO_STR_REGISTER + V60_R3:				sprintf(info->s = cpuintrf_temp_str(), "R3:%08X", R3);	break;
		case CPUINFO_STR_REGISTER + V60_R4:				sprintf(info->s = cpuintrf_temp_str(), "R4:%08X", R4);	break;
		case CPUINFO_STR_REGISTER + V60_R5:				sprintf(info->s = cpuintrf_temp_str(), "R5:%08X", R5);	break;
		case CPUINFO_STR_REGISTER + V60_R6:				sprintf(info->s = cpuintrf_temp_str(), "R6:%08X", R6);	break;
		case CPUINFO_STR_REGISTER + V60_R7:				sprintf(info->s = cpuintrf_temp_str(), "R7:%08X", R7);	break;
		case CPUINFO_STR_REGISTER + V60_R8:				sprintf(info->s = cpuintrf_temp_str(), "R8:%08X", R8);	break;
		case CPUINFO_STR_REGISTER + V60_R9:				sprintf(info->s = cpuintrf_temp_str(), "R9:%08X", R9);	break;
		case CPUINFO_STR_REGISTER + V60_R10:			sprintf(info->s = cpuintrf_temp_str(), "R10:%08X", R10); break;
		case CPUINFO_STR_REGISTER + V60_R11:			sprintf(info->s = cpuintrf_temp_str(), "R11:%08X", R11); break;
		case CPUINFO_STR_REGISTER + V60_R12:			sprintf(info->s = cpuintrf_temp_str(), "R12:%08X", R12); break;
		case CPUINFO_STR_REGISTER + V60_R13:			sprintf(info->s = cpuintrf_temp_str(), "R13:%08X", R13); break;
		case CPUINFO_STR_REGISTER + V60_R14:			sprintf(info->s = cpuintrf_temp_str(), "R14:%08X", R14); break;
		case CPUINFO_STR_REGISTER + V60_R15:			sprintf(info->s = cpuintrf_temp_str(), "R15:%08X", R15); break;
		case CPUINFO_STR_REGISTER + V60_R16:			sprintf(info->s = cpuintrf_temp_str(), "R16:%08X", R16); break;
		case CPUINFO_STR_REGISTER + V60_R17:			sprintf(info->s = cpuintrf_temp_str(), "R17:%08X", R17); break;
		case CPUINFO_STR_REGISTER + V60_R18:			sprintf(info->s = cpuintrf_temp_str(), "R18:%08X", R18); break;
		case CPUINFO_STR_REGISTER + V60_R19:			sprintf(info->s = cpuintrf_temp_str(), "R19:%08X", R19); break;
		case CPUINFO_STR_REGISTER + V60_R20:			sprintf(info->s = cpuintrf_temp_str(), "R20:%08X", R20); break;
		case CPUINFO_STR_REGISTER + V60_R21:			sprintf(info->s = cpuintrf_temp_str(), "R21:%08X", R21); break;
		case CPUINFO_STR_REGISTER + V60_R22:			sprintf(info->s = cpuintrf_temp_str(), "R22:%08X", R22); break;
		case CPUINFO_STR_REGISTER + V60_R23:			sprintf(info->s = cpuintrf_temp_str(), "R23:%08X", R23); break;
		case CPUINFO_STR_REGISTER + V60_R24:			sprintf(info->s = cpuintrf_temp_str(), "R24:%08X", R24); break;
		case CPUINFO_STR_REGISTER + V60_R25:			sprintf(info->s = cpuintrf_temp_str(), "R25:%08X", R25); break;
		case CPUINFO_STR_REGISTER + V60_R26:			sprintf(info->s = cpuintrf_temp_str(), "R26:%08X", R26); break;
		case CPUINFO_STR_REGISTER + V60_R27:			sprintf(info->s = cpuintrf_temp_str(), "R27:%08X", R27); break;
		case CPUINFO_STR_REGISTER + V60_R28:			sprintf(info->s = cpuintrf_temp_str(), "R28:%08X", R28); break;
		case CPUINFO_STR_REGISTER + V60_AP:				sprintf(info->s = cpuintrf_temp_str(), "AP:%08X", AP); break;
		case CPUINFO_STR_REGISTER + V60_FP:				sprintf(info->s = cpuintrf_temp_str(), "FP:%08X", FP); break;
		case CPUINFO_STR_REGISTER + V60_SP:				sprintf(info->s = cpuintrf_temp_str(), "SP:%08X", SP); break;
		case CPUINFO_STR_REGISTER + V60_PC:				sprintf(info->s = cpuintrf_temp_str(), "PC:%08X", PC); break;
		case CPUINFO_STR_REGISTER + V60_PSW:			sprintf(info->s = cpuintrf_temp_str(), "PSW:%08X", v60ReadPSW()); break;
		case CPUINFO_STR_REGISTER + V60_ISP:			sprintf(info->s = cpuintrf_temp_str(), "ISP:%08X", ISP); break;
		case CPUINFO_STR_REGISTER + V60_L0SP:			sprintf(info->s = cpuintrf_temp_str(), "L0SP:%08X", L0SP); break;
		case CPUINFO_STR_REGISTER + V60_L1SP:			sprintf(info->s = cpuintrf_temp_str(), "L1SP:%08X", L1SP); break;
		case CPUINFO_STR_REGISTER + V60_L2SP:			sprintf(info->s = cpuintrf_temp_str(), "L2SP:%08X", L2SP); break;
		case CPUINFO_STR_REGISTER + V60_L3SP:			sprintf(info->s = cpuintrf_temp_str(), "L3SP:%08X", L3SP); break;
		case CPUINFO_STR_REGISTER + V60_SBR:			sprintf(info->s = cpuintrf_temp_str(), "SBR:%08X", SBR); break;
		case CPUINFO_STR_REGISTER + V60_TR:				sprintf(info->s = cpuintrf_temp_str(), "TR:%08X", TR); break;
		case CPUINFO_STR_REGISTER + V60_SYCW:			sprintf(info->s = cpuintrf_temp_str(), "SYCW:%08X", SYCW); break;
		case CPUINFO_STR_REGISTER + V60_TKCW:			sprintf(info->s = cpuintrf_temp_str(), "TKCW:%08X", TKCW); break;
		case CPUINFO_STR_REGISTER + V60_PIR:			sprintf(info->s = cpuintrf_temp_str(), "PIR:%08X", PIR); break;
		case CPUINFO_STR_REGISTER + V60_PSW2:			sprintf(info->s = cpuintrf_temp_str(), "PSW2:%08X", PSW2); break;
		case CPUINFO_STR_REGISTER + V60_ATBR0:			sprintf(info->s = cpuintrf_temp_str(), "ATBR0:%08X", ATBR0); break;
		case CPUINFO_STR_REGISTER + V60_ATLR0:			sprintf(info->s = cpuintrf_temp_str(), "ATLR0:%08X", ATLR0); break;
		case CPUINFO_STR_REGISTER + V60_ATBR1:			sprintf(info->s = cpuintrf_temp_str(), "ATBR1:%08X", ATBR1); break;
		case CPUINFO_STR_REGISTER + V60_ATLR1:			sprintf(info->s = cpuintrf_temp_str(), "ATLR1:%08X", ATLR1); break;
		case CPUINFO_STR_REGISTER + V60_ATBR2:			sprintf(info->s = cpuintrf_temp_str(), "ATBR2:%08X", ATBR2); break;
		case CPUINFO_STR_REGISTER + V60_ATLR2:			sprintf(info->s = cpuintrf_temp_str(), "ATLR2:%08X", ATLR2); break;
		case CPUINFO_STR_REGISTER + V60_ATBR3:			sprintf(info->s = cpuintrf_temp_str(), "ATBR3:%08X", ATBR3); break;
		case CPUINFO_STR_REGISTER + V60_ATLR3:			sprintf(info->s = cpuintrf_temp_str(), "ATLR3:%08X", ATLR3); break;
		case CPUINFO_STR_REGISTER + V60_TRMODE:			sprintf(info->s = cpuintrf_temp_str(), "TRMODE:%08X", TRMODE); break;
		case CPUINFO_STR_REGISTER + V60_ADTR0:			sprintf(info->s = cpuintrf_temp_str(), "ADTR0:%08X", ADTR0); break;
		case CPUINFO_STR_REGISTER + V60_ADTR1:			sprintf(info->s = cpuintrf_temp_str(), "ADTR1:%08X", ADTR1); break;
		case CPUINFO_STR_REGISTER + V60_ADTMR0:			sprintf(info->s = cpuintrf_temp_str(), "ADTMR0:%08X", ADTMR0); break;
		case CPUINFO_STR_REGISTER + V60_ADTMR1:			sprintf(info->s = cpuintrf_temp_str(), "ADTMR1:%08X", ADTMR1); break;
	}
}


/**************************************************************************
 * CPU-specific set_info
 **************************************************************************/

void v70_get_info(UINT32 state, union cpuinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case CPUINFO_INT_DATABUS_WIDTH + ADDRESS_SPACE_PROGRAM:	info->i = 32;					break;
		case CPUINFO_INT_ADDRBUS_WIDTH + ADDRESS_SPACE_PROGRAM: info->i = 32;					break;
		case CPUINFO_INT_ADDRBUS_SHIFT + ADDRESS_SPACE_PROGRAM: info->i = 0;					break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case CPUINFO_PTR_INIT:							info->init = v70_init;					break;
#ifdef MAME_DEBUG
		case CPUINFO_PTR_DISASSEMBLE:					info->disassemble = v70_dasm;			break;
#endif /* MAME_DEBUG */

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case CPUINFO_STR_NAME:							strcpy(info->s = cpuintrf_temp_str(), "V70"); break;

		default:
			v60_get_info(state, info);
			break;
	}
}
#endif
