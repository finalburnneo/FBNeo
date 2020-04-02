#include "mcs51.h"

void qs1000_reset();
void qs1000_init(UINT8 *program_rom, UINT8 *samples, INT32 samplesize);
void qs1000_exit();
void qs1000_scan(INT32 nAction, INT32 *);
void qs1000_update(INT16 *outputs, INT32 samples_len);

void qs1000_set_volume(double volume);
void qs1000_set_write_handler(INT32 port, void (*handler)(UINT8));
void qs1000_set_read_handler(INT32 port, UINT8 (*handler)());
void qs1000_serial_in(UINT8 data);
void qs1000_set_irq(int state);
void qs1000_set_bankedrom(UINT8 *rom);
