// SDD-1 Decoder by Andreas Naive (Public Domain)
// Mapper, everything else by dink

#include "snes.h"
#include "sdd1.h"

static uint8_t *rom;
static int32_t rom_mask;
static uint8_t *sram;
static int32_t sram_mask;

static uint8_t dma_enable[2];
static uint8_t banks[4];

static uint32_t dma_address[8];
static uint16_t dma_size[8];
static bool dma_initxfer;

static uint8_t rom_read(uint32_t address) {
	return rom[(banks[(address >> 20) & 3] << 20) + (address & 0x0fffff)];
}

#define set_byte(var, data, offset) (var = (var & (~(0xff << (((offset) & 3) * 8)))) | (data << (((offset) & 3) * 8)))

struct SDD1_IM {
	uint32_t byte_address;
	uint8_t bit_count;

	void init()
	{
		byte_address = 0;
		bit_count = 0;
	}

	void prepareDecomp(uint32_t in_buf)
	{
		byte_address = in_buf;
		bit_count = 4;
	}

	uint8_t getCodeword(const uint8_t code_len)
	{
		uint8_t codeword = rom_read(byte_address) << bit_count;

		++bit_count;

		if (codeword & 0x80) {
			codeword |= rom_read(byte_address + 1) >> (9 - bit_count);
			bit_count += code_len;
		}

		if (bit_count & 0x08) {
			byte_address++;
			bit_count &= 0x07;
		}

		return codeword;
	}
};

static SDD1_IM IM;

struct SDD1_GCD {
	void GCD_getRunCount(uint8_t code_num, uint8_t* MPScount, uint8_t* LPSind)
	{
		const uint8_t run_count[] =
		{
			0x00, 0x00, 0x01, 0x00, 0x03, 0x01, 0x02, 0x00,
			0x07, 0x03, 0x05, 0x01, 0x06, 0x02, 0x04, 0x00,
			0x0f, 0x07, 0x0b, 0x03, 0x0d, 0x05, 0x09, 0x01,
			0x0e, 0x06, 0x0a, 0x02, 0x0c, 0x04, 0x08, 0x00,
			0x1f, 0x0f, 0x17, 0x07, 0x1b, 0x0b, 0x13, 0x03,
			0x1d, 0x0d, 0x15, 0x05, 0x19, 0x09, 0x11, 0x01,
			0x1e, 0x0e, 0x16, 0x06, 0x1a, 0x0a, 0x12, 0x02,
			0x1c, 0x0c, 0x14, 0x04, 0x18, 0x08, 0x10, 0x00,
			0x3f, 0x1f, 0x2f, 0x0f, 0x37, 0x17, 0x27, 0x07,
			0x3b, 0x1b, 0x2b, 0x0b, 0x33, 0x13, 0x23, 0x03,
			0x3d, 0x1d, 0x2d, 0x0d, 0x35, 0x15, 0x25, 0x05,
			0x39, 0x19, 0x29, 0x09, 0x31, 0x11, 0x21, 0x01,
			0x3e, 0x1e, 0x2e, 0x0e, 0x36, 0x16, 0x26, 0x06,
			0x3a, 0x1a, 0x2a, 0x0a, 0x32, 0x12, 0x22, 0x02,
			0x3c, 0x1c, 0x2c, 0x0c, 0x34, 0x14, 0x24, 0x04,
			0x38, 0x18, 0x28, 0x08, 0x30, 0x10, 0x20, 0x00,
			0x7f, 0x3f, 0x5f, 0x1f, 0x6f, 0x2f, 0x4f, 0x0f,
			0x77, 0x37, 0x57, 0x17, 0x67, 0x27, 0x47, 0x07,
			0x7b, 0x3b, 0x5b, 0x1b, 0x6b, 0x2b, 0x4b, 0x0b,
			0x73, 0x33, 0x53, 0x13, 0x63, 0x23, 0x43, 0x03,
			0x7d, 0x3d, 0x5d, 0x1d, 0x6d, 0x2d, 0x4d, 0x0d,
			0x75, 0x35, 0x55, 0x15, 0x65, 0x25, 0x45, 0x05,
			0x79, 0x39, 0x59, 0x19, 0x69, 0x29, 0x49, 0x09,
			0x71, 0x31, 0x51, 0x11, 0x61, 0x21, 0x41, 0x01,
			0x7e, 0x3e, 0x5e, 0x1e, 0x6e, 0x2e, 0x4e, 0x0e,
			0x76, 0x36, 0x56, 0x16, 0x66, 0x26, 0x46, 0x06,
			0x7a, 0x3a, 0x5a, 0x1a, 0x6a, 0x2a, 0x4a, 0x0a,
			0x72, 0x32, 0x52, 0x12, 0x62, 0x22, 0x42, 0x02,
			0x7c, 0x3c, 0x5c, 0x1c, 0x6c, 0x2c, 0x4c, 0x0c,
			0x74, 0x34, 0x54, 0x14, 0x64, 0x24, 0x44, 0x04,
			0x78, 0x38, 0x58, 0x18, 0x68, 0x28, 0x48, 0x08,
			0x70, 0x30, 0x50, 0x10, 0x60, 0x20, 0x40, 0x00,
		};

		uint8_t codeword = IM.getCodeword(code_num);

		if (codeword & 0x80) {
			*LPSind = 1;
			*MPScount = run_count[codeword >> (code_num ^ 0x07)];
		} else {
			*MPScount = (1 << code_num);
		}
	}
};

