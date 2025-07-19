/*
Project Little Man
*/

// ported to fbneo by dink, 2025
// BIG thanks to the R.E. of paprium & author of paprium.h
//
// -dink- notes
// To get around stalls while decoding mp3's,
// I've switched to the FBNeo sample player
// w/uncompressed audio samples (zipped).
//

#include "samples.h"

#define cart_rom RomMain
#define sram_sram SRam

#define DEBUG_CHEAT 0

static INT16 *samples_buffer = NULL;
static UINT8 *paprium_obj_ram = NULL;

static int paprium_tmss;

static int paprium_sfx_ptr;
static int paprium_tile_ptr;
static int paprium_sprite_ptr;

typedef struct paprium_voice_t
{
	int volume;
	int panning;
	int flags;
	int type;

	int size;
	int ptr;
	int tick;

	int loop;
	int echo;

	int count;

	int time;

	int start;
	int num;

	int decay;
} paprium_voice_t;


struct paprium_t
{
	UINT8 ram[0x10000];
	UINT8 decoder_ram[0x10000];
	UINT8 scaler_ram[0x1000];
	UINT8 exps_ram[14*8];

	paprium_voice_t sfx[8];

	int audio_flags, sfx_volume, music_volume;

	int decoder_mode, decoder_ptr, decoder_size;
	int draw_src, draw_dst;
	int obj[0x31];

	int echo_l[48000/4], echo_r[48000/4];
	int echo_ptr, echo_pan;
} paprium_s;

static void paprium_music_synth_dink(int pos, int *out_l, int *out_r)
{
	int l = 0, r = 0;

	l = samples_buffer[pos*2 + 0];
	r = samples_buffer[pos*2 + 1];

	l = (l * paprium_s.music_volume) / 256;
	r = (r * paprium_s.music_volume) / 256;

	*out_l = l;
	*out_r = r;
}

static void paprium_sfx_voice(int *out_l, int *out_r)
{
	int l = 0, r = 0;

	int rateScale = ((UINT32)48000 << 16) / nBurnSoundRate;

	for (int ch = 0; ch < 8; ch++) {
		paprium_voice_t *voice = paprium_s.sfx + ch;
		const int _rates[] = { 1, 2, 4, 5, 8, 9, 1, 1 };  /* 48000, 24000, 12000, 9600, 6000, 5333, invalid, invalid */
		int rate = _rates[(voice->type >> 4) & 7] << 16;  /* 16.16 */
		int depth = voice->type & 0x03;
		/* voice->type & 0xC0; */

		int vol = voice->volume;
		int pan = voice->panning;

		int sample, sample_l, sample_r;

		if ( voice->size == 0 ) goto next;

		sample = *(UINT8 *)(cart_rom + paprium_sfx_ptr + (voice->ptr^1));

		switch (depth) {
			case 1:
				sample = (((sample & 0xff) * 65536) / 256) - 32768;
				break;
			case 2:
				sample = ((((sample >> 4 * (~voice->count & 1)) & 0x0f) * 65536) / 16) - 32768;
				break;
		}

		sample = (sample * vol) / 0x400;

		sample_l = (sample * ((pan <= 0x80) ? 0x80 : 0x100 - pan)) / 0x80;
		sample_r = (sample * ((pan >= 0x80) ? 0x80 : pan)) / 0x80;

		l += sample_l;
		r += sample_r;

		if ( voice->flags & 0x4000 ) {  /* echo */
			if ( voice->echo & 1 )
				paprium_s.echo_l[paprium_s.echo_ptr % (48000/6)] += (sample_l * 33) / 100;
			else
				paprium_s.echo_r[paprium_s.echo_ptr % (48000/6)] += (sample_r * 33) / 100;
		}

		if ( voice->flags & 0x100 ) {  /* amplify */
			l = (l * 125) / 100;
			r = (r * 125) / 100;
		}

		voice->time++;

		voice->tick += rateScale; //0x10000;
		voice->tick -= (voice->flags & 0x8000) ? 0x800 : 0;  /* tiny pitch */
		voice->tick -= (voice->flags & 0x2000) ? 0x8000 : 0;  /* huge pitch */

		if ( voice->tick >= rate ) {
			voice->tick -= rate;

			voice->count++;
			voice->size--;

			if ( voice->count >= depth ) {
				voice->ptr++;

				voice->count = 0;
			}
		}

next:
		if ( voice->size == 0 ) {
			voice->count = 0;

			if ( voice->loop ) {
				UINT8 *sfx = paprium_sfx_ptr + cart_rom;

				voice->ptr = (*(UINT16 *)(sfx + voice->num*8) << 16) | (*(UINT16 *)(sfx + voice->num*8 + 2));
				voice->size = (*(UINT8 *)(sfx + voice->num*8 + 4) << 16) | (*(UINT16 *)(sfx + voice->num*8 + 6));
			}
		}
	}

	*out_l += l;
	*out_r += r;
}

