
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

	// http://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm

	INT32 dx = abs(x1 - x0);
	INT32 dy = abs(y1 - y0);
	INT32 sx = x0 < x1 ? 1 : -1;
	INT32 sy = y0 < y1 ? 1 : -1;
	INT32 err = (dx>dy ? dx : -dy)/2, e2;

	while (1)
	{
		if (x0 >= 0 && x0 < nScreenWidth && y0 >= 0 && y0 < nScreenHeight) {
			pTransDraw[y0 * nScreenWidth + x0] = color;
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

	BurnTransferClear();

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

	BurnTransferCopy(palette);
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

	vector_table = (struct vector_line*)malloc(TABLE_SIZE * sizeof(vector_line));

	memset (vector_table, 0, TABLE_SIZE * sizeof(vector_line));

	vector_reset();
}

void vector_exit()
{
	GenericTilesExit();

	free (vector_table);
	vector_table = NULL;
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
