
// video
void vdc_reset();
void vdc_write(INT32 which, UINT8 offset, UINT8 data);
UINT8 vdc_read(INT32 which, UINT8 offset);
extern UINT8 *vdc_vidram[2];
extern UINT16 *vdc_tmp_draw;

void pce_interrupt(); // update scanline...
void sgx_interrupt();

void pce_hblank();
void sgx_hblank(); // update hblank...


void vdc_get_dimensions(INT32 which, INT32 *x, INT32 *y); // get resolution

void sgx_vdc_write(UINT8 offset, UINT8 data);
UINT8 sgx_vdc_read(UINT8 offset);

void vdc_init();
void vdc_exit();

void vdc_check_hblank_raster_irq(INT32 which);
//INT32 vdc_vdw_start(INT32 which);
//INT32 vdc_vdw_length(INT32 which);


// priority
void vpc_reset();
void vpc_write(UINT8 offset, UINT8 data);
UINT8 vpc_read(UINT8 offset);

// palette
#define PCE_LINE 1368
UINT16 *vdc_get_line(UINT32 y);
INT32 vce_linecount();
void vce_reset();
void vce_palette_init(UINT32 *Palette, UINT32 *Alt_Palette);
void vce_write(UINT8 offset, UINT8 data);
UINT8 vce_read(UINT8 offset);
extern UINT16 *vce_data;

INT32 vdc_scan(INT32 nAction, INT32 *pnMin);
