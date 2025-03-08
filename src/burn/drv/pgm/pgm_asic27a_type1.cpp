#include "pgm.h"
#include "bitswap.h"

/*
	IGS Asic27a (type 1) proper emulation

	Used by:
		Knights of Valour Superheroes
		Photo Y2K
		Knights of Valour Super Heroes Plus (Using a small hack)
		Various Knights of Valour/Knights of Valour Super Heroes/Knights of Valour Super Heroes Plus bootlegs

	Simulations

	Used by:
		Oriental Legends Super Plus
		-----------------------------------------------------------
		DoDonPachi Dai-Ou-Jou / DoDonPachi Dai-Ou-Jou Black Label
		Ketsui Kizuna Jigoku Tachi
		Espgaluda
		-----------------------------------------------------------
		Knights of Valour
		-----------------------------------------------------------
		Puzzli 2
		-----------------------------------------------------------
		Photo Y2K2
		-----------------------------------------------------------
		Puzzle Star - NOT WORKING!
*/

static UINT16 highlatch_to_arm;
static UINT16 lowlatch_to_arm;
static UINT16 highlatch_to_68k;
static UINT16 lowlatch_to_68k;

static UINT32 arm_counter;

static inline void pgm_cpu_sync()
{
	while (SekTotalCycles() > Arm7TotalCycles())
		Arm7Run(SekTotalCycles() - Arm7TotalCycles());
}

static void __fastcall kovsh_asic27a_write_word(UINT32 address, UINT16 data)
{
	switch (address & 0x6)
	{
		case 0:
			lowlatch_to_arm = data;
		return;

		case 2:
			highlatch_to_arm = data;
		return;
		
		case 4:
			// this MUST be written to before writing a new command (at least on a bootleg)
		return;
	}
}

static UINT16 __fastcall kovsh_asic27a_read_word(UINT32 address)
{
	if ((address & 0xffffe0) == 0x4f0000) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(PGMARMShareRAM + (address & 0x1e))));
	}

	switch (address & 0x6)
	{
		case 0:
			pgm_cpu_sync();
			return lowlatch_to_68k;

		case 2:
			pgm_cpu_sync();
			return highlatch_to_68k;
	}

	return 0;
}

static void kovsh_asic27a_arm7_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffff80) == 0x50800000) {	// written and never read
		*((UINT16*)(PGMARMShareRAM + ((address>>1) & 0x1e))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}
}

static void kovsh_asic27a_arm7_write_long(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x40000000:
		{
			highlatch_to_68k = data >> 16;
			lowlatch_to_68k = data;
			lowlatch_to_arm = 0;
			highlatch_to_arm = 0;
		}
		return;
	}
}

static UINT32 kovsh_asic27a_arm7_read_long(UINT32 address)
{
	switch (address)
	{
		case 0x40000000:
			return (highlatch_to_arm << 16) | lowlatch_to_arm;

		case 0x4000000c:
			return arm_counter++;
	}

	return 0;
}

static void reset_kovsh_asic27a()
{
	highlatch_to_arm = 0;
	lowlatch_to_arm = 0;
	highlatch_to_68k = 0;
	lowlatch_to_68k = 0;

	arm_counter = 1;
}

static INT32 kovsh_asic27aScan(INT32 nAction, INT32 *)
{
	struct BurnArea ba;

	if (nAction & ACB_MEMORY_RAM) {
		ba.Data		= PGMARMShareRAM;
		ba.nLen		= 0x0000040;
		ba.nAddress	= 0x400000;
		ba.szName	= "ARM SHARE RAM";
		BurnAcb(&ba);

		ba.Data		= PGMARMRAM0;
		ba.nLen		= 0x0000400;
		ba.nAddress	= 0;
		ba.szName	= "ARM RAM 0";
		BurnAcb(&ba);

		ba.Data		= PGMARMRAM2;
		ba.nLen		= 0x0000400;
		ba.nAddress	= 0;
		ba.szName	= "ARM RAM 1";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		Arm7Scan(nAction);

		SCAN_VAR(highlatch_to_arm);
		SCAN_VAR(lowlatch_to_arm);
		SCAN_VAR(highlatch_to_68k);
		SCAN_VAR(lowlatch_to_68k);

		SCAN_VAR(arm_counter);
	}

 	return 0;
}

void install_protection_asic27a_kovsh()
{
	pPgmScanCallback = kovsh_asic27aScan;
	pPgmResetCallback = reset_kovsh_asic27a;

	SekOpen(0);
	SekMapMemory(PGMARMShareRAM,	0x4f0000, 0x4f001f|0x3ff, MAP_ROM); // This is a 32 byte overlay at 4f0000, it is NOT WRITEABLE! (68k Page size in FBN is 1024 byte)

	SekMapHandler(4,				0x500000, 0x600005, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 		kovsh_asic27a_read_word);
	SekSetWriteWordHandler(4, 		kovsh_asic27a_write_word);
	SekClose();

	Arm7Init(0);
	Arm7Open(0);
	Arm7MapMemory(PGMARMROM,		0x00000000, 0x00003fff, MAP_ROM);
//	encrypted 68k rom is mapped		0x08100000, 0x083fffff
	Arm7MapMemory(PGMARMRAM0,		0x10000000, 0x100003ff, MAP_RAM);
	Arm7MapMemory(PGMARMRAM2,		0x50000000, 0x500003ff, MAP_RAM);
	Arm7SetWriteWordHandler(kovsh_asic27a_arm7_write_word);
	Arm7SetWriteLongHandler(kovsh_asic27a_arm7_write_long);
	Arm7SetReadLongHandler(kovsh_asic27a_arm7_read_long);
	Arm7Close();
}

/*
	The internal ARM7 ROM hash checks either the 68K ROM (if there is no External ARM7 ROM) 
	or the external ROM to ensure that it not been modified. If the hash check fails,
	the internal ARM7 writes junk to the encryption xor table.

struct define_hash
{
	unsigned int start_address;
	unsigned short add_or_xor;
	unsigned int length;
};

// kovsh - 68K ROM - checked while encrypted
static define_hash hash_regions_kovsh[0x0d] = {	// table in asic @ $2144
	{ 0x00000, 0x0002, 0x1000 },
	{ 0x48000, 0x0001, 0x8000 },
	{ 0x58000, 0x0002, 0x8000 },
	{ 0x68000, 0x0001, 0x8000 },
	{ 0x78000, 0x0002, 0x8000 },
	{ 0x88000, 0x0001, 0x8000 },
	{ 0x98000, 0x0002, 0x8000 },
	{ 0xa8000, 0x0001, 0x8000 },
	{ 0xb8000, 0x0002, 0x8000 },
	{ 0xc8000, 0x0001, 0x8000 },
	{ 0xd8000, 0x0002, 0x4000 },
	{ 0xe8000, 0x0001, 0x3000 },
	{ 0x39abe, 0, 0 }	// address to find stored hash
};

// photoy2k - 68K ROM - checked while encrypted
static define_hash hash_regions_photoy2k[0x11] = { // table in asic @ 17c0
	{ 0x010000, 0x0001, 0x5000 },
	{ 0x030000, 0x0002, 0x5000 },
	{ 0x050000, 0x0001, 0x5000 },
	{ 0x070000, 0x0002, 0x5000 },
	{ 0x090000, 0x0001, 0x5000 },
	{ 0x0b0000, 0x0002, 0x5000 },
	{ 0x0d0000, 0x0001, 0x5000 },
	{ 0x0f0000, 0x0002, 0x5000 },
	{ 0x110000, 0x0001, 0x5000 },
	{ 0x130000, 0x0002, 0x5000 },
	{ 0x150000, 0x0001, 0x5000 },
	{ 0x170000, 0x0002, 0x5000 },
	{ 0x190000, 0x0001, 0x5000 },
	{ 0x1b0000, 0x0002, 0x5000 },
	{ 0x1d0000, 0x0001, 0x5000 },
	{ 0x1e0000, 0x0002, 0x5000 },
	{ 0x1f0000, 0, 0 } // address to find stored hash
};

// martmast - External ARM7 ROM - checked after decryption
static define_hash hash_regions_martmast[16+1] = { // table in asic @ 3508
	{ 0x000000, 0x0001, 0x0500 },
	{ 0x020000, 0x0002, 0x0500 },
	{ 0x040000, 0x0001, 0x0500 },
	{ 0x060000, 0x0002, 0x0500 },
	{ 0x080000, 0x0001, 0x0500 },
	{ 0x0a0000, 0x0002, 0x0500 },
	{ 0x0c0000, 0x0001, 0x0500 },
	{ 0x0e0000, 0x0002, 0x0500 },
	{ 0x100000, 0x0001, 0x0500 },
	{ 0x110000, 0x0002, 0x0500 },
	{ 0x120000, 0x0001, 0x0500 },
	{ 0x130000, 0x0002, 0x0500 },
	{ 0x140000, 0x0001, 0x0500 },
	{ 0x150000, 0x0002, 0x0500 },
	{ 0x160000, 0x0001, 0x0500 },
	{ 0x170000, 0x0002, 0x0500 },
	{ 0x1fff00, 0, 0 }		// address to find stored hash
};

// ddp2 - checked after decryption
static define_hash hash_regions_ddp2[1 + 1] = {
	{ 0x00000, 0x0001, 0x1fff8 },
	{ 0x1fffc, 0, 0 }
};

// kov2 & kov2p - checked after decryption
static define_hash hash_regions_kov2[1 + 1] = {
	{ 0x000000, 0x0001, 0x1ffffc },
	{ 0x1ffffc, 0, 0 }
};

//	These do not do hash checks
//		Dragon World 2001
//		Dragon World Pretty Chance

void verify_hash_function(unsigned char *src, define_hash *ptr)
{
	int i, j;
	unsigned short shash = 0, hash = 0, value, *rom = (unsigned short*)src;

	while (1)
	{
		for (j = 0, shash = 0; j < ptr->length; j+=2)
		{
			value = rom[(ptr->start_address + j)/2];
			shash = (ptr->add_or_xor == 1) ? (shash + value) : (shash ^ value);
		}
		ptr++;
		hash += shash;
		if (ptr->length == 0) break;
	}
	
	printf ("Calculated: %4.4x, Correct: %4.4x\n", hash, rom[ptr->start_address/2]);
}
*/

//-------------------------------
// Knights of Valour Super Heroes Plus Hack