static void paprium_audio(INT16 *outbuf, int samples)
{
	BurnSampleRender(samples_buffer, samples);

	for (int lcv = 0; lcv < samples; lcv++) {
		int l = 0, r = 0;

		paprium_s.echo_l[paprium_s.echo_ptr] = 0;
		paprium_s.echo_r[paprium_s.echo_ptr] = 0;

		paprium_music_synth_dink(lcv, &l, &r);
		paprium_sfx_voice(&l, &r);

		paprium_s.echo_ptr = (paprium_s.echo_ptr+1) % (48000/6);

		l += paprium_s.echo_l[paprium_s.echo_ptr];
		r += paprium_s.echo_r[paprium_s.echo_ptr];

		l = (l * paprium_s.sfx_volume) / 0x100;
		r = (r * paprium_s.sfx_volume) / 0x100;

		if ( paprium_s.audio_flags & 0x08 ) {  /* gain */
			l = l * 4 / 2;
			r = r * 4 / 2;
		}

		l = BURN_SND_CLIP(l);
		r = BURN_SND_CLIP(r);

		outbuf[0] = BURN_SND_CLIP(outbuf[0] + l);
		outbuf[1] = BURN_SND_CLIP(outbuf[1] + r);
		outbuf += 2;
	}
}

static void paprium_decoder_lz_rle(UINT32 src, UINT8 *dst)
{
	int size = 0;
	int len = 0, lz = 0, rle = 0, code = 0;

	while (1) {
		int type = cart_rom[(src++)^1];

		code = type >> 6;
		len = type & 0x3F;

		if ( (code == 0) && (len == 0))
			break;

		else if ( code == 1 )
			rle = cart_rom[(src++)^1];

		else if ( code == 2 )
			lz = size - cart_rom[(src++)^1];


		while ( len-- > 0 ) {
			switch(code) {
				case 0: dst[(size++)^1] = cart_rom[(src++)^1]; break;
				case 1: dst[(size++)^1] = rle; break;
				case 2: dst[(size++)^1] = dst[(lz++)^1]; break;
				case 3: dst[(size++)^1] = 0; break;
			}
		}
	}

	paprium_s.decoder_size = size;
}

static void paprium_decoder_lzo(UINT32 src, UINT8 *dst)
{
	int size = 0;
	int len = 0, lz = 0, raw = 0;
	int state = 0;

	while (1) {
		int code = cart_rom[(src++)^1];

		if ( code & 0x80 ) goto code_80;
		if ( code & 0x40 ) goto code_40;
		if ( code & 0x20 ) goto code_20;
		if ( code & 0x10 ) goto code_10;

//code_00:
		if ( state == 0 ) {
			raw = code & 0x0F;

			if ( raw == 0 ) {
				int extra = 0;

				while (1) {
					raw = cart_rom[(src++)^1];
					if ( raw ) break;

					extra += 255;
				}

				raw += extra;
				raw += 15;
			}
			raw += 3;

			len = 0;
			state = 4;
			goto copy_loop;
		}

		else if ( state < 4 ) {
			raw = code & 0x03;
			lz = (code >> 2) & 0x03;

			lz += cart_rom[(src++)^1] << 2;
			lz += 1;

			len = 2;
			goto copy_loop;
		}

		else {
			raw = code & 0x03;
			lz = (code >> 2) & 0x03;

			lz += cart_rom[(src++)^1] << 2;
			lz += 2049;

			len = 3;
			goto copy_loop;
		}

code_10:
		len = code & 0x07;

		if ( len == 0 ) {
			int extra = 0;

			while (1) {
				len = cart_rom[(src++)^1];
				if ( len ) break;

				extra += 255;
			}

			len += extra;
			len += 7;
		}
		len += 2;

		lz = ((code >> 3) & 1) << 14;

		code = cart_rom[(src++)^1];
		raw = code & 0x03;
		lz += code >> 2;

		lz += cart_rom[(src++)^1] << 6;
		lz += 16384;

		if ( lz == 16384 ) break;
		goto copy_loop;

code_20:
		len = code & 0x1F;

		if ( len == 0 ) {
			int extra = 0;

			while (1) {
				len = cart_rom[(src++)^1];
				if ( len ) break;

				extra += 255;
			}

			len += extra;
			len += 31;
		}
		len += 2;

		code = cart_rom[(src++)^1];
		raw = code & 0x03;

		lz = code >> 2;
		lz += cart_rom[(src++)^1] << 6;
		lz += 1;

		goto copy_loop;

code_40:
		raw = code & 0x03;
		len = ((code >> 5) & 1) + 3;
		lz = (code >> 2) & 0x07;

		lz += cart_rom[(src++)^1] << 3;
		lz += 1;

		goto copy_loop;

code_80:
		raw = code & 0x03;
		len = ((code >> 5) & 0x03) + 5;
		lz = (code >> 2) & 0x07;

		lz += cart_rom[(src++)^1] << 3;
		lz += 1;

copy_loop:
		if ( len > 0 )
			state = raw;
		else
			state = 4;

		lz = size - lz;

		while (1) {
			if ( len > 0 ) {
				dst[(size++)^1] = dst[(lz++)^1];
				len--;
			}

			else if ( raw > 0 ) {
				dst[(size++)^1] = cart_rom[(src++)^1];
				raw--;
			}

			else
				break;
		}
	}

	paprium_s.decoder_size = size;
}


