// Based on MAME sources by David Haywood

#include "tiles_generic.h"
#include "konamiic.h"

#define K056832_FLIPY	0x00002
#define K056832_FLIPX	0x00001

static UINT16 k056832Regs[0x20];
static UINT16 k056832Regsb[0x20];
static UINT16 *K056832VideoRAM;

static UINT8 *K056832Rom;
static UINT8 *K056832RomExp;
static UINT8 *K056832TransTab;

static INT32 K056832RomExpMask;

static INT32 m_layer_offs[8][2];
static INT32 global_clip[4];

static INT32 m_use_ext_linescroll;
static INT32 m_layer_association;
static INT32 m_active_layer;
static INT32 m_layer_assoc_with_page[16];
static INT32 m_selected_page;
static INT32 m_selected_page_x4096;
static INT32 m_lsram_page[8][2];
static INT32 m_default_layer_association;
static INT32 m_uses_tile_banks;
static INT32 m_cur_tile_bank;
static INT32 m_layer_tile_mode[8];
static INT32 m_page_tile_mode[16];
static INT32 m_num_gfx_banks;
static INT32 m_cur_gfx_banks;
static INT32 tilemap_flip;
static INT32 m_rom_half;
static INT32 K056832_metamorphic_textfix = 0;

#define CLIP_MINX	global_clip[0]
#define CLIP_MAXX	global_clip[1]
#define CLIP_MINY	global_clip[2]
#define CLIP_MAXY	global_clip[3]

static void (*m_callback)(INT32 layer, INT32 *code, INT32 *color, INT32 *flags);

void K056832Reset()
{
	memset (K056832VideoRAM, 0, 0x2000 * 0x11 * 2);

	for (INT32 i = 0; i < 0x20; i++) {
		k056832Regs[i] = 0;
		k056832Regsb[i] = 0;
	}

	m_cur_gfx_banks = 0;
	m_uses_tile_banks = 0;
	m_cur_tile_bank = 0;
	m_active_layer = 0;
	tilemap_flip = 0;
	m_rom_half = 0;
	m_selected_page = 0;
	m_selected_page_x4096 = 0;
}

static void CalculateTranstab()
{
	INT32 explen = (K056832RomExpMask + 1);

	K056832TransTab = (UINT8*)BurnMalloc(explen);

	memset (K056832TransTab, 1, explen);

	for (INT32 i = 0; i < explen*0x40; i+= 0x40) {
		for (INT32 j = 0; j < 0x40; j++) {
			if (K056832RomExp[i+j]) {
				K056832TransTab[i/0x40] = 0;
				break;
			}
		}
	}
}

void K056832Init(UINT8 *rom, UINT8 *romexp, INT32 rom_size, void (*cb)(INT32 layer, INT32 *code, INT32 *color, INT32 *flags))
{
	KonamiIC_K056832InUse = 1;

	for (INT32 i = 0; i < 8; i++) {
		m_layer_offs[i][0] = m_layer_offs[i][1] = 0;
		m_lsram_page[i][0] = i;
		m_lsram_page[i][1] = i << 11;
		m_layer_tile_mode[i] = 1;
	}

	for (INT32 i = 0; i < 16; i++) {
		m_page_tile_mode[i] = 1;
	}

	K056832SetGlobalOffsets(0, 0);

	KonamiAllocateBitmaps();

	m_callback = cb;

	K056832Rom = rom;
	K056832RomExp = romexp;
	K056832RomExpMask = ((rom_size * 2) / 0x40) - 1;
	m_num_gfx_banks = rom_size / 0x2000;

	CalculateTranstab();

	m_use_ext_linescroll = 0;	// enable later, no reset.
	m_default_layer_association = 1;
	m_layer_association = m_default_layer_association;

	K056832VideoRAM = (UINT16*)BurnMalloc(0x2000 * 0x11 * 2);

	K056832Reset();
}

void K056832Exit()
{
	BurnFree (K056832VideoRAM);
	BurnFree (K056832TransTab);

	K056832_metamorphic_textfix = 0;

	m_callback = NULL;
}

void K056832SetLayerAssociation(INT32 status)
{
	m_layer_association = m_default_layer_association = status;
}

