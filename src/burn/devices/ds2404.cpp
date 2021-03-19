/* Dallas DS2404 RTC/NVRAM */

#include "burnint.h"
#include "ds2404.h"
#include <time.h>

typedef enum {
	DS2404_STATE_IDLE = 1,				/* waiting for ROM command, in 1-wire mode */
	DS2404_STATE_COMMAND,				/* waiting for memory command */
	DS2404_STATE_ADDRESS1,				/* waiting for address bits 0-7 */
	DS2404_STATE_ADDRESS2,				/* waiting for address bits 8-15 */
	DS2404_STATE_OFFSET,				/* waiting for ending offset */
	DS2404_STATE_INIT_COMMAND,
	DS2404_STATE_READ_MEMORY,			/* Read Memory command active */
	DS2404_STATE_WRITE_SCRATCHPAD,		/* Write Scratchpad command active */
	DS2404_STATE_READ_SCRATCHPAD,		/* Read Scratchpad command active */
	DS2404_STATE_COPY_SCRATCHPAD		/* Copy Scratchpad command active */
} DS2404_STATE;

typedef struct {
	UINT16 address;
	UINT16 offset;
	UINT16 end_offset;
	UINT8 a1, a2;
	UINT8 ram[32];		/* scratchpad ram, 256 bits */
	UINT8 rtc[5];		/* 40-bit RTC counter */
	DS2404_STATE state[8];
	int state_ptr;
	UINT8 sram[512];	/* 4096 bits */
} DS2404;

static DS2404 ds2404;
INT32 ds2404_counter;

static void ds2404_rom_cmd(UINT8 cmd)
{
	switch(cmd)
	{
		case 0xcc:		/* Skip ROM */
			ds2404.state[0] = DS2404_STATE_COMMAND;
			ds2404.state_ptr = 0;
			break;

		default:
			break;
	}
}

static void ds2404_cmd(UINT8 cmd)
{
	switch(cmd)
	{
		case 0x0f:		/* Write scratchpad */
			ds2404.state[0] = DS2404_STATE_ADDRESS1;
			ds2404.state[1] = DS2404_STATE_ADDRESS2;
			ds2404.state[2] = DS2404_STATE_INIT_COMMAND;
			ds2404.state[3] = DS2404_STATE_WRITE_SCRATCHPAD;
			ds2404.state_ptr = 0;
			break;

		case 0x55:		/* Copy scratchpad */
			ds2404.state[0] = DS2404_STATE_ADDRESS1;
			ds2404.state[1] = DS2404_STATE_ADDRESS2;
			ds2404.state[2] = DS2404_STATE_OFFSET;
			ds2404.state[3] = DS2404_STATE_INIT_COMMAND;
			ds2404.state[4] = DS2404_STATE_COPY_SCRATCHPAD;
			ds2404.state_ptr = 0;
			break;

		case 0xf0:		/* Read memory */
			ds2404.state[0] = DS2404_STATE_ADDRESS1;
			ds2404.state[1] = DS2404_STATE_ADDRESS2;
			ds2404.state[2] = DS2404_STATE_INIT_COMMAND;
			ds2404.state[3] = DS2404_STATE_READ_MEMORY;
			ds2404.state_ptr = 0;
			break;

		default:
			break;
	}
}

static UINT8 ds2404_readmem(void)
{
	if( ds2404.address < 0x200 )
	{
		return ds2404.sram[ ds2404.address ];
	}
	else if( ds2404.address >= 0x202 && ds2404.address <= 0x206 )
	{
		return ds2404.rtc[ ds2404.address - 0x202 ];
	}
	return 0;
}

static void ds2404_writemem(UINT8 value)
{
	if( ds2404.address < 0x200 )
	{
		ds2404.sram[ ds2404.address ] = value;
	}
	else if( ds2404.address >= 0x202 && ds2404.address <= 0x206 )
	{
		ds2404.rtc[ ds2404.address - 0x202 ] = value;
	}
}

void ds2404_1w_reset_write(UINT8)
{
	ds2404.state[0] = DS2404_STATE_IDLE;
	ds2404.state_ptr = 0;
}

void ds2404_3w_reset_write(UINT8)
{
	ds2404.state[0] = DS2404_STATE_COMMAND;
	ds2404.state_ptr = 0;
}

UINT8 ds2404_data_read()
{
	UINT8 value;
	switch( ds2404.state[ds2404.state_ptr] )
	{
		case DS2404_STATE_IDLE:
		case DS2404_STATE_COMMAND:
		case DS2404_STATE_ADDRESS1:
		case DS2404_STATE_ADDRESS2:
		case DS2404_STATE_OFFSET:
		case DS2404_STATE_INIT_COMMAND:
			break;

		case DS2404_STATE_READ_MEMORY:
			value = ds2404_readmem();
			return value;

		case DS2404_STATE_READ_SCRATCHPAD:
			if( ds2404.offset < 0x20 ) {
				value = ds2404.ram[ds2404.offset];
				ds2404.offset++;
				return value;
			}
			break;

		case DS2404_STATE_WRITE_SCRATCHPAD:
			break;

		case DS2404_STATE_COPY_SCRATCHPAD:
			break;
	}
	return 0;
}

