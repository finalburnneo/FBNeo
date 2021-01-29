UINT16 narc_sound_response_read();
void narc_sound_write(UINT16 data);
void narc_sound_reset_write(INT32 state);
void narc_sound_reset();
void narc_sound_init(UINT8 *rom0, UINT8 *rom1);
void narc_sound_exit();
INT32 narc_sound_in_reset();
void narc_sound_update(INT16 *stream, INT32 length);
INT32 narc_sound_scan(INT32 nAction, INT32 *pnMin);
