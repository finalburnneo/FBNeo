#include "msm5205.h"
#include "dtimer.h"

extern INT32 hardware_type;

enum { PCE_HW = 0, TG16_HW, SGX_HW, CD_HW, ACARD_HW };

void CDSubsystemTick();

void CDSubsystemRegsWrite(UINT32 address, UINT8 data);
int CDSubsystemMiscWrite(UINT32 address, UINT8 data);

UINT8 CDSubsystemRegsRead(UINT32 address);
UINT8 CDSubsystemMiscRead(UINT32 address);

void CDSubsystemMemIndex(UINT8 *&Next);
void CDSubsystemReset();
void CDSubsystemInit();
void CDSubsystemExit();
void CDSubsystemSoundUpdate(INT16 *output, INT32 samples_len);
void CDSubsystemScan(INT32 nAction, INT32 *pnMin);

#define HAS_CD	(hardware_type == CD_HW || hardware_type == ACARD_HW)
