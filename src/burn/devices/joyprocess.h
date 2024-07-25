#ifndef JOYPROCESS
#define JOYPROCESS

// ---[ ProcessJoystick() Flags (grep ProcessJoystick in drv/pre90s for examples)
#define INPUT_4WAY              0x02  // convert 8-way inputs to 4-way
#define INPUT_4WAY_ALT          0x22  // alternate method for 4-way processing
#define INPUT_CLEAROPPOSITES    0x04  // disallow up+down or left+right
#define INPUT_MAKEACTIVELOW     0x08  // input is active-high, make active-low after processing
#define INPUT_ISACTIVELOW       0x10  // input is active-low to begin with

// --- [ INPUT_4WAY / INPUT_4WAY_ALT methodology
// INPUT_4WAY: cancels previous frame's direction bit when diagonal is pressed
// INPUT_4WAY_ALT: holds previous non-diagonal direction until diagonal is released

void ProcessJoystick(UINT8 *input, INT8 playernum, INT8 up_bit, INT8 down_bit, INT8 left_bit, INT8 right_bit, UINT8 flags);
void CompileInput(UINT8 **input, void *output, INT32 num, INT32 bits, UINT32 *init);

// ---[ ProcessAnalog()
// Mostly used for self-centering Wheel, brake, accelerator.  Linear (0-ff),
// non-linear (f.ex centered @ 0x80)  grep ProcessAnalog in burn/drv/konami for
// examples :)

// ---[ ProcessAnalog() flags
// by default, it centers at the midpoint between scalemin and scalemax.
// use INPUT_LINEAR to change to linear-mode.
#define INPUT_DEADZONE          0x01
#define INPUT_LINEAR            0x02

// for analog pedals, allows a mapped digital button to work
#define INPUT_MIGHTBEDIGITAL    0x04

// Need something else / looking for:
// analog dial, paddle & trackball, use BurnTrackball device (burn_gun.h)
// lightgun, use BurnGun device (also burn_gun.h)

UINT8 ProcessAnalog(INT16 anaval, INT32 reversed, INT32 flags, UINT8 scalemin, UINT8 scalemax);
UINT8 ProcessAnalog(INT16 anaval, INT32 reversed, INT32 flags, UINT8 scalemin, UINT8 scalemax, UINT8 centerval);

INT32 AnalogDeadZone(INT32 anaval);

UINT32 scalerange(UINT32 x, UINT32 in_min, UINT32 in_max, UINT32 out_min, UINT32 out_max);

// ButtonToggle, an easy-to-use state-scannable togglebutton
struct ButtonToggle {
	INT32 state;
	INT32 last_state;

	ButtonToggle() {
		state = 0;
		last_state = 0;
	}

	INT32 Toggle(UINT8 &input) {
		INT32 toggled = 0;
		if (!last_state && input && !bBurnRunAheadFrame) {
			state ^= 1;
			toggled = 1;
		}
		last_state = input;
		input = state;

		return (toggled);
	}

	void Scan() {
		SCAN_VAR(state);
		SCAN_VAR(last_state);
	}
};

// E-Z HoldCoin logic (see pgm_run.cpp, d_discoboy.cpp)
template <int N, typename T = UINT8>
struct HoldCoin {
	T prev[N];
	T counter[N];

	void reset() {
		memset(&prev, 0, sizeof(prev));
		memset(&counter, 0, sizeof(counter));
	}

	void scan() {
		SCAN_VAR(prev);
		SCAN_VAR(counter);
	}

	void check(UINT8 num, T &inp, T bit, UINT8 hold_count) {
		if ((prev[num] & bit) != (inp & bit) && (inp & bit) && !counter[num]) {
			counter[num] = hold_count + 1;
		}
		prev[num] = (inp & bit);
		if (counter[num]) {
			counter[num]--;
			inp |= bit;
		}
		if (!counter[num]) {
			inp &= ~bit;
		}
	}

	void checklow(UINT8 num, T &inp, T bit, UINT8 hold_count) {
		if ((prev[num] & bit) != (inp & bit) && (~inp & bit) && !counter[num]) {
			counter[num] = hold_count + 1;
		}
		prev[num] = (inp & bit);
		if (counter[num]) {
			counter[num]--;
			inp &= ~bit;
		}
		if (!counter[num]) {
			inp |= bit;
		}
	}
};

template <int N, typename T>
struct ClearOpposite {
	T prev[N<<2];

	void reset() {
		memset(&prev, 0, sizeof(prev));
	}

	void scan() {
		SCAN_VAR(prev);
	}

	void checkval(UINT8 n, T &inp, T val_a, T val_b) {
		// When opposites become pressed simultaneously, and a 3rd direction isn't pressed
		// remove the previously stored direction if it exists, cancel both otherwise
		if ((inp & val_a) == val_a)
			inp &= (prev[n] && (inp & val_b) == 0 ? (inp ^ prev[n]) : ~val_a);
		// Store direction anytime it's pressed without its opposite
		else if (inp & val_a)
			prev[n] = inp & val_a;
	}

	void check(UINT8 num, T &inp, T val1, T val2) {
		checkval((num<<1)  , inp, val1, val2);
		checkval((num<<1)+1, inp, val2, val1);
	}
};

#endif
