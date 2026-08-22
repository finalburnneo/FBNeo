#pragma once

#ifndef GBA_ARM7_H
#define GBA_ARM7_H 1

#include <stdint.h>
#include <stdio.h>
#include <string.h>

////////////////
// Data Types //
////////////////

#define LR			14
#define PC			15
#define CPSR		16
#define SPSR		17 

#define R13_fiq		22 
#define R13_irq		24 
#define R13_svc		26 
#define R13_abt		28 
#define R13_und		30 

#define R14_fiq		23 
#define R14_irq		25 
#define R14_svc		27 
#define R14_abt		29 
#define R14_und		31 

#define SPSR_fiq	32 
#define SPSR_irq	33 
#define SPSR_svc	34 
#define SPSR_abt	35 
#define SPSR_und	36 

// Memory IO functions for the emulated CPU (these must be defined by the user)
typedef UINT32 (*arm_read32_fn_t)(void* user_data, UINT32 address);
typedef UINT32 (*arm_read16_fn_t)(void* user_data, UINT32 address);
typedef UINT32 (*arm_read32_seq_fn_t)(void* user_data, UINT32 address,bool is_sequential);
typedef UINT32 (*arm_read16_seq_fn_t)(void* user_data, UINT32 address,bool is_sequential);
typedef UINT8  (*arm_read8_fn_t)(void* user_data, UINT32 address);
typedef void   (*arm_write32_fn_t)(void* user_data, UINT32 address, UINT32 data);
typedef void   (*arm_write16_fn_t)(void* user_data, UINT32 address, UINT16 data);
typedef void   (*arm_write8_fn_t)(void* user_data, UINT32 address, UINT8 data);
typedef UINT32 (*arm_coproc_read_fn_t)(void* user_data, INT32 coproc, INT32 opcode, INT32 Cn, INT32 Cm, INT32 Cp);
typedef void   (*arm_coproc_write_fn_t)(void* user_data, INT32 coproc, INT32 opcode, INT32 Cn, INT32 Cm, INT32 Cp, UINT32 data);
typedef void   (*arm_trigger_breakpoint_fn_t)(void* user_data);


typedef struct {
	// Registers
	/*
	0-15: R0-R15
	16: CPSR
	17-23: R8_fiq-R14_fiq
	24-25: R13_irq-R14_irq
	26-27: R13_svc-R14_svc
	28-29: R13_abt-R14_abt
	30-31: R13_und-R14_und
	32: SPSR_fiq
	33: SPSR_irq
	34: SPSR_svc
	35: SPSR_abt
	36: SPSR_und
	*/

	UINT32						prefetch_pc;
	UINT32						step_instructions;		//Instructions to step before triggering a breakpoint
	UINT32						prefetch_opcode[5];
	UINT32						i_cycles;				//Executed i-cycles minus 1
	bool						next_fetch_sequential;
	UINT32						registers[37];
	void*						user_data;
	arm_read32_fn_t				read32;
	arm_read16_fn_t				read16;
	arm_read32_seq_fn_t			read32_seq;
	arm_read16_seq_fn_t			read16_seq;
	arm_read8_fn_t				read8;
	arm_write32_fn_t			write32;
	arm_write16_fn_t			write16;
	arm_write8_fn_t				write8;
	arm_coproc_read_fn_t		coprocessor_read;
	arm_coproc_write_fn_t		coprocessor_write;
	arm_trigger_breakpoint_fn_t	trigger_breakpoint;
	bool						wait_for_interrupt;
	UINT32						phased_opcode;
	UINT32						phased_op_id;
	UINT32						phase;
	struct {
		UINT32					addr;
		UINT32					r15_off;
		UINT32					last_bank;
		UINT32					base_addr;
		UINT32					cycle;
		UINT32					num_regs;
	} block;
} arm7_t;

typedef void (*arm7_handler_t)(arm7_t* cpu, UINT32 opcode);
typedef struct {
	arm7_handler_t				handler;
	char						name[12];
	char						bitfield[33];
} arm7_instruction_t;

#define ARM_PHASED_NONE      0 
#define ARM_PHASED_FILL_PIPE 1
#define ARM_PHASED_BLOCK_TRANSFER 2

////////////////////////
// User API Functions //
////////////////////////

// This function initializes the internal state needed for the arm7 core emulation
static inline arm7_t arm7_init(void* user_data);
static inline void   arm7_exec_instruction(arm7_t* cpu);

// Write the dissassembled opcode from mem_address into the out_disasm string up to out_size characters
// Used to send an interrupt to the emulated CPU. The n'th set bit triggers the n'th interrupt
static inline void arm7_process_interrupts(arm7_t* cpu);
///////////////////////////////////////////
// Functions for Internal Implementation //
///////////////////////////////////////////

// ARM Instruction Implementations
static inline void arm7_data_processing(arm7_t* cpu, UINT32 opcode);
static inline void arm7_multiply(arm7_t* cpu, UINT32 opcode);
static inline void arm7_multiply_long(arm7_t* cpu, UINT32 opcode);
static inline void arm7_single_data_swap(arm7_t* cpu, UINT32 opcode);
static inline void arm7_branch_exchange(arm7_t* cpu, UINT32 opcode);
static inline void arm7_half_word_transfer(arm7_t* cpu, UINT32 opcode);
static inline void arm7_single_word_transfer(arm7_t* cpu, UINT32 opcode);
static inline void arm7_undefined(arm7_t* cpu, UINT32 opcode);
static inline void arm7_block_transfer(arm7_t* cpu, UINT32 opcode);
static inline void arm7_branch(arm7_t* cpu, UINT32 opcode);

static inline void arm7_coproc_data_transfer(arm7_t* cpu, UINT32 opcode);
static inline void arm7_coproc_data_op(arm7_t* cpu, UINT32 opcode);
static inline void arm7_coproc_reg_transfer(arm7_t* cpu, UINT32 opcode);
static inline void arm7_software_interrupt(arm7_t* cpu, UINT32 opcode);

static inline void arm7_mrs(arm7_t* cpu, UINT32 opcode);
static inline void arm7_msr(arm7_t* cpu, UINT32 opcode);

