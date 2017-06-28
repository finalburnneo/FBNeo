/**********************************************************************************************
 *
 *   Ensoniq ES5505/6 driver
 *   by Aaron Giles
 *
 **********************************************************************************************/

#ifndef __ES5506_H__
#define __ES5506_H__

typedef void (*irq_callback)(INT32 param);
typedef UINT16(port_read)();

typedef struct _es5505_interface es5505_interface;
struct _es5505_interface
{
	const INT8 * region0;						/* memory region where the sample ROM lives */
	const INT8 * region1;						/* memory region where the sample ROM lives */
	void (*irq_callback)(INT32 state);	/* irq callback */
	UINT16 (*read_port)();			/* input port read */
};

typedef struct _es5506_interface es5506_interface;
struct _es5506_interface
{
	const INT8 * region0;						/* memory region where the sample ROM lives */
	const INT8 * region1;						/* memory region where the sample ROM lives */
	const INT8 * region2;						/* memory region where the sample ROM lives */
	const INT8 * region3;						/* memory region where the sample ROM lives */
	void (*irq_callback)(INT32 state);	/* irq callback */
	UINT16 (*read_port)();			/* input port read */
};

extern INT32 ES550X_twincobra2_pan_fix;

void ES5506Update(INT16 *pBuffer, INT32 samples);
#define ES5505Update ES5506Update
void ES5506Init(INT32 clock, UINT8 *region0, UINT8 *region1, UINT8 *region2, UINT8 *region3, irq_callback callback);
void ES5506Exit();
void ES5506Reset();
void ES5506Write(UINT32 offset, UINT8 data);
UINT8 ES5506Read(UINT32 offset);
void es5506_voice_bank_w(INT32 voice, INT32 bank);
void ES5505Init(INT32 clock, UINT8 *region0, UINT8* region1, irq_callback callback);
#define ES5505Reset	ES5506Reset
#define ES5505Exit	ES5506Exit
void ES5505Write(UINT32 offset, UINT16 data);
UINT16 ES5505Read(UINT32 offset);
void es5505_voice_bank_w(INT32 voice, INT32 bank);
void ES5506Scan(INT32 nAction, INT32* pnMin);
void ES5506ScanRoutes(INT32 nAction, INT32* pnMin);

void ES5506SetRoute(INT32 chip /*always 0*/, double nVolume, INT32 nRoute);
#define BURN_SND_ES5506_ROUTE_LEFT	1
#define BURN_SND_ES5506_ROUTE_RIGHT	2
#define BURN_SND_ES5506_ROUTE_BOTH	3

#endif /* __ES5506_H__ */
