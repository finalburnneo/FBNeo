#ifndef _Z80_H_
#define _Z80_H_

#define	CPUINFO_PTR_CPU_SPECIFIC	0x18000
#define Z80_CLEAR_LINE		0
#define Z80_ASSERT_LINE		1
#define Z80_INPUT_LINE_NMI	32

#include "z80daisy.h"

typedef union
{
#ifdef LSB_FIRST
	struct { UINT8 l,h,h2,h3; } b;
	struct { UINT16 l,h; } w;
#else
	struct { UINT8 h3,h2,h,l; } b;
	struct { UINT16 h,l; } w;
#endif
	UINT32 d;
} Z80_PAIR;

typedef struct
{
	Z80_PAIR	prvpc,pc,sp,af,bc,de,hl,ix,iy;
	Z80_PAIR	af2,bc2,de2,hl2,wz;
	UINT8	r,r2,iff1,iff2,halt,im,i;
	UINT8	nmi_state;			/* nmi line state */
	UINT8	nmi_pending;		/* nmi pending */
	UINT8	irq_state;			/* irq line state */
	UINT8   vector;             /* vector */
	UINT8	after_ei;			/* are we in the EI shadow? */
	INT32   cycles_left;
	INT32   ICount;
	INT32   end_run;
	UINT32  EA;
	INT32   hold_irq;

	const struct z80_irq_daisy_chain *daisy;
	int (*irq_callback)(int irqline);

	int (*spectrum_tape_cb)();
	int spectrum_mode;
} Z80_Regs;

enum {
	Z80_PC=1, Z80_SP,
	Z80_A, Z80_B, Z80_C, Z80_D, Z80_E, Z80_H, Z80_L,
	Z80_AF, Z80_BC, Z80_DE, Z80_HL,
	Z80_IX, Z80_IY,	Z80_AF2, Z80_BC2, Z80_DE2, Z80_HL2,
	Z80_R, Z80_I, Z80_IM, Z80_IFF1, Z80_IFF2, Z80_HALT,
	Z80_DC0, Z80_DC1, Z80_DC2, Z80_DC3
};

enum {
	Z80_TABLE_op,
	Z80_TABLE_cb,
	Z80_TABLE_ed,
	Z80_TABLE_xy,
	Z80_TABLE_xycb,
	Z80_TABLE_ex	/* cycles counts for taken jr/jp/call and interrupt latency (rst opcodes) */
};

enum
{
	CPUINFO_PTR_Z80_CYCLE_TABLE = CPUINFO_PTR_CPU_SPECIFIC,
	CPUINFO_PTR_Z80_CYCLE_TABLE_LAST = CPUINFO_PTR_Z80_CYCLE_TABLE + Z80_TABLE_ex
};

void Z80Init();
void Z80InitContention(int is_on_type, void (*rastercallback)(int));
void Z80Contention_set_bank(int bankno);
void Z80Reset();
void Z80Exit();
int  Z80Execute(int cycles);
void Z80Burn(int cycles);
void Z80SetIrqLine(int irqline, int state);
void Z80GetContext (void *dst);
void Z80SetContext (void *src);
int Z80Scan(int nAction);
INT32 z80TotalCycles();
void Z80StopExecute();
void z80_set_spectrum_tape_callback(int (*tape_cb)());

extern unsigned char Z80Vector;
extern void (*z80edfe_callback)(Z80_Regs *Regs);
extern int z80_ICount;
extern UINT32 EA;

typedef unsigned char (__fastcall *Z80ReadIoHandler)(unsigned int a);
typedef void (__fastcall *Z80WriteIoHandler)(unsigned int a, unsigned char v);
typedef unsigned char (__fastcall *Z80ReadProgHandler)(unsigned int a);
typedef void (__fastcall *Z80WriteProgHandler)(unsigned int a, unsigned char v);
typedef unsigned char (__fastcall *Z80ReadOpHandler)(unsigned int a);
typedef unsigned char (__fastcall *Z80ReadOpArgHandler)(unsigned int a);

void Z80SetIOReadHandler(Z80ReadIoHandler handler);
void Z80SetIOWriteHandler(Z80WriteIoHandler handler);
void Z80SetProgramReadHandler(Z80ReadProgHandler handler);
void Z80SetProgramWriteHandler(Z80WriteProgHandler handler);
void Z80SetCPUOpReadHandler(Z80ReadOpHandler handler);
void Z80SetCPUOpArgReadHandler(Z80ReadOpArgHandler handler);

