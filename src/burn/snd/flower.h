// flower sound core
void flower_sound_init(UINT8 *rom_sample, UINT8 *rom_volume);
void flower_sound_exit();
void flower_sound_scan();
void flower_sound_reset();
void flower_sound1_w(UINT16 offset, UINT8 data);
void flower_sound2_w(UINT16 offset, UINT8 data);
void flower_sound_update(INT16 *outputs, INT32 samples_len);

