void ics2115_init(INT32 cpu_clock, void (*cpu_irq_cb)(INT32), UINT8 *sample_rom, INT32 sample_rom_size);
void ics2115_reset();
UINT8 ics2115read(UINT8 offset);
void ics2115write(UINT8 offset, UINT8 data);
void ics2115_adjust_timer(INT32 ticks);
void ics2115_update(INT16 *outputs, int samples);
void ics2115_exit();
void ics2115_scan(INT32 nAction,INT32 * /*pnMin*/);

extern INT32 ICS2115_ddp2beestormmode; // hack to fix volume fadeouts in ddp2 bee storm
