/*##########################################################################

    atarirle.c

    RLE sprite handling for early-to-mid 90's Atari raster games.

############################################################################

    Description:

    Beginning with Hydra, and continuing through to Primal Rage, Atari used
    RLE-compressed sprites. These sprites were decoded, colored, and scaled
    on the fly using an AMD 29C101 ALU unit. The instructions for the ALU
    were read from 3 512-byte PROMs and fed into the instruction input.

    See the bottom of the source for more details on the operation of these
    components.

##########################################################################*/

#include "tiles_generic.h"
#include "atarirle.h"


/*##########################################################################
    TYPES & STRUCTURES
##########################################################################*/

/* internal structure containing a word index, shift and mask */
struct atarirle_mask
{
	int					word;				/* word index */
	int					shift;				/* shift amount */
	int					mask;				/* final mask */
};

/* internal structure for sorting the motion objects */
struct mo_sort_entry
{
	struct mo_sort_entry *next;
	int entry;
};

/* internal structure describing each object in the ROMs */
struct atarirle_info
{
	INT16				width;
	INT16 				height;
	INT16 				xoffs;
	INT16 				yoffs;
	UINT8 				bpp;
	const UINT16 *	table;
	const UINT16 *	data;
};

/* internal structure containing the state of the motion objects */
struct atarirle_data
{
	int					bitmapwidth;		/* width of the full playfield bitmap */
	int					bitmapheight;		/* height of the full playfield bitmap */
	int					bitmapxmask;		/* x coordinate mask for the playfield bitmap */
	int					bitmapymask;		/* y coordinate mask for the playfield bitmap */

	int					spriterammask;		/* combined mask when accessing sprite RAM with raw addresses */
	int					spriteramsize;		/* total size of sprite RAM, in entries */

	int					palettebase;		/* base palette entry */
	int					maxcolors;			/* maximum number of colors */

	clip_struct	cliprect;			/* clipping clip_struct */

	struct atarirle_mask codemask;			/* mask for the code index */
	struct atarirle_mask colormask;			/* mask for the color */
	struct atarirle_mask xposmask;			/* mask for the X position */
	struct atarirle_mask yposmask;			/* mask for the Y position */
	struct atarirle_mask scalemask;			/* mask for the scale factor */
	struct atarirle_mask hflipmask;			/* mask for the horizontal flip */
	struct atarirle_mask ordermask;			/* mask for the order */
	struct atarirle_mask prioritymask;		/* mask for the priority */
	struct atarirle_mask vrammask;			/* mask for the VRAM target */

	const UINT16 *	rombase;			/* pointer to the base of the GFX ROM */
	int					romlength;			/* length of the GFX ROM */
	int					objectcount;		/* number of objects in the ROM */
	struct atarirle_info *info;				/* list of info records */
	struct atarirle_entry *spriteram;		/* pointer to sprite RAM */

	UINT16 *vram[2][2];			/* pointers to VRAM bitmaps and backbuffers */
	int					partial_scanline;	/* partial update scanline */

	UINT8				control_bits;		/* current control bits */
	UINT8				command;			/* current command */
	UINT8				is32bit;			/* 32-bit or 16-bit? */
	UINT16				checksums[256];		/* checksums for each 0x40000 bytes */
};



/*##########################################################################
    MACROS
##########################################################################*/

/* data extraction */
#define EXTRACT_DATA(_input, _mask) (((_input)->data[(_mask).word] >> (_mask).shift) & (_mask).mask)



/*##########################################################################
    GLOBAL VARIABLES
##########################################################################*/

UINT16 *atarirle_0_spriteram;
UINT32 *atarirle_0_spriteram32;

UINT16 *atarirle_0_command;
UINT32 *atarirle_0_command32;

UINT16 *atarirle_0_table;
UINT32 *atarirle_0_table32;

int atarirle_hilite_index = -1;



/*##########################################################################
    STATIC VARIABLES
##########################################################################*/

static struct atarirle_data atarirle[ATARIRLE_MAX];

static UINT8 rle_bpp[8];
static UINT16 *rle_table[8];
static UINT16 *rle_table_base;



/*##########################################################################
    STATIC FUNCTION DECLARATIONS
##########################################################################*/

static int build_rle_tables(void);
static int count_objects(const UINT16 *base, int length);
static void prescan_rle(const struct atarirle_data *mo, int which);
static void sort_and_render(struct atarirle_data *mo);
static void compute_checksum(struct atarirle_data *mo);
static void draw_rle(struct atarirle_data *mo, UINT16 *bitmap, int code, int color, int hflip, int vflip,
		int x, int y, int xscale, int yscale, const clip_struct *clip);
static void draw_rle_zoom(UINT16 *bitmap, const struct atarirle_info *gfx,
		UINT32 palette, int sx, int sy, int scalex, int scaley,
		const clip_struct *clip);
static void draw_rle_zoom_hflip(UINT16 *bitmap, const struct atarirle_info *gfx,
		UINT32 palette, int sx, int sy, int scalex, int scaley,
		const clip_struct *clip);



/*##########################################################################
    inline FUNCTIONS
##########################################################################*/

/*---------------------------------------------------------------
    compute_log: Computes the number of bits necessary to
    hold a given value. The input must be an even power of
    two.
---------------------------------------------------------------*/

inline int compute_log(int value)
{
	int log = 0;

	if (value == 0)
		return -1;
	while (!(value & 1))
		log++, value >>= 1;
	if (value != 1)
		return -1;
	return log;
}


