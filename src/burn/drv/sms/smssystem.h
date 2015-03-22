
#ifndef _SMSSYSTEM_H_
#define _SMSSYSTEM_H_

#define APP_NAME            "SMS Plus"
#define APP_VERSION         "1.3"

#define PALETTE_SIZE        0x20

/* Mask for removing unused pixel data */
#define PIXEL_MASK          0x1F

/* These can be used for 'input.pad[]' */
#define INPUT_UP            0x00000001
#define INPUT_DOWN          0x00000002
#define INPUT_LEFT          0x00000004
#define INPUT_RIGHT         0x00000008
#define INPUT_BUTTON2       0x00000010
#define INPUT_BUTTON1       0x00000020

/* These can be used for 'input.system' */
#define INPUT_START         0x00000001  /* Game Gear only */    
#define INPUT_PAUSE         0x00000002  /* Master System only */
#define INPUT_RESET         0x00000004  /* Master System only */

enum {
    SRAM_SAVE   = 0,
    SRAM_LOAD   = 1
};


/* User input structure */
typedef struct
{
    UINT8 pad[2];
    UINT8 analog[2];
    UINT32 system;
} input_t;

/* Game image structure */
typedef struct
{
    UINT8 *rom;
    UINT8 pages;
    UINT8 pages8k;
    UINT32 sram_crc;
    INT32 mapper;
    UINT8 sram[0x8000];
    UINT8 fcr[4];
} cart_t;

/* Bitmap structure */
typedef struct
{
    unsigned char *data;
    INT32 width;
    INT32 height;
    INT32 pitch;
    INT32 depth;
    INT32 granularity;
    struct {
        INT32 x, y, w, h;
        INT32 ox, oy, ow, oh;
        INT32 changed;
    } viewport;        
    struct
    {
        UINT8 color[PALETTE_SIZE][3];
        UINT8 dirty[PALETTE_SIZE];
        UINT8 update;
    }pal;
} bitmap_t;

/* Global variables */
extern bitmap_t bitmap;     /* Display bitmap */
extern cart_t cart;         /* Game cartridge data */
extern input_t input;       /* Controller input */

/* Function prototypes */
void system_frame(INT32 skip_render);
void system_init(void);
void system_shutdown(void);
void system_reset(void);
void system_manage_sram(UINT8 *sram, INT32 slot, INT32 mode);
void system_poweron(void);
void system_poweroff(void);

#endif /* _SYSTEM_H_ */


