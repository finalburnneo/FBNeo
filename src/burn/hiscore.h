extern INT32 EnableHiscores;

void HiscoreInit();
void HiscoreReset(INT32 bDisableInversionWriteback = 0);
void HiscoreApply();
void HiscoreScan(INT32 nAction, INT32* pnMin);
void HiscoreExit();
