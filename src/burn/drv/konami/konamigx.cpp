#include "tiles_generic.h"
#include "konamiic.h"

static INT32 konamigx_wrport1_0 = 0;

UINT8 m_sound_ctrl;
UINT8 m_sound_intck;
UINT32 m_fantjour_dma[8];
int m_konamigx_current_frame;
int m_gx_objdma, m_gx_primode;

#define GX_MAX_SPRITES 512
#define GX_MAX_LAYERS  6
#define GX_MAX_OBJECTS (GX_MAX_SPRITES + GX_MAX_LAYERS)

static struct GX_OBJ { int order, offs, code, color; } *gx_objpool;
static UINT16 *gx_spriteram;

static int *K054338_shdRGB;

static UINT8 *gx_shdzbuf, *gx_objzbuf;

static INT32 k053247_vrcbk[4];
static INT32 k053247_opset;
static INT32 k053247_coreg;
static INT32 k053247_coregshift;

static INT32 opri;
static INT32 oinprion;
static INT32 vcblk[6];
static INT32 ocblk;
static INT32 vinmix;
static INT32 vmixon;
static INT32 osinmix;
static INT32 osmixon;

void konamigx_precache_registers()
{
	// (see sprite color coding scheme on p.46 & 47)
	static const int coregmasks[5] = {0xf,0xe,0xc,0x8,0x0};
	static const int coregshifts[5]= {4,5,6,7,8};
	int i;

	i = K053247ReadRegs(0x8/2);
	k053247_vrcbk[0] = (i & 0x000f) << 14;
	k053247_vrcbk[1] = (i & 0x0f00) << 6;
	i = K053247ReadRegs(0xa/2);
	k053247_vrcbk[2] = (i & 0x000f) << 14;
	k053247_vrcbk[3] = (i & 0x0f00) << 6;

	// COREG == OBJSET2+1C == bit8-11 of OPSET ??? (see p.50 last table, needs p.49 to confirm)
	k053247_opset = K053247ReadRegs(0xc/2);

	i = k053247_opset & 7; if (i > 4) i = 4;

	k053247_coreg = K053247ReadRegs(0xc/2)>>8 & 0xf;
	k053247_coreg =(k053247_coreg & coregmasks[i]) << 12;

	k053247_coregshift = coregshifts[i];

	opri     = K055555ReadRegister(K55_PRIINP_8);
	oinprion = K055555ReadRegister(K55_OINPRI_ON);
	vcblk[0] = K055555ReadRegister(K55_PALBASE_A);
	vcblk[1] = K055555ReadRegister(K55_PALBASE_B);
	vcblk[2] = K055555ReadRegister(K55_PALBASE_C);
	vcblk[3] = K055555ReadRegister(K55_PALBASE_D);
	vcblk[4] = K055555ReadRegister(K55_PALBASE_SUB1);
	vcblk[5] = K055555ReadRegister(K55_PALBASE_SUB2);
	ocblk    = K055555ReadRegister(K55_PALBASE_OBJ);
	vinmix   = K055555ReadRegister(K55_BLEND_ENABLES);
	vmixon   = K055555ReadRegister(K55_VINMIX_ON);
	osinmix  = K055555ReadRegister(K55_OSBLEND_ENABLES);
	osmixon  = K055555ReadRegister(K55_OSBLEND_ON);
}

void konamigx_mixer_init(int objdma)
{
	m_gx_objdma = 0;
	m_gx_primode = 0;

	gx_shdzbuf = (UINT8*)BurnMalloc(512 * 256 * 2);
	gx_objzbuf = (UINT8*)BurnMalloc(512 * 256 * 2);

	gx_objpool = (struct GX_OBJ*)BurnMalloc(GX_MAX_OBJECTS * sizeof(GX_OBJ));

	K054338_export_config(&K054338_shdRGB);

	gx_spriteram = (UINT16*)K053247Ram;

	if (objdma)
	{
		gx_spriteram = (UINT16*)BurnMalloc(0x1000); 
		m_gx_objdma = 1;
	}
	else
		gx_spriteram = (UINT16*)K053247Ram;

//	m_palette->set_shadow_dRGB32(3,-80,-80,-80, 0);
	K054338_invert_alpha(1);
}