static SDD1_GCD GCD;

struct SDD1_BG {
	uint8_t code_num;
	uint8_t MPScount;
	uint8_t LPSind;

	void init(int code)
	{
		code_num = code;
		MPScount = 0;
		LPSind = 0;
	}

	void prepareDecomp()
	{
		MPScount = 0;
		LPSind = 0;
	}

	uint8_t getBit(uint8_t* endOfRun)
	{
		uint8_t bit;

		if (!(MPScount || LPSind)) {
			GCD.GCD_getRunCount(code_num, &MPScount, &LPSind);
		}

		if (MPScount) {
			bit = 0;
			MPScount--;
		} else {
			bit = 1;
			LPSind = 0;
		}

		if (MPScount || LPSind)	{
			(*endOfRun) = 0;
		} else {
			(*endOfRun) = 1;
		}

		return bit;
	}
};

static SDD1_BG BG[8];

struct SDD1_PEM_state
{
	uint8_t code_num;
	uint8_t nextIfMPS;
	uint8_t nextIfLPS;
};

static const SDD1_PEM_state PEM_evolution_table[33] =
{
	{ 0,25,25},
	{ 0, 2, 1},
	{ 0, 3, 1},
	{ 0, 4, 2},
	{ 0, 5, 3},
	{ 1, 6, 4},
	{ 1, 7, 5},
	{ 1, 8, 6},
	{ 1, 9, 7},
	{ 2,10, 8},
	{ 2,11, 9},
	{ 2,12,10},
	{ 2,13,11},
	{ 3,14,12},
	{ 3,15,13},
	{ 3,16,14},
	{ 3,17,15},
	{ 4,18,16},
	{ 4,19,17},
	{ 5,20,18},
	{ 5,21,19},
	{ 6,22,20},
	{ 6,23,21},
	{ 7,24,22},
	{ 7,24,23},
	{ 0,26, 1},
	{ 1,27, 2},
	{ 2,28, 4},
	{ 3,29, 8},
	{ 4,30,12},
	{ 5,31,16},
	{ 6,32,18},
	{ 7,24,22}
};

struct SDD1_PEM_ContextInfo
{
	uint8_t status;
	uint8_t MPS;
};

struct SDD1_PEM {
	SDD1_PEM_ContextInfo contextInfo[32];

	void prepareDecomp()
	{
		for (int i = 0; i < 32; i++)
		{
			contextInfo[i].status = 0;
			contextInfo[i].MPS = 0;
		}
	}

