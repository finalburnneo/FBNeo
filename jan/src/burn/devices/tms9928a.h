
#define TMS9928A_PALETTE_SIZE           16

typedef enum
{
	TMS_INVALID_MODEL,
	TMS99x8,
	TMS9929,
	TMS99x8A,
	TMS9929A
} tms9928a_model;

void TMS9928AInit(INT32 model, INT32 vram, INT32 borderx, INT32 bordery, void (*INTCallback)(INT32));
void TMS9928AReset();
void TMS9928AExit();
void TMS9928AScanline(INT32 scanline);
INT32 TMS9928ADraw();

void TMS9928ASetSpriteslimit(INT32 limit);

void TMS9928AWriteRegs(INT32 data);
UINT8 TMS9928AReadRegs();

void TMS9928AWriteVRAM(INT32 data);
UINT8 TMS9928AReadVRAM();

INT32 TMS9928AScan(INT32 nAction, INT32 *pnMin);
