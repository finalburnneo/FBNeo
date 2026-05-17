extern INT32 EnableHiscores;

// don't init hiscore memory range with ~0 @ reset.
// in some games, f.ex system16b (aliensyn, everything else) rom is mapped
// on reset where the hiscore data resides.  Writing here @ init is bad, hommes.
#define HI_NOCOMPLIMENTWB	(1 << 0)

// don't confirm on the next frame that the hiscore data written was read back
// successfully.  some entries use a game's timer to know when to write the
// hiscore data.  on the next frame, it'll be a different number so it will
// fail anyways mijo.
#define HI_NOCONFIRM		(1 << 1)

void HiscoreInit();
void HiscoreReset(INT32 nHiscoreOptions = 0);
void HiscoreApply();
void HiscoreScan(INT32 nAction, INT32* pnMin);
void HiscoreExit();
