#include "burn_ym3812.h"
#include "burn_ym2151.h"
#include "burn_ym2203.h"
#include "msm6295.h"

extern UINT8 *SeibuZ80DecROM;
extern UINT8 *SeibuZ80ROM;
extern UINT8 *SeibuZ80RAM;

extern UINT8 *SeibuADPCMData[2];
extern INT32 SeibuADPCMDataLen[2];

extern INT32 seibu_coin_input;
extern INT32 seibu_fm_type;

unsigned char seibu_main_word_read(INT32 offset);
void seibu_main_word_write(INT32 offset, UINT8 data);
void seibu_sound_mustb_write_word(INT32 offset, UINT16 data);

void seibu_sound_reset();

void seibu_sound_update(INT16 *pbuf, INT32 nLen);
void seibu_sound_update_cabal(INT16 *pbuf, INT32 nLen);

/*
	type & 3

	Type 0 - YM3812
	Type 1 - YM2151
	Type 2 - YM2203

	type & 8 - adpcm (disable oki6295)

	otherwise add a single oki6295
	add 4 to init a second oki6295
*/

void seibu_sound_init(INT32 type, INT32 encrypted_len, INT32 freq0 /*cpu*/, INT32 freq1 /*ym*/, INT32 freq2 /*oki*/);
void seibu_sound_exit();

void seibu_sound_scan(INT32 nAction, INT32 *pnMin);
