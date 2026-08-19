// FB Neo PC-Engine CD driver module
// Based on MAME/MESS driver by Wilbert Pol & Angelo Salese

#include "burnint.h"
#include "h6280_intf.h"
#include "bitswap.h"
#include "pce_cd.h"
#include "cd_interface.h"

static UINT8 bram_locked = 1;

static UINT8 *PCECDBRAM;
static UINT8 *PCEADPCMRAM;
static UINT8 *PCECDRAM;
static UINT8 *PCESuperRAM;
static UINT8 *PCEAcardRAM;

static dtimer cd_ack_clear_timer;

struct acard_port_t {
	UINT8  ctrl;
	UINT32 base_addr;
	UINT16 addr_offset;
	UINT16 addr_inc;

	UINT32 ram_addr() const
	{
		if (ctrl & 0x02)
			return (base_addr + addr_offset + ((ctrl & 0x08) ? 0xff0000 : 0)) & 0x1fffff;
		else
			return base_addr & 0x1fffff;
	}

	void addr_increment()
	{
		if (ctrl & 0x01) {
			if (ctrl & 0x10) {
				base_addr += addr_inc;
				base_addr &= 0xffffff;
			} else {
				addr_offset += addr_inc;
			}
		}
	}

	void adjust_addr()
	{
		base_addr += addr_offset + ((ctrl & 0x08) ? 0xff0000 : 0);
		base_addr &= 0xffffff;
	}
};
static acard_port_t acard_port[4];
static UINT32 acard_shift;
static UINT8  acard_shift_reg;
static UINT8  acard_rotate_reg;

static UINT8 dec_2_bcd(UINT8 v) { return ((v / 10) << 4) | (v % 10); }
static UINT8 bcd_2_dec(UINT8 v) { return ((v >> 4) * 10) + (v & 0x0f); }

// IRQ2 cause bits (register 0x02 mask / composed into irq_status)
#define PCE_CD_IRQ_TRANSFER_READY   0x40
#define PCE_CD_IRQ_TRANSFER_DONE    0x20
#define PCE_CD_IRQ_BRAM             0x10
#define PCE_CD_IRQ_SAMPLE_FULL_PLAY 0x08
#define PCE_CD_IRQ_SAMPLE_HALF_PLAY 0x04

enum { PCE_CD_CDDA_OFF = 0, PCE_CD_CDDA_PLAYING, PCE_CD_CDDA_PAUSED };

// discrete SCSI bus lines
static UINT8 scsi_BSY, scsi_SEL, scsi_CD, scsi_IO, scsi_MSG, scsi_REQ, scsi_ACK, scsi_ATN;
static UINT8 scsi_RST, scsi_last_RST;
static UINT8 cd_motor_on;
static UINT8 cd_selected;

#define PCE_CD_COMMAND_BUFFER_SIZE 16
static UINT8 cd_command_buffer[PCE_CD_COMMAND_BUFFER_SIZE];
static INT32 cd_command_buffer_index;

static UINT8 cd_status_sent, cd_message_after_status, cd_message_sent;

#define PCE_CD_DATA_BUFFER_SIZE 2352
static UINT8 cd_data_buffer[PCE_CD_DATA_BUFFER_SIZE];
static INT32 cd_data_buffer_size;
static INT32 cd_data_buffer_index;
static UINT8 cd_data_transferred;

static INT32 cd_current_frame, cd_end_frame, cd_last_frame;
static UINT8 cd_cdda_status, cd_cdda_play_mode;
static UINT8 cd_end_mark;
static double cd_cdda_volume = 100.0, cd_adpcm_volume = 100.0;

#define PCE_CD_TICKS_PER_SEC (262 * 60)
static double cd_cdda_fade_step = 0.0, cd_adpcm_fade_step = 0.0;
static UINT8 cd_cdda_fade_active = 0, cd_adpcm_fade_active = 0;
static UINT8 adpcm_dma_active;

#define PCE_CD_CLOCK 9216000

// register-visible state
static UINT8 cd_reset_reg;
static UINT8 cd_irq_mask, cd_irq_status;
static UINT8 cd_cdc_status, cd_cdc_data;
static UINT8 cd_bram_status;
static UINT8 cd_adpcm_status;
static UINT16 cd_adpcm_latch_address;
static UINT8 cd_adpcm_control;
static UINT8 adpcm_rate;
static UINT8 cd_fader_ctrl;
static UINT8 cd_adpcm_dma_reg;

static UINT16 adpcm_read_ptr, adpcm_write_ptr;
static UINT8  adpcm_read_buf, adpcm_write_buf;
static UINT16 adpcm_length;

static UINT32 msm_start_addr, msm_end_addr, msm_half_addr;
static UINT8  msm_nibble;
static UINT8  msm_repeat;
static UINT8  msm_idle;

#define PCE_IRQ2_LINE 1

static void cd_set_irq_line(INT32 num, INT32 state)
{
	if (state) {
		cd_irq_status |= num;
	} else {
		cd_irq_status &= ~num;
	}

	INT32 asserted = (cd_irq_mask & cd_irq_status & 0x7c) ? 1 : 0;

	static INT32 last_asserted = -1;
	if (asserted != last_asserted) {
		last_asserted = asserted;
	}

	if (asserted) {
		h6280SetIRQLine(PCE_IRQ2_LINE, CPU_IRQSTATUS_ACK);
	} else {
		h6280SetIRQLine(PCE_IRQ2_LINE, CPU_IRQSTATUS_NONE);
	}
}

static void cd_reply_status_byte(UINT8 status)
{
	// Setting CD in reply_status_byte
	scsi_CD = scsi_IO = scsi_REQ = 1;
	scsi_MSG = 0;
	cd_message_after_status = 1;
	cd_status_sent = cd_message_sent = 0;

	cd_cdc_data = (status == 0x00) ? 0x00 : 0x01;
}

/* 0x00 - TEST UNIT READY */
static void cd_test_unit_ready()
{
	cd_reply_status_byte(0x00);
}

// Read one CD sector, handle success/error status & IRQs
static INT32 cd_read_sector()
{
	// Attempt to read one data sector from CD image
	if (CDEmuReadDataSector(cd_current_frame, cd_data_buffer)) {
		// Sector read failed: reset buffers, report SCSI CHECK CONDITION (0x02)
		cd_data_buffer_size  = 0;
		cd_data_buffer_index = 0;
		cd_data_transferred  = 0;
		cd_set_irq_line(PCE_CD_IRQ_TRANSFER_READY, 0);
		cd_reply_status_byte(0x02);
		cd_set_irq_line(PCE_CD_IRQ_TRANSFER_DONE,  1);
		return 1;	// Return 1 = read error
	}

	// Sector read succeeded, setup buffer and state
	cd_data_buffer_size  = 2048;
	cd_data_buffer_index = 0;
	cd_current_frame++;
	scsi_IO = 1;
	scsi_CD = 0;
	cd_data_transferred = (cd_current_frame == cd_end_frame);
	if (cd_data_transferred) {
		cd_cdda_status = PCE_CD_CDDA_PAUSED;
	}
	cd_set_irq_line(PCE_CD_IRQ_TRANSFER_READY, 1);
	return 0;	// Return 0 = read ok
}

