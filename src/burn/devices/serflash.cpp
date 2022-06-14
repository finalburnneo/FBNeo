// license:BSD-3-Clause
// copyright-holders:David Haywood, Luca Elia
/* Serial Flash Device */

#include "burnint.h"
#include "serflash.h"


// runtime state
enum { IDLE = 0, READ, READ_ID, READ_STATUS, BLOCK_ERASE, PAGE_PROGRAM };

static INT32  m_length;
static UINT8* m_region;

static UINT32 m_row_num;
static UINT16 m_flash_page_size;

static UINT8 m_flash_state;

static UINT8 m_flash_enab;

static UINT8 m_flash_cmd_seq;
static UINT32 m_flash_cmd_prev;

static UINT8 m_flash_addr_seq;
static UINT8 m_flash_read_seq;

static UINT32 m_flash_row;
static UINT16 m_flash_col;
static int m_flash_page_addr;
static UINT32 m_flash_page_index;

static UINT8 *m_flashwritemap;

static UINT8 m_last_flash_cmd;

static UINT32 m_flash_addr;

static UINT8 *m_flash_page_data;

static void flash_change_state(UINT8 state); // forward

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------
void serflash_nvram_read();
void serflash_nvram_write();

void serflash_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(m_flash_state);
		SCAN_VAR(m_flash_enab);
		SCAN_VAR(m_flash_cmd_seq);
		SCAN_VAR(m_flash_cmd_prev);
		SCAN_VAR(m_flash_addr_seq);
		SCAN_VAR(m_flash_read_seq);
		SCAN_VAR(m_flash_row);
		SCAN_VAR(m_flash_col);
		SCAN_VAR(m_flash_page_addr);
		SCAN_VAR(m_flash_page_index);

		ScanVar(m_flashwritemap, m_row_num, "FlashWriteMap");

		SCAN_VAR(m_last_flash_cmd);

		SCAN_VAR(m_flash_addr);

		ScanVar(m_flash_page_data, m_flash_page_size, "FlashPageData");
	}

	if (nAction & ACB_NVRAM) {
		if (nAction & ACB_READ) { // read from game -> write to state
			serflash_nvram_write();
		}
		if (nAction & ACB_WRITE) { // write to game <- read from state
			serflash_nvram_read();
		}
	}
}

void serflash_init(UINT8 *rom, INT32 length)
{
	m_length = length;
	m_region = rom;

	m_flash_page_size = 2048+64;
	m_row_num = m_length / m_flash_page_size;

	m_flashwritemap = (UINT8*)BurnMalloc(m_row_num);
	memset(m_flashwritemap, 0, m_row_num);

	m_flash_page_data = (UINT8*)BurnMalloc(m_flash_page_size);
	memset(m_flash_page_data, 0, m_flash_page_size);
}

void serflash_exit()
{
	BurnFree(m_flashwritemap);
	BurnFree(m_flash_page_data);
}

static void serflash_hard_reset()
{
//  logerror("%08x FLASH: RESET\n", cpuexec_describe_context(machine));

	m_flash_state = READ;

	m_flash_cmd_prev = -1;
	m_flash_cmd_seq = 0;

	m_flash_addr_seq = 0;
	m_flash_read_seq = 0;

	m_flash_row = 0;
	m_flash_col = 0;

	memset(m_flash_page_data, 0, m_flash_page_size);
	m_flash_page_addr = 0;
	m_flash_page_index = 0;
}

void serflash_reset()
{
	m_flash_enab = 0;
	serflash_hard_reset();

	m_last_flash_cmd = 0x00;
	m_flash_addr_seq = 0;
	m_flash_addr = 0;

	m_flash_page_addr = 0;
}


//-------------------------------------------------
//  nvram_read - called to read SERFLASH from the
//  .nv file
//-------------------------------------------------

#define END_MARKER 0x12345678

void serflash_nvram_read()
{
	if (m_length % m_flash_page_size) return; // region size must be multiple of flash page size
	int size = m_length / m_flash_page_size;

	{
		UINT32 page = 0xffffffff; // prevent stupidity when fbn state tallys nvram size
		SCAN_VAR(page);

		while (page < size && page != END_MARKER)
		{
			m_flashwritemap[page] = 1;
			ScanVar(m_region + page * m_flash_page_size, m_flash_page_size, "block");
			SCAN_VAR(page);
		}
	}
}


//-------------------------------------------------
//  nvram_write - called to write SERFLASH to the
//  .nv file
//-------------------------------------------------

