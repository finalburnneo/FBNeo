// FB Neo Sega CD / Mega CD add-on
// Ported from the ares Mega CD core (ISC license)

#include "burnint.h"
#include "m68000_intf.h"
#include "m68000_debug.h"
#include "megadrive.h"
#include "segacd.h"
#include "cd_interface.h"
#include "stream.h"
#include <math.h>

INT32 SegaCDMode = 0;
INT32 (*SegaCDMainCycles)() = NULL;

#define SCD_CLOCK		12500000
#define SCD_PCM_RATE	(SCD_CLOCK / 384)
#define MAIN_NTSC		(53693175 / 7)
#define MAIN_PAL		(53203424 / 7)

static UINT8 *AllMemScd = NULL;
static UINT8 *PrgRam;		// 512KiB, Sek byte order (directly mapped on the sub cpu)
static UINT16 *WordRam;		// 256KiB
static UINT16 *CdcRam;		// 16KiB
static UINT8 *PcmRam;		// 64KiB
static UINT8 *BackupRam;	// 8KiB
static UINT8 *BiosRom;

static INT32 scd_bios_index = 0;
static INT32 main_clock = MAIN_NTSC;
static INT32 sub_cycles_frame = 0;
static INT32 sub_carry = 0;

// gate array
static UINT8 sub_run;
static UINT8 sub_request;
static UINT8 sub_halt;
static UINT8 sub_reset_pending;
static UINT8 sub_wram_wait; // 1: blocked inside an instruction, 2: waiting for word ram
static UINT8 wram_mode;				// 0 = 2Mbit, 1 = 1Mbit
static UINT8 wram_switch_request;
static UINT8 wram_switch;
static UINT8 wram_select;
static UINT8 wram_priority;
static UINT8 pram_bank;
static UINT8 pram_protect;
static UINT32 vector_level4;
static UINT8 led_status;
static UINT8 comm_cfm;
static UINT8 comm_cfs;
static UINT16 comm_command[8];
static UINT16 comm_status[8];
static UINT32 counter_divider;
static UINT32 counter_dma;

// sub cpu interrupts: bit n = level n
static UINT8 irq_mask;
static UINT8 irq_pending;
static UINT8 irq_dirty;

static UINT8 timer_frequency;
static UINT8 timer_counter;

// Sanyo LC8951 CD controller
struct scd_cdc {
	UINT8 address;
	UINT16 stopwatch;

	UINT8 irq_decoder_enable, irq_decoder_pending;
	UINT8 irq_transfer_enable, irq_transfer_pending;
	UINT8 irq_command_enable, irq_command_pending;

	UINT8 cmd_fifo[8], cmd_read, cmd_write, cmd_empty;
	UINT8 st_fifo[8], st_read, st_write, st_empty, st_enable, st_active, st_busy, st_wait;

	UINT8 tr_destination;
	UINT32 tr_address;
	UINT16 tr_source, tr_target, tr_pointer, tr_length;
	UINT8 tr_enable, tr_active, tr_busy, tr_wait, tr_ready, tr_completed;

	UINT8 dec_enable, dec_mode, dec_form, dec_valid;

	UINT8 head_minute, head_second, head_frame, head_mode;
	UINT8 sub_file, sub_channel, sub_submode, sub_coding;

	UINT8 ctrl_head, ctrl_mode, ctrl_form, ctrl_command_break, ctrl_mode_byte_check;
	UINT8 ctrl_erasure_request, ctrl_write_request, ctrl_pcode, ctrl_qcode, ctrl_auto;
	UINT8 ctrl_error, ctrl_edc, ctrl_correction_write, ctrl_descramble, ctrl_sync;
	UINT8 ctrl_sync_irq, ctrl_erasure_correction, ctrl_status_trigger, ctrl_status_control;
};

static scd_cdc cdc;

// NEC uPD75006 CD drive controller (HLE)
enum {
	CDD_STOPPED = 0x0, CDD_PLAYING = 0x1, CDD_SEEKING = 0x2, CDD_SCANNING = 0x3,
	CDD_PAUSED = 0x4, CDD_DOOR_OPENED = 0x5, CDD_CHECKSUM_ERROR = 0x6, CDD_COMMAND_ERROR = 0x7,
	CDD_FUNCTION_ERROR = 0x8, CDD_READING_TOC = 0x9, CDD_TRACKING = 0xa, CDD_NO_DISC = 0xb,
	CDD_LEAD_OUT = 0xc, CDD_LEAD_IN = 0xd, CDD_TRAY_MOVING = 0xe, CDD_TEST = 0xf
};

struct scd_cdd {
	UINT16 counter;
	UINT8 host_clock_enable;
	UINT8 status_pending;
	UINT8 status[10];
	UINT8 command[10];

	UINT8 io_status;
	UINT8 io_seeking;
	UINT16 io_latency;
	INT32 io_sector;
	INT32 io_track;
	UINT8 io_toc_read;

	UINT8 dac_rate;
	UINT8 dac_deemphasis;
	UINT16 dac_attenuator;
	UINT16 dac_attenuated;

	UINT8 audio_playing;
	INT32 audio_track;
};

static scd_cdd cdd;

// table of contents, rebuilt from the cd image (not saved in states)
static INT32 disc_present = 0;
static INT32 toc_first = 0, toc_last = 0, toc_leadout = 0;
static INT32 toc_lba[100];		// index 1
static INT32 toc_start[100];	// index 0, the pregap belongs to the track
static UINT8 toc_isdata[100];

// graphics asic (stamp rotation / scaling)
struct scd_gpu {
	UINT8 font_bg, font_fg;
	UINT16 font_data;

	UINT8 stamp_repeat, stamp_tile_size, stamp_map_size;
	UINT32 stamp_map_base, stamp_map_address;

	UINT32 image_base;
	UINT8 image_offset, image_vcells, image_vdots;
	UINT16 image_hdots;
	UINT32 image_address;

	UINT32 vector_base, vector_address;

	UINT8 active;
	UINT32 counter, period;
};

static scd_gpu gpu;

// Ricoh RF5C164
struct scd_pcm_channel {
	UINT8 enable, envelope, pan, start;
	UINT16 step, loop;
	UINT32 address;
};

static scd_pcm_channel pcm_ch[8];
static UINT8 pcm_enable, pcm_bank, pcm_channel;

#define PCM_FIFO_SIZE	4096
static INT16 pcm_fifo[PCM_FIFO_SIZE][2];
static INT32 pcm_fifo_read, pcm_fifo_count;
static INT16 pcm_last[2];
static Stream pcm_stream;

static inline UINT8 dec2bcd(INT32 v) { return ((v / 10) << 4) | (v % 10); }
static inline INT32 bcd2dec(UINT8 v) { return ((v >> 4) * 10) + (v & 0x0f); }

// -------------------------------------------------------------------------
// sub cpu interrupts

static void sub_irq_update()
{
	INT32 level = 0;
	for (INT32 i = 6; i >= 1; i--) {
		if (irq_pending & (1 << i)) { level = i; break; }
	}

	if (level) {
		SekSetIRQLine(level, CPU_IRQSTATUS_ACK);
	} else {
		SekSetIRQLine(0, CPU_IRQSTATUS_NONE);
	}

	irq_dirty = 0;
}

static void irq_sync()
{
	// the sub cpu line can only be changed while it is the active cpu
	if (SekGetActive() == 1) {
		sub_irq_update();
	} else {
		irq_dirty = 1;
	}
}

static void irq_raise(INT32 level)
{
	UINT8 bit = 1 << level;
	if (irq_pending & bit) return;
	if (irq_mask & bit) {
		irq_pending |= bit;
		irq_sync();
	}
}

static void irq_lower(INT32 level)
{
	UINT8 bit = 1 << level;
	if (~irq_pending & bit) return;
	irq_pending &= ~bit;
	irq_sync();
}

static INT32 __fastcall sub_irq_callback(INT32 irq)
{
	if (irq >= 1 && irq <= 6) irq_pending &= ~(1 << irq);
	sub_irq_update();

	return M68K_INT_ACK_AUTOVECTOR;
}

// -------------------------------------------------------------------------
// memory helpers

static inline UINT16 prg_read_word(UINT32 a)
{
	return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(PrgRam + (a & 0x7fffe))));
}

static inline void prg_write(UINT32 a, UINT16 d, INT32 upper, INT32 lower)
{
	a &= 0x7fffe;
	if (upper) PrgRam[(a | 0) ^ 1] = d >> 8;
	if (lower) PrgRam[(a | 1) ^ 1] = d & 0xff;
}

static inline void wram_write(UINT32 idx, UINT16 d, INT32 upper, INT32 lower)
{
	UINT16 *p = &WordRam[idx & 0x1ffff];
	if (upper && lower) {
		*p = d;
	} else if (upper) {
		*p = (*p & 0x00ff) | (d & 0xff00);
	} else if (lower) {
		*p = (*p & 0xff00) | (d & 0x00ff);
	}
}

// 1Mbit mode, main cpu side: 0x220000-0x23ffff are cell-mapped views of the bank
static UINT32 wram_main_1m_index(UINT32 a)
{
	if (a >= 0x220000) {
		if (a < 0x230000) {
			a = ((a & 0x0fc00) >> 8) | ((a & 0x003fc) << 6) | (a & 0x10002);
		} else if (a < 0x238000) {
			a = ((a & 0x07e00) >> 7) | ((a & 0x001fc) << 6) | (a & 0x18002);
		} else if (a < 0x23c000) {
			a = ((a & 0x03f00) >> 6) | ((a & 0x000fc) << 6) | (a & 0x1c002);
		} else {
			a = ((a & 0x01f80) >> 5) | ((a & 0x0007c) << 6) | (a & 0x1e002);
		}
	}

	return (a & 0x1fffe) | wram_select;
}

// -------------------------------------------------------------------------
// table of contents

