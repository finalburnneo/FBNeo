#include "burnint.h"
#include "driver.h"
#include "atmel_arm_aic.h"

static UINT32 m_aic_smr[32];
static UINT32 m_aic_svr[32];

static UINT32 m_irqs_enabled;
static UINT32 m_irqs_pending;
static UINT32 m_current_irq_vector;
static UINT32 m_current_firq_vector;
static UINT32 m_status;
static UINT32 m_core_status;
static UINT32 m_spurious_vector;
static UINT32 m_debug;
static UINT32 m_fast_irqs;
static INT32 m_lvlidx;
static INT32 m_level_stack[9];

static void (*ArmAicIrqCcallback)(INT32) = NULL;

void ArmAicSetIrqCallback(void (*irqcallback)(INT32)) {
    ArmAicIrqCcallback = irqcallback;
}

static void push_level(int lvl) 
{
    m_level_stack[++m_lvlidx] = lvl;
};

static void pop_level()
{
    if (m_lvlidx) 
        --m_lvlidx;
};

static void set_lines() {
    if (ArmAicIrqCcallback) {
        ArmAicIrqCcallback((m_core_status & ~m_debug & 2) ? ASSERT_LINE : CLEAR_LINE);
    }
}

static UINT32 irq_vector_r()
{
    UINT32 mask = m_irqs_enabled & m_irqs_pending & ~m_fast_irqs;
    m_current_irq_vector = m_spurious_vector;
    if (mask)
    {
        // looking for highest level pending interrupt, bigger than current
        INT32 pri = m_level_stack[m_lvlidx];
        INT32 midx = -1;
        do
        {
            UINT8 idx = 31 - count_leading_zeros_32(mask);
            if ((INT32)(m_aic_smr[idx] & 7) >= pri)
            {
                midx = idx;
                pri = m_aic_smr[idx] & 7;
            }
            mask ^= 1 << idx;
        } while (mask);

        if (midx > 0)
        {
            m_status = midx;
            m_current_irq_vector = m_aic_svr[midx];
            // note: Debug PROTect mode not implemented (new level pushed on stack and pending line clear only when this register writen after read)
            push_level(m_aic_smr[midx] & 7);
            if (m_aic_smr[midx] & 0x20)         // auto clear pending if edge trigger mode
                m_irqs_pending ^= 1 << midx;
        }
    }

    m_core_status &= ~2;
    set_lines();
    return m_current_irq_vector;
}

static UINT32 firq_vector_r()
{
    m_current_firq_vector = (m_irqs_enabled & m_irqs_pending & m_fast_irqs) ? m_aic_svr[0] : m_spurious_vector;
    return m_current_firq_vector;
}

static void check_irqs()
{
    m_core_status = 0;

    UINT32 mask = m_irqs_enabled & m_irqs_pending & ~m_fast_irqs;
    if (mask)
    {
        // check if we have pending interrupt with level more than current
        int pri = m_level_stack[m_lvlidx];
        do
        {
            UINT8 idx = 31 - count_leading_zeros_32(mask);
            if ((int)(m_aic_smr[idx] & 7) > pri)
            {
                m_core_status |= 2;
                break;
            }
            mask ^= 1 << idx;
        } while (mask);
    }

    if (m_irqs_enabled & m_irqs_pending & m_fast_irqs)
        m_core_status |= 1;

    set_lines();
}

void ArmAicSetIrq(int line, int state)
{
    // note: might be incorrect if edge triggered mode set
    if (state == CPU_IRQSTATUS_ACK)
        m_irqs_pending |= 1 << line;
    else
        if (m_aic_smr[line] & 0x40)
            m_irqs_pending &= ~(1 << line);

    check_irqs();
}

UINT32 ArmAicRead(UINT32 sekAddress)
{
    sekAddress &= 0xfff;
    if (sekAddress < 0x80) {
        UINT32 idx = sekAddress >> 2;
        return m_aic_smr[idx];
    } else if (sekAddress >= 0x80 && sekAddress < 0x100) {
        UINT32 idx = (sekAddress - 0x80) >> 2;
        return m_aic_svr[idx];
    } else {
        switch (sekAddress) {
            case 0x100:
                return irq_vector_r();
            case 0x104:
                return firq_vector_r();
            case 0x108:
                return m_status;
            case 0x10c:
                return m_irqs_pending;
            case 0x110:
                return m_irqs_enabled;
            case 0x114:
                return m_core_status;
            case 0x148:
                return m_fast_irqs;
        }
    }
    return 0;
}

void ArmAicWrite(UINT32 sekAddress, UINT32 data, UINT32 mem_mask)
{
    sekAddress &= 0xfff;
    if (sekAddress < 0x80) {
        UINT32 idx = sekAddress >> 2;
        m_aic_smr[idx] = data;
    } else if (sekAddress >= 0x80 && sekAddress < 0x100) {
        UINT32 idx = (sekAddress - 0x80) >> 2;
        m_aic_svr[idx] = data;
    } else {
        switch (sekAddress) {
            case 0x120:
                m_irqs_enabled |= (data & mem_mask);
                check_irqs();
                break;
            case 0x124:
                m_irqs_enabled &= ~(data & mem_mask);
                check_irqs();
                break;
            case 0x128:
                m_irqs_pending &= ~(data & mem_mask);
                check_irqs();
                break;
            case 0x12c:
                m_irqs_pending |= (data & mem_mask);
                check_irqs();
                break;
            case 0x130:
                m_status = 0;
                pop_level();
                check_irqs();
                break;
            case 0x134:
                COMBINE_DATA(&m_spurious_vector);
                break;
            case 0x138:
                COMBINE_DATA(&m_debug);
                check_irqs();
                break;
            case 0x140:
                m_fast_irqs |= (data & mem_mask);
                check_irqs();
                break;
            case 0x144:
                m_fast_irqs &= ~(data & mem_mask) | 1;
                check_irqs();
                break;
        }
    }
}

void ArmAicReset()
{
    m_irqs_enabled = 0;
    m_irqs_pending = 0;
    m_current_irq_vector = 0;
    m_current_firq_vector = 0;
    m_status = 0;
    m_core_status = 0;
    m_spurious_vector = 0;
    m_debug = 0;
    m_fast_irqs = 1;
    m_lvlidx = 0;

    for(auto & elem : m_aic_smr) { elem = 0; }
    for(auto & elem : m_aic_svr) { elem = 0; }
    for(auto & elem : m_level_stack) { elem = 0; }

    m_level_stack[0] = -1;
}

INT32 ArmAicScan()
{
    SCAN_VAR(m_aic_smr);
    SCAN_VAR(m_aic_svr);
    SCAN_VAR(m_level_stack);
    SCAN_VAR(m_irqs_enabled);
    SCAN_VAR(m_irqs_pending);
    SCAN_VAR(m_current_irq_vector);
    SCAN_VAR(m_current_firq_vector);
    SCAN_VAR(m_status);
    SCAN_VAR(m_core_status);
    SCAN_VAR(m_spurious_vector);
    SCAN_VAR(m_debug);
    SCAN_VAR(m_fast_irqs);
    SCAN_VAR(m_lvlidx);
    
    return 0;
}