/* 0x08 - READ (6) */
static void cd_read_6()
{
	UINT32 frame = ((cd_command_buffer[1] & 0x1f) << 16) | (cd_command_buffer[2] << 8) | cd_command_buffer[3];
	UINT32 frame_count = cd_command_buffer[4] ? cd_command_buffer[4] : 256;

	if (cd_cdda_status != PCE_CD_CDDA_OFF) {
		cd_cdda_status = PCE_CD_CDDA_OFF;
		CDEmuStop();
		cd_end_mark = 0;
	}

	cd_current_frame = frame;
	cd_end_frame = frame + frame_count;
	cd_motor_on = 1;
	cd_read_sector();
}

/* 0xD8 - SET AUDIO PLAYBACK START POSITION (NEC) */
static void cd_nec_set_audio_start_position()
{
	UINT32 frame = 0;
	UINT8 mode = cd_command_buffer[9] & 0xc0;

	switch (mode) {
		case 0x00:
			frame = (cd_command_buffer[3] << 16) | (cd_command_buffer[4] << 8) | cd_command_buffer[5];
			break;
		case 0x40: {
			UINT8 m = bcd_2_dec(cd_command_buffer[2]);
			UINT8 s = bcd_2_dec(cd_command_buffer[3]);
			UINT8 f = bcd_2_dec(cd_command_buffer[4]);
			frame = f + 75 * (s + m * 60);
			break;
		}
		case 0x80: {
			UINT8 track = cd_command_buffer[2];
			UINT8 *toc = CDEmuReadTOC(track);
			UINT8 m = bcd_2_dec(toc[0]);
			UINT8 s = bcd_2_dec(toc[1]);
			UINT8 f = bcd_2_dec(toc[2]);
			frame = f + 75 * (s + m * 60);
			break;
		}
		default:
			break;
	}

	cd_current_frame = frame;
	cd_cdda_status = PCE_CD_CDDA_PAUSED;

	{
		UINT8 play_mode = cd_command_buffer[1] & 0x03;
		if (play_mode) {
			// Required by audio CD player
			cd_motor_on = 1;
			cd_cdda_status = PCE_CD_CDDA_PLAYING;
			cd_end_frame = cd_last_frame;
			UINT32 msf = frame;
			UINT8 m = dec_2_bcd((msf / 75) / 60);
			UINT8 s = dec_2_bcd((msf / 75) % 60);
			UINT8 f = dec_2_bcd(msf % 75);
			CDEmuPlay(m, s, f);
			cd_cdda_play_mode = (play_mode & 0x02) ? 2 : 3;
			cd_end_mark = (play_mode & 0x02) ? 1 : 0;
		} else {
			// Several places definitely don't want this to start redbook,
			// it's done later with 0xd9 command.
			// - fzone2 / fzone2j
			// - draculax (stage 2' pre-boss)
			// - manhole (fires this during Sunsoft logo but expects playback on successive
			//            credit sequence instead)
			//if (m_end_frame > m_current_frame)
			//  m_cdda->start_audio(m_current_frame, m_end_frame - m_current_frame);

			// These ones additionally wants a CDDA pause issued:
			// - audio CD player ("fade out" button trigger, otherwise will playback the
			//                    very next track at the end of the sequence)
			// - ppersia (picking up sword in stage 1, cancels then restarts redbook BGM)
			CDEmuPause();
			cd_cdda_play_mode = 3;
			cd_end_mark = 0;

			// snatcher requires that the irq is sent here
			// otherwise it will hang on title screen
			cd_set_irq_line(PCE_CD_IRQ_TRANSFER_DONE, 1);
		}
	}

	cd_reply_status_byte(0x00);
}

/* 0xD9 - SET AUDIO PLAYBACK END POSITION (NEC) */
static void cd_nec_set_audio_stop_position()
{
	UINT32 frame = 0;
	UINT8 mode = cd_command_buffer[9] & 0xc0;

	switch (mode) {
		case 0x00:
			frame = (cd_command_buffer[3] << 16) | (cd_command_buffer[4] << 8) | cd_command_buffer[5];
			break;
		case 0x40: {
			UINT8 m = bcd_2_dec(cd_command_buffer[2]);
			UINT8 s = bcd_2_dec(cd_command_buffer[3]);
			UINT8 f = bcd_2_dec(cd_command_buffer[4]);
			frame = f + 75 * (s + m * 60);
			break;
		}
		case 0x80: {
			// NB: crazyhos uses this command with track = 1 on pre-title screen intro.
			// It's not supposed to playback anything according to real HW refs.
			UINT8 track = cd_command_buffer[2];
			UINT8 *toc = CDEmuReadTOC(track);
			UINT8 m = bcd_2_dec(toc[0]);
			UINT8 s = bcd_2_dec(toc[1]);
			UINT8 f = bcd_2_dec(toc[2]);
			frame = f + 75 * (s + m * 60);
			break;
		}
		default:
			break;
	}

	cd_end_frame = frame;
	cd_cdda_play_mode = cd_command_buffer[1] & 0x03;

	if (cd_cdda_play_mode) {
		cd_motor_on = 1;
		if (cd_cdda_status == PCE_CD_CDDA_PAUSED) {
			CDEmuResume();
		}
		UINT32 msf = cd_current_frame;
		UINT8 m = dec_2_bcd((msf / 75) / 60);
		UINT8 s = dec_2_bcd((msf / 75) % 60);
		UINT8 f = dec_2_bcd(msf % 75);
		CDEmuPlay(m, s, f);
		cd_end_mark = 1;
		cd_cdda_status = PCE_CD_CDDA_PLAYING;
	} else {
		cd_motor_on = 0;
		cd_cdda_status = PCE_CD_CDDA_OFF;
		CDEmuStop();
		cd_end_frame = cd_last_frame;
		cd_end_mark = 0;
	}

	cd_reply_status_byte(0x00);
}

/* 0xDA - PAUSE (NEC) */
static void cd_nec_pause()
{
	if (cd_cdda_status == PCE_CD_CDDA_OFF) {
		cd_reply_status_byte(0x02);
		return;
	}

	cd_cdda_status = PCE_CD_CDDA_PAUSED;
	cd_current_frame = CDEmuGetCurrentLBA();
	CDEmuPause();
	cd_reply_status_byte(0x00);
}

/* 0xDD - READ SUBCHANNEL Q (NEC) */
static void cd_nec_get_subq()
{
	UINT8 status_byte;
	switch (cd_cdda_status) {
		case PCE_CD_CDDA_PAUSED:  status_byte = 2; break;
		case PCE_CD_CDDA_PLAYING: status_byte = 0; break;
		default:                  status_byte = 3; break;
	}

	UINT8 *q = CDEmuReadQChannel();

	cd_data_buffer[0] = status_byte;
	cd_data_buffer[1] = 0x01 | ((q[7] & 0x04) ? 0x40 : 0x00);
	// track
	cd_data_buffer[2] = q[0];
	// index
	cd_data_buffer[3] = 1;
	// MSF (relative)
	cd_data_buffer[4] = q[4];
	cd_data_buffer[5] = q[5];
	cd_data_buffer[6] = q[6];
	// MSF (absolute)
	cd_data_buffer[7] = q[1];
	cd_data_buffer[8] = q[2];
	cd_data_buffer[9] = q[3];

	cd_data_buffer_size = 10;
	cd_data_buffer_index = 0;
	cd_data_transferred = 1;
	scsi_IO = 1;
	scsi_CD = 0;
}

