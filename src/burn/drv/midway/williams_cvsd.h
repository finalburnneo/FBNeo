void williams_cvsd_write(UINT16 data);
void williams_cvsd_reset_write(UINT16 state);
void williams_cvsd_reset();
void williams_cvsd_init(UINT8 *prgrom, INT32 prot_start, INT32 prot_end, INT32 small);
void williams_cvsd_exit();
INT32 williams_cvsd_in_reset();
void williams_cvsd_update(INT16 *stream, INT32 length);
INT32 williams_cvsd_scan(INT32 nAction, INT32 *pnMin);
