/*
 * Copyright (c) 2015, Marcos Medeiros
 * Licensed under BSD 3-clause.
 */
#include <iostream>
#include "mips3.h"
#include "mipsdef.h"
#include "mips3_memory.h"

namespace mips
{

const char *mips3::cop0_reg_names[32] = {
    "Index", "Random", "EntryLo0", "EntryLo1", "Context",
    "PageMask", "Wired", "--", "BadVAddr", "Count", "EntryHi",
    "Compare", "SR", "Cause", "EPC", "PRId", "Config", "LLAddr",
    "WatchLo", "WatchHi", "XContext", "--", "--", "--", "--","--",
    "ECC", "CacheErr", "TagLo", "TagHi", "ErrorEPC", "--"
};

#define CR(x)   m_state.cpr[0][x]

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
        if (RTNUM) {
            if (RDNUM == COP0_Count) {
                RT = (uint32_t) ((m_state.total_cycles - m_state.reset_cycle) / 2);
                return;
            }
            RT = CR(RDNUM);
        }
        break;

    // MTC
    case 0x04:
        m_state.cpr[0][RDNUM] = RT;
        if (RDNUM == COP0_Count) {
            m_state.reset_cycle = m_state.total_cycles - ((uint64_t)(uint32_t)RT * 2);
        }
        break;

    // TLBWI
    case 0x10: {
        unsigned char idx = CR(COP0_Index);
        if (idx >= 48) {
            cout << "TLBWI index > 48" << endl;
            return;
        }
        tlb_entry *e = &m_tlb[idx];
        e->b.even_lo = CR(COP0_EntryLo0);
        e->b.odd_lo = CR(COP0_EntryLo1);
        e->b.hi = CR(COP0_EntryHi);
        e->b.pagemask = CR(COP0_PageMask);
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
