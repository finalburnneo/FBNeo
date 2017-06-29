// wiping sound core
void wipingsnd_init(UINT8 *rom, UINT8 *prom);
void wipingsnd_exit();
void wipingsnd_reset();
void wipingsnd_write(INT32 offset, UINT8 data);
void wipingsnd_update(INT16 *outputs, INT32 samples_len);
void wipingsnd_scan();
void wipingsnd_wipingmode();

