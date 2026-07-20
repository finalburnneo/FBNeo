// =============================================================================
//  SPC7110 coprocessor (Hudson) for FBNeo
// =============================================================================
// Ported from ares (ares/sfc/coprocessor/spc7110), cross-checked with bsnes-mercury.
//   decompressor: original neviksti, optimized talarubi
// FBNeo flat-C port / integration: (this file)
//
// Model: the ares/mercury cores run the chip as a clock-stepped thread and defer
// decompress/multiply/divide to a pending flag consumed by main().  Here we run the
// operations immediately inside the register write, which is functionally identical
// for software that polls the status/busy bits (they are set then cleared within the
// same access), and avoids pulling in a scheduler thread.
// =============================================================================

#include "spc7110.h"
#include "epsonrtc.h"
#include <string.h>

//======================
// cart memory + config
//======================

static UINT8* s_prom       = NULL;	// program ROM
static UINT32 s_promSize   = 0;
static UINT8* s_drom       = NULL;	// data ROM
static UINT32 s_dromSize   = 0;
static UINT8* s_erom       = NULL;	// expansion ROM (EXSPC7110 board, mapped $40-4f, bypasses the chip)
static UINT32 s_eromSize   = 0;
static UINT8* s_ram        = NULL;	// save SRAM
static UINT32 s_ramSize    = 0;
static INT32  s_hasRTC     = 0;
static INT32  s_rtcPowered = 0;		// 1 once the RTC coin-cell has been "installed" (cold power);
									// cleared only on cartridge unload, so console resets preserve the clock

//===============
// registers
//===============

//decompression unit
static UINT8 r4801;		//compression table B0
static UINT8 r4802;		//compression table B1
static UINT8 r4803;		//compression table B2 (n7)
static UINT8 r4804;		//compression table index
static UINT8 r4805;		//adjust length B0
static UINT8 r4806;		//adjust length B1
static UINT8 r4807;		//stride length
static UINT8 r4809;		//compression counter B0
static UINT8 r480a;		//compression counter B1
static UINT8 r480b;		//decompression settings (n2)
static UINT8 r480c;		//decompression status

static UINT8  dcuPending;
static UINT8  dcuMode;		//n2
static UINT32 dcuAddress;	//n23
static UINT32 dcuOffset;	//n32
static UINT8  dcuTile[32];

//data port unit
static UINT8 r4810;		//data port read + seek
static UINT8 r4811;		//data offset B0
static UINT8 r4812;		//data offset B1
static UINT8 r4813;		//data offset B2 (n7)
static UINT8 r4814;		//data adjust B0
static UINT8 r4815;		//data adjust B1
static UINT8 r4816;		//data stride B0
static UINT8 r4817;		//data stride B1
static UINT8 r4818;		//data port settings
static UINT8 r481a;		//data port seek

//arithmetic logic unit
static UINT8 r4820;		//16-bit multiplicand B0, 32-bit dividend B0
static UINT8 r4821;		//16-bit multiplicand B1, 32-bit dividend B1
static UINT8 r4822;		//32-bit dividend B2
static UINT8 r4823;		//32-bit dividend B3
static UINT8 r4824;		//16-bit multiplier B0
static UINT8 r4825;		//16-bit multiplier B1
static UINT8 r4826;		//16-bit divisor B0
static UINT8 r4827;		//16-bit divisor B1
static UINT8 r4828;		//32-bit product B0, 32-bit quotient B0
static UINT8 r4829;		//32-bit product B1, 32-bit quotient B1
static UINT8 r482a;		//32-bit product B2, 32-bit quotient B2
static UINT8 r482b;		//32-bit product B3, 32-bit quotient B3
static UINT8 r482c;		//16-bit remainder B0
static UINT8 r482d;		//16-bit remainder B1
static UINT8 r482e;		//math settings
static UINT8 r482f;		//math status

//memory control unit
static UINT8 r4830;		//bank 0 mapping + SRAM write enable
static UINT8 r4831;		//bank 1 mapping
static UINT8 r4832;		//bank 2 mapping
static UINT8 r4833;		//bank 3 mapping
static UINT8 r4834;		//bank mapping settings

//=====================
// bus mirror (ares Bus::mirror)
//=====================

