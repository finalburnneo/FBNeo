// license:???
// copyright-holders:Alex Pasadyn, Zsolt Vasvari, Kurt Mahan, Ernesto Corvi, Aaron Giles
/*************************************************************************

    Driver for Midway T-unit games.

**************************************************************************/
// Adapted to FBAlpha
#define XPOSMASK        0x3ff
#define YPOSMASK        0x1ff

enum
{
    DMA_LRSKIP = 0,
    DMA_COMMAND,
    DMA_OFFSETLO,
    DMA_OFFSETHI,
    DMA_XSTART,
    DMA_YSTART,
    DMA_WIDTH,
    DMA_HEIGHT,
    DMA_PALETTE,
    DMA_COLOR,
    DMA_SCALE_X,
    DMA_SCALE_Y,
    DMA_TOPCLIP,
    DMA_BOTCLIP,
    DMA_UNKNOWN_E,  /* MK1/2 never write here; NBA only writes 0 */
    DMA_CONFIG,
    DMA_LEFTCLIP,   /* pseudo-register */
    DMA_RIGHTCLIP   /* pseudo-register */
};


static struct
{
    UINT8 *     gfxrom;

    UINT32      offset;         /* source offset, in bits */
    INT32       rowbits;        /* source bits to skip each row */
    INT32       xpos;           /* x position, clipped */
    INT32       ypos;           /* y position, clipped */
    INT32       width;          /* horizontal pixel count */
    INT32       height;         /* vertical pixel count */
    UINT16      palette;        /* palette base */
    UINT16      color;          /* current foreground color with palette */

    UINT8       yflip;          /* yflip? */
    UINT8       bpp;            /* bits per pixel */
    UINT8       preskip;        /* preskip scale */
    UINT8       postskip;       /* postskip scale */
    INT32       topclip;        /* top clipping scanline */
    INT32       botclip;        /* bottom clipping scanline */
    INT32       leftclip;       /* left clipping column */
    INT32       rightclip;      /* right clipping column */
    INT32       startskip;      /* pixels to skip at start */
    INT32       endskip;        /* pixels to skip at end */
    UINT16      xstep;          /* 8.8 fixed number scale x factor */
    UINT16      ystep;          /* 8.8 fixed number scale y factor */
} dma_state;

/*** constant definitions ***/
#define PIXEL_SKIP      0
#define PIXEL_COLOR     1
#define PIXEL_COPY      2

#define XFLIP_NO        0
#define XFLIP_YES       1

#define SKIP_NO         0
#define SKIP_YES        1

#define SCALE_NO        0
#define SCALE_YES       1

#define XPOSMASK        0x3ff
#define YPOSMASK        0x1ff


typedef void (*dma_draw_func)(void);


/*** fast pixel extractors ***/
#if 1//!defined(ALIGN_SHORTS) && defined(LSB_FIRST)
#define EXTRACTGEN(m)   ((*(UINT16 *)&base[o >> 3] >> (o & 7)) & (m))
#elif defined(powerc)
#define EXTRACTGEN(m)   ((__lhbrx(base, o >> 3) >> (o & 7)) & (m))
#else
#define EXTRACTGEN(m)   (((base[o >> 3] | (base[(o >> 3) + 1] << 8)) >> (o & 7)) & (m))
#endif

