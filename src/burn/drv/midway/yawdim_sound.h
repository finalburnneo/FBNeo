void yawdim_sound_reset();
void yawdim_soundlatch_write(UINT16 data);
void yawdim_sound_init(UINT8 *prgrom, UINT8 *samples, INT32 yawdim2);
void yawdim_sound_exit();
INT32 yawdim_sound_in_reset();
void yawdim_sound_update(INT16 *output, INT32 length);
INT32 yawdim_sound_scan(INT32 nAction, INT32 *pnMin);
