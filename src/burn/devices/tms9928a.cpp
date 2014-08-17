#include "tiles_generic.h"
#include "tms9928a.h"

static int TMS9928A_palette[16] = {
	0x000000, 0x000000, 0x21c842, 0x5edc78, 0x5455ed, 0x7d76fc, 0xd4524d, 0x42ebf5,
	0xfc5554, 0xff7978, 0xd4c154, 0xe6ce80, 0x21b03b, 0xc95bba, 0xcccccc, 0xffffff
};

typedef struct {
	unsigned char ReadAhead;
	unsigned char Regs[8];
	unsigned char StatusReg;
	unsigned char FirstByte;
	unsigned char latch;
	unsigned char INT;
	int Addr;
	int colour;
	int pattern;
	int nametbl;
	int spriteattribute;
	int spritepattern;
	int colourmask;
	int patternmask;

	unsigned char *vMem;
	unsigned char *dBackMem;
	unsigned short *tmpbmp;
	int vramsize;
	int model;

	int max_x;
	int min_x;
	int max_y;
	int min_y;

	int LimitSprites;
	int top_border;
	int bottom_border;

	void (*INTCallback)(int);
} TMS9928A;

static TMS9928A tms;

static unsigned int Palette[16]; // high color support

static void TMS89928aPaletteRecalc()
{
	for (int i = 0; i < 16; i++) {
		Palette[i] = BurnHighCol(TMS9928A_palette[i] >> 16, TMS9928A_palette[i] >> 8, TMS9928A_palette[i] >> 0 , 0);
	}
}

static void change_register(int reg, unsigned char val)
{
	static const unsigned char Mask[8] = { 0x03, 0xfb, 0x0f, 0xff, 0x07, 0x7f, 0x07, 0xff };

	val &= Mask[reg];
	tms.Regs[reg] = val;

	switch (reg)
	{
		case 0:
		{
			if (val & 2) {
				tms.colour = ((tms.Regs[3] & 0x80) * 64) & (tms.vramsize - 1);
				tms.colourmask = (tms.Regs[3] & 0x7f) * 8 | 7;
				tms.pattern = ((tms.Regs[4] & 4) * 2048) & (tms.vramsize - 1);
				tms.patternmask = (tms.Regs[4] & 3) * 256 |
					(tms.colourmask & 255);
			} else {
				tms.colour = (tms.Regs[3] * 64) & (tms.vramsize - 1);
				tms.pattern = (tms.Regs[4] * 2048) & (tms.vramsize - 1);
			}
		}
		break;

		case 1:
		{
		      	int b = (val & 0x20) && (tms.StatusReg & 0x80);
		        if (b != tms.INT) {
		            tms.INT = b;
		            if (tms.INTCallback) tms.INTCallback (tms.INT);
		        }
		}
		break;

		case 2:
			tms.nametbl = (val * 1024) & (tms.vramsize - 1);
		break;

		case 3:
		{
			if (tms.Regs[0] & 2) {
				tms.colour = ((val & 0x80) * 64) & (tms.vramsize - 1);
				tms.colourmask = (val & 0x7f) * 8 | 7;
			} else {
				tms.colour = (val * 64) & (tms.vramsize - 1);
			}
			tms.patternmask = (tms.Regs[4] & 3) * 256 | (tms.colourmask & 255);
		}
		break;

		case 4:
		{
			if (tms.Regs[0] & 2) {
				tms.pattern = ((val & 4) * 2048) & (tms.vramsize - 1);
				tms.patternmask = (val & 3) * 256 | 255;
			} else {
				tms.pattern = (val * 2048) & (tms.vramsize - 1);
			}
		}
		break;

		case 5:
			tms.spriteattribute = (val * 128) & (tms.vramsize - 1);
		break;

		case 6:
			tms.spritepattern = (val * 2048) & (tms.vramsize - 1);
		break;

		case 7:
			/* The backdrop is updated at TMS9928A_refresh() */
		break;
	}
}

void TMS9928AReset()
{
	for (int i = 0; i < 8; i++)
		tms.Regs[i] = 0;

	tms.StatusReg = 0;
	tms.nametbl = tms.pattern = tms.colour = 0;
	tms.spritepattern = tms.spriteattribute = 0;
	tms.colourmask = tms.patternmask = 0;
	tms.Addr = tms.ReadAhead = tms.INT = 0;
	tms.FirstByte = 0;
	tms.latch = 0;
}