/* 0xDE - GET DIR INFO (NEC) */
static void cd_nec_get_dir_info()
{
	switch (cd_command_buffer[1]) {
		case 0x00: {
			UINT8 *toc = CDEmuReadTOC(CDEmuTOC_FIRSTLAST);
			cd_data_buffer[0] = toc[0];
			cd_data_buffer[1] = toc[1];
			cd_data_buffer_size = 2;
			break;
		}
		case 0x01: {
			UINT8 *toc = CDEmuReadTOC(CDEmuTOC_LASTMSF);
			// M
			cd_data_buffer[0] = toc[0];
			// S
			cd_data_buffer[1] = toc[1];
			// F
			cd_data_buffer[2] = toc[2];
			cd_data_buffer_size = 3;
			break;
		}
		case 0x02: {
			if (cd_command_buffer[2] == 0xaa) {
				UINT8 *toc = CDEmuReadTOC(CDEmuTOC_LASTMSF);
				cd_data_buffer[0] = toc[0];
				cd_data_buffer[1] = toc[1];
				cd_data_buffer[2] = toc[2];
				cd_data_buffer[3] = toc[3];
				cd_data_buffer_size = 4;
				break;
			}
			UINT8 *toc = CDEmuReadTOC(cd_command_buffer[2]);
			// M
			cd_data_buffer[0] = toc[0];
			// S
			cd_data_buffer[1] = toc[1];
			// F
			cd_data_buffer[2] = toc[2];
			cd_data_buffer[3] = toc[3];
			cd_data_buffer_size = 4;
			break;
		}
		default:
			break;
	}

	cd_data_buffer_index = 0;
	cd_data_transferred = 1;
	scsi_IO = 1;
	scsi_CD = 0;
}

static void cd_end_of_list()
{
	cd_reply_status_byte(0x02);
}

typedef void (*cd_command_handler_func)();

static void cd_handle_data_output()
{
	static const struct {
		UINT8 command_byte;
		UINT8 command_size;
		cd_command_handler_func command_handler;
	} pce_cd_commands[] = {
		{ 0x00, 6, cd_test_unit_ready },
		{ 0x08, 6, cd_read_6 },
		{ 0xd8,10, cd_nec_set_audio_start_position },
		{ 0xd9,10, cd_nec_set_audio_stop_position },
		{ 0xda,10, cd_nec_pause },
		{ 0xdd,10, cd_nec_get_subq },
		{ 0xde,10, cd_nec_get_dir_info },
		{ 0xff, 1, cd_end_of_list }
	};
	const INT32 num_commands = sizeof(pce_cd_commands) / sizeof(pce_cd_commands[0]);

	if (scsi_REQ && scsi_ACK) {
		/* Command byte received */
		/* Check for buffer overflow */
		if (cd_command_buffer_index < PCE_CD_COMMAND_BUFFER_SIZE) {
			cd_command_buffer[cd_command_buffer_index++] = cd_cdc_data;
		}
		scsi_REQ = 0;
	}

	if (!scsi_REQ && !scsi_ACK && cd_command_buffer_index) {
		INT32 i;
		for (i = 0; i < num_commands - 1 && cd_command_buffer[0] > pce_cd_commands[i].command_byte; i++);

		/* Check for unknown commands */
		if (cd_command_buffer[0] != pce_cd_commands[i].command_byte) {
			i = num_commands - 1;
		}

		if (cd_command_buffer_index == pce_cd_commands[i].command_size) {
			pce_cd_commands[i].command_handler();
			cd_command_buffer_index = 0;
		} else {
			scsi_REQ = 1;
		}
	}
}

static void cd_handle_data_input()
{
	if (scsi_CD) {
		/* Command / Status byte */
		if (scsi_REQ && scsi_ACK) {
			// status sent
			scsi_REQ = 0;
			cd_status_sent = 1;
		}
		if (!scsi_REQ && !scsi_ACK && cd_status_sent) {
			cd_status_sent = 0;
			if (cd_message_after_status) {
				// message after status
				cd_message_after_status = 0;
				scsi_MSG = scsi_REQ = 1;
				cd_cdc_data = 0;
			}
		}
	} else {
		/* Data */
		if (scsi_REQ && scsi_ACK) {
			scsi_REQ = 0;
		}
		if (!scsi_REQ && !scsi_ACK) {
			if (cd_data_buffer_index == cd_data_buffer_size) {
				cd_set_irq_line(PCE_CD_IRQ_TRANSFER_READY, 0);
				if (cd_data_transferred) {
					cd_data_transferred = 0;
					cd_reply_status_byte(0x00);
					cd_set_irq_line(PCE_CD_IRQ_TRANSFER_DONE, 1);
				} else {
					cd_read_sector();
				}
			} else {
				cd_cdc_data = cd_data_buffer[cd_data_buffer_index];
				cd_data_buffer_index++;
				scsi_REQ = 1;
			}
		}
	}
}

static void cd_handle_message_output()
{
	if (scsi_REQ && scsi_ACK) {
		scsi_REQ = 0;
	}
}

static void cd_handle_message_input()
{
	if (scsi_REQ && scsi_ACK) {
		scsi_REQ = 0;
		cd_message_sent = 1;
	}
	if (!scsi_REQ && !scsi_ACK && cd_message_sent) {
		cd_message_sent = 0;
		scsi_BSY = 0;
	}
}

/* Update internal CD statuses */
static void cd_update()
{
	/* Check for reset of CD unit */
	if (scsi_RST != scsi_last_RST) {
		if (scsi_RST) {
			// Performing CD reset
			/* Reset internal data */
			scsi_BSY = scsi_SEL = scsi_CD = scsi_IO = 0;
			scsi_MSG = scsi_REQ = scsi_ATN = 0;
			cd_motor_on = 0;
			cd_selected = 0;
			cd_cdda_status = PCE_CD_CDDA_OFF;
			CDEmuStop();
			adpcm_dma_active = 0; // stop ADPCM DMA here
		}
		scsi_last_RST = scsi_RST;
	}

	/* Check if bus can be freed */
	if (!scsi_SEL && !scsi_BSY && cd_selected) {
		// freeing bus
		cd_selected = 0;
		scsi_CD = scsi_MSG = scsi_IO = scsi_REQ = 0;
		cd_set_irq_line(PCE_CD_IRQ_TRANSFER_DONE, 0);
	}

	/* Select the CD device */
	if (scsi_SEL) {
		if (!cd_selected) {
			cd_selected = 1;
			// Setting CD in device selection
			scsi_BSY = scsi_REQ = scsi_CD = 1;
			scsi_MSG = scsi_IO = 0;
		}
	}

	if (!scsi_ATN) {
		/* Check for data and message phases */
		if (scsi_BSY) {
			if (scsi_MSG) {
				/* message phase */
				if (scsi_IO) {
					cd_handle_message_input();
				} else {
					cd_handle_message_output();
				}
			} else {
				/* data phase */
				if (scsi_IO) {
					/* Reading data from target */
					cd_handle_data_input();
				} else {
					/* Sending data to target */
					cd_handle_data_output();
				}
			}
		}
	}
}