void K056832SetGlobalOffsets(INT32 minx, INT32 miny)
{
	CLIP_MINX = (minx < 0) ? 0 : minx;
	CLIP_MAXX = CLIP_MINX + nScreenWidth;
	CLIP_MINY = (miny < 0) ? 0 : miny;
	CLIP_MAXY = CLIP_MINY + nScreenHeight;
}

void K056832SetLayerOffsets(INT32 layer, INT32 xoffs, INT32 yoffs)
{
	m_layer_offs[layer & 3][0] = xoffs;
	m_layer_offs[layer & 3][1] = yoffs;
}

void K056832SetExtLinescroll()
{
	m_use_ext_linescroll = 1;
}

INT32 K056832IsIrqEnabled()
{
	return k056832Regs[3] & 1;
}

void K056832ReadAvac(INT32 *mode, INT32 *data)
{
	*mode = k056832Regs[0x04/2] & 7;
	*data = k056832Regs[0x38/2];
}

UINT16 K056832ReadRegister(INT32 reg)
{
	return k056832Regs[reg & 0x1f];
}

INT32 K056832GetLookup( INT32 bits )
{
	INT32 res;

	res = (k056832Regs[0x1c] >> (bits << 2)) & 0x0f;

	if (m_uses_tile_banks)   /* Asterix */
		res |= m_cur_tile_bank << 4;

	return res;
}

static void mark_all_tilemaps_dirty()
{ // this sets the page/layer associations.
	for (INT32 i = 0; i < 16; i++)
	{
		if (m_layer_assoc_with_page[i] != -1)
		{
			m_page_tile_mode[i] = m_layer_tile_mode[m_layer_assoc_with_page[i]];
		}
	}
}

static void k056832_change_rambank()
{
	INT32 bank = k056832Regs[0x19];

	if (k056832Regs[0] & 0x02)    // external linescroll enable
		m_selected_page = 16;
	else
		m_selected_page = ((bank >> 1) & 0xc) | (bank & 3);

	m_selected_page_x4096 = m_selected_page << 12;

	mark_all_tilemaps_dirty();
}

#if 0
static INT32 k056832_get_current_rambank()
{
	INT32 bank = k056832Regs[0x19];

	return ((bank >> 1) & 0xc) | (bank & 3);
}
#endif

static void k056832_change_rombank()
{
	INT32 bank;

	if (m_uses_tile_banks)   /* Asterix */
		bank = (k056832Regs[0x1a] >> 8) | (k056832Regs[0x1b] << 4) | (m_cur_tile_bank << 6);
	else
		bank = k056832Regs[0x1a] | (k056832Regs[0x1b] << 16);

	m_cur_gfx_banks = bank % m_num_gfx_banks;
}

void K056832SetTileBank(INT32 bank)
{
	m_uses_tile_banks = 1;

	m_cur_tile_bank = bank;

	k056832_change_rombank();
}

static void k056832_update_page_layout()
{
	m_layer_association = m_default_layer_association;

	INT32 m_y[4] = { 0, 0, 0, 0 };	// not sure why initializing these is necessary
	INT32 m_x[4] = { 0, 0, 0, 0 };	// as they should be all set below, but it causes
	INT32 m_h[4] = { 0, 0, 0, 0 };	// a hang in monster maulers otherswise. -iq
	INT32 m_w[4] = { 0, 0, 0, 0 };

	for (INT32 layer = 0; layer < 4; layer++)
	{
		m_y[layer] = (k056832Regs[0x08|layer] & 0x18) >> 3;
		m_x[layer] = (k056832Regs[0x0c|layer] & 0x18) >> 3;
		m_h[layer] = (k056832Regs[0x08|layer] & 0x03) >> 0;
		m_w[layer] = (k056832Regs[0x0c|layer] & 0x03) >> 0;

		if (!m_y[layer] && !m_x[layer] && m_h[layer] == 3 && m_w[layer] == 3)
		{
			m_layer_association = 0;
			break;
		}
	}

	// disable all tilemaps
	for (INT32 page_idx = 0; page_idx < 16; page_idx++)
	{
		m_layer_assoc_with_page[page_idx] = -1;
	}

	// enable associated tilemaps
	for (INT32 layer = 0; layer < 4; layer++)
	{
		INT32 rowstart = m_y[layer];
		INT32 colstart = m_x[layer];
		INT32 rowspan  = m_h[layer] + 1;
		INT32 colspan  = m_w[layer] + 1;

		INT32 setlayer = (m_layer_association) ? layer : m_active_layer;

		for (INT32 r = 0; r < rowspan; r++)
		{
			for (INT32 c = 0; c < colspan; c++)
			{
				INT32 page_idx = (((rowstart + r) & 3) << 2) + ((colstart + c) & 3);
				if (m_layer_assoc_with_page[page_idx] == -1)
					m_layer_assoc_with_page[page_idx] = setlayer;
			}
		}
	}

	mark_all_tilemaps_dirty();
}

