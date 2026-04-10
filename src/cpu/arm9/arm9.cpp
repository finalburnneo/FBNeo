/*****************************************************************************
 *
 *   arm9.cpp
 *   ARM946E-S CPU wrapper for FBNeo - PGM2 / IGS036
 *
 *   Based on arm7.cpp by Steve Ellenoff.
 *   ARM9 extensions (ARMv5TE ISA, CP15) added for PGM2 support.
 *
 *   The ARM9 core works by reusing arm7core.c / arm7exec.c with:
 *     - ARM9_MODE defined to 1  (enables ARMv5TE instructions)
 *     - All ARM7 memory callbacks remapped to Arm9* variants
 *     - Coprocessor callback globals renamed to avoid link conflict
 *     - CPU state struct and icount renamed (arm9 / arm9_icount)
 *
 *****************************************************************************/

#include "burnint.h"
#include "arm7/arm7core.h"
#include "arm9_intf.h"

// ---------------------------------------------------------------------------
// Redirect ARM7 memory I/O to ARM9 equivalents BEFORE including arm7core.c
// ---------------------------------------------------------------------------
#define Arm7WriteByte         Arm9WriteByte
#define Arm7WriteWord         Arm9WriteWord
#define Arm7WriteLong         Arm9WriteLong
#define Arm7ReadByte          Arm9ReadByte
#define Arm7ReadWord          Arm9ReadWord
#define Arm7ReadLong          Arm9ReadLong
#define Arm7FetchWord         Arm9FetchWord
#define Arm7FetchLong         Arm9FetchLong
#define Arm7RunEndEatCycles   Arm9RunEndEatCycles

// ---------------------------------------------------------------------------
// Rename coprocessor callback globals to avoid linker clash with arm7.cpp
// ---------------------------------------------------------------------------
#define arm7_coproc_do_callback   arm9_coproc_do_callback
#define arm7_coproc_rt_r_callback arm9_coproc_rt_r_callback
#define arm7_coproc_rt_w_callback arm9_coproc_rt_w_callback
#define arm7_coproc_dt_r_callback arm9_coproc_dt_r_callback
#define arm7_coproc_dt_w_callback arm9_coproc_dt_w_callback

// ---------------------------------------------------------------------------
// ARM9_MODE = 1 enables ARMv5TE instructions in arm7exec.c
// ---------------------------------------------------------------------------
#define ARM9_MODE 1

// ---------------------------------------------------------------------------
// CPU state macros - point to our arm9-specific state
// ---------------------------------------------------------------------------
#define ARM7REG(reg)    arm9.sArmRegister[reg]
#define ARM7            arm9
#define ARM7_ICOUNT     arm9_icount

/* CPU Registers */
typedef struct
{
	ARM7CORE_REGS
} ARM9_REGS;

static ARM9_REGS arm9;
static int ARM9_ICOUNT_VAR;
#define arm9_icount ARM9_ICOUNT_VAR

static int arm9_total_cycles = 0;
static int arm9_curr_cycles  = 0;
static int arm9_end_run      = 0;

// arm7exec.c references total_cycles/curr_cycles/end_run directly
#define total_cycles  arm9_total_cycles
#define curr_cycles   arm9_curr_cycles
#define end_run       arm9_end_run

// CP15 state
static UINT32 arm9_cp15_regs[16 * 8 * 16 * 8];

static inline UINT32 arm9_cp15_index(UINT32 insn)
{
	UINT32 crn = (insn >> 16) & 0x0f;
	UINT32 op1 = (insn >> 21) & 0x07;
	UINT32 crm = insn & 0x0f;
	UINT32 op2 = (insn >> 5) & 0x07;
	return (((crn * 8) + op1) * 16 + crm) * 8 + op2;
}

static UINT32 arm9_cp15_read(UINT32 insn)
{
	UINT32 cp_num = (insn >> 8) & 0x0f;
	if (cp_num != 15) return 0;

	UINT32 crn = (insn >> 16) & 0x0f;
	UINT32 op1 = (insn >> 21) & 0x07;
	UINT32 crm = insn & 0x0f;
	UINT32 op2 = (insn >> 5) & 0x07;

	// Main ID register - ARM946E-S
	if (crn == 0 && op1 == 0 && crm == 0 && op2 == 0) {
		return 0x41069265;
	}

	return arm9_cp15_regs[arm9_cp15_index(insn)];
}