void ActiveZ80SetPC(int pc);
int ActiveZ80GetPC();
int ActiveZ80GetAF();
int ActiveZ80GetAF2();
void ActiveZ80SetAF2(int af2);
int ActiveZ80GetBC();
int ActiveZ80GetDE();
int ActiveZ80GetHL();
int ActiveZ80GetI();
int ActiveZ80GetIX();
int ActiveZ80GetIM();
int ActiveZ80GetSP();
int ActiveZ80GetPrevPC();
void ActiveZ80SetCarry(int carry);
int ActiveZ80GetCarry();
int ActiveZ80GetCarry2();
void ActiveZ80EXAF();
int ActiveZ80GetPOP();
int ActiveZ80GetA();
void ActiveZ80SetA(int a);
int ActiveZ80GetF();
void ActiveZ80SetF(int f);
int ActiveZ80GetIFF1();
int ActiveZ80GetIFF2();
void ActiveZ80SetDE(int de);
void ActiveZ80SetHL(int hl);
void ActiveZ80SetIX(int ix);
void ActiveZ80SetSP(int sp);

void ActiveZ80SetIRQHold();
int ActiveZ80GetVector();
void ActiveZ80SetVector(INT32 vector);

#define MAX_CMSE	9	//Maximum contended memory script elements
#define MAX_RWINFO	6	//Maximum reads/writes per opcode
#define MAX_CM_SCRIPTS 37

enum CMSE_TYPES
{
	CMSE_TYPE_MEMORY,
	CMSE_TYPE_IO_PORT,
	CMSE_TYPE_IR_REGISTER,
	CMSE_TYPE_BC_REGISTER,
	CMSE_TYPE_UNCONTENDED
};

enum ULA_VARIANT_TYPES
{
	ULA_VARIANT_NONE,
	ULA_VARIANT_SINCLAIR,
	ULA_VARIANT_AMSTRAD
};

enum RWINFO_FLAGS
{
	RWINFO_READ      = 0x01,
	RWINFO_WRITE     = 0x02,
	RWINFO_IO_PORT   = 0x04,
	RWINFO_MEMORY    = 0x08,
	RWINFO_PROCESSED = 0x10
};

typedef struct ContendedMemoryScriptElement
{
	int	rw_ix;
	int	inst_cycles;
	int     type;
	int	multiplier;
	bool	is_optional;
}CMSE;

typedef struct ContendedMemoryScriptBreakdown
{
	CMSE elements[MAX_CMSE];
	int  number_of_elements;
	int  inst_cycles_mandatory;
	int  inst_cycles_optional;
	int  inst_cycles_total;
}CM_SCRIPT_BREAKDOWN;

typedef struct ContendedMemoryScriptDescription
{
	const char*		sinclair;
	const char*		amstrad;
}CM_SCRIPT_DESCRIPTION;

typedef struct ContendedMemoryScript
{
	int 			id;
	const char*		desc;
	CM_SCRIPT_BREAKDOWN	breakdown;
}CM_SCRIPT;

typedef struct MemoryReadWriteInformation
{
	UINT16   addr;
	UINT8    val;
        UINT16   flags;
	const char *dbg;
} RWINFO;

typedef struct OpcodeHistory
{
	bool     capturing;
	RWINFO   rw[MAX_RWINFO];
	int      rw_count;
	int      tstate_start;
	UINT16 register_ir;
	UINT16 register_bc;

	int 	 uncontended_cycles_predicted;
	int      uncontended_cycles_eaten;
	bool     do_optional;

	CM_SCRIPT           *script;
	CM_SCRIPT_BREAKDOWN *breakdown;
	int                 element;
}OPCODE_HISTORY;

enum CYCLES_TYPE
{
	CYCLES_ISR,		// Cycles eaten when processing interrupts
	CYCLES_EXEC,		// Cycles eaten when the EXEC() macro is called
	CYCLES_CONTENDED,	// Contended cycles eaten when processing opcode history (specz80_device only)
	CYCLES_UNCONTENDED	// Uncontended cycles eaten when processing opcode history (specz80_device only)
};

#endif