static void k056832_word_write_update(INT32 offset) // (offset/2)&0x1f internally
{
	offset = (offset / 2) & 0x1f;

	UINT16 data = k056832Regs[offset];

	if (offset >= 0x10/2 && offset <= 0x1e/2)
	{
		m_active_layer = offset & 3;
		k056832_update_page_layout();
		return;
	}

	switch (offset)
	{
		case 0x00/2:
			tilemap_flip = 0;
			if (data & 0x20) tilemap_flip |= K056832_FLIPY;
			if (data & 0x10) tilemap_flip |= K056832_FLIPX;
			k056832_change_rambank();
		break;

		case 0x08/2:
			for (INT32 layer = 0; layer < 4; layer++) {
				m_layer_tile_mode[layer] = data & (1 << layer);

				INT32 tilemode = m_layer_tile_mode[layer];

				for (INT32 i = 0; i < 16; i++)
				{
					if (m_layer_assoc_with_page[i] == layer)
					{
						m_page_tile_mode[i] = tilemode;
						//mark_page_dirty(i);
					}
				}
			}
		break;

		case 0x32/2:
			k056832_change_rambank();
		break;

		case 0x34/2:
		case 0x36/2:
			k056832_change_rombank();
		break;
	}
}

void K056832WordWrite(INT32 offset, UINT16 data)
{
	k056832Regs[(offset / 2) & 0x1f] = data;
	k056832_word_write_update(offset);
}

void K056832ByteWrite(INT32 offset, UINT8 data)
{
	UINT8 *regs = (UINT8*)&k056832Regs;
	regs[(offset & 0x3f) ^ 1] = data;

	k056832_word_write_update(offset);
}

UINT16 K056832RomWordRead(UINT16 offset)
{
	offset &= 0x1ffe;

	INT32 addr = 0x2000 * m_cur_gfx_banks + offset;

	return K056832Rom[addr+1] | (K056832Rom[addr] << 8);
}

void K056832HalfRamWriteWord(UINT32 offset, UINT16 data)
{
	K056832VideoRAM[m_selected_page_x4096 + (offset & 0xffe) + 1] = data;
}

void K056832HalfRamWriteByte(UINT32 offset, UINT8 data)
{
	UINT8 *ram = (UINT8*)(K056832VideoRAM + (m_selected_page_x4096 + (offset & 0xffe) + 1));

	ram[(offset & 1) ^ 1] = data;
}

UINT16 K056832HalfRamReadWord(UINT32 offset)
{
	return K056832VideoRAM[m_selected_page_x4096 + (offset & 0xffe) + (((offset >> 12) ^ 1) & 1)];
}

UINT8 K056832HalfRamReadByte(UINT32 offset)
{
	UINT8 *ram = (UINT8*)(K056832VideoRAM + m_selected_page_x4096 + ((offset & 0xffe) + (((offset >> 12) ^ 1) & 1)));
	return ram[(offset & 1) ^ 1];
}

void K056832RamWriteWord(UINT32 offset, UINT16 data)
{
	offset = (offset & 0x1fff) / 2;

	K056832VideoRAM[m_selected_page_x4096 + (offset)] = data;
}

void K056832RamWriteByte(UINT32 offset, UINT8 data)
{
	UINT8 *ram = (UINT8*)(K056832VideoRAM + m_selected_page_x4096);

	ram[(offset & 0x1fff) ^ 1] = data;
}

