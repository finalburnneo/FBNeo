/*****************************************************************************
 *
 *   arm7core.c
 *   Portable ARM7TDMI Core Emulator
 *
 *   Copyright Steve Ellenoff, all rights reserved.
 *
 *   - This source code is released as freeware for non-commercial purposes.
 *   - You are free to use and redistribute this code in modified or
 *     unmodified form, provided you list me in the credits.
 *   - If you modify this source code, you must add a notice to each modified
 *     source file that it has been changed.  If you're a nice person, you
 *     will clearly mark each change too.  :)
 *   - If you wish to use this for commercial purposes, please contact me at
 *     sellenoff@hotmail.com
 *   - The author of this copywritten work reserves the right to change the
 *     terms of its usage and license at any time, including retroactively
 *   - This entire notice must remain in the source code.
 *
 *  This work is based on:
 *  #1) 'Atmel Corporation ARM7TDMI (Thumb) Datasheet - January 1999'
 *  #2) Arm 2/3/6 emulator By Bryan McPhail (bmcphail@tendril.co.uk) and Phil Stroffolino (MAME CORE 0.76)
 *  #3) Thumb support by Ryan Holtz
 *  #4) Additional Thumb support and bugfixes by R. Belmont
 *
 *****************************************************************************/

/******************************************************************************
 *  Notes:

    **This core comes from my AT91 cpu core contributed to PinMAME,
      but with all the AT91 specific junk removed,
      which leaves just the ARM7TDMI core itself. I further removed the CPU specific MAME stuff
      so you just have the actual ARM7 core itself, since many cpu's incorporate an ARM7 core, but add on
      many cpu specific functionality.

      Therefore, to use the core, you simpy include this file along with the .h file into your own cpu specific
      implementation, and therefore, this file shouldn't be compiled as part of your project directly.
      Additionally, you will need to include arm7exec.c in your cpu's execute routine.

      For better or for worse, the code itself is very much intact from it's arm 2/3/6 origins from
      Bryan & Phil's work. I contemplated merging it in, but thought the fact that the CPSR is
      no longer part of the PC was enough of a change to make it annoying to merge.
    **

    Coprocessor functions are heavily implementation specific, so callback handlers are used to allow the
    implementation to handle the functionality. Custom DASM handlers are included as well to allow the DASM
    output to be tailored to the co-proc implementation details.

    Todo:
    26 bit compatibility mode not implemented.
    Data Processing opcodes need cycle count adjustments (see page 194 of ARM7TDMI manual for instruction timing summary)
    Multi-emulated cpu support untested, but probably will not work too well, as no effort was made to code for more than 1.
    Could not find info on what the TEQP opcode is from page 44..
    I have no idea if user bank switching is right, as I don't fully understand it's use.
    Search for Todo: tags for remaining items not done.


    Differences from Arm 2/3 (6 also?)
    -Thumb instruction support
    -Full 32 bit address support
    -PC no longer contains CPSR information, CPSR is own register now
    -New register SPSR to store previous contents of CPSR (this register is banked in many modes)
    -New opcodes for CPSR transfer, Long Multiplication, Co-Processor support, and some others
    -User Bank Mode transfer using certain flags which were previously unallowed (LDM/STM with S Bit & R15)
    -New operation modes? (unconfirmed)

    Based heavily on arm core from MAME 0.76:
    *****************************************
    ARM 2/3/6 Emulation

    Todo:
    Software interrupts unverified (nothing uses them so far, but they should be ok)
    Timing - Currently very approximated, nothing relies on proper timing so far.
    IRQ timing not yet correct (again, nothing is affected by this so far).

    By Bryan McPhail (bmcphail@tendril.co.uk) and Phil Stroffolino
*****************************************************************************/

#include <stdarg.h>
//#include "deprecat.h"

#define ARM7_DEBUG_CORE 0
#define logerror	printf
#define fatalerror	logerror

#if 0
#define LOG(x) mame_printf_debug x
#else
#define LOG(x) logerror x
#endif

#define ARM7_INLINE	static inline

/* Prototypes */

// SJE: should these be inline? or are they too big to see any benefit?

static void HandleCoProcDO(UINT32 insn);
static void HandleCoProcRT(UINT32 insn);
static void HandleCoProcDT(UINT32 insn);
static void HandleHalfWordDT(UINT32 insn);
static void HandleSwap(UINT32 insn);
static void HandlePSRTransfer(UINT32 insn);
static void HandleALU(UINT32 insn);
static void HandleMul(UINT32 insn);
static void HandleUMulLong(UINT32 insn);
static void HandleSMulLong(UINT32 insn);
ARM7_INLINE void HandleBranch(UINT32 insn, bool h_bit);       // pretty short, so ARM7_INLINE should be ok
static void HandleMemSingle(UINT32 insn);
static void HandleMemBlock(UINT32 insn);
static UINT32 decodeShift(UINT32 insn, UINT32 *pCarry);
ARM7_INLINE void SwitchMode(int);
static void arm7_check_irq_state(void);

ARM7_INLINE void arm7_cpu_write32(UINT32 addr, UINT32 data);
ARM7_INLINE void arm7_cpu_write16(UINT32 addr, UINT16 data);
ARM7_INLINE void arm7_cpu_write8(UINT32 addr, UINT8 data);
ARM7_INLINE UINT32 arm7_cpu_read32(UINT32 addr);
ARM7_INLINE UINT16 arm7_cpu_read16(UINT32 addr);
ARM7_INLINE UINT8 arm7_cpu_read8(UINT32 addr);

UINT32 (*arm7_cpu_readop32_func_t)(UINT32 addr);
UINT32 (*arm7_cpu_readop16_func_t)(UINT32 addr);
UINT32 (*arm7_cpu_read32_func_t)(UINT32 addr);
void (*arm7_cpu_write32_func_t)(UINT32 addr, UINT32 data);
UINT16 (*arm7_cpu_read16_func_t)(UINT32 addr);
void (*arm7_cpu_write16_func_t)(UINT32 addr, UINT16 data);
UINT8 (*arm7_cpu_read8_func_t)(UINT32 addr);
void (*arm7_cpu_write8_func_t)(UINT32 addr, UINT8 data);

/* Static Vars */
// Note: for multi-cpu implementation, this approach won't work w/o modification
void((*arm7_coproc_do_callback)(UINT32, UINT32));    // holder for the co processor Data Operations Callback func.
UINT32((*arm7_coproc_rt_r_callback)(UINT32));   // holder for the co processor Register Transfer Read Callback func.
void((*arm7_coproc_rt_w_callback)(UINT32, UINT32));  // holder for the co processor Register Transfer Write Callback Callback func.
// holder for the co processor Data Transfer Read & Write Callback funcs
void (*arm7_coproc_dt_r_callback)(UINT32 insn, UINT32 *prn, UINT32 (*read32)(UINT32 addr));
void (*arm7_coproc_dt_w_callback)(UINT32 insn, UINT32 *prn, void (*write32)(UINT32 addr, UINT32 data));


/***************************************************************************
 * Default Memory Handlers
 ***************************************************************************/
ARM7_INLINE void arm7_cpu_write32(UINT32 addr, UINT32 data)
{
	addr &= ~3;
	Arm7WriteLong(addr, data);
}


ARM7_INLINE void arm7_cpu_write16(UINT32 addr, UINT16 data)
{
	addr &= ~1;
	Arm7WriteWord(addr, data);
}

ARM7_INLINE void arm7_cpu_write8(UINT32 addr, UINT8 data)
{
	Arm7WriteByte(addr, data);
}

ARM7_INLINE UINT32 arm7_cpu_read32(UINT32 addr)
{
    UINT32 result;

    if (addr & 3)
    {
	result = Arm7ReadLong(addr & ~3);
        result = (result >> (8 * (addr & 3))) | (result << (32 - (8 * (addr & 3))));
    }
    else
    {
        result = Arm7ReadLong(addr);
    }

    return result;
}

ARM7_INLINE UINT16 arm7_cpu_read16(UINT32 addr)
{
    UINT16 result;

    result = Arm7ReadWord(addr & ~1);

    if (addr & 1)
    {
        result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
    }

    return result;
}

ARM7_INLINE UINT8 arm7_cpu_read8(UINT32 addr)
{
	UINT8 result = Arm7ReadByte(addr);
    // Handle through normal 8 bit handler (for 32 bit cpu)
    return result;
}

ARM7_INLINE UINT32 arm7_cpu_readop32(UINT32 addr)
{
    UINT32 result;

    if (addr & 3)
    {
        result = Arm7FetchLong(addr & ~3);
        result = (result >> (8 * (addr & 3))) | (result << (32 - (8 * (addr & 3))));
    }
    else
    {
        result = Arm7FetchLong(addr);
    }

    return result;
}

ARM7_INLINE UINT32 arm7_cpu_readop16(UINT32 addr)
{
    UINT16 result;

    result = Arm7FetchWord(addr & ~1);

    if (addr & 1)
    {
        result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
    }

    return result;
}

ARM7_INLINE UINT8 arm7_read8_func(UINT32 addr) {
    if (arm7_cpu_read8_func_t != NULL) {
        return arm7_cpu_read8_func_t(addr);
    } else {
        return arm7_cpu_read8(addr);
    }
}

ARM7_INLINE UINT16 arm7_read16_func(UINT32 addr) {
    if (arm7_cpu_read16_func_t != NULL) {
        return arm7_cpu_read16_func_t(addr);
    } else {
        return arm7_cpu_read16(addr);
    }
}

ARM7_INLINE UINT32 arm7_read32_func(UINT32 addr) {
    if (arm7_cpu_read32_func_t != NULL) {
        return arm7_cpu_read32_func_t(addr);
    } else {
        return arm7_cpu_read32(addr);
    }
}

ARM7_INLINE void arm7_write8_func(UINT32 addr, UINT32 data) {
    if (arm7_cpu_write8_func_t != NULL) {
        arm7_cpu_write8_func_t(addr, data);
    } else {
        arm7_cpu_write8(addr, data);
    }
}

ARM7_INLINE void arm7_write16_func(UINT32 addr, UINT32 data) {
    if (arm7_cpu_write16_func_t != NULL) {
        arm7_cpu_write16_func_t(addr, data);
    } else {
        arm7_cpu_write16(addr, data);
    }
}

ARM7_INLINE void arm7_write32_func(UINT32 addr, UINT32 data) {
    if (arm7_cpu_write32_func_t != NULL) {
        arm7_cpu_write32_func_t(addr, data);
    } else {
        arm7_cpu_write32(addr, data);
    }
}

ARM7_INLINE UINT32 cpu_readop16(UINT32 addr) {
    if (arm7_cpu_readop16_func_t != NULL) {
        return arm7_cpu_readop16_func_t(addr);
    } else {
        return arm7_cpu_readop16(addr);
    }
}

ARM7_INLINE UINT32 cpu_readop32(UINT32 addr) {
    if (arm7_cpu_readop32_func_t != NULL) {
        return arm7_cpu_readop32_func_t(addr);
    } else {
        return arm7_cpu_readop32(addr);
    }
}

/***************************************************************************
 * arm946es Handlers
 ***************************************************************************/

ARM7_INLINE void RefreshDTCM() {
    if (cp15_control & (1<<16))
    {
        cp15_dtcm_base = (cp15_dtcm_reg & ~0xfff);
        cp15_dtcm_size = 512 << ((cp15_dtcm_reg & 0x3f) >> 1);
        cp15_dtcm_end = cp15_dtcm_base + cp15_dtcm_size;
        //printf("DTCM enabled: base %08x size %x\n", cp15_dtcm_base, cp15_dtcm_size);
    }
    else
    {
        cp15_dtcm_base = 0xffffffff;
        cp15_dtcm_size = cp15_dtcm_end = 0;
    }
}

ARM7_INLINE void RefreshITCM() {
    if (cp15_control & (1<<18))
    {
        cp15_itcm_base = 0; //(cp15_itcm_reg & ~0xfff);
        cp15_itcm_size = 512 << ((cp15_itcm_reg & 0x3f) >> 1);
        cp15_itcm_end = cp15_itcm_base + cp15_itcm_size;
        //printf("ITCM enabled: base %08x size %x\n", cp15_dtcm_base, cp15_dtcm_size);
    }
    else
    {
        cp15_itcm_base = 0xffffffff;
        cp15_itcm_size = cp15_itcm_end = 0;
    }
}


ARM7_INLINE UINT32 arm946es_rt_r_callback (UINT32 offset) {
    UINT32 opcode = offset;
    UINT8 cReg = ( opcode & INSN_COPRO_CREG ) >> INSN_COPRO_CREG_SHIFT;
    UINT8 op2 =  ( opcode & INSN_COPRO_OP2 )  >> INSN_COPRO_OP2_SHIFT;
    UINT8 op3 =    opcode & INSN_COPRO_OP3;
    UINT8 cpnum = (opcode & INSN_COPRO_CPNUM) >> INSN_COPRO_CPNUM_SHIFT;
    UINT32 data = 0;

    //printf("arm7946: read cpnum %d cReg %d op2 %d op3 %d (%x)\n", cpnum, cReg, op2, op3, opcode);
    if (cpnum == 15)
    {
        switch( cReg )
        {
            case 0:
                switch (op2)
                {
                    case 0: // chip ID
                        data = 0x41059461;
                        break;

                    case 1: // cache ID
                        data = 0x0f0d2112;
                        break;

                    case 2: // TCM size
                        data = (6 << 6) | (5 << 18);
                        break;
                }
                break;

            case 1:
                return cp15_control;
                break;

            case 9:
                if (op3 == 1)
                {
                    if (op2 == 0)
                    {
                        return cp15_dtcm_reg;
                    }
                    else
                    {
                        return cp15_itcm_reg;
                    }
                }
                break;
        }
    }
    return data;
}

ARM7_INLINE void arm946es_rt_w_callback (UINT32 offset, UINT32 data) {
    UINT32 opcode = offset;
    UINT8 cReg = ( opcode & INSN_COPRO_CREG ) >> INSN_COPRO_CREG_SHIFT;
    UINT8 op2 =  ( opcode & INSN_COPRO_OP2 )  >> INSN_COPRO_OP2_SHIFT;
    UINT8 op3 =    opcode & INSN_COPRO_OP3;
    UINT8 cpnum = (opcode & INSN_COPRO_CPNUM) >> INSN_COPRO_CPNUM_SHIFT;

//  printf("arm7946: copro %d write %x to cReg %d op2 %d op3 %d (mask %08x)\n", cpnum, data, cReg, op2, op3, mem_mask);

    if (cpnum == 15)
    {
        switch (cReg)
        {
            case 1: // control
                cp15_control = data;
                RefreshDTCM();
                RefreshITCM();
                break;

            case 2: // Protection Unit cacheability bits
                break;

            case 3: // write bufferability bits for PU
                break;

            case 5: // protection unit region controls
                break;

            case 6: // protection unit region controls 2
                break;

            case 7: // cache commands
                break;

            case 9: // cache lockdown & TCM controls
                if (op3 == 1)
                {
                    if (op2 == 0)
                    {
                        cp15_dtcm_reg = data;
                        RefreshDTCM();
                    }
                    else if (op2 == 1)
                    {
                        cp15_itcm_reg = data;
                        RefreshITCM();
                    }
                }
                break;
        }
    }
}

ARM7_INLINE void arm946es_cpu_write32(UINT32 addr, UINT32 data)
{
    addr &= ~3;
    if ((addr >= cp15_itcm_base) && (addr <= cp15_itcm_end))
    {
        UINT32 *wp = (UINT32*)&ITCM[addr&0x7fff];
        *wp = data;
        return;
    }
    else if ((addr >= cp15_dtcm_base) && (addr <= cp15_dtcm_end))
    {
        UINT32 *wp = (UINT32 *)&DTCM[addr&0x3fff];
        *wp = data;
        return;
    }
    
    Arm9WriteLong(addr, data);
}


ARM7_INLINE void arm946es_cpu_write16(UINT32 addr, UINT16 data)
{
    addr &= ~1;
    if ((addr >= cp15_itcm_base) && (addr <= cp15_itcm_end))
    {
        UINT16 *wp = (UINT16 *)&ITCM[addr&0x7fff];
        *wp = data;
        return;
    }
    else if ((addr >= cp15_dtcm_base) && (addr <= cp15_dtcm_end))
    {
        UINT16 *wp = (UINT16 *)&DTCM[addr&0x3fff];
        *wp = data;
        return;
    }
    
    Arm9WriteWord(addr, data);
}

ARM7_INLINE void arm946es_cpu_write8(UINT32 addr, UINT8 data)
{
    if ((addr >= cp15_itcm_base) && (addr <= cp15_itcm_end))
    {
        ITCM[addr&0x7fff] = data;
        return;
    }
    else if ((addr >= cp15_dtcm_base) && (addr <= cp15_dtcm_end))
    {
        DTCM[addr&0x3fff] = data;
        return;
    }
    
    Arm9WriteByte(addr, data);
}

ARM7_INLINE UINT32 arm946es_cpu_read32(UINT32 addr)
{
    UINT32 result;
    if ((addr >= cp15_itcm_base) && (addr <= cp15_itcm_end))
    {
        if (addr & 3)
        {
            UINT32 *wp = (UINT32 *)&ITCM[(addr & ~3)&0x7fff];
            result = *wp;
            result = (result >> (8 * (addr & 3))) | (result << (32 - (8 * (addr & 3))));
        }
        else
        {
            UINT32 *wp = (UINT32 *)&ITCM[addr&0x7fff];
            result = *wp;
        }
    }
    else if ((addr >= cp15_dtcm_base) && (addr <= cp15_dtcm_end))
    {
        if (addr & 3)
        {
            UINT32 *wp = (UINT32 *)&DTCM[(addr & ~3)&0x3fff];
            result = *wp;
            result = (result >> (8 * (addr & 3))) | (result << (32 - (8 * (addr & 3))));
        }
        else
        {
            UINT32 *wp = (UINT32 *)&DTCM[addr&0x3fff];
            result = *wp;
        }
    } else {
        if (addr & 3)
        {
            result = Arm9ReadLong(addr & ~3);
            result = (result >> (8 * (addr & 3))) | (result << (32 - (8 * (addr & 3))));
        }
        else
        {
            result = Arm9ReadLong(addr);
        }
    }
    return result;
}

ARM7_INLINE UINT16 arm946es_cpu_read16(UINT32 addr)
{
    addr &= ~1;

    if ((addr >= cp15_itcm_base) && (addr <= cp15_itcm_end))
    {
        UINT16* wp = (UINT16*)&ITCM[addr & 0x7fff];
        return *wp;
    }
    else if ((addr >= cp15_dtcm_base) && (addr <= cp15_dtcm_end))
    {
        UINT16* wp = (UINT16*)&DTCM[addr & 0x3fff];
        return *wp;
    }

    return Arm9ReadWord(addr);
}

ARM7_INLINE UINT8 arm946es_cpu_read8(UINT32 addr)
{
    if ((addr >= cp15_itcm_base) && (addr <= cp15_itcm_end))
    {
        return ITCM[addr & 0x7fff];
    }
    else if ((addr >= cp15_dtcm_base) && (addr <= cp15_dtcm_end))
    {
        return DTCM[addr & 0x3fff];
    }
    UINT8 result = Arm9ReadByte(addr);
    // Handle through normal 8 bit handler (for 32 bit cpu)
    return result;
}

ARM7_INLINE UINT32 arm946es_cpu_readop32(UINT32 addr)
{
    UINT32 result;

    if (addr & 3)
    {
        result = Arm9FetchLong(addr & ~3);
        result = (result >> (8 * (addr & 3))) | (result << (32 - (8 * (addr & 3))));
    }
    else
    {
        result = Arm9FetchLong(addr);
    }

    return result;
}

