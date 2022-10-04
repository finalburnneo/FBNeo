void ptm6840_init(INT32 clock);
void ptm6840_set_irqcb(void (*irqcb)(INT32));
void ptm6840_exit();
void ptm6840_reset();
void ptm6840_scan(INT32 nAction);

void ptm6840_write(INT32 offset, UINT8 data);
UINT8 ptm6840_read(INT32 offset);

INT32 ptm6840Run(INT32 cycles); // run timers
INT32 ptm6840Idle(INT32 cycles);
INT32 ptm6840TotalCycles(); // for sync
void  ptm6840NewFrame();

void ptm6840_set_external_clocks(double clock0, double clock1, double clock2);
void ptm6840_set_ext_clock(int counter, double clock);

void ptm6840_set_g1(INT32 state);
void ptm6840_set_g2(INT32 state);
void ptm6840_set_g3(INT32 state);

void ptm6840_set_c1(INT32 state);
void ptm6840_set_c2(INT32 state);
void ptm6840_set_c3(INT32 state);

