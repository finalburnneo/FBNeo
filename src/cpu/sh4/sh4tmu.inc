/* SH3/4 Timer Unit */

/*#include "emu.h"
#include "debugger.h"
#include "sh4.h"
#include "sh4comn.h"
#include "sh3comn.h"
#include "sh4tmu.h"*/

static const int tcnt_div[8] = { 4, 16, 64, 256, 1024, 1, 1, 1 };

/*-------------------------------------------------
    sh4_scale_up_mame_time - multiply a attotime by
    a (constant+1) where 0 <= constant < 2^32
-------------------------------------------------*/

/*INLINE attotime sh4_scale_up_mame_time(const attotime &_time1, UINT32 factor1)
{
	return _time1 * factor1 + _time1;
}*/

static UINT32 compute_ticks_timer(int timer_num, int hertz, int divisor)
{
	//double ret=((timer->remaining().as_double() * (double)hertz) / (double)divisor) - 1;
	return (m_timer[timer_num].timeleft()) - 1;
}

static void sh4_timer_recompute(int which)
{
//	double ticks;

	UINT32 tcnt = 0;
	UINT32 tcr = 0;
	switch (which)
	{
		case 0:
			tcr = m_SH4_TCR0;
			tcnt = m_SH4_TCNT0;
			break;

		case 1:
			tcr = m_SH4_TCR1;
			tcnt = m_SH4_TCNT1;
			break;

		case 2:
			tcr = m_SH4_TCR2;
			tcnt = m_SH4_TCNT2;
			break;
	}

	//ticks = tcnt;
	//m_timer[which]->adjust(sh4_scale_up_mame_time(attotime::from_hz(m_pm_clock) * tcnt_div[tcr & 7], ticks), which);
	//bprintf(0, _T("sh4_timer_recompute(), starting timer #%d\n"), which);
	m_timer[which].set_prescaler(m_pm_divider * tcnt_div[tcr & 7] * ratio_multi);
	m_timer[which].start(tcnt, which, 1, 0);
}


static void sh4_timer_callback(int param)
{
	int which = param;
	m_cpu_off = 0; //dink
	switch (which)
	{
		case 0:
			m_SH4_TCNT0 = m_SH4_TCOR0;
			break;

		case 1:
			m_SH4_TCNT1 = m_SH4_TCOR1;
			break;

		case 2:
			m_SH4_TCNT2 = m_SH4_TCOR2;
			break;

	}

	sh4_timer_recompute(which);

	switch (which)
	{
		case 0:
			m_SH4_TCR0 |= 0x100;
			break;

		case 1:
			m_SH4_TCR1 |= 0x100;
			break;

		case 2:
			m_SH4_TCR2 |= 0x100;
			break;

	}

	switch (which)
	{
		case 0:
			if (m_SH4_TCR0 & 0x20)
			{
				sh4_exception_request(SH4_INTC_TUNI0);
			//  logerror("SH4_INTC_TUNI0 requested\n");
			}
			break;

		case 1:
			if (m_SH4_TCR1 & 0x20)
			{
				sh4_exception_request(SH4_INTC_TUNI1);
			//  logerror("SH4_INTC_TUNI1 requested\n");
			}
			break;

		case 2:
			if (m_SH4_TCR2 & 0x20)
			{
				sh4_exception_request(SH4_INTC_TUNI2);
			//  logerror("SH4_INTC_TUNI2 requested\n");
			}
			break;

	}
}


static UINT32 sh4_handle_tcnt0_addr_r(UINT32 mem_mask)
{
	if (m_SH4_TSTR & 1)
		return compute_ticks_timer(0, m_pm_clock, tcnt_div[m_SH4_TCR0 & 7]);
	else
		return m_SH4_TCNT0;
}

static UINT32 sh4_handle_tcnt1_addr_r(UINT32 mem_mask)
{
	if (m_SH4_TSTR & 2)
		return compute_ticks_timer(1, m_pm_clock, tcnt_div[m_SH4_TCR1 & 7]);
	else
		return m_SH4_TCNT1;
}

static UINT32 sh4_handle_tcnt2_addr_r(UINT32 mem_mask)
{
	if (m_SH4_TSTR & 4)
		return compute_ticks_timer(2, m_pm_clock, tcnt_div[m_SH4_TCR2 & 7]);
	else
		return m_SH4_TCNT2;
}

static UINT32 sh4_handle_tcor0_addr_r(UINT32 mem_mask)
{
	return m_SH4_TCOR0;
}

static UINT32 sh4_handle_tcor1_addr_r(UINT32 mem_mask)
{
	return m_SH4_TCOR1;
}

static UINT32 sh4_handle_tcor2_addr_r(UINT32 mem_mask)
{
	return m_SH4_TCOR2;
}

static UINT32 sh4_handle_tcr0_addr_r(UINT32 mem_mask)
{
	return m_SH4_TCR0;
}

static UINT32 sh4_handle_tcr1_addr_r(UINT32 mem_mask)
{
	return m_SH4_TCR1;
}

static UINT32 sh4_handle_tcr2_addr_r(UINT32 mem_mask)
{
	return m_SH4_TCR2;
}

static UINT32 sh4_handle_tstr_addr_r(UINT32 mem_mask)
{
	return m_SH4_TSTR;
}

static UINT32 sh4_handle_tocr_addr_r(UINT32 mem_mask)
{
	return m_SH4_TOCR;
}

