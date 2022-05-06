// license:BSD-3-Clause
// copyright-holders:Angelo Salese, David Haywood
/***************************************************************************

    rtc9701.c

    Epson RTC-9701-JE

    Serial Real Time Clock + EEPROM


***************************************************************************/

#include "burnint.h"
#include "rtc9701.h"

enum {
	CMD_WAIT = 0,
	RTC_READ,
	RTC_WRITE,
	EEPROM_READ,
	EEPROM_WRITE,
	AFTER_WRITE_ENABLE
};

struct regs_t
{
	UINT8 sec, min, hour, day, wday, month, year;
};

static int m_latch;
static int m_reset_line;
static int m_clock_line;

static UINT8 rtc_state;
static int cmd_stream_pos;
static int current_cmd;

static int rtc9701_address_pos;
static int rtc9701_current_address;

static UINT16 rtc9701_current_data;
static int rtc9701_data_pos;

static UINT16 rtc9701_data[0x100];

static regs_t m_rtc;

static UINT32 framenum;

void rtc9701_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(m_latch);
		SCAN_VAR(m_reset_line);
		SCAN_VAR(m_clock_line);
		SCAN_VAR(rtc_state);
		SCAN_VAR(cmd_stream_pos);
		SCAN_VAR(current_cmd);
		SCAN_VAR(rtc9701_address_pos);
		SCAN_VAR(rtc9701_current_address);
		SCAN_VAR(rtc9701_current_data);
		SCAN_VAR(rtc9701_data_pos);
		SCAN_VAR(m_rtc);
		SCAN_VAR(framenum);
	}

	if (nAction & ACB_NVRAM) {
		SCAN_VAR(rtc9701_data);
	}
}


