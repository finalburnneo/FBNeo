
#ifndef CX4_H
#define CX4_H

void cx4_init(void *mem);
uint8_t cx4_read(uint32_t addr);
void cx4_write(uint32_t addr, uint8_t value);
void cx4_run();
void cx4_reset();
void cx4_handleState(StateHandler* sh);

#endif