static UINT8 cd_get_adpcm_ram_byte()
{
	if (adpcm_read_buf > 0) {
		adpcm_read_buf--;
		return 0;
	}
	UINT8 res = PCEADPCMRAM[adpcm_read_ptr];
	adpcm_read_ptr = (adpcm_read_ptr + 1) & 0xffff;
	return res;
}

static void cd_set_adpcm_ram_byte(UINT8 val)
{
	if (adpcm_write_buf > 0) {
		adpcm_write_buf--;
	} else {
		PCEADPCMRAM[adpcm_write_ptr] = val;
		adpcm_write_ptr = (adpcm_write_ptr + 1) & 0xffff;
	}
}

static void cd_ack_clear_timer_cb(int param)
{
	cd_update();
	scsi_ACK = 0;
	// "Ginga Fukei Densetsu Sapphire" hangs if we don't update again
	cd_update();
	if (scsi_CD) {
		cd_adpcm_dma_reg &= 0xfc;
	}
}

static UINT8 cd_get_cd_data_byte()
{
	UINT8 data = cd_cdc_data;
	if (scsi_REQ && !scsi_ACK && !scsi_CD) {
		if (scsi_IO) {
			scsi_ACK = 1;
			cd_ack_clear_timer.start(15, 0, 1, 0);
		}
	}
	return data;
}

static void cd_adpcm_dma_timer_cb(int param)
{
	if (scsi_REQ && !scsi_ACK && !scsi_CD && scsi_IO) {
		PCEADPCMRAM[adpcm_write_ptr] = cd_get_cd_data_byte();
		adpcm_write_ptr = (adpcm_write_ptr + 1) & 0xffff;
		cd_adpcm_status &= ~4;

		if (adpcm_length < 0xffff) adpcm_length++;
		cd_set_irq_line(PCE_CD_IRQ_SAMPLE_HALF_PLAY, (adpcm_length < 0x8000) ? 1 : 0);
	}
}

/*
 * CD Interface Register 0x00 - CDC status
 *
 * x--- ---- busy signal
 * -x-- ---- request signal
 * ---x ---- cd signal
 * ---- x--- i/o signal
 *
 */
static UINT8 cd_reg_cdc_status_r()
{
	UINT8 res = (cd_cdc_status & 7);
	res |= scsi_BSY ? 0x80 : 0;
	res |= scsi_REQ ? 0x40 : 0;
	res |= scsi_MSG ? 0x20 : 0;
	res |= scsi_CD  ? 0x10 : 0;
	res |= scsi_IO  ? 0x08 : 0;
	return res;
}

static void cd_reg_cdc_status_w(UINT8 data)
{
	/* select device (which bits??) */
	scsi_SEL = 1;
	cd_update();
	scsi_SEL = 0;
	adpcm_dma_active = 0; // stop ADPCM DMA here
	/* any write here clears CD transfer irqs */
	cd_set_irq_line(0x70, 0);
	cd_cdc_status = data;
}

/*
 * CD Interface Register 0x01 - CDC command / status / data
 */
static UINT8 cd_reg_cdc_data_r()
{
	return cd_cdc_data;
}

static void cd_reg_cdc_data_w(UINT8 data)
{
	cd_cdc_data = data;
}

/*
 * CD Interface Register 0x02 - IRQ Mask and CD control
 *
 * x--- ---- to SCSI ACK
 * -x-- ---- transfer ready irq
 * --x- ---- transfer done irq
 * ---x ---- BRAM irq?
 * ---- x--- ADPCM FULL irq
 * ---- -x-- ADPCM HALF irq
 */
static UINT8 cd_reg_irq_mask_r()
{
	return cd_irq_mask;
}

static void cd_reg_irq_mask_w(UINT8 data)
{
	scsi_ACK = (data & 0x80) ? 1 : 0;
	cd_irq_mask = data;
	cd_set_irq_line(0, 0);
}

/*
 * CD Interface Register 0x03 - BRAM lock / CD status (read only)
 *
 * -x-- ---- CD acknowledge signal
 * --x- ---- CD done signal
 * ---x ---- bram signal (?)
 * ---- x--- ADPCM 2
 * ---- -x-- ADPCM 1
 * ---- --x- CDDA left/right speaker select
 */
static UINT8 cd_reg_irq_status_r()
{
	UINT8 res = cd_irq_status & 0x6e;
	// a read here locks the BRAM
	bram_locked = 1;
	res |= (cd_motor_on ? 0x10 : 0);
	// gross hack, needs actual behaviour of CDDA data select
	cd_irq_status ^= 0x02;
	return res;
}

/*
 * CD Interface Register 0x04 - CD reset
 *
 * ---- --x- to SCSI RST
 */
static UINT8 cd_reg_cdc_reset_r()
{
	return cd_reset_reg;
}

static void cd_reg_cdc_reset_w(UINT8 data)
{
	scsi_RST = data & 0x02;
	cd_reset_reg = data;
}

/*
 * CD Interface Register 0x05 - CD-DA Volume low 8-bit port
 * CD Interface Register 0x06 - CD-DA Volume high 8-bit port
 */
static UINT8 cd_reg_cdda_data_r(INT32 offset)
{
	// note: we are using CDEmuGetSoundBuffer() in PCEFrame(), so we don't need to do anything here ?
	return 0;
}

/*
 * CD Interface Register 0x07 - BRAM unlock / CD status
 *
 * x--- ---- Enables BRAM
 */
static UINT8 cd_reg_bram_status_r()
{
	return (bram_locked ? (cd_bram_status & 0x7f) : (cd_bram_status | 0x80));
}

static void cd_reg_bram_unlock_w(UINT8 data)
{
	if (data & 0x80) bram_locked = 0;
	cd_bram_status = data;
}

/*
 * CD Interface Register 0x08 - CD data (R) / ADPCM address low (W)
 */
static UINT8 cd_reg_cd_data_r()
{
	return cd_get_cd_data_byte();
}

static void cd_reg_adpcm_address_lo_w(UINT8 data)
{
	cd_adpcm_latch_address = (data & 0xff) | (cd_adpcm_latch_address & 0xff00);
}

/*
 * CD Interface Register 0x09 - ADPCM address high (W)
 */
static void cd_reg_adpcm_address_hi_w(UINT8 data)
{
	cd_adpcm_latch_address = (data << 8) | (cd_adpcm_latch_address & 0xff);
}

/*
 * CD interface Register 0x0a - ADPCM RAM data port
 */
static UINT8 cd_reg_adpcm_data_r()
{
	return cd_get_adpcm_ram_byte();
}

static void cd_reg_adpcm_data_w(UINT8 data)
{
	cd_set_adpcm_ram_byte(data);
}