void rtc9701_once_per_frame()
{
	framenum++;
	if ((framenum % 60) == 59)
	{
		static const UINT8 dpm[12] = { 0x31, 0x28, 0x31, 0x30, 0x31, 0x30, 0x31, 0x31, 0x30, 0x31, 0x30, 0x31 };
		int dpm_count;

		m_rtc.sec++;

		if((m_rtc.sec & 0x0f) >= 0x0a)              { m_rtc.sec+=0x10; m_rtc.sec&=0xf0; }
		if((m_rtc.sec & 0xf0) >= 0x60)              { m_rtc.min++; m_rtc.sec = 0; }
		if((m_rtc.min & 0x0f) >= 0x0a)              { m_rtc.min+=0x10; m_rtc.min&=0xf0; }
		if((m_rtc.min & 0xf0) >= 0x60)              { m_rtc.hour++; m_rtc.min = 0; }
		if((m_rtc.hour & 0x0f) >= 0x0a)             { m_rtc.hour+=0x10; m_rtc.hour&=0xf0; }
		if((m_rtc.hour & 0xff) >= 0x24)             { m_rtc.day++; m_rtc.wday<<=1; m_rtc.hour = 0; }
		if(m_rtc.wday & 0x80)                       { m_rtc.wday = 1; }
		if((m_rtc.day & 0x0f) >= 0x0a)              { m_rtc.day+=0x10; m_rtc.day&=0xf0; }

		/* TODO: crude leap year support */
		dpm_count = (m_rtc.month & 0xf) + (((m_rtc.month & 0x10) >> 4)*10)-1;

		if(((m_rtc.year % 4) == 0) && m_rtc.month == 2)
		{
			if((m_rtc.day & 0xff) >= dpm[dpm_count]+1+1)
			{ m_rtc.month++; m_rtc.day = 0x01; }
		}
		else if((m_rtc.day & 0xff) >= dpm[dpm_count]+1){ m_rtc.month++; m_rtc.day = 0x01; }
		if((m_rtc.month & 0x0f) >= 0x0a)            { m_rtc.month = 0x10; }
		if(m_rtc.month >= 0x13)                     { m_rtc.year++; m_rtc.month = 1; }
		if((m_rtc.year & 0x0f) >= 0x0a)             { m_rtc.year+=0x10; m_rtc.year&=0xf0; }
		if((m_rtc.year & 0xf0) >= 0xa0)             { m_rtc.year = 0; } //2000-2099 possible timeframe
	}
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void rtc9701_init()
{
	tm time;
	BurnGetLocalTime(&time);

	m_rtc.day = ((time.tm_mday / 10)<<4) | ((time.tm_mday % 10) & 0xf);
	m_rtc.month = (((time.tm_mon+1) / 10) << 4) | (((time.tm_mon+1) % 10) & 0xf);
	m_rtc.wday = 1 << time.tm_wday;
	m_rtc.year = (((time.tm_year % 100)/10)<<4) | ((time.tm_year % 10) & 0xf);
	m_rtc.hour = ((time.tm_hour / 10)<<4) | ((time.tm_hour % 10) & 0xf);
	m_rtc.min = ((time.tm_min / 10)<<4) | ((time.tm_min % 10) & 0xf);
	m_rtc.sec = ((time.tm_sec / 10)<<4) | ((time.tm_sec % 10) & 0xf);

	rtc_state = CMD_WAIT;
	cmd_stream_pos = 0;
	current_cmd = 0;

	framenum = 0;

	for (INT32 offs = 0; offs < 0x100; offs++)
		rtc9701_data[offs] = 0xffff;
}

void rtc9701_exit()
{
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void rtc9701_reset()
{
	rtc_state = CMD_WAIT;
	cmd_stream_pos = 0;
	current_cmd = 0;
	rtc9701_address_pos = 0;
	rtc9701_current_address = 0;
	rtc9701_current_data = 0;
	rtc9701_data_pos = 0;
	m_latch = 0;
	m_reset_line = 0;
	m_clock_line = 0;
}



//-------------------------------------------------
//  rtc_read - used to route RTC reading registers
//-------------------------------------------------

static UINT8 rtc_read(UINT8 offset)
{
	UINT8 res;

	res = 0;

	switch(offset)
	{
		case 0: res = m_rtc.sec; break;
		case 1: res = m_rtc.min; break;
		case 2: res = m_rtc.hour; break;
		case 3: res = m_rtc.wday; break; /* untested */
		case 4: res = m_rtc.day; break;
		case 5: res = m_rtc.month; break;
		case 6: res = m_rtc.year & 0xff; break;
		case 7: res = 0x20; break;
	}

	return res;
}

static void rtc_write(UINT8 offset, UINT8 data)
{
	switch(offset)
	{
		case 0: m_rtc.sec = data; break;
		case 1: m_rtc.min = data; break;
		case 2: m_rtc.hour = data; break;
		case 3: m_rtc.wday = data; break; /* untested */
		case 4: m_rtc.day = data; break;
		case 5: m_rtc.month = data; break;
		case 6: m_rtc.year = data; break;
		case 7: break; // NOP
	}
}

//**************************************************************************
//  READ/WRITE HANDLERS
//**************************************************************************

void rtc9701_write_bit(UINT8 data)
{
	m_latch = data & 1;
}


UINT8 rtc9701_read_bit()
{
	if (rtc_state == RTC_READ)
	{
		//printf("RTC data bits left c9701_data_pos %02x\n", rtc9701_data_pos);
		return ((rtc9701_current_data) >> (rtc9701_data_pos-1))&1;

	}
	else if (rtc_state == EEPROM_READ)
	{
		//printf("EEPROM data bits left c9701_data_pos %02x\n", rtc9701_data_pos);
		return ((rtc9701_current_data) >> (rtc9701_data_pos-1))&1;

	}
	else
	{
		//printf("read something else (status?) %02x\n", rtc9701_data_pos);
	}



	return 0;
}


void rtc9701_set_cs_line(UINT8 data)
{
	//logerror("set reset line %d\n",data);
	m_reset_line = data;

	if (m_reset_line != 0)
	{
		rtc_state = CMD_WAIT;
		cmd_stream_pos = 0;
		current_cmd = 0;
		rtc9701_address_pos = 0;
		rtc9701_current_address = 0;
		rtc9701_current_data = 0;
		rtc9701_data_pos = 0;

	}
}


void rtc9701_set_clock_line(UINT8 data)
{
	//logerror("set clock line %d\n",data);

	if (m_reset_line == 0)
	{
		if (data==1)
		{
			//logerror("write latched bit %d\n",m_latch);

			switch (rtc_state)
			{
				case CMD_WAIT:

					//logerror("xx\n");
					current_cmd = (current_cmd << 1) | (m_latch&1);
					cmd_stream_pos++;

					if (cmd_stream_pos==4)
					{
						cmd_stream_pos = 0;
						//logerror("Comamnd is %02x\n", current_cmd);

						if (current_cmd==0x00) /* 0000 */
						{
							//logerror("WRITE RTC MODE\n");
							rtc_state = RTC_WRITE;
							cmd_stream_pos = 0;
							rtc9701_address_pos = 0;
							rtc9701_current_address = 0;
							rtc9701_data_pos = 0;
							rtc9701_current_data = 0;
						}
						else if (current_cmd==0x02) /* 0010 */
						{
							//logerror("WRITE EEPROM MODE\n");
							rtc_state = EEPROM_WRITE;
							cmd_stream_pos = 0;
							rtc9701_address_pos = 0;
							rtc9701_current_address = 0;
							rtc9701_data_pos = 0;
							rtc9701_current_data = 0;

						}
						else if (current_cmd==0x06) /* 0110 */
						{
							//logerror("WRITE ENABLE\n");
							rtc_state = AFTER_WRITE_ENABLE;
							cmd_stream_pos = 0;
						}
						else if (current_cmd==0x08) /* 1000 */
						{
							//logerror("READ RTC MODE\n");
							rtc_state = RTC_READ;
							cmd_stream_pos = 0;
							rtc9701_address_pos = 0;
							rtc9701_current_address = 0;
							rtc9701_data_pos = 0;
							rtc9701_current_data = 0;
						}
						else if (current_cmd==0x0a) /* 1010 */
						{
							//logerror("READ EEPROM MODE\n");
							rtc_state = EEPROM_READ;
							cmd_stream_pos = 0;
							rtc9701_address_pos = 0;
							rtc9701_current_address = 0;
							rtc9701_data_pos = 0;
							rtc9701_current_data = 0;


						}
						else
						{
							//logerror("RTC9701 UNKNOWN MODE\n");
						}

						current_cmd = 0;
					}
					break;

				case AFTER_WRITE_ENABLE:
					cmd_stream_pos++;
					if (cmd_stream_pos==12)
					{
						cmd_stream_pos = 0;
						//logerror("Written 12 bits, going back to WAIT mode\n");
						rtc_state = CMD_WAIT;
					}
					break;

				case RTC_WRITE:
					cmd_stream_pos++;
					if (cmd_stream_pos<=4)
					{
						rtc9701_address_pos++;
						rtc9701_current_address = (rtc9701_current_address << 1) | (m_latch&1);
						if (cmd_stream_pos==4)
						{
							//printf("Set RTC Write Address To %04x\n", rtc9701_current_address );
						}
					}

					if (cmd_stream_pos>4)
					{
						rtc9701_data_pos++;
						rtc9701_current_data = (rtc9701_current_data << 1) | (m_latch&1);
					}

					if (cmd_stream_pos==12)
					{
						cmd_stream_pos = 0;
						rtc_write(rtc9701_current_address,rtc9701_current_data);
						//logerror("Written 12 bits, going back to WAIT mode\n");
						rtc_state = CMD_WAIT;
					}
					break;



				case EEPROM_READ:
					cmd_stream_pos++;
					if (cmd_stream_pos<=12)
					{
						rtc9701_address_pos++;
						rtc9701_current_address = (rtc9701_current_address << 1) | (m_latch&1);
						if (cmd_stream_pos==12)
						{
							//printf("Set EEPROM Read Address To %04x - ", (rtc9701_current_address>>1)&0xff );
							rtc9701_current_data = rtc9701_data[(rtc9701_current_address>>1)&0xff];
							//printf("Setting data latch for reading to %04x\n", rtc9701_current_data);
							rtc9701_data_pos = 16;
						}
					}

					if (cmd_stream_pos>12)
					{
						rtc9701_data_pos--;

					}

					if (cmd_stream_pos==28)
					{
						cmd_stream_pos = 0;
					//  //logerror("accesed 28 bits, going back to WAIT mode\n");
					//  rtc_state = CMD_WAIT;
					}
					break;



				case EEPROM_WRITE:
					cmd_stream_pos++;

					if (cmd_stream_pos<=12)
					{
						rtc9701_address_pos++;
						rtc9701_current_address = (rtc9701_current_address << 1) | (m_latch&1);
						if (cmd_stream_pos==12)
						{
							//printf("Set EEPROM Write Address To %04x\n", rtc9701_current_address );
						}
					}

					if (cmd_stream_pos>12)
					{
						rtc9701_data_pos++;
						rtc9701_current_data = (rtc9701_current_data << 1) | (m_latch&1);
					}

					if (cmd_stream_pos==28)
					{
						cmd_stream_pos = 0;
						//printf("written 28 bits - writing data %04x to %04x and going back to WAIT mode\n", rtc9701_current_data, (rtc9701_current_address>>1)&0xff);
						rtc9701_data[(rtc9701_current_address>>1)&0xff] = rtc9701_current_data;
						rtc_state = CMD_WAIT;
					}
					break;

				case RTC_READ:
					cmd_stream_pos++;
					if (cmd_stream_pos<=4)
					{
						rtc9701_address_pos++;
						rtc9701_current_address = (rtc9701_current_address << 1) | (m_latch&1);
						if (cmd_stream_pos==4)
						{
							//printf("Set RTC Read Address To %04x\n", rtc9701_current_address );
							rtc9701_current_data = rtc_read(rtc9701_current_address);
							//printf("Setting data latch for reading to %04x\n", rtc9701_current_data);
							rtc9701_data_pos = 8;
						}
					}

					if (cmd_stream_pos>4)
					{
						rtc9701_data_pos--;
					}

					if (cmd_stream_pos==12)
					{
						cmd_stream_pos = 0;
					//  //logerror("accessed 12 bits, going back to WAIT mode\n");
					//  rtc_state = CMD_WAIT;
					}
					break;


				default:
					break;

			}
		}
	}
}
