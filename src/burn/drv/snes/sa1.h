void snes_sa1_init(void *mem, uint8_t *srom, int32_t sromsize, void *s_ram, int32_t s_ram_size);
void snes_sa1_run();
void snes_sa1_exit();
void snes_sa1_reset();
uint8_t snes_sa1_cart_read(uint32_t address);
void snes_sa1_cart_write(uint32_t address, uint8_t data);
void snes_sa1_handleState(StateHandler* sh);

uint8_t sa1_cpuRead(uint32_t adr);
void sa1_cpuWrite(uint32_t adr, uint8_t val);
void sa1_cpuIdle();
uint64_t sa1_getcycles();
bool sa1_isrom_address(uint32_t address);
bool sa1_snes_rom_conflict();
uint32_t sa1_snes_lastSpeed();