static UINT32 sh4_handle_tcpr2_addr_r(UINT32 mem_mask)
{
	return m_SH4_TCPR2;
}


static void sh4_handle_tstr_addr_w(UINT32 data, UINT32 mem_mask)
{
	UINT32 old2 = m_SH4_TSTR;
	COMBINE_DATA(&m_SH4_TSTR);

	if (old2 & 1)
		m_SH4_TCNT0 = compute_ticks_timer(0, m_pm_clock, tcnt_div[m_SH4_TCR0 & 7]);
	if ((m_SH4_TSTR & 1) == 0) {
		m_timer[0].stop();
	} else
		sh4_timer_recompute(0);

	if (old2 & 2)
		m_SH4_TCNT1 = compute_ticks_timer(1, m_pm_clock, tcnt_div[m_SH4_TCR1 & 7]);
	if ((m_SH4_TSTR & 2) == 0) {
		m_timer[1].stop();
	} else
		sh4_timer_recompute(1);

	if (old2 & 4)
		m_SH4_TCNT2 = compute_ticks_timer(2, m_pm_clock, tcnt_div[m_SH4_TCR2 & 7]);
	if ((m_SH4_TSTR & 4) == 0) {
		m_timer[2].stop();
	} else
		sh4_timer_recompute(2);
}

static void sh4_handle_tcr0_addr_w(UINT32 data, UINT32 mem_mask)
{
	UINT32 old2 = m_SH4_TCR0;
	COMBINE_DATA(&m_SH4_TCR0);
	if (m_SH4_TSTR & 1)
	{
		m_SH4_TCNT0 = compute_ticks_timer(0, m_pm_clock, tcnt_div[old2 & 7]);
		sh4_timer_recompute(0);
	}
	if (!(m_SH4_TCR0 & 0x20) || !(m_SH4_TCR0 & 0x100))
		sh4_exception_unrequest(SH4_INTC_TUNI0);
}

static void sh4_handle_tcr1_addr_w(UINT32 data, UINT32 mem_mask)
{
	UINT32 old2 = m_SH4_TCR1;
	COMBINE_DATA(&m_SH4_TCR1);
	if (m_SH4_TSTR & 2)
	{
		m_SH4_TCNT1 = compute_ticks_timer(1, m_pm_clock, tcnt_div[old2 & 7]);
		sh4_timer_recompute(1);
	}
	if (!(m_SH4_TCR1 & 0x20) || !(m_SH4_TCR1 & 0x100))
		sh4_exception_unrequest(SH4_INTC_TUNI1);
}

static void sh4_handle_tcr2_addr_w(UINT32 data, UINT32 mem_mask)
{
	UINT32 old2 = m_SH4_TCR2;
	COMBINE_DATA(&m_SH4_TCR2);
	if (m_SH4_TSTR & 4)
	{
		m_SH4_TCNT2 = compute_ticks_timer(2, m_pm_clock, tcnt_div[old2 & 7]);
		sh4_timer_recompute(2);
	}
	if (!(m_SH4_TCR2 & 0x20) || !(m_SH4_TCR2 & 0x100))
		sh4_exception_unrequest(SH4_INTC_TUNI2);
}

static void sh4_handle_tcor0_addr_w(UINT32 data, UINT32 mem_mask)
{
	COMBINE_DATA(&m_SH4_TCOR0);
	if (m_SH4_TSTR & 1)
	{
		m_SH4_TCNT0 = compute_ticks_timer(0, m_pm_clock, tcnt_div[m_SH4_TCR0 & 7]);
		sh4_timer_recompute(0);
	}
}

static void sh4_handle_tcor1_addr_w(UINT32 data, UINT32 mem_mask)
{
	COMBINE_DATA(&m_SH4_TCOR1);
	if (m_SH4_TSTR & 2)
	{
		m_SH4_TCNT1 = compute_ticks_timer(1, m_pm_clock, tcnt_div[m_SH4_TCR1 & 7]);
		sh4_timer_recompute(1);
	}
}

static void sh4_handle_tcor2_addr_w(UINT32 data, UINT32 mem_mask)
{
	COMBINE_DATA(&m_SH4_TCOR2);
	if (m_SH4_TSTR & 4)
	{
		m_SH4_TCNT2 = compute_ticks_timer(2, m_pm_clock, tcnt_div[m_SH4_TCR2 & 7]);
		sh4_timer_recompute(2);
	}
}

static void sh4_handle_tcnt0_addr_w(UINT32 data, UINT32 mem_mask)
{
	COMBINE_DATA(&m_SH4_TCNT0);
	if (m_SH4_TSTR & 1)
		sh4_timer_recompute(0);
}

static void sh4_handle_tcnt1_addr_w(UINT32 data, UINT32 mem_mask)
{
	COMBINE_DATA(&m_SH4_TCNT1);
	if (m_SH4_TSTR & 2)
		sh4_timer_recompute(1);
}

static void sh4_handle_tcnt2_addr_w(UINT32 data, UINT32 mem_mask)
{
	COMBINE_DATA(&m_SH4_TCNT2);
	if (m_SH4_TSTR & 4)
		sh4_timer_recompute(2);
}

static void sh4_handle_tocr_addr_w(UINT32 data, UINT32 mem_mask)
{
	COMBINE_DATA(&m_SH4_TOCR);
}

static void sh4_handle_tcpr2_addr_w(UINT32 data, UINT32 mem_mask)
{
	COMBINE_DATA(&m_SH4_TCPR2);
}
