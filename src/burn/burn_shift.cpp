// FBAlpha Shifter / Gear Display
//
// Todo:
//   1: Figure out how to convince iq_132 to make this work with 1 single font
//      per character with rotation tricks.
//   2: Add internal shift toggle function and remove from driver.

#include "burnint.h"
#include "burn_shift.h"

INT32 BurnShiftEnabled = 1;

static INT32 shift_status;

static INT32 shift_alpha_level;
static INT32 shift_alpha_level2;
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
static INT32 flipscreen = -1;

#define a 0,
#define b 1,
UINT8 BurnGearRender[8*8];

UINT8 BurnGearL[8*8] = {
	  a a a a a a a a
	  a b b a a a a a
	  a b b a a a a a
	  a b b a a a a a
	  a b b a a a a a
	  a b b b b b a a
	  a b b b b b a a
	  a a a a a a a a };

UINT8 BurnGearL_V[8*8] = {
	  a a a a a a a a
	  a b b b b b b a
	  a b b b b b b a
	  a b b a a a a a
	  a b b a a a a a
	  a b b a a a a a
	  a a a a a a a a
	  a a a a a a a a };

UINT8 BurnGearL_V2[8*8] = {
	  a a a a a a a a
	  a a a a a a a a
	  a a a a a b b a
	  a a a a a b b a
	  a a a a a b b a
	  a b b b b b b a
	  a b b b b b b a
	  a a a a a a a a };

UINT8 BurnGearH[8*8] = {
	  a a a a a a a a
	  a b b a a b b a
	  a b b a a b b a
	  a b b b b b b a
	  a b b b b b b a
	  a b b a a b b a
	  a b b a a b b a
	  a a a a a a a a };

UINT8 BurnGearH_V[8*8] = {
	  a a a a a a a a
	  a b b b b b b a
	  a b b b b b b a
	  a a a b b a a a
	  a a a b b a a a
	  a b b b b b b a
	  a b b b b b b a
	  a a a a a a a a };
#undef b
#undef a

static inline UINT32 alpha_blend32(UINT32 d)
{
	return (((((shift_color & 0xff00ff) * shift_alpha_level) + ((d & 0xff00ff) * shift_alpha_level2)) & 0xff00ff00) |
		((((shift_color & 0x00ff00) * shift_alpha_level) + ((d & 0x00ff00) * shift_alpha_level2)) & 0x00ff0000)) >> 8;
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

	BurnShiftSetStatus(0);

	BurnShiftSetFlipscreen(0);
}

void BurnShiftInit(INT32 position, INT32 color, INT32 transparency)
{
	Debug_BurnShiftInitted = 1;

	shift_color = color;
	shift_size = 8;
	shift_position0 = position;

	shift_alpha_level = (255 * transparency) / 100;
	shift_alpha_level2 = 256 - shift_alpha_level;

	screen_flipped = (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) ? 1 : 0;

	BurnShiftReset();
}

void BurnShiftSetStatus(UINT32 status)
{
#if defined FBA_DEBUG
	if (!Debug_BurnShiftInitted) bprintf(PRINT_ERROR, _T("BurnShiftSetStatus called without init\n"));
#endif

	shift_status = status ? 1 : 0;

	if (status) { // HIGH
		if (shift_position0 & SHIFT_POSITION_ROTATE_CCW) {
			//bprintf(0, _T("ro-H!"));
			memcpy(&BurnGearRender, &BurnGearH_V, sizeof(BurnGearRender));
		} else {
			memcpy(&BurnGearRender, &BurnGearH, sizeof(BurnGearRender));
		}
	} else { // LOW
		if (shift_position0 & SHIFT_POSITION_ROTATE_CCW) {
			//bprintf(0, _T("ro-L!"));
			memcpy(&BurnGearRender, &BurnGearL_V2, sizeof(BurnGearRender));
		} else {
			memcpy(&BurnGearRender, &BurnGearL, sizeof(BurnGearRender));
		}
	}
}

void BurnShiftExit()
{
#if defined FBA_DEBUG
	if (!Debug_BurnShiftInitted) bprintf(PRINT_ERROR, _T("BurnShiftExit called without init\n"));
#endif

	BurnShiftReset();

	shift_alpha_level = 0;
	shift_alpha_level2 = 0;
	shift_color = 0;
	shift_size = 0;
	shift_position = 0;
	shift_position0 = 0;

	shift_xpos = 0;
	shift_ypos = 0;

	screen_flipped = 0;
	nScreenWidth = 0;
	nScreenHeight = 0;

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
							*((UINT32*)ptr) = alpha_blend32(*((UINT32*)ptr));
						}
						else if (nBurnBpp == 3)
						{
							UINT32 t = alpha_blend32((ptr[2] << 16) | (ptr[1] << 8) | ptr[0]);

							ptr[2] = t >> 16;
							ptr[1] = t >> 8;
							ptr[0] = t >> 0;
						}
						else if (nBurnBpp == 2) // alpha blend not supported for 16-bit
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
}

INT32 BurnShiftScan(INT32 nAction, INT32 *pnMin)
{
#if defined FBA_DEBUG
	if (!Debug_BurnShiftInitted) bprintf(PRINT_ERROR, _T("BurnShiftScan called without init\n"));
#endif

	if (pnMin != NULL) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(shift_status);
	}

	return 0;
}
