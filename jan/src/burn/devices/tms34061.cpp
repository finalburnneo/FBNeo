/****************************************************************************
 *                                                                          *
 *  Functions to emulate the TMS34061 video controller                      *
 *                                                                          *
 *  Created by Zsolt Vasvari on 5/26/1998.                                  *
 *  Updated by Aaron Giles on 11/21/2000.                                   *
 *  Modified for use in FBA by iq_132 4/15/2014                             *
 *                                                                          *
 *  This is far from complete. See the TMS34061 User's Guide available on   *
 *  www.spies.com/arcade                                                    *
 *                                                                          *
 ****************************************************************************/

#include "burnint.h"
#include "driver.h"

/* register constants */
enum
{
	TMS34061_HORENDSYNC = 0,
	TMS34061_HORENDBLNK,
	TMS34061_HORSTARTBLNK,
	TMS34061_HORTOTAL,
	TMS34061_VERENDSYNC,
	TMS34061_VERENDBLNK,
	TMS34061_VERSTARTBLNK,
	TMS34061_VERTOTAL,
	TMS34061_DISPUPDATE,
	TMS34061_DISPSTART,
	TMS34061_VERINT,
	TMS34061_CONTROL1,
	TMS34061_CONTROL2,
	TMS34061_STATUS,
	TMS34061_XYOFFSET,
	TMS34061_XYADDRESS,
	TMS34061_DISPADDRESS,
	TMS34061_VERCOUNTER,
	TMS34061_REGCOUNT
};

static UINT8		   m_rowshift;
static UINT32              m_vramsize;
static UINT16              m_regs[TMS34061_REGCOUNT];
static UINT16              m_xmask;
static UINT8               m_yshift;
static UINT32              m_vrammask;
static UINT8 *             m_vram;
static UINT8 *             m_vram_orig;
static UINT8 *             m_latchram;
static UINT8 *             m_latchram_orig;
static UINT8               m_latchdata;
static UINT8 *             m_shiftreg;
static INT32 		   m_timer;
static void              (*m_interrupt_cb)(INT32 state);
static void              (*m_partial_update)();
INT32 			   tms34061_current_scanline;

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void tms34061_reset()
{
#if defined FBA_DEBUG
	if (!DebugDev_Tms34061Initted) bprintf(PRINT_ERROR, _T("tms34061_reset called without init\n"));
#endif

	memset (m_vram, 0, m_vramsize);
	memset (m_latchram, 0, m_vramsize);

	/* initialize registers to their default values from the manual */
	m_regs[TMS34061_HORENDSYNC]   = 0x0010;
	m_regs[TMS34061_HORENDBLNK]   = 0x0020;
	m_regs[TMS34061_HORSTARTBLNK] = 0x01f0;
	m_regs[TMS34061_HORTOTAL]     = 0x0200;
	m_regs[TMS34061_VERENDSYNC]   = 0x0004;
	m_regs[TMS34061_VERENDBLNK]   = 0x0010;
	m_regs[TMS34061_VERSTARTBLNK] = 0x00f0;
	m_regs[TMS34061_VERTOTAL]     = 0x0100;
	m_regs[TMS34061_DISPUPDATE]   = 0x0000;
	m_regs[TMS34061_DISPSTART]    = 0x0000;
	m_regs[TMS34061_VERINT]       = 0x0000;
	m_regs[TMS34061_CONTROL1]     = 0x7000;
	m_regs[TMS34061_CONTROL2]     = 0x0600;
	m_regs[TMS34061_STATUS]       = 0x0000;
	m_regs[TMS34061_XYOFFSET]     = 0x0010;
	m_regs[TMS34061_XYADDRESS]    = 0x0000;
	m_regs[TMS34061_DISPADDRESS]  = 0x0000;
	m_regs[TMS34061_VERCOUNTER]   = 0x0000;

	/* start vertical interrupt timer */
	m_timer = -1; // disable
}

