void ymz770_init(UINT8 *rom, INT32 rom_length);
void ymz774_init(UINT8 *rom, INT32 rom_length); // ymz774-specific!
void ymz770_exit();
void ymz770_set_buffered(INT32 (*pCPUCyclesCB)(), INT32 nCPUMhz);

void ymz770_write(INT32 offset, UINT8 data);
UINT8 ymz774_read(INT32 offset); // ymz774-specific!

void ymz770_scan(INT32 nAction, INT32 *pnMin);
void ymz770_update(INT16 *output, INT32 samples_len);
void ymz770_reset();
