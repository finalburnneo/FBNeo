extern UINT8 *K007342VidRAM[1];
extern UINT8 *K007342ScrRAM[1];
extern UINT8  K007342Regs[1][8];
extern UINT8 *K007420RAM[1];

void K007342Init(UINT8 *gfx, void (*Callback)(INT32 layer, INT32 bank, INT32 *code, INT32 *color, INT32 *flags));
void K007342SetOffsets(INT32 x, INT32 y);
void K007342Reset();
INT32 K007342_irq_enabled();

#define K007342_OPAQUE	(1<<16)

void K007342DrawLayer(INT32 layer, INT32 baseflags, INT32 priority);
INT32 K007342Scan(INT32 nAction);

void K007420Init(INT32 banklimit, void (*Callback)(INT32 *code, INT32 *color));
void K007420SetOffsets(INT32 x, INT32 y);
void K007420DrawSprites(UINT8 *gfxbase);