static UINT32 bus_mirror(UINT32 addr, UINT32 size)
{
	if (size == 0) return 0;
	UINT32 base = 0;
	UINT32 mask = 1u << 23;
	while (addr >= size) {
		while (!(addr & mask)) mask >>= 1;
		addr -= mask;
		if (size > mask) { size -= mask; base += mask; }
		mask >>= 1;
	}
	return base + addr;
}

//=====================
// data ROM access
//=====================

static UINT8 datarom_read(UINT32 address)
{
	UINT32 size   = 1u << (r4834 & 3);  //size in MB
	UINT32 mask   = 0x100000u * size - 1;
	UINT32 offset = address & mask;
	if ((r4834 & 3) != 3 && (address & 0x400000)) return 0x00;
	return s_drom[bus_mirror(offset, s_dromSize)];
}

//=====================
// decompressor (arithmetic coder)
//=====================

enum { DEC_MPS = 0, DEC_LPS = 1 };
enum { DEC_One = 0xaa, DEC_Half = 0x55, DEC_Max = 0xff };

typedef struct { UINT8 probability; UINT8 next[2]; } ModelState;

static const ModelState dec_evolution[53] = {
	{0x5a, { 1, 1}}, {0x25, { 2, 6}}, {0x11, { 3, 8}},
	{0x08, { 4,10}}, {0x03, { 5,12}}, {0x01, { 5,15}},

	{0x5a, { 7, 7}}, {0x3f, { 8,19}}, {0x2c, { 9,21}},
	{0x20, {10,22}}, {0x17, {11,23}}, {0x11, {12,25}},
	{0x0c, {13,26}}, {0x09, {14,28}}, {0x07, {15,29}},
	{0x05, {16,31}}, {0x04, {17,32}}, {0x03, {18,34}},
	{0x02, { 5,35}},

	{0x5a, {20,20}}, {0x48, {21,39}}, {0x3a, {22,40}},
	{0x2e, {23,42}}, {0x26, {24,44}}, {0x1f, {25,45}},
	{0x19, {26,46}}, {0x15, {27,25}}, {0x11, {28,26}},
	{0x0e, {29,26}}, {0x0b, {30,27}}, {0x09, {31,28}},
	{0x08, {32,29}}, {0x07, {33,30}}, {0x05, {34,31}},
	{0x04, {35,33}}, {0x04, {36,33}}, {0x03, {37,34}},
	{0x02, {38,35}}, {0x02, { 5,36}},

	{0x58, {40,39}}, {0x4d, {41,47}}, {0x43, {42,48}},
	{0x3b, {43,49}}, {0x34, {44,50}}, {0x2e, {45,51}},
	{0x29, {46,44}}, {0x25, {24,45}},

	{0x56, {48,47}}, {0x4f, {49,47}}, {0x47, {50,48}},
	{0x41, {51,49}}, {0x3c, {52,50}}, {0x37, {43,51}},
};

typedef struct { UINT8 prediction; UINT8 swap; } DecContext;

static DecContext dec_context[5][15];
static UINT32 dec_bpp;		//bits per pixel (1/2/4)
static UINT32 dec_offset;	//data ROM read offset
static UINT32 dec_bits;		//bits remaining in input
static UINT16 dec_range;	//arithmetic range (Max+1 = 256)
static UINT16 dec_input;	//input data from data ROM
static UINT8  dec_output;
static UINT64 dec_pixels;
static UINT64 dec_colormap;	//most recently used list
static UINT32 dec_result;	//decompressed word after decode()

static UINT8 dec_read()
{
	return datarom_read(dec_offset++);
}

//inverse morton code transform: unpack big-endian packed pixels
//returns odd bits in lower half; even bits in upper half
static UINT64 dec_deinterleave(UINT64 data, UINT32 bits)
{
	data = data & ((1ull << bits) - 1);
	data = 0x5555555555555555ull & (data << bits | data >> 1);
	data = 0x3333333333333333ull & (data         | data >> 1);
	data = 0x0f0f0f0f0f0f0f0full & (data         | data >> 2);
	data = 0x00ff00ff00ff00ffull & (data         | data >> 4);
	data = 0x0000ffff0000ffffull & (data         | data >> 8);
	return data | data >> 16;
}

