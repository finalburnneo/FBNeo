// Sega digital sound board, as on Model 1 and Model 2: a z80 drives the mpeg decoder through i/o ports

#include "burnint.h"
#include "z80_intf.h"
#include "model1_snd.h"
#include "mpeg_audio.h"
#include "sega_dsb.h"

#define DSB_RATE	32000

static mpeg_audio *dsb_decoder = NULL;
static UINT32 dsb_mp_start, dsb_mp_end, dsb_lp_start, dsb_lp_end, dsb_start, dsb_end;
static INT32 dsb_mp_pos, dsb_mp_vol, dsb_mp_pan, dsb_mp_state;
static INT32 dsb_audio_pos, dsb_audio_avail;
static INT16 dsb_audio_buf[1152 * 2];
static INT32 dsb_frac;
static INT16 dsb_last[2];	// the previous frame's last sample, for the interpolation
static INT16 *dsb_stream = NULL;

void DsbIRQUpdate()
{
	ZetSetIRQLine(0, M1UsartRxRdy(M1_USART_DSB) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void __fastcall dsb_write_port(UINT16 port, UINT8 data)
{
	port &= 0xff;

	switch (port) {
		case 0xe0:
			dsb_mp_state = data;
			if (data == 0) {
				dsb_audio_pos = dsb_audio_avail = 0;
			} else if (data == 1 || data == 2) {
				dsb_mp_pos = dsb_mp_start * 8;
			}
			return;

		// writes while playing are latched, used when the current stream ends
		case 0xe2: case 0xe3: case 0xe4: {
			INT32 shift = (0xe4 - port) * 8;
			dsb_start = (dsb_start & ~(0xff << shift)) | (data << shift);
			if (port == 0xe4) {
				if (dsb_mp_state == 0) dsb_mp_start = dsb_start;
				else dsb_lp_start = dsb_start;
			}
			return;
		}

		case 0xe5: case 0xe6: case 0xe7: {
			INT32 shift = (0xe7 - port) * 8;
			dsb_end = (dsb_end & ~(0xff << shift)) | (data << shift);
			if (port == 0xe7) {
				if (dsb_mp_state == 0) dsb_mp_end = dsb_end;
				else dsb_lp_end = dsb_end;
			}
			return;
		}

		case 0xe8:
			dsb_mp_vol = ~data & 0x7f;
			return;

		case 0xe9:
			dsb_mp_pan = data & 3;
			return;

		case 0xf0:
			M1UsartDataWrite(M1_USART_DSB, data);
			return;

		case 0xf1:
			M1UsartControlWrite(M1_USART_DSB, data);
			return;
	}
}

static UINT8 __fastcall dsb_read_port(UINT16 port)
{
	port &= 0xff;

	switch (port) {
		case 0xe2: case 0xe3: case 0xe4:
			return ((dsb_mp_pos >> 3) >> ((0xe4 - port) * 8)) & 0xff;

		case 0xf0:
		case 0xf1: {
			UINT8 r = M1UsartRead(M1_USART_DSB, port & 1);
			DsbIRQUpdate();
			return r;
		}
	}

	return 0;
}

static void dsb_stream_update(INT16 *out, INT32 samples)
{
	while (samples) {
		while (samples && dsb_audio_pos < dsb_audio_avail) {
			INT32 l = dsb_audio_buf[dsb_audio_pos * 2 + 0];
			INT32 r = dsb_audio_buf[dsb_audio_pos * 2 + 1];
			if (dsb_mp_pan == 1) r = l;
			if (dsb_mp_pan == 2) l = r;
			if (dsb_mp_pan == 3) l = r = 0;
			out[0] = (l * dsb_mp_vol) >> 7;
			out[1] = (r * dsb_mp_vol) >> 7;
			out += 2;
			dsb_audio_pos++;
			samples--;
		}

		if (!samples) break;

		if (dsb_mp_state == 0) {
			memset(out, 0, samples * 2 * sizeof(INT16));
			break;
		}

		INT32 sample_rate, channel_count;
		if (dsb_decoder->decode_buffer(dsb_mp_pos, dsb_mp_end * 8, dsb_audio_buf, dsb_audio_avail, sample_rate, channel_count)) {
			dsb_audio_pos = 0;
		} else {
			if (dsb_mp_state == 2) {
				// looping on undecodable data, give up
				if (dsb_mp_pos == (INT32)(dsb_lp_start * 8)) dsb_mp_state = 0;
				dsb_mp_pos = dsb_lp_start * 8;
				if (dsb_lp_end) dsb_mp_end = dsb_lp_end;
			} else {
				dsb_mp_state = 0;
			}
		}
	}
}

void DsbRender(INT16 *out, INT32 len)
{
	INT32 total = dsb_frac + DSB_RATE * 100;
	INT32 count = total / nBurnFPS;
	dsb_frac = total % nBurnFPS;

	dsb_stream_update(dsb_stream, count);

	if (count == 0) return;

	// 32KHz to the output rate, linear interpolation: output sample i is at (i + 1) * count / len - 1
	// in the stream, the previous frame's last sample standing at -1
	for (INT32 i = 0; out && i < len; i++) {
		INT64 t = ((INT64)(i + 1) * count << 16) / len - 0x10000;
		INT32 k = (INT32)(t >> 16);
		INT32 f = (INT32)(t & 0xffff);
		for (INT32 c = 0; c < 2; c++) {
			INT32 a = (k < 0) ? dsb_last[c] : dsb_stream[k * 2 + c];
			INT32 b = (k + 1 < count) ? dsb_stream[(k + 1) * 2 + c] : a;
			out[i * 2 + c] = BURN_SND_CLIP(out[i * 2 + c] + a + (((b - a) * f) >> 16));
		}
	}

	dsb_last[0] = dsb_stream[(count - 1) * 2 + 0];
	dsb_last[1] = dsb_stream[(count - 1) * 2 + 1];
}

void DsbInit(INT32 nCpu, UINT8 *rom, UINT8 *ram, UINT8 *mpeg)
{
	ZetInit(nCpu);
	ZetOpen(nCpu);
	ZetMapMemory(rom,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(ram,	0x8000, 0xffff, MAP_RAM);
	ZetSetOutHandler(dsb_write_port);
	ZetSetInHandler(dsb_read_port);
	ZetClose();

	dsb_decoder = new mpeg_audio(mpeg, mpeg_audio::L2, false, 0);
	dsb_stream = (INT16*)BurnMalloc(2048 * 2 * sizeof(INT16));
}

void DsbExit()
{
	delete dsb_decoder;
	dsb_decoder = NULL;
	BurnFree(dsb_stream);
}

void DsbReset()
{
	dsb_decoder->clear();
	dsb_mp_start = dsb_mp_end = dsb_lp_start = dsb_lp_end = dsb_start = dsb_end = 0;
	dsb_mp_pos = 0;
	dsb_mp_vol = 0x7f;
	dsb_mp_pan = 0;
	dsb_mp_state = 0;
	dsb_audio_pos = dsb_audio_avail = 0;
	memset(dsb_audio_buf, 0, sizeof(dsb_audio_buf));
	dsb_frac = 0;
	dsb_last[0] = dsb_last[1] = 0;
}

void DsbScan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		dsb_decoder->scan();
		SCAN_VAR(dsb_mp_start);
		SCAN_VAR(dsb_mp_end);
		SCAN_VAR(dsb_lp_start);
		SCAN_VAR(dsb_lp_end);
		SCAN_VAR(dsb_start);
		SCAN_VAR(dsb_end);
		SCAN_VAR(dsb_mp_pos);
		SCAN_VAR(dsb_mp_vol);
		SCAN_VAR(dsb_mp_pan);
		SCAN_VAR(dsb_mp_state);
		SCAN_VAR(dsb_audio_pos);
		SCAN_VAR(dsb_audio_avail);
		SCAN_VAR(dsb_audio_buf);
		SCAN_VAR(dsb_frac);
		SCAN_VAR(dsb_last);
	}
}
