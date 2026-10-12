// X-Men Vs. Street Fighter 3v3 + DHC (hack by Morphus56K): the board additions its ROMs use.
//
// The hack's 68000 code finds two windows that the stock CPS2 board doesn't have:
//   0x808000-0x8083ff  input window, read only: every player's held, pressed and released
//                      buttons, a sample counter, a 16-sample history, and a capability word
//                      that tells the ROM what this board can draw
//   0x810000-0x81ffff  64KB of extra work RAM, each 68000 word stored high byte first
// The ROMs also mark the mini portraits on the HUD so that this board can draw them in the
// fighter's chosen colour (one of eighteen), decoded from the colour table in the program ROM.
// Nothing here is used by any other game: cps_rw.cpp only enables it for the 3v3 drivers.

#ifndef XMVSF3V3_H
#define XMVSF3V3_H

#define XMV6_BASE					0x808000
#define XMV6_WINDOW_BYTES			0x400
#define XMV6_VERSION				1
#define XMV6_PLAYERS				6
#define XMV6_VALID_MASK				0x1f7f
#define XMV6_DEPTH					16
#define XMV6_RECORD_BYTES			0x28
#define XMV6_WORK_RAM_BASE			0x810000
#define XMV6_WORK_RAM_BYTES			0x10000

// Input window registers (byte offsets from XMV6_BASE)
#define XMV6_REG_MAGIC_XM			0x00
#define XMV6_REG_MAGIC_6P			0x02
#define XMV6_REG_VERSION			0x04
#define XMV6_REG_PLAYERS			0x06
#define XMV6_REG_SEQUENCE			0x08
#define XMV6_REG_VALID_MASK			0x0c
#define XMV6_REG_DEPTH				0x0e
#define XMV6_REG_HELD				0x10
#define XMV6_REG_PRESSED			0x20
#define XMV6_REG_RELEASED			0x30
#define XMV6_REG_PREVIOUS			0x40
#define XMV6_REG_CAPABILITIES		0x4c
#define XMV6_REG_COUNT				0x50
#define XMV6_REG_HISTORY			0x100

// History record layout (byte offsets in each XMV6_RECORD_BYTES record)
#define XMV6_REC_SEQUENCE			0x00
#define XMV6_REC_HELD				0x04
#define XMV6_REC_PRESSED			0x10
#define XMV6_REC_RELEASED			0x1c

// Button bits in every held/pressed/released word
#define XMV6_BIT_RIGHT				0x0001
#define XMV6_BIT_LEFT				0x0002
#define XMV6_BIT_DOWN				0x0004
#define XMV6_BIT_UP					0x0008
#define XMV6_BIT_FIRE_1				0x0010
#define XMV6_BIT_FIRE_2				0x0020
#define XMV6_BIT_FIRE_3				0x0040
#define XMV6_BIT_FIRE_4				0x0100
#define XMV6_BIT_FIRE_5				0x0200
#define XMV6_BIT_FIRE_6				0x0400
#define XMV6_BIT_START				0x0800
#define XMV6_BIT_COIN				0x1000

// Capability word: what this board does for the ROM
#define XMV6_CAP_WORK_RAM			0x0001	// the work RAM above
#define XMV6_CAP_PORTRAITS			0x0002	// HUD portraits in the costume colour
#define XMV6_CAP_PORTRAITS_6		0x0004	// ... in six colours
#define XMV6_CAP_WIDE				0x0008	// 512 pixel wide screen
#define XMV6_CAP_WIDE_SCROLL1		0x0010	// wide screen: scroll1 columns 64-127 from work RAM
#define XMV6_CAP_PORTRAITS_12		0x0020	// HUD portraits in twelve colours
#define XMV6_CAP_PORTRAITS_18		0x0040	// HUD portraits in eighteen colours

struct XmvSixRecord {
	UINT32 sequence;
	UINT16 held[XMV6_PLAYERS], pressed[XMV6_PLAYERS], released[XMV6_PLAYERS];
};

struct XmvSixInputState {
	UINT32 abi;
	UINT32 sequence;
	UINT16 count;
	UINT16 held[XMV6_PLAYERS], previous[XMV6_PLAYERS];
	UINT16 pressed[XMV6_PLAYERS], released[XMV6_PLAYERS];
	XmvSixRecord history[XMV6_DEPTH];
};