static void paprium_decoder_type(int src, UINT8 *dst)
{
	int type = cart_rom[(src++)^1];

	switch (type) {
		case 0x80:
			paprium_decoder_lz_rle(src, dst);
			break;
		case 0x81:
			paprium_decoder_lzo(src, dst);
			break;
	}
}

static void paprium_decoder(UINT8 mode)
{
	int offset = *(UINT16 *)(paprium_s.ram + 0x1E10);
	int ptr = (*(UINT16 *)(paprium_s.ram + 0x1E12) << 16) + *(UINT16 *)(paprium_s.ram + 0x1E14);

	paprium_decoder_type(ptr, paprium_s.decoder_ram + offset);

	paprium_s.decoder_mode = mode;
	paprium_s.decoder_ptr = offset;
}


static void paprium_decoder_copy(UINT8 arg)
{
	int offset = *(UINT16 *)(paprium_s.ram + 0x1E12);
	int size = *(UINT16 *)(paprium_s.ram + 0x1E14);

	paprium_s.decoder_ptr = offset;
	paprium_s.decoder_size = size;
}

static void paprium_sprite(UINT8 index)
{
	int lcv, spr_x, spr_y, count;
	int spritePtr;

	int dmaPtr = *(UINT16*) (paprium_s.ram + 0x1F16);

	int spriteCount = *(UINT16*) (paprium_s.ram + 0x1F18);

	int anim = *(UINT16*) (paprium_s.ram + 0xF80 + index*16);
	int nextAnim = *(UINT16*) (paprium_s.ram + 0xF82 + index*16);
	int obj = *(UINT16*) (paprium_s.ram + 0xF84 + index*16) & 0x7FFF;
	int objAttr = *(UINT16*) (paprium_s.ram + 0xF88 + index*16);
	int reset = *(UINT16*) (paprium_s.ram + 0xF8A + index*16);
	int pos_x = *(UINT16*) (paprium_s.ram + 0xF8C + index*16);
	int pos_y = *(UINT16*) (paprium_s.ram + 0xF8E + index*16);

	int src = paprium_s.draw_src;
	int vram = paprium_s.draw_dst;
	int flip_h = objAttr & 0x800;
	int animFlags;


	int animPtr = *(UINT32*) (paprium_obj_ram + (obj+1)*4);
	int framePtr = paprium_s.obj[index];

	if ( index != 0x30 ) {
		//return;
	}

reload:
	if ( reset == 1 ) {
		framePtr = *(UINT32*) (paprium_obj_ram + (animPtr + anim*4));

		*(UINT16*) (paprium_s.ram + 0xF8A + index*16) = 0;
	}
	if ( (framePtr == 0) || (framePtr == -1) ) {
		*(UINT16*) (paprium_s.ram + 0xF80 + index*16) = 0;
		return;
	}


	spritePtr  = paprium_obj_ram[framePtr + 0] << 0;
	spritePtr += paprium_obj_ram[framePtr + 1] << 8;
	spritePtr += paprium_obj_ram[framePtr + 2] << 16;
	animFlags = paprium_obj_ram[framePtr + 3];

	if ( spritePtr == 0 ) return;

	framePtr += 4; 

	if ( animFlags < 0x80 ) {
		if ( nextAnim == 0xFFFF ) {
			int nextFrame;

			nextFrame  = paprium_obj_ram[framePtr + 0] << 0;
			nextFrame += paprium_obj_ram[framePtr + 1] << 8;
			nextFrame += paprium_obj_ram[framePtr + 2] << 16;

			framePtr = nextFrame;
		}
		else if ( reset == 0 ) {
			anim = nextAnim;
			reset = 1;

			goto reload;
		}
	}

	paprium_s.obj[index] = framePtr;

	count = paprium_obj_ram[(spritePtr++)^1];
	int flags2 = paprium_obj_ram[(spritePtr++)^1];
	flags2 = 0; // eliminate unused variable warning.  (spritePtr++ ..)

	spr_x = pos_x;
	spr_y = pos_y;

	for ( lcv = 0; lcv < count; lcv++ ) {
//		int misc = ((paprium_obj_ram[spritePtr + 3] >> 4) & 0x0F);
		int size_x = ((paprium_obj_ram[spritePtr + 3] >> 2) & 0x03) + 1;
		int size_y = ((paprium_obj_ram[spritePtr + 3] >> 0) & 0x03) + 1;
		int tile = *(UINT16*) (paprium_obj_ram + spritePtr + 4);
		int tileAttr = *(UINT16*) (paprium_obj_ram + spritePtr + 6) & ~0x1FF;
		int ofs = *(UINT16*) (paprium_obj_ram + spritePtr + 6) & 0x1FF;

		int tilePtr = paprium_tile_ptr + tile*4;
		int tileSize = size_x * size_y * 0x20;

		int sprAttr = 0;


		if ( !flip_h )
			spr_x += (INT8) paprium_obj_ram[spritePtr + 1];
		else
			spr_x -= (INT8) paprium_obj_ram[spritePtr + 1];

		spr_y += (INT8) paprium_obj_ram[spritePtr + 0];
		spritePtr += 8;


		if ( tile == 0 ) continue;
		if ( spriteCount >= 94 ) continue;

		sprAttr  = (tileAttr & 0x8000) ? 0x8000 : (objAttr & 0x8000);
		sprAttr += (tileAttr & 0x6000) ? (tileAttr & 0x6000) : (objAttr & 0x6000);
		sprAttr += (tileAttr & 0x1800) ^ (objAttr & 0x1800);

		if ( (spr_y + size_y*8 >= (128-16)) && (spr_y + size_y*8 < (368+16)) ) {
			if ( (!flip_h && ((spr_x + size_x*8 >= 128) && spr_x < 448)) ||
				 (flip_h && ((spr_x - size_x*8 < 448) && spr_x >= 128)) ) {
				if ( spriteCount < 80 ) {
					*(UINT16*) (paprium_s.ram + 0xB00 + spriteCount*8) = spr_y & 0x3FF;
					*(UINT16*) (paprium_s.ram + 0xB02 + spriteCount*8) = ((size_x-1) << 10) + ((size_y-1) << 8) + (spriteCount+1);
					*(UINT16*) (paprium_s.ram + 0xB04 + spriteCount*8) = sprAttr + ((vram / 0x20) & 0x7FF);
					*(UINT16*) (paprium_s.ram + 0xB06 + spriteCount*8) = (spr_x - ((!flip_h) ? 0 : size_x*8)) & 0x1FF;
				}
				else {
					if ( spriteCount == 80 ) {  /* sprite 0 lock */
						paprium_s.ram[0xD7A] = 1;  /* exp.s list */

						memcpy(paprium_s.exps_ram, paprium_s.ram + 0xB00, 8);
						paprium_s.exps_ram[2] = 14;  /* normal list */

						spriteCount++;
					}

					*(UINT16*) (paprium_s.exps_ram + 0 + (spriteCount-80)*8) = spr_y & 0x3FF;
					*(UINT16*) (paprium_s.exps_ram + 2 + (spriteCount-80)*8) = ((size_x-1) << 10) + ((size_y-1) << 8) + ((spriteCount-80)+1);
					*(UINT16*) (paprium_s.exps_ram + 4 + (spriteCount-80)*8) = sprAttr + ((vram / 0x20) & 0x7FF);
					*(UINT16*) (paprium_s.exps_ram + 6 + (spriteCount-80)*8) = (spr_x - ((!flip_h) ? 0 : size_x*8)) & 0x1FF;
				}

				spriteCount++;
			}
		}

		int ptr = paprium_tile_ptr + (*(UINT16*)(cart_rom + tilePtr) << 16) + *(UINT16*)(cart_rom + tilePtr + 2);
		static UINT8 tile_ram[0x10000];

		paprium_decoder_type(ptr, tile_ram);
		memcpy(paprium_s.ram + src, tile_ram + ofs * 0x20, tileSize);

		*(UINT16*) (paprium_s.ram + 0x1400 + dmaPtr*16) = 0x8F02;
		*(UINT16*) (paprium_s.ram + 0x1402 + dmaPtr*16) = 0x9300 + ((tileSize >> 1) & 0xFF);
		*(UINT16*) (paprium_s.ram + 0x1404 + dmaPtr*16) = 0x9500 + ((src >> 1) & 0xFF);
		*(UINT16*) (paprium_s.ram + 0x1406 + dmaPtr*16) = 0x9400 + ((tileSize >> 9) & 0xFF);
		*(UINT16*) (paprium_s.ram + 0x1408 + dmaPtr*16) = 0x9700;
		*(UINT16*) (paprium_s.ram + 0x140A + dmaPtr*16) = 0x9600 + ((src >> 9) & 0xFF);
		*(UINT16*) (paprium_s.ram + 0x140C + dmaPtr*16) = 0x4000 + (vram & 0x3FFF);
		*(UINT16*) (paprium_s.ram + 0x140E + dmaPtr*16) = 0x0080 + (vram >> 14);

		*(UINT16*) (paprium_s.ram + 0x1F16) = ++dmaPtr;
		*(UINT16*) (paprium_s.ram + 0x1F18) = spriteCount;


		src += tileSize;
		vram += tileSize;
	}

	paprium_s.draw_src = src;
	paprium_s.draw_dst = vram;
}