UINT16 K056832RamReadWord(UINT32 offset)
{
	return K056832VideoRAM[m_selected_page_x4096 + ((offset & 0x1fff) / 2)];
}

UINT8 K056832RamReadByte(UINT32 offset)
{
	UINT8 *ram = (UINT8*)(K056832VideoRAM + m_selected_page_x4096);

	m_rom_half = 0;

	return ram[(offset & 0x1fff) ^ 1];
}

UINT16 K056832RomWord8000Read(INT32 offset)
{
	int addr = 0x8000 * m_cur_gfx_banks + (offset & 0x7ffe);

	return K056832Rom[addr + 2] | (K056832Rom[addr] << 8);
}

void K056832WritebRegsWord(INT32 offset, UINT16 data)
{
	k056832Regsb[(offset & 0x1f)/2] = data;
}

void K056832WritebRegsByte(INT32 offset, UINT8 data)
{
	UINT8 *regs = (UINT8*)&k056832Regsb;

	regs[(offset & 0x1f)^1] = data;
}

UINT16 K056832mwRomWordRead(INT32 address)
{
	INT32 offset = (address / 2) & 0x1fff;
	INT32 bank = (0x800 * m_cur_gfx_banks) * 5;

	if (k056832Regsb[0x02] & 0x08)
	{
		UINT16 temp = K056832Rom[((offset / 4) * 5) + 4 + bank];

		switch (offset & 3)
		{
			case 0:	return ((temp & 0x80) <<  5) | ((temp & 0x40) >> 2);
			case 1:	return ((temp & 0x20) <<  7) | ((temp & 0x10) >> 0);
			case 2: return ((temp & 0x08) <<  9) | ((temp & 0x04) << 2);
			case 3: return ((temp & 0x02) << 11) | ((temp & 0x01) << 4);
		}
	}
	else
	{
		INT32 addr = ((offset >> 1) * 5) + ((offset & 1) * 2) + bank;

		return K056832Rom[addr + 1] | (K056832Rom[addr] << 8);
	}

	return 0;
}


static inline UINT32 alpha_blend(UINT32 d, UINT32 s, UINT32 p)
{
	if (p == 0) return d;

	INT32 a = 256 - p;

	return (((((s & 0xff00ff) * p) + ((d & 0xff00ff) * a)) & 0xff00ff00) |
		((((s & 0x00ff00) * p) + ((d & 0x00ff00) * a)) & 0x00ff0000)) >> 8;
}

