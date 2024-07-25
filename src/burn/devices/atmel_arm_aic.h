
#define COMBINE_DATA(varptr, mem_mask)    (*(varptr) = (*(varptr) & ~mem_mask) | (data & mem_mask))

UINT32 ArmAicRead(UINT32 sekAddress);
void ArmAicWrite(UINT32 sekAddress, UINT32 data);
void ArmAicSetIrq(INT32 line, INT32 state);
void ArmAicReset();
INT32 ArmAicScan();
void ArmAicSetIrqCallback(void (*irqcallback)(INT32));
