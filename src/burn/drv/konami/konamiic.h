// konamiic.cpp
//---------------------------------------------------------------------------------------------------------------
extern UINT32 KonamiIC_K051960InUse;
extern UINT32 KonamiIC_K052109InUse;
extern UINT32 KonamiIC_K051316InUse;
extern UINT32 KonamiIC_K053245InUse;
extern UINT32 KonamiIC_K053247InUse;
extern UINT32 KonamiIC_K053936InUse;
extern UINT32 KonamiIC_K053250InUse;
extern UINT32 KonamiIC_K055555InUse;
extern UINT32 KonamiIC_K054338InUse;
extern UINT32 KonamiIC_K056832InUse;

extern UINT8 *konami_priority_bitmap;
extern UINT32 *konami_bitmap32;
extern UINT32 *konami_palette32;

void KonamiClearBitmaps(UINT32 color);
void KonamiBlendCopy(UINT32 *palette);

void KonamiICReset();
void KonamiICExit();
void KonamiICScan(INT32 nAction);

void konami_sortlayers3( int *layer, int *pri );
void konami_sortlayers4( int *layer, int *pri );
void konami_sortlayers5( int *layer, int *pri );

void KonamiRecalcPalette(UINT8 *src, UINT32 *dst, INT32 len);

void konami_rom_deinterleave_2(UINT8 *src, INT32 len);
void konami_rom_deinterleave_4(UINT8 *src, INT32 len);

// internal
void KonamiAllocateBitmaps();
void konami_draw_16x16_tile(UINT8 *gfx, INT32 code, INT32 bpp, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy);
void konami_draw_16x16_prio_tile(UINT8 *gfx, INT32 code, INT32 bpp, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy, UINT32 priority);
void konami_draw_16x16_zoom_tile(UINT8 *gfx, INT32 code, INT32 bpp, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy);
void konami_draw_16x16_priozoom_tile(UINT8 *gfx, INT32 code, INT32 bpp, INT32 color, INT32 t, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, UINT32 priority);
void konami_render_zoom_shadow_tile(UINT8 *gfx, INT32 code, INT32 bpp, INT32 color, INT32 sx, INT32 sy, INT32 fx, INT32 fy, INT32 width, INT32 height, INT32 zoomx, INT32 zoomy, UINT32 priority, INT32 shadow);
void konami_set_highlight_mode(INT32 mode);
void konami_set_highlight_over_sprites_mode(INT32 mode);

// k051960 / k052109 shared
//---------------------------------------------------------------------------------------------------------------
void K052109_051960_w(INT32 offset, INT32 data);
UINT8 K052109_051960_r(INT32 offset);

// k051960.cpp
//---------------------------------------------------------------------------------------------------------------
extern INT32 K051960ReadRoms;
extern INT32 K052109_irq_enabled;
void K051960SpritesRender(INT32 min_priority, INT32 max_priority);
UINT8 K0519060FetchRomData(UINT32 Offset);
UINT8 K051960Read(UINT32 Offset);
void K051960Write(UINT32 Offset, UINT8 Data);
void K051960SetCallback(void (*Callback)(INT32 *Code, INT32 *Colour, INT32 *Priority, INT32 *Shadow));
void K051960SetSpriteOffset(INT32 x, INT32 y);
void K051960Reset();
void K051960GfxDecode(UINT8 *src, UINT8 *dst, INT32 len);
void K051960Init(UINT8* pRomSrc, UINT8* pRomSrcExp, UINT32 RomMask);
void K051960Exit();
void K051960Scan(INT32 nAction);
void K051960SetBpp(INT32 bpp);

void K051937Write(UINT32 Offset, UINT8 Data);
UINT8 K051937Read(UINT32 Offset);

// k052109.cpp
//---------------------------------------------------------------------------------------------------------------
extern INT32 K052109RMRDLine;
extern INT32 K051960_irq_enabled;
extern INT32 K051960_nmi_enabled;
extern INT32 K051960_spriteflip;

void K052109UpdateScroll();
void K052109AdjustScroll(INT32 x, INT32 y);

