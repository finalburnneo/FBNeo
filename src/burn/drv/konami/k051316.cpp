// license:BSD-3-Clause
// copyright-holders:Fabio Priuli,Acho A. Tang, R. Belmont

#include "tiles_generic.h"
#include "konamiic.h"

static UINT16* K051316TileMap[3];
static void (*K051316Callback[3])(INT32* code, INT32* color, INT32* flags);
static INT32 K051316Depth[3];
static INT32 K051316TransColor[3];
static UINT32 K051316TransMask[3];
static UINT8* K051316Gfx[3];
static UINT8* K051316GfxExp[3];
static INT32 K051316Mask[3];
static INT32 K051316Offs[3][2];

static INT32 force_update[3];

static UINT8* K051316Ram[3];
static UINT8 K051316Ctrl[3][16];
static UINT8 K051316Wrap[3];

void K051316Init(INT32 chip, UINT8* gfx, UINT8* gfxexp, INT32 mask,
                 void (*callback)(INT32* code, INT32* color, INT32* flags), INT32 bpp, INT32 transp)
{
	K051316Ram[chip] = BurnMalloc(0x800);
	K051316TileMap[chip] = (UINT16*)BurnMalloc(((32 * 16) * (32 * 16)) * sizeof(UINT16));

	K051316Callback[chip] = callback;

	if (gfxexp == nullptr) gfxexp = gfx;

	K051316Depth[chip] = bpp;
	K051316Gfx[chip] = gfx;
	K051316GfxExp[chip] = gfxexp;

	K051316Mask[chip] = mask;

	if (bpp == 4)
	{
		BurnNibbleExpand(gfx, gfxexp, mask + 1, 0, 0);
	}

	KonamiAllocateBitmaps();

	KonamiIC_K051316InUse = 1;

	K051316Offs[chip][0] = K051316Offs[chip][1] = 0;

	K051316TransMask[chip] = 0;
	K051316TransColor[chip] = transp & 0xff;

	if (transp & 0x200)
	{
		K051316TransMask[chip] = transp & 0xff;
		K051316TransColor[chip] = 0;
	}
}

void K051316Reset()
{
	for (INT32 i = 0; i < 3; i++)
	{
		if (K051316Ram[i])
		{
			memset(K051316Ram[i], 0xff, 0x800);
			force_update[i] = 1;
		}

		memset(K051316Ctrl[i], 0, 16);

		K051316Wrap[i] = 0;

		if (K051316TileMap[i])
		{
			memset(K051316TileMap[i], 0, (32 * 16) * (32 * 16) * sizeof(INT16));
		}
	}
}

void K051316Exit()
{
	for (INT32 i = 0; i < 3; i++)
	{
		BurnFree(K051316Ram[i]);
		BurnFree(K051316TileMap[i]);
		K051316Callback[i] = nullptr;
	}
}

void K051316SetOffset(INT32 chip, INT32 xoffs, INT32 yoffs)
{
	K051316Offs[chip][0] = xoffs;
	K051316Offs[chip][1] = yoffs;
}

UINT8 K051316ReadRom(INT32 chip, INT32 offset)
{
	if ((K051316Ctrl[chip][0x0e] & 0x01) == 0)
	{
		INT32 addr = offset + (K051316Ctrl[chip][0x0c] << 11) + (K051316Ctrl[chip][0x0d] << 19);
		if (K051316Depth[chip] <= 4) addr /= 2;
		addr &= K051316Mask[chip];

		return K051316Gfx[chip][addr];
	}

	return 0;
}

UINT8 K051316Read(INT32 chip, INT32 offset)
{
	return K051316Ram[chip][offset];
}

