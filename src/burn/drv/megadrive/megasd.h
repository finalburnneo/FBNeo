// MegaSD "MD+" cd overlay: a patched cartridge game plays cd audio (and reads data sectors) from a cd image
// Based on the MegaSD Dev Manual rev.2 (Terraonion)
// The cd image is the driver's hdd (BurnHDDInfo), a cd chd in <hdd path>/<romset>/

#include "cd_interface.h"
#include "burn_hdd.h"

#define MEGASD_CHUNK	32		// samples rendered between playback checks (loop accuracy)

struct megasd_state {
	UINT8 overlay;
	UINT8 playing;				// 0: stopped, 1: playing, 2: paused
	UINT8 loop;
	UINT8 volume;
	UINT16 result;
	INT32 track;
	INT32 end_track;
	INT32 end_sector;			// absolute sectors (lba + 150)
	INT32 loop_track;
	INT32 loop_sector;
	INT32 fade_total;
	INT32 fade_count;			// in samples
	INT32 data_lba;
	UINT8 buffer[0x800];
};

static INT32 megasdmode = 0;
static megasd_state megasd;
static TCHAR megasd_prev_image[MAX_PATH];

// toc in absolute sectors, rebuilt from the image
static INT32 megasd_first, megasd_last;
static INT32 megasd_start[100], megasd_end[100];
static UINT8 megasd_audio[100];

static inline INT32 megasd_bcd(UINT8 v) { return (v >> 4) * 10 + (v & 0x0f); }
static inline UINT8 megasd_tobcd(INT32 v) { return ((v / 10) << 4) | (v % 10); }
static inline INT32 megasd_msf(const UINT8 *p) { return (megasd_bcd(p[0]) * 60 + megasd_bcd(p[1])) * 75 + megasd_bcd(p[2]); }
static inline UINT32 megasd_be32(INT32 offs) { return (megasd.buffer[offs] << 24) | (megasd.buffer[offs + 1] << 16) | (megasd.buffer[offs + 2] << 8) | megasd.buffer[offs + 3]; }

static void megasd_toc()
{
	megasd_first = megasd_last = 0;
	memset(megasd_start, 0, sizeof(megasd_start));
	memset(megasd_end, 0, sizeof(megasd_end));
	memset(megasd_audio, 0, sizeof(megasd_audio));

	UINT8 *toc = CDEmuReadTOC(CDEmuTOC_FIRSTLAST);
	if (toc == NULL) return;

	INT32 first = megasd_bcd(toc[0]);
	INT32 last = megasd_bcd(toc[1]);
	if (first < 1 || last < first || last > 99) return;

	for (INT32 t = first; t <= last; t++) {
		toc = CDEmuReadTOC(megasd_tobcd(t));
		if (toc == NULL) return;
		megasd_start[t] = megasd_msf(toc);
		megasd_audio[t] = (toc[3] & 0x04) ? 0 : 1;
	}

	toc = CDEmuReadTOC(CDEmuTOC_LASTMSF);
	if (toc == NULL) return;
	megasd_end[last] = megasd_msf(toc);

	for (INT32 t = first; t < last; t++) {
		megasd_end[t] = megasd_start[t + 1];
		toc = CDEmuReadTOC(CDEmuTOC_TRACKEND | megasd_tobcd(t));
		if (toc) {
			// the next track's pregap isn't played
			INT32 end = megasd_msf(toc);
			if (end > megasd_start[t] && end < megasd_end[t]) megasd_end[t] = end;
		}
	}

	megasd_first = first;
	megasd_last = last;
}

static inline INT32 megasd_audio_track(INT32 t)
{
	return megasd_first && t >= megasd_first && t <= megasd_last && megasd_audio[t];
}

static INT32 megasd_find_track(INT32 sector)
{
	for (INT32 t = megasd_first; megasd_first && t <= megasd_last; t++) {
		if (sector < megasd_end[t]) return t;
	}
	return 0;
}

static void megasd_play(INT32 track, INT32 sector)
{
	CDEmuPlay(megasd_tobcd(sector / (75 * 60)), megasd_tobcd((sector / 75) % 60), megasd_tobcd(sector % 75));
	megasd.track = track;
	megasd.playing = 1;
	megasd.fade_count = 0;
}

static void megasd_stop()
{
	if (megasd.playing) CDEmuStop();
	megasd.playing = 0;
	megasd.fade_count = 0;
}

