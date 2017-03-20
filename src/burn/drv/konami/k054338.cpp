// 054338

// license:BSD-3-Clause
// copyright-holders:David Haywood

#include "tiles_generic.h"
#include "konamiic.h"

static UINT16 k54338_regs[32];
static INT32 m_shd_rgb[9];

static INT32 k054338_alphainverted;

void K054338Reset()
{
	memset(k54338_regs, 0, sizeof(UINT16)*32);
	memset (m_shd_rgb, 0, sizeof(INT32)*9);
}

void K054338Exit()
{

}

void K054338Scan(INT32 nAction)
{
	struct BurnArea ba;
	
	if (nAction & ACB_DRIVER_DATA) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = (UINT8*)k54338_regs;
		ba.nLen	  = 32 * sizeof(INT16);
		ba.szName = "K054338 Regs";
		BurnAcb(&ba);
	}

//	SCAN_VARS(m_shd_rgb);
}

void K054338Init()
{
	KonamiIC_K054338InUse = 1;
	KonamiAllocateBitmaps();

	K054338Reset();
	k054338_alphainverted = 1;
}

void K054338WriteWord(INT32 offset, UINT16 data)
{
	k54338_regs[(offset & 0x1e)/2] = data;
}

void K054338WriteByte(INT32 offset, UINT8 data)
{
	UINT8 *regs = (UINT8*)&k54338_regs;

	regs[(offset & 0x1f)^1] = data;
}

// returns a 16-bit '338 register
INT32 K054338_read_register(INT32 reg)
{
	return k54338_regs[reg];
}

void K054338_update_all_shadows()
{
	INT32 i, d;

	for (i = 0; i < 9; i++)
	{
		d = k54338_regs[K338_REG_SHAD1R + i] & 0x1ff;
		if (d >= 0x100)
			d -= 0x200;
		m_shd_rgb[i] = d;
	}
}

void K054338_export_config(INT32 **shd_rgb)
{
	*shd_rgb = m_shd_rgb;
}

// k054338 BG color fill
void K054338_fill_solid_bg()
{
	UINT32 bgcolor;
	UINT32 *pLine;
	INT32 x, y;

	bgcolor = (K054338_read_register(K338_REG_BGC_R)&0xff)<<16;
	bgcolor |= K054338_read_register(K338_REG_BGC_GB);

	/* and fill the screen with it */
	for (y = 0; y < nScreenHeight; y++)
	{
		pLine = konami_bitmap32;
		pLine += (nScreenWidth*y);
		for (x = 0; x < nScreenWidth; x++)
			*pLine++ = bgcolor;
	}
}

// Unified k054338/K055555 BG color fill
void K054338_fill_backcolor(INT32 palette_offset, INT32 mode) // (see p.67)
{
	INT32 clipx, clipy, clipw, cliph, i, dst_pitch;
	INT32 BGC_CBLK, BGC_SET;
	UINT32 *dst_ptr, *pal_ptr;
	INT32 bgcolor;

	clipx = 0 & ~3;
	clipy = 0;
	clipw = ((nScreenWidth - 1) + 4) & ~3;
	cliph = (nScreenHeight-1) + 1;

	dst_ptr = konami_bitmap32 + palette_offset;
	dst_pitch = nScreenWidth;
	dst_ptr += 0;

	BGC_SET = 0;
	pal_ptr = konami_palette32;

	if (!mode)
	{
		// single color output from CLTC
		bgcolor = (int)(k54338_regs[K338_REG_BGC_R]&0xff)<<16 | (int)k54338_regs[K338_REG_BGC_GB];
	}
	else
	{
		BGC_CBLK = K055555ReadRegister(0);
		BGC_SET  = K055555ReadRegister(1);
		pal_ptr += BGC_CBLK << 9;

		// single color output from PCU2
		if (!(BGC_SET & 2)) { bgcolor = *pal_ptr; mode = 0; } else bgcolor = 0;
	}

	if (!mode)
	{
		// single color fill
		dst_ptr += clipw;
		i = clipw = -clipw;
		do
		{
			do { dst_ptr[i] = dst_ptr[i+1] = dst_ptr[i+2] = dst_ptr[i+3] = bgcolor; } while (i += 4);
			dst_ptr += dst_pitch;
			i = clipw;
		}
		while (--cliph);
	}
	else
	{
		if (!(BGC_SET & 1))
		{
			// vertical gradient fill
			pal_ptr += clipy;
			dst_ptr += clipw;
			bgcolor = *pal_ptr++;
			i = clipw = -clipw;
			do
			{
				do { dst_ptr[i] = dst_ptr[i+1] = dst_ptr[i+2] = dst_ptr[i+3] = bgcolor; } while (i += 4);
				dst_ptr += dst_pitch;
				bgcolor = *pal_ptr++;
				i = clipw;
			}
			while (--cliph);
		}
		else
		{
			// horizontal gradient fill
			pal_ptr += clipx;
			clipw <<= 2;
			do
			{
				memcpy(dst_ptr, pal_ptr, clipw);
				dst_ptr += dst_pitch;
			}
			while (--cliph);
		}
	}
}

// addition blending unimplemented (requires major changes to drawgfx and tilemap.c)
INT32 K054338_set_alpha_level(INT32 pblend)
{
	UINT16 *regs;
	INT32 ctrl, mixpri, mixset, mixlv;

	if (pblend <= 0 || pblend > 3)
	{
	//	alpha_set_level(255);
		return(255);
	}

	regs   = k54338_regs;
	ctrl   = k54338_regs[K338_REG_CONTROL];
	mixpri = ctrl & K338_CTL_MIXPRI;
	mixset = regs[K338_REG_PBLEND + (pblend>>1 & 1)] >> (~pblend<<3 & 8);
	mixlv  = mixset & 0x1f;

	if (k054338_alphainverted) mixlv = 0x1f - mixlv;

	if (!(mixset & 0x20))
	{
		mixlv = mixlv<<3 | mixlv>>2;
	//	alpha_set_level(mixlv); // source x alpha/255  +  target x (255-alpha)/255
	}
	else
	{
		if (!mixpri)
		{
			// source x alpha  +  target (clipped at 255)
		}
		else
		{
			// source  +  target x alpha (clipped at 255)
		}

		// DUMMY
		if (mixlv && mixlv<0x1f) mixlv = 0x10;
		mixlv = mixlv<<3 | mixlv>>2;
	//	alpha_set_level(mixlv);
	}

	return(mixlv);
}

void K054338_invert_alpha(INT32 invert)
{
	k054338_alphainverted = invert;
}