static void __fastcall kovshp_asic27a_write_word(UINT32 address, UINT16 data)
{
	switch (address & 6)
	{
		case 0:
			lowlatch_to_arm = data;
		return;

		case 2:
		{
			unsigned char asic_key = data >> 8;
			unsigned char asic_cmd = (data & 0xff) ^ asic_key;

		//	bprintf (0, _T("Command: %2.2x, data: %4.4x, key: %2.2x PC(%5.5x)\n"), asic_cmd, lowlatch_to_arm ^ (asic_key) ^ (asic_key << 8), asic_key, SekGetPC(-1));

			switch (asic_cmd) // Intercept commands and translate them to those used by kovsh
			{
			// kovassga
				case 0x9a: asic_cmd = 0x99; break;
				case 0xa6: asic_cmd = 0xa9; break;
				case 0xaa: asic_cmd = 0x56; break;
				case 0xf8: asic_cmd = 0xf3; break;

			// kovshp
				case 0x38: asic_cmd = 0xad; break;
				case 0x43: asic_cmd = 0xca; break;
				case 0x56: asic_cmd = 0xac; break;
				case 0x73: asic_cmd = 0x93; break;
				case 0x84: asic_cmd = 0xb3; break;
				case 0x87: asic_cmd = 0xb1; break;
				case 0x89: asic_cmd = 0xb6; break;
				case 0x93: asic_cmd = 0x73; break;
				case 0xa5: asic_cmd = 0xa9; break;
				case 0xac: asic_cmd = 0x56; break;
				case 0xad: asic_cmd = 0x38; break;
				case 0xb1: asic_cmd = 0x87; break;
				case 0xb3: asic_cmd = 0x84; break;
				case 0xb4: asic_cmd = 0x90; break;
				case 0xb6: asic_cmd = 0x89; break;
				case 0xc5: asic_cmd = 0x8c; break;
				case 0xca: asic_cmd = 0x43; break;
				case 0xcc: asic_cmd = 0xf0; break;
				case 0xd0: asic_cmd = 0xe0; break;
				case 0xe0: asic_cmd = 0xd0; break;
				case 0xe7: asic_cmd = 0x70; break;
				case 0xed: asic_cmd = 0xcb; break;
				case 0xf0: asic_cmd = 0xcc; break;
				case 0xf1: asic_cmd = 0xf5; break;
				case 0xf2: asic_cmd = 0xf1; break;
				case 0xf4: asic_cmd = 0xf2; break;
				case 0xf5: asic_cmd = 0xf4; break;
				case 0xfc: asic_cmd = 0xc0; break;
				case 0xfe: asic_cmd = 0xc3; break;
			}

			highlatch_to_arm = asic_cmd ^ (asic_key | (asic_key << 8));
		}
		return;
		
		case 4: return;
	}
}

void install_protection_asic27a_kovshp()
{
	pPgmScanCallback = kovsh_asic27aScan;

	SekOpen(0);
	SekMapMemory(PGMARMShareRAM,	0x4f0000, 0x4f001f | 0x3ff, MAP_ROM);

	SekMapHandler(4,				0x500000, 0x600005, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 		kovsh_asic27a_read_word);
	SekSetWriteWordHandler(4, 		kovshp_asic27a_write_word);
	SekClose();

	Arm7Init(0);
	Arm7Open(0);
	Arm7MapMemory(PGMARMROM,	0x00000000, 0x00003fff, MAP_ROM);
	Arm7MapMemory(PGMARMRAM0,	0x10000000, 0x100003ff, MAP_RAM);
	Arm7MapMemory(PGMARMRAM2,	0x50000000, 0x500003ff, MAP_RAM);
	Arm7SetWriteWordHandler(kovsh_asic27a_arm7_write_word);
	Arm7SetWriteLongHandler(kovsh_asic27a_arm7_write_long);
	Arm7SetReadLongHandler(kovsh_asic27a_arm7_read_long);
	Arm7Close();
}


//-------------------------------
// Preliminary kovgsyx Hack