#define K052109_OPAQUE		0x10000
#define K052109_CATEGORY(x)	(0x100|((x)&0xff))

void K052109RenderLayer(INT32 nLayer, INT32 Flags, INT32 Priority);
UINT8 K052109Read(UINT32 Offset);
void K052109Write(UINT32 Offset, UINT8 Data);
void K052109SetCallback(void (*Callback)(INT32 Layer, INT32 Bank, INT32 *Code, INT32 *Colour, INT32 *xFlip, INT32 *priority));
void K052109Reset();
void K052109GfxDecode(UINT8 *src, UINT8 *dst, INT32 nLen);
void K052109Init(UINT8 *pRomSrc, UINT8* pRomSrcExp, UINT32 RomMask);
void K052109Exit();
void K052109Scan(INT32 nAction);

#define K051960ByteRead(nStartAddress)						\
if (a >= nStartAddress && a <= nStartAddress + 0x3ff) {				\
	return K051960Read(a - nStartAddress);					\
}

#define K051960ByteWrite(nStartAddress)						\
if (a >= nStartAddress && a <= nStartAddress + 0x3ff) {				\
	K051960Write((a - nStartAddress), d);					\
	return;									\
}

#define K051960WordWrite(nStartAddress)						\
if (a >= nStartAddress && a <= nStartAddress + 0x3ff) {				\
	if (a & 1) {								\
		K051960Write((a - nStartAddress) + 1, d & 0xff);		\
	} else {								\
		K051960Write((a - nStartAddress) + 0, (d >> 8) & 0xff);		\
	}									\
	return;									\
}

#define K051937ByteRead(nStartAddress)						\
if (a >= nStartAddress && a <= nStartAddress + 7) {				\
	INT32 Offset = (a - nStartAddress);					\
										\
	if (Offset == 0) {							\
		static INT32 Counter;						\
		return (Counter++) & 1;						\
	}									\
										\
	if (K051960ReadRoms && (Offset >= 0x04 && Offset <= 0x07)) {		\
		return K0519060FetchRomData(Offset & 3);			\
	}									\
										\
	return 0;								\
}

#define K015937ByteWrite(nStartAddress)						\
if (a >= nStartAddress && a <= nStartAddress + 7) {				\
	K051937Write((a - nStartAddress), d);					\
	return;									\
}

#define K052109WordNoA12Read(nStartAddress)					\
if (a >= nStartAddress && a <= nStartAddress + 0x7fff) {			\
	INT32 Offset = (a - nStartAddress) >> 1;					\
	Offset = ((Offset & 0x3000) >> 1) | (Offset & 0x07ff);			\
										\
	if (a & 1) {								\
		return K052109Read(Offset + 0x2000);				\
	} else {								\
		return K052109Read(Offset + 0x0000);				\
	}									\
}


#define K052109WordNoA12Write(nStartAddress)					\
if (a >= nStartAddress && a <= nStartAddress + 0x7fff) {			\
	INT32 Offset = (a - nStartAddress) >> 1;					\
	Offset = ((Offset & 0x3000) >> 1) | (Offset & 0x07ff);			\
										\
	if (a & 1) {								\
		K052109Write(Offset + 0x2000, d);				\
	} else {								\
		K052109Write(Offset + 0x0000, d);				\
	}									\
	return;									\
}


// K056832.cpp
//---------------------------------------------------------------------------------------------------------------
#define K056832_LAYER_ALPHA			0x00100000
#define K056832_LAYER_OPAQUE			0x00400000
#define K056832_DRAW_FLAG_MIRROR     		0x00800000
#define K056382_DRAW_FLAG_FORCE_XYSCROLL	0x00800000 // same as flag mirror

#define K056832_SET_ALPHA(x)			(K056832_LAYER_ALPHA | ((x)<<8))