void TMS9928AInit(int model, int vram, int borderx, int bordery, void (*INTCallback)(int))
{
	GenericTilesInit();

	tms.model = model;

	tms.INTCallback = INTCallback;

	tms.top_border		= ((tms.model == TMS9929) || (tms.model == TMS9929A)) ? 51 : 27;
	tms.bottom_border	= ((tms.model == TMS9929) || (tms.model == TMS9929A)) ? 51 : 24;

#define MIN(x,y) ((x)<(y)?(x):(y))

	tms.min_x = 15 - MIN(borderx, 15);
	tms.max_x = 15 + 32*8 - 1 + MIN(borderx, 15);
	tms.min_y = tms.top_border - MIN(bordery, tms.top_border);
	tms.max_y = tms.top_border + 24*8 - 1 + MIN(bordery, tms.bottom_border);

#undef MIN

	tms.vramsize = vram;
	tms.vMem = (unsigned char*)malloc(tms.vramsize);

	tms.dBackMem = (unsigned char*)malloc(256 * 192);

	tms.tmpbmp = (unsigned short*)malloc(256 * 192 * sizeof(short));

	TMS9928AReset ();
	tms.LimitSprites = 1;
}

void TMS9928AExit()
{
	TMS9928AReset();

	GenericTilesExit();

	free (tms.tmpbmp);
	free (tms.vMem);
	free (tms.dBackMem);
}

void TMS9928APostLoad()
{
	for (int i = 0; i < 8; i++)
		change_register(i, tms.Regs[i]);

	if (tms.INTCallback) tms.INTCallback(tms.INT);
}

unsigned char TMS9928AReadVRAM()
{
	int b = tms.ReadAhead;
	tms.ReadAhead = tms.vMem[tms.Addr];
	tms.Addr = (tms.Addr + 1) & (tms.vramsize - 1);
	tms.latch = 0;
	return b;
}

void TMS9928AWriteVRAM(int data)
{
	tms.vMem[tms.Addr] = data;
	tms.Addr = (tms.Addr + 1) & (tms.vramsize - 1);
	tms.ReadAhead = data;
	tms.latch = 0;
}

unsigned char TMS9928AReadRegs()
{
	int b = tms.StatusReg;
	tms.StatusReg = 0x1f;
	if (tms.INT) {
		tms.INT = 0;
		if (tms.INTCallback) tms.INTCallback (tms.INT);
	}
	tms.latch = 0;
	return b;
}

void TMS9928AWriteRegs(int data)
{
	if (tms.latch) {
		if (data & 0x80) {
			change_register(data & 0x07, tms.FirstByte);
		} else {
			tms.Addr = ((unsigned short)data << 8 | tms.FirstByte) & (tms.vramsize - 1);
			if (!(data & 0x40)) {
				TMS9928AReadVRAM();
			}
		}

		tms.latch = 0;
	} else {
		tms.FirstByte = data;
		tms.latch = 1;
	}
}

void TMS9928ASetSpriteslimit(int limit)
{
	tms.LimitSprites = limit;
}

static void draw_mode0(unsigned short *bitmap)
{
	unsigned char *patternptr;

	for (int y = 0, name = 0; y < 24; y++)
	{
		for (int x = 0; x < 32; x++, name++)
		{
			int charcode = tms.vMem[tms.nametbl+name];

			patternptr = tms.vMem + tms.pattern + charcode*8;

			int color = tms.vMem[tms.colour+charcode/8];

			int fg = color >> 4;
			int bg = color & 0x0f;

			for (int yy = 0; yy < 8; yy++)
			{
				int bits = *patternptr++;

				for (int xx = 0; xx < 8; xx++, bits<<=1)
				{
					bitmap[(y * 8 + yy) * 256 + (x * 8 + xx)] = (bits & 0x80) ? fg : bg;
				}
			}
		}
	}
}

static void draw_mode1(unsigned short *bitmap)
{
	int pattern,x,y,yy,xx,name,charcode;
	UINT8 fg,bg,*patternptr;

	fg = tms.Regs[7] / 16;
	bg = tms.Regs[7] & 15;

	for (y = 0; y < 191; y++) {
		for (x = 0; x < 7; x++) {
			bitmap[y * 256 + x + 248] = bitmap[y * 256 + x] = bg;
		}
	}

    name = 0;

    for (y=0;y<24;y++) {
        for (x=0;x<40;x++) {
            charcode = tms.vMem[tms.nametbl+name];
            name++;
            patternptr = tms.vMem + tms.pattern + (charcode*8);
            for (yy=0;yy<8;yy++) {
                pattern = *patternptr++;
                for (xx=0;xx<6;xx++) {
			bitmap[(y * 8 + yy) * 256 + (8 + x*6+xx)] = (pattern & 0x80) ? fg : bg;
			pattern *= 2;
                }
            }
        }
    }
}

