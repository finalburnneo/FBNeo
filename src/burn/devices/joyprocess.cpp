#include "burnint.h"
#include "joyprocess.h"

// Digital Processing
void ProcessJoystick(UINT8 *input, INT8 playernum, INT8 up_bit, INT8 down_bit, INT8 left_bit, INT8 right_bit, UINT8 flags)
{ // limitations: 4 players max., processes 8-bit inputs only!
	static INT32 fourway[4]      = { 0, 0, 0, 0 }; // 4-way buffer
	static UINT8 DrvInputPrev[4] = { 0, 0, 0, 0 }; // 4-way buffer

	if (flags & INPUT_ISACTIVELOW) {
		*input ^= 0xff;
	}

	UINT8 ud = (1 << up_bit) | (1 << down_bit);
	UINT8 rl = (1 << right_bit) | (1 << left_bit);

	UINT8 udrlmask = ud | rl; // bitmask to process
	UINT8 othermask = udrlmask ^ 0xff; // bitmask to preserve (invert of udrlmask)

	if (flags & INPUT_4WAY) {
		playernum &= 3; // just incase.
		if(*input != DrvInputPrev[playernum]) {
			fourway[playernum] = *input & udrlmask;

			if((fourway[playernum] & rl) && (fourway[playernum] & ud))
				fourway[playernum] ^= (fourway[playernum] & (DrvInputPrev[playernum] & udrlmask));

			if((fourway[playernum] & rl) && (fourway[playernum] & ud))
				fourway[playernum] &= ud | ud; // diagonals aren't allowed w/INPUT_4WAY
		}

		DrvInputPrev[playernum] = *input;

		*input = fourway[playernum] | (DrvInputPrev[playernum] & othermask); // add back the unprocessed/other bits
	}

	if (flags & INPUT_CLEAROPPOSITES) {
		if ((*input & rl) == rl) {
			*input &= ~rl;
		}
		if ((*input & ud) == ud) {
			*input &= ~ud;
		}
	}

	if (flags & INPUT_MAKEACTIVELOW || flags & INPUT_ISACTIVELOW) {
		*input ^= 0xff;
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

// Analog Processing
INT16 AnalogDeadZone(INT16 anaval)
{
	INT32 negative = (anaval < 0);

	anaval = abs(anaval);

	// < 160 is usually "noise" with modern gamepad thumbsticks
	// (mouse movements are usually above 200)
	if (anaval < 160) {
		anaval = 0;
	} else {
		anaval -= 160;
	}

	return (negative) ? -anaval : anaval;
}

static UINT32 scalerange(UINT32 x, UINT32 in_min, UINT32 in_max, UINT32 out_min, UINT32 out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

UINT8 ProcessAnalog(INT16 anaval, INT32 reversed, INT32 flags, UINT8 scalemin, UINT8 scalemax)
{
    UINT8 linear_min = 0, linear_max = 0;

    if (flags & INPUT_MIGHTBEDIGITAL && (UINT16)anaval == 0xffff) {
		anaval = 0x3fc; // digital button mapped here & pressed.
	}

    if (flags & INPUT_LINEAR) {
        anaval = abs(anaval);
        linear_min = scalemin;
        linear_max = scalemax;
        scalemin = 0x00;
        scalemax = 0xff;
    }

	INT32 DeadZone = (flags & INPUT_DEADZONE) ? 10 : 0;
	INT16 Temp = (reversed) ? (0x7f - (anaval / 16)) : (0x7f + (anaval / 16));  // - for reversed, + for normal

	if (flags & INPUT_DEADZONE) { // deadzones
		if (flags & INPUT_LINEAR) {
			if (Temp < DeadZone) Temp = 0;
		} else {
			// 0x7f is center, 0x3f right, 0xbe left.  0x7f +-10 is noise.
			if (!(Temp < 0x7f-DeadZone || Temp > 0x7f+DeadZone)) {
				Temp = 0x7f; // we hit a dead-zone, return mid-range
			} else {
				// so we don't jump between 0x7f (center) and next value after deadzone
				if (Temp < 0x7f-DeadZone) Temp += DeadZone;
				else if (Temp > 0x7f+DeadZone) Temp -= DeadZone;
			}
		}
    }

	if (Temp < 0x3f + DeadZone) Temp = 0x3f + DeadZone; // clamping for happy scalerange()
	if (Temp > 0xbe - DeadZone) Temp = 0xbe - DeadZone;
	Temp = scalerange(Temp, 0x3f + DeadZone, 0xbe - DeadZone, scalemin, scalemax);

	if (flags & INPUT_LINEAR) {
		Temp -= 0x80;
		Temp = scalerange(Temp, 0, 0x7f, linear_min, linear_max);
	}

	return Temp;
}