void serflash_nvram_write()
{
	if (m_length % m_flash_page_size) return; // region size must be multiple of flash page size
	int size = m_length / m_flash_page_size;

	UINT32 page = 0;
	while (page < size)
	{
		if (m_flashwritemap[page])
		{
			SCAN_VAR(page);
			ScanVar(m_region + page * m_flash_page_size, m_flash_page_size, "block");
		}
		page++;
	}

	page = END_MARKER;

	SCAN_VAR(page);
}

void serflash_enab_write(UINT8 data)
{
	//logerror("%08x FLASH: enab = %02X\n", m_maincpu->pc(), data);
	m_flash_enab = data;
}

static void flash_change_state(UINT8 state)
{
	m_flash_state = state;

	m_flash_cmd_prev = -1;
	m_flash_cmd_seq = 0;

	m_flash_read_seq = 0;
	m_flash_addr_seq = 0;

	//logerror("flash_change_state - FLASH: state = %s\n", m_flash_state_name[state]);
}

void serflash_cmd_write(UINT8 data)
{
	if (!m_flash_enab)
		return;

	//logerror("%08x FLASH: cmd = %02X (prev = %02X)\n", m_maincpu->pc(), data, m_flash_cmd_prev);

	if (m_flash_cmd_prev == -1)
	{
		m_flash_cmd_prev = data;

		switch (data)
		{
			case 0x00:  // READ
				m_flash_addr_seq = 0;
				break;

			case 0x60:  // BLOCK ERASE
				m_flash_addr_seq = 2; // row address only
				break;

			case 0x70:  // READ STATUS
				flash_change_state( READ_STATUS );
				break;

			case 0x80:  // PAGE / CACHE PROGRAM
				m_flash_addr_seq = 0;
				// this actually seems to be set with the next 2 writes?
				m_flash_page_addr = 0;
				break;

			case 0x90:  // READ ID
				flash_change_state( READ_ID );
				break;

			case 0xff:  // RESET
				flash_change_state( IDLE );
				break;

			default:
			{
				//logerror("%s FLASH: unknown cmd1 = %02X\n", machine().describe_context(), data);
			}
		}
	}
	else
	{
		switch (m_flash_cmd_prev)
		{
			case 0x00:  // READ
				if (data == 0x30)
				{
					if (m_flash_row < m_row_num)
					{
						memcpy(m_flash_page_data, m_region + m_flash_row * m_flash_page_size, m_flash_page_size);
						m_flash_page_addr = m_flash_col;
						m_flash_page_index = m_flash_row;
					}
					flash_change_state( READ );

					//logerror("%08x FLASH: caching page = %04X\n", m_maincpu->pc(), m_flash_row);
				}
				break;

			case 0x60: // BLOCK ERASE
				if (data==0xd0)
				{
					flash_change_state( BLOCK_ERASE );
					if (m_flash_row < m_row_num)
					{
						m_flashwritemap[m_flash_row] |= 1;
						memset(m_region + m_flash_row * m_flash_page_size, 0xff, m_flash_page_size);
					}
					//bprintf(0, _T("erased block %04x (%08x - %08x)\n"), m_flash_row, m_flash_row * m_flash_page_size,  ((m_flash_row+1) * m_flash_page_size)-1);
				}
				else
				{
					//logerror("unexpected 2nd command after BLOCK ERASE\n");
				}
				break;
			case 0x80:
				if (data==0x10)
				{
					flash_change_state( PAGE_PROGRAM );
					if (m_flash_row < m_row_num)
					{
						m_flashwritemap[m_flash_row] |= (memcmp(m_region + m_flash_row * m_flash_page_size, m_flash_page_data, m_flash_page_size) != 0);
						memcpy(m_region + m_flash_row * m_flash_page_size, m_flash_page_data, m_flash_page_size);
					}
					//bprintf(0, _T("re-written block %04x (%08x - %08x)\n"), m_flash_row, m_flash_row * m_flash_page_size,  ((m_flash_row+1) * m_flash_page_size)-1);
					//bprintf(0, _T("writemap for block %x: %x\n"), m_flash_row, m_flashwritemap[m_flash_row]);
				}
				else
				{
					//logerror("unexpected 2nd command after SPAGE PROGRAM\n");
				}
				break;


			default:
			{
				//logerror("%08x FLASH: unknown cmd2 = %02X (cmd1 = %02X)\n", m_maincpu->pc(), data, m_flash_cmd_prev);
			}
		}
	}
}

void serflash_data_write(UINT8 data)
{
	if (!m_flash_enab)
		return;

	//logerror("flash data write %04x\n", m_flash_page_addr);
	if (m_flash_page_addr < m_flash_page_size)
	{
		m_flash_page_data[m_flash_page_addr] = data;
	}
	m_flash_page_addr++;
}

