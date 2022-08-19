INT32 inputbuf_embed(FILE *fp);
void inputbuf_load();
void inputbuf_save();
void inputbuf_exit();
INT32 inputbuf_eof();
void inputbuf_addbuffer(UINT8 c);
UINT8 inputbuf_getbuffer();

INT32 inputbuf_freeze(UINT8 **buf, INT32 *size);
INT32 inputbuf_unfreeze(UINT8 *buf, INT32 size);