void K056832Reset();
void K056832Init(UINT8 *rom, UINT8 *romexp, INT32 rom_size, void (*cb)(INT32 layer, INT32 *code, INT32 *color, INT32 *flags));
void K056832Exit();
void K056832Scan(INT32 nAction);
void K056832SetLayerAssociation(INT32 status);
void K056832SetGlobalOffsets(INT32 minx, INT32 miny);
void K056832SetLayerOffsets(INT32 layer, INT32 xoffs, INT32 yoffs);
void K056832SetExtLinescroll();
INT32 K056832IsIrqEnabled();
void K056832ReadAvac(INT32 *mode, INT32 *data);
UINT16 K056832ReadRegister(int reg);
INT32 K056832GetLookup(INT32 bits);
void K056832SetTileBank(int bank);
void K056832WordWrite(INT32 offset, UINT16 data);
void K056832ByteWrite(INT32 offset, UINT8 data);
UINT16 K056832RomWordRead(UINT16 offset);
void K056832HalfRamWriteWord(UINT32 offset, UINT16 data);
void K056832HalfRamWriteByte(UINT32 offset, UINT8 data);
UINT16 K056832HalfRamReadWord(UINT32 offset);
UINT8 K056832HalfRamReadByte(UINT32 offset);
void K056832RamWriteWord(UINT32 offset, UINT16 data);
void K056832RamWriteByte(UINT32 offset, UINT8 data);
UINT16 K056832RamReadWord(UINT32 offset);
UINT8 K056832RamReadByte(UINT32 offset);
UINT16 K056832RomWord8000Read(INT32 offset);
void K056832WritebRegsWord(INT32 offset, UINT16 data);
void K056832WritebRegsByte(INT32 offset, UINT8 data);
UINT16 K056832mwRomWordRead(INT32 address);
void K056832Draw(INT32 layer, UINT32 flags, UINT32 priority);
INT32 K056832GetLayerAssociation();
void K056832Metamorphic_Fixup();

// K051316.cpp
//---------------------------------------------------------------------------------------------------------------
void K051316Init(INT32 chip, UINT8 *gfx, UINT8 *gfxexp, INT32 mask, void (*callback)(INT32 *code,INT32 *color,INT32 *flags), INT32 bpp, INT32 transp);
void K051316Reset();
void K051316Exit();

#define K051316_16BIT	(1<<8)

void K051316RedrawTiles(INT32 chip);

UINT8 K051316ReadRom(INT32 chip, INT32 offset);
UINT8 K051316Read(INT32 chip, INT32 offset);
void K051316Write(INT32 chip, INT32 offset, INT32 data);

void K051316WriteCtrl(INT32 chip, INT32 offset, INT32 data);
void K051316WrapEnable(INT32 chip, INT32 status);
void K051316SetOffset(INT32 chip, INT32 xoffs, INT32 yoffs);
void K051316_zoom_draw(INT32 chip, INT32 flags);
void K051316Scan(INT32 nAction);

// K053245 / k053247 shared
//---------------------------------------------------------------------------------------------------------------
extern INT32 K05324xZRejection;
void K05324xSetZRejection(INT32 z);

// K053245.cpp
//---------------------------------------------------------------------------------------------------------------
INT32 K053245Reset();
void K053245GfxDecode(UINT8 *src, UINT8 *dst, INT32 len);
void K053245Init(INT32 chip, UINT8 *gfx, UINT8 *gfxexp, INT32 mask, void (*callback)(INT32 *code,INT32 *color,INT32 *priority));
void K053245Exit();
void K053245SetBpp(INT32 chip, INT32 bpp);

void K053245SpritesRender(INT32 chip);

void K053245SetSpriteOffset(INT32 chip,INT32 offsx, INT32 offsy);
void K053245ClearBuffer(INT32 chip);
void K053245UpdateBuffer(INT32 chip);
void K053244BankSelect(INT32 chip, INT32 bank);

UINT16 K053245ReadWord(INT32 chip, INT32 offset);
void K053245WriteWord(INT32 chip, INT32 offset, INT32 data);

UINT8 K053245Read(INT32 chip, INT32 offset);
void K053245Write(INT32 chip, INT32 offset, INT32 data);
UINT8 K053244Read(INT32 chip, INT32 offset);
void K053244Write(INT32 chip, INT32 offset, INT32 data);

void K053245Scan(INT32 nAction);

extern UINT8 *K053245Ram[2];