void konamigx_mixer_exit()
{
	BurnFree(gx_shdzbuf);
	BurnFree(gx_objzbuf);
	if (m_gx_objdma && gx_spriteram) {
		BurnFree(gx_spriteram);
	}
	BurnFree(gx_objpool);
	m_gx_objdma = 0;
}

static void gx_wipezbuf(int noshadow)
{
#define GX_ZBUFW	512

	int w = (nScreenWidth - 1);
	int h = (nScreenHeight - 1);

	UINT8 *zptr = gx_objzbuf;
	int ecx = h;

	do { memset(zptr, -1, w); zptr += GX_ZBUFW; } while (--ecx);

	if (!noshadow)
	{
		zptr = gx_shdzbuf;
		w <<= 1;
		ecx = h;
		do { memset(zptr, -1, w); zptr += (GX_ZBUFW<<1); } while (--ecx);
	}
}

void konamigx_mixer_primode(int mode)
{
	m_gx_primode = mode;
}

static void gx_draw_basic_tilemaps(int mixerflags, int code)
{
	int temp1,temp2,temp3,temp4;
	int i = code<<1;
	int j = mixerflags>>i & 3;
	int k = 0;

	int disp = K055555ReadRegister(K55_INPUT_ENABLES);
	if (disp & (1<<code))
	{
		if (j == GXMIX_BLEND_NONE)  { temp1 = 0xff; temp2 = temp3 = 0; } else
		if (j == GXMIX_BLEND_FORCE) { temp1 = 0x00; temp2 = mixerflags>>(i+16); temp3 = 3; }
		else
		{
			temp1 = vinmix;
			temp2 = vinmix>>i & 3;
			temp3 = vmixon>>i & 3;
		}

		/* blend layer only when:
		    1) vinmix != 0xff
		    2) its internal mix code is set
		    3) all mix code bits are internal(overriden until tile blending has been implemented)
		    4) 0 > alpha < 255;
		*/
		if (temp1!=0xff && temp2 /*&& temp3==3*/)
		{
			temp4 = K054338_set_alpha_level(temp2);

			if (temp4 <= 0) return;
			if (temp4 < 255) k = K056832_SET_ALPHA(temp4);
		}

		if (mixerflags & 1<<(code+12)) k |= K056382_DRAW_FLAG_FORCE_XYSCROLL;

		if (nBurnLayer & (1 << code)) K056832Draw(code, k, 0);
	}
}

static void gx_draw_basic_extended_tilemaps_1(int mixerflags, int code, int sub1, int sub1flags, int rushingheroes_hack, int offs)
{
	int temp1,temp2,temp3,temp4;
	int i = code<<1;
	int j = mixerflags>>i & 3;
	int k = 0;
	static int parity = 0;
	parity ^= 1;

	sub1 ^= 0; // kill warnings

	int disp = K055555ReadRegister(K55_INPUT_ENABLES);
	if ((disp & K55_INP_SUB1) || (rushingheroes_hack))
	{
		int alpha = 255;

		if (j == GXMIX_BLEND_NONE)  { temp1 = 0xff; temp2 = temp3 = 0; } else
		if (j == GXMIX_BLEND_FORCE) { temp1 = 0x00; temp2 = mixerflags>>24; temp3 = 3; }
		else
		{
			temp1 = osinmix;
			temp2 = osinmix>>2 & 3;
			temp3 = osmixon>>2 & 3;
		}

		if (temp1!=0xff && temp2 /*&& temp3==3*/)
		{
			alpha = temp4 = K054338_set_alpha_level(temp2);

			if (temp4 <= 0) return;
			if (temp4 < 255) k = (j == GXMIX_BLEND_FAST) ? ~parity : 1;
		}

		int l = sub1flags & 0xf;

		if (offs == -2)
		{

			int pixeldouble_output = 0;

			int width = nScreenWidth;

			if (width>512) // vsnetscr case
				pixeldouble_output = 1;

			if (nSpriteEnable & 4 && K053936_external_bitmap)
				K053936GP_0_zoom_draw(K053936_external_bitmap, l, k, alpha, pixeldouble_output, m_k053936_0_ctrl_16, m_k053936_0_linectrl_16, m_k053936_0_ctrl, m_k053936_0_linectrl);
		}
		else
		{
			if (nSpriteEnable & 2)
				if (KonamiIC_K053250InUse) K053250Draw(0, vcblk[4]<<l, 0, 0);
		}
	}
}

