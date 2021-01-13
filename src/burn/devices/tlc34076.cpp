/***************************************************************************

    tlc34076.c

    Basic implementation of the TLC34076 palette chip and similar
    compatible chips.

***************************************************************************/

#include "burnint.h"
#include "tlc34076.h"

static UINT8 local_paletteram[0x300];
static UINT8 regs[0x10];
static UINT8 palettedata[3];
static UINT8 writeindex;
static UINT8 readindex;
static UINT8 dacbits;

#define PALETTE_WRITE_ADDR	0x00
#define PALETTE_DATA		0x01
#define PIXEL_READ_MASK		0x02
#define PALETTE_READ_ADDR	0x03
#define GENERAL_CONTROL		0x08
#define INPUT_CLOCK_SEL		0x09
#define OUTPUT_CLOCK_SEL	0x0a
#define MUX_CONTROL			0x0b
#define PALETTE_PAGE		0x0c
#define TEST_REGISTER		0x0e
#define RESET_STATE			0x0f

static void update_palette(int which)
{
	int totalcolors = (BurnDrvGetPaletteEntries() <= 256) ? BurnDrvGetPaletteEntries() : 256;
	int i, mask = regs[PIXEL_READ_MASK];

	for (i = 0; i < totalcolors; i++)
	{
		if (which == -1 || (i & mask) == which)
		{
			int r = local_paletteram[3 * i + 0];
			int g = local_paletteram[3 * i + 1];
			int b = local_paletteram[3 * i + 2];
			if (dacbits == 6)
			{
				r = (r << 2) | (r >> 4);
				g = (g << 2) | (g >> 4);
				b = (b << 2) | (b >> 4);
			}

			pBurnDrvPalette[i] = BurnHighCol(r,g,b,0);
		}
	}
}

void tlc34076_recalc_palette()
{
	update_palette(-1);
}

void tlc34076_reset(INT32 dacwidth)
{
	dacbits = dacwidth;
	if (dacbits != 6 && dacbits != 8)
	{
		dacbits = 6;
	}

	regs[PIXEL_READ_MASK]	= 0xff;
	regs[GENERAL_CONTROL]	= 0x03;
	regs[INPUT_CLOCK_SEL]	= 0x00;
	regs[OUTPUT_CLOCK_SEL]	= 0x3f;
	regs[MUX_CONTROL]		= 0x2d;
	regs[PALETTE_PAGE]		= 0x00;
	regs[TEST_REGISTER]		= 0x00;
	regs[RESET_STATE]		= 0x00;
}

UINT8 tlc34076_read(UINT32 offset)
{
	UINT8 result;

	offset &= 0x0f;
	result = regs[offset];

	switch (offset)
	{
		case PALETTE_DATA:
			if (readindex == 0)
			{
				palettedata[0] = local_paletteram[3 * regs[PALETTE_READ_ADDR] + 0];
				palettedata[1] = local_paletteram[3 * regs[PALETTE_READ_ADDR] + 1];
				palettedata[2] = local_paletteram[3 * regs[PALETTE_READ_ADDR] + 2];
			}
			result = palettedata[readindex++];
			if (readindex == 3)
			{
				readindex = 0;
				regs[PALETTE_READ_ADDR]++;
			}
			break;
	}

	return result;
}

void tlc34076_write(UINT32 offset, UINT8 data)
{
	UINT8 oldval;

	offset &= 0x0f;
	oldval = regs[offset];
	regs[offset] = data;

	switch (offset)
	{
		case PALETTE_WRITE_ADDR:
			writeindex = 0;
			break;

		case PALETTE_DATA:
			palettedata[writeindex++] = data;
			if (writeindex == 3)
			{
				local_paletteram[3 * regs[PALETTE_WRITE_ADDR] + 0] = palettedata[0];
				local_paletteram[3 * regs[PALETTE_WRITE_ADDR] + 1] = palettedata[1];
				local_paletteram[3 * regs[PALETTE_WRITE_ADDR] + 2] = palettedata[2];
				update_palette(regs[PALETTE_WRITE_ADDR]);
				writeindex = 0;
				regs[PALETTE_WRITE_ADDR]++;
			}
			break;

		case PALETTE_READ_ADDR:
			readindex = 0;
			break;

		case GENERAL_CONTROL:
		case INPUT_CLOCK_SEL:
		case OUTPUT_CLOCK_SEL:
			break;

		case PIXEL_READ_MASK:
		case PALETTE_PAGE:
			update_palette(-1);
			break;

		case RESET_STATE:
			tlc34076_reset(dacbits);
			break;
	}
}

void tlc34076_write16(UINT32 address, UINT16 data)
{
	 tlc34076_write(address/2, data);
}

UINT8 tlc34076_read16(UINT32 address)
{
	UINT16 ret = tlc34076_read(address/2);

	return ret | (ret << 8);
}

INT32 tlc34076_Scan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA)
	{
		SCAN_VAR(writeindex);
		SCAN_VAR(readindex);
		SCAN_VAR(dacbits);
		SCAN_VAR(palettedata);
		SCAN_VAR(regs);
		SCAN_VAR(local_paletteram);
	}

	if (nAction & ACB_WRITE) {
		tlc34076_recalc_palette();
	}

	return 0;
}

