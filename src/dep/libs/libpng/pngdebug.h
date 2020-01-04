#ifndef PNGDEBUG_H
#define PNGDEBUG_H
/* These settings control the formatting of messages in png.c and pngerror.c */
/* Moved to pngdebug.h at 1.5.0 */
#  ifndef PNG_LITERAL_SHARP
#    define PNG_LITERAL_SHARP 0x23
#  endif
#  ifndef PNG_LITERAL_LEFT_SQUARE_BRACKET
#    define PNG_LITERAL_LEFT_SQUARE_BRACKET 0x5b
#  endif
#  ifndef PNG_LITERAL_RIGHT_SQUARE_BRACKET
#    define PNG_LITERAL_RIGHT_SQUARE_BRACKET 0x5d
#  endif
#  ifndef PNG_STRING_NEWLINE
#    define PNG_STRING_NEWLINE "\n"
#  endif

#ifdef PNG_DEBUG
#  if (PNG_DEBUG > 0)
#    if !defined(PNG_DEBUG_FILE) && defined(_MSC_VER)
#      include <crtdbg.h>
#      if (PNG_DEBUG > 1)
#        ifndef _DEBUG
#          define _DEBUG
#        endif
#        ifndef png_debug
#          define png_debug(l,m)  _RPT0(_CRT_WARN,m PNG_STRING_NEWLINE)
#        endif
#        ifndef png_debug1
#          define png_debug1(l,m,p1)  _RPT1(_CRT_WARN,m PNG_STRING_NEWLINE,p1)
#        endif
#        ifndef png_debug2
#          define png_debug2(l,m,p1,p2) \
             _RPT2(_CRT_WARN,m PNG_STRING_NEWLINE,p1,p2)
#        endif
#      endif
#    else /* PNG_DEBUG_FILE || !_MSC_VER */
#      ifndef PNG_STDIO_SUPPORTED
#        include <stdio.h> /* not included yet */
#      endif
#      ifndef PNG_DEBUG_FILE
#        define PNG_DEBUG_FILE stderr
#      endif /* PNG_DEBUG_FILE */

#      if (PNG_DEBUG > 1)
#        ifdef __STDC__
#          ifndef png_debug
#            define png_debug(l,m) \
       do { \
       int num_tabs=l; \
       fprintf(PNG_DEBUG_FILE,"%s" m PNG_STRING_NEWLINE,(num_tabs==1 ? "   " : \
         (num_tabs==2 ? "      " : (num_tabs>2 ? "         " : "")))); \
       } while (0)
#          endif
#          ifndef png_debug1
#            define png_debug1(l,m,p1) \
       do { \
       int num_tabs=l; \
       fprintf(PNG_DEBUG_FILE,"%s" m PNG_STRING_NEWLINE,(num_tabs==1 ? "   " : \
         (num_tabs==2 ? "      " : (num_tabs>2 ? "         " : ""))),p1); \
       } while (0)
#          endif
#          ifndef png_debug2
#            define png_debug2(l,m,p1,p2) \
       do { \
       int num_tabs=l; \
       fprintf(PNG_DEBUG_FILE,"%s" m PNG_STRING_NEWLINE,(num_tabs==1 ? "   " : \
         (num_tabs==2 ? "      " : (num_tabs>2 ? "         " : ""))),p1,p2);\
       } while (0)
#          endif
#        else /* __STDC __ */
#          ifndef png_debug
#            define png_debug(l,m) \
       do { \
       int num_tabs=l; \
       char format[256]; \
       snprintf(format,256,"%s%s%s",(num_tabs==1 ? "\t" : \
         (num_tabs==2 ? "\t\t":(num_tabs>2 ? "\t\t\t":""))), \
         m,PNG_STRING_NEWLINE); \
       fprintf(PNG_DEBUG_FILE,format); \
       } while (0)
#          endif
#          ifndef png_debug1
#            define png_debug1(l,m,p1) \
       do { \
       int num_tabs=l; \
       char format[256]; \
       snprintf(format,256,"%s%s%s",(num_tabs==1 ? "\t" : \
         (num_tabs==2 ? "\t\t":(num_tabs>2 ? "\t\t\t":""))), \
         m,PNG_STRING_NEWLINE); \
       fprintf(PNG_DEBUG_FILE,format,p1); \
       } while (0)
#          endif
#          ifndef png_debug2
#            define png_debug2(l,m,p1,p2) \
       do { \
       int num_tabs=l; \
       char format[256]; \
       snprintf(format,256,"%s%s%s",(num_tabs==1 ? "\t" : \
         (num_tabs==2 ? "\t\t":(num_tabs>2 ? "\t\t\t":""))), \
         m,PNG_STRING_NEWLINE); \
       fprintf(PNG_DEBUG_FILE,format,p1,p2); \
       } while (0)
#          endif
#        endif /* __STDC __ */
#      endif /* (PNG_DEBUG > 1) */

#    endif /* _MSC_VER */
#  endif /* (PNG_DEBUG > 0) */
#endif /* PNG_DEBUG */
#ifndef png_debug
#  define png_debug(l, m) ((void)0)
#endif
#ifndef png_debug1
#  define png_debug1(l, m, p1) ((void)0)
#endif
#ifndef png_debug2
#  define png_debug2(l, m, p1, p2) ((void)0)
#endif
#endif /* PNGDEBUG_H */
