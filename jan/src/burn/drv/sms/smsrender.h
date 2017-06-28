
#ifndef _SMSRENDER_H_
#define _SMSRENDER_H_

/* Pack RGB data into a 16-bit RGB 5:6:5 format */
#define MAKE_PIXEL(r,g,b)   (((r << 8) & 0xF800) | ((g << 3) & 0x07E0) | ((b >> 3) & 0x001F))

/* Used for blanking a line in whole or in part */
#define BACKDROP_COLOR      (0x10 | (vdp.reg[7] & 0x0F))

extern UINT8 sms_cram_expand_table[4];
extern UINT8 gg_cram_expand_table[16];
extern void (*render_bg)(INT16 line);
extern void (*render_obj)(INT16 line);
extern UINT8 *linebuf;
extern UINT8 internal_buffer[0x200];
extern UINT16 pixel[];
extern UINT8 bg_name_dirty[0x200];     
extern UINT16 bg_name_list[0x200];     
extern UINT16 bg_list_index;           
extern UINT8 bg_pattern_cache[0x20000];
extern UINT8 tms_lookup[16][256][2];
extern UINT8 mc_lookup[16][256][8];
extern UINT8 txt_lookup[256][2];
extern UINT8 bp_expand[256][8];
extern UINT8 lut[0x10000];
extern UINT32 bp_lut[0x10000];
extern UINT32 gg_overscanmode;

void render_shutdown(void);
void render_init(void);
void render_reset(void);
void render_line(INT16 line);
void render_bg_sms(INT16 line);
void render_obj_sms(INT16 line);
void update_bg_pattern_cache(void);
void palette_sync(INT16 index, INT16 force);
void remap_8_to_16(INT16 line, INT16 extend);

#endif /* _RENDER_H_ */