/*
 * CD interface Register 0x0b - ADPCM DMA control
 */
static UINT8 cd_reg_adpcm_dma_control_r()
{
	return cd_adpcm_dma_reg;
}

static void cd_reg_adpcm_dma_control_w(UINT8 data)
{
	if (data & 3) {
		adpcm_dma_active = 1;
		cd_adpcm_status |= 4;
	}
	cd_adpcm_dma_reg = data;
}

/*
 * CD Interface Register 0x0c - ADPCM status
 *
 * x--- ---- ADPCM is reading data
 * ---- x--- ADPCM playback (0) stopped (1) currently playing
 * ---- -x-- pending ADPCM data write
 * ---- ---x ADPCM playback (1) stopped (0) currently playing
 */
static UINT8 cd_reg_adpcm_status_r()
{
	return cd_adpcm_status;
}

/*
 * CD Interface Register 0x0d - ADPCM address control
 *
 * x--- ---- ADPCM reset
 * -x-- ---- ADPCM play   - may be reversed
 * --x- ---- ADPCM repeat /
 * ---x ---- ADPCM set length
 * ---- x--- ADPCM set read address
 * ---- --xx ADPCM set write address
 */
static UINT8 cd_reg_adpcm_address_control_r()
{
	// TODO: some games read bit 5 and want it to be low otherwise they hang
	// how that can cope with "repeat"?
	return cd_adpcm_control;
}

static void cd_adpcm_stop(UINT8 irq_flag)
{
	cd_adpcm_status |= 0x01;
	cd_adpcm_status &= ~0x08;
	if (irq_flag) cd_set_irq_line(PCE_CD_IRQ_SAMPLE_FULL_PLAY, 1);
	cd_adpcm_control &= ~0x60;
	msm_idle = 1;
}

static void cd_adpcm_play()
{
	cd_adpcm_status &= ~0x01;
	cd_adpcm_status |= 0x08;
	cd_set_irq_line(PCE_CD_IRQ_SAMPLE_FULL_PLAY, 0);
	cd_irq_status &= ~0x0c;
	msm_idle = 0;
}

static void cd_reg_adpcm_address_control_w(UINT8 data)
{
	if ((cd_adpcm_control & 0x80) && !(data & 0x80)) { // ADPCM reset
		/* Reset ADPCM hardware */
		adpcm_read_ptr = 0;
		adpcm_write_ptr = 0;
		msm_start_addr = 0;
		msm_end_addr = 0;
		msm_half_addr = 0;
		msm_nibble = 0;
		adpcm_length = 0;
		cd_adpcm_stop(0);
		MSM5205ResetWrite(0, 1);
	}

	// TODO: gulliver really starts an ADPCM play with bit 5 rather than 6
	// Is it a doc mistake and is actually reversed?
	msm_repeat = BIT(data, 5);

	if ((data & 0x40) && ((cd_adpcm_control & 0x40) == 0)) { // ADPCM play
		msm_start_addr = adpcm_read_ptr;
		msm_nibble = 0;
		cd_adpcm_play();
		cd_set_irq_line(PCE_CD_IRQ_SAMPLE_HALF_PLAY, (adpcm_length < 0x8000) ? 1 : 0);
		MSM5205ResetWrite(0, 0);
	} else if ((data & 0x40) == 0) {
		// used by bbros to cancel an in-flight sample// used by bbros to cancel an in-flight sample
		cd_adpcm_stop(0);
		MSM5205ResetWrite(0, 1);
		// addfam wants to irq ack here
		// https://mametesters.org/view.php?id=7261
		if (!BIT(data, 5)) {
			cd_set_irq_line(PCE_CD_IRQ_SAMPLE_HALF_PLAY, 0);
			cd_set_irq_line(PCE_CD_IRQ_SAMPLE_FULL_PLAY, 0);
		}
	}

	if (data & 0x10) { // ADPCM set length
		adpcm_length = cd_adpcm_latch_address;
	}
	if (data & 0x08) { // ADPCM set read address
		adpcm_read_ptr = cd_adpcm_latch_address;
		adpcm_read_buf = 2;
	}
	if ((data & 0x02) == 0x02) { // ADPCM set write address
		adpcm_write_ptr = cd_adpcm_latch_address;
		adpcm_write_buf = data & 1;
	}

	cd_adpcm_control = data;
}

/*
 * CD Interface Register 0x0e - ADPCM playback rate
 */
static void cd_reg_adpcm_playback_rate_w(UINT8 data)
{
	adpcm_rate = data & 0x0f;
	UINT8 divider = 0x10 - adpcm_rate;
	MSM5205SetClock(0, (PCE_CD_CLOCK / 6) / divider);
}

static void cd_start_fade(double &volume, UINT8 &active, double &step, double target, INT32 duration_ms)
{
	double total_ticks = ((double)duration_ms / 1000.0) * PCE_CD_TICKS_PER_SEC;
	if (total_ticks < 1.0) total_ticks = 1.0;
	step = (target - volume) / total_ticks;
	active = 1;
}

/*
 * CD Interface Register 0x0f - CD-DA/ADPCM fader in/out register
 *
 * ---- xxxx command setting:
 * 0x00 ADPCM/CD-DA fade-in
 * 0x01 CD-DA fade-in
 * 0x08 CD-DA fade-out (short) ADPCM fade-in
 * 0x09 CD-DA fade-out (long)
 * 0x0a ADPCM fade-out (long)
 * 0x0c CD-DA fade-out (short) ADPCM fade-in
 * 0x0d CD-DA fade-out (short)
 * 0x0e ADPCM fade-out (short)
 */
static void cd_reg_fader_control_w(UINT8 data)
{
	if (cd_fader_ctrl != data) {
		switch (data & 0xf) {
			case 0x00:
				// ADPCM/CD-DA enable (100 msecs)
				cd_start_fade(cd_cdda_volume, cd_cdda_fade_active, cd_cdda_fade_step, 100.0, 100);
				cd_start_fade(cd_adpcm_volume, cd_adpcm_fade_active, cd_adpcm_fade_step, 100.0, 100);
				break;
			case 0x01:
				// CD-DA enable (100 msecs)
				cd_start_fade(cd_cdda_volume, cd_cdda_fade_active, cd_cdda_fade_step, 100.0, 100);
				break;
			case 0x08:
			case 0x0c:
				// CD-DA short (1500 msecs) fade out / ADPCM enable
				cd_start_fade(cd_cdda_volume, cd_cdda_fade_active, cd_cdda_fade_step, 0.0, 1500);
				cd_start_fade(cd_adpcm_volume, cd_adpcm_fade_active, cd_adpcm_fade_step, 100.0, 100);
				break;
			case 0x09:
				// CD-DA long (5000 msecs) fade out
				cd_start_fade(cd_cdda_volume, cd_cdda_fade_active, cd_cdda_fade_step, 0.0, 5000);
				break;
			case 0x0d:
				// CD-DA short (1500 msecs) fade out
				cd_start_fade(cd_cdda_volume, cd_cdda_fade_active, cd_cdda_fade_step, 0.0, 1500);
				break;
			case 0x0a:
				// ADPCM long (5000 msecs) fade out
				cd_start_fade(cd_adpcm_volume, cd_adpcm_fade_active, cd_adpcm_fade_step, 0.0, 5000);
				break;
			case 0x0e:
				// ADPCM short (1500 msecs) fade out
				cd_start_fade(cd_adpcm_volume, cd_adpcm_fade_active, cd_adpcm_fade_step, 0.0, 1500);
				break;
			default:
				break;
		}
	}
	cd_fader_ctrl = data;
}

