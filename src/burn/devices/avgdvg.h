#ifndef __AVGDVG__
#define __AVGDVG__

INT32 avgdvg_done(void);
void avgdvg_go();

void avgdvg_reset();

void avg_set_flip_x(INT32 flip);
void avg_set_flip_y(INT32 flip);

void avg_tempest_start(UINT8 *vectram, INT32 (*pCPUCyclesCB)());
void avg_starwars_start(UINT8 *vectram, INT32 (*pCPUCyclesCB)());
void dvg_asteroids_start(UINT8 *vectram, INT32 (*pCPUCyclesCB)());
void dvg_omegrace_start(UINT8 *vectram, INT32 (*pCPUCyclesCB)());
void avg_quantum_start(UINT8 *vectram, INT32 (*pCPUCyclesCB)());

#endif
