// lunar lander sound custom, ported from mame 0.37b7
void llander_sound_init();
void llander_sound_exit();
void llander_sound_update(INT16 *buffer, INT32 n);
void llander_sound_lfsr_reset();
void llander_sound_reset();
void llander_sound_write(UINT8 data);