//extract a nibble and move it to the low four bits
static UINT64 dec_moveToFront(UINT64 list, UINT32 nibble)
{
	UINT64 mask = ~15ull;
	for (UINT64 n = 0; n < 64; n += 4, mask <<= 4) {
		if ((UINT32)(list >> n & 15) != nibble) continue;
		list = (list & mask) + (list << 4 & ~mask) + nibble;
		return list;
	}
	return list;
}

static void dec_initialize(UINT32 mode, UINT32 origin)
{
	for (INT32 r = 0; r < 5; r++) {
		for (INT32 n = 0; n < 15; n++) {
			dec_context[r][n].prediction = 0;
			dec_context[r][n].swap = 0;
		}
	}
	dec_bpp      = 1 << mode;
	dec_offset   = origin;
	dec_bits     = 8;
	dec_range    = DEC_Max + 1;
	dec_input    = dec_read();
	dec_input    = dec_input << 8 | dec_read();
	dec_output   = 0;
	dec_pixels   = 0;
	dec_colormap = 0xfedcba9876543210ull;
}

static void dec_decode()
{
	for (UINT32 pixel = 0; pixel < 8; pixel++) {
		UINT64 map = dec_colormap;
		UINT32 diff = 0;

		if (dec_bpp > 1) {
			UINT32 pa = (dec_bpp == 2 ? (UINT32)(dec_pixels >>  2) & 3 : (UINT32)(dec_pixels >>  0) & 15);
			UINT32 pb = (dec_bpp == 2 ? (UINT32)(dec_pixels >> 14) & 3 : (UINT32)(dec_pixels >> 28) & 15);
			UINT32 pc = (dec_bpp == 2 ? (UINT32)(dec_pixels >> 16) & 3 : (UINT32)(dec_pixels >> 32) & 15);

			if (pa != pb || pb != pc) {
				UINT32 match = pa ^ pb ^ pc;
				diff = 4;							//no match; all pixels differ
				if ((match ^ pc) == 0) diff = 3;	//a == b; pixel c differs
				if ((match ^ pb) == 0) diff = 2;	//c == a; pixel b differs
				if ((match ^ pa) == 0) diff = 1;	//b == c; pixel a differs
			}

			dec_colormap = dec_moveToFront(dec_colormap, pa);

			map = dec_moveToFront(map, pc);
			map = dec_moveToFront(map, pb);
			map = dec_moveToFront(map, pa);
		}

		for (UINT32 plane = 0; plane < dec_bpp; plane++) {
			UINT32 bit     = dec_bpp > 1 ? 1u << plane : 1u << (pixel & 3);
			UINT32 history = (bit - 1) & dec_output;
			UINT32 set     = 0;

			if (dec_bpp == 1) set = pixel >= 4;
			if (dec_bpp == 2) set = diff;
			if (plane >= 2 && history <= 1) set = diff;

			DecContext* ctx = &dec_context[set][bit + history - 1];
			const ModelState* model = &dec_evolution[ctx->prediction];
			UINT8 lps_offset = dec_range - model->probability;
			INT32 symbol = dec_input >= (lps_offset << 8);  //test only the MSB

			dec_output = dec_output << 1 | (symbol ^ ctx->swap);

			if (symbol == DEC_MPS) {			//[0 ... range-p]
				dec_range = lps_offset;			//range = range-p
			} else {							//[range-p+1 ... range]
				dec_range -= lps_offset;		//range = p-1, with p < 0.75
				dec_input -= lps_offset << 8;	//therefore, always rescale
			}

			while (dec_range <= DEC_Max / 2) {	//scale back into [0.75 ... 1.5]
				ctx->prediction = model->next[symbol];

				dec_range <<= 1;
				dec_input <<= 1;

				if (--dec_bits == 0) {
					dec_bits = 8;
					dec_input += dec_read();
				}
			}

			if (symbol == DEC_LPS && model->probability > DEC_Half) ctx->swap ^= 1;
		}

		UINT32 index = dec_output & ((1u << dec_bpp) - 1);
		if (dec_bpp == 1) index ^= (UINT32)(dec_pixels >> 15) & 1;

		dec_pixels = dec_pixels << dec_bpp | (map >> (4 * index) & 15);
	}

	if (dec_bpp == 1) dec_result = dec_pixels;
	if (dec_bpp == 2) dec_result = dec_deinterleave(dec_pixels, 16);
	if (dec_bpp == 4) dec_result = dec_deinterleave(dec_deinterleave(dec_pixels, 32), 32);
}