static void draw_layer_internal(INT32 layer, INT32 pageIndex, INT32 *clip, INT32 scrollx, INT32 scrolly, INT32 flags, INT32 priority)
{
	static const struct K056832_SHIFTMASKS
	{
		INT32 flips, palm1, pals2, palm2;
	}
	k056832_shiftmasks[4] = {{6, 0x3f, 0, 0x00}, {4, 0x0f, 2, 0x30}, {2, 0x03, 2, 0x3c}, {0, 0x00, 2, 0x3f}};
	const struct K056832_SHIFTMASKS *smptr;

	scrollx &= 0x1ff;
	scrolly &= 0xff;

	INT32 minx = clip[0];
	INT32 maxx = clip[1];
	INT32 miny = clip[2];
	INT32 maxy = clip[3];

	INT32 alpha_enable = flags & K056832_LAYER_ALPHA;
	INT32 alpha = (flags >> 8) & 0xff;
	INT32 opaque = flags & K056832_LAYER_OPAQUE;

	if (alpha == 255) alpha_enable = 0;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		sx -= scrollx;
		if (sx < -7) sx += 512;
		sy -= scrolly;
		if (sy < -7) sy += 256;

		if (tilemap_flip & 1) sx = (512 - 8) - sx;
		if (tilemap_flip & 2) sy = (256 - 8) - sy;

		if (sy < (miny-7) || sy > maxy || sx < (minx-7) || sx > maxx) continue;

		UINT16 *pMem = &K056832VideoRAM[(pageIndex << 12) + (offs << 1)];

		INT32 attr  = pMem[0];
		INT32 code  = pMem[1];

		if (m_layer_association)
		{
			layer = m_layer_assoc_with_page[pageIndex];
			if (layer == -1)
				layer = 0;  // use layer 0's palette info for unmapped pages
		}
		else
			layer = m_active_layer;

		INT32 fbits = (k056832Regs[3] >> 6) & 3;
		INT32 flip  = (k056832Regs[1] >> (layer << 1)) & 0x3; // tile-flip override (see p.20 3.2.2 "REG2")
		smptr = &k056832_shiftmasks[fbits];

		flip &= (attr >> smptr->flips) & 3;
		INT32 color = (attr & smptr->palm1) | (attr >> smptr->pals2 & smptr->palm2);
		INT32 g_flags = flip & 3;

		m_callback(layer, &code, &color, &g_flags);

		// hack - mystic warriors' water level - iq
		if (g_flags & 0x8000) {
			alpha_enable = 1;
			alpha = (g_flags >> 16) & 0xff;
		}

	//	code &= K056832RomExpMask; // mask in callback if necessary

		if (!opaque) {
			if (K056832TransTab[code]) continue;
		}

		{
			if (tilemap_flip & 1) g_flags ^= 1;
			if (tilemap_flip & 2) g_flags ^= 2;

			UINT8 *rom = K056832RomExp + (code * 0x40);
			UINT32 *pal = konami_palette32 + (color * 16); // if > 4 bit, adjust in tilemap callback

			INT32 flip_tile = 0;
			if (g_flags & 0x01) flip_tile |= 0x07;
			if (g_flags & 0x02) flip_tile |= 0x38;

			UINT8 *pri = konami_priority_bitmap + ((sy -  CLIP_MINY) * nScreenWidth) - CLIP_MINX;
			UINT32 *dst = konami_bitmap32 + ((sy -  CLIP_MINY) * nScreenWidth) - CLIP_MINX;

			{
				if (alpha_enable) {
					for (INT32 iy = 0; iy < 8; iy++, dst += nScreenWidth, pri += nScreenWidth) {
						INT32 yy = sy+iy;
		
						if (yy < miny || yy > maxy) continue;
	
						for (INT32 ix = 0; ix < 8; ix++) {
							INT32 xx = sx+ix;
		
							if (xx < minx || xx > maxx) continue;
		
							INT32 pxl = rom[((iy*8)+ix)^flip_tile];
		
							if (pxl || opaque) {
								dst[xx] = alpha_blend(dst[xx], pal[pxl], alpha);
								pri[xx] = priority;
							}
						}
					}
				} else {
					for (INT32 iy = 0; iy < 8; iy++, dst += nScreenWidth, pri += nScreenWidth) {
						INT32 yy = sy+iy;
		
						if (yy < miny || yy > maxy) continue;
			
						for (INT32 ix = 0; ix < 8; ix++) {
							INT32 xx = sx+ix;
		
							if (xx < minx || xx > maxx) continue;
		
							INT32 pxl = rom[((iy*8)+ix)^flip_tile];
		
							if (pxl || opaque) {
								dst[xx] = pal[pxl];
								pri[xx] = priority;
							}
						}
					}
				}
			}
		}
	}
}

#define K056832_PAGE_COLS 64
#define K056832_PAGE_ROWS 32
#define K056832_PAGE_HEIGHT (K056832_PAGE_ROWS*8)
#define K056832_PAGE_WIDTH  (K056832_PAGE_COLS*8)

#define K056832_PAGE_COUNT 16

