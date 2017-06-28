// gaelco sound
void    gaelcosnd_update(INT16 *outputs, INT32 samples);
UINT16  gaelcosnd_r(INT32 offset);
void    gaelcosnd_w(INT32 offset, UINT16 data);
void    gaelcosnd_start(UINT8 *soundrom, int offs1, int offs2, int offs3, int offs4);
void    gaelcosnd_exit();
void    gaelcosnd_scan();
void    gaelcosnd_reset();