//=====================
// decompression control unit (dcu)
//=====================

static void dcu_load_address()
{
	UINT32 table = r4801 | r4802 << 8 | r4803 << 16;
	UINT32 index = r4804 << 2;

	UINT32 address = table + index;
	dcuMode     = datarom_read(address + 0) & 3;		//n2
	dcuAddress  = datarom_read(address + 1) << 16;
	dcuAddress |= datarom_read(address + 2) <<  8;
	dcuAddress |= datarom_read(address + 3) <<  0;
	dcuAddress &= 0x7fffff;								//n23
}

static void dcu_begin_transfer()
{
	if (dcuMode == 3) return;	//invalid mode

	dec_initialize(dcuMode, dcuAddress);
	dec_decode();

	UINT16 seek = (r480b & 2) ? (r4805 | r4806 << 8) : 0;
	while (seek--) dec_decode();

	r480c |= 0x80;
	dcuOffset = 0;
}

static UINT8 dcu_read()
{
	if ((r480c & 0x80) == 0) return 0x00;

	if (dcuOffset == 0) {
		for (INT32 row = 0; row < 8; row++) {
			switch (dec_bpp) {
				case 1:
					dcuTile[row] = dec_result;
					break;
				case 2:
					dcuTile[row * 2 +  0] = dec_result >>  0;
					dcuTile[row * 2 +  1] = dec_result >>  8;
					break;
				case 4:
					dcuTile[row * 2 +  0] = dec_result >>  0;
					dcuTile[row * 2 +  1] = dec_result >>  8;
					dcuTile[row * 2 + 16] = dec_result >> 16;
					dcuTile[row * 2 + 17] = dec_result >> 24;
					break;
			}

			UINT8 seek = (r480b & 1) ? r4807 : (UINT8)1;
			while (seek--) dec_decode();
		}
	}

	UINT8 data = dcuTile[dcuOffset++];
	dcuOffset &= 8 * dec_bpp - 1;
	return data;
}

//=====================
// data port unit
//=====================

static UINT32 data_offset() { return r4811 | r4812 << 8 | r4813 << 16; }
static UINT16 data_adjust() { return r4814 | r4815 << 8; }
static UINT16 data_stride() { return r4816 | r4817 << 8; }
static void set_data_offset(UINT32 address) { r4811 = address; r4812 = address >> 8; r4813 = (address >> 16) & 0x7f; }
static void set_data_adjust(UINT16 address) { r4814 = address; r4815 = address >> 8; }

static void data_port_read()
{
	UINT32 offset = data_offset();
	UINT32 adjust = (r4818 & 2) ? data_adjust() : 0;
	if (r4818 & 8) adjust = (INT16)adjust;
	r4810 = datarom_read(offset + adjust);
}

static void data_port_increment_4810()
{
	UINT32 offset = data_offset();
	UINT32 stride = (r4818 & 1) ? data_stride() : 1;
	UINT32 adjust = data_adjust();
	if (r4818 & 4) stride = (INT16)stride;
	if (r4818 & 8) adjust = (INT16)adjust;
	if ((r4818 & 16) == 0) set_data_offset(offset + stride);
	if ((r4818 & 16) != 0) set_data_adjust(adjust + stride);
	data_port_read();
}

static void data_port_increment_4814()
{
	if (r4818 >> 5 != 1) return;
	UINT32 offset = data_offset();
	UINT32 adjust = data_adjust();
	if (r4818 & 8) adjust = (INT16)adjust;
	set_data_offset(offset + adjust);
	data_port_read();
}

static void data_port_increment_4815()
{
	if (r4818 >> 5 != 2) return;
	UINT32 offset = data_offset();
	UINT32 adjust = data_adjust();
	if (r4818 & 8) adjust = (INT16)adjust;
	set_data_offset(offset + adjust);
	data_port_read();
}

static void data_port_increment_481a()
{
	if (r4818 >> 5 != 3) return;
	UINT32 offset = data_offset();
	UINT32 adjust = data_adjust();
	if (r4818 & 8) adjust = (INT16)adjust;
	set_data_offset(offset + adjust);
	data_port_read();
}

//=====================
// arithmetic logic unit
//=====================