static void draw_mode12(unsigned short *bitmap)
{
	int pattern,x,y,yy,xx,name,charcode;
	UINT8 fg,bg,*patternptr;

	fg = tms.Regs[7] / 16;
	bg = tms.Regs[7] & 15;

	for (y = 0; y < 191; y++) {
		for (x = 0; x < 7; x++) {
			bitmap[y * 256 + x + 248] = bitmap[y * 256 + x] = bg;
		}
	}

    name = 0;
    for (y=0;y<24;y++) {
        for (x=0;x<40;x++) {
            charcode = (tms.vMem[tms.nametbl+name]+(y/8)*256)&tms.patternmask;
            name++;
            patternptr = tms.vMem + tms.pattern + (charcode*8);
            for (yy=0;yy<8;yy++) {
                pattern = *patternptr++;
                for (xx=0;xx<6;xx++) {
			bitmap[(y * 8 + yy) * 256 + (8 + x*6+xx)] = (pattern & 0x80) ? fg : bg;
			pattern *= 2;
                }
            }
        }
    }
}

static void draw_mode2(unsigned short *bitmap)
{
    int colour,name,x,y,yy,pattern,xx,charcode;
    UINT8 fg,bg;
    UINT8 *colourptr,*patternptr;

    name = 0;
    for (y=0;y<24;y++) {
        for (x=0;x<32;x++) {
            charcode = tms.vMem[tms.nametbl+name]+(y/8)*256;
            name++;
            colour = (charcode&tms.colourmask);
            pattern = (charcode&tms.patternmask);
            patternptr = tms.vMem+tms.pattern+colour*8;
            colourptr = tms.vMem+tms.colour+pattern*8;
            for (yy=0;yy<8;yy++) {
                pattern = *patternptr++;
                colour = *colourptr++;
                fg = colour / 16;
                bg = colour & 15;
                for (xx=0;xx<8;xx++) {
			bitmap[(y * 8 + yy) * 256 + (x*8+xx)] = (pattern & 0x80) ? fg : bg;
                    pattern *= 2;
                }
            }
        }
    }
}

static void draw_mode3(unsigned short *bitmap)
{
    int x,y,yy,yyy,name,charcode;
    UINT8 fg,bg,*patternptr;

    name = 0;
    for (y=0;y<24;y++) {
        for (x=0;x<32;x++) {
            charcode = tms.vMem[tms.nametbl+name];
            name++;
            patternptr = tms.vMem+tms.pattern+charcode*8+(y&3)*2;
            for (yy=0;yy<2;yy++) {
                fg = (*patternptr / 16);
                bg = ((*patternptr++) & 15);
                for (yyy=0;yyy<4;yyy++) {
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+0)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+1)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+2)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+3)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+4)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+5)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+6)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+7)] = fg;
                }
            }
        }
    }
}

static void draw_mode23(unsigned short *bitmap)
{
    int x,y,yy,yyy,name,charcode;
    UINT8 fg,bg,*patternptr;

    name = 0;
    for (y=0;y<24;y++) {
        for (x=0;x<32;x++) {
            charcode = tms.vMem[tms.nametbl+name];
            name++;
            patternptr = tms.vMem + tms.pattern +
                ((charcode+(y&3)*2+(y/8)*256)&tms.patternmask)*8;
            for (yy=0;yy<2;yy++) {
                fg = (*patternptr / 16);
                bg = ((*patternptr++) & 15);
                for (yyy=0;yyy<4;yyy++) {
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+0)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+1)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+2)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+3)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+4)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+5)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+6)] = fg;
			bitmap[(y*8+yy*4+yyy) * 256 + (x*8+7)] = fg;
                }
            }
        }
    }
}

