// license:BSD-3-Clause
// copyright-holders:David Haywood

// k053245

#include "tiles_generic.h"
#include "konamiic.h"

static void (*K053245Callback[2])(INT32 *code,INT32 *color,INT32 *priority);

UINT8 *K053245Ram[2];
static UINT8 *K053245Buf[2];
static UINT8 *K053245Gfx[2];
static UINT8 *K053245GfxExp[2];
static INT32 K053245Mask[2];
static INT32 K053245MaskExp[2];
static INT32 K053245_dx[2];
static INT32 K053245_dy[2];

static INT32 nBpp[2];
static UINT8 K053244Regs[2][0x10];
static INT32 K053244Bank[2];

static INT32 K053245Active = 0;

INT32 K05324xZRejection = -1;

INT32 K053245Reset()
{
	for (INT32 i = 0; i < K053245Active; i++) {
		memset (K053245Ram[i], 0, 0x800);
		memset (K053245Buf[i], 0, 0x800);
		memset (K053244Regs[i], 0, 0x10);
	
		K053244Bank[i] = 0;
	}

	return 0;
}

void K053245GfxDecode(UINT8 *src, UINT8 *dst, INT32 len)
{
	INT32 Plane[4]  = { STEP4(24, -8) };
	INT32 XOffs[16] = { STEP8(0, 1), STEP8(256, 1) };
	INT32 YOffs[16] = { STEP8(0,32), STEP8(512,32) };

	GfxDecode(len >> 7, 4, 16, 16, Plane, XOffs, YOffs, 0x400, src, dst);
}

void K053245Init(INT32 chip, UINT8 *gfx, UINT8 *gfxexp, INT32 mask, void (*callback)(INT32 *code,INT32 *color,INT32 *priority))
{
	K053245Ram[chip] = (UINT8*)BurnMalloc(0x800); // enough
	K053245Buf[chip] = (UINT8*)BurnMalloc(0x800); // enough

	K053245Mask[chip] = mask;
	K053245MaskExp[chip] = (mask * 2) / (16 * 16);

	K053245Gfx[chip] = gfx;
	K053245GfxExp[chip] = gfxexp;

	K053245Callback[chip] = callback;

	K053245Active++;

	KonamiIC_K053245InUse = 1;

	KonamiAllocateBitmaps();

	K053245Reset();

	nBpp[chip] = 4;
}

void K053245SetBpp(INT32 chip, INT32 bpp)
{
	nBpp[chip] = bpp;
}

void K053245Exit()
{
	for (INT32 i = 0; i < K053245Active; i++) {
		BurnFree (K053245Ram[i]);
		BurnFree (K053245Buf[i]);

		K053245_dx[i] = 0;
		K053245_dy[i] = 0;
	}

	K053245Active = 0;
}

// Useful functions

void K053245SetSpriteOffset(INT32 chip,INT32 offsx, INT32 offsy)
{
	K053245_dx[chip] = offsx;
	K053245_dy[chip] = offsy;
}

void K053245ClearBuffer(INT32 chip)
{
	UINT16 *buf = (UINT16*)K053245Buf[chip];

	for (INT32 i = 0; i < 0x800/2; i+=8) buf[i] = 0;
}

void K053245UpdateBuffer(INT32 chip)
{
	memcpy (K053245Buf[chip], K053245Ram[chip], 0x800);
}

void K053244BankSelect(INT32 chip, INT32 bank)
{
	K053244Bank[chip] = bank;
}


// 8 bit read/write handlers

UINT8 K053245Read(INT32 chip, INT32 offset)
{
	return K053245Ram[chip][offset ^ 1]; //
}

void K053245Write(INT32 chip, INT32 offset, INT32 data)
{
	K053245Ram[chip][offset ^ 1] = data; //
}

UINT16 K053245ReadWord(INT32 chip, INT32 offset)
{
	UINT16 *ret = (UINT16*)K053245Ram[chip];

#if 0
	INT32 r = ret[offset];

	return (r << 8) | (r >> 8);
#else
	return BURN_ENDIAN_SWAP_INT16(ret[offset]);
#endif
}

void K053245WriteWord(INT32 chip, INT32 offset, INT32 data)
{
	UINT16 *ret = (UINT16*)K053245Ram[chip];

#if 0
	ret[offset] = (data << 8) | (data >> 8);
#else
	ret[offset] = BURN_ENDIAN_SWAP_INT16(data);
#endif
}

