#include "upd7810.h"

void upd7810SetReadPortHandler(UINT8 (*read_port)(UINT8));
void upd7810SetWritePortHandler(void (*write_port)(UINT8,UINT8));
void upd7810SetReadHandler(UINT8 (*read)(UINT16));
void upd7810SetWriteHandler(void (*write)(UINT16,UINT8));
void upd7810MapMemory(UINT8 *rom, UINT16 start, UINT16 end, UINT8 map);

void upd7810Init(INT32 (*io_callback)(INT32 ioline, INT32 state));
void upd7807Init(INT32 (*io_callback)(INT32 ioline, INT32 state));

void upd7810Reset();
INT32 upd7810Run(INT32 cycles);
void upd7810Exit();

void upd7810SetIRQLine(INT32 line, INT32 state);

INT32 upd7810TotalCycles();
void upd7810NewFrame();
void upd7810RunEnd();

INT32 upd7810Scan(INT32 nAction);
