// Based on MAME sources by Luca Elia

#include "burnint.h"
#include "msm6295.h"
#include "burn_sound.h"
#include "x1010.h"

UINT8 *X1010SNDROM;
INT32 X1010_Arbalester_Mode = 0;

struct x1_010_info * x1_010_chip = NULL;

void x1010_sound_bank_w(UINT32 offset, UINT16 data)
{
#if defined FBA_DEBUG
	if (!DebugSnd_X1010Initted) bprintf(PRINT_ERROR, _T("x1010_sound_bank_w called without init\n"));
#endif

	//int banks = (memory_region_length( REGION_SOUND1 ) - 0x100000) / 0x20000;
	//if ( data >= banks ) {
	//	bprintf(PRINT_NORMAL, _T("invalid sound bank %04x\n"), data);
	//	data %= banks;
	//}
	memcpy(X1010SNDROM + offset * 0x20000, X1010SNDROM + 0x100000 + data * 0x20000, 0x20000);

	// backup sound bank index, need when game load status
	x1_010_chip->sound_banks[ offset ] = data;
}

UINT8 x1010_sound_read(UINT32 offset)
{
#if defined FBA_DEBUG
	if (!DebugSnd_X1010Initted) bprintf(PRINT_ERROR, _T("x1010_sound_read called without init\n"));
#endif

	offset ^= x1_010_chip->address;
	return x1_010_chip->reg[offset];
}

UINT16 x1010_sound_read_word(UINT32 offset)
{
#if defined FBA_DEBUG
	if (!DebugSnd_X1010Initted) bprintf(PRINT_ERROR, _T("x1010_sound_read_word called without init\n"));
#endif

	UINT16 ret;
	
	ret = x1_010_chip->HI_WORD_BUF[offset] << 8;
	ret += x1010_sound_read(offset);
	
	return ret;
}