ARM7_INLINE UINT32 arm946es_cpu_readop16(UINT32 addr)
{
    UINT16 result;

    result = Arm9FetchWord(addr & ~1);

    if (addr & 1)
    {
        result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
    }

    return result;
}

/***************
 * helper funcs
 ***************/

// TODO LD:
//  - SIGN_BITS_DIFFER = THUMB_SIGN_BITS_DIFFER
//  - do while (0)
//  - HandleALUAddFlags = HandleThumbALUAddFlags except for PC incr
//  - HandleALUSubFlags = HandleThumbALUSubFlags except for PC incr

/* Set NZCV flags for ADDS / SUBS */
#define HandleALUAddFlags(rd, rn, op2)                                                \
  if (insn & INSN_S)                                                                  \
    SET_CPSR(((GET_CPSR & ~(N_MASK | Z_MASK | V_MASK | C_MASK))                       \
              | (((!SIGN_BITS_DIFFER(rn, op2)) && SIGN_BITS_DIFFER(rn, rd)) << V_BIT) \
              | (((~(rn)) < (op2)) << C_BIT)                                          \
              | HandleALUNZFlags(rd)));                                               \
  R15 += 4;

#define HandleThumbALUAddFlags(rd, rn, op2)                                                       \
    SET_CPSR(((GET_CPSR & ~(N_MASK | Z_MASK | V_MASK | C_MASK))                                   \
              | (((!THUMB_SIGN_BITS_DIFFER(rn, op2)) && THUMB_SIGN_BITS_DIFFER(rn, rd)) << V_BIT) \
              | (((~(rn)) < (op2)) << C_BIT)                                                      \
              | HandleALUNZFlags(rd)));                                                           \
  R15 += 2;

#define IsNeg(i) ((i) >> 31)
#define IsPos(i) ((~(i)) >> 31)

#define HandleALUSubFlags(rd, rn, op2)                                                                         \
  if (insn & INSN_S)                                                                                           \
    SET_CPSR(((GET_CPSR & ~(N_MASK | Z_MASK | V_MASK | C_MASK))                                                \
              | ((SIGN_BITS_DIFFER(rn, op2) && SIGN_BITS_DIFFER(rn, rd)) << V_BIT)                             \
              | (((IsNeg(rn) & IsPos(op2)) | (IsNeg(rn) & IsPos(rd)) | (IsPos(op2) & IsPos(rd))) ? C_MASK : 0) \
              | HandleALUNZFlags(rd)));                                                                        \
  R15 += 4;

#define HandleThumbALUSubFlags(rd, rn, op2)                                                                    \
    SET_CPSR(((GET_CPSR & ~(N_MASK | Z_MASK | V_MASK | C_MASK))                                                \
              | ((THUMB_SIGN_BITS_DIFFER(rn, op2) && THUMB_SIGN_BITS_DIFFER(rn, rd)) << V_BIT)                 \
              | (((IsNeg(rn) & IsPos(op2)) | (IsNeg(rn) & IsPos(rd)) | (IsPos(op2) & IsPos(rd))) ? C_MASK : 0) \
              | HandleALUNZFlags(rd)));                                                                        \
  R15 += 2;

/* Set NZC flags for logical operations. */

// This macro (which I didn't write) - doesn't make it obvious that the SIGN BIT = 31, just as the N Bit does,
// therefore, N is set by default
#define HandleALUNZFlags(rd)               \
  (((rd) & SIGN_BIT) | ((!(rd)) << Z_BIT))


// Long ALU Functions use bit 63
#define HandleLongALUNZFlags(rd)                            \
  ((((rd) & ((UINT64)1 << 63)) >> 32) | ((!(rd)) << Z_BIT))

#define HandleALULogicalFlags(rd, sc)                  \
  if (insn & INSN_S)                                   \
    SET_CPSR(((GET_CPSR & ~(N_MASK | Z_MASK | C_MASK)) \
              | HandleALUNZFlags(rd)                   \
              | (((sc) != 0) << C_BIT)));              \
  R15 += 4;

// convert cpsr mode num into to text
static const char modetext[ARM7_NUM_MODES][5] = {
    "USER", "FIRQ", "IRQ",  "SVC", "ILL1", "ILL2", "ILL3", "ABT",
    "ILL4", "ILL5", "ILL6", "UND", "ILL7", "ILL8", "ILL9", "SYS"
};

/*static const char *GetModeText(int cpsr)
{
    return modetext[cpsr & MODE_FLAG];
}*/

// used to be functions, but no longer a need, so we'll use define for better speed.
#define GetRegister(rIndex)        ARM7REG(sRegisterTable[GET_MODE][rIndex])
#define SetRegister(rIndex, value) ARM7REG(sRegisterTable[GET_MODE][rIndex]) = value
#define GetModeRegister(model, rIndex)        ARM7REG(sRegisterTable[model][rIndex])
#define SetModeRegister(model, rIndex, value) ARM7REG(sRegisterTable[model][rIndex]) = value

ARM7_INLINE void set_cpsr(UINT32 val)
{
    {
        val |= 0x10; // force valid mode
    }
    GET_CPSR = val;
}

// I could prob. convert to macro, but Switchmode shouldn't occur that often in emulated code..
ARM7_INLINE void SwitchMode(int cpsr_mode_val)
{
    UINT32 cspr = GET_CPSR & ~MODE_FLAG;
    SET_CPSR(cspr | cpsr_mode_val);
}

INT64 saturate_qbit_overflow(INT64 res) {
    if (res > 2147483647)   // INT32_MAX
	{   // overflow high? saturate and set Q
		res = 2147483647;
		SET_CPSR(GET_CPSR | Q_MASK);
	}
	else if (res < (-2147483647-1)) // INT32_MIN
	{   // overflow low? saturate and set Q
		res = (-2147483647-1);
		SET_CPSR(GET_CPSR | Q_MASK);
	}

	return res;
}

/* Decodes an Op2-style shifted-register form.  If @carry@ is non-zero the
 * shifter carry output will manifest itself as @*carry == 0@ for carry clear
 * and @*carry != 0@ for carry set.

   SJE: Rules:
   IF RC = 256, Result = no shift.
   LSL   0   = Result = RM, Carry = Old Contents of CPSR C Bit
   LSL(0,31) = Result shifted, least significant bit is in carry out
   LSL  32   = Result of 0, Carry = Bit 0 of RM
   LSL >32   = Result of 0, Carry out 0
   LSR   0   = LSR 32 (see below)
   LSR  32   = Result of 0, Carry = Bit 31 of RM
   LSR >32   = Result of 0, Carry out 0
   ASR >=32  = ENTIRE Result = bit 31 of RM
   ROR  32   = Result = RM, Carry = Bit 31 of RM
   ROR >32   = Same result as ROR n-32 until amount in range of 1-32 then follow rules
*/

static UINT32 decodeShift(UINT32 insn, UINT32 *pCarry)
{
    UINT32 k  = (insn & INSN_OP2_SHIFT) >> INSN_OP2_SHIFT_SHIFT;  // Bits 11-7
    UINT32 rm = GET_REGISTER(insn & INSN_OP2_RM);
    UINT32 t  = (insn & INSN_OP2_SHIFT_TYPE) >> INSN_OP2_SHIFT_TYPE_SHIFT;

    if ((insn & INSN_OP2_RM) == 0xf) {
        // "If a register is used to specify the shift amount the PC will be 12 bytes ahead." (instead of 8)
        rm += t & 1 ? 12 : 8;
    }

    /* All shift types ending in 1 are Rk, not #k */
    if (t & 1)
    {
//      LOG(("%08x:  RegShift %02x %02x\n", R15, k >> 1, GET_REGISTER(k >> 1)));
#if ARM7_DEBUG_CORE
            if ((insn & 0x80) == 0x80)
                LOG(("%08x:  RegShift ERROR (p36)\n", R15));
#endif

        // see p35 for check on this
        //k = GET_REGISTER(k >> 1) & 0x1f;

        // Keep only the bottom 8 bits for a Register Shift
        k = GET_REGISTER(k >> 1) & 0xff;

        if (k == 0) /* Register shift by 0 is a no-op */
        {
//          LOG(("%08x:  NO-OP Regshift\n", R15));
            /* TODO this is wrong for at least ROR by reg with lower
             *      5 bits 0 but lower 8 bits non zero */
            if (pCarry)
                *pCarry = GET_CPSR & C_MASK;
            return rm;
        }
    }
    /* Decode the shift type and perform the shift */
    switch (t >> 1)
    {
    case 0:                     /* LSL */
        // LSL  32   = Result of 0, Carry = Bit 0 of RM
        // LSL >32   = Result of 0, Carry out 0
        if (k >= 32)
        {
            if (pCarry)
                *pCarry = (k == 32) ? rm & 1 : 0;
            return 0;
        }
        else
        {
            if (pCarry)
            {
            // LSL      0   = Result = RM, Carry = Old Contents of CPSR C Bit
            // LSL (0,31)   = Result shifted, least significant bit is in carry out
            *pCarry = k ? (rm & (1 << (32 - k))) : (GET_CPSR & C_MASK);
            }
            return k ? LSL(rm, k) : rm;
        }
        break;

    case 1:                         /* LSR */
        if (k == 0 || k == 32)
        {
            if (pCarry)
                *pCarry = rm & SIGN_BIT;
            return 0;
        }
        else if (k > 32)
        {
            if (pCarry)
                *pCarry = 0;
            return 0;
        }
        else
        {
            if (pCarry)
                *pCarry = (rm & (1 << (k - 1)));
            return LSR(rm, k);
        }
        break;

    case 2:                     /* ASR */
        if (k == 0 || k > 32)
            k = 32;

        if (pCarry)
            *pCarry = (rm & (1 << (k - 1)));
        if (k >= 32)
            return rm & SIGN_BIT ? 0xffffffffu : 0;
        else
        {
            if (rm & SIGN_BIT)
                return LSR(rm, k) | (0xffffffffu << (32 - k));
            else
                return LSR(rm, k);
        }
        break;

    case 3:                     /* ROR and RRX */
        if (k)
        {
            k &= 31;
            if (k)
            {
                if (pCarry)
                    *pCarry = rm & (1 << (k - 1));
                return ROR(rm, k);
            }
            else
            {
                if (pCarry)
                    *pCarry = rm & SIGN_BIT;
                return rm;
            }
        }
        else
        {
            /* RRX */
            if (pCarry)
                *pCarry = (rm & 1);
            return LSR(rm, 1) | ((GET_CPSR & C_MASK) << 2);
        }
        break;
    }

    LOG(("%08x: Decodeshift error\n", R15));
    return 0;
} /* decodeShift */


static int loadInc(UINT32 pat, UINT32 rbv, UINT32 s, int mode)
{
    int i, result;
    UINT32 data;

    result = 0;
    rbv &= ~3;
    for (i = 0; i < 16; i++)
    {
        if ((pat >> i) & 1)
        {
            if (!ARM7.pendingAbtD) // "Overwriting of registers stops when the abort happens."
            {
                data = READ32(rbv += 4);
                if (i == 15)
                {
                    if (s) /* Pull full contents from stack */
                        SET_MODE_REGISTER(mode, 15, data);
                    else if (MODE32) /* Pull only address, preserve mode & status flags */
                        SET_MODE_REGISTER(mode, 15, data);
                    else
                    {
                        SET_MODE_REGISTER(mode, 15, (GET_MODE_REGISTER(mode, 15) & ~0x03FFFFFC) | (data & 0x03FFFFFC));
                    }
                }
                else
                {
                    SET_MODE_REGISTER(mode, i, data);
                }
            }
            result++;
        }
    }
    return result;
}

static int loadDec(UINT32 pat, UINT32 rbv, UINT32 s, INT32 mode)
{
    int i, result;
    UINT32 data;

    result = 0;
    rbv &= ~3;
    for (i = 15; i >= 0; i--)
    {
        if ((pat >> i) & 1)
        {
            if (!ARM7.pendingAbtD) // "Overwriting of registers stops when the abort happens."
            {
                data = READ32(rbv -= 4);
                if (i == 15)
                {
                    if (s) /* Pull full contents from stack */
                        SET_MODE_REGISTER(mode, 15, data);
                    else if (MODE32) /* Pull only address, preserve mode & status flags */
                        SET_MODE_REGISTER(mode, 15, data);
                    else
                    {
                        SET_MODE_REGISTER(mode, 15, (GET_MODE_REGISTER(mode, 15) & ~0x03FFFFFC) | (data & 0x03FFFFFC));
                    }
                }
                else
                {
                    SET_MODE_REGISTER(mode, i, data);
                }
            }
            result++;
        }
    }
    return result;
}

static int storeInc(UINT32 pat, UINT32 rbv, INT32 mode)
{
    int i, result;

    result = 0;
    for (i = 0; i < 16; i++)
    {
        if ((pat >> i) & 1)
        {
#if ARM7_DEBUG_CORE
            if (i == 15) /* R15 is plus 12 from address of STM */
                LOG(("%08x: StoreInc on R15\n", R15));
#endif
            WRITE32(rbv += 4, GET_MODE_REGISTER(mode, i));
            result++;
        }
    }
    return result;
} /* storeInc */

static int storeDec(UINT32 pat, UINT32 rbv, INT32 mode)
{
    // pre-count the # of registers being stored
    int const result = population_count_32(pat & 0x0000ffff);

    // adjust starting address
    rbv -= (result << 2);

    for (int i = 0; i <= 15; i++)
    {
        if ((pat >> i) & 1)
        {
#if ARM7_DEBUG_CORE
            if (i == 15) /* R15 is plus 12 from address of STM */
                LOG(("%08x: StoreDec on R15\n", R15));
#endif
            WRITE32(rbv, GET_MODE_REGISTER(mode, i));
            rbv += 4;
        }
    }
    return result;
} /* storeDec */

/***************************************************************************
 *                            Main CPU Funcs
 ***************************************************************************/

// CPU INIT
#if 0
static void arm7_core_init(const char *cpuname, int index)
{
    state_save_register_item_array(cpuname, index, ARM7.sArmRegister);
    state_save_register_item(cpuname, index, ARM7.pendingIrq);
    state_save_register_item(cpuname, index, ARM7.pendingFiq);
    state_save_register_item(cpuname, index, ARM7.pendingAbtD);
    state_save_register_item(cpuname, index, ARM7.pendingAbtP);
    state_save_register_item(cpuname, index, ARM7.pendingUnd);
    state_save_register_item(cpuname, index, ARM7.pendingSwi);
}
#endif

// CPU RESET
static void arm7_core_reset(void)
{
    memset(&ARM7, 0, sizeof(ARM7));

    /* start up in SVC mode with interrupts disabled. */
    SwitchMode(eARM7_MODE_SVC);
    SET_CPSR(GET_CPSR | I_MASK | F_MASK | 0x10);
    R15 = 0;
//    change_pc(R15);
}

// Execute used to be here.. moved to separate file (arm7exec.c) to be included by cpu cores separately

// CPU CHECK IRQ STATE
// Note: couldn't find any exact cycle counts for most of these exceptions
static void arm7_check_irq_state(void)
{
    UINT32 cpsr = GET_CPSR;   /* save current CPSR */
    UINT32 pc = R15 + 4;      /* save old pc (already incremented in pipeline) */;

    /* Exception priorities:

        Reset
        Data abort
        FIRQ
        IRQ
        Prefetch abort
        Undefined instruction
        Software Interrupt
    */

    // Data Abort
    if (ARM7.pendingAbtD) {
        SwitchMode(eARM7_MODE_ABT);             /* Set ABT mode so PC is saved to correct R14 bank */
        SET_REGISTER(14, pc);                   /* save PC to R14 */
        SET_REGISTER(SPSR, cpsr);               /* Save current CPSR */
        SET_CPSR(GET_CPSR | I_MASK);            /* Mask IRQ */
        SET_CPSR(GET_CPSR & ~T_MASK);
        R15 = 0x10;                             /* IRQ Vector address */
//        change_pc(R15);
        ARM7.pendingAbtD = 0;
        return;
    }

    // FIQ
    if (ARM7.pendingFiq && (cpsr & F_MASK) == 0) {
        SwitchMode(eARM7_MODE_FIQ);             /* Set FIQ mode so PC is saved to correct R14 bank */
        SET_REGISTER(14, pc);                   /* save PC to R14 */
        SET_REGISTER(SPSR, cpsr);               /* Save current CPSR */
        SET_CPSR(GET_CPSR | I_MASK | F_MASK);   /* Mask both IRQ & FIQ */
        SET_CPSR(GET_CPSR & ~T_MASK);
        R15 = 0x1c;                             /* IRQ Vector address */
  //      change_pc(R15);
        return;
    }

    // IRQ
    if (ARM7.pendingIrq && (cpsr & I_MASK) == 0) {
        SwitchMode(eARM7_MODE_IRQ);             /* Set IRQ mode so PC is saved to correct R14 bank */
        SET_REGISTER(14, pc);                   /* save PC to R14 */
        SET_REGISTER(SPSR, cpsr);               /* Save current CPSR */
        SET_CPSR(GET_CPSR | I_MASK);            /* Mask IRQ */
        SET_CPSR(GET_CPSR & ~T_MASK);
        R15 = 0x18;                             /* IRQ Vector address */
  //      change_pc(R15);
        return;
    }

    // Prefetch Abort
    if (ARM7.pendingAbtP) {
        SwitchMode(eARM7_MODE_ABT);             /* Set ABT mode so PC is saved to correct R14 bank */
        SET_REGISTER(14, pc);                   /* save PC to R14 */
        SET_REGISTER(SPSR, cpsr);               /* Save current CPSR */
        SET_CPSR(GET_CPSR | I_MASK);            /* Mask IRQ */
        SET_CPSR(GET_CPSR & ~T_MASK);
        R15 = 0x0c;                             /* IRQ Vector address */
  //      change_pc(R15);
        ARM7.pendingAbtP = 0;
        return;
    }

    // Undefined instruction
    if (ARM7.pendingUnd) {
        SwitchMode(eARM7_MODE_UND);             /* Set UND mode so PC is saved to correct R14 bank */
        SET_REGISTER(14, pc);                   /* save PC to R14 */
        if (T_IS_SET(GET_CPSR))
        {
                SET_REGISTER(14, pc-2);         /* save PC to R14 */
        }
        else
        {
                SET_REGISTER(14, pc - 4);           /* save PC to R14 */
        }
        SET_REGISTER(SPSR, cpsr);               /* Save current CPSR */
        SET_CPSR(GET_CPSR | I_MASK);            /* Mask IRQ */
        SET_CPSR(GET_CPSR & ~T_MASK);
        R15 = 0x04;                             /* IRQ Vector address */
 //       change_pc(R15);
        ARM7.pendingUnd = 0;
        return;
    }

    // Software Interrupt
    if (ARM7.pendingSwi) {
        SwitchMode(eARM7_MODE_SVC);             /* Set SVC mode so PC is saved to correct R14 bank */
        // compensate for prefetch (should this also be done for normal IRQ?)
        if (T_IS_SET(GET_CPSR))
        {
                SET_REGISTER(14, pc-2);         /* save PC to R14 */
        }
        else
        {
                SET_REGISTER(14, pc);           /* save PC to R14 */
        }
        SET_REGISTER(SPSR, cpsr);               /* Save current CPSR */
        SET_CPSR(GET_CPSR | I_MASK);            /* Mask IRQ */
        SET_CPSR(GET_CPSR & ~T_MASK);           /* Go to ARM mode */
        R15 = 0x08;                             /* Jump to the SWI vector */
   //     change_pc(R15);
        ARM7.pendingSwi = 0;
        return;
    }
}

