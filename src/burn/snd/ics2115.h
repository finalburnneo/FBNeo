void ics2115_init(void(*cpu_irq_cb)(INT32), UINT8 *sample_rom, INT32 sample_rom_size);
void ics_2115_set_volume(double volume);
void ics2115_reset();
UINT8 ics2115read(UINT8 offset);
void ics2115write(UINT8 offset, UINT8 data);
void ics2115_update(INT32 samples);
void ics2115_exit();
void ics2115_scan(INT32 nAction, INT32 * /*pnMin*/);
