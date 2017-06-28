#include "pgm.h"

/*
	IGS Asic27a (type 3) proper emulation

	Used by:
		Demon Front
		The Gladiator
		The Killing Blade Plus
		Spectral VS Generation
		Happy 6in1
*/

static UINT8 svg_ram_sel = 0;
static UINT8 *svg_ram[2];
static UINT8 asic27a_68k_to_arm = 0;
static UINT8 asic27a_arm_to_68k = 0;

static inline void pgm_cpu_sync()
{
	INT32 nCycles = SekTotalCycles() - Arm7TotalCycles();

	if (nCycles > 100) {
		Arm7Run(nCycles);
	}
}

static void svg_set_ram_bank(INT32 data)
{
	svg_ram_sel = data & 1;
	Arm7MapMemory(svg_ram[svg_ram_sel],	0x38000000, 0x3800ffff, MAP_RAM);
	SekMapMemory(svg_ram[svg_ram_sel^1],	0x500000, 0x50ffff, MAP_RAM);
}

static void __fastcall svg_write_byte(UINT32 address, UINT8 /*data*/)
{
	pgm_cpu_sync();

	switch (address)
	{
		case 0x5c0000:
			Arm7SetIRQLine(ARM7_FIRQ_LINE, CPU_IRQSTATUS_AUTO);
		return;
	}
}

static void __fastcall svg_write_word(UINT32 address, UINT16 data)
{
	pgm_cpu_sync();

	switch (address)
	{
		case 0x5c0300:
			asic27a_68k_to_arm = data; // byte
		return;
	}
}

static UINT16 __fastcall svg_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x5c0300:
			pgm_cpu_sync();
			return asic27a_arm_to_68k;
	}

	return 0;
}

static void svg_arm7_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x40000018:
			svg_set_ram_bank(data);
		return;

		case 0x48000000:
			asic27a_arm_to_68k = data;
		return;
	}
}

static UINT8 svg_arm7_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x48000000:
			return asic27a_68k_to_arm;
	}

	return 0;
}

static INT32 svg_asic27aScan(INT32 nAction,INT32 *)
{
	struct BurnArea ba;

	if (nAction & ACB_MEMORY_RAM) {
		ba.Data		= PGMARMShareRAM;
		ba.nLen		= 0x0020000;
		ba.nAddress	= 0x400000;
		ba.szName	= "ARM SHARE RAM #0 (address 500000)";
		BurnAcb(&ba);

		ba.Data		= PGMARMShareRAM2;
		ba.nLen		= 0x0020000;
		ba.nAddress	= 0x500000;
		ba.szName	= "ARM SHARE RAM #1";
		BurnAcb(&ba);

		ba.Data		= PGMARMRAM0;
		ba.nLen		= 0x0000400;
		ba.nAddress	= 0;
		ba.szName	= "ARM RAM 0";
		BurnAcb(&ba);

		ba.Data		= PGMARMRAM1;
		ba.nLen		= 0x0040000;
		ba.nAddress	= 0;
		ba.szName	= "ARM RAM 1";
		BurnAcb(&ba);

		ba.Data		= PGMARMRAM2;
		ba.nLen		= 0x0000400;
		ba.nAddress	= 0;
		ba.szName	= "ARM RAM 2";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		Arm7Scan(nAction);
		SCAN_VAR(asic27a_68k_to_arm);
		SCAN_VAR(asic27a_arm_to_68k);

		SCAN_VAR(svg_ram_sel);
	}

	if (nAction & ACB_WRITE) {
		SekOpen(0);
		svg_set_ram_bank(svg_ram_sel);
		SekClose();
	}

 	return 0;
}

void install_protection_asic27a_svg()
{
	nPGMArm7Type = 3;

	pPgmScanCallback = svg_asic27aScan;

	svg_ram_sel = 0;
	svg_ram[0] = PGMARMShareRAM;
	svg_ram[1] = PGMARMShareRAM2;

	SekOpen(0);
	SekMapHandler(5,		0x500000, 0x5fffff, MAP_RAM);
	SekSetReadWordHandler(5, 	svg_read_word);
	SekSetWriteWordHandler(5, 	svg_write_word);
	SekSetWriteByteHandler(5, 	svg_write_byte);
	SekClose();

	Arm7Init(0);
	Arm7Open(0);
	Arm7MapMemory(PGMARMROM,	0x00000000, 0x00003fff, MAP_ROM);
	Arm7MapMemory(PGMUSER0,		0x08000000, 0x08000000 | (nPGMExternalARMLen-1), MAP_ROM);
	Arm7MapMemory(PGMARMRAM0,	0x10000000, 0x100003ff, MAP_RAM);
	Arm7MapMemory(PGMARMRAM1,	0x18000000, 0x1803ffff, MAP_RAM);
	Arm7MapMemory(svg_ram[1],	0x38000000, 0x3800ffff, MAP_RAM);
	Arm7MapMemory(PGMARMRAM2,	0x50000000, 0x500003ff, MAP_RAM);
	Arm7SetWriteByteHandler(svg_arm7_write_byte);
	Arm7SetReadByteHandler(svg_arm7_read_byte);
	Arm7Close();
}
