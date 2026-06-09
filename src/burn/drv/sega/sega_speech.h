void sega_speech_reset();
void sega_speech_data_write(UINT8 data);
void sega_speech_init(UINT8 *program, UINT8 *samples);
void sega_speech_exit();
void sega_speech_new_frame();
void sega_speech_run(INT32 i, INT32 nInterleave, INT32 *nCyclesTotal, INT32 *nCyclesDone, INT32 which);
void sega_speech_render(INT16 *output, INT32 length);
void sega_speech_scan(INT32 nAction, INT32 *pnMin);