static void toc_build()
{
	disc_present = 0;
	toc_first = toc_last = toc_leadout = 0;
	memset(toc_lba, 0, sizeof(toc_lba));
	memset(toc_start, 0, sizeof(toc_start));
	memset(toc_isdata, 0, sizeof(toc_isdata));

	UINT8 *toc = CDEmuReadTOC(CDEmuTOC_FIRSTLAST);
	if (toc == NULL) return;

	INT32 first = bcd2dec(toc[0]);
	INT32 last = bcd2dec(toc[1]);
	if (first < 1 || last < first || last > 99) return;

	for (INT32 t = first; t <= last; t++) {
		toc = CDEmuReadTOC(dec2bcd(t));
		if (toc == NULL) return;
		toc_lba[t] = (bcd2dec(toc[0]) * 60 + bcd2dec(toc[1])) * 75 + bcd2dec(toc[2]) - 150;
		toc_isdata[t] = (toc[3] & 0x04) ? 1 : 0;
		toc_start[t] = (t == first) ? -150 : toc_lba[t];
	}

	for (INT32 t = first; t < last; t++) {
		toc = CDEmuReadTOC(CDEmuTOC_TRACKEND | dec2bcd(t));
		if (toc == NULL) break;
		INT32 end = (bcd2dec(toc[0]) * 60 + bcd2dec(toc[1])) * 75 + bcd2dec(toc[2]) - 150;
		if (end > toc_lba[t] && end <= toc_lba[t + 1]) toc_start[t + 1] = end;
	}

	toc = CDEmuReadTOC(CDEmuTOC_LASTMSF);
	if (toc == NULL) return;
	toc_leadout = (bcd2dec(toc[0]) * 60 + bcd2dec(toc[1])) * 75 + bcd2dec(toc[2]) - 150;

	toc_first = first;
	toc_last = last;
	disc_present = 1;

	bprintf(0, _T("Sega CD: %d track(s), lead-out at lba %d\n"), last - first + 1, toc_leadout);
}

static INT32 cdd_in_track(INT32 lba)
{
	if (!disc_present || lba < -150 || lba >= toc_leadout) return 0;

	for (INT32 t = toc_last; t > toc_first; t--) {
		if (lba >= toc_start[t]) return t;
	}

	return toc_first;
}

static inline INT32 cdd_in_lead_in(INT32 lba) { return disc_present && lba < -150; }
static inline INT32 cdd_in_lead_out(INT32 lba) { return disc_present && lba >= toc_leadout; }
static inline INT32 track_valid(INT32 t) { return disc_present && t >= toc_first && t <= toc_last; }
static inline INT32 track_is_data(INT32 t) { return track_valid(t) && toc_isdata[t]; }
static inline INT32 track_is_audio(INT32 t) { return track_valid(t) && !toc_isdata[t]; }

static void msf_from_aba(INT32 frames, UINT8 *m, UINT8 *s, UINT8 *f)
{
	if (frames < 0) frames += 100 * 60 * 75;
	if (frames < 0 || frames >= 100 * 60 * 75) {
		*m = *s = *f = 0;
		return;
	}

	*m = frames / (75 * 60) % 100;
	*s = frames / 75 % 60;
	*f = frames % 75;
}

static void msf_from_lba(INT32 lba, UINT8 *m, UINT8 *s, UINT8 *f)
{
	msf_from_aba(lba + 150, m, s, f);
}

static INT32 msf_to_lba(INT32 m, INT32 s, INT32 f)
{
	if (m > 99 || s > 59 || f > 74) return 100 * 60 * 75;

	INT32 frames = m * 60 * 75 + s * 75 + f;
	if (m >= 90) frames -= 100 * 60 * 75;

	return frames - 150;
}

// -------------------------------------------------------------------------
// CDC

static void cdc_poll()
{
	INT32 pending = 0;
	pending |= cdc.irq_decoder_enable  && cdc.irq_decoder_pending;
	pending |= cdc.irq_transfer_enable && cdc.irq_transfer_pending;
	pending |= cdc.irq_command_enable  && cdc.irq_command_pending;

	if (pending) {
		irq_raise(5);
	} else {
		irq_lower(5);
	}
}

static void cdc_transfer_complete()
{
	cdc.tr_active = 0;
	cdc.tr_busy = 0;
	cdc.tr_ready = 0;
	cdc.tr_completed = 1;
	cdc.irq_transfer_pending = 1;
	cdc_poll();
}

static void cdc_transfer_stop()
{
	cdc.tr_active = 0;
	cdc.tr_busy = 0;
	cdc.tr_ready = 0;
}

static void cdc_transfer_start()
{
	if (!cdc.tr_enable) return;

	cdc.tr_active = 1;
	cdc.tr_busy = 1;
	cdc.tr_ready = (cdc.tr_destination == 2 || cdc.tr_destination == 3);
	cdc.tr_completed = 0;
	cdc.irq_transfer_pending = 0;
	cdc_poll();
}

static inline void cdc_length_step()
{
	if (cdc.tr_length == 0) {
		cdc_transfer_complete();
		cdc.tr_length = 0xfff;
	} else {
		cdc.tr_length--;
	}
}

static UINT16 cdc_transfer_read()
{
	if (!cdc.tr_ready) return 0xffff;

	UINT16 data = CdcRam[(cdc.tr_source >> 1) & 0x1fff];
	cdc.tr_source += 2;
	cdc_length_step();
	cdc_length_step();

	return data;
}

static void pcm_write(UINT32 index, UINT8 data); // forward
static void sub_write16(UINT32 a, UINT16 d, INT32 upper, INT32 lower); // forward

static void cdc_transfer_dma()
{
	if (!cdc.tr_active) return;
	UINT8 dest = cdc.tr_destination;
	if (dest != 4 && dest != 5 && dest != 7) return;

	UINT16 data = CdcRam[(cdc.tr_source >> 1) & 0x1fff];

	switch (dest) {
		case 4: { // pcm wave ram, two byte writes per transfer
			UINT32 offs = 0x1000 | ((cdc.tr_address >> 1) & 0xfff);
			pcm_write(offs | 1, data >> 8);
			pcm_write(offs | 0, data & 0xff);
			cdc.tr_address += 2;
		}
		break;

		case 5:
			sub_write16(cdc.tr_address & 0x7fffe, data, 1, 1);
		break;

		case 7:
			if (wram_mode == 0) {
				WordRam[(cdc.tr_address >> 1) & 0x1ffff] = data;
			} else {
				sub_write16(0x0c0000 | (cdc.tr_address & 0x1fffe), data, 1, 1);
			}
		break;
	}

	cdc.tr_address = (cdc.tr_address + 2) & 0x7ffff;
	cdc.tr_source += 2;
	cdc_length_step();
	cdc_length_step();
}

static void cdc_decode(INT32 sector)
{
	if (!cdc.dec_enable || !disc_present) return;

	UINT8 m, s, f;
	msf_from_lba(sector, &m, &s, &f);
	cdc.head_minute = dec2bcd(m);
	cdc.head_second = dec2bcd(s);
	cdc.head_frame  = dec2bcd(f);
	cdc.head_mode   = 0x01;

	cdc.dec_valid = 1;
	cdc.irq_decoder_pending = 1;
	cdc_poll();

	if (cdc.ctrl_write_request) {
		cdc.tr_pointer += 2352;
		cdc.tr_target  += 2352;

		// rebuild a raw mode 1 sector, the sync pattern lands at the tail
		UINT8 raw[2352];
		memset(raw, 0, sizeof(raw));
		memset(raw + 1, 0xff, 10);
		raw[12] = cdc.head_minute;
		raw[13] = cdc.head_second;
		raw[14] = cdc.head_frame;
		raw[15] = cdc.head_mode;
		if (CDEmuReadDataSector(sector, raw + 16)) {
			memset(raw + 16, 0, 2048);
		}

		for (INT32 i = 0; i < 12; i += 2) {
			CdcRam[((cdc.tr_pointer + i + 2340) >> 1) & 0x1fff] = (raw[i] << 8) | raw[i + 1];
		}
		for (INT32 i = 0; i < 2340; i += 2) {
			CdcRam[((cdc.tr_pointer + i) >> 1) & 0x1fff] = (raw[12 + i] << 8) | raw[13 + i];
		}
	}
}

static UINT8 cdc_read()
{
	UINT8 data = 0;

	switch (cdc.address) {
		case 0x0: // COMIN
			if (cdc.cmd_empty) { data = 0xff; break; }
			data = cdc.cmd_fifo[cdc.cmd_read];
			cdc.cmd_read = (cdc.cmd_read + 1) & 7;
			if (cdc.cmd_read == cdc.cmd_write) {
				cdc.cmd_empty = 1;
				cdc.irq_command_pending = 0;
				cdc_poll();
			}
		break;

		case 0x1: // IFSTAT
			data  = (!cdc.st_active) << 0;
			data |= (!cdc.tr_active) << 1;
			data |= (!cdc.st_busy) << 2;
			data |= (!cdc.tr_busy) << 3;
			data |= 1 << 4;
			data |= (!cdc.irq_decoder_pending) << 5;
			data |= (!cdc.irq_transfer_pending) << 6;
			data |= (!cdc.irq_command_pending) << 7;
		break;

		case 0x2: data = cdc.tr_length & 0xff; break;			// DBCL
		case 0x3: data = (cdc.tr_length >> 8) & 0x0f; break;	// DBCH
		case 0x4: data = cdc.ctrl_head ? cdc.sub_file    : cdc.head_minute; break;
		case 0x5: data = cdc.ctrl_head ? cdc.sub_channel : cdc.head_second; break;
		case 0x6: data = cdc.ctrl_head ? cdc.sub_submode : cdc.head_frame;  break;
		case 0x7: data = cdc.ctrl_head ? cdc.sub_coding  : cdc.head_mode;   break;
		case 0x8: data = cdc.tr_pointer & 0xff; break;			// PTL
		case 0x9: data = cdc.tr_pointer >> 8; break;			// PTH
		case 0xa: data = cdc.tr_target & 0xff; break;			// WAL
		case 0xb: data = cdc.tr_target >> 8; break;				// WAH
		case 0xc: data = cdc.dec_enable << 7; break;			// STAT0: CRCOK
		case 0xd: data = 0; break;								// STAT1
		case 0xe: data = (cdc.dec_form << 2) | (cdc.dec_mode << 3); break; // STAT2

		case 0xf: // STAT3
			data = (!cdc.dec_valid) << 7;
			cdc.dec_valid = 0;
			cdc.irq_decoder_pending = 0;
			cdc_poll();
		break;
	}

	// COMIN reads do not increment the address
	if (cdc.address) cdc.address = (cdc.address + 1) & 0x1f;

	return data;
}

