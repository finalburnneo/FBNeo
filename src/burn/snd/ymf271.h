typedef void (*OPL3_TIMERHANDLER)(INT32, INT32 timer,double period);
typedef void (*OPL3_IRQHANDLER)(INT32, INT32 irq);

void ymf271_init(int clock, UINT8 *rom, INT32 romsize, void (*irq_cb)(INT32, INT32), void (*timer_cb)(INT32, INT32, double));
void ymf271_exit();
void ymf271_reset();

void ymf271_set_external_handlers(UINT8 (*read)(UINT32), void (*write)(UINT32, UINT8));

void ymf271_update(INT16 **buffers, int samples);
INT32 ymf271_timer_over(INT32 id);
UINT8 ymf271_read(INT32 offset);
void ymf271_write(INT32 offset, UINT8 data);
void ymf271_scan(INT32 nAction);

