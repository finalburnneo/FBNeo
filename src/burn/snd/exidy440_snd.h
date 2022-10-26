void exidy440_init(UINT8 *samples_rom, INT32 samples_len, INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz);
void exidy440_update(INT16 *output, INT32 samples_len);
void exidy440_exit();
void exidy440_reset();
void exidy440_scan(INT32 nAction, INT32 *pnMin);

UINT8 exidy440_sound_command_read();
UINT8 exidy440_sound_command_ram();
void exidy440_sound_command(UINT8 param);
UINT8 exidy440_sound_command_ack();
UINT8 exidy440_sound_volume_read(INT32 offset);
void exidy440_sound_volume_write(INT32 offset, UINT8 data);
UINT8 exidy440_m6844_read(INT32 offset);
void exidy440_m6844_write(INT32 offset, UINT8 data);
void exidy440_sound_banks_write(INT32 offset, UINT8 data);
