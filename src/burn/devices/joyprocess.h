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

// joyprocess.cpp
extern INT32 nSocd[6];

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

// Take over and handle logic in the opposite input direction in advance when necessary
// When SOCD is disabled, control is transferred to the default frame control
template <int N, typename T>
struct ClearOpposite {
	T prev[N], prev_ud[N], prev_lr[N];

	void reset() {
		memset(&prev,    0, sizeof(prev));
		memset(&prev_ud, 0, sizeof(prev_ud));
		memset(&prev_lr, 0, sizeof(prev_lr));
	}

	void scan() {
		SCAN_VAR(prev);
		SCAN_VAR(prev_ud);
		SCAN_VAR(prev_lr);
	}

	// Insert the positive direction into the two fast-switching oblique directions
	void interp(UINT8 n, T& inp, const T val_ud, const T val_lr) {
		if (((inp | prev[n]) & val_ud) == val_ud) inp &= ~val_ud;
		if (((inp | prev[n]) & val_lr) == val_lr) inp &= ~val_lr;

		prev[n] = inp;
	}

	// Oppose simultaneous input from left and right or up and down
	void oppoxy(UINT8 n, T& inp, const T val_ud, const T val_lr) {
		if ((inp & val_ud) == val_ud) inp &= ~val_ud;
		if ((inp & val_lr) == val_lr) inp &= ~val_lr;
	}

	// SOCD - Simultaneous Neutral
	void neu(UINT8 n, T& inp, const T val_u, const T val_d, const T val_l, const T val_r) {
		const T val_ud = val_u | val_d, val_lr = val_l | val_r;
		oppoxy(n, inp, val_ud, val_lr);
		interp(n, inp, val_ud, val_lr);
	}

	// SOCD - Last Input Priority (4 Way)
	void lif(UINT8 n, T& inp, const T val_u, const T val_d, const T val_l, const T val_r) {
		const T val_ud = val_u | val_d, val_lr = val_l | val_r;
		const T inp_lr = inp & val_lr;
		if (inp_lr == val_lr) inp &= ~prev_lr[n];
		else if (inp_lr) prev_lr[n] = inp_lr;

		const T inp_ud = inp & val_ud;
		if (inp_ud == val_ud) inp &= ~prev_ud[n];
		else if (inp_ud) prev_ud[n] = inp_ud;

		neu(n, inp, val_u, val_d, val_l, val_r);
	}

	// SOCD - Last Input Priority (8 Way)
	void lie(UINT8 n, T& inp, const T val_u, const T val_d, const T val_l, const T val_r) {
		const T prev_state = prev[n];
		lif(n, inp, val_u, val_d, val_l, val_r);

		const T val_ud = (val_u | val_d), val_lr = (val_l | val_r);
		const T inp_e  = (inp & (val_ud | val_lr)), prev_e = (prev_state & (val_ud | val_lr));
		if (((inp_e == (val_d | val_l)) && (prev_e == (val_d | val_r))) ||
			((inp_e == (val_d | val_r)) && (prev_e == (val_d | val_l))) ||
			((inp_e == (val_u | val_l)) && (prev_e == (val_u | val_r))) ||
			((inp_e == (val_u | val_r)) && (prev_e == (val_u | val_l)))) {
			inp &= ~val_lr;
		}
		if (((inp_e == (val_d | val_l)) && (prev_e == (val_u | val_l))) ||
			((inp_e == (val_d | val_r)) && (prev_e == (val_u | val_r))) ||
			((inp_e == (val_u | val_l)) && (prev_e == (val_d | val_l))) ||
			((inp_e == (val_u | val_r)) && (prev_e == (val_d | val_r)))) {
			inp &= ~val_ud;
		}
		prev[n] = inp;
	}

	// SOCD - First Input Priority
	void fip(UINT8 n, T& inp, const T val_u, const T val_d, const T val_l, const T val_r) {
		const T val_ud = (val_u | val_d), val_lr = (val_l | val_r);
		const T inp_lr = inp & val_lr;
		if (inp_lr == val_lr) inp &= (~val_lr) | prev_lr[n];
		else if (inp_lr) prev_lr[n] = inp_lr;

		const T inp_ud = inp & val_ud;
		if (inp_ud == val_ud) inp &= (~val_ud) | prev_ud[n];
		else if (inp_ud) prev_ud[n] = inp_ud;

		neu(n, inp, val_u, val_d, val_l, val_r);
	}

	// SOCD - Up Priority (Up-override Down)
	void uod(UINT8 n, T& inp, const T val_u, const T val_d, const T val_l, const T val_r) {
		const T val_ud = (val_u | val_d), val_lr = (val_l | val_r), inp_u = inp & val_u;
		oppoxy(n, inp, val_ud, val_lr);
		if (inp_u) inp |= inp_u;

		neu(n, inp, val_u, val_d, val_l, val_r);
	}

	// SOCD - Down Priority (Left/Right Last Input Priority)​
	void dlr(UINT8 n, T& inp, const T val_u, const T val_d, const T val_l, const T val_r) {
		const T val_ud = (val_u | val_d), val_lr = (val_l | val_r), inp_d = (inp & val_d);
		const T inp_lr = inp & val_lr;
		if (inp_lr == val_lr) inp &= ~prev_lr[n];
		else if (inp_lr) prev_lr[n] = inp_lr;

		const T inp_ud = inp & val_ud;
		if (inp_ud == val_ud) inp &= (~val_ud) | inp_d;

		neu(n, inp, val_u, val_d, val_l, val_r);
	}

	void check(UINT8 num, T& inp, const T val_u, const T val_d, const T val_l, const T val_r, INT32 socd = 2) {
#define DISPATCH_SOCD(x) x(num, inp, val_u, val_d, val_l, val_r)

		switch (socd) {
			case 1: DISPATCH_SOCD(neu); break;
			case 2: DISPATCH_SOCD(lif); break;
			case 3: DISPATCH_SOCD(lie); break;
			case 4: DISPATCH_SOCD(fip); break;
			case 5: DISPATCH_SOCD(uod); break;
			case 6: DISPATCH_SOCD(dlr); break;
			default:                    break;	// Disable SOCD
		}

#undef DISPATCH_SOCD
	}
};

#endif
