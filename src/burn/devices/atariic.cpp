#include "burnint.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "slapstic.h"

static INT32 atari_eeprom_initialized = 0;
static INT32 atari_slapstic_initialized = 0;


static UINT8 *atari_eeprom;
static UINT32 atari_eeprom_size;
static INT32 atari_eeprom_addrmask;
static UINT32 atari_eeprom_address_start;
static INT32 atari_eeprom_unlocked;


static UINT8 *atari_slapstic_rom;


static void __fastcall atari_eeprom_write_word(UINT32 address, UINT16 data)
{
	if (atari_eeprom_initialized == 0) {
		bprintf (0, _T("atari_eeprom_write_word(%x, %4.4x) called without being initialized!\n"), address, data);
	}

	if (atari_eeprom_unlocked)
	{
		*((UINT16*)(atari_eeprom + (address & atari_eeprom_addrmask))) = data | 0xff00;
		atari_eeprom_unlocked = 0;
	}
}

static void __fastcall atari_eeprom_write_byte(UINT32 address, UINT8 data)
{	
	if (atari_eeprom_initialized == 0) {
		bprintf (0, _T("atari_eeprom_write_byte(%x, %2.2x) called without being initialized!\n"), address, data);
	}

	if (atari_eeprom_unlocked )
	{
		*((UINT16*)(atari_eeprom + (address & atari_eeprom_addrmask))) = data | 0xff00;
		atari_eeprom_unlocked = 0;
	}
}

void AtariEEPROMUnlockWrite()
{
	if (atari_eeprom_initialized == 0) {
		bprintf (0, _T("AtariEEPROMUnlockWrite() called without being initialized!\n"));
		return;
	}

	atari_eeprom_unlocked = 1;
}

void AtariEEPROMReset()
{
	atari_eeprom_unlocked = 0;
}

void AtariEEPROMInit(INT32 size)
{
	atari_eeprom_initialized = 1;
	atari_eeprom_size = size;
	atari_eeprom_addrmask = (atari_eeprom_size - 1) & ~1;

	atari_eeprom = (UINT8*)BurnMalloc(atari_eeprom_size);

	memset (atari_eeprom, 0xff, atari_eeprom_size);
}

void AtariEEPROMInstallMap(INT32 map_handler, UINT32 address_start, UINT32 address_end)
{
	if (atari_eeprom_initialized == 0) {
		bprintf (0, _T("AtariEEPROMInstallMap(%d, %x, %x) called without being initialized!\n"), address_start, address_end);
		return;
	}

	if (((address_end+1)-address_start) > atari_eeprom_size)
	{
		address_end = address_start + (address_end & (atari_eeprom_size - 1));
	}

	atari_eeprom_address_start = address_start;

	//bprintf (0, _T("atarieeprom size: %4.4x, address_start: %6.6x, address_end: %6.6x\n"), atari_eeprom_size, address_start, address_end);

	SekMapMemory(atari_eeprom, 				address_start, address_end, MAP_ROM);
	SekMapHandler(map_handler,				address_start, address_end, MAP_WRITE);
	SekSetWriteWordHandler(map_handler,		atari_eeprom_write_word);
	SekSetWriteByteHandler(map_handler,		atari_eeprom_write_byte);
}

void AtariEEPROMLoad(UINT8 *src)
{
	if (atari_eeprom_initialized == 0) {
		bprintf (0, _T("AtariEEPROMLoad(UINT8 *src) called without being initialized!\n"));
		return;
	}

	for (INT32 i = 0; i < atari_eeprom_size; i+=2)
	{
		*((UINT16*)(atari_eeprom + i)) = src[i] | 0xff00;
	}
}

void AtariEEPROMExit()
{
	BurnFree(atari_eeprom);
	atari_eeprom_initialized = 0;
}

INT32 AtariEEPROMScan(INT32 nAction, INT32 *)
{
	struct BurnArea ba;

	if (nAction & ACB_NVRAM) {
		ba.Data		= atari_eeprom;
		ba.nLen		= atari_eeprom_size;
		ba.nAddress	= atari_eeprom_address_start;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_VOLATILE)
	{
		SCAN_VAR(atari_eeprom_unlocked);
	}

	return 0;
}


