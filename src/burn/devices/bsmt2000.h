
#include "tms32010.h"

void bsmt2k_write_reg(UINT16 reg);
void bsmt2k_write_data(UINT16 data);
INT32 bsmt2k_read_status();

void bsmt2kReset();
void bsmt2kResetCpu();
void bsmt2kInit(INT32 clock, UINT8 *tmsrom, UINT8 *tmsram, UINT8 *data, INT32 size, void (*cb)());
void bsmt2kNewFrame();
void bsmt2k_update();
void bsmt2kExit();
void bsmt2kScan(INT32 nAction, INT32 *pnMin);

