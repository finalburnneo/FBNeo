#include "pgm.h"

/*
	IGS Asic27a (type 1) proper emulation

	Used by:
		Knights of Valour Superheroes
		Photo Y2K
		Knights of Valour Super Heroes Plus (Using a small hack)
		Various Knights of Valour bootlegs

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
		Photo Y2K2 - NOT WORKING!
		-----------------------------------------------------------
		Puzzle Star - NOT WORKING!
*/

static UINT16 kovsh_highlatch_arm_w;
static UINT16 kovsh_lowlatch_arm_w;
static UINT16 kovsh_highlatch_68k_w;
static UINT16 kovsh_lowlatch_68k_w ;
static UINT32 kovsh_counter;

static inline void pgm_cpu_sync()
{
	INT32 nCycles = SekTotalCycles() - Arm7TotalCycles();

	if (nCycles > 100) {
		Arm7Run(nCycles);
	}
}

static void __fastcall kovsh_asic27a_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x500000:
		case 0x600000:
			kovsh_lowlatch_68k_w = data;
		return;

		case 0x500002:
		case 0x600002:
			kovsh_highlatch_68k_w = data;
		return;
	}
}

static UINT16 __fastcall kovsh_asic27a_read_word(UINT32 address)
{
	if ((address & 0xffffc0) == 0x4f0000) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(PGMARMShareRAM + (address & 0x3e))));
	}

	switch (address)
	{
		case 0x500000:
		case 0x600000:
			pgm_cpu_sync();
			return kovsh_lowlatch_arm_w;

		case 0x500002:
		case 0x600002:
			pgm_cpu_sync();
			return kovsh_highlatch_arm_w;
	}

	return 0;
}

static void kovsh_asic27a_arm7_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffff80) == 0x50800000) {	// written... but never read?
		*((UINT16*)(PGMARMShareRAM + ((address>>1) & 0x3e))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}
}

static void kovsh_asic27a_arm7_write_long(UINT32 address, UINT32 data)
{
	switch (address)
	{
		case 0x40000000:
		{
			kovsh_highlatch_arm_w = data >> 16;
			kovsh_lowlatch_arm_w = data;

			kovsh_highlatch_68k_w = 0;
			kovsh_lowlatch_68k_w = 0;
		}
		return;
	}
}

static UINT32 kovsh_asic27a_arm7_read_long(UINT32 address)
{
	switch (address)
	{
		case 0x40000000:
			return (kovsh_highlatch_68k_w << 16) | (kovsh_lowlatch_68k_w);

		case 0x4000000c:
			return kovsh_counter++;
	}

	return 0;
}

static void reset_kovsh_asic27a()
{
	kovsh_highlatch_arm_w = 0;
	kovsh_lowlatch_arm_w = 0;
	kovsh_highlatch_68k_w = 0;
	kovsh_lowlatch_68k_w = 0;
	kovsh_counter = 1;
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

		SCAN_VAR(kovsh_highlatch_arm_w);
		SCAN_VAR(kovsh_lowlatch_arm_w);
		SCAN_VAR(kovsh_highlatch_68k_w);
		SCAN_VAR(kovsh_lowlatch_68k_w);
		SCAN_VAR(kovsh_counter);
	}

 	return 0;
}