static inline void XmvSixReset(XmvSixInputState* s)
{
	memset(s, 0, sizeof(*s));
	s->abi = XMV6_VERSION;
}

// Opposite directions held together cancel out
static inline UINT16 XmvSixClean(UINT16 value)
{
	value &= XMV6_VALID_MASK;
	if ((value & 3) == 3) value &= ~3;
	if ((value & 12) == 12) value &= ~12;
	return value;
}

// One sample per frame; the counter wraps
static inline void XmvSixSample(XmvSixInputState* s, const UINT16* raw)
{
	s->sequence++;
	if (s->count < XMV6_DEPTH) s->count++;
	XmvSixRecord* r = &s->history[s->sequence % XMV6_DEPTH];
	r->sequence = s->sequence;
	for (INT32 p = 0; p < XMV6_PLAYERS; p++) {
		s->previous[p] = s->held[p];
		s->held[p] = XmvSixClean(raw[p]);
		s->pressed[p] = s->held[p] & ~s->previous[p];
		s->released[p] = s->previous[p] & ~s->held[p];
		r->held[p] = s->held[p];
		r->pressed[p] = s->pressed[p];
		r->released[p] = s->released[p];
	}
}

static inline UINT16 XmvSixReadWord(const XmvSixInputState* s, UINT16 capabilities, UINT32 offset)
{
	if ((offset & 1) || offset >= XMV6_WINDOW_BYTES) return 0;

	switch (offset) {
		case XMV6_REG_MAGIC_XM:			return 0x584d;	// 'XM'
		case XMV6_REG_MAGIC_6P:			return 0x3650;	// '6P'
		case XMV6_REG_VERSION:			return XMV6_VERSION;
		case XMV6_REG_PLAYERS:			return XMV6_PLAYERS;
		case XMV6_REG_SEQUENCE:			return (UINT16)(s->sequence >> 16);
		case XMV6_REG_SEQUENCE + 2:		return (UINT16)s->sequence;
		case XMV6_REG_VALID_MASK:		return XMV6_VALID_MASK;
		case XMV6_REG_DEPTH:			return XMV6_DEPTH;
		case XMV6_REG_COUNT:			return s->count;
		case XMV6_REG_CAPABILITIES:		return capabilities;
	}

	static const UINT32 starts[4] = { XMV6_REG_HELD, XMV6_REG_PRESSED, XMV6_REG_RELEASED, XMV6_REG_PREVIOUS };
	const UINT16* arrays[4] = { s->held, s->pressed, s->released, s->previous };
	for (INT32 i = 0; i < 4; i++) {
		if (offset >= starts[i] && offset < starts[i] + XMV6_PLAYERS * 2)
			return arrays[i][(offset - starts[i]) / 2];
	}

	if (offset >= XMV6_REG_HISTORY && offset < XMV6_REG_HISTORY + XMV6_DEPTH * XMV6_RECORD_BYTES) {
		UINT32 index = (offset - XMV6_REG_HISTORY) / XMV6_RECORD_BYTES;
		UINT32 at = (offset - XMV6_REG_HISTORY) % XMV6_RECORD_BYTES;
		const XmvSixRecord* r = &s->history[index];
		if (at == XMV6_REC_SEQUENCE) return (UINT16)(r->sequence >> 16);
		if (at == XMV6_REC_SEQUENCE + 2) return (UINT16)r->sequence;
		if (at < XMV6_REC_PRESSED) return r->held[(at - XMV6_REC_HELD) / 2];
		if (at < XMV6_REC_RELEASED) return r->pressed[(at - XMV6_REC_PRESSED) / 2];
		return r->released[(at - XMV6_REC_RELEASED) / 2];
	}

	return 0;
}

static inline UINT8 XmvSixReadByte(const XmvSixInputState* s, UINT16 capabilities, UINT32 offset)
{
	if (offset >= XMV6_WINDOW_BYTES) return 0;
	UINT16 word = XmvSixReadWord(s, capabilities, offset & ~1);
	return (UINT8)((offset & 1) ? word : (word >> 8));
}

