#include "pgm.h"
#include "bitswap.h"

/*
	IGS Asic3 emulation

	Used by:
		Oriental Legends
*/

static UINT8 asic3_latch[3];
static UINT8 asic3_x;
static UINT8 asic3_reg;
static UINT16 asic3_hold;
static UINT16 asic3_hilo;

static void asic3_compute_hold(INT32 y, INT32 z)
{
	UINT16 old = asic3_hold;

	asic3_hold = ((old << 1) | (old >> 15));
	asic3_hold ^= 0x2bad;
	asic3_hold ^= BIT(z, y);
	asic3_hold ^= BIT(asic3_x, 2) << 10;
	asic3_hold ^= BIT(old, 5);

	static INT32 modes[8] = { 1, 1, 3, 2, 4, 4, 4, 4 };

	switch (modes[PgmInput[7] & 7]) // The mode is dependent on the region
	{
		case 1:
			asic3_hold ^= BIT(old, 10) ^ BIT(old, 8) ^ (BIT(asic3_x, 0) << 1) ^ (BIT(asic3_x, 1) << 6) ^ (BIT(asic3_x, 3) << 14);
		break;

		case 2:
			asic3_hold ^= BIT(old,  7) ^ BIT(old, 6) ^ (BIT(asic3_x, 0) << 4) ^ (BIT(asic3_x, 1) << 6) ^ (BIT(asic3_x, 3) << 12);
		break;

		case 3:
			asic3_hold ^= BIT(old, 10) ^ BIT(old, 8) ^ (BIT(asic3_x, 0) << 4) ^ (BIT(asic3_x, 1) << 6) ^ (BIT(asic3_x, 3) << 12);
		break;

		case 4:
			asic3_hold ^= BIT(old,  7) ^ BIT(old, 6) ^ (BIT(asic3_x, 0) << 3) ^ (BIT(asic3_x, 1) << 8) ^ (BIT(asic3_x, 3) << 14);
		break;
	}
}

static UINT16 __fastcall asic3_read_word(UINT32 )
{
	switch (asic3_reg)
	{
		case 0x00:
			return (asic3_latch[0] & 0xf7) | ((PgmInput[7] << 3) & 0x08);

		case 0x01:
			return (asic3_latch[1]);

		case 0x02:
			return (asic3_latch[2] & 0x7f) | ((PgmInput[7] << 6) & 0x80);

		case 0x03:
			return BITSWAP08(asic3_hold, 5,2,9,7,10,13,12,15);

		case 0x20: return 0x49;
		case 0x21: return 0x47;
		case 0x22: return 0x53;

		case 0x24: return 0x41;
		case 0x25: return 0x41;
		case 0x26: return 0x7f;
		case 0x27: return 0x41;
		case 0x28: return 0x41;

		case 0x2a: return 0x3e;
		case 0x2b: return 0x41;
		case 0x2c: return 0x49;
		case 0x2d: return 0xf9;
		case 0x2e: return 0x0a;

		case 0x30: return 0x26;
		case 0x31: return 0x49;
		case 0x32: return 0x49;
		case 0x33: return 0x49;
		case 0x34: return 0x32;
	}

	return 0;
}

static void __fastcall asic3_write_word(UINT32 address, UINT16 data)
{
	if (address == 0xc04000) {
		asic3_reg = data;
		return;
	}

	switch (asic3_reg)
	{
		case 0x00:
		case 0x01:
		case 0x02:
			asic3_latch[asic3_reg] = data << 1;
		break;

		case 0x40:
			asic3_hilo = (asic3_hilo << 8) | data;
		break;

		case 0x48:
		{
			asic3_x = 0;
			if ((asic3_hilo & 0x0090) == 0) asic3_x |= 0x01;
			if ((asic3_hilo & 0x0006) == 0) asic3_x |= 0x02;
			if ((asic3_hilo & 0x9000) == 0) asic3_x |= 0x04;
			if ((asic3_hilo & 0x0a00) == 0) asic3_x |= 0x08;
		}
		break;

		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87:
			asic3_compute_hold(asic3_reg & 0x07, data);
		break;

		case 0xa0:
			asic3_hold = 0;
		break;
	}
}

static void reset_asic3()
{
	memset (asic3_latch, 0, 3 * sizeof(UINT8));

	asic3_hold = 0;
	asic3_reg = 0;
	asic3_x = 0;
	asic3_hilo = 0;
}

static INT32 asic3Scan(INT32 nAction, INT32 *)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(asic3_reg);
		SCAN_VAR(asic3_latch[0]);
		SCAN_VAR(asic3_latch[1]);
		SCAN_VAR(asic3_latch[2]);
		SCAN_VAR(asic3_x);
		SCAN_VAR(asic3_hilo);
		SCAN_VAR(asic3_hold);
	}

	return 0;
}

void install_protection_asic3_orlegend()
{
	pPgmScanCallback = asic3Scan;
	pPgmResetCallback = reset_asic3;

	SekOpen(0);
	SekMapHandler(4,	0xc04000, 0xc0400f, MAP_READ | MAP_WRITE);
	SekSetReadWordHandler(4, asic3_read_word);
	SekSetWriteWordHandler(4, asic3_write_word);
	SekClose();
}