// CPU - SET IRQ LINE
static void arm7_core_set_irq_line(int irqline, int state)
{
    switch (irqline) {

    case ARM7_IRQ_LINE: /* IRQ */
        ARM7.pendingIrq = state & 1;
        break;

    case ARM7_FIRQ_LINE: /* FIRQ */
        ARM7.pendingFiq = state & 1;
        break;

    case ARM7_ABORT_EXCEPTION:
        ARM7.pendingAbtD = state & 1;
        break;
    case ARM7_ABORT_PREFETCH_EXCEPTION:
        ARM7.pendingAbtP = state & 1;
        break;

    case ARM7_UNDEFINE_EXCEPTION:
        ARM7.pendingUnd = state & 1;
        break;
    }

    ARM7_CHECKIRQ;
}

/***************************************************************************
 *                            OPCODE HANDLING
 ***************************************************************************/

// Co-Processor Data Operation
static void HandleCoProcDO(UINT32 insn)
{
    // This instruction simply instructs the co-processor to do something, no data is returned to ARM7 core
    if (arm7_coproc_do_callback)
        arm7_coproc_do_callback(0, insn);   // simply pass entire opcode to callback - since data format is actually dependent on co-proc implementation
    else
        LOG(("%08x: Co-Processor Data Operation executed, but no callback defined!\n", R15));
}

// Co-Processor Register Transfer - To/From Arm to Co-Proc
static void HandleCoProcRT(UINT32 insn)
{

    /* xxxx 1110 oooL nnnn dddd cccc ppp1 mmmm */

    // Load (MRC) data from Co-Proc to ARM7 register
    if (insn & 0x00100000)       // Bit 20 = Load or Store
    {
        if (arm7_coproc_rt_r_callback)
        {
            UINT32 res = arm7_coproc_rt_r_callback(insn);   // RT Read handler must parse opcode & return appropriate result
            SET_REGISTER((insn >> 12) & 0xf, res);
        }
        else
            LOG(("%08x: Co-Processor Register Transfer executed, but no RT Read callback defined!\n", R15));
    }
    // Store (MCR) data from ARM7 to Co-Proc register
    else
    {
        if (arm7_coproc_rt_w_callback)
            arm7_coproc_rt_w_callback(insn, GET_REGISTER((insn >> 12) & 0xf));
        else
            LOG(("%08x: Co-Processor Register Transfer executed, but no RT Write callback defined!\n", R15));
    }
}

/* Data Transfer - To/From Arm to Co-Proc
   Loading or Storing, the co-proc function is responsible to read/write from the base register supplied + offset
   8 bit immediate value Base Offset address is << 2 to get the actual #

  issues - #1 - the co-proc function, needs direct access to memory reads or writes (ie, so we must send a pointer to a func)
         - #2 - the co-proc may adjust the base address (especially if it reads more than 1 word), so a pointer to the register must be used
                but the old value of the register must be restored if write back is not set..
         - #3 - when post incrementing is used, it's up to the co-proc func. to add the offset, since the transfer
                address supplied in that case, is simply the base. I suppose this is irrelevant if write back not set
                but if co-proc reads multiple address, it must handle the offset adjustment itself.
*/
// todo: test with valid instructions
static void HandleCoProcDT(UINT32 insn)
{
    UINT32 rn = (insn >> 16) & 0xf;
    UINT32 rnv = GET_REGISTER(rn);    // Get Address Value stored from Rn
    UINT32 ornv = rnv;                // Keep value of Rn
    UINT32 off = (insn & 0xff) << 2;  // Offset is << 2 according to manual
    UINT32 *prn = &ARM7REG(rn);       // Pointer to our register, so it can be changed in the callback

    // Pointers to read32/write32 functions
    void (*write32)(UINT32 addr, UINT32 data);
    UINT32 (*read32)(UINT32 addr);
    write32 = PTR_WRITE32;
    read32 = PTR_READ32;

#if ARM7_DEBUG_CORE
    if (((insn >> 16) & 0xf) == 15 && (insn & 0x200000))
        LOG(("%08x: Illegal use of R15 as base for write back value!\n", R15));
#endif

    // Pre-Increment base address (IF POST INCREMENT - CALL BACK FUNCTION MUST DO IT)
    if ((insn & 0x1000000) && off)
    {
        // Up - Down bit
        if (insn & 0x800000)
            rnv += off;
        else
            rnv -= off;
    }

    // Load (LDC) data from ARM7 memory to Co-Proc memory
    if (insn & 0x00100000)
    {
        if (arm7_coproc_dt_r_callback)
            arm7_coproc_dt_r_callback(insn, prn, read32);
        else
            LOG(("%08x: Co-Processer Data Transfer executed, but no READ callback defined!\n", R15));
    }
    // Store (STC) data from Co-Proc to ARM7 memory
    else
    {
        if (arm7_coproc_dt_w_callback)
            arm7_coproc_dt_w_callback(insn, prn, write32);
        else
            LOG(("%08x: Co-Processer Data Transfer executed, but no WRITE callback defined!\n", R15));
    }
    if (ARM7.pendingUnd != 0) return;
    // If writeback not used - ensure the original value of RN is restored in case co-proc callback changed value
    if ((insn & 0x200000) == 0)
        SET_REGISTER(rn, ornv);
}

static void HandleBranch(UINT32 insn, bool h_bit)
{
    UINT32 off = (insn & INSN_BRANCH) << 2;
    if (h_bit)
    {
        // H goes to bit1
        off |= (insn & 0x01000000) >> 23;
    }

    /* Save PC into LR if this is a branch with link or a BLX */
    if ((insn & INSN_BL) || ((m_archRev >= 5) && ((insn & 0xfe000000) == 0xfa000000)))
    {
        SET_REGISTER(14, R15 + 4);
    }

    /* Sign-extend the 24-bit offset in our calculations */
    if (off & 0x2000000u)
    {
        if (MODE32)
            R15 -= ((~(off | 0xfc000000u)) + 1) - 8;
        else
            R15 = ((R15 - (((~(off | 0xfc000000u)) + 1) - 8)) & 0x03FFFFFC) | (R15 & ~0x03FFFFFC);
    }
    else
    {
        if (MODE32)
            R15 += off + 8;
        else
            R15 = ((R15 + (off + 8)) & 0x03FFFFFC) | (R15 & ~0x03FFFFFC);
    }

//    change_pc(R15);
}

static void HandleMemSingle(UINT32 insn)
{
    UINT32 rn, rnv, off, rd, rnv_old = 0;

    /* Fetch the offset */
    if (insn & INSN_I)
    {
        /* Register Shift */
        off = decodeShift(insn, NULL);
    }
    else
    {
        /* Immediate Value */
        off = insn & INSN_SDT_IMM;
    }

    /* Calculate Rn, accounting for PC */
    rn = (insn & INSN_RN) >> INSN_RN_SHIFT;

    if (insn & INSN_SDT_P)
    {
        /* Pre-indexed addressing */
        if (insn & INSN_SDT_U)
        {
            if ((MODE32) || (rn != eR15))
                rnv = (GET_REGISTER(rn) + off);
            else
                rnv = (GET_PC + off);
        }
        else
        {
            if ((MODE32) || (rn != eR15))
                rnv = (GET_REGISTER(rn) - off);
            else
                rnv = (GET_PC - off);
        }

        if (insn & INSN_SDT_W)
        {
            rnv_old = GET_REGISTER(rn);
            SET_REGISTER(rn, rnv);

    // check writeback???
        }
        else if (rn == eR15)
        {
            rnv = rnv + 8;
        }
    }
    else
    {
        /* Post-indexed addressing */
        if (rn == eR15)
        {
            if (MODE32)
                rnv = R15 + 8;
            else
                rnv = GET_PC + 8;
        }
        else
        {
            rnv = GET_REGISTER(rn);
        }
    }

    /* Do the transfer */
    rd = (insn & INSN_RD) >> INSN_RD_SHIFT;
    if (insn & INSN_SDT_L)
    {
        /* Load */
        if (insn & INSN_SDT_B)
        {
            UINT32 data = READ8(rnv);
            if (!ARM7.pendingAbtD)
            {
                SET_REGISTER(rd, data);
            }
        }
        else
        {
            UINT32 data = READ32(rnv);
            if (!ARM7.pendingAbtD)
            {
                if (rd == eR15)
                {
                    if (MODE32)
                        R15 = data - 4;
                    else
                        R15 = (R15 & ~0x03FFFFFC) /* N Z C V I F M1 M0 */ | ((data - 4) & 0x03FFFFFC);
                    // LDR, PC takes 2S + 2N + 1I (5 total cycles)
                    ARM7_ICOUNT -= 2;
                    if ((data & 1) && m_archRev >= 5)
                    {
                        SET_CPSR(GET_CPSR | T_MASK);
                        R15--;
                    }
                }
                else
                {
                    SET_REGISTER(rd, data);
                }
            }
        }
    }
    else
    {
        /* Store */
        if (insn & INSN_SDT_B)
        {
#if ARM7_DEBUG_CORE
                if (rd == eR15)
                    LOG(("Wrote R15 in byte mode\n"));
#endif

            WRITE8(rnv, (UINT8) GET_REGISTER(rd) & 0xffu);
        }
        else
        {
#if ARM7_DEBUG_CORE
                if (rd == eR15)
                    LOG(("Wrote R15 in 32bit mode\n"));
#endif

            //WRITE32(rnv, rd == eR15 ? R15 + 8 : GET_REGISTER(rd));
            WRITE32(rnv, rd == eR15 ? R15 + 8 + 4 : GET_REGISTER(rd)); // manual says STR rd = PC, +12
        }
        // Store takes only 2 N Cycles, so add + 1
        ARM7_ICOUNT += 1;
    }

    if (ARM7.pendingAbtD)
    {
        if ((insn & INSN_SDT_P) && (insn & INSN_SDT_W))
        {
            SET_REGISTER(rn, rnv_old);
        }
    }
    else
    {
        /* Do post-indexing writeback */
        if (!(insn & INSN_SDT_P)/* && (insn & INSN_SDT_W)*/)
        {
            if (insn & INSN_SDT_U)
            {
                /* Writeback is applied in pipeline, before value is read from mem,
                 so writeback is effectively ignored */
                if (rd == rn) {
                    SET_REGISTER(rn, GET_REGISTER(rd));
                    // todo: check for offs... ?
                }
                else {
                    if ((insn & INSN_SDT_W) != 0)
                        LOG(("%08x:  RegisterWritebackIncrement %d %d %d\n", R15, (insn & INSN_SDT_P) != 0, (insn & INSN_SDT_W) != 0, (insn & INSN_SDT_U) != 0));
                    
                    SET_REGISTER(rn, (rnv + off));
                }
            }
            else
            {
                /* Writeback is applied in pipeline, before value is read from mem,
                 so writeback is effectively ignored */
                if (rd == rn) {
                    SET_REGISTER(rn, GET_REGISTER(rd));
                }
                else {
                    SET_REGISTER(rn, (rnv - off));
                    
                    if ((insn & INSN_SDT_W) != 0)
                        LOG(("%08x:  RegisterWritebackDecrement %d %d %d\n", R15, (insn & INSN_SDT_P) != 0, (insn & INSN_SDT_W) != 0, (insn & INSN_SDT_U) != 0));
                }
            }
        }
    }
} /* HandleMemSingle */

static void HandleHalfWordDT(UINT32 insn)
{
    UINT32 rn, rnv, off, rd, rnv_old = 0;

    // Immediate or Register Offset?
    if (insn & 0x400000) {               // Bit 22 - 1 = immediate, 0 = register
        // imm. value in high nibble (bits 8-11) and lo nibble (bit 0-3)
        off = (((insn >> 8) & 0x0f) << 4) | (insn & 0x0f);
    }
    else {
        // register
        off = GET_REGISTER(insn & 0x0f);
    }

    /* Calculate Rn, accounting for PC */
    rn = (insn & INSN_RN) >> INSN_RN_SHIFT;

    if (insn & INSN_SDT_P)
    {
        /* Pre-indexed addressing */
        if (insn & INSN_SDT_U)
        {
            rnv = (GET_REGISTER(rn) + off);
        }
        else
        {
            rnv = (GET_REGISTER(rn) - off);
        }

        if (insn & INSN_SDT_W)
        {
            rnv_old = GET_REGISTER(rn);
            SET_REGISTER(rn, rnv);

        // check writeback???
        }
        else if (rn == eR15)
        {
            rnv = (rnv) + 8;
        }
    }
    else
    {
        /* Post-indexed addressing */
        if (rn == eR15)
        {
            rnv = R15 + 8;
        }
        else
        {
            rnv = GET_REGISTER(rn);
        }
    }

    /* Do the transfer */
    rd = (insn & INSN_RD) >> INSN_RD_SHIFT;

    /* Load */
    if (insn & INSN_SDT_L)
    {
        // Signed?
        if (insn & 0x40)
        {
            UINT32 newval;

            // Signed Half Word?
            if (insn & 0x20) {
                INT32 data = (INT32)(INT16)(UINT16)READ16(rnv & ~1);
                if ((rnv & 1) && m_archRev < 5)
                    data >>= 8;
                newval = (UINT32)data;
            }
            // Signed Byte
            else {
                UINT8 databyte;
                UINT32 signbyte;
                databyte = READ8(rnv) & 0xff;
                signbyte = (databyte & 0x80) ? 0xffffff : 0;
                newval = (UINT32)(signbyte << 8)|databyte;
            }

            if (!ARM7.pendingAbtD)
            {
            // PC?
            if (rd == eR15)
            {
                R15 = newval + 8;
                // LDR(H,SH,SB) PC takes 2S + 2N + 1I (5 total cycles)
                ARM7_ICOUNT -= 2;

            }
            else
            {
                SET_REGISTER(rd, newval);
                R15 += 4;
            }

            }
            else
            {
                R15 += 4;
            }

        }
        // Unsigned Half Word
        else
        {
            UINT32 newval = READ16(rnv);

            if (!ARM7.pendingAbtD)
            {
                if (rd == eR15)
                {
                    R15 = newval + 8;
                    // extra cycles for LDR(H,SH,SB) PC (5 total cycles)
                    ARM7_ICOUNT -= 2;
                }
                else
                {
                    SET_REGISTER(rd, newval);
                    R15 += 4;
                }
            }
            else
            {
                R15 += 4;
            }

        }


    }
    /* Store or ARMv5+ dword insns */
    else
    {
        if ((insn & 0x60) == 0x40)  // LDRD
        {
            SET_REGISTER(rd, READ32(rnv));
            SET_REGISTER(rd+1, READ32(rnv+4));
                R15 += 4;
        }
        else if ((insn & 0x60) == 0x60) // STRD
        {
            WRITE32(rnv, GET_REGISTER(rd));
            WRITE32(rnv+4, GET_REGISTER(rd+1));
            R15 += 4;
        }
        else
        {
            // WRITE16(rnv, rd == eR15 ? R15 + 8 : GET_REGISTER(rd));
            WRITE16(rnv, rd == eR15 ? R15 + 8 + 4 : GET_REGISTER(rd)); // manual says STR RD=PC, +12 of address

    // if R15 is not increased then e.g. "STRH R10, [R15,#$10]" will be executed over and over again
    #if 0
            if (rn != eR15)
    #endif
            R15 += 4;

            // STRH takes 2 cycles, so we add + 1
            ARM7_ICOUNT += 1;
        }
    }

    if (ARM7.pendingAbtD)
    {
        if ((insn & INSN_SDT_P) && (insn & INSN_SDT_W))
        {
            SET_REGISTER(rn, rnv_old);
        }
    }
    else
    {
        // SJE: No idea if this writeback code works or makes sense here..

        /* Do post-indexing writeback */
        if (!(insn & INSN_SDT_P)/* && (insn & INSN_SDT_W)*/)
        {
            if (insn & INSN_SDT_U)
            {
                /* Writeback is applied in pipeline, before value is read from mem,
                    so writeback is effectively ignored */
                if (rd == rn) {
                    SET_REGISTER(rn, GET_REGISTER(rd));
                    // todo: check for offs... ?
                }
                else {
                    if ((insn & INSN_SDT_W) != 0)
                        LOG(("%08x:  RegisterWritebackIncrement %d %d %d\n", R15, (insn & INSN_SDT_P) != 0, (insn & INSN_SDT_W) != 0, (insn & INSN_SDT_U) != 0));

                    SET_REGISTER(rn, (rnv + off));
                }
            }
            else
            {
                /* Writeback is applied in pipeline, before value is read from mem,
                    so writeback is effectively ignored */
                if (rd == rn) {
                    SET_REGISTER(rn, GET_REGISTER(rd));
                }
                else {
                    SET_REGISTER(rn, (rnv - off));

                    if ((insn & INSN_SDT_W) != 0)
                        LOG(("%08x:  RegisterWritebackDecrement %d %d %d\n", R15, (insn & INSN_SDT_P) != 0, (insn & INSN_SDT_W) != 0, (insn & INSN_SDT_U) != 0));
                }
            }
        }

    }
}

static void HandleSwap(UINT32 insn)
{
    UINT32 rn, rm, rd, tmp;

    rn = GET_REGISTER((insn >> 16) & 0xf);  // reg. w/read address
    rm = GET_REGISTER(insn & 0xf);          // reg. w/write address
    rd = (insn >> 12) & 0xf;                // dest reg

#if ARM7_DEBUG_CORE
    if (rn == 15 || rm == 15 || rd == 15)
        LOG(("%08x: Illegal use of R15 in Swap Instruction\n", R15));
#endif

    // can be byte or word
    if (insn & 0x400000)
    {
        tmp = READ8(rn);
        WRITE8(rn, rm);
        SET_REGISTER(rd, tmp);
    }
    else
    {
        tmp = READ32(rn);
        WRITE32(rn, rm);
        SET_REGISTER(rd, tmp);
    }

    R15 += 4;
    // Instruction takes 1S+2N+1I cycles - so we subtract one more..
    ARM7_ICOUNT -= 1;
}