/*** core blitter routine macro ***/
#define DMA_DRAW_FUNC_BODY(name, bitsperpixel, extractor, xflip, skip, scale, zero, nonzero)    \
{                                                             \
    int height = dma_state.height << 8;                       \
    UINT8 *base = dma_state.gfxrom;                           \
    UINT32 offset = dma_state.offset;                         \
    UINT16 pal = dma_state.palette;                           \
    UINT16 color = pal | dma_state.color;                     \
    int sy = dma_state.ypos, iy = 0, ty;                      \
    int bpp = bitsperpixel;                                   \
    int mask = (1 << bpp) - 1;                                \
    int xstep = scale ? dma_state.xstep : 0x100;              \
                                                              \
    /* loop over the height */                                \
    while (iy < height)                                       \
    {                                                         \
        int startskip = dma_state.startskip << 8;             \
        int endskip = dma_state.endskip << 8;                 \
        int width = dma_state.width << 8;                     \
        int sx = dma_state.xpos, ix = 0, tx;                  \
        UINT32 o = offset;                                    \
        int pre, post;                                        \
        UINT16 *d;                                            \
                                                              \
        /* handle skipping */                                 \
        if (skip)                                             \
        {                                                     \
            UINT8 value = EXTRACTGEN(0xff);                   \
            o += 8;                                           \
                                                              \
            /* adjust for preskip */                          \
            pre = (value & 0x0f) << (dma_state.preskip + 8);  \
            tx = pre / xstep;                                 \
            if (xflip)                                        \
                sx = (sx - tx) & XPOSMASK;                    \
            else                                              \
                sx = (sx + tx) & XPOSMASK;                    \
            ix += tx * xstep;                                 \
                                                              \
            /* adjust for postskip */                         \
            post = ((value >> 4) & 0x0f) << (dma_state.postskip + 8); \
            width -= post;                                    \
            endskip -= post;                                  \
        }                                                     \
                                                              \
        /* handle Y clipping */                               \
        if (sy < dma_state.topclip || sy > dma_state.botclip) \
            goto clipy;                                       \
                                                              \
        /* handle start skip */                               \
        if (ix < startskip)                                   \
        {                                                     \
            tx = ((startskip - ix) / xstep) * xstep;          \
            ix += tx;                                         \
            o += (tx >> 8) * bpp;                             \
        }                                                     \
                                                              \
        /* handle end skip */                                 \
        if ((width >> 8) > dma_state.width - dma_state.endskip) \
            width = (dma_state.width - dma_state.endskip) << 8; \
                                                              \
        /* determine destination pointer */                   \
        d = &DrvVRAM16[sy * 512];                             \
                                                              \
        /* loop until we draw the entire width */             \
        while (ix < width)                                    \
        {                                                     \
            /* only process if not clipped */                 \
            if (sx >= dma_state.leftclip && sx <= dma_state.rightclip) \
            {                                                 \
                /* special case similar handling of zero/non-zero */ \
                if (zero == nonzero)                          \
                {                                             \
                    if (zero == PIXEL_COLOR)                  \
                        d[sx] = color;                        \
                    else if (zero == PIXEL_COPY)              \
                        d[sx] = (extractor(mask)) | pal;      \
                }                                             \
                                                              \
                /* otherwise, read the pixel and look */      \
                else                                          \
                {                                             \
                    int pixel = (extractor(mask));            \
                                                              \
                    /* non-zero pixel case */                 \
                    if (pixel)                                \
                    {                                         \
                        if (nonzero == PIXEL_COLOR)           \
                            d[sx] = color;                    \
                        else if (nonzero == PIXEL_COPY)       \
                            d[sx] = pixel | pal;              \
                    }                                         \
                                                              \
                    /* zero pixel case */                     \
                    else                                      \
                    {                                         \
                        if (zero == PIXEL_COLOR)              \
                            d[sx] = color;                    \
                        else if (zero == PIXEL_COPY)          \
                            d[sx] = pal;                      \
                    }                                         \
                }                                             \
            }                                                 \
                                                              \
            /* update pointers */                             \
            if (xflip)                                        \
                sx = (sx - 1) & XPOSMASK;                     \
            else                                              \
                sx = (sx + 1) & XPOSMASK;                     \
                                                              \
            /* advance to the next pixel */                   \
            if (!scale)                                       \
            {                                                 \
                ix += 0x100;                                  \
                o += bpp;                                     \
            }                                                 \
            else                                              \
            {                                                 \
                tx = ix >> 8;                                 \
                ix += xstep;                                  \
                tx = (ix >> 8) - tx;                          \
                o += bpp * tx;                                \
            }                                                 \
        }                                                     \
                                                              \
clipy:                                                        \
        /* advance to the next row */                         \
        if (dma_state.yflip)                                  \
            sy = (sy - 1) & YPOSMASK;                         \
        else                                                  \
            sy = (sy + 1) & YPOSMASK;                         \
        if (!scale)                                           \
        {                                                     \
            iy += 0x100;                                      \
            width = dma_state.width;                          \
            if (skip)                                         \
            {                                                 \
                offset += 8;                                  \
                width -= (pre + post) >> 8;                   \
                if (width > 0) offset += width * bpp;         \
            }                                                 \
            else                                              \
                offset += width * bpp;                        \
        }                                                     \
        else                                                  \
        {                                                     \
            ty = iy >> 8;                                     \
            iy += dma_state.ystep;                            \
            ty = (iy >> 8) - ty;                              \
            if (!skip)                                        \
                offset += ty * dma_state.width * bpp;         \
            else if (ty--)                                    \
            {                                                 \
                o = offset + 8;                               \
                width = dma_state.width - ((pre + post) >> 8);\
                if (width > 0) o += width * bpp;              \
                while (ty--)                                  \
                {                                             \
                    UINT8 value = EXTRACTGEN(0xff);           \
                    o += 8;                                   \
                    pre = (value & 0x0f) << dma_state.preskip;\
                    post = ((value >> 4) & 0x0f) << dma_state.postskip; \
                    width = dma_state.width - pre - post;     \
                    if (width > 0) o += width * bpp;          \
                }                                             \
                offset = o;                                   \
            }                                                 \
        }                                                     \
    }                                                         \
}


