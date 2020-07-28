/********************************************************************
 Hyperstone cpu emulator
 written by Pierpaolo Prazzoli

 All the types are compatible, but they have different IRAM size and cycles

 Hyperstone models:

 16 bits
 - E1-16T
 - E1-16XT
 - E1-16XS
 - E1-16XSR

 32bits
 - E1-32N   or  E1-32T
 - E1-32XN  or  E1-32XT
 - E1-32XS
 - E1-32XSR

 Hynix models:

 16 bits
 - GMS30C2116
 - GMS30C2216

 32bits
 - GMS30C2132
 - GMS30C2232

 TODO:
 - some wrong cycle counts

 CHANGELOG:

 Pierpaolo Prazzoli
 - Fixed LDxx.N/P/S opcodes not to increment the destination register when
   it's the same as the source or "next source" one.

 Pierpaolo Prazzoli
 - Removed nested delays
 - Added better delay branch support
 - Fixed PC seen by a delay instruction, because a delay instruction
   should use the delayed PC (thus allowing the execution of software
   opcodes too)

 Tomasz Slanina
 - Fixed delayed branching for delay instructions longer than 2 bytes

 Pierpaolo Prazzoli
 - Added and fixed Timer without hack

 Tomasz Slanina
 - Fixed MULU/MULS
 - Fixed Carry in ADDC/SUBC

 Pierpaolo Prazzoli
 - Fixed software opcodes used as delay instructions
 - Added nested delays

 Tomasz Slanina
 - Added "undefined" C flag to shift left instructions

 Pierpaolo Prazzoli
 - Added interrupts-block for delay instructions
 - Fixed get_emu_code_addr
 - Added LDW.S and STW.S instructions
 - Fixed floating point opcodes

 Tomasz Slanina
 - interrputs after call and before frame are prohibited now
 - emulation of FCR register
 - Floating point opcodes (preliminary)
 - Fixed stack addressing in RET/FRAME opcodes
 - Fixed bug in SET_RS macro
 - Fixed bug in return opcode (S flag)
 - Added C/N flags calculation in add/adc/addi/adds/addsi and some shift opcodes
 - Added writeback to ROL
 - Fixed ROL/SAR/SARD/SHR/SHRD/SHL/SHLD opcode decoding (Local/Global regs)
 - Fixed I and T flag in RET opcode
 - Fixed XX/XM opcodes
 - Fixed MOV opcode, when RD = PC
 - Fixed execute_trap()
 - Fixed ST opcodes, when when RS = SR
 - Added interrupts
 - Fixed I/O addressing

 Pierpaolo Prazzoli
 - Fixed fetch
 - Fixed decode of hyperstone_xm opcode
 - Fixed 7 bits difference number in FRAME / RET instructions
 - Some debbugger fixes
 - Added generic registers decode function
 - Some other little fixes.

 MooglyGuy 29/03/2004
    - Changed MOVI to use unsigned values instead of signed, correcting
      an ugly glitch when loading 32-bit immediates.
 Pierpaolo Prazzoli
    - Same fix in get_const

 MooglyGuy - 02/27/04
    - Fixed delayed branching
    - const_val for CALL should always have bit 0 clear

 Pierpaolo Prazzoli - 02/25/04
    - Fixed some wrong addresses to address local registers instead of memory
    - Fixed FRAME and RET instruction
    - Added preliminary I/O space
    - Fixed some load / store instructions

 Pierpaolo Prazzoli - 02/20/04
    - Added execute_exception function
    - Added FL == 0 always interpreted as 16

 Pierpaolo Prazzoli - 02/19/04
    - Changed the reset to use the execute_trap(reset) which should be right to set
      the initiale state of the cpu
    - Added Trace exception
    - Set of T flag in RET instruction
    - Set I flag in interrupts entries and resetted by a RET instruction
    - Added correct set instruction for SR

 Pierpaolo Prazzoli - 10/26/03
    - Changed get_lrconst to get_const and changed it to use the removed GET_CONST_RR
      macro.
    - Removed the High flag used in some opcodes, it should be used only in
      MOV and MOVI instruction.
    - Fixed MOV and MOVI instruction.
    - Set to 1 FP is SR register at reset.
      (From the doc: A Call, Trap or Software instruction increments the FP and sets FL
      to 6, thus creating a new stack frame with the length of 6 registers).

 MooglyGuy - 10/25/03
    - Fixed CALL enough that it at least jumps to the right address, no word
      yet as to whether or not it's working enough to return.
    - Added get_lrconst() to get the const value for the CALL operand, since
      apparently using immediate_value() was wrong. The code is ugly, but it
      works properly. Vampire 1/2 now gets far enough to try to test its RAM.
    - Just from looking at it, CALL apparently doesn't frame properly. I'm not
      sure about FRAME, but perhaps it doesn't work properly - I'm not entirely
      positive. The return address when vamphalf's memory check routine is
      called at FFFFFD7E is stored in register L8, and then the RET instruction
      at the end of the routine uses L1 as the return address, so that might
      provide some clues as to how it works.
    - I'd almost be willing to bet money that there's no framing at all since
      the values in L0 - L15 as displayed by the debugger would change during a
      CALL or FRAME operation. I'll look when I'm in the mood.
    - The mood struck me, and I took a look at SET_L_REG and GET_L_REG.
      Apparently no matter what the current frame pointer is they'll always use
      local_regs[0] through local_regs[15].

 MooglyGuy - 08/20/03
    - Added H flag support for MOV and MOVI
    - Changed init routine to set S flag on boot. Apparently the CPU defaults to
      supervisor mode as opposed to user mode when it powers on, as shown by the
      vamphalf power-on routines. Makes sense, too, since if the machine booted
      in user mode, it would be impossible to get into supervisor mode.

 Pierpaolo Prazzoli - 08/19/03
    - Added check for D_BIT and S_BIT where PC or SR must or must not be denoted.
      (movd, divu, divs, ldxx1, ldxx2, stxx1, stxx2, mulu, muls, set, mul
      call, chk)

 MooglyGuy - 08/17/03
    - Working on support for H flag, nothing quite done yet
    - Added trap Range Error for CHK PC, PC
    - Fixed relative jumps, they have to be taken from the opcode following the
      jump minstead of the jump opcode itself.

 Pierpaolo Prazzoli - 08/17/03
    - Fixed get_pcrel() when OP & 0x80 is set.
    - Decremented PC by 2 also in MOV, ADD, ADDI, SUM, SUB and added the check if
      D_BIT is not set. (when pc is changed they are implicit branch)

 MooglyGuy - 08/17/03
    - Implemented a crude hack to set FL in the SR to 6, since according to the docs
      that's supposed to happen each time a trap occurs, apparently including when
      the processor starts up. The 3rd opcode executed in vamphalf checks to see if
      the FL flag in SR 6, so it's apparently the "correct" behaviour despite the
      docs not saying anything on it. If FL is not 6, the branch falls through and
      encounters a CHK PC, L2, which at that point will always throw a range trap.
      The range trap vector contains 00000000 (CHK PC, PC), which according to the
      docs will always throw a range trap (which would effectively lock the system).
      This revealed a bug: CHK PC, PC apparently does not throw a range trap, which
      needs to be fixed. Now that the "correct" behaviour is hacked in with the FL
      flags, it reveals yet another bug in that the branch is interpreted as being
      +0x8700. This means that the PC then wraps around to 000082B0, give or take
      a few bytes. While it does indeed branch to valid code, I highly doubt that
      this is the desired effect. Check for signed/unsigned relative branch, maybe?

 MooglyGuy - 08/16/03
    - Fixed the debugger at least somewhat so that it displays hex instead of decimal,
      and so that it disassembles opcodes properly.
    - Fixed hyperstone_execute() to increment PC *after* executing the opcode instead of
      before. This is probably why vamphalf was booting to fffffff8, but executing at
      fffffffa instead.
    - Changed execute_trap to decrement PC by 2 so that the next opcode isn't skipped
      after a trap
    - Changed execute_br to decrement PC by 2 so that the next opcode isn't skipped
      after a branch
    - Changed hyperstone_movi to decrement PC by 2 when G0 (PC) is modified so that the
      next opcode isn't skipped after a branch
    - Changed hyperstone_movi to default to a UINT32 being moved into the register
      as opposed to a UINT8. This is wrong, the bit width is quite likely to be
      dependent on the n field in the Rimm instruction type. However, vamphalf uses
      MOVI G0,[FFFF]FBAC (n=$13) since there's apparently no absolute branch opcode.
      What kind of CPU is this that it doesn't have an absolute jump in its branch
      instructions and you have to use an immediate MOV to do an abs. jump!?
    - Replaced usage of logerror() with smf's verboselog()

*********************************************************************/

#include "burnint.h"
#include "e132xs.h"
#include "e132xs_intf.h"

static UINT8 *mem[2][0x100000];

static void (*write_byte_handler)(UINT32,UINT8);
static void (*write_word_handler)(UINT32,UINT16);
static void (*write_dword_handler)(UINT32,UINT32);
static UINT8 (*read_byte_handler)(UINT32);
static UINT16 (*read_word_handler)(UINT32);
static UINT32 (*read_dword_handler)(UINT32);

static void (*io_write_dword_handler)(UINT32,UINT32);
static UINT32 (*io_read_dword_handler)(UINT32);

void E132XSSetWriteByteHandler(void (*handler)(UINT32,UINT8))
{
	write_byte_handler = handler;
}

void E132XSSetWriteWordHandler(void (*handler)(UINT32,UINT16))
{
	write_word_handler = handler;
}

void E132XSSetWriteLongHandler(void (*handler)(UINT32,UINT32))
{
	write_dword_handler = handler;
}

void E132XSSetReadByteHandler(UINT8 (*handler)(UINT32))
{
	read_byte_handler = handler;
}

void E132XSSetReadWordHandler(UINT16 (*handler)(UINT32))
{
	read_word_handler = handler;
}

void E132XSSetReadLongHandler(UINT32 (*handler)(UINT32))
{
	read_dword_handler = handler;
}

void E132XSSetIOWriteHandler(void (*handler)(UINT32,UINT32))
{
	io_write_dword_handler = handler;
}

void E132XSSetIOReadHandler(UINT32 (*handler)(UINT32))
{
	io_read_dword_handler = handler;
}

void E132XSMapMemory(UINT8 *ptr, UINT32 start, UINT32 end, INT32 flags)
{
	start >>= 12;
	end >>= 12;

	for (UINT32 i = 0; i < (end - start) + 1; i++)
	{
		if (flags & MAP_READ) mem[0][start + i] = (ptr == NULL) ? NULL : (ptr + (i << 12));
		if (flags & MAP_WRITE) mem[1][start + i] = (ptr == NULL) ? NULL : (ptr + (i << 12));
	}
}

static UINT8 program_read_byte_16be(UINT32 address)
{
//	bprintf (0, _T("RB: %8.8x\n"), address);

	UINT8 *ptr = mem[0][address >> 12];

	if (ptr) {
		return ptr[(address & 0xfff)^1];
	}

	if (read_byte_handler) {
		return read_byte_handler(address);
	}

	return 0;
}

static UINT16 program_read_word_16be(UINT32 address)
{
//	bprintf (0, _T("RW: %8.8x\n"), address);

	UINT8 *ptr = mem[0][address >> 12];

	if (ptr) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(ptr + (address & 0xffe))));
	}

	if (read_word_handler) {
		return read_word_handler(address);
	}

	return 0;
}

static UINT32 program_read_dword_32be(UINT32 address)
{
//	bprintf (0, _T("RL: %8.8x\n"), address);

	UINT8 *ptr = mem[0][address >> 12];

	if (ptr) {
		UINT32 ret = *((UINT32*)(ptr + (address & 0xffe))); // word aligned
		return BURN_ENDIAN_SWAP_INT32((ret << 16) | (ret >> 16));
	}

	if (read_dword_handler) {
		return read_dword_handler(address);
	}

	return 0;
}

static UINT16 cpu_readop16(UINT32 address)
{
//	bprintf (0, _T("RO: %8.8x\n"), address);

	UINT8 *ptr = mem[0][address >> 12];

	if (ptr) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(ptr + (address & 0xffe))));
	}

	if (read_word_handler) {
		return read_word_handler(address);
	}

	return 0;
}

static void program_write_byte_16be(UINT32 address, UINT8 data)
{
//	bprintf (0, _T("WB: %8.8x, %2.2x\n"), address, data);

	UINT8 *ptr = mem[1][address >> 12];

	if (ptr) {
		ptr[(address & 0xfff)^1] = data;
		return;
	}

	if (write_byte_handler) {
		write_byte_handler(address, data);
	}
}