static inline void K051316_write_tile(INT32 offset, INT32 chip)
{
	offset &= 0x3ff;

	INT32 sx = (offset & 0x1f) << 4;
	INT32 sy = (offset >> 5) << 4;

	INT32 code = K051316Ram[chip][offset];
	INT32 color = K051316Ram[chip][offset + 0x400];
	INT32 flags = 0;

	(*K051316Callback[chip])(&code, &color, &flags);

	UINT8* src = K051316GfxExp[chip] + (code * 16 * 16);
	UINT16* dst;

	color <<= K051316Depth[chip];

	INT32 flipx = flags & 1;
	INT32 flipy = flags & 2;
	if (flipx) flipx = 0x0f;
	if (flipy) flipy = 0x0f;

	INT32 tmask = K051316TransMask[chip];

	for (INT32 y = 0; y < 16; y++, sy++)
	{
		dst = K051316TileMap[chip] + ((sy << 9) + sx);

		for (INT32 x = 0; x < 16; x++)
		{
			INT32 pxl = src[((y ^ flipy) << 4) | (x ^ flipx)];

			if (tmask)
			{
				if ((pxl & tmask) == tmask)
				{
					dst[x] = color | pxl;
				}
				else
				{
					dst[x] = 0x8000 | color | pxl;
				}
			}
			else
			{
				if (pxl != K051316TransColor[chip])
				{
					dst[x] = color | pxl;
				}
				else
				{
					dst[x] = 0x8000 | color | pxl;
				}
			}
		}
	}
}

void K051316Write(INT32 chip, INT32 offset, INT32 data)
{
	if (K051316Ram[chip][offset] != data)
	{
		K051316Ram[chip][offset] = data;
		K051316_write_tile(offset & 0x3ff, chip);
	}
	force_update[chip] = 1;
}

UINT8 K051316ReadCtrl(INT32 chip, INT32 offset)
{
	return K051316Ctrl[chip][offset & 0x0f];
}

void K051316WriteCtrl(INT32 chip, INT32 offset, INT32 data)
{
	K051316Ctrl[chip][offset & 0x0f] = data;
	force_update[chip] = 1;
}

void K051316WrapEnable(INT32 chip, INT32 status)
{
	K051316Wrap[chip] = status;
	force_update[chip] = 1;
}

static inline void copy_roz(INT32 chip, UINT32 startx, UINT32 starty, INT32 incxx, INT32 incxy, INT32 incyx,
                            INT32 incyy, INT32 wrap, INT32 transp, INT32 flags)
{
	UINT32 hshift = 512 << 16;
	UINT32 wshift = 512 << 16;

	if (flags & 0x200) transp = 0; // force opaque

	INT32 priority = flags & 0xff;

	if (flags & 0x100) // indexed colors
	{
		UINT16* dst = pTransDraw;
		UINT16* src = K051316TileMap[chip];

		for (INT32 sy = 0; sy < nScreenHeight; sy++, startx += incyx, starty += incyy)
		{
			UINT32 cx = startx;
			UINT32 cy = starty;

			if (wrap)
			{
				if (transp)
				{
					for (INT32 x = 0; x < nScreenWidth; x++, cx += incxx, cy += incxy, dst++)
					{
						INT32 pxl = src[(((cy >> 16) & 0x1ff) << 9) | ((cx >> 16) & 0x1ff)];

						if (!(pxl & 0x8000))
						{
							*dst = pxl;
						}
					}
				}
				else
				{
					for (INT32 x = 0; x < nScreenWidth; x++, cx += incxx, cy += incxy, dst++)
					{
						*dst = src[(((cy >> 16) & 0x1ff) << 9) | ((cx >> 16) & 0x1ff)] & 0x7fff;
					}
				}
			}
			else
			{
				if (transp)
				{
					for (INT32 x = 0; x < nScreenWidth; x++, cx += incxx, cy += incxy, dst++)
					{
						if (cx < wshift && cy < hshift)
						{
							INT32 pxl = src[(((cy >> 16) & 0x1ff) << 9) | ((cx >> 16) & 0x1ff)];
							if (!(pxl & 0x8000))
							{
								*dst = pxl;
							}
						}
					}
				}
				else
				{
					for (INT32 x = 0; x < nScreenWidth; x++, cx += incxx, cy += incxy, dst++)
					{
						UINT32 pos = ((cy >> 16) << 9) | (cx >> 16);

						if (pos >= 0x40000) continue;

						*dst = src[pos] & 0x7fff;
					}
				}
			}
		}
	}
	else // 32-bit colors
	{
		UINT32* dst = konami_bitmap32;
		UINT8* pri = konami_priority_bitmap;
		UINT16* src = K051316TileMap[chip];
		UINT32* pal = konami_palette32;

		for (INT32 sy = 0; sy < nScreenHeight; sy++, startx += incyx, starty += incyy)
		{
			UINT32 cx = startx;
			UINT32 cy = starty;

			if (wrap)
			{
				if (transp)
				{
					for (INT32 x = 0; x < nScreenWidth; x++, cx += incxx, cy += incxy, dst++, pri++)
					{
						INT32 pxl = src[(((cy >> 16) & 0x1ff) << 9) | ((cx >> 16) & 0x1ff)];

						if (!(pxl & 0x8000))
						{
							*dst = pal[pxl];
							*pri = priority;
						}
					}
				}
				else
				{
					for (INT32 x = 0; x < nScreenWidth; x++, cx += incxx, cy += incxy, dst++, pri++)
					{
						*dst = pal[src[(((cy >> 16) & 0x1ff) << 9) | ((cx >> 16) & 0x1ff)] & 0x7fff];
						*pri = priority;
					}
				}
			}
			else
			{
				if (transp)
				{
					for (INT32 x = 0; x < nScreenWidth; x++, cx += incxx, cy += incxy, dst++, pri++)
					{
						if (cx < wshift && cy < hshift)
						{
							INT32 pxl = src[(((cy >> 16) & 0x1ff) << 9) | ((cx >> 16) & 0x1ff)];
							if (!(pxl & 0x8000))
							{
								*dst = pal[pxl];
								*pri = priority;
							}
						}
					}
				}
				else
				{
					for (INT32 x = 0; x < nScreenWidth; x++, cx += incxx, cy += incxy, dst++, pri++)
					{
						UINT32 pos = ((cy >> 16) << 9) | (cx >> 16);

						if (pos >= 0x40000) continue;

						*dst = pal[src[pos] & 0x7fff];
						*pri = priority;
					}
				}
			}
		}
	}
}