// K053251.cpp
//---------------------------------------------------------------------------------------------------------------
void K053251Reset();

void K053251Write(INT32 offset, INT32 data);

INT32 K053251GetPriority(INT32 idx);
INT32 K053251GetPaletteIndex(INT32 idx);

void K053251Write(INT32 offset, INT32 data);

void K053251Scan(INT32 nAction);

// K053247.cpp
//---------------------------------------------------------------------------------------------------------------
void K053247Reset();
void K053247Init(UINT8 *gfxrom, UINT8 *gfxromexp, INT32 gfxlen, void (*Callback)(INT32 *code, INT32 *color, INT32 *priority), INT32 flags);
void K053247Exit();
void K053247Scan(INT32 nAction);

void K053247SetBpp(INT32 bpp);

extern UINT8 *K053247Ram;
extern void (*K053247Callback)(INT32 *code, INT32 *color, INT32 *priority);

void K053247Export(UINT8 **ram, UINT8 **gfx, void (**callback)(INT32 *, INT32 *, INT32 *), INT32 *dx, INT32 *dy);
void K053247GfxDecode(UINT8 *src, UINT8 *dst, INT32 len); // 16x16
void K053247SetSpriteOffset(INT32 offsx, INT32 offsy);
void K053247WrapEnable(INT32 status);

void K053246_set_OBJCHA_line(INT32 state); // 1 assert, 0 clear
INT32 K053246_is_IRQ_enabled();

UINT8 K053247Read(INT32 offset);
void K053247Write(INT32 offset, INT32 data);
UINT8 K053246Read(INT32 offset);
void K053246Write(INT32 offset, INT32 data);

void K053247WriteRegsByte(INT32 offset, UINT8 data);
void K053247WriteRegsWord(INT32 offset, UINT16 data);
UINT16 K053247ReadRegs(INT32 offset);
UINT16 K053246ReadRegs(INT32 offset);

void K053247SpritesRender();

UINT16 K053247ReadWord(INT32 offset);
void K053247WriteWord(INT32 offset, UINT16 data);

// k054000.cpp
//------------------------------------------------------------------------------------------
void K054000Reset();
void K054000Write(INT32 offset, INT32 data);
UINT8 K054000Read(INT32 address);
void K054000Scan(INT32 nAction);

// K051733.cpp
//------------------------------------------------------------------------------------------
void K051733Reset();
void K051733Write(INT32 offset, INT32 data);
UINT8 K051733Read(INT32 offset);
void K051733Scan(INT32 nAction);

// K053936.cpp
//------------------------------------------------------------------------------------------

void K053936Init(INT32 chip, UINT8 *ram, INT32 len, INT32 w, INT32 h, void (*pCallback)(INT32 offset, UINT16 *ram, INT32 *code, INT32 *color, INT32 *sx, INT32 *sy, INT32 *fx, INT32 *fy));

void K053936Reset();
void K053936Exit();
void K053936Scan(INT32 nAction);

void K053936EnableWrap(INT32 chip, INT32 status);
void K053936SetOffset(INT32 chip, INT32 xoffs, INT32 yoffs);
void K053936PredrawTiles3(INT32 chip, UINT8 *gfx, INT32 tile_size_x, INT32 tile_size_y, INT32 transparent);
void K053936PredrawTiles2(INT32 chip, UINT8 *gfx);
void K053936PredrawTiles(INT32 chip, UINT8 *gfx, INT32 transparent, INT32 tcol /*transparent color*/);
void K053936Draw(INT32 chip, UINT16 *ctrl, UINT16 *linectrl, INT32 transp);

extern UINT16 *m_k053936_0_ctrl_16;
extern UINT16 *m_k053936_0_linectrl_16;
extern UINT16 *m_k053936_0_ctrl;
extern UINT16 *m_k053936_0_linectrl;
extern UINT16 *K053936_external_bitmap;