	uint8_t getBit(uint8_t context)
	{
		uint8_t endOfRun;
		uint8_t bit;

		SDD1_PEM_ContextInfo *pContInfo = &(contextInfo)[context];
		uint8_t currStatus = pContInfo->status;
		const SDD1_PEM_state* pState = &(PEM_evolution_table[currStatus]);
		uint8_t currentMPS = pContInfo->MPS;

		bit = BG[pState->code_num].getBit(&endOfRun);

		if (endOfRun) {
			if (bit) {
				if (!(currStatus & 0xfe)) {
					(pContInfo->MPS) ^= 0x01;
				}
				pContInfo->status = pState->nextIfLPS;
			} else {
				pContInfo->status = pState->nextIfMPS;
			}
		}

		return bit ^ currentMPS;
	}
};

static SDD1_PEM PEM;

struct SDD1_CM {
	uint8_t bitplanesInfo;
	uint8_t contextBitsInfo;
	uint8_t bit_number;
	uint8_t currBitplane;
	uint16_t prevBitplaneBits[8];

	void init()
	{
		bitplanesInfo = 0;
		contextBitsInfo = 0;
		bit_number = 0;
		currBitplane = 0;
		for (int i = 0; i < 8; i++) {
			prevBitplaneBits[i];
		}
	}

	void prepareDecomp(uint32_t first_byte)
	{
		bitplanesInfo = rom_read(first_byte) & 0xc0;
		contextBitsInfo = rom_read(first_byte) & 0x30;
		bit_number = 0;
		for (int i = 0; i < 8; i++)	{
			prevBitplaneBits[i] = 0;
		}
		switch (bitplanesInfo) {
			case 0x00:
				currBitplane = 1;
				break;
			case 0x40:
				currBitplane = 7;
				break;
			case 0x80:
				currBitplane = 3;
				break;
		}
	}

	uint8_t getBit()
	{
		uint8_t currContext;
		uint16_t *context_bits;

		switch (bitplanesInfo) {
			case 0x00:
				currBitplane ^= 0x01;
			break;
			case 0x40:
				currBitplane ^= 0x01;
				if (!(bit_number & 0x7f))
					currBitplane = ((currBitplane + 2) & 0x07);
				break;
			case 0x80:
				currBitplane ^= 0x01;
				if (!(bit_number & 0x7f))
				currBitplane ^= 0x02;
				break;
			case 0xc0:
				currBitplane = bit_number & 0x07;
				break;
		}

		context_bits = &(prevBitplaneBits)[currBitplane];

		currContext = (currBitplane & 0x01) << 4;
		switch (contextBitsInfo) {
			case 0x00:
				currContext |= ((*context_bits & 0x01c0) >> 5) | (*context_bits & 0x0001);
				break;
			case 0x10:
				currContext |= ((*context_bits & 0x0180) >> 5) | (*context_bits & 0x0001);
				break;
			case 0x20:
				currContext |= ((*context_bits & 0x00c0) >> 5) | (*context_bits & 0x0001);
				break;
			case 0x30:
				currContext |= ((*context_bits & 0x0180) >> 5) | (*context_bits & 0x0003);
				break;
		}

		uint8_t bit = PEM.getBit(currContext);

		*context_bits <<= 1;
		*context_bits |= bit;

		bit_number++;

		return bit;
	}
};

static SDD1_CM CM;

struct SDD1_OL {
	uint8_t bitplanesInfo;
	uint16_t length;
	uint8_t i;
	uint8_t register1;
	uint8_t register2;

	void init()
	{
		bitplanesInfo = 0;
		length = 0;
		i = 0;
		register1 = 0;
		register2 = 0;
	}

	void prepareDecomp(uint32_t first_byte, uint16_t out_len)
	{
		bitplanesInfo = rom_read(first_byte) & 0xc0;
		length = out_len;

		i = 1;
		register1 = 0;
		register2 = 0;
	}

