#include <stdlib.h>
#include <string/stdstring.h>
#include <ugui.h>
#include <stdio.h>

#define UGUI_MAX_OBJECTS 2
static UG_GUI gui;
static UG_WINDOW gui_window;
static UG_TEXTBOX gui_textbox;
static UG_OBJECT gui_objbuf_wnd[UGUI_MAX_OBJECTS];
static unsigned *frame_buf = NULL;
static int width = 0;
static int height = 0;
static char gui_message[4096] = {0};

static void gui_window_callback(UG_MESSAGE *msg)
{
}

unsigned* gui_get_framebuffer(void)
{
   return frame_buf;
}

/* uGUI callback that draws raw pixels onto our frame buffer */
static void UserPixelSetFunction(UG_S16 x, UG_S16 y, UG_COLOR c)
{
   frame_buf[width * y + x] = c;
}

void gui_init(int w, int h, int bpp)
{
   width = w;
   height = h;
   frame_buf = (unsigned*)calloc(width * height, bpp);

   /* init uGUI */
   UG_Init(&gui, UserPixelSetFunction, width, height);
   UG_FontSelect(&FONT_8X8);

   /* create a single window with no buttons */
   UG_WindowCreate(&gui_window, gui_objbuf_wnd, UGUI_MAX_OBJECTS, gui_window_callback);
   UG_WindowSetForeColor(&gui_window, C_BLACK);

   UG_WindowSetXStart(&gui_window, 0);
   UG_WindowSetYStart(&gui_window, 0);
   UG_WindowSetXEnd(&gui_window, width - 1);
   UG_WindowSetYEnd(&gui_window, height - 1);

   UG_TextboxCreate(&gui_window, &gui_textbox, TXB_ID_0, 0, 0, UG_WindowGetInnerWidth(&gui_window) - 1, UG_WindowGetInnerHeight(&gui_window) - 1);
   UG_TextboxSetAlignment(&gui_window, TXB_ID_0, ALIGN_CENTER);

   UG_WindowShow(&gui_window);
}

void gui_set_message(const char *message)
{
   memset(gui_message, 0, sizeof(gui_message));

   snprintf(gui_message, sizeof(gui_message), "%s", message);

   gui_message[sizeof(gui_message) - 1] = '\0';

   UG_TextboxSetText(&gui_window, TXB_ID_0, gui_message);
}

void gui_window_resize(int x, int y, int width, int height)
{
   UG_WindowResize(&gui_window, x, y, width, height);
}

void gui_set_window_title(const char *title)
{
   UG_WindowSetTitleText(&gui_window, (char*)title);
}

void gui_draw(void)
{
   if (!string_is_empty(gui_message))
      UG_TextboxSetText(&gui_window, TXB_ID_0, gui_message);
   UG_Update();
}