static void draw_modebogus(unsigned short *bitmap)
{
    UINT8 fg,bg;
    int x,y,n,xx;

    fg = tms.Regs[7] / 16;
    bg = tms.Regs[7] & 15;

    for (y=0;y<192;y++) {
        xx=0;
        n=8; while (n--) bitmap[y * 256 + (xx++)] = bg;
        for (x=0;x<40;x++) {
            n=4; while (n--) bitmap[y * 256 + (xx++)] = fg;
            n=2; while (n--) bitmap[y * 256 + (xx++)] = bg;
        }
        n=8; while (n--) bitmap[y * 256 + (xx++)] = bg;
    }
}

static void (*const ModeHandlers[])(unsigned short *bitmap) = {
        draw_mode0, draw_mode1, draw_mode2,  draw_mode12,
        draw_mode3, draw_modebogus, draw_mode23,
        draw_modebogus
};

static void draw_sprites()
{
	UINT8 *attributeptr,*patternptr,c;
	int p,x,y,size,i,j,large,yy,xx,limit[192],
	illegalsprite,illegalspriteline;
	UINT16 line,line2;

	attributeptr = tms.vMem + tms.spriteattribute;
	size = (tms.Regs[1] & 2) ? 16 : 8;
	large = (int)(tms.Regs[1] & 1);

	for (x=0;x<192;x++) limit[x] = 4;
	tms.StatusReg = 0x80;
	illegalspriteline = 255;
	illegalsprite = 0;

	memset (tms.dBackMem, 0, 256 * 192);

    for (p=0;p<32;p++) {
        y = *attributeptr++;
        if (y == 208) break;
        if (y > 208) {
            y=-(~y&255);
        } else {
            y++;
        }
        x = *attributeptr++;
        patternptr = tms.vMem + tms.spritepattern +
            ((size == 16) ? *attributeptr & 0xfc : *attributeptr) * 8;
        attributeptr++;
        c = (*attributeptr & 0x0f);
        if (*attributeptr & 0x80) x -= 32;
        attributeptr++;

        if (!large) {
            /* draw sprite (not enlarged) */
            for (yy=y;yy<(y+size);yy++) {
                if ( (yy < 0) || (yy > 191) ) continue;
                if (limit[yy] == 0) {
                    /* illegal sprite line */
                    if (yy < illegalspriteline) {
                        illegalspriteline = yy;
                        illegalsprite = p;
                    } else if (illegalspriteline == yy) {
                        if (illegalsprite > p) {
                            illegalsprite = p;
                        }
                    }
                    if (tms.LimitSprites) continue;
                } else limit[yy]--;
                line = 256*patternptr[yy-y] + patternptr[yy-y+16];
                for (xx=x;xx<(x+size);xx++) {
                    if (line & 0x8000) {
                        if ((xx >= 0) && (xx < 256)) {
                            if (tms.dBackMem[yy*256+xx]) {
                                tms.StatusReg |= 0x20;
                            } else {
                                tms.dBackMem[yy*256+xx] = 0x01;
                            }
                            if (c && ! (tms.dBackMem[yy*256+xx] & 0x02))
                            {
                                tms.dBackMem[yy*256+xx] |= 0x02;
				pTransDraw[(tms.top_border+yy) * nScreenWidth + (15 + xx)] = c;
                            }
                        }
                    }
                    line *= 2;
                }
            }
        } else {
            /* draw enlarged sprite */
            for (i=0;i<size;i++) {
                yy=y+i*2;
                line2 = 256*patternptr[i] + patternptr[i+16];
                for (j=0;j<2;j++) {
                    if ( (yy >= 0) && (yy <= 191) ) {
                        if (limit[yy] == 0) {
                            /* illegal sprite line */
                            if (yy < illegalspriteline) {
                                illegalspriteline = yy;
                                illegalsprite = p;
                            } else if (illegalspriteline == yy) {
                                if (illegalsprite > p) {
                                    illegalsprite = p;
                                }
                            }
                            if (tms.LimitSprites) continue;
                        } else limit[yy]--;
                        line = line2;
                        for (xx=x;xx<(x+size*2);xx+=2) {
                            if (line & 0x8000) {
                                if ((xx >=0) && (xx < 256)) {
                                    if (tms.dBackMem[yy*256+xx]) {
                                        tms.StatusReg |= 0x20;
                                    } else {
                                        tms.dBackMem[yy*256+xx] = 0x01;
                                    }
                                    if (c && ! (tms.dBackMem[yy*256+xx] & 0x02))
                                    {
                                        tms.dBackMem[yy*256+xx] |= 0x02;
					pTransDraw[(tms.top_border+yy) * nScreenWidth + (15 + xx)] = c;
                                    }
                                }
                                if (((xx+1) >=0) && ((xx+1) < 256)) {
                                    if (tms.dBackMem[yy*256+xx+1]) {
                                        tms.StatusReg |= 0x20;
                                    } else {
                                        tms.dBackMem[yy*256+xx+1] = 0x01;
                                    }
                                    if (c && ! (tms.dBackMem[yy*256+xx+1] & 0x02))
                                    {
                                        tms.dBackMem[yy*256+xx+1] |= 0x02;
					pTransDraw[(tms.top_border+yy) * nScreenWidth + (15 + xx + 1)] = c;
                                    }
                                }
                            }
                            line *= 2;
                        }
                    }
                    yy++;
                }
            }
        }
    }
    if (illegalspriteline == 255) {
        tms.StatusReg |= (p > 31) ? 31 : p;
    } else {
        tms.StatusReg |= 0x40 + illegalsprite;
    }
}


