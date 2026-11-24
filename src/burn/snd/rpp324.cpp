// Roland RPP-324 RDAC phrase player, as used on System Board Y2
// Port of MAME's roland_rpp324 device (Peter Wilhelmsen): the codec and the phrase directory were
// reconstructed independently, the register protocol comes from the games' BDSND SDK.
// TODO (as in MAME): gain/pan law, fade and status pulse time units, Late modes, filters, stereo effect

#include "burnint.h"
#include "rpp324.h"
#include <math.h>

#define RPP_RATE		44100
#define RPP_VOICES		32
#define RPP_SEQUENCES	8
#define RPP_QUEUE		8

// ---------------------------------------------------------------------------
// RDAC block decoder

struct RdacFormat
{
	UINT16 format;
	UINT8 length[256];
};

static const RdacFormat RdacFormats[] = {
	{ 0x0c1f, {
		2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 0, 2, 2, 2, 2, 4, 4, 4, 4,
		4, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2,
	} },
	{ 0x122f, {
		2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 6, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 2, 2, 2, 2, 2, 4, 0, 4, 4,
		4, 2, 2, 4, 4, 6, 6, 6, 4, 4, 4, 6, 6, 6, 6, 2,
	} },
	{ 0x131f, {
		2, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 6, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 2, 2, 2, 2, 2, 4, 4, 4, 4,
		4, 2, 4, 4, 4, 6, 6, 6, 4, 4, 4, 6, 6, 6, 6, 2,
	} },
	{ 0x132f, {
		2, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		6, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 4, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0,
		0, 4, 4, 4, 6, 6, 6, 6, 4, 4, 0, 6, 6, 6, 6, 2,
	} },
	{ 0x141f, {
		4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		6, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 4, 0, 6, 0, 6, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 2, 2, 2, 2, 2, 4, 4, 4, 4,
		4, 4, 4, 4, 6, 6, 6, 6, 4, 4, 6, 6, 6, 6, 6, 2,
	} },
	{ 0x1a2f, {
		2, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 6, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		6, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		6, 0, 4, 0, 4, 0, 4, 0, 4, 0, 6, 0, 6, 0, 8, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 2, 2, 2, 2, 2, 4, 4, 4, 4,
		4, 4, 4, 6, 6, 8, 8, 8, 4, 6, 6, 8, 8, 8, 8, 2,
	} },
	{ 0x1b1f, {
		4, 0, 0, 0, 4, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		6, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 6, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		6, 0, 4, 0, 4, 0, 4, 0, 6, 0, 6, 0, 6, 0, 8, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 2, 2, 2, 2, 2, 4, 4, 4, 4,
		4, 4, 6, 6, 6, 8, 8, 8, 6, 6, 6, 8, 8, 8, 8, 2,
	} },
	{ 0x1b2f, {
		4, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 4, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		6, 0, 4, 0, 4, 0, 6, 0, 6, 0, 6, 0, 8, 0, 8, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 2, 2, 2, 2, 4, 4, 4, 4, 4,
		6, 6, 6, 6, 8, 8, 8, 8, 6, 6, 8, 8, 8, 8, 8, 2,
	} },
	{ 0x1c1f, {
		6, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		6, 0, 4, 0, 6, 0, 6, 0, 6, 0, 6, 0, 8, 0, 8, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 2, 2, 4, 4, 4, 4, 4, 4, 6,
		6, 6, 6, 6, 8, 8, 8, 8, 6, 6, 8, 8, 8, 8, 8, 2,
	} },
	{ 0x222f, {
		4, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0, 8, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 4, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 0, 4, 0, 4, 0, 6, 0, 6, 0, 8, 0, 8, 0, 10, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 2, 2, 2, 2, 2, 4, 4, 4, 4,
		6, 6, 6, 8, 8, 10, 10, 10, 6, 8, 8, 10, 10, 10, 10, 2,
	} },
	{ 0x231f, {
		6, 0, 0, 0, 6, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		6, 0, 0, 0, 6, 0, 0, 0, 6, 0, 0, 0, 8, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 0, 6, 0, 6, 0, 6, 0, 8, 0, 8, 0, 8, 0, 10, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 2, 2, 4, 4, 4, 4, 4, 6, 6,
		6, 6, 8, 8, 8, 10, 10, 10, 8, 8, 8, 10, 10, 10, 10, 2,
	} },
	{ 0x232f, {
		6, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		10, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0, 6, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		6, 0, 0, 0, 6, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 0, 6, 0, 6, 0, 8, 0, 8, 0, 8, 0, 10, 0, 10, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 2, 4, 4, 4, 6, 4, 6, 6, 6,
		8, 8, 8, 8, 10, 10, 10, 10, 8, 8, 10, 10, 10, 10, 10, 2,
	} },
	{ 0x241f, {
		8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		10, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0, 6, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		6, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 0, 6, 0, 8, 0, 8, 0, 8, 0, 8, 0, 10, 0, 10, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 4, 4, 6, 6, 6, 6, 6, 6, 8,
		8, 8, 8, 8, 10, 10, 10, 10, 8, 8, 10, 10, 10, 10, 10, 2,
	} },
	{ 0x2a2f, {
		6, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 10, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		10, 0, 0, 0, 12, 0, 0, 0, 12, 0, 0, 0, 4, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		6, 0, 0, 0, 6, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		10, 0, 6, 0, 6, 0, 8, 0, 8, 0, 10, 0, 10, 0, 12, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 2, 2, 2, 4, 4, 4, 4, 6, 6,
		8, 8, 8, 10, 10, 12, 12, 12, 8, 10, 10, 12, 12, 12, 12, 2,
	} },
	{ 0x2b1f, {
		8, 0, 0, 0, 8, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		10, 0, 0, 0, 12, 0, 0, 0, 12, 0, 0, 0, 6, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 0, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 10, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		10, 0, 8, 0, 8, 0, 8, 0, 10, 0, 10, 0, 10, 0, 12, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 4, 4, 6, 6, 6, 6, 6, 8, 8,
		8, 8, 10, 10, 10, 12, 12, 12, 10, 10, 10, 12, 12, 12, 12, 2,
	} },
	{ 0x2b2f, {
		8, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		12, 0, 0, 0, 12, 0, 0, 0, 12, 0, 0, 0, 8, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 0, 0, 0, 8, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		10, 0, 8, 0, 8, 0, 10, 0, 10, 0, 10, 0, 12, 0, 12, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 4, 6, 6, 6, 8, 6, 8, 8, 8,
		10, 10, 10, 10, 12, 12, 12, 12, 10, 10, 12, 12, 12, 12, 12, 2,
	} },
	{ 0x2c1f, {
		10, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		12, 0, 0, 0, 12, 0, 0, 0, 12, 0, 0, 0, 8, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		10, 0, 8, 0, 10, 0, 10, 0, 10, 0, 10, 0, 12, 0, 12, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 2, 0, 6, 6, 8, 8, 8, 8, 8, 8, 10,
		10, 10, 10, 10, 12, 12, 12, 12, 10, 10, 12, 12, 12, 12, 12, 2,
	} },
};

