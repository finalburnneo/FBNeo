#include "burnint.h"
#include "watchdog.h"
#include "m68000_intf.h"

typedef struct
{
	UINT16 x1p, y1p, x1s, y1s;
	UINT16 x2p, y2p, x2s, y2s;
	INT16 x12, y12, x21, y21;
	UINT16 mult_a, mult_b;
} calc1_hit_t;

static calc1_hit_t hit;

static UINT16 __fastcall kaneko_hit_calc_read_word(UINT32 address)
{
	address &= 0x1e;
	switch (address)
	{
		case 0x00:
			BurnWatchdogRead();
			return 0;

		case 0x04:
		{
			UINT16 data = 0;

			if      (hit.x1p >  hit.x2p)	data |= 0x0200; // X Absolute Collision
			else if (hit.x1p == hit.x2p)	data |= 0x0400;
			else if (hit.x1p <  hit.x2p)	data |= 0x0800;

			if      (hit.y1p >  hit.y2p)	data |= 0x2000; // Y Absolute Collision
			else if (hit.y1p == hit.y2p)	data |= 0x4000;
			else if (hit.y1p <  hit.y2p)	data |= 0x8000;

			hit.x12 = (hit.x1p) - (hit.x2p + hit.x2s); // XY Overlap Collision
			hit.y12 = (hit.y1p) - (hit.y2p + hit.y2s);
			hit.x21 = (hit.x1p + hit.x1s) - (hit.x2p);
			hit.y21 = (hit.y1p + hit.y1s) - (hit.y2p);

			if ((hit.x12 < 0) && (hit.y12 < 0) && (hit.x21 >= 0) && (hit.y21 >= 0))
				data |= 0x0001;

			return data;
		}

		case 0x10:
			return (((UINT32)hit.mult_a * (UINT32)hit.mult_b) >> 16);

		case 0x12:
			return (((UINT32)hit.mult_a * (UINT32)hit.mult_b) & 0xffff);

		case 0x14:
			return BurnRandom();
	}

	return 0;
}

static UINT8 __fastcall kaneko_hit_calc_read_byte(UINT32 address)
{
	return kaneko_hit_calc_read_word(address) >> ((~address & 1) << 3);
}

static inline UINT16 read_regs(INT32 address)
{
	address &= 0x1e;

	switch (address)
	{
		case 0x00: return hit.x1p;
		case 0x02: return hit.x1s;
		case 0x04: return hit.y1p;
		case 0x06: return hit.y1s;
		case 0x08: return hit.x2p;
		case 0x0a: return hit.x2s;
		case 0x0c: return hit.y2p;
		case 0x0e: return hit.y2s;
		case 0x10: return hit.mult_a;
		case 0x12: return hit.mult_b;
	}

	return 0;
}

static void __fastcall kaneko_hit_calc_write_word(UINT32 address, UINT16 data)
{
	address &= 0x1e;

	switch (address)
	{
		case 0x00: hit.x1p    = data; break;
		case 0x02: hit.x1s    = data; break;
		case 0x04: hit.y1p    = data; break;
		case 0x06: hit.y1s    = data; break;
		case 0x08: hit.x2p    = data; break;
		case 0x0a: hit.x2s    = data; break;
		case 0x0c: hit.y2p    = data; break;
		case 0x0e: hit.y2s    = data; break;
		case 0x10: hit.mult_a = data; break;
		case 0x12: hit.mult_b = data; break;
	}
}

static void __fastcall kaneko_hit_calc_write_byte(UINT32 address, UINT8 data)
{
	UINT16 word = read_regs(address);

	word &= 0xff00 >> ((~address & 1) * 8);
	word |= data << ((~address & 1) * 8);

	kaneko_hit_calc_write_word(address & 0x1e, word);
}

void kaneko_hit_calc_reset()
{
	memset (&hit, 0, sizeof(hit));
}

void kaneko_hit_calc_init(INT32 nMapNumber, UINT32 address)
{
	SekMapHandler(nMapNumber,	address, address + 0x3ff, MAP_RAM);
	SekSetWriteWordHandler(nMapNumber,	kaneko_hit_calc_write_word);
	SekSetWriteByteHandler(nMapNumber,	kaneko_hit_calc_write_byte);
	SekSetReadWordHandler(nMapNumber,	kaneko_hit_calc_read_word);
	SekSetReadByteHandler(nMapNumber,	kaneko_hit_calc_read_byte);
}

INT32 kaneko_hit_calc_scan(INT32 nAction)
{
	SCAN_VAR(hit);
	BurnRandomScan(nAction);
	BurnWatchdogScan(nAction);

	return 0;
}
