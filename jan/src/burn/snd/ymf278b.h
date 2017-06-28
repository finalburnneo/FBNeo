#ifndef __YMF278B_H__
#define __YMF278B_H__

#define MAX_YMF278B	(1)

#define YMF278B_STD_CLOCK (33868800)						/* standard clock for OPL4 */

void ymf278b_scan(INT32 nAction, INT32* pnMin);
void ymf278b_pcm_update(int num, INT16 **outputs, int samples);
int ymf278b_timer_over(int num, int timer);
int ymf278b_start(int num, UINT8 *rom, INT32 romsize, void (*irq_cb)(INT32, INT32), void (*timer_cb)(INT32, INT32, double), int clock, int rate);
void ymf278b_reset();
void YMF278B_sh_stop(void);

READ8_HANDLER( YMF278B_status_port_0_r );
READ8_HANDLER( YMF278B_data_port_0_r );
WRITE8_HANDLER( YMF278B_control_port_0_A_w );
WRITE8_HANDLER( YMF278B_data_port_0_A_w );
WRITE8_HANDLER( YMF278B_control_port_0_B_w );
WRITE8_HANDLER( YMF278B_data_port_0_B_w );
WRITE8_HANDLER( YMF278B_control_port_0_C_w );
WRITE8_HANDLER( YMF278B_data_port_0_C_w );
#endif