static void __fastcall kovgsyx_asic27a_write_word(UINT32 address, UINT16 data)
{
	static const UINT8 gsyx_cmd_lut[0x100] = { // 00 is unknown
		0x00, 0x25, 0x40, 0x13, 0x51, 0x56, 0x5e, 0x24, 0x24, 0x93, 0x50, 0x75, 0x3e, 0x68, 0x46, 0x46,
		0x75, 0x24, 0x58, 0x6a, 0x60, 0x68, 0x40, 0x13, 0x93, 0x24, 0x59, 0xca, 0x4a, 0x93, 0x60, 0x58,	
		0x68, 0x7d, 0x40, 0x68, 0x72, 0x51, 0x75, 0x13, 0x11, 0x3e, 0x93, 0x51, 0x58, 0x35, 0x72, 0x51,	
		0x62, 0x51, 0x4d, 0x7d, 0xca, 0x59, 0x7d, 0x24, 0x5e, 0x7d, 0x5e, 0x46, 0x72, 0x13, 0x4a, 0x11,
		0x60, 0x3e, 0x62, 0x53, 0x6a, 0x7d, 0x36, 0x6a, 0x36, 0x53, 0x53, 0x6a, 0x53, 0x11, 0x5e, 0xca,
		0x68, 0x5d, 0x35, 0x25, 0x4a, 0x36, 0xca, 0x60, 0x50, 0x6c, 0x59, 0x46, 0x36, 0x5d, 0x5e, 0x36,
		0x72, 0x51, 0x62, 0x56, 0x68, 0x72, 0x6a, 0x40, 0x72, 0xca, 0x58, 0x62, 0x75, 0xca, 0x6f, 0x53,
		0x75, 0x35, 0x25, 0x13, 0x53, 0x46, 0xad, 0x58, 0x60, 0x60, 0x62, 0x5e, 0x7d, 0x58, 0xac, 0x6a,
		0x00, 0x00, 0x00, 0xb5, 0x00, 0xe0, 0x89, 0xb1, 0x73, 0xf3, 0xe0, 0xcc, 0xf5, 0xd4, 0x8a, 0xc3,
		0xc3, 0x00, 0x00, 0x00, 0xec, 0xa9, 0x00, 0x89, 0x00, 0xcc, 0xa2, 0xab, 0xb1, 0xa2, 0x00, 0x00,
		0x00, 0x00, 0x00, 0xd0, 0x00, 0xf2, 0x84, 0xa1, 0xdc, 0x73, 0x00, 0xdc, 0xdc, 0x38, 0x00, 0xcc,
		0x73, 0x00, 0xd0, 0xc0, 0xcb, 0x00, 0xc3, 0x89, 0x00, 0x73, 0x00, 0xe0, 0x00, 0x89, 0xc0, 0xd0,
		0x00, 0xe0, 0x00, 0xcb, 0xdc, 0xcc, 0xcc, 0xdc, 0xc3, 0x84, 0xe0, 0xb5, 0xa2, 0x90, 0x00, 0xcb,
		0xf1, 0x00, 0xa1, 0xc0, 0xb5, 0xb5, 0xc0, 0x89, 0xcb, 0xa1, 0x00, 0xcc, 0x00, 0x00, 0x00, 0xc0,
		0x00, 0x89, 0xf4, 0xa2, 0x00, 0xcb, 0x00, 0xd0, 0xc9, 0x00, 0xa2, 0xcd, 0x00, 0xe2, 0xcb, 0x99,
		0xf0, 0x90, 0xc0, 0xa1, 0xa1, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, 0x70, 0x00, 0xc3, 0xc3, 0x00
	};

	switch (address & 6)
	{
		case 0:
			lowlatch_to_arm = data;
		return;

		case 2:
		{
			unsigned char asic_key = data >> 8;
			unsigned char asic_cmd = (data & 0xff) ^ asic_key;

			if (gsyx_cmd_lut[asic_cmd]) asic_cmd = gsyx_cmd_lut[asic_cmd];

#if 0
			UINT16 hack_data = lowlatch_to_arm ^ asic_key ^ (asic_key << 8);

			switch (asic_cmd)
			{
				case 0xb5:	// verified on hardware
				{
					char s[16] = { 'W', 'D', 'F', '*', 'Z', 'S', 'C', 'S', '-', '0', '4', '5', '9', 0x16, 0, 0 };
					response = (unsigned char)s[hack_data & 0xf];
				}
				break;
			
				case 0xbc:	// verified on hardware
				{
					char s[16] = { 'V', '3', '0', '0', 'C', 'N', 0xf3, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
					response = (unsigned char)s[hack_data & 0xf];
				}
				break;
			
				case 0xe9:	// verified on hardware
				{
					char s[16] = { '2', '0', '1', '1', '-', '0', '4', '-', '2', '8', 0xb7, 0, 0, 0, 0, 0 };
					response = (unsigned char)s[hack_data & 0xf];
				}
			}
#endif

			highlatch_to_arm = asic_cmd ^ (asic_key | (asic_key << 8));
		}
		return;
	}
}

void install_protection_asic27a_kovgsyx()
{
	pPgmScanCallback = kovsh_asic27aScan;

	// asic responds 880000 as default response, this expects 910000, hack it out in the program
	{
		UINT16 *rom = (UINT16*)PGM68KROM;

		rom[0x9b32c/2] = 0x0088;
		rom[0x9b550/2] = 0x0088;
	}

	SekOpen(0);
	SekMapMemory(PGMARMShareRAM,		0x4f0000, 0x4f003f, MAP_RAM);
	SekMapMemory(PGM68KROM + 0x300000,	0x500000, 0x5fffff, MAP_ROM); // mirror last 1mb
	SekMapMemory(PGM68KROM + 0x300000,	0x600000, 0x6fffff, MAP_ROM); // mirror last 1mb
	SekMapHandler(4,					0xd00000, 0xd00005, MAP_READ | MAP_WRITE);
	SekMapMemory(PGM68KROM + 0x300000,	0xf00000, 0xffffff, MAP_ROM); // mirror last 1mb
	SekSetReadWordHandler(4, 		kovsh_asic27a_read_word);
	SekSetWriteWordHandler(4, 		kovgsyx_asic27a_write_word);
	SekClose();

	Arm7Init(0);
	Arm7Open(0);
	Arm7MapMemory(PGMARMROM,	0x00000000, 0x00003fff, MAP_ROM);
	Arm7MapMemory(PGMARMRAM0,	0x10000000, 0x100003ff, MAP_RAM);
	Arm7MapMemory(PGMARMRAM2,	0x50000000, 0x500003ff, MAP_RAM);
	Arm7SetWriteWordHandler(kovsh_asic27a_arm7_write_word);
	Arm7SetWriteLongHandler(kovsh_asic27a_arm7_write_long);
	Arm7SetReadLongHandler(kovsh_asic27a_arm7_read_long);
	Arm7Close();
}


//------------------------------------------
// Common asic27a (type 1) simulation functions


static UINT16 asic27a_sim_value;
static UINT16 asic27a_sim_key;
static UINT32 asic27a_sim_response;
static UINT32 asic27a_sim_slots[0x100];
static UINT16 asic27a_sim_regs[0x100];
static UINT8  asic27a_sim_internal_slot;

static void (*asic27a_sim_command)(UINT8);

static void __fastcall asic27a_sim_write(UINT32 offset, UINT16 data)
{
	switch (offset & 0x06)
	{
		case 0: asic27a_sim_value = data; return;

		case 2:
		{
			if ((data >> 8) == 0xff) asic27a_sim_key = 0xffff;

			asic27a_sim_value ^= asic27a_sim_key;

			UINT8 command = (data ^ asic27a_sim_key) & 0xff;

			asic27a_sim_regs[command] = asic27a_sim_value;

			//bprintf (0, _T("Command: %2.2x, Data: %2.2x\n"), command, asic27a_sim_value);

			asic27a_sim_command(command);

			asic27a_sim_key = (asic27a_sim_key + 0x0100) & 0xff00;
			if (asic27a_sim_key == 0xff00) asic27a_sim_key = 0x0100;
			asic27a_sim_key |= asic27a_sim_key >> 8;
		}
		return;

		case 4: return; // cannot start a new command without writing to this first (data & 1) == 0!
	}
}

static UINT16 __fastcall asic27a_sim_read(UINT32 offset)
{
	switch (offset & 0x02)
	{
		case 0: return (asic27a_sim_response >>  0) ^ asic27a_sim_key;
		case 2: return (asic27a_sim_response >> 16) ^ asic27a_sim_key;
	}

	return 0;
}

static void asic27a_sim_reset()
{
	// The ASIC27a writes this to shared RAM
	UINT8 ram_string[16] = {
		'I', 'G', 'S', 'P', 'G', 'M', 0, 0, 0, 0/*REGION*/, 'C', 'H', 'I', 'N', 'A', 0
	};

	memset (PGMUSER0, 0, 0x400);

	ram_string[9] = PgmInput[7]; // region

	memcpy (PGMUSER0, ram_string, 16);

	BurnByteswap(PGMUSER0, 0x10);

	memset (asic27a_sim_slots, 0, 0x100 * sizeof(INT32));
	memset (asic27a_sim_regs,  0, 0x100 * sizeof(INT16));

	asic27a_sim_value = 0;
	asic27a_sim_key = 0;
	asic27a_sim_response = 0;
	asic27a_sim_internal_slot = 0;
}

static INT32 asic27a_sim_scan(INT32 nAction, INT32 *)
{
	struct BurnArea ba;

	if (nAction & ACB_MEMORY_RAM) {
		ba.Data		= (UINT8*)asic27a_sim_slots;
		ba.nLen		= 0x0000100 * sizeof(INT32);
		ba.nAddress	= 0xff00000;
		ba.szName	= "ASIC27a Slots";
		BurnAcb(&ba);

		ba.Data		= (UINT8*)asic27a_sim_regs;
		ba.nLen		= 0x0000100 * sizeof(INT16);
		ba.nAddress	= 0xff01000;
		ba.szName	= "ASIC27a Regs";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(asic27a_sim_value);
		SCAN_VAR(asic27a_sim_key);
		SCAN_VAR(asic27a_sim_response);
		SCAN_VAR(asic27a_sim_internal_slot);
	}

	return 0;
}


//---------------------------------
// Simulation used by DoDonPachi Dai-Ou-Jou, Ketsui Kizuna Jigoku Tachi, and Espgaluda

static void ddp3_asic27a_sim_command(UINT8 command)
{
	switch (command)
	{
		case 0x40: // Combine slot values
			asic27a_sim_slots[(asic27a_sim_value >> 10) & 0x1f] = (asic27a_sim_slots[(asic27a_sim_value >> 5) & 0x1f] + asic27a_sim_slots[(asic27a_sim_value >> 0) & 0x1f]) & 0xffffff;
			asic27a_sim_response = 0x880000;
		break;

		case 0x67: // Select slot & write (high)
			asic27a_sim_internal_slot = asic27a_sim_value >> 8;
			asic27a_sim_slots[asic27a_sim_internal_slot] = (asic27a_sim_value & 0x00ff) << 16;
			asic27a_sim_response = 0x880000;
		break;

		case 0xe5: // Write slot (low)
			asic27a_sim_slots[asic27a_sim_internal_slot] |= asic27a_sim_value;
			asic27a_sim_response = 0x880000;
		break;

		case 0x8e: // Read slot
			asic27a_sim_response = asic27a_sim_slots[asic27a_sim_value & 0xff];
		break;

		case 0x99: // Reset?
			asic27a_sim_key = 0;
			asic27a_sim_response = 0x880000 | (PgmInput[7] << 8);
		break;

		default:
			asic27a_sim_response = 0x880000;
		break;
	}
}

void install_protection_asic27a_ketsui()
{
	pPgmResetCallback = asic27a_sim_reset;
	pPgmScanCallback = asic27a_sim_scan;
	asic27a_sim_command = ddp3_asic27a_sim_command;

	SekOpen(0);
	SekMapHandler(4,			0x400000, 0x400005, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 	asic27a_sim_read);
	SekSetWriteWordHandler(4, 	asic27a_sim_write);
	SekClose();
}

void install_protection_asic27a_ddp3()
{
	pPgmResetCallback = asic27a_sim_reset;
	pPgmScanCallback = asic27a_sim_scan;
	asic27a_sim_command = ddp3_asic27a_sim_command;

	SekOpen(0);
	SekMapHandler(4,			0x500000, 0x500005, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 	asic27a_sim_read);
	SekSetWriteWordHandler(4, 	asic27a_sim_write);
	SekClose();
}


//--------------------------
// Simulation for Oriental Legends Super Plus

static const UINT8 oldsplus_fc[0x20]={
	0x00,0x00,0x0a,0x3a,0x4e,0x2e,0x03,0x40,0x33,0x43,0x26,0x2c,0x00,0x00,0x00,0x00,
	0x00,0x00,0x44,0x4d,0x0b,0x27,0x3d,0x0f,0x37,0x2b,0x02,0x2f,0x15,0x45,0x0e,0x30
};

static const UINT16 oldsplus_90[0x7]={
	0x50,0xa0,0xc8,0xf0,0x190,0x1f4,0x258
};

static const UINT8 oldsplus_5e[0x20]={
	0x04,0x04,0x04,0x04,0x04,0x03,0x03,0x03,0x02,0x02,0x02,0x01,0x01,0x01,0x01,0x01,
	0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const UINT8 oldsplus_b0[0xe0]={
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,
	0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
	0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,
	0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,
	0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
	0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
	0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,
	0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x1f,0x1f,0x1f,0x1f
};

static const UINT8 oldsplus_ae[0xe0]={
	0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
	0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
	0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
	0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,
	0x1E,0x1F,0x20,0x21,0x22,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
	0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
	0x1F,0x20,0x21,0x22,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
	0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
	0x20,0x21,0x22,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
	0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
	0x21,0x22,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
	0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
	0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,
	0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23
};

static const UINT16 oldsplus_ba[0x4]={
	0x3138,0x2328,0x1C20,0x1518
};

static const UINT16 oldsplus_8c[0x20]={
	0x0032,0x0032,0x0064,0x0096,0x0096,0x00fa,0x012c,0x015e,0x0032,0x0064,0x0096,0x00c8,0x00c8,0x012c,0x015e,0x0190,
	0x0064,0x0096,0x00c8,0x00fa,0x00fa,0x015e,0x0190,0x01c2,0x0096,0x00c8,0x00fa,0x012c,0x012c,0x0190,0x01c2,0x01f4
};

static inline UINT16 oldsplus_9d(UINT16 a)
{
	const UINT8 tab[8] = { 0x3c, 0x46, 0x5a, 0x6e, 0x8c, 0xc8, 0x50, };

	if ((a % 0x27) <= 0x07) return (a % 0x27) * 0x64;
	if ((a % 0x27) >= 0x17) return 0x6bc;
	
	return 0x2bc + (tab[a / 0x27] * ((a % 0x27) - 7));
}

static void oldsplus_asic27a_sim_command(UINT8 command)
{
	switch (command)
	{
		case 0x88: // Reset?
			asic27a_sim_key = 0;
			asic27a_sim_response = 0x990000 | (PgmInput[7] << 8);
		break;

		case 0xa0:
			asic27a_sim_response = ((asic27a_sim_value >= 0x0f) ? 0x0f : asic27a_sim_value) * 0x23;
		break;

		case 0xd0: // Text palette offset
			asic27a_sim_response = 0xa01000 + (asic27a_sim_value << 5);
		break;

		case 0xc0: // Sprite palette offset
			asic27a_sim_response = 0xa00000 + (asic27a_sim_value << 6);
		break;

		case 0xc3: // Background palette offset
			asic27a_sim_response = 0xa00800 + (asic27a_sim_value << 6);
		break;

		case 0x33: // Store regs
			asic27a_sim_response = 0x990000;
		break;

		case 0x35: // Add '36' reg
			asic27a_sim_regs[0x36] += asic27a_sim_value;
			asic27a_sim_response = 0x990000;
		break;

		case 0x36: // Store regs
			asic27a_sim_response = 0x990000;
		break;

		case 0x37: // Add '33' reg
			asic27a_sim_regs[0x33] += asic27a_sim_value;
			asic27a_sim_response = 0x990000;
		break;

		case 0x34: // Read '36' reg
			asic27a_sim_response = asic27a_sim_regs[0x36];
		break;

		case 0x38: // Read '33' reg
			asic27a_sim_response = asic27a_sim_regs[0x33];
		break;

		case 0xe7: // Select slot
		{
			asic27a_sim_response = 0x990000;

			asic27a_sim_internal_slot = (asic27a_sim_value >> 12) & 0x0f;
		}
		break;

		case 0xe5: // Write slot
		{
			asic27a_sim_response = 0x990000;

			asic27a_sim_slots[asic27a_sim_internal_slot] = asic27a_sim_value;

			if (asic27a_sim_internal_slot == 0x0b) asic27a_sim_slots[0xc] = 0; // ??
		}
		break;

		case 0xf8: // Read slot
			asic27a_sim_response = asic27a_sim_slots[asic27a_sim_value];
		break;

		case 0xc5: // Increment slot 'd'
			asic27a_sim_slots[0xd]--;
			asic27a_sim_response = 0x990000;
		break;

		case 0xd6: // Increment slot 'b'
			asic27a_sim_slots[0xb]++;
			asic27a_sim_response = 0x990000;
		break;

		case 0x3a: // Clear slot 'f'
			asic27a_sim_slots[0xf] = 0;
			asic27a_sim_response = 0x990000;
		break;

		case 0xf0: // Background layer 'x' select
			asic27a_sim_response = 0x990000;
		break;

		case 0xed: // Background layer offset
			if (asic27a_sim_value & 0x400) asic27a_sim_value = -(0x400 - (asic27a_sim_value & 0x3ff));
			asic27a_sim_response = 0x900000 + ((asic27a_sim_regs[0xf0] + (asic27a_sim_value * 0x40)) * 4);
		break;

		case 0xe0: // Text layer 'x' select
			asic27a_sim_response = 0x990000;
		break;

		case 0xdc: // Text layer offset
			asic27a_sim_response = 0x904000 + ((asic27a_sim_regs[0xe0] + (asic27a_sim_value * 0x40)) * 4);
		break;

		case 0xcb: // Some sort of status read?
			asic27a_sim_response = 0x00c000;
		break;

		case 0x5e: // Read from data table
			asic27a_sim_response = oldsplus_5e[asic27a_sim_value];
		break;

		case 0x80: // Read from data table
			asic27a_sim_response = (asic27a_sim_value < 4) ? ((asic27a_sim_value + 1) * 0xbb8) : 0xf4240;
		break;

		case 0x8c: // Read from data table
			asic27a_sim_response = oldsplus_8c[asic27a_sim_value];
		break;

		case 0x90: // Read from data table
			asic27a_sim_response = oldsplus_90[asic27a_sim_value];
		break;

		case 0x9d: // Read from data table
			asic27a_sim_response = oldsplus_9d(asic27a_sim_value);
		break;

		case 0xae: // Read from data table
			asic27a_sim_response = oldsplus_ae[asic27a_sim_value];
		break;

		case 0xb0: // Read from data table
			asic27a_sim_response = oldsplus_b0[asic27a_sim_value];
		break;

		case 0xba: // Read from data table
			asic27a_sim_response = oldsplus_ba[asic27a_sim_value];
		break;

		case 0xfc: // Read from data table
			asic27a_sim_response = oldsplus_fc[asic27a_sim_value];
		break;

		default:
			asic27a_sim_response = 0x990000;
		break;
	}
}

void install_protection_asic27a_oldsplus()
{
	pPgmResetCallback = asic27a_sim_reset;
	pPgmScanCallback = asic27a_sim_scan;
	asic27a_sim_command = oldsplus_asic27a_sim_command;

	SekOpen(0);
	SekMapMemory(PGMUSER0,		0x4f0000, 0x4f001f | 0x3ff, MAP_READ); // ram

	SekMapHandler(4,			0x500000, 0x500003, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 	asic27a_sim_read);
	SekSetWriteWordHandler(4, 	asic27a_sim_write);
	SekClose();
}



//-----------------------------------------------------------------------------------------------------
// Simulation used by Knights of Valour


static const UINT8 B0TABLE[8] = { 2, 0, 1, 4, 3, 7, 5, 6 }; // Maps char portraits to tables

static const UINT8 BATABLE[0x40] = {
	0x00,0x29,0x2c,0x35,0x3a,0x41,0x4a,0x4e,0x57,0x5e,0x77,0x79,0x7a,0x7b,0x7c,0x7d,
	0x7e,0x7f,0x82,0x81,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x90,
	0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9e,0xa3,0xd4,0xa9,0xaf,0xb5,0xbb,0xc1,
	0xd1,0xd2,0xd3
};

static void kov_asic27a_sim_command(UINT8 command)
{
    switch (command)
    {
        case 0x67: // unknown or status check?
        case 0x8e:
        case 0xa3:
        case 0x33: // kovsgqyz (a3)
        case 0x3a: // kovplus
            asic27a_sim_response = 0x880000;
            break;
            
        case 0xc5: // kovplus
            asic27a_sim_slots[asic27a_sim_value & 0xf] = asic27a_sim_slots[asic27a_sim_value & 0xf] - 1;
            asic27a_sim_response = 0x880000;
        break;
            
        case 0x99: // Reset
            asic27a_sim_key = 0;
            asic27a_sim_response = 0x880000 | (PgmInput[7] << 8);
        break;
        
        case 0x9d: // Sprite palette offset
            asic27a_sim_response = 0xa00000 + ((asic27a_sim_value & 0x1f) * 0x40);
        break;
            
        case 0xb0: // Read from data table
            asic27a_sim_response = B0TABLE[asic27a_sim_value & 0x0f];
        break;
            
        case 0xb4: // Copy slot 'a' to slot 'b'
        case 0xb7: // kovsgqyz (b4)
        {
            asic27a_sim_slots[(asic27a_sim_value >> 8) & 0x0f] = asic27a_sim_slots[(asic27a_sim_value >> 4) & 0x0f] + asic27a_sim_slots[(asic27a_sim_value >> 0) & 0x0f];
            asic27a_sim_response = 0x880000;
        }
        break;
            
        case 0xba: // Read from data table
            asic27a_sim_response = BATABLE[asic27a_sim_value & 0x3f];
        break;
            
        case 0xc0: // Text layer 'x' select
            asic27a_sim_response = 0x880000;
        break;
            
        case 0xc3: // Text layer offset
            asic27a_sim_response = 0x904000 + ((asic27a_sim_regs[0xc0] + (asic27a_sim_value * 0x40)) * 4);
        break;
            
        case 0xcb: // Background layer 'x' select
            asic27a_sim_response = 0x880000;
        break;
            
        case 0xcc: // Background layer offset
        {
            INT32 y = asic27a_sim_value;
            if (0xf < y) y = y & 0xf;
            if (y & 0x400) y = -(0x400 - (y & 0x3ff));
            asic27a_sim_response = 0x900000 + (((asic27a_sim_regs[0xcb] + y * 64) * 4));
        }
        break;
            
        case 0xd0: // Text palette offset
        case 0xcd: // kovsgqyz (d0)
            asic27a_sim_response = 0xa01000 + (asic27a_sim_value * 0x20);
        break;
            
        case 0xd6: // Copy slot to slot 0
            asic27a_sim_slots[asic27a_sim_value & 0xf] = asic27a_sim_slots[asic27a_sim_value & 0xf] + 1;
            asic27a_sim_response = 0x880000;
        break;
            
        case 0xdc: // Background palette offset
        case 0x11: // kovsgqyz (dc)
            asic27a_sim_response = 0xa00800 + (asic27a_sim_value * 0x40);
        break;
            
        case 0xe0: // Sprite palette offset
        case 0x9e: // kovsgqyz (e0)
            asic27a_sim_response = 0xa00000 + ((asic27a_sim_value & 0x1f) * 0x40);
        break;
            
        case 0xe5: // Write slot (low)
        {
            asic27a_sim_slots[asic27a_sim_internal_slot] = (asic27a_sim_slots[asic27a_sim_internal_slot] & 0x00ff0000) | ((asic27a_sim_value & 0xffff) <<  0);
            asic27a_sim_response = 0x880000;
        }
        break;
            
        case 0xe7: // Write slot (and slot select) (high)
        {
            asic27a_sim_internal_slot = (asic27a_sim_value >> 12) & 0x0f;

            asic27a_sim_slots[asic27a_sim_internal_slot] = (asic27a_sim_slots[asic27a_sim_internal_slot] & 0x0000ffff) | ((asic27a_sim_value & 0x00ff) << 16);
            asic27a_sim_response = 0x880000;
        }
        break;
            
        case 0xf0: // Some sort of status read?
            asic27a_sim_response = 0x00c000;
        break;
            
        case 0xf8: // Read slot
        case 0xab: // kovsgqyz (f8)
            asic27a_sim_response = asic27a_sim_slots[asic27a_sim_value & 0x0f] & 0x00ffffff;
        break;

        case 0xfc: // Adjust damage level to char experience level
            asic27a_sim_response = (asic27a_sim_value * asic27a_sim_regs[0xfe]) >> 6;
        break;
            
        case 0xfe: // Damage level adjust
            asic27a_sim_response = 0x880000;
        break;

        default:
            asic27a_sim_response = 0x880000;
        break;
    }
}

void install_protection_asic27_kov()
{
	pPgmResetCallback = asic27a_sim_reset;
	pPgmScanCallback = asic27a_sim_scan;
	asic27a_sim_command = kov_asic27a_sim_command;

	SekOpen(0);
	SekMapMemory(PGMUSER0,		0x4f0000, 0x4f001f | 0x3ff, MAP_READ);

	SekMapHandler(4,			0x500000, 0x500003, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 	asic27a_sim_read);
	SekSetWriteWordHandler(4, 	asic27a_sim_write);
	SekClose();
}


//--------------------
// Simulation used by Puzzli 2

static int stage;
static int tableoffs;
static int tableoffs2;
static int entries_left;
static int currentcolumn;
static int currentrow;
static int num_entries;
static int full_entry;
static int prev_tablloc;
static int numbercolumns;
static int depth;
static UINT16 m_row_bitmask;
static int hackcount;
static int hackcount2;
static int hack_47_value;
static int hack_31_table_offset;
static int p2_31_retcounter;
static int command_31_write_type;
static UINT16 level_structure[8][10];

static const UINT8 puzzli2_level_decode[256] = {
	0x32, 0x3e, 0xb2, 0x37, 0x31, 0x22, 0xd6, 0x0d, 0x35, 0x5c, 0x8d, 0x3c, 0x7a, 0x5f, 0xd7, 0xac, // 0x0
	0x53, 0xff, 0xeb, 0x44, 0xe8, 0x11, 0x69, 0x77, 0xd9, 0x34, 0x36, 0x45, 0xa6, 0xe9, 0x1c, 0xc6, // 0x1
	0x3b, 0xbd, 0xad, 0x2e, 0x18, 0xdf, 0xa1, 0xab, 0xdd, 0x52, 0x57, 0xc2, 0xe5, 0x0a, 0x00, 0x6d, // 0x2
 	0x67, 0x64, 0x15, 0x70, 0xb6, 0x39, 0x27, 0x78, 0x82, 0xd2, 0x71, 0xb9, 0x13, 0xf5, 0x93, 0x92, // 0x3
	0xfa, 0xe7, 0x5e, 0xb0, 0xf6, 0xaf, 0x95, 0x8a, 0x7c, 0x73, 0xf9, 0x63, 0x86, 0xcb, 0x1a, 0x56, // 0x4
	0xf1, 0x3a, 0xae, 0x61, 0x01, 0x29, 0x97, 0x23, 0x8e, 0x5d, 0x9a, 0x65, 0x74, 0x21, 0x20, 0x40, // 0x5
	0xd3, 0x05, 0xa2, 0xe1, 0xbc, 0x9e, 0x1e, 0x10, 0x14, 0x0c, 0x88, 0x9c, 0xec, 0x38, 0xb5, 0x9d, // 0x6
	0x2d, 0xf7, 0x17, 0x0e, 0x84, 0xc7, 0x7d, 0xce, 0x94, 0x16, 0x48, 0xa8, 0x81, 0x6e, 0x7b, 0xd8, // 0x7
	0xa7, 0x7f, 0x42, 0xe6, 0xa0, 0x2a, 0xef, 0xee, 0x24, 0xba, 0xb8, 0x7e, 0xc9, 0x2b, 0x90, 0xcc, // 0x8
	0x5b, 0xd1, 0xf3, 0xe2, 0x6f, 0xed, 0x9f, 0xf0, 0x4b, 0x54, 0x8c, 0x08, 0xf8, 0x51, 0x68, 0xc8, // 0x9
	0x03, 0x0b, 0xbb, 0xc1, 0xe3, 0x4d, 0x04, 0xc5, 0x8f, 0x09, 0x0f, 0xbf, 0x62, 0x49, 0x76, 0x59, // 0xa
	0x1d, 0x80, 0xde, 0x60, 0x07, 0xe0, 0x1b, 0x66, 0xa5, 0xbe, 0xcd, 0x87, 0xdc, 0xc3, 0x6b, 0x4e, // 0xb
	0xd0, 0xfd, 0xd4, 0x3f, 0x98, 0x96, 0x2f, 0x4c, 0xb3, 0xea, 0x2c, 0x75, 0xe4, 0xc0, 0x6c, 0x6a, // 0xc
	0x9b, 0xb7, 0x43, 0x8b, 0x41, 0x47, 0x02, 0xdb, 0x99, 0x3d, 0xa3, 0x79, 0x50, 0x4f, 0xb4, 0x55, // 0xd
	0x5a, 0x25, 0xf4, 0xca, 0x58, 0x30, 0xc4, 0x12, 0xa9, 0x46, 0xda, 0x91, 0xa4, 0xaa, 0xfc, 0x85, // 0xe
	0xfb, 0x89, 0x06, 0xcf, 0xfe, 0x33, 0xd5, 0x28, 0x1f, 0x19, 0x4a, 0xb1, 0x83, 0xf2, 0x72, 0x26, // 0xf
};

static int get_position_of_bit(UINT16 value, int bit_wanted)
{
	int count = 0;
	for (int i=0;i<16;i++)
	{
		int bit = (value >> i) & 1;

		if (bit) count++;

		if (count==(bit_wanted+1))
			return i;
	}

	return -1;
}

static int puzzli2_take_leveldata_value(UINT8 datvalue)
{
	if (stage==-1)
	{
		tableoffs = 0;
		tableoffs2 = 0;
		entries_left = 0;
		currentcolumn = 0;
		currentrow = 0;
		num_entries = 0;
		full_entry = 0;
		prev_tablloc = 0;
		numbercolumns = 0;
		depth = 0;
		m_row_bitmask = 0;

		tableoffs = datvalue;
		tableoffs2 = 0;
		stage = 0;
	}
	else
	{
		UINT8 rawvalue = datvalue;
		UINT8 tableloc = (tableoffs+tableoffs2)&0xff;
		rawvalue ^= puzzli2_level_decode[tableloc];

		tableoffs2++;
		tableoffs2&=0xf;

		if (stage==0)
		{
			stage = 1;
			depth = (rawvalue & 0xf0);
			numbercolumns = (rawvalue & 0x0f);
			numbercolumns++;
		}
		else if (stage==1)
		{
			stage = 2;
			entries_left = (rawvalue >> 4);
			m_row_bitmask = (rawvalue & 0x0f)<<8;
			full_entry = rawvalue;
			prev_tablloc = tableloc;
			num_entries = entries_left;
		}
		else if (stage==2)
		{	
			stage = 3;
			m_row_bitmask |= rawvalue;

			if (entries_left == 0)
			{
				stage = 1;
				currentcolumn++;
				currentrow = 0;
				m_row_bitmask = 0;

				if (currentcolumn==numbercolumns)
				{
					return 1;
				}
			}
		}
		else if (stage==3)
		{
			UINT16 object_value;

			     if (rawvalue<=0x10) object_value = 0x0100 + rawvalue; 
			else if (rawvalue<=0x21) object_value = 0x0120 + (rawvalue - 0x11); 
			else if (rawvalue<=0x32) object_value = 0x0140 + (rawvalue - 0x22); 
			else if (rawvalue<=0x43) object_value = 0x0180 + (rawvalue - 0x33); 
			else if (rawvalue==0xd0) object_value = 0x0200;
			else if (rawvalue==0xe0) object_value = 0x8000;
			else if (rawvalue==0xe1) object_value = 0x8020; // solid slant top down
			else if (rawvalue==0xe2) object_value = 0x8040; // solid slant top up
			else if (rawvalue==0xe3) object_value = 0x8060;
			else if (rawvalue==0xe4) object_value = 0x8080; // sold slant bottom up
			else                     object_value = 0x0110;

			int realrow = get_position_of_bit(m_row_bitmask, currentrow);

			if (realrow != -1)
				level_structure[currentcolumn][realrow] = object_value;

			currentrow++;

			entries_left--;
			if (entries_left == 0)
			{
				stage = 1;
				currentcolumn++;
				currentrow = 0;
				m_row_bitmask = 0;

				if (currentcolumn==numbercolumns)
				{
					return 1;
				}
			}
		}
	}

	return 0;
}
	
static void puzzli2_asic27a_sim_command(UINT8 command)
{
	switch (command)
	{
		case 0x31:
		{
			if (command_31_write_type==2)
			{
				if (hackcount2==0)
				{
					puzzli2_take_leveldata_value(asic27a_sim_value&0xff);

					hack_31_table_offset = asic27a_sim_value & 0xff;
					hackcount2++;
					asic27a_sim_response = 0x00d20000;
				}
				else
				{
					int end = puzzli2_take_leveldata_value(asic27a_sim_value&0xff);

					if (!end)
					{
						asic27a_sim_response = 0x00d20000;
						hackcount2++;
					}
					else
					{
						hackcount2=0;
						asic27a_sim_response = 0x00630000 | numbercolumns;
					}
				}
			}
			else
			{
				asic27a_sim_response = 0x00d20000 | p2_31_retcounter;
				p2_31_retcounter++;
			}
		}
		break;

		case 0x13:
		{
			UINT16* leveldata = &level_structure[0][0];
			if (hackcount==0)
			{
				asic27a_sim_response = 0x002d0000 | ((depth>>4)+1);
			}
			else if (hackcount<((10*numbercolumns)+1))
			{
				asic27a_sim_response = 0x002d0000 | leveldata[hackcount-1];
			}
			else
			{
				hackcount=0;
				asic27a_sim_response = 0x00740054;
			}

			hackcount++;
		}
		break;

		case 0x38: // Reset
		{
			asic27a_sim_response = 0x780000 | (PgmInput[7] << 8);
			asic27a_sim_key = 0x100;
		}	
		break;

		case 0x47:
			hack_47_value = asic27a_sim_value;
			asic27a_sim_response = 0x00740047;
		break;

		case 0x52:
		{
			int val = ((hack_47_value & 0x0f00)>>8) * 0x19;
			if (asic27a_sim_value!=0x0000)
			{
				val +=((hack_47_value & 0x000f)>>0) * 0x05;
			}
			val += asic27a_sim_value & 0x000f;
			asic27a_sim_response = 0x00740000 | (val & 0xffff);
		}
		break;

		case 0x61:
			command_31_write_type = 1;
			asic27a_sim_response = 0x360000;
			p2_31_retcounter = 0xc;
		break;

		case 0x41: // ASIC status?
			command_31_write_type = 0;
			asic27a_sim_response = 0x740061;
		break;

		case 0x54: // ??
		{
			command_31_write_type = 2;
			stage = -1;
			hackcount2 = 0;
			hackcount = 0;
			asic27a_sim_response = 0x360000;
			
			//  clear the return structure
			for (int columns=0;columns<8;columns++)
				for (int rows=0;rows<10;rows++)
					level_structure[columns][rows] = 0x0000;
		}
		break;

		case 0x63:
		{
			UINT32 z80table[2][8] = {
				{ 0x1694a8, 0x16cfae, 0x16ebf2, 0x16faa8, 0x174416, 0x600000, 0x600000, 0x600000 }, // puzzli2
				{ 0x19027a, 0x193D80, 0x1959c4, 0x19687a, 0x19b1e8, 0x600000, 0x600000, 0x600000 }  // puzzli2s
			};

			if (!strcmp(BurnDrvGetTextA(DRV_NAME),"puzzli2"))
			{
				asic27a_sim_response = z80table[0][asic27a_sim_value & 7];
			}
			else {// puzzli2 super
				asic27a_sim_response = z80table[1][asic27a_sim_value & 7];
			}
		}
		break;

		case 0x67:
		{
			UINT32 z80table[2][8] = {
				{ 0x166178, 0x166178, 0x166178, 0x166178, 0x166e72, 0x600000, 0x600000, 0x600000 }, // puzzli2
				{ 0x18cf4a, 0x18cf4a, 0x18cf4a, 0x18cf4a, 0x18dc44, 0x600000, 0x600000, 0x600000 } // puzzli2s
			};

			if (!strcmp(BurnDrvGetTextA(DRV_NAME),"puzzli2"))
			{
				asic27a_sim_response = z80table[0][asic27a_sim_value & 7];
			}
			else {// puzzli2 super
				asic27a_sim_response = z80table[1][asic27a_sim_value & 7];
			}
		}
		break;

		default:
			asic27a_sim_response = 0x740000;
		break;
	}
}

void install_protection_asic27a_puzzli2()
{
	pPgmResetCallback = asic27a_sim_reset;
	pPgmScanCallback = asic27a_sim_scan;
	asic27a_sim_command = puzzli2_asic27a_sim_command;

	SekOpen(0);
	SekMapMemory(PGMUSER0,		0x4f0000, 0x4f003f | 0x3ff, MAP_READ);

	SekMapHandler(4,		0x500000, 0x500003, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 	asic27a_sim_read);
	SekSetWriteWordHandler(4, 	asic27a_sim_write);
	SekClose();
}


//-------------------------------------------------------------------------------------------
// Simulation used by Photo Y2k2

static UINT16 py2k2_sprite_pos = 0;
static UINT16 py2k2_sprite_base = 0;
static INT16 py2k2_sprite_value = 0;        //must INT16 type
static INT16 py2k2_sprite_ba_value = 0;     //must INT16 type

static UINT32 py2k2_sprite_offset(UINT16 base, UINT16 pos)
{
	UINT16 ret = 0;
	UINT16 offset = (base * 16) + (pos & 0xf);

	switch (base & ~0x3f)
	{
		case 0x000: ret = BITSWAP16(offset ^ 0x0030, 15, 14, 13, 12, 11, 10, 0, 2, 3, 9, 5, 4, 8, 7, 6, 1); break;
		case 0x040: ret = BITSWAP16(offset ^ 0x03c0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 1, 2, 0, 5, 3, 4); break;
		case 0x080: ret = BITSWAP16(offset ^ 0x0000, 15, 14, 13, 12, 11, 10, 0, 3, 4, 6, 8, 7, 5, 9, 2, 1); break;
		case 0x0c0: ret = BITSWAP16(offset ^ 0x0001, 15, 14, 13, 12, 11, 10, 6, 5, 4, 3, 2, 1, 9, 8, 7, 0); break;
		case 0x100: ret = BITSWAP16(offset ^ 0x0030, 15, 14, 13, 12, 11, 10, 0, 2, 3, 9, 5, 4, 8, 7, 6, 1); break;
		case 0x140: ret = BITSWAP16(offset ^ 0x01c0, 15, 14, 13, 12, 11, 10, 2, 8, 7, 6, 4, 3, 5, 9, 0, 1); break;
		case 0x180: ret = BITSWAP16(offset ^ 0x0141, 15, 14, 13, 12, 11, 10, 4, 8, 2, 6, 1, 7, 9, 5, 3, 0); break;
		case 0x1c0: ret = BITSWAP16(offset ^ 0x0090, 15, 14, 13, 12, 11, 10, 5, 3, 7, 2, 1, 4, 0, 9, 8, 6); break;
		case 0x200: ret = BITSWAP16(offset ^ 0x02a1, 15, 14, 13, 12, 11, 10, 9, 1, 7, 8, 5, 6, 2, 4, 3, 0); break;
		case 0x240: ret = BITSWAP16(offset ^ 0x0000, 15, 14, 13, 12, 11, 10, 3, 2, 1, 0, 9, 8, 7, 6, 5, 4); break;
		case 0x280: ret = BITSWAP16(offset ^ 0x02a1, 15, 14, 13, 12, 11, 10, 9, 1, 7, 8, 5, 6, 2, 4, 3, 0); break;
		case 0x2c0: ret = BITSWAP16(offset ^ 0x0000, 15, 14, 13, 12, 11, 10, 0, 3, 4, 6, 8, 7, 5, 9, 2, 1); break;
		case 0x300: ret = BITSWAP16(offset ^ 0x03c0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 1, 2, 0, 5, 3, 4); break;
		case 0x340: ret = BITSWAP16(offset ^ 0x0030, 15, 14, 13, 12, 11, 10, 0, 2, 3, 9, 5, 4, 8, 7, 6, 1); break;
		case 0x380: ret = BITSWAP16(offset ^ 0x0001, 15, 14, 13, 12, 11, 10, 6, 5, 4, 3, 2, 1, 9, 8, 7, 0); break;
		case 0x3c0: ret = BITSWAP16(offset ^ 0x0090, 15, 14, 13, 12, 11, 10, 5, 3, 7, 2, 1, 4, 0, 9, 8, 6); break;
		case 0x400: ret = BITSWAP16(offset ^ 0x02a1, 15, 14, 13, 12, 11, 10, 9, 1, 7, 8, 5, 6, 2, 4, 3, 0); break;
		case 0x440: ret = BITSWAP16(offset ^ 0x03c0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 1, 2, 0, 5, 3, 4); break;
		case 0x480: ret = BITSWAP16(offset ^ 0x0141, 15, 14, 13, 12, 11, 10, 4, 8, 2, 6, 1, 7, 9, 5, 3, 0); break;
		case 0x4c0: ret = BITSWAP16(offset ^ 0x01c0, 15, 14, 13, 12, 11, 10, 2, 8, 7, 6, 4, 3, 5, 9, 0, 1); break;
		case 0x500: ret = BITSWAP16(offset ^ 0x0141, 15, 14, 13, 12, 11, 10, 4, 8, 2, 6, 1, 7, 9, 5, 3, 0); break;
		case 0x540: ret = BITSWAP16(offset ^ 0x0030, 15, 14, 13, 12, 11, 10, 0, 2, 3, 9, 5, 4, 8, 7, 6, 1); break;
		case 0x580: ret = BITSWAP16(offset ^ 0x03c0, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 1, 2, 0, 5, 3, 4); break;
		case 0x5c0: ret = BITSWAP16(offset ^ 0x0090, 15, 14, 13, 12, 11, 10, 5, 3, 7, 2, 1, 4, 0, 9, 8, 6); break;
		case 0x600: ret = BITSWAP16(offset ^ 0x0000, 15, 14, 13, 12, 11, 10, 0, 3, 4, 6, 8, 7, 5, 9, 2, 1); break;
		case 0x640: ret = BITSWAP16(offset ^ 0x0000, 15, 14, 13, 12, 11, 10, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5); break;
	}

	if (offset >= 0xce80/2 && offset <= 0xceff/2) ret -= 0x0100;
	if (offset >= 0xcf00/2 && offset <= 0xcf7f/2) ret += 0x0100;

	return ret;
}

static const UINT16 py2k2_40_table[8] = { 0x00e0, 0x00a8, 0x0080, 0x0080, 0x0100, 0x0080, 0x0180, 0x0080 };

static const UINT16 py2k2_4d_table[16] = {
    0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x0000,
    0x0001, 0x000d, 0x000e, 0x0000
};

static const UINT16 py2k2_50_table[16] = {
    0x006c, 0x00a8, 0x0000, 0x0000, 0x0154, 0x0044, 0x0000, 0x0000, 0x006c, 0x0044, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
};

static const UINT16 py2k2_5e_table[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x01a0, 0x0064, 0x0000, 0x0008, 0x0020, 0x0064, 0x0000, 0x0008,
    0x0000, 0x0000, 0x0000, 0x0000
};

static const UINT16 py2k2_60_table[16] = {
    0x0064, 0x00c0, 0x0000, 0x0000, 0x01a0, 0x003c, 0x0000, 0x0000, 0x2000, 0x3c00, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000
};

static const UINT16 py2k2_6a_table[16] = {
    0x00e0, 0x008d, 0x0012, 0x0000, 0x0160, 0x0069, 0x0000, 0x000a, 0x005c, 0x0069, 0x0000, 0x000a,
    0x0000, 0x0000, 0x0000, 0x0000
};

static const UINT16 py2k2_70_table[16] = {
    0x00e0, 0x009c, 0x004a, 0x0000, 0x014a, 0x0069, 0x0000, 0x0020, 0x0076, 0x0069, 0x0000, 0x0020,
    0x0000, 0x0000, 0x0000, 0x0000
};

static const UINT16 py2k2_7b_table[16] = {
    0x00e3, 0x00b6, 0x0012, 0x0000, 0x01a0, 0x0064, 0x0000, 0x0006, 0x0020, 0x0064, 0x0000, 0x0006,
    0x0000, 0x0000, 0x0000, 0x0000
};

static const UINT16 py2k2_80_table[16] = {
    0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x0000,
    0x0001, 0x000d, 0x000e, 0x0000
};

static const UINT16 py2k2_8c_table[16] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x019a, 0x0079, 0x0000, 0x000a, 0x00e2, 0x003c, 0x0016, 0x0000,
    0x002b, 0x0079, 0x0000, 0x000a
};

static const UINT16 py2k2_9d_table[4] = {
    0x00e0, 0x00a0, 0x0000 ,0x0000
};

static const UINT16 py2k2_a0_table[16] = {
    0x0020, 0x00C5, 0x000C, 0x0000, 0x01AC, 0x00C5, 0xFFF4, 0x0000, 0x01AC, 0x0046, 0xFFF4, 0x0000,
    0x0020, 0x0046, 0x000C, 0x0000
};

static const UINT16 py2k2_ae_table[16] = {
    0x00e2, 0x008e, 0x0010, 0x0000, 0x014d, 0x0079, 0x0010, 0x0000, 0x00e2, 0x0065, 0x0010, 0x0000,
    0x0078, 0x0079, 0x0010, 0x0000
};

static const UINT16 py2k2_b0_table[16] = {
    0x00e2, 0x00b6, 0x0016, 0x0000, 0x019a, 0x0079, 0x0000, 0x0006, 0x00e2, 0x003c, 0x0016, 0x0000,
    0x002b, 0x0079, 0x0000, 0x0006
};

static const UINT16 py2k2_ba_table[64] = {
    0x2a, 0x28, 0x26, 0x24, 0x22, 0x1e, 0x1c, 0x1a, 0x16, 0x26, 0x24, 0x22, 0x20, 0x1e, 0x1a, 0x18,
    0x16, 0x14, 0x22, 0x21, 0x20, 0x1e, 0x1c, 0x1a, 0x16, 0x14, 0x14, 0x1e, 0x1e, 0x1e, 0x1c, 0x1a,
    0x18, 0x16, 0x14, 0x14, 0x1e, 0x1e, 0x1c, 0x1a, 0x18, 0x16, 0x14, 0x14, 0x14, 0x00, 0x0a, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void py2k2_asic27a_sim_command(UINT8 command)
{
	switch (command)
	{
		case 0x30: // pgm3in1
            asic27a_sim_response = py2k2_sprite_offset(py2k2_sprite_base, py2k2_sprite_pos++);
		break;

		case 0x32:
            py2k2_sprite_base = asic27a_sim_value;
            py2k2_sprite_pos = 0;
            asic27a_sim_response = py2k2_sprite_offset(py2k2_sprite_base, py2k2_sprite_pos++);
		break;
        
        case 0x3a:
            asic27a_sim_response = 0x880000;
            asic27a_sim_slots[asic27a_sim_value & 0xf] = 0;
        break;

		case 0x40:  // Read from data table
			asic27a_sim_response = py2k2_40_table[asic27a_sim_value & 7];
		break;
            
        case 0x4d:// Read from data table
            asic27a_sim_response = py2k2_4d_table[asic27a_sim_value & 0xf];
        break;
            
        case 0x50:// Read from data table
            asic27a_sim_response = py2k2_50_table[asic27a_sim_value & 0xf];
        break;
            
        case 0x5e:// Read from data table
            asic27a_sim_response = py2k2_5e_table[asic27a_sim_value & 0xf];
        break;
            
        case 0x60:// Read from data table
            asic27a_sim_response = py2k2_60_table[asic27a_sim_value & 0xf];
        break;
            
        case 0x6a:// Read from data table
            asic27a_sim_response = py2k2_6a_table[asic27a_sim_value & 0xf];
        break;
            
        case 0x70:// Read from data table
            asic27a_sim_response = py2k2_70_table[asic27a_sim_value & 0xf];
        break;
            
        case 0x7b:// Read from data table
            asic27a_sim_response = py2k2_7b_table[asic27a_sim_value & 0xf];
        break;
            
        case 0x80:// Read from data table
            asic27a_sim_response = py2k2_80_table[asic27a_sim_value & 0xf];
        break;
            
        case 0x8c:// Read from data table
            asic27a_sim_response = py2k2_8c_table[asic27a_sim_value & 0xf];
        break;

		case 0x99: // Reset?
            asic27a_sim_key = 0;
            asic27a_sim_response = 0x880000 | (PgmInput[7] << 8);
		break;
            
        case 0x9d:// Read from data table
            asic27a_sim_response = py2k2_9d_table[asic27a_sim_value & 0x1];
        break;
            
        case 0xa0:// Read from data table
            asic27a_sim_response = py2k2_a0_table[asic27a_sim_value & 0xf];
        break;
            
        case 0xae:// Read from data table
            asic27a_sim_response = py2k2_ae_table[asic27a_sim_value & 0xf];
        break;
            
        case 0xb0:// Read from data table
            asic27a_sim_response = py2k2_b0_table[asic27a_sim_value & 0xf];
        break;

        case 0xba:// Read from data table
            asic27a_sim_response = py2k2_ba_table[asic27a_sim_value & 0x3f];
        break;
            
        case 0xc1:  //this command implement game choose
        {
            UINT16 x = asic27a_sim_value;
            UINT16 y = 0;
            if ((py2k2_sprite_ba_value & 0xf000) == 0xf000)
            {
                y = -asic27a_sim_value;
                if (-asic27a_sim_value < py2k2_sprite_ba_value)
                {
                    y = py2k2_sprite_ba_value;
                }
            }
            else
            {
                if (py2k2_sprite_ba_value == 0)
                {
                    y = 0;
                }
                else
                {
                    y = asic27a_sim_value;
                    if (py2k2_sprite_ba_value < asic27a_sim_value)
                    {
                        y = py2k2_sprite_ba_value;
                    }
                }
            }
            if ((py2k2_sprite_value & 0xf000) == 0xf000)
            {
                x = -asic27a_sim_value;
                if (-asic27a_sim_value < py2k2_sprite_value)
                {
                    x = py2k2_sprite_value;
                }
            }
            else
            {
                if (py2k2_sprite_value == 0)
                {
                    x = 0;
                }
                else
                {
                    x = asic27a_sim_value;
                    if (py2k2_sprite_value < asic27a_sim_value)
                    {
                        x = py2k2_sprite_value;
                    }
                }
            }
            asic27a_sim_response = x | (y << 8);
        }
        break;

        case 0xc3:
            asic27a_sim_response = 0x904000 + ((asic27a_sim_regs[0xc0] + (asic27a_sim_value * 0x40)) * 4);
        break;
            
        case 0xc5:
            asic27a_sim_response = 0x880000;
            asic27a_sim_slots[asic27a_sim_value & 0xf] = asic27a_sim_slots[asic27a_sim_value & 0xf] - 1;
        break;
            
        case 0xc7:
            py2k2_sprite_value = asic27a_sim_value;
            asic27a_sim_response = 0x880000;
        break;
            
        case 0xcc: // Background layer offset (pgm3in1, same as kov)
        {
            INT32 y = asic27a_sim_value;
            if (0xf < y) y = y & 0xf;
            if (y & 0x400) y = -(0x400 - (y & 0x3ff));
            asic27a_sim_response = 0x900000 + ((asic27a_sim_regs[0xcb] + (y * 0x40)) * 4);
        }
        break;
            
        case 0xcf:
            py2k2_sprite_ba_value = asic27a_sim_value;
            asic27a_sim_response = 0x880000;
        break;

        case 0xd0:
            asic27a_sim_response = 0xa01000 + (asic27a_sim_value * 0x20);
        break;
            
        case 0xd6:
            asic27a_sim_response = 0x880000;
            asic27a_sim_slots[asic27a_sim_value & 0xf] = asic27a_sim_slots[asic27a_sim_value & 0xf] + 1;
        break;

        case 0xdc:
            asic27a_sim_response = 0xa00800 + (asic27a_sim_value * 0x40);
        break;

        case 0xe0:
            asic27a_sim_response = 0xa00000 + ((asic27a_sim_value & 0x1f) * 0x40);
        break;
            
        case 0xe5:
            asic27a_sim_response = 0x880000;
            asic27a_sim_slots[asic27a_sim_internal_slot] = (asic27a_sim_slots[asic27a_sim_internal_slot] & 0x00ff0000) | ((asic27a_sim_value & 0xffff) <<  0);
        break;
            
        case 0xe7:
            asic27a_sim_response = 0x880000;
            asic27a_sim_internal_slot = (asic27a_sim_value >> 12) & 0x0f;
            asic27a_sim_slots[asic27a_sim_internal_slot] = (asic27a_sim_slots[asic27a_sim_internal_slot] & 0x0000ffff) | ((asic27a_sim_value & 0x00ff) << 16);
        break;
            
        case 0xf8:
            asic27a_sim_response = asic27a_sim_slots[asic27a_sim_value & 0x0f] & 0x00ffffff;
        break;
            
		default:
			asic27a_sim_response = 0x880000;
		break;
	}
}

static void py2k2_sim_reset()
{
    py2k2_sprite_pos = 0;
    py2k2_sprite_base = 0;
    py2k2_sprite_value = 0;
    py2k2_sprite_ba_value = 0;
    asic27a_sim_reset();
}

static INT32 py2k2_sim_scan(INT32 nAction, INT32 *pnMin)
{
	asic27a_sim_scan(nAction, pnMin);

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(py2k2_sprite_pos);
		SCAN_VAR(py2k2_sprite_base);
        SCAN_VAR(py2k2_sprite_value);
        SCAN_VAR(py2k2_sprite_ba_value);
	}

	return 0;
}

void install_protection_asic27a_py2k2()
{
	pPgmResetCallback = 	py2k2_sim_reset;
	pPgmScanCallback = 		py2k2_sim_scan;
	asic27a_sim_command =	py2k2_asic27a_sim_command;

	SekOpen(0);
	SekMapMemory(PGMUSER0,		0x4f0000, 0x4f003f | 0x3ff, MAP_READ);

	SekMapHandler(4,			0x500000, 0x500003, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 	asic27a_sim_read);
	SekSetWriteWordHandler(4, 	asic27a_sim_write);
	SekClose();
}

//-------------------------------------------------------------------------
// Simulation used by Puzzle Star


static const UINT8 Pstar_ba[0x1e]={
	0x02,0x00,0x00,0x01,0x00,0x03,0x00,0x00,0x02,0x00,0x06,0x00,0x22,0x04,0x00,0x03,
	0x00,0x00,0x06,0x00,0x20,0x07,0x00,0x03,0x00,0x21,0x01,0x00,0x00,0x63
};

static const UINT8 Pstar_b0[0x10]={
	0x09,0x0A,0x0B,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x00,0x00,0x00,0x00
};

static const UINT16 Pstar_ae[0x10]={
	0x5D,0x86,0x8C,0x8B,0xE0,0x8B,0x62,0xAF,0xB6,0xAF,0x10A,0xAF,0x00,0x00,0x00,0x00
};

static const UINT8 Pstar_a0[0x10]={
	0x02,0x03,0x04,0x05,0x06,0x01,0x0A,0x0B,0x0C,0x0D,0x0E,0x09,0x00,0x00,0x00,0x00
};

static const UINT8 Pstar_9d[0x10]={
	0x05,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const UINT8 Pstar_90[0x10]={
	0x0C,0x10,0x0E,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const UINT8 Pstar_8c[0x23]={
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,
	0x01,0x01,0x01,0x01,0x02,0x02,0x02,0x02,0x02,0x02,0x03,0x03,0x03,0x04,0x04,0x04,
	0x03,0x03,0x03
};

static const UINT8 Pstar_80[0x1a3]={
	0x03,0x03,0x04,0x04,0x04,0x04,0x05,0x05,0x05,0x05,0x06,0x06,0x03,0x03,0x04,0x04,
	0x05,0x05,0x05,0x05,0x06,0x06,0x07,0x07,0x03,0x03,0x04,0x04,0x05,0x05,0x05,0x05,
	0x06,0x06,0x07,0x07,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x07,0x07,0x07,0x07,0x07,
	0x06,0x06,0x06,0x06,0x06,0x06,0x07,0x07,0x07,0x07,0x08,0x08,0x05,0x05,0x05,0x05,
	0x05,0x05,0x05,0x06,0x06,0x06,0x07,0x07,0x06,0x06,0x06,0x07,0x07,0x07,0x08,0x08,
	0x09,0x09,0x09,0x09,0x07,0x07,0x07,0x07,0x07,0x08,0x08,0x08,0x08,0x09,0x09,0x09,
	0x06,0x06,0x07,0x07,0x07,0x08,0x08,0x08,0x08,0x08,0x09,0x09,0x05,0x05,0x06,0x06,
	0x06,0x07,0x07,0x08,0x08,0x08,0x08,0x09,0x07,0x07,0x07,0x07,0x07,0x08,0x08,0x08,
	0x08,0x09,0x09,0x09,0x06,0x06,0x07,0x03,0x07,0x06,0x07,0x07,0x08,0x07,0x05,0x04,
	0x03,0x03,0x04,0x04,0x05,0x05,0x06,0x06,0x06,0x06,0x06,0x06,0x03,0x04,0x04,0x04,
	0x04,0x05,0x05,0x06,0x06,0x06,0x06,0x07,0x04,0x04,0x05,0x05,0x06,0x06,0x06,0x06,
	0x06,0x07,0x07,0x08,0x05,0x05,0x06,0x07,0x07,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
	0x05,0x05,0x05,0x07,0x07,0x07,0x07,0x07,0x07,0x08,0x08,0x08,0x08,0x08,0x09,0x09,
	0x09,0x09,0x03,0x04,0x04,0x05,0x05,0x05,0x06,0x06,0x07,0x07,0x07,0x07,0x08,0x08,
	0x08,0x09,0x09,0x09,0x03,0x04,0x05,0x05,0x04,0x03,0x04,0x04,0x04,0x05,0x05,0x04,
	0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x04,0x04,0x04,0x04,0x04,
	0x04,0x04,0x04,0x04,0x04,0x03,0x03,0x03,0x03,0x03,0x03,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,
	0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00
};

static UINT16 __fastcall puzlstar_protram_read_word(UINT32 offset)
{
	if ((offset & 0x3e) == 0x08) return PgmInput[7]; // Region
	if ((offset & 0x38) == 0x20) return asic27a_sim_slots[((offset & 0x06)/2)+0x10]--; // Timer

	return 0;
}

static UINT8 __fastcall puzlstar_protram_read_byte(UINT32 offset)
{
	if ((offset & 0x3e) == 0x08) return PgmInput[7]; // Region

	return 0;
}

static void puzlstar_asic27a_sim_command(UINT8 command)
{
	switch (command)
	{
		case 0x99: // Reset?
			asic27a_sim_key = 0x100;
			asic27a_sim_response = 0x880000 | (PgmInput[7] << 8);
		break;

		case 0xb1:
			asic27a_sim_response = 0x890000;
		break;

		case 0xbf:
			asic27a_sim_response = asic27a_sim_regs[0xb1] * asic27a_sim_value;
		break;

		case 0xc1: // TODO: TIMER  0,1,2,FIX TO 0 should be OK?
			asic27a_sim_response = 0;
		break;

		case 0xce: // TODO: TIMER  0,1,2
			asic27a_sim_response = 0x890000;
		break;

		case 0xcf: // TODO:TIMER  0,1,2
			asic27a_sim_slots[asic27a_sim_regs[0xce] + 0x10] = asic27a_sim_value;
			asic27a_sim_response = 0x890000;
		break;

		case 0xd0: // Text palette offset
			asic27a_sim_response = 0xa01000 + (asic27a_sim_value << 5);
		break;

		case 0xdc: // Background palette offset
			asic27a_sim_response = 0xa00800 + (asic27a_sim_value << 6);
		break;

		case 0xe0: // Sprite palette offset
			asic27a_sim_response = 0xa00000 + (asic27a_sim_value << 6);
		break;

		case 0xe5: // Write slot (low)
		{
			asic27a_sim_response = 0x890000;

			asic27a_sim_slots[asic27a_sim_internal_slot] = (asic27a_sim_slots[asic27a_sim_internal_slot] & 0xff0000) | (asic27a_sim_value);
		}
		break;

		case 0xe7: // Write slot (and slot select) (high)
		{
			asic27a_sim_response = 0x890000;

			asic27a_sim_internal_slot = (asic27a_sim_value >> 12) & 0xf;
			asic27a_sim_slots[asic27a_sim_internal_slot] = (asic27a_sim_slots[asic27a_sim_internal_slot] & 0x00ffff) | ((asic27a_sim_value & 0xff) << 16);
		}
		break;

		case 0xf8: // Read slot
			asic27a_sim_response = asic27a_sim_slots[asic27a_sim_value];
		break;

		case 0x80: // Read from data table
	    		asic27a_sim_response = Pstar_80[asic27a_sim_value];
	   	break;

		case 0x8c: // Read from data table
	    		asic27a_sim_response = Pstar_8c[asic27a_sim_value];
	   	break;

		case 0x90: // Read from data table
	    		asic27a_sim_response = Pstar_90[asic27a_sim_value];
	   	break;

		case 0x9d: // Read from data table
	    		asic27a_sim_response = Pstar_9d[asic27a_sim_value];
	   	break;

		case 0xa0: // Read from data table
	    		asic27a_sim_response = Pstar_a0[asic27a_sim_value];
	   	break;

		case 0xae: // Read from data table
	    		asic27a_sim_response = Pstar_ae[asic27a_sim_value];
	   	break;

		case 0xb0: // Read from data table
	    		asic27a_sim_response = Pstar_b0[asic27a_sim_value];
	   	break;

		case 0xba: // Read from data table
	    		asic27a_sim_response = Pstar_ba[asic27a_sim_value];
	   	break;

		default:
			asic27a_sim_response = 0x890000;
		break;
	}
}

void install_protection_asic27a_puzlstar()
{
	pPgmResetCallback = asic27a_sim_reset;
	pPgmScanCallback = asic27a_sim_scan;

	asic27a_sim_command = puzlstar_asic27a_sim_command;

	SekOpen(0);
	SekMapHandler(4,		0x500000, 0x500003, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 	asic27a_sim_read);
	SekSetWriteWordHandler(4, 	asic27a_sim_write);

	SekMapHandler(5,		0x4f0000, 0x4f03ff, MAP_READ);
	SekSetReadWordHandler(5, 	puzlstar_protram_read_word);
	SekSetReadByteHandler(5, 	puzlstar_protram_read_byte);
	SekClose();
}

#if 0
// Reference simulation for photoy2k - use emulation instead

static UINT32 photoy2k_seqpos;

static UINT32 photoy2k_spritenum()
{
	switch((photoy2k_seqpos >> 10) & 0xf)
	{
		case 0x0:
		case 0xa:
			return BITSWAP16(photoy2k_seqpos, 15,14,13,12,11,10, 0,8,3,1,5,9,4,2,6,7) ^ 0x124;
		case 0x1:
		case 0xb:
			return BITSWAP16(photoy2k_seqpos, 15,14,13,12,11,10, 5,1,7,4,0,8,3,6,9,2) ^ 0x088;
		case 0x2:
		case 0x8:
			return BITSWAP16(photoy2k_seqpos, 15,14,13,12,11,10, 3,5,9,7,6,4,1,8,2,0) ^ 0x011;
		case 0x3:
		case 0x9:
			return BITSWAP16(photoy2k_seqpos, 15,14,13,12,11,10, 1,8,3,6,0,4,5,2,9,7) ^ 0x154;
		case 0x4:
		case 0xe:
			return BITSWAP16(photoy2k_seqpos, 15,14,13,12,11,10, 2,1,7,4,5,8,3,6,9,0) ^ 0x0a9;
		case 0x5:
		case 0xf:
			return BITSWAP16(photoy2k_seqpos, 15,14,13,12,11,10, 9,4,6,8,2,1,7,5,3,0) ^ 0x201;
		case 0x6:
		case 0xd:
			return BITSWAP16(photoy2k_seqpos, 15,14,13,12,11,10, 4,6,0,8,9,7,3,5,1,2) ^ 0x008;
		case 0x7:
		case 0xc:
			return BITSWAP16(photoy2k_seqpos, 15,14,13,12,11,10, 8,9,3,2,0,1,6,7,5,4) ^ 0x000;
	}
	
	return 0;
}

static UINT16 photoy2k_decode_sprite()
{
	UINT8 salt = PGMARMROM[0x1cfc + (asic27a_sim_regs[0x23] >> 1)] >> ((asic27a_sim_regs[0x23] & 1) * 4); // lsb of command 23 value selects high or low nibble of salt data

	return (asic27a_sim_regs[0x22] & ~0x210a) | ((salt & 0x01) << 1) | ((salt & 0x02) << 2) | ((salt & 0x04) << 6) | ((salt & 0x08) << 10);
}

static void photoy2k_asic27a_sim_command(UINT8 command)
{
	switch (command)
	{
		case 0x23: // Address for 'salt' look-up for decode_sprite
		case 0x22: // Data to be 'salted' for decode_sprite
			asic27a_sim_response = 0x880000;
		break;

		case 0x21: // Actually perform sprite decode
			asic27a_sim_response = photoy2k_decode_sprite();
		break;
		
		case 0x20: // Get high byte value for sprite decode (just return value written to command 21)
			asic27a_sim_response = asic27a_sim_regs[0x21];
		break;

		case 0x30: // Advance sprite number position
			photoy2k_seqpos++;
			asic27a_sim_response = photoy2k_spritenum();
		break;

		case 0x32: // Set sprite number position
			photoy2k_seqpos = asic27a_sim_value << 4;
			asic27a_sim_response = photoy2k_spritenum();
		break;

		case 0xae: // Bonus round look-up table
			asic27a_sim_response = PGMARMROM[0x193C + (asic27a_sim_value * 4)];
		break;

		case 0xba: // Another look-up table
			asic27a_sim_response = PGMARMROM[0x1888 + (asic27a_sim_value * 4)];
		break;

	// from kov
		case 0x99: // Reset
			asic27a_sim_key = 0;
			asic27a_sim_response = 0x880000;
		break;

        case 0xc0: // Text layer 'x' select
            asic27a_sim_response = 0x880000;
        break;

        case 0xc3: // Text layer offset
            asic27a_sim_response = 0x904000 + ((asic27a_sim_regs[0xc0] + (asic27a_sim_value * 0x40)) * 4);
        break;

        case 0xcb: // Background layer 'x' select
            asic27a_sim_response = 0x880000;
        break;

        case 0xcc: // Background layer offset
        {
            INT32 y = asic27a_sim_value;
            if (0xf < y) y = y & 0xf;
            if (y & 0x400) y = -(0x400 - (y & 0x3ff));
            asic27a_sim_response = 0x900000 + (((asic27a_sim_regs[0xcb] + y * 64) * 4));
        }
        break;

        case 0xd0: // Text palette offset
            asic27a_sim_response = 0xa01000 + (asic27a_sim_value * 0x20);
        break;

        case 0xdc: // Background palette offset
            asic27a_sim_response = 0xa00800 + (asic27a_sim_value * 0x40);
        break;

        case 0xe0: // Sprite palette offset
            asic27a_sim_response = 0xa00000 + ((asic27a_sim_value & 0x1f) * 0x40);
        break;

		default:
			asic27a_sim_response = 0x880000;
	//		bprintf (0, _T("Unknown command: %2.2x, Value: %4.4x\n"), command, asic27a_sim_value);
	}
}

static void photoy2k_sim_reset()
{
    photoy2k_seqpos = 0;
    asic27a_sim_reset();
}

static INT32 photoy2k_sim_scan(INT32 nAction, INT32 *pnMin)
{
	asic27a_sim_scan(nAction, pnMin);

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(photoy2k_seqpos);
	}

	return 0;
}

void install_protection_asic27a_photoy2k_sim()
{
	PGMARMROM = (UINT8*)BurnMalloc(0x4000);

	// load arm7 data for use in table look-up commands (ae, ba)
	{
		char* pRomName;
		struct BurnRomInfo ri;

		for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
			BurnDrvGetRomInfo(&ri, i);

			if (ri.nLen == 0x4000) {
				BurnLoadRom(PGMARMROM, i, 1); 
			}
		}
	}

	pPgmResetCallback 		= photoy2k_sim_reset;
	pPgmScanCallback 		= photoy2k_sim_scan;
	asic27a_sim_command 	= photoy2k_asic27a_sim_command;

	SekOpen(0);
	SekMapMemory(PGMUSER0,		0x4f0000, 0x4f003f | 0x3ff, MAP_READ); // shared overlay ram (not writeable by 68k)

	SekMapHandler(4,			0x500000, 0x500005, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 	asic27a_sim_read);
	SekSetWriteWordHandler(4, 	asic27a_sim_write);
	SekClose();
}
#endif