// end of the current track, or of the requested range
static void megasd_check_end()
{
	INT32 end = (megasd.track == megasd.end_track) ? megasd.end_sector : megasd_end[megasd.track];

	if (CDEmuGetStatus() == playing && CDEmuGetCurrentLBA() < end) return;

	if (megasd.track < megasd.end_track && megasd_audio_track(megasd.track + 1)) {
		megasd_play(megasd.track + 1, megasd_start[megasd.track + 1]);
	} else if (megasd.loop) {
		megasd_play(megasd.loop_track, megasd.loop_sector);
	} else {
		megasd_stop();
	}
}

static void megasd_command(UINT16 data)
{
	INT32 param = data & 0xff;

	switch (data >> 8) {
		case 0x10: { // version and serial number
			static const UINT8 version[16] = { 'M','E','G','A','S','D', 0x01, 0x04, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 };
			memcpy(megasd.buffer, version, sizeof(version));
		}
		break;

		case 0x11: // play track
		case 0x12: // play track, loop
		case 0x1a: // play track, loop from an offset
			if (megasd_audio_track(param)) {
				megasd.end_track = param;
				megasd.end_sector = megasd_end[param];
				megasd.loop = (data >> 8) != 0x11;
				megasd.loop_track = param;
				megasd.loop_sector = megasd_start[param] + (((data >> 8) == 0x1a) ? megasd_be32(0) : 0);
				if (megasd.loop_sector < megasd_start[param] || megasd.loop_sector >= megasd_end[param]) megasd.loop_sector = megasd_start[param];
				megasd_play(param, megasd_start[param]);
			}
		break;

		case 0x13: // pause, with an optional fade out in sectors
			if (megasd.playing == 1) {
				if (param) {
					megasd.fade_total = megasd.fade_count = param * 588;
				} else {
					CDEmuPause();
					megasd.playing = 2;
				}
			}
		break;

		case 0x14: // resume
			if (megasd.playing == 2) {
				CDEmuResume();
				megasd.playing = 1;
			}
		break;

		case 0x15: // volume
			megasd.volume = param;
		break;

		case 0x16: // playback status
			megasd.result = (megasd.playing == 1) ? 1 : 0;
		break;

		case 0x17: // seek a data sector
			megasd_stop();
			megasd.data_lba = megasd_be32(0) - 150;
		break;

		case 0x18: // transfer the sector
			CDEmuReadDataSector(megasd.data_lba, megasd.buffer);
		break;

		case 0x19: // next data sector
			megasd.data_lba++;
		break;

		case 0x1b: { // play a range of sectors
			INT32 start = megasd_be32(0);
			INT32 t = megasd_find_track(start);
			if (!megasd_audio_track(t)) break;
			if (start < megasd_start[t]) start = megasd_start[t];

			INT32 end = megasd_be32(4);
			if (end > start) {
				megasd.end_track = megasd_find_track(end - 1);
				if (megasd.end_track == 0) {
					megasd.end_track = megasd_last;
					end = megasd_end[megasd_last];
				}
			} else {
				megasd.end_track = t;
				end = megasd_end[t];
			}
			megasd.end_sector = end;

			megasd.loop = param & 1;
			megasd.loop_track = t;
			megasd.loop_sector = megasd.loop ? megasd_be32(8) : megasd_start[t];
			if (megasd.loop_sector < megasd_start[t] || megasd.loop_sector >= megasd_end[t] || megasd.loop_sector >= end) megasd.loop_sector = megasd_start[t];

			megasd_play(t, start);
		}
		break;

		case 0x1c: case 0x1d: case 0x1e: case 0x1f: case 0x20: case 0x21:
			// file access on the sd card, not supported
			megasd.result = 0;
		break;
	}
}

static void megasd_map()
{
	if (megasd.overlay) {
		SekMapHandler(2, 0x03f400, 0x03ffff, MAP_READ);
	} else {
		SekMapMemory(RomMain + 0x03f400, 0x03f400, 0x03ffff, MAP_READ);
	}
}

static UINT16 __fastcall megasd_read_word(UINT32 address)
{
	address &= 0xfffffe;

	switch (address) {
		case 0x03f7f6: return 0x4241; // id
		case 0x03f7f8: return 0x5445;
		case 0x03f7fa: return 0xcd54; // overlay
		case 0x03f7fc: return megasd.result;
		case 0x03f7fe: return 0; // command done
	}

	if (address >= 0x03f800) {
		return (megasd.buffer[address & 0x7fe] << 8) | megasd.buffer[(address & 0x7fe) + 1];
	}

	return *((UINT16*)(RomMain + address));
}