void K053936GP_set_colorbase(INT32 chip, INT32 color_base);
void K053936GP_enable(int chip, int enable);
void K053936GP_set_offset(int chip, int xoffs, int yoffs);
void K053936GP_clip_enable(int chip, int status);
void K053936GP_set_cliprect(int chip, int minx, int maxx, int miny, int maxy);
void K053936GP_0_zoom_draw(UINT16 *bitmap, int tilebpp, int blend, int alpha, int pixeldouble_output, UINT16* temp_m_k053936_0_ctrl_16, UINT16* temp_m_k053936_0_linectrl_16,UINT16* temp_m_k053936_0_ctrl, UINT16* temp_m_k053936_0_linectrl);
void K053936GpInit();
void K053936GPExit();

// k053250.cpp
//------------------------------------------------------------------------------------------

extern UINT16 *K053250Ram; // allocated in k053250Init

void K053250Init(INT32 chip, UINT8 *rom, UINT8 *romexp, INT32 size);
void K053250SetOffsets(INT32 chip, int offx, int offy);
void K053250Reset();
void K053250Scan(INT32 nAction);
void K053250Exit();

void K053250Draw(INT32 chip, int colorbase, int flags, int priority);

UINT16 K053250RegRead(INT32 chip, INT32 offset);
void K053250RegWrite(INT32 chip, INT32 offset, UINT8 data);
UINT16 K053250RomRead(INT32 chip, INT32 offset);


// k054338.cpp
//------------------------------------------------------------------------------------------

#define K338_REG_BGC_R      0
#define K338_REG_BGC_GB     1
#define K338_REG_SHAD1R     2
#define K338_REG_BRI3       11
#define K338_REG_PBLEND     13
#define K338_REG_CONTROL    15

#define K338_CTL_KILL       0x01    /* 0 = no video output, 1 = enable */
#define K338_CTL_MIXPRI     0x02
#define K338_CTL_SHDPRI     0x04
#define K338_CTL_BRTPRI     0x08
#define K338_CTL_WAILSL     0x10
#define K338_CTL_CLIPSL     0x20

void K054338Init();
void K054338Reset();
void K054338Exit();
void K054338Scan(INT32 nAction);
void K054338WriteWord(INT32 offset, UINT16 data);
void K054338WriteByte(INT32 offset, UINT8 data);
INT32 K054338_read_register(int reg);
void K054338_fill_solid_bg();
void K054338_fill_backcolor(int palette_offset, int mode);
INT32 K054338_set_alpha_level(int pblend);
void K054338_invert_alpha(int invert);
void K054338_update_all_shadows(INT32 rushingheroes_hack);
void K054338_export_config(int **shdRGB);

extern INT32 m_shd_rgb[12];

// k055555.cpp
//------------------------------------------------------------------------------------------

#define K55_PALBASE_BG      0   // background palette
#define K55_CONTROL         1   // control register
#define K55_COLSEL_0        2   // layer A, B color depth
#define K55_COLSEL_1        3   // layer C, D color depth
#define K55_COLSEL_2        4   // object, S1 color depth
#define K55_COLSEL_3        5   // S2, S3 color depth

#define K55_PRIINP_0        7   // layer A pri 0
#define K55_PRIINP_1        8   // layer A pri 1
#define K55_PRIINP_2        9   // layer A "COLPRI"
#define K55_PRIINP_3        10  // layer B pri 0
#define K55_PRIINP_4        11  // layer B pri 1
#define K55_PRIINP_5        12  // layer B "COLPRI"
#define K55_PRIINP_6        13  // layer C pri
#define K55_PRIINP_7        14  // layer D pri
#define K55_PRIINP_8        15  // OBJ pri
#define K55_PRIINP_9        16  // sub 1 (GP:PSAC) pri
#define K55_PRIINP_10       17  // sub 2 (GX:PSAC) pri
#define K55_PRIINP_11       18  // sub 3 pri

#define K55_OINPRI_ON       19  // object priority bits selector

#define K55_PALBASE_A       23  // layer A palette
#define K55_PALBASE_B       24  // layer B palette
#define K55_PALBASE_C       25  // layer C palette
#define K55_PALBASE_D       26  // layer D palette
#define K55_PALBASE_OBJ     27  // OBJ palette
#define K55_PALBASE_SUB1    28  // SUB1 palette
#define K55_PALBASE_SUB2    29  // SUB2 palette
#define K55_PALBASE_SUB3    30  // SUB3 palette