void tms34061_init(UINT8 rowshift, UINT32 ram_size, void (*partial_update)(), void (*callback)(INT32 state))
{
	DebugDev_Tms34061Initted = 1;
	
	m_partial_update = partial_update;
	m_rowshift = rowshift;
	m_vramsize = ram_size;

	/* resolve callbak */
	m_interrupt_cb = callback;

	/* reset the data */
	m_vrammask = m_vramsize - 1;

	/* allocate memory for VRAM */
	m_vram = m_vram_orig = (UINT8*)BurnMalloc(m_vramsize + 256 * 2);

	/* allocate memory for latch RAM */
	m_latchram = m_latchram_orig = (UINT8*)BurnMalloc(m_vramsize + 256 * 2);

	/* add some buffer space for VRAM and latch RAM */
	m_vram += 256;
	m_latchram += 256;

	/* point the shift register to the base of VRAM for now */
	m_shiftreg = m_vram;
}

void tms34061_exit()
{
#if defined FBA_DEBUG
	if (!DebugDev_Tms34061Initted) bprintf(PRINT_ERROR, _T("tms34061_exit called without init\n"));
#endif

	BurnFree(m_vram_orig);
	m_vram = NULL;
	BurnFree(m_latchram_orig);
	m_latchram = NULL;
	
	DebugDev_Tms34061Initted = 0;
}

/*************************************
 *
 *  Interrupt handling
 *
 *************************************/

static void update_interrupts()
{
	/* if we have a callback, process it */
	if (m_interrupt_cb)
	{
		/* if the status bit is set, and ints are enabled, turn it on */
		if ((m_regs[TMS34061_STATUS] & 0x0001) && (m_regs[TMS34061_CONTROL1] & 0x0400))
			m_interrupt_cb(ASSERT_LINE);
		else
			m_interrupt_cb(CLEAR_LINE);
	}
}


void tms34061_interrupt()
{
#if defined FBA_DEBUG
	if (!DebugDev_Tms34061Initted) bprintf(PRINT_ERROR, _T("tms34061_interrupt called without init\n"));
#endif

	if (tms34061_current_scanline != m_timer) return;

	/* set the interrupt bit in the status reg */
	m_regs[TMS34061_STATUS] |= 1;

	/* update the interrupt state */
	update_interrupts();
}



/*************************************
 *
 *  Register writes
 *
 *************************************/

static void register_w(INT32 offset, UINT8 data)
{
	INT32 scanline;
	INT32 regnum = offset >> 2;

	/* certain registers affect the display directly */
	if ((regnum >= TMS34061_HORENDSYNC && regnum <= TMS34061_DISPSTART) ||
		(regnum == TMS34061_CONTROL2))
		m_partial_update();

	/* store the hi/lo half */
	if (regnum < TMS34061_REGCOUNT)
	{
		if (offset & 0x02)
			m_regs[regnum] = (m_regs[regnum] & 0x00ff) | (data << 8);
		else
			m_regs[regnum] = (m_regs[regnum] & 0xff00) | data;
	}

	/* update the state of things */
	switch (regnum)
	{
		/* vertical interrupt: adjust the timer */
		case TMS34061_VERINT:
			scanline = m_regs[TMS34061_VERINT] - m_regs[TMS34061_VERENDBLNK];

			if (scanline < 0)
				scanline += m_regs[TMS34061_VERTOTAL];

			// FBA -- this isn't accurate, it will only trigger on a per-scanline basis, but should be
			// scanline and horizontal position in the scanline. It doesn't seem to bother the tested
			// games much though, so don't worry about it
			m_timer = scanline; //m_regs[TMS34061_HORSTARTBLNK];
		//	m_timer->adjust(m_screen->time_until_pos(scanline, m_regs[TMS34061_HORSTARTBLNK]));
			break;

		/* XY offset: set the X and Y masks */
		case TMS34061_XYOFFSET:
			switch (m_regs[TMS34061_XYOFFSET] & 0x00ff)
			{
				case 0x01:  m_yshift = 2;    break;
				case 0x02:  m_yshift = 3;    break;
				case 0x04:  m_yshift = 4;    break;
				case 0x08:  m_yshift = 5;    break;
				case 0x10:  m_yshift = 6;    break;
				case 0x20:  m_yshift = 7;    break;
				case 0x40:  m_yshift = 8;    break;
				case 0x80:  m_yshift = 9;    break;
				default:    /*logerror("Invalid value for XYOFFSET = %04x\n", m_regs[TMS34061_XYOFFSET]);*/  break;
			}
			m_xmask = (1 << m_yshift) - 1;
			break;

		/* CONTROL1: they could have turned interrupts on */
		case TMS34061_CONTROL1:
			update_interrupts();
			break;

		/* other supported registers */
		case TMS34061_XYADDRESS:
			break;
	}
}