/*---------------------------------------------------------------
    round_to_powerof2: Rounds a number up to the nearest
    power of 2. Even powers of 2 are rounded up to the
    next greatest power (e.g., 4 returns 8).
---------------------------------------------------------------*/

inline int round_to_powerof2(int value)
{
	int log = 0;

	if (value == 0)
		return 1;
	while ((value >>= 1) != 0)
		log++;
	return 1 << (log + 1);
}


/*---------------------------------------------------------------
    collapse_bits: Moving right-to-left, for each 1 bit in
    the mask, copy the corresponding bit from the input
    value into the result, packing the bits along the way.
---------------------------------------------------------------*/

inline int collapse_bits(int value, int mask)
{
	int testmask, ormask;
	int result = 0;

	for (testmask = ormask = 1; testmask != 0; testmask <<= 1)
		if (mask & testmask)
		{
			if (value & testmask)
				result |= ormask;
			ormask <<= 1;
		}
	return result;
}


/*---------------------------------------------------------------
    convert_mask: Converts a 4-word mask into a word index,
    shift, and adjusted mask. Returns 0 if invalid.
---------------------------------------------------------------*/

inline int convert_mask(const struct atarirle_entry *input, struct atarirle_mask *result)
{
	int i, temp;

	/* determine the word and make sure it's only 1 */
	result->word = -1;
	for (i = 0; i < 8; i++)
		if (input->data[i])
		{
			if (result->word == -1)
				result->word = i;
			else
				return 0;
		}

	/* if all-zero, it's valid */
	if (result->word == -1)
	{
		result->word = result->shift = result->mask = 0;
		return 1;
	}

	/* determine the shift and final mask */
	result->shift = 0;
	temp = input->data[result->word];
	while (!(temp & 1))
	{
		result->shift++;
		temp >>= 1;
	}
	result->mask = temp;
	return 1;
}



/*##########################################################################
    GLOBAL FUNCTIONS
##########################################################################*/

/*---------------------------------------------------------------
    atarirle_init: Configures the motion objects using the input
    description. Allocates all memory necessary and generates
    the attribute lookup table.
---------------------------------------------------------------*/

int atarirle_init(int map, const struct atarirle_desc *desc, UINT8 *rombase, INT32 romsize)
{
	const UINT16 *base = (const UINT16 *)rombase;
	struct atarirle_data *mo = &atarirle[map];
	int i;

	/* verify the map index */
	if (map < 0 || map >= ATARIRLE_MAX)
		return 0;

	/* build and allocate the generic tables */
	if (!build_rle_tables())
		return 0;

	/* determine the masks first */
	convert_mask(&desc->codemask,     &mo->codemask);
	convert_mask(&desc->colormask,    &mo->colormask);
	convert_mask(&desc->xposmask,     &mo->xposmask);
	convert_mask(&desc->yposmask,     &mo->yposmask);
	convert_mask(&desc->scalemask,    &mo->scalemask);
	convert_mask(&desc->hflipmask,    &mo->hflipmask);
	convert_mask(&desc->ordermask,    &mo->ordermask);
	convert_mask(&desc->prioritymask, &mo->prioritymask);
	convert_mask(&desc->vrammask,     &mo->vrammask);

	/* copy in the basic data */
	mo->bitmapwidth   = round_to_powerof2(mo->xposmask.mask);
	mo->bitmapheight  = round_to_powerof2(mo->yposmask.mask);
	mo->bitmapxmask   = mo->bitmapwidth - 1;
	mo->bitmapymask   = mo->bitmapheight - 1;

	mo->spriteramsize = desc->spriteramentries;
	mo->spriterammask = desc->spriteramentries - 1;

	mo->palettebase   = desc->palettebase;
	mo->maxcolors     = desc->maxcolors / 16;

	mo->rombase       = base;
	mo->romlength     = romsize;
	mo->objectcount   = count_objects(base, mo->romlength);

	mo->cliprect.nMinx = 0;
	mo->cliprect.nMaxx = nScreenWidth;
	mo->cliprect.nMiny = 0;
	mo->cliprect.nMaxy = nScreenHeight;

	if (desc->rightclip)
	{
		mo->cliprect.nMinx = desc->leftclip;
		mo->cliprect.nMaxx = desc->rightclip;
	}

	/* compute the checksums */
	memset(mo->checksums, 0, sizeof(mo->checksums));
	for (i = 0; i < mo->romlength / 0x20000; i++)
	{
		const UINT16 *csbase = &mo->rombase[0x10000 * i];
		int cursum = 0, j;
		for (j = 0; j < 0x10000; j++) {
			cursum += (*csbase << 8) | (*csbase >> 8);
			csbase++;
		}
		mo->checksums[i] = cursum;
	}

	/* allocate the object info */
	mo->info = (atarirle_info*)BurnMalloc(sizeof(atarirle_info) * mo->objectcount);

	/* fill in the data */
	memset(mo->info, 0, sizeof(atarirle_info) * mo->objectcount);
	for (i = 0; i < mo->objectcount; i++)
		prescan_rle(mo, i);

	/* allocate the spriteram */
	mo->spriteram = (atarirle_entry*)BurnMalloc(sizeof(atarirle_entry) * mo->spriteramsize);

	/* clear it to zero */
	memset(mo->spriteram, 0, sizeof(atarirle_entry) * mo->spriteramsize);

	BurnBitmapAllocate(1, nScreenWidth, nScreenHeight+4, 0); // +4 because rle sometimes likes to ignore clipping and go out of bounds..
	BurnBitmapAllocate(2, nScreenWidth, nScreenHeight+4, 0);
	BurnBitmapAllocate(3, nScreenWidth, nScreenHeight+4, 0);
	BurnBitmapAllocate(4, nScreenWidth, nScreenHeight+4, 0);

	/* allocate bitmaps */
	mo->vram[0][0] = BurnBitmapGetBitmap(1);
	mo->vram[0][1] = BurnBitmapGetBitmap(2);
	if (!mo->vram[0][0] || !mo->vram[0][1])
		return 0;
	BurnBitmapFill(1, 0);
	BurnBitmapFill(2, 0);

	/* allocate alternate bitmaps if needed */
	if (mo->vrammask.mask != 0)
	{
		mo->vram[1][0] = BurnBitmapGetBitmap(3);
		mo->vram[1][1] = BurnBitmapGetBitmap(4);
		if (!mo->vram[1][0] || !mo->vram[1][1])
			return 0;
		BurnBitmapFill(3, 0);
		BurnBitmapFill(4, 0);
	}

	mo->partial_scanline = -1;
	return 1;
}

