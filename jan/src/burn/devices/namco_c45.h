extern UINT8 *c45RoadRAM; // external

void c45RoadReset();
void c45RoadInit(UINT32 trans_color, UINT8 *clut);
void c45RoadExit();
void c45RoadDraw();
void c45RoadState(INT32 nAction);

void c45RoadMap68k(UINT32 address);
