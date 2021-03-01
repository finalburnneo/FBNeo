// Functions for recording & replaying input
// Stub version for SDL/Pi
#include "burner.h"

INT32  nReplayStatus = 0; // 1 record, 2 replay, 0 nothing
INT32  nReplayUndoCount = 0;
UINT32 nReplayCurrentFrame = 0;
UINT32 nStartFrame = 0;

INT32 FreezeInput(UINT8** buf, INT32* size)
{
	return 0;
}

INT32 UnfreezeInput(const UINT8* buf, INT32 size)
{
	return 0;
}