void atarirle_exit()
{
	BurnFree(rle_table_base);

	for (INT32 i = 0; i < ATARIRLE_MAX; i++) {
		struct atarirle_data *mo = &atarirle[i];

		BurnFree(mo->info);
		BurnFree(mo->spriteram);
	}
}

INT32 atarirle_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_VOLATILE)
	{
		for (INT32 i = 0; i < ATARIRLE_MAX; i++)
		{
			atarirle_data *mo = &atarirle[i];

			if (mo->spriteram)
			{
				ScanVar(mo->spriteram, sizeof(atarirle_entry) * mo->spriteramsize, "AtariRLE RAM");

				SCAN_VAR(mo->control_bits);
				SCAN_VAR(mo->command);
			}
		}
	}

	return 0;
}

// partial_erase() only erases full lines (doesn't respect nMinx/nMaxx as it doesn't need to)
static void partial_erase(INT32 map, clip_struct *cliprect)
{
	if (cliprect->nMiny > cliprect->nMaxy || cliprect->nMiny == cliprect->nMaxy)
		return;

	for (INT32 y = cliprect->nMiny; y < cliprect->nMaxy; y++) {
		UINT16 *line = BurnBitmapGetPosition(map, 0, y);

		if (y < nScreenHeight)
			memset(line, 0, nScreenWidth * sizeof(UINT16));
	}
}

/*---------------------------------------------------------------
    atarirle_control_w: Write handler for MO control bits.
---------------------------------------------------------------*/

void atarirle_control_w(int map, UINT8 bits, INT32 scanline)
{
	struct atarirle_data *mo = &atarirle[map];
	int oldbits = mo->control_bits;

	/* do nothing if nothing changed */
	if (oldbits == bits)
		return;

	/* force a partial update first */
	//	video_screen_update_partial(0, scanline); // iq_132

	/* if the erase flag was set, erase the front map */
	if (oldbits & ATARIRLE_CONTROL_ERASE)
	{
		clip_struct cliprect = mo->cliprect;

		/* compute the top and bottom of the rect */
		if (mo->partial_scanline + 1 > cliprect.nMiny)
			cliprect.nMiny = mo->partial_scanline + 1;
		if (scanline < cliprect.nMaxy)
			cliprect.nMaxy = scanline;

		//bprintf(0, _T("  partial erase %d-%d (frame %d)\n"), cliprect.nMiny, cliprect.nMaxy, (oldbits & ATARIRLE_CONTROL_FRAME) >> 2);

		/* erase the bitmap */
		//BurnBitmapFill(1+((oldbits & ATARIRLE_CONTROL_FRAME) >> 2), 0);
		partial_erase(1+((oldbits & ATARIRLE_CONTROL_FRAME) >> 2), &cliprect);
		if (mo->vrammask.mask != 0)
			partial_erase(3+((oldbits & ATARIRLE_CONTROL_FRAME) >> 2), &cliprect);
		// BurnBitmapFill(3+((oldbits & ATARIRLE_CONTROL_FRAME) >> 2), 0);
	}

	/* update the bits */
	mo->control_bits = bits;

	/* if mogo is set, do a render on the rising edge */
	if (!(oldbits & ATARIRLE_CONTROL_MOGO) && (bits & ATARIRLE_CONTROL_MOGO))
	{
		if (mo->command == ATARIRLE_COMMAND_DRAW) {
			sort_and_render(mo);
		}
		else if (mo->command == ATARIRLE_COMMAND_CHECKSUM)
			compute_checksum(mo);
	}

	/* remember where we left off */
	mo->partial_scanline = scanline;
}



/*---------------------------------------------------------------
    atarirle_control_w: Write handler for MO command bits.
---------------------------------------------------------------*/

void atarirle_command_w(int map, UINT8 command)
{
	struct atarirle_data *mo = &atarirle[map];
	mo->command = command;
}



/*---------------------------------------------------------------
    video_eof_atarirle: Flush remaining changes.
---------------------------------------------------------------*/

