// 055555

// license:BSD-3-Clause
// copyright-holders:David Haywood

#include "burnint.h"
#include "konamiic.h"

static UINT8 m_regs[128];
INT32 K055555_enabled = 0;

void K055555WriteReg(UINT8 regnum, UINT8 regdat)
{
	m_regs[regnum & 0x7f] = regdat;
}

#if 0
void K055555LongWrite(INT32 offset, UINT32 data)
{
	UINT8 regnum, regdat;

	if (ACCESSING_BITS_24_31)
	{
		regnum = offset<<1;
		regdat = data>>24;
	}
	else
	{
		if (ACCESSING_BITS_8_15)
		{
			regnum = (offset<<1)+1;
			regdat = data>>8;
		}
		else
		{
			return;
		}
	}
	k055555_write_reg(regnum, regdat);
}
#endif

void K055555WordWrite(INT32 offset, UINT16 data)
{
	K055555WriteReg((offset / 2) & 0x7f, data&0xff);
}

void K055555ByteWrite(INT32 offset, UINT8 data)
{
	K055555WriteReg((offset / 2) & 0x7f, data&0xff);
}

INT32 K055555ReadRegister(INT32 regnum)
{
	return m_regs[regnum & 0x7f];
}

INT32 K055555GetPaletteIndex(INT32 idx)
{
	return m_regs[(K55_PALBASE_A + idx) & 0x7f];
}

void K055555Reset()
{
	memset(m_regs, 0, 128 * sizeof(UINT8));
}

void K055555Init()
{
	K055555_enabled = 1;
	KonamiIC_K055555InUse = 1;
}

void K055555Exit()
{
	K055555_enabled = 0;
}

void K055555Scan(INT32 nAction)
{
	struct BurnArea ba;
	
	if (nAction & ACB_DRIVER_DATA) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = m_regs;
		ba.nLen	  = 128;
		ba.szName = "K055555 Regs";
		BurnAcb(&ba);
	}
}