static void cdc_write(UINT8 data)
{
	switch (cdc.address) {
		case 0x0: // SBOUT
			if (cdc.st_wait && cdc.tr_busy) break;
			if (cdc.st_read == cdc.st_write && !cdc.st_empty) cdc.st_read = (cdc.st_read + 1) & 7;
			cdc.st_fifo[cdc.st_write] = data;
			cdc.st_write = (cdc.st_write + 1) & 7;
			cdc.st_empty = 0;
			cdc.st_active = 1;
			cdc.st_busy = 1;
		break;

		case 0x1: // IFCTRL
			cdc.st_enable           = (data >> 0) & 1;
			cdc.tr_enable           = (data >> 1) & 1;
			cdc.st_wait             = !((data >> 2) & 1);
			cdc.tr_wait             = !((data >> 3) & 1);
			cdc.ctrl_command_break  = !((data >> 4) & 1);
			cdc.irq_decoder_enable  = (data >> 5) & 1;
			cdc.irq_transfer_enable = (data >> 6) & 1;
			cdc.irq_command_enable  = (data >> 7) & 1;
			cdc_poll();
			if (!cdc.tr_enable) cdc_transfer_stop();
		break;

		case 0x2: cdc.tr_length = (cdc.tr_length & 0xf00) | data; break;					// DBCL
		case 0x3: cdc.tr_length = (cdc.tr_length & 0x0ff) | ((data & 0x0f) << 8); break;	// DBCH
		case 0x4: cdc.tr_source = (cdc.tr_source & 0xff00) | data; break;					// DACL
		case 0x5: cdc.tr_source = (cdc.tr_source & 0x00ff) | (data << 8); break;			// DACH
		case 0x6: cdc_transfer_start(); break;												// DTRG

		case 0x7: // DTACK
			cdc.irq_transfer_pending = 0;
			cdc_poll();
		break;

		case 0x8: cdc.tr_target = (cdc.tr_target & 0xff00) | data; break;					// WAL
		case 0x9: cdc.tr_target = (cdc.tr_target & 0x00ff) | (data << 8); break;			// WAH

		case 0xa: // CTRL0
			cdc.ctrl_pcode           = (data >> 0) & 1;
			cdc.ctrl_qcode           = (data >> 1) & 1;
			cdc.ctrl_write_request   = (data >> 2) & 1;
			cdc.ctrl_erasure_request = (data >> 3) & 1;
			cdc.ctrl_auto            = (data >> 4) & 1;
			cdc.ctrl_error           = (data >> 5) & 1;
			cdc.ctrl_edc             = (data >> 6) & 1;
			cdc.dec_enable           = (data >> 7) & 1;
			cdc.dec_mode = cdc.ctrl_mode;
			cdc.dec_form = cdc.ctrl_form & cdc.ctrl_auto;
		break;

		case 0xb: // CTRL1
			cdc.ctrl_head             = (data >> 0) & 1;
			cdc.ctrl_mode_byte_check  = (data >> 1) & 1;
			cdc.ctrl_form             = (data >> 2) & 1;
			cdc.ctrl_mode             = (data >> 3) & 1;
			cdc.ctrl_correction_write = (data >> 4) & 1;
			cdc.ctrl_descramble       = (data >> 5) & 1;
			cdc.ctrl_sync             = (data >> 6) & 1;
			cdc.ctrl_sync_irq         = (data >> 7) & 1;
			cdc.dec_mode = cdc.ctrl_mode;
			cdc.dec_form = cdc.ctrl_form & cdc.ctrl_auto;
		break;

		case 0xc: cdc.tr_pointer = (cdc.tr_pointer & 0xff00) | data; break;				// PTL
		case 0xd: cdc.tr_pointer = (cdc.tr_pointer & 0x00ff) | (data << 8); break;			// PTH

		case 0xe: // CTRL2
			cdc.ctrl_status_trigger     = (data >> 0) & 1;
			cdc.ctrl_status_control     = (data >> 1) & 1;
			cdc.ctrl_erasure_correction = (data >> 2) & 1;
		break;

		case 0xf: { // RESET
			UINT8 address = cdc.address;
			UINT16 stopwatch = cdc.stopwatch;
			UINT8 tr_destination = cdc.tr_destination;
			UINT32 tr_address = cdc.tr_address;
			UINT16 tr_source = cdc.tr_source, tr_target = cdc.tr_target, tr_pointer = cdc.tr_pointer, tr_length = cdc.tr_length;
			UINT8 tr_completed = cdc.tr_completed;
			UINT8 st_fifo[8], cmd_fifo[8];
			memcpy(st_fifo, cdc.st_fifo, 8);
			memcpy(cmd_fifo, cdc.cmd_fifo, 8);
			UINT8 st_read = cdc.st_read, st_write = cdc.st_write, st_empty = cdc.st_empty;
			UINT8 cmd_read = cdc.cmd_read, cmd_write = cdc.cmd_write, cmd_empty = cdc.cmd_empty;
			UINT8 dec_valid = cdc.dec_valid;

			memset(&cdc, 0, sizeof(cdc));

			cdc.address = address;
			cdc.stopwatch = stopwatch;
			cdc.tr_destination = tr_destination;
			cdc.tr_address = tr_address;
			cdc.tr_source = tr_source;
			cdc.tr_target = tr_target;
			cdc.tr_pointer = tr_pointer;
			cdc.tr_length = tr_length;
			cdc.tr_completed = tr_completed;
			memcpy(cdc.st_fifo, st_fifo, 8);
			memcpy(cdc.cmd_fifo, cmd_fifo, 8);
			cdc.st_read = st_read; cdc.st_write = st_write; cdc.st_empty = st_empty;
			cdc.cmd_read = cmd_read; cdc.cmd_write = cmd_write; cdc.cmd_empty = cmd_empty;
			cdc.dec_valid = dec_valid;

			cdc.st_wait = 1;
			cdc.tr_wait = 1;
			cdc.ctrl_command_break = 1;

			cdc_transfer_stop();
			cdc_poll();
		}
		break;
	}

	// SBOUT writes do not increment the address
	if (cdc.address) cdc.address = (cdc.address + 1) & 0x1f;
}

static void cdc_power()
{
	memset(CdcRam, 0, 0x4000);
	memset(&cdc, 0, sizeof(cdc));
	cdc.cmd_empty = 1;
	cdc.st_empty = 1;
}

// -------------------------------------------------------------------------
// CDD

static void cdd_update_audio()
{
	INT32 want = (cdd.io_status == CDD_PLAYING) && track_is_audio(cdd.io_track);

	if (want) {
		if (!cdd.audio_playing || (CDEmuGetStatus() != playing && cdd.audio_track != cdd.io_track)) {
			INT32 aba = cdd.io_sector + 150;
			if (aba < 0) aba = 0;
			CDEmuPlay(dec2bcd(aba / (75 * 60)), dec2bcd((aba / 75) % 60), dec2bcd(aba % 75));
			cdd.audio_playing = 1;
			cdd.audio_track = cdd.io_track;
		}
	} else if (cdd.audio_playing) {
		CDEmuStop();
		cdd.audio_playing = 0;
	}
}

static void cdd_advance()
{
	INT32 track = cdd_in_track(cdd.io_sector + 1);
	if (track) {
		cdd.io_track = track;
		cdd.io_sector++;
		return;
	}

	if (cdd.io_sector + 1 < 0) {
		cdd.io_status = CDD_LEAD_IN;
		cdd.io_track = 0xa0;
	} else {
		cdd.io_status = CDD_LEAD_OUT;
		cdd.io_track = 0xaa;
	}
}

static void cdd_clock()
{
	if (++cdd.counter < 434) return;
	cdd.counter = 0;

	// 75hz
	if (!cdd.host_clock_enable) return;

	if (cdd.status_pending) {
		cdd.status_pending = 0;
		irq_raise(4);
	}

	switch (cdd.io_status) {
		case CDD_STOPPED:
			cdd.io_status = disc_present ? CDD_READING_TOC : CDD_NO_DISC;
			cdd.io_sector = 0;
			cdd.io_track = 0;
		break;

		case CDD_READING_TOC:
			cdd.io_sector++;
			if (!cdd_in_lead_in(cdd.io_sector)) {
				cdd.io_status = CDD_PAUSED;
				INT32 track = cdd_in_track(cdd.io_sector);
				if (track) cdd.io_track = track;
				cdd.io_toc_read = 1;
			}
		break;

		case CDD_PLAYING:
			if (!track_is_audio(cdd.io_track)) {
				cdc_decode(cdd.io_sector);
			}
			cdd_advance();
		break;

		case CDD_TRACKING:
		case CDD_SEEKING: {
			if (cdd.io_latency && --cdd.io_latency) break;
			cdd.io_status = cdd.io_seeking;
			INT32 track = cdd_in_track(cdd.io_sector);
			if (track) cdd.io_track = track;
		}
		break;
	}

	cdd_update_audio();
}

// normalized position of a sector on the disc surface, for seek latency
static double cdd_position(INT32 sector)
{
	const double sectors = 7500.0 + 330000.0 + 6750.0;
	const double radius = 0.058 - 0.024;
	const double inner = 0.024 * 0.024;
	const double outer = 0.058 * 0.058;

	sector += -7350;
	double v = sector / sectors * (outer - inner) + inner;
	if (v < 0.0) v = 0.0;

	return sqrt(v) / radius;
}

static INT32 cdd_valid()
{
	UINT8 sum = 0;
	for (INT32 i = 0; i < 9; i++) sum += cdd.command[i];
	return ((~sum) & 0xf) == cdd.command[9];
}

static void cdd_checksum()
{
	UINT8 sum = 0;
	for (INT32 i = 0; i < 9; i++) sum += cdd.status[i];
	cdd.status[9] = (~sum) & 0xf;
}

static void cdd_status_clear(INT32 first)
{
	for (INT32 i = first; i < 9; i++) cdd.status[i] = 0;
}