void atarirle_eof()
{
	int i;

//logerror("video_eof_atarirle\n");

	/* loop over all RLE handlers */
	for (i = 0; i < ATARIRLE_MAX; i++)
	{
		struct atarirle_data *mo = &atarirle[i];

		/* if the erase flag is set, erase to the end of the screen */
		if (mo->control_bits & ATARIRLE_CONTROL_ERASE)
		{
			clip_struct cliprect = mo->cliprect;

			/* compute top only; bottom is equal to visible_area */
			if (mo->partial_scanline + 1 > cliprect.nMiny)
				cliprect.nMiny = mo->partial_scanline + 1;

			//bprintf(0, _T("  partial erase %d-%d (frame %d)\n"), cliprect.nMiny, cliprect.nMaxy, (mo->control_bits & ATARIRLE_CONTROL_FRAME) >> 2);

			/* erase the bitmap */
			//BurnBitmapFill(1+((mo->control_bits & ATARIRLE_CONTROL_FRAME) >> 2), 0);
			partial_erase(1+((mo->control_bits & ATARIRLE_CONTROL_FRAME) >> 2), &cliprect);
			if (mo->vrammask.mask != 0)
				partial_erase(3+((mo->control_bits & ATARIRLE_CONTROL_FRAME) >> 2), &cliprect);
				//BurnBitmapFill(3+((mo->control_bits & ATARIRLE_CONTROL_FRAME) >> 2), 0);
		}

		/* reset the partial scanline to -1 so we can detect full updates */
		mo->partial_scanline = -1;
	}
}



/*---------------------------------------------------------------
    atarirle_0_spriteram_w: Write handler for the spriteram.
---------------------------------------------------------------*/

void atarirle_0_spriteram_w(UINT32 offset)
{
	int entry = (offset >> 3) & atarirle[0].spriterammask;
	int idx = offset & 7;

	/* combine raw data */
//	COMBINE_DATA(&atarirle_0_spriteram[offset]);

	/* store a copy in our local spriteram */
	atarirle[0].spriteram[entry].data[idx] = atarirle_0_spriteram[offset];
	atarirle[0].is32bit = 0;
}



/*---------------------------------------------------------------
    atarirle_0_spriteram32_w: Write handler for the spriteram.
---------------------------------------------------------------*/

#if 0
WRITE32_HANDLER( atarirle_0_spriteram32_w )
{
	int entry = (offset >> 2) & atarirle[0].spriterammask;
	int idx = 2 * (offset & 3);

	/* combine raw data */
	COMBINE_DATA(&atarirle_0_spriteram32[offset]);

	/* store a copy in our local spriteram */
	atarirle[0].spriteram[entry].data[idx+0] = atarirle_0_spriteram32[offset] >> 16;
	atarirle[0].spriteram[entry].data[idx+1] = atarirle_0_spriteram32[offset];
	atarirle[0].is32bit = 1;
}
#endif


/*---------------------------------------------------------------
    atarirle_get_vram: Return the VRAM bitmap.
---------------------------------------------------------------*/

UINT16 *atarirle_get_vram(int map, int idx)
{
	struct atarirle_data *mo = &atarirle[map];
//logerror("atarirle_get_vram (frame %d)\n", (mo->control_bits & ATARIRLE_CONTROL_FRAME) >> 2);
	return mo->vram[idx][(mo->control_bits & ATARIRLE_CONTROL_FRAME) >> 2];
}



/*---------------------------------------------------------------
    build_rle_tables: Builds internal table for RLE mapping.
---------------------------------------------------------------*/

static int build_rle_tables(void)
{
	int i;

	/* if we've already done it, don't bother */
//  if (rle_table[0])
//      return 0;

	/* allocate all 5 tables */
	rle_table_base = (UINT16*)BurnMalloc(0x500 * sizeof(UINT16));

	/* assign the tables */
	rle_table[0] = &rle_table_base[0x000];
	rle_table[1] = &rle_table_base[0x100];
	rle_table[2] = rle_table[3] = &rle_table_base[0x200];
	rle_table[4] = rle_table[6] = &rle_table_base[0x300];
	rle_table[5] = rle_table[7] = &rle_table_base[0x400];

	/* set the bpps */
	rle_bpp[0] = 4;
	rle_bpp[1] = rle_bpp[2] = rle_bpp[3] = 5;
	rle_bpp[4] = rle_bpp[5] = rle_bpp[6] = rle_bpp[7] = 6;

	/* build the 4bpp table */
	for (i = 0; i < 256; i++)
		rle_table[0][i] = (((i & 0xf0) + 0x10) << 4) | (i & 0x0f);

	/* build the 5bpp table */
	for (i = 0; i < 256; i++)
		rle_table[2][i] = (((i & 0xe0) + 0x20) << 3) | (i & 0x1f);

	/* build the special 5bpp table */
	for (i = 0; i < 256; i++)
	{
		if ((i & 0x0f) == 0)
			rle_table[1][i] = (((i & 0xf0) + 0x10) << 4) | (i & 0x0f);
		else
			rle_table[1][i] = (((i & 0xe0) + 0x20) << 3) | (i & 0x1f);
	}

	/* build the 6bpp table */
	for (i = 0; i < 256; i++)
		rle_table[5][i] = (((i & 0xc0) + 0x40) << 2) | (i & 0x3f);

	/* build the special 6bpp table */
	for (i = 0; i < 256; i++)
	{
		if ((i & 0x0f) == 0)
			rle_table[4][i] = (((i & 0xf0) + 0x10) << 4) | (i & 0x0f);
		else
			rle_table[4][i] = (((i & 0xc0) + 0x40) << 2) | (i & 0x3f);
	}

	return 1;
}



