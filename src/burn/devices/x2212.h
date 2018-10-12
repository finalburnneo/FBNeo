// Atari EE-ROM (X2212), impl. by dink

UINT8 x2212_read(INT32 chip, UINT16 offset);
void x2212_write(INT32 chip, UINT16 offset, UINT8 data);
void x2212_store(INT32 chip, INT32 state);
void x2212_recall(INT32 chip, INT32 state);

void x2212_init(INT32 num_chips);
void x2212_init_autostore(INT32 num_chips); // auto-store on write to nvram
void x2212_exit();
void x2212_reset();
void x2212_scan(INT32 nAction, INT32 *pnMin);