static void gx_draw_basic_extended_tilemaps_2(int mixerflags, int code, int sub2, int sub2flags, INT32 extra_bitmap, int offs)
{
	int temp1,temp2,temp3,temp4;
	int i = code<<1;
	int j = mixerflags>>i & 3;
//  int k = 0;
//  static int parity = 0;
//  parity ^= 1;

	sub2 ^= 0; // kill warnings

	int disp = K055555ReadRegister(K55_INPUT_ENABLES);
	if (disp & K55_INP_SUB2)
	{
		//int alpha = 255;
		if (j == GXMIX_BLEND_NONE)  { temp1 = 0xff; temp2 = temp3 = 0; } else
		if (j == GXMIX_BLEND_FORCE) { temp1 = 0x00; temp2 = mixerflags>>26; temp3 = 3; }
		else
		{
			temp1 = osinmix;
			temp2 = osinmix>>4 & 3;
			temp3 = osmixon>>4 & 3;
		}

		if (temp1!=0xff && temp2 /*&& temp3==3*/)
		{
			//alpha =
			temp4 = K054338_set_alpha_level(temp2);

			if (temp4 <= 0) return;
			//if (temp4 < 255) k = (j == GXMIX_BLEND_FAST) ? ~parity : 1;
		}

		int l = sub2flags & 0xf;

		if (offs == -3)
		{
			if (extra_bitmap) // soccer superstars roz layer
			{
#if 0 // iq_132
				int xx,yy;
				int width = screen.width();
				int height = screen.height();
				const pen_t *paldata = m_palette->pens();

				// the output size of the roz layer has to be doubled horizontally
				// so that it aligns with the sprites and normal tilemaps.  This appears
				// to be done as a post-processing / mixing step effect
				//
				// - todo, use the pixeldouble_output I just added for vsnet instead?
				for (yy=0;yy<height;yy++)
				{
					UINT16* src = &extra_bitmap->pix16(yy);
					UINT32* dst = &bitmap.pix32(yy);
					int shiftpos = 0;
					for (xx=0;xx<width;xx+=2)
					{
						UINT16 dat = src[(((xx/2)+shiftpos))%width];
						if (dat&0xff)
							dst[xx+1] = dst[xx] = paldata[dat];
					}
				}
#endif
			}
			else
			{
			//  int pixeldouble_output = 0;
			//  K053936GP_1_zoom_draw(machine, bitmap, cliprect, sub2, l, k, alpha, pixeldouble_output);
			l = 0; // kill warning
			}
		}
		else {
// iq_132			machine().device<k053250_device>("k053250_2")->draw(bitmap, cliprect, vcblk[5]<<l, 0, screen.priority(), 0);
		}
	}
}

extern void k053247_draw_single_sprite_gxcore(UINT8 *,UINT8 *gx_shdzbuf, int code, unsigned short *gx_spriteram, int offs,
		int color, int alpha, int drawmode, int zcode, int pri,
		int primask, int shadow, unsigned char *drawmode_table, unsigned char *shadowmode_table, int shdmask);


