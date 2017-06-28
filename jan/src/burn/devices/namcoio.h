
#define NAMCO56xx	0
#define NAMCO58xx	1
#define NAMCO59xx	2

void namcoio_init(INT32 chip, INT32 type, UINT8 (*in0)(UINT8), UINT8 (*in1)(UINT8), UINT8 (*in2)(UINT8), UINT8 (*in3)(UINT8), void (*out0)(UINT8, UINT8), void (*out1)(UINT8, UINT8));
void namcoio_reset(INT32 chip);
INT32 namcoio_scan(INT32 chip);

void namco56xx_customio_run(INT32 chip);
void namco59xx_customio_run(INT32 chip);
void namco58xx_customio_run(INT32 chip);

void namcoio_run(INT32 chip);

UINT8 namcoio_read(INT32 chip, UINT8 offset);
void namcoio_write(INT32 chip, UINT8 offset, UINT8 data);
UINT8 namcoio_read_reset_line(INT32 chip);
void namcoio_set_reset_line(INT32 chip, INT32 state);