static void __fastcall slapstic_write_word(UINT32 address, UINT16 )
{
	SlapsticTweak((address & 0x7ffe)/2);
}

static void __fastcall slapstic_write_byte(UINT32 address, UINT8 )
{
	SlapsticTweak((address & 0x7ffe)/2);
}

static UINT16 __fastcall slapstic_read_word(UINT32 address)
{
	UINT16 ret = *((UINT16*)(atari_slapstic_rom + (address & 0x1ffe) + ((SlapsticBank() & 3) * 0x2000)));

	SlapsticTweak((address & 0x7ffe)/2);

	return ret;
}

static UINT8 __fastcall slapstic_read_byte(UINT32 address)
{
	UINT8 ret = atari_slapstic_rom[((address & 0x1fff) ^ 1) + ((SlapsticBank() & 3) * 0x2000)];

	SlapsticTweak((address & 0x7ffe)/2);

	return ret;
}

void AtariSlapsticInit(UINT8 *rom, INT32 slapsticnum)
{
	atari_slapstic_initialized = 1;

	atari_slapstic_rom = rom;

	SlapsticInit(slapsticnum);
}

void AtariSlapsticReset()
{
	SlapsticReset();
}

void AtariSlapsticInstallMap(INT32 map_handler, UINT32 address_start)
{
	if (atari_slapstic_initialized == 0) {
		return;
	}

	SekMapHandler(map_handler,				address_start, address_start | 0x7fff, MAP_RAM);
	SekSetWriteWordHandler(map_handler,		slapstic_write_word);
	SekSetWriteByteHandler(map_handler,		slapstic_write_byte);
	SekSetReadWordHandler(map_handler,		slapstic_read_word);
	SekSetReadByteHandler(map_handler,		slapstic_read_byte);
}

void AtariSlapsticExit()
{
	SlapsticExit();
	atari_slapstic_initialized = 0;
}

INT32 AtariSlapsticScan(INT32 nAction, INT32 *)
{
	if (nAction & ACB_VOLATILE)
	{
		SlapsticScan(nAction);
	}

	return 0;
}


void AtariPaletteUpdateIRGB(UINT8 *ram, UINT32 *palette, INT32 ramsize)
{
	UINT16 *p = (UINT16*)ram;

	for (INT32 i = 0; i < ramsize/2; i++)
	{
		INT32 n = p[i] >> 15;
		UINT8 r = ((p[i] >> 9) & 0x3e) | n;
		UINT8 g = ((p[i] >> 4) & 0x3e) | n;
		UINT8 b = ((p[i] << 1) & 0x3e) | n;

		r = (r << 2) | (r >> 4);
		g = (g << 2) | (g >> 4);
		b = (b << 2) | (b >> 4);

		palette[i] = BurnHighCol(r,g,b,0);
	}
}

void AtariPaletteWriteIRGB(INT32 offset, UINT8 *ram, UINT32 *palette)
{
	UINT16 data = *((UINT16*)(ram + (offset * 2)));
	UINT8 i = data >> 15;
	UINT8 r = ((data >>  9) & 0x3e) | i;
	UINT8 g = ((data >>  4) & 0x3e) | i;
	UINT8 b = ((data <<  1) & 0x3e) | i;

	r = (r << 2) | (r >> 4);
	g = (g << 2) | (g >> 4);
	b = (b << 2) | (b >> 4);

	palette[offset] = BurnHighCol(r,g,b,0);
}

void AtariPaletteUpdate4IRGB(UINT8 *ram, UINT32 *palette, INT32 ramsize)
{
	static const UINT8 ztable[16] = {
		0x0, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9,	0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11
	};

	UINT16 *p = (UINT16*)ram;

	for (INT32 i = 0; i < ramsize/2; i++)
	{
		INT32 n = ztable[p[i] >> 12];
		UINT8 r = ((p[i] >> 8) & 0xf) * n;
		UINT8 g = ((p[i] >> 4) & 0xf) * n;
		UINT8 b = ((p[i] << 0) & 0xf) * n;

		palette[i] = BurnHighCol(r,g,b,0);
	}
}