/*************************************
 *
 *  Register reads
 *
 *************************************/

static UINT8 register_r(INT32 offset)
{
	INT32 regnum = offset >> 2;
	UINT16 result;

	/* extract the correct portion of the register */
	if (regnum < TMS34061_REGCOUNT)
		result = m_regs[regnum];
	else
		result = 0xffff;

	/* special cases: */
	switch (regnum)
	{
		/* status register: a read here clears it */
		case TMS34061_STATUS:
			m_regs[TMS34061_STATUS] = 0;
			update_interrupts();
			break;

		/* vertical count register: return the current scanline */
		case TMS34061_VERCOUNTER:
			result = (tms34061_current_scanline + m_regs[TMS34061_VERENDBLNK]) % m_regs[TMS34061_VERTOTAL];
			break;
	}

	/* log it */
	return (offset & 0x02) ? (result >> 8) : result;
}



/*************************************
 *
 *  XY addressing
 *
 *************************************/

static void adjust_xyaddress(INT32 offset)
{
	/* note that carries are allowed if the Y coordinate isn't being modified */
	switch (offset & 0x1e)
	{
		case 0x00:  /* no change */
			break;

		case 0x02:  /* X + 1 */
			m_regs[TMS34061_XYADDRESS]++;
			break;

		case 0x04:  /* X - 1 */
			m_regs[TMS34061_XYADDRESS]--;
			break;

		case 0x06:  /* X = 0 */
			m_regs[TMS34061_XYADDRESS] &= ~m_xmask;
			break;

		case 0x08:  /* Y + 1 */
			m_regs[TMS34061_XYADDRESS] += 1 << m_yshift;
			break;

		case 0x0a:  /* X + 1, Y + 1 */
			m_regs[TMS34061_XYADDRESS] = (m_regs[TMS34061_XYADDRESS] & ~m_xmask) |
					((m_regs[TMS34061_XYADDRESS] + 1) & m_xmask);
			m_regs[TMS34061_XYADDRESS] += 1 << m_yshift;
			break;

		case 0x0c:  /* X - 1, Y + 1 */
			m_regs[TMS34061_XYADDRESS] = (m_regs[TMS34061_XYADDRESS] & ~m_xmask) |
					((m_regs[TMS34061_XYADDRESS] - 1) & m_xmask);
			m_regs[TMS34061_XYADDRESS] += 1 << m_yshift;
			break;

		case 0x0e:  /* X = 0, Y + 1 */
			m_regs[TMS34061_XYADDRESS] &= ~m_xmask;
			m_regs[TMS34061_XYADDRESS] += 1 << m_yshift;
			break;

		case 0x10:  /* Y - 1 */
			m_regs[TMS34061_XYADDRESS] -= 1 << m_yshift;
			break;

		case 0x12:  /* X + 1, Y - 1 */
			m_regs[TMS34061_XYADDRESS] = (m_regs[TMS34061_XYADDRESS] & ~m_xmask) |
					((m_regs[TMS34061_XYADDRESS] + 1) & m_xmask);
			m_regs[TMS34061_XYADDRESS] -= 1 << m_yshift;
			break;

		case 0x14:  /* X - 1, Y - 1 */
			m_regs[TMS34061_XYADDRESS] = (m_regs[TMS34061_XYADDRESS] & ~m_xmask) |
					((m_regs[TMS34061_XYADDRESS] - 1) & m_xmask);
			m_regs[TMS34061_XYADDRESS] -= 1 << m_yshift;
			break;

		case 0x16:  /* X = 0, Y - 1 */
			m_regs[TMS34061_XYADDRESS] &= ~m_xmask;
			m_regs[TMS34061_XYADDRESS] -= 1 << m_yshift;
			break;

		case 0x18:  /* Y = 0 */
			m_regs[TMS34061_XYADDRESS] &= m_xmask;
			break;

		case 0x1a:  /* X + 1, Y = 0 */
			m_regs[TMS34061_XYADDRESS]++;
			m_regs[TMS34061_XYADDRESS] &= m_xmask;
			break;

		case 0x1c:  /* X - 1, Y = 0 */
			m_regs[TMS34061_XYADDRESS]--;
			m_regs[TMS34061_XYADDRESS] &= m_xmask;
			break;

		case 0x1e:  /* X = 0, Y = 0 */
			m_regs[TMS34061_XYADDRESS] = 0;
			break;
	}
}


