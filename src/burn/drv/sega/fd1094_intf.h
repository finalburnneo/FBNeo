void s24_fd1094_machine_init();

// cpu # 0 or 1
// cachesize is 8 for sys16, 16 for s24
// fd1094 key
// code base (68k rom/ram)
// code base length (68k rom/ram) length
// callback to memory mapper - 0-fffff for sys16, 0-3ffff, mirror 0x40000 for sys24
void s24_fd1094_driver_init(INT32 nCPU, INT32 cachesize, UINT8 *key, UINT8 *codebase, INT32 codebaselen, void (*cb)(UINT8*));
void s24_fd1094_exit();
void s24_fd1094_scan(INT32 nAction);

// pointer to current cache (allocated internally)
extern UINT16* s24_fd1094_userregion;