static UINT8 __fastcall megasd_read_byte(UINT32 address)
{
	UINT16 data = megasd_read_word(address);
	return (address & 1) ? (data & 0xff) : (data >> 8);
}

static void __fastcall megasd_write_word(UINT32 address, UINT16 data)
{
	address &= 0xfffffe;

	// word writes only
	if (address == 0x03f7fa) {
		UINT8 overlay = (data == 0xcd54);
		if (overlay != megasd.overlay) {
			megasd.overlay = overlay;
			megasd_map();
		}
		return;
	}

	if (!megasd.overlay) return;

	if (address == 0x03f7fe) {
		megasd_command(data);
		return;
	}

	if (address >= 0x03f800) {
		megasd.buffer[address & 0x7fe] = data >> 8;
		megasd.buffer[(address & 0x7fe) + 1] = data & 0xff;
	}
}

static void __fastcall megasd_write_byte(UINT32 address, UINT8 data)
{
	if (megasd.overlay && address >= 0x03f800 && address <= 0x03ffff) {
		megasd.buffer[address & 0x7ff] = data;
	}
}

static void megasd_reset()
{
	megasd_stop();
	memset(&megasd, 0, sizeof(megasd));
	megasd.volume = 0xff;

	SekOpen(0);
	megasd_map();
	SekClose();
}

// md+ games are the cartridge drivers with a cd image as hdd
static INT32 megasd_init()
{
	// BurnDrvGetHDDName() succeeds without setting a name for drivers without hdd
	char *name = NULL;
	if (BurnDrvGetHDDName(&name, 0, 0) || name == NULL) return 0;

	TCHAR path[MAX_PATH];
	if (BurnHDDGetPath(0, _T(".chd"), path)) {
		bprintf(PRINT_ERROR, _T("MegaSD: cd image not found\n"));
		return 1;
	}

	_tcscpy(megasd_prev_image, CDEmuImage);
	_tcscpy(CDEmuImage, path);
	if (CDEmuInit()) {
		bprintf(PRINT_ERROR, _T("MegaSD: can't open %s\n"), path);
		_tcscpy(CDEmuImage, megasd_prev_image);
		return 1;
	}

	megasd_toc();
	megasdmode = 1;

	SekOpen(0);
	SekMapHandler(2, 0x03f400, 0x03ffff, MAP_WRITE);
	SekSetReadByteHandler (2, megasd_read_byte);
	SekSetReadWordHandler (2, megasd_read_word);
	SekSetWriteByteHandler(2, megasd_write_byte);
	SekSetWriteWordHandler(2, megasd_write_word);
	SekClose();

	bprintf(PRINT_IMPORTANT, _T("MegaSD: MD+ cd audio overlay, %d track(s)\n"), megasd_first ? (megasd_last - megasd_first + 1) : 0);

	return 0;
}

static void megasd_exit()
{
	if (!megasdmode) return;

	megasd_stop();
	CDEmuExit();
	_tcscpy(CDEmuImage, megasd_prev_image);

	megasdmode = 0;
}

static void megasd_sound_update(INT16 *output, INT32 samples)
{
	INT16 scratch[MEGASD_CHUNK * 2];

	// playback goes on without sound output, games poll the playback status
	for (INT32 pos = 0; pos < samples; pos += MEGASD_CHUNK) {
		INT32 count = ((samples - pos) < MEGASD_CHUNK) ? (samples - pos) : MEGASD_CHUNK;
		if (megasd.playing != 1) continue;

		INT32 volume = megasd.volume;
		if (megasd.fade_count > 0) volume = volume * megasd.fade_count / megasd.fade_total;
		CDEmuSetVolume(volume * 50.0 / 255);

		if (output) {
			CDEmuGetSoundBuffer(output + pos * 2, count);
		} else {
			memset(scratch, 0, sizeof(scratch));
			CDEmuGetSoundBuffer(scratch, count);
		}

		if (megasd.fade_count > 0) {
			megasd.fade_count -= count;
			if (megasd.fade_count <= 0) {
				megasd.fade_count = 0;
				CDEmuPause();
				megasd.playing = 2;
				continue;
			}
		}

		megasd_check_end();
	}
}

static void megasd_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_VOLATILE) {
		SCAN_VAR(megasd);
		CDEmuScan(nAction, pnMin);

		if (nAction & ACB_WRITE) {
			SekOpen(0);
			megasd_map();
			SekClose();
		}
	}
}
