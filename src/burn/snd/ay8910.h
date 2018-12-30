#ifndef DRIVER_H
#include "driver.h"
#endif

#ifndef AY8910_CORE
extern "C" {
#endif

#ifndef AY8910_H
#define AY8910_H

#define MAX_8910 5
#define ALL_8910_CHANNELS -1

struct AY8910interface
{
	INT32 num;	/* total number of 8910 in the machine */
	INT32 baseclock;
	INT32 mixing_level[MAX_8910];
	read8_handler portAread[MAX_8910];
	read8_handler portBread[MAX_8910];
	write8_handler portAwrite[MAX_8910];
	write8_handler portBwrite[MAX_8910];
	void (*handler[MAX_8910])(INT32 irq);	/* IRQ handler for the YM2203 */
};

extern INT16 *pAY8910Buffer[(MAX_8910 + 1) * 3];

extern INT32 ay8910burgertime_mode;

extern INT32 ay8910_index_ym;

void AY8910_set_clock(INT32 chip, INT32 clock);

void AY8910Write(INT32 chip, INT32 a, INT32 data);
INT32 AY8910Read(INT32 chip);

void AY8910Reset(INT32 chip);
void AY8910Exit(INT32 chip);

// add signal ignored for any chip but #0
INT32 AY8910Init(INT32 chip, INT32 clock, INT32 add_signal);

INT32 AY8910InitYM(INT32 chip, INT32 clock, INT32 sample_rate,
		read8_handler portAread, read8_handler portBread,
		write8_handler portAwrite, write8_handler portBwrite,
		void (*update_callback)(void));

void AY8910Scan(INT32 nAction, INT32* pnMin);

INT32 AY8910SetPorts(INT32 chip, read8_handler portAread, read8_handler portBread,write8_handler portAwrite, write8_handler portBwrite);

void AY8910Render(INT16* dest, INT32 length); // render everything
void AY8910Update(INT32 chip, INT16** buffer, INT32 length); // render a single chip to buffer
void AY8910RenderInternal(INT32 length); // render everything to internal buffers (pAY8910Buffer[])

void AY8910SetRoute(INT32 chip, INT32 nIndex, double nVolume, INT32 nRouteDir);

#define BURN_SND_AY8910_ROUTE_1		0
#define BURN_SND_AY8910_ROUTE_2		1
#define BURN_SND_AY8910_ROUTE_3		2

#define AY8910SetAllRoutes(i, v, d)	do {						\
	AY8910SetRoute(i, BURN_SND_AY8910_ROUTE_1, v, d);	\
	AY8910SetRoute(i, BURN_SND_AY8910_ROUTE_2, v, d);	\
	AY8910SetRoute(i, BURN_SND_AY8910_ROUTE_3, v, d);   \
    } while (0)
#endif

#ifndef AY8910_CORE
} // extern "C"
#endif
