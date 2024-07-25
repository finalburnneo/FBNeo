/*****************************************************************************
 *
 *   arm7exec.c
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
 *
 *****************************************************************************/

/******************************************************************************
 *  Notes:
 *         This file contains the code to run during the CPU EXECUTE METHOD.
 *         It has been split into it's own file (from the arm7core.c) so it can be
 *         directly compiled into any cpu core that wishes to use it.
 *
 *         It should be included as follows in your cpu core:
 *
 *         int arm7_execute(int cycles)
 *         {
 *         #include "arm7exec.c"
 *         }
 *
*****************************************************************************/

/* This implementation uses an improved switch() for hopefully faster opcode fetches compared to my last version
.. though there's still room for improvement. */
{
    UINT32 pc;
    UINT32 insn;

    ARM7_ICOUNT = cycles;
    curr_cycles = cycles;
	end_run = 0;

    do
    {
        /* handle Thumb instructions if active */
        if (T_IS_SET(GET_CPSR))
        {
            pc = GET_PC;
            insn = cpu_readop16(pc & (~1));

            (*thumb_handler[(insn & 0xffc0) >> 6])(pc, insn);
        }
        else
        {

            /* load 32 bit instruction */
            pc = GET_PC;
            insn = cpu_readop32(pc & (~3));
            INT32 op_offset = 0;
            /* process condition codes for this instruction */
            switch (insn >> INSN_COND_SHIFT)
            {
            case COND_EQ:
                if (Z_IS_CLEAR(GET_CPSR))
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_NE:
                if (Z_IS_SET(GET_CPSR))
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_CS:
                if (C_IS_CLEAR(GET_CPSR))
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_CC:
                if (C_IS_SET(GET_CPSR))
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_MI:
                if (N_IS_CLEAR(GET_CPSR))
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_PL:
                if (N_IS_SET(GET_CPSR))
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_VS:
                if (V_IS_CLEAR(GET_CPSR))
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_VC:
                if (V_IS_SET(GET_CPSR))
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_HI:
                if (C_IS_CLEAR(GET_CPSR) || Z_IS_SET(GET_CPSR))
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_LS:
                if (C_IS_SET(GET_CPSR) && Z_IS_CLEAR(GET_CPSR))
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_GE:
                if (!(GET_CPSR & N_MASK) != !(GET_CPSR & V_MASK)) /* Use x ^ (x >> ...) method */
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_LT:
                if (!(GET_CPSR & N_MASK) == !(GET_CPSR & V_MASK))
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_GT:
                if (Z_IS_SET(GET_CPSR) || (!(GET_CPSR & N_MASK) != !(GET_CPSR & V_MASK)))
                {
                    UNEXECUTED();  goto L_Next;
                }
                break;
            case COND_LE:
                if (Z_IS_CLEAR(GET_CPSR) && (!(GET_CPSR & N_MASK) == !(GET_CPSR & V_MASK)))
                { UNEXECUTED();  goto L_Next; }
                break;
            case COND_NV:
                if (m_archRev < 5)
                {
                    UNEXECUTED();  goto L_Next;
                }
                else 
                {
                    op_offset = 0x10;
                }
            }
            /*******************************************************************/
            /* If we got here - condition satisfied, so decode the instruction */
            /*******************************************************************/
            (*ops_handler[((insn & 0xF000000) >> 24) + op_offset])(insn);
        }

    L_Next:
        ARM7_CHECKIRQ;

        /* All instructions remove 3 cycles.. Others taking less / more will have adjusted this # prior to here */
        ARM7_ICOUNT -= 3;
    } while (ARM7_ICOUNT > 0 && !end_run);

	cycles = curr_cycles - ARM7_ICOUNT;
	total_cycles += cycles;
	curr_cycles = ARM7_ICOUNT = 0;

    return cycles;
}