	uint16_t Get8Bits() // hi byte = status, 0x100 = end of transfer
	{
		if (length == 0) return 0x100;

		switch (bitplanesInfo)
		{
			case 0x00:
			case 0x40:
			case 0x80:
				if (!i) {
					i = ~i;
					length--;
					return register2 | (length == 0 ? 0x100 : 0);
				} else {
					for (register1 = register2 = 0, i = 0x80; i; i >>= 1) {
						if (CM.getBit())
							register1 |= i;

						if (CM.getBit())
							register2 |= i;
					}
					length--;
					return register1 | (length == 0 ? 0x100 : 0);
				}
				break;
			case 0xc0:
				for (register1 = 0, i = 0x01; i; i <<= 1) {
					if (CM.getBit())
						register1 |= i;
				}
				length--;
				return register1 | (length == 0 ? 0x100 : 0);
		}
		return 0x100;
	}
};

static SDD1_OL OL;

static void SDD1emu_begin_decompress(uint32_t start_address, uint16_t out_len)
{
	IM.prepareDecomp(start_address);
	BG[0].prepareDecomp();
	BG[1].prepareDecomp();
	BG[2].prepareDecomp();
	BG[3].prepareDecomp();
	BG[4].prepareDecomp();
	BG[5].prepareDecomp();
	BG[6].prepareDecomp();
	BG[7].prepareDecomp();
	PEM.prepareDecomp();
	CM.prepareDecomp(start_address);
	OL.prepareDecomp(start_address, out_len);
}

void snes_sdd1_init(uint8_t *s_rom, int32_t rom_size, void *s_ram, int32_t sram_size)
{
	rom = s_rom;
	rom_mask = rom_size - 1;
	bprintf(0, _T("sdd1 rom mask %x\n"), rom_mask);

	sram_mask = (sram_mask != 0) ? (sram_size - 1) : 0;
	sram = (uint8_t*)s_ram;
}

void snes_sdd1_exit()
{
}

void snes_sdd1_reset()
{
	dma_enable[0] = dma_enable[1] = 0;

	banks[0] = 0;
	banks[1] = 1;
	banks[2] = 2;
	banks[3] = 3;

	IM.init();
	CM.init();
	OL.init();

	for (int i = 0; i < 8; i++) {
		BG[i].init(i);

		dma_address[i] = 0;
		dma_size[i] = 0;
	}
}

void snes_sdd1_handleState(StateHandler* sh) {
	sh_handleBools(sh, &dma_initxfer, NULL);
	sh_handleBytes(sh, &dma_enable[0], &dma_enable[1], &banks[0], &banks[1], &banks[2], &banks[3], NULL);
	sh_handleWords(sh, &dma_size[0], &dma_size[1], &dma_size[2], &dma_size[3], &dma_size[4], &dma_size[5], &dma_size[6], &dma_size[7], NULL);
	sh_handleInts(sh, &dma_address[0], &dma_address[1], &dma_address[2], &dma_address[3], &dma_address[4], &dma_address[5], &dma_address[6], &dma_address[7], NULL);

	sh_handleBytes(sh, &BG[0].code_num, &BG[0].MPScount, &BG[0].LPSind, &BG[1].code_num, &BG[1].MPScount, &BG[1].LPSind, &BG[2].code_num, &BG[2].MPScount, &BG[2].LPSind, &BG[3].code_num, &BG[3].MPScount, &BG[3].LPSind, NULL);
	sh_handleBytes(sh, &BG[4].code_num, &BG[4].MPScount, &BG[4].LPSind, &BG[5].code_num, &BG[5].MPScount, &BG[5].LPSind, &BG[6].code_num, &BG[6].MPScount, &BG[6].LPSind, &BG[7].code_num, &BG[7].MPScount, &BG[7].LPSind, NULL);

	sh_handleBytes(sh, &IM.bit_count, &CM.bitplanesInfo, &CM.contextBitsInfo, &CM.bit_number, &CM.currBitplane, &OL.bitplanesInfo, &OL.i, &OL.register1, &OL.register2, NULL);
	sh_handleWords(sh, &CM.prevBitplaneBits[0], &CM.prevBitplaneBits[1], &CM.prevBitplaneBits[2], &CM.prevBitplaneBits[3], &CM.prevBitplaneBits[4], &CM.prevBitplaneBits[5], &CM.prevBitplaneBits[6], &CM.prevBitplaneBits[7], &OL.length, NULL);
	sh_handleInts(sh, &IM.byte_address, NULL);

	sh_handleByteArray(sh, (uint8_t*)&PEM.contextInfo, sizeof(PEM.contextInfo));
}

