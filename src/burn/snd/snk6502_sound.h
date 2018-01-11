#ifndef M_LN2
#define M_LN2       0.69314718055994530942
#endif

void snk6502_sound_reset();
void snk6502_sound_init(UINT8 *soundrom);
void snk6502_set_music_freq(INT32 freq);
void snk6502_set_music_clock(double clock_time);
INT32 snk6502_music0_playing();
void sasuke_sound_w(UINT16 offset, UINT8 data);
void satansat_sound_w(UINT16 offset, UINT8 data);
void vanguard_sound_w(UINT16 offset, UINT8 data);
void fantasy_sound_w(UINT16 offset, UINT8 data);
void vanguard_speech_w(UINT8 data);
void fantasy_speech_w(UINT8 data);
void snk_sound_update(INT16 *buffer, INT32 samples);
void snk6502_sound_savestate();
