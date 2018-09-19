#ifndef __AVGDVG__
#define __AVGDVG__

#define AVGDVG_MIN          1
#define USE_DVG             1
#define USE_AVG_RBARON      2
#define USE_AVG_BZONE       3
#define USE_AVG             4
#define USE_AVG_TEMPEST     5
#define USE_AVG_MHAVOC      6
#define USE_AVG_ALPHAONE    7
#define USE_AVG_SWARS       8
#define USE_AVG_QUANTUM     9
#define AVGDVG_MAX          10

void avgdvg_init(INT32 type, UINT8 *vectram, INT32 vramromsize, INT32 (*pCPUCyclesCB)(), INT32 w, INT32 h);
void avgdvg_exit();
void avgdvg_set_cycles(INT32 cycles); // use if main cpu is not 1512000

INT32 avgdvg_done(void);
void avgdvg_go();

void avgdvg_reset();
void avgdvg_scan(INT32 nAction, INT32 *);

void avg_set_flip_x(INT32 flip);
void avg_set_flip_y(INT32 flip);

#endif