int TMS9928ADraw()
{
	int BackColour = tms.Regs[7] & 15;

	if (!BackColour) BackColour=1;

	TMS89928aPaletteRecalc();
	Palette[0] = Palette[BackColour];

	if (!(tms.Regs[1] & 0x40))
	{
		for (int i = 0; i < nScreenWidth * nScreenHeight; i++) {
			pTransDraw[i] = BackColour;
		}
	}
	else
	{
		int mode = ((((tms.model == TMS99x8A) || (tms.model == TMS9929A)) ? (tms.Regs[0] & 2) : 0) | ((tms.Regs[1] & 0x10)>>4) | ((tms.Regs[1] & 8)>>1));

		(*ModeHandlers[mode])(tms.tmpbmp);

		{
			for (int y = 0; y < 192; y++)
			{
				for (int x = 0; x < 256; x++)
				{
					pTransDraw[(y + tms.top_border) * nScreenWidth + x + 15] = tms.tmpbmp[y * 256 + x];
				}
			}
		}

		{
			for (int y = 0; y < tms.top_border; y++) { // top
				for (int x = 0; x < 15 + 256 + 15; x++) {
					pTransDraw[y * nScreenWidth + x] = BackColour;
				}
			}

			for (int y = tms.top_border+192; y < tms.top_border+192+tms.bottom_border; y++) { // bottom
				for (int x = 0; x < 15 + 256 + 15; x++) {
					pTransDraw[y * nScreenWidth + x] = BackColour;
				}
			}


			for (int y = tms.top_border; y < tms.top_border+192; y++) { // left
				for (int x = 0; x < 15; x++) {
					pTransDraw[y * nScreenWidth + x] = BackColour;
				}
			}

			for (int y = tms.top_border; y < tms.top_border+192; y++) { // right
				for (int x = 15+256; x < 15 + 256 + 15; x++) {
					pTransDraw[y * nScreenWidth + x] = BackColour;
				}
			}
		}

		if ((tms.Regs[1] & 0x50) == 0x40)
			draw_sprites();
	}

	BurnTransferCopy(Palette);

	return 0;
}

int TMS9928AInterrupt()
{
	int  b = (tms.Regs[1] & 0x20) != 0;

	tms.StatusReg |= 0x80;

	if (b != tms.INT) {
		tms.INT = b;
		if (tms.INTCallback) tms.INTCallback (tms.INT);
	}

	return b;
}

int TMS9928AScan(int nAction, int *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_VOLATILE) {		
	//	memset(&ba, 0, sizeof(ba));

		ba.Data   = tms.vMem;
		ba.nLen   = tms.vramsize;
		ba.szName = "video ram";
		BurnAcb(&ba);

		ba.Data	  = tms.Regs;
		ba.nLen	  = 8;
		ba.szName = "tms registers";
		BurnAcb(&ba);

		SCAN_VAR(tms.ReadAhead);
		SCAN_VAR(tms.StatusReg);
		SCAN_VAR(tms.FirstByte);
		SCAN_VAR(tms.latch);
		SCAN_VAR(tms.INT);
		SCAN_VAR(tms.Addr);
		SCAN_VAR(tms.colour);
		SCAN_VAR(tms.pattern);
		SCAN_VAR(tms.nametbl);
		SCAN_VAR(tms.spriteattribute);
		SCAN_VAR(tms.spritepattern);
		SCAN_VAR(tms.colourmask);
		SCAN_VAR(tms.patternmask);
	}

	return 0;
}
