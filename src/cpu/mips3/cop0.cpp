/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#include <iostream>
#include <cstdint>
#include "mips3.h"
#include "mipsdef.h"
#include "memory.h"

namespace mips
{

const char *mips3::cop0_reg_names[32] = {
    "Index", "Random", "EntryLo0", "EntryLo1", "Context",
    "PageMask", "Wired", "--", "BadVAddr", "Count", "EntryHi",
    "Compare", "SR", "Cause", "EPC", "PRId", "Config", "LLAddr",
    "WatchLo", "WatchHi", "XContext", "--", "--", "--", "--","--",
    "ECC", "CacheErr", "TagLo", "TagHi", "ErrorEPC", "--"
};

enum {
    INDEX = 0,
    RANDOM,
    ENTRYLO0,
    ENTRYLO1,
    CONTEXT,
    PAGEMASK,
    WIRED,
    __COP0_UNUSED0,
    BADVADDR,
    COUNT,
    ENTRYHI,
    COMPARE,
    SR,
    CAUSE,
    EPC,
    PRId,
    CONFIG,
    LLADDR,
    WATCHLO,
    WATCHHI,
    XCONTEXT,
    __COP0_UNUSED1,
    __COP0_UNUSED2,
    __COP0_UNUSED3,
    __COP0_UNUSED4,
    __COP0_UNUSED5,
    ECC,
    CACHEERR,
    TAGLO,
    TAGHI,
    ERROREPC
} ;

#define COP0_R(x)   m_state.cpr[0][x]

void mips3::tlb_init()
{
    m_tlb = new tlb_entry[m_tlb_entries];
}

void mips3::tlb_flush()
{
    for (int i = 0; i < m_tlb_entries; i++) {
        m_tlb[i].v[0] = 0;
        m_tlb[i].v[1] = 0;
    }
}


void mips3::cop0_execute(uint32_t opcode)
{
    switch (RSNUM) {
    // MFC
    case 0x00:
        RT = COP0_R(RDNUM & 0x1F);
        break;

    // MTC
    case 0x04:
        m_state.cpr[0][RDNUM] = RT;
        break;

    // TLBWI
    case 0x10: {
        unsigned char idx = COP0_R(INDEX);
        if (idx >= 48) {
            cout << "TLBWI index > 48" << endl;
            return;
        }
        tlb_entry *e = &m_tlb[idx];
        e->b.even_lo = COP0_R(ENTRYLO0);
        e->b.odd_lo = COP0_R(ENTRYLO1);
        e->b.hi = COP0_R(ENTRYHI);
        e->b.pagemask = COP0_R(PAGEMASK);
        break;
    }

    default:
        cout << "Op: " << RSNUM << " [COP0]" << endl;
        break;
    }
}

void mips3::cop0_reset()
{
    for (int i = 0; i < 32; i++)
        m_state.cpr[0][i] = 0;
}

}
