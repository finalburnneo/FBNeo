#include <cstddef>
#include "string.h"

#include "inp_keys.h"

struct Input {
	const char *name;
	const int code;
};

#define P2_JOY 0x4000
#define P3_JOY 0x4100
#define P4_JOY 0x4200

static const Input allInputs[] = {
	// Generic
	{ "P1 START", FBK_1, },
	{ "P2 START", FBK_2, },
	{ "P1 COIN", FBK_5, },
	{ "P2 COIN", FBK_6, },
	{ "TEST", FBK_F2, },
	{ "SERVICE", FBK_9, },
	{ "SELECT1", FBK_3, },
	{ "SELECT2", FBK_4, },
	{ "RESET", FBK_F3, },
	{ "QUIT", FBK_ESCAPE, },

	// Keyboard
	{ "P1 UP", FBK_UPARROW, },
	{ "P1 LEFT", FBK_LEFTARROW, },
	{ "P1 RIGHT", FBK_RIGHTARROW, },
	{ "P1 DOWN", FBK_DOWNARROW, },
	{ "P1 JAB", FBK_A, },
	{ "P1 STRONG", FBK_S, },
	{ "P1 FIERCE", FBK_D, },
	{ "P1 SHORT", FBK_Z, },
	{ "P1 FORWARD", FBK_X, },
	{ "P1 ROUNDHOUSE", FBK_C, },
	{ "P1 BUTTON 1", FBK_Z, },
	{ "P1 BUTTON 2", FBK_X, },
	{ "P1 BUTTON 3", FBK_C, },
	{ "P1 BUTTON 4", FBK_V, },

	// Joystick
	{ "P2 UP",  P2_JOY | 0x02, },
	{ "P2 LEFT", P2_JOY | 0x00, },
	{ "P2 RIGHT", P2_JOY | 0x01, },
	{ "P2 DOWN", P2_JOY | 0x03, },
	{ "P2 BUTTON 1", P2_JOY | 0x80, },
	{ "P2 BUTTON 2", P2_JOY | 0x81, },
	{ "P2 BUTTON 3", P2_JOY | 0x82, },
	{ "P2 BUTTON 4", P2_JOY | 0x83, },
	{ "P2 JAB", P2_JOY | 0x80, },
	{ "P2 STRONG", P2_JOY | 0x81, },
	{ "P2 FIERCE", P2_JOY | 0x82, },
	{ "P2 SHORT", P2_JOY | 0x83, },
	{ "P2 FORWARD", P2_JOY | 0x84, },
	{ "P2 ROUNDHOUSE", P2_JOY | 0x85, },

	{ "P3 UP",  P3_JOY | 0x02, },
	{ "P3 LEFT", P3_JOY | 0x00, },
	{ "P3 RIGHT", P3_JOY | 0x01, },
	{ "P3 DOWN", P3_JOY | 0x03, },
	{ "P3 BUTTON 1", P3_JOY | 0x80, },
	{ "P3 BUTTON 2", P3_JOY | 0x81, },
	{ "P3 BUTTON 3", P3_JOY | 0x82, },
	{ "P3 BUTTON 4", P3_JOY | 0x83, },
	{ "P3 JAB", P3_JOY | 0x80, },
	{ "P3 STRONG", P3_JOY | 0x81, },
	{ "P3 FIERCE", P3_JOY | 0x82, },
	{ "P3 SHORT", P3_JOY | 0x83, },
	{ "P3 FORWARD", P3_JOY | 0x84, },
	{ "P3 ROUNDHOUSE", P3_JOY | 0x85, },

	{ "P4 UP",  P4_JOY | 0x02, },
	{ "P4 LEFT", P4_JOY | 0x00, },
	{ "P4 RIGHT", P4_JOY | 0x01, },
	{ "P4 DOWN", P4_JOY | 0x03, },
	{ "P4 BUTTON 1", P4_JOY | 0x80, },
	{ "P4 BUTTON 2", P4_JOY | 0x81, },
	{ "P4 BUTTON 3", P4_JOY | 0x82, },
	{ "P4 BUTTON 4", P4_JOY | 0x83, },
	{ "P4 JAB", P4_JOY | 0x80, },
	{ "P4 STRONG", P4_JOY | 0x81, },
	{ "P4 FIERCE", P4_JOY | 0x82, },
	{ "P4 SHORT", P4_JOY | 0x83, },
	{ "P4 FORWARD", P4_JOY | 0x84, },
	{ "P4 ROUNDHOUSE", P4_JOY | 0x85, },

	{ NULL, 0, },
};

int InputFindCode(const char *keystring)
{
	for (const struct Input *input = allInputs; input->name != NULL; input++) {
		if (strcasecmp(input->name, keystring) == 0) {
			return input->code;
		}
	}

	return -1;
}