/* Callback for new data from the MSM5205.
  The PCE cd unit actually divides the clock signal supplied to the MSM5205.
  Currently we can only use static clocks for the MSM5205.
 */
static void cd_msm5205_vclk_callback()
{
	if (msm_idle) return;

	/* Supply new ADPCM data */
	UINT8 msm_data = (msm_nibble) ? (PCEADPCMRAM[msm_start_addr & 0xffff] & 0x0f) : ((PCEADPCMRAM[msm_start_addr & 0xffff] & 0xf0) >> 4);

	MSM5205DataWrite(0, msm_data);

	msm_nibble ^= 1;
	if (msm_nibble == 0) {
		msm_start_addr++;
		// adpcm_length represents "how many unplayed bytes are currently buffered",
		// decremented here on consumption, incremented on DMA/manual write (see cd_adpcm_dma_timer_cb).
		// MAME uses a fixed start/end address comparison, which is breaking sound in Last Armagueddon's intro
		if (adpcm_length > 0) {
			adpcm_length--;
			cd_set_irq_line(PCE_CD_IRQ_SAMPLE_HALF_PLAY, (adpcm_length < 0x8000) ? 1 : 0);
		} else {
			cd_set_irq_line(PCE_CD_IRQ_SAMPLE_HALF_PLAY, 0);
			cd_adpcm_stop(1);
			MSM5205ResetWrite(0, 1);
		}
	}
}

static void cd_cdda_check_end_mark()
{
	// handle end playback event
	if (!cd_end_mark) return;
	if (cd_cdda_status != PCE_CD_CDDA_PLAYING) return;

	INT32 lba = CDEmuGetCurrentLBA();
	if (lba < cd_end_frame) return;

	switch (cd_cdda_play_mode & 3) {
		case 1: {
			// play with repeat
			UINT32 msf = cd_current_frame;
			UINT8 m = dec_2_bcd((msf / 75) / 60);
			UINT8 s = dec_2_bcd((msf / 75) % 60);
			UINT8 f = dec_2_bcd(msf % 75);
			CDEmuPlay(m, s, f);
			cd_end_mark = 1;
			break;
		}
		case 2:
			 // IRQ when finished
			cd_set_irq_line(PCE_CD_IRQ_TRANSFER_DONE, 1);
			cd_end_mark = 0;
			break;
		case 3:
			// play without repeat
			// fzone2 / fzone2j wants a STOP thru SUBQ command during intro
			cd_cdda_status = PCE_CD_CDDA_OFF;
			CDEmuStop();
			cd_end_mark = 0;
			break;
		default:
			break;
	}
}

void CDSubsystemTick()
{
	// MAME uses a callback in its cd interface to do this (i think)
	cd_cdda_check_end_mark();

	// MAME uses timer for the following 3 mecanisms
	if (cd_cdda_fade_active) {
		cd_cdda_volume += cd_cdda_fade_step;
		if ((cd_cdda_fade_step < 0.0 && cd_cdda_volume <= 0.0) ||
		    (cd_cdda_fade_step > 0.0 && cd_cdda_volume >= 100.0)) {
			cd_cdda_volume = (cd_cdda_fade_step < 0.0) ? 0.0 : 100.0;
			cd_cdda_fade_active = 0;
		}
	}

	if (cd_adpcm_fade_active) {
		cd_adpcm_volume += cd_adpcm_fade_step;
		if ((cd_adpcm_fade_step < 0.0 && cd_adpcm_volume <= 0.0) ||
		    (cd_adpcm_fade_step > 0.0 && cd_adpcm_volume >= 100.0)) {
			cd_adpcm_volume = (cd_adpcm_fade_step < 0.0) ? 0.0 : 100.0;
			cd_adpcm_fade_active = 0;
		}
	}

	// MAME is using a timer for this, but doing the same breaks "Ginga Fukei Densetsu Sapphire" in FBNeo.
	// specifically, a few seconds of intro just before starting to play are being skipped.
	// I couldn't test whether MAME suffered from the same issue or not,
	// with MAME being a nightmare when it comes to starting pce arcade card games
	if (adpcm_dma_active) cd_adpcm_dma_timer_cb(0);
}

static UINT8 acard_peripheral_r(UINT32 offset)
{
	if ((offset & 0xe0) == 0xe0) {
		switch (offset & 0x1f) {
			case 0x00: return (acard_shift >> 0) & 0xff;
			case 0x01: return (acard_shift >> 8) & 0xff;
			case 0x02: return (acard_shift >> 16) & 0xff;
			case 0x03: return (acard_shift >> 24) & 0xff;
			case 0x04: return acard_shift_reg;
			case 0x05: return acard_rotate_reg;
			case 0x1c: return 0x00;
			case 0x1d: return 0x00;
			case 0x1e: return 0x10; // version number (MSB?)
			case 0x1f: return 0x51; // Arcade Card ID
		}
		return 0xff;
	}

	acard_port_t &port = acard_port[(offset & 0x30) >> 4];

	switch (offset & 0x8f) {
		case 0x00:
		case 0x01: {
			UINT8 res = PCEAcardRAM[port.ram_addr()];
			port.addr_increment();
			return res;
		}
		case 0x02: return (port.base_addr >> 0) & 0xff;
		case 0x03: return (port.base_addr >> 8) & 0xff;
		case 0x04: return (port.base_addr >> 16) & 0xff;
		case 0x05: return (port.addr_offset >> 0) & 0xff;
		case 0x06: return (port.addr_offset >> 8) & 0xff;
		case 0x07: return (port.addr_inc >> 0) & 0xff;
		case 0x08: return (port.addr_inc >> 8) & 0xff;
		case 0x09: return port.ctrl;
		default:   return 0xff;
	}
}

