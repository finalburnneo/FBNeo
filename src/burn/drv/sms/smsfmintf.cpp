/*
    fmintf.c --
	Interface to EMU2413 and YM2413 emulators.
*/
#include "smsshared.h"
#include "burn_ym2413.h"


void FM_Init(void)
{
	BurnYM2413Init(snd.fm_clock);
	BurnYM2413SetAllRoutes(2.10, BURN_SND_ROUTE_BOTH);
}


void FM_Shutdown(void)
{
	BurnYM2413Exit();
}


void FM_Reset(void)
{
	BurnYM2413Reset();
}


void FM_Write(INT32 offset, INT32 data)
{
	BurnYM2413Write(offset, data);
}