static void alu_multiply()
{
	if (r482e & 1) {
		//signed 16-bit x 16-bit multiplication
		INT16 r0 = (INT16)(r4824 | r4825 << 8);
		INT16 r1 = (INT16)(r4820 | r4821 << 8);
		INT32 result = r0 * r1;
		r4828 = result;
		r4829 = result >>  8;
		r482a = result >> 16;
		r482b = result >> 24;
	} else {
		//unsigned 16-bit x 16-bit multiplication
		UINT16 r0 = (UINT16)(r4824 | r4825 << 8);
		UINT16 r1 = (UINT16)(r4820 | r4821 << 8);
		UINT32 result = (UINT32)r0 * r1;  //force unsigned mul (avoid int-promotion signed overflow UB)
		r4828 = result;
		r4829 = result >>  8;
		r482a = result >> 16;
		r482b = result >> 24;
	}
	r482f &= 0x7f;
}

static void alu_divide()
{
	if (r482e & 1) {
		//signed 32-bit / 16-bit division
		INT32 dividend = (INT32)(r4820 | r4821 << 8 | r4822 << 16 | r4823 << 24);
		INT16 divisor  = (INT16)(r4826 | r4827 << 8);
		INT32 quotient;
		INT16 remainder;
		if (divisor == 0) {
			//illegal division by zero
			quotient  = 0;
			remainder = dividend;
		} else if (divisor == -1 && dividend == (INT32)0x80000000) {
			//INT_MIN / -1 overflows a 32-bit signed result and raises #DE on x86;
			//take the two's-complement wrap (quotient = INT_MIN, remainder = 0) instead.
			quotient  = (INT32)0x80000000;
			remainder = 0;
		} else {
			quotient  = (INT32)(dividend / divisor);
			remainder = (INT16)(dividend % divisor);
		}
		r4828 = quotient;
		r4829 = quotient >>  8;
		r482a = quotient >> 16;
		r482b = quotient >> 24;
		r482c = remainder;
		r482d = remainder >> 8;
	} else {
		//unsigned 32-bit / 16-bit division
		UINT32 dividend = (UINT32)(r4820 | r4821 << 8 | r4822 << 16 | r4823 << 24);
		UINT16 divisor  = (UINT16)(r4826 | r4827 << 8);
		UINT32 quotient;
		UINT16 remainder;
		if (divisor) {
			quotient  = (UINT32)(dividend / divisor);
			remainder = (UINT16)(dividend % divisor);
		} else {
			//illegal division by zero
			quotient  = 0;
			remainder = dividend;
		}
		r4828 = quotient;
		r4829 = quotient >>  8;
		r482a = quotient >> 16;
		r482b = quotient >> 24;
		r482c = remainder;
		r482d = remainder >> 8;
	}
	r482f &= 0x7f;
}

//=====================
// register I/O
//=====================

static UINT8 reg_read(UINT32 addr)
{
	switch (addr) {
		//decompression unit
		case 0x4800: {
			UINT16 counter = r4809 | r480a << 8;
			counter--;
			r4809 = counter >> 0;
			r480a = counter >> 8;
			return dcu_read();
		}
		case 0x4801: return r4801;
		case 0x4802: return r4802;
		case 0x4803: return r4803;
		case 0x4804: return r4804;
		case 0x4805: return r4805;
		case 0x4806: return r4806;
		case 0x4807: return r4807;
		case 0x4808: return 0x00;
		case 0x4809: return r4809;
		case 0x480a: return r480a;
		case 0x480b: return r480b;
		case 0x480c: return r480c;

		//data port unit
		case 0x4810: {
			UINT8 data = r4810;
			data_port_increment_4810();
			return data;
		}
		case 0x4811: return r4811;
		case 0x4812: return r4812;
		case 0x4813: return r4813;
		case 0x4814: return r4814;
		case 0x4815: return r4815;
		case 0x4816: return r4816;
		case 0x4817: return r4817;
		case 0x4818: return r4818;
		case 0x481a: {
			data_port_increment_481a();
			return 0x00;
		}

		//arithmetic logic unit
		case 0x4820: return r4820;
		case 0x4821: return r4821;
		case 0x4822: return r4822;
		case 0x4823: return r4823;
		case 0x4824: return r4824;
		case 0x4825: return r4825;
		case 0x4826: return r4826;
		case 0x4827: return r4827;
		case 0x4828: return r4828;
		case 0x4829: return r4829;
		case 0x482a: return r482a;
		case 0x482b: return r482b;
		case 0x482c: return r482c;
		case 0x482d: return r482d;
		case 0x482e: return r482e;
		case 0x482f: return r482f;

		//memory control unit
		case 0x4830: return r4830;
		case 0x4831: return r4831;
		case 0x4832: return r4832;
		case 0x4833: return r4833;
		case 0x4834: return r4834;
	}
	return 0x00;
}