static void arm9_cp15_write(UINT32 insn, UINT32 value)
{
	UINT32 cp_num = (insn >> 8) & 0x0f;
	if (cp_num != 15) return;

	UINT32 crn = (insn >> 16) & 0x0f;
	UINT32 op1 = (insn >> 21) & 0x07;
	UINT32 crm = insn & 0x0f;
	UINT32 op2 = (insn >> 5) & 0x07;

	arm9_cp15_regs[arm9_cp15_index(insn)] = value;
}

// Memory access macros required by arm7core.c and arm7exec.c
#define READ8(addr)         arm7_cpu_read8(addr)
#define WRITE8(addr,data)   arm7_cpu_write8(addr,data)
#define READ16(addr)        arm7_cpu_read16(addr)
#define WRITE16(addr,data)  arm7_cpu_write16(addr,data)
#define READ32(addr)        arm7_cpu_read32(addr)
#define WRITE32(addr,data)  arm7_cpu_write32(addr,data)
#define PTR_READ32          &arm7_cpu_read32
#define PTR_WRITE32         &arm7_cpu_write32

// Include the ARM7 core - all static functions compile fresh in this TU
#include "arm7/arm7core.c"

void Arm9Open(int) {}
void Arm9Close()   {}

int Arm9TotalCycles()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9TotalCycles called without init\n"));
#endif
	return arm9_total_cycles + (arm9_curr_cycles - arm9_icount);
}

void Arm9RunEndEatCycles()
{
	arm9_icount = 0;
}

void Arm9BurnUntilInterrupt()
{
	arm7_burn_until_irq(1);
}

void Arm9RunEnd()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9RunEnd called without init\n"));
#endif
	arm9_end_run = 1;
}

void Arm9BurnCycles(int cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9BurnCycles called without init\n"));
#endif
	arm9_icount -= cycles;
}

INT32 Arm9Idle(int cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9Idle called without init\n"));
#endif
	arm9_total_cycles += cycles;
	return cycles;
}

void Arm9NewFrame()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9NewFrame called without init\n"));
#endif
	arm9_total_cycles = 0;
}

void Arm9Reset()
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9Reset called without init\n"));
#endif
	memset(arm9_cp15_regs, 0, sizeof(arm9_cp15_regs));
	arm9_coproc_rt_r_callback = arm9_cp15_read;
	arm9_coproc_rt_w_callback = arm9_cp15_write;
	arm7_core_reset();
}

int Arm9Run(int cycles)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9Run called without init\n"));
#endif

/* include the arm9 core execute code (ARM9_MODE=1 enables ARMv5TE) */
#include "arm7/arm7exec.c"
}

void arm9_set_irq_line(int irqline, int state)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("arm9_set_irq_line called without init\n"));
#endif

	arm7_burn_until_irq(0);
	arm7_core_set_irq_line(irqline, state);
}

UINT32 Arm9DbgGetPC()
{
	return arm9.sArmRegister[eR15];
}

UINT32 Arm9DbgGetCPSR()
{
	return arm9.sArmRegister[eCPSR];
}

UINT32 Arm9DbgGetRegister(INT32 index)
{
	if (index < 0 || index >= kNumRegisters) return 0;
	return arm9.sArmRegister[index];
}

void Arm9DbgSetRegister(INT32 index, UINT32 value)
{
	if (index >= 0 && index < kNumRegisters)
		arm9.sArmRegister[index] = value;
}

UINT32 Arm9DbgGetCp15Value(UINT32 crn, UINT32 op1, UINT32 crm, UINT32 op2)
{
	if (crn == 0 && op1 == 0 && crm == 0 && op2 == 0) return 0x41069265;
	UINT32 insn = ((op1 & 7) << 21) | ((crn & 0x0f) << 16) | ((op2 & 7) << 5) | (crm & 0x0f) | (15 << 8);
	return arm9_cp15_regs[arm9_cp15_index(insn)];
}

int Arm9Scan(int nAction)
{
#if defined FBNEO_DEBUG
	if (!DebugCPU_ARM9Initted) bprintf(PRINT_ERROR, _T("Arm9Scan called without init\n"));
#endif

	struct BurnArea ba;

	if (nAction & ACB_DRIVER_DATA) {
		memset(&ba, 0, sizeof(ba));
		ba.Data   = (unsigned char*)&arm9;
		ba.nLen   = sizeof(arm9);
		ba.szName = "ARM9 Registers";
		BurnAcb(&ba);

		SCAN_VAR(arm9_icount);
		SCAN_VAR(arm9_total_cycles);
		SCAN_VAR(arm9_curr_cycles);
		SCAN_VAR(arm9_cp15_regs);
		SCAN_VAR(burn_until_irq);
	}

	return 0;
}