/*---------------------------------------------------------------
    count_objects: Determines the number of objects in the
    motion object ROM.
---------------------------------------------------------------*/

int count_objects(const UINT16 *base, int length)
{
	int lowest_address = length;
	int i;

	/* first determine the lowest address of all objects */
	for (i = 0; i < lowest_address; i += 4)
	{
		int offset = ((base[i + 2] & 0xff) << 16) | base[i + 3];
		//logerror("count_objects: i=%d offset=%08X\n", i, offset);
		if (offset > i && offset < lowest_address)
			lowest_address = offset;
	}

	//bprintf (0, _T("Lowest: %x\n"), lowest_address/4);

	/* that determines how many objects */
	return lowest_address / 4;
}



/*---------------------------------------------------------------
    prescan_rle: Prescans an RLE object, computing the
    width, height, and other goodies.
---------------------------------------------------------------*/

static void prescan_rle(const struct atarirle_data *mo, int which)
{
	struct atarirle_info *rledata = &mo->info[which];
	UINT16 *base = (UINT16 *)&mo->rombase[which * 4];
	const UINT16 *end = mo->rombase + mo->romlength / 2;
	int width = 0, height, flags, offset;
	const UINT16 *table;

	/* look up the offset */
	rledata->xoffs = (INT16)base[0];
	rledata->yoffs = (INT16)base[1];

	/* determine the depth and table */
	flags = base[2];
	rledata->bpp = rle_bpp[(flags >> 8) & 7];
	table = rledata->table = rle_table[(flags >> 8) & 7];

	/* determine the starting offset */
	offset = ((base[2] & 0xff) << 16) | base[3];
	rledata->data = base = (UINT16 *)&mo->rombase[offset];

	/* make sure it's valid */
	if (offset < which * 4 || offset >= mo->romlength)
	{
		memset(rledata, 0, sizeof(atarirle_info));
		return;
	}

	/* first pre-scan to determine the width and height */
	for (height = 0; height < 1024 && base < end; height++)
	{
		int tempwidth = 0;
		int entry_count = *base++;

		/* if the high bit is set, assume we're inverted */
		if (entry_count & 0x8000)
		{
			entry_count ^= 0xffff;

			/* also change the ROM data so we don't have to do this again at runtime */
			base[-1] ^= 0xffff;
		}

		/* we're done when we hit 0 */
		if (entry_count == 0)
			break;

		/* track the width */
		while (entry_count-- && base < end)
		{
			int word = *base++;
			int count, value;

			/* decode the low byte first */
			count = table[word & 0xff];
			value = count & 0xff;
			tempwidth += count >> 8;

			/* decode the upper byte second */
			count = table[word >> 8];
			value = count & 0xff;
			tempwidth += count >> 8;
		}

		/* only remember the max */
		if (tempwidth > width)
			width = tempwidth;
	}

	/* fill in the data */
	rledata->width = width;
	rledata->height = height;
}



/*---------------------------------------------------------------
    compute_checksum: Compute the checksum values on the ROMs.
---------------------------------------------------------------*/

static void compute_checksum(struct atarirle_data *mo)
{
	int reqsums = mo->spriteram[0].data[0] + 1;
	int i;

	//bprintf(0, _T("atarirle: compute_checksum(%d)\n"), reqsums);

	/* number of checksums is in the first word */
	if (reqsums > 256)
		reqsums = 256;

	/* stuff them back */
	if (!mo->is32bit)
	{
		for (i = 0; i < reqsums; i++)
			atarirle_0_spriteram[i] = mo->checksums[i];
	}
	else
	{
		for (i = 0; i < reqsums; i++)
			if (i & 1)
				atarirle_0_spriteram32[i/2] = (atarirle_0_spriteram32[i/2] & 0xffff0000) | mo->checksums[i];
			else
				atarirle_0_spriteram32[i/2] = (atarirle_0_spriteram32[i/2] & 0x0000ffff) | (mo->checksums[i] << 16);
	}
}


/*---------------------------------------------------------------
    sort_and_render: Render all motion objects in order.
---------------------------------------------------------------*/