static void HandlePSRTransfer(UINT32 insn)
{
    int reg = (insn & 0x400000) ? SPSR : eCPSR; // Either CPSR or SPSR
    UINT32 newval, val;
    int oldmode = GET_CPSR & MODE_FLAG;

    // get old value of CPSR/SPSR
    newval = GET_REGISTER(reg);

    // MSR (bit 21 set) - Copy value to CPSR/SPSR
    if ((insn & 0x00200000))
    {
        // Immediate Value?
        if (insn & INSN_I) {
            // Value can be specified for a Right Rotate, 2x the value specified.
            int by = (insn & INSN_OP2_ROTATE) >> INSN_OP2_ROTATE_SHIFT;
            if (by)
                val = ROR(insn & INSN_OP2_IMM, by << 1);
            else
                val = insn & INSN_OP2_IMM;
        }
        // Value from Register
        else
        {
            val = GET_REGISTER(insn & 0x0f);
        }

        // apply field code bits
        if (reg == eCPSR)
        {
            if (oldmode != eARM7_MODE_USER)
            {
                if (insn & 0x00010000)
                {
                    newval = (newval & 0xffffff00) | (val & 0x000000ff);
                }
                if (insn & 0x00020000)
                {
                    newval = (newval & 0xffff00ff) | (val & 0x0000ff00);
                }
                if (insn & 0x00040000)
                {
                    newval = (newval & 0xff00ffff) | (val & 0x00ff0000);
                }
            }

            // status flags can be modified regardless of mode
            if (insn & 0x00080000)
            {
                // TODO for non ARMv5E mask should be 0xf0000000 (ie mask Q bit)
                newval = (newval & 0x00ffffff) | (val & 0xf8000000);
            }
        }
        else    // SPSR has stricter requirements
        {
            if (((GET_CPSR & 0x1f) > 0x10) && ((GET_CPSR & 0x1f) < 0x1f))
            {
                if (insn & 0x00010000)
                {
                    newval = (newval & 0xffffff00) | (val & 0xff);
                }
                if (insn & 0x00020000)
                {
                    newval = (newval & 0xffff00ff) | (val & 0xff00);
                }
                if (insn & 0x00040000)
                {
                    newval = (newval & 0xff00ffff) | (val & 0xff0000);
                }
                if (insn & 0x00080000)
                {
                    // TODO for non ARMv5E mask should be 0xf0000000 (ie mask Q bit)
                    newval = (newval & 0x00ffffff) | (val & 0xf8000000);
                }
            }
        }

#if 0
        // force valid mode
        newval |= 0x10;
#endif

        // Update the Register
        if (reg == eCPSR)
        {
            SET_CPSR(newval);
        }
        else
            SET_REGISTER(reg, newval);

        // Switch to new mode if changed
        if ((newval & MODE_FLAG) != oldmode)
            SwitchMode(GET_MODE);

    }
    // MRS (bit 21 clear) - Copy CPSR or SPSR to specified Register
    else
    {
        SET_REGISTER((insn >> 12)& 0x0f, GET_REGISTER(reg));
    }
}

static void HandleALU(UINT32 insn)
{
    UINT32 op2, sc = 0, rd, rn, opcode;
    UINT32 by, rdn;
 //   UINT32 oldR15 = R15;

    opcode = (insn & INSN_OPCODE) >> INSN_OPCODE_SHIFT;

    rd = 0;
    rn = 0;

    /* --------------*/
    /* Construct Op2 */
    /* --------------*/

    /* Immediate constant */
    if (insn & INSN_I)
    {
        by = (insn & INSN_OP2_ROTATE) >> INSN_OP2_ROTATE_SHIFT;
        if (by)
        {
            op2 = ROR(insn & INSN_OP2_IMM, by << 1);
            sc = op2 & SIGN_BIT;
        }
        else
        {
            op2 = insn & INSN_OP2;      // SJE: Shouldn't this be INSN_OP2_IMM?
            sc = GET_CPSR & C_MASK;
        }
    }
    /* Op2 = Register Value */
    else
    {
        op2 = decodeShift(insn, (insn & INSN_S) ? &sc : NULL);

        // LD TODO sc will always be 0 if this applies
        if (!(insn & INSN_S))
            sc = 0;

        // extra cycle (register specified shift)
        ARM7_ICOUNT -= 1;
    }

    // LD TODO this comment is wrong
    /* Calculate Rn to account for pipelining */
    if ((opcode & 0xd) != 0xd) /* No Rn in MOV */
    {
        if ((rn = (insn & INSN_RN) >> INSN_RN_SHIFT) == eR15)
        {
    #if ARM7_DEBUG_CORE
            LOG(("%08x:  Pipelined R15 (Shift %d)\n", R15, (insn & INSN_I ? 8 : insn & 0x10u ? 12 : 12)));
    #endif
            if (MODE32)
                rn = R15 + 8;
            else
                rn = GET_PC + 8;
        }
        else
        {
            rn = GET_REGISTER(rn);
        }
    }

    /* Perform the operation */

    switch (opcode)
    {
    /* Arithmetic operations */
    case OPCODE_SBC:
        rd = (rn - op2 - (GET_CPSR & C_MASK ? 0 : 1));
        HandleALUSubFlags(rd, rn, op2);
        break;
    case OPCODE_CMP:
    case OPCODE_SUB:
        rd = (rn - op2);
        HandleALUSubFlags(rd, rn, op2);
        break;
    case OPCODE_RSC:
        rd = (op2 - rn - (GET_CPSR & C_MASK ? 0 : 1));
        HandleALUSubFlags(rd, op2, rn);
        break;
    case OPCODE_RSB:
        rd = (op2 - rn);
        HandleALUSubFlags(rd, op2, rn);
        break;
    case OPCODE_ADC:
        rd = (rn + op2 + ((GET_CPSR & C_MASK) >> C_BIT));
        HandleALUAddFlags(rd, rn, op2);
        break;
    case OPCODE_CMN:
    case OPCODE_ADD:
        rd = (rn + op2);
        HandleALUAddFlags(rd, rn, op2);
        break;

    /* Logical operations */
    case OPCODE_AND:
    case OPCODE_TST:
        rd = rn & op2;
        HandleALULogicalFlags(rd, sc);
        break;
    case OPCODE_BIC:
        rd = rn & ~op2;
        HandleALULogicalFlags(rd, sc);
        break;
    case OPCODE_TEQ:
    case OPCODE_EOR:
        rd = rn ^ op2;
        HandleALULogicalFlags(rd, sc);
        break;
    case OPCODE_ORR:
        rd = rn | op2;
        HandleALULogicalFlags(rd, sc);
        break;
    case OPCODE_MOV:
        rd = op2;
        HandleALULogicalFlags(rd, sc);
        break;
    case OPCODE_MVN:
        rd = (~op2);
        HandleALULogicalFlags(rd, sc);
        break;
    }

    /* Put the result in its register if not one of the test only opcodes (TST,TEQ,CMP,CMN) */
    rdn = (insn & INSN_RD) >> INSN_RD_SHIFT;
    if ((opcode & 0xc) != 0x8)
    {
        // If Rd = R15, but S Flag not set, Result is placed in R15, but CPSR is not affected (page 44)
        if (rdn == eR15 && !(insn & INSN_S))
        {
            if (MODE32)
            {
                R15 = rd;
            }
            else
            {
                R15 = (R15 & ~0x03FFFFFC) | (rd & 0x03FFFFFC);
            }
            // extra cycles (PC written)
            ARM7_ICOUNT -= 2;
        }
        else
        {
            // Rd = 15 and S Flag IS set, Result is placed in R15, and current mode SPSR moved to CPSR
            if (rdn == eR15) {
                if (MODE32)
                {
                // When Rd is R15 and the S flag is set the result of the operation is placed in R15 and the SPSR corresponding to
                // the current mode is moved to the CPSR. This allows state changes which automatically restore both PC and
                // CPSR. --> This form of instruction should not be used in User mode. <--

                if (GET_MODE != eARM7_MODE_USER)
                {
                    // Update CPSR from SPSR
                    SET_CPSR(GET_REGISTER(SPSR));
                    SwitchMode(GET_MODE);
                }

                R15 = rd;

                }
                else
                {
                    UINT32 temp;
                    R15 = rd; //(R15 & 0x03FFFFFC) | (rd & 0xFC000003);
                    temp = (GET_CPSR & 0x0FFFFF20) | (rd & 0xF0000000) /* N Z C V */ | ((rd & 0x0C000000) >> (26 - 6)) /* I F */ | (rd & 0x00000003) /* M1 M0 */;
                    SET_CPSR( temp);
                    SwitchMode( temp & 3);
                }

                // extra cycles (PC written)
                ARM7_ICOUNT -= 2;

                /* IRQ masks may have changed in this instruction */
//              ARM7_CHECKIRQ;
            }
            else
                /* S Flag is set - Write results to register & update CPSR (which was already handled using HandleALU flag macros) */
                SET_REGISTER(rdn, rd);
        }
    }
    // SJE: Don't think this applies any more.. (see page 44 at bottom)
    /* TST & TEQ can affect R15 (the condition code register) with the S bit set */
    else if (rdn == eR15)
    {
        if (insn & INSN_S) {
    #if ARM7_DEBUG_CORE
                LOG(("%08x: TST class on R15 s bit set\n", R15));
    #endif
            if (MODE32)
                R15 = rd;
            else
            {
                UINT32 temp;
                R15 = (R15 & 0x03FFFFFC) | (rd & ~0x03FFFFFC);
                temp = (GET_CPSR & 0x0FFFFF20) | (rd & 0xF0000000) /* N Z C V */ | ((rd & 0x0C000000) >> (26 - 6)) /* I F */ | (rd & 0x00000003) /* M1 M0 */;
                SET_CPSR( temp);
                SwitchMode( temp & 3);
            }

            /* IRQ masks may have changed in this instruction */
//          ARM7_CHECKIRQ;
        }
        else
        {
#if ARM7_DEBUG_CORE
                LOG(("%08x: TST class on R15 no s bit set\n", R15));
#endif
        }
        // extra cycles (PC written)
        ARM7_ICOUNT -= 2;
    }

    // compensate for the -3 at the end
    ARM7_ICOUNT += 2;
}

static void HandleMul(UINT32 insn)
{
    UINT32 r, rm, rs;

    // MUL takes 1S + mI and MLA 1S + (m+1)I cycles to execute, where S and I are as
    // defined in 6.2 Cycle Types on page 6-2.
    // m is the number of 8 bit multiplier array cycles required to complete the
    // multiply, which is controlled by the value of the multiplier operand
    // specified by Rs.

    rm = GET_REGISTER(insn & INSN_MUL_RM);
    rs = GET_REGISTER((insn & INSN_MUL_RS) >> INSN_MUL_RS_SHIFT);

    /* Do the basic multiply of Rm and Rs */
    r = rm * rs;

#if ARM7_DEBUG_CORE
    if ((insn & INSN_MUL_RM) == 0xf ||
        ((insn & INSN_MUL_RS) >> INSN_MUL_RS_SHIFT) == 0xf ||
        ((insn & INSN_MUL_RN) >> INSN_MUL_RN_SHIFT) == 0xf)
        LOG(("%08x:  R15 used in mult\n", R15));
#endif

    /* Add on Rn if this is a MLA */
    if (insn & INSN_MUL_A)
    {
        r += GET_REGISTER((insn & INSN_MUL_RN) >> INSN_MUL_RN_SHIFT);
        // extra cycle for MLA
        ARM7_ICOUNT -= 1;
    }

    /* Write the result */
    SET_REGISTER((insn & INSN_MUL_RD) >> INSN_MUL_RD_SHIFT, r);

    /* Set N and Z if asked */
    if (insn & INSN_S)
    {
        SET_CPSR((GET_CPSR & ~(N_MASK | Z_MASK)) | HandleALUNZFlags(r));
    }

    if (rs & SIGN_BIT) rs = -rs;
    if (rs < 0x00000100) ARM7_ICOUNT -= 1 + 1;
    else if (rs < 0x00010000) ARM7_ICOUNT -= 1 + 2;
    else if (rs < 0x01000000) ARM7_ICOUNT -= 1 + 3;
    else ARM7_ICOUNT -= 1 + 4;

    ARM7_ICOUNT += 3;
}

// todo: add proper cycle counts
static void HandleSMulLong(UINT32 insn)
{
    INT32 rm, rs;
    UINT32 rhi, rlo;
    INT64 res;

    // MULL takes 1S + (m+1)I and MLAL 1S + (m+2)I cycles to execute, where m is the
    // number of 8 bit multiplier array cycles required to complete the multiply, which is
    // controlled by the value of the multiplier operand specified by Rs.

    rm  = (INT32)GET_REGISTER(insn & 0xf);
    rs  = (INT32)GET_REGISTER(((insn >> 8) & 0xf));
    rhi = (insn >> 16) & 0xf;
    rlo = (insn >> 12) & 0xf;

#if ARM7_DEBUG_CORE
        if ((insn & 0xf) == 15 || ((insn >> 8) & 0xf) == 15 || ((insn >> 16) & 0xf) == 15 || ((insn >> 12) & 0xf) == 15)
            LOG(("%08x: Illegal use of PC as a register in SMULL opcode\n", R15));
#endif

    /* Perform the multiplication */
    res = (INT64)rm * rs;

    /* Add on Rn if this is a MLA */
    if (insn & INSN_MUL_A)
    {
        INT64 acum = (INT64)((((INT64)(GET_REGISTER(rhi))) << 32) | GET_REGISTER(rlo));
        res += acum;
        // extra cycle for MLA
        ARM7_ICOUNT -= 1;
    }

    /* Write the result (upper dword goes to RHi, lower to RLo) */
    SET_REGISTER(rhi, res >> 32);
    SET_REGISTER(rlo, res & 0xFFFFFFFF);

    /* Set N and Z if asked */
    if (insn & INSN_S)
    {
        SET_CPSR((GET_CPSR & ~(N_MASK | Z_MASK)) | HandleLongALUNZFlags(res));
    }

    if (rs < 0) rs = -rs;
    if (rs < 0x00000100) ARM7_ICOUNT -= 1 + 1 + 1;
    else if (rs < 0x00010000) ARM7_ICOUNT -= 1 + 2 + 1;
    else if (rs < 0x01000000) ARM7_ICOUNT -= 1 + 3 + 1;
    else ARM7_ICOUNT -= 1 + 4 + 1;

    ARM7_ICOUNT += 3;
}

// todo: add proper cycle counts
static void HandleUMulLong(UINT32 insn)
{
    UINT32 rm, rs;
    UINT32 rhi, rlo;
    UINT64 res;

    // MULL takes 1S + (m+1)I and MLAL 1S + (m+2)I cycles to execute, where m is the
    // number of 8 bit multiplier array cycles required to complete the multiply, which is
    // controlled by the value of the multiplier operand specified by Rs.

    rm  = (INT32)GET_REGISTER(insn & 0xf);
    rs  = (INT32)GET_REGISTER(((insn >> 8) & 0xf));
    rhi = (insn >> 16) & 0xf;
    rlo = (insn >> 12) & 0xf;

#if ARM7_DEBUG_CORE
        if (((insn & 0xf) == 15) || (((insn >> 8) & 0xf) == 15) || (((insn >> 16) & 0xf) == 15) || (((insn >> 12) & 0xf) == 15))
            LOG(("%08x: Illegal use of PC as a register in SMULL opcode\n", R15));
#endif

    /* Perform the multiplication */
    res = (UINT64)rm * rs;

    /* Add on Rn if this is a MLA */
    if (insn & INSN_MUL_A)
    {
        UINT64 acum = (UINT64)((((UINT64)(GET_REGISTER(rhi))) << 32) | GET_REGISTER(rlo));
        res += acum;
        // extra cycle for MLA
        ARM7_ICOUNT -= 1;
    }

    /* Write the result (upper dword goes to RHi, lower to RLo) */
    SET_REGISTER(rhi, res >> 32);
    SET_REGISTER(rlo, res & 0xFFFFFFFF);

    /* Set N and Z if asked */
    if (insn & INSN_S)
    {
        SET_CPSR((GET_CPSR & ~(N_MASK | Z_MASK)) | HandleLongALUNZFlags(res));
    }

    if (rs < 0x00000100) ARM7_ICOUNT -= 1 + 1 + 1;
    else if (rs < 0x00010000) ARM7_ICOUNT -= 1 + 2 + 1;
    else if (rs < 0x01000000) ARM7_ICOUNT -= 1 + 3 + 1;
    else ARM7_ICOUNT -= 1 + 4 + 1;

    ARM7_ICOUNT += 3;
}