#define RDAC_FORMATS	(INT32)(sizeof(RdacFormats) / sizeof(RdacFormats[0]))

static inline double RdacAverage(double x, double y) { return floor((x + y) / 2); }

// each byte lane holds eight samples: end/midpoint anchors, then progressively finer midpoint residuals
static INT32 RdacDecodeHalf(const UINT8 *data, INT32 bytes, INT32 lane, INT32 h, INT32 previous, INT16 *out)
{
	if (h >= 0xf8 && bytes < 2) return 0;

	const INT32 prefix = (h < 0xc0) ? 2 : (h < 0xe0) ? 3 : 4;
	INT32 anchor, middle, quarter, leaf, shift;

	if (h < 0x4c) {
		anchor = bytes + 1; middle = bytes + 1; quarter = bytes; leaf = bytes - 1;
		shift = ((h >> 4) & 12) + ((h >> 2) & 3) + 9 - bytes;
	} else if (h < 0xc0) {
		anchor = bytes + 5; middle = bytes + 1; quarter = bytes; leaf = bytes - 2;
		shift = ((h >> 4) & 12) + ((h >> 2) & 3) - bytes;
	} else if (h == 0xc0) {
		anchor = bytes + 4; middle = bytes + 1; quarter = bytes; leaf = bytes - 2;
		shift = 12 - bytes;
	} else if (h < 0xd0) {
		anchor = bytes + 3; middle = bytes + 2; quarter = bytes; leaf = bytes - 2;
		shift = ((h & 14) >> 1) + 7 - bytes;
	} else if (h == 0xe5 || h == 0xe7 || h == 0xe8 || h == 0xe9) {
		anchor = bytes + 3; middle = bytes - 1; quarter = bytes - 1; leaf = bytes - 1;
		shift = ((h == 0xe5) ? 1 : (h - 0xe5)) - bytes;
	} else if (h == 0xea || h == 0xeb) {
		anchor = bytes + 2; middle = bytes; quarter = bytes - 1; leaf = bytes - 1;
		shift = h - 0xea + 6 - bytes;
	} else if (h >= 0xec && h <= 0xf0) {
		anchor = bytes + 6; middle = bytes; quarter = bytes - 1; leaf = bytes - 2;
		shift = h - 0xee + 8 - bytes;
	} else if (h >= 0xf1 && h <= 0xfe) {
		anchor = bytes; middle = bytes; quarter = bytes; leaf = bytes - 1;
		shift = h - ((h >= 0xf8) ? 0xf8 : 0xf1) + 10 - bytes;
	} else {
		return 0;
	}

	if (bytes > 8 || anchor < 0 || middle < 0 || quarter < 0 || leaf < 0
		|| prefix + anchor + middle + 2 * quarter + 4 * leaf != 8 * bytes) {
		return 0;
	}

	UINT64 raw = 0;
	for (INT32 i = 0; i < bytes; i++) raw = (raw << 8) | data[2 * i + lane];

	const INT32 widths[8] = { anchor, middle, quarter, quarter, leaf, leaf, leaf, leaf };
	INT32 position = prefix;
	double field[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

	for (INT32 i = 0; i < 8; i++) {
		const INT32 width = widths[i];
		if (!width) continue;

		INT64 value = (raw >> (8 * bytes - position - width)) & (((UINT64)1 << width) - 1);
		if (value & ((INT64)1 << (width - 1))) value -= (INT64)1 << width;
		position += width;

		const INT32 scale = shift + ((h >= 0xf8 && i >= 4) ? 1 : 0);
		field[i] = ldexp((double)value, scale);
		if (scale > 0) {
			field[i] += ldexp(1.0, scale - 1);
			if (i == 0 || (i == 1 && (h < 0x4c || h == 0xce || h >= 0xf1))
				|| ((i == 2 || i == 3) && h >= 0xf1) || h >= 0xf8) {
				field[i] -= 1;
			}
		}
	}

	double sample[8];
	sample[7] = floor(field[0]);
	if (h == 0xce) {
		const INT32 value = (INT32)sample[7];
		sample[7] = ((value + 32768) % 65536 + 65536) % 65536 - 32768;
	}
	sample[3] = (h < 0x4c || h == 0xce || h >= 0xf1) ? floor(field[1]) : (RdacAverage(previous, sample[7]) + field[1]);
	sample[1] = (h >= 0xf1) ? floor(field[2]) : (RdacAverage(previous, sample[3]) + field[2]);
	sample[5] = (h >= 0xf1) ? floor(field[3]) : (RdacAverage(sample[3], sample[7]) + field[3]);
	sample[0] = (h >= 0xf8) ? field[4] : (RdacAverage(previous, sample[1]) + field[4]);
	sample[2] = (h >= 0xf8) ? field[5] : (RdacAverage(sample[1], sample[3]) + field[5]);
	sample[4] = (h >= 0xf8) ? field[6] : (RdacAverage(sample[3], sample[5]) + field[6]);
	sample[6] = (h >= 0xf8) ? field[7] : (RdacAverage(sample[5], sample[7]) + field[7]);

	for (INT32 i = 0; i < 8; i++) {
		double s = floor(sample[i]);
		out[i] = (INT16)((s < -32768.0) ? -32768 : (s > 32767.0) ? 32767 : (INT32)s);
	}

	return 1;
}

static inline UINT32 RdacHeader(const UINT8 *data)
{
	const UINT32 h = (data[1] & 0xf0) | (data[0] >> 4);
	return h & ((h < 0xc0) ? 0xcc : (h < 0xe0) ? 0xee : 0xff);
}

static inline UINT32 RdacRead32(const UINT8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((UINT32)p[3] << 24); }
static inline UINT32 RdacRead16(const UINT8 *p) { return p[0] | (p[1] << 8); }

struct RdacPhrase
{
	UINT32 start;
	UINT32 end;
	UINT32 limit;
	UINT8 table;
	UINT8 channels;
	UINT8 step;
};

static UINT32 RdacPhraseCount(const UINT8 *rom, UINT32 size)
{
	if (size < 4 || RdacRead16(rom) != 0x490c) return 0;

	const UINT32 end = RdacRead16(rom + 2) * 2;
	if (end < 4 || end > size || (end - 4) % 10) return 0;

	return ((end - 4) / 10 < 4096) ? (end - 4) / 10 : 4096;
}

static INT32 RdacDescribe(const UINT8 *rom, UINT32 size, UINT32 number, RdacPhrase &p)
{
	const UINT32 count = RdacPhraseCount(rom, size);
	if (number >= count) return 0;

	const UINT8 *entry = rom + 4 + number * 10;
	const UINT32 start = RdacRead32(entry);
	const UINT32 end = RdacRead32(entry + 4);
	const UINT32 format = RdacRead16(entry + 8);

	INT32 table = 0;
	while (table < RDAC_FORMATS && RdacFormats[table].format != (format & 0xff7f)) table++;
	if (table == RDAC_FORMATS) return 0;

	// nominal rates: 44.1 kHz * (tag + 1) / 8
	const UINT32 tag = start & 15;
	if (tag != 2 && tag != 3 && tag != 4 && tag != 7) return 0;

	p.start = (start >> 4) * 2;
	p.end = (end >> 4) * 2;
	p.limit = (number + 1 < count) ? ((RdacRead32(entry + 10) >> 4) * 2) : size;
	p.table = table;
	p.channels = (format & 0x80) ? 2 : 1;
	p.step = tag + 1;

	if ((end & 15) != 15 || p.start < RdacRead16(rom + 2) * 2U || p.start > p.end || p.limit > size || p.end + 2 > p.limit) return 0;

	const UINT32 length = RdacFormats[table].length[RdacHeader(rom + p.end)];
	return length >= 2 && p.end + length <= p.limit;
}

// at most sixteen native frames are buffered; FF runs are counted per channel
struct RdacDecoder
{
	UINT32 cursor;
	UINT32 end;
	UINT32 limit;
	UINT16 silence[2];
	INT16 previous[2];
	INT16 pcm[2][16];
	UINT8 table;
	UINT8 channels;
	UINT8 index;
	UINT8 failed;
};

static void RdacStart(RdacDecoder &d, const RdacPhrase &p)
{
	memset(&d, 0, sizeof(d));
	d.index = 16;
	d.cursor = p.start;
	d.end = p.end;
	d.limit = p.limit;
	d.table = p.table;
	d.channels = p.channels;
}

static INT32 RdacFrame(RdacDecoder &d, const UINT8 *rom, INT16 *out)
{
	if (d.failed) return 0;

	if (d.index == 16) {
		if (d.cursor > d.end && !d.silence[0] && !d.silence[1]) return 0;

		for (INT32 ch = 0; ch < d.channels; ch++) {
			if (d.silence[ch]) {
				d.silence[ch]--;
				memset(d.pcm[ch], 0, sizeof(d.pcm[ch]));
				continue;
			}
			if (d.cursor > d.end || d.cursor + 2 > d.limit) {
				d.failed = 1;
				return 0;
			}

			const UINT8 *data = rom + d.cursor;
			const UINT32 h = RdacHeader(data);
			const UINT32 length = RdacFormats[d.table].length[h];
			if (length < 2 || (length & 1) || d.cursor + length > d.limit) {
				d.failed = 1;
				return 0;
			}

			if (h == 0xff) {
				const UINT32 word = RdacRead16(data);
				d.silence[ch] = ((word >> 4) & 0xf0) | (word & 15);
				memset(d.pcm[ch], 0, sizeof(d.pcm[ch]));
			} else if (!RdacDecodeHalf(data, length / 2, 1, h, d.previous[ch], d.pcm[ch]) || !RdacDecodeHalf(data, length / 2, 0, h, d.pcm[ch][7], d.pcm[ch] + 8)) {
				d.failed = 1;
				return 0;
			}

			d.previous[ch] = d.pcm[ch][15];
			d.cursor += length;
		}
		d.index = 0;
	}

	out[0] = d.pcm[0][d.index];
	out[1] = (d.channels == 2) ? d.pcm[1][d.index] : 0;
	d.index++;

	return 1;
}

// ---------------------------------------------------------------------------
// player

// a level that moves linearly to its target over a fade time
struct RppRamp
{
	double value;
	double target;
	double delta;
	UINT32 remaining;
};

struct RppVoice
{
	RdacDecoder decoder;
	UINT16 phrase;
	UINT16 queued[RPP_QUEUE];
	UINT8 queued_loops[RPP_QUEUE];
	UINT8 queued_late[RPP_QUEUE];
	UINT8 queue_count;
	UINT8 loops;
	UINT8 channels;
	UINT8 step;
	UINT8 phase;
	UINT8 late;
	INT8 owner;
	INT8 sequence;
	UINT8 playing;
	UINT8 paused;
	UINT8 release;
	UINT8 next_valid;
	INT16 current[2];
	INT16 next[2];
};

struct RppSequence
{
	UINT32 start;
	UINT32 cursor;
	UINT32 end;
	UINT32 delay;
	UINT8 loops;
	UINT8 playing;
	UINT8 paused;
	UINT8 ended;
	UINT8 release;
};

struct RppPort
{
	UINT8 source;
	UINT8 config;
	UINT8 time;
	UINT32 pulse;
};

// everything here is plain data and goes into savestates as one block
static struct {
	RppVoice voice[RPP_VOICES];
	RppSequence sequence[RPP_SEQUENCES];
	RppPort port[8];
	RppRamp gain[RPP_VOICES];
	RppRamp pan[RPP_VOICES];
	RppRamp route[RPP_VOICES][4];
	RppRamp master[2];
	RppRamp output[4];
	UINT16 lowcut[4];
	UINT16 highcut[4];
	UINT8 lowgain[4];
	UINT8 effect[RPP_VOICES];
	UINT8 total_effect;
	UINT8 mix_c;
	UINT8 mono_c;
	UINT16 control;
	UINT8 handshake;
	UINT8 packet[6];
	UINT8 packet_pos;
} st;

static UINT8 *Rom = NULL;
static UINT32 nRomSize = 0;
static UINT32 nSequenceTable = 0;
static UINT32 nSequenceCount = 0;

static UINT64 (*pCpuCycles)() = NULL;
static UINT32 nClock = 1;
static UINT64 nLastCycles = 0;
static UINT64 nCycleAcc = 0;

static INT16 *SampleBuf = NULL;		// stereo, 44.1 kHz, current frame
static INT32 nSampleBufLen = 0;
static INT32 nSamplePos = 0;
static INT16 LastSample[2];
static double fVolume = 1.0;

static void RampInit(RppRamp &r)
{
	r.value = r.target = 127;
	r.delta = 0;
	r.remaining = 0;
}

static void RampSet(RppRamp &r, UINT8 level, UINT8 fade)
{
	// provisional linear law, 10 ms per fade step; 0 and the SDK's 127 default are immediate until measured
	r.target = level;
	r.remaining = (fade && fade != 127) ? (UINT32)fade * 441 : 0;
	r.delta = r.remaining ? (r.target - r.value) / r.remaining : 0;
	if (!r.remaining) r.value = r.target;
}

static inline double RampTick(RppRamp &r)
{
	if (r.remaining) {
		if (--r.remaining) r.value += r.delta;
		else r.value = r.target;
	}
	return r.value;
}

static void VoiceInit(RppVoice &v)
{
	memset(&v, 0, sizeof(v));
	v.decoder.index = 16;
	v.loops = 1;
	v.channels = 1;
	v.step = 8;
	v.owner = -1;
	v.sequence = -1;
}

static UINT32 CommandSize(UINT8 op)
{
	if ((op >= 0xc0 && op <= 0xc5) || (op >= 0xc8 && op <= 0xcd)) return 5;

	switch (op) {
		case 0x80: case 0x81: case 0xfe:
			return 1;
		case 0x82: case 0x8c: case 0x8d: case 0x98: case 0x99: case 0x9c: case 0x9d:
		case 0xd0: case 0xd1: case 0xe0: case 0xe1: case 0xf1:
			return 2;
		case 0x83: case 0x84:
			return 3;
		case 0x85: case 0x87: case 0x89: case 0x90: case 0x91: case 0x94: case 0x95:
		case 0xa0: case 0xb0: case 0xfc:
			return 4;
		case 0x86: case 0x88: case 0xf0:
			return 5;
	}

	return 0;
}

static void PortEvent(UINT32 source, INT32 start)
{
	for (INT32 i = 0; i < 8; i++) {
		RppPort &p = st.port[i];
		const UINT32 mode = (p.config >> 4) & 3;
		if (p.source == source && (mode == 3 || mode == (UINT32)(start ? 1 : 2))) {
			p.pulse = ((p.time > 1) ? p.time : 1) * 441;	// provisional 10 ms units
		}
	}
}

static INT32 SourcePlaying(UINT32 source)
{
	if (source & 0x40) return st.sequence[source & 7].playing;

	const RppVoice &v = st.voice[source & 31];
	return v.owner >= 0 && st.voice[(INT32)v.owner].playing;
}

static void StopVoice(UINT32 ch)
{
	if (ch >= RPP_VOICES || st.voice[ch].owner < 0) return;

	const UINT32 owner = st.voice[ch].owner;
	RppVoice &v = st.voice[owner];
	for (UINT32 n = 0; n < v.channels; n++) {
		PortEvent(owner + n, 0);
		st.voice[owner + n].owner = -1;
	}
	v.playing = 0;
	v.queue_count = 0;
	v.sequence = -1;
}

static INT32 StartVoice(UINT32 ch, UINT32 phrase, UINT8 loops, UINT8 late, INT32 seq)
{
	RdacPhrase p;
	if (!RdacDescribe(Rom, nRomSize, phrase, p) || ch + p.channels > RPP_VOICES) return 0;

	StopVoice(ch);
	if (p.channels == 2) StopVoice(ch + 1);

	RppVoice &v = st.voice[ch];
	VoiceInit(v);
	v.phrase = phrase;
	v.loops = loops;
	v.late = late;
	v.channels = p.channels;
	v.step = p.step;
	v.sequence = seq;
	RdacStart(v.decoder, p);
	if (!RdacFrame(v.decoder, Rom, v.current)) return 0;
	v.next_valid = RdacFrame(v.decoder, Rom, v.next);
	v.playing = 1;

	for (UINT32 n = 0; n < p.channels; n++) {
		st.voice[ch + n].owner = ch;
		PortEvent(ch + n, 1);
	}

	return 1;
}

static void Play(UINT32 ch, UINT32 phrase, UINT8 loops, UINT8 late, INT32 immediate, INT32 seq)
{
	if (ch >= RPP_VOICES) return;

	RdacPhrase p;
	if (!RdacDescribe(Rom, nRomSize, phrase, p)) return;

	// stereo phrases use an even/odd channel pair, and KOF addresses either member of the pair
	if (p.channels == 2) ch &= ~1U;

	if (!immediate && st.voice[ch].owner >= 0) {
		RppVoice &v = st.voice[(INT32)st.voice[ch].owner];
		if (v.queue_count == RPP_QUEUE) return;

		const UINT32 q = v.queue_count++;
		v.queued[q] = phrase;
		v.queued_loops[q] = loops;
		v.queued_late[q] = late;
		return;
	}

	StartVoice(ch, phrase, loops, late, seq);
}

static void FinishVoice(UINT32 ch)
{
	RppVoice &v = st.voice[ch];
	const UINT8 phase = v.phase;

	if (v.decoder.failed || v.release) {
		StopVoice(ch);
	} else if (v.queue_count) {
		const RppVoice old = v;
		StartVoice(ch, old.queued[0], old.queued_loops[0], old.queued_late[0], old.sequence);
		v.queue_count = old.queue_count - 1;
		for (UINT32 i = 0; i < v.queue_count; i++) {
			v.queued[i] = old.queued[i + 1];
			v.queued_loops[i] = old.queued_loops[i + 1];
			v.queued_late[i] = old.queued_late[i + 1];
		}
	} else if (!v.loops || v.loops > 1) {
		StartVoice(ch, v.phrase, v.loops ? v.loops - 1 : 0, v.late, v.sequence);
	} else {
		StopVoice(ch);
	}

	// keep the fractional sample clock across loops and chained phrases
	if (v.playing) v.phase = phase;
}

static void StopSequence(UINT32 id)
{
	for (UINT32 ch = 0; ch < RPP_VOICES; ch++) {
		if (st.voice[ch].owner == (INT32)ch && st.voice[ch].sequence == (INT32)id) StopVoice(ch);
	}
	if (st.sequence[id].playing) PortEvent(0x40 | id, 0);
	memset(&st.sequence[id], 0, sizeof(st.sequence[id]));
	st.sequence[id].loops = 1;
}

static void PlaySequence(UINT32 id, UINT32 number, UINT8 loops)
{
	StopSequence(id);
	if (number >= nSequenceCount) return;

	RdacPhrase first;
	if (!RdacDescribe(Rom, nRomSize, 0, first)) return;

	const UINT32 start = RdacRead32(Rom + nSequenceTable + number * 4) >> 3;
	const UINT32 end = (number + 1 < nSequenceCount) ? (RdacRead32(Rom + nSequenceTable + (number + 1) * 4) >> 3) : first.start;
	if (start < nSequenceTable + nSequenceCount * 4 || start >= end || end > first.start || (start & 1)) return;

	RppSequence &s = st.sequence[id];
	s.start = start;
	s.cursor = start;
	s.end = end;
	s.loops = loops;
	s.playing = 1;
	PortEvent(0x40 | id, 1);
}

static void ResetPlayers()
{
	for (UINT32 ch = 0; ch < RPP_VOICES; ch++) StopVoice(ch);
	for (UINT32 id = 0; id < RPP_SEQUENCES; id++) StopSequence(id);
}

// the player's own reset; the 0x80 command does this without touching the board's control latch
static void SoftReset()
{
	ResetPlayers();

	for (INT32 ch = 0; ch < RPP_VOICES; ch++) {
		VoiceInit(st.voice[ch]);
		RampInit(st.gain[ch]);
		RampInit(st.pan[ch]);
		RampSet(st.pan[ch], 64, 0);
		for (INT32 n = 0; n < 4; n++) RampInit(st.route[ch][n]);
	}
	for (INT32 i = 0; i < 2; i++) RampInit(st.master[i]);
	for (INT32 i = 0; i < 4; i++) {
		RampInit(st.output[i]);
		st.lowcut[i] = 0;
		st.highcut[i] = 4096;
		st.lowgain[i] = 127;
	}
	for (INT32 i = 0; i < 8; i++) {
		memset(&st.port[i], 0, sizeof(st.port[i]));
		st.port[i].source = i;
		st.port[i].config = 0x40;
		st.port[i].time = 1;
	}

	memset(st.effect, 0, sizeof(st.effect));
	st.total_effect = 0;
	st.mix_c = 0;
	st.mono_c = 0;
	st.packet_pos = 0;
	st.handshake = 0;
}

static void Execute(const UINT8 *p, INT32 seq)
{
	const UINT32 op = p[0];
	const UINT32 ch = p[1] & 31;

	if ((op >= 0xc0 && op <= 0xc5) || (op >= 0xc8 && op <= 0xcd)) {
		Play(ch, (p[3] << 7) | p[4], p[2], op & 7, (op >> 3) & 1, seq);
		return;
	}

	switch (op) {
		case 0x80:
			SoftReset();
			break;

		case 0x81:
			ResetPlayers();
			RampSet(st.master[1], 127, 0);
			break;

		case 0x82:
			st.mono_c = p[1] ? 1 : 0;
			break;

		case 0x83: case 0x84:
			RampSet(st.master[op - 0x83], p[2], p[1]);
			break;

		case 0x85:
			if ((p[1] & ~5) == 0) RampSet(st.output[(p[1] & 1) | ((p[1] >> 1) & 2)], p[3], p[2]);
			break;

		case 0x86: case 0x87: case 0x88:
			// filter settings are stored, their response is unknown
			if ((p[1] & ~5) == 0) {
				const UINT32 output = (p[1] & 1) | ((p[1] >> 1) & 2);
				if (op == 0x86) st.lowcut[output] = (p[3] << 7) | p[4];
				else if (op == 0x87) st.lowgain[output] = p[3];
				else st.highcut[output] = (p[3] << 7) | p[4];
			}
			break;

		case 0x89: {
			RppPort &target = st.port[p[2] & 7];
			if (target.source != p[1] || target.config != p[2]) target.pulse = 0;
			target.source = p[1];
			target.config = p[2];
			target.time = p[3];
			break;
		}

		case 0x8c:
			st.total_effect = p[1];
			break;

		case 0x8d:
			st.mix_c = p[1] ? 1 : 0;
			break;

		case 0x90: case 0x91: case 0x94: case 0x95:
			RampSet(st.route[ch][(op & 1) | ((op >> 1) & 2)], p[3], p[2]);
			break;

		case 0x98: case 0x99: case 0x9c: case 0x9d: {
			const UINT32 bit = (op >> 2) & 1;
			st.effect[ch] = (st.effect[ch] & ~(1U << bit)) | ((op & 1) << bit);
			break;
		}

		case 0xa0:
			RampSet(st.pan[ch], p[3], p[2]);
			break;

		case 0xb0:
			RampSet(st.gain[ch], p[3], p[2]);
			break;

		case 0xd0: case 0xd1:
			if (st.voice[ch].owner >= 0) st.voice[(INT32)st.voice[ch].owner].paused = op & 1;
			break;

		case 0xe0:
			StopVoice(ch);
			break;

		case 0xe1:
			if (st.voice[ch].owner >= 0) st.voice[(INT32)st.voice[ch].owner].release = 1;
			break;

		case 0xf0:
			PlaySequence(p[1] & 7, (p[3] << 7) | p[4], p[2]);
			break;

		case 0xf1: {
			const UINT32 id = p[1] & 7;
			const UINT32 mode = (p[1] >> 5) & 3;
			if (!mode) {
				StopSequence(id);
			} else if (mode == 1) {
				st.sequence[id].release = 1;
				st.sequence[id].ended = 1;
				for (INT32 i = 0; i < RPP_VOICES; i++) {
					if (st.voice[i].sequence == (INT32)id) st.voice[i].release = 1;
				}
			} else {
				st.sequence[id].paused = (mode == 2);
				for (INT32 i = 0; i < RPP_VOICES; i++) {
					if (st.voice[i].sequence == (INT32)id) st.voice[i].paused = (mode == 2);
				}
			}
			break;
		}
	}
}

static void StepSequence(UINT32 id)
{
	RppSequence &s = st.sequence[id];
	if (!s.playing || s.paused) return;

	if (s.delay) {
		s.delay--;
		return;
	}

	if (s.ended) {
		for (UINT32 ch = 0; ch < RPP_VOICES; ch++) {
			if (st.voice[ch].playing && st.voice[ch].owner == (INT32)ch && st.voice[ch].sequence == (INT32)id) return;
		}
		if (!s.release && (!s.loops || s.loops > 1)) {
			if (s.loops) s.loops--;
			s.cursor = s.start;
			s.ended = 0;
		} else {
			StopSequence(id);
			return;
		}
	}

	// the packets are stored word-swapped; bound the work per sample, including for malformed programs
	for (UINT32 count = 0; count < 32 && !s.ended && !s.delay; count++) {
		if (s.cursor >= s.end) {
			StopSequence(id);
			return;
		}

		UINT8 packet[6] = { 0, 0, 0, 0, 0, 0 };
		packet[0] = Rom[s.cursor ^ 1];
		const UINT32 size = CommandSize(packet[0]);
		if (!size || s.cursor + size + 1 > s.end) {
			StopSequence(id);
			return;
		}

		UINT8 sum = packet[0];
		for (UINT32 i = 1; i <= size; i++) {
			packet[i] = Rom[(s.cursor + i) ^ 1];
			if (i < size) sum += packet[i];
		}
		if ((sum & 0x7f) != packet[size]) {
			StopSequence(id);
			return;
		}

		s.cursor += (size + 2) & ~1U;

		if (packet[0] == 0xfe) {
			s.ended = 1;
		} else if (packet[0] == 0xfc) {
			s.delay = ((UINT32)packet[1] << 14) | (packet[2] << 7) | packet[3];
		} else if (packet[0] == 0xf0 || packet[0] == 0xf1 || packet[0] == 0x80 || packet[0] == 0x81) {
			StopSequence(id);
			return;
		} else {
			Execute(packet, id);
		}
	}
}

static void RenderSample(INT16 *out)
{
	for (UINT32 id = 0; id < RPP_SEQUENCES; id++) StepSequence(id);

	double gain[RPP_VOICES], pan[RPP_VOICES], route[RPP_VOICES][4];
	for (INT32 ch = 0; ch < RPP_VOICES; ch++) {
		gain[ch] = RampTick(st.gain[ch]) / 127.0;
		pan[ch] = RampTick(st.pan[ch]);
		for (INT32 n = 0; n < 4; n++) route[ch][n] = RampTick(st.route[ch][n]) / 127.0;
	}

	double mix[4] = { 0, 0, 0, 0 };
	for (UINT32 ch = 0; ch < RPP_VOICES; ch++) {
		RppVoice &v = st.voice[ch];
		if (!v.playing || v.owner != (INT32)ch || v.paused) continue;

		for (UINT32 n = 0; n < v.channels; n++) {
			const UINT32 physical = ch + n;
			double sample = v.current[n];
			if (v.next_valid) sample += (v.next[n] - sample) * (v.phase / 8.0);
			sample *= gain[physical];

			double left = (127.0 - pan[physical]) / 63.0;
			double right = pan[physical] / 64.0;
			if (left > 1.0) left = 1.0;
			if (right > 1.0) right = 1.0;

			for (INT32 o = 0; o < 4; o++) mix[o] += sample * route[physical][o] * ((o & 1) ? right : left);
		}

		v.phase += v.step;
		if (v.phase >= 8) {
			v.phase -= 8;
			if (v.next_valid) {
				v.current[0] = v.next[0];
				v.current[1] = v.next[1];
				v.next_valid = RdacFrame(v.decoder, Rom, v.next);
			} else {
				FinishVoice(ch);
			}
		}
	}

	const double master = RampTick(st.master[0]) * RampTick(st.master[1]) / (127.0 * 127.0);
	for (INT32 o = 0; o < 4; o++) mix[o] *= RampTick(st.output[o]) * master / 127.0;
	if (st.mono_c) mix[2] = mix[3] = (mix[2] + mix[3]) * 0.5;
	if (st.mix_c) {
		mix[0] += mix[2];
		mix[1] += mix[3];
	}

	// only the main pair reaches the speakers
	for (INT32 o = 0; o < 2; o++) {
		double s = mix[o] * fVolume;
		out[o] = (INT16)((s < -32768.0) ? -32768 : (s > 32767.0) ? 32767 : (INT32)s);
	}

	for (INT32 i = 0; i < 8; i++) {
		if (st.port[i].pulse) st.port[i].pulse--;
	}
}

// the chip runs on the cpu's timeline, whether or not the frontend wants audio
static void RppSync()
{
	UINT64 now = pCpuCycles();
	if (now < nLastCycles) nLastCycles = now;

	nCycleAcc += (now - nLastCycles) * RPP_RATE;
	nLastCycles = now;

	UINT64 samples = nCycleAcc / nClock;
	nCycleAcc -= samples * nClock;

	while (samples--) {
		INT16 out[2];
		RenderSample(out);
		if (nSamplePos < nSampleBufLen) {
			SampleBuf[nSamplePos * 2 + 0] = out[0];
			SampleBuf[nSamplePos * 2 + 1] = out[1];
			nSamplePos++;
		}
	}
}

static void CommandByte(UINT8 data)
{
	if (data & 0x80) {
		st.packet_pos = 0;
		if (!CommandSize(data)) {
			st.handshake |= 0x80;
			return;
		}
	} else if (!st.packet_pos) {
		return;
	}

	st.packet[st.packet_pos++] = data;
	const UINT32 length = CommandSize(st.packet[0]);
	if (st.packet_pos != length + 1) return;

	UINT8 sum = 0;
	for (UINT32 i = 0; i < length; i++) sum += st.packet[i];
	st.packet_pos = 0;

	if ((sum & 0x7f) != data) {
		st.handshake |= 0x80;
		return;
	}

	RppSync();
	Execute(st.packet, -1);
	st.handshake &= ~0x80;
}

UINT16 RPP324HostRead(INT32 offset)
{
	return offset ? st.handshake : 0;
}

void RPP324HostWrite(INT32 offset, UINT16 data)
{
	if (offset) {
		st.handshake = data;
		if (data & 0x80) st.packet_pos = 0;
	} else {
		CommandByte(data);
	}
}

UINT16 RPP324ControlRead()
{
	return st.control;
}

void RPP324ControlWrite(UINT16 data, UINT16 mask)
{
	// Y2 FPGA board-control latch; only the initialization value 0xbd01 is known
	st.control = (st.control & ~mask) | (data & mask);
}

UINT16 RPP324StatusRead()
{
	RppSync();

	UINT16 result = 0;
	for (INT32 i = 0; i < 8; i++) {
		const RppPort &p = st.port[i];
		const INT32 active = (p.config & 0x30) ? (p.pulse != 0) : SourcePlaying(p.source);
		if (active == ((p.config >> 6) & 1)) result |= 1 << i;
	}

	return result;
}

void RPP324Update(INT16 *buffer, INT32 length)
{
	RppSync();

	if (buffer && length > 0) {
		// linear resampling of this frame's samples, continuing from the last sample of the previous frame
		const INT32 n = nSamplePos;
		for (INT32 i = 0; i < length; i++) {
			if (n == 0) {
				buffer[i * 2 + 0] = LastSample[0];
				buffer[i * 2 + 1] = LastSample[1];
				continue;
			}
			const UINT64 t = ((UINT64)(i + 1) * n << 16) / length;	// 16.16, source index + 1
			const INT32 idx = (INT32)(t >> 16) - 1;
			const INT32 frac = (INT32)(t & 0xffff);
			for (INT32 c = 0; c < 2; c++) {
				const INT32 s0 = (idx < 0) ? LastSample[c] : SampleBuf[idx * 2 + c];
				const INT32 s1 = (idx + 1 < n) ? SampleBuf[(idx + 1) * 2 + c] : s0;
				buffer[i * 2 + c] = (INT16)(s0 + (((s1 - s0) * frac) >> 16));
			}
		}
	}

	if (nSamplePos) {
		LastSample[0] = SampleBuf[(nSamplePos - 1) * 2 + 0];
		LastSample[1] = SampleBuf[(nSamplePos - 1) * 2 + 1];
	}
	nSamplePos = 0;
}

void RPP324Reset()
{
	SoftReset();
	st.control = 0;

	nLastCycles = pCpuCycles();
	nCycleAcc = 0;
	nSamplePos = 0;
	LastSample[0] = LastSample[1] = 0;
}

void RPP324SetVolume(double vol)
{
	fVolume = vol;
}

void RPP324Init(UINT8 *rom, UINT32 rom_size, UINT64 (*pCycles)(), UINT32 nCpuClock)
{
	Rom = rom;
	nRomSize = rom_size;
	pCpuCycles = pCycles;
	nClock = nCpuClock;
	fVolume = 1.0;

	// the sequence table follows the phrase directory and shares its halfword addressing; Higurashi has none
	RdacPhrase first;
	nSequenceTable = RdacPhraseCount(Rom, nRomSize) * 10 + 4;
	nSequenceCount = 0;
	if (RdacDescribe(Rom, nRomSize, 0, first) && nSequenceTable + 4 <= first.start) {
		const UINT32 first_sequence = RdacRead32(Rom + nSequenceTable) >> 3;
		if (first_sequence > nSequenceTable && first_sequence < first.start && !((first_sequence - nSequenceTable) & 3)) {
			nSequenceCount = (first_sequence - nSequenceTable) / 4;
			if (nSequenceCount > 4096) nSequenceCount = 4096;
		}
	}

	nSampleBufLen = RPP_RATE / 10;	// a frame's worth with plenty of margin
	SampleBuf = (INT16*)BurnMalloc(nSampleBufLen * 2 * sizeof(INT16));

	memset(&st, 0, sizeof(st));
	RPP324Reset();
}

void RPP324Exit()
{
	BurnFree(SampleBuf);
	SampleBuf = NULL;
	Rom = NULL;
	pCpuCycles = NULL;
}

void RPP324Scan(INT32 nAction, INT32 *)
{
	if (nAction & ACB_DRIVER_DATA) {
		struct BurnArea ba;
		memset(&ba, 0, sizeof(ba));
		ba.Data = &st;
		ba.nLen = sizeof(st);
		ba.szName = "RPP-324";
		BurnAcb(&ba);

		SCAN_VAR(nLastCycles);
		SCAN_VAR(nCycleAcc);
		SCAN_VAR(LastSample);
	}
}
