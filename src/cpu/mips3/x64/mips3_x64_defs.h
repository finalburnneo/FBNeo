/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#ifndef MIPS3_X64_DEFS
#define MIPS3_X64_DEFS

#include <stddef.h>
#include "../mipsdef.h"

#define DEBUG_DRC   1

#if DEBUG_DRC
# define drc_log(...)   printf("drc: " __VA_ARGS__); fflush(stdout)
#else
# define drc_log(...)
#endif

#define drc_log_error(...)  printf("drc_err: " __VA_ARGS__); fflush(stdout)

#define DEBUG_CALL(f)   \
    mov(rax, (size_t)(void*)&f);  \
    call(rax);

#define CORE_PC     (m_core->m_state.pc)
#define RS_ref      ((size_t)&m_core->m_state.r[RSNUM])
#define RD_ref      ((size_t)&m_core->m_state.r[RDNUM])
#define RT_ref      ((size_t)&m_core->m_state.r[RTNUM])
#define LO_ref      ((size_t)&m_core->m_state.lo)
#define HI_ref      ((size_t)&m_core->m_state.hi)
#define R_ref(n)    ((size_t)&m_core->m_state.r[n])
#define ADR(n)      ((size_t)&n)

namespace mips {	
template<class C, class R, typename...Args>
size_t get_method_addr(R (C::*f)(Args...)) {
	return reinterpret_cast<size_t>((void* &)f);
}
}

#define F_ADR(f)    ((size_t)(void*)&f)
#define M_ADR(f)    (mips::get_method_addr(&f))

#define FPR_ref(n)  ((size_t)&m_core->m_state.cpr[1][n])
#define FCR_ref(n)  ((size_t)&m_core->m_state.fcr[n])

#define Rn_x(n)     ptr[rbx + ((size_t)offsetof(mips3::cpu_state, r[n]))]
#define RS_x        ptr[rbx + ((size_t)offsetof(mips3::cpu_state, r[RSNUM]))]
#define RD_x        ptr[rbx + ((size_t)offsetof(mips3::cpu_state, r[RDNUM]))]
#define RT_x        ptr[rbx + ((size_t)offsetof(mips3::cpu_state, r[RTNUM]))]
#define LO_x        ptr[rbx + ((size_t)offsetof(mips3::cpu_state, lo))]
#define HI_x        ptr[rbx + ((size_t)offsetof(mips3::cpu_state, hi))]
#define PC_x        ptr[rbx + ((size_t)offsetof(mips3::cpu_state, pc))]

#define COP0_x(n)   ptr[rbx + ((size_t)offsetof(mips3::cpu_state, cpr[0][n]))]
#define COP1_x(n)   ptr[rbx + ((size_t)offsetof(mips3::cpu_state, cpr[1][n]))]

#define TOTAL_x     ptr[rbx + ((size_t)offsetof(mips3::cpu_state, total_cycles))]
#define RSTCYC_x    ptr[rbx + ((size_t)offsetof(mips3::cpu_state, reset_cycle))]

#define RS_q        qword[rbx + ((size_t)offsetof(mips3::cpu_state, r[RSNUM]))]
#define RD_q        qword[rbx + ((size_t)offsetof(mips3::cpu_state, r[RDNUM]))]
#define RT_q        qword[rbx + ((size_t)offsetof(mips3::cpu_state, r[RTNUM]))]
#define PC_q        qword[rbx + ((size_t)offsetof(mips3::cpu_state, pc))]

#define COP0_q(n)   qword[rbx + ((size_t)offsetof(mips3::cpu_state, cpr[0][n]))]
#define COP1_q(n)   qword[rbx + ((size_t)offsetof(mips3::cpu_state, cpr[1][n]))]

#define FD_x        ptr[rbx + ((size_t)offsetof(mips3::cpu_state, cpr[1][FDNUM]))]
#define FS_x        ptr[rbx + ((size_t)offsetof(mips3::cpu_state, cpr[1][FSNUM]))]
#define FT_x        ptr[rbx + ((size_t)offsetof(mips3::cpu_state, cpr[1][FTNUM]))]
#define FCR31_x     ptr[rbx + ((size_t)offsetof(mips3::cpu_state, fcr[31]))]

#define TOTAL_q     qword[rbx + ((size_t)offsetof(mips3::cpu_state, total_cycles))]
#define RSTCYC_q    qword[rbx + ((size_t)offsetof(mips3::cpu_state, reset_cycle))]

#endif // MIPS3_X64_DEFS