static void paprium_sprite_init(UINT8 arg)
{
	memset(paprium_s.ram + 0x1F20, 0, 14*8);
}

static void paprium_sprite_start(UINT8 arg)
{
	int count = *(UINT16*)(paprium_s.ram + 0x1F18);

	*(UINT16*) (paprium_s.ram + 0x1F16) = 0x0001;  /* dma count */

	*(UINT16*) (paprium_s.ram + 0x1400) = 0x8f02;
	*(UINT16*) (paprium_s.ram + 0x1402) = 0x9340;
	*(UINT16*) (paprium_s.ram + 0x1404) = 0x9580;
	*(UINT16*) (paprium_s.ram + 0x1406) = 0x9401;
	*(UINT16*) (paprium_s.ram + 0x1408) = 0x9700;
	*(UINT16*) (paprium_s.ram + 0x140a) = 0x9605;
	*(UINT16*) (paprium_s.ram + 0x140c) = 0x7000;
	*(UINT16*) (paprium_s.ram + 0x140e) = 0x0083;


	if ( count < 80 )
		memcpy(paprium_s.ram + 0x1F20, paprium_s.ram + 0xB00, 14*8);
	else
		memcpy(paprium_s.ram + 0x1F20, paprium_s.exps_ram, 14*8);

	paprium_s.draw_src = 0x2000;
	paprium_s.draw_dst = 0x200;
}