static void konamigx_mixer_draw(int sub1, int sub1flags,int sub2, int sub2flags,int mixerflags,  int extra_bitmap, int rushingheroes_hack,struct GX_OBJ *objpool,int *objbuf,int nobj)
{
	// traverse draw list
	int disp = K055555ReadRegister(K55_INPUT_ENABLES);

	for (int count=0; count<nobj; count++)
	{
		struct GX_OBJ *objptr = objpool + objbuf[count];
		int order  = objptr->order;
		int offs   = objptr->offs;
		int code   = objptr->code;
		int color  = objptr->color;

		/* entries >=0 in our list are sprites */
		if (offs >= 0)
		{
			if (!(disp & K55_INP_OBJ)) continue;

			int drawmode = order>>4 & 0xf;

			int alpha = 255;
			int pri = 0;
			int zcode = -1; // negative zcode values turn off z-buffering

			if (drawmode & 2)
			{
				alpha = color>>K055555_MIXSHIFT & 3;
				if (alpha) alpha = K054338_set_alpha_level(alpha);
				if (alpha <= 0) continue;
			}
			color &= K055555_COLORMASK;

		//	if (drawmode >= 4) m_palette->set_shadow_mode(order & 0x0f);	// iq_132

			if (!(mixerflags & GXMIX_NOZBUF))
			{
				zcode = order>>16 & 0xff;
				pri = order>>24 & 0xff;
			}

			if (nSpriteEnable & 1)
				k053247_draw_single_sprite_gxcore(gx_objzbuf, gx_shdzbuf,code,gx_spriteram,offs,color,alpha,drawmode,zcode,pri,0,0,NULL,NULL,0);
		}
		else
		{
			switch (offs)
			{
				case -1:
					gx_draw_basic_tilemaps(mixerflags, code);
					continue;
				case -2:
				case -4:
					gx_draw_basic_extended_tilemaps_1(mixerflags, code, sub1, sub1flags, rushingheroes_hack, offs);
				continue;
				case -3:
				case -5:
					gx_draw_basic_extended_tilemaps_2(mixerflags, code, sub2, sub2flags, extra_bitmap, offs);
				continue;
			}
			continue;
		}
	}
}