void K051316_zoom_draw(INT32 chip, INT32 flags)
{
	UINT32 startx, starty;
	INT32 incxx, incxy, incyx, incyy;

	startx = 256 * static_cast<INT16>(256 * K051316Ctrl[chip][0x00] + K051316Ctrl[chip][0x01]);
	incxx = static_cast<INT16>(256 * K051316Ctrl[chip][0x02] + K051316Ctrl[chip][0x03]);
	incyx = static_cast<INT16>(256 * K051316Ctrl[chip][0x04] + K051316Ctrl[chip][0x05]);
	starty = 256 * static_cast<INT16>(256 * K051316Ctrl[chip][0x06] + K051316Ctrl[chip][0x07]);
	incxy = static_cast<INT16>(256 * K051316Ctrl[chip][0x08] + K051316Ctrl[chip][0x09]);
	incyy = static_cast<INT16>(256 * K051316Ctrl[chip][0x0a] + K051316Ctrl[chip][0x0b]);

	startx -= (16 + K051316Offs[chip][1]) * incyx;
	starty -= (16 + K051316Offs[chip][1]) * incyy;

	startx -= (89 + K051316Offs[chip][0]) * incxx;
	starty -= (89 + K051316Offs[chip][0]) * incxy;

	INT32 transp = K051316TransColor[chip] + 1;

	copy_roz(chip, startx << 5, starty << 5, incxx << 5, incxy << 5, incyx << 5, incyy << 5, K051316Wrap[chip], transp,
	         flags); // transp..
}

void K051316RedrawTiles(INT32 chip)
{
	if (K051316Ram[chip] && force_update[chip])
	{
		for (INT32 j = 0; j < 0x400; j++)
		{
			K051316_write_tile(j, chip);
		}
		force_update[chip] = 0;
	}
}

void K051316Scan(INT32 nAction)
{
	struct BurnArea ba;

	if (nAction & ACB_MEMORY_RAM)
	{
		for (INT32 i = 0; i < 3; i++)
		{
			if (K051316Ram[i])
			{
				memset(&ba, 0, sizeof(ba));
				ba.Data = K051316Ram[i];
				ba.nLen = 0x800;
				ba.szName = "K052109 Ram";
				BurnAcb(&ba);
			}

			memset(&ba, 0, sizeof(ba));
			ba.Data = K051316Ctrl[i];
			ba.nLen = 0x010;
			ba.szName = "K052109 Control";
			BurnAcb(&ba);
		}
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		SCAN_VAR(K051316Wrap[0]);
		SCAN_VAR(K051316Wrap[1]);
		SCAN_VAR(K051316Wrap[2]);
	}

	if (nAction & ACB_WRITE)
	{
		force_update[0] = 1;
		force_update[1] = 1;
		force_update[2] = 1;
		K051316RedrawTiles(0);
		K051316RedrawTiles(1);
		K051316RedrawTiles(2);
	}
}