void install_protection_asic27a_kovsh()
{
	nPGMArm7Type = 1;
	pPgmScanCallback = kovsh_asic27aScan;
	pPgmResetCallback = reset_kovsh_asic27a;

	SekOpen(0);
	SekMapMemory(PGMARMShareRAM,	0x4f0000, 0x4f003f, MAP_RAM);

	SekMapHandler(4,		0x500000, 0x600005, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 	kovsh_asic27a_read_word);
	SekSetWriteWordHandler(4, 	kovsh_asic27a_write_word);
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
// Knights of Valour Super Heroes Plus Hack

void __fastcall kovshp_asic27a_write_word(UINT32 address, UINT16 data)
{
	switch (address & 6)
	{
		case 0:
			kovsh_lowlatch_68k_w = data;
		return;

		case 2:
		{
			unsigned char asic_key = data >> 8;
			unsigned char asic_cmd = (data & 0xff) ^ asic_key;

			switch (asic_cmd) // Intercept commands and translate them to those used by kovsh
			{
				case 0x9a: asic_cmd = 0x99; break; // kovassga
				case 0xa6: asic_cmd = 0xa9; break; // kovassga
				case 0xaa: asic_cmd = 0x56; break; // kovassga
				case 0xf8: asic_cmd = 0xf3; break; // kovassga

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

			kovsh_highlatch_68k_w = asic_cmd ^ (asic_key | (asic_key << 8));
		}
		return;
	}
}

void install_protection_asic27a_kovshp()
{
	nPGMArm7Type = 1;
	pPgmScanCallback = kovsh_asic27aScan;

	SekOpen(0);
	SekMapMemory(PGMARMShareRAM,	0x4f0000, 0x4f003f, MAP_RAM);

	SekMapHandler(4,		0x500000, 0x600005, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 	kovsh_asic27a_read_word);
	SekSetWriteWordHandler(4, 	kovshp_asic27a_write_word);
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

void __fastcall asic27a_sim_write(UINT32 offset, UINT16 data)
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

		//	bprintf (0, _T("Command: %2.2x, Data: %2.2x\n"), command, asic27a_sim_value);

			asic27a_sim_command(command);

			asic27a_sim_key = (asic27a_sim_key + 0x0100) & 0xff00;
			if (asic27a_sim_key == 0xff00) asic27a_sim_key = 0x0100;
			asic27a_sim_key |= asic27a_sim_key >> 8;
		}
		return;

		case 4: return;
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
	SekMapHandler(4,		0x400000, 0x400005, MAP_READ | MAP_WRITE);
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
	SekMapHandler(4,		0x500000, 0x500005, MAP_READ | MAP_WRITE);
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
	SekMapMemory(PGMUSER0,		0x4f0000, 0x4f003f | 0x3ff, MAP_READ); // ram

	SekMapHandler(4,		0x500000, 0x500003, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, 	asic27a_sim_read);
	SekSetWriteWordHandler(4, 	asic27a_sim_write);
	SekClose();
}



//-----------------------------------------------------------------------------------------------------
// Simulation used by Knights of Valour


static const UINT8 B0TABLE[8] = { 2, 0, 1, 4, 3 }; // Maps char portraits to tables

static const UINT8 BATABLE[0x40] = {
	0x00,0x29,0x2c,0x35,0x3a,0x41,0x4a,0x4e,0x57,0x5e,0x77,0x79,0x7a,0x7b,0x7c,0x7d,
	0x7e,0x7f,0x80,0x81,0x82,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x90,
	0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9e,0xa3,0xd4,0xa9,0xaf,0xb5,0xbb,0xc1
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
		case 0xc5: // kovplus
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
			asic27a_sim_response = B0TABLE[asic27a_sim_value & 0x07];
		break;

		case 0xb4: // Copy slot 'a' to slot 'b'
		case 0xb7: // kovsgqyz (b4)
		{
			asic27a_sim_response = 0x880000;

			if (asic27a_sim_value == 0x0102) asic27a_sim_value = 0x0100; // why?

			asic27a_sim_slots[(asic27a_sim_value >> 8) & 0x0f] = asic27a_sim_slots[(asic27a_sim_value >> 0) & 0x0f];
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
	    		if (y & 0x400) y = -(0x400 - (y & 0x3ff));
	    		asic27a_sim_response = 0x900000 + (((asic27a_sim_regs[0xcb] + y * 64) * 4));
   		}
		break;

		case 0xd0: // Text palette offset
		case 0xcd: // kovsgqyz (d0)
			asic27a_sim_response = 0xa01000 + (asic27a_sim_value * 0x20);
		break;

		case 0xd6: // Copy slot to slot 0
			asic27a_sim_response = 0x880000;
			asic27a_sim_slots[0] = asic27a_sim_slots[asic27a_sim_value & 0x0f];
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
			asic27a_sim_response = 0x880000;

			asic27a_sim_slots[asic27a_sim_internal_slot] = (asic27a_sim_slots[asic27a_sim_internal_slot] & 0x00ff0000) | ((asic27a_sim_value & 0xffff) <<  0);
		}
		break;

		case 0xe7: // Write slot (and slot select) (high)
		{
			asic27a_sim_response = 0x880000;
			asic27a_sim_internal_slot = (asic27a_sim_value >> 12) & 0x0f;

			asic27a_sim_slots[asic27a_sim_internal_slot] = (asic27a_sim_slots[asic27a_sim_internal_slot] & 0x0000ffff) | ((asic27a_sim_value & 0x00ff) << 16);
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
	SekMapMemory(PGMUSER0,		0x4f0000, 0x4f003f | 0x3ff, MAP_READ);

	SekMapHandler(4,		0x500000, 0x500003, MAP_READ | MAP_WRITE);
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
// Simulation used by Photo Y2k2 (not working!)


static void py2k2_asic27a_sim_command(UINT8 command)
{
	switch (command)
	{
		case 0x30: // Sprite sequence, advance to next sprite
		break;

		case 0x32: // Sprite sequence, decode offset
		break;

	//	case 0x33:
	//	case 0x34:
	//	case 0x35: // table?
	//	case 0x38: // ?
	//	break;

		// "CCHANGE.C 285 ( TIME >= VALUE1 ) && TIME <= ALL_LEV_TIME )
		case 0xba: // ??
			asic27a_sim_response = asic27a_sim_value + 1; // gets us in-game
		break;

		case 0x99: // Reset?
			asic27a_sim_key = 0x100;
			asic27a_sim_response = 0x880000 | (PgmInput[7] << 8);
		break;

		case 0xc0:
			asic27a_sim_response = 0x880000;
		break;

		case 0xc3:
			asic27a_sim_response = 0x904000 + ((asic27a_sim_regs[0xc0] + (asic27a_sim_value * 0x40)) * 4);
		break;

		case 0xd0:
			asic27a_sim_response = 0xa01000 + (asic27a_sim_value * 0x20);
		break;

		case 0xdc:
			asic27a_sim_response = 0xa00800 + (asic27a_sim_value * 0x40);
		break;

		case 0xe0:
			asic27a_sim_response = 0xa00000 + ((asic27a_sim_value & 0x1f) * 0x40);
		break;

		case 0xcb: // Background layer 'x' select (pgm3in1, same as kov)
			asic27a_sim_response = 0x880000;
		break;

		case 0xcc: // Background layer offset (pgm3in1, same as kov)
		{
			INT32 y = asic27a_sim_value;
			if (y & 0x400) y = -(0x400 - (y & 0x3ff));
			asic27a_sim_response = 0x900000 + ((asic27a_sim_regs[0xcb] + (y * 0x40)) * 4);
		}
		break;

		default:
			asic27a_sim_response = 0x880000;
			bprintf (0, _T("Unknown ASIC Command %2.2x Value: %4.4x\n"), command, asic27a_sim_value);
		break;
	}
}

void install_protection_asic27a_py2k2()
{
	pPgmResetCallback = asic27a_sim_reset;
	pPgmScanCallback = asic27a_sim_scan;
	asic27a_sim_command = py2k2_asic27a_sim_command;

	SekOpen(0);
	SekMapMemory(PGMUSER0,		0x4f0000, 0x4f003f | 0x3ff, MAP_READ);

	SekMapHandler(4,		0x500000, 0x500003, MAP_READ | MAP_WRITE);
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