void K056832Draw(INT32 layer, UINT32 flags, UINT32 priority)
{
	UINT16 *m_videoram = K056832VideoRAM;
	UINT16 *m_regs = k056832Regs;

	UINT32 last_dx, last_visible, last_active;
	INT32 sx, sy, ay, tx, ty, width, height;
	INT32 clipw, clipx, cliph, clipy, clipmaxy;
	INT32 line_height, line_endy, line_starty, line_y;
	INT32 sdat_start, sdat_walk, sdat_adv, sdat_wrapmask, sdat_offs;
	INT32 pageIndex, flipx, flipy, corr, r, c;
	INT32 cminy, cmaxy, cminx, cmaxx;
	INT32 dminy, dmaxy, dminx, dmaxx;
	UINT16 *p_scroll_data;
	UINT16 ram16[2];
	INT32 tmap;

	INT32 tmap_scrollx=0,tmap_scrolly=0;

	INT32 clip_data[4] = {0, 0, 0, 0}; // minx, maxx, miny, maxy

	INT32 rowstart = (m_regs[0x08|layer] & 0x18) >> 3;
	INT32 colstart = (m_regs[0x0c|layer] & 0x18) >> 3;
	INT32 rowspan  = ((m_regs[0x08|layer] & 0x03) >> 0) + 1;
	INT32 colspan  = ((m_regs[0x0c|layer] & 0x03) >> 0) + 1;
	INT32 dy = (INT16)m_regs[0x10|layer];
	INT32 dx = (INT16)m_regs[0x14|layer];;
	INT32 scrollbank = ((m_regs[0x18] >> 1) & 0xc) | (m_regs[0x18] & 3);
	INT32 scrollmode = m_regs[0x05] >> (m_lsram_page[layer][0] << 1) & 3;

	if (m_use_ext_linescroll)
	{
		scrollbank = K056832_PAGE_COUNT;
	}

	height = rowspan * K056832_PAGE_HEIGHT;
	width  = colspan * K056832_PAGE_WIDTH;

	cminx = CLIP_MINX;
	cmaxx = CLIP_MAXX - 1;
	cminy = CLIP_MINY;
	cmaxy = CLIP_MAXY - 1;

	// flip correction registers
	flipy = m_regs[0] & 0x20;
	if (flipy)
	{
		corr = m_regs[0x3c/2];
		if (corr & 0x400)
			corr |= 0xfffff800;
	}
	else
		corr = 0;

	dy += corr;
	ay = (UINT32)(dy - m_layer_offs[layer][1]) % height;

	flipx = m_regs[0] & 0x10;
	if (flipx)
	{
		corr = m_regs[0x3a/2];
		if (corr & 0x800)
			corr |= 0xfffff000;
	}
	else
		corr = 0;

	corr -= m_layer_offs[layer][0];

	switch( scrollmode )
	{
		case 0: // linescroll
			p_scroll_data = &m_videoram[scrollbank<<12] + (m_lsram_page[layer][1]>>1);
			line_height = 1;
			sdat_wrapmask = 0x3ff;
			sdat_adv = 2;
		break;
		case 2: // rowscroll
			p_scroll_data = &m_videoram[scrollbank << 12] + (m_lsram_page[layer][1] >> 1);
			line_height = 8;
			sdat_wrapmask = 0x3ff;
			sdat_adv = 16;
		break;
		default: // xyscroll
			p_scroll_data = ram16;
			line_height = K056832_PAGE_HEIGHT;
			sdat_wrapmask = 0;
			sdat_adv = 0;
			ram16[0] = 0;
			ram16[1] = dx;
	}
	if (flipy)
		sdat_adv = -sdat_adv;

	last_active = m_active_layer;

	for (r = 0; r < rowspan; r++)
	{
		if (rowspan > 1)
		{
			sy = ay;
			ty = r * K056832_PAGE_HEIGHT;

			if (!flipy)
			{
				// handle bottom-edge wraparoundness and cull off-screen tilemaps
				if ((r == 0) && (sy > height - K056832_PAGE_HEIGHT)) sy -= height;
				if ((sy + K056832_PAGE_HEIGHT <= ty) || (sy - K056832_PAGE_HEIGHT >= ty)) continue;

				// switch frame of reference and clip y
				if ((ty -= sy) >= 0)
				{
					cliph = K056832_PAGE_HEIGHT - ty;
					clipy = line_starty = ty;
					line_endy = K056832_PAGE_HEIGHT;
					sdat_start = 0;
				}
				else
				{
					cliph = K056832_PAGE_HEIGHT + ty;
					ty = -ty;
					clipy = line_starty = 0;
					line_endy = cliph;
					sdat_start = ty;
					if (scrollmode == 2) { sdat_start &= ~7; line_starty -= ty & 7; }
				}
			}
			else
			{
				ty += K056832_PAGE_HEIGHT;

				// handle top-edge wraparoundness and cull off-screen tilemaps
				if ((r == rowspan - 1) && (sy < K056832_PAGE_HEIGHT)) sy += height;
				if ((sy + K056832_PAGE_HEIGHT <= ty) || (sy - K056832_PAGE_HEIGHT >= ty)) continue;

				// switch frame of reference and clip y
				if ((ty -= sy) <= 0)
				{
					cliph = K056832_PAGE_HEIGHT + ty;
					clipy = line_starty = -ty;
					line_endy = K056832_PAGE_HEIGHT;
					sdat_start = K056832_PAGE_HEIGHT - 1;
					if (scrollmode == 2) sdat_start &= ~7;
				}
				else
				{
					cliph = K056832_PAGE_HEIGHT - ty;
					clipy = line_starty = 0;
					line_endy = cliph;
					sdat_start = cliph - 1;
					if (scrollmode == 2)
					{
						sdat_start &= ~7;
						line_starty -= ty & 7;
					}
				}
			}
		}
		else
		{
			cliph = line_endy = K056832_PAGE_HEIGHT;
			clipy = line_starty = 0;

			if (!flipy)
				sdat_start = dy;
			else
				/*
				    doesn't work with Metamorphic Force and Martial Champion (software Y-flipped) but
				    LE2U (naturally Y-flipped) seems to expect this condition as an override.

				    sdat_start = K056832_PAGE_HEIGHT-1 -dy;
				*/
			sdat_start = K056832_PAGE_HEIGHT - 1;

			if (scrollmode == 2) {
				if (K056832_metamorphic_textfix) sdat_start = dy - 8; // fix for Metamorphic Force "Break the Statue"
				sdat_start &= ~7;
				line_starty -= dy & 7;
			}
		}

		sdat_start += r * K056832_PAGE_HEIGHT;
		sdat_start <<= 1;

		clipmaxy = clipy + cliph - 1;

		for (c = 0; c < colspan; c++)
		{
			pageIndex = (((rowstart + r) & 3) << 2) + ((colstart + c) & 3);

			if (m_layer_association)
			{
				if (m_layer_assoc_with_page[pageIndex] != layer)
					continue;
			}
			else
			{
				if (m_layer_assoc_with_page[pageIndex] == -1)
					continue;

				m_active_layer = layer;
			}

			if (K055555_enabled == 0)       // are we using k055555 palette?
			{
				if (!pageIndex)
					m_active_layer = 0;
			}

		//	if (update_linemap(pageIndex, flags)) // unnecessary?
			//		continue;

			if (!m_page_tile_mode[pageIndex]) continue; // this was hidden in update_linemap()

			tmap = pageIndex;

			tmap_scrolly = ay;

			last_dx = 0x100000;
			last_visible = 0;

			for (sdat_walk = sdat_start, line_y = line_starty; line_y < line_endy; sdat_walk += sdat_adv, line_y += line_height)
			{
				dminy = line_y;
				dmaxy = line_y + line_height - 1;

				if (dminy < clipy) dminy = clipy;
				if (dmaxy > clipmaxy) dmaxy = clipmaxy;
				if (dminy > cmaxy || dmaxy < cminy) continue;

				sdat_offs = sdat_walk & sdat_wrapmask;

				clip_data[2] = (dminy < cminy ) ? cminy : dminy;
				clip_data[3] = (dmaxy > cmaxy ) ? cmaxy : dmaxy;

				if ((scrollmode == 2) && (flags & K056832_DRAW_FLAG_MIRROR) && (flipy))
					dx = ((INT32)p_scroll_data[sdat_offs + 0x1e0 + 14]<<16 | (INT32)p_scroll_data[sdat_offs + 0x1e0 + 15]) + corr;
				else
					dx = ((INT32)p_scroll_data[sdat_offs]<<16 | (INT32)p_scroll_data[sdat_offs + 1]) + corr;

				if ((INT32)last_dx == dx) { if (last_visible) draw_layer_internal(layer, tmap, clip_data, tmap_scrollx, tmap_scrolly, flags, priority); continue; }
				last_dx = dx;

				if (colspan > 1)
				{
					//sx = (UINT32)dx % width;
					sx = (UINT32)dx & (width-1);

					//tx = c * K056832_PAGE_WIDTH;
					tx = c << 9;

					if (!flipx)
					{
						// handle right-edge wraparoundness and cull off-screen tilemaps
						if ((c == 0) && (sx > width - K056832_PAGE_WIDTH)) sx -= width;
						if ((sx + K056832_PAGE_WIDTH <= tx) || (sx - K056832_PAGE_WIDTH >= tx))
							{ last_visible = 0; continue; }

						// switch frame of reference and clip x
						if ((tx -= sx) <= 0) { clipw = K056832_PAGE_WIDTH + tx; clipx = 0; }
						else { clipw = K056832_PAGE_WIDTH - tx; clipx = tx; }
					}
					else
					{
						tx += K056832_PAGE_WIDTH;

						// handle left-edge wraparoundness and cull off-screen tilemaps
						if ((c == colspan-1) && (sx < K056832_PAGE_WIDTH)) sx += width;
						if ((sx + K056832_PAGE_WIDTH <= tx) || (sx - K056832_PAGE_WIDTH >= tx))
							{ last_visible = 0; continue; }

						// switch frame of reference and clip y
						if ((tx -= sx) >= 0) { clipw = K056832_PAGE_WIDTH - tx; clipx = 0; }
						else { clipw = K056832_PAGE_WIDTH + tx; clipx = -tx; }
					}
				}
				else { clipw = K056832_PAGE_WIDTH; clipx = 0; }

				last_visible = 1;

				dminx = clipx;
				dmaxx = clipx + clipw - 1;

				clip_data[0] = (dminx < cminx ) ? cminx : dminx;
				clip_data[1] = (dmaxx > cmaxx ) ? cmaxx : dmaxx;

				// soccer superstars visible area is >512 pixels, this causes problems with the logic because
				// the tilemaps are 512 pixels across.  Assume that if the limits were set as below that we
				// want the tilemap to be drawn on the right hand side..  this is probably not the correct
				// logic, but it works.
				if ((clip_data[0]>0) && (clip_data[1]==511))
					clip_data[1]=cmaxx;

				tmap_scrollx = dx;

				draw_layer_internal(layer, tmap, clip_data, tmap_scrollx, tmap_scrolly, flags, priority);
			}
		}
	}

	m_active_layer = last_active;
}