static void reg_write(UINT32 addr, UINT8 data)
{
	switch (addr) {
		//decompression unit
		case 0x4801: r4801 = data; break;
		case 0x4802: r4802 = data; break;
		case 0x4803: r4803 = data & 0x7f; break;
		case 0x4804: r4804 = data; dcu_load_address(); break;
		case 0x4805: r4805 = data; break;
		case 0x4806: r4806 = data; r480c &= 0x7f; dcu_begin_transfer(); break;	//immediate
		case 0x4807: r4807 = data; break;
		case 0x4808: break;
		case 0x4809: r4809 = data; break;
		case 0x480a: r480a = data; break;
		case 0x480b: r480b = data & 0x03; break;

		//data port unit
		case 0x4811: r4811 = data; break;
		case 0x4812: r4812 = data; break;
		case 0x4813: r4813 = data & 0x7f; data_port_read(); break;
		case 0x4814: r4814 = data; data_port_increment_4814(); break;
		case 0x4815: r4815 = data; if (r4818 & 2) data_port_read(); data_port_increment_4815(); break;
		case 0x4816: r4816 = data; break;
		case 0x4817: r4817 = data; break;
		case 0x4818: r4818 = data & 0x7f; data_port_read(); break;

		//arithmetic logic unit
		case 0x4820: r4820 = data; break;
		case 0x4821: r4821 = data; break;
		case 0x4822: r4822 = data; break;
		case 0x4823: r4823 = data; break;
		case 0x4824: r4824 = data; break;
		case 0x4825: r4825 = data; r482f |= 0x81; alu_multiply(); break;		//immediate
		case 0x4826: r4826 = data; break;
		case 0x4827: r4827 = data; r482f |= 0x80; alu_divide(); break;			//immediate
		case 0x482e: r482e = data & 0x01; break;

		//memory control unit
		case 0x4830: r4830 = data & 0x87; break;
		case 0x4831: r4831 = data & 0x07; break;
		case 0x4832: r4832 = data & 0x07; break;
		case 0x4833: r4833 = data & 0x07; break;
		case 0x4834: r4834 = data & 0x07; break;
	}
}

//=====================
// MCU ROM / RAM mapping
//=====================

//map $00-3f,80-bf:8000-ffff and $c0-ff:0000-ffff
static UINT8 mcurom_read(UINT32 addr)
{
	if ((addr & 0x708000) == 0x008000											//$00-0f,80-8f:8000-ffff
	 || (addr & 0xf00000) == 0xc00000) {										//      $c0-cf:0000-ffff
		addr &= 0x0fffff;
		if (s_promSize) return s_prom[bus_mirror(0x000000 + addr, s_promSize)];	//8mbit PROM
		addr |= 0x100000 * (r4830 & 7);
		return datarom_read(addr);
	}

	if ((addr & 0x708000) == 0x108000											//$10-1f,90-9f:8000-ffff
	 || (addr & 0xf00000) == 0xd00000) {										//      $d0-df:0000-ffff
		addr &= 0x0fffff;
		if (r4834 & 4) return s_prom[bus_mirror(0x100000 + addr, s_promSize)];	//16mbit PROM
		addr |= 0x100000 * (r4831 & 7);
		return datarom_read(addr);
	}

	if ((addr & 0x708000) == 0x208000											//$20-2f,a0-af:8000-ffff
	 || (addr & 0xf00000) == 0xe00000) {										//      $e0-ef:0000-ffff
		addr &= 0x0fffff;
		addr |= 0x100000 * (r4832 & 7);
		return datarom_read(addr);
	}

	if ((addr & 0x708000) == 0x308000											//$30-3f,b0-bf:8000-ffff
	 || (addr & 0xf00000) == 0xf00000) {										//      $f0-ff:0000-ffff
		addr &= 0x0fffff;
		addr |= 0x100000 * (r4833 & 7);
		return datarom_read(addr);
	}

	return 0x00;
}