static void cdd_put_msf(UINT8 m, UINT8 s, UINT8 f)
{
	cdd.status[2] = m / 10; cdd.status[3] = m % 10;
	cdd.status[4] = s / 10; cdd.status[5] = s % 10;
	cdd.status[6] = f / 10; cdd.status[7] = f % 10;
}

static void cdd_seek(INT32 seek_status)
{
	INT32 m = cdd.command[2] * 10 + cdd.command[3];
	INT32 s = cdd.command[4] * 10 + cdd.command[5];
	INT32 f = cdd.command[6] * 10 + cdd.command[7];
	INT32 lba = msf_to_lba(m, s, f) - 3;

	cdd.counter = 0;
	cdd.io_status = CDD_SEEKING;
	cdd.io_seeking = seek_status;
	cdd.io_latency = (UINT16)(11 + 112.5 * fabs(cdd_position(cdd.io_sector) - cdd_position(lba)));
	cdd.io_sector = lba;

	cdd.status[1] = 0xf;
	cdd_status_clear(2);
}

static void cdd_process()
{
	if (!cdd_valid()) {
		cdd.io_status = CDD_CHECKSUM_ERROR;
	} else {
		switch (cdd.command[0]) {
			case 0x0: // idle
				if (!cdd.io_latency && cdd.status[1] == 0xf) {
					cdd.status[1] = 0x2;
					cdd.status[2] = (cdd.io_track / 10) & 0xf;
					cdd.status[3] = cdd.io_track % 10;
				}
			break;

			case 0x1: // stop
				cdd.io_status = disc_present ? CDD_STOPPED : CDD_NO_DISC;
				cdd.status[1] = 0x0;
				cdd_status_clear(2);
			break;

			case 0x2: { // request
				UINT8 m, s, f;
				switch (cdd.command[3]) {
					case 0x0: // absolute time
						msf_from_lba(cdd.io_sector, &m, &s, &f);
						cdd.status[1] = cdd.command[3];
						cdd_put_msf(m, s, f);
						cdd.status[8] = track_is_data(cdd.io_track) << 2;
					break;

					case 0x1: // relative time
						msf_from_aba(cdd.io_sector - (track_valid(cdd.io_track) ? toc_lba[cdd.io_track] : 0), &m, &s, &f);
						cdd.status[1] = cdd.command[3];
						cdd_put_msf(m, s, f);
						cdd.status[8] = track_is_data(cdd.io_track) << 2;
					break;

					case 0x2: // track information
						cdd.status[1] = cdd.command[3];
						cdd.status[2] = (cdd.io_track / 10) & 0xf;
						cdd.status[3] = cdd.io_track % 10;
						cdd_status_clear(4);
						if (cdd_in_lead_in(cdd.io_sector))  { cdd.status[2] = 0x0; cdd.status[3] = 0x0; }
						if (cdd_in_lead_out(cdd.io_sector)) { cdd.status[2] = 0xa; cdd.status[3] = 0xa; }
					break;

					case 0x3: // disc completion time
						msf_from_lba(toc_leadout, &m, &s, &f);
						cdd.status[1] = cdd.command[3];
						cdd_put_msf(m, s, f);
						cdd.status[8] = 0x0;
					break;

					case 0x4: // first and last track numbers
						cdd.status[1] = cdd.command[3];
						cdd.status[2] = toc_first / 10; cdd.status[3] = toc_first % 10;
						cdd.status[4] = toc_last / 10;  cdd.status[5] = toc_last % 10;
						cdd.status[6] = 0x0; cdd.status[7] = 0x0;
						cdd.status[8] = 0x0;
					break;

					case 0x5: { // track start time
						if (cdd.command[4] > 9 || cdd.command[5] > 9) break;
						INT32 track = cdd.command[4] * 10 + cdd.command[5];
						if (track_valid(track)) {
							msf_from_lba(toc_lba[track], &m, &s, &f);
						} else {
							m = s = f = 0;
						}
						cdd.status[1] = cdd.command[3];
						cdd_put_msf(m, s, f);
						if (track_is_data(track)) cdd.status[6] |= 0x8;
						cdd.status[8] = track % 10;
					}
					break;

					case 0x6: // error information
						cdd.status[1] = cdd.command[3];
						cdd_status_clear(2);
					break;
				}
			}
			break;

			case 0x3: cdd_seek(CDD_PLAYING); break;	// seek & play
			case 0x4: cdd_seek(CDD_PAUSED); break;	// seek & pause

			case 0x6: // pause
				if (cdd.io_status == CDD_SEEKING) cdd.io_seeking = CDD_PAUSED;
				else cdd.io_status = CDD_PAUSED;
			break;

			case 0x7: // play
				if (cdd.io_status == CDD_SEEKING) cdd.io_seeking = CDD_PLAYING;
				else cdd.io_status = CDD_PLAYING;
			break;

			case 0xa: // track skip, handled as a null seek
				cdd.counter = 0;
				cdd.io_status = CDD_TRACKING;
				cdd.io_seeking = CDD_PAUSED;
				cdd.io_latency = 0;
				cdd.status[1] = 0xf;
				cdd_status_clear(2);
			break;

			default:
				cdd.io_status = CDD_COMMAND_ERROR;
			break;
		}
	}

	cdd.status[0] = cdd.io_status;
	cdd_checksum();
	cdd.status_pending = 1;

	cdd_update_audio();
}

static void cdd_insert()
{
	if (!disc_present) {
		cdd.io_status = CDD_NO_DISC;
		return;
	}

	// start right before the pregap so that reading the toc completes quickly
	cdd.io_status = CDD_READING_TOC;
	cdd.io_sector = -151;
	cdd.io_track = 0;
}

static void cdd_power()
{
	irq_mask &= ~0x10;
	irq_lower(4);

	if (cdd.audio_playing) CDEmuStop();

	memset(&cdd, 0, sizeof(cdd));
	cdd.io_status = CDD_NO_DISC;
	cdd.dac_attenuator = 0x4000;
	cdd.dac_attenuated = 0x4000;

	cdd_insert();
	cdd_checksum();
}

// -------------------------------------------------------------------------
// timer

static void timer_clock()
{
	if (timer_frequency == 0) return;

	if (timer_counter == 0) {
		timer_counter = timer_frequency;
		irq_raise(3);
	} else {
		timer_counter--;
	}
}

// -------------------------------------------------------------------------
// graphics asic

static inline UINT8 gpu_read(UINT32 address)
{
	address &= 0x7ffff;
	INT32 shift = 12 - ((address & 3) << 2);
	return (WordRam[address >> 2] >> shift) & 0xf;
}

static inline void gpu_write(UINT32 address, UINT8 data)
{
	address &= 0x7ffff;
	INT32 shift = 12 - ((address & 3) << 2);
	UINT16 *p = &WordRam[address >> 2];
	*p = (*p & ~(0xf << shift)) | ((data & 0xf) << shift);
}

static void gpu_render(UINT32 address, INT32 width)
{
	INT32 stamp_shift = 4;
	INT32 map_shift = 4 << gpu.stamp_map_size;
	UINT32 index_mask = 0x7ff;
	UINT32 pixel_mask = 0x0f;

	if (gpu.stamp_tile_size) {
		stamp_shift++;
		map_shift--;
		index_mask &= ~3;
		pixel_mask |= 0x10;
	}

	UINT32 image_width = (gpu.image_vcells + 1) << 6;
	UINT32 map_mask = (!gpu.stamp_map_size) ? 0x07ffff : 0x7fffff;

	UINT32 x = (WordRam[gpu.vector_address] << 8) & 0xffffff;
	gpu.vector_address = (gpu.vector_address + 1) & 0x1ffff;
	UINT32 y = (WordRam[gpu.vector_address] << 8) & 0xffffff;
	gpu.vector_address = (gpu.vector_address + 1) & 0x1ffff;
	INT16 xstep = (INT16)WordRam[gpu.vector_address];
	gpu.vector_address = (gpu.vector_address + 1) & 0x1ffff;
	INT16 ystep = (INT16)WordRam[gpu.vector_address];
	gpu.vector_address = (gpu.vector_address + 1) & 0x1ffff;

	while (width--) {
		if (gpu.stamp_repeat) {
			x &= map_mask;
			y &= map_mask;
		}

		UINT8 output = 0;
		if (((x | y) & ~map_mask & 0xffffff) == 0) {
			UINT32 xtrunc = x >> 11;
			UINT32 ytrunc = y >> 11;
			UINT32 xstamp = xtrunc >> stamp_shift;
			UINT32 ystamp = ytrunc >> stamp_shift;

			UINT16 map_entry = WordRam[(gpu.stamp_map_address + (ystamp << map_shift) + xstamp) & 0x1ffff];
			UINT32 index = map_entry & index_mask;
			INT32 lroll = (map_entry >> 13) & 1;
			INT32 hroll = (map_entry >> 14) & 1;
			INT32 hflip = (map_entry >> 15) & 1;

			if (index) {
				if (hflip) { xtrunc = ~xtrunc; }
				if (hroll) { xtrunc = ~xtrunc; ytrunc = ~ytrunc; }
				if (lroll) { UINT32 t = xtrunc; xtrunc = ~ytrunc; ytrunc = t; }

				UINT32 xpixel = xtrunc & pixel_mask;
				UINT32 ypixel = ytrunc & pixel_mask;

				output = gpu_read((index << 8) | ((xpixel & ~7) << stamp_shift) | (ypixel << 3) | (xpixel & 7));
			}
		}

		UINT8 input = gpu_read(address);
		switch (wram_priority) {
			case 1: output = input ? input : output; break;
			case 2: output = output ? output : input; break;
			case 3: output = input; break;
		}
		gpu_write(address, output);

		address = (address + 1) & 0x7ffff;
		if (!(address & 7)) address = (address + image_width - 8) & 0x7ffff;

		x = (x + xstep) & 0xffffff;
		y = (y + ystep) & 0xffffff;
	}
}