/*** slightly simplified one for most blitters ***/
#define DMA_DRAW_FUNC(name, bpp, extract, xflip, skip, scale, zero, nonzero)    \
    static void name(void)                                                      \
{                                                                               \
    DMA_DRAW_FUNC_BODY(name, bpp, extract, xflip, skip, scale, zero, nonzero)   \
}

/*** empty blitter ***/
static void dma_draw_none(void)
{
}

/*** super macro for declaring an entire blitter family ***/
#define DECLARE_BLITTER_SET(prefix, bpp, extract, skip, scale)                                         \
    DMA_DRAW_FUNC(prefix##_p0,      bpp, extract, XFLIP_NO,  skip, scale, PIXEL_COPY,  PIXEL_SKIP)      \
    DMA_DRAW_FUNC(prefix##_p1,      bpp, extract, XFLIP_NO,  skip, scale, PIXEL_SKIP,  PIXEL_COPY)      \
    DMA_DRAW_FUNC(prefix##_c0,      bpp, extract, XFLIP_NO,  skip, scale, PIXEL_COLOR, PIXEL_SKIP)      \
    DMA_DRAW_FUNC(prefix##_c1,      bpp, extract, XFLIP_NO,  skip, scale, PIXEL_SKIP,  PIXEL_COLOR)     \
    DMA_DRAW_FUNC(prefix##_p0p1,    bpp, extract, XFLIP_NO,  skip, scale, PIXEL_COPY,  PIXEL_COPY)      \
    DMA_DRAW_FUNC(prefix##_c0c1,    bpp, extract, XFLIP_NO,  skip, scale, PIXEL_COLOR, PIXEL_COLOR)     \
    DMA_DRAW_FUNC(prefix##_c0p1,    bpp, extract, XFLIP_NO,  skip, scale, PIXEL_COLOR, PIXEL_COPY)      \
    DMA_DRAW_FUNC(prefix##_p0c1,    bpp, extract, XFLIP_NO,  skip, scale, PIXEL_COPY,  PIXEL_COLOR)     \
    \
    DMA_DRAW_FUNC(prefix##_p0_xf,   bpp, extract, XFLIP_YES, skip, scale, PIXEL_COPY,  PIXEL_SKIP)      \
    DMA_DRAW_FUNC(prefix##_p1_xf,   bpp, extract, XFLIP_YES, skip, scale, PIXEL_SKIP,  PIXEL_COPY)      \
    DMA_DRAW_FUNC(prefix##_c0_xf,   bpp, extract, XFLIP_YES, skip, scale, PIXEL_COLOR, PIXEL_SKIP)      \
    DMA_DRAW_FUNC(prefix##_c1_xf,   bpp, extract, XFLIP_YES, skip, scale, PIXEL_SKIP,  PIXEL_COLOR)     \
    DMA_DRAW_FUNC(prefix##_p0p1_xf, bpp, extract, XFLIP_YES, skip, scale, PIXEL_COPY,  PIXEL_COPY)      \
    DMA_DRAW_FUNC(prefix##_c0c1_xf, bpp, extract, XFLIP_YES, skip, scale, PIXEL_COLOR, PIXEL_COLOR)     \
    DMA_DRAW_FUNC(prefix##_c0p1_xf, bpp, extract, XFLIP_YES, skip, scale, PIXEL_COLOR, PIXEL_COPY)      \
    DMA_DRAW_FUNC(prefix##_p0c1_xf, bpp, extract, XFLIP_YES, skip, scale, PIXEL_COPY,  PIXEL_COLOR)     \
    \
    static const dma_draw_func prefix[32] =                                                                 \
    {                                                                                                       \
    /*  B0:N / B1:N         B0:Y / B1:N         B0:N / B1:Y         B0:Y / B1:Y */                          \
    dma_draw_none,      prefix##_p0,        prefix##_p1,        prefix##_p0p1,      /* no color */          \
    prefix##_c0,        prefix##_c0,        prefix##_c0p1,      prefix##_c0p1,      /* color 0 pixels */    \
    prefix##_c1,        prefix##_p0c1,      prefix##_c1,        prefix##_p0c1,      /* color non-0 pixels */\
    prefix##_c0c1,      prefix##_c0c1,      prefix##_c0c1,      prefix##_c0c1,      /* fill */              \
    \
    dma_draw_none,      prefix##_p0_xf,     prefix##_p1_xf,     prefix##_p0p1_xf,   /* no color */          \
    prefix##_c0_xf,     prefix##_c0_xf,     prefix##_c0p1_xf,   prefix##_c0p1_xf,   /* color 0 pixels */    \
    prefix##_c1_xf,     prefix##_p0c1_xf,   prefix##_c1_xf,     prefix##_p0c1_xf,   /* color non-0 pixels */\
    prefix##_c0c1_xf,   prefix##_c0c1_xf,   prefix##_c0c1_xf,   prefix##_c0c1_xf    /* fill */              \
    };


/*** blitter family declarations ***/
DECLARE_BLITTER_SET(dma_draw_skip_scale,       dma_state.bpp, EXTRACTGEN,   SKIP_YES, SCALE_YES)
DECLARE_BLITTER_SET(dma_draw_noskip_scale,     dma_state.bpp, EXTRACTGEN,   SKIP_NO,  SCALE_YES)
DECLARE_BLITTER_SET(dma_draw_skip_noscale,     dma_state.bpp, EXTRACTGEN,   SKIP_YES, SCALE_NO)
DECLARE_BLITTER_SET(dma_draw_noskip_noscale,   dma_state.bpp, EXTRACTGEN,   SKIP_NO,  SCALE_NO)


static UINT16 TUnitDmaRead(UINT32 address)
{
    UINT32 offset = (address >> 4) & 0xF;
    if (offset == 0)
        offset = 1;
    return nDMA[offset];
}

#define DMA_IRQ     TMS34010_INT_EX1
static void TUnitDmaWrite(UINT32 address, UINT16 value)
{
    dma_state.gfxrom = DrvGfxROM;
    static const UINT8 register_map[2][16] =
    {
        { 0,1,2,3,4,5,6,7,8,9,10,11,16,17,14,15 },
        { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 }
    };
    int regbank = (nDMA[DMA_CONFIG] >> 5) & 1;
    int reg = register_map[regbank][(address >> 4) & 0xF];
    int command, bpp, regnum;
    UINT32 gfxoffset;
    int pixels = 0;
  
    nDMA[reg] = value;

    if (reg != DMA_COMMAND)
        return;
    command = nDMA[DMA_COMMAND];
    regnum = reg;

    TMS34010ClearIRQ(DMA_IRQ);
    if (!(value & 0x8000)) {
        return;
    }
    bpp = (command >> 12) & 7;

    /* fill in the basic data */
    dma_state.xpos = nDMA[DMA_XSTART] & XPOSMASK;
    dma_state.ypos = nDMA[DMA_YSTART] & YPOSMASK;
    dma_state.width = nDMA[DMA_WIDTH] & 0x3ff;
    dma_state.height = nDMA[DMA_HEIGHT] & 0x3ff;
    dma_state.palette = nDMA[DMA_PALETTE] & 0x7f00;
    dma_state.color = nDMA[DMA_COLOR] & 0xff;

    /* fill in the rev 2 data */
    dma_state.yflip = (command & 0x20) >> 5;
    dma_state.bpp = bpp ? bpp : 8;
    dma_state.preskip = (command >> 8) & 3;
    dma_state.postskip = (command >> 10) & 3;
    dma_state.xstep = nDMA[DMA_SCALE_X] ? nDMA[DMA_SCALE_X] : 0x100;
    dma_state.ystep = nDMA[DMA_SCALE_Y] ? nDMA[DMA_SCALE_Y] : 0x100;

    /* clip the clippers */
    dma_state.topclip = nDMA[DMA_TOPCLIP] & 0x1ff;
    dma_state.botclip = nDMA[DMA_BOTCLIP] & 0x1ff;
    dma_state.leftclip = nDMA[DMA_LEFTCLIP] & 0x3ff;
    dma_state.rightclip = nDMA[DMA_RIGHTCLIP] & 0x3ff;

    /* determine the offset */
    gfxoffset = nDMA[DMA_OFFSETLO] | (nDMA[DMA_OFFSETHI] << 16);

    /* special case: drawing mode C doesn't need to know about any pixel data */
    if ((command & 0x0f) == 0x0c)
        gfxoffset = 0;

    /* determine the location */
    if (!bGfxRomLarge && gfxoffset >= 0x2000000)
        gfxoffset -= 0x2000000;
    if (gfxoffset >= 0xf8000000)
        gfxoffset -= 0xf8000000;
    if (gfxoffset < 0x10000000)
        dma_state.offset = gfxoffset;
    else
    {
        goto skipdma;
    }

    /* there seems to be two types of behavior for the DMA chip */
    /* for MK1 and MK2, the upper byte of the LRSKIP is the     */
    /* starting skip value, and the lower byte is the ending    */
    /* skip value; for the NBA Jam, Hangtime, and Open Ice, the */
    /* full word seems to be the starting skip value.           */
    if (command & 0x40)
    {
        dma_state.startskip = nDMA[DMA_LRSKIP] & 0xff;
        dma_state.endskip = nDMA[DMA_LRSKIP] >> 8;
    }
    else
    {
        dma_state.startskip = 0;
        dma_state.endskip = nDMA[DMA_LRSKIP];
    }

    /* then draw */
    if (dma_state.xstep == 0x100 && dma_state.ystep == 0x100)
    {
        if (command & 0x80)
            (*dma_draw_skip_noscale[command & 0x1f])();
        else
            (*dma_draw_noskip_noscale[command & 0x1f])();

        pixels = dma_state.width * dma_state.height;
    }
    else
    {
        if (command & 0x80)
            (*dma_draw_skip_scale[command & 0x1f])();
        else
            (*dma_draw_noskip_scale[command & 0x1f])();

        if (dma_state.xstep && dma_state.ystep)
            pixels = ((dma_state.width << 8) / dma_state.xstep) * ((dma_state.height << 8) / dma_state.ystep);
        else
            pixels = 0;
    }

skipdma:
    TMS34010GenerateIRQ(DMA_IRQ);
    nDMA[DMA_COMMAND] &= ~0x8000;

}