void serflash_addr_write(UINT8 data)
{
	if (!m_flash_enab)
		return;

	//logerror("%08x FLASH: addr = %02X (seq = %02X)\n", m_maincpu->pc(), data, m_flash_addr_seq);

	switch( m_flash_addr_seq++ )
	{
		case 0:
			m_flash_col = (m_flash_col & 0xff00) | data;
			break;
		case 1:
			m_flash_col = (m_flash_col & 0x00ff) | (data << 8);
			break;
		case 2:
			m_flash_row = (m_flash_row & 0xffff00) | data;
			if (m_row_num <= 256)
			{
				m_flash_addr_seq = 0;
			}
			break;
		case 3:
			m_flash_row = (m_flash_row & 0xff00ff) | (data << 8);
			if (m_row_num <= 65536)
			{
				m_flash_addr_seq = 0;
			}
			break;
		case 4:
			m_flash_row = (m_flash_row & 0x00ffff) | (data << 16);
			m_flash_addr_seq = 0;
			break;
	}
}

UINT8 serflash_io_read()
{
	UINT8 data = 0x00;
//  UINT32 old;

	if (!m_flash_enab)
		return 0xff;

	switch (m_flash_state)
	{
		case READ_ID:
			//old = m_flash_read_seq;

			switch( m_flash_read_seq++ )
			{
				case 0:
					data = 0xEC;    // Manufacturer
					break;
				case 1:
					data = 0xF1;    // Device
					break;
				case 2:
					data = 0x00;    // XX
					break;
				case 3:
					data = 0x15;    // Flags
					m_flash_read_seq = 0;
					break;
			}

			//logerror("%08x FLASH: read %02X from id(%02X)\n", m_maincpu->pc(), data, old);
			break;

		case READ:
			if (m_flash_page_addr > m_flash_page_size-1)
				m_flash_page_addr = m_flash_page_size-1;

			//old = m_flash_page_addr;

			data = m_flash_page_data[m_flash_page_addr++];

			//logerror("%08x FLASH: read data %02X from addr %03X (page %04X)\n", m_maincpu->pc(), data, old, m_flash_page_index);
			break;

		case READ_STATUS:
			// bit 7 = writeable, bit 6 = ready, bit 5 = ready/true ready, bit 1 = fail(N-1), bit 0 = fail
			data = 0xe0;
			//logerror("%08x FLASH: read status %02X\n", m_maincpu->pc(), data);
			break;

		default:
		{
		//  logerror("%08x FLASH: unknown read in state %s\n",0x00/*m_maincpu->pc()*/, m_flash_state_name[m_flash_state]);
		}
	}

	return data;
}

UINT8 serflash_ready_read()
{
	return 1;
}



UINT8 serflash_n3d_flash_read(INT32 offset)
{
	if (m_last_flash_cmd==0x70) return 0xe0;

	if (m_last_flash_cmd==0x00)
	{
		UINT8 retdat = m_flash_page_data[m_flash_page_addr];

		//logerror("n3d_flash_r %02x %04x\n", offset, m_flash_page_addr);

		m_flash_page_addr++;
		return retdat;
	}

	return 0x00;
}


void serflash_n3d_flash_cmd_write(INT32 offset, UINT8 data)
{
	m_last_flash_cmd = data;

	if (data==0x00)
	{
		if (m_flash_addr < m_row_num)
		{
			memcpy(m_flash_page_data, m_region + m_flash_addr * m_flash_page_size, m_flash_page_size);
		}
	}
}

void serflash_n3d_flash_addr_write(INT32 offset, UINT8 data)
{
//  logerror("n3d_flash_addr_w %02x %02x\n", offset, data);

	m_flash_addr_seq++;

	if (m_flash_addr_seq==3)
	{
		m_flash_addr = (m_flash_addr & 0xffff00) | data;
		if (m_row_num <= 256)
		{
			m_flash_addr_seq = 0;
			m_flash_page_addr = 0;
			//logerror("set flash block to %08x\n", m_flash_addr);
		}
	}
	if (m_flash_addr_seq==4)
	{
		m_flash_addr = (m_flash_addr & 0xff00ff) | data << 8;
		if (m_row_num <= 65536)
		{
			m_flash_addr_seq = 0;
			m_flash_page_addr = 0;
			//logerror("set flash block to %08x\n", m_flash_addr);
		}
	}
	if (m_flash_addr_seq==5)
	{
		m_flash_addr = (m_flash_addr & 0x00ffff) | data << 16;
		m_flash_addr_seq = 0;
		m_flash_page_addr = 0;
		//logerror("set flash block to %08x\n", m_flash_addr);
	}
}