static void gpu_start()
{
	if (wram_mode) return; // 2Mbit mode only

	gpu.active = 1;
	gpu.period = 5 * gpu.image_hdots;
	if (gpu.period == 0) gpu.period = 1;
	gpu.counter = 0;

	gpu.image_address = ((gpu.image_base << 1) + gpu.image_offset) & 0x7ffff;
	gpu.vector_address = (gpu.vector_base >> 1) & 0x1ffff;
	gpu.stamp_map_address = gpu.stamp_map_base >> 1;

	if (gpu.stamp_map_size == 0 && gpu.stamp_tile_size == 0) gpu.stamp_map_address &= 0x1ff00;
	if (gpu.stamp_map_size == 0 && gpu.stamp_tile_size == 1) gpu.stamp_map_address &= 0x1ffc0;
	if (gpu.stamp_map_size == 1 && gpu.stamp_tile_size == 0) gpu.stamp_map_address &= 0x10000;
	if (gpu.stamp_map_size == 1 && gpu.stamp_tile_size == 1) gpu.stamp_map_address &= 0x1c000;
}

static void gpu_step(INT32 clocks)
{
	if (!gpu.active) return;

	gpu.counter += clocks;
	while (gpu.counter >= gpu.period) {
		gpu.counter -= gpu.period;
		gpu_render(gpu.image_address, gpu.image_hdots);
		gpu.image_address = (gpu.image_address + 8) & 0x7ffff;
		gpu.image_vdots--;
		if (gpu.image_vdots == 0) {
			gpu.active = 0;
			irq_raise(1);
			break;
		}
	}
}

// -------------------------------------------------------------------------
// pcm

static void pcm_clock()
{
	INT32 left = 0;
	INT32 right = 0;

	if (pcm_enable) {
		for (INT32 i = 0; i < 8; i++) {
			scd_pcm_channel *ch = &pcm_ch[i];
			if (!ch->enable) continue;

			INT32 data = PcmRam[(ch->address >> 11) & 0xffff];
			if (data == 0xff) { // loop
				ch->address = ch->loop << 11;
				data = PcmRam[ch->loop];
				if (data == 0xff) continue;
			} else {
				ch->address = (ch->address + ch->step) & 0x7ffffff;
			}

			data = (data & 0x80) ? (data & 0x7f) : -(data & 0x7f);

			left  += (data * ch->envelope * (ch->pan & 0x0f)) >> 5;
			right += (data * ch->envelope * (ch->pan >> 4)) >> 5;
		}
	}

	// 10-bit dac
	if (left < -32768) left = -32768; else if (left > 32767) left = 32767;
	if (right < -32768) right = -32768; else if (right > 32767) right = 32767;
	left  = (left >> 2) & ~15;
	right = (right >> 2) & ~15;

	INT32 pos = (pcm_fifo_read + pcm_fifo_count) % PCM_FIFO_SIZE;
	pcm_fifo[pos][0] = left;
	pcm_fifo[pos][1] = right;
	if (pcm_fifo_count < PCM_FIFO_SIZE) {
		pcm_fifo_count++;
	} else {
		pcm_fifo_read = (pcm_fifo_read + 1) % PCM_FIFO_SIZE;
	}
}

static void pcm_stream_update(INT16 **streams, INT32 samples)
{
	for (INT32 i = 0; i < samples; i++) {
		if (pcm_fifo_count) {
			pcm_last[0] = pcm_fifo[pcm_fifo_read][0];
			pcm_last[1] = pcm_fifo[pcm_fifo_read][1];
			pcm_fifo_read = (pcm_fifo_read + 1) % PCM_FIFO_SIZE;
			pcm_fifo_count--;
		}
		streams[0][i] = pcm_last[0];
		streams[1][i] = pcm_last[1];
	}
}

static UINT8 pcm_read(UINT32 index)
{
	if (index >= 0x1000 && index <= 0x1fff) {
		return PcmRam[(pcm_bank << 12) | (index & 0xfff)];
	}

	if (index >= 0x0010 && index <= 0x001f) {
		UINT16 offset = pcm_ch[(index >> 1) & 7].address >> 11;
		return (index & 1) ? (offset >> 8) : (offset & 0xff);
	}

	return 0xff;
}

static void pcm_write(UINT32 index, UINT8 data)
{
	if (index >= 0x1000 && index <= 0x1fff) {
		PcmRam[(pcm_bank << 12) | (index & 0xfff)] = data;
		return;
	}

	scd_pcm_channel *ch = &pcm_ch[pcm_channel];

	switch (index) {
		case 0x0: ch->envelope = data; break;
		case 0x1: ch->pan = data; break;
		case 0x2: ch->step = (ch->step & 0xff00) | data; break;
		case 0x3: ch->step = (ch->step & 0x00ff) | (data << 8); break;
		case 0x4: ch->loop = (ch->loop & 0xff00) | data; break;
		case 0x5: ch->loop = (ch->loop & 0x00ff) | (data << 8); break;

		case 0x6:
			ch->start = data;
			if (!ch->enable) ch->address = (ch->start << 8) << 11;
		break;

		case 0x7:
			if (data & 0x40) {
				pcm_channel = data & 7;
			} else {
				pcm_bank = data & 0x0f;
			}
			pcm_enable = (data >> 7) & 1;
		break;

		case 0x8:
			for (INT32 i = 0; i < 8; i++) {
				pcm_ch[i].enable = !(data & 1);
				data >>= 1;
				if (!pcm_ch[i].enable) pcm_ch[i].address = (pcm_ch[i].start << 8) << 11;
			}
		break;
	}
}

static void pcm_power()
{
	memset(PcmRam, 0, 0x10000);
	pcm_enable = pcm_bank = pcm_channel = 0;
	memset(pcm_ch, 0, sizeof(pcm_ch));
	for (INT32 i = 0; i < 8; i++) pcm_ch[i].pan = 0xff;

	pcm_fifo_read = pcm_fifo_count = 0;
	pcm_last[0] = pcm_last[1] = 0;
}

// -------------------------------------------------------------------------
// sub cpu i/o (gate array)

static UINT16 sub_io_read(UINT32 a, INT32 upper, INT32 lower)
{
	a = 0xff8000 | (a & 0x1fe);

	switch (a) {
		case 0xff8000:
			return 1 | (led_status << 8); // peripheral ready, version 0

		case 0xff8002: {
			UINT16 d = (!wram_mode) ? !wram_switch : wram_select;
			d |= ((!wram_mode) ? wram_switch : wram_switch_request) << 1;
			d |= wram_mode << 2;
			d |= wram_priority << 3;
			d |= pram_protect << 8;
			return d;
		}

		case 0xff8004:
			return cdc.address | (cdc.tr_destination << 8) | (cdc.tr_ready << 14) | (cdc.tr_completed << 15);

		case 0xff8006:
			return lower ? cdc_read() : 0;

		case 0xff8008:
			return cdc_transfer_read();

		case 0xff800a:
			return (cdc.tr_address >> 3) & 0xffff;

		case 0xff800c:
			return cdc.stopwatch & 0xfff;

		case 0xff800e:
			return (comm_cfm << 8) | comm_cfs;

		case 0xff8030:
			return timer_frequency;

		case 0xff8032:
			return irq_mask & 0x7e;

		case 0xff8034:
			return 0; // fade data transfer complete

		case 0xff8036:
			return (cdd.host_clock_enable << 2) | (track_is_data(cdd.io_track) << 8);

		case 0xff804c:
			return gpu.font_bg | (gpu.font_fg << 4);

		case 0xff804e:
			return gpu.font_data;

		case 0xff8058:
			return gpu.stamp_repeat | (gpu.stamp_tile_size << 1) | (gpu.stamp_map_size << 2) | (gpu.active << 15);

		case 0xff805a:
			return ((gpu.stamp_map_base >> 9) & 0x1ff) << 7;

		case 0xff805c:
			return gpu.image_vcells;

		case 0xff805e:
			return ((gpu.image_base >> 5) & 0x1fff) << 3;

		case 0xff8060:
			return gpu.image_offset;

		case 0xff8062:
			return gpu.image_hdots;

		case 0xff8064:
			return gpu.image_vdots;

		case 0xff8066:
			return ((gpu.vector_base >> 3) & 0x7fff) << 1;

		case 0xff8068:
			return 0; // subcode address
	}

	if (a >= 0xff8010 && a <= 0xff801f) return comm_command[(a - 0xff8010) >> 1];
	if (a >= 0xff8020 && a <= 0xff802f) return comm_status[(a - 0xff8020) >> 1];

	if (a >= 0xff8038 && a <= 0xff8041) {
		UINT32 index = a - 0xff8038;
		return cdd.status[index | 1] | (cdd.status[index] << 8);
	}

	if (a >= 0xff8042 && a <= 0xff804b) {
		UINT32 index = a - 0xff8042;
		return cdd.command[index | 1] | (cdd.command[index] << 8);
	}

	if (a >= 0xff8050 && a <= 0xff8057) {
		INT32 index = 12 - ((a - 0xff8050) << 1);
		UINT16 d = 0;
		for (INT32 i = 0; i < 4; i++) {
			UINT8 c = (gpu.font_data & (1 << (index | i))) ? gpu.font_fg : gpu.font_bg;
			d |= c << (i * 4);
		}
		return d;
	}

	return 0; // includes subcode buffer (not emulated)
}

