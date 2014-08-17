
#define TMS9928A_PALETTE_SIZE           16

typedef enum
{
	TMS_INVALID_MODEL,
	TMS99x8,
	TMS9929,
	TMS99x8A,
	TMS9929A
} tms9928a_model;


int  TMS9928AInterrupt();
void TMS9928AInit(int model, int vram, int borderx, int bordery, void (*INTCallback)(int));
void TMS9928AReset();
void TMS9928AExit();
int  TMS9928ADraw();

void TMS9928ASetSpriteslimit(int limit);

void TMS9928AWriteRegs(int data);
unsigned char TMS9928AReadRegs();

void TMS9928AWriteVRAM(int data);
unsigned char TMS9928AReadVRAM();

int TMS9928AScan(int nAction, int *pnMin);
