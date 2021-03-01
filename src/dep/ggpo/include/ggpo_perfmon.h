#ifndef _PERFMON_H
#define _PERFMON_H

#include "ggponet.h"

void ggpoutil_perfmon_init(HWND hwnd);
void ggpoutil_perfmon_update(GGPOSession *ggpo, GGPONetworkStats &stats);
void ggpoutil_perfmon_toggle();
bool ggpoutil_perfmon_isopen();

#endif
