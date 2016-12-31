#include "mcs51.h"

void mermaidWrite(UINT8 data);
UINT8 mermaidRead();
UINT8 mermaidStatus();

extern INT32 mermaid_sub_z80_reset;
void mermaidReset();
void mermaidInit(UINT8 *rom, UINT8 *inputs);
void mermaidExit();
INT32 mermaidRun(INT32 cycles);
INT32 mermaidScan(INT32 nAction);