static void sort_and_render(struct atarirle_data *mo)
{
	UINT16 *bitmap1 = mo->vram[0][(~mo->control_bits & ATARIRLE_CONTROL_FRAME) >> 2];
	UINT16 *bitmap2 = mo->vram[1][(~mo->control_bits & ATARIRLE_CONTROL_FRAME) >> 2];
	struct atarirle_entry *obj = mo->spriteram;
	struct mo_sort_entry sort_entry[256];
	struct mo_sort_entry *list_head[256];
	struct mo_sort_entry *current;
	int i;

	struct atarirle_entry *hilite = NULL;
	int count = 0;

	/* sort the motion objects into their proper priorities */
	memset(list_head, 0, sizeof(list_head));
	for (i = 0; i < 256; i++, obj++)
	{
		int order = EXTRACT_DATA(obj, mo->ordermask);
		sort_entry[i].entry = i;
		sort_entry[i].next = list_head[order];
		list_head[order] = &sort_entry[i];
	}

	/* now loop back and process */
	count = 0;
	for (i = 1; i < 256; i++)
		for (current = list_head[i]; current; current = current->next)
		{
			int scale, code;

			/* extract scale and code */
			obj = &mo->spriteram[current->entry];
			scale = EXTRACT_DATA(obj, mo->scalemask);
			code = EXTRACT_DATA(obj, mo->codemask);

			/* make sure they are in range */
			if (scale > 0 && code < mo->objectcount)
			{
				//bprintf (0, _T("draw: %x, %x, %x\n"), scale, code, mo->objectcount);

				int hflip = EXTRACT_DATA(obj, mo->hflipmask);
				int color = EXTRACT_DATA(obj, mo->colormask);
				int priority = EXTRACT_DATA(obj, mo->prioritymask);
				int x = EXTRACT_DATA(obj, mo->xposmask);
				int y = EXTRACT_DATA(obj, mo->yposmask);
				int which = EXTRACT_DATA(obj, mo->vrammask);

				if (count++ == atarirle_hilite_index)
					hilite = obj;

				if (x & ((mo->xposmask.mask + 1) >> 1))
					x = (INT16)(x | ~mo->xposmask.mask);
				if (y & ((mo->yposmask.mask + 1) >> 1))
					y = (INT16)(y | ~mo->yposmask.mask);
				x += mo->cliprect.nMinx;

				/* merge priority and color */
				color = (color << 4) | (priority << ATARIRLE_PRIORITY_SHIFT);

				/* render to one or both bitmaps */
				if (which == 0) {
					draw_rle(mo, bitmap1, code, color, hflip, 0, x, y, scale, scale, &mo->cliprect);
				}
				if (bitmap2 && which != 0)
					draw_rle(mo, bitmap2, code, color, hflip, 0, x, y, scale, scale, &mo->cliprect);
			}
		}

	if (hilite)
	{
		int scale, code, which;

		/* extract scale and code */
		obj = hilite;
		scale = EXTRACT_DATA(obj, mo->scalemask);
		code = EXTRACT_DATA(obj, mo->codemask);
		which = EXTRACT_DATA(obj, mo->vrammask);

		/* make sure they are in range */
		if (scale > 0 && code < mo->objectcount)
		{
			int hflip = EXTRACT_DATA(obj, mo->hflipmask);
			int color = EXTRACT_DATA(obj, mo->colormask);
			int priority = EXTRACT_DATA(obj, mo->prioritymask);
			int x = EXTRACT_DATA(obj, mo->xposmask);
			int y = EXTRACT_DATA(obj, mo->yposmask);
			int scaled_xoffs, scaled_yoffs;
			const struct atarirle_info *info;

			if (x & ((mo->xposmask.mask + 1) >> 1))
				x = (INT16)(x | ~mo->xposmask.mask);
			if (y & ((mo->yposmask.mask + 1) >> 1))
				y = (INT16)(y | ~mo->yposmask.mask);
			x += mo->cliprect.nMinx;

			/* merge priority and color */
			color = (color << 4) | (priority << ATARIRLE_PRIORITY_SHIFT);

			info = &mo->info[code];
			scaled_xoffs = (scale * info->xoffs) >> 12;
			scaled_yoffs = (scale * info->yoffs) >> 12;

			/* we're hflipped, account for it */
			if (hflip)
				scaled_xoffs = ((scale * info->width) >> 12) - scaled_xoffs;

			/* adjust for the x and y offsets */
			x -= scaled_xoffs;
			y -= scaled_yoffs;

			do
			{
				int scaled_width = (scale * info->width + 0x7fff) >> 12;
				int scaled_height = (scale * info->height + 0x7fff) >> 12;
				int dx, dy, ex, ey, sx = x, sy = y, tx, ty;

				/* make sure we didn't end up with 0 */
				if (scaled_width == 0) scaled_width = 1;
				if (scaled_height == 0) scaled_height = 1;

				/* compute the remaining parameters */
				dx = (info->width << 12) / scaled_width;
				dy = (info->height << 12) / scaled_height;
				ex = sx + scaled_width - 1;
				ey = sy + scaled_height - 1;

				/* left edge clip */
				if (sx < 0)
					sx = 0;
				if (sx >= nScreenWidth)
					break;

				/* right edge clip */
				if (ex >= nScreenWidth)
					ex = nScreenWidth - 1; // right?? iq_132
				else if (ex < 0)
					break;

				/* top edge clip */
				if (sy < 0)
					sy = 0;
				else if (sy >= nScreenHeight)
					break;

				/* bottom edge clip */
				if (ey >= nScreenHeight)
					ey = nScreenHeight-1; // right?? iq_132
				else if (ey < 0)
					break;

				for (ty = sy; ty <= ey; ty++)
				{
					bitmap1[(ty * nScreenWidth) + sx] = rand() & 0xff;
					bitmap1[(ty * nScreenWidth) + ex] = rand() & 0xff;
					//	plot_pixel(bitmap1, sx, ty, rand() & 0xff);
					//	plot_pixel(bitmap1, ex, ty, rand() & 0xff);
				}
				for (tx = sx; tx <= ex; tx++)
				{
					bitmap1[(sy * nScreenWidth) + tx] = rand() & 0xff;
					bitmap1[(ey * nScreenWidth) + tx] = rand() & 0xff;
					//	plot_pixel(bitmap1, tx, sy, rand() & 0xff);
					//	plot_pixel(bitmap1, tx, ey, rand() & 0xff);
				}
			} while (0);

			/*fprintf(stderr, "   Sprite: c=%04X l=%04X h=%d X=%4d (o=%4d w=%3d) Y=%4d (o=%4d h=%d) s=%04X\n",
			 code, color, hflip,
			 x, -scaled_xoffs, (scale * info->width) >> 12,
			 y, -scaled_yoffs, (scale * info->height) >> 12, scale); */
		}
	}
}



