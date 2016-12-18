
#define SHIFT_COLOR_RED			0xff0000
#define SHIFT_COLOR_GREEN			0x00ff00
#define SHIFT_COLOR_BLUE			0x0000ff
#define SHIFT_COLOR_WHITE			0xffffff
#define SHIFT_COLOR_YELLOW		0xffff00

#define SHIFT_POSITION_TOP_LEFT		0
#define SHIFT_POSITION_TOP_RIGHT		1
#define SHIFT_POSITION_BOTTOM_LEFT	2
#define SHIFT_POSITION_BOTTOM_RIGHT	3
#define SHIFT_POSITION_ROTATE_CCW     0x10

// transparency is a percentage 0 - 100
void BurnShiftInit(INT32 position, INT32 color, INT32 transparency);

void BurnShiftReset();
void BurnShiftSetFlipscreen(INT32 flip);
void BurnShiftRender();
void BurnShiftSetStatus(UINT32 status);
void BurnShiftExit();

INT32 BurnShiftScan(INT32 nAction, INT32 *pnMin);

extern INT32 BurnShiftEnabled;
