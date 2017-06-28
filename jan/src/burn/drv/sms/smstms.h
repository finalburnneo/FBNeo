#ifndef _SMSTMS_H_
#define _SMSTMS_H_

extern INT16 text_counter;


void make_tms_tables(void);
void render_bg_tms(INT16 line);
void render_bg_m0(INT16 line);
void render_bg_m1(INT16 line);
void render_bg_m1x(INT16 line);
void render_bg_inv(INT16 line);
void render_bg_m3(INT16 line);
void render_bg_m3x(INT16 line);
void render_bg_m2(INT16 line);
void render_obj_tms(INT16 line);
void parse_line(INT16 line);

#endif /* _TMS_H_ */
