#ifndef tms5110_h
#define tms5110_h


/* TMS5110 commands */
                                     /* CTL8  CTL4  CTL2  CTL1  |   PDC's  */
                                     /* (MSB)             (LSB) | required */
#define TMS5110_CMD_RESET        (0) /*    0     0     0     x  |     1    */
#define TMS5110_CMD_LOAD_ADDRESS (2) /*    0     0     1     x  |     2    */
#define TMS5110_CMD_OUTPUT       (4) /*    0     1     0     x  |     3    */
#define TMS5110_CMD_READ_BIT     (8) /*    1     0     0     x  |     1    */
#define TMS5110_CMD_SPEAK       (10) /*    1     0     1     x  |     1    */
#define TMS5110_CMD_READ_BRANCH (12) /*    1     1     0     x  |     1    */
#define TMS5110_CMD_TEST_TALK   (14) /*    1     1     1     x  |     3    */

/* Variants */

#define TMS5110_IS_5110A	(1)
#define TMS5110_IS_5100		(2)
#define TMS5110_IS_5110		(3)

#define TMS5110_IS_CD2801	TMS5110_IS_5100
#define TMS5110_IS_TMC0281	TMS5110_IS_5100

#define TMS5110_IS_CD2802	TMS5110_IS_5110
#define TMS5110_IS_M58817	TMS5110_IS_5110

void tms5110_init(INT32 freq, UINT8 *rom);
void tms5110_exit();
void tms5110_reset();
void tms5110_scan(INT32 nAction, INT32 *pnMin);

void tms5110_set_variant(INT32 vari);

void tms5110_set_buffered(INT32 (*pCPUCyclesCB)(), INT32 nCpuMhz);
void tms5110_volume(double vol);

void tms5110_set_M0_callback(INT32 (*func)());
void tms5110_set_load_address(void (*func)(int));

INT32 tms5110_ctl_read();
INT32 m58817_status_read();

INT32 tms5110_status_read();
INT32 tms5110_ready_read();
void tms5110_CTL_set(UINT8 data);
void tms5110_PDC_set(UINT8 data);

void tms5110_set_frequency(UINT32 freq);
void tms5110_update(INT16 *buffer, INT32 samples_len); // render samples

#endif