void konamigx_mixer(int sub1 /*extra tilemap 1*/, int sub1flags, int sub2 /*extra tilemap 2*/, int sub2flags, int mixerflags, int extra_bitmap /*extra tilemap 3*/, int rushingheroes_hack)
{
	int objbuf[GX_MAX_OBJECTS];
	int shadowon[3], shdpri[3], layerid[6], layerpri[6];

	struct GX_OBJ *objpool, *objptr;
	int cltc_shdpri, /*prflp,*/ disp;

	// abort if object database failed to initialize
	objpool = gx_objpool;
	if (!objpool) return;

	// clear screen with backcolor and update flicker pulse
	if (konamigx_wrport1_0 & 0x20)
		K054338_fill_backcolor(K055555ReadRegister(0) << 9, K055555ReadRegister(1));
	else
		K054338_fill_solid_bg();

	// abort if video has been disabled
	disp = K055555ReadRegister(K55_INPUT_ENABLES);
	if (!disp) return;
	cltc_shdpri = K054338_read_register(K338_REG_CONTROL);

	if (!rushingheroes_hack) // Slam Dunk 2 never sets this.  It's either part of the protection, or type4 doesn't use it
	{
		if (!(cltc_shdpri & K338_CTL_KILL)) return;
	}

	// demote shadows by one layer when this bit is set??? (see p.73 8.6)
	cltc_shdpri &= K338_CTL_SHDPRI;

	// wipe z-buffer
	if (mixerflags & GXMIX_NOZBUF)
		mixerflags |= GXMIX_NOSHADOW;
	else
		gx_wipezbuf(mixerflags & GXMIX_NOSHADOW);

	// cache global parameters
	konamigx_precache_registers();

	// init OBJSET2 and mixer parameters (see p.51 and chapter 7)
	layerid[0] = 0; layerid[1] = 1; layerid[2] = 2; layerid[3] = 3; layerid[4] = 4; layerid[5] = 5;

	// invert layer priority when this flag is set (not used by any GX game?)
	//prflp = K055555ReadRegister(K55_CONTROL) & K55_CTL_FLIPPRI;

	layerpri[0] = K055555ReadRegister(K55_PRIINP_0);
	layerpri[1] = K055555ReadRegister(K55_PRIINP_3);
	layerpri[3] = K055555ReadRegister(K55_PRIINP_7);
	layerpri[4] = K055555ReadRegister(K55_PRIINP_9);
	layerpri[5] = K055555ReadRegister(K55_PRIINP_10);

	int shdprisel;

	if (m_gx_primode == -1)
	{
		// Lethal Enforcer hack (requires pixel color comparison)
		layerpri[2] = K055555ReadRegister(K55_PRIINP_3) + 0x20;
		shdprisel = 0x3f;
	}
	else
	{
		layerpri[2] = K055555ReadRegister(K55_PRIINP_6);
		shdprisel = K055555ReadRegister(K55_SHD_PRI_SEL);
	}

	// SHDPRISEL filters shadows by different priority comparison methods (UNIMPLEMENTED, see detail on p.66)
	if (!(shdprisel & 0x03)) shadowon[0] = 0;
	if (!(shdprisel & 0x0c)) shadowon[1] = 0;
	if (!(shdprisel & 0x30)) shadowon[2] = 0;

	shdpri[0]   = K055555ReadRegister(K55_SHAD1_PRI);
	shdpri[1]   = K055555ReadRegister(K55_SHAD2_PRI);
	shdpri[2]   = K055555ReadRegister(K55_SHAD3_PRI);

	int spri_min = 0;

	shadowon[2] = shadowon[1] = shadowon[0] = 0;

	int k = 0;
	if (!(mixerflags & GXMIX_NOSHADOW))
	{
		int i,j;
		// only enable shadows beyond a +/-7 RGB threshold
		for (j=0,i=0; i<3; j+=3,i++)
		{
			k = K054338_shdRGB[j  ]; if (k < -7 || k > 7) { shadowon[i] = 1; continue; }
			k = K054338_shdRGB[j+1]; if (k < -7 || k > 7) { shadowon[i] = 1; continue; }
			k = K054338_shdRGB[j+2]; if (k < -7 || k > 7) { shadowon[i] = 1; }
		}

		// SHDON specifies layers on which shadows can be projected (see detail on p.65 7.2.8)
		int temp = K055555ReadRegister(K55_SHD_ON);
		for (i=0; i<4; i++) if (!(temp>>i & 1) && spri_min < layerpri[i]) spri_min = layerpri[i]; // HACK

		// update shadows status
		K054338_update_all_shadows();
	}

	// pre-sort layers
	for (int j=0; j<5; j++)
	{
		int temp1 = layerpri[j];
		for (int i=j+1; i<6; i++)
		{
			int temp2 = layerpri[i];
			if ((UINT32)temp1 <= (UINT32)temp2)
			{
				layerpri[i] = temp1; layerpri[j] = temp1 = temp2;
				temp2 = layerid[i]; layerid[i] = layerid[j]; layerid[j] = temp2;
			}
		}
	}

	// build object database and create indices
	objptr = objpool;
	int nobj = 0;

	for (int i=5; i>=0; i--)
	{
		int offs;

		int code = layerid[i];
		switch (code)
		{
			/*
			    Background layers are represented by negative offset values as follow:

			    0+ : normal sprites
			    -1 : tile layer A - D
			    -2 : K053936 ROZ+ layer 1
			    -3 : K053936 ROZ+ layer 2
			    -4 : K053250 LVC layer 1
			    -5 : K053250 LVC layer 2
			*/
			case 4 :
				offs = -128;
				if (sub1flags & 0xf) { if (sub1flags & GXSUB_K053250) offs = -4; else if (sub1) offs = -2; }
			break;
			case 5 :
				offs = -128;
				if (sub2flags & 0xf) { if (sub2flags & GXSUB_K053250) offs = -5; else if (sub2) offs = -3; }
				if (extra_bitmap) offs = -3;
			break;
			default: offs = -1;
		}

		if (offs != -128)
		{
			objptr->order = layerpri[i]<<24;
			objptr->code  = code;
			objptr->offs = offs;
			objptr++;

			objbuf[nobj] = nobj;
			nobj++;
		}
	}

//  i = j = 0xff;
	int l = 0;

	for (int offs=0; offs<0x800; offs+=8)
	{
		int pri = 0;

		if (!(gx_spriteram[offs] & 0x8000)) continue;

		int zcode = gx_spriteram[offs] & 0xff;

		// invert z-order when opset_pri is set (see p.51 OPSET PRI)
		if (k053247_opset & 0x10) zcode = 0xff - zcode;

		int code  = gx_spriteram[offs+1];
		int color = k = gx_spriteram[offs+6];
		l     = gx_spriteram[offs+7];

		K053247Callback(&code, &color, &pri);

		/*
		    shadow = shadow code
		    spri   = shadow priority
		    temp1  = add solid object
		    temp2  = solid pens draw mode
		    temp3  = add shadow object
		    temp4  = shadow pens draw mode
		*/
		int temp4 = 0;
		int temp3 = 0;
		int temp2 = 0;
		int temp1 = 0;
		int spri = 0;
		int shadow = 0;

		if (color & K055555_FULLSHADOW)
		{
			shadow = 3; // use default intensity and color
			spri = pri; // retain host priority
			temp3 = 1; // add shadow
			temp4 = 5; // draw full shadow
		}
		else
		{
			shadow = k>>10 & 3;
			if (shadow) // object has shadow?
			{
				int k053246_objset1 = K053246ReadRegs(5);

				if (shadow != 1 || k053246_objset1 & 0x20)
				{
					shadow--;
					temp1 = 1; // add solid
					temp2 = 1; // draw partial solid
					if (shadowon[shadow])
					{
						temp3 = 1; // add shadow
						temp4 = 4; // draw partial shadow
					}
				}
				else
				{
					// drop the entire sprite to shadow if its shadow code is 1 and SD0EN is off (see p.48)
					shadow = 0;
					if (!shadowon[0]) continue;
					temp3 = 1; // add shadow
					temp4 = 5; // draw full shadow
				}
			}
			else
			{
				temp1 = 1; // add solid
				temp2 = 0; // draw full solid
			}

			if (temp1)
			{
				// tag sprite for alpha blending
				if (color>>K055555_MIXSHIFT & 3) temp2 |= 2;
			}

			if (temp3)
			{
				// determine shadow priority
				spri = (k053247_opset & 0x20) ? pri : shdpri[shadow]; // (see p.51 OPSET SDSEL)
			}
		}

		switch (m_gx_primode & 0xf)
		{
			// Dadandarn zcode suppression
			case  1:
				zcode = 0;
			break;

			// Daisukiss bad shadow filter
			case  4:
				if (k & 0x3000 || k == 0x0800) continue;

			// Tokkae shadow masking (INACCURATE)
			case  5:
				if (spri < spri_min) spri = spri_min;
			break;
		}

		/*
		    default sort order:
		    fedcba9876543210fedcba9876543210
		    xxxxxxxx------------------------ (priority)
		    --------xxxxxxxx---------------- (zcode)
		    ----------------xxxxxxxx-------- (offset)
		    ------------------------xxxx---- (shadow mode)
		    ----------------------------xxxx (shadow code)
		*/
		if (temp1)
		{

			// add objects with solid or alpha pens
			int order = pri<<24 | zcode<<16 | offs<<(8-3) | temp2<<4;
			objptr->order = order;
			objptr->offs  = offs;
			objptr->code  = code;
			objptr->color = color;
			objptr++;

			objbuf[nobj] = nobj;
			nobj++;
		}

		if (temp3 && !(color & K055555_SKIPSHADOW) && !(mixerflags & GXMIX_NOSHADOW))
		{
			// add objects with shadows if enabled
			int order = spri<<24 | zcode<<16 | offs<<(8-3) | temp4<<4 | shadow;
			objptr->order = order;
			objptr->offs  = offs;
			objptr->code  = code;
			objptr->color = color;
			objptr++;

			objbuf[nobj] = nobj;
			nobj++;
		}
	}

	// sort objects in decending order (SLOW)
	k = nobj;
	l = nobj - 1;

	for (int j=0; j<l; j++)
	{
		int temp1 = objbuf[j];
		int temp2 = objpool[temp1].order;
		for (int i=j+1; i<k; i++)
		{
			int temp3 = objbuf[i];
			int temp4 = objpool[temp3].order;
			if ((UINT32)temp2 <= (UINT32)temp4) { temp2 = temp4; objbuf[i] = temp1; objbuf[j] = temp1 = temp3; }
		}
	}

	konamigx_mixer_draw(sub1,sub1flags,sub2,sub2flags,mixerflags,extra_bitmap,rushingheroes_hack,objpool,objbuf,nobj);
}
