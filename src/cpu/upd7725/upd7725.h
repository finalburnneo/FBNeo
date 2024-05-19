void  upd96050Init(INT32 type, UINT8 *opcode, UINT8 *data, UINT8 *ram, void (*p0_cb)(INT32), void (*p1_cb)(INT32));
void  upd96050Reset();
INT32 upd96050Run(INT32 cycles);
INT32 upd96050Idle(INT32 cycles);
void  upd96050RunEnd();
INT32 upd96050TotalCycles();
void  upd96050NewFrame();

UINT8 snesdsp_read(bool mode);
void  snesdsp_write(bool mode, UINT8 data);
INT32 upd96050Scan(INT32 /*nAction*/);
INT32 upd96050ExportRegs(void **data);
