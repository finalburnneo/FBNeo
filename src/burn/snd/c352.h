void c352_init(INT32 clock, INT32 divider, UINT8 *c352_rom, INT32 c352_romsize, INT32 AddToStream);
void c352_set_sync(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz);
void c352_exit();
void c352_scan(INT32 nAction, INT32 *);
void c352_reset();
UINT16 c352_read(unsigned long address);
void c352_write(unsigned long address, unsigned short val);
void c352_update(INT16 *output, INT32 samples_len);
