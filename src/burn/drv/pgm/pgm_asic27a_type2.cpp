#include "pgm.h"

/*
	IGS Asic27a (type 2) proper emulation

	Used by:
		Martial Masters
		Bee Storm - DoDonPachi II
		Dragon World 2001
		Dragon World Pretty Chance
		Knights of Valour 2
		Knights of Valour 2 Plus - Nine Dragons
*/

static UINT8 asic27a_to_arm = 0;
static UINT8 asic27a_to_68k = 0;

static inline void pgm_cpu_sync()
{
	INT32 nCycles = SekTotalCycles() - Arm7TotalCycles();

	if (nCycles > 100) {
		Arm7Run(nCycles);
	}
}

static void __fastcall asic27a_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffffe) == 0xd10000) {	// ddp2
		pgm_cpu_sync();
		asic27a_to_arm = data;
		Arm7SetIRQLine(ARM7_FIRQ_LINE, CPU_IRQSTATUS_ACK);
		return;
	}
}

static void __fastcall asic27a_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffffe) == 0xd10000) {
		pgm_cpu_sync();
		asic27a_to_arm = data & 0xff;
		Arm7SetIRQLine(ARM7_FIRQ_LINE, CPU_IRQSTATUS_ACK);
		return;
	}
}

static UINT8 __fastcall asic27a_read_byte(UINT32 address)
{
	if ((address & 0xfffffc) == 0xd10000) {
		pgm_cpu_sync();
		return asic27a_to_68k;
	}

	return 0;
}

static UINT16 __fastcall asic27a_read_word(UINT32 address)
{
	if ((address & 0xfffffc) == 0xd10000) {
		pgm_cpu_sync();
		return asic27a_to_68k;
	}

	return 0;
}

static void asic27a_arm7_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x38000000:
			asic27a_to_68k = data;
		return;
	}
}

static UINT8 asic27a_arm7_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x38000000:
			Arm7SetIRQLine(ARM7_FIRQ_LINE, CPU_IRQSTATUS_NONE);
			return asic27a_to_arm;
	}

	return 0;
}

static INT32 asic27aScan(INT32 nAction,INT32 *)
{
	struct BurnArea ba;

	if (nAction & ACB_MEMORY_RAM) {
		ba.Data		= PGMARMShareRAM;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0xd00000;
		ba.szName	= "ARM SHARE RAM";
		BurnAcb(&ba);

		ba.Data		= PGMARMRAM0;
		ba.nLen		= 0x0000400;
		ba.nAddress	= 0;
		ba.szName	= "ARM RAM 0";
		BurnAcb(&ba);

		ba.Data		= PGMARMRAM1;
		ba.nLen		= 0x0010000;
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

		SCAN_VAR(asic27a_to_arm);
		SCAN_VAR(asic27a_to_68k);
	}

 	return 0;
}

void install_protection_asic27a_martmast()
{
	nPGMArm7Type = 2;
	pPgmScanCallback = asic27aScan;

	SekOpen(0);

	SekMapMemory(PGMARMShareRAM,	0xd00000, 0xd0ffff, MAP_RAM);

	SekMapHandler(4,		0xd10000, 0xd10003, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, asic27a_read_word);
	SekSetReadByteHandler(4, asic27a_read_byte);
	SekSetWriteWordHandler(4, asic27a_write_word);
	SekSetWriteByteHandler(4, asic27a_write_byte);
	SekClose();

	Arm7Init(0);
	Arm7Open(0);
	Arm7MapMemory(PGMARMROM,	0x00000000, 0x00003fff, MAP_ROM);
	Arm7MapMemory(PGMUSER0,		0x08000000, 0x08000000+(nPGMExternalARMLen-1), MAP_ROM);
	Arm7MapMemory(PGMARMRAM0,	0x10000000, 0x100003ff, MAP_RAM);
	Arm7MapMemory(PGMARMRAM1,	0x18000000, 0x1800ffff, MAP_RAM);
	Arm7MapMemory(PGMARMShareRAM,	0x48000000, 0x4800ffff, MAP_RAM);
	Arm7MapMemory(PGMARMRAM2,	0x50000000, 0x500003ff, MAP_RAM);
	Arm7SetWriteByteHandler(asic27a_arm7_write_byte);
	Arm7SetReadByteHandler(asic27a_arm7_read_byte);
	Arm7Close();
}