//map $00-3f,80-bf:6000-7fff -> save SRAM
static UINT8 mcuram_read(UINT32 addr)
{
	if (r4830 & 0x80) {
		UINT32 bank = (addr >> 16) & 0x3f;
		UINT32 a = bus_mirror(bank * 0x2000 + (addr & 0x1fff), s_ramSize);
		return s_ram ? s_ram[a] : 0x00;
	}
	return 0x00;
}

static void mcuram_write(UINT32 addr, UINT8 data)
{
	if (r4830 & 0x80) {
		UINT32 bank = (addr >> 16) & 0x3f;
		UINT32 a = bus_mirror(bank * 0x2000 + (addr & 0x1fff), s_ramSize);
		if (s_ram) s_ram[a] = data;
	}
}

//=====================
// cart bus entry points
//=====================

UINT8 snes_spc7110_cart_read(UINT32 addr, UINT8 openbus)
{
	UINT8 bank = addr >> 16;
	UINT16 off = addr & 0xffff;

	//fast decompression read ports: $50:xxxx -> $4800, $58:xxxx -> $4808
	if (bank == 0x50) return reg_read(0x4800);
	if (bank == 0x58) return reg_read(0x4808);

	//expansion ROM (EXSPC7110-RAM-EPSONRTC board): $40-4f:0000-ffff maps a flat
	//1MB window directly onto the bus, bypassing the SPC7110 decode logic.
	if (s_erom && bank >= 0x40 && bank <= 0x4f) {
		UINT32 eoff = ((UINT32)(bank - 0x40) << 16) | off;
		return s_erom[bus_mirror(eoff, s_eromSize)];
	}

	if (bank <= 0x3f || (bank >= 0x80 && bank <= 0xbf)) {
		if (off >= 0x4800 && off <= 0x483f) return reg_read(0x4800 | (off & 0x3f));
		if (s_hasRTC && off >= 0x4840 && off <= 0x4842) return snes_epsonrtc_read(off & 3);
		if (off >= 0x6000 && off <= 0x7fff) return mcuram_read(addr);
		if (off >= 0x8000) return mcurom_read(addr);
		return openbus;
	}

	if (bank >= 0xc0) return mcurom_read(addr);  //$c0-ff:0000-ffff

	return openbus;
}

void snes_spc7110_cart_write(UINT32 addr, UINT8 data)
{
	UINT8 bank = addr >> 16;
	UINT16 off = addr & 0xffff;

	if (bank <= 0x3f || (bank >= 0x80 && bank <= 0xbf)) {
		if (off >= 0x4800 && off <= 0x483f) { reg_write(0x4800 | (off & 0x3f), data); return; }
		if (s_hasRTC && off >= 0x4840 && off <= 0x4842) { snes_epsonrtc_write(off & 3, data); return; }
		if (off >= 0x6000 && off <= 0x7fff) { mcuram_write(addr, data); return; }
	}
}

void snes_spc7110_init_default_ram()
{
//	memset(s_ram + 0x000, 0xff, 0x2000);
//	memset(s_ram + 0x020, 0x00, 0x0004);
//	memset(s_ram + 0x100, 0x00, 0x0100);
	s_ram[0x1ff0] = 0x53;
	s_ram[0x1ff1] = 0x50;
	s_ram[0x1ff2] = 0x43;
	s_ram[0x1ff3] = 0x37;
	s_ram[0x1ff4] = 0x31;
	s_ram[0x1ff5] = 0x31;
	s_ram[0x1ff6] = 0x30;
	s_ram[0x1ff7] = 0x20;
	s_ram[0x1ff8] = 0x43;
	s_ram[0x1ff9] = 0x48;
	s_ram[0x1ffa] = 0x45;
	s_ram[0x1ffb] = 0x43;
	s_ram[0x1ffc] = 0x4b;
	s_ram[0x1ffd] = 0x20;
	s_ram[0x1ffe] = 0x4f;
	s_ram[0x1fff] = 0x4b;
}

//=====================
// init / reset / state
//=====================