static void program_write_word_16be(UINT32 address, UINT16 data)
{
//	bprintf (0, _T("WW: %8.8x, %4.4x\n"), address, data);

	UINT8 *ptr = mem[1][address >> 12];

	if (ptr) {
		*((UINT16*)(ptr + (address & 0xffe))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (write_word_handler) {
		write_word_handler(address, data);
	}
}

static void program_write_dword_32be(UINT32 address, UINT32 data)
{
//	bprintf (0, _T("WL: %8.8x, %8.8x\n"), address, data);

	UINT8 *ptr = mem[1][address >> 12];

	if (ptr) {
		data = (data << 16) | (data >> 16);
		*((UINT32*)(ptr + (address & 0xffe))) = BURN_ENDIAN_SWAP_INT32(data); // word aligned!
		return;
	}

	if (write_dword_handler) {
		write_dword_handler(address, data);
	}
}

static void io_write_dword_32be(UINT32 address, UINT32 data)
{
//	bprintf (0, _T("IOL32: %8.8x %8.8x\n"), address,data);

	if (io_write_dword_handler) {
		io_write_dword_handler(address, data);
		return;
	}
}

static UINT32 io_read_dword_32be(UINT32 address)
{
//	bprintf (0, _T("IORL32: %8.8x\n"), address);

	if (io_read_dword_handler) {
		return io_read_dword_handler(address);
	}

	return 0;
}

#define CONCAT_64(A, B)		((((UINT64)(A))<<32) | (UINT32)(B))
#define EXTRACT_64HI(A) 	(((UINT64)(A)) >> 32)
#define EXTRACT_64LO(A)		((A) & 0xffffffff)

static UINT64 mulu_32x32(UINT32 a, UINT32 b)
{
	return (UINT64)(((UINT64)a) * ((UINT64)b));
}

#ifdef MAME_DEBUG
#define DEBUG_PRINTF(x) do { osd_printf_debug x; } while (0)
#else
#define DEBUG_PRINTF(x) do { } while (0)
#endif

// set C in adds/addsi/subs/sums
#define SETCARRYS 0
#define MISSIONCRAFT_FLAGS 1

/* Registers */

/* Internal registers */

#define SREG  (decode)->src_value
#define SREGF (decode)->next_src_value
#define DREG  (decode)->dst_value
#define DREGF (decode)->next_dst_value
#define EXTRA_U (decode)->extra.u
#define EXTRA_S (decode)->extra.s

#define SET_SREG( _data_ )  ((decode)->src_is_local ? set_local_register((decode)->src, (UINT32)_data_) : set_global_register((decode)->src, (UINT32)_data_))
#define SET_SREGF( _data_ ) ((decode)->src_is_local ? set_local_register((decode)->src + 1, (UINT32)_data_) : set_global_register((decode)->src + 1, (UINT32)_data_))
#define SET_DREG( _data_ )  ((decode)->dst_is_local ? set_local_register((decode)->dst, (UINT32)_data_) : set_global_register((decode)->dst, (UINT32)_data_))
#define SET_DREGF( _data_ ) ((decode)->dst_is_local ? set_local_register((decode)->dst + 1, (UINT32)_data_) : set_global_register((decode)->dst + 1, (UINT32)_data_))

#define SRC_IS_PC      (!(decode)->src_is_local && (decode)->src == PC_REGISTER)
#define DST_IS_PC      (!(decode)->dst_is_local && (decode)->dst == PC_REGISTER)
#define SRC_IS_SR      (!(decode)->src_is_local && (decode)->src == SR_REGISTER)
#define DST_IS_SR      (!(decode)->dst_is_local && (decode)->dst == SR_REGISTER)
#define SAME_SRC_DST   (decode)->same_src_dst
#define SAME_SRC_DSTF  (decode)->same_src_dstf
#define SAME_SRCF_DST  (decode)->same_srcf_dst

/* Delay information */
struct delay_info
{
	INT32   delay_cmd;
	UINT32  delay_pc;
};

// CPU registers
static UINT32  m_global_regs[32];
static UINT32  m_local_regs[64];

static UINT8 internal_ram[0x4000];

/* internal stuff */
static UINT32  m_ppc;          // previous pc
static UINT16  m_op;           // opcode
static UINT32  m_trap_entry;   // entry point to get trap address

static UINT8   m_clock_scale_mask;
static UINT8   m_clock_scale;
static UINT8   m_clock_cycles_1;
static UINT8   m_clock_cycles_2;
static UINT8   m_clock_cycles_4;
static UINT8   m_clock_cycles_6;

static UINT64  m_tr_base_cycles;
static UINT32  m_tr_base_value;
static UINT32  m_tr_clocks_per_tick;
static UINT8   m_timer_int_pending;

static INT32 timer_time;
static INT32 timer_param;
static INT32 m_hold_irq;

static delay_info m_delay;

static INT32 m_instruction_length;
static INT32 m_intblock;

// other internal state
static int		m_icount;
static UINT64	itotal_cycles; // internal total cycles (timers etc)
static UINT64	utotal_cycles; // user-total cycles (E132XSTotalCycles() / E132XSNewFrame() etc..)
static INT32	n_cycles;
static INT32    sleep_until_int;

struct regs_decode
{
	UINT8   src, dst;       // destination and source register code
	UINT32  src_value;      // current source register value
	UINT32  next_src_value; // current next source register value
	UINT32  dst_value;      // current destination register value
	UINT32  next_dst_value; // current next destination register value
	UINT8   sub_type;       // sub type opcode (for DD and X_CODE bits)
	union
	{
		UINT32 u;
		INT32  s;
	} extra;                // extra value such as immediate value, const, pcrel, ...
	UINT8   src_is_local;
	UINT8   dst_is_local;
	UINT8   same_src_dst;
	UINT8   same_src_dstf;
	UINT8   same_srcf_dst;
};

/* Return the entry point for a determinated trap */
static UINT32 get_trap_addr(UINT8 trapno)
{
	UINT32 addr;
	if( m_trap_entry == 0xffffff00 ) /* @ MEM3 */
	{
		addr = trapno * 4;
	}
	else
	{
		addr = (63 - trapno) * 4;
	}
	addr |= m_trap_entry;

	return addr;
}

/* Return the entry point for a determinated emulated code (the one for "extend" opcode is reserved) */
static UINT32 get_emu_code_addr(UINT8 num) /* num is OP */
{
	UINT32 addr;
	if( m_trap_entry == 0xffffff00 ) /* @ MEM3 */
	{
		addr = (m_trap_entry - 0x100) | ((num & 0xf) << 4);
	}
	else
	{
		addr = m_trap_entry | (0x10c | ((0xcf - num) << 4));
	}
	return addr;
}

static void hyperstone_set_trap_entry(int which)
{
	switch( which )
	{
		case E132XS_ENTRY_MEM0:
			m_trap_entry = 0x00000000;
			break;

		case E132XS_ENTRY_MEM1:
			m_trap_entry = 0x40000000;
			break;

		case E132XS_ENTRY_MEM2:
			m_trap_entry = 0x80000000;
			break;

		case E132XS_ENTRY_MEM3:
			m_trap_entry = 0xffffff00;
			break;

		case E132XS_ENTRY_IRAM:
			m_trap_entry = 0xc0000000;
			break;

		default:
			DEBUG_PRINTF(("Set entry point to a reserved value: %d\n", which));
			break;
	}
}

#define OP              m_op
#define PPC             m_ppc //previous pc
#define PC              m_global_regs[0] //Program Counter
#define SR              m_global_regs[1] //Status Register
#define FER             m_global_regs[2] //Floating-Point Exception Register
// 03 - 15  General Purpose Registers
// 16 - 17  Reserved
#define SP              m_global_regs[18] //Stack Pointer
#define UB              m_global_regs[19] //Upper Stack Bound
#define BCR             m_global_regs[20] //Bus Control Register
#define TPR             m_global_regs[21] //Timer Prescaler Register
#define TCR             m_global_regs[22] //Timer Compare Register
#define TR              compute_tr() //Timer Register
#define WCR             m_global_regs[24] //Watchdog Compare Register
#define ISR             m_global_regs[25] //Input Status Register
#define FCR             m_global_regs[26] //Function Control Register
#define MCR             m_global_regs[27] //Memory Control Register
// 28 - 31  Reserved

/* SR flags */
#define GET_C                   ( SR & 0x00000001)      // bit 0 //CARRY
#define GET_Z                   ((SR & 0x00000002)>>1)  // bit 1 //ZERO
#define GET_N                   ((SR & 0x00000004)>>2)  // bit 2 //NEGATIVE
#define GET_V                   ((SR & 0x00000008)>>3)  // bit 3 //OVERFLOW
#define GET_M                   ((SR & 0x00000010)>>4)  // bit 4 //CACHE-MODE
#define GET_H                   ((SR & 0x00000020)>>5)  // bit 5 //HIGHGLOBAL
// bit 6 RESERVED (always 0)
#define GET_I                   ((SR & 0x00000080)>>7)  // bit 7 //INTERRUPT-MODE
#define GET_FTE                 ((SR & 0x00001f00)>>8)  // bits 12 - 8  //Floating-Point Trap Enable
#define GET_FRM                 ((SR & 0x00006000)>>13) // bits 14 - 13 //Floating-Point Rounding Mode
#define GET_L                   ((SR & 0x00008000)>>15) // bit 15 //INTERRUPT-LOCK
#define GET_T                   ((SR & 0x00010000)>>16) // bit 16 //TRACE-MODE
#define GET_P                   ((SR & 0x00020000)>>17) // bit 17 //TRACE PENDING
#define GET_S                   ((SR & 0x00040000)>>18) // bit 18 //SUPERVISOR STATE
#define GET_ILC                 ((SR & 0x00180000)>>19) // bits 20 - 19 //INSTRUCTION-LENGTH
/* if FL is zero it is always interpreted as 16 */
#define GET_FL                  ((SR & 0x01e00000) ? ((SR & 0x01e00000)>>21) : 16) // bits 24 - 21 //FRAME LENGTH
#define GET_FP                  ((SR & 0xfe000000)>>25) // bits 31 - 25 //FRAME POINTER

#define SET_C(val)              (SR = (SR & ~0x00000001) | (val))
#define SET_Z(val)              (SR = (SR & ~0x00000002) | ((val) << 1))
#define SET_N(val)              (SR = (SR & ~0x00000004) | ((val) << 2))
#define SET_V(val)              (SR = (SR & ~0x00000008) | ((val) << 3))
#define SET_M(val)              (SR = (SR & ~0x00000010) | ((val) << 4))
#define SET_H(val)              (SR = (SR & ~0x00000020) | ((val) << 5))
#define SET_I(val)              (SR = (SR & ~0x00000080) | ((val) << 7))
#define SET_FTE(val)            (SR = (SR & ~0x00001f00) | ((val) << 8))
#define SET_FRM(val)            (SR = (SR & ~0x00006000) | ((val) << 13))
#define SET_L(val)              (SR = (SR & ~0x00008000) | ((val) << 15))
#define SET_T(val)              (SR = (SR & ~0x00010000) | ((val) << 16))
#define SET_P(val)              (SR = (SR & ~0x00020000) | ((val) << 17))
#define SET_S(val)              (SR = (SR & ~0x00040000) | ((val) << 18))
#define SET_ILC(val)            (SR = (SR & ~0x00180000) | ((val) << 19))
#define SET_FL(val)             (SR = (SR & ~0x01e00000) | ((val) << 21))
#define SET_FP(val)             (SR = (SR & ~0xfe000000) | ((val) << 25))

#define SET_PC(val)             PC = ((val) & 0xfffffffe) //PC(0) = 0
#define SET_SP(val)             SP = ((val) & 0xfffffffc) //SP(0) = SP(1) = 0
#define SET_UB(val)             UB = ((val) & 0xfffffffc) //UB(0) = UB(1) = 0

#define SET_LOW_SR(val)         (SR = (SR & 0xffff0000) | ((val) & 0x0000ffff)) // when SR is addressed, only low 16 bits can be changed


#define CHECK_C(x)              (SR = (SR & ~0x00000001) | (((x) & (((UINT64)1) << 32)) ? 1 : 0 ))
#define CHECK_VADD(x,y,z)       (SR = (SR & ~0x00000008) | ((((x) ^ (z)) & ((y) ^ (z)) & 0x80000000) ? 8: 0))
#define CHECK_VADD3(x,y,w,z)    (SR = (SR & ~0x00000008) | ((((x) ^ (z)) & ((y) ^ (z)) & ((w) ^ (z)) & 0x80000000) ? 8: 0))
#define CHECK_VSUB(x,y,z)       (SR = (SR & ~0x00000008) | ((((z) ^ (y)) & ((y) ^ (x)) & 0x80000000) ? 8: 0))


/* FER flags */
#define GET_ACCRUED             (FER & 0x0000001f) //bits  4 - 0 //Floating-Point Accrued Exceptions
#define GET_ACTUAL              (FER & 0x00001f00) //bits 12 - 8 //Floating-Point Actual  Exceptions
//other bits are reversed, in particular 7 - 5 for the operating system.
//the user program can only changes the above 2 flags


static UINT64 total_cycles() // for internal use only!! (timers, etc)
{
	return itotal_cycles;
}

static UINT32 compute_tr()
{
	UINT64 cycles_since_base = total_cycles() - m_tr_base_cycles;
	UINT64 clocks_since_base = cycles_since_base >> m_clock_scale;
	return m_tr_base_value + (clocks_since_base / m_tr_clocks_per_tick);
}

static void update_timer_prescale()
{
	UINT32 prevtr = compute_tr();
	TPR &= ~0x80000000;
	m_clock_scale = (TPR >> 26) & m_clock_scale_mask;
	m_clock_cycles_1 = 1 << m_clock_scale;
	m_clock_cycles_2 = 2 << m_clock_scale;
	m_clock_cycles_4 = 4 << m_clock_scale;
	m_clock_cycles_6 = 6 << m_clock_scale;
	m_tr_clocks_per_tick = ((TPR >> 16) & 0xff) + 2;
	m_tr_base_value = prevtr;
	m_tr_base_cycles = total_cycles();
}

static void timer_callback(INT32 param);

static void set_timer(INT32 cyc, INT32 param)
{
	timer_time = cyc;
	timer_param = param;
}

static void adjust_timer_interrupt()
{
	UINT64 cycles_since_base = total_cycles() - m_tr_base_cycles;
	UINT64 clocks_since_base = cycles_since_base >> m_clock_scale;
	UINT64 cycles_until_next_clock = cycles_since_base - (clocks_since_base << m_clock_scale);

	if (cycles_until_next_clock == 0)
		cycles_until_next_clock = (UINT64)(1 << m_clock_scale);

	/* special case: if we have a change pending, set a timer to fire then */
	if (TPR & 0x80000000)
	{
		UINT64 clocks_until_int = m_tr_clocks_per_tick - (clocks_since_base % m_tr_clocks_per_tick);
		UINT64 cycles_until_int = (clocks_until_int << m_clock_scale) + cycles_until_next_clock;
		set_timer(cycles_until_int + 1, 1);
//		m_timer->adjust(cycles_to_attotime(cycles_until_int + 1), 1); // iq_132
	}

	/* else if the timer interrupt is enabled, configure it to fire at the appropriate time */
	else if (!(FCR & 0x00800000))
	{
		UINT32 curtr = m_tr_base_value + (clocks_since_base / m_tr_clocks_per_tick);
		UINT32 delta = TCR - curtr;
		if (delta > 0x80000000)
		{
			if (!m_timer_int_pending) {
				set_timer(1, 0);
				//timer_callback(0);
			}
		}
		else
		{
			UINT64 clocks_until_int = mulu_32x32(delta, m_tr_clocks_per_tick);
			UINT64 cycles_until_int = (clocks_until_int << m_clock_scale) + cycles_until_next_clock;
			set_timer(cycles_until_int, 0);
		//	m_timer->adjust(cycles_to_attotime(cycles_until_int)); // iq_132
		}
	}

	/* otherwise, disable the timer */
	else
	{
		set_timer(-1, 0);
	}
}

static void timer_callback(INT32 param)
{
	int update = param;
	//bprintf(0, _T("--->timer(%d) @ %d\n"), param, itotal_cycles);
	/* update the values if necessary */
	if (update)
		update_timer_prescale();

	/* see if the timer is right for firing */
	if (!((compute_tr() - TCR) & 0x80000000))
		m_timer_int_pending = 1;

	/* adjust ourselves for the next time */
	else
		adjust_timer_interrupt();
}




static UINT32 get_global_register(UINT8 code)
{
/*
    if( code >= 16 )
    {
        switch( code )
        {
        case 16:
        case 17:
        case 28:
        case 29:
        case 30:
        case 31:
            DEBUG_PRINTF(("read _Reserved_ Global Register %d @ %08X\n",code,PC));
            break;

        case BCR_REGISTER:
            DEBUG_PRINTF(("read write-only BCR register @ %08X\n",PC));
            return 0;

        case TPR_REGISTER:
            DEBUG_PRINTF(("read write-only TPR register @ %08X\n",PC));
            return 0;

        case FCR_REGISTER:
            DEBUG_PRINTF(("read write-only FCR register @ %08X\n",PC));
            return 0;

        case MCR_REGISTER:
            DEBUG_PRINTF(("read write-only MCR register @ %08X\n",PC));
            return 0;
        }
    }
*/
	if (code == TR_REGISTER)
	{
		/* it is common to poll this in a loop */
		if (m_icount > m_tr_clocks_per_tick / 2)
			m_icount -= m_tr_clocks_per_tick / 2;
		return compute_tr();
	}
	return m_global_regs[code];
}

static void set_local_register(UINT8 code, UINT32 val)
{
	UINT8 new_code = (code + GET_FP) % 64;

	m_local_regs[new_code] = val;
}

static void set_global_register(UINT8 code, UINT32 val)
{
	//TODO: add correct FER set instruction

	if( code == PC_REGISTER )
	{
		SET_PC(val);
	}
	else if( (INT32)code == SR_REGISTER )
	{
		SET_LOW_SR(val); // only a RET instruction can change the full content of SR
		SR &= ~0x40; //reserved bit 6 always zero
		if (m_intblock < 1)
			m_intblock = 1;
	}
	else
	{
		UINT32 oldval = m_global_regs[code];
		if( code != ISR_REGISTER )
			m_global_regs[code] = val;
		else
			DEBUG_PRINTF(("Written to ISR register. PC = %08X\n", PC));

		//are these set only when privilege bit is set?
		if( code >= 16 )
		{
			switch( code )
			{
			case 18:
				SET_SP(val);
				break;

			case 19:
				SET_UB(val);
				break;
/*
            case ISR_REGISTER:
                DEBUG_PRINTF(("written %08X to read-only ISR register\n",val));
                break;

            case TCR_REGISTER:
//              DEBUG_PRINTF(("written %08X to TCR register\n",val));
                break;

            case 23:
//              DEBUG_PRINTF(("written %08X to TR register\n",val));
                break;

            case 24:
//              DEBUG_PRINTF(("written %08X to WCR register\n",val));
                break;

            case 16:
            case 17:
            case 28:
            case 29:
            case 30:
            case 31:
                DEBUG_PRINTF(("written %08X to _Reserved_ Global Register %d\n",val,code));
                break;

            case BCR_REGISTER:
                break;
*/
			case TR_REGISTER:
				m_tr_base_value = val;
				m_tr_base_cycles = total_cycles();
				adjust_timer_interrupt();
				break;

			case TPR_REGISTER:
				if (!(val & 0x80000000)) /* change immediately */
					update_timer_prescale();
				adjust_timer_interrupt();
				break;

			case TCR_REGISTER:
				if (oldval != val)
				{
					adjust_timer_interrupt();
					if (m_intblock < 1)
						m_intblock = 1;
				}
				break;

			case FCR_REGISTER:
				if ((oldval ^ val) & 0x00800000)
					adjust_timer_interrupt();
				if (m_intblock < 1)
					m_intblock = 1;
				break;

			case MCR_REGISTER:
				// bits 14..12 EntryTableMap
				hyperstone_set_trap_entry((val & 0x7000) >> 12);
				break;
			}
		}
	}
}

#define GET_ABS_L_REG(code)         m_local_regs[code]
#define SET_L_REG(code, val)        set_local_register(code, val)
#define SET_ABS_L_REG(code, val)    m_local_regs[code] = val
#define GET_G_REG(code)             get_global_register(code)
#define SET_G_REG(code, val)        set_global_register(code, val)

#define S_BIT                   ((OP & 0x100) >> 8)
#define N_BIT                   S_BIT
#define D_BIT                   ((OP & 0x200) >> 9)
#define N_VALUE                 ((N_BIT << 4) | (OP & 0x0f))
#define DST_CODE                ((OP & 0xf0) >> 4)
#define SRC_CODE                (OP & 0x0f)
#define SIGN_BIT(val)           ((val & 0x80000000) >> 31)

#define LOCAL  1

static const INT32 immediate_values[32] =
{
	0, 1, 2, 3, 4, 5, 6, 7,
	8, 9, 10, 11, 12, 13, 14, 15,
	16, 0, 0, 0, 32, 64, 128, (INT32)0x80000000,
	-8, -7, -6, -5, -4, -3, -2, -1
};

#define WRITE_ONLY_REGMASK  ((1 << BCR_REGISTER) | (1 << TPR_REGISTER) | (1 << FCR_REGISTER) | (1 << MCR_REGISTER))

#define decode_source(decode, local, hflag)                                         \
do                                                                                  \
{                                                                                   \
	if(local)                                                                       \
	{                                                                               \
		UINT8 code = (decode)->src;                                                 \
		(decode)->src_is_local = 1;                                                 \
		code = ((decode)->src + GET_FP) % 64; /* registers offset by frame pointer  */\
		SREG = m_local_regs[code];                                                  \
		code = ((decode)->src + 1 + GET_FP) % 64;                                   \
		SREGF = m_local_regs[code];                                                 \
	}                                                                               \
	else                                                                            \
	{                                                                               \
		(decode)->src_is_local = 0;                                                 \
																					\
		if (!hflag)                                                                 \
		{                                                                           \
			SREG = get_global_register((decode)->src);                              \
																					\
			/* bound safe */                                                        \
			if ((decode)->src != 15)                                                \
				SREGF = get_global_register((decode)->src + 1);                     \
		}                                                                           \
		else                                                                        \
		{                                                                           \
			(decode)->src += 16;                                                    \
																					\
			SREG = get_global_register((decode)->src);                              \
			if ((WRITE_ONLY_REGMASK >> (decode)->src) & 1)                          \
				SREG = 0; /* write-only registers */                                \
			else if ((decode)->src == ISR_REGISTER)                                 \
				DEBUG_PRINTF(("read src ISR. PC = %08X\n",PPC));                    \
																					\
			/* bound safe */                                                        \
			if ((decode)->src != 31)                                                \
				SREGF = get_global_register((decode)->src + 1);                     \
		}                                                                           \
	}                                                                               \
} while (0)

#define decode_dest(decode, local, hflag)                                           \
do                                                                                  \
{                                                                                   \
	if(local)                                                                       \
	{                                                                               \
		UINT8 code = (decode)->dst;                                                 \
		(decode)->dst_is_local = 1;                                                 \
		code = ((decode)->dst + GET_FP) % 64; /* registers offset by frame pointer */\
		DREG = m_local_regs[code];                                                  \
		code = ((decode)->dst + 1 + GET_FP) % 64;                                   \
		DREGF = m_local_regs[code];                                                 \
	}                                                                               \
	else                                                                            \
	{                                                                               \
		(decode)->dst_is_local = 0;                                                 \
																					\
		if (!hflag)                                                                 \
		{                                                                           \
			DREG = get_global_register((decode)->dst);                              \
																					\
			/* bound safe */                                                        \
			if ((decode)->dst != 15)                                                \
				DREGF = get_global_register((decode)->dst + 1);                     \
		}                                                                           \
		else                                                                        \
		{                                                                           \
			(decode)->dst += 16;                                                    \
																					\
			DREG = get_global_register((decode)->dst);                              \
			if( (decode)->dst == ISR_REGISTER )                                     \
				DEBUG_PRINTF(("read dst ISR. PC = %08X\n",PPC));                    \
																					\
			/* bound safe */                                                        \
			if ((decode)->dst != 31)                                                \
				DREGF = get_global_register((decode)->dst + 1);                     \
		}                                                                           \
	}                                                                               \
} while (0)

#define decode_RR(decode, dlocal, slocal)                                           \
do                                                                                  \
{                                                                                   \
	(decode)->src = SRC_CODE;                                                       \
	(decode)->dst = DST_CODE;                                                       \
	decode_source(decode, slocal, 0);                                               \
	decode_dest(decode, dlocal, 0);                                                 \
																					\
	if( (slocal) == (dlocal) && SRC_CODE == DST_CODE )                              \
		SAME_SRC_DST = 1;                                                           \
																					\
	if( (slocal) == LOCAL && (dlocal) == LOCAL )                                    \
	{                                                                               \
		if( SRC_CODE == ((DST_CODE + 1) % 64) )                                     \
			SAME_SRC_DSTF = 1;                                                      \
																					\
		if( ((SRC_CODE + 1) % 64) == DST_CODE )                                     \
			SAME_SRCF_DST = 1;                                                      \
	}                                                                               \
	else if( (slocal) == 0 && (dlocal) == 0 )                                       \
	{                                                                               \
		if( SRC_CODE == (DST_CODE + 1) )                                            \
			SAME_SRC_DSTF = 1;                                                      \
																					\
		if( (SRC_CODE + 1) == DST_CODE )                                            \
			SAME_SRCF_DST = 1;                                                      \
	}                                                                               \
} while (0)

#define decode_LL(decode)                                                           \
do                                                                                  \
{                                                                                   \
	(decode)->src = SRC_CODE;                                                       \
	(decode)->dst = DST_CODE;                                                       \
	decode_source(decode, LOCAL, 0);                                                \
	decode_dest(decode, LOCAL, 0);                                                  \
																					\
	if( SRC_CODE == DST_CODE )                                                      \
		SAME_SRC_DST = 1;                                                           \
																					\
	if( SRC_CODE == ((DST_CODE + 1) % 64) )                                         \
		SAME_SRC_DSTF = 1;                                                          \
} while (0)

#define decode_LR(decode, slocal)                                                   \
do                                                                                  \
{                                                                                   \
	(decode)->src = SRC_CODE;                                                       \
	(decode)->dst = DST_CODE;                                                       \
	decode_source(decode, slocal, 0);                                               \
	decode_dest(decode, LOCAL, 0);                                                  \
																					\
	if( ((SRC_CODE + 1) % 64) == DST_CODE && slocal == LOCAL )                      \
		SAME_SRCF_DST = 1;                                                          \
} while (0)

#define check_delay_PC()                                                            \
do                                                                                  \
{                                                                                   \
	/* if PC is used in a delay instruction, the delayed PC should be used */       \
	if( m_delay.delay_cmd == DELAY_EXECUTE )                                        \
	{                                                                               \
		PC = m_delay.delay_pc;                                                      \
		m_delay.delay_cmd = NO_DELAY;                                               \
	}                                                                               \
} while (0)

#define decode_immediate(decode, nbit)                                              \
do                                                                                  \
{                                                                                   \
	if (!nbit)                                                                      \
		EXTRA_U = immediate_values[OP & 0x0f];                                      \
	else                                                                            \
		switch( OP & 0x0f )                                                         \
		{                                                                           \
			default:                                                                \
				EXTRA_U = immediate_values[0x10 + (OP & 0x0f)];                     \
				break;                                                              \
																					\
			case 1:                                                                 \
				m_instruction_length = 3;                                           \
				EXTRA_U = (READ_OP(PC) << 16) | READ_OP(PC + 2);                    \
				PC += 4;                                                            \
				break;                                                              \
																					\
			case 2:                                                                 \
				m_instruction_length = 2;                                           \
				EXTRA_U = READ_OP(PC);                                              \
				PC += 2;                                                            \
				break;                                                              \
																					\
			case 3:                                                                 \
				m_instruction_length = 2;                                           \
				EXTRA_U = 0xffff0000 | READ_OP(PC);                                 \
				PC += 2;                                                            \
				break;                                                              \
		}                                                                           \
} while (0)

#define decode_const(decode)                                                        \
do                                                                                  \
{                                                                                   \
	UINT16 imm_1 = READ_OP(PC);                                                     \
																					\
	PC += 2;                                                                        \
	m_instruction_length = 2;                                                       \
																					\
	if( E_BIT(imm_1) )                                                              \
	{                                                                               \
		UINT16 imm_2 = READ_OP(PC);                                                 \
																					\
		PC += 2;                                                                    \
		m_instruction_length = 3;                                                   \
																					\
		EXTRA_S = imm_2;                                                            \
		EXTRA_S |= ((imm_1 & 0x3fff) << 16);                                        \
																					\
		if( S_BIT_CONST(imm_1) )                                                    \
		{                                                                           \
			EXTRA_S |= 0xc0000000;                                                  \
		}                                                                           \
	}                                                                               \
	else                                                                            \
	{                                                                               \
		EXTRA_S = imm_1 & 0x3fff;                                                   \
																					\
		if( S_BIT_CONST(imm_1) )                                                    \
		{                                                                           \
			EXTRA_S |= 0xffffc000;                                                  \
		}                                                                           \
	}                                                                               \
} while (0)

#define decode_pcrel(decode)                                                        \
do                                                                                  \
{                                                                                   \
	if( OP & 0x80 )                                                                 \
	{                                                                               \
		UINT16 next = READ_OP(PC);                                                  \
																					\
		PC += 2;                                                                    \
		m_instruction_length = 2;                                                   \
																					\
		EXTRA_S = (OP & 0x7f) << 16;                                                \
		EXTRA_S |= (next & 0xfffe);                                                 \
																					\
		if( next & 1 )                                                              \
			EXTRA_S |= 0xff800000;                                                  \
	}                                                                               \
	else                                                                            \
	{                                                                               \
		EXTRA_S = OP & 0x7e;                                                        \
																					\
		if( OP & 1 )                                                                \
			EXTRA_S |= 0xffffff80;                                                  \
	}                                                                               \
} while (0)

#define decode_dis(decode)                                                          \
do                                                                                  \
{                                                                                   \
	UINT16 next_1 = READ_OP(PC);                                                    \
																					\
	PC += 2;                                                                        \
	m_instruction_length = 2;                                                       \
																					\
	(decode)->sub_type = DD(next_1);                                                \
																					\
	if( E_BIT(next_1) )                                                             \
	{                                                                               \
		UINT16 next_2 = READ_OP(PC);                                                \
																					\
		PC += 2;                                                                    \
		m_instruction_length = 3;                                                   \
																					\
		EXTRA_S = next_2;                                                           \
		EXTRA_S |= ((next_1 & 0xfff) << 16);                                        \
																					\
		if( S_BIT_CONST(next_1) )                                                   \
		{                                                                           \
			EXTRA_S |= 0xf0000000;                                                  \
		}                                                                           \
	}                                                                               \
	else                                                                            \
	{                                                                               \
		EXTRA_S = next_1 & 0xfff;                                                   \
																					\
		if( S_BIT_CONST(next_1) )                                                   \
		{                                                                           \
			EXTRA_S |= 0xfffff000;                                                  \
		}                                                                           \
	}                                                                               \
} while (0)

#define decode_lim(decode)                                                          \
do                                                                                  \
{                                                                                   \
	UINT32 next = READ_OP(PC);                                                      \
	PC += 2;                                                                        \
	m_instruction_length = 2;                                                       \
																					\
	(decode)->sub_type = X_CODE(next);                                              \
																					\
	if( E_BIT(next) )                                                               \
	{                                                                               \
		EXTRA_U = ((next & 0xfff) << 16) | READ_OP(PC);                             \
		PC += 2;                                                                    \
		m_instruction_length = 3;                                                   \
	}                                                                               \
	else                                                                            \
	{                                                                               \
		EXTRA_U = next & 0xfff;                                                     \
	}                                                                               \
} while (0)

#define RRdecode(decode, dlocal, slocal)                                            \
do                                                                                  \
{                                                                                   \
	check_delay_PC();                                                               \
	decode_RR(decode, dlocal, slocal);                                              \
} while (0)

#define RRlimdecode(decode, dlocal, slocal)                                         \
do                                                                                  \
{                                                                                   \
	decode_lim(decode);                                                             \
	check_delay_PC();                                                               \
	decode_RR(decode, dlocal, slocal);                                              \
} while (0)

#define RRconstdecode(decode, dlocal, slocal)                                       \
do                                                                                  \
{                                                                                   \
	decode_const(decode);                                                           \
	check_delay_PC();                                                               \
	decode_RR(decode, dlocal, slocal);                                              \
} while (0)

#define RRdisdecode(decode, dlocal, slocal)                                         \
do                                                                                  \
{                                                                                   \
	decode_dis(decode);                                                             \
	check_delay_PC();                                                               \
	decode_RR(decode, dlocal, slocal);                                              \
} while (0)

#define RRdecodewithHflag(decode, dlocal, slocal)                                   \
do                                                                                  \
{                                                                                   \
	check_delay_PC();                                                               \
	(decode)->src = SRC_CODE;                                                       \
	(decode)->dst = DST_CODE;                                                       \
	decode_source(decode, slocal, GET_H);                                           \
	decode_dest(decode, dlocal, GET_H);                                             \
																					\
	if(GET_H)                                                                       \
		if(slocal == 0 && dlocal == 0)                                              \
			DEBUG_PRINTF(("MOV with hflag and 2 GRegs! PC = %08X\n",PPC));          \
} while (0)

#define Rimmdecode(decode, dlocal, nbit)                                            \
do                                                                                  \
{                                                                                   \
	decode_immediate(decode, nbit);                                                 \
	check_delay_PC();                                                               \
	(decode)->dst = DST_CODE;                                                       \
	decode_dest(decode, dlocal, 0);                                                 \
} while (0)

#define Rndecode(decode, dlocal)                                                    \
do                                                                                  \
{                                                                                   \
	check_delay_PC();                                                               \
	(decode)->dst = DST_CODE;                                                       \
	decode_dest(decode, dlocal, 0);                                                 \
} while (0)

#define RimmdecodewithHflag(decode, dlocal, nbit)                                   \
do                                                                                  \
{                                                                                   \
	decode_immediate(decode, nbit);                                                 \
	check_delay_PC();                                                               \
	(decode)->dst = DST_CODE;                                                       \
	decode_dest(decode, dlocal, GET_H);                                             \
} while (0)

#define Lndecode(decode)                                                            \
do                                                                                  \
{                                                                                   \
	check_delay_PC();                                                               \
	(decode)->dst = DST_CODE;                                                       \
	decode_dest(decode, LOCAL, 0);                                                  \
} while (0)

#define LLdecode(decode)                                                            \
do                                                                                  \
{                                                                                   \
	check_delay_PC();                                                               \
	decode_LL(decode);                                                              \
} while (0)

#define LLextdecode(decode)                                                         \
do                                                                                  \
{                                                                                   \
	m_instruction_length = 2;                                                       \
	EXTRA_U = READ_OP(PC);                                                          \
	PC += 2;                                                                        \
	check_delay_PC();                                                               \
	decode_LL(decode);                                                              \
} while (0)

#define LRdecode(decode, slocal)                                                    \
do                                                                                  \
{                                                                                   \
	check_delay_PC();                                                               \
	decode_LR(decode, slocal);                                                      \
} while (0)

#define LRconstdecode(decode, slocal)                                               \
do                                                                                  \
{                                                                                   \
	decode_const(decode);                                                           \
	check_delay_PC();                                                               \
	decode_LR(decode, slocal);                                                      \
} while (0)

#define PCreldecode(decode)                                                         \
do                                                                                  \
{                                                                                   \
	decode_pcrel(decode);                                                           \
	check_delay_PC();                                                               \
} while (0)

#define PCadrdecode(decode)                                                         \
do                                                                                  \
{                                                                                   \
	check_delay_PC();                                                               \
} while (0)

#define no_decode(decode)                                                           \
do                                                                                  \
{                                                                                   \
} while (0)


static void execute_br(struct regs_decode *decode)
{
	PPC = PC;
	PC += EXTRA_S;
	SET_M(0);

	m_icount -= m_clock_cycles_2;
}

static void execute_dbr(struct regs_decode *decode)
{
	m_delay.delay_cmd = DELAY_EXECUTE;
	m_delay.delay_pc  = PC + EXTRA_S;

	m_intblock = 3;
}


static void execute_trap(UINT32 addr)
{
	UINT8 reg;
	UINT32 oldSR;
	reg = GET_FP + GET_FL;

	SET_ILC(m_instruction_length & 3);

	oldSR = SR;

	SET_FL(6);
	SET_FP(reg);

	SET_L_REG(0, (PC & 0xfffffffe) | GET_S);
	SET_L_REG(1, oldSR);

	SET_M(0);
	SET_T(0);
	SET_L(1);
	SET_S(1);

	PPC = PC;
	PC = addr;

	m_icount -= m_clock_cycles_2;
}


static void execute_int(UINT32 addr)
{
	UINT8 reg;
	UINT32 oldSR;
	reg = GET_FP + GET_FL;

	SET_ILC(m_instruction_length & 3);

	oldSR = SR;

	SET_FL(2);
	SET_FP(reg);

	SET_L_REG(0, (PC & 0xfffffffe) | GET_S);
	SET_L_REG(1, oldSR);

	SET_M(0);
	SET_T(0);
	SET_L(1);
	SET_S(1);
	SET_I(1);

	PPC = PC;
	PC = addr;

	m_icount -= m_clock_cycles_2;
}

/* TODO: mask Parity Error and Extended Overflow exceptions */
static void execute_exception(UINT32 addr)
{
	UINT8 reg;
	UINT32 oldSR;
	reg = GET_FP + GET_FL;

	SET_ILC(m_instruction_length & 3);

	oldSR = SR;

	SET_FP(reg);
	SET_FL(2);

	SET_L_REG(0, (PC & 0xfffffffe) | GET_S);
	SET_L_REG(1, oldSR);

	SET_M(0);
	SET_T(0);
	SET_L(1);
	SET_S(1);

	PPC = PC;
	PC = addr;

	DEBUG_PRINTF(("EXCEPTION! PPC = %08X PC = %08X\n",PPC-2,PC-2));
	m_icount -= m_clock_cycles_2;
}

static void execute_software(struct regs_decode *decode)
{
	UINT8 reg;
	UINT32 oldSR;
	UINT32 addr;
	UINT32 stack_of_dst;

	SET_ILC(1);

	addr = get_emu_code_addr((OP & 0xff00) >> 8);
	reg = GET_FP + GET_FL;

	//since it's sure the register is in the register part of the stack,
	//set the stack address to a value above the highest address
	//that can be set by a following frame instruction
	stack_of_dst = (SP & ~0xff) + 64*4 + (((GET_FP + decode->dst) % 64) * 4); //converted to 32bits offset

	oldSR = SR;

	SET_FL(6);
	SET_FP(reg);

	SET_L_REG(0, stack_of_dst);
	SET_L_REG(1, SREG);
	SET_L_REG(2, SREGF);
	SET_L_REG(3, (PC & 0xfffffffe) | GET_S);
	SET_L_REG(4, oldSR);

	SET_M(0);
	SET_T(0);
	SET_L(1);

	PPC = PC;
	PC = addr;
}


/*
    IRQ lines :
        0 - IO2     (trap 48)
        1 - IO1     (trap 49)
        2 - INT4    (trap 50)
        3 - INT3    (trap 51)
        4 - INT2    (trap 52)
        5 - INT1    (trap 53)
        6 - IO3     (trap 54)
        7 - TIMER   (trap 55)
*/

#define INT1_LINE_STATE     ((ISR >> 0) & 1)
#define INT2_LINE_STATE     ((ISR >> 1) & 1)
#define INT3_LINE_STATE     ((ISR >> 2) & 1)
#define INT4_LINE_STATE     ((ISR >> 3) & 1)
#define IO1_LINE_STATE      ((ISR >> 4) & 1)
#define IO2_LINE_STATE      ((ISR >> 5) & 1)
#define IO3_LINE_STATE      ((ISR >> 6) & 1)

static void execute_set_input(int inputnum, int state)
{
	if (state) {
		m_hold_irq = (state & 2) ? (0x1000 | inputnum) : 0;
		ISR |= 1 << inputnum;
	} else {
		ISR &= ~(1 << inputnum);
	}
}

static void standard_irq_callback(INT32 line)
{
	if (m_hold_irq && (m_hold_irq & 0xff) == line) {
		m_hold_irq = 0;
		execute_set_input(line, 0);
	}
}

INT32 E132XSInterruptActive()
{
	if ((FCR & (1 << 29)) == 0) return 1;
	return 0;
}

UINT32 E132XSGetPC(INT32)
{
	return PC;
}

static void check_interrupts()
{
	/* Interrupt-Lock flag isn't set */
	if (GET_L || m_intblock > 0)
		return;

	/* quick exit if nothing */
	if (!m_timer_int_pending && (ISR & 0x7f) == 0)
		return;

	/* IO3 is priority 5; state is in bit 6 of ISR; FCR bit 10 enables input and FCR bit 8 inhibits interrupt */
	if (IO3_LINE_STATE && (FCR & 0x00000500) == 0x00000400)
	{
		execute_int(get_trap_addr(TRAPNO_IO3));
		standard_irq_callback(IRQ_IO3);
		return;
	}

	/* timer int might be priority 6 if FCR bits 20-21 == 3; FCR bit 23 inhibits interrupt */
	if (m_timer_int_pending && (FCR & 0x00b00000) == 0x00300000)
	{
		m_timer_int_pending = 0;
		execute_int(get_trap_addr(TRAPNO_TIMER));
		return;
	}

	/* INT1 is priority 7; state is in bit 0 of ISR; FCR bit 28 inhibits interrupt */
	if (INT1_LINE_STATE && (FCR & 0x10000000) == 0x00000000)
	{
		execute_int(get_trap_addr(TRAPNO_INT1));
		standard_irq_callback(IRQ_INT1);
		return;
	}

	/* timer int might be priority 8 if FCR bits 20-21 == 2; FCR bit 23 inhibits interrupt */
	if (m_timer_int_pending && (FCR & 0x00b00000) == 0x00200000)
	{
		m_timer_int_pending = 0;
		execute_int(get_trap_addr(TRAPNO_TIMER));
		return;
	}

	/* INT2 is priority 9; state is in bit 1 of ISR; FCR bit 29 inhibits interrupt */
	if (INT2_LINE_STATE && (FCR & 0x20000000) == 0x00000000)
	{
		execute_int(get_trap_addr(TRAPNO_INT2));
		standard_irq_callback(IRQ_INT2);
		return;
	}

	/* timer int might be priority 10 if FCR bits 20-21 == 1; FCR bit 23 inhibits interrupt */
	if (m_timer_int_pending && (FCR & 0x00b00000) == 0x00100000)
	{
		m_timer_int_pending = 0;
		execute_int(get_trap_addr(TRAPNO_TIMER));
		return;
	}

	/* INT3 is priority 11; state is in bit 2 of ISR; FCR bit 30 inhibits interrupt */
	if (INT3_LINE_STATE && (FCR & 0x40000000) == 0x00000000)
	{
		execute_int(get_trap_addr(TRAPNO_INT3));
		standard_irq_callback(IRQ_INT3);
		return;
	}

	/* timer int might be priority 12 if FCR bits 20-21 == 0; FCR bit 23 inhibits interrupt */
	if (m_timer_int_pending && (FCR & 0x00b00000) == 0x00000000)
	{
		m_timer_int_pending = 0;
		execute_int(get_trap_addr(TRAPNO_TIMER));
		return;
	}

	/* INT4 is priority 13; state is in bit 3 of ISR; FCR bit 31 inhibits interrupt */
	if (INT4_LINE_STATE && (FCR & 0x80000000) == 0x00000000)
	{
		execute_int(get_trap_addr(TRAPNO_INT4));
		standard_irq_callback(IRQ_INT4);
		return;
	}

	/* IO1 is priority 14; state is in bit 4 of ISR; FCR bit 2 enables input and FCR bit 0 inhibits interrupt */
	if (IO1_LINE_STATE && (FCR & 0x00000005) == 0x00000004)
	{
		execute_int(get_trap_addr(TRAPNO_IO1));
		standard_irq_callback(IRQ_IO1);
		return;
	}

	/* IO2 is priority 15; state is in bit 5 of ISR; FCR bit 6 enables input and FCR bit 4 inhibits interrupt */
	if (IO2_LINE_STATE && (FCR & 0x00000050) == 0x00000040)
	{
		execute_int(get_trap_addr(TRAPNO_IO2));
		standard_irq_callback(IRQ_IO2);
		return;
	}
}


//-------------------------------------------------
//  memory_space_config - return the configuration
//  of the specified address space, or NULL if
//  the space doesn't exist
//-------------------------------------------------

static void hyperstone_chk(struct regs_decode *decode)
{
	UINT32 addr = get_trap_addr(TRAPNO_RANGE_ERROR);

	if( SRC_IS_SR )
	{
		if( DREG == 0 )
			execute_exception(addr);
	}
	else
	{
		if( SRC_IS_PC )
		{
			if( DREG >= SREG )
				execute_exception(addr);
		}
		else
		{
			if( DREG > SREG )
				execute_exception(addr);
		}
	}

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_movd(struct regs_decode *decode)
{
	if( DST_IS_PC ) // Rd denotes PC
	{
		// RET instruction

		UINT8 old_s, old_l;
		INT8 difference; // really it's 7 bits

		if( SRC_IS_PC || SRC_IS_SR )
		{
			DEBUG_PRINTF(("Denoted PC or SR in RET instruction. PC = %08X\n", PC));
		}
		else
		{
			old_s = GET_S;
			old_l = GET_L;
			PPC = PC;

			SET_PC(SREG);
			SR = (SREGF & 0xffe00000) | ((SREG & 0x01) << 18 ) | (SREGF & 0x3ffff);
			if (m_intblock < 1)
				m_intblock = 1;

			m_instruction_length = 0; // undefined

			if( (!old_s && GET_S) || (!GET_S && !old_l && GET_L))
			{
				UINT32 addr = get_trap_addr(TRAPNO_PRIVILEGE_ERROR);
				execute_exception(addr);
			}

			difference = GET_FP - ((SP & 0x1fc) >> 2);

			/* convert to 8 bits */
			if(difference > 63)
				difference = (INT8)(difference|0x80);
			else if( difference < -64 )
				difference = difference & 0x7f;

			if( difference < 0 ) //else it's finished
			{
				do
				{
					SP -= 4;
					SET_ABS_L_REG(((SP & 0xfc) >> 2), READ_W(SP));
					difference++;

				} while(difference != 0);
			}
		}

		//TODO: no 1!
		m_icount -= m_clock_cycles_1;
	}
	else if( SRC_IS_SR ) // Rd doesn't denote PC and Rs denotes SR
	{
		SET_DREG(0);
		SET_DREGF(0);
		SET_Z(1);
		SET_N(0);

		m_icount -= m_clock_cycles_2;
	}
	else // Rd doesn't denote PC and Rs doesn't denote SR
	{
		UINT64 tmp;

		SET_DREG(SREG);
		SET_DREGF(SREGF);

		tmp = CONCAT_64(SREG, SREGF);
		SET_Z( tmp == 0 ? 1 : 0 );
		SET_N( SIGN_BIT(SREG) );

		m_icount -= m_clock_cycles_2;
	}
}

static void hyperstone_divu(struct regs_decode *decode)
{
	if( SAME_SRC_DST || SAME_SRC_DSTF )
	{
		DEBUG_PRINTF(("Denoted the same register code in hyperstone_divu instruction. PC = %08X\n", PC));
	}
	else
	{
		if( SRC_IS_PC || SRC_IS_SR )
		{
			DEBUG_PRINTF(("Denoted PC or SR as source register in hyperstone_divu instruction. PC = %08X\n", PC));
		}
		else
		{
			UINT64 dividend;

			dividend = CONCAT_64(DREG, DREGF);

			if( SREG == 0 )
			{
				//Rd//Rdf -> undefined
				//Z -> undefined
				//N -> undefined
				UINT32 addr;
				SET_V(1);
				addr = get_trap_addr(TRAPNO_RANGE_ERROR);
				execute_exception(addr);
			}
			else
			{
				UINT32 quotient, remainder;

				/* TODO: add quotient overflow */
				quotient = dividend / SREG;
				remainder = dividend % SREG;

				SET_DREG(remainder);
				SET_DREGF(quotient);

				SET_Z( quotient == 0 ? 1 : 0 );
				SET_N( SIGN_BIT(quotient) );
				SET_V(0);
			}
		}
	}

	m_icount -= 36 << m_clock_scale;
}

static void hyperstone_divs(struct regs_decode *decode)
{
	if( SAME_SRC_DST || SAME_SRC_DSTF )
	{
		DEBUG_PRINTF(("Denoted the same register code in hyperstone_divs instruction. PC = %08X\n", PC));
	}
	else
	{
		if( SRC_IS_PC || SRC_IS_SR )
		{
			DEBUG_PRINTF(("Denoted PC or SR as source register in hyperstone_divs instruction. PC = %08X\n", PC));
		}
		else
		{
			INT64 dividend;

			dividend = (INT64) CONCAT_64(DREG, DREGF);

			if( SREG == 0 || (DREG & 0x80000000) )
			{
				//Rd//Rdf -> undefined
				//Z -> undefined
				//N -> undefined
				UINT32 addr;
				SET_V(1);
				addr = get_trap_addr(TRAPNO_RANGE_ERROR);
				execute_exception(addr);
			}
			else
			{
				INT32 quotient, remainder;

				/* TODO: add quotient overflow */
				quotient = dividend / ((INT32)(SREG));
				remainder = dividend % ((INT32)(SREG));

				SET_DREG(remainder);
				SET_DREGF(quotient);

				SET_Z( quotient == 0 ? 1 : 0 );
				SET_N( SIGN_BIT(quotient) );
				SET_V(0);
			}
		}
	}

	m_icount -= 36 << m_clock_scale;
}

static void hyperstone_xm(struct regs_decode *decode)
{
	if( SRC_IS_SR || DST_IS_SR || DST_IS_PC )
	{
		DEBUG_PRINTF(("Denoted PC or SR in hyperstone_xm. PC = %08X\n", PC));
	}
	else
	{
		switch( decode->sub_type ) // x_code
		{
			case 0:
			case 1:
			case 2:
			case 3:
				if( !SRC_IS_PC && (SREG > EXTRA_U) )
				{
					UINT32 addr = get_trap_addr(TRAPNO_RANGE_ERROR);
					execute_exception(addr);
				}
				else if( SRC_IS_PC && (SREG >= EXTRA_U) )
				{
					UINT32 addr = get_trap_addr(TRAPNO_RANGE_ERROR);
					execute_exception(addr);
				}
				else
				{
					SREG <<= decode->sub_type;
				}

				break;

			case 4:
			case 5:
			case 6:
			case 7:
				decode->sub_type -= 4;
				SREG <<= decode->sub_type;

				break;
		}

		SET_DREG(SREG);
	}

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_mask(struct regs_decode *decode)
{
	DREG = SREG & EXTRA_U;

	SET_DREG(DREG);
	SET_Z( DREG == 0 ? 1 : 0 );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_sum(struct regs_decode *decode)
{
	UINT64 tmp;

	if( SRC_IS_SR )
		SREG = GET_C;

	tmp = (UINT64)(SREG) + (UINT64)(EXTRA_U);
	CHECK_C(tmp);
	CHECK_VADD(SREG,EXTRA_U,tmp);

	DREG = SREG + EXTRA_U;

	SET_DREG(DREG);

	if( DST_IS_PC )
		SET_M(0);

	SET_Z( DREG == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(DREG) );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_sums(struct regs_decode *decode)
{
	INT32 res;
	INT64 tmp;

	if( SRC_IS_SR )
		SREG = GET_C;

	tmp = (INT64)((INT32)(SREG)) + (INT64)(EXTRA_S);
	CHECK_VADD(SREG,EXTRA_S,tmp);

//#if SETCARRYS
//  CHECK_C(tmp);
//#endif

	res = (INT32)(SREG) + EXTRA_S;

	SET_DREG(res);

	SET_Z( res == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(res) );

	m_icount -= m_clock_cycles_1;

	if( GET_V && !SRC_IS_SR )
	{
		UINT32 addr = get_trap_addr(TRAPNO_RANGE_ERROR);
		execute_exception(addr);
	}
}

static void hyperstone_cmp(struct regs_decode *decode)
{
	UINT64 tmp;

	if( SRC_IS_SR )
		SREG = GET_C;

	if( DREG == SREG )
		SET_Z(1);
	else
		SET_Z(0);

	if( (INT32) DREG < (INT32) SREG )
		SET_N(1);
	else
		SET_N(0);

	tmp = (UINT64)(DREG) - (UINT64)(SREG);
	CHECK_VSUB(SREG,DREG,tmp);

	if( DREG < SREG )
		SET_C(1);
	else
		SET_C(0);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_mov(struct regs_decode *decode)
{
	if( !GET_S && decode->dst >= 16 )
	{
		UINT32 addr = get_trap_addr(TRAPNO_PRIVILEGE_ERROR);
		execute_exception(addr);
	}

	SET_DREG(SREG);

	if( DST_IS_PC )
		SET_M(0);

	SET_Z( SREG == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(SREG) );

	m_icount -= m_clock_cycles_1;
}


static void hyperstone_add(struct regs_decode *decode)
{
	UINT64 tmp;

	if( SRC_IS_SR )
		SREG = GET_C;

	tmp = (UINT64)(SREG) + (UINT64)(DREG);
	CHECK_C(tmp);
	CHECK_VADD(SREG,DREG,tmp);

	DREG = SREG + DREG;
	SET_DREG(DREG);

	if( DST_IS_PC )
		SET_M(0);

	SET_Z( DREG == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(DREG) );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_adds(struct regs_decode *decode)
{
	INT32 res;
	INT64 tmp;

	if( SRC_IS_SR )
		SREG = GET_C;

	tmp = (INT64)((INT32)(SREG)) + (INT64)((INT32)(DREG));

	CHECK_VADD(SREG,DREG,tmp);

//#if SETCARRYS
//  CHECK_C(tmp);
//#endif

	res = (INT32)(SREG) + (INT32)(DREG);

	SET_DREG(res);
	SET_Z( res == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(res) );

	m_icount -= m_clock_cycles_1;

	if( GET_V )
	{
		UINT32 addr = get_trap_addr(TRAPNO_RANGE_ERROR);
		execute_exception(addr);
	}
}

static void hyperstone_cmpb(struct regs_decode *decode)
{
	SET_Z( (DREG & SREG) == 0 ? 1 : 0 );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_andn(struct regs_decode *decode)
{
	DREG = DREG & ~SREG;

	SET_DREG(DREG);
	SET_Z( DREG == 0 ? 1 : 0 );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_or(struct regs_decode *decode)
{
	DREG = DREG | SREG;

	SET_DREG(DREG);
	SET_Z( DREG == 0 ? 1 : 0 );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_xor(struct regs_decode *decode)
{
	DREG = DREG ^ SREG;

	SET_DREG(DREG);
	SET_Z( DREG == 0 ? 1 : 0 );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_subc(struct regs_decode *decode)
{
	UINT64 tmp;

	if( SRC_IS_SR )
	{
		tmp = (UINT64)(DREG) - (UINT64)(GET_C);
		CHECK_VSUB(GET_C,DREG,tmp);
	}
	else
	{
		tmp = (UINT64)(DREG) - ((UINT64)(SREG) + (UINT64)(GET_C));
		//CHECK!
		CHECK_VSUB(SREG + GET_C,DREG,tmp);
	}


	if( SRC_IS_SR )
	{
		DREG = DREG - GET_C;
	}
	else
	{
		DREG = DREG - (SREG + GET_C);
	}

	CHECK_C(tmp);

	SET_DREG(DREG);

	SET_Z( GET_Z & (DREG == 0 ? 1 : 0) );
	SET_N( SIGN_BIT(DREG) );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_not(struct regs_decode *decode)
{
	SET_DREG(~SREG);
	SET_Z( ~SREG == 0 ? 1 : 0 );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_sub(struct regs_decode *decode)
{
	UINT64 tmp;

	if( SRC_IS_SR )
		SREG = GET_C;

	tmp = (UINT64)(DREG) - (UINT64)(SREG);
	CHECK_C(tmp);
	CHECK_VSUB(SREG,DREG,tmp);

	DREG = DREG - SREG;
	SET_DREG(DREG);

	if( DST_IS_PC )
		SET_M(0);

	SET_Z( DREG == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(DREG) );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_subs(struct regs_decode *decode)
{
	INT32 res;
	INT64 tmp;

	if( SRC_IS_SR )
		SREG = GET_C;

	tmp = (INT64)((INT32)(DREG)) - (INT64)((INT32)(SREG));

//#ifdef SETCARRYS
//  CHECK_C(tmp);
//#endif

	CHECK_VSUB(SREG,DREG,tmp);

	res = (INT32)(DREG) - (INT32)(SREG);

	SET_DREG(res);

	SET_Z( res == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(res) );

	m_icount -= m_clock_cycles_1;

	if( GET_V )
	{
		UINT32 addr = get_trap_addr(TRAPNO_RANGE_ERROR);
		execute_exception(addr);
	}
}

static void hyperstone_addc(struct regs_decode *decode)
{
	UINT64 tmp;

	if( SRC_IS_SR )
	{
		tmp = (UINT64)(DREG) + (UINT64)(GET_C);
		CHECK_VADD(DREG,GET_C,tmp);
	}
	else
	{
		tmp = (UINT64)(SREG) + (UINT64)(DREG) + (UINT64)(GET_C);

		//CHECK!
		//CHECK_VADD1: V = (DREG == 0x7FFF) && (C == 1);
		//OVERFLOW = CHECK_VADD1(DREG, C, DREG+C) | CHECK_VADD(SREG, DREG+C, SREG+DREG+C)
		/* check if DREG + GET_C overflows */
//      if( (DREG == 0x7FFFFFFF) && (GET_C == 1) )
//          SET_V(1);
//      else
//          CHECK_VADD(SREG,DREG + GET_C,tmp);

		CHECK_VADD3(SREG,DREG,GET_C,tmp);
	}



	if( SRC_IS_SR )
		DREG = DREG + GET_C;
	else
		DREG = SREG + DREG + GET_C;

	CHECK_C(tmp);

	SET_DREG(DREG);
	SET_Z( GET_Z & (DREG == 0 ? 1 : 0) );
	SET_N( SIGN_BIT(DREG) );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_and(struct regs_decode *decode)
{
	DREG = DREG & SREG;

	SET_DREG(DREG);
	SET_Z( DREG == 0 ? 1 : 0 );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_neg(struct regs_decode *decode)
{
	UINT64 tmp;

	if( SRC_IS_SR )
		SREG = GET_C;

	tmp = -(UINT64)(SREG);
	CHECK_C(tmp);
	CHECK_VSUB(SREG,0,tmp);

	DREG = -SREG;

	SET_DREG(DREG);

	SET_Z( DREG == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(DREG) );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_negs(struct regs_decode *decode)
{
	INT32 res;
	INT64 tmp;

	if( SRC_IS_SR )
		SREG = GET_C;

	tmp = -(INT64)((INT32)(SREG));
	CHECK_VSUB(SREG,0,tmp);

//#if SETCARRYS
//  CHECK_C(tmp);
//#endif

	res = -(INT32)(SREG);

	SET_DREG(res);

	SET_Z( res == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(res) );


	m_icount -= m_clock_cycles_1;

	if( GET_V && !SRC_IS_SR ) //trap doesn't occur when source is SR
	{
		UINT32 addr = get_trap_addr(TRAPNO_RANGE_ERROR);
		execute_exception(addr);
	}
}

static void hyperstone_cmpi(struct regs_decode *decode)
{
	UINT64 tmp;

	tmp = (UINT64)(DREG) - (UINT64)(EXTRA_U);
	CHECK_VSUB(EXTRA_U,DREG,tmp);

	if( DREG == EXTRA_U )
		SET_Z(1);
	else
		SET_Z(0);

	if( (INT32) DREG < (INT32) EXTRA_U )
		SET_N(1);
	else
		SET_N(0);

	if( DREG < EXTRA_U )
		SET_C(1);
	else
		SET_C(0);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_movi(struct regs_decode *decode)
{
	if( !GET_S && decode->dst >= 16 )
	{
		UINT32 addr = get_trap_addr(TRAPNO_PRIVILEGE_ERROR);
		execute_exception(addr);
	}

	SET_DREG(EXTRA_U);

	if( DST_IS_PC )
		SET_M(0);

	SET_Z( EXTRA_U == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(EXTRA_U) );

#if MISSIONCRAFT_FLAGS
	SET_V(0); // or V undefined ?
#endif

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_addi(struct regs_decode *decode)
{
	UINT32 imm;
	UINT64 tmp;

	if( N_VALUE )
		imm = EXTRA_U;
	else
		imm = GET_C & ((GET_Z == 0 ? 1 : 0) | (DREG & 0x01));


	tmp = (UINT64)(imm) + (UINT64)(DREG);
	CHECK_C(tmp);
	CHECK_VADD(imm,DREG,tmp);

	DREG = imm + DREG;
	SET_DREG(DREG);

	if( DST_IS_PC )
		SET_M(0);

	SET_Z( DREG == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(DREG) );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_addsi(struct regs_decode *decode)
{
	INT32 imm, res;
	INT64 tmp;

	if( N_VALUE )
		imm = EXTRA_S;
	else
		imm = GET_C & ((GET_Z == 0 ? 1 : 0) | (DREG & 0x01));

	tmp = (INT64)(imm) + (INT64)((INT32)(DREG));
	CHECK_VADD(imm,DREG,tmp);

//#if SETCARRYS
//  CHECK_C(tmp);
//#endif

	res = imm + (INT32)(DREG);

	SET_DREG(res);

	SET_Z( res == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(res) );

	m_icount -= m_clock_cycles_1;

	if( GET_V )
	{
		UINT32 addr = get_trap_addr(TRAPNO_RANGE_ERROR);
		execute_exception(addr);
	}
}

static void hyperstone_cmpbi(struct regs_decode *decode)
{
	UINT32 imm;

	if( N_VALUE )
	{
		if( N_VALUE == 31 )
		{
			imm = 0x7fffffff; // bit 31 = 0, others = 1
		}
		else
		{
			imm = EXTRA_U;
		}

		SET_Z( (DREG & imm) == 0 ? 1 : 0 );
	}
	else
	{
		if( (DREG & 0xff000000) == 0 || (DREG & 0x00ff0000) == 0 ||
			(DREG & 0x0000ff00) == 0 || (DREG & 0x000000ff) == 0 )
			SET_Z(1);
		else
			SET_Z(0);
	}

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_andni(struct regs_decode *decode)
{
	UINT32 imm;

	if( N_VALUE == 31 )
		imm = 0x7fffffff; // bit 31 = 0, others = 1
	else
		imm = EXTRA_U;

	DREG = DREG & ~imm;

	SET_DREG(DREG);
	SET_Z( DREG == 0 ? 1 : 0 );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_ori(struct regs_decode *decode)
{
	DREG = DREG | EXTRA_U;

	SET_DREG(DREG);
	SET_Z( DREG == 0 ? 1 : 0 );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_xori(struct regs_decode *decode)
{
	DREG = DREG ^ EXTRA_U;

	SET_DREG(DREG);
	SET_Z( DREG == 0 ? 1 : 0 );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_shrdi(struct regs_decode *decode)
{
	UINT32 low_order, high_order;
	UINT64 val;

	high_order = DREG;
	low_order  = DREGF;

	val = CONCAT_64(high_order, low_order);

	if( N_VALUE )
		SET_C((val >> (N_VALUE - 1)) & 1);
	else
		SET_C(0);

	val >>= N_VALUE;

	high_order = EXTRACT_64HI(val);
	low_order  = EXTRACT_64LO(val);

	SET_DREG(high_order);
	SET_DREGF(low_order);
	SET_Z( val == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(high_order) );

	m_icount -= m_clock_cycles_2;
}

static void hyperstone_shrd(struct regs_decode *decode)
{
	UINT32 low_order, high_order;
	UINT64 val;
	UINT8 n = SREG & 0x1f;

	// result undefined if Ls denotes the same register as Ld or Ldf
	if( SAME_SRC_DST || SAME_SRC_DSTF )
	{
		DEBUG_PRINTF(("Denoted same registers in hyperstone_shrd. PC = %08X\n", PC));
	}
	else
	{
		high_order = DREG;
		low_order  = DREGF;

		val = CONCAT_64(high_order, low_order);

		if( n )
			SET_C((val >> (n - 1)) & 1);
		else
			SET_C(0);

		val >>= n;

		high_order = EXTRACT_64HI(val);
		low_order  = EXTRACT_64LO(val);

		SET_DREG(high_order);
		SET_DREGF(low_order);

		SET_Z( val == 0 ? 1 : 0 );
		SET_N( SIGN_BIT(high_order) );
	}

	m_icount -= m_clock_cycles_2;
}

static void hyperstone_shr(struct regs_decode *decode)
{
	UINT32 ret;
	UINT8 n;

	n = SREG & 0x1f;
	ret = DREG;

	if( n )
		SET_C((ret >> (n - 1)) & 1);
	else
		SET_C(0);

	ret >>= n;

	SET_DREG(ret);
	SET_Z( ret == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(ret) );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_sardi(struct regs_decode *decode)
{
	UINT32 low_order, high_order;
	UINT64 val;
	UINT8 sign_bit;

	high_order = DREG;
	low_order  = DREGF;

	val = CONCAT_64(high_order, low_order);

	if( N_VALUE )
		SET_C((val >> (N_VALUE - 1)) & 1);
	else
		SET_C(0);

	sign_bit = val >> 63;
	val >>= N_VALUE;

	if( sign_bit )
	{
		int i;
		for( i = 0; i < N_VALUE; i++ )
		{
			val |= ((UINT64)(0x8000000000000000ULL) >> i);
		}
	}

	high_order = val >> 32;
	low_order  = val & 0xffffffff;

	SET_DREG(high_order);
	SET_DREGF(low_order);

	SET_Z( val == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(high_order) );

	m_icount -= m_clock_cycles_2;
}

static void hyperstone_sard(struct regs_decode *decode)
{
	UINT32 low_order, high_order;
	UINT64 val;
	UINT8 n, sign_bit;

	n = SREG & 0x1f;

	// result undefined if Ls denotes the same register as Ld or Ldf
	if( SAME_SRC_DST || SAME_SRC_DSTF )
	{
		DEBUG_PRINTF(("Denoted same registers in hyperstone_sard. PC = %08X\n", PC));
	}
	else
	{
		high_order = DREG;
		low_order  = DREGF;

		val = CONCAT_64(high_order, low_order);

		if( n )
			SET_C((val >> (n - 1)) & 1);
		else
			SET_C(0);

		sign_bit = val >> 63;

		val >>= n;

		if( sign_bit )
		{
			int i;
			for( i = 0; i < n; i++ )
			{
				val |= ((UINT64)(0x8000000000000000ULL) >> i);
			}
		}

		high_order = val >> 32;
		low_order  = val & 0xffffffff;

		SET_DREG(high_order);
		SET_DREGF(low_order);
		SET_Z( val == 0 ? 1 : 0 );
		SET_N( SIGN_BIT(high_order) );
	}

	m_icount -= m_clock_cycles_2;
}

static void hyperstone_sar(struct regs_decode *decode)
{
	UINT32 ret;
	UINT8 n, sign_bit;

	n = SREG & 0x1f;
	ret = DREG;
	sign_bit = (ret & 0x80000000) >> 31;

	if( n )
		SET_C((ret >> (n - 1)) & 1);
	else
		SET_C(0);

	ret >>= n;

	if( sign_bit )
	{
		int i;
		for( i = 0; i < n; i++ )
		{
			ret |= (0x80000000 >> i);
		}
	}

	SET_DREG(ret);
	SET_Z( ret == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(ret) );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_shldi(struct regs_decode *decode)
{
	UINT32 low_order, high_order, tmp;
	UINT64 val, mask;

	high_order = DREG;
	low_order  = DREGF;

	val  = CONCAT_64(high_order, low_order);
	SET_C( (N_VALUE)?(((val<<(N_VALUE-1))&((UINT64)(0x8000000000000000ULL)))?1:0):0);
	mask = ((((UINT64)1) << (32 - N_VALUE)) - 1) ^ 0xffffffff;
	tmp  = high_order << N_VALUE;

	if( ((high_order & mask) && (!(tmp & 0x80000000))) ||
			(((high_order & mask) ^ mask) && (tmp & 0x80000000)) )
		SET_V(1);
	else
		SET_V(0);

	val <<= N_VALUE;

	high_order = EXTRACT_64HI(val);
	low_order  = EXTRACT_64LO(val);

	SET_DREG(high_order);
	SET_DREGF(low_order);

	SET_Z( val == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(high_order) );

	m_icount -= m_clock_cycles_2;
}

static void hyperstone_shld(struct regs_decode *decode)
{
	UINT32 low_order, high_order, tmp, n;
	UINT64 val, mask;

	n = SREG & 0x1f;

	// result undefined if Ls denotes the same register as Ld or Ldf
	if( SAME_SRC_DST || SAME_SRC_DSTF )
	{
		DEBUG_PRINTF(("Denoted same registers in hyperstone_shld. PC = %08X\n", PC));
	}
	else
	{
		high_order = DREG;
		low_order  = DREGF;

		mask = ((((UINT64)1) << (32 - n)) - 1) ^ 0xffffffff;

		val = CONCAT_64(high_order, low_order);
		SET_C( (n)?(((val<<(n-1))&((UINT64)(0x8000000000000000ULL)))?1:0):0);
		tmp = high_order << n;

		if( ((high_order & mask) && (!(tmp & 0x80000000))) ||
				(((high_order & mask) ^ mask) && (tmp & 0x80000000)) )
			SET_V(1);
		else
			SET_V(0);

		val <<= n;

		high_order = EXTRACT_64HI(val);
		low_order  = EXTRACT_64LO(val);

		SET_DREG(high_order);
		SET_DREGF(low_order);

		SET_Z( val == 0 ? 1 : 0 );
		SET_N( SIGN_BIT(high_order) );
	}

	m_icount -= m_clock_cycles_2;
}

static void hyperstone_shl(struct regs_decode *decode)
{
	UINT32 base, ret, n;
	UINT64 mask;

	n    = SREG & 0x1f;
	base = DREG;
	mask = ((((UINT64)1) << (32 - n)) - 1) ^ 0xffffffff;
	SET_C( (n)?(((base<<(n-1))&0x80000000)?1:0):0);
	ret  = base << n;

	if( ((base & mask) && (!(ret & 0x80000000))) ||
			(((base & mask) ^ mask) && (ret & 0x80000000)) )
		SET_V(1);
	else
		SET_V(0);

	SET_DREG(ret);
	SET_Z( ret == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(ret) );

	m_icount -= m_clock_cycles_1;
}

static void reserved(struct regs_decode *decode)
{
	DEBUG_PRINTF(("Executed Reserved opcode. PC = %08X OP = %04X\n", PC, OP));
}

static void hyperstone_testlz(struct regs_decode *decode)
{
	UINT8 zeros = 0;
	UINT32 mask;

	for( mask = 0x80000000; ; mask >>= 1 )
	{
		if( SREG & mask )
			break;
		else
			zeros++;

		if( zeros == 32 )
			break;
	}

	SET_DREG(zeros);

	m_icount -= m_clock_cycles_2;
}

static void hyperstone_rol(struct regs_decode *decode)
{
	UINT32 val, base;
	UINT8 n;
	UINT64 mask;

	n = SREG & 0x1f;

	val = base = DREG;

	mask = ((((UINT64)1) << (32 - n)) - 1) ^ 0xffffffff;

	while( n > 0 )
	{
		val = (val << 1) | ((val & 0x80000000) >> 31);
		n--;
	}

#ifdef MISSIONCRAFT_FLAGS

	if( ((base & mask) && (!(val & 0x80000000))) ||
			(((base & mask) ^ mask) && (val & 0x80000000)) )
		SET_V(1);
	else
		SET_V(0);

#endif

	SET_DREG(val);

	SET_Z( val == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(val) );

	m_icount -= m_clock_cycles_1;
}

//TODO: add trap error
static void hyperstone_ldxx1(struct regs_decode *decode)
{
	UINT32 load;

	if( DST_IS_SR )
	{
		switch( decode->sub_type )
		{
			case 0: // LDBS.A

				load = READ_B(EXTRA_S);
				load |= (load & 0x80) ? 0xffffff00 : 0;
				SET_SREG(load);

				break;

			case 1: // LDBU.A

				load = READ_B(EXTRA_S);
				SET_SREG(load);

				break;

			case 2:

				load = READ_HW(EXTRA_S & ~1);

				if( EXTRA_S & 1 ) // LDHS.A
				{
					load |= (load & 0x8000) ? 0xffff0000 : 0;
				}
				/*
				else          // LDHU.A
				{
				    // nothing more
				}
				*/

				SET_SREG(load);

				break;

			case 3:

				if( (EXTRA_S & 3) == 3 )      // LDD.IOA
				{
					load = IO_READ_W(EXTRA_S & ~3);
					SET_SREG(load);

					load = IO_READ_W((EXTRA_S & ~3) + 4);
					SET_SREGF(load);

					m_icount -= m_clock_cycles_1; // extra cycle
				}
				else if( (EXTRA_S & 3) == 2 ) // LDW.IOA
				{
					load = IO_READ_W(EXTRA_S & ~3);
					SET_SREG(load);
				}
				else if( (EXTRA_S & 3) == 1 ) // LDD.A
				{
					load = READ_W(EXTRA_S & ~1);
					SET_SREG(load);

					load = READ_W((EXTRA_S & ~1) + 4);
					SET_SREGF(load);

					m_icount -= m_clock_cycles_1; // extra cycle
				}
				else                      // LDW.A
				{
					load = READ_W(EXTRA_S & ~1);
					SET_SREG(load);
				}

				break;
		}
	}
	else
	{
		switch( decode->sub_type )
		{
			case 0: // LDBS.D

				load = READ_B(DREG + EXTRA_S);
				load |= (load & 0x80) ? 0xffffff00 : 0;
				SET_SREG(load);

				break;

			case 1: // LDBU.D

				load = READ_B(DREG + EXTRA_S);
				SET_SREG(load);

				break;

			case 2:

				load = READ_HW(DREG + (EXTRA_S & ~1));

				if( EXTRA_S & 1 ) // LDHS.D
				{
					load |= (load & 0x8000) ? 0xffff0000 : 0;
				}
				/*
				else          // LDHU.D
				{
				    // nothing more
				}
				*/

				SET_SREG(load);

				break;

			case 3:

				if( (EXTRA_S & 3) == 3 )      // LDD.IOD
				{
					load = IO_READ_W(DREG + (EXTRA_S & ~3));
					SET_SREG(load);

					load = IO_READ_W(DREG + (EXTRA_S & ~3) + 4);
					SET_SREGF(load);

					m_icount -= m_clock_cycles_1; // extra cycle
				}
				else if( (EXTRA_S & 3) == 2 ) // LDW.IOD
				{
					load = IO_READ_W(DREG + (EXTRA_S & ~3));
					SET_SREG(load);
				}
				else if( (EXTRA_S & 3) == 1 ) // LDD.D
				{
					load = READ_W(DREG + (EXTRA_S & ~1));
					SET_SREG(load);

					load = READ_W(DREG + (EXTRA_S & ~1) + 4);
					SET_SREGF(load);

					m_icount -= m_clock_cycles_1; // extra cycle
				}
				else                      // LDW.D
				{
					load = READ_W(DREG + (EXTRA_S & ~1));
					SET_SREG(load);
				}

				break;
		}
	}

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_ldxx2(struct regs_decode *decode)
{
	UINT32 load;

	if( DST_IS_PC || DST_IS_SR )
	{
		DEBUG_PRINTF(("Denoted PC or SR in hyperstone_ldxx2. PC = %08X\n", PC));
	}
	else
	{
		switch( decode->sub_type )
		{
			case 0: // LDBS.N

				if(SAME_SRC_DST)
					DEBUG_PRINTF(("LDBS.N denoted same regs @ %08X",PPC));

				load = READ_B(DREG);
				load |= (load & 0x80) ? 0xffffff00 : 0;
				SET_SREG(load);

				if(!SAME_SRC_DST)
					SET_DREG(DREG + EXTRA_S);

				break;

			case 1: // LDBU.N

				if(SAME_SRC_DST)
					DEBUG_PRINTF(("LDBU.N denoted same regs @ %08X",PPC));

				load = READ_B(DREG);
				SET_SREG(load);

				if(!SAME_SRC_DST)
					SET_DREG(DREG + EXTRA_S);

				break;

			case 2:

				load = READ_HW(DREG);

				if( EXTRA_S & 1 ) // LDHS.N
				{
					load |= (load & 0x8000) ? 0xffff0000 : 0;

					if(SAME_SRC_DST)
						DEBUG_PRINTF(("LDHS.N denoted same regs @ %08X",PPC));
				}
				/*
				else          // LDHU.N
				{
				    // nothing more
				}
				*/

				SET_SREG(load);

				if(!SAME_SRC_DST)
					SET_DREG(DREG + (EXTRA_S & ~1));

				break;

			case 3:

				if( (EXTRA_S & 3) == 3 )      // LDW.S
				{
					if(SAME_SRC_DST)
						DEBUG_PRINTF(("LDW.S denoted same regs @ %08X",PPC));

					if(DREG < SP)
						SET_SREG(READ_W(DREG));
					else
						SET_SREG(GET_ABS_L_REG((DREG & 0xfc) >> 2));

					if(!SAME_SRC_DST)
						SET_DREG(DREG + (EXTRA_S & ~3));

					m_icount -= m_clock_cycles_2; // extra cycles
				}
				else if( (EXTRA_S & 3) == 2 ) // Reserved
				{
					DEBUG_PRINTF(("Executed Reserved instruction in hyperstone_ldxx2. PC = %08X\n", PC));
				}
				else if( (EXTRA_S & 3) == 1 ) // LDD.N
				{
					if(SAME_SRC_DST || SAME_SRCF_DST)
						DEBUG_PRINTF(("LDD.N denoted same regs @ %08X",PPC));

					load = READ_W(DREG);
					SET_SREG(load);

					load = READ_W(DREG + 4);
					SET_SREGF(load);

					if(!SAME_SRC_DST && !SAME_SRCF_DST)
						SET_DREG(DREG + (EXTRA_S & ~1));

					m_icount -= m_clock_cycles_1; // extra cycle
				}
				else                      // LDW.N
				{
					if(SAME_SRC_DST)
						DEBUG_PRINTF(("LDW.N denoted same regs @ %08X",PPC));

					load = READ_W(DREG);
					SET_SREG(load);

					if(!SAME_SRC_DST)
						SET_DREG(DREG + (EXTRA_S & ~1));
				}

				break;
		}
	}

	m_icount -= m_clock_cycles_1;
}

//TODO: add trap error
static void hyperstone_stxx1(struct regs_decode *decode)
{
	if( SRC_IS_SR )
		SREG = SREGF = 0;

	if( DST_IS_SR )
	{
		switch( decode->sub_type )
		{
			case 0: // STBS.A

				/* TODO: missing trap on range error */
				WRITE_B(EXTRA_S, SREG & 0xff);

				break;

			case 1: // STBU.A

				WRITE_B(EXTRA_S, SREG & 0xff);

				break;

			case 2:

				WRITE_HW(EXTRA_S & ~1, SREG & 0xffff);

				/*
				if( EXTRA_S & 1 ) // STHS.A
				{
				    // TODO: missing trap on range error
				}
				else          // STHU.A
				{
				    // nothing more
				}
				*/

				break;

			case 3:

				if( (EXTRA_S & 3) == 3 )      // STD.IOA
				{
					IO_WRITE_W(EXTRA_S & ~3, SREG);
					IO_WRITE_W((EXTRA_S & ~3) + 4, SREGF);

					m_icount -= m_clock_cycles_1; // extra cycle
				}
				else if( (EXTRA_S & 3) == 2 ) // STW.IOA
				{
					IO_WRITE_W(EXTRA_S & ~3, SREG);
				}
				else if( (EXTRA_S & 3) == 1 ) // STD.A
				{
					WRITE_W(EXTRA_S & ~1, SREG);
					WRITE_W((EXTRA_S & ~1) + 4, SREGF);

					m_icount -= m_clock_cycles_1; // extra cycle
				}
				else                      // STW.A
				{
					WRITE_W(EXTRA_S & ~1, SREG);
				}

				break;
		}
	}
	else
	{
		switch( decode->sub_type )
		{
			case 0: // STBS.D

				/* TODO: missing trap on range error */
				WRITE_B(DREG + EXTRA_S, SREG & 0xff);

				break;

			case 1: // STBU.D

				WRITE_B(DREG + EXTRA_S, SREG & 0xff);

				break;

			case 2:

				WRITE_HW(DREG + (EXTRA_S & ~1), SREG & 0xffff);

				/*
				if( EXTRA_S & 1 ) // STHS.D
				{
				    // TODO: missing trap on range error
				}
				else          // STHU.D
				{
				    // nothing more
				}
				*/

				break;

			case 3:

				if( (EXTRA_S & 3) == 3 )      // STD.IOD
				{
					IO_WRITE_W(DREG + (EXTRA_S & ~3), SREG);
					IO_WRITE_W(DREG + (EXTRA_S & ~3) + 4, SREGF);

					m_icount -= m_clock_cycles_1; // extra cycle
				}
				else if( (EXTRA_S & 3) == 2 ) // STW.IOD
				{
					IO_WRITE_W(DREG + (EXTRA_S & ~3), SREG);
				}
				else if( (EXTRA_S & 3) == 1 ) // STD.D
				{
					WRITE_W(DREG + (EXTRA_S & ~1), SREG);
					WRITE_W(DREG + (EXTRA_S & ~1) + 4, SREGF);

					m_icount -= m_clock_cycles_1; // extra cycle
				}
				else                      // STW.D
				{
					WRITE_W(DREG + (EXTRA_S & ~1), SREG);
				}

				break;
		}
	}

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_stxx2(struct regs_decode *decode)
{
	if( SRC_IS_SR )
		SREG = SREGF = 0;

	if( DST_IS_PC || DST_IS_SR )
	{
		DEBUG_PRINTF(("Denoted PC or SR in hyperstone_stxx2. PC = %08X\n", PC));
	}
	else
	{
		switch( decode->sub_type )
		{
			case 0: // STBS.N

				/* TODO: missing trap on range error */
				WRITE_B(DREG, SREG & 0xff);
				SET_DREG(DREG + EXTRA_S);

				break;

			case 1: // STBU.N

				WRITE_B(DREG, SREG & 0xff);
				SET_DREG(DREG + EXTRA_S);

				break;

			case 2:

				WRITE_HW(DREG, SREG & 0xffff);
				SET_DREG(DREG + (EXTRA_S & ~1));

				/*
				if( EXTRA_S & 1 ) // STHS.N
				{
				    // TODO: missing trap on range error
				}
				else          // STHU.N
				{
				    // nothing more
				}
				*/

				break;

			case 3:

				if( (EXTRA_S & 3) == 3 )      // STW.S
				{
					if(DREG < SP)
						WRITE_W(DREG, SREG);
					else
					{
						if(((DREG & 0xfc) >> 2) == ((decode->src + GET_FP) % 64) && S_BIT == LOCAL)
							DEBUG_PRINTF(("STW.S denoted the same local register @ %08X\n",PPC));

						SET_ABS_L_REG((DREG & 0xfc) >> 2,SREG);
					}

					SET_DREG(DREG + (EXTRA_S & ~3));

					m_icount -= m_clock_cycles_2; // extra cycles

				}
				else if( (EXTRA_S & 3) == 2 ) // Reserved
				{
					DEBUG_PRINTF(("Executed Reserved instruction in hyperstone_stxx2. PC = %08X\n", PC));
				}
				else if( (EXTRA_S & 3) == 1 ) // STD.N
				{
					WRITE_W(DREG, SREG);
					SET_DREG(DREG + (EXTRA_S & ~1));

					if( SAME_SRCF_DST )
						WRITE_W(DREG + 4, SREGF + (EXTRA_S & ~1));  // because DREG == SREGF and DREG has been incremented
					else
						WRITE_W(DREG + 4, SREGF);

					m_icount -= m_clock_cycles_1; // extra cycle
				}
				else                      // STW.N
				{
					WRITE_W(DREG, SREG);
					SET_DREG(DREG + (EXTRA_S & ~1));
				}

				break;
		}
	}

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_shri(struct regs_decode *decode)
{
	UINT32 val;

	val = DREG;

	if( N_VALUE )
		SET_C((val >> (N_VALUE - 1)) & 1);
	else
		SET_C(0);

	val >>= N_VALUE;

	SET_DREG(val);
	SET_Z( val == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(val) );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_sari(struct regs_decode *decode)
{
	UINT32 val;
	UINT8 sign_bit;

	val = DREG;
	sign_bit = (val & 0x80000000) >> 31;

	if( N_VALUE )
		SET_C((val >> (N_VALUE - 1)) & 1);
	else
		SET_C(0);

	val >>= N_VALUE;

	if( sign_bit )
	{
		int i;
		for( i = 0; i < N_VALUE; i++ )
		{
			val |= (0x80000000 >> i);
		}
	}

	SET_DREG(val);
	SET_Z( val == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(val) );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_shli(struct regs_decode *decode)
{
	UINT32 val, val2;
	UINT64 mask;

	val  = DREG;
	SET_C( (N_VALUE)?(((val<<(N_VALUE-1))&0x80000000)?1:0):0);
	mask = ((((UINT64)1) << (32 - N_VALUE)) - 1) ^ 0xffffffff;
	val2 = val << N_VALUE;

	if( ((val & mask) && (!(val2 & 0x80000000))) ||
			(((val & mask) ^ mask) && (val2 & 0x80000000)) )
		SET_V(1);
	else
		SET_V(0);

	SET_DREG(val2);
	SET_Z( val2 == 0 ? 1 : 0 );
	SET_N( SIGN_BIT(val2) );

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_mulu(struct regs_decode *decode)
{
	UINT32 low_order, high_order;
	UINT64 double_word;

	// PC or SR aren't denoted, else result is undefined
	if( SRC_IS_PC || SRC_IS_SR || DST_IS_PC || DST_IS_SR  )
	{
		DEBUG_PRINTF(("Denoted PC or SR in hyperstone_mulu instruction. PC = %08X\n", PC));
	}
	else
	{
		double_word = (UINT64)SREG *(UINT64)DREG;

		low_order = double_word & 0xffffffff;
		high_order = double_word >> 32;

		SET_DREG(high_order);
		SET_DREGF(low_order);

		SET_Z( double_word == 0 ? 1 : 0 );
		SET_N( SIGN_BIT(high_order) );
	}

	if(SREG <= 0xffff && DREG <= 0xffff)
		m_icount -= m_clock_cycles_4;
	else
		m_icount -= m_clock_cycles_6;
}

static void hyperstone_muls(struct regs_decode *decode)
{
	UINT32 low_order, high_order;
	INT64 double_word;

	// PC or SR aren't denoted, else result is undefined
	if( SRC_IS_PC || SRC_IS_SR || DST_IS_PC || DST_IS_SR  )
	{
		DEBUG_PRINTF(("Denoted PC or SR in hyperstone_muls instruction. PC = %08X\n", PC));
	}
	else
	{
		double_word = (INT64)(INT32)(SREG) * (INT64)(INT32)(DREG);
		low_order = double_word & 0xffffffff;
		high_order = double_word >> 32;

		SET_DREG(high_order);
		SET_DREGF(low_order);

		SET_Z( double_word == 0 ? 1 : 0 );
		SET_N( SIGN_BIT(high_order) );
	}

	if((SREG >= 0xffff8000 && SREG <= 0x7fff) && (DREG >= 0xffff8000 && DREG <= 0x7fff))
		m_icount -= m_clock_cycles_4;
	else
		m_icount -= m_clock_cycles_6;
}

static void hyperstone_set(struct regs_decode *decode)
{
	int n = N_VALUE;

	if( DST_IS_PC )
	{
		DEBUG_PRINTF(("Denoted PC in hyperstone_set. PC = %08X\n", PC));
	}
	else if( DST_IS_SR )
	{
		//TODO: add fetch opcode when there's the pipeline

		//TODO: no 1!
		m_icount -= m_clock_cycles_1;
	}
	else
	{
		switch( n )
		{
			// SETADR
			case 0:
			{
				UINT32 val;
				val =  (SP & 0xfffffe00) | (GET_FP << 2);

				//plus carry into bit 9
				val += (( (SP & 0x100) && (SIGN_BIT(SR) == 0) ) ? 1 : 0);

				SET_DREG(val);

				break;
			}
			// Reserved
			case 1:
			case 16:
			case 17:
			case 19:
				DEBUG_PRINTF(("Used reserved N value (%d) in hyperstone_set. PC = %08X\n", n, PC));
				break;

			// SETxx
			case 2:
				SET_DREG(1);
				break;

			case 3:
				SET_DREG(0);
				break;

			case 4:
				if( GET_N || GET_Z )
				{
					SET_DREG(1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 5:
				if( !GET_N && !GET_Z )
				{
					SET_DREG(1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 6:
				if( GET_N )
				{
					SET_DREG(1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 7:
				if( !GET_N )
				{
					SET_DREG(1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 8:
				if( GET_C || GET_Z )
				{
					SET_DREG(1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 9:
				if( !GET_C && !GET_Z )
				{
					SET_DREG(1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 10:
				if( GET_C )
				{
					SET_DREG(1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 11:
				if( !GET_C )
				{
					SET_DREG(1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 12:
				if( GET_Z )
				{
					SET_DREG(1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 13:
				if( !GET_Z )
				{
					SET_DREG(1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 14:
				if( GET_V )
				{
					SET_DREG(1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 15:
				if( !GET_V )
				{
					SET_DREG(1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 18:
				SET_DREG(-1);
				break;

			case 20:
				if( GET_N || GET_Z )
				{
					SET_DREG(-1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 21:
				if( !GET_N && !GET_Z )
				{
					SET_DREG(-1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 22:
				if( GET_N )
				{
					SET_DREG(-1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 23:
				if( !GET_N )
				{
					SET_DREG(-1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 24:
				if( GET_C || GET_Z )
				{
					SET_DREG(-1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 25:
				if( !GET_C && !GET_Z )
				{
					SET_DREG(-1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 26:
				if( GET_C )
				{
					SET_DREG(-1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 27:
				if( !GET_C )
				{
					SET_DREG(-1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 28:
				if( GET_Z )
				{
					SET_DREG(-1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 29:
				if( !GET_Z )
				{
					SET_DREG(-1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 30:
				if( GET_V )
				{
					SET_DREG(-1);
				}
				else
				{
					SET_DREG(0);
				}

				break;

			case 31:
				if( !GET_V )
				{
					SET_DREG(-1);
				}
				else
				{
					SET_DREG(0);
				}

				break;
		}

		m_icount -= m_clock_cycles_1;
	}
}

static void hyperstone_mul(struct regs_decode *decode)
{
	UINT32 single_word;

	// PC or SR aren't denoted, else result is undefined
	if( SRC_IS_PC || SRC_IS_SR || DST_IS_PC || DST_IS_SR  )
	{
		DEBUG_PRINTF(("Denoted PC or SR in hyperstone_mul instruction. PC = %08X\n", PC));
	}
	else
	{
		single_word = (SREG * DREG);// & 0xffffffff; // only the low-order word is taken

		SET_DREG(single_word);

		SET_Z( single_word == 0 ? 1 : 0 );
		SET_N( SIGN_BIT(single_word) );
	}

	if((SREG >= 0xffff8000 && SREG <= 0x7fff) && (DREG >= 0xffff8000 && DREG <= 0x7fff))
		m_icount -= 3 << m_clock_scale;
	else
		m_icount -= 5 << m_clock_scale;
}

static void hyperstone_fadd(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_faddd(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_fsub(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_fsubd(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_fmul(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_fmuld(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_fdiv(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_fdivd(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_fcmp(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_fcmpd(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_fcmpu(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_fcmpud(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_fcvt(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_fcvtd(struct regs_decode *decode)
{
	execute_software(decode);
	m_icount -= m_clock_cycles_6;
}

static void hyperstone_extend(struct regs_decode *decode)
{
	//TODO: add locks, overflow error and other things
	UINT32 vals, vald;

	vals = SREG;
	vald = DREG;

	switch( EXTRA_U ) // extended opcode
	{
		// signed or unsigned multiplication, single word product
		case EMUL:
		case 0x100: // used in "N" type cpu
		{
			UINT32 result;

			result = vals * vald;
			SET_G_REG(15, result);

			break;
		}
		// unsigned multiplication, double word product
		case EMULU:
		{
			UINT64 result;

			result = (UINT64)vals * (UINT64)vald;
			vals = result >> 32;
			vald = result & 0xffffffff;
			SET_G_REG(14, vals);
			SET_G_REG(15, vald);

			break;
		}
		// signed multiplication, double word product
		case EMULS:
		{
			INT64 result;

			result = (INT64)(INT32)(vals) * (INT64)(INT32)(vald);
			vals = result >> 32;
			vald = result & 0xffffffff;
			SET_G_REG(14, vals);
			SET_G_REG(15, vald);

			break;
		}
		// signed multiply/add, single word product sum
		case EMAC:
		{
			INT32 result;

			result = (INT32)GET_G_REG(15) + ((INT32)(vals) * (INT32)(vald));
			SET_G_REG(15, result);

			break;
		}
		// signed multiply/add, double word product sum
		case EMACD:
		{
			INT64 result;

			result = (INT64)CONCAT_64(GET_G_REG(14), GET_G_REG(15)) + (INT64)((INT64)(INT32)(vals) * (INT64)(INT32)(vald));

			vals = result >> 32;
			vald = result & 0xffffffff;
			SET_G_REG(14, vals);
			SET_G_REG(15, vald);

			break;
		}
		// signed multiply/substract, single word product difference
		case EMSUB:
		{
			INT32 result;

			result = (INT32)GET_G_REG(15) - ((INT32)(vals) * (INT32)(vald));
			SET_G_REG(15, result);

			break;
		}
		// signed multiply/substract, double word product difference
		case EMSUBD:
		{
			INT64 result;

			result = (INT64)CONCAT_64(GET_G_REG(14), GET_G_REG(15)) - (INT64)((INT64)(INT32)(vals) * (INT64)(INT32)(vald));

			vals = result >> 32;
			vald = result & 0xffffffff;
			SET_G_REG(14, vals);
			SET_G_REG(15, vald);

			break;
		}
		// signed half-word multiply/add, single word product sum
		case EHMAC:
		{
			INT32 result;

			result = (INT32)GET_G_REG(15) + ((INT32)((vald & 0xffff0000) >> 16) * (INT32)((vals & 0xffff0000) >> 16)) + ((INT32)(vald & 0xffff) * (INT32)(vals & 0xffff));
			SET_G_REG(15, result);

			break;
		}
		// signed half-word multiply/add, double word product sum
		case EHMACD:
		{
			INT64 result;

			result = (INT64)CONCAT_64(GET_G_REG(14), GET_G_REG(15)) + (INT64)((INT64)(INT32)((vald & 0xffff0000) >> 16) * (INT64)(INT32)((vals & 0xffff0000) >> 16)) + ((INT64)(INT32)(vald & 0xffff) * (INT64)(INT32)(vals & 0xffff));

			vals = result >> 32;
			vald = result & 0xffffffff;
			SET_G_REG(14, vals);
			SET_G_REG(15, vald);

			break;
		}
		// half-word complex multiply
		case EHCMULD:
		{
			UINT32 result;

			result = (((vald & 0xffff0000) >> 16) * ((vals & 0xffff0000) >> 16)) - ((vald & 0xffff) * (vals & 0xffff));
			SET_G_REG(14, result);

			result = (((vald & 0xffff0000) >> 16) * (vals & 0xffff)) + ((vald & 0xffff) * ((vals & 0xffff0000) >> 16));
			SET_G_REG(15, result);

			break;
		}
		// half-word complex multiply/add
		case EHCMACD:
		{
			UINT32 result;

			result = GET_G_REG(14) + (((vald & 0xffff0000) >> 16) * ((vals & 0xffff0000) >> 16)) - ((vald & 0xffff) * (vals & 0xffff));
			SET_G_REG(14, result);

			result = GET_G_REG(15) + (((vald & 0xffff0000) >> 16) * (vals & 0xffff)) + ((vald & 0xffff) * ((vals & 0xffff0000) >> 16));
			SET_G_REG(15, result);

			break;
		}
		// half-word (complex) add/substract
		// Ls is not used and should denote the same register as Ld
		case EHCSUMD:
		{
			UINT32 result;

			result = ((((vals & 0xffff0000) >> 16) + GET_G_REG(14)) << 16) & 0xffff0000;
			result |= ((vals & 0xffff) + GET_G_REG(15)) & 0xffff;
			SET_G_REG(14, result);

			result = ((((vals & 0xffff0000) >> 16) - GET_G_REG(14)) << 16) & 0xffff0000;
			result |= ((vals & 0xffff) - GET_G_REG(15)) & 0xffff;
			SET_G_REG(15, result);

			break;
		}
		// half-word (complex) add/substract with fixed point adjustment
		// Ls is not used and should denote the same register as Ld
		case EHCFFTD:
		{
			UINT32 result;

			result = ((((vals & 0xffff0000) >> 16) + (GET_G_REG(14) >> 15)) << 16) & 0xffff0000;
			result |= ((vals & 0xffff) + (GET_G_REG(15) >> 15)) & 0xffff;
			SET_G_REG(14, result);

			result = ((((vals & 0xffff0000) >> 16) - (GET_G_REG(14) >> 15)) << 16) & 0xffff0000;
			result |= ((vals & 0xffff) - (GET_G_REG(15) >> 15)) & 0xffff;
			SET_G_REG(15, result);

			break;
		}
		// half-word (complex) add/substract with fixed point adjustment and shift
		// Ls is not used and should denote the same register as Ld
		case EHCFFTSD:
		{
			UINT32 result;

			result = (((((vals & 0xffff0000) >> 16) + (GET_G_REG(14) >> 15)) >> 1) << 16) & 0xffff0000;
			result |= ((((vals & 0xffff) + (GET_G_REG(15) >> 15)) >> 1) & 0xffff);
			SET_G_REG(14, result);

			result = (((((vals & 0xffff0000) >> 16) - (GET_G_REG(14) >> 15)) >> 1) << 16) & 0xffff0000;
			result |= ((((vals & 0xffff) - (GET_G_REG(15) >> 15)) >> 1) & 0xffff);
			SET_G_REG(15, result);

			break;
		}
		default:
			DEBUG_PRINTF(("Executed Illegal extended opcode (%X). PC = %08X\n", EXTRA_U, PC));
			break;
	}

	m_icount -= m_clock_cycles_1; //TODO: with the latency it can change
}

static void hyperstone_do(struct regs_decode *)
{
//	fatalerror("Executed hyperstone_do instruction. PC = %08X\n", PPC);
}

static void hyperstone_ldwr(struct regs_decode *decode)
{
	SET_SREG(READ_W(DREG));

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_lddr(struct regs_decode *decode)
{
	SET_SREG(READ_W(DREG));
	SET_SREGF(READ_W(DREG + 4));

	m_icount -= m_clock_cycles_2;
}

static void hyperstone_ldwp(struct regs_decode *decode)
{
	SET_SREG(READ_W(DREG));

	// post increment the destination register if it's different from the source one
	// (needed by Hidden Catch)
	if(!(decode->src == decode->dst && S_BIT == LOCAL))
		SET_DREG(DREG + 4);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_lddp(struct regs_decode *decode)
{
	SET_SREG(READ_W(DREG));
	SET_SREGF(READ_W(DREG + 4));

	// post increment the destination register if it's different from the source one
	// and from the "next source" one
	if(!(decode->src == decode->dst && S_BIT == LOCAL) &&   !SAME_SRCF_DST )
	{
		SET_DREG(DREG + 8);
	}
	else
	{
		DEBUG_PRINTF(("LDD.P denoted same regs @ %08X",PPC));
	}

	m_icount -= m_clock_cycles_2;
}

static void hyperstone_stwr(struct regs_decode *decode)
{
	if( SRC_IS_SR )
		SREG = 0;

	WRITE_W(DREG, SREG);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_stdr(struct regs_decode *decode)
{
	if( SRC_IS_SR )
		SREG = SREGF = 0;

	WRITE_W(DREG, SREG);
	WRITE_W(DREG + 4, SREGF);

	m_icount -= m_clock_cycles_2;
}

static void hyperstone_stwp(struct regs_decode *decode)
{
	if( SRC_IS_SR )
		SREG = 0;

	WRITE_W(DREG, SREG);
	SET_DREG(DREG + 4);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_stdp(struct regs_decode *decode)
{
	if( SRC_IS_SR )
		SREG = SREGF = 0;

	WRITE_W(DREG, SREG);
	SET_DREG(DREG + 8);

	if( SAME_SRCF_DST )
		WRITE_W(DREG + 4, SREGF + 8); // because DREG == SREGF and DREG has been incremented
	else
		WRITE_W(DREG + 4, SREGF);

	m_icount -= m_clock_cycles_2;
}

static void hyperstone_dbv(struct regs_decode *decode)
{
	if( GET_V )
		execute_dbr(decode);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_dbnv(struct regs_decode *decode)
{
	if( !GET_V )
		execute_dbr(decode);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_dbe(struct regs_decode *decode) //or DBZ
{
	if( GET_Z )
		execute_dbr(decode);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_dbne(struct regs_decode *decode) //or DBNZ
{
	if( !GET_Z )
		execute_dbr(decode);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_dbc(struct regs_decode *decode) //or DBST
{
	if( GET_C )
		execute_dbr(decode);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_dbnc(struct regs_decode *decode) //or DBHE
{
	if( !GET_C )
		execute_dbr(decode);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_dbse(struct regs_decode *decode)
{
	if( GET_C || GET_Z )
		execute_dbr(decode);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_dbht(struct regs_decode *decode)
{
	if( !GET_C && !GET_Z )
		execute_dbr(decode);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_dbn(struct regs_decode *decode) //or DBLT
{
	if( GET_N )
		execute_dbr(decode);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_dbnn(struct regs_decode *decode) //or DBGE
{
	if( !GET_N )
		execute_dbr(decode);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_dble(struct regs_decode *decode)
{
	if( GET_N || GET_Z )
		execute_dbr(decode);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_dbgt(struct regs_decode *decode)
{
	if( !GET_N && !GET_Z )
		execute_dbr(decode);

	m_icount -= m_clock_cycles_1;
}

static void hyperstone_dbr(struct regs_decode *decode)
{
	execute_dbr(decode);
}

static void hyperstone_frame(struct regs_decode *)
{
	INT8 difference; // really it's 7 bits
	UINT8 realfp = GET_FP - SRC_CODE;

	SET_FP(realfp);
	SET_FL(DST_CODE);
	SET_M(0);

	difference = ((SP & 0x1fc) >> 2) + (64 - 10) - (realfp + GET_FL);

	/* convert to 8 bits */
	if(difference > 63)
		difference = (INT8)(difference|0x80);
	else if( difference < -64 )
		difference = difference & 0x7f;

	if( difference < 0 ) // else it's finished
	{
		UINT8 tmp_flag;

		tmp_flag = ( SP >= UB ? 1 : 0 );

		do
		{
			WRITE_W(SP, GET_ABS_L_REG((SP & 0xfc) >> 2));
			SP += 4;
			difference++;

		} while(difference != 0);

		if( tmp_flag )
		{
			UINT32 addr = get_trap_addr(TRAPNO_FRAME_ERROR);
			execute_exception(addr);
		}
	}

	//TODO: no 1!
	m_icount -= m_clock_cycles_1;
}

static void hyperstone_call(struct regs_decode *decode)
{
	if( SRC_IS_SR )
		SREG = 0;

	if( !DST_CODE )
		decode->dst = 16;

	EXTRA_S = (EXTRA_S & ~1) + SREG;

	SET_ILC(m_instruction_length & 3);

	SET_DREG((PC & 0xfffffffe) | GET_S);
	SET_DREGF(SR);

	SET_FP(GET_FP + decode->dst);

	SET_FL(6); //default value for call
	SET_M(0);

	PPC = PC;
	PC = EXTRA_S; // const value

	m_intblock = 2;

	//TODO: add interrupt locks, errors, ....

	//TODO: no 1!
	m_icount -= m_clock_cycles_1;
}

static void hyperstone_bv(struct regs_decode *decode)
{
	if( GET_V )
		execute_br(decode);
	else
		m_icount -= m_clock_cycles_1;
}

static void hyperstone_bnv(struct regs_decode *decode)
{
	if( !GET_V )
		execute_br(decode);
	else
		m_icount -= m_clock_cycles_1;
}

static void hyperstone_be(struct regs_decode *decode) //or BZ
{
	if( GET_Z )
		execute_br(decode);
	else
		m_icount -= m_clock_cycles_1;
}

static void hyperstone_bne(struct regs_decode *decode) //or BNZ
{
	if( !GET_Z )
		execute_br(decode);
	else
		m_icount -= m_clock_cycles_1;
}

static void hyperstone_bc(struct regs_decode *decode) //or BST
{
	if( GET_C )
		execute_br(decode);
	else
		m_icount -= m_clock_cycles_1;
}

static void hyperstone_bnc(struct regs_decode *decode) //or BHE
{
	if( !GET_C )
		execute_br(decode);
	else
		m_icount -= m_clock_cycles_1;
}

static void hyperstone_bse(struct regs_decode *decode)
{
	if( GET_C || GET_Z )
		execute_br(decode);
	else
		m_icount -= m_clock_cycles_1;
}

static void hyperstone_bht(struct regs_decode *decode)
{
	if( !GET_C && !GET_Z )
		execute_br(decode);
	else
		m_icount -= m_clock_cycles_1;
}

static void hyperstone_bn(struct regs_decode *decode) //or BLT
{
	if( GET_N )
		execute_br(decode);
	else
		m_icount -= m_clock_cycles_1;
}

static void hyperstone_bnn(struct regs_decode *decode) //or BGE
{
	if( !GET_N )
		execute_br(decode);
	else
		m_icount -= m_clock_cycles_1;
}

static void hyperstone_ble(struct regs_decode *decode)
{
	if( GET_N || GET_Z )
		execute_br(decode);
	else
		m_icount -= m_clock_cycles_1;
}

static void hyperstone_bgt(struct regs_decode *decode)
{
	if( !GET_N && !GET_Z )
		execute_br(decode);
	else
		m_icount -= m_clock_cycles_1;
}

static void hyperstone_br(struct regs_decode *decode)
{
	execute_br(decode);
}

static void hyperstone_trap(struct regs_decode *)
{
	UINT8 code, trapno;
	UINT32 addr;

	trapno = (OP & 0xfc) >> 2;

	addr = get_trap_addr(trapno);
	code = ((OP & 0x300) >> 6) | (OP & 0x03);

	switch( code )
	{
		case TRAPLE:
			if( GET_N || GET_Z )
				execute_trap(addr);

			break;

		case TRAPGT:
			if( !GET_N && !GET_Z )
				execute_trap(addr);

			break;

		case TRAPLT:
			if( GET_N )
				execute_trap(addr);

			break;

		case TRAPGE:
			if( !GET_N )
				execute_trap(addr);

			break;

		case TRAPSE:
			if( GET_C || GET_Z )
				execute_trap(addr);

			break;

		case TRAPHT:
			if( !GET_C && !GET_Z )
				execute_trap(addr);

			break;

		case TRAPST:
			if( GET_C )
				execute_trap(addr);

			break;

		case TRAPHE:
			if( !GET_C )
				execute_trap(addr);

			break;

		case TRAPE:
			if( GET_Z )
				execute_trap(addr);

			break;

		case TRAPNE:
			if( !GET_Z )
				execute_trap(addr);

			break;

		case TRAPV:
			if( GET_V )
				execute_trap(addr);

			break;

		case TRAP:
			execute_trap(addr);

			break;
	}

	m_icount -= m_clock_cycles_1;
}


#include "e132xsop.inc"


static void map_internal_ram(UINT32 size)
{
	for (INT32 i = 0; i < 0x20000000; i += size)
	{
		E132XSMapMemory(internal_ram,	0xc0000000 + i, 0xc0000000 + i + (size - 1), MAP_RAM);
	}
}

static void core_init(int scale_mask)
{
	memset(m_global_regs, 0, sizeof(UINT32) * 32);
	memset(m_local_regs, 0, sizeof(UINT32) * 64);
	m_ppc = 0;
	m_op = 0;
	m_trap_entry = 0;
	m_clock_scale_mask = 0;
	m_clock_scale = 0;
	m_clock_cycles_1 = 0;
	m_clock_cycles_2 = 0;
	m_clock_cycles_4 = 0;
	m_clock_cycles_6 = 0;

	m_tr_base_cycles = 0;
	m_tr_base_value = 0;
	m_tr_clocks_per_tick = 0;
	m_timer_int_pending = 0;

	m_instruction_length = 0;
	m_intblock = 0;

	m_icount = 0;

	timer_time = -1;
	timer_param = 0;
	m_clock_scale_mask = scale_mask;
}

void E132XSInit(INT32 , INT32 type, INT32 )
{
	memset (mem, 0, sizeof(mem));

	write_byte_handler = NULL;
	write_word_handler = NULL;
	write_dword_handler = NULL;
	read_byte_handler = NULL;
	read_word_handler = NULL;
	read_dword_handler = NULL;
	io_write_dword_handler = NULL;
	io_read_dword_handler = NULL;

	switch (type)
	{
		case TYPE_E116T:
			core_init(0);
			map_internal_ram(0x1000);
		break;

		case TYPE_E116XT:
			core_init(3);
			map_internal_ram(0x2000);
		break;

		case TYPE_E116XS:
			core_init(7);
			map_internal_ram(0x4000);
		break;

		case TYPE_E116XSR:
			core_init(7);
			map_internal_ram(0x4000);
		break;

		case TYPE_E132N:
			core_init(0);
			map_internal_ram(0x1000);
		break;

		case TYPE_E132T:
			core_init(0);
			map_internal_ram(0x1000);
		break;

		case TYPE_E132XN:
			core_init(3);
			map_internal_ram(0x2000);
		break;

		case TYPE_E132XT:
			core_init(3);
			map_internal_ram(0x2000);
		break;

		case TYPE_E132XS:
			core_init(7);
			map_internal_ram(0x4000);
		break;

		case TYPE_E132XSR:
			core_init(7);
			map_internal_ram(0x4000);
		break;

		case TYPE_GMS30C2116:
			core_init(0);
			map_internal_ram(0x1000);
		break;

		case TYPE_GMS30C2132:
			core_init(0);
			map_internal_ram(0x1000);
		break;

		case TYPE_GMS30C2216:
			core_init(0);
			map_internal_ram(0x2000);
		break;

		case TYPE_GMS30C2232:
			core_init(0);
			map_internal_ram(0x2000);
		break;
	}
}

void E132XSReset()
{
	//TODO: Add different reset initializations for BCR, MCR, FCR, TPR

	m_tr_clocks_per_tick = 2;

	hyperstone_set_trap_entry(E132XS_ENTRY_MEM3); /* default entry point @ MEM3 */

	set_global_register(BCR_REGISTER, ~0);
	set_global_register(MCR_REGISTER, ~0);
	set_global_register(FCR_REGISTER, ~0);
	set_global_register(TPR_REGISTER, 0xc000000);

	PC = get_trap_addr(TRAPNO_RESET);

	SET_FP(0);
	SET_FL(2);

	SET_M(0);
	SET_T(0);
	SET_L(1);
	SET_S(1);

	SET_L_REG(0, (PC & 0xfffffffe) | GET_S);
	SET_L_REG(1, SR);

	m_icount = 0;
	m_icount -= m_clock_cycles_2;
	itotal_cycles = 0;
	utotal_cycles = 0;
	n_cycles = 0;

	m_hold_irq = 0;
	sleep_until_int = 0;
}

void E132XSOpen(INT32 nCpu)
{
	if (nCpu){}
}

void E132XSClose()
{
}

void E132XSExit()
{

}

INT64 E132XSTotalCycles()
{
	return utotal_cycles + (n_cycles - m_icount);
}

void E132XSNewFrame()
{
	utotal_cycles = 0;
}

void E132XSScan(INT32 nAction)
{
	SCAN_VAR(m_global_regs);
	SCAN_VAR(m_local_regs);

	SCAN_VAR(internal_ram);

	SCAN_VAR(m_ppc);          // previous pc
	SCAN_VAR(m_op);           // opcode
	SCAN_VAR(m_trap_entry);   // entry point to get trap address

	SCAN_VAR(m_clock_scale_mask);
	SCAN_VAR(m_clock_scale);
	SCAN_VAR(m_clock_cycles_1);
	SCAN_VAR(m_clock_cycles_2);
	SCAN_VAR(m_clock_cycles_4);
	SCAN_VAR(m_clock_cycles_6);

	SCAN_VAR(m_tr_base_cycles);
	SCAN_VAR(m_tr_base_value);
	SCAN_VAR(m_tr_clocks_per_tick);
	SCAN_VAR(m_timer_int_pending);

	SCAN_VAR(timer_time);
	SCAN_VAR(timer_param);
	SCAN_VAR(m_hold_irq);

	SCAN_VAR(m_delay);

	SCAN_VAR(m_instruction_length);
	SCAN_VAR(m_intblock);

	SCAN_VAR(m_icount);
	SCAN_VAR(itotal_cycles); // internal total cycles (timers etc)
	SCAN_VAR(utotal_cycles); // user-total cycles (E132XSTotalCycles() / E132XSNewFrame() etc..)
	SCAN_VAR(n_cycles);
}

void E132XSSetIRQLine(INT32 line, INT32 state)
{
	if (state) sleep_until_int = 0;

	switch (state) {
		case CPU_IRQSTATUS_HOLD:
			execute_set_input(line, 2);
			break;
		case CPU_IRQSTATUS_AUTO:
			execute_set_input(line, 1);
			E132XSRun(10);
			execute_set_input(line, 0);
			break;
		default:
			execute_set_input(line, state ? 1 : 0);
			E132XSRun(10);
			break;
	}
}

void E132XSBurnUntilInt()
{
	sleep_until_int = 1;
}

INT32 E132XSIdle(INT32 cycles)
{
	utotal_cycles += cycles;

	return cycles;
}

static INT32 end_run = 0;

INT32 E132XSRun(INT32 cycles)
{
	if (sleep_until_int) {
		return E132XSIdle(cycles);
	}

	m_icount = cycles;
	n_cycles = m_icount;

	if (m_intblock < 0)
		m_intblock = 0;

	check_interrupts();

	INT32 t_icount;

	end_run = 0;

	do
	{
		t_icount = m_icount;

		UINT32 oldh = SR & 0x00000020;

		PPC = PC;   /* copy PC to previous PC */

		OP = READ_OP(PC);
		PC += 2;

		m_instruction_length = 1;

		switch (OP >> 8)
		{
			case 0x00: op00(); break;
			case 0x01: op01(); break;
			case 0x02: op02(); break;
			case 0x03: op03(); break;
			case 0x04: op04(); break;
			case 0x05: op05(); break;
			case 0x06: op06(); break;
			case 0x07: op07(); break;
			case 0x08: op08(); break;
			case 0x09: op09(); break;
			case 0x0a: op0a(); break;
			case 0x0b: op0b(); break;
			case 0x0c: op0c(); break;
			case 0x0d: op0d(); break;
			case 0x0e: op0e(); break;
			case 0x0f: op0f(); break;
			case 0x10: op10(); break;
			case 0x11: op11(); break;
			case 0x12: op12(); break;
			case 0x13: op13(); break;
			case 0x14: op14(); break;
			case 0x15: op15(); break;
			case 0x16: op16(); break;
			case 0x17: op17(); break;
			case 0x18: op18(); break;
			case 0x19: op19(); break;
			case 0x1a: op1a(); break;
			case 0x1b: op1b(); break;
			case 0x1c: op1c(); break;
			case 0x1d: op1d(); break;
			case 0x1e: op1e(); break;
			case 0x1f: op1f(); break;
			case 0x20: op20(); break;
			case 0x21: op21(); break;
			case 0x22: op22(); break;
			case 0x23: op23(); break;
			case 0x24: op24(); break;
			case 0x25: op25(); break;
			case 0x26: op26(); break;
			case 0x27: op27(); break;
			case 0x28: op28(); break;
			case 0x29: op29(); break;
			case 0x2a: op2a(); break;
			case 0x2b: op2b(); break;
			case 0x2c: op2c(); break;
			case 0x2d: op2d(); break;
			case 0x2e: op2e(); break;
			case 0x2f: op2f(); break;
			case 0x30: op30(); break;
			case 0x31: op31(); break;
			case 0x32: op32(); break;
			case 0x33: op33(); break;
			case 0x34: op34(); break;
			case 0x35: op35(); break;
			case 0x36: op36(); break;
			case 0x37: op37(); break;
			case 0x38: op38(); break;
			case 0x39: op39(); break;
			case 0x3a: op3a(); break;
			case 0x3b: op3b(); break;
			case 0x3c: op3c(); break;
			case 0x3d: op3d(); break;
			case 0x3e: op3e(); break;
			case 0x3f: op3f(); break;
			case 0x40: op40(); break;
			case 0x41: op41(); break;
			case 0x42: op42(); break;
			case 0x43: op43(); break;
			case 0x44: op44(); break;
			case 0x45: op45(); break;
			case 0x46: op46(); break;
			case 0x47: op47(); break;
			case 0x48: op48(); break;
			case 0x49: op49(); break;
			case 0x4a: op4a(); break;
			case 0x4b: op4b(); break;
			case 0x4c: op4c(); break;
			case 0x4d: op4d(); break;
			case 0x4e: op4e(); break;
			case 0x4f: op4f(); break;
			case 0x50: op50(); break;
			case 0x51: op51(); break;
			case 0x52: op52(); break;
			case 0x53: op53(); break;
			case 0x54: op54(); break;
			case 0x55: op55(); break;
			case 0x56: op56(); break;
			case 0x57: op57(); break;
			case 0x58: op58(); break;
			case 0x59: op59(); break;
			case 0x5a: op5a(); break;
			case 0x5b: op5b(); break;
			case 0x5c: op5c(); break;
			case 0x5d: op5d(); break;
			case 0x5e: op5e(); break;
			case 0x5f: op5f(); break;
			case 0x60: op60(); break;
			case 0x61: op61(); break;
			case 0x62: op62(); break;
			case 0x63: op63(); break;
			case 0x64: op64(); break;
			case 0x65: op65(); break;
			case 0x66: op66(); break;
			case 0x67: op67(); break;
			case 0x68: op68(); break;
			case 0x69: op69(); break;
			case 0x6a: op6a(); break;
			case 0x6b: op6b(); break;
			case 0x6c: op6c(); break;
			case 0x6d: op6d(); break;
			case 0x6e: op6e(); break;
			case 0x6f: op6f(); break;
			case 0x70: op70(); break;
			case 0x71: op71(); break;
			case 0x72: op72(); break;
			case 0x73: op73(); break;
			case 0x74: op74(); break;
			case 0x75: op75(); break;
			case 0x76: op76(); break;
			case 0x77: op77(); break;
			case 0x78: op78(); break;
			case 0x79: op79(); break;
			case 0x7a: op7a(); break;
			case 0x7b: op7b(); break;
			case 0x7c: op7c(); break;
			case 0x7d: op7d(); break;
			case 0x7e: op7e(); break;
			case 0x7f: op7f(); break;
			case 0x80: op80(); break;
			case 0x81: op81(); break;
			case 0x82: op82(); break;
			case 0x83: op83(); break;
			case 0x84: op84(); break;
			case 0x85: op85(); break;
			case 0x86: op86(); break;
			case 0x87: op87(); break;
			case 0x88: op88(); break;
			case 0x89: op89(); break;
			case 0x8a: op8a(); break;
			case 0x8b: op8b(); break;
			case 0x8c: op8c(); break;
			case 0x8d: op8d(); break;
			case 0x8e: op8e(); break;
			case 0x8f: op8f(); break;
			case 0x90: op90(); break;
			case 0x91: op91(); break;
			case 0x92: op92(); break;
			case 0x93: op93(); break;
			case 0x94: op94(); break;
			case 0x95: op95(); break;
			case 0x96: op96(); break;
			case 0x97: op97(); break;
			case 0x98: op98(); break;
			case 0x99: op99(); break;
			case 0x9a: op9a(); break;
			case 0x9b: op9b(); break;
			case 0x9c: op9c(); break;
			case 0x9d: op9d(); break;
			case 0x9e: op9e(); break;
			case 0x9f: op9f(); break;
			case 0xa0: opa0(); break;
			case 0xa1: opa1(); break;
			case 0xa2: opa2(); break;
			case 0xa3: opa3(); break;
			case 0xa4: opa4(); break;
			case 0xa5: opa5(); break;
			case 0xa6: opa6(); break;
			case 0xa7: opa7(); break;
			case 0xa8: opa8(); break;
			case 0xa9: opa9(); break;
			case 0xaa: opaa(); break;
			case 0xab: opab(); break;
			case 0xac: opac(); break;
			case 0xad: opad(); break;
			case 0xae: opae(); break;
			case 0xaf: opaf(); break;
			case 0xb0: opb0(); break;
			case 0xb1: opb1(); break;
			case 0xb2: opb2(); break;
			case 0xb3: opb3(); break;
			case 0xb4: opb4(); break;
			case 0xb5: opb5(); break;
			case 0xb6: opb6(); break;
			case 0xb7: opb7(); break;
			case 0xb8: opb8(); break;
			case 0xb9: opb9(); break;
			case 0xba: opba(); break;
			case 0xbb: opbb(); break;
			case 0xbc: opbc(); break;
			case 0xbd: opbd(); break;
			case 0xbe: opbe(); break;
			case 0xbf: opbf(); break;
			case 0xc0: opc0(); break;
			case 0xc1: opc1(); break;
			case 0xc2: opc2(); break;
			case 0xc3: opc3(); break;
			case 0xc4: opc4(); break;
			case 0xc5: opc5(); break;
			case 0xc6: opc6(); break;
			case 0xc7: opc7(); break;
			case 0xc8: opc8(); break;
			case 0xc9: opc9(); break;
			case 0xca: opca(); break;
			case 0xcb: opcb(); break;
			case 0xcc: opcc(); break;
			case 0xcd: opcd(); break;
			case 0xce: opce(); break;
			case 0xcf: opcf(); break;
			case 0xd0: opd0(); break;
			case 0xd1: opd1(); break;
			case 0xd2: opd2(); break;
			case 0xd3: opd3(); break;
			case 0xd4: opd4(); break;
			case 0xd5: opd5(); break;
			case 0xd6: opd6(); break;
			case 0xd7: opd7(); break;
			case 0xd8: opd8(); break;
			case 0xd9: opd9(); break;
			case 0xda: opda(); break;
			case 0xdb: opdb(); break;
			case 0xdc: opdc(); break;
			case 0xdd: opdd(); break;
			case 0xde: opde(); break;
			case 0xdf: opdf(); break;
			case 0xe0: ope0(); break;
			case 0xe1: ope1(); break;
			case 0xe2: ope2(); break;
			case 0xe3: ope3(); break;
			case 0xe4: ope4(); break;
			case 0xe5: ope5(); break;
			case 0xe6: ope6(); break;
			case 0xe7: ope7(); break;
			case 0xe8: ope8(); break;
			case 0xe9: ope9(); break;
			case 0xea: opea(); break;
			case 0xeb: opeb(); break;
			case 0xec: opec(); break;
			case 0xed: oped(); break;
			case 0xee: opee(); break;
			case 0xef: opef(); break;
			case 0xf0: opf0(); break;
			case 0xf1: opf1(); break;
			case 0xf2: opf2(); break;
			case 0xf3: opf3(); break;
			case 0xf4: opf4(); break;
			case 0xf5: opf5(); break;
			case 0xf6: opf6(); break;
			case 0xf7: opf7(); break;
			case 0xf8: opf8(); break;
			case 0xf9: opf9(); break;
			case 0xfa: opfa(); break;
			case 0xfb: opfb(); break;
			case 0xfc: opfc(); break;
			case 0xfd: opfd(); break;
			case 0xfe: opfe(); break;
			case 0xff: opff(); break;
		}

		/* clear the H state if it was previously set */
		SR ^= oldh;

		SET_ILC(m_instruction_length & 3);

		if( GET_T && GET_P && m_delay.delay_cmd == NO_DELAY ) /* Not in a Delayed Branch instructions */
		{
			UINT32 addr = get_trap_addr(TRAPNO_TRACE_EXCEPTION);
			execute_exception(addr);
		}

		if (--m_intblock == 0)
			check_interrupts();

			// here??
		if (timer_time > 0) {
			timer_time -= t_icount - m_icount;
			if (timer_time <= 0) {
				timer_callback(timer_param);
			}
		}

		itotal_cycles += t_icount - m_icount;

	} while( m_icount > 0 && !end_run );

	cycles = n_cycles - m_icount;
	utotal_cycles += cycles;

	n_cycles = m_icount = 0;

	return cycles;
}

void E132XSRunEnd()
{
	end_run = 1;
}

void E132XSRunEndBurnAllCycles()
{
	m_icount = 0;
}

INT32 E132XSBurnCycles(INT32 cycles)
{
	m_icount -= cycles;
	return cycles;
}