static inline UINT16 XmvSixRamReadWord(const UINT8* ram, UINT32 offset)
{
	if ((offset & 1) || offset >= XMV6_WORK_RAM_BYTES - 1) return 0;
	return (UINT16)((ram[offset] << 8) | ram[offset + 1]);
}

static inline void XmvSixRamWriteWord(UINT8* ram, UINT32 offset, UINT16 value)
{
	if ((offset & 1) || offset >= XMV6_WORK_RAM_BYTES - 1) return;
	ram[offset + 0] = (UINT8)(value >> 8);
	ram[offset + 1] = (UINT8)value;
}

// ----------------------------------------------------------------------------
// HUD portraits
//
// Program ROM reads: CpsRom keeps each 68000 word as (low byte, high byte), on every host.

static inline UINT16 XmvPortraitWord(const UINT8* rom, UINT32 at)
{
	return (UINT16)(rom[at] | (rom[at + 1] << 8));
}

// One 68000 byte
static inline UINT32 XmvPortraitByte(const UINT8* rom, UINT32 at)
{
	return rom[at ^ 1];
}

// The ROM marks a mini portrait through its OBJ palette field:
//   0x1c/0x1d  stock costume 0/1
//   0x12..0x17 colour 0..5
//   0x06..0x11 colour 6..17 (the marker is the colour)
// Normal screen: left raw X 0xe040, right 0xe1a0; wide screen: left 0xe000, right 0xe1e0. Raw Y
// is one of the three HUD rows. Left portraits are unflipped 2x1 objects (0x0100), right ones
// are X-flipped (0x0120). Anything else is not a portrait.
static inline bool XmvPortraitObject(UINT32 tile, UINT32 rawX, UINT32 rawY, UINT32 attributes, bool wide, UINT32* color)
{
	if (tile < 0x1ff00 || tile > 0x1ff22 || (tile & 1)) return false;
	if (rawY != 0x2020 && rawY != 0x202e && rawY != 0x203c) return false;

	UINT32 flags = attributes & ~0x1f, marker = attributes & 0x1f;
	if (rawX == (wide ? 0xe000 : 0xe040)) {
		if (flags != 0x0100) return false;
	} else if (rawX == (wide ? 0xe1e0 : 0xe1a0)) {
		if (flags != 0x0120) return false;
	} else {
		return false;
	}

	if (marker == 0x1c || marker == 0x1d) { *color = marker - 0x1c; return true; }
	if (marker >= 0x12 && marker <= 0x17) { *color = marker - 0x12; return true; }
	if (marker >= 0x06 && marker <= 0x11) { *color = marker; return true; }
	return false;
}

// Row 0 of colour 2..17 from the packed 'X3CZ' colour table at 0x335000: costume 1's row 0,
// then the colour's row 0 changes (optionally starting from costume 0's row), 12-bit colours
// packed two per three bytes. An entry of 0xffff keeps costume 1's row. `pointer` is the
// fighter's stock costume 0 palette block.
static inline bool XmvPortraitPacked(const UINT8* rom, UINT32 romBytes, UINT32 character, UINT32 color, UINT32 pointer, UINT16* row)
{
	const UINT32 table = 0x335000;
	if (XmvPortraitWord(rom, table + 4) != 1) return false;

	UINT32 identities = XmvPortraitWord(rom, table + 6), colours = XmvPortraitWord(rom, table + 8);
	UINT32 indexAt = XmvPortraitWord(rom, table + 10), dataAt = XmvPortraitWord(rom, table + 12);
	if (character / 2 >= identities || color < 2 || color - 2 >= colours) return false;

	UINT32 entry = table + indexAt + ((character / 2) * colours + color - 2) * 2;
	if (entry + 2 > romBytes) return false;

	for (INT32 pen = 0; pen < 16; pen++)
		row[pen] = XmvPortraitWord(rom, pointer + 0x60 + pen * 2) & 0x0fff;

	UINT32 offset = XmvPortraitWord(rom, entry);
	if (offset == 0xffff) return true;

	UINT32 at = table + dataAt + offset;
	if (at >= romBytes) return false;

	UINT32 rows = XmvPortraitByte(rom, at++);
	for (UINT32 n = 0; n < rows; n++) {
		if (at + 3 > romBytes) return false;
		UINT32 info = XmvPortraitByte(rom, at++);
		UINT32 r = info & 3;
		if (r == 0 && (info & 0x80)) {
			for (INT32 pen = 0; pen < 16; pen++)
				row[pen] = XmvPortraitWord(rom, pointer + pen * 2) & 0x0fff;
		}
		UINT32 mask = (XmvPortraitByte(rom, at) << 8) | XmvPortraitByte(rom, at + 1);
		at += 2;
		INT32 pending = -1;
		for (INT32 pen = 0; pen < 16; pen++) {
			if (!(mask & (1 << pen))) continue;
			UINT32 colour;
			if (pending < 0) {
				if (at + 2 > romBytes) return false;
				UINT32 first = XmvPortraitByte(rom, at), second = XmvPortraitByte(rom, at + 1);
				at += 2;
				colour = (first << 4) | (second >> 4);
				pending = second & 15;
			} else {
				if (at + 1 > romBytes) return false;
				colour = (pending << 8) | XmvPortraitByte(rom, at++);
				pending = -1;
			}
			if (r == 0) row[pen] = (UINT16)colour;
		}
	}

	return true;
}