#define K55_BLEND_ENABLES   33  // blend enables for tilemaps
#define K55_VINMIX_ON       34  // additional blend enables for tilemaps
#define K55_OSBLEND_ENABLES 35  // obj/sub blend enables
#define K55_OSBLEND_ON      36  // not sure, related to obj/sub blend

#define K55_SHAD1_PRI       37  // shadow/highlight 1 priority
#define K55_SHAD2_PRI       38  // shadow/highlight 2 priority
#define K55_SHAD3_PRI       39  // shadow/highlight 3 priority
#define K55_SHD_ON          40  // shadow/highlight
#define K55_SHD_PRI_SEL     41  // shadow/highlight

#define K55_VBRI            42  // VRAM layer brightness enable
#define K55_OSBRI           43  // obj/sub brightness enable, part 1
#define K55_OSBRI_ON        44  // obj/sub brightness enable, part 2
#define K55_INPUT_ENABLES   45  // input enables

/* bit masks for the control register */
#define K55_CTL_GRADDIR     0x01    // 0=vertical, 1=horizontal
#define K55_CTL_GRADENABLE  0x02    // 0=BG is base color only, 1=gradient
#define K55_CTL_FLIPPRI     0x04    // 0=standard Konami priority, 1=reverse
#define K55_CTL_SDSEL       0x08    // 0=normal shadow timing, 1=(not used by GX)

/* bit masks for the input enables */
#define K55_INP_VRAM_A      0x01
#define K55_INP_VRAM_B      0x02
#define K55_INP_VRAM_C      0x04
#define K55_INP_VRAM_D      0x08
#define K55_INP_OBJ         0x10
#define K55_INP_SUB1        0x20
#define K55_INP_SUB2        0x40
#define K55_INP_SUB3        0x80

#define K055555_COLORMASK   0x0000ffff
#define K055555_MIXSHIFT    16
#define K055555_BRTSHIFT    18
#define K055555_SKIPSHADOW  0x40000000
#define K055555_FULLSHADOW  0x80000000

extern INT32 K055555_enabled;
void K055555WriteReg(UINT8 regnum, UINT8 regdat);
void K055555LongWrite(INT32 offset, UINT32 data); // not implimented
void K055555WordWrite(INT32 offset, UINT16 data);
void K055555ByteWrite(INT32 offset, UINT8 data);
INT32 K055555ReadRegister(INT32 regnum);
INT32 K055555GetPaletteIndex(INT32 idx);
void K055555Reset();
void K055555Init();
void K055555Exit();
void K055555Scan(INT32 nAction);


// konamigx.cpp
#define GXMIX_BLEND_AUTO    0           // emulate all blend effects
#define GXMIX_BLEND_NONE    1           // disable all blend effects
#define GXMIX_BLEND_FAST    2           // simulate translucency
#define GXMIX_BLEND_FORCE   3           // force mix code on selected layer(s)
#define GXMIX_NOLINESCROLL  0x1000      // disable linescroll on selected layer(s)
#define GXMIX_NOSHADOW      0x10000000  // disable all shadows (shadow pens will be skipped)
#define GXMIX_NOZBUF        0x20000000  // disable z-buffering (shadow pens will be drawn as solid)

// Sub Layer Flags
#define GXSUB_K053250   0x10    // chip type: 0=K053936 ROZ+, 1=K053250 LVC
#define GXSUB_4BPP      0x04    //  16 colors
#define GXSUB_5BPP      0x05    //  32 colors
#define GXSUB_8BPP      0x08    // 256 colors

void konamigx_mixer_init(int objdma);
void konamigx_mixer_exit();
void konamigx_mixer_primode(int mode);
void konamigx_mixer(int sub1 /*extra tilemap 1*/, int sub1flags, int sub2 /*extra tilemap 2*/, int sub2flags, int mixerflags, int extra_bitmap /*extra tilemap 3*/, int rushingheroes_hack);
extern INT32 konamigx_mystwarr_kludge;
