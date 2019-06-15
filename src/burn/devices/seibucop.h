void seibu_cop_write(UINT16 offset, UINT16 data);
UINT16 seibu_cop_read(UINT16 offset);
void seibu_cop_reset();
void seibu_cop_config(INT32 is_68k, void (*vidram_write)(INT32,UINT16,UINT16), void (*pal_write)(INT32,UINT16));
INT32 seibu_cop_scan(INT32 nAction, INT32 *);
