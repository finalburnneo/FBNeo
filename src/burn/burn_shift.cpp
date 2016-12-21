// FBAlpha Toggable Shifter and Gear Display
//
// For how-to, search BurnShift in konami/d_chqflag.cpp :)
//

#include "burnint.h"
#include "burn_shift.h"

INT32 BurnShiftEnabled = 1; // enable/disable rendering
INT32 bBurnShiftStatus; // current status of shifter

static INT32 prev_shift;

static INT32 shift_alpha_level;
static INT32 shift_alpha_level2;
static INT32 shift_alpha_level_grey;
static INT32 shift_alpha_level2_grey;
static INT32 shift_color;
static INT32 shift_size;
static INT32 shift_position0;
static INT32 shift_position;
static INT32 shift_xpos;
static INT32 shift_ypos;
static INT32 shift_xadv;
static INT32 shift_yadv;

static INT32 nScreenWidth, nScreenHeight;
static INT32 screen_flipped;
static INT32 screen_vertical;
static INT32 flipscreen = -1;
static INT32 colortab[10] = { 0, 0, 0x42f4f4, 0xffffff, 0x0000ff, 0x9200f4, 0xf4f400, 0xf45900, 0x00d8f4, 0 };
static INT32 lhtimer = 0; // color-cycle effect counter

#define a 0,
#define b 1,
#define c 2,
#define d 3,
UINT8 BurnGearRender[8*8];
#if 0
// small font
UINT8 BurnGearL[8*8] = {
	  a a a a a a a a
	  a a a a a a a a
	  a a a b c a a a
	  a a a b c a a a
	  a a a b c c c a
	  a a a b b b c a
	  a a a c c c c a
	  a a a a a a a a };

UINT8 BurnGearH[8*8] = {
	  a a a a a a a a
	  a a a a a a a a
	  a a a b c b c a
	  a a a b b b c a
	  a a a b c b c a
	  a a a c c c c a
	  a a a a a a a a
	  a a a a a a a a };
#endif
#if 0
// blocky font (Gab75)
UINT8 BurnGearL[8*8] = {
	  b b d a a d a a
	  b b d a a d a a
	  d d d d d d d d
	  b b d a a d a a
	  b b d a a d a a
	  d d d d d d d d
	  b b d b b d b b
	  b b d b b d b b };

UINT8 BurnGearH[8*8] = {
	  b b d a a d b b
	  b b d a a d b b
	  d d d d d d d d
	  b b d b b d b b
	  b b d b b d b b
	  d d d d d d d d
	  b b d a a d b b
	  b b d a a d b b };
#endif
#if 1
// Shadow font (Gab75)
UINT8 BurnGearL[8*8] = {
	  a a a a a a a a
	  a a b b a a a a
	  a a b b c a a a
	  a a b b c a a a
	  a a b b c a a a
	  a a b b b b a a
	  a a a c c c c a
	  a a a a a a a a };

UINT8 BurnGearH[8*8] = {
	  a a a a a a a a
	  a b b a a b b a
	  a b b c a b b c
	  a b b b b b b c
	  a b b c c b b c
	  a b b c a b b c
	  a a c c a a c c
	  a a a a a a a a };
#endif
#undef b
#undef a
#undef c
#undef d

static UINT32 alpha_blend32(UINT32 d, UINT32 col)
{
	if (col == 3) col = 3 /*0x1f1f1f*/; else
	if (col == 2) col = 0; else
		if (lhtimer == 0) col = colortab[1]; else
		col = colortab[lhtimer/2];


	if (col == 3) { // handle grey with a lighter transparency
		col = 0x1f1f1f;
		return (((((col & 0xff00ff) * shift_alpha_level_grey) + ((d & 0xff00ff) * shift_alpha_level2_grey)) & 0xff00ff00) |
				((((col & 0x00ff00) * shift_alpha_level_grey) + ((d & 0x00ff00) * shift_alpha_level2_grey)) & 0x00ff0000)) >> 8;
	}

	return (((((col & 0xff00ff) * shift_alpha_level) + ((d & 0xff00ff) * shift_alpha_level2)) & 0xff00ff00) |
		((((col & 0x00ff00) * shift_alpha_level) + ((d & 0x00ff00) * shift_alpha_level2)) & 0x00ff0000)) >> 8;
}