void x1010_sound_update()
{
#if defined FBA_DEBUG
	if (!DebugSnd_X1010Initted) bprintf(PRINT_ERROR, _T("x1010_sound_update called without init\n"));
#endif

	INT16* pSoundBuf = pBurnSoundOut;
	memset(pSoundBuf, 0, nBurnSoundLen * sizeof(INT16) * 2);

	X1_010_CHANNEL	*reg;
	INT32 ch, i, volL, volR, freq, mempos, div;
	INT8 *start, *end, data;
	UINT8 *env;
	UINT32 smp_offs, smp_step, env_offs, env_step, delta;

	for( ch = 0; ch < SETA_NUM_CHANNELS; ch++ ) {
		reg = (X1_010_CHANNEL *) & (x1_010_chip->reg[ch * sizeof(X1_010_CHANNEL)]);
		if( reg->status & 1 ) {	// Key On
			INT16 *bufL = pSoundBuf + 0;
			INT16 *bufR = pSoundBuf + 1;
			div = (reg->status&0x80) ? 1 : 0;
			if( (reg->status & 2) == 0 ) { // PCM sampling
				start    = (INT8*)( reg->start * 0x1000 + X1010SNDROM );
				mempos   = reg->start * 0x1000; // used only for bounds checking
				end      = (INT8*)((0x100 - reg->end) * 0x1000 + X1010SNDROM );
				volL     = ((reg->volume >> 4) & 0xf) * VOL_BASE;
				volR     = ((reg->volume >> 0) & 0xf) * VOL_BASE;
				smp_offs = x1_010_chip->smp_offset[ch];
				freq     = reg->frequency>>div;
				if (!volL) volL = volR;       // dink aug.17,2016: fix missing samples in ms gundam
				if (!volR) volR = volL;
				// Meta Fox does not write the frequency register. Ever
				if( freq == 0 ) freq = 4;
				// Special handling for Arbalester -dink
				if( X1010_Arbalester_Mode && ch==0x0f && reg->start != 0xc0 && reg->start != 0xc8 )
					freq = 8;

				smp_step = (UINT32)((float)x1_010_chip->rate / (float)nBurnSoundRate / 8.0 * freq * (1 << FREQ_BASE_BITS) );
#if 0
				if( smp_offs == 0 ) {
					bprintf(PRINT_ERROR, _T("Play sample %06X - %06X, channel %X volumeL %d volumeR %d freq %X step %X offset %X\n"),
							reg->start, reg->end, ch, volL, volR, freq, smp_step, smp_offs);
				}
#endif
				for( i = 0; i < nBurnSoundLen; i++ ) {
					delta = smp_offs >> FREQ_BASE_BITS;
					// sample ended?
					if( start + delta >= end) {
						reg->status &= 0xfe;					// Key off
						break;
					}

					if (mempos + delta >= 0xfffff) {            // bounds checking
						reg->status &= 0xfe;					// Key off
						bprintf(0, _T("X1-010: Overflow detected (PCM)!\n"));
						break;
					}

					data = *(start + delta);
					
					INT32 nLeftSample = 0, nRightSample = 0;
					
					if ((x1_010_chip->output_dir[BURN_SND_X1010_ROUTE_1] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
						nLeftSample += (INT32)((data * volL / 256) * x1_010_chip->gain[BURN_SND_X1010_ROUTE_1]);
					}
					if ((x1_010_chip->output_dir[BURN_SND_X1010_ROUTE_1] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
						nRightSample += (INT32)((data * volL / 256) * x1_010_chip->gain[BURN_SND_X1010_ROUTE_1]);
					}
					
					if ((x1_010_chip->output_dir[BURN_SND_X1010_ROUTE_2] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
						nLeftSample += (INT32)((data * volR / 256) * x1_010_chip->gain[BURN_SND_X1010_ROUTE_2]);
					}
					if ((x1_010_chip->output_dir[BURN_SND_X1010_ROUTE_2] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
						nRightSample += (INT32)((data * volR / 256) * x1_010_chip->gain[BURN_SND_X1010_ROUTE_2]);
					}
					
					nLeftSample = BURN_SND_CLIP(nLeftSample);
					nRightSample = BURN_SND_CLIP(nRightSample);
					
					*bufL = BURN_SND_CLIP(*bufL + nLeftSample); bufL += 2;;
					*bufR = BURN_SND_CLIP(*bufR + nRightSample); bufR += 2;

					smp_offs += smp_step;
				}
				x1_010_chip->smp_offset[ch] = smp_offs;

			} else { // Wave form
				start    = (INT8*) & (x1_010_chip->reg[reg->volume * 128 + 0x1000]);
				mempos   = reg->volume * 128 + 0x1000; // used only for bounds checking
				smp_offs = x1_010_chip->smp_offset[ch];
				freq     = ((reg->pitch_hi << 8) + reg->frequency)>>div;
				smp_step = (UINT32)((float)x1_010_chip->rate / (float)nBurnSoundRate / 128.0 / 4.0 * freq * (1 << FREQ_BASE_BITS) );

				env      = (UINT8*) & (x1_010_chip->reg[reg->end * 128]);
				env_offs = x1_010_chip->env_offset[ch];
				env_step = (UINT32)((float)x1_010_chip->rate / (float)nBurnSoundRate / 128.0 / 4.0 * reg->start * (1 << ENV_BASE_BITS) );
#if 0
				if( smp_offs == 0 ) {
					bprintf(PRINT_ERROR, _T("Play waveform %X, channel %X volume %X freq %4X step %X offset %X dlta %X\n"),
       						reg->volume, ch, reg->end, freq, smp_step, smp_offs, env_offs>>ENV_BASE_BITS );
				}
#endif
				if (mempos > 0x2000 - 0x80) {            // bounds checking
					reg->status &= 0xfe;					// Key off
					bprintf(0, _T("X1-010: Overflow detected (Waveform)!\n"));
					break;
				}

				for( i = 0; i < nBurnSoundLen; i++ ) {
					INT32 vol;
					delta = env_offs>>ENV_BASE_BITS;
	 				// Envelope one shot mode
					if( (reg->status&4) != 0 && delta >= 0x80 ) {
						reg->status &= 0xfe;					// Key off
						break;
					}

					vol = *(env + (delta & 0x7f));
					volL = ((vol >> 4) & 0xf) * VOL_BASE;
					volR = ((vol >> 0) & 0xf) * VOL_BASE;
					data  = *(start + ((smp_offs >> FREQ_BASE_BITS) & 0x7f));

					INT32 nLeftSample = 0, nRightSample = 0;
					
					if ((x1_010_chip->output_dir[BURN_SND_X1010_ROUTE_1] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
						nLeftSample += (INT32)((data * volL / 256) * x1_010_chip->gain[BURN_SND_X1010_ROUTE_1]);
					}
					if ((x1_010_chip->output_dir[BURN_SND_X1010_ROUTE_1] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
						nRightSample += (INT32)((data * volL / 256) * x1_010_chip->gain[BURN_SND_X1010_ROUTE_1]);
					}
					
					if ((x1_010_chip->output_dir[BURN_SND_X1010_ROUTE_2] & BURN_SND_ROUTE_LEFT) == BURN_SND_ROUTE_LEFT) {
						nLeftSample += (INT32)((data * volR / 256) * x1_010_chip->gain[BURN_SND_X1010_ROUTE_2]);
					}
					if ((x1_010_chip->output_dir[BURN_SND_X1010_ROUTE_2] & BURN_SND_ROUTE_RIGHT) == BURN_SND_ROUTE_RIGHT) {
						nRightSample += (INT32)((data * volR / 256) * x1_010_chip->gain[BURN_SND_X1010_ROUTE_2]);
					}
					
					nLeftSample = BURN_SND_CLIP(nLeftSample);
					nRightSample = BURN_SND_CLIP(nRightSample);
					
					*bufL = BURN_SND_CLIP(*bufL + nLeftSample); bufL += 2;;
					*bufR = BURN_SND_CLIP(*bufR + nRightSample); bufR += 2;

					smp_offs += smp_step;
					env_offs += env_step;
				}

				x1_010_chip->smp_offset[ch] = smp_offs;
				x1_010_chip->env_offset[ch] = env_offs;
			}
		}
	}
}

void x1010_sound_init(UINT32 base_clock, INT32 address)
{
	DebugSnd_X1010Initted = 1;
	
	x1_010_chip = (struct x1_010_info *) BurnMalloc( sizeof(struct x1_010_info) );

	memset(x1_010_chip, 0, sizeof(struct x1_010_info));

	x1_010_chip->base_clock = base_clock;
	x1_010_chip->rate = x1_010_chip->base_clock / 1024;
	x1_010_chip->address = address;
	
	x1_010_chip->gain[BURN_SND_X1010_ROUTE_1] = 1.00;
	x1_010_chip->gain[BURN_SND_X1010_ROUTE_2] = 1.00;
	x1_010_chip->output_dir[BURN_SND_X1010_ROUTE_1] = BURN_SND_ROUTE_BOTH;
	x1_010_chip->output_dir[BURN_SND_X1010_ROUTE_2] = BURN_SND_ROUTE_BOTH;
	X1010_Arbalester_Mode = 0;
}

void x1010_set_route(INT32 nIndex, double nVolume, INT32 nRouteDir)
{
#if defined FBA_DEBUG
	if (!DebugSnd_X1010Initted) bprintf(PRINT_ERROR, _T("x1010_set_route called without init\n"));
	if (nIndex < 0 || nIndex > 1) bprintf(PRINT_ERROR, _T("x1010_set_route called with invalid index %i\n"), nIndex);
#endif

	x1_010_chip->gain[nIndex] = nVolume;
	x1_010_chip->output_dir[nIndex] = nRouteDir;
}

void x1010_scan(INT32 nAction,INT32 *pnMin)
{
#if defined FBA_DEBUG
	if (!DebugSnd_X1010Initted) bprintf(PRINT_ERROR, _T("x1010_scan called without init\n"));
#endif

	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin =  0x029672;
	}
	
	if ( nAction & ACB_DRIVER_DATA ) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = x1_010_chip;
		ba.nLen	  = sizeof(struct x1_010_info);
		ba.szName = "X1-010";
		BurnAcb(&ba);
	}
}

void x1010_exit()
{
#if defined FBA_DEBUG
	if (!DebugSnd_X1010Initted) bprintf(PRINT_ERROR, _T("x1010_exit called without init\n"));
#endif

	BurnFree(x1_010_chip);
	
	DebugSnd_X1010Initted = 0;
}