static void sub_io_write(UINT32 a, UINT16 d, INT32 upper, INT32 lower)
{
	a = 0xff8000 | (a & 0x1fe);

	switch (a) {
		case 0xff8000:
			if (lower && !(d & 1)) cdd_power(); // peripheral reset
			if (upper) led_status = (d >> 8) & 3;
		return;

		case 0xff8002: { // 8-bit register, writes go to the low byte
			UINT8 v = d & 0xff;
			if (wram_select != (v & 1)) wram_switch_request = 0;
			wram_select = v & 1;
			wram_mode = (v >> 2) & 1;
			wram_priority = (v >> 3) & 3;
			if (wram_select) wram_switch = 0;
		}
		return;

		case 0xff8004:
			if (lower) cdc.address = d & 0x1f;
			if (upper) cdc.tr_destination = (d >> 8) & 7;
		return;

		case 0xff8006:
			if (lower) cdc_write(d & 0xff);
		return;

		case 0xff800a:
			cdc.tr_address = (d & 0xffff) << 3;
		return;

		case 0xff800c:
			cdc.stopwatch = 0;
		return;

		case 0xff800e:
			comm_cfs = d & 0xff;
		return;

		case 0xff8030:
			timer_counter = timer_frequency = d & 0xff;
		return;

		case 0xff8032:
			if (lower) irq_mask = d & 0x7e;
		return;

		case 0xff8034:
			cdd.dac_rate = (d >> 1) & 1;
			cdd.dac_deemphasis = (d >> 2) & 3;
			cdd.dac_attenuator = d & 0xffc0;
		return;

		case 0xff8036:
			if (lower) {
				if (!cdd.host_clock_enable && (d & 4)) irq_raise(4);
				cdd.host_clock_enable = (d >> 2) & 1;
				cdd.counter = 0;
			}
		return;

		case 0xff804c:
			gpu.font_bg = d & 0xf;
			gpu.font_fg = (d >> 4) & 0xf;
		return;

		case 0xff804e:
			if (lower) gpu.font_data = (gpu.font_data & 0xff00) | (d & 0x00ff);
			if (upper) gpu.font_data = (gpu.font_data & 0x00ff) | (d & 0xff00);
		return;

		case 0xff8058:
			if (lower) {
				gpu.stamp_repeat = d & 1;
				gpu.stamp_tile_size = (d >> 1) & 1;
				gpu.stamp_map_size = (d >> 2) & 1;
			}
		return;

		case 0xff805a:
			gpu.stamp_map_base = (gpu.stamp_map_base & 0x7f) | (((d >> 5) & 0x7ff) << 7);
		return;

		case 0xff805c:
			gpu.image_vcells = d & 0x1f;
		return;

		case 0xff805e:
			gpu.image_base = (gpu.image_base & 0x1f) | (((d >> 3) & 0x1fff) << 5);
		return;

		case 0xff8060:
			gpu.image_offset = d & 0x3f;
		return;

		case 0xff8062:
			gpu.image_hdots = d & 0x1ff;
		return;

		case 0xff8064:
			gpu.image_vdots = d & 0xff;
		return;

		case 0xff8066:
			gpu.vector_base = (gpu.vector_base & 0x7) | (((d >> 1) & 0x7fff) << 3);
			gpu_start();
		return;
	}

	if (a >= 0xff8020 && a <= 0xff802f) {
		UINT16 *p = &comm_status[(a - 0xff8020) >> 1];
		if (lower) *p = (*p & 0xff00) | (d & 0x00ff);
		if (upper) *p = (*p & 0x00ff) | (d & 0xff00);
		return;
	}

	if (a >= 0xff8042 && a <= 0xff804b) {
		UINT32 index = a - 0xff8042;
		if (lower) cdd.command[index | 1] = d & 0xf;
		if (upper) cdd.command[index] = (d >> 8) & 0xf;
		if (lower && (index | 1) == 9) cdd_process();
		return;
	}
}

// -------------------------------------------------------------------------
// sub cpu bus

// 2M word ram owned by the main cpu: the sub cpu bus is held (/DTACK) until the main cpu
// gives it back (Mortal Kombat). The instruction runs to its end, then is replayed once released.
static UINT32 wait_regs[16];
static UINT32 wait_pc;
static UINT32 wait_sr;

static void wait_revert(INT32 mode, INT32 reg, INT32 size)
{
	if (size == 1 && reg == 7) size = 2;
	if (mode == 3) wait_regs[8 + reg] -= size; // (An)+
	if (mode == 4) wait_regs[8 + reg] += size; // -(An)
}

static void sub_wram_block(UINT32 address, INT32 write)
{
	if (sub_wram_wait || SekGetActive() != 1) return;

	for (INT32 i = 0; i < 16; i++)
		wait_regs[i] = SekDbgGetRegister((SekRegister)(SEK_REG_D0 + i));
	wait_pc = SekDbgGetRegister(SEK_REG_PPC);
	wait_sr = SekDbgGetRegister(SEK_REG_SR);

	// undo the address register updates already done by the instruction
	UINT32 ir = SekFetchWord(wait_pc);
	if (ir >= 0x1000 && ir < 0x4000) { // move
		static const INT32 move_size[4] = { 0, 1, 4, 2 };
		INT32 size = move_size[(ir >> 12) & 3];
		wait_revert((ir >> 3) & 7, ir & 7, size);
		if (write) wait_revert((ir >> 6) & 7, (ir >> 9) & 7, size);
	} else if ((ir & 0xfb80) != 0x4880) { // movem updates the register after the transfer
		INT32 size = 1 << ((ir >> 6) & 3);
		if ((ir & 0xb0c0) == 0x90c0 || (ir & 0xf0c0) == 0xb0c0) size = (ir & 0x100) ? 4 : 2; // adda, suba, cmpa
		if ((ir & 0xf100) == 0x0100 || (ir & 0xff00) == 0x0800 || (ir & 0xffc0) == 0x4ac0 || (ir & 0xf0c0) == 0x50c0) size = 1; // bit ops, tas, scc
		if (size == 8) size = 2;
		wait_revert((ir >> 3) & 7, ir & 7, size);
	}

	sub_wram_wait = 1;
	SekRunEnd();
}

// returns 0 if the cpu core can't set the registers (the access is then simply dropped)
static INT32 sub_wram_replay()
{
	// condition codes may be stale inside a handler (Cyclone), only the system byte is restored
	if (!SekDbgSetRegister(SEK_REG_SR, (wait_sr & 0xff00) | (SekDbgGetRegister(SEK_REG_SR) & 0x00ff))) return 0;
	for (INT32 i = 0; i < 16; i++)
		SekDbgSetRegister((SekRegister)(SEK_REG_D0 + i), wait_regs[i]);
	SekDbgSetRegister(SEK_REG_PC, wait_pc);
	return 1;
}

static UINT16 sub_read16(UINT32 a, INT32 upper, INT32 lower)
{
	a &= 0xffffff;

	if (a < 0x080000) {
		return prg_read_word(a);
	}

	if (a < 0x0c0000) {
		if (wram_mode == 0) {
			if (wram_switch == 0) {
				sub_wram_block(a, 0);
				return 0xffff;
			}
			return WordRam[(a >> 1) & 0x1ffff];
		}

		// 1Mbit mode: dot-mapped view, one nibble per byte
		INT32 ofs = (a & 2) ? 0 : 8;
		UINT16 w = WordRam[((a >> 1) & 0x1fffe) | (wram_select ^ 1)];
		return (((w >> (4 + ofs)) & 0xf) << 8) | ((w >> ofs) & 0xf);
	}

	if (a < 0x0e0000) {
		if (wram_mode == 1) return WordRam[(a & 0x1fffe) | (wram_select ^ 1)];
		return 0;
	}

	if (a >= 0xfe0000 && a <= 0xfeffff) {
		return lower ? BackupRam[(a >> 1) & 0x1fff] : 0;
	}

	if (a >= 0xff0000 && a <= 0xff7fff) {
		return lower ? pcm_read((a >> 1) & 0x1fff) : 0;
	}

	if (a >= 0xff8000) {
		return sub_io_read(a, upper, lower);
	}

	return 0;
}

static void sub_write16(UINT32 a, UINT16 d, INT32 upper, INT32 lower)
{
	a &= 0xffffff;

	if (a < 0x080000) {
		if (a >= ((UINT32)pram_protect << 9)) prg_write(a, d, upper, lower);
		return;
	}

	if (a < 0x0c0000) {
		if (wram_mode == 0) {
			if (wram_switch == 0) {
				sub_wram_block(a, 1);
				return;
			}
			wram_write((a >> 1) & 0x1ffff, d, upper, lower);
			return;
		}

		// 1Mbit mode: dot-mapped view, with priority modes
		INT32 ofs = (a & 2) ? 0 : 8;
		UINT32 idx = ((a >> 1) & 0x1fffe) | (wram_select ^ 1);
		UINT16 w = WordRam[idx];
		UINT16 hi = (w >> (4 + ofs)) & 0xf;
		UINT16 lo = (w >> ofs) & 0xf;
		UINT16 dhi = (d >> 8) & 0xf;
		UINT16 dlo = d & 0xf;

		switch (wram_priority) {
			case 0:
				if (upper) hi = dhi;
				if (lower) lo = dlo;
			break;

			case 1: // under write
				if (upper && !hi) hi = dhi;
				if (lower && !lo) lo = dlo;
			break;

			case 2: // over write
				if (upper && dhi) hi = dhi;
				if (lower && dlo) lo = dlo;
			break;

			default:
				return;
		}

		WordRam[idx] = (w & ~(0xff << ofs)) | (hi << (4 + ofs)) | (lo << ofs);
		return;
	}

	if (a < 0x0e0000) {
		if (wram_mode == 1) wram_write((a & 0x1fffe) | (wram_select ^ 1), d, upper, lower);
		return;
	}

	if (a >= 0xfe0000 && a <= 0xfeffff) {
		if (lower) BackupRam[(a >> 1) & 0x1fff] = d & 0xff;
		return;
	}

	if (a >= 0xff0000 && a <= 0xff7fff) {
		if (lower) pcm_write((a >> 1) & 0x1fff, d & 0xff);
		return;
	}

	if (a >= 0xff8000) {
		sub_io_write(a, d, upper, lower);
	}
}

static UINT8 __fastcall sub_read_byte(UINT32 a)
{
	UINT16 d = sub_read16(a & ~1, !(a & 1), a & 1);
	return (a & 1) ? (d & 0xff) : (d >> 8);
}

static UINT16 __fastcall sub_read_word(UINT32 a)
{
	return sub_read16(a, 1, 1);
}

static void __fastcall sub_write_byte(UINT32 a, UINT8 d)
{
	sub_write16(a & ~1, (d << 8) | d, !(a & 1), a & 1);
}

static void __fastcall sub_write_word(UINT32 a, UINT16 d)
{
	sub_write16(a, d, 1, 1);
}

// -------------------------------------------------------------------------
// main cpu bus

