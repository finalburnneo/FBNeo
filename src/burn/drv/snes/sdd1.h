void snes_sdd1_init(uint8_t *s_rom, int32_t rom_size, void *s_ram, int32_t sram_size);
void snes_sdd1_exit();
void snes_sdd1_reset();
void snes_sdd1_handleState(StateHandler* sh);
uint8_t snes_sdd1_cart_read(uint32_t address);
void snes_sdd1_cart_write(uint32_t address, uint8_t data);