static void HandleMemBlock(UINT32 insn)
{
    UINT32 rb = (insn & INSN_RN) >> INSN_RN_SHIFT;
    UINT32 rbp = GET_REGISTER(rb);
//    UINT32 oldR15 = R15;
    int result;

#if ARM7_DEBUG_CORE
    if (rbp & 3)
        LOG(("%08x: Unaligned Mem Transfer @ %08x\n", R15, rbp));
#endif

    // Normal LDM instructions take nS + 1N + 1I and LDM PC takes (n+1)S + 2N + 1I
    // incremental cycles, where S,N and I are as defined in 6.2 Cycle Types on page 6-2.
    // STM instructions take (n-1)S + 2N incremental cycles to execute, where n is the
    // number of words transferred.

    if (insn & INSN_BDT_L)
    {
        /* Loading */
        if (insn & INSN_BDT_U)
        {
            /* Incrementing */
            if (!(insn & INSN_BDT_P))
            {
                rbp = rbp + (- 4);
            }

            // S Flag Set, but R15 not in list = User Bank Transfer
            if (insn & INSN_BDT_S && (insn & 0x8000) == 0)
            {
                // !! actually switching to user mode triggers a section permission fault in Happy Fish 302-in-1 (BP C0030DF4, press F5 ~16 times) !!
                // set to user mode - then do the transfer, and set back
                //int curmode = GET_MODE;
                //SwitchMode(eARM7_MODE_USER);
                LOG(("%08x: User Bank Transfer not fully tested - please check if working properly!\n", R15));
                result = loadInc(insn & 0xffff, rbp, insn & INSN_BDT_S, eARM7_MODE_USER);
                // todo - not sure if Writeback occurs on User registers also..
                //SwitchMode(curmode);
            }
            else
                result = loadInc(insn & 0xffff, rbp, insn & INSN_BDT_S, GET_MODE);

            if ((insn & INSN_BDT_W) && !ARM7.pendingAbtD)
            {
    #if ARM7_DEBUG_CORE
                    if (rb == 15)
                        LOG(("%08x:  Illegal LDRM writeback to r15\n", R15));
    #endif
                // "A LDM will always overwrite the updated base if the base is in the list." (also for a user bank transfer?)
                // GBA "V-Rally 3" expects R0 not to be overwritten with the updated base value [BP 8077B0C]
                if (((insn >> rb) & 1) == 0)
                {
                    SET_REGISTER(rb, GET_REGISTER(rb) + result * 4);
                }
            }

            // R15 included? (NOTE: CPSR restore must occur LAST otherwise wrong registers restored!)
            if ((insn & 0x8000) && !ARM7.pendingAbtD)
            {
                R15 -= 4;     // SJE: I forget why i did this?
                // S - Flag Set? Signals transfer of current mode SPSR->CPSR
                if (insn & INSN_BDT_S)
                {
                    if (MODE32)
                    {
                        SET_CPSR(GET_REGISTER(SPSR));
                        SwitchMode(GET_MODE);
                    }
                    else
                    {
                        UINT32 temp;
    //                      LOG(("LDM + S | R15 %08X CPSR %08X\n", R15, GET_CPSR));
                        temp = (GET_CPSR & 0x0FFFFF20) | (R15 & 0xF0000000) /* N Z C V */ | ((R15 & 0x0C000000) >> (26 - 6)) /* I F */ | (R15 & 0x00000003) /* M1 M0 */;
                        SET_CPSR( temp);
                        SwitchMode(temp & 3);
                    }
                }
                else
                    if ((R15 & 1) && m_archRev >= 5)
                    {
                        SET_CPSR(GET_CPSR | T_MASK);
                        R15--;
                    }
                // LDM PC - takes 2 extra cycles
                ARM7_ICOUNT -= 2;
            }
        }
        else
        {
            /* Decrementing */
            if (!(insn & INSN_BDT_P))
            {
                rbp = rbp - (- 4);
            }

            // S Flag Set, but R15 not in list = User Bank Transfer
            if (insn & INSN_BDT_S && ((insn & 0x8000) == 0))
            {
                // set to user mode - then do the transfer, and set back
                //int curmode = GET_MODE;
                //SwitchMode(eARM7_MODE_USER);
                LOG(("%08x: User Bank Transfer not fully tested - please check if working properly!\n", R15));
                result = loadDec(insn & 0xffff, rbp, insn & INSN_BDT_S, eARM7_MODE_USER);
                // todo - not sure if Writeback occurs on User registers also..
                //SwitchMode(curmode);
            }
            else
                result = loadDec(insn & 0xffff, rbp, insn & INSN_BDT_S, GET_MODE);

            if ((insn & INSN_BDT_W) && !ARM7.pendingAbtD)
            {
                if (rb == 0xf)
                    LOG(("%08x:  Illegal LDRM writeback to r15\n", R15));
                // "A LDM will always overwrite the updated base if the base is in the list." (also for a user bank transfer?)
                if (((insn >> rb) & 1) == 0)
                {
                    SET_REGISTER(rb, GET_REGISTER(rb) - result * 4);
                }
            }

            // R15 included? (NOTE: CPSR restore must occur LAST otherwise wrong registers restored!)
            if ((insn & 0x8000) && !ARM7.pendingAbtD) {
                R15 -= 4;     // SJE: I forget why i did this?
                // S - Flag Set? Signals transfer of current mode SPSR->CPSR
                if (insn & INSN_BDT_S)
                {
                    if (MODE32)
                    {
                        SET_CPSR(GET_REGISTER(SPSR));
                        SwitchMode(GET_MODE);
                    }
                    else
                    {
                        UINT32 temp;
    //                      LOG(("LDM + S | R15 %08X CPSR %08X\n", R15, GET_CPSR));
                        temp = (GET_CPSR & 0x0FFFFF20) /* N Z C V I F M4 M3 M2 M1 M0 */ | (R15 & 0xF0000000) /* N Z C V */ | ((R15 & 0x0C000000) >> (26 - 6)) /* I F */ | (R15 & 0x00000003) /* M1 M0 */;
                        SET_CPSR(temp);
                        SwitchMode(temp & 3);
                    }
                }
                else
                    if ((R15 & 1) && m_archRev >= 5)
                    {
                        SET_CPSR(GET_CPSR | T_MASK);
                        R15--;
                    }
                // LDM PC - takes 2 extra cycles
                ARM7_ICOUNT -= 2;
            }
        }
        // LDM (NO PC) takes (n)S + 1N + 1I cycles (n = # of register transfers)
        ARM7_ICOUNT -= result + 1 + 1;
    } /* Loading */
    else
    {
        /* Storing */
        if (insn & (1 << eR15))
        {
#if ARM7_DEBUG_CORE
                LOG(("%08x: Writing R15 in strm\n", R15));
#endif
            /* special case handling if writing to PC */
            R15 += 12;
        }
        if (insn & INSN_BDT_U)
        {
            /* Incrementing */
            if (!(insn & INSN_BDT_P))
            {
                rbp = rbp + (- 4);
            }

            // S Flag Set = User Bank Transfer
            if (insn & INSN_BDT_S)
            {
                // todo: needs to be tested..

                // set to user mode - then do the transfer, and set back
                //int curmode = GET_MODE;
                //SwitchMode(eARM7_MODE_USER);
                LOG(("%08x: User Bank Transfer not fully tested - please check if working properly!\n", R15));
                result = storeInc(insn & 0xffff, rbp, eARM7_MODE_USER);
                // todo - not sure if Writeback occurs on User registers also..
                //SwitchMode(curmode);
            }
            else
                result = storeInc(insn & 0xffff, rbp, GET_MODE);

            if ((insn & INSN_BDT_W) && !ARM7.pendingAbtD)
            {
                SET_REGISTER(rb, GET_REGISTER(rb) + result * 4);
            }
        }
        else
        {
            /* Decrementing - but real CPU writes in incrementing order */
            if (!(insn & INSN_BDT_P))
            {
                rbp = rbp - (-4);
            }

            // S Flag Set = User Bank Transfer
            if (insn & INSN_BDT_S)
            {
                // set to user mode - then do the transfer, and set back
                //int curmode = GET_MODE;
                //SwitchMode(eARM7_MODE_USER);
                LOG(("%08x: User Bank Transfer not fully tested - please check if working properly!\n", R15));
                result = storeDec(insn & 0xffff, rbp, eARM7_MODE_USER);
                // todo - not sure if Writeback occurs on User registers also..
                //SwitchMode(curmode);
            }
            else
                result = storeDec(insn & 0xffff, rbp, GET_MODE);

            if ((insn & INSN_BDT_W) && !ARM7.pendingAbtD)
            {
                SET_REGISTER(rb, GET_REGISTER(rb) - result * 4);
            }
        }
        if (insn & (1 << eR15))
            R15 -= 12;

        // STM takes (n-1)S + 2N cycles (n = # of register transfers)
        ARM7_ICOUNT -= (result - 1) + 2;
    }

    // We will specify the cycle count for each case, so remove the -3 that occurs at the end
    ARM7_ICOUNT += 3;
} /* HandleMemBlock */

const arm7ops_ophandler ops_handler[0x20] =
{
    &arm7ops_0123, &arm7ops_0123, &arm7ops_0123, &arm7ops_0123,
    &arm7ops_4567, &arm7ops_4567, &arm7ops_4567, &arm7ops_4567,
    &arm7ops_89,   &arm7ops_89,   &arm7ops_ab,   &arm7ops_ab,
    &arm7ops_cd,   &arm7ops_cd,   &arm7ops_e,    &arm7ops_f,
    &arm9ops_undef,&arm9ops_1,    &arm9ops_undef,&arm9ops_undef,
    &arm9ops_undef,&arm9ops_57,   &arm9ops_undef,&arm9ops_57,
    &arm9ops_89,   &arm9ops_89,   &arm9ops_ab,   &arm9ops_ab,
    &arm9ops_c,    &arm9ops_undef,&arm9ops_e,    &arm9ops_undef,
};

void arm9ops_undef(UINT32 insn)
{
    // unsupported instruction
    LOG(("ARM7: Instruction %08X unsupported\n", insn));
}

void arm9ops_1(UINT32 insn)
{
    /* Change processor state (CPS) */
    if ((insn & 0x00f10020) == 0x00000000)
    {
        // unsupported (armv6 onwards only)
        arm9ops_undef(insn);
        R15 += 4;
    }
    else if ((insn & 0x00ff00f0) == 0x00010000) /* set endianness (SETEND) */
    {
        // unsupported (armv6 onwards only)
        if (m_archRev < 6) arm9ops_undef(insn);
        else
        {
            UINT32 new_cpsr = GET_CPSR & ~(1 << 9);
            SET_CPSR(new_cpsr | (insn & (1 << 9)));
        }
        R15 += 4;
    }
    else
    {
        arm9ops_undef(insn);
        R15 += 4;
    }
}

void arm9ops_57(UINT32 insn)
{
    /* Cache Preload (PLD) */
    if ((insn & 0x0070f000) == 0x0050f000)
    {
        // unsupported (armv6 onwards only)
        if (m_archRev < 6) arm9ops_undef(insn);
        R15 += 4;
    }
    else
    {
        arm9ops_undef(insn);
        R15 += 4;
    }
}

void arm9ops_89(UINT32 insn)
{
    /* Save Return State (SRS) */
    if ((insn & 0x005f0f00) == 0x004d0500)
    {
        // unsupported (armv6 onwards only)
        arm9ops_undef(insn);
        R15 += 4;
    }
    else if ((insn & 0x00500f00) == 0x00100a00) /* Return From Exception (RFE) */
    {
        // unsupported (armv6 onwards only)
        arm9ops_undef(insn);
        R15 += 4;
    }
    else
    {
        arm9ops_undef(insn);
        R15 += 4;
    }
}

void arm9ops_ab(UINT32 insn)
{
    // BLX
    HandleBranch(insn, true);
    SET_CPSR(GET_CPSR | T_MASK);
}

void arm9ops_c(UINT32 insn)
{
    /* Additional coprocessor double register transfer */
    if ((insn & 0x00e00000) == 0x00400000)
    {
        // unsupported
        arm9ops_undef(insn);
        R15 += 4;
    }
    else
    {
        arm9ops_undef(insn);
        R15 += 4;
    }
}

void arm9ops_e(UINT32 insn)
{
    /* Additional coprocessor register transfer */
    // unsupported
    arm9ops_undef(insn);
    R15 += 4;
}


void arm7ops_0123(UINT32 insn)
{
    //case 0:
    //case 1:
    //case 2:
    //case 3:
        /* Branch and Exchange (BX) */
    if ((insn & 0x0ffffff0) == 0x012fff10)     // bits 27-4 == 000100101111111111110001
    {
        R15 = GET_REGISTER(insn & 0x0f);
        // If new PC address has A0 set, switch to Thumb mode
        if (R15 & 1) {
            SET_CPSR(GET_CPSR | T_MASK);
            R15--;
        }
    }
    else if ((insn & 0x0ff000f0) == 0x01200030) // BLX Rn - v5
    {
        // save link address
        SET_REGISTER(14, R15 + 4);

        R15 = GET_REGISTER(insn & 0x0f);
        // If new PC address has A0 set, switch to Thumb mode
        if (R15 & 1) {
            SET_CPSR(GET_CPSR | T_MASK);
            R15--;
        }
    }
    else if ((insn & 0x0ff000f0) == 0x01600010) // CLZ - v5
    {
        UINT32 rm = insn & 0xf;
        UINT32 rd = (insn >> 12) & 0xf;

        SET_REGISTER(rd, count_leading_zeros_32(GET_REGISTER(rm)));

        R15 += 4;
    }
    else if ((insn & 0x0ff000f0) == 0x01000050) // QADD - v5
    {
        INT32 src1 = GET_REGISTER(insn & 0xf);
        INT32 src2 = GET_REGISTER((insn >> 16) & 0xf);
        INT64 res;

        res = saturate_qbit_overflow((INT64)src1 + (INT64)src2);

        SET_REGISTER((insn >> 12) & 0xf, (INT32)res);
        R15 += 4;
    }
    else if ((insn & 0x0ff000f0) == 0x01400050) // QDADD - v5
    {
        INT32 src1 = GET_REGISTER(insn & 0xf);
        INT32 src2 = GET_REGISTER((insn >> 16) & 0xf);
        INT64 res;

        // check if doubling operation will overflow
        res = (INT64)src2 * 2;
        saturate_qbit_overflow(res);

        src2 *= 2;
        res = saturate_qbit_overflow((INT64)src1 + (INT64)src2);

        SET_REGISTER((insn >> 12) & 0xf, (INT32)res);
        R15 += 4;
    }
    else if ((insn & 0x0ff000f0) == 0x01200050) // QSUB - v5
    {
        INT32 src1 = GET_REGISTER(insn & 0xf);
        INT32 src2 = GET_REGISTER((insn >> 16) & 0xf);
        INT64 res;

        res = saturate_qbit_overflow((INT64)src1 - (INT64)src2);

        SET_REGISTER((insn >> 12) & 0xf, (INT32)res);
        R15 += 4;
    }
    else if ((insn & 0x0ff000f0) == 0x01600050) // QDSUB - v5
    {
        INT32 src1 = GET_REGISTER(insn & 0xf);
        INT32 src2 = GET_REGISTER((insn >> 16) & 0xf);
        INT64 res;

        // check if doubling operation will overflow
        res = (INT64)src2 * 2;
        saturate_qbit_overflow(res);

        src2 *= 2;
        res = saturate_qbit_overflow((INT64)src1 - (INT64)src2);

        SET_REGISTER((insn >> 12) & 0xf, (INT32)res);
        R15 += 4;
    }
    else if ((insn & 0x0ff00090) == 0x01000080) // SMLAxy - v5
    {
        INT32 src1 = GET_REGISTER(insn & 0xf);
        INT32 src2 = GET_REGISTER((insn >> 8) & 0xf);
        INT32 res1;

        // select top and bottom halves of src1/src2 and sign extend if necessary
        if (insn & 0x20)
        {
            src1 >>= 16;
        }

        src1 &= 0xffff;
        if (src1 & 0x8000)
        {
            src1 |= 0xffff0000;
        }

        if (insn & 0x40)
        {
            src2 >>= 16;
        }

        src2 &= 0xffff;
        if (src2 & 0x8000)
        {
            src2 |= 0xffff0000;
        }

        // do the signed multiply
        res1 = src1 * src2;
        // and the accumulate.  NOTE: only the accumulate can cause an overflow, which is why we do it this way.
        saturate_qbit_overflow((INT64)res1 + (INT64)GET_REGISTER((insn >> 12) & 0xf));

        SET_REGISTER((insn >> 16) & 0xf, res1 + GET_REGISTER((insn >> 12) & 0xf));
        R15 += 4;
    }
    else if ((insn & 0x0ff00090) == 0x01400080) // SMLALxy - v5
    {
        INT32 src1 = GET_REGISTER(insn & 0xf);
        INT32 src2 = GET_REGISTER((insn >> 8) & 0xf);
        INT64 dst;

        dst = (INT64)GET_REGISTER((insn >> 12) & 0xf);
        dst |= (INT64)GET_REGISTER((insn >> 16) & 0xf) << 32;

        // do the multiply and accumulate
        dst += (INT64)src1 * (INT64)src2;

        // write back the result
        SET_REGISTER((insn >> 12) & 0xf, (UINT32)dst);
        SET_REGISTER((insn >> 16) & 0xf, (UINT32)(dst >> 32));
        R15 += 4;
    }
    else if ((insn & 0x0ff00090) == 0x01600080) // SMULxy - v5
    {
        INT32 src1 = GET_REGISTER(insn & 0xf);
        INT32 src2 = GET_REGISTER((insn >> 8) & 0xf);
        INT32 res;

        // select top and bottom halves of src1/src2 and sign extend if necessary
        if (insn & 0x20)
        {
            src1 >>= 16;
        }

        src1 &= 0xffff;
        if (src1 & 0x8000)
        {
            src1 |= 0xffff0000;
        }

        if (insn & 0x40)
        {
            src2 >>= 16;
        }

        src2 &= 0xffff;
        if (src2 & 0x8000)
        {
            src2 |= 0xffff0000;
        }

        res = src1 * src2;
        SET_REGISTER((insn >> 16) & 0xf, res);
        R15 += 4;
    }
    else if ((insn & 0x0ff000b0) == 0x012000a0) // SMULWy - v5
    {
        INT32 src1 = GET_REGISTER(insn & 0xf);
        INT32 src2 = GET_REGISTER((insn >> 8) & 0xf);
        INT64 res;

        if (insn & 0x40)
        {
            src2 >>= 16;
        }

        src2 &= 0xffff;
        if (src2 & 0x8000)
        {
            src2 |= 0xffff0000;
        }

        res = (INT64)src1 * (INT64)src2;
        res >>= 16;
        SET_REGISTER((insn >> 16) & 0xf, (UINT32)res);
        R15 += 4;
    }
    else if ((insn & 0x0ff000b0) == 0x01200080) // SMLAWy - v5
    {
        INT32 src1 = GET_REGISTER(insn & 0xf);
        INT32 src2 = GET_REGISTER((insn >> 8) & 0xf);
        INT32 src3 = GET_REGISTER((insn >> 12) & 0xf);
        INT64 res;

        if (insn & 0x40)
        {
            src2 >>= 16;
        }

        src2 &= 0xffff;
        if (src2 & 0x8000)
        {
            src2 |= 0xffff0000;
        }

        res = (INT64)src1 * (INT64)src2;
        res >>= 16;

        // check for overflow and set the Q bit
        saturate_qbit_overflow((INT64)src3 + res);

        // do the real accumulate
        src3 += (INT32)res;

        // write the result back
        SET_REGISTER((insn >> 16) & 0xf, (UINT32)res);
        R15 += 4;
    }
    else
        /* Multiply OR Swap OR Half Word Data Transfer */
        if ((insn & 0x0e000000) == 0 && (insn & 0x80) && (insn & 0x10))  // bits 27-25=000 bit 7=1 bit 4=1
        {
            /* Half Word Data Transfer */
            if (insn & 0x60)         // bits = 6-5 != 00
            {
                HandleHalfWordDT(insn);
            }
            else
                /* Swap */
                if (insn & 0x01000000)   // bit 24 = 1
                {
                    HandleSwap(insn);
                }
            /* Multiply Or Multiply Long */
                else
                {
                    /* multiply long */
                    if (insn & 0x800000) // Bit 23 = 1 for Multiply Long
                    {
                        /* Signed? */
                        if (insn & 0x00400000)
                            HandleSMulLong(insn);
                        else
                            HandleUMulLong(insn);
                    }
                    /* multiply */
                    else
                    {
                        HandleMul(insn);
                    }
                    R15 += 4;
                }
        }
    /* Data Processing OR PSR Transfer */
        else if ((insn & 0x0c000000) == 0)   // bits 27-26 == 00 - This check can only exist properly after Multiplication check above
        {
            /* PSR Transfer (MRS & MSR) */
            if (((insn & 0x00100000) == 0) && ((insn & 0x01800000) == 0x01000000)) // S bit must be clear, and bit 24,23 = 10
            {
                HandlePSRTransfer(insn);
                ARM7_ICOUNT += 2;       // PSR only takes 1 - S Cycle, so we add + 2, since at end, we -3..
                R15 += 4;
            }
            /* Data Processing */
            else
            {
                HandleALU(insn);
            }
        }
    //  break;
}

void arm7ops_4567(UINT32 insn) /* Data Transfer - Single Data Access */
{
    //case 4:
    //case 5:
    //case 6:
    //case 7:
    HandleMemSingle(insn);
    R15 += 4;
    //  break;
}

void arm7ops_89(UINT32 insn) /* Block Data Transfer/Access */
{
    //case 8:
    //case 9:
    HandleMemBlock(insn);
    R15 += 4;
    //  break;
}

void arm7ops_ab(UINT32 insn) /* Branch or Branch & Link */
{
    //case 0xa:
    //case 0xb:
    HandleBranch(insn, false);
    //  break;
}

void arm7ops_cd(UINT32 insn) /* Co-Processor Data Transfer */
{
    //case 0xc:
    //case 0xd:
    HandleCoProcDT(insn);
    R15 += 4;
    //  break;
}

void arm7ops_e(UINT32 insn) /* Co-Processor Data Operation or Register Transfer */
{
    //case 0xe:
    if (insn & 0x10)
        HandleCoProcRT(insn);
    else
        HandleCoProcDO(insn);
    R15 += 4;
    //  break;
}

void arm7ops_f(UINT32 insn) /* Software Interrupt */
{
    ARM7.pendingSwi = 1;
    arm7_check_irq_state();
    //couldn't find any cycle counts for SWI
//  break;
}