static void acard_peripheral_w(UINT32 offset, UINT8 data)
{
	if ((offset & 0xe0) == 0xe0) {
		switch (offset & 0x0f) {
			case 0: acard_shift = (data & 0xff) | (acard_shift & 0xffffff00); break;
			case 1: acard_shift = ((UINT32)data << 8)  | (acard_shift & 0xffff00ff); break;
			case 2: acard_shift = ((UINT32)data << 16) | (acard_shift & 0xff00ffff); break;
			case 3: acard_shift = ((UINT32)data << 24) | (acard_shift & 0x00ffffff); break;
			case 4:
				acard_shift_reg = data & 0x0f;
				if (acard_shift_reg != 0) {
					acard_shift = (acard_shift_reg < 8)
							? (acard_shift << acard_shift_reg)
							: (acard_shift >> (16 - acard_shift_reg));
				}
				break;
			case 5:
				acard_rotate_reg = data & 0x0f;
				if (acard_rotate_reg != 0) {
					acard_shift = (acard_rotate_reg < 8)
							? ((acard_shift << acard_rotate_reg) | (acard_shift >> (32 - acard_rotate_reg)))
							: ((acard_shift >> (16 - acard_rotate_reg)) | (acard_shift << (32 - (16 - acard_rotate_reg))));
				}
				break;
		}
	} else {
		acard_port_t &port = acard_port[(offset & 0x30) >> 4];

		switch (offset & 0x8f) {
			case 0x00:
			case 0x01:
				PCEAcardRAM[port.ram_addr()] = data;
				port.addr_increment();
				break;
			case 0x02: port.base_addr = (data & 0xff) | (port.base_addr & 0xffff00); break;
			case 0x03: port.base_addr = ((UINT32)data << 8) | (port.base_addr & 0xff00ff); break;
			case 0x04: port.base_addr = ((UINT32)data << 16) | (port.base_addr & 0x00ffff); break;
			case 0x05:
				port.addr_offset = (data & 0xff) | (port.addr_offset & 0xff00);
				if ((port.ctrl & 0x60) == 0x20) port.adjust_addr();
				break;
			case 0x06:
				port.addr_offset = ((UINT16)data << 8) | (port.addr_offset & 0x00ff);
				if ((port.ctrl & 0x60) == 0x40) port.adjust_addr();
				break;
			case 0x07: port.addr_inc = (data & 0xff) | (port.addr_inc & 0xff00); break;
			case 0x08: port.addr_inc = ((UINT16)data << 8) | (port.addr_inc & 0x00ff); break;
			case 0x09: port.ctrl = data & 0x7f; break;
			case 0x0a:
				if ((port.ctrl & 0x60) == 0x60) port.adjust_addr();
				break;
		}
	}
}

static UINT8 acard_ram_r(UINT32 address)
{
	UINT32 offset = address & 0x7fff;
	return acard_peripheral_r((offset & 0x6000) >> 9);
}

static void acard_ram_w(UINT32 address, UINT8 data)
{
	UINT32 offset = address & 0x7fff;
	acard_peripheral_w((offset & 0x6000) >> 9, data);
}

void CDSubsystemRegsWrite(UINT32 address, UINT8 data)
{
	if (HAS_CD) {
		if ((address & 0xff) >= 0xc0 && (address & 0xff) <= 0xc7) {
			return;
		}

		if ((hardware_type == ACARD_HW) && (address >= 0x1ffa00) && (address <= 0x1ffaff)) {
			acard_peripheral_w(address & 0xff, data);
			return;
		}

		cd_update();

		switch (address & 0xf)
		{
			case 0x00: cd_reg_cdc_status_w(data); break;
			case 0x01: cd_reg_cdc_data_w(data); break;
			case 0x02: cd_reg_irq_mask_w(data); break;
			case 0x04: cd_reg_cdc_reset_w(data); break;
			case 0x07: cd_reg_bram_unlock_w(data); break;
			case 0x08: cd_reg_adpcm_address_lo_w(data); break;
			case 0x09: cd_reg_adpcm_address_hi_w(data); break;
			case 0x0a: cd_reg_adpcm_data_w(data); break;
			case 0x0b: cd_reg_adpcm_dma_control_w(data); break;
			case 0x0d: cd_reg_adpcm_address_control_w(data); break;
			case 0x0e: cd_reg_adpcm_playback_rate_w(data); break;
			case 0x0f: cd_reg_fader_control_w(data); break;
		}

		cd_update();
	}
}

int CDSubsystemMiscWrite(UINT32 address, UINT8 data)
{
	if (HAS_CD) {
		if ((address >= 0x1ee000) && (address <= 0x1ee7ff)) {
			if (!bram_locked)
			{
				PCECDBRAM[address & 0x7FF] = data;
			}
			return 1;
		}

		if ((hardware_type == ACARD_HW) && (address >= 0x080000) && (address <= 0x087fff)) {
			acard_ram_w(address, data);
			return 1;
		}
	}

	return 0; // "not handled here"
}

UINT8 CDSubsystemRegsRead(UINT32 address)
{
	if (HAS_CD) {
		if (address >= 0x1ff8c0 && address <= 0x1ff8c7) {
			switch (address & 0x0f) {
				case 0x1: return 0xaa;
				case 0x2: return 0x55;
				case 0x3: return 0x00;
				case 0x5: return 0xaa; // JP - 0x55 for US
				case 0x6: return 0x55; // JP - 0xaa for US
				case 0x7: return 0x03;
				default:  return 0x00;
			}
		}

		if ((hardware_type == ACARD_HW) && (address >= 0x1ffa00) && (address <= 0x1ffaff)) {
			return acard_peripheral_r(address & 0xff);
		}

		cd_update();

		UINT8 ret = 0;
		switch (address & 0xf)
		{
			case 0x00: ret = cd_reg_cdc_status_r(); break;
			case 0x01: ret = cd_reg_cdc_data_r(); break;
			case 0x02: ret = cd_reg_irq_mask_r(); break;
			case 0x03: ret = cd_reg_irq_status_r(); break;
			case 0x04: ret = cd_reg_cdc_reset_r(); break;
			case 0x05: ret = cd_reg_cdda_data_r(0); break;
			case 0x06: ret = cd_reg_cdda_data_r(1); break;
			case 0x07: ret = cd_reg_bram_status_r(); break;
			case 0x08: ret = cd_reg_cd_data_r(); break;
			case 0x0a: ret = cd_reg_adpcm_data_r(); break;
			case 0x0b: ret = cd_reg_adpcm_dma_control_r(); break;
			case 0x0c: ret = cd_reg_adpcm_status_r(); break;
			case 0x0d: ret = cd_reg_adpcm_address_control_r(); break;
		}

		return ret;
	}

	return 0;
}

UINT8 CDSubsystemMiscRead(UINT32 address)
{
	if (HAS_CD) {
		if ((address >= 0x1ee000) && (address <= 0x1ee7ff)) {
			if (bram_locked) return 0xff;
			return PCECDBRAM[address & 0x7ff];
		}

		if ((hardware_type == ACARD_HW) && (address >= 0x080000) && (address <= 0x087fff)) {
			return acard_ram_r(address);
		}
	}

	return 0;
}

void CDSubsystemMemIndex(UINT8 *&Next)
{
	if (HAS_CD) {
		PCECDBRAM       = Next; Next += 0x000800;
		PCEADPCMRAM     = Next; Next += 0x010000;
		PCECDRAM        = Next; Next += 0x010000;
		PCESuperRAM     = Next; Next += 0x030000;
		if (hardware_type == ACARD_HW) {
			PCEAcardRAM = Next; Next += 0x200000;
		}
	}
}