static void paprium_sprite_stop(UINT8 arg)
{
	int count = *(UINT16*)(paprium_s.ram + 0x1F18);

	if (count == 0) {
		memset(paprium_s.ram + 0xB00, 0, 8);
		memset(paprium_s.exps_ram, 0, 8);
	}
	else if (count <= 80) {
		paprium_s.ram[0xB02 + (count-1)*8] = 0;

		if (count <= 14) {
			paprium_s.exps_ram[2 + (count-1)*8] = 0;
		}
	}
	else {
		paprium_s.exps_ram[2 + (count-81)*8] = 0;
	}

	if (arg == 2) {
		*(UINT16*) (paprium_s.ram + 0x1F1C) = 1;  /* exp.s - force draw */
	}
}

static void paprium_sprite_pause(UINT8 arg)
{
	int count = *(UINT16*)(paprium_s.ram + 0x1F18);

	if (count == 0) {
		memset(paprium_s.ram + 0xB00, 0, 8);
	}
}

static void paprium_scaler_init(UINT8 arg)
{
	UINT8 temp[0x800];

	int ptr = (*(UINT16 *)(paprium_s.ram + 0x1E10) << 16) + *(UINT16 *)(paprium_s.ram + 0x1E12);
	paprium_decoder_type(ptr, temp);

	int out = 0;

	for ( int col = 0; col < 64; col++ ) {
		for ( int row = 0; row < 64; row++ ) {
			if ( col & 1 )
				paprium_s.scaler_ram[out] = temp[(row*32 + (col/2))^1] & 0x0F;
			else
				paprium_s.scaler_ram[out] = temp[(row*32 + (col/2))^1] >> 4;

			out++;
		}
	}
}

static void paprium_scaler(UINT8 arg)
{
	int left = *(UINT16 *)(paprium_s.ram + 0x1E10);
	int right = *(UINT16 *)(paprium_s.ram + 0x1E12);
	int scale = *(UINT16 *)(paprium_s.ram + 0x1E14);
	int ptr = *(UINT16 *)(paprium_s.ram + 0x1E16);
	int step = 64 * 0x10000 / scale;
	int ptr_frac = 0;

	for ( int col = 0; col < 128; col++ ) {
		int base = (col & 4) ? 0x600 : 0x200;
		int out = ((col / 8) * 64) + ((col & 2) / 2);

		for ( int row = 0; row < 64; row += 2 ) {
			if ( (col >= left) && (col < right) ) {
				if (col & 1)
					paprium_s.ram[(base + out)^1] += paprium_s.scaler_ram[row*64 + ptr] & 0x0F;
				else
					paprium_s.ram[(base + out)^1] = paprium_s.scaler_ram[row*64 + ptr] << 4;
			}
			else if ( (col & 1) == 0 )
				paprium_s.ram[(base + out)^1] = 0;

			out += 2;
		}

		if ( (col >= left) && (col < right) && (ptr < 64) ) {
			ptr_frac += 0x10000;

			while ( ptr_frac >= step ) {
				ptr++;
				ptr_frac -= step;
			}
		}
	}
}

static void paprium_sram_read(int bank)
{
	int offset = *(UINT16 *)(paprium_s.ram + 0x1E10);

	if ((bank >= 0) && (bank <= 3)) {
		memcpy(paprium_s.ram + offset, sram_sram + (bank * 0x780), 0x780);
	}
}

static void paprium_sram_write(int bank)
{
	int offset = *(UINT16 *)(paprium_s.ram + 0x1E12);

	if ((bank >= 0) && (bank <= 3)) {
		memcpy(sram_sram + (bank * 0x780), paprium_s.ram + offset, 0x780);
	}
}

static void paprium_mapper(UINT8 arg)
{
	memcpy(paprium_s.ram + 0x8000, cart_rom + 0x8000, 0x8000);  /* troll / minigame */
}

static void paprium_boot(UINT8 arg)
{
	paprium_sfx_ptr = (*(UINT16*)(paprium_s.ram + 0x1E20) << 16) + *(UINT16*)(paprium_s.ram + 0x1E22);
	paprium_sprite_ptr = (*(UINT16*)(paprium_s.ram + 0x1E24) << 16) + *(UINT16*)(paprium_s.ram + 0x1E26);
	paprium_tile_ptr = (*(UINT16*)(paprium_s.ram + 0x1E28) << 16) + *(UINT16*)(paprium_s.ram + 0x1E2A);

	paprium_decoder_type(paprium_sprite_ptr, paprium_obj_ram);

	paprium_s.decoder_size = 0;
}