// this is our master dispatch jump table for THUMB mode, representing [(INSN & 0xffc0) >> 6] bits of the 16-bit decoded instruction
const arm7thumb_ophandler thumb_handler[0x40 * 0x10] =
{
    // #define THUMB_SHIFT_R       ((UINT16)0x0800)
        &tg00_0,     &tg00_0,     &tg00_0,     &tg00_0,
        &tg00_0,     &tg00_0,     &tg00_0,     &tg00_0,
        &tg00_0,     &tg00_0,     &tg00_0,     &tg00_0,
        &tg00_0,     &tg00_0,     &tg00_0,     &tg00_0,
        &tg00_0,     &tg00_0,     &tg00_0,     &tg00_0,
        &tg00_0,     &tg00_0,     &tg00_0,     &tg00_0,
        &tg00_0,     &tg00_0,     &tg00_0,     &tg00_0,
        &tg00_0,     &tg00_0,     &tg00_0,     &tg00_0,
        &tg00_1,     &tg00_1,     &tg00_1,     &tg00_1,
        &tg00_1,     &tg00_1,     &tg00_1,     &tg00_1,
        &tg00_1,     &tg00_1,     &tg00_1,     &tg00_1,
        &tg00_1,     &tg00_1,     &tg00_1,     &tg00_1,
        &tg00_1,     &tg00_1,     &tg00_1,     &tg00_1,
        &tg00_1,     &tg00_1,     &tg00_1,     &tg00_1,
        &tg00_1,     &tg00_1,     &tg00_1,     &tg00_1,
        &tg00_1,     &tg00_1,     &tg00_1,     &tg00_1,
    // #define THUMB_INSN_ADDSUB   ((UINT16)0x0800)   // #define THUMB_ADDSUB_TYPE   ((UINT16)0x0600)
        &tg01_0,     &tg01_0,     &tg01_0,     &tg01_0,
        &tg01_0,     &tg01_0,     &tg01_0,     &tg01_0,
        &tg01_0,     &tg01_0,     &tg01_0,     &tg01_0,
        &tg01_0,     &tg01_0,     &tg01_0,     &tg01_0,
        &tg01_0,     &tg01_0,     &tg01_0,     &tg01_0,
        &tg01_0,     &tg01_0,     &tg01_0,     &tg01_0,
        &tg01_0,     &tg01_0,     &tg01_0,     &tg01_0,
        &tg01_0,     &tg01_0,     &tg01_0,     &tg01_0,
        &tg01_10,    &tg01_10,    &tg01_10,    &tg01_10,
        &tg01_10,    &tg01_10,    &tg01_10,    &tg01_10,
        &tg01_11,    &tg01_11,    &tg01_11,    &tg01_11,
        &tg01_11,    &tg01_11,    &tg01_11,    &tg01_11,
        &tg01_12,    &tg01_12,    &tg01_12,    &tg01_12,
        &tg01_12,    &tg01_12,    &tg01_12,    &tg01_12,
        &tg01_13,    &tg01_13,    &tg01_13,    &tg01_13,
        &tg01_13,    &tg01_13,    &tg01_13,    &tg01_13,
    // #define THUMB_INSN_CMP      ((UINT16)0x0800)
        &tg02_0,     &tg02_0,     &tg02_0,     &tg02_0,
        &tg02_0,     &tg02_0,     &tg02_0,     &tg02_0,
        &tg02_0,     &tg02_0,     &tg02_0,     &tg02_0,
        &tg02_0,     &tg02_0,     &tg02_0,     &tg02_0,
        &tg02_0,     &tg02_0,     &tg02_0,     &tg02_0,
        &tg02_0,     &tg02_0,     &tg02_0,     &tg02_0,
        &tg02_0,     &tg02_0,     &tg02_0,     &tg02_0,
        &tg02_0,     &tg02_0,     &tg02_0,     &tg02_0,
        &tg02_1,     &tg02_1,     &tg02_1,     &tg02_1,
        &tg02_1,     &tg02_1,     &tg02_1,     &tg02_1,
        &tg02_1,     &tg02_1,     &tg02_1,     &tg02_1,
        &tg02_1,     &tg02_1,     &tg02_1,     &tg02_1,
        &tg02_1,     &tg02_1,     &tg02_1,     &tg02_1,
        &tg02_1,     &tg02_1,     &tg02_1,     &tg02_1,
        &tg02_1,     &tg02_1,     &tg02_1,     &tg02_1,
        &tg02_1,     &tg02_1,     &tg02_1,     &tg02_1,
    // #define THUMB_INSN_SUB      ((UINT16)0x0800)
        &tg03_0,     &tg03_0,     &tg03_0,     &tg03_0,
        &tg03_0,     &tg03_0,     &tg03_0,     &tg03_0,
        &tg03_0,     &tg03_0,     &tg03_0,     &tg03_0,
        &tg03_0,     &tg03_0,     &tg03_0,     &tg03_0,
        &tg03_0,     &tg03_0,     &tg03_0,     &tg03_0,
        &tg03_0,     &tg03_0,     &tg03_0,     &tg03_0,
        &tg03_0,     &tg03_0,     &tg03_0,     &tg03_0,
        &tg03_0,     &tg03_0,     &tg03_0,     &tg03_0,
        &tg03_1,     &tg03_1,     &tg03_1,     &tg03_1,
        &tg03_1,     &tg03_1,     &tg03_1,     &tg03_1,
        &tg03_1,     &tg03_1,     &tg03_1,     &tg03_1,
        &tg03_1,     &tg03_1,     &tg03_1,     &tg03_1,
        &tg03_1,     &tg03_1,     &tg03_1,     &tg03_1,
        &tg03_1,     &tg03_1,     &tg03_1,     &tg03_1,
        &tg03_1,     &tg03_1,     &tg03_1,     &tg03_1,
        &tg03_1,     &tg03_1,     &tg03_1,     &tg03_1,
    //#define THUMB_GROUP4_TYPE   ((UINT16)0x0c00)  //#define THUMB_ALUOP_TYPE    ((UINT16)0x03c0)  // #define THUMB_HIREG_OP      ((UINT16)0x0300)  // #define THUMB_HIREG_H       ((UINT16)0x00c0)
        &tg04_00_00, &tg04_00_01, &tg04_00_02, &tg04_00_03,
        &tg04_00_04, &tg04_00_05, &tg04_00_06, &tg04_00_07,
        &tg04_00_08, &tg04_00_09, &tg04_00_0a, &tg04_00_0b,
        &tg04_00_0c, &tg04_00_0d, &tg04_00_0e, &tg04_00_0f,
        &tg04_01_00, &tg04_01_01, &tg04_01_02, &tg04_01_03,
        &tg04_01_10, &tg04_01_11, &tg04_01_12, &tg04_01_13,
        &tg04_01_20, &tg04_01_21, &tg04_01_22, &tg04_01_23,
        &tg04_01_30, &tg04_01_31, &tg04_01_32, &tg04_01_33,
        &tg04_0203,  &tg04_0203,  &tg04_0203,  &tg04_0203,
        &tg04_0203,  &tg04_0203,  &tg04_0203,  &tg04_0203,
        &tg04_0203,  &tg04_0203,  &tg04_0203,  &tg04_0203,
        &tg04_0203,  &tg04_0203,  &tg04_0203,  &tg04_0203,
        &tg04_0203,  &tg04_0203,  &tg04_0203,  &tg04_0203,
        &tg04_0203,  &tg04_0203,  &tg04_0203,  &tg04_0203,
        &tg04_0203,  &tg04_0203,  &tg04_0203,  &tg04_0203,
        &tg04_0203,  &tg04_0203,  &tg04_0203,  &tg04_0203,
    //#define THUMB_GROUP5_TYPE   ((UINT16)0x0e00)
        &tg05_0,     &tg05_0,     &tg05_0,     &tg05_0,
        &tg05_0,     &tg05_0,     &tg05_0,     &tg05_0,
        &tg05_1,     &tg05_1,     &tg05_1,     &tg05_1,
        &tg05_1,     &tg05_1,     &tg05_1,     &tg05_1,
        &tg05_2,     &tg05_2,     &tg05_2,     &tg05_2,
        &tg05_2,     &tg05_2,     &tg05_2,     &tg05_2,
        &tg05_3,     &tg05_3,     &tg05_3,     &tg05_3,
        &tg05_3,     &tg05_3,     &tg05_3,     &tg05_3,
        &tg05_4,     &tg05_4,     &tg05_4,     &tg05_4,
        &tg05_4,     &tg05_4,     &tg05_4,     &tg05_4,
        &tg05_5,     &tg05_5,     &tg05_5,     &tg05_5,
        &tg05_5,     &tg05_5,     &tg05_5,     &tg05_5,
        &tg05_6,     &tg05_6,     &tg05_6,     &tg05_6,
        &tg05_6,     &tg05_6,     &tg05_6,     &tg05_6,
        &tg05_7,     &tg05_7,     &tg05_7,     &tg05_7,
        &tg05_7,     &tg05_7,     &tg05_7,     &tg05_7,
    //#define THUMB_LSOP_L        ((UINT16)0x0800)
        &tg06_0,     &tg06_0,     &tg06_0,     &tg06_0,
        &tg06_0,     &tg06_0,     &tg06_0,     &tg06_0,
        &tg06_0,     &tg06_0,     &tg06_0,     &tg06_0,
        &tg06_0,     &tg06_0,     &tg06_0,     &tg06_0,
        &tg06_0,     &tg06_0,     &tg06_0,     &tg06_0,
        &tg06_0,     &tg06_0,     &tg06_0,     &tg06_0,
        &tg06_0,     &tg06_0,     &tg06_0,     &tg06_0,
        &tg06_0,     &tg06_0,     &tg06_0,     &tg06_0,
        &tg06_1,     &tg06_1,     &tg06_1,     &tg06_1,
        &tg06_1,     &tg06_1,     &tg06_1,     &tg06_1,
        &tg06_1,     &tg06_1,     &tg06_1,     &tg06_1,
        &tg06_1,     &tg06_1,     &tg06_1,     &tg06_1,
        &tg06_1,     &tg06_1,     &tg06_1,     &tg06_1,
        &tg06_1,     &tg06_1,     &tg06_1,     &tg06_1,
        &tg06_1,     &tg06_1,     &tg06_1,     &tg06_1,
        &tg06_1,     &tg06_1,     &tg06_1,     &tg06_1,
    //#define THUMB_LSOP_L        ((UINT16)0x0800)
        &tg07_0,     &tg07_0,     &tg07_0,     &tg07_0,
        &tg07_0,     &tg07_0,     &tg07_0,     &tg07_0,
        &tg07_0,     &tg07_0,     &tg07_0,     &tg07_0,
        &tg07_0,     &tg07_0,     &tg07_0,     &tg07_0,
        &tg07_0,     &tg07_0,     &tg07_0,     &tg07_0,
        &tg07_0,     &tg07_0,     &tg07_0,     &tg07_0,
        &tg07_0,     &tg07_0,     &tg07_0,     &tg07_0,
        &tg07_0,     &tg07_0,     &tg07_0,     &tg07_0,
        &tg07_1,     &tg07_1,     &tg07_1,     &tg07_1,
        &tg07_1,     &tg07_1,     &tg07_1,     &tg07_1,
        &tg07_1,     &tg07_1,     &tg07_1,     &tg07_1,
        &tg07_1,     &tg07_1,     &tg07_1,     &tg07_1,
        &tg07_1,     &tg07_1,     &tg07_1,     &tg07_1,
        &tg07_1,     &tg07_1,     &tg07_1,     &tg07_1,
        &tg07_1,     &tg07_1,     &tg07_1,     &tg07_1,
        &tg07_1,     &tg07_1,     &tg07_1,     &tg07_1,
    // #define THUMB_HALFOP_L      ((UINT16)0x0800)
        &tg08_0,     &tg08_0,     &tg08_0,     &tg08_0,
        &tg08_0,     &tg08_0,     &tg08_0,     &tg08_0,
        &tg08_0,     &tg08_0,     &tg08_0,     &tg08_0,
        &tg08_0,     &tg08_0,     &tg08_0,     &tg08_0,
        &tg08_0,     &tg08_0,     &tg08_0,     &tg08_0,
        &tg08_0,     &tg08_0,     &tg08_0,     &tg08_0,
        &tg08_0,     &tg08_0,     &tg08_0,     &tg08_0,
        &tg08_0,     &tg08_0,     &tg08_0,     &tg08_0,
        &tg08_1,     &tg08_1,     &tg08_1,     &tg08_1,
        &tg08_1,     &tg08_1,     &tg08_1,     &tg08_1,
        &tg08_1,     &tg08_1,     &tg08_1,     &tg08_1,
        &tg08_1,     &tg08_1,     &tg08_1,     &tg08_1,
        &tg08_1,     &tg08_1,     &tg08_1,     &tg08_1,
        &tg08_1,     &tg08_1,     &tg08_1,     &tg08_1,
        &tg08_1,     &tg08_1,     &tg08_1,     &tg08_1,
        &tg08_1,     &tg08_1,     &tg08_1,     &tg08_1,
    // #define THUMB_STACKOP_L     ((UINT16)0x0800)
        &tg09_0,     &tg09_0,     &tg09_0,     &tg09_0,
        &tg09_0,     &tg09_0,     &tg09_0,     &tg09_0,
        &tg09_0,     &tg09_0,     &tg09_0,     &tg09_0,
        &tg09_0,     &tg09_0,     &tg09_0,     &tg09_0,
        &tg09_0,     &tg09_0,     &tg09_0,     &tg09_0,
        &tg09_0,     &tg09_0,     &tg09_0,     &tg09_0,
        &tg09_0,     &tg09_0,     &tg09_0,     &tg09_0,
        &tg09_0,     &tg09_0,     &tg09_0,     &tg09_0,
        &tg09_1,     &tg09_1,     &tg09_1,     &tg09_1,
        &tg09_1,     &tg09_1,     &tg09_1,     &tg09_1,
        &tg09_1,     &tg09_1,     &tg09_1,     &tg09_1,
        &tg09_1,     &tg09_1,     &tg09_1,     &tg09_1,
        &tg09_1,     &tg09_1,     &tg09_1,     &tg09_1,
        &tg09_1,     &tg09_1,     &tg09_1,     &tg09_1,
        &tg09_1,     &tg09_1,     &tg09_1,     &tg09_1,
        &tg09_1,     &tg09_1,     &tg09_1,     &tg09_1,
    // #define THUMB_RELADDR_SP    ((UINT16)0x0800)
        &tg0a_0,     &tg0a_0,     &tg0a_0,     &tg0a_0,
        &tg0a_0,     &tg0a_0,     &tg0a_0,     &tg0a_0,
        &tg0a_0,     &tg0a_0,     &tg0a_0,     &tg0a_0,
        &tg0a_0,     &tg0a_0,     &tg0a_0,     &tg0a_0,
        &tg0a_0,     &tg0a_0,     &tg0a_0,     &tg0a_0,
        &tg0a_0,     &tg0a_0,     &tg0a_0,     &tg0a_0,
        &tg0a_0,     &tg0a_0,     &tg0a_0,     &tg0a_0,
        &tg0a_0,     &tg0a_0,     &tg0a_0,     &tg0a_0,
        &tg0a_1,     &tg0a_1,     &tg0a_1,     &tg0a_1,
        &tg0a_1,     &tg0a_1,     &tg0a_1,     &tg0a_1,
        &tg0a_1,     &tg0a_1,     &tg0a_1,     &tg0a_1,
        &tg0a_1,     &tg0a_1,     &tg0a_1,     &tg0a_1,
        &tg0a_1,     &tg0a_1,     &tg0a_1,     &tg0a_1,
        &tg0a_1,     &tg0a_1,     &tg0a_1,     &tg0a_1,
        &tg0a_1,     &tg0a_1,     &tg0a_1,     &tg0a_1,
        &tg0a_1,     &tg0a_1,     &tg0a_1,     &tg0a_1,
    // #define THUMB_STACKOP_TYPE  ((UINT16)0x0f00)
        &tg0b_0,     &tg0b_0,     &tg0b_0,     &tg0b_0,
        &tg0b_1,     &tg0b_1,     &tg0b_1,     &tg0b_1,
        &tg0b_2,     &tg0b_2,     &tg0b_2,     &tg0b_2,
        &tg0b_3,     &tg0b_3,     &tg0b_3,     &tg0b_3,
        &tg0b_4,     &tg0b_4,     &tg0b_4,     &tg0b_4,
        &tg0b_5,     &tg0b_5,     &tg0b_5,     &tg0b_5,
        &tg0b_6,     &tg0b_6,     &tg0b_6,     &tg0b_6,
        &tg0b_7,     &tg0b_7,     &tg0b_7,     &tg0b_7,
        &tg0b_8,     &tg0b_8,     &tg0b_8,     &tg0b_8,
        &tg0b_9,     &tg0b_9,     &tg0b_9,     &tg0b_9,
        &tg0b_a,     &tg0b_a,     &tg0b_a,     &tg0b_a,
        &tg0b_b,     &tg0b_b,     &tg0b_b,     &tg0b_b,
        &tg0b_c,     &tg0b_c,     &tg0b_c,     &tg0b_c,
        &tg0b_d,     &tg0b_d,     &tg0b_d,     &tg0b_d,
        &tg0b_e,     &tg0b_e,     &tg0b_e,     &tg0b_e,
        &tg0b_f,     &tg0b_f,     &tg0b_f,     &tg0b_f,
    // #define THUMB_MULTLS        ((UINT16)0x0800)
        &tg0c_0,     &tg0c_0,     &tg0c_0,     &tg0c_0,
        &tg0c_0,     &tg0c_0,     &tg0c_0,     &tg0c_0,
        &tg0c_0,     &tg0c_0,     &tg0c_0,     &tg0c_0,
        &tg0c_0,     &tg0c_0,     &tg0c_0,     &tg0c_0,
        &tg0c_0,     &tg0c_0,     &tg0c_0,     &tg0c_0,
        &tg0c_0,     &tg0c_0,     &tg0c_0,     &tg0c_0,
        &tg0c_0,     &tg0c_0,     &tg0c_0,     &tg0c_0,
        &tg0c_0,     &tg0c_0,     &tg0c_0,     &tg0c_0,
        &tg0c_1,     &tg0c_1,     &tg0c_1,     &tg0c_1,
        &tg0c_1,     &tg0c_1,     &tg0c_1,     &tg0c_1,
        &tg0c_1,     &tg0c_1,     &tg0c_1,     &tg0c_1,
        &tg0c_1,     &tg0c_1,     &tg0c_1,     &tg0c_1,
        &tg0c_1,     &tg0c_1,     &tg0c_1,     &tg0c_1,
        &tg0c_1,     &tg0c_1,     &tg0c_1,     &tg0c_1,
        &tg0c_1,     &tg0c_1,     &tg0c_1,     &tg0c_1,
        &tg0c_1,     &tg0c_1,     &tg0c_1,     &tg0c_1,
    // #define THUMB_COND_TYPE     ((UINT16)0x0f00)
        &tg0d_0,     &tg0d_0,     &tg0d_0,     &tg0d_0,
        &tg0d_1,     &tg0d_1,     &tg0d_1,     &tg0d_1,
        &tg0d_2,     &tg0d_2,     &tg0d_2,     &tg0d_2,
        &tg0d_3,     &tg0d_3,     &tg0d_3,     &tg0d_3,
        &tg0d_4,     &tg0d_4,     &tg0d_4,     &tg0d_4,
        &tg0d_5,     &tg0d_5,     &tg0d_5,     &tg0d_5,
        &tg0d_6,     &tg0d_6,     &tg0d_6,     &tg0d_6,
        &tg0d_7,     &tg0d_7,     &tg0d_7,     &tg0d_7,
        &tg0d_8,     &tg0d_8,     &tg0d_8,     &tg0d_8,
        &tg0d_9,     &tg0d_9,     &tg0d_9,     &tg0d_9,
        &tg0d_a,     &tg0d_a,     &tg0d_a,     &tg0d_a,
        &tg0d_b,     &tg0d_b,     &tg0d_b,     &tg0d_b,
        &tg0d_c,     &tg0d_c,     &tg0d_c,     &tg0d_c,
        &tg0d_d,     &tg0d_d,     &tg0d_d,     &tg0d_d,
        &tg0d_e,     &tg0d_e,     &tg0d_e,     &tg0d_e,
        &tg0d_f,     &tg0d_f,     &tg0d_f,     &tg0d_f,
    // #define THUMB_BLOP_LO       ((UINT16)0x0800)
        &tg0e_0,     &tg0e_0,     &tg0e_0,     &tg0e_0,
        &tg0e_0,     &tg0e_0,     &tg0e_0,     &tg0e_0,
        &tg0e_0,     &tg0e_0,     &tg0e_0,     &tg0e_0,
        &tg0e_0,     &tg0e_0,     &tg0e_0,     &tg0e_0,
        &tg0e_0,     &tg0e_0,     &tg0e_0,     &tg0e_0,
        &tg0e_0,     &tg0e_0,     &tg0e_0,     &tg0e_0,
        &tg0e_0,     &tg0e_0,     &tg0e_0,     &tg0e_0,
        &tg0e_0,     &tg0e_0,     &tg0e_0,     &tg0e_0,
        &tg0e_1,     &tg0e_1,     &tg0e_1,     &tg0e_1,
        &tg0e_1,     &tg0e_1,     &tg0e_1,     &tg0e_1,
        &tg0e_1,     &tg0e_1,     &tg0e_1,     &tg0e_1,
        &tg0e_1,     &tg0e_1,     &tg0e_1,     &tg0e_1,
        &tg0e_1,     &tg0e_1,     &tg0e_1,     &tg0e_1,
        &tg0e_1,     &tg0e_1,     &tg0e_1,     &tg0e_1,
        &tg0e_1,     &tg0e_1,     &tg0e_1,     &tg0e_1,
        &tg0e_1,     &tg0e_1,     &tg0e_1,     &tg0e_1,
    // #define THUMB_BLOP_LO       ((UINT16)0x0800)
        &tg0f_0,     &tg0f_0,     &tg0f_0,     &tg0f_0,
        &tg0f_0,     &tg0f_0,     &tg0f_0,     &tg0f_0,
        &tg0f_0,     &tg0f_0,     &tg0f_0,     &tg0f_0,
        &tg0f_0,     &tg0f_0,     &tg0f_0,     &tg0f_0,
        &tg0f_0,     &tg0f_0,     &tg0f_0,     &tg0f_0,
        &tg0f_0,     &tg0f_0,     &tg0f_0,     &tg0f_0,
        &tg0f_0,     &tg0f_0,     &tg0f_0,     &tg0f_0,
        &tg0f_0,     &tg0f_0,     &tg0f_0,     &tg0f_0,
        &tg0f_1,     &tg0f_1,     &tg0f_1,     &tg0f_1,
        &tg0f_1,     &tg0f_1,     &tg0f_1,     &tg0f_1,
        &tg0f_1,     &tg0f_1,     &tg0f_1,     &tg0f_1,
        &tg0f_1,     &tg0f_1,     &tg0f_1,     &tg0f_1,
        &tg0f_1,     &tg0f_1,     &tg0f_1,     &tg0f_1,
        &tg0f_1,     &tg0f_1,     &tg0f_1,     &tg0f_1,
        &tg0f_1,     &tg0f_1,     &tg0f_1,     &tg0f_1,
        &tg0f_1,     &tg0f_1,     &tg0f_1,     &tg0f_1,
};

