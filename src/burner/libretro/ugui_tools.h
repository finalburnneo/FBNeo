#ifndef UGUI_TOOLS_H_
#define UGUI_TOOLS_H_

#ifdef __cplusplus
extern "C"
{
#endif

/* bpp = bytes per pixel */
void gui_init(int width, int height, int bpp);

void gui_draw(void);

void gui_set_window_title(const char *title);

void gui_set_message(const char *message);

void gui_window_resize(int x, int y, int width, int height);

unsigned* gui_get_framebuffer(void);

#ifdef __cplusplus
}
#endif

#endif