static uint8_t sdd1_mapper_read(uint32_t address)
{
	switch (address) {
		case 0x4804:
		case 0x4805:
		case 0x4806:
		case 0x4807:
			return banks[address & 3];
	}

	return 0xff;
}


static void sdd1_mapper_write(uint32_t address, uint8_t data)
{
	switch (address) {
		case 0x4800:
		case 0x4801:
			dma_enable[address & 1] = data;
			break;

		case 0x4804:
		case 0x4805:
		case 0x4806:
		case 0x4807:
			banks[address & 3] = data & 0xf;
			break;
	}

	if (address >= 0x4300 && address <= 0x437f) {
		switch(address & 0xf) {
			case 2:
			case 3:
			case 4:
				set_byte(dma_address[(address >> 4) & 7], data, (address & 0xf) - 2);
				break;

			case 5:
			case 6:
				set_byte(dma_size[(address >> 4) & 7], data, (address & 0xf) - 5);
				break;
		}
	}
}

static uint8_t sdd1_bankedarea_dma_read(uint32_t address)
{
	const uint8_t dma_active = dma_enable[0] & dma_enable[1];

	if (dma_active) {
		for (int i = 0; i < 8; i++) {
			if (dma_active & (1 << i) && address == dma_address[i]) {
				if (dma_initxfer == false) {
					//bprintf(0, _T("dma, r: %x  dma_addr[%x]: %x  size: %x\n"), address, i, dma_address[i], dma_size[i]);
					SDD1emu_begin_decompress(address, dma_size[i]);
					dma_initxfer = true;
				}

				uint16_t data = OL.Get8Bits();

				if (data & 0x100) { // EOT
					dma_initxfer = false;
					dma_enable[1] &= ~(1 << i);
				}

				return data & 0xff;
			}
		}
	}

	return rom_read(address);
}

uint8_t snes_sdd1_cart_read(uint32_t address)
{
	const uint32_t bank = (address & 0xff0000) >> 16;

	if ((bank & 0xc0) == 0xc0) {
		return sdd1_bankedarea_dma_read(address);
	}

	if ( (bank & 0x7f) < 0x40 && (address & 0xf000) == 0x4000 )	{
		return sdd1_mapper_read(address & 0xffff);
	}

	if ( (bank & 0x7f) < 0x40 && (address & 0x8000) ) { // LoROM accessor
		return rom[(((bank & 0x7f) << 15) + (address & 0x7fff)) & rom_mask];
	}

	if ( (bank >= 0x70 && bank <= 0x73) || ((bank & 0x7f) <= 0x3f && (address & 0xe000) == 0x6000) ) {
		if (sram_mask) {
			return sram[address & sram_mask];
		}
	}

	bprintf(0, _T("sdd1 - unmap_read: %x\n"), address);

	return 0xff;
}

void snes_sdd1_cart_write(uint32_t address, uint8_t data)
{
	const uint32_t bank = (address & 0xff0000) >> 16;

	if ( (bank & 0x7f) < 0x40 && (address & 0xf000) == 0x4000 ) {
		sdd1_mapper_write(address & 0xffff, data);
		return;
	}

	if ( (bank >= 0x70 && bank <= 0x73) || ((bank & 0x7f) <= 0x3f && (address & 0xe000) == 0x6000) ) {
		if (sram_mask) {
			sram[address & sram_mask] = data;
		}
		return;
	}
}
