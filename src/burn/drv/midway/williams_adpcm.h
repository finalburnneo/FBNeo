void williams_adpcm_sound_write(UINT16 data);
void williams_adpcm_reset_write(UINT16 state);
UINT16 williams_adpcm_sound_irq_read();
void williams_adpcm_reset();
void williams_adpcm_init(UINT8 *prgrom, UINT8 *samples, INT32 prot_start, INT32 prot_end);
void williams_adpcm_exit();
INT32 williams_adpcm_sound_in_reset();
void williams_adpcm_update(INT16 *output, INT32 length);
INT32 williams_adpcm_scan(INT32 nAction, INT32 *pnMin);