/*---------------------------------------------------------------
    draw_rle: Render a single RLE-compressed motion
    object.
---------------------------------------------------------------*/

void draw_rle(struct atarirle_data *mo, UINT16 *bitmap, int code, int color, int hflip, int vflip,
	int x, int y, int xscale, int yscale, const clip_struct *clip)
{
	UINT32 palettebase = mo->palettebase + color;
	const struct atarirle_info *info = &mo->info[code];
	int scaled_xoffs = (xscale * info->xoffs) >> 12;
	int scaled_yoffs = (yscale * info->yoffs) >> 12;

	if (vflip) {}

	/* we're hflipped, account for it */
	if (hflip)
		scaled_xoffs = ((xscale * info->width) >> 12) - scaled_xoffs;

//if (clip->nMiny == Machine->screen[0].visarea.nMiny)
//logerror("   Sprite: c=%04X l=%04X h=%d X=%4d (o=%4d w=%3d) Y=%4d (o=%4d h=%d) s=%04X\n",
//  code, color, hflip,
//  x, -scaled_xoffs, (xscale * info->width) >> 12,
//  y, -scaled_yoffs, (yscale * info->height) >> 12, xscale);

	/* adjust for the x and y offsets */
	x -= scaled_xoffs;
	y -= scaled_yoffs;

	/* bail on a NULL object */
	if (!info->data)
		return;

	/* 16-bit case */
//	if (bitmap->depth == 16)
	{
		if (!hflip)
			draw_rle_zoom(bitmap, info, palettebase, x, y, xscale << 4, yscale << 4, clip);
		else
			draw_rle_zoom_hflip(bitmap, info, palettebase, x, y, xscale << 4, yscale << 4, clip);
	}
}



/*---------------------------------------------------------------
    draw_rle_zoom: Draw an RLE-compressed object to a 16-bit
    bitmap.
---------------------------------------------------------------*/

void draw_rle_zoom(UINT16 *bitmap, const struct atarirle_info *gfx,
		UINT32 palette, int sx, int sy, int scalex, int scaley,
		const clip_struct *clip)
{


	const UINT16 *row_start = gfx->data;
	const UINT16 *table = gfx->table;
	volatile int current_row = 0;

	int scaled_width = (scalex * gfx->width + 0x7fff) >> 16;
	int scaled_height = (scaley * gfx->height + 0x7fff) >> 16;

	int pixels_to_skip = 0, xclipped = 0;
	int dx, dy, ex, ey;
	int y, sourcey;

	/* make sure we didn't end up with 0 */
	if (scaled_width == 0) scaled_width = 1;
	if (scaled_height == 0) scaled_height = 1;

	/* compute the remaining parameters */
	dx = (gfx->width << 16) / scaled_width;
	dy = (gfx->height << 16) / scaled_height;
	ex = sx + scaled_width - 1;
	ey = sy + scaled_height - 1;
	sourcey = dy / 2;

	//bprintf (0, _T("draw rle: %x, %x, %x, %x\n"), sy, ey, sx, ex);
	//bprintf (0, _T("clip: %x, %x, %x, %x\n"), clip->nMiny, nScreenHeight, clip->nMinx, nScreenWidth);
	//bprintf (0, _T("LEC\n"));

	/* left edge clip */
	if (sx < clip->nMinx)
		pixels_to_skip = clip->nMinx - sx, xclipped = 1;
	if (sx >= clip->nMaxx)
		return;

	/* right edge clip */
	if (ex >= clip->nMaxx)
		ex = clip->nMaxx, xclipped = 1;
	else if (ex < clip->nMinx)
		return;

	/* top edge clip */
	if (sy < clip->nMiny)
	{
		sourcey += (clip->nMiny - sy) * dy;
		sy = clip->nMiny;
	}
	else if (sy >= clip->nMaxy)
		return;

	/* bottom edge clip */
	if (ey >= clip->nMaxy)
		ey = clip->nMaxy;
	else if (ey < clip->nMiny)
		return;

	/* loop top to bottom */
	for (y = sy; y <= ey; y++, sourcey += dy)
	{
		UINT16 *dest = bitmap + (y * nScreenWidth) + sx; //UINT16 *)bitmap->line[y])[sx];
		int j, sourcex = dx / 2, rle_end = 0;
		const UINT16 *base;
		int entry_count;

		/* loop until we hit the row we're on */
		for ( ; current_row != (sourcey >> 16); current_row++)
			row_start += 1 + *row_start;

		/* grab our starting parameters from this row */
		base = row_start;
		entry_count = *base++;

		/* non-clipped case */
		if (!xclipped)
		{
			/* decode the pixels */
			for (j = 0; j < entry_count; j++)
			{
				int word = *base++;
				int count, value;

				/* decode the low byte first */
				count = table[word & 0xff];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (value)
				{
					value += palette;
					while (sourcex < rle_end)
						*dest++ = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest++, sourcex += dx;
				}

				/* decode the upper byte second */
				count = table[word >> 8];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (value)
				{
					value += palette;
					while (sourcex < rle_end)
						*dest++ = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest++, sourcex += dx;
				}
			}
		}

		/* clipped case */
		else
		{
			const UINT16 *end = bitmap + (y * nScreenWidth) + ex; //&((const UINT16 *)bitmap->line[y])[ex];
			int to_be_skipped = pixels_to_skip;

			/* decode the pixels */
			for (j = 0; j < entry_count && dest <= end; j++)
			{
				int word = *base++;
				int count, value;

				/* decode the low byte first */
				count = table[word & 0xff];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (to_be_skipped)
				{
					while (to_be_skipped && sourcex < rle_end)
						dest++, sourcex += dx, to_be_skipped--;
					if (to_be_skipped) goto next3;
				}
				if (value)
				{
					value += palette;
					while (sourcex < rle_end && dest <= end)
						*dest++ = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest++, sourcex += dx;
				}

			next3:
				/* decode the upper byte second */
				count = table[word >> 8];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (to_be_skipped)
				{
					while (to_be_skipped && sourcex < rle_end)
						dest++, sourcex += dx, to_be_skipped--;
					if (to_be_skipped) goto next4;
				}
				if (value)
				{
					value += palette;
					while (sourcex < rle_end && dest <= end)
						*dest++ = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest++, sourcex += dx;
				}
			next4:
				;
			}
		}
	}
}



