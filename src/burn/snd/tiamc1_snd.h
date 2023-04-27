// tia mc1 sound core
void tiamc1_sound_init();
void tiamc1_sound_init_kot();
void tiamc1_sound_exit();
void tiamc1_sound_update(INT16 *output, INT32 samples_len);
void tiamc1_sound_scan(INT32 nAction, INT32 *pnMin);
void tiamc1_sound_reset();

void tiamc1_sound_timer0_write(INT32 offset, UINT8 data);
void tiamc1_sound_timer1_write(INT32 offset, UINT8 data);
void tiamc1_sound_gate_write(UINT8 data);