static void set_shift_draw_position()
{
	shift_position = shift_position0;

	if (screen_flipped ^ flipscreen) {
		switch (shift_position & 3) {
			case SHIFT_POSITION_TOP_LEFT: shift_position = SHIFT_POSITION_BOTTOM_RIGHT; break;
			case SHIFT_POSITION_TOP_RIGHT: shift_position = SHIFT_POSITION_BOTTOM_LEFT; break;
			case SHIFT_POSITION_BOTTOM_LEFT: shift_position = SHIFT_POSITION_TOP_RIGHT; break;
			case SHIFT_POSITION_BOTTOM_RIGHT: shift_position = SHIFT_POSITION_TOP_LEFT; break;
		}
	}

	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		BurnDrvGetVisibleSize(&nScreenHeight, &nScreenWidth);

		screen_vertical = 1;

		shift_xadv = 0;
		shift_yadv = shift_size + 1;

		switch (shift_position & 3)
		{
			case SHIFT_POSITION_TOP_LEFT:
				shift_xpos = (nScreenWidth - 1) - shift_size;
				shift_ypos = 1;
			break;

			case SHIFT_POSITION_BOTTOM_RIGHT:
				shift_xpos = 1;
				shift_ypos = (nScreenHeight - 1) - (shift_yadv * 1);
			break;

			case SHIFT_POSITION_BOTTOM_LEFT:
				shift_xpos = 1;
				shift_ypos = 1;
			break;

			case SHIFT_POSITION_TOP_RIGHT:
			default:
				shift_xpos = (nScreenWidth  - 1) - shift_size;
				shift_ypos = (nScreenHeight - 1) - (shift_yadv * 1);
			break;
		}
	} else {
		BurnDrvGetVisibleSize(&nScreenWidth, &nScreenHeight);

		screen_vertical = 0;

		shift_xadv = shift_size + 1;
		shift_yadv = 0;

		switch (shift_position & 3)
		{
			case SHIFT_POSITION_BOTTOM_LEFT:
				shift_xpos = 1;
				shift_ypos = (nScreenHeight - 1) - shift_size;
//				shift_ypos;
			break;

			case SHIFT_POSITION_TOP_RIGHT:
				shift_xpos = (nScreenWidth - 1) - (shift_xadv * 1);
				shift_ypos = 1;
			break;

			case SHIFT_POSITION_TOP_LEFT:
				shift_xpos = 1;
				shift_ypos = 1;
			break;

			case SHIFT_POSITION_BOTTOM_RIGHT:
			default:
				shift_xpos = (nScreenWidth  - 1) - (shift_xadv * 1);
				shift_ypos = (nScreenHeight - 1) - shift_size;
			break;
		}
	}
}

void BurnShiftSetFlipscreen(INT32 flip)
{
#if defined FBA_DEBUG
	if (!Debug_BurnShiftInitted) bprintf(PRINT_ERROR, _T("BurnShiftSetFlipscreen called without init\n"));
#endif

	flip = flip ? 1 : 0;

	if (flipscreen != flip) {
		flipscreen = flip;
		set_shift_draw_position();
	}
}

void BurnShiftReset()
{
#if defined FBA_DEBUG
	if (!Debug_BurnShiftInitted) bprintf(PRINT_ERROR, _T("BurnShiftReset called without init\n"));
#endif

	prev_shift = 0;

	BurnShiftSetStatus(0);

	BurnShiftSetFlipscreen(0);

	lhtimer = 0;
}

void BurnShiftInit(INT32 position, INT32 color, INT32 transparency)
{
	Debug_BurnShiftInitted = 1;

	colortab[1] = shift_color = color;

	shift_size = 8;
	shift_position0 = position;

	shift_alpha_level = (255 * transparency) / 100;
	shift_alpha_level2 = 256 - shift_alpha_level;

	shift_alpha_level_grey = (255 * 20) / 100;
	shift_alpha_level2_grey = 256 - shift_alpha_level_grey;

	screen_flipped = (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) ? 1 : 0;
	screen_vertical = (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) ? 1 : 0;

	BurnShiftReset();
}

void BurnShiftInitDefault()
{
	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_GREEN, 80);
}