void ds2404_data_write(UINT8 data)
{
	int i;

	switch( ds2404.state[ds2404.state_ptr] )
	{
		case DS2404_STATE_IDLE:
			ds2404_rom_cmd(data & 0xff);
			break;

		case DS2404_STATE_COMMAND:
			ds2404_cmd(data & 0xff);
			break;

		case DS2404_STATE_ADDRESS1:
			ds2404.a1 = data & 0xff;
			ds2404.state_ptr++;
			break;

		case DS2404_STATE_ADDRESS2:
			ds2404.a2 = data & 0xff;
			ds2404.state_ptr++;
			break;

		case DS2404_STATE_OFFSET:
			ds2404.end_offset = data & 0xff;
			ds2404.state_ptr++;
			break;

		case DS2404_STATE_INIT_COMMAND:
			break;

		case DS2404_STATE_READ_MEMORY:
			break;

		case DS2404_STATE_READ_SCRATCHPAD:
			break;

		case DS2404_STATE_WRITE_SCRATCHPAD:
			if( ds2404.offset < 0x20 ) {
				ds2404.ram[ds2404.offset] = data & 0xff;
				ds2404.offset++;
			} else {
				/* Set OF flag */
			}
			break;

		case DS2404_STATE_COPY_SCRATCHPAD:
			break;
	}

	if( ds2404.state[ds2404.state_ptr] == DS2404_STATE_INIT_COMMAND ) {
		switch( ds2404.state[ds2404.state_ptr+1] )
		{
			case DS2404_STATE_IDLE:
			case DS2404_STATE_COMMAND:
			case DS2404_STATE_ADDRESS1:
			case DS2404_STATE_ADDRESS2:
			case DS2404_STATE_OFFSET:
			case DS2404_STATE_INIT_COMMAND:
				break;

			case DS2404_STATE_READ_MEMORY:
				ds2404.address = (ds2404.a2 << 8) | ds2404.a1;
				ds2404.address -= 1;
				break;

			case DS2404_STATE_WRITE_SCRATCHPAD:
				ds2404.address = (ds2404.a2 << 8) | ds2404.a1;
				ds2404.offset = ds2404.address & 0x1f;
				break;

			case DS2404_STATE_READ_SCRATCHPAD:
				ds2404.address = (ds2404.a2 << 8) | ds2404.a1;
				ds2404.offset = ds2404.address & 0x1f;
				break;

			case DS2404_STATE_COPY_SCRATCHPAD:
				ds2404.address = (ds2404.a2 << 8) | ds2404.a1;

				for( i=0; i <= ds2404.end_offset; i++ ) {
					ds2404_writemem( ds2404.ram[i] );
					ds2404.address++;
				}
				break;
		}
		ds2404.state_ptr++;
	}
}

void ds2404_clk_write(UINT8 )
{
	switch( ds2404.state[ds2404.state_ptr] )
	{
		case DS2404_STATE_READ_MEMORY:
			ds2404.address++;
			break;

		case DS2404_STATE_IDLE:
		case DS2404_STATE_COMMAND:
		case DS2404_STATE_ADDRESS1:
		case DS2404_STATE_ADDRESS2:
		case DS2404_STATE_OFFSET:
		case DS2404_STATE_INIT_COMMAND:
		case DS2404_STATE_READ_SCRATCHPAD:
		case DS2404_STATE_WRITE_SCRATCHPAD:
		case DS2404_STATE_COPY_SCRATCHPAD:
			break;
	}
}

void ds2404_timer_update() // 256hz
{
	int i;
	for( i = 0; i < 5; i++ )
	{
		ds2404.rtc[ i ]++;
		if( ds2404.rtc[ i ] != 0 )
		{
			break;
		}
	}
}

void ds2404Init(UINT8 *rawData, int ref_year, int ref_month, int ref_day)
{

	struct tm ref_tm;
	memset(&ref_tm, 0, sizeof(ref_tm));
	ref_tm.tm_year = ref_year - 1900;
	ref_tm.tm_mon = ref_month - 1;
	ref_tm.tm_mday = ref_day;
	time_t ref_time = mktime(&ref_tm);

	tm tmLocalTime;
	BurnGetLocalTime(&tmLocalTime);
	time_t current_time = mktime(&tmLocalTime);
	current_time -= ref_time;

	ds2404.rtc[ 0 ] = 0x0;
	ds2404.rtc[ 1 ] = ( current_time >> 0 ) & 0xff;
	ds2404.rtc[ 2 ] = ( current_time >> 8 ) & 0xff;
	ds2404.rtc[ 3 ] = ( current_time >> 16 ) & 0xff;
	ds2404.rtc[ 4 ] = ( current_time >> 24 ) & 0xff;

	memset (ds2404.sram, 0, 512);
	if (rawData) // rfdt2 hack
		memcpy (ds2404.sram, rawData, 32);
	
	ds2404_counter = 0;
}

INT32 ds2404_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_VOLATILE)
	{
		SCAN_VAR(ds2404.address);
		SCAN_VAR(ds2404.offset);
		SCAN_VAR(ds2404.end_offset);
		SCAN_VAR(ds2404.a1);
		SCAN_VAR(ds2404.a2);
		SCAN_VAR(ds2404.ram);
		SCAN_VAR(ds2404.rtc);
		SCAN_VAR(ds2404.state);
		SCAN_VAR(ds2404.state_ptr);
		SCAN_VAR(ds2404_counter);
	}

	if (nAction & ACB_NVRAM)
	{
		struct BurnArea ba;

		ba.Data		= ds2404.sram;
		ba.nLen		= 0x200;
		ba.nAddress	= 0;
		ba.szName	= "DS2404 SRAM";
		BurnAcb(&ba);
	}
	
	return 0;
}