static const int track_lut[0x40] = { // track# requested by game <-> ost track
	-1,  2,  8, 42,  5, 31, 43,  3, -1, -1, -1, 26, 23, -1,  7, 36,
	35, 30, 21, 44, 29, 24, 27,  4, 45, 15, -1, 28, 33, 41, 46, -1,
	13, 22, 11, 38, 40, 47, 17, 25, 16, -1, 19, 14, -1, -1,  9, 20,
	-1, 18, 48, 49, 10, 12, 32,  6, 50,  1, 39, 34, 37, 51, 52, -1
};

static void paprium_music(UINT8 track)
{
	track &= 0x3f;

	bprintf(0, _T("load track: $%x (%d)  LUT %d\n"), track, track, track_lut[track]);
	if ((track_lut[track] - 1) >= 0) {
		bprintf(0, _T("playing $%x (%d)\n"), track_lut[track] - 1, track_lut[track] - 1);
		BurnSampleChannelPlay(0, track_lut[track] - 1, -1);
	}
}

static void paprium_music_volume(UINT8 level)
{
	paprium_s.music_volume = level;

	bprintf(0, _T("music volume: %x (%d)\n"), level, level);
}

static void paprium_audio_setting(UINT8 flags)
{
	paprium_s.audio_flags = flags;

	paprium_s.ram[0x1801] = flags & 0x01;  /* dac */
	paprium_s.ram[0x1800]  = (flags & 0x01) ? 0x80 : 0x00;  /* dac */
	paprium_s.ram[0x1800] += (flags & 0x02) ? 0x40 : 0x00;  /* ntsc */
}

static void paprium_cmd_ack()
{
	*(UINT16*)(paprium_s.ram + 0x1FEA) &= 0x7FFF;
}

static void paprium_sfx_volume(UINT8 level)
{
	paprium_s.sfx_volume = level;
}

static void paprium_sfx_play(UINT8 data)
{
	int chan, vol, pan, flags;
	int ptr, size, type;
	int lcv;
	int newch = 0, maxtime = 0;
	UINT8 *sfx = paprium_sfx_ptr + cart_rom;

	chan = *(UINT16 *)(paprium_s.ram + 0x1E10);
	vol = *(UINT16 *)(paprium_s.ram + 0x1E12);
	pan = *(UINT16 *)(paprium_s.ram + 0x1E14);
	flags = *(UINT16 *)(paprium_s.ram + 0x1E16);

	ptr = (*(UINT16 *)(sfx + data*8) << 16) | (*(UINT16 *)(sfx + data*8 + 2));
	size = (*(UINT8 *)(sfx + data*8 + 4) << 16) | (*(UINT16 *)(sfx + data*8 + 6));
	type = *(UINT8 *)(sfx + data*8 + 5);

	for ( lcv = 0; lcv < 8; lcv++, chan >>= 1 ) {
		if ( (chan & 1) == 0 ) continue;

		if ( paprium_s.sfx[lcv].size ) {
			if ( maxtime < paprium_s.sfx[lcv].time ) {
				maxtime = paprium_s.sfx[lcv].time;
				newch = lcv;
			}
			continue;
		}

		newch = lcv;
		break;
	}

	paprium_s.sfx[newch].volume = vol;
	paprium_s.sfx[newch].panning = pan;
	paprium_s.sfx[newch].type = type;

	paprium_s.sfx[newch].num = data;
	paprium_s.sfx[newch].flags = flags;

	paprium_s.sfx[newch].loop = 0;
	paprium_s.sfx[newch].count = 0;
	paprium_s.sfx[newch].time = 0;
	paprium_s.sfx[newch].tick = 0;
	paprium_s.sfx[newch].decay = 0;

	if ( flags & 0x4000 )
		paprium_s.sfx[newch].echo = (paprium_s.echo_pan++) & 1;

	paprium_s.sfx[newch].ptr = ptr;
	paprium_s.sfx[newch].start = ptr;
	paprium_s.sfx[newch].size = size;
}

static void paprium_sfx_stop(UINT8 data)
{
	int flags = *(UINT16 *)(paprium_s.ram + 0x1E10);

	for (int lcv = 0; lcv < 8; lcv++ ) {
		if ( !(data & (1 << lcv) ) ) continue;

		if ( flags == 0 ) {
			paprium_s.sfx[lcv].size = 0;
		}

		paprium_s.sfx[lcv].decay = flags;
		paprium_s.sfx[lcv].loop = 0;

		break;
	}
}

static void paprium_sfx_loop(UINT8 data)
{
	for (int lcv = 0; lcv < 8; lcv++, data >>= 1) {
		if ((data & 1) == 0) continue;

		paprium_s.sfx[lcv].volume = *(UINT16 *)(paprium_s.ram + 0x1E10);
		paprium_s.sfx[lcv].panning = *(UINT16 *)(paprium_s.ram + 0x1E12);
		paprium_s.sfx[lcv].decay = *(UINT16 *)(paprium_s.ram + 0x1E14);

		paprium_s.sfx[lcv].loop = 1;

		break;
	}
}