static void xypixel_w(INT32 offset, UINT8 data)
{
	/* determine the offset, then adjust it */
	INT32 pixeloffs = m_regs[TMS34061_XYADDRESS];
	if (offset)
		adjust_xyaddress(offset);

	/* adjust for the upper bits */
	pixeloffs |= (m_regs[TMS34061_XYOFFSET] & 0x0f00) << 8;

	/* mask to the VRAM size */
	pixeloffs &= m_vrammask;

	/* set the pixel data */
	m_vram[pixeloffs] = data;
	m_latchram[pixeloffs] = m_latchdata;
}


static UINT8 xypixel_r(INT32 offset)
{
	/* determine the offset, then adjust it */
	INT32 pixeloffs = m_regs[TMS34061_XYADDRESS];
	if (offset)
		adjust_xyaddress(offset);

	/* adjust for the upper bits */
	pixeloffs |= (m_regs[TMS34061_XYOFFSET] & 0x0f00) << 8;

	/* mask to the VRAM size */
	pixeloffs &= m_vrammask;

	/* return the result */
	return m_vram[pixeloffs];
}



/*************************************
 *
 *  Core writes
 *
 *************************************/

void tms34061_write(INT32 col, INT32 row, INT32 func, UINT8 data)
{
#if defined FBA_DEBUG
	if (!DebugDev_Tms34061Initted) bprintf(PRINT_ERROR, _T("tms34061_write called without init\n"));
#endif

	INT32 offs;

	/* the function code determines what to do */
	switch (func)
	{
		/* both 0 and 2 map to register access */
		case 0:
		case 2:
			register_w(col, data);
			break;

		/* function 1 maps to XY access; col is the address adjustment */
		case 1:
			xypixel_w(col, data);
			break;

		/* function 3 maps to direct access */
		case 3:
			offs = ((row << m_rowshift) | col) & m_vrammask;
			if (m_regs[TMS34061_CONTROL2] & 0x0040)
				offs |= (m_regs[TMS34061_CONTROL2] & 3) << 16;

			if (m_vram[offs] != data || m_latchram[offs] != m_latchdata)
			{
				m_vram[offs] = data;
				m_latchram[offs] = m_latchdata;
			}
			break;

		/* function 4 performs a shift reg transfer to VRAM */
		case 4:
			offs = col << m_rowshift;
			if (m_regs[TMS34061_CONTROL2] & 0x0040)
				offs |= (m_regs[TMS34061_CONTROL2] & 3) << 16;
			offs &= m_vrammask;

			memcpy(&m_vram[offs], m_shiftreg, (size_t)1 << m_rowshift);
			memset(&m_latchram[offs], m_latchdata, (size_t)1 << m_rowshift);
			break;

		/* function 5 performs a shift reg transfer from VRAM */
		case 5:
			offs = col << m_rowshift;
			if (m_regs[TMS34061_CONTROL2] & 0x0040)
				offs |= (m_regs[TMS34061_CONTROL2] & 3) << 16;
			offs &= m_vrammask;

			m_shiftreg = &m_vram[offs];
			break;

		/* log anything else */
		default:
			break;
	}
}