/* Shift operations */

void tg00_0(UINT32 pc, UINT32 op) /* Shift left */
{
    UINT32 rs, rd, rrs;
    INT32 offs;

    SET_CPSR(GET_CPSR & ~(N_MASK | Z_MASK));

    rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    rrs = GET_REGISTER(rs);
    offs = (op & THUMB_SHIFT_AMT) >> THUMB_SHIFT_AMT_SHIFT;
    if (offs != 0)
    {
        SET_REGISTER(rd, rrs << offs);
        if (rrs & (1 << (31 - (offs - 1))))
        {
            SET_CPSR(GET_CPSR | C_MASK);
        }
        else
        {
            SET_CPSR(GET_CPSR & ~C_MASK);
        }
    }
    else
    {
        SET_REGISTER(rd, rrs);
    }
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
    R15 += 2;
}

void tg00_1(UINT32 pc, UINT32 op) /* Shift right */
{
    UINT32 rs, rd, rrs;
    INT32 offs;

    rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    rrs = GET_REGISTER(rs);
    offs = (op & THUMB_SHIFT_AMT) >> THUMB_SHIFT_AMT_SHIFT;
    if (offs != 0)
    {
        SET_REGISTER(rd, rrs >> offs);
        if (rrs & (1 << (offs - 1)))
        {
            SET_CPSR(GET_CPSR | C_MASK);
        }
        else
        {
            SET_CPSR(GET_CPSR & ~C_MASK);
        }
    }
    else
    {
        SET_REGISTER(rd, 0);
        if (rrs & 0x80000000)
        {
            SET_CPSR(GET_CPSR | C_MASK);
        }
        else
        {
            SET_CPSR(GET_CPSR & ~C_MASK);
        }
    }
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
    R15 += 2;
}

/* Arithmetic */

void tg01_0(UINT32 pc, UINT32 op)
{
    UINT32 rs, rd, rrs;
    INT32 offs;
    /* ASR.. */
    //if (op & THUMB_SHIFT_R) /* Shift right */
    {
        rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
        rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
        rrs = GET_REGISTER(rs);
        offs = (op & THUMB_SHIFT_AMT) >> THUMB_SHIFT_AMT_SHIFT;
        if (offs == 0)
        {
            offs = 32;
        }
        if (offs >= 32)
        {
            if (rrs >> 31)
            {
                SET_CPSR(GET_CPSR | C_MASK);
            }
            else
            {
                SET_CPSR(GET_CPSR & ~C_MASK);
            }
            SET_REGISTER(rd, (rrs & 0x80000000) ? 0xFFFFFFFF : 0x00000000);
        }
        else
        {
            if ((rrs >> (offs - 1)) & 1)
            {
                SET_CPSR(GET_CPSR | C_MASK);
            }
            else
            {
                SET_CPSR(GET_CPSR & ~C_MASK);
            }
            SET_REGISTER(rd,
                (rrs & 0x80000000)
                ? ((0xFFFFFFFF << (32 - offs)) | (rrs >> offs))
                : (rrs >> offs));
        }
        SET_CPSR(GET_CPSR & ~(N_MASK | Z_MASK));
        SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
        R15 += 2;
    }
}