static void paprium_cmd(UINT16 data)
{
	UINT8 cmd = data >> 8;
	data &= 0xff;

	switch (cmd) {
		case 0x84: paprium_mapper(data); break;
		case 0x88: paprium_audio_setting(data); break;
		case 0x8C: paprium_music(data); break;
		case 0xAD: paprium_sprite(data); break;
		case 0xAE: paprium_sprite_start(data); break;
		case 0xAF: paprium_sprite_stop(data); break;
		case 0xB0: paprium_sprite_init(data); break;
		case 0xB1: paprium_sprite_pause(data); break;
		case 0xC6: paprium_boot(data); break;
		case 0xC9: paprium_music_volume(data); break;
		case 0xCA: paprium_sfx_volume(data); break;
		case 0xD1: paprium_sfx_play(data); break;
		case 0xD2: paprium_sfx_stop(data); break;
		case 0xD3: paprium_sfx_loop(data); break;
		case 0xDA: paprium_decoder(data); break;
		case 0xDB: paprium_decoder_copy(data); break;
		case 0xDF: paprium_sram_read(data - 1); break;
		case 0xE0: paprium_sram_write(data - 1); break;
		case 0xF4: paprium_scaler_init(data); break;
		case 0xF5: paprium_scaler(data); break;
	}

	paprium_cmd_ack();
}

static UINT8 __fastcall paprium_r8(UINT32 address)
{
	int data = paprium_s.ram[address^1];

	if ( address == 0x1800 ) {
		data = 0;
	}
	else if ( address >= 0x1880 && address < 0x1b00 ) {
		data = BurnRandom() & 0xff;
	}

	return data;
}


static UINT16 __fastcall paprium_r16(UINT32 address)
{
	int data = 0;

	if ( (address == 0xC000) && (paprium_s.decoder_size > 0) ) {
		int max = 0, size = 0;

		if ( paprium_s.decoder_mode == 2 )
			max = 0x4000;

		else if ( paprium_s.decoder_mode == 7 )
			max = 0x800;

		size = (paprium_s.decoder_size > max) ? max : paprium_s.decoder_size;

		memcpy(paprium_s.ram + 0xC000, paprium_s.decoder_ram + paprium_s.decoder_ptr, size);

		paprium_s.decoder_ptr += size;
		paprium_s.decoder_size -= size;
	}

	switch( address ) {
		case 0x1FE4:
			data = 0xFFFF;
			data &= ~(1<<2);
			data &= ~(1<<6);
			break;

		case 0x1FE6:
			data = 0xFFFF;
			data &= ~(1<<14);
			data &= ~(1<<8);  /* sram */
			data &= ~(1<<9);  /* sram */
			break;

		case 0x1FEA:
			data = 0xFFFF;
			data &= ~(1<<15);
			break;

		default:
			data = *(UINT16 *)(paprium_s.ram + address);
			break;
	}

	return data;
}

static void __fastcall paprium_w8(UINT32 address, UINT8 data)
{
	paprium_s.ram[address^1] = data;
}

static void __fastcall paprium_w16(UINT32 address, UINT16 data)
{
	*(UINT16 *)(paprium_s.ram + address) = data;

	if ( address == 0x1FEA ) {
		paprium_cmd(data);
	}
}

static UINT8 __fastcall paprium_io_r8(UINT32 address)
{
	if ( address == 0xA14101 ) {
		return paprium_tmss;
	}
	return MegadriveReadByte(address);
}

static void __fastcall paprium_io_w8(UINT32 address, UINT8 data)
{
	if ( address == 0xA14101 ) {
		paprium_tmss = data;
		return;
	}

	MegadriveWriteByte(address, data);
}


static void paprium_map()
{
	SekOpen(0);

	SekMapHandler(7, 0x000000, 0x00ffff, MAP_READ | MAP_WRITE);
	SekSetReadByteHandler(7, paprium_r8);
	SekSetReadWordHandler(7, paprium_r16);
	SekSetWriteByteHandler(7, paprium_w8);
	SekSetWriteWordHandler(7, paprium_w16);

	SekSetReadByteHandler (0, paprium_io_r8);
	SekSetWriteByteHandler(0, paprium_io_w8);

	// note: 0-ffff FETCH comes directly from paprium_s.ram (not handlers)
	SekMapMemory(&paprium_s.ram[0],	0x000000, 0x00ffff, MAP_FETCH);

	SekClose();
}

static void paprium_exit()
{
	BurnFree(paprium_obj_ram);
	paprium_obj_ram = NULL;

	BurnFree(samples_buffer);
	samples_buffer = NULL;

	BurnSampleExit();
}

static void paprium_init()
{
	if (!paprium_obj_ram)
		paprium_obj_ram = (UINT8*)BurnMalloc(0x80000);

	if (!samples_buffer)
		samples_buffer = (INT16*)BurnMalloc(0x1000 * 2 * 2);

	INT32 on_demand_samples = MegadriveDIP[2] & 1;

#if !defined BUILD_X64_EXE && !defined __LIBRETRO__
	// 32bit process can't handle it
	// libretro is a more complex case so let's not enable that limitation for now
	on_demand_samples = 1;
#endif

	BurnSampleInit(0 + (on_demand_samples ? 0x8000 : 0)); // setting nostore / load on demand via dip

	paprium_map();
}

