// Based on sources by Manuel Abadia

#include "burnint.h"
#include "m68000_intf.h"
#include "bitswap.h"

static INT32 decrypt(INT32 const param1, INT32 const param2, INT32 const enc_prev_word, INT32 const dec_prev_word, INT32 const enc_word)
{
	INT32 const swap = (BIT(dec_prev_word, 8) << 1) | BIT(dec_prev_word, 7);
	INT32 const type = (BIT(dec_prev_word,12) << 1) | BIT(dec_prev_word, 2);
	INT32 res=0;
	INT32 k=0;

	switch (swap)
	{
		case 0:	res = BITSWAP16(enc_word,  1, 2, 0,14,12,15, 4, 8,13, 7, 3, 6,11, 5,10, 9); break;
		case 1:	res = BITSWAP16(enc_word, 14,10, 4,15, 1, 6,12,11, 8, 0, 9,13, 7, 3, 5, 2); break;
		case 2:	res = BITSWAP16(enc_word,  2,13,15, 1,12, 8,14, 4, 6, 0, 9, 5,10, 7, 3,11); break;
		case 3:	res = BITSWAP16(enc_word,  3, 8, 1,13,14, 4,15, 0,10, 2, 7,12, 6,11, 9, 5); break;
	}

	res ^= param2;

	switch (type)
	{
		case 0:
			k =	0x003a;
			break;

		case 1:
			k =	(BIT(dec_prev_word, 0) << 0) |
				(BIT(dec_prev_word, 1) << 1) |
				(BIT(dec_prev_word, 1) << 2) |
				(BIT(enc_prev_word, 3) << 3) |
				(BIT(enc_prev_word, 8) << 4) |
				(BIT(enc_prev_word,15) << 5);
			break;

		case 2:
			k =	(BIT(enc_prev_word, 5) << 0) |
				(BIT(dec_prev_word, 5) << 1) |
				(BIT(enc_prev_word, 7) << 2) |
				(BIT(enc_prev_word, 3) << 3) |
				(BIT(enc_prev_word,13) << 4) |
				(BIT(enc_prev_word,14) << 5);
			break;

		case 3:
			k =	(BIT(enc_prev_word, 0) << 0) |
				(BIT(enc_prev_word, 9) << 1) |
				(BIT(enc_prev_word, 6) << 2) |
				(BIT(dec_prev_word, 4) << 3) |
				(BIT(enc_prev_word, 2) << 4) |
				(BIT(dec_prev_word,11) << 5);
			break;
	}

	k ^= param1;

	res  = ((res & 0xffc0) | ((res + k) & 0x003f)) ^ param1;

	switch (type)
	{
		case 0:
			k =	(BIT(enc_word, 9) << 0) |
				(BIT(res,2)       << 1) |
				(BIT(enc_word, 5) << 2) |
				(BIT(res,5)       << 3) |
				(BIT(res,4)       << 4);
			break;

		case 1:
			k =	(BIT(dec_prev_word, 2) << 0) |
				(BIT(enc_prev_word, 4) << 1) |
				(BIT(dec_prev_word,14) << 2) |
				(BIT(res, 1)           << 3) |
				(BIT(dec_prev_word,12) << 4);
			break;

		case 2:
			k =	(BIT(enc_prev_word, 6) << 0) |
				(BIT(dec_prev_word, 6) << 1) |
				(BIT(dec_prev_word,15) << 2) |
				(BIT(res,0)            << 3) |
				(BIT(dec_prev_word, 7) << 4);
			break;

		case 3:
			k =	(BIT(dec_prev_word, 2) << 0) |
				(BIT(dec_prev_word, 9) << 1) |
				(BIT(enc_prev_word, 5) << 2) |
				(BIT(dec_prev_word, 1) << 3) |
				(BIT(enc_prev_word,10) << 4);

			break;
	}

	k ^= param1;

	res  = ((res & 0x003f) | ((res + (k <<  6)) & 0x07c0) |	((res + (k << 11)) & 0xf800)) ^ ((param1 << 6) | (param1 << 11));

	return BITSWAP16(res, 2,6,0,11,14,12,7,10,5,4,8,3,9,1,13,15);
}

UINT16 __fastcall gaelco_decrypt(INT32 offset, INT32 data, INT32 param1, INT32 param2)
{
	static INT32 lastpc, lastoffset, lastencword, lastdecword;

	INT32 thispc = SekGetPC(-1);

	if (lastpc == thispc && offset == lastoffset + 1)
	{
		lastpc = 0;
		data = decrypt(param1, param2, lastencword, lastdecword, data);
	}
	else
	{
		lastpc = thispc;
		lastoffset = offset;
		lastencword = data;

		data = decrypt(param1, param2, 0, 0, data);

		lastdecword = data;
	}

	return BURN_ENDIAN_SWAP_INT16(data);
}
