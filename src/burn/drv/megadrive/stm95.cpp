// license:BSD-3-Clause
// copyright-holders:Fabio Priuli, MetalliC
/***************************************************************************


 MegaDrive / Genesis Cart + STM95 EEPROM device


 TO DO: split STM95 to a separate device...

***************************************************************************/


#include "burnint.h"
#include "bitswap.h"
#include "m68000_intf.h"
#include "driver.h"

#define M95320_SIZE 0x1000

enum STMSTATE
{
	IDLE = 0,
	CMD_WRSR,
	CMD_RDSR,
	M95320_CMD_READ,
	CMD_WRITE,
	READING,
	WRITING
};

static UINT8 eeprom_data[M95320_SIZE];
static INT32 latch;
static INT32 reset_line;
static INT32 sck_line;
static INT32 WEL;
static INT32 stm_state;
static INT32 stream_pos;
static INT32 stream_data;
static INT32 eeprom_addr;

static UINT8 bank[3];
static INT32 rdcnt;

static UINT16 *game_rom;

static void set_cs_line(INT32 state)
{
	reset_line = state;
	if (reset_line != CLEAR_LINE)
	{
		stream_pos = 0;
		stm_state = IDLE;
	}
}

static void set_si_line(INT32 state)
{
	latch = state;
}

static INT32 get_so_line(void)
{
	if (stm_state == READING || stm_state == CMD_RDSR)
		return (stream_data >> 8) & 1;
	else
		return 0;
}

static void set_sck_line(INT32 state)
{
	if (reset_line == CLEAR_LINE)
	{
		if (state == ASSERT_LINE && sck_line == CLEAR_LINE)
		{
			switch (stm_state)
			{
				case IDLE:
					stream_data = (stream_data << 1) | (latch ? 1 : 0);
					stream_pos++;
					if (stream_pos == 8)
					{
						stream_pos = 0;
						//printf("STM95 EEPROM: got cmd %02X\n", stream_data&0xff);
						switch(stream_data & 0xff)
						{
							case 0x01:  // write status register
								if (WEL != 0)
									stm_state = CMD_WRSR;
								WEL = 0;
								break;
							case 0x02:  // write
								if (WEL != 0)
									stm_state = CMD_WRITE;
								stream_data = 0;
								WEL = 0;
								break;
							case 0x03:  // read
								stm_state = M95320_CMD_READ;
								stream_data = 0;
								break;
							case 0x04:  // write disable
								WEL = 0;
								break;
							case 0x05:  // read status register
								stm_state = CMD_RDSR;
								stream_data = WEL<<1;
								break;
							case 0x06:  // write enable
								WEL = 1;
								break;
						}
					}
					break;
				case CMD_WRSR:
					stream_pos++;       // just skip, don't care block protection
					if (stream_pos == 8)
					{
						stm_state = IDLE;
						stream_pos = 0;
					}
					break;
				case CMD_RDSR:
					stream_data = stream_data<<1;
					stream_pos++;
					if (stream_pos == 8)
					{
						stm_state = IDLE;
						stream_pos = 0;
					}
					break;
				case M95320_CMD_READ:
					stream_data = (stream_data << 1) | (latch ? 1 : 0);
					stream_pos++;
					if (stream_pos == 16)
					{
						eeprom_addr = stream_data & (M95320_SIZE - 1);
						stream_data = eeprom_data[eeprom_addr];
						stm_state = READING;
						stream_pos = 0;
					}
					break;
				case READING:
					stream_data = stream_data<<1;
					stream_pos++;
					if (stream_pos == 8)
					{
						if (++eeprom_addr == M95320_SIZE)
							eeprom_addr = 0;
						stream_data |= eeprom_data[eeprom_addr];
						stream_pos = 0;
					}
					break;
				case CMD_WRITE:
					stream_data = (stream_data << 1) | (latch ? 1 : 0);
					stream_pos++;
					if (stream_pos == 16)
					{
						eeprom_addr = stream_data & (M95320_SIZE - 1);
						stm_state = WRITING;
						stream_pos = 0;
					}
					break;
				case WRITING:
					stream_data = (stream_data << 1) | (latch ? 1 : 0);
					stream_pos++;
					if (stream_pos == 8)
					{
						eeprom_data[eeprom_addr] = stream_data;
						if (++eeprom_addr == M95320_SIZE)
							eeprom_addr = 0;
						stream_pos = 0;
					}
					break;
			}
		}
	}
	sck_line = state;
}

