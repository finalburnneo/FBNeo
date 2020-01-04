#include "cps.h"


static INT32 bitswap1(INT32 src,INT32 key,INT32 select)
{
	if (select & (1 << ((key >> 0) & 7)))
		src = (src & 0xfc) | ((src & 0x01) << 1) | ((src & 0x02) >> 1);
	if (select & (1 << ((key >> 4) & 7)))
		src = (src & 0xf3) | ((src & 0x04) << 1) | ((src & 0x08) >> 1);
	if (select & (1 << ((key >> 8) & 7)))
		src = (src & 0xcf) | ((src & 0x10) << 1) | ((src & 0x20) >> 1);
	if (select & (1 << ((key >>12) & 7)))
		src = (src & 0x3f) | ((src & 0x40) << 1) | ((src & 0x80) >> 1);

	return src;
}

static INT32 bitswap2(INT32 src,INT32 key,INT32 select)
{
	if (select & (1 << ((key >>12) & 7)))
		src = (src & 0xfc) | ((src & 0x01) << 1) | ((src & 0x02) >> 1);
	if (select & (1 << ((key >> 8) & 7)))
		src = (src & 0xf3) | ((src & 0x04) << 1) | ((src & 0x08) >> 1);
	if (select & (1 << ((key >> 4) & 7)))
		src = (src & 0xcf) | ((src & 0x10) << 1) | ((src & 0x20) >> 1);
	if (select & (1 << ((key >> 0) & 7)))
		src = (src & 0x3f) | ((src & 0x40) << 1) | ((src & 0x80) >> 1);

	return src;
}

static INT32 bytedecode(INT32 src,INT32 swap_key1,INT32 swap_key2,INT32 xor_key,INT32 select)
{
	src = bitswap1(src,swap_key1 & 0xffff,select & 0xff);
	src = ((src & 0x7f) << 1) | ((src & 0x80) >> 7);
	src = bitswap2(src,swap_key1 >> 16,select & 0xff);
	src ^= xor_key;
	src = ((src & 0x7f) << 1) | ((src & 0x80) >> 7);
	src = bitswap2(src,swap_key2 & 0xffff,select >> 8);
	src = ((src & 0x7f) << 1) | ((src & 0x80) >> 7);
	src = bitswap1(src,swap_key2 >> 16,select >> 8);
	return src;
}

void kabuki_decode(UINT8 *src,UINT8 *dest_op,UINT8 *dest_data,
		INT32 base_addr,INT32 length,INT32 swap_key1,INT32 swap_key2,INT32 addr_key,INT32 xor_key)
{
	INT32 A;
	INT32 select;

	for (A = 0;A < length;A++)
	{
		/* decode opcodes */
		select = (A + base_addr) + addr_key;
		dest_op[A] = (UINT8)bytedecode(src[A],swap_key1,swap_key2,xor_key,select);

		/* decode data */
		select = ((A + base_addr) ^ 0x1fc0) + addr_key + 1;
		dest_data[A] = (UINT8)bytedecode(src[A],swap_key1,swap_key2,xor_key,select);
	}
}

static void cps1_decode(INT32 swap_key1,INT32 swap_key2,INT32 addr_key,INT32 xor_key)
{
	UINT8 *rom = CpsZRom;
	INT32 diff = nCpsZRomLen / 2;

	CpsZRom=rom+diff;
	kabuki_decode(rom,rom+diff,rom,0x0000,0x8000, swap_key1,swap_key2,addr_key,xor_key);
}

void wof_decode(void)      { cps1_decode(0x01234567,0x54163072,0x5151,0x51); }
void dino_decode(void)     { cps1_decode(0x76543210,0x24601357,0x4343,0x43); }
void punisher_decode(void) { cps1_decode(0x67452103,0x75316024,0x2222,0x22); }
void slammast_decode(void) { cps1_decode(0x54321076,0x65432107,0x3131,0x19); }