static UINT16 main_read16(UINT32 a)
{
	a &= 0x3fffff;
	a &= ~0x1c0000; // mirrors

	if (a < 0x020000) {
		if (a == 0x70) return vector_level4 >> 16;
		if (a == 0x72) return vector_level4 & 0xffff;
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(BiosRom + (a & 0x1fffe))));
	}

	if (a < 0x040000) {
		if (!sub_request) return 0xffff;
		return prg_read_word((pram_bank << 17) | (a & 0x1fffe));
	}

	if (a >= 0x200000 && a < 0x240000) {
		UINT32 idx;
		if (wram_mode == 0) {
			if (wram_switch == 1) return 0xffff;
			idx = (a >> 1) & 0x1ffff;
		} else {
			idx = wram_main_1m_index(a);
		}

		return WordRam[idx];
	}

	return 0xffff;
}

static void main_write16(UINT32 a, UINT16 d, INT32 upper, INT32 lower)
{
	a &= 0x3fffff;
	a &= ~0x1c0000;

	if (a >= 0x020000 && a < 0x040000) {
		if (!sub_request) return;
		prg_write((pram_bank << 17) | (a & 0x1fffe), d, upper, lower);
		return;
	}

	if (a >= 0x200000 && a < 0x240000) {
		UINT32 idx;
		if (wram_mode == 0) {
			if (wram_switch == 1) return;
			idx = (a >> 1) & 0x1ffff;
		} else {
			idx = wram_main_1m_index(a);
		}
		wram_write(idx, d, upper, lower);
	}
}

static UINT8 __fastcall main_read_byte(UINT32 a)
{
	UINT16 d = main_read16(a & ~1);
	return (a & 1) ? (d & 0xff) : (d >> 8);
}

static UINT16 __fastcall main_read_word(UINT32 a)
{
	return main_read16(a);
}

static void __fastcall main_write_byte(UINT32 a, UINT8 d)
{
	main_write16(a & ~1, (d << 8) | d, !(a & 1), a & 1);
}

static void __fastcall main_write_word(UINT32 a, UINT16 d)
{
	main_write16(a, d, 1, 1);
}

static void sub_run_cycles(INT32 cycles); // forward

// the main cpu runs ahead of the sub cpu: bring the sub cpu up to date before the main cpu
// touches the gate array, otherwise handshakes (word ram swaps, comm flags) can deadlock.
// A write only lands at the end of the main cpu instruction, the sub cpu must not see it
// before (Dune: word ram 1M swap request answered too early, then requested twice)
#define MAIN_WRITE_DELAY	16

static void sync_sub(INT32 main_extra)
{
	if (SegaCDMainCycles == NULL || SekGetActive() != 0) return;

	INT32 target = (INT64)(SegaCDMainCycles() + main_extra) * SCD_CLOCK / main_clock;
	if (target <= sub_cycles_frame) return;

	SekClose();
	SekOpen(1);
	sub_run_cycles(target - SekTotalCycles());
	SekClose();
	SekOpen(0);
}

// main cpu side of the gate array (a12000-a1203f, mirrored up to a120ff)
static UINT16 main_io_read(UINT32 a)
{
	if ((a & 0xffff00) != 0xa12000) return 0xffff;
	sync_sub(0);
	a = 0xa12000 | (a & 0x3e);

	switch (a) {
		case 0xa12000:
			return sub_run | (sub_request << 1) | (((irq_pending >> 2) & 1) << 8) | (((irq_mask >> 2) & 1) << 15);

		case 0xa12002: {
			UINT16 d = (!wram_mode) ? !wram_switch : wram_select;
			d |= ((!wram_mode) ? wram_switch : wram_switch_request) << 1;
			d |= wram_mode << 2;
			d |= pram_bank << 6;
			d |= pram_protect << 8;
			return d;
		}

		case 0xa12004:
			return (cdc.tr_destination << 8) | (cdc.tr_ready << 14) | (cdc.tr_completed << 15);

		case 0xa12006:
			return vector_level4 & 0xffff;

		case 0xa12008:
			return (cdc.tr_destination == 2) ? cdc_transfer_read() : 0;

		case 0xa1200c:
			return cdc.stopwatch & 0xfff;

		case 0xa1200e:
			return (comm_cfm << 8) | comm_cfs;
	}

	if (a >= 0xa12010 && a <= 0xa1201f) return comm_command[(a - 0xa12010) >> 1];
	if (a >= 0xa12020 && a <= 0xa1202f) return comm_status[(a - 0xa12020) >> 1];

	return 0;
}

static void main_io_write(UINT32 a, UINT16 d, INT32 upper, INT32 lower)
{
	if ((a & 0xffff00) != 0xa12000) return;
	sync_sub(MAIN_WRITE_DELAY);
	a = 0xa12000 | (a & 0x3e);

	switch (a) {
		case 0xa12000:
			if (lower) {
				if (sub_run && !(d & 1)) sub_reset_pending = 1;
				sub_run = d & 1;
				sub_request = sub_run ? ((d >> 1) & 1) : 1; // reset forces a bus request
				sub_halt = sub_request;
			}
			if (upper) {
				if (d & 0x100) irq_raise(2);
			}
		return;

		case 0xa12002:
			if (lower) {
				if ((d & 2) == 0) wram_switch_request = 1;
				else wram_switch = 1;
				pram_bank = (d >> 6) & 3;
			}
			if (upper) pram_protect = d >> 8;
		return;

		case 0xa12006:
			if (upper) vector_level4 = (vector_level4 & 0xffff00ff) | (d & 0xff00);
			if (lower) vector_level4 = (vector_level4 & 0xffffff00) | (d & 0x00ff);
		return;

		case 0xa1200e: // 8-bit register, writes go to the high byte
			comm_cfm = d >> 8;
		return;
	}

	if (a >= 0xa12010 && a <= 0xa1201f) {
		UINT16 *p = &comm_command[(a - 0xa12010) >> 1];
		if (lower) *p = (*p & 0xff00) | (d & 0x00ff);
		if (upper) *p = (*p & 0x00ff) | (d & 0xff00);
	}
}

// ram cartridge area (not emulated): writes are ignored, reads stay unmapped
static void __fastcall main_cart_write_byte(UINT32, UINT8) { }
static void __fastcall main_cart_write_word(UINT32, UINT16) { }

static UINT8 __fastcall main_io_read_byte(UINT32 a)
{
	UINT16 d = main_io_read(a & ~1);
	return (a & 1) ? (d & 0xff) : (d >> 8);
}

static UINT16 __fastcall main_io_read_word(UINT32 a)
{
	return main_io_read(a);
}

static void __fastcall main_io_write_byte(UINT32 a, UINT8 d)
{
	main_io_write(a & ~1, (d << 8) | d, !(a & 1), a & 1);
}

static void __fastcall main_io_write_word(UINT32 a, UINT16 d)
{
	main_io_write(a, d, 1, 1);
}

// -------------------------------------------------------------------------
// sub cpu execution

static void peripherals_step(INT32 clocks)
{
	gpu_step(clocks);

	counter_divider += clocks;
	while (counter_divider >= 384) {
		counter_divider -= 384;
		cdc.stopwatch = (cdc.stopwatch + 1) & 0xfff;
		cdd_clock();
		timer_clock();
		pcm_clock();
	}

	counter_dma += clocks;
	if (!cdc.tr_active) {
		counter_dma %= 6;
		return;
	}
	while (counter_dma >= 6) {
		counter_dma -= 6;
		cdc_transfer_dma();
	}
}

static void sub_run_cycles(INT32 cycles)
{
	while (cycles > 0) {
		if (irq_dirty) sub_irq_update();

		INT32 runnable = sub_run && !sub_halt;
		if (runnable && sub_reset_pending) {
			// the reset vectors are fetched once the cpu is released
			SekReset();
			sub_reset_pending = 0;
			sub_wram_wait = 0;
		}

		if (sub_wram_wait == 2 && (wram_mode == 1 || wram_switch == 1)) sub_wram_wait = 0;

		INT32 done;
		if (runnable && !sub_wram_wait) {
			done = SekRun(cycles);
			if (done <= 0) done = cycles;
			if (sub_wram_wait == 1) {
				sub_wram_wait = sub_wram_replay() ? 2 : 0;
			}
		} else {
			SekIdle(cycles);
			done = cycles;
		}

		peripherals_step(done);
		cycles -= done;
	}

	sub_cycles_frame = SekTotalCycles();
}

void SegaCDRunSub(INT32 main_cycles)
{
	INT32 target = (INT64)main_cycles * SCD_CLOCK / main_clock;
	if (target <= sub_cycles_frame) return;

	SekClose();
	SekOpen(1);
	sub_run_cycles(target - SekTotalCycles());
	SekClose();
	SekOpen(0);
}

void SegaCDNewFrame()
{
	SekNewFrame();

	SekOpen(1);
	SekIdle(sub_carry);
	sub_cycles_frame = SekTotalCycles();
	SekClose();
}

void SegaCDEndFrame(INT32 main_cycles)
{
	INT32 target = (INT64)main_cycles * SCD_CLOCK / main_clock;
	sub_carry = sub_cycles_frame - target;
	if (sub_carry < 0) sub_carry = 0;

	// cd-da fader, ramps one step per sample
	INT32 step = 44100 / ((nBurnFPS > 0) ? (nBurnFPS / 100) : 60);
	if (cdd.dac_attenuated < cdd.dac_attenuator) {
		cdd.dac_attenuated = ((cdd.dac_attenuator - cdd.dac_attenuated) > step) ? cdd.dac_attenuated + step : cdd.dac_attenuator;
	} else if (cdd.dac_attenuated > cdd.dac_attenuator) {
		cdd.dac_attenuated = ((cdd.dac_attenuated - cdd.dac_attenuator) > step) ? cdd.dac_attenuated - step : cdd.dac_attenuator;
	}
}

void SegaCDSoundUpdate(INT16 *output, INT32 samples)
{
	if (output == NULL) {
		pcm_fifo_read = pcm_fifo_count = 0;
		return;
	}

	// keep the pcm latency bounded
	INT32 needed = pcm_stream.samples_to_source(samples);
	while (pcm_fifo_count > needed * 2) {
		pcm_fifo_read = (pcm_fifo_read + 1) % PCM_FIFO_SIZE;
		pcm_fifo_count--;
	}

	pcm_stream.render(output, samples);

	CDEmuSetVolume(cdd.dac_attenuated * 50.0 / 0x4000);
	CDEmuGetSoundBuffer(output, samples);
}