static void paprium_reset()
{
	BurnSampleReset();

	memset(&paprium_s, 0, sizeof(paprium_s));
	memcpy(paprium_s.ram, cart_rom, 0x10000);

	memset(paprium_obj_ram, 0, 0x80000);

	paprium_tmss = 1;

#if 1  /* fast loadstate */
	int ptr = (*(UINT16*)(cart_rom + 0xaf77c) << 16) + *(UINT16*)(cart_rom + 0xaf77e);

	paprium_sfx_ptr = (*(UINT16*)(cart_rom + ptr + 0x778)<< 16) + *(UINT16*)(cart_rom + ptr + 0x77a);
	paprium_sprite_ptr = (*(UINT16*)(cart_rom + 0x10014)<< 16) + *(UINT16*)(cart_rom + 0x10016);
	paprium_tile_ptr = (*(UINT16*)(cart_rom + ptr + 0x780)<< 16) + *(UINT16*)(cart_rom + ptr + 0x782);

	paprium_decoder_type(paprium_sprite_ptr, paprium_obj_ram);
	paprium_s.decoder_size = 0;

//	*(UINT16*)(paprium_s.ram + 0x192) = 0x3634;  /* 6-button, multitap */
#endif


	paprium_s.ram[0x1D1D^1] = 0x04;  /* rom ok - dynamic patch */
	paprium_s.ram[0x1D2C^1] = 0x67;


#if 1  /* boot hack */
	*(UINT16*) (paprium_s.ram + 0x1560) = 0x4EF9;
	*(UINT16*) (paprium_s.ram + 0x1562) = 0x0001;
	*(UINT16*) (paprium_s.ram + 0x1564) = 0x0100;
#endif


#if DEBUG_CHEAT  /* cheat - big hurt */
	*(UINT16*) (cart_rom + 0x9FE38 + 6) = 0x007F;	/* Tug */
	*(UINT16*) (cart_rom + 0x9FE58 + 6) = 0x007F;
	*(UINT16*) (cart_rom + 0x9FF18 + 6) = 0x007F;
	*(UINT16*) (cart_rom + 0x9FF38 + 6) = 0x007F;

	*(UINT16*) (cart_rom + 0x9FB58 + 6) = 0x007F;	/* Alex */
	*(UINT16*) (cart_rom + 0x9FB78 + 6) = 0x007F;
	*(UINT16*) (cart_rom + 0x9FBF8 + 6) = 0x007F;
	*(UINT16*) (cart_rom + 0x9FC18 + 6) = 0x007F;
	*(UINT16*) (cart_rom + 0x9FCB8 + 6) = 0x007F;
	*(UINT16*) (cart_rom + 0x9FCD8 + 6) = 0x007F;

	*(UINT16*) (cart_rom + 0x9F758 + 6) = 0x007F;	/* Dice */
	*(UINT16*) (cart_rom + 0x9F778 + 6) = 0x007F;
	*(UINT16*) (cart_rom + 0x9F798 + 6) = 0x007F;
	*(UINT16*) (cart_rom + 0x9F7B8 + 6) = 0x007F;
	*(UINT16*) (cart_rom + 0x9F7D8 + 6) = 0x007F;
	*(UINT16*) (cart_rom + 0x9F898 + 6) = 0x007F;
#endif


#if 1  /* WM text - pre-irq delay */
	*(UINT16*)(cart_rom + 0xb9094) = 0x2079;
	*(UINT16*)(cart_rom + 0xb9096) = 0x000a;
	*(UINT16*)(cart_rom + 0xb9098) = 0xf85c;

	*(UINT16*)(cart_rom + 0xb909a) = 0x20bc;
	*(UINT16*)(cart_rom + 0xb909c) = 0x0000;
	*(UINT16*)(cart_rom + 0xb909e) = 0x0003;

	*(UINT16*)(cart_rom + 0xb90a0) = 0x20bc;
	*(UINT16*)(cart_rom + 0xb90a2) = 0x0000;
	*(UINT16*)(cart_rom + 0xb90a4) = 0x0003;

	*(UINT16*)(cart_rom + 0xb90a6) = 0x20bc;
	*(UINT16*)(cart_rom + 0xb90a8) = 0x0000;
	*(UINT16*)(cart_rom + 0xb90aa) = 0x0003;

	*(UINT16*)(cart_rom + 0xb90ac) = 0x20bc;
	*(UINT16*)(cart_rom + 0xb90ae) = 0x0000;
	*(UINT16*)(cart_rom + 0xb90b0) = 0x0003;

	*(UINT16*)(cart_rom + 0xb90b2) = 0x20bc;
	*(UINT16*)(cart_rom + 0xb90b4) = 0x0000;
	*(UINT16*)(cart_rom + 0xb90b6) = 0x0003;
#endif

	paprium_s.echo_pan = BurnRandom();
}

static void paprium_scan(INT32 nAction, INT32 *pnMin)
{
	SCAN_VAR(paprium_s);
	SCAN_VAR(paprium_tmss);
	SCAN_VAR(paprium_sfx_ptr);
	SCAN_VAR(paprium_tile_ptr);
	SCAN_VAR(paprium_sprite_ptr);

	BurnSampleScan(nAction, pnMin);
}
