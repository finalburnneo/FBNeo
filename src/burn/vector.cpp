
#include "tiles_generic.h"
#include "math.h"

#define TABLE_SIZE  0x10000 // excessive?

struct vector_line {
	INT32 x;
	INT32 y;
	INT32 color;
	UINT8 intensity;
};

static struct vector_line *vector_table;
struct vector_line *vector_ptr; // pointer
static INT32 vector_cnt;
static UINT32 *pBitmap = NULL;
static UINT32 *pPalette = NULL;

void vector_add_point(INT32 x, INT32 y, INT32 color, INT32 intensity)
{
	vector_ptr->x = x >> 16;
	vector_ptr->y = y >> 16;
	vector_ptr->color = color;
	vector_ptr->intensity = intensity;

	vector_cnt++;
	if (vector_cnt > (TABLE_SIZE - 2)) return;
	vector_ptr++;
	vector_ptr->color = -1; // mark it as the last one to save some cycles later...
}

static void lineSimple(INT32 x0, INT32 y0, INT32 x1, INT32 y1, INT32 color, INT32 intensity)
{
	color = color * 256 + intensity;
	UINT32 p = pPalette[color];
	if (p == 0) return; // safe to assume we can't draw black??

	// http://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm

	INT32 dx = abs(x1 - x0);
	INT32 dy = abs(y1 - y0);
	INT32 sx = x0 < x1 ? 1 : -1;
	INT32 sy = y0 < y1 ? 1 : -1;
	INT32 err = (dx>dy ? dx : -dy)/2, e2;

	while (1)
	{
		if (x0 >= 0 && x0 < nScreenWidth && y0 >= 0 && y0 < nScreenHeight) {
			INT32 coords = y0 * nScreenWidth + x0;
	
			UINT32 d = pBitmap[coords];

			if (d) { // is this actually how it works?
				INT32 r = ((d >> 16) & 0xff) + ((p >> 16) & 0xff);
				INT32 g = ((d >>  8) & 0xff) + ((p >>  8) & 0xff);
				INT32 b = (d & 0xff) + (p & 0xff);
				if (b > 0xff) b = 0xff; // clamp
				if (r > 0xff) r = 0xff; // clamp
				if (g > 0xff) g = 0xff; // clamp
	
				pBitmap[coords] = (r << 16) | (g << 8) | b;
			}
			else
			{
				pBitmap[y0 * nScreenWidth + x0] = p;
			}
		}

		if (x0 == x1 && y0 == y1) break;

		e2 = err;

		if (e2 >-dx) { err -= dy; x0 += sx; }
		if (e2 < dy) { err += dx; y0 += sy; }
	}
}

void draw_vector(UINT32 *palette)
{
	struct vector_line *ptr = &vector_table[0];

	INT32 prev_x = 0, prev_y = 0;

	memset (pBitmap, 0, nScreenWidth * nScreenHeight * sizeof(INT32));
	pPalette = palette;

	for (INT32 i = 0; i < vector_cnt && i < TABLE_SIZE; i++, ptr++)
	{
		if (ptr->color == -1) break;

		INT32 curr_y = ptr->y;
		INT32 curr_x = ptr->x;

		if (ptr->intensity != 0) { // intensity 0 means turn off the beam...
			lineSimple(curr_x, curr_y, prev_x, prev_y, ptr->color, ptr->intensity);
		}

		prev_x = curr_x;
		prev_y = curr_y;
	}

	// copy to the screen, only draw pixels that aren't black
	// should be safe for any bit depth with putpix
	{
		memset (pBurnDraw, 0, nScreenWidth * nScreenHeight * nBurnBpp);

		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++)
		{
			UINT32 p = pBitmap[i];
			
			if (p) {
				PutPix(pBurnDraw + i * nBurnBpp, BurnHighCol((p >> 16) & 0xff, (p >> 8) & 0xff, p & 0xff, 0));
			}
		}
	}
}

void vector_reset()
{
	vector_cnt = 0;
	vector_ptr = &vector_table[0];
	vector_ptr->color = -1;
}

void vector_init()
{
	GenericTilesInit();
	
	pBitmap = (UINT32*)BurnMalloc(nScreenWidth * nScreenHeight * sizeof(INT32));

	vector_table = (struct vector_line*)BurnMalloc(TABLE_SIZE * sizeof(vector_line));

	memset (vector_table, 0, TABLE_SIZE * sizeof(vector_line));

	vector_reset();
}

void vector_exit()
{
	GenericTilesExit();
	
	if (pBitmap) {
		BurnFree (pBitmap);
	}
	pPalette = NULL;

	BurnFree (vector_table);
	vector_ptr = NULL;
}

INT32 vector_scan(INT32 nAction)
{
	struct BurnArea ba;

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data   = (UINT8*)vector_table;
		ba.nLen   = TABLE_SIZE * sizeof(vector_line);
		ba.szName = "Vector Table";
		BurnAcb(&ba);

		SCAN_VAR(vector_cnt);
	}

	if (nAction & ACB_WRITE) {
		vector_ptr = &vector_table[vector_cnt];
	}

	return 0;
}