INT32 BurnShiftInputCheckToggle(UINT8 shiftinput)
{
	if (prev_shift != shiftinput && shiftinput) {
		bBurnShiftStatus = !bBurnShiftStatus;
		BurnShiftSetStatus(bBurnShiftStatus);
	}

	prev_shift = shiftinput;

	return bBurnShiftStatus;
}

void BurnShiftSetStatus(UINT32 status)
{
#if defined FBA_DEBUG
	if (!Debug_BurnShiftInitted) bprintf(PRINT_ERROR, _T("BurnShiftSetStatus called without init\n"));
#endif

	bBurnShiftStatus = status ? 1 : 0;

	UINT8 *source = (status) ? &BurnGearH[0] : &BurnGearL[0];

	for (UINT8 y = 0; y < 8; y++)
		for (UINT8 x = 0; x < 8; x++)
		{
			if ((screen_flipped ^ flipscreen)) {
				if (screen_vertical)
				{ // flipped + vertical
					BurnGearRender[(y * 8) + x] = source[(x * 8) + (7-y)];
				}
				else
				{ // flipped
					BurnGearRender[(y * 8) + x] = source[(y * 8) + (7-x)];
				}
			} else if (screen_vertical)
			{ // vertical
				BurnGearRender[(y * 8) + x] = source[(x * 8) + y];
			} else
			{ // normal
				BurnGearRender[(y * 8) + x] = source[(y * 8) + x];
			}
		}
	lhtimer = 19; // for color effects
}

void BurnShiftExit()
{
#if defined FBA_DEBUG
	if (!Debug_BurnShiftInitted) bprintf(PRINT_ERROR, _T("BurnShiftExit called without init\n"));
#endif

	shift_alpha_level = 0;
	shift_alpha_level2 = 0;
	shift_alpha_level_grey = 0;
	shift_alpha_level2_grey = 0;
	shift_color = 0;
	shift_size = 0;
	shift_position = 0;
	shift_position0 = 0;

	shift_xpos = 0;
	shift_ypos = 0;

	screen_flipped = 0;
	nScreenWidth = 0;
	nScreenHeight = 0;
	lhtimer = 0;

	flipscreen = -1;
	
	Debug_BurnShiftInitted = 0;
}

void BurnShiftRender()
{
#if defined FBA_DEBUG
	if (!Debug_BurnShiftInitted) bprintf(PRINT_ERROR, _T("BurnShiftRender called without init\n"));
#endif

	if (!BurnShiftEnabled) return;

	INT32 xpos = shift_xpos;
	INT32 ypos = shift_ypos;
	INT32 color = BurnHighCol((shift_color >> 16) & 0xff, (shift_color >> 8) & 0xff, (shift_color >> 0) & 0xff, 0);

	{
		if (xpos < 0 || xpos > (nScreenWidth - shift_size)) return;

		{
			for (INT32 y = 0; y < 8; y++)
			{
				UINT8 *ptr = pBurnDraw + (((ypos + y) * nScreenWidth) + xpos) * nBurnBpp;

				for (INT32 x = 0; x < 8; x++) {
					if (BurnGearRender[(y*8)+x])
					{
						if (nBurnBpp >= 4)
						{
							*((UINT32*)ptr) = alpha_blend32(*((UINT32*)ptr), BurnGearRender[(y*8)+x]);
						}
						else if (nBurnBpp == 3)
						{
							UINT32 t = alpha_blend32((ptr[2] << 16) | (ptr[1] << 8) | ptr[0], BurnGearRender[(y*8)+x]);

							ptr[2] = t >> 16;
							ptr[1] = t >> 8;
							ptr[0] = t >> 0;
						}
						else if (nBurnBpp == 2 && BurnGearRender[(y*8)+x] == 1) // alpha blend not supported for 16-bit
						{
							*((UINT16*)ptr) =  color;
						}
					}
					ptr += nBurnBpp;
				}
			}
		}

		xpos += shift_xadv;
		ypos += shift_yadv;
	}
	if (lhtimer > 0) lhtimer--;
}

INT32 BurnShiftScan(INT32 nAction)
{
#if defined FBA_DEBUG
	if (!Debug_BurnShiftInitted) bprintf(PRINT_ERROR, _T("BurnShiftScan called without init\n"));
#endif

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(bBurnShiftStatus);
	}

	if (nAction & ACB_WRITE) {
		BurnShiftSetStatus(bBurnShiftStatus);
		lhtimer = 0;
	}

	return 0;
}