void CDSubsystemReset()
{
	if (HAS_CD) {
		bram_locked = 1;

		adpcm_read_buf = 0;
		adpcm_write_buf = 0;
		cd_adpcm_status |= 1;
		cd_adpcm_status &= ~8;

		scsi_RST = 0;
		scsi_last_RST = 0;
		scsi_SEL = 0;
		scsi_BSY = 0;
		cd_selected = 0;
		scsi_ATN = 0;
		cd_end_mark = 0;

		cd_command_buffer_index = 0;
		cd_data_buffer_size = 0;
		cd_data_buffer_index = 0;
		cd_current_frame = 0;
		cd_end_frame = 0;
		cd_cdda_status = PCE_CD_CDDA_OFF;
		cd_irq_mask = 0;
		cd_irq_status = 0;
		adpcm_dma_active = 0;
		adpcm_read_ptr = adpcm_write_ptr = 0;
		msm_idle = 1;
		msm_start_addr = msm_end_addr = msm_half_addr = 0;
		msm_nibble = 0;
		msm_repeat = 0;
		adpcm_rate = 0;
		MSM5205Reset();
		timerReset();
		cd_cdda_fade_active = 0;
		cd_adpcm_fade_active = 0;

		if (hardware_type == ACARD_HW) {
			memset(acard_port, 0, sizeof(acard_port));
			acard_shift = 0;
			acard_shift_reg = 0;
			acard_rotate_reg = 0;
		}
	}
}

static INT32 PCECDSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(double)h6280TotalCycles() * nSoundRate / 7159090;
}

void CDSubsystemInit()
{
	h6280Open(0);
	h6280MapMemory(PCECDRAM   , 0x100000, 0x10ffff, MAP_RAM);
	h6280MapMemory(PCESuperRAM, 0x0d0000, 0x0fffff, MAP_RAM);
	h6280Close();

	{
		static const UINT8 bram_default[8] = { 'H', 'U', 'B', 'M', 0x00, 0x88, 0x10, 0x80 };
		memset(PCECDBRAM, 0, 0x800);
		memcpy(PCECDBRAM, bram_default, sizeof(bram_default));
	}

	MSM5205Init(0, PCECDSynchroniseStream, (PCE_CD_CLOCK / 6), cd_msm5205_vclk_callback, MSM5205_S48_4B, 1);
	MSM5205PlaymodeWrite(0, MSM5205_S48_4B);
	MSM5205SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	{
		UINT8 *toc = CDEmuReadTOC(CDEmuTOC_LASTMSF);
		UINT8 m = bcd_2_dec(toc[0]);
		UINT8 s = bcd_2_dec(toc[1]);
		UINT8 f = bcd_2_dec(toc[2]);
		cd_last_frame = f + 75 * (s + m * 60);
	}
	cd_end_frame = cd_last_frame;

	memset(PCEADPCMRAM, 0, 0x10000);

	if (hardware_type == ACARD_HW) {
		memset(PCEAcardRAM, 0, 0x200000);
		memset(acard_port, 0, sizeof(acard_port));
		acard_shift = 0;
		acard_shift_reg = 0;
		acard_rotate_reg = 0;
	}

	timerInit();
	timerAdd(cd_ack_clear_timer, 0, cd_ack_clear_timer_cb);
	h6280Open(0);
	h6280SetCallback(timerRun);
	h6280Close();
}

void CDSubsystemExit()
{
	if (HAS_CD) {
		CDEmuExit();
		MSM5205Exit();
		timerExit();
	}
}

void CDSubsystemSoundUpdate(INT16 *output, INT32 samples_len)
{
	if (HAS_CD) {
		CDEmuSetVolume(cd_cdda_volume);
		CDEmuGetSoundBuffer(output, samples_len);
		MSM5205SetRoute(0, cd_adpcm_volume / 100.0, BURN_SND_ROUTE_BOTH);
		MSM5205Render(0, output, samples_len);
	}
}

void CDSubsystemScan(INT32 nAction, INT32 *pnMin)
{
	if (HAS_CD) {
		if (nAction & ACB_DRIVER_DATA) {
			SCAN_VAR(bram_locked);

			SCAN_VAR(scsi_BSY);
			SCAN_VAR(scsi_SEL);
			SCAN_VAR(scsi_CD);
			SCAN_VAR(scsi_IO);
			SCAN_VAR(scsi_MSG);
			SCAN_VAR(scsi_REQ);
			SCAN_VAR(scsi_ACK);
			SCAN_VAR(scsi_ATN);
			SCAN_VAR(scsi_RST);
			SCAN_VAR(scsi_last_RST);
			SCAN_VAR(cd_motor_on);
			SCAN_VAR(cd_selected);

			SCAN_VAR(cd_command_buffer);
			SCAN_VAR(cd_command_buffer_index);
			SCAN_VAR(cd_status_sent);
			SCAN_VAR(cd_message_after_status);
			SCAN_VAR(cd_message_sent);

			ScanVar(cd_data_buffer, sizeof(cd_data_buffer), "CD Data Buffer");
			SCAN_VAR(cd_data_buffer_size);
			SCAN_VAR(cd_data_buffer_index);
			SCAN_VAR(cd_data_transferred);

			SCAN_VAR(cd_current_frame);
			SCAN_VAR(cd_end_frame);
			SCAN_VAR(cd_last_frame);
			SCAN_VAR(cd_cdda_status);
			SCAN_VAR(cd_cdda_play_mode);
			SCAN_VAR(cd_end_mark);

			SCAN_VAR(cd_reset_reg);
			SCAN_VAR(cd_irq_mask);
			SCAN_VAR(cd_irq_status);
			SCAN_VAR(cd_cdc_status);
			SCAN_VAR(cd_cdc_data);
			SCAN_VAR(cd_bram_status);
			SCAN_VAR(cd_adpcm_status);
			SCAN_VAR(cd_adpcm_latch_address);
			SCAN_VAR(cd_adpcm_control);
			SCAN_VAR(cd_fader_ctrl);
			SCAN_VAR(cd_adpcm_dma_reg);

			SCAN_VAR(adpcm_read_ptr);
			SCAN_VAR(adpcm_write_ptr);
			SCAN_VAR(adpcm_read_buf);
			SCAN_VAR(adpcm_write_buf);
			SCAN_VAR(adpcm_length);
			SCAN_VAR(adpcm_rate);
			SCAN_VAR(cd_cdda_fade_step);
			SCAN_VAR(cd_cdda_fade_active);
			SCAN_VAR(cd_adpcm_fade_step);
			SCAN_VAR(cd_adpcm_fade_active);
			SCAN_VAR(adpcm_dma_active);
			SCAN_VAR(msm_start_addr);
			SCAN_VAR(msm_end_addr);
			SCAN_VAR(msm_half_addr);
			SCAN_VAR(msm_nibble);
			SCAN_VAR(msm_repeat);
			SCAN_VAR(msm_idle);

			CDEmuScan(nAction, pnMin);
			MSM5205Scan(nAction, pnMin);
			timerScan();

			if (hardware_type == ACARD_HW) {
				ScanVar(acard_port, sizeof(acard_port), "Arcade Card DRAM Ports");
				SCAN_VAR(acard_shift);
				SCAN_VAR(acard_shift_reg);
				SCAN_VAR(acard_rotate_reg);
			}
		}

		if (nAction & ACB_NVRAM) {
			ScanVar(PCECDBRAM, 0x800, "ADPCM Ram");
		}
	}
}
