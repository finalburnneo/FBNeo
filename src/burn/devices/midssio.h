extern UINT8 *ssio_inputs; // 5 - point to inputs
extern UINT8 ssio_dips; // 1 - dips for ssio board
extern INT32 ssio_spyhunter;

void ssio_14024_clock(INT32 interleave); // interrupt generator

void ssio_reset_write(INT32 state);
void ssio_write_ports(UINT8 offset, UINT8 data);
UINT8 ssio_read_ports(UINT8 offset);

void ssio_set_custom_input(INT32 which, INT32 mask, UINT8 (*handler)(UINT8 offset));
void ssio_set_custom_output(INT32 which, INT32 mask, void (*handler)(UINT8 offset, UINT8 data));
void ssio_reset();
void ssio_init(UINT8 *rom, UINT8 *ram, UINT8 *prom);
void ssio_basevolume(double vol);
void ssio_exit();
void ssio_scan(INT32 nAction, INT32 *pnMin);
INT32 ssio_initialized();