// Thumb Instruction Implementations
static inline void arm7t_mov_shift_reg(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_add_sub(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_mov_cmp_add_sub_imm(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_alu_op(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_hi_reg_op(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_pc_rel_ldst(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_reg_off_ldst(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_ldst_bh(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_imm_off_ldst(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_imm_off_ldst_bh(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_stack_off_ldst(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_load_addr(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_add_off_sp(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_push_pop_reg(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_mult_ldst(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_cond_branch(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_soft_interrupt(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_branch(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_long_branch_link(arm7_t* cpu, UINT32 opcode);
static inline void arm7t_unknown(arm7_t* cpu, UINT32 opcode);

// Internal functions
static inline INT32 arm_lookup_arm_instruction_class(const arm7_instruction_t*instruction_table, UINT32 opcode_key);
static inline INT32 arm_lookup_thumb_instruction_class(const arm7_instruction_t*instruction_table,UINT32 opcode_key);
static inline bool arm7_check_cond_code(arm7_t* cpu, UINT32 opcode);
static inline UINT32 arm7_reg_read(arm7_t*cpu, UINT32 reg);
static inline UINT32 arm7_reg_read_r15_adj(arm7_t*cpu, UINT32 reg, INT32 r15_off);
static inline void   arm7_reg_write(arm7_t*cpu, UINT32 reg, UINT32 value);
static inline UINT32 arm7_reg_index(arm7_t* cpu, UINT32 reg);
static inline UINT32 arm7_shift(arm7_t* arm, UINT32 opcode, UINT64 value, UINT32 shift_value, INT32* carry);
static inline UINT32 arm7_load_shift_reg(arm7_t* arm, UINT32 opcode, INT32* carry);
static inline UINT32 arm7_rotr(UINT32 value, UINT32 rotate);
static inline bool   arm7_get_thumb_bit(arm7_t* cpu);
static inline void   arm7_set_thumb_bit(arm7_t* cpu, bool value);

#define ARM7_BFE(VALUE, BITOFFSET, SIZE) (((VALUE) >> (BITOFFSET)) & ((1u << (SIZE)) - 1))

// ARM7 ARM Classes
const static arm7_instruction_t arm7_instruction_classes[] = {
	{ arm7_data_processing,			"DP",			"cccc0010oooSnnnnddddrrrrOOOOOOOO"	},
	{ arm7_data_processing,			"DP",			"cccc00111ooSnnnnddddrrrrOOOOOOOO"	},
	{ arm7_data_processing,			"DP",			"cccc00110oo1nnnnddddrrrrOOOOOOOO"	},
	//These duplications are to handle disambiguating bit 5 and 7 set to ones for DP 
	{ arm7_data_processing,			"DP",			"cccc0000oooSnnnnddddsssssss0mmmm"	},
	{ arm7_data_processing,			"DP",			"cccc0000oooSnnnnddddssss0tt1mmmm"	},
	//Handle TST, TEQ, CMP, CMN must set S case
	{ arm7_data_processing,			"DP",			"cccc00011ooSnnnnddddsssssss0mmmm"	},
	{ arm7_data_processing,			"DP",			"cccc00011ooSnnnnddddssss0tt1mmmm"	},
	{ arm7_data_processing,			"DP",			"cccc00010oo1nnnnddddssss0tt1mmmm"	},
	{ arm7_data_processing,			"DP",			"cccc00010oo1nnnnddddsssssss0mmmm"	},

	{ arm7_multiply,				"MUL",			"cccc000000ASddddnnnnssss1001mmmm"	},
	{ arm7_multiply_long,			"MLONG",		"cccc00001UASddddnnnnssss1001mmmm"	},
	{ arm7_single_data_swap,		"SDS",			"cccc00010B00nnnndddd00001001mmmm"	},
	{ arm7_branch_exchange,			"BX",			"cccc000100101111111111110001nnnn"	},

	{ arm7_undefined,				"LDRD/STRD",	"cccc000PUIW0nnnnddddoooo11S1oooo"	},
	{ arm7_half_word_transfer,		"HDT(h)",		"cccc000PUIWLnnnndddd00001011mmmm"	},
	{ arm7_half_word_transfer,		"HDT(sb)",		"cccc000PUIW1nnnnddddOOOO1101OOOO"	},
	{ arm7_half_word_transfer,		"HDT(sh)",		"cccc000PUIW1nnnnddddOOOO1111OOOO"	},
	{ arm7_single_word_transfer,	"SDT",			"cccc010PUBWLnnnnddddOOOOOOOOOOOO"	},
	{ arm7_single_word_transfer,	"SDT",			"cccc011PUBWLnnnnddddOOOOOOO0mmmm"	},

	{ arm7_undefined,				"UDEF",			"cccc011--------------------1----"	},
	{ arm7_block_transfer,			"BDT",			"cccc100PUSWLnnnnllllllllllllllll"	},
	{ arm7_branch,					"B",			"cccc1010OOOOOOOOOOOOOOOOOOOOOOOO"	},
	{ arm7_branch,					"BL",			"cccc1011OOOOOOOOOOOOOOOOOOOOOOOO"	},
	{ arm7_coproc_data_transfer,	"CDT",			"cccc110PUNWLnnnndddd####OOOOOOOO"	},
	{ arm7_coproc_data_op,			"CDO",			"cccc1110oooonnnndddd####ppp0mmmm"	},
	{ arm7_coproc_reg_transfer,		"CRT",			"cccc1110oooLnnnndddd####ppp1mmmm"	},
	{ arm7_software_interrupt,		"SWI",			"cccc1111------------------------"	},
	{ arm7_mrs,						"MRS",			"cccc00010P001111dddd000000000000"	},
	{ arm7_msr,						"MSR",			"cccc00010P10100F111100000000mmmm"	},
	{ arm7_msr,						"MSR",			"cccc00110P10100F1111oooooooooooo"	},
	{ arm7_undefined,				"UNKNOWN1",		"cccc000001--------------1001----"	},
	{ arm7_undefined,				"UNKNOWN2",		"cccc00011---------------1001----"	},
	{ arm7_undefined,				"UNKNOWN3",		"cccc00010-1-------------1001----"	},
	{ arm7_undefined,				"UNKNOWN4",		"cccc00010-01------------1001----"	},
	// Handle invalid opcode space in DP
	{ arm7_undefined,				"UNKNOWN6",		"cccc00010-00------------01-0----"	},
	{ arm7_undefined,				"UNKNOWN7",		"cccc00010-00------------0010----"	},

	{ arm7_undefined,				"QADD/QSUB",	"cccc00010oo0nnnndddd00000101mmmm"	},
	{ arm7_undefined,				"UNKNOWN8",		"cccc00010-00------------00-1----"	},
	{ arm7_undefined,				"UNKNOWNN",		"cccc00010-00------------0111----"	},
	{ arm7_undefined,				"SMLA",			"cccc00010oo0ddddnnnnssss1yx0mmmm"	},
	{ arm7_undefined,				"UNKNOWNA",		"cccc00010110------------01-0----"	},
	{ arm7_undefined,				"UNKNOWNB",		"cccc00010110------------0010----"	},
	{ arm7_undefined,				"CLZ",			"cccc000101101111DDDD11110001MMMM"	},
	{ arm7_undefined,				"UNKNOWNC",		"cccc00010110------------0111----"	},
	{ arm7_undefined,				"UNKNOWND",		"cccc00010110------------0011----"	},
	{ arm7_undefined,				"UNKNOWNE",		"cccc00010010------------0111----"	},
	{ arm7_undefined,				"BLX",			"cccc000100101111111111110011nnnn"	},
	{ arm7_undefined,				"UNKNOWNG",		"cccc00010010------------01-0----"	},
	{ arm7_undefined,				"UNKNOWNH",		"cccc00010010------------0010----"	},
	{ arm7_undefined,				"UNKNOWNI",		"cccc00110-000000000000001-------"	},
	{ arm7_undefined,				"UNKNOWNJ",		"cccc00110-0000000000000001------"	},
	{ arm7_undefined,				"UNKNOWNK",		"cccc00110-00000000000000001-----"	},
	{ arm7_undefined,				"UNKNOWNL",		"cccc00110-000000000000000001----"	},
	{ arm7_undefined,				"UNKNOWNM",		"----00110-00------------0000----"	},
	{NULL},

};

// ARM7 ARM Classes

// ARM7 Thumb Classes
const static arm7_instruction_t arm7t_instruction_classes[]={
	{ arm7t_mov_shift_reg,			"LSL",			"00000OOOOOsssddd"	},
	{ arm7t_mov_shift_reg,			"LSR",			"00001OOOOOsssddd"	},
	{ arm7t_mov_shift_reg,			"ASR",			"00010OOOOOsssddd"	},
	{ arm7t_add_sub,				"ADD",			"00011I0nnnsssddd"	},
	{ arm7t_add_sub,				"SUB",			"00011I1nnnsssddd"	},
	{ arm7t_mov_cmp_add_sub_imm,	"MCASIMM",		"001oodddOOOOOOOO"	},
	{ arm7t_alu_op,					"ALU",			"010000oooosssddd"	},
	{ arm7t_hi_reg_op,				"HROP",			"010001oohHsssddd"	},
	{ arm7t_pc_rel_ldst,			"PCRLD",		"01001dddOOOOOOOO"	},
	{ arm7t_reg_off_ldst,			"LDST[RD]",		"0101LB0ooobbbddd"	},
	{ arm7t_ldst_bh,				"SLDST[RD]",	"0101HS1ooobbbddd"	},
	{ arm7t_imm_off_ldst,			"LDST[IMM]",	"011BLOOOOObbbddd"	},
	{ arm7t_imm_off_ldst_bh,		"SLDSTH[IMM]",	"1000LOOOOObbbddd"	},
	{ arm7t_stack_off_ldst,			"LDST[SP]",		"1001LdddOOOOOOOO"	},
	{ arm7t_load_addr,				"LDADDR",		"1010SdddOOOOOOOO"	},
	{ arm7t_add_off_sp,				"SP+=OFF",		"10110000SOOOOOOO"	},
	{ arm7t_push_pop_reg,			"PUSHPOPREG",	"1011L10Rllllllll"	},
	{ arm7t_mult_ldst,				"MLDST",		"1100Lbbbllllllll"	},
	// Conditional branches cant branch on condition 1111
	{ arm7t_cond_branch,			"COND B",		"11010cccOOOOOOOO"	},
	{ arm7t_cond_branch,			"COND B",		"110110ccOOOOOOOO"	},
	{ arm7t_cond_branch,			"COND B",		"1101110cOOOOOOOO"	},
	{ arm7t_cond_branch,			"COND B",		"11011110OOOOOOOO"	},
	{ arm7t_soft_interrupt,			"SWI",			"11011111OOOOOOOO"	},
	{ arm7t_branch,					"B",			"11100OOOOOOOOOOO"	},
	{ arm7t_long_branch_link,		"BL",			"1111HOOOOOOOOOOO"	},
	{ arm7t_long_branch_link,		"BLX",			"11101OOOOOOOOOOO"	},
	//Empty Opcode Space
	{ arm7t_unknown,				"UNKNOWN1",		"1011--1---------"	},
	{ arm7t_unknown,				"UNKNOWN2",		"10110001--------"	},
	{ arm7t_unknown,				"UNKNOWN3",		"1011100---------"	},
	{NULL},
};

// ARM7 Thumb Classes

static arm7_handler_t arm7_lookup_table[4096] = { 0 };
static arm7_handler_t arm7t_lookup_table[256] = { 0 };


static inline UINT32 arm7_reg_index(arm7_t* cpu, UINT32 reg)
{
	if (SB_LIKELY(reg < 8))
		return reg;
	INT32 mode = cpu->registers[CPSR] & 0xf;

	const static INT8 lookup[10 * 16 + 8] = {
		-1, -1, -1, -1, -1, -1, -1, -1,				//8 extra padding to remove the need to -8 from computation
		 8,  9, 10, 11, 12, 13, 14, 15, 16, 16,		//mode 0x0 (user)
		17, 18, 19, 20, 21, 22, 23, 15, 16, 32,		//mode 0x1 (fiq)
		 8,  9, 10, 11, 12, 24, 25, 15, 16, 33,		//mode 0x2 (irq)
		 8,  9, 10, 11, 12, 26, 27, 15, 16, 34,		//mode 0x3 (svc)
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//mode 0x4 (inv)
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//mode 0x5 (inv)
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//mode 0x6 (inv)
		 8,  9, 10, 11, 12, 28, 29, 15, 16, 35,		//mode 0x7 (abt)
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//mode 0x8 (inv)
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//mode 0x9 (inv)
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//mode 0xA (inv)
		 8,  9, 10, 11, 12, 30, 31, 15, 16, 36,		//mode 0xB (undefined)
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//mode 0xC (inv)
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//mode 0xD (inv)
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,		//mode 0xE (inv)
		 8,  9, 10, 11, 12, 13, 14, 15, 16, 16,		//mode 0xF (system)
	};
	INT8 r = lookup[mode * 10 + reg];
	if (SB_LIKELY(r != -1))return r;
	if (cpu->trigger_breakpoint)cpu->trigger_breakpoint(cpu->user_data);
	printf("Undefined ARM mode: %d\n", mode);
	return 0;
}

static inline void arm7_reg_write(arm7_t* cpu, UINT32 reg, UINT32 value)
{
	cpu->registers[arm7_reg_index(cpu, reg)] = value;
}


static inline UINT32 arm7_reg_read(arm7_t* cpu, UINT32 reg)
{
	return cpu->registers[arm7_reg_index(cpu, reg)];
}

static inline UINT32 arm7_reg_read_r15_adj(arm7_t* cpu, UINT32 reg, INT32 r15_off)
{
	UINT32 v = arm7_reg_read(cpu, reg);
	if (SB_UNLIKELY(reg == PC)) {
		v += r15_off;
		if (arm7_get_thumb_bit(cpu))v -= 2;
	}
	return v;
}

static inline INT32 arm_lookup_arm_instruction_class(const arm7_instruction_t* instruction_table, UINT32 opcode_key)
{
	INT32 key_bits[] = { 4,5,6,7, 20,21,22,23,24,25,26,27 };
	INT32 matched_class = -1;
	for (INT32 c = 0; instruction_table[c].handler; ++c) {
		bool matches = true;
		for (INT32 bit = 0; bit < ARRAY_SIZE(key_bits); ++bit) {
			bool bit_value = (opcode_key >> bit) & 1;
			INT32 b_off = key_bits[bit];
			matches &= instruction_table[c].bitfield[31 - b_off] != '1' || bit_value == true;
			matches &= instruction_table[c].bitfield[31 - b_off] != '0' || bit_value == false;
			if (!matches)
				break;
		}
		if (matches) {
			if (matched_class != -1) {
				INT32  op_value = 0;
				char opcode[33] = "00000000000000000000000000000000";
				for (INT32 bit = 0; bit < ARRAY_SIZE(key_bits); ++bit) {
					bool bit_value = (opcode_key >> bit) & 1;
					if (bit_value) {
						opcode[31 - key_bits[bit]] = '1';
						op_value |= 1 << key_bits[bit];
					}
				}
				printf("ARM7: Class %s and %s have ambiguous encodings for: %s %08x %d\n",
					instruction_table[c].name,
					instruction_table[matched_class].name,
					opcode, op_value, opcode_key);
			}
			matched_class = c;
		}
	}
	if (matched_class == -1) {
		UINT32 op_value = 0;
		char opcode[33] = "00000000000000000000000000000000";
		for (INT32 bit = 0; bit < ARRAY_SIZE(key_bits); ++bit) {
			bool bit_value = (opcode_key >> bit) & 1;
			if (bit_value) {
				opcode[31 - key_bits[bit]] = '1';
				op_value |= 1 << key_bits[bit];
			}
		}
		printf("ARM: No matching instruction class for key: %s %08x\n", opcode, op_value);
	}
	return matched_class;
}

static inline INT32 arm_lookup_thumb_instruction_class(const arm7_instruction_t* instruction_table, UINT32 opcode_key)
{
	INT32 key_bits[] = { 8,9,10,11,12,13,14,15 };
	INT32 matched_class = -1;
	for (INT32 c = 0; instruction_table[c].handler; ++c) {
		bool matches = true;
		for (INT32 bit = 0; bit < ARRAY_SIZE(key_bits); ++bit) {
			bool bit_value = (opcode_key >> bit) & 1;
			INT32 b_off = key_bits[bit];
			matches &= instruction_table[c].bitfield[15 - b_off] != '1' || bit_value == true;
			matches &= instruction_table[c].bitfield[15 - b_off] != '0' || bit_value == false;
			if (!matches)
				break;
		}

		if (matches) {
			if (matched_class != -1) {
				printf("ARM7t: Class %s and %s have ambiguous encodings\n",
					instruction_table[c].name,
					instruction_table[matched_class].name);
			}
			matched_class = c;
		}
	}
	if (matched_class == -1) {
		UINT32 op_value = 0;
		char opcode[17] = "0000000000000000";
		for (INT32 bit = 0; bit < ARRAY_SIZE(key_bits); ++bit) {
			bool bit_value = (opcode_key >> bit) & 1;
			if (bit_value) {
				opcode[15 - key_bits[bit]] = '1';
				op_value |= 1 << key_bits[bit];
			}
		}
		printf("ARM7T: No matching instruction class for key: %s %04x\n", opcode, op_value);
	}
	return matched_class;
}
static inline arm7_t arm7_init(void* user_data)
{
	// Generate ARM lookup table
	for (INT32 i = 0; i < 4096; ++i) {
		INT32 inst_class = arm_lookup_arm_instruction_class(arm7_instruction_classes, i);
		arm7_lookup_table[i] = inst_class == -1 ? NULL : arm7_instruction_classes[inst_class].handler;
	}
	// Generate Thumb Lookup Table
	for (INT32 i = 0; i < 256; ++i) {
		INT32 inst_class = arm_lookup_thumb_instruction_class(arm7t_instruction_classes, i);
		arm7t_lookup_table[i] = inst_class == -1 ? NULL : arm7t_instruction_classes[inst_class].handler;
	}
	// Generate Thumb Lookup Table
	arm7_t arm = {};
	arm.user_data = user_data;
	arm.prefetch_pc = -1;
	arm.phase = 0;
	arm.phased_op_id = ARM_PHASED_FILL_PIPE;
	return arm;

}
static inline bool arm7_get_thumb_bit(arm7_t* cpu) { return ARM7_BFE(cpu->registers[CPSR],5,1);}
static inline void arm7_set_thumb_bit(arm7_t* cpu, bool value) {
	cpu->registers[CPSR] &= ~(1 << 5);
	if (value)
		cpu->registers[CPSR] |= 1 << 5;
}
static inline void arm7_process_interrupts(arm7_t* cpu) {
  cpu->wait_for_interrupt=false;
  UINT32 cpsr = cpu->registers[CPSR];
  bool I = ARM7_BFE(cpsr,7,1);
  if(I==0&&cpu->phased_op_id==0){
	//Interrupts are enabled when I ==0
	cpu->registers[R14_irq] = cpu->registers[PC]+4;
	cpu->registers[PC] = 0+ 0x18; 
	cpu->registers[SPSR_irq] = cpsr;
	//Update mode to IRQ
	cpu->registers[CPSR] = (cpsr&0xffffffE0)| 0x12;
	//Disable interrupts(set I bit)
	cpu->registers[CPSR] |= 1<<7;
	cpu->i_cycles+=1;
	arm7_set_thumb_bit(cpu,false); 
	cpu->phased_op_id = ARM_PHASED_FILL_PIPE;
	cpu->phase=0;
  }
}

static inline bool arm7_check_cond_code(arm7_t* cpu, UINT32 opcode)
{
	UINT32 cond_code = ARM7_BFE(opcode, 28, 4);
	if (SB_LIKELY(cond_code == 0xE))return true;
	UINT32 cpsr = cpu->registers[CPSR];
	bool N = ARM7_BFE(cpsr, 31, 1);
	bool Z = ARM7_BFE(cpsr, 30, 1);
	bool C = ARM7_BFE(cpsr, 29, 1);
	bool V = ARM7_BFE(cpsr, 28, 1);
	switch (cond_code) {
		case 0x0: return  Z;				//EQ: Equal
		case 0x1: return !Z;				//NE: !Equal
		case 0x2: return  C;				//CS: Unsigned >=
		case 0x3: return !C;				//CC: Unsigned <
		case 0x4: return  N;				//MI: Negative
		case 0x5: return !N;				//PL: Positive or Zero
		case 0x6: return  V;				//VS: Overflow
		case 0x7: return !V;				//VC: No Overflow
		case 0x8: return  C && !Z;			//HI: Unsigned >
		case 0x9: return !C ||  Z;			//LS: Unsigned <=
		case 0xA: return  N == V;			//GE: Signed >=
		case 0xB: return  N != V;			//LT: Signed <  
		case 0xC: return !Z && (N == V);	//GT: Signed >  
		case 0xD: return  Z || (N != V);	//LE: Signed <= 
		case 0xE: return true;
		case 0xF: return true;
	};
	return false;
}


static inline void arm7_fill_pipeline(arm7_t* cpu)
{
	bool thumb = arm7_get_thumb_bit(cpu);
	if (thumb) {
		cpu->registers[PC] &= ~1;
		cpu->prefetch_opcode[cpu->phase] = cpu->read16_seq(cpu->user_data, cpu->registers[PC] + 2 * cpu->phase, cpu->phase != 0);
	} else {
		cpu->registers[PC] &= ~3;
		cpu->prefetch_opcode[cpu->phase] = cpu->read32_seq(cpu->user_data, cpu->registers[PC] + 4 * cpu->phase, cpu->phase != 0);
	}
	++cpu->phase;
	if (cpu->phase != 2)
		return;
	cpu->phase = 0;
	cpu->phased_op_id = 0;
	cpu->prefetch_pc = cpu->registers[PC];
	cpu->next_fetch_sequential = true;
}

static inline bool arm7_run_phased_opcode(arm7_t* cpu)
{
	switch (cpu->phased_op_id) {
		case ARM_PHASED_NONE:
			return true;
		case ARM_PHASED_FILL_PIPE:
			arm7_fill_pipeline(cpu);
			break;
		case ARM_PHASED_BLOCK_TRANSFER:
			arm7_block_transfer(cpu, cpu->phased_opcode);
			break;
		default:
			cpu->phased_op_id = 0;
			return true;
	}
	return false;
}

static inline void arm7_exec_instruction(arm7_t* cpu)
{
	bool thumb = arm7_get_thumb_bit(cpu);
	if (SB_LIKELY(arm7_run_phased_opcode(cpu))) {
		if (SB_UNLIKELY(cpu->wait_for_interrupt)) {
			cpu->i_cycles += 1;
			return;
		}
		cpu->next_fetch_sequential = true;
		UINT32 opcode = cpu->prefetch_opcode[0];
		cpu->prefetch_opcode[0] = cpu->prefetch_opcode[1];
		cpu->prefetch_opcode[1] = cpu->prefetch_opcode[2];
		if (thumb == false) {
			cpu->registers[PC] += 4;
			cpu->prefetch_pc = cpu->registers[PC];
			if (SB_LIKELY(arm7_check_cond_code(cpu, opcode))) {
				UINT32 key = ((opcode >> 4) & 0xf) | ((opcode >> 16) & 0xff0);
				arm7_lookup_table[key](cpu, opcode);
			}
		} else {
			cpu->registers[PC] += 2;
			cpu->prefetch_pc = cpu->registers[PC];
			UINT32 key = ((opcode >> 8) & 0xff);
			arm7t_lookup_table[key](cpu, opcode);
		}
		if (SB_UNLIKELY(cpu->step_instructions)) {
			--cpu->step_instructions;
			if (cpu->step_instructions == 0) {
				if (cpu->trigger_breakpoint)
					cpu->trigger_breakpoint(cpu->user_data);
			}
		}
	}
	if (SB_UNLIKELY(cpu->phased_op_id))
		return;
	if (thumb == false) {
		if (SB_LIKELY(cpu->prefetch_pc == cpu->registers[PC]))
			cpu->prefetch_opcode[2] = cpu->read32_seq(cpu->user_data, cpu->registers[PC] + 8, cpu->next_fetch_sequential);
		else
			cpu->phased_op_id = ARM_PHASED_FILL_PIPE;
	} else {
		if (SB_LIKELY(cpu->prefetch_pc == cpu->registers[PC]))
			cpu->prefetch_opcode[2] = cpu->read16_seq(cpu->user_data, cpu->registers[PC] + 4, cpu->next_fetch_sequential);
		else
			cpu->phased_op_id = ARM_PHASED_FILL_PIPE;
	}
}




static inline UINT32 arm7_rotr(UINT32 value, UINT32 rotate)
{
	return ((UINT64)value >> (rotate & 31)) | ((UINT64)value << (32 - (rotate & 31)));
}

static inline UINT32 arm7_load_shift_reg(arm7_t* arm, UINT32 opcode, INT32* carry)
{
	UINT32 value = arm7_reg_read(arm, ARM7_BFE(opcode, 0, 4));
	UINT32 shift_value = 0;
	if (ARM7_BFE(opcode, 4, 1) == true) {
		INT32 rs = ARM7_BFE(opcode, 8, 4);
		shift_value = arm7_reg_read(arm, rs);
	} else {
		shift_value = ARM7_BFE(opcode, 7, 5);
	}
	return arm7_shift(arm, opcode, value, shift_value, carry);
}

static inline UINT32 arm7_shift(arm7_t* arm, UINT32 opcode, UINT64 value, UINT32 shift_value, INT32* carry)
{
	INT32 shift_type = ARM7_BFE(opcode, 5, 2);
	// Shift value of 0 has special behavior from a register: 
	// If this byte is zero, the unchanged contents of Rm will be used as the second operand,
	// and the old value of the CPSR C flag will be passed on as the shifter carry output.
	if (shift_value == 0 && (ARM7_BFE(opcode, 4, 1) || shift_type == 0)) {
		*carry = -1;
		return value;
	}
	switch (shift_type) {
		case 0:
			if (shift_value > 32) {
				*carry = 0;
				value = 0;
			} else {
				*carry = shift_value == 0 ? -1 : ARM7_BFE(value, 32 - shift_value, 1);
				value = value << shift_value;
			}
			break;
		case 1:
			if (shift_value > 32) {
				*carry = 0;
				value = 0;
			} else {
				if (shift_value == 0) { shift_value = 32; }
				*carry = ARM7_BFE(value, shift_value - 1, 1);
				value = value >> shift_value;
			}
			break;
		case 2:
			if (shift_value > 32) {
				bool b31 = ARM7_BFE(value, 31, 1);
				value = b31 ? 0xffffffff : 0;
				*carry = b31;
			} else {
				if (shift_value == 0) { shift_value = 32; }
				*carry = ARM7_BFE(value, shift_value - 1, 1);
				value = (INT64)((INT32)value) >> shift_value;
			}
			break;
		case 3:
			if (shift_value == 0) {
				UINT32 cpsr = arm->registers[CPSR];
				INT32 C = ARM7_BFE(cpsr, 29, 1);
				//Rotate Extended (RRX)
				*carry = ARM7_BFE(value, 0, 1); value = (value >> 1) | (C << 31);

			} else {
				//Rotate
				value = arm7_rotr(value, shift_value); *carry = ARM7_BFE(value, 31, 1);
			}
			break;
	}
	return value;
}

static inline void arm7_data_processing(arm7_t* cpu, UINT32 opcode)
{
	// If it's used as anything but the shift amount in an operation with a register-specified shift, r15 will be PC + 12
	// I.e. add r0, r15, r15, lsl r15 would set r0 to PC + 12 + ((PC + 12) << (PC + 8))
	UINT64 Rd = ARM7_BFE(opcode, 12, 4);
	INT32 S = ARM7_BFE(opcode, 20, 1);
	INT32 op = ARM7_BFE(opcode, 21, 4);
	INT32 r15_off = 4;
	// Load Second Operand
	UINT64 Rm = 0;
	INT32 barrel_shifter_carry = -1;
	if (opcode & ((1 << 25) | (0xff0))) {
		INT32 I = ARM7_BFE(opcode, 25, 1);
		if (I) {
			UINT32 imm = ARM7_BFE(opcode, 0, 8);
			UINT32 rot = ARM7_BFE(opcode, 8, 4) * 2;
			Rm = arm7_rotr(imm, ARM7_BFE(opcode, 8, 4) * 2);
			//C is preserved when rot ==0 
			barrel_shifter_carry = rot == 0 ? -1 : ARM7_BFE(Rm, 31, 1);
		} else {
			UINT32 shift_value = 0;
			if (ARM7_BFE(opcode, 4, 1)) {
				INT32 rs = ARM7_BFE(opcode, 8, 4);
				// Only the first byte is used
				shift_value = arm7_reg_read_r15_adj(cpu, rs, r15_off) & 0xff;
				r15_off += 4; //Using r15 for a shift adds 4 cycles
				cpu->i_cycles += 1;
			} else shift_value = ARM7_BFE(opcode, 7, 5);
			UINT32 value = arm7_reg_read_r15_adj(cpu, ARM7_BFE(opcode, 0, 4), r15_off);
			Rm = arm7_shift(cpu, opcode, value, shift_value, &barrel_shifter_carry);
		}
	} else
		Rm = arm7_reg_read_r15_adj(cpu, ARM7_BFE(opcode, 0, 4), r15_off);;

	UINT64 Rn = arm7_reg_read_r15_adj(cpu, ARM7_BFE(opcode, 16, 4), r15_off);

	UINT64 result = 0;
	// Perform main operation 
	switch (op) {
	/*AND*/ case 0:  arm7_reg_write(cpu, Rd, result = Rn & Rm);                                             break;
	/*EOR*/ case 1:  arm7_reg_write(cpu, Rd, result = Rn ^ Rm);                                             break;
	/*SUB*/ case 2:  arm7_reg_write(cpu, Rd, result = Rn - Rm);                                             break;
	/*RSB*/ case 3:  arm7_reg_write(cpu, Rd, result = Rm - Rn);                                             break;
	/*ADD*/ case 4:  arm7_reg_write(cpu, Rd, result = Rn + Rm);                                             break;
	/*ADC*/ case 5:  arm7_reg_write(cpu, Rd, result = Rn + Rm + ARM7_BFE(cpu->registers[CPSR], 29, 1));     break;
	/*SBC*/ case 6:  arm7_reg_write(cpu, Rd, result = Rn - Rm + ARM7_BFE(cpu->registers[CPSR], 29, 1) - 1); break;
	/*RSC*/ case 7:  arm7_reg_write(cpu, Rd, result = Rm - Rn + ARM7_BFE(cpu->registers[CPSR], 29, 1) - 1); break;
	/*TST*/ case 8:  result = Rn & Rm;                                                                      break;
	/*TEQ*/ case 9:  result = Rn ^ Rm;                                                                      break;
	/*CMP*/ case 10: result = Rn - Rm;                                                                      break;
	/*CMN*/ case 11: result = Rn + Rm;                                                                      break;
	/*ORR*/ case 12: arm7_reg_write(cpu, Rd, result = Rn | Rm);                                             break;
	/*MOV*/ case 13: arm7_reg_write(cpu, Rd, result = Rm);                                                  break;
	/*BIC*/ case 14: arm7_reg_write(cpu, Rd, result = Rn & ~Rm);                                            break;
	/*MVN*/ case 15: arm7_reg_write(cpu, Rd, result = ~Rm);                                                 break;
	}
	//Update flags
	if (S) {
		//Rd is not valid for TST, TEQ, CMP, or CMN
		{
			UINT32 cpsr = cpu->registers[CPSR];
			bool C = ARM7_BFE(cpsr, 29, 1);
			bool N = ARM7_BFE(result, 31, 1);
			bool Z = (result & 0xffffffff) == 0;
			bool V = ARM7_BFE(cpsr, 28, 1);

			switch (op) {
				// Logical Ops flags
			/*AND*/ case 0:
			/*EOR*/ case 1:
			/*TST*/ case 8:
			/*TEQ*/ case 9:
			/*ORR*/ case 12:
			/*MOV*/ case 13:
			/*BIC*/ case 14:
			/*MVN*/ case 15:
				C = barrel_shifter_carry == -1 ? C : barrel_shifter_carry;
				break;

			/*SUB*/ case 2:
			/*SBC*/ case 6:
			/*CMP*/ case 10:
				C = !ARM7_BFE(result, 32, 1);
				// if (Rn has a different sign as Rm and result has a differnt sign to Rn)
				V = (((Rn ^ Rm) & (Rn ^ result)) >> 31) & 1;
				break;

			/*RSB*/ case 3:
			/*RSC*/ case 7:
				C = !ARM7_BFE(result, 32, 1);
				// if (Rm has a different sign as Rn and result has a differnt sign to Rm)
				V = (((Rm ^ Rn) & (Rm ^ result)) >> 31) & 1;
				break;

			/*ADD*/ case 4:
			/*ADC*/ case 5:
			/*CMN*/ case 11:
				C = ARM7_BFE(result, 32, 1);
				// if (Rm has the same sign as Rn and result has a different sign to Rm)
				V = (((Rm ^ ~Rn) & (Rm ^ result)) >> 31) & 1;
				break;
			}
			cpsr &= 0x0fffffff;
			cpsr |= (N ? 1 : 0) << 31;
			cpsr |= (Z ? 1 : 0) << 30;
			cpsr |= (C ? 1 : 0) << 29;
			cpsr |= (V ? 1 : 0) << 28;
			cpu->registers[CPSR] = cpsr;
		}
		if (Rd == 15) {
			// When Rd is R15 and the S flag is set the result of the operation is placed in R15 
			// and the SPSR corresponding to the current mode is moved to the CPSR. This allows
			// state changes which atomically restore both PC and CPSR. This form of instruction
			// should not be used in User mode.
			cpu->registers[CPSR] = arm7_reg_read(cpu, SPSR);
		}
	}
}

static inline void arm7_multiply(arm7_t* cpu, UINT32 opcode)
{
	bool A = ARM7_BFE(opcode, 21, 1);
	bool S = ARM7_BFE(opcode, 20, 1);
	INT64 Rd = ARM7_BFE(opcode, 16, 4);
	INT64 Rn = arm7_reg_read(cpu, ARM7_BFE(opcode, 12, 4));
	INT64 Rs = arm7_reg_read(cpu, ARM7_BFE(opcode, 8, 4));
	INT64 Rm = arm7_reg_read(cpu, ARM7_BFE(opcode, 0, 4));

	if (SB_BFE(Rs, 8, 24) == 0 || SB_BFE(Rs, 8, 24) == 0x00ffffff)
		cpu->i_cycles += 1;
	else if (SB_BFE(Rs, 16, 16) == 0 || SB_BFE(Rs, 16, 16) == 0x0000ffff)
		cpu->i_cycles += 2;
	else if (SB_BFE(Rs, 24, 8) == 0 || SB_BFE(Rs, 24, 8) == 0x000000ff)
		cpu->i_cycles += 3;
	else
		cpu->i_cycles += 4;

	INT64 result = Rm * Rs;
	if (A) { result += Rn; cpu->i_cycles += 1; }

	arm7_reg_write(cpu, Rd, result);

	if (S) {
		UINT32 cpsr = cpu->registers[CPSR];
		bool N = ARM7_BFE(result, 31, 1);
		bool Z = (result & 0xffffffff) == 0;
		bool C = ARM7_BFE(cpsr, 29, 1);
		bool V = ARM7_BFE(cpsr, 28, 1);
		cpsr &= 0x0fffffff;
		cpsr |= (N ? 1u : 0u) << 31;
		cpsr |= (Z ? 1  : 0)  << 30;
		cpsr |= (C ? 1  : 0)  << 29;
		cpsr |= (V ? 1  : 0)  << 28;
		cpu->registers[CPSR] = cpsr;
	}
}

//SMULLxxx

static inline void arm7_multiply_long(arm7_t* cpu, UINT32 opcode)
{
	bool U = ARM7_BFE(opcode, 22, 1);
	bool A = ARM7_BFE(opcode, 21, 1);
	bool S = ARM7_BFE(opcode, 20, 1);
	INT64 RdHi = ARM7_BFE(opcode, 16, 4);
	INT64 RdLo = ARM7_BFE(opcode, 12, 4);
	INT64 Rs = arm7_reg_read(cpu, ARM7_BFE(opcode, 8, 4));
	INT64 Rm = arm7_reg_read(cpu, ARM7_BFE(opcode, 0, 4));

	INT64 RdHiLo = arm7_reg_read(cpu, RdHi);
	RdHiLo = (RdHiLo << 32) | arm7_reg_read(cpu, RdLo);

	if (U) {
		Rm = (INT32)Rm;
		Rs = (INT32)Rs;
		if (SB_BFE(Rs, 8, 24) == 0 || SB_BFE(Rs, 8, 24) == 0x00ffffff)
			cpu->i_cycles += 2;
		else if (SB_BFE(Rs, 16, 16) == 0 || SB_BFE(Rs, 16, 16) == 0x0000ffff)
			cpu->i_cycles += 3;
		else if (SB_BFE(Rs, 24, 8) == 0 || SB_BFE(Rs, 24, 8) == 0x000000ff)
			cpu->i_cycles += 4;
		else
			cpu->i_cycles += 5;
	} else {
		if (SB_BFE(Rs, 8, 24) == 0)
			cpu->i_cycles += 2;
		else if (SB_BFE(Rs, 16, 16) == 0)
			cpu->i_cycles += 3;
		else if (SB_BFE(Rs, 24, 8) == 0)
			cpu->i_cycles += 4;
		else
			cpu->i_cycles += 5;
	}


	INT64 result = Rm * Rs;
	if (A) { result += RdHiLo; cpu->i_cycles += 1; }

	arm7_reg_write(cpu, RdHi, result >> 32);
	arm7_reg_write(cpu, RdLo, result & 0xffffffff);

	if (S) {
		UINT32 cpsr = cpu->registers[CPSR];
		bool N = ARM7_BFE(result, 63, 1);
		bool Z = result == 0;
		bool C = ARM7_BFE(cpsr, 29, 1);
		bool V = ARM7_BFE(cpsr, 28, 1);
		cpsr &= 0x0fffffff;
		cpsr |= (N ? 1 : 0) << 31;
		cpsr |= (Z ? 1 : 0) << 30;
		cpsr |= (C ? 1 : 0) << 29;
		cpsr |= (V ? 1 : 0) << 28;
		cpu->registers[CPSR] = cpsr;
	}
}

static inline void arm7_single_data_swap(arm7_t* cpu, UINT32 opcode)
{
	bool B = ARM7_BFE(opcode, 22, 1);
	UINT32 addr = arm7_reg_read_r15_adj(cpu, ARM7_BFE(opcode, 16, 4), 4);
	UINT32 Rd = ARM7_BFE(opcode, 12, 4);
	UINT32 Rm = ARM7_BFE(opcode, 0, 4);
	// Load
	UINT32 read_data = B ? cpu->read8(cpu->user_data, addr) : arm7_rotr(cpu->read32(cpu->user_data, addr), (addr & 0x3) * 8);

	UINT32 store_data = arm7_reg_read_r15_adj(cpu, Rm, 8);
	if (B == 1)
		cpu->write8(cpu->user_data, addr, store_data);
	else
		cpu->write32(cpu->user_data, addr, store_data);

	arm7_reg_write(cpu, Rd, read_data);
	cpu->i_cycles += 1;
}

static inline void arm7_branch_exchange(arm7_t* cpu, UINT32 opcode)
{
	INT32 v = arm7_reg_read_r15_adj(cpu, ARM7_BFE(opcode, 0, 4), 4);
	bool thumb = (v & 1) == 1;
	if (thumb)
		cpu->registers[PC] = (v & ~1);
	else
		cpu->registers[PC] = (v & ~3);
	cpu->prefetch_pc = -1;
	arm7_set_thumb_bit(cpu, thumb);
}


static inline void arm7_half_word_transfer(arm7_t* cpu, UINT32 opcode)
{
	bool P   = ARM7_BFE(opcode, 24, 1);
	bool U   = ARM7_BFE(opcode, 23, 1);
	bool I   = ARM7_BFE(opcode, 22, 1);
	bool W   = ARM7_BFE(opcode, 21, 1);
	bool L   = ARM7_BFE(opcode, 20, 1);
	INT32 Rn = ARM7_BFE(opcode, 16, 4);

	bool S   = ARM7_BFE(opcode,  6, 1);
	bool H   = ARM7_BFE(opcode,  5, 1);

	INT32 offset = I == 0 ?
		arm7_reg_read(cpu, ARM7_BFE(opcode, 0, 4)) :
		((opcode >> 4) & 0xf0) | (opcode & 0xf);
	UINT64 Rd = ARM7_BFE(opcode, 12, 4);
	UINT32 addr = arm7_reg_read_r15_adj(cpu, Rn, 4);

	INT32 increment = U ? offset : -offset;
	if (P)
		addr += increment;
	// Store before writeback
	if (L == 0) {
		UINT32 data = arm7_reg_read_r15_adj(cpu, Rd, 8);
		if (H == 1)
			cpu->write16(cpu->user_data, addr, data);
		else
			cpu->write8(cpu->user_data, addr, data);
	}
	UINT32 write_back_addr = addr;
	if (!P) {
		write_back_addr += increment;
		W = true;
	}
	if (W)
		arm7_reg_write(cpu, Rn, write_back_addr);
	if (L == 1) {	// Load
		UINT32 data = H ? arm7_rotr(cpu->read16(cpu->user_data, addr), (addr & 0x1) * 8) : cpu->read8(cpu->user_data, addr);
		if (S) {
			data &= 0xffff;
			// Unaligned signed half words and signed byte loads sign extend the byte
			if (H && !(addr & 1)) {
				data |= 0xffff0000 * ARM7_BFE(data, 15, 1);
			} else
				data |= 0xffffff00 * ARM7_BFE(data,  7, 1);
		}
		arm7_reg_write(cpu, Rd, data);
		cpu->i_cycles += 1;
	}
}

static inline void arm7_single_word_transfer(arm7_t* cpu, UINT32 opcode)
{
	bool I   = ARM7_BFE(opcode, 25, 1);
	bool P   = ARM7_BFE(opcode, 24, 1);
	bool U   = ARM7_BFE(opcode, 23, 1);
	bool B   = ARM7_BFE(opcode, 22, 1);
	bool W   = ARM7_BFE(opcode, 21, 1);
	bool L   = ARM7_BFE(opcode, 20, 1);
	INT32 Rn = ARM7_BFE(opcode, 16, 4);
	INT32 carry;
	INT32 offset = I == 0 ? ARM7_BFE(opcode, 0, 12) :
		arm7_load_shift_reg(cpu, opcode, &carry);

	UINT64 Rd = ARM7_BFE(opcode, 12, 4);
	UINT32 addr = arm7_reg_read_r15_adj(cpu, Rn, 4);
	INT32 increment = U ? offset : -offset;

	if (P)
		addr += increment;

	// Store before write back
	if (L == 0) {
		UINT32 data = arm7_reg_read_r15_adj(cpu, Rd, 8);
		if (B == 1)
			cpu->write8(cpu->user_data, addr, data);
		else
			cpu->write32(cpu->user_data, addr, data);
	}

	//Write back address before load
	UINT32 write_back_addr = addr;
	if (!P) {
		write_back_addr += increment;
		W = true;
	}
	if (W)
		arm7_reg_write(cpu, Rn, write_back_addr);

	if (L == 1) {	// Load
		UINT32 data = B ? cpu->read8(cpu->user_data, addr) : arm7_rotr(cpu->read32(cpu->user_data, addr), (addr & 0x3) * 8);
		arm7_reg_write(cpu, Rd, data);
		cpu->i_cycles += 1;
	}
}



static inline void arm7_undefined(arm7_t* cpu, UINT32 opcode)
{
	bool thumb = arm7_get_thumb_bit(cpu);
	cpu->registers[R14_und] = cpu->registers[PC] - (thumb ? 0 : 4);
	cpu->registers[PC] = 0 + 0x4;
	UINT32 cpsr = cpu->registers[CPSR];
	cpu->registers[SPSR_und] = cpsr;
	//Update mode to supervisor and block irqs
	cpu->registers[CPSR] = (cpsr & 0xffffffE0) | 0x1b | 0x80;
	arm7_set_thumb_bit(cpu, false);
	printf("Unhandled Instruction Class (arm7_undefined) Opcode: %x PC:%08x\n", opcode, cpu->registers[R14_und]);
	cpu->i_cycles += 1;
}



static inline void arm7_block_transfer(arm7_t* cpu, UINT32 opcode)
{
	INT32 P       = ARM7_BFE(opcode, 24,  1);
	INT32 U       = ARM7_BFE(opcode, 23,  1);
	INT32 S       = ARM7_BFE(opcode, 22,  1);
	INT32 w       = ARM7_BFE(opcode, 21,  1);
	INT32 L       = ARM7_BFE(opcode, 20,  1);
	INT32 Rn      = ARM7_BFE(opcode, 16,  4);
	INT32 reglist = ARM7_BFE(opcode,  0, 16);

	// Examples pushing R1, R5, R7
	// P= 0(post) U = 0(dec)
	//   mem[Rn-8]   = R1
	//   mem[Rn-4] = R5
	//   mem[Rn-0] = R7
	//   Rn-=12

	// P= 0(post) U = 1(inc)
	//   mem[Rn]   = R1
	//   mem[Rn+4] = R5
	//   mem[Rn+8] = R7
	//   Rn+=12

	// P= 1(pre) U = 0(dec)
	//   mem[Rn-12]   = R1
	//   mem[Rn-8] = R5
	//   mem[Rn-4] = R7
	//   Rn-=12

	// P= 1(pre) U = 1(inc)
	//   mem[Rn+4]   = R1
	//   mem[Rn+8] = R5
	//   mem[Rn+12] = R7
	//   Rn+=12

	if (cpu->phase == 0) {
		INT32 addr = arm7_reg_read_r15_adj(cpu, Rn, 4);
		INT32 increment = U ? 4 : -4;
		INT32 num_regs = 0;
		for (INT32 i = 0; i < 16; ++i)
			if (ARM7_BFE(reglist, i, 1) == 1)
				num_regs += 1;
		INT32 base_addr = addr;
		if (reglist == 0) {
			// Handle Empty Rlist case: R15 loaded/stored (ARMv4 only), and Rb=Rb+/-40h (ARMv4-v5).
			reglist = 1 << 15;
			num_regs = 16;
		}
		if (!U)
			base_addr += (num_regs)*increment;
		addr = base_addr;
		if (U)
			base_addr += (num_regs)*increment;
		cpu->block.base_addr = base_addr;

		if (!(P ^ U))
			addr += 4;
		cpu->block.cycle = 0;
		cpu->block.addr = addr;
		// TODO: For some reason r15 is only offset by 4 in thumb mode.
		// Check if other people do this to. 
		cpu->block.r15_off = arm7_get_thumb_bit(cpu) ? 4 : 8;
		// Address are word aligned
		//addr&=~3;
		cpu->block.last_bank = -1;

	}
	bool user_bank_transfer = S && (!L || !SB_BFE(reglist, 15, 1));

	for (INT32 i = cpu->phase; i < 16; ++i) {
		//Writeback happens on second cycle
		//Todo, does post increment force writeback?

		if (ARM7_BFE(reglist, i, 1) == 0)
			continue;

		// When S is set the registers are read from the user bank
		INT32 reg_index = user_bank_transfer ? i : arm7_reg_index(cpu, i);
		//Store happens before writeback 
		INT32 a = cpu->block.addr;
		//Inexplicablly SRAM accesses are not DWORD aligned. GBA suite memory test can be used to verify this.
		if ((a & 0xfe000000) != 0x0e000000)
			a &= ~3;
		if (!L)
			cpu->write32(cpu->user_data, a, cpu->registers[reg_index] + (i == 15 ? cpu->block.r15_off : 0));

		//Writeback happens on second cycle
		if (++cpu->block.cycle == 1 && w) {
			arm7_reg_write(cpu, Rn, cpu->block.base_addr);
		}

		// R15 is stored at PC+12
		if (L) {
			INT32 bank = ARM7_BFE(a, 24, 8);
			cpu->registers[reg_index] = cpu->read32_seq(cpu->user_data, a, bank == cpu->block.last_bank);
			cpu->block.last_bank = bank;
		}

		cpu->block.addr += 4;

		// If the instruction is a LDM then SPSR_<mode> is transferred to CPSR at
		// the same time as R15 is loaded.
		if (L && S && i == 15) {
			cpu->registers[CPSR] = arm7_reg_read(cpu, SPSR);
		}
		cpu->phased_op_id = ARM_PHASED_BLOCK_TRANSFER;
		cpu->phased_opcode = opcode;
		cpu->phase = i + 1;
		return;
	}
	if (L)
		cpu->i_cycles += 1;
	cpu->phase = 0;
	cpu->phased_op_id = 0;
}


static inline void arm7_branch(arm7_t* cpu, UINT32 opcode)
{
	//Write Link Register if L=1
	if (ARM7_BFE(opcode, 24, 1))
		arm7_reg_write(cpu, LR, cpu->registers[PC]);
	//Decode V and sign extend
	INT32 v = ARM7_BFE(opcode, 0, 24);
	if (ARM7_BFE(v, 23, 1))
		v |= 0xff000000;
	//Shift left and take into account prefetch
	INT32  pc_off = (v << 2) + 4;
	cpu->registers[PC] += pc_off;
	cpu->prefetch_pc = -1;
}


static inline void arm7_coproc_data_transfer(arm7_t* cpu, UINT32 opcode)
{
	printf("Unhandled Instruction Class (arm7_coproc_data_transfer) Opcode: %x\n", opcode);
	if (cpu->trigger_breakpoint)
		cpu->trigger_breakpoint(cpu->user_data);
}

static inline void arm7_coproc_data_op(arm7_t* cpu, UINT32 opcode)
{
	printf("Unhandled Instruction Class (arm7_coproc_data_op) Opcode: %x\n", opcode);
	if (cpu->trigger_breakpoint)
		cpu->trigger_breakpoint(cpu->user_data);
}

static inline void arm7_coproc_reg_transfer(arm7_t* cpu, UINT32 opcode)
{
	INT32 coprocessor_opcode = SB_BFE(opcode, 21, 3);
	bool coprocessor_read    = SB_BFE(opcode, 20, 1);
	INT32 Cn = SB_BFE(opcode, 16, 4);
	INT32 Rd = SB_BFE(opcode, 12, 4);
	INT32 Pn = SB_BFE(opcode,  8, 4);
	INT32 Cp = SB_BFE(opcode,  5, 3);
	INT32 Cm = SB_BFE(opcode,  0, 4);
	if (coprocessor_read) {
		if (!cpu->coprocessor_read) {
			printf("Coprocessor Read Issued without bound coprocessor_read handler: %x\n", opcode);
			return;
		}
		UINT32 data = cpu->coprocessor_read(cpu->user_data, Pn, coprocessor_opcode, Cn, Cm, Cp);
		arm7_reg_write(cpu, Rd, data);
	} else {
		if (!cpu->coprocessor_write) {
			printf("Coprocessor Write Issued without bound coprocessor_write handler: %x\n", opcode);
			return;
		}
		UINT32 data = arm7_reg_read_r15_adj(cpu, Rd, 8);
		cpu->coprocessor_write(cpu->user_data, Pn, coprocessor_opcode, Cn, Cm, Cp, data);
	}
}

static inline void arm7_software_interrupt(arm7_t* cpu, UINT32 /*opcode*/)
{
	cpu->registers[R14_svc] = cpu->registers[PC];
	cpu->registers[PC] = 0 + 0x8;
	UINT32 cpsr = cpu->registers[CPSR];
	cpu->registers[SPSR_svc] = cpsr;
	//Update mode to supervisor and block irqs
	cpu->registers[CPSR] = (cpsr & 0xffffffE0) | 0x13 | 0x80;
	arm7_set_thumb_bit(cpu, false);
}

static inline void arm7_mrs(arm7_t* cpu, UINT32 opcode)
{
	INT32 P  = ARM7_BFE(opcode, 22, 1);
	INT32 Rd = ARM7_BFE(opcode, 12, 4);
	INT32 data = arm7_reg_read(cpu, P ? SPSR : CPSR);
	arm7_reg_write(cpu, Rd, data);
}

static inline void arm7_msr(arm7_t* cpu, UINT32 opcode)
{
	INT32 P          =  ARM7_BFE(opcode, 22, 1);
	INT32 I          =  ARM7_BFE(opcode, 25, 1);
	INT32 data = 0;
	INT32 dest_reg = P ? SPSR : CPSR;

	// Mask behavior from: https://problemkaputt.de/gbatek.htm#armopcodespsrtransfermrsmsr
	UINT32 mask = 0;
	mask |= 0xff000000 * ARM7_BFE(opcode, 19, 1);
	mask |= 0x00ff0000 * ARM7_BFE(opcode, 18, 1);
	mask |= 0x0000ff00 * ARM7_BFE(opcode, 17, 1);
	mask |= 0x000000ff * ARM7_BFE(opcode, 16, 1);

	INT32 mode = cpu->registers[CPSR] & 0x1f;

	// There is no SPSR in user or system mode
	if (P && (mode == 0x10 || mode == 0x1f))
		return;
	//User mode can only change the flags
	if (mode == 0x10)
		mask &= 0xf0000000;

	if (I) {
		INT32 imm = ARM7_BFE(opcode, 0, 8);
		INT32 rot = ARM7_BFE(opcode, 8, 4) * 2;
		data = arm7_rotr(imm, rot);
	} else
		data = arm7_reg_read(cpu, ARM7_BFE(opcode, 0, 4));

	INT32 old_data = arm7_reg_read(cpu, dest_reg);
	data &= mask;
	data |= old_data & ~mask;

	arm7_reg_write(cpu, dest_reg, data);
}

// Thumb Instruction Implementations
static inline void arm7t_mov_shift_reg(arm7_t* cpu, UINT32 opcode)
{
	UINT32 op     = ARM7_BFE(opcode, 11, 2);
	UINT32 offset = ARM7_BFE(opcode,  6, 5);
	UINT32 Rs     = ARM7_BFE(opcode,  3, 3);
	UINT32 Rd     = ARM7_BFE(opcode,  0, 3);

	opcode = (0xD << 21) | (1 << 20) | (Rd << 12) | (offset << 7) | (op << 5) | (Rs);
	arm7_data_processing(cpu, opcode);
}

static inline void arm7t_add_sub(arm7_t* cpu, UINT32 opcode)
{
	bool I =   ARM7_BFE(opcode, 10, 1);
	INT32 op = ARM7_BFE(opcode,  9, 1) ? /*Sub*/ 2 : /*Add*/ 4;
	INT32 Rn = ARM7_BFE(opcode,  6, 3);
	INT32 Rs = ARM7_BFE(opcode,  3, 3);
	INT32 Rd = ARM7_BFE(opcode,  0, 3);
	UINT32 arm_op = (I << 25) | (op << 21) | (1 << 20) | (Rs << 16) | (Rd << 12) | (Rn);
	arm7_data_processing(cpu, arm_op);
}

static inline void arm7t_mov_cmp_add_sub_imm(arm7_t* cpu, UINT32 opcode)
{
	INT32 op  = ARM7_BFE(opcode, 11, 2);
	INT32 Rd  = ARM7_BFE(opcode,  8, 3);
	INT32 imm = ARM7_BFE(opcode,  0, 8);
	op = (0x24AD >> (op * 4)) & 0xf;/*MOV*//*CMP*//*ADD*//*SUB*/
	UINT32 arm_op = (1 << 25) | (op << 21) | (1 << 20) | (Rd << 16) | (Rd << 12) | (imm);
	arm7_data_processing(cpu, arm_op);
}

static inline void arm7t_alu_op(arm7_t* cpu, UINT32 opcode)
{
	INT32 op = ARM7_BFE(opcode, 6, 4);
	INT32 Rs = ARM7_BFE(opcode, 3, 3);
	INT32 Rd = ARM7_BFE(opcode, 0, 3);
	if (op == 13) {
		UINT32 arm_op = (0xE << 28) | (1 << 20) | (Rd << 16) | (Rd << 8) | (9 << 4) | (Rs);
		arm7_multiply(cpu, arm_op);
		cpu->registers[CPSR] &= ~(1 << 29);
		return;
	}

	INT32 alu_op   = (0xfe0cba38d65ddd10ULL >> (op * 4)) & 0xf;
	INT32 shift_op = (0x0000000030021000ULL >> (op * 4)) & 0xf;
	INT32 Rn = (op == 9) ? Rs : Rd;

	UINT32 arm_op = (0xEu << 28) | (alu_op << 21) | (1 << 20) | (Rn << 16) | (Rd << 12) | (shift_op << 5);

	if (alu_op == 13)
		arm_op |= (Rs << 8) | (1 << 4) | Rd;	// Special case shifts
	else if (op == 9)
		arm_op |= 1 << 25;						// Special case NEG
	else
		arm_op |= Rs;
	arm7_data_processing(cpu, arm_op);
}

static inline void arm7t_hi_reg_op(arm7_t* cpu, UINT32 opcode)
{
	INT32 op = ARM7_BFE(opcode, 8, 2);
	INT32 H1 = ARM7_BFE(opcode, 7, 1);
	INT32 H2 = ARM7_BFE(opcode, 6, 1);
	INT32 Rs = ARM7_BFE(opcode, 3, 3);
	INT32 Rd = ARM7_BFE(opcode, 0, 3);

	Rs |= H2 << 3;
	Rd |= H1 << 3;

	if (op == 3) {
		// Only the Rs field is populated since that is all that is needed for
		// arm7_branch_exchange
		INT32 arm_op = Rs;
		arm7_branch_exchange(cpu, arm_op);
	} else {
		INT32 S = op == 1;
		INT32 op_mapping[3] = {/*Add*/4, /*CMP*/10,/*MOV*/13 };
		op = op_mapping[op];
		//cccc 001o oooS nnnn dddd rrrr OOOO OOOO
		UINT32 arm_op = (op << 21) | (S << 20) | (op == 13 ? 0 : (Rd << 16)) | (Rd << 12) | (Rs << 0);
		arm7_data_processing(cpu, arm_op);
	}
}



static inline void arm7t_pc_rel_ldst(arm7_t* cpu, UINT32 opcode)
{
	INT32  offset = ARM7_BFE(opcode, 0, 8) * 4;
	INT32  Rd     = ARM7_BFE(opcode, 8, 3);
	UINT32 addr   = (cpu->registers[PC] + offset + 2) & (~3);
	UINT32 data   =  cpu->read32(cpu->user_data, addr);
	arm7_reg_write(cpu, Rd, data);
	cpu->i_cycles++;
}

static inline void arm7t_reg_off_ldst(arm7_t* cpu, UINT32 opcode)
{
	bool  B  = ARM7_BFE(opcode, 10, 1);
	bool  L  = ARM7_BFE(opcode, 11, 1);
	INT32 Ro = ARM7_BFE(opcode,  6, 3);
	INT32 Rb = ARM7_BFE(opcode,  3, 3);
	INT32 Rd = ARM7_BFE(opcode,  0, 3);

	INT32 r15_off = 2;
	Ro = arm7_reg_read_r15_adj(cpu, Ro, r15_off);
	Rb = arm7_reg_read_r15_adj(cpu, Rb, r15_off);

	UINT32 addr = Ro + Rb;
	// Store before write back
	if (L == 0) {
		UINT32 data = arm7_reg_read_r15_adj(cpu, Rd, r15_off);
		if (B == 1)
			cpu->write8( cpu->user_data, addr, data);
		else
			cpu->write32(cpu->user_data, addr, data);
	} else { // Load
		UINT32 data = B ? cpu->read8(cpu->user_data, addr) : arm7_rotr(cpu->read32(cpu->user_data, addr), (addr & 0x3) * 8);
		arm7_reg_write(cpu, Rd, data);
		cpu->i_cycles++;
	}
}


static inline void arm7t_ldst_bh(arm7_t* cpu, UINT32 opcode)
{
	INT32 op = ARM7_BFE(opcode, 10, 2);
	INT32 Ro = ARM7_BFE(opcode,  6, 3);
	INT32 Rb = ARM7_BFE(opcode,  3, 3);
	INT32 Rd = ARM7_BFE(opcode,  0, 3);

	INT32 r15_off = 2;
	Ro = arm7_reg_read_r15_adj(cpu, Ro, r15_off);
	Rb = arm7_reg_read_r15_adj(cpu, Rb, r15_off);

	UINT32 addr = Ro + Rb;

	UINT32 data;
	switch (op) {
		case 0: //Store Halfword
			data = arm7_reg_read_r15_adj(cpu, Rd, r15_off);
			break;
		case 1: //Load Sign Extended Byte
			data = cpu->read8(cpu->user_data, addr);
			cpu->i_cycles++;
			if (ARM7_BFE(data, 7, 1))
				data |= 0xffffff00;
			break;
		case 2: //Load Halfword
			data = arm7_rotr(cpu->read16(cpu->user_data, addr), (addr & 0x1) * 8);
			cpu->i_cycles++;
			break;
		case 3: //Load Sign Extended Half
			data = arm7_rotr(cpu->read16(cpu->user_data, addr), (addr & 0x1) * 8) & 0xffff;
			cpu->i_cycles++;
			//Unaligned halfwords sign extend the byte
			if ((addr & 1) && ARM7_BFE(data, 7, 1))
				data |= 0xffffff00;
			else if (ARM7_BFE(data, 15, 1))
				data |= 0xffff0000;
			break;
	}
	if (op == 0)
		cpu->write16(cpu->user_data, addr, data);
	else
		arm7_reg_write(cpu, Rd, data);
}

static inline void arm7t_imm_off_ldst(arm7_t* cpu, UINT32 opcode) {
	bool  B      = ARM7_BFE(opcode, 12, 1);
	bool  L      = ARM7_BFE(opcode, 11, 1);
	INT32 offset = ARM7_BFE(opcode,  6, 5);

	UINT32 Rd    = ARM7_BFE(opcode,  0, 3);
	UINT32 Rb    = ARM7_BFE(opcode,  3, 3);
	UINT32 addr  = arm7_reg_read_r15_adj(cpu, Rb, 4);
	//Offset is in 4B increments for word loads
	if (!B)
		offset *= 4;
	addr += offset;
	if (L == 0) {	// Store
		UINT32 data = arm7_reg_read_r15_adj(cpu, Rd, 8);
		if (B == 1)
			cpu->write8( cpu->user_data, addr, data);
		else
			cpu->write32(cpu->user_data, addr, data);
	} else {		// Load
		UINT32 data = B ? cpu->read8(cpu->user_data, addr) : arm7_rotr(cpu->read32(cpu->user_data, addr), (addr & 0x3) * 8);
		cpu->i_cycles++;
		arm7_reg_write(cpu, Rd, data);
	}
}


static inline void arm7t_imm_off_ldst_bh(arm7_t* cpu, UINT32 opcode)
{
	bool  L      = ARM7_BFE(opcode, 11, 1);
	INT32 offset = ARM7_BFE(opcode,  6, 5);

	UINT32 Rd    = ARM7_BFE(opcode,  0, 3);
	UINT32 addr  = arm7_reg_read_r15_adj(cpu, ARM7_BFE(opcode, 3, 3), 4);

	addr += offset * 2;
	UINT32 data = 0;
	if (L == 0) {	// Store
		data = arm7_reg_read_r15_adj(cpu, Rd, 8);
		cpu->write16(cpu->user_data, addr, data);
	} else {		// Load
		data = arm7_rotr(cpu->read16(cpu->user_data, addr), (addr & 0x1) * 8);
		arm7_reg_write(cpu, Rd, data);
		cpu->i_cycles++;
	}
}

static inline void arm7t_stack_off_ldst(arm7_t* cpu, UINT32 opcode)
{
	bool   L      = ARM7_BFE(opcode, 11, 1);
	UINT64 Rd     = ARM7_BFE(opcode,  8, 3);
	INT32  offset = ARM7_BFE(opcode,  0, 8);
	UINT32 addr   = arm7_reg_read(cpu, 13);

	addr += offset * 4;
	UINT32 data;
	if (L == 0) {	// Store
		data = arm7_reg_read_r15_adj(cpu, Rd, 8);
		cpu->write32(cpu->user_data, addr, data);
	} else {		// Load
		data = arm7_rotr(cpu->read32(cpu->user_data, addr), (addr & 0x3) * 8);
		arm7_reg_write(cpu, Rd, data);
		cpu->i_cycles++;
	}
}


static inline void arm7t_load_addr(arm7_t* cpu, UINT32 opcode)
{
	bool  SP  = ARM7_BFE(opcode, 11, 1);
	INT32 Rd  = ARM7_BFE(opcode,  8, 3);
	INT32 imm = ARM7_BFE(opcode,  0, 8) * 4;

	UINT32 v = arm7_reg_read_r15_adj(cpu, SP ? 13 : 15, 4);
	if (!SP)
		v &= ~3; //Bit 1 of PC always read as 0
	v += imm;
	arm7_reg_write(cpu, Rd, v);
}

static inline void arm7t_add_off_sp(arm7_t* cpu, UINT32 opcode)
{
	INT32 offset = ARM7_BFE(opcode, 0, 7) * 4;
	INT32 sign   = ARM7_BFE(opcode, 7, 1);
	if (sign)
		offset = -offset;
	UINT32 value = arm7_reg_read(cpu, 13);
	arm7_reg_write(cpu, 13, value + offset);
}

static inline void arm7t_push_pop_reg(arm7_t* cpu, UINT32 opcode)
{
	bool   push_or_pop   = ARM7_BFE(opcode, 11, 1);
	bool   include_pc_lr = ARM7_BFE(opcode,  8, 1);
	UINT32 r_list        = ARM7_BFE(opcode,  0, 8);
	INT32 P = !push_or_pop;
	INT32 W = 1;
	INT32 U =  push_or_pop;

	UINT32 arm_op = (0xe << 28) | (4 << 25) | (P << 24) | (U << 23) | (W << 21) | (push_or_pop << 20) | (13 << 16) | r_list;
	if (include_pc_lr)
		arm_op |= push_or_pop ? 0x8000 : 0x4000;
	arm7_block_transfer(cpu, arm_op);
}


static inline void arm7t_mult_ldst(arm7_t* cpu, UINT32 opcode)
{
	bool   write_or_read = ARM7_BFE(opcode, 11, 1);
	INT32  Rb            = ARM7_BFE(opcode,  8, 3);
	UINT32 r_list        = ARM7_BFE(opcode,  0, 8);

	INT32 P = 0;
	INT32 U = 1;
	INT32 W = 1;
	// Maps to LDMIA, STMIA opcode
	UINT32 arm_op = (0xe << 28) | (4 << 25) | (P << 24) | (U << 23) | (W << 21) | (write_or_read << 20) | (Rb << 16) | r_list;
	arm7_block_transfer(cpu, arm_op);
}


static inline void arm7t_cond_branch(arm7_t* cpu, UINT32 opcode)
{
	INT32 cond  = ARM7_BFE(opcode, 8, 4);
	INT32 s_off = ARM7_BFE(opcode, 0, 8);
	if (ARM7_BFE(s_off, 7, 1))
		s_off |= 0xFFFFFF00;
	//ARM equv: cccc 1010 OOOO OOOO OOOO OOOO OOOO OOOO
	UINT32 arm_op = (cond << 28) | (0xA << 24);
	if (arm7_check_cond_code(cpu, arm_op)) {
		cpu->registers[PC] += s_off * 2 + 2;
		cpu->prefetch_pc = -1;
	}
}

static inline void arm7t_soft_interrupt(arm7_t* cpu, UINT32 opcode)
{
	arm7_software_interrupt(cpu, opcode);
}

static inline void arm7t_branch(arm7_t* cpu, UINT32 opcode) {
	INT32 offset = ARM7_BFE(opcode, 0, 11) << 1;
	if (ARM7_BFE(offset, 11, 1))
		offset |= 0xfffff000;
	cpu->registers[PC] += offset + 2;
	cpu->prefetch_pc = -1;
}

static inline void arm7t_long_branch_link(arm7_t* cpu, UINT32 opcode)
{
	bool   H            = ARM7_BFE(opcode, 11,  1);
	INT32  offset       = ARM7_BFE(opcode,  0, 11);
	INT32  thumb_branch = ARM7_BFE(opcode, 12,  1);
	UINT32 link_reg = arm7_reg_read(cpu, LR);
	// TODO: Is this +4 supposed to be +2 ARM7TDMI page 5-40
	if (H == 0) {
		offset <<= 12;
		if (offset & 0x400000)
			offset |= 0xFF800000;
		arm7_reg_write(cpu, LR, cpu->registers[PC] + offset + 2);
	} else {
		link_reg += (offset << 1);
		UINT32 pc = cpu->registers[PC];
		cpu->registers[PC] = link_reg;
		arm7_set_thumb_bit(cpu, thumb_branch);
		arm7_reg_write(cpu, LR, (pc | 1));
		cpu->prefetch_pc = -1;
		if (!thumb_branch)
			arm7_set_thumb_bit(cpu, false);
	}
}

static inline void arm7t_unknown(arm7_t* cpu, UINT32 opcode)
{
	bool thumb = arm7_get_thumb_bit(cpu);
	cpu->registers[R14_und] = cpu->registers[PC] - (thumb ? 0 : 4);
	cpu->registers[PC] = 0 + 0x4;
	UINT32 cpsr = cpu->registers[CPSR];
	cpu->registers[SPSR_und] = cpsr;
	//Update mode to supervisor and block irqs
	cpu->registers[CPSR] = (cpsr & 0xffffffE0) | 0x1b | 0x80;
	arm7_set_thumb_bit(cpu, false);
	printf("Unhandled Thumb Instruction Class: (arm7t_unknown) Opcode %x\n", opcode);
	printf("PC: %08x\n", cpu->registers[PC]);
	if (cpu->trigger_breakpoint)cpu->trigger_breakpoint(cpu->user_data);
}

#endif
