#ifndef __I386INTF_H
#define __I386INTF_H

//#include "memory.h"
//#include "osd_cpu.h"

void i386_get_info(UINT32, union cpuinfo*);
void i486_get_info(UINT32, union cpuinfo*);
void pentium_get_info(UINT32, union cpuinfo*);
void mediagx_get_info(UINT32, union cpuinfo*);

#endif /* __I386INTF_H */