// The portrait's 16 colours (RGB444) in CpsPalUpdate's pen order. Colours 0/1 come from the
// stock palette directory at 0x1145a8 (the Europe 961004 program), colours 2..17 from the
// colour table at 0x335000: the packed 'X3CZ' table, or for colours 2..5 the older 'X3C6' one
// (version 1, 19 fighters, 4 colours, 96 bytes each, data at +0x10). False leaves `output`
// alone, and Cps2ObjDraw doesn't draw that portrait.
static inline bool XmvPortraitColors(const UINT8* rom, UINT32 romBytes, UINT32 tile, UINT32 color, UINT16* output)
{
	static const UINT32 characters[18] = {
		0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e,
		0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e, 0x20, 0x22, 0x24
	};
	static const UINT32 pointers[19] = {
		0x114d7c, 0x114e3c, 0x114fbc, 0x11531c, 0x1153dc,
		0x11549c, 0x11559c, 0x1156bc, 0x11577c, 0x11589c,
		0x115a1c, 0x115b9c, 0x115c5c, 0x115d5c, 0x115fdc,
		0x11609c, 0x11621c, 0x11639c, 0x1164bc
	};

	if (rom == NULL || output == NULL || romBytes < 0x338000 || color > 17) return false;
	if (tile < 0x1ff00 || tile > 0x1ff22 || (tile & 1)) return false;

	UINT32 character = characters[(tile - 0x1ff00) / 2];
	UINT32 directory = 0x1145a8 + character * 2;
	UINT32 pointer = ((UINT32)XmvPortraitWord(rom, directory) << 16) | XmvPortraitWord(rom, directory + 2);
	if (pointer != pointers[character / 2]) return false;

	UINT16 row[16];
	const UINT32 table = 0x335000;
	if (color <= 1) {
		for (INT32 pen = 0; pen < 16; pen++)
			row[pen] = XmvPortraitWord(rom, pointer + color * 0x60 + pen * 2) & 0x0fff;
	} else if (XmvPortraitWord(rom, table) == 0x5833 && XmvPortraitWord(rom, table + 2) == 0x435a) {	// 'X3CZ'
		if (!XmvPortraitPacked(rom, romBytes, character, color, pointer, row)) return false;
	} else if (color <= 5 && XmvPortraitWord(rom, table) == 0x5833 && XmvPortraitWord(rom, table + 2) == 0x4336 &&	// 'X3C6'
			XmvPortraitWord(rom, table + 4) == 1 && XmvPortraitWord(rom, table + 6) == 19 &&
			XmvPortraitWord(rom, table + 8) == 4 && XmvPortraitWord(rom, table + 10) == 96 &&
			XmvPortraitWord(rom, table + 12) == 0x10) {
		UINT32 block = table + 0x10 + ((character / 2) * 4 + (color - 2)) * 96;
		for (INT32 pen = 0; pen < 16; pen++)
			row[pen] = XmvPortraitWord(rom, block + pen * 2) & 0x0fff;
	} else {
		return false;
	}

	for (INT32 pen = 0; pen < 16; pen++)
		output[pen ^ 15] = row[pen];

	return true;
}

#endif
