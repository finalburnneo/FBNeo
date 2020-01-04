// Atari EA-ROM, impl. by dink

UINT8 earom_read(UINT16 /*address*/);
void earom_write(UINT16 offset, UINT8 data);
void earom_ctrl_write(UINT16 /*offset*/, UINT8 data);
void earom_reset();
void earom_init();
void earom_exit();
void earom_scan(INT32 nAction, INT32 *pnMin);
