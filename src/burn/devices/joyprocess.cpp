#include "burnint.h"
#include "joyprocess.h"

#define INPUT_4WAY              2
#define INPUT_CLEAROPPOSITES    4
#define INPUT_MAKEACTIVELOW     8

void ProcessJoystick(UINT8 *input, INT8 playernum, INT8 up_bit, INT8 down_bit, INT8 left_bit, INT8 right_bit, UINT8 flags)
{
	// Fixed Apr. 26, 2016: Limitation: this only works on the first 4 bits - active high to start with
	// Note: only works on the first 4 or last 4 bits for udlr - active high to start with
	// grep ProcessJoystick in drv/pre90s for examples

	static INT32 fourway[4]      = { 0, 0, 0, 0 }; // 4-way buffer
	static UINT8 DrvInputPrev[4] = { 0, 0, 0, 0 }; // 4-way buffer

	UINT8 mask1 = (up_bit > 3) ? 0xf0 : 0xf;
	UINT8 mask2 = (up_bit > 3) ? 0xf : 0xf0;

	UINT8 ud = (1 << up_bit) | (1 << down_bit);
	UINT8 rl = (1 << right_bit) | (1 << left_bit);

	if (flags & INPUT_4WAY) {
		playernum &= 3; // just incase.
		if(*input != DrvInputPrev[playernum]) {
			fourway[playernum] = *input & mask1; //0xf;

			if((fourway[playernum] & rl) && (fourway[playernum] & ud))
				fourway[playernum] ^= (fourway[playernum] & (DrvInputPrev[playernum] & 0xf));

			if((fourway[playernum] & rl) && (fourway[playernum] & ud)) // if it starts out diagonally, pick a direction
				fourway[playernum] &= (rand()&1) ? rl : ud;
		}

		DrvInputPrev[playernum] = *input;

		*input = fourway[playernum] | (DrvInputPrev[playernum] & mask2);//0xf0); // preserve the unprocessed/other bits
	}

	if (flags & INPUT_CLEAROPPOSITES) {
		if ((*input & rl) == rl) {
			*input &= ~rl;
		}
		if ((*input & ud) == ud) {
			*input &= ~ud;
		}
	}

	if (flags & INPUT_MAKEACTIVELOW) {
		*input = 0xff - *input;
	}
}

void CompileInput(UINT8 **input, void *output, INT32 num, INT32 bits, UINT32 *init)
{
	for (INT32 j = 0; j < num; j++) {
		if (bits > 16) ((UINT32*)output)[j] = init[j];
		if (bits > 8 && bits < 17) ((UINT16*)output)[j] = init[j];
		if (bits < 9) ((UINT8*)output)[j] = init[j];

		for (INT32 i = 0; i < bits; i++) {
			if (bits > 16) ((UINT32*)output)[j] ^= (input[j][i] & 1) << i;
			if (bits > 8 && bits < 17) ((UINT16*)output)[j] ^= (input[j][i] & 1) << i;
			if (bits < 9) ((UINT8*)output)[j] ^= (input[j][i] & 1) << i;
		}
	}
}