UINT8 K053244Read(INT32 chip, INT32 offset)
{
	if (K053244Regs[chip][5] & 0x10 && (offset & 0x0c) == 0x0c)
	{
		INT32 addr  = (K053244Bank[chip]    << 19) | ((K053244Regs[chip][11] & 0x07) << 18);
		    addr |= (K053244Regs[chip][8] << 10) |  (K053244Regs[chip][ 9] << 2);
		    addr |= (offset & 3) ^ 1;
		    addr &=  K053245Mask[chip];

		return K053245Gfx[chip][addr];
	}
	else if (offset == 0x06) {
		K053245UpdateBuffer(chip);
	}

	return 0;
}

void K053244Write(INT32 chip, INT32 offset, INT32 data)
{
	K053244Regs[chip][offset] = data;

	if (offset == 0x06) {
		K053245UpdateBuffer(chip);
	}
}

void K05324xSetZRejection(INT32 z)
{
	K05324xZRejection = z;
}

// Sprite Rendering

void K053245SpritesRender(INT32 chip)
{
#define NUM_SPRITES 128
	INT32 offs,pri_code,i;
	INT32 sortedlist[NUM_SPRITES];
	INT32 flipscreenX, flipscreenY, spriteoffsX, spriteoffsY;

	flipscreenX = K053244Regs[chip][5] & 0x01;
	flipscreenY = K053244Regs[chip][5] & 0x02;
	spriteoffsX = (K053244Regs[chip][0] << 8) | K053244Regs[chip][1];
	spriteoffsY = (K053244Regs[chip][2] << 8) | K053244Regs[chip][3];

	for (offs = 0;offs < NUM_SPRITES;offs++)
		sortedlist[offs] = -1;

	UINT16 *sprbuf = (UINT16*)K053245Buf[chip];

	UINT8 *gfxdata = K053245GfxExp[chip];

	/* prebuild a sorted table */
	for (i=0x800/2, offs=0; offs<i; offs+=8)
	{
		pri_code = BURN_ENDIAN_SWAP_INT16(sprbuf[offs]);
		if (pri_code & 0x8000)
		{
			pri_code &= 0x007f;

			if (offs && pri_code == K05324xZRejection) continue;

			if (sortedlist[pri_code] == -1) sortedlist[pri_code] = offs;
		}
	}

	for (pri_code = NUM_SPRITES - 1; pri_code >= 0; pri_code--)
	{
		INT32 ox,oy,color,code,size,w,h,x,y,flipx,flipy,mirrorx,mirrory,shadow,zoomx,zoomy,pri;

		offs = sortedlist[pri_code];
		if (offs == -1) continue;

		code = BURN_ENDIAN_SWAP_INT16(sprbuf[offs+1]);
		code = ((code & 0xffe1) + ((code & 0x0010) >> 2) + ((code & 0x0008) << 1)
				 + ((code & 0x0004) >> 1) + ((code & 0x0002) << 2));
		color = BURN_ENDIAN_SWAP_INT16(sprbuf[offs+6]) & 0x00ff;
		pri = 0;

		(*K053245Callback[chip])(&code,&color,&pri);

		size = (BURN_ENDIAN_SWAP_INT16(sprbuf[offs]) & 0x0f00) >> 8;

		w = 1 << (size & 0x03);
		h = 1 << ((size >> 2) & 0x03);

		/* zoom control:
           0x40 = normal scale
          <0x40 enlarge (0x20 = double size)
          >0x40 reduce (0x80 = half size)
        */
		zoomy = BURN_ENDIAN_SWAP_INT16(sprbuf[offs+4]);
		if (zoomy > 0x2000) continue;
		if (zoomy) zoomy = (0x400000+zoomy/2) / zoomy;
		else zoomy = 2 * 0x400000;
		if ((BURN_ENDIAN_SWAP_INT16(sprbuf[offs]) & 0x4000) == 0)
		{
			zoomx = BURN_ENDIAN_SWAP_INT16(sprbuf[offs+5]);
			if (zoomx > 0x2000) continue;
			if (zoomx) zoomx = (0x400000+zoomx/2) / zoomx;
			else zoomx = 2 * 0x400000;
//         		else zoomx = zoomy; /* workaround for TMNT2 */
		}
		else zoomx = zoomy;

		ox = BURN_ENDIAN_SWAP_INT16(sprbuf[offs+3]) + spriteoffsX;
		oy = BURN_ENDIAN_SWAP_INT16(sprbuf[offs+2]);

		ox += K053245_dx[chip];
		oy += K053245_dy[chip];

		flipx = BURN_ENDIAN_SWAP_INT16(sprbuf[offs]) & 0x1000;
		flipy = BURN_ENDIAN_SWAP_INT16(sprbuf[offs]) & 0x2000;
		mirrorx = BURN_ENDIAN_SWAP_INT16(sprbuf[offs+6]) & 0x0100;
		if (mirrorx) flipx = 0; // documented and confirmed
		mirrory = BURN_ENDIAN_SWAP_INT16(sprbuf[offs+6]) & 0x0200;
		shadow = BURN_ENDIAN_SWAP_INT16(sprbuf[offs+6]) & 0x0080;

		if (flipscreenX)
		{
			ox = 512 - ox;
			if (!mirrorx) flipx = !flipx;
		}
		if (flipscreenY)
		{
			oy = -oy;
			if (!mirrory) flipy = !flipy;
		}

		ox = (ox + 0x5d) & 0x3ff;
		if (ox >= 768) ox -= 1024;
		oy = (-(oy + spriteoffsY + 0x07)) & 0x3ff;
		if (oy >= 640) oy -= 1024;

		/* the coordinates given are for the *center* of the sprite */
		ox -= (zoomx * w) >> 13;
		oy -= (zoomy * h) >> 13;

		for (y = 0;y < h;y++)
		{
			INT32 sx,sy,zw,zh;

			sy = oy + ((zoomy * y + (1<<11)) >> 12);
			zh = (oy + ((zoomy * (y+1) + (1<<11)) >> 12)) - sy;

			for (x = 0;x < w;x++)
			{
				INT32 c,fx,fy;

				sx = ox + ((zoomx * x + (1<<11)) >> 12);
				zw = (ox + ((zoomx * (x+1) + (1<<11)) >> 12)) - sx;
				c = code;
				if (mirrorx)
				{
					if ((flipx == 0) ^ (2*x < w))
					{
						/* mirror left/right */
						c += (w-x-1);
						fx = 1;
					}
					else
					{
						c += x;
						fx = 0;
					}
				}
				else
				{
					if (flipx) c += w-1-x;
					else c += x;
					fx = flipx;
				}
				if (mirrory)
				{
					if ((flipy == 0) ^ (2*y >= h))
					{
						/* mirror top/bottom */
						c += 8*(h-y-1);
						fy = 1;
					}
					else
					{
						c += 8*y;
						fy = 0;
					}
				}
				else
				{
					if (flipy) c += 8*(h-1-y);
					else c += 8*y;
					fy = flipy;
				}

				c = ((c & 0x3f) | (code & ~0x3f)) & K053245MaskExp[chip];

				if (shadow) {
					konami_render_zoom_shadow_tile(gfxdata, c, nBpp[chip], color, sx, sy, fx, fy, 16, 16, zw << 12, zh << 12, pri, 0);
					continue;
				}

				if (zoomx == 0x10000 && zoomy == 0x10000)
				{
					konami_draw_16x16_prio_tile(gfxdata, c, nBpp[chip], color, sx, sy, fx, fy, pri);
				}
				else
				{
					konami_draw_16x16_priozoom_tile(gfxdata, c, nBpp[chip], color, 0, sx, sy, fx, fy, 16, 16, zw << 12, zh << 12, pri);
				}
			}
		}
	}
}

void K053245Scan(INT32 nAction)
{
	struct BurnArea ba;
	
	if (nAction & ACB_MEMORY_RAM) {
		for (INT32 i = 0; i < 2; i++) {
			if (K053245Ram[i]) {
				memset(&ba, 0, sizeof(ba));
				ba.Data	  = K053245Ram[i];
				ba.nLen	  = 0x800;
				ba.szName = "K053245 Ram";
				BurnAcb(&ba);

				ba.Data	  = K053245Buf[i];
				ba.nLen	  = 0x800;
				ba.szName = "K053245 Buffer";
				BurnAcb(&ba);
			}

			memset(&ba, 0, sizeof(ba));
			ba.Data	  = K053244Regs[i];
			ba.nLen	  = 0x010;
			ba.szName = "K053244 Registers";
			BurnAcb(&ba);	
		}
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(K053244Bank[0]);
		SCAN_VAR(K053244Bank[1]);
	}
}