int K056832GetLayerAssociation()
{
	return m_layer_association;
}

void K056832Metamorphic_Fixup()
{ // Metmorphic Force (metamrph)'s scroll data has a different offset.  Notably, this fixes the jumbled up "Break the Statue" text. (for those familiar with the game)
	K056832_metamorphic_textfix = 1;
}

// some of these this may not be necessary to save...
void K056832Scan(INT32 nAction)
{
	struct BurnArea ba;
	
	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = (UINT8*)K056832VideoRAM;
		ba.nLen	  = 0x2000 * 0x11 * 2;
		ba.szName = "K056832 Video RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		for (INT32 i = 0; i < 0x20; i++) {
			SCAN_VAR(k056832Regs[i]);
			SCAN_VAR(k056832Regsb[i]);
		}

		for (INT32 i = 0; i < 16; i++) {
			SCAN_VAR(m_layer_assoc_with_page[i]);
		}

		for (INT32 i = 0; i < 8; i++) {
			SCAN_VAR(m_layer_tile_mode[i]);
			SCAN_VAR(m_lsram_page[i][0]);
			SCAN_VAR(m_lsram_page[i][1]);
		}

		SCAN_VAR(m_use_ext_linescroll); // ?
		SCAN_VAR(m_layer_association); // ?
		SCAN_VAR(m_active_layer);
		SCAN_VAR(m_selected_page);
		SCAN_VAR(m_selected_page_x4096);
		SCAN_VAR(m_default_layer_association);
		SCAN_VAR(m_uses_tile_banks);
		SCAN_VAR(m_cur_tile_bank);
		SCAN_VAR(m_cur_gfx_banks);
		SCAN_VAR(m_num_gfx_banks);
		SCAN_VAR(tilemap_flip);
		SCAN_VAR(m_rom_half);
	}
}