/*---------------------------------------------------------------
    draw_rle_zoom_hflip: Draw an RLE-compressed object to a
    16-bit bitmap with horizontal flip.
---------------------------------------------------------------*/

void draw_rle_zoom_hflip(UINT16 *bitmap, const struct atarirle_info *gfx,
		UINT32 palette, int sx, int sy, int scalex, int scaley,
		const clip_struct *clip)
{
	const UINT16 *row_start = gfx->data;
	const UINT16 *table = gfx->table;
	volatile int current_row = 0;

	int scaled_width = (scalex * gfx->width + 0x7fff) >> 16;
	int scaled_height = (scaley * gfx->height + 0x7fff) >> 16;
	int pixels_to_skip = 0, xclipped = 0;
	int dx, dy, ex, ey;
	int y, sourcey;

	/* make sure we didn't end up with 0 */
	if (scaled_width == 0) scaled_width = 1;
	if (scaled_height == 0) scaled_height = 1;

	/* compute the remaining parameters */
	dx = (gfx->width << 16) / scaled_width;
	dy = (gfx->height << 16) / scaled_height;
	ex = sx + scaled_width - 1;
	ey = sy + scaled_height - 1;
	sourcey = dy / 2;

	/* left edge clip */
	if (sx < clip->nMinx)
		sx = clip->nMinx, xclipped = 1;
	if (sx >= clip->nMaxx)
		return;

	/* right edge clip */
	if (ex >= clip->nMaxx)
		pixels_to_skip = ex - clip->nMaxx, xclipped = 1;
	else if (ex < clip->nMinx)
		return;

	/* top edge clip */
	if (sy < clip->nMiny)
	{
		sourcey += (clip->nMiny - sy) * dy;
		sy = clip->nMiny;
	}
	else if (sy >= clip->nMaxy)
		return;

	/* bottom edge clip */
	if (ey >= clip->nMaxy)
		ey = clip->nMaxy;
	else if (ey < clip->nMiny)
		return;

	/* loop top to bottom */
	for (y = sy; y <= ey; y++, sourcey += dy)
	{
		UINT16 *dest = bitmap + (y * nScreenWidth) + ex; //&((UINT16 *)bitmap->line[y])[ex];
		int j, sourcex = dx / 2, rle_end = 0;
		const UINT16 *base;
		int entry_count;

		/* loop until we hit the row we're on */
		for ( ; current_row != (sourcey >> 16); current_row++)
			row_start += 1 + *row_start;

		/* grab our starting parameters from this row */
		base = row_start;
		entry_count = *base++;

		/* non-clipped case */
		if (!xclipped)
		{
			/* decode the pixels */
			for (j = 0; j < entry_count; j++)
			{
				int word = *base++;
				int count, value;

				/* decode the low byte first */
				count = table[word & 0xff];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (value)
				{
					value += palette;
					while (sourcex < rle_end)
						*dest-- = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest--, sourcex += dx;
				}

				/* decode the upper byte second */
				count = table[word >> 8];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (value)
				{
					value += palette;
					while (sourcex < rle_end)
						*dest-- = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest--, sourcex += dx;
				}
			}
		}

		/* clipped case */
		else
		{
			const UINT16 *start = bitmap + (y * nScreenWidth) + sx; //&((const UINT16 *)bitmap->line[y])[sx];
			int to_be_skipped = pixels_to_skip;

			/* decode the pixels */
			for (j = 0; j < entry_count && dest >= start; j++)
			{
				int word = *base++;
				int count, value;

				/* decode the low byte first */
				count = table[word & 0xff];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (to_be_skipped)
				{
					while (to_be_skipped && sourcex < rle_end)
						dest--, sourcex += dx, to_be_skipped--;
					if (to_be_skipped) goto next3;
				}
				if (value)
				{
					value += palette;
					while (sourcex < rle_end && dest >= start)
						*dest-- = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest--, sourcex += dx;
				}

			next3:
				/* decode the upper byte second */
				count = table[word >> 8];
				value = count & 0xff;
				rle_end += (count & 0xff00) << 8;

				/* store copies of the value until we pass the end of this chunk */
				if (to_be_skipped)
				{
					while (to_be_skipped && sourcex < rle_end)
						dest--, sourcex += dx, to_be_skipped--;
					if (to_be_skipped) goto next4;
				}
				if (value)
				{
					value += palette;
					while (sourcex < rle_end && dest >= start)
						*dest-- = value, sourcex += dx;
				}
				else
				{
					while (sourcex < rle_end)
						dest--, sourcex += dx;
				}
			next4:
				;
			}
		}
	}
}