UINT8 tms34061_read(INT32 col, INT32 row, INT32 func)
{
#if defined FBA_DEBUG
	if (!DebugDev_Tms34061Initted) bprintf(PRINT_ERROR, _T("tms34061_read called without init\n"));
#endif

	INT32 result = 0;
	INT32 offs;

	/* the function code determines what to do */
	switch (func)
	{
		/* both 0 and 2 map to register access */
		case 0:
		case 2:
			result = register_r(col);
			break;

		/* function 1 maps to XY access; col is the address adjustment */
		case 1:
			result = xypixel_r(col);
			break;

		/* funtion 3 maps to direct access */
		case 3:
			offs = ((row << m_rowshift) | col) & m_vrammask;
			result = m_vram[offs];
			break;

		/* function 4 performs a shift reg transfer to VRAM */
		case 4:
			offs = col << m_rowshift;
			if (m_regs[TMS34061_CONTROL2] & 0x0040)
				offs |= (m_regs[TMS34061_CONTROL2] & 3) << 16;
			offs &= m_vrammask;

			memcpy(&m_vram[offs], m_shiftreg, (size_t)1 << m_rowshift);
			memset(&m_latchram[offs], m_latchdata, (size_t)1 << m_rowshift);
			break;

		/* function 5 performs a shift reg transfer from VRAM */
		case 5:
			offs = col << m_rowshift;
			if (m_regs[TMS34061_CONTROL2] & 0x0040)
				offs |= (m_regs[TMS34061_CONTROL2] & 3) << 16;
			offs &= m_vrammask;

			m_shiftreg = &m_vram[offs];
			break;

		/* log anything else */
		default:
			break;
	}

	return result;
}



/*************************************
 *
 *  Misc functions
 *
 *************************************/

UINT8 tms34061_latch_read()
{
#if defined FBA_DEBUG
	if (!DebugDev_Tms34061Initted) bprintf(PRINT_ERROR, _T("tms34061_latch_read called without init\n"));
#endif

	return m_latchdata;
}


void tms34061_latch_write(UINT8 data)
{
#if defined FBA_DEBUG
	if (!DebugDev_Tms34061Initted) bprintf(PRINT_ERROR, _T("tms34061_latch_write called without init\n"));
#endif

	m_latchdata = data;
}

INT32 tms34061_display_blanked()
{
#if defined FBA_DEBUG
	if (!DebugDev_Tms34061Initted) bprintf(PRINT_ERROR, _T("tms34061_display_blanked called without init\n"));
#endif

	return (~m_regs[TMS34061_CONTROL2] >> 13) & 1;
}

UINT8 *tms34061_get_vram_pointer()
{
#if defined FBA_DEBUG
	if (!DebugDev_Tms34061Initted) bprintf(PRINT_ERROR, _T("tms34061_get_vram_pointer called without init\n"));
#endif

	return m_vram;
}


/*************************************
 *
 *  Save function
 *
 *************************************/

INT32 tms34061_scan(INT32 nAction, INT32 *)
{
#if defined FBA_DEBUG
	if (!DebugDev_Tms34061Initted) bprintf(PRINT_ERROR, _T("tms34061_scan called without init\n"));
#endif

	struct BurnArea ba;

	if (nAction & ACB_VOLATILE) {		
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = m_vram;
		ba.nLen	  = m_vramsize+256;
		ba.szName = "tms34061 video ram";
		BurnAcb(&ba);

		ba.Data	  = m_latchram;
		ba.nLen	  = m_vramsize+256;
		ba.szName = "tms34061 latch ram";
		BurnAcb(&ba);

		ba.Data	  = m_regs;
		ba.nLen	  = TMS34061_REGCOUNT * sizeof(UINT16);
		ba.szName = "tms34061 registers";
		BurnAcb(&ba);

		SCAN_VAR(m_xmask);
		SCAN_VAR(m_yshift);
		SCAN_VAR(m_latchdata);
		SCAN_VAR(m_timer);
	}

	return 0;
}