void snes_spc7110_init(UINT8* rom, INT32 romSize, UINT8* ram, INT32 ramSize, INT32 promSize, INT32 eromSize, INT32 hasRTC)
{
	// ROM image = [ program ROM (promSize) | data ROM | expansion ROM (eromSize) ]
	// The expansion ROM (EXSPC7110 board) is the trailing chunk; it is mapped to the
	// bus directly at $40-4f and is NOT part of the SPC7110 data ROM.
	if (promSize < 0) promSize = 0;
	if (promSize > romSize) promSize = romSize;
	if (eromSize < 0) eromSize = 0;
	if (eromSize > romSize - promSize) eromSize = romSize - promSize;
	s_prom = rom;
	s_promSize = (UINT32)promSize;
	s_drom = rom + promSize;
	s_dromSize = (UINT32)(romSize - promSize - eromSize);
	s_erom = eromSize ? (rom + promSize + s_dromSize) : NULL;
	s_eromSize = (UINT32)eromSize;
	s_ram  = ram;
	s_ramSize  = (UINT32)ramSize;
	s_hasRTC = hasRTC;

	// prefill battery ram to skip first boot steps
	// if a saved battery ram exists, it'll override this one when the scan function is called later
	snes_spc7110_init_default_ram();

	// The Epson RTC is coin-cell backed: power it on (clear + seed the clock)
	// exactly once per cartridge insertion.  cart_reset() re-runs init on every
	// console reset, but the calendar must survive those, so guard on s_rtcPowered.
	if (hasRTC && !s_rtcPowered) {
		snes_epsonrtc_power();
		s_rtcPowered = 1;
	}
}

void snes_spc7110_exit()
{
	s_prom = s_drom = s_erom = s_ram = NULL;
	s_promSize = s_dromSize = s_eromSize = s_ramSize = 0;
	s_hasRTC = 0;
	s_rtcPowered = 0;  // cartridge removed: next insertion cold-powers the RTC again
}

void snes_spc7110_reset()
{
	r4801 = r4802 = r4803 = r4804 = r4805 = r4806 = r4807 = 0;
	r4809 = r480a = r480b = r480c = 0;
	dcuPending = 0; dcuMode = 0; dcuAddress = 0; dcuOffset = 0;
	memset(dcuTile, 0, sizeof(dcuTile));

	r4810 = r4811 = r4812 = r4813 = r4814 = r4815 = r4816 = r4817 = r4818 = r481a = 0;

	r4820 = r4821 = r4822 = r4823 = r4824 = r4825 = r4826 = r4827 = 0;
	r4828 = r4829 = r482a = r482b = r482c = r482d = r482e = r482f = 0;

	r4830 = 0x00;
	r4831 = 0x00;
	r4832 = 0x01;
	r4833 = 0x02;
	r4834 = 0x00;

	// decompressor
	memset(dec_context, 0, sizeof(dec_context));
	dec_bpp = 0; dec_offset = 0; dec_bits = 0; dec_range = 0; dec_input = 0;
	dec_output = 0; dec_pixels = 0; dec_colormap = 0; dec_result = 0;

	if (s_hasRTC) snes_epsonrtc_reset();
}

void snes_spc7110_handleState(StateHandler* sh)
{
	sh_handleBytes(sh,
		&r4801, &r4802, &r4803, &r4804, &r4805, &r4806, &r4807,
		&r4809, &r480a, &r480b, &r480c,
		&dcuPending, &dcuMode,
		&r4810, &r4811, &r4812, &r4813, &r4814, &r4815, &r4816, &r4817, &r4818, &r481a,
		&r4820, &r4821, &r4822, &r4823, &r4824, &r4825, &r4826, &r4827,
		&r4828, &r4829, &r482a, &r482b, &r482c, &r482d, &r482e, &r482f,
		&r4830, &r4831, &r4832, &r4833, &r4834,
		&dec_output, NULL);

	sh_handleInts(sh, &dcuAddress, &dcuOffset, &dec_bpp, &dec_offset, &dec_bits, &dec_result, NULL);
	sh_handleWords(sh, &dec_range, &dec_input, NULL);
	sh_handleLongLongs(sh, &dec_pixels, &dec_colormap, NULL);
	sh_handleByteArray(sh, dcuTile, sizeof(dcuTile));
	sh_handleByteArray(sh, (UINT8*)dec_context, sizeof(dec_context));

	if (s_hasRTC) snes_epsonrtc_handleState(sh);
}