void tg01_10(UINT32 pc, UINT32 op)  /* ADD Rd, Rs, Rn */
{
    UINT32 rn = GET_REGISTER((op & THUMB_ADDSUB_RNIMM) >> THUMB_ADDSUB_RNIMM_SHIFT);
    UINT32 rs = GET_REGISTER((op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT);
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    SET_REGISTER(rd, rs + rn);
    HandleThumbALUAddFlags(GET_REGISTER(rd), rs, rn);

}

void tg01_11(UINT32 pc, UINT32 op) /* SUB Rd, Rs, Rn */
{
    UINT32 rn = GET_REGISTER((op & THUMB_ADDSUB_RNIMM) >> THUMB_ADDSUB_RNIMM_SHIFT);
    UINT32 rs = GET_REGISTER((op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT);
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    SET_REGISTER(rd, rs - rn);
    HandleThumbALUSubFlags(GET_REGISTER(rd), rs, rn);

}

void tg01_12(UINT32 pc, UINT32 op) /* ADD Rd, Rs, #imm */
{
    UINT32 imm = (op & THUMB_ADDSUB_RNIMM) >> THUMB_ADDSUB_RNIMM_SHIFT;
    UINT32 rs = GET_REGISTER((op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT);
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    SET_REGISTER(rd, rs + imm);
    HandleThumbALUAddFlags(GET_REGISTER(rd), rs, imm);

}

void tg01_13(UINT32 pc, UINT32 op) /* SUB Rd, Rs, #imm */
{
    UINT32 imm = (op & THUMB_ADDSUB_RNIMM) >> THUMB_ADDSUB_RNIMM_SHIFT;
    UINT32 rs = GET_REGISTER((op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT);
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    SET_REGISTER(rd, rs - imm);
    HandleThumbALUSubFlags(GET_REGISTER(rd), rs, imm);

}

/* CMP / MOV */

void tg02_0(UINT32 pc, UINT32 op)
{
    UINT32 rd = (op & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT;
    UINT32 op2 = (op & THUMB_INSN_IMM);
    SET_REGISTER(rd, op2);
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
    R15 += 2;
}

void tg02_1(UINT32 pc, UINT32 op)
{
    UINT32 rn = GET_REGISTER((op & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT);
    UINT32 op2 = op & THUMB_INSN_IMM;
    UINT32 rd = rn - op2;
    HandleThumbALUSubFlags(rd, rn, op2);
}

/* ADD/SUB immediate */

void tg03_0(UINT32 pc, UINT32 op) /* ADD Rd, #Offset8 */
{
    UINT32 rn = GET_REGISTER((op & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT);
    UINT32 op2 = op & THUMB_INSN_IMM;
    UINT32 rd = rn + op2;
    SET_REGISTER((op & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT, rd);
    HandleThumbALUAddFlags(rd, rn, op2);
}

void tg03_1(UINT32 pc, UINT32 op) /* SUB Rd, #Offset8 */
{
    UINT32 rn = GET_REGISTER((op & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT);
    UINT32 op2 = op & THUMB_INSN_IMM;
    UINT32 rd = rn - op2;
    SET_REGISTER((op & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT, rd);
    HandleThumbALUSubFlags(rd, rn, op2);
}

/* Rd & Rm instructions */

void tg04_00_00(UINT32 pc, UINT32 op) /* AND Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    SET_REGISTER(rd, GET_REGISTER(rd) & GET_REGISTER(rs));
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
    R15 += 2;
}

void tg04_00_01(UINT32 pc, UINT32 op) /* EOR Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    SET_REGISTER(rd, GET_REGISTER(rd) ^ GET_REGISTER(rs));
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
    R15 += 2;
}

void tg04_00_02(UINT32 pc, UINT32 op) /* LSL Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    UINT32 rrd = GET_REGISTER(rd);
    INT32 offs = GET_REGISTER(rs) & 0x000000ff;
    if (offs > 0)
    {
        if (offs < 32)
        {
            SET_REGISTER(rd, rrd << offs);
            if (rrd & (1 << (31 - (offs - 1))))
            {
                SET_CPSR(GET_CPSR | C_MASK);
            }
            else
            {
                SET_CPSR(GET_CPSR & ~C_MASK);
            }
        }
        else if (offs == 32)
        {
            SET_REGISTER(rd, 0);
            if (rrd & 1)
            {
                SET_CPSR(GET_CPSR | C_MASK);
            }
            else
            {
                SET_CPSR(GET_CPSR & ~C_MASK);
            }
        }
        else
        {
            SET_REGISTER(rd, 0);
            SET_CPSR(GET_CPSR & ~C_MASK);
        }
    }
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
    R15 += 2;
}

void tg04_00_03(UINT32 pc, UINT32 op) /* LSR Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    UINT32 rrd = GET_REGISTER(rd);
    INT32 offs = GET_REGISTER(rs) & 0x000000ff;
    if (offs > 0)
    {
        if (offs < 32)
        {
            SET_REGISTER(rd, rrd >> offs);
            if (rrd & (1 << (offs - 1)))
            {
                SET_CPSR(GET_CPSR | C_MASK);
            }
            else
            {
                SET_CPSR(GET_CPSR & ~C_MASK);
            }
        }
        else if (offs == 32)
        {
            SET_REGISTER(rd, 0);
            if (rrd & 0x80000000)
            {
                SET_CPSR(GET_CPSR | C_MASK);
            }
            else
            {
                SET_CPSR(GET_CPSR & ~C_MASK);
            }
        }
        else
        {
            SET_REGISTER(rd, 0);
            SET_CPSR(GET_CPSR & ~C_MASK);
        }
    }
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
    R15 += 2;
}

void tg04_00_04(UINT32 pc, UINT32 op) /* ASR Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    UINT32 rrs = GET_REGISTER(rs) & 0xff;
    UINT32 rrd = GET_REGISTER(rd);
    if (rrs != 0)
    {
        if (rrs >= 32)
        {
            if (rrd >> 31)
            {
                SET_CPSR(GET_CPSR | C_MASK);
            }
            else
            {
                SET_CPSR(GET_CPSR & ~C_MASK);
            }
            SET_REGISTER(rd, (GET_REGISTER(rd) & 0x80000000) ? 0xFFFFFFFF : 0x00000000);
        }
        else
        {
            if ((rrd >> (rrs - 1)) & 1)
            {
                SET_CPSR(GET_CPSR | C_MASK);
            }
            else
            {
                SET_CPSR(GET_CPSR & ~C_MASK);
            }
            SET_REGISTER(rd, (rrd & 0x80000000)
                ? ((0xFFFFFFFF << (32 - rrs)) | (rrd >> rrs))
                : (rrd >> rrs));
        }
    }
    SET_CPSR(GET_CPSR & ~(N_MASK | Z_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
    R15 += 2;
}

void tg04_00_05(UINT32 pc, UINT32 op) /* ADC Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    UINT32 op2 = (GET_CPSR & C_MASK) ? 1 : 0;
    UINT32 rn = GET_REGISTER(rd) + GET_REGISTER(rs) + op2;
    HandleThumbALUAddFlags(rn, GET_REGISTER(rd), (GET_REGISTER(rs))); // ?
    SET_REGISTER(rd, rn);
}

void tg04_00_06(UINT32 pc, UINT32 op)  /* SBC Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    UINT32 op2 = (GET_CPSR & C_MASK) ? 0 : 1;
    UINT32 rn = GET_REGISTER(rd) - GET_REGISTER(rs) - op2;
    HandleThumbALUSubFlags(rn, GET_REGISTER(rd), (GET_REGISTER(rs))); //?
    SET_REGISTER(rd, rn);
}

void tg04_00_07(UINT32 pc, UINT32 op) /* ROR Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    UINT32 rrd = GET_REGISTER(rd);
    UINT32 imm = GET_REGISTER(rs) & 0xff;
    if (imm > 0)
    {
        SET_REGISTER(rd, ROR(rrd, imm));
        SET_CPSR(GET_CPSR | C_MASK);
    }
    else
    {
        SET_CPSR(GET_CPSR & ~C_MASK);
    }
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
    R15 += 2;
}

void tg04_00_08(UINT32 pc, UINT32 op) /* TST Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd) & GET_REGISTER(rs)));
    R15 += 2;
}

void tg04_00_09(UINT32 pc, UINT32 op) /* NEG Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    UINT32 rrs = GET_REGISTER(rs);
    SET_REGISTER(rd, 0 - rrs);
    HandleThumbALUSubFlags(GET_REGISTER(rd), 0, rrs);
}

void tg04_00_0a(UINT32 pc, UINT32 op) /* CMP Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    UINT32 rn = GET_REGISTER(rd) - GET_REGISTER(rs);
    HandleThumbALUSubFlags(rn, GET_REGISTER(rd), GET_REGISTER(rs));
}

void tg04_00_0b(UINT32 pc, UINT32 op) /* CMN Rd, Rs - check flags, add dasm */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    UINT32 rn = GET_REGISTER(rd) + GET_REGISTER(rs);
    HandleThumbALUAddFlags(rn, GET_REGISTER(rd), GET_REGISTER(rs));
}

void tg04_00_0c(UINT32 pc, UINT32 op) /* ORR Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    SET_REGISTER(rd, GET_REGISTER(rd) | GET_REGISTER(rs));
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
    R15 += 2;
}

void tg04_00_0d(UINT32 pc, UINT32 op) /* MUL Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    UINT32 rn = GET_REGISTER(rd) * GET_REGISTER(rs);
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_REGISTER(rd, rn);
    SET_CPSR(GET_CPSR | HandleALUNZFlags(rn));
    R15 += 2;
}

void tg04_00_0e(UINT32 pc, UINT32 op) /* BIC Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    SET_REGISTER(rd, GET_REGISTER(rd) & (~GET_REGISTER(rs)));
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
    R15 += 2;
}

void tg04_00_0f(UINT32 pc, UINT32 op) /* MVN Rd, Rs */
{
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    UINT32 op2 = GET_REGISTER(rs);
    SET_REGISTER(rd, ~op2);
    SET_CPSR(GET_CPSR & ~(Z_MASK | N_MASK));
    SET_CPSR(GET_CPSR | HandleALUNZFlags(GET_REGISTER(rd)));
    R15 += 2;
}

/* ADD Rd, Rs group */

void tg04_01_00(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: G4-1-0 Undefined Thumb instruction: %04x %x\n", pc, op, (op & THUMB_HIREG_H) >> THUMB_HIREG_H_SHIFT);
}

void tg04_01_01(UINT32 pc, UINT32 op) /* ADD Rd, HRs */
{
    UINT32 rs = (op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
    UINT32 rd = op & THUMB_HIREG_RD;
    SET_REGISTER(rd, GET_REGISTER(rd) + GET_REGISTER(rs + 8));
    // emulate the effects of pre-fetch
    if (rs == 7)
    {
        SET_REGISTER(rd, GET_REGISTER(rd) + 4);
    }

    R15 += 2;
}

void tg04_01_02(UINT32 pc, UINT32 op) /* ADD HRd, Rs */
{
    UINT32 rs = (op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
    UINT32 rd = op & THUMB_HIREG_RD;
    SET_REGISTER(rd + 8, GET_REGISTER(rd + 8) + GET_REGISTER(rs));
    if (rd == 7)
    {
        R15 += 2;
    }

    R15 += 2;
}

void tg04_01_03(UINT32 pc, UINT32 op) /* Add HRd, HRs */
{
    UINT32 rs = (op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
    UINT32 rd = op & THUMB_HIREG_RD;
    SET_REGISTER(rd + 8, GET_REGISTER(rd + 8) + GET_REGISTER(rs + 8));
    // emulate the effects of pre-fetch
    if (rs == 7)
    {
        SET_REGISTER(rd + 8, GET_REGISTER(rd + 8) + 4);
    }
    if (rd == 7)
    {
        R15 += 2;
    }

    R15 += 2;
}

void tg04_01_10(UINT32 pc, UINT32 op)  /* CMP Rd, Rs */
{
    UINT32 rs = GET_REGISTER(((op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT));
    UINT32 rd = GET_REGISTER(op & THUMB_HIREG_RD);
    UINT32 rn = rd - rs;
    HandleThumbALUSubFlags(rn, rd, rs);
}

void tg04_01_11(UINT32 pc, UINT32 op) /* CMP Rd, Hs */
{
    UINT32 rs = GET_REGISTER(((op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT) + 8);
    UINT32 rd = GET_REGISTER(op & THUMB_HIREG_RD);
    UINT32 rn = rd - rs;
    HandleThumbALUSubFlags(rn, rd, rs);
}

void tg04_01_12(UINT32 pc, UINT32 op) /* CMP Hd, Rs */
{
    UINT32 rs = GET_REGISTER(((op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT));
    UINT32 rd = GET_REGISTER((op & THUMB_HIREG_RD) + 8);
    UINT32 rn = rd - rs;
    HandleThumbALUSubFlags(rn, rd, rs);
}

void tg04_01_13(UINT32 pc, UINT32 op) /* CMP Hd, Hs */
{
    UINT32 rs = GET_REGISTER(((op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT) + 8);
    UINT32 rd = GET_REGISTER((op & THUMB_HIREG_RD) + 8);
    UINT32 rn = rd - rs;
    HandleThumbALUSubFlags(rn, rd, rs);
}

/* MOV group */

// "The action of H1 = 0, H2 = 0 for Op = 00 (ADD), Op = 01 (CMP) and Op = 10 (MOV) is undefined, and should not be used."
void tg04_01_20(UINT32 pc, UINT32 op) /* MOV Rd, Rs (undefined) */
{
    UINT32 rs = (op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
    UINT32 rd = op & THUMB_HIREG_RD;
    SET_REGISTER(rd, GET_REGISTER(rs));
    R15 += 2;
}

void tg04_01_21(UINT32 pc, UINT32 op) /* MOV Rd, Hs */
{
    UINT32 rs = (op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
    UINT32 rd = op & THUMB_HIREG_RD;
    SET_REGISTER(rd, GET_REGISTER(rs + 8));
    if (rs == 7)
    {
        SET_REGISTER(rd, GET_REGISTER(rd) + 4);
    }
    R15 += 2;
}

void tg04_01_22(UINT32 pc, UINT32 op) /* MOV Hd, Rs */
{
    UINT32 rs = (op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
    UINT32 rd = op & THUMB_HIREG_RD;
    SET_REGISTER(rd + 8, GET_REGISTER(rs));
    if (rd != 7)
    {
        R15 += 2;
    }
    else
    {
        R15 &= ~1;
    }
}

void tg04_01_23(UINT32 pc, UINT32 op) /* MOV Hd, Hs */
{
    UINT32 rs = (op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
    UINT32 rd = op & THUMB_HIREG_RD;
    if (rs == 7)
    {
        SET_REGISTER(rd + 8, GET_REGISTER(rs + 8) + 4);
    }
    else
    {
        SET_REGISTER(rd + 8, GET_REGISTER(rs + 8));
    }
    if (rd != 7)
    {
        R15 += 2;
    }
    else
    {
        R15 &= ~1;
    }
}

void tg04_01_30(UINT32 pc, UINT32 op)
{
    UINT32 rd = (op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
    UINT32 addr = GET_REGISTER(rd);
    if (addr & 1)
    {
        addr &= ~1;
    }
    else
    {
        SET_CPSR(GET_CPSR & ~T_MASK);
        if (addr & 2)
        {
            addr += 2;
        }
    }
    R15 = addr;
}

void tg04_01_31(UINT32 pc, UINT32 op)
{
    UINT32 rs = (op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT;
    UINT32 addr = GET_REGISTER(rs + 8);
    if (rs == 7)
    {
        addr += 2;
    }
    if (addr & 1)
    {
        addr &= ~1;
    }
    else
    {
        SET_CPSR(GET_CPSR & ~T_MASK);
        if (addr & 2)
        {
            addr += 2;
        }
    }
    R15 = addr;
}

/* BLX */
void tg04_01_32(UINT32 pc, UINT32 op)
{
    UINT32 addr = GET_REGISTER((op & THUMB_HIREG_RS) >> THUMB_HIREG_RS_SHIFT);
    SET_REGISTER(14, (R15 + 2) | 1);

    // are we also switching to ARM mode?
    if (!(addr & 1))
    {
        SET_CPSR(GET_CPSR & ~T_MASK);
        if (addr & 2)
        {
            addr += 2;
        }
    }
    else
    {
        addr &= ~1;
    }

    R15 = addr;
}

void tg04_01_33(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: G4-3 Undefined Thumb instruction: %04x\n", pc, op);
}

void tg04_0203(UINT32 pc, UINT32 op)
{
    UINT32 readword = READ32((R15 & ~2) + 4 + ((op & THUMB_INSN_IMM) << 2));
    SET_REGISTER((op & THUMB_INSN_IMM_RD) >> THUMB_INSN_IMM_RD_SHIFT, readword);
    R15 += 2;
}

/* LDR* STR* group */

void tg05_0(UINT32 pc, UINT32 op)  /* STR Rd, [Rn, Rm] */
{
    UINT32 rm = (op & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
    UINT32 rn = (op & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
    UINT32 rd = (op & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
    UINT32 addr = GET_REGISTER(rn) + GET_REGISTER(rm);
    WRITE32(addr, GET_REGISTER(rd));
    R15 += 2;
}

void tg05_1(UINT32 pc, UINT32 op)  /* STRH Rd, [Rn, Rm] */
{
    UINT32 rm = (op & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
    UINT32 rn = (op & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
    UINT32 rd = (op & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
    UINT32 addr = GET_REGISTER(rn) + GET_REGISTER(rm);
    WRITE16(addr, GET_REGISTER(rd));
    R15 += 2;
}

void tg05_2(UINT32 pc, UINT32 op)  /* STRB Rd, [Rn, Rm] */
{
    UINT32 rm = (op & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
    UINT32 rn = (op & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
    UINT32 rd = (op & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
    UINT32 addr = GET_REGISTER(rn) + GET_REGISTER(rm);
    WRITE8(addr, GET_REGISTER(rd));
    R15 += 2;
}

void tg05_3(UINT32 pc, UINT32 op)  /* LDSB Rd, [Rn, Rm] todo, add dasm */
{
    UINT32 rm = (op & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
    UINT32 rn = (op & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
    UINT32 rd = (op & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
    UINT32 addr = GET_REGISTER(rn) + GET_REGISTER(rm);
    UINT32 op2 = READ8(addr);
    if (op2 & 0x00000080)
    {
        op2 |= 0xffffff00;
    }
    SET_REGISTER(rd, op2);
    R15 += 2;
}

void tg05_4(UINT32 pc, UINT32 op)  /* LDR Rd, [Rn, Rm] */
{
    UINT32 rm = (op & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
    UINT32 rn = (op & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
    UINT32 rd = (op & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
    UINT32 addr = GET_REGISTER(rn) + GET_REGISTER(rm);
    UINT32 op2 = READ32(addr);
    SET_REGISTER(rd, op2);
    R15 += 2;
}

void tg05_5(UINT32 pc, UINT32 op)  /* LDRH Rd, [Rn, Rm] */
{
    UINT32 rm = (op & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
    UINT32 rn = (op & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
    UINT32 rd = (op & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
    UINT32 addr = GET_REGISTER(rn) + GET_REGISTER(rm);
    UINT32 op2 = READ16(addr);
    SET_REGISTER(rd, op2);
    R15 += 2;
}

void tg05_6(UINT32 pc, UINT32 op)  /* LDRB Rd, [Rn, Rm] */
{
    UINT32 rm = (op & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
    UINT32 rn = (op & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
    UINT32 rd = (op & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
    UINT32 addr = GET_REGISTER(rn) + GET_REGISTER(rm);
    UINT32 op2 = READ8(addr);
    SET_REGISTER(rd, op2);
    R15 += 2;
}

void tg05_7(UINT32 pc, UINT32 op)  /* LDSH Rd, [Rn, Rm] */
{
    UINT32 rm = (op & THUMB_GROUP5_RM) >> THUMB_GROUP5_RM_SHIFT;
    UINT32 rn = (op & THUMB_GROUP5_RN) >> THUMB_GROUP5_RN_SHIFT;
    UINT32 rd = (op & THUMB_GROUP5_RD) >> THUMB_GROUP5_RD_SHIFT;
    UINT32 addr = GET_REGISTER(rn) + GET_REGISTER(rm);
    INT32 op2 = (INT32)(INT16)(UINT16)READ16(addr & ~1);
    if ((addr & 1) && m_archRev < 5)
        op2 >>= 8;
    SET_REGISTER(rd, op2);
    R15 += 2;
}

/* Word Store w/ Immediate Offset */

void tg06_0(UINT32 pc, UINT32 op) /* Store */
{
    UINT32 rn = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = op & THUMB_ADDSUB_RD;
    INT32 offs = ((op & THUMB_LSOP_OFFS) >> THUMB_LSOP_OFFS_SHIFT) << 2;
    WRITE32(GET_REGISTER(rn) + offs, GET_REGISTER(rd));
    R15 += 2;
}

void tg06_1(UINT32 pc, UINT32 op) /* Load */
{
    UINT32 rn = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = op & THUMB_ADDSUB_RD;
    INT32 offs = ((op & THUMB_LSOP_OFFS) >> THUMB_LSOP_OFFS_SHIFT) << 2;
    SET_REGISTER(rd, READ32(GET_REGISTER(rn) + offs)); // fix
    R15 += 2;
}

/* Byte Store w/ Immeidate Offset */

void tg07_0(UINT32 pc, UINT32 op) /* Store */
{
    UINT32 rn = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = op & THUMB_ADDSUB_RD;
    INT32 offs = (op & THUMB_LSOP_OFFS) >> THUMB_LSOP_OFFS_SHIFT;
    WRITE8(GET_REGISTER(rn) + offs, GET_REGISTER(rd));
    R15 += 2;
}

void tg07_1(UINT32 pc, UINT32 op)  /* Load */
{
    UINT32 rn = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = op & THUMB_ADDSUB_RD;
    INT32 offs = (op & THUMB_LSOP_OFFS) >> THUMB_LSOP_OFFS_SHIFT;
    SET_REGISTER(rd, READ8(GET_REGISTER(rn) + offs));
    R15 += 2;
}

/* Load/Store Halfword */

void tg08_0(UINT32 pc, UINT32 op) /* Store */
{
    UINT32 imm = (op & THUMB_HALFOP_OFFS) >> THUMB_HALFOP_OFFS_SHIFT;
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    WRITE16(GET_REGISTER(rs) + (imm << 1), GET_REGISTER(rd));
    R15 += 2;
}

void tg08_1(UINT32 pc, UINT32 op) /* Load */
{
    UINT32 imm = (op & THUMB_HALFOP_OFFS) >> THUMB_HALFOP_OFFS_SHIFT;
    UINT32 rs = (op & THUMB_ADDSUB_RS) >> THUMB_ADDSUB_RS_SHIFT;
    UINT32 rd = (op & THUMB_ADDSUB_RD) >> THUMB_ADDSUB_RD_SHIFT;
    SET_REGISTER(rd, READ16(GET_REGISTER(rs) + (imm << 1)));
    R15 += 2;
}

/* Stack-Relative Load/Store */

void tg09_0(UINT32 pc, UINT32 op) /* Store */
{
    UINT32 rd = (op & THUMB_STACKOP_RD) >> THUMB_STACKOP_RD_SHIFT;
    INT32 offs = (UINT8)(op & THUMB_INSN_IMM);
    WRITE32(GET_REGISTER(13) + ((UINT32)offs << 2), GET_REGISTER(rd));
    R15 += 2;
}

void tg09_1(UINT32 pc, UINT32 op) /* Load */
{
    UINT32 rd = (op & THUMB_STACKOP_RD) >> THUMB_STACKOP_RD_SHIFT;
    INT32 offs = (UINT8)(op & THUMB_INSN_IMM);
    UINT32 readword = READ32((GET_REGISTER(13) + ((UINT32)offs << 2)) & ~3);
    SET_REGISTER(rd, readword);
    R15 += 2;
}

/* Get relative address */

void tg0a_0(UINT32 pc, UINT32 op)  /* ADD Rd, PC, #nn */
{
    UINT32 rd = (op & THUMB_RELADDR_RD) >> THUMB_RELADDR_RD_SHIFT;
    INT32 offs = (UINT8)(op & THUMB_INSN_IMM) << 2;
    SET_REGISTER(rd, ((R15 + 4) & ~2) + offs);
    R15 += 2;
}

void tg0a_1(UINT32 pc, UINT32 op) /* ADD Rd, SP, #nn */
{
    UINT32 rd = (op & THUMB_RELADDR_RD) >> THUMB_RELADDR_RD_SHIFT;
    INT32 offs = (UINT8)(op & THUMB_INSN_IMM) << 2;
    SET_REGISTER(rd, GET_REGISTER(13) + offs);
    R15 += 2;
}

/* Stack-Related Opcodes */

void tg0b_0(UINT32 pc, UINT32 op) /* ADD SP, #imm */
{
    UINT32 addr = (op & THUMB_INSN_IMM);
    addr &= ~THUMB_INSN_IMM_S;
    SET_REGISTER(13, GET_REGISTER(13) + ((op & THUMB_INSN_IMM_S) ? -(addr << 2) : (addr << 2)));
    R15 += 2;
}

void tg0b_1(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, op);
}

void tg0b_2(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, op);
}

void tg0b_3(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, op);
}

void tg0b_4(UINT32 pc, UINT32 op) /* PUSH {Rlist} */
{
    for (INT32 offs = 7; offs >= 0; offs--)
    {
        if (op & (1 << offs))
        {
            SET_REGISTER(13, GET_REGISTER(13) - 4);
            WRITE32(GET_REGISTER(13), GET_REGISTER(offs));
        }
    }
    R15 += 2;
}

void tg0b_5(UINT32 pc, UINT32 op) /* PUSH {Rlist}{LR} */
{
    SET_REGISTER(13, GET_REGISTER(13) - 4);
    WRITE32(GET_REGISTER(13), GET_REGISTER(14));
    for (INT32 offs = 7; offs >= 0; offs--)
    {
        if (op & (1 << offs))
        {
            SET_REGISTER(13, GET_REGISTER(13) - 4);
            WRITE32(GET_REGISTER(13), GET_REGISTER(offs));
        }
    }
    R15 += 2;
}

void tg0b_6(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, op);
}

void tg0b_7(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, op);
}

void tg0b_8(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, op);
}

void tg0b_9(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, op);
}

void tg0b_a(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, op);
}

void tg0b_b(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, op);
}

void tg0b_c(UINT32 pc, UINT32 op) /* POP {Rlist} */
{
    for (INT32 offs = 0; offs < 8; offs++)
    {
        if (op & (1 << offs))
        {
            SET_REGISTER(offs, READ32(GET_REGISTER(13) & ~3));
            SET_REGISTER(13, GET_REGISTER(13) + 4);
        }
    }
    R15 += 2;
}

void tg0b_d(UINT32 pc, UINT32 op) /* POP {Rlist}{PC} */
{
    for (INT32 offs = 0; offs < 8; offs++)
    {
        if (op & (1 << offs))
        {
            SET_REGISTER(offs, READ32(GET_REGISTER(13) & ~3));
            SET_REGISTER(13, GET_REGISTER(13) + 4);
        }
    }
    UINT32 addr = READ32(GET_REGISTER(13) & ~3);
    if (m_archRev < 5)
    {
        R15 = addr & ~1;
    }
    else
    {
        if (addr & 1)
        {
            addr &= ~1;
        }
        else
        {
            SET_CPSR(GET_CPSR & ~T_MASK);
            if (addr & 2)
            {
                addr += 2;
            }
        }

        R15 = addr;
    }
    SET_REGISTER(13, GET_REGISTER(13) + 4);
}

void tg0b_e(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, op);
}

void tg0b_f(UINT32 pc, UINT32 op)
{
    fatalerror("%08x: Gb Undefined Thumb instruction: %04x\n", pc, op);
}

/* Multiple Load/Store */

// "The address should normally be a word aligned quantity and non-word aligned addresses do not affect the instruction."
// "However, the bottom 2 bits of the address will appear on A[1:0] and might be interpreted by the memory system."

// Endrift says LDMIA/STMIA ignore the low 2 bits and GBA Test Suite assumes it.

void tg0c_0(UINT32 pc, UINT32 op) /* Store */
{
    UINT32 rd = (op & THUMB_MULTLS_BASE) >> THUMB_MULTLS_BASE_SHIFT;
    UINT32 ld_st_address = GET_REGISTER(rd);
    for (INT32 offs = 0; offs < 8; offs++)
    {
        if (op & (1 << offs))
        {
            WRITE32(ld_st_address & ~3, GET_REGISTER(offs));
            ld_st_address += 4;
        }
    }
    SET_REGISTER(rd, ld_st_address);
    R15 += 2;
}

void tg0c_1(UINT32 pc, UINT32 op) /* Load */
{
    UINT32 rd = (op & THUMB_MULTLS_BASE) >> THUMB_MULTLS_BASE_SHIFT;
    int rd_in_list = op & (1 << rd);
    UINT32 ld_st_address = GET_REGISTER(rd);
    for (INT32 offs = 0; offs < 8; offs++)
    {
        if (op & (1 << offs))
        {
            SET_REGISTER(offs, READ32(ld_st_address & ~3));
            ld_st_address += 4;
        }
    }
    if (!rd_in_list)
    {
        SET_REGISTER(rd, ld_st_address);
    }
    R15 += 2;
}

/* Conditional Branch */

void tg0d_0(UINT32 pc, UINT32 op) // COND_EQ:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (Z_IS_SET(GET_CPSR))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }

}

void tg0d_1(UINT32 pc, UINT32 op) // COND_NE:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (Z_IS_CLEAR(GET_CPSR))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_2(UINT32 pc, UINT32 op) // COND_CS:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (C_IS_SET(GET_CPSR))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_3(UINT32 pc, UINT32 op) // COND_CC:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (C_IS_CLEAR(GET_CPSR))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_4(UINT32 pc, UINT32 op) // COND_MI:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (N_IS_SET(GET_CPSR))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_5(UINT32 pc, UINT32 op) // COND_PL:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (N_IS_CLEAR(GET_CPSR))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_6(UINT32 pc, UINT32 op) // COND_VS:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (V_IS_SET(GET_CPSR))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_7(UINT32 pc, UINT32 op) // COND_VC:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (V_IS_CLEAR(GET_CPSR))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_8(UINT32 pc, UINT32 op) // COND_HI:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (C_IS_SET(GET_CPSR) && Z_IS_CLEAR(GET_CPSR))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_9(UINT32 pc, UINT32 op) // COND_LS:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (C_IS_CLEAR(GET_CPSR) || Z_IS_SET(GET_CPSR))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_a(UINT32 pc, UINT32 op) // COND_GE:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (!(GET_CPSR & N_MASK) == !(GET_CPSR & V_MASK))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_b(UINT32 pc, UINT32 op) // COND_LT:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (!(GET_CPSR & N_MASK) != !(GET_CPSR & V_MASK))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_c(UINT32 pc, UINT32 op) // COND_GT:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (Z_IS_CLEAR(GET_CPSR) && !(GET_CPSR & N_MASK) == !(GET_CPSR & V_MASK))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_d(UINT32 pc, UINT32 op) // COND_LE:
{
    INT32 offs = (INT8)(op & THUMB_INSN_IMM);
    if (Z_IS_SET(GET_CPSR) || !(GET_CPSR & N_MASK) != !(GET_CPSR & V_MASK))
    {
        R15 += 4 + (offs << 1);
    }
    else
    {
        R15 += 2;
    }
}

void tg0d_e(UINT32 pc, UINT32 op) // COND_AL:
{
    fatalerror("%08x: Undefined Thumb instruction: %04x (ARM9 reserved)\n", pc, op);
}

void tg0d_f(UINT32 pc, UINT32 op) // COND_NV:   // SWI (this is sort of a "hole" in the opcode encoding)
{
    ARM7.pendingSwi = 1;
    ARM7_CHECKIRQ;
}

/* B #offs */

void tg0e_0(UINT32 pc, UINT32 op)
{
    INT32 offs = (op & THUMB_BRANCH_OFFS) << 1;
    if (offs & 0x00000800)
    {
        offs |= 0xfffff800;
    }
    R15 += 4 + offs;
}

void tg0e_1(UINT32 pc, UINT32 op)
{
    /* BLX (LO) */

    UINT32 addr = GET_REGISTER(14);
    addr += (op & THUMB_BLOP_OFFS) << 1;
    addr &= 0xfffffffc;
    SET_REGISTER(14, (R15 + 2) | 1);
    R15 = addr;
    SET_CPSR(GET_CPSR & ~T_MASK);
}


void tg0f_0(UINT32 pc, UINT32 op)
{
    /* BL (HI) */

    UINT32 addr = (op & THUMB_BLOP_OFFS) << 12;
    if (addr & (1 << 22))
    {
        addr |= 0xff800000;
    }
    addr += R15 + 4;
    SET_REGISTER(14, addr);
    R15 += 2;
}

void tg0f_1(UINT32 pc, UINT32 op) /* BL */
{
    /* BL (LO) */

    UINT32 addr = GET_REGISTER(14) & ~1;
    addr += (op & THUMB_BLOP_OFFS) << 1;
    SET_REGISTER(14, (R15 + 2) | 1);
    R15 = addr;
    //R15 += 2;
}

