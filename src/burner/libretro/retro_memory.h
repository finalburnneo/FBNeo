#ifndef __RETRO_MEMORY__
#define __RETRO_MEMORY__

#include <string>
#include "libretro.h"
#include "burner.h"

extern void* pMainRamData;
extern size_t nMainRamSize;
extern bool bMainRamFound;
extern int nMemoryCount;
extern struct retro_memory_descriptor sMemoryDescriptors[10];
extern bool bMemoryMapFound;

int StateGetMainRamAcb(BurnArea *pba);

#endif