void md_eeprom_stm95_reset()
{
	stream_pos = 0;
	stm_state = IDLE;

	rdcnt = 0;
	bank[0] = 0;
	bank[1] = 0;
	bank[2] = 0;
}

/*-------------------------------------------------
 mapper specific handlers
 -------------------------------------------------*/

static UINT16 __fastcall read_word(UINT32 offset)
{
	offset /= 2;

	if (offset == 0x0015e6/2 || offset == 0x0015e8/2)
	{
		// ugly hack until we don't know much about game protection
		// first 3 reads from 15e6 return 0x00000010, then normal 0x00018010 value for crc check
		UINT16 res;
		offset -= 0x0015e6/2;

		if (rdcnt < 6)
		{
			rdcnt++;
			res = offset ? 0x10 : 0;
		}
		else
			res = offset ? 0x8010 : 0x0001;
		return res;
	}
	if (offset < 0x280000/2)
		return game_rom[offset];
	else    // last 0x180000 are bankswitched
	{
		UINT8 banksel = (offset - 0x280000/2) >> 18;
		return game_rom[(offset & 0x7ffff/2) + (bank[banksel] * 0x80000)/2];
	}
}

UINT16 md_psolar_rw(UINT32 offset) // dmatransaktionizationimmizer
{
	return read_word(offset);
}

static UINT8 __fastcall read_byte(UINT32 offset)
{
	if (offset & 1)
		return read_word(offset);

	return read_word(offset) >> 8;
}

static UINT16 __fastcall read_a13_word(UINT32 offset)
{
	offset = (offset/2) & 0x7f;

	if (offset == 0x0a/2)
	{
		return get_so_line() & 1;
	}
	return 0xffff;
}

static UINT8 __fastcall read_a13_byte(UINT32 offset)
{
	return read_a13_word(offset);
}

static void __fastcall write_a13_word(UINT32 offset, UINT16 data)
{
	offset = (offset/2) & 0x7f;

	if (offset < 0x08/2)
	{
		if (offset != 0) {
			bank[offset - 1] = data & 0x0f;
		}
	}
	else if (offset < 0x0a/2)
	{
		set_si_line(BIT(data, 0));
		set_sck_line(BIT(data, 1));
	//	set_halt_line(BIT(data, 2));
		set_cs_line(BIT(data, 3));
	}
}

static void __fastcall write_a13_byte(UINT32 offset, UINT8 data)
{
	write_a13_word(offset, data);
}


void md_eeprom_stm95_init(UINT8 *rom)
{
	game_rom = (UINT16*)rom;

	SekOpen(0);

	// unmap everything except vectors
	for (INT32 i = 0x000; i < 0xa00000; i+= 0x400) {
		SekMapMemory(NULL,	i, i+0x3ff, MAP_RAM);
	}

	SekMapHandler(5,		0x000000, 0x9fffff, MAP_ROM);
	SekSetReadByteHandler (5, 	read_byte);
	SekSetReadWordHandler (5, 	read_word);

	SekMapHandler(6,		0xa13000, 0xa130ff, MAP_RAM);
	SekSetReadByteHandler (6, 	read_a13_byte);
	SekSetReadWordHandler (6, 	read_a13_word);
	SekSetWriteByteHandler(6, 	write_a13_byte);
	SekSetWriteWordHandler(6, 	write_a13_word);
	
	SekClose();
}

void md_eeprom_stm95_scan(INT32 nAction)
{
	struct BurnArea ba;

	if (nAction & ACB_NVRAM) {
		ba.Data		= eeprom_data;
		ba.nLen		= M95320_SIZE;
		ba.nAddress	= 0xa13000;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {

		SCAN_VAR(latch);
		SCAN_VAR(reset_line);
		SCAN_VAR(sck_line);
		SCAN_VAR(WEL);
		SCAN_VAR(stm_state);
		SCAN_VAR(stream_pos);
		SCAN_VAR(stream_data);
		SCAN_VAR(eeprom_addr);

		SCAN_VAR(bank);
		SCAN_VAR(rdcnt);
	}
}