void SegaCDSetPAL(INT32 pal)
{
	main_clock = pal ? MAIN_PAL : MAIN_NTSC;
}

// -------------------------------------------------------------------------
// bios / region

static INT32 scd_detect_region()
{
	UINT8 sector[2048];

	if (CDEmuReadDataSector(0, sector)) return 0;
	if (memcmp(sector, "SEGADISCSYSTEM", 14) && memcmp(sector, "SEGABOOTDISC", 12)) return 0;

	// the bios checks the region specific security code, header region field is unreliable (Sol-Feace EU says 'U')
	switch (sector[0x20b]) {
		case 0x64: return 1;
		case 0xa1: return 2;
	}

	return 0;
}

INT32 SegaCDLoadBios(UINT8 *dest)
{
	INT32 select = MegadriveDIP[0] & 0x03;
	INT32 wanted = (select) ? (select - 1) : scd_detect_region();
	INT32 loaded = -1;

	for (INT32 n = 0; n < 4 && loaded < 0; n++) {
		INT32 idx = (n == 0) ? wanted : (n - 1);
		if (n && idx == wanted) continue;

		struct BurnRomInfo ri;
		ri.nLen = 0;
		BurnDrvGetRomInfo(&ri, idx);
		if (ri.nLen != 0x20000) continue;

		if (BurnLoadRom(dest, idx, 1) == 0) loaded = idx;
	}

	if (loaded < 0) {
		bprintf(PRINT_ERROR, _T("Sega CD: no bios could be loaded\n"));
		return 1;
	}

	if (loaded != wanted) {
		bprintf(PRINT_IMPORTANT, _T("Sega CD: wanted bios #%d is unavailable, using bios #%d\n"), wanted, loaded);
	}

	// accept both big-endian and byteswapped dumps
	if (!(dest[0x100] == 'E' && dest[0x101] == 'S')) {
		BurnByteswap(dest, 0x20000);
	}

	scd_bios_index = loaded;

	return 0;
}

UINT8 SegaCDGetHardware()
{
	// version register: region bits, bit 5 clear = expansion unit attached
	static const UINT8 hw[3] = { 0x80, 0xc0, 0x00 };

	return hw[scd_bios_index % 3];
}

// -------------------------------------------------------------------------
// init / reset / exit

static void scd_power(INT32 reset)
{
	sub_run = 0;
	sub_request = 0;
	sub_halt = 1;
	sub_reset_pending = 1;
	sub_wram_wait = 0;

	wram_mode = 0;
	wram_switch_request = 0;
	wram_switch = 0;
	wram_select = 0;
	wram_priority = 0;
	pram_bank = 0;
	pram_protect = 0;
	vector_level4 = reset ? vector_level4 : 0xffffffff;
	led_status = 0;

	comm_cfm = comm_cfs = 0;
	memset(comm_command, 0, sizeof(comm_command));
	memset(comm_status, 0, sizeof(comm_status));

	counter_divider = 0;
	counter_dma = 0;

	irq_mask = reset ? (irq_mask & 0x40) : 0;
	irq_pending = reset ? (irq_pending & 0x40) : 0;
	irq_dirty = 1;

	timer_frequency = 0;
	timer_counter = 0;

	memset(&gpu, 0, sizeof(gpu));

	cdc_power();
	pcm_power();
	cdd_power();

}

static void bram_format()
{
	// blank internal backup ram directory, so the bios doesn't ask to format it
	static const UINT8 format[0x40] = {
		0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x00, 0x00, 0x00, 0x00, 0x40,
		0x00, 0x7d, 0x00, 0x7d, 0x00, 0x7d, 0x00, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x53, 0x45, 0x47, 0x41, 0x5f, 0x43, 0x44, 0x5f, 0x52, 0x4f, 0x4d, 0x00, 0x01, 0x00, 0x00, 0x00,
		0x52, 0x41, 0x4d, 0x5f, 0x43, 0x41, 0x52, 0x54, 0x52, 0x49, 0x44, 0x47, 0x45, 0x5f, 0x5f, 0x5f
	};

	memset(BackupRam, 0, 0x2000);
	memcpy(BackupRam + 0x2000 - 0x40, format, 0x40);
}

INT32 SegaCDInit(UINT8 *bios)
{
	BiosRom = bios;

	INT32 nLen = 0x80000 + 0x40000 + 0x4000 + 0x10000 + 0x2000;
	AllMemScd = (UINT8*)BurnMalloc(nLen);
	if (AllMemScd == NULL) return 1;
	memset(AllMemScd, 0, nLen);

	UINT8 *Next = AllMemScd;
	PrgRam    = Next; Next += 0x80000;
	WordRam   = (UINT16*)Next; Next += 0x40000;
	CdcRam    = (UINT16*)Next; Next += 0x4000;
	PcmRam    = Next; Next += 0x10000;
	BackupRam = Next; Next += 0x2000;

	bram_format();
	toc_build();

	if (!disc_present) {
		bprintf(PRINT_IMPORTANT, _T("Sega CD: no disc\n"));
	}

	// main cpu: bios, prg-ram window and word ram in the 0x000000-0x3fffff expansion area
	SekOpen(0);
	SekMapHandler(5, 0x000000, 0x3fffff, MAP_RAM);
	for (INT32 i = 0; i < 0x200000; i += 0x40000) {
		SekMapMemory(BiosRom, i, i + 0x1ffff, MAP_ROM);
	}
	SekMapHandler(5, 0x000000, 0x0003ff, MAP_READ); // level 4 vector redirection at 0x70
	SekMapHandler(6, 0xa12000, 0xa123ff, MAP_RAM);
	SekMapHandler(7, 0x400000, 0x7fffff, MAP_WRITE);

	SekSetReadByteHandler (5, main_read_byte);
	SekSetReadWordHandler (5, main_read_word);
	SekSetWriteByteHandler(5, main_write_byte);
	SekSetWriteWordHandler(5, main_write_word);

	SekSetReadByteHandler (6, main_io_read_byte);
	SekSetReadWordHandler (6, main_io_read_word);
	SekSetWriteByteHandler(6, main_io_write_byte);
	SekSetWriteWordHandler(6, main_io_write_word);

	SekSetWriteByteHandler(7, main_cart_write_byte);
	SekSetWriteWordHandler(7, main_cart_write_word);
	SekClose();

	// sub cpu
	SekInit(1, 0x68000);
	SekOpen(1);
	SekMapMemory(PrgRam, 0x000000, 0x07ffff, MAP_ROM); // writes go through the handler (write protection)
	SekSetReadByteHandler (0, sub_read_byte);
	SekSetReadWordHandler (0, sub_read_word);
	SekSetWriteByteHandler(0, sub_write_byte);
	SekSetWriteWordHandler(0, sub_write_word);
	SekSetIrqCallback(sub_irq_callback);
	SekClose();

	pcm_stream.init(SCD_PCM_RATE, nBurnSoundRate, 2, 1, pcm_stream_update);
	pcm_stream.set_volume(1.00);

	main_clock = MAIN_NTSC;
	sub_carry = 0;
	sub_cycles_frame = 0;

	scd_power(0);

	return 0;
}

void SegaCDReset()
{
	scd_power(1);

	SekOpen(1);
	SekReset();
	sub_irq_update();
	SekClose();

	sub_carry = 0;
	sub_cycles_frame = 0;
}

void SegaCDExit()
{
	if (cdd.audio_playing) CDEmuStop();

	pcm_stream.exit();

	BurnFree(AllMemScd);
	AllMemScd = NULL;

	disc_present = 0;
	scd_bios_index = 0;
	SegaCDMode = 0;
}

INT32 SegaCDScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_VOLATILE) {
		ScanVar(PrgRam, 0x80000, "SCD PRG RAM");
		ScanVar(WordRam, 0x40000, "SCD Word RAM");
		ScanVar(CdcRam, 0x4000, "SCD CDC RAM");
		ScanVar(PcmRam, 0x10000, "SCD PCM RAM");

		SCAN_VAR(sub_run);
		SCAN_VAR(sub_request);
		SCAN_VAR(sub_halt);
		SCAN_VAR(sub_reset_pending);
		SCAN_VAR(sub_wram_wait);
		SCAN_VAR(sub_carry);
		SCAN_VAR(sub_cycles_frame);
		SCAN_VAR(wram_mode);
		SCAN_VAR(wram_switch_request);
		SCAN_VAR(wram_switch);
		SCAN_VAR(wram_select);
		SCAN_VAR(wram_priority);
		SCAN_VAR(pram_bank);
		SCAN_VAR(pram_protect);
		SCAN_VAR(vector_level4);
		SCAN_VAR(led_status);
		SCAN_VAR(comm_cfm);
		SCAN_VAR(comm_cfs);
		SCAN_VAR(comm_command);
		SCAN_VAR(comm_status);
		SCAN_VAR(counter_divider);
		SCAN_VAR(counter_dma);
		SCAN_VAR(irq_mask);
		SCAN_VAR(irq_pending);
		SCAN_VAR(timer_frequency);
		SCAN_VAR(timer_counter);
		SCAN_VAR(cdc);
		SCAN_VAR(cdd);
		SCAN_VAR(gpu);
		SCAN_VAR(pcm_ch);
		SCAN_VAR(pcm_enable);
		SCAN_VAR(pcm_bank);
		SCAN_VAR(pcm_channel);

		CDEmuScan(nAction, pnMin);

		if (nAction & ACB_WRITE) {
			pcm_fifo_read = pcm_fifo_count = 0;
			irq_dirty = 1;
		}
	}

	if (nAction & ACB_NVRAM) {
		struct BurnArea ba;
		memset(&ba, 0, sizeof(ba));
		ba.Data		= BackupRam;
		ba.nLen		= 0x2000;
		ba.nAddress	= 0;
		ba.szName	= "SCD Backup RAM";
		BurnAcb(&ba);
	}

	return 0;
}
