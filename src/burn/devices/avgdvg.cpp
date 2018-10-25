/*************************************************************************

    avgdvg.c: Atari DVG and AVG simulators

    Copyright 1991, 1992, 1996 Eric Smith

    Modified for the MAME project 1997 by
    Brad Oliver, Bernd Wiebelt, Aaron Giles, Andrew Caldwell

    971108 Disabled vector timing routines, introduced an ugly (but fast!)
            busy flag hack instead. BW
    980202 New anti aliasing code by Andrew Caldwell (.ac)
    980206 New (cleaner) busy flag handling.
            Moved LBO's buffered point into generic vector code. BW
    980212 Introduced timing code based on Aaron timer routines. BW
    980318 Better color handling, Bzone and MHavoc clipping. BW

    Battlezone uses a red overlay for the top of the screen and a green one
    for the rest. There is a circuit to clip color 0 lines extending to the
    red zone. This is emulated now. Thanks to Neil Bradley for the info. BW

    Frame and interrupt rates (Neil Bradley) BW
        ~60 fps/4.0ms: Asteroid, Asteroid Deluxe
        ~40 fps/4.0ms: Lunar Lander
        ~40 fps/4.1ms: Battle Zone
        ~45 fps/5.4ms: Space Duel, Red Baron
        ~30 fps/5.4ms: StarWars

    Games with self adjusting framerate
        4.1ms: Black Widow, Gravitar
        4.1ms: Tempest
        Major Havoc
        Quantum

    TODO: accurate vector timing (need timing diagramm)

************************************************************************/

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "avgdvg.h"
#include "vector.h"

//#define VG_DEBUG

#ifdef VG_DEBUG
#define VGLOG(x)  bprintf x
#else
#define VGLOG(x)
#endif


/*************************************
 *
 *  Constants
 *
 *************************************/

#define BRIGHTNESS	12

#define MAXSTACK	8		/* Tempest needs more than 4 */

/* AVG commands */
#define VCTR		0
#define HALT		1
#define SVEC		2
#define STAT		3
#define CNTR		4
#define JSRL		5
#define RTSL		6
#define JMPL		7
#define SCAL		8

/* DVG commands */
#define DVCTR		0x01
#define DLABS		0x0a
#define DHALT		0x0b
#define DJSRL		0x0c
#define DRTSL		0x0d
#define DJMPL		0x0e
#define DSVEC		0x0f


/*************************************
 *
 *  Static variables
 *
 *************************************/

static INT32 translucency = 0; // for starwars? -dink

static UINT8 vector_engine;
static UINT8 flipword;
static UINT8 busy;
static UINT32 colorram[32];

static INT32 width, height;
static INT32 xcenter, ycenter;
static INT32 xmin, xmax;
static INT32 ymin, ymax;

static INT32 flip_x, flip_y, swap_xy;

#define BANK_SIZE (0x2000)
#define NUM_BANKS (2)
static UINT8 *vectorbank[NUM_BANKS];

static UINT8 *vectorram;
static INT32 vectorram_size = 0;

static UINT32 sparkle_callback(void)
{
	return colorram[16 + ((rand() >> 8) & 15)];
}

#ifdef INLINE
#undef INLINE
#endif

#define INLINE static

#define VGVECTOR 0
#define VGCLIP 1
#define MAXVECT 10000

static INT32 nvect = 0;
static INT32 has_clip = 0;

struct vgvector
{
	INT32 x; INT32 y;
	INT32 color;
	INT32 intensity;
	INT32 arg1; INT32 arg2;
	INT32 status;
};

static vgvector *vectbuf = NULL;

static void vg_flush()
{
	INT32 cx0 = 0, cy0 = 0, cx1 = 0x5000000, cy1 = 0x5000000;
	INT32 i = 0;

	while (vectbuf[i].status == VGCLIP) {
		i++;
	}
	INT32 xs = vectbuf[i].x;
	INT32 ys = vectbuf[i].y;

	for (i = 0; i < nvect; i++)
	{
		if (vectbuf[i].status == VGVECTOR)
		{
			INT32 xe = vectbuf[i].x;
			INT32 ye = vectbuf[i].y;
			INT32 x0 = xs, y0 = ys, x1 = xe, y1 = ye;

			if (has_clip) {
				xs = xe;
				ys = ye;

				if((x0 < cx0 && x1 < cx0) || (x0 > cx1 && x1 > cx1))
					continue;

				if(x0 < cx0) {
					y0 += INT64(cx0-x0)*INT64(y1-y0)/(x1-x0);
					x0 = cx0;
				} else if(x0 > cx1) {
					y0 += INT64(cx1-x0)*INT64(y1-y0)/(x1-x0);
					x0 = cx1;
				}
				if(x1 < cx0) {
					y1 += INT64(cx0-x1)*INT64(y1-y0)/(x1-x0);
					x1 = cx0;
				} else if(x1 > cx1) {
					y1 += INT64(cx1-x1)*INT64(y1-y0)/(x1-x0);
					x1 = cx1;
				}

				if((y0 < cy0 && y1 < cy0) || (y0 > cy1 && y1 > cy1))
					continue;

				if(y0 < cy0) {
					x0 += INT64(cy0-y0)*INT64(x1-x0)/(y1-y0);
					y0 = cy0;
				} else if(y0 > cy1) {
					x0 += INT64(cy1-y0)*INT64(x1-x0)/(y1-y0);
					y0 = cy1;
				}
				if(y1 < cy0) {
					x1 += INT64(cy0-y1)*INT64(x1-x0)/(y1-y0);
					y1 = cy0;
				} else if(y1 > cy1) {
					x1 += INT64(cy1-y1)*INT64(x1-x0)/(y1-y0);
					y1 = cy1;
				}

				vector_add_point(x0, y0, vectbuf[i].color, 0);
				vector_add_point(x1, y1, vectbuf[i].color, vectbuf[i].intensity);
			} else {
				vector_add_point(x1, y1, vectbuf[i].color, vectbuf[i].intensity);
			}
		}

		if (vectbuf[i].status == VGCLIP) {
			cx0 = vectbuf[i].x;
			cy0 = vectbuf[i].y;
			cx1 = vectbuf[i].arg1;
			cy1 = vectbuf[i].arg2;
			if(cx0 > cx1) {
				INT32 t = cx1;
				cx1 = cx0;
				cx0 = t;
			}
			if(cy0 > cx1) {
				INT32 t = cy1;
				cy1 = cy0;
				cy0 = t;
			}
		}
	}

	nvect = 0;
}

void vg_vector_add_point(INT32 x, INT32 y, INT32 color, INT32 intensity)
{
	if (nvect < MAXVECT)
	{
		vectbuf[nvect].status = VGVECTOR;
		vectbuf[nvect].x = x;
		vectbuf[nvect].y = y;
		vectbuf[nvect].color = color;
		vectbuf[nvect].intensity = intensity;
		nvect++;
	}
}

void vg_vector_add_clip (INT32 c_xmin, INT32 c_ymin, INT32 c_xmax, INT32 c_ymax)
{
	if (nvect < MAXVECT)
	{
		has_clip = 1;

		vectbuf[nvect].status = VGCLIP;
		vectbuf[nvect].x = c_xmin;
		vectbuf[nvect].y = c_ymin;
		vectbuf[nvect].arg1 = c_xmax;
		vectbuf[nvect].arg2 = c_ymax;
		nvect++;
	}
}



/*************************************
 *
 *  Compute 2's complement value
 *
 *************************************/

INLINE INT32 twos_comp_val(INT32 num, INT32 bits)
{
	return (INT32)(num << (32 - bits)) >> (32 - bits);
}

/*************************************
 *
 *  Vector RAM accesses
 *
 *************************************/

INLINE UINT16 vector_word(UINT16 offset)
{
	UINT8 *base;
	/* convert from word offset to byte */
	offset *= 2;
	if (offset >= vectorram_size) {
		bprintf(0, _T("AVG/DVG Overflow at address: %X\n"), offset);
		return (vector_engine == USE_DVG) ? DHALT << 12 : HALT << 13; // halt!
	}
	/* get address of the word */
	base = &vectorbank[offset / BANK_SIZE][offset % BANK_SIZE];

	/* result is based on flipword */
	if (flipword)
		return base[1] | (base[0] << 8);
	else
		return base[0] | (base[1] << 8);
}



/*************************************
 *
 *  Vector timing
 *
 *************************************/

INLINE INT32 vector_timer(INT32 deltax, INT32 deltay)
{
	deltax = abs(deltax);
	deltay = abs(deltay);
	if (deltax > deltay)
		return deltax >> 16;
	else
		return deltay >> 16;
}


INLINE INT32 dvg_vector_timer(INT32 scale)
{
	return scale;
}



/*************************************
 *
 *  AVG brightness computation
 *
 *************************************/

INLINE INT32 effective_z(INT32 z, INT32 statz)
{
	/* Star Wars blends Z and an 8-bit STATZ */
	/* STATZ of 128 should give highest intensity */
	if (vector_engine == USE_AVG_SWARS)
	{
		z = (z * statz) / (translucency ? 12 : 8);
		if (z > 0xff)
			z = 0xff;
	}

	/* everyone else uses this */
	else
	{
		/* special case for Alpha One -- no idea if this is right */
		if (vector_engine == USE_AVG_ALPHAONE)
		{
			if (z)
				z ^= 0x15;
		}

		/* Z == 2 means use the value from STATZ */
		else if (z == 2)
			z = statz;

		z *= (translucency) ? BRIGHTNESS : 16;
	}

	return z;
}



/*************************************
 *
 *  DVG vector generator
 *
 *************************************/

static INT32 dvg_generate_vector_list(void)
{
	static const char *dvg_mnem[] =
	{
		"????", "vct1", "vct2", "vct3",
		"vct4", "vct5", "vct6", "vct7",
		"vct8", "vct9", "labs", "halt",
		"jsrl", "rtsl", "jmpl", "svec"
	};

	INT32 stack[MAXSTACK];
	INT32 pc = 0;
	INT32 sp = 0;
	INT32 scale = 0;
	INT32 currentx = 0, currenty = 0;
	INT32 total_length = 1;
	INT32 done = 0;

	INT32 firstwd, secondwd = 0;
	INT32 opcode;
	INT32 x, y, z, temp, a;
	INT32 deltax, deltay;

	/* reset the vector list */
	vector_reset();

	/* loop until finished */
	while (!done)
	{
		/* fetch the first word and get its opcode */
		firstwd = vector_word(pc++);
		opcode = firstwd >> 12;

		/* the DVCTR and DLABS opcodes take two words */
		if (opcode >= 0 && opcode <= DLABS)
			secondwd = vector_word(pc++);

		/* debugging */
		VGLOG((0, L"%4x: %4x ", pc, firstwd));
		if (opcode <= DLABS)
		{
			(void)dvg_mnem;
			VGLOG((0, L"%S ", dvg_mnem[opcode]));
			VGLOG((0, L"%4x  ", secondwd));
		}

		/* switch off the opcode */
		switch (opcode)
		{
			/* 0 is an invalid opcode */
			case 0:
	 			VGLOG((0, L"Error: DVG opcode 0!  Addr %4x Instr %4x %4x\n", pc-2, firstwd, secondwd));
#ifdef VG_DEBUG
				done = 1;
				break;
#endif

			/* 1-9 are DVCTR ops: draw a vector */
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 8:
			case 9:

				/* compute raw X and Y values */
	  			y = firstwd & 0x03ff;
				x = secondwd & 0x3ff;
				if (firstwd & 0x400)
					y = -y;
				if (secondwd & 0x400)
					x = -x;

				/* determine the brightness */
				z = secondwd >> 12;
				VGLOG((0, L"(%d,%d) z: %d scal: %d", x, y, z, opcode));

				/* compute the effective brightness */
				z = effective_z(z, z);

				/* determine the scale factor; scale 9 means -1 */
				temp = ((scale + opcode) & 0x0f);

	  			if (temp > 9)
					temp = -1;

				/* compute the deltas */
	  			deltax = (x << 16) >> (9-temp);
				deltay = (y << 16) >> (9-temp);

				/* adjust the current position and compute timing */
	  			currentx += deltax;
				currenty -= deltay;
				total_length += dvg_vector_timer(temp);

				/* add the new point */
				vg_vector_add_point(currentx, currenty, colorram[1], z);
				break;

			/* DSVEC: draw a short vector */
			case DSVEC:

				/* compute raw X and Y values */
				y = firstwd & 0x0300;
				x = (firstwd & 0x03) << 8;
				if (firstwd & 0x0400)
					y = -y;
				if (firstwd & 0x04)
					x = -x;

				/* determine the brightness */
				z = (firstwd >> 4) & 0x0f;

				/* compute the effective brightness */
				z = effective_z(z, z);

				/* determine the scale factor; scale 9 means -1 */
				temp = 2 + ((firstwd >> 2) & 0x02) + ((firstwd >> 11) & 0x01);
	  			temp = (scale + temp) & 0x0f;

				if (temp > 9)
					temp = -1;
				VGLOG((0, L"(%d,%d) z: %d scal: %d", x, y, z, temp));

				/* compute the deltas */
				deltax = (x << 16) >> (9 - temp);
				deltay = (y << 16) >> (9 - temp);

				/* adjust the current position and compute timing */
	  			currentx += deltax;
				currenty -= deltay;
				total_length += dvg_vector_timer(temp);

				/* add the new point */
				vg_vector_add_point(currentx, currenty, colorram[1], z);
				break;

			/* DLABS: move to an absolute location */
			case DLABS:

				/* extract the new X,Y coordinates */
				y = twos_comp_val(firstwd, 12);
				x = twos_comp_val(secondwd, 12);

				/* global scale comes from upper 4 bits of second word */
	  			scale = secondwd >> 12;

	  			/* set the current X,Y */
				currentx = (x - xmin) << 16;
				currenty = (ymax - y) << 16;
				VGLOG((0, L"(%d,%d) scal: %d", x, y, secondwd >> 12));
				break;

			/* DRTSL: return from subroutine */
			case DRTSL:

				/* handle stack underflow */
				if (sp == 0)
	    		{
					bprintf(0, _T("\n*** Vector generator stack underflow! ***\n"));
					done = 1;
					sp = MAXSTACK - 1;
				}
				else
					sp--;

				/* pull the new PC from the stack */
				pc = stack[sp];

				/* debugging */
				if (firstwd & 0x1fff)
					VGLOG((0, L"(%d?)", firstwd & 0x1fff));
				break;

			/* DHALT: all done! */
			case DHALT:
				done = 1;

				/* debugging */
				if (firstwd & 0x1fff)
      				VGLOG((0, L"(%d?)", firstwd & 0x0fff));
				break;

			/* DJMPL: jump to a new program location */
			case DJMPL:
				a = firstwd & 0x0fff;
				VGLOG((0, L"%4x", a));
				pc = a;

				if (!pc)
					done=1;
				break;

			/* DJSRL: jump to a new program location as subroutine */
			case DJSRL:
				a = firstwd & 0x0fff;
				VGLOG((0, L"%4x", a));

				/* push the next PC on the stack */
				stack[sp] = pc;

				/* check for stack overflows */
				if (sp == (MAXSTACK - 1))
	    		{
					bprintf(0, _T("\n*** Vector generator stack overflow! ***\n"));
					done = 1;
					sp = 0;
				}
				else
					sp++;

				/* jump to the new location */
				pc = a;
				break;

			/* anything else gets caught here */
			default:
				bprintf(0, _T("Unknown DVG opcode found\n"));
				done = 1;
		}
   		VGLOG((0, L"\n"));
	}

	/* return the total length of everything drawn */
	return total_length;
}

void avg_set_flip_x(INT32 flip)
{
	flip_x = (flip) ? 1 : 0;
}

void avg_set_flip_y(INT32 flip)
{
	flip_y = (flip) ? 1 : 0;
}

void avg_apply_flipping_and_swapping(INT32 *x, INT32 *y)
{
	if (flip_x)
		*x += (xcenter-*x)<<1;
	if (flip_y)
		*y += (ycenter-*y)<<1;

	if (swap_xy)
	{
		INT32 temp = *x;
		*x = *y - ycenter + xcenter;
		*y = temp - xcenter + ycenter;
	}
}

void avg_add_point(INT32 x, INT32 y, UINT32 color, INT32 intensity)
{
	avg_apply_flipping_and_swapping(&x, &y);
	vg_vector_add_point(x, y, color, intensity);
}

void avg_add_point_callback(INT32 x, INT32 y, UINT32 (*color_callback)(void), INT32 intensity)
{
	avg_apply_flipping_and_swapping(&x, &y);
	vg_vector_add_point(x, y, color_callback(), intensity);
}

/*************************************
 *
 *  AVG vector generator
 *
 *************************************

    Atari Analog Vector Generator Instruction Set

    Compiled from Atari schematics and specifications
    Eric Smith  7/2/92
    ---------------------------------------------

    NOTE: The vector generator is little-endian.  The instructions are 16 bit
          words, which need to be stored with the least significant byte in the
          lower (even) address.  They are shown here with the MSB on the left.

    The stack allows four levels of subroutine calls in the TTL version, but only
    three levels in the gate array version.

    inst  bit pattern          description
    ----  -------------------  -------------------
    VCTR  000- yyyy yyyy yyyy  normal vector
          zzz- xxxx xxxx xxxx
    HALT  001- ---- ---- ----  halt - does CNTR also on newer hardware
    SVEC  010y yyyy zzzx xxxx  short vector - don't use zero length
    STAT  0110 ---- zzzz cccc  status
    SCAL  0111 -bbb llll llll  scaling
    CNTR  100- ---- dddd dddd  center
    JSRL  101a aaaa aaaa aaaa  jump to subroutine
    RTSL  110- ---- ---- ----  return
    JMPL  111a aaaa aaaa aaaa  jump

    -     unused bits
    x, y  relative x and y coordinates in two's complement (5 or 13 bit,
          5 bit quantities are scaled by 2, so x=1 is really a length 2 vector.
    z     intensity, 0 = blank, 1 means use z from STAT instruction,  2-7 are
          doubled for actual range of 4-14
    c     color
    b     binary scaling, multiplies all lengths by 2**(1-b), 0 is double size,
          1 is normal, 2 is half, 3 is 1/4, etc.
    l     linear scaling, multiplies all lengths by 1-l/256, don't exceed $80
    d     delay time, use $40
    a     address (word address relative to base of vector memory)

    Notes:

    Quantum:
            the VCTR instruction has a four bit Z field, that is not
            doubled.  The value 2 means use Z from STAT instruction.

            the SVEC instruction can't be used

    Major Havoc:
            SCAL bit 11 is used for setting a Y axis window.

            STAT bit 11 is used to enable "sparkle" color.
            STAT bit 10 inverts the X axis of vectors.
            STAT bits 9 and 8 are the Vector ROM bank select.

    Star Wars:
            STAT bits 10, 9, and 8 are used directly for R, G, and B.

 *************************************/

static INT32 avg_generate_vector_list(void)
{
	static const char *avg_mnem[] =
	{
		"vctr", "halt", "svec", "stat", "cntr",
		"jsrl", "rtsl", "jmpl", "scal"
	};

	INT32 stack[MAXSTACK] = {0,0};
	INT32 pc = 0;
	INT32 sp = 0;
	INT32 scale = 0;
	INT32 statz = 0;
	INT32 sparkle = 0;
	INT32 xflip = 0;
	INT32 color = 0;
	INT32 ywindow = 1;
	INT32 currentx = xcenter;
	INT32 currenty = ycenter;
	INT32 total_length = 1;
	INT32 done = 0;

	INT32 firstwd, secondwd = 0;
	INT32 opcode;
	INT32 x, y, z = 0, b, l, d, a;
	INT32 deltax, deltay;

	/* check for zeroed vector RAM */
	if (vector_word(pc) == 0 && vector_word(pc + 1) == 0)
	{
		VGLOG((0, L"VGO with zeroed vector memory\n"));
		return total_length;
	}

	/* kludge to bypass Major Havoc's empty frames */
	if ((vector_engine == USE_AVG_MHAVOC || vector_engine == USE_AVG_ALPHAONE) && vector_word(pc) == 0xafe2)
		return total_length;

	/* reset the vector list */
	vector_reset();

	/* loop until finished... */
	while (!done)
	{
		/* fetch the first word and get its opcode */
		firstwd = vector_word(pc++);
		opcode = firstwd >> 13;

		/* the VCTR opcode takes two words */
		if (opcode == VCTR)
			secondwd = vector_word(pc++);

		/* SCAL is a variant of STAT; convert it here */
		else if (opcode == STAT && (firstwd & 0x1000))
			opcode = SCAL;

		/* debugging */
		(void)avg_mnem;
		VGLOG((0, L"%4x: %4x ", pc, firstwd));

		if (opcode == VCTR)
			VGLOG((0, L"%4x  ", secondwd));
		else
			VGLOG((0, L"      "));
		VGLOG((0, L"%S ", avg_mnem[opcode]));

		/* switch off the opcode */
		switch (opcode)
		{
			/* VCTR: draw a long vector */
			case VCTR:

				/* Quantum uses 12-bit vectors and a 4-bit Z value */
				if (vector_engine == USE_AVG_QUANTUM)
				{
					x = twos_comp_val(secondwd, 12);
					y = twos_comp_val(firstwd, 12);
					z = (secondwd >> 12) & 0x0f;
				}

				/* everyone else uses 13-bit vectors and a 3-bit Z value */
				else
				{
					x = twos_comp_val(secondwd, 13);
					y = twos_comp_val(firstwd, 13);
					z = (secondwd >> 12) & 0x0e;
				}

				/* compute the effective brightness */
				z = effective_z(z, statz);

				/* compute the deltas */
				deltax = x * scale;
				deltay = y * scale;
				if (xflip) deltax = -deltax;

				/* adjust the current position and compute timing */
				currentx += deltax;
				currenty -= deltay;
				total_length += vector_timer(deltax, deltay);

				/* add the new point */
				if (sparkle)
					avg_add_point_callback(currentx, currenty, sparkle_callback, z);
				else
					avg_add_point(currentx, currenty, colorram[color], z);
				VGLOG((0, L"VCTR x:%d y:%d z:%d statz:%d", x, y, z, statz));
				break;

			/* SVEC: draw a short vector */
			case SVEC:

				/* Quantum doesn't support this */
				if (vector_engine == USE_AVG_QUANTUM)
					break;

				/* two 5-bit vectors plus a 3-bit Z value */
				x = twos_comp_val(firstwd, 5) * 2;
				y = twos_comp_val(firstwd >> 8, 5) * 2;
				z = (firstwd >> 4) & 0x0e;

				/* compute the effective brightness */
				z = effective_z(z, statz);

				/* compute the deltas */
				deltax = x * scale;
				deltay = y * scale;
				if (xflip) deltax = -deltax;

				/* adjust the current position and compute timing */
				currentx += deltax;
				currenty -= deltay;
				total_length += vector_timer(deltax, deltay);

				/* add the new point */
				if (sparkle)
					avg_add_point_callback(currentx, currenty, sparkle_callback, z);
				else
					avg_add_point(currentx, currenty, colorram[color], z);
				VGLOG((0, L"SVEC x:%d y:%d z:%d statz:%d", x, y, z, statz));
				break;

			/* STAT: control colors, clipping, sparkling, and flipping */
			case STAT:

				/* Star Wars takes RGB directly and has an 8-bit brightness */
				if (vector_engine == USE_AVG_SWARS)
				{
					color = (firstwd >> 8) & 7;
					statz = firstwd & 0xff;
				}

				/* everyone else has a 4-bit color and 4-bit brightness */
				else
				{
					color = firstwd & 0x0f;
					statz = (firstwd >> 4) & 0x0f;
				}

				/* Tempest has the sparkle bit in bit 11 */
				if (vector_engine == USE_AVG_TEMPEST)
					sparkle = !(firstwd & 0x0800);

				/* Major Havoc/Alpha One have sparkle bit, xflip, and banking */
				else if (vector_engine == USE_AVG_MHAVOC || vector_engine == USE_AVG_ALPHAONE)
				{
					sparkle = firstwd & 0x0800;
					xflip = firstwd & 0x0400;
					vectorbank[1] = vectorram + 0x8000 + (((firstwd >> 8) & 3) * 0x2000);
				}

				/* BattleZone has a clipping circuit */
				else if (vector_engine == USE_AVG_BZONE)
				{
					INT32 newymin = (color == 0) ? 0x0050 : ymin;
					vg_vector_add_clip(xmin << 16, newymin << 16,
									xmax << 16, ymax << 16);
				}

				/* debugging */
				VGLOG((0, L"STAT: statz: %d color: %d", statz, color));
				if (xflip || sparkle)
					VGLOG((0, L"xflip: %02x  sparkle: %02x\n", xflip, sparkle));
				break;

			/* SCAL: set the scale factor */
			case SCAL:
				b = ((firstwd >> 8) & 7) + 8;
				l = ~firstwd & 0xff;

				scale = (l << 16) >> (b);

				/* Y-Window toggle for Major Havoc */
				if (vector_engine == USE_AVG_MHAVOC || vector_engine == USE_AVG_ALPHAONE)
					if (firstwd & 0x0800)
					{
						INT32 newymin = ymin;
						VGLOG((0, L"CLIP %d\n", firstwd & 0x0800));

						/* toggle the current state */
						ywindow = !ywindow;

						/* adjust accordingly */
						if (ywindow)
							newymin = (vector_engine == USE_AVG_MHAVOC) ? 0x0048 : 0x0083;
						vg_vector_add_clip(xmin << 16, newymin << 16,
										xmax << 16, ymax << 16);
					}

				/* debugging */
				VGLOG((0, L"bin: %d, lin: ", b));
				if (l > 0x80)
					VGLOG((0, L"(%d?)", l));
				else
					VGLOG((0, L"%d", l));
				VGLOG((0, L" scale: %f", (scale/(float)(1<<16))));
				break;

			/* CNTR: center the beam */
			case CNTR:

				/* delay stored in low 8 bits; normally is 0x40 */
				d = firstwd & 0xff;
				if (d != 0x40) VGLOG((0, L"%d", d));

				/* move back to the middle */
				currentx = xcenter;
				currenty = ycenter;
				avg_add_point(currentx, currenty, 0, 0);
				break;

			/* RTSL: return from subroutine */
			case RTSL:

				/* handle stack underflow */
				if (sp == 0)
				{
					VGLOG((0, L"\n*** Vector generator stack underflow! ***\n"));
					done = 1;
					sp = MAXSTACK - 1;
				}
				else
					sp--;

				/* pull the new PC from the stack */
				pc = stack[sp];

				/* debugging */
				if (firstwd & 0x1fff)
					VGLOG((0, L"(%d?)", firstwd & 0x1fff));
				break;

			/* HALT: all done! */
			case HALT:
				done = 1;

				/* debugging */
				if (firstwd & 0x1fff)
					VGLOG((0, L"(%d?)", firstwd & 0x1fff));
				break;

			/* JMPL: jump to a new program location */
			case JMPL:
				a = firstwd & 0x1fff;
				VGLOG((0, L"%4x", a));

				/* if a = 0x0000, treat as HALT */
				if (a == 0x0000)
					done = 1;
				else
					pc = a;
				break;

			/* JSRL: jump to a new program location as subroutine */
			case JSRL:
				a = firstwd & 0x1fff;
				VGLOG((0, L"%4x", a));

				/* if a = 0x0000, treat as HALT */
				if (a == 0x0000)
					done = 1;
				else
				{
					/* push the next PC on the stack */
					stack[sp] = pc;

					/* check for stack overflows */
					if (sp == (MAXSTACK - 1))
					{
						VGLOG((0, L"\n*** Vector generator stack overflow! ***\n"));
						done = 1;
						sp = 0;
					}
					else
						sp++;

					/* jump to the new location */
					pc = a;
				}
				break;

			/* anything else gets caught here */
			default:
				VGLOG((0, L"internal error\n"));
		}
		VGLOG((0, L"\n"));
	}

	/* return the total length of everything drawn */
	return total_length;
}



/*************************************
 *
 *  AVG execution/busy detection
 *
 ************************************/
#define AVG_DEFAULT_CPU 1512000
static INT32 avgdvg_cpu_rate = 0;
static INT32 avgdvg_halt_next = 0;
static INT32 last_cyc = 0;

static void avgdvg_clr_busy(INT32 /*dummy*/)
{
	busy = 0;
}

// cycle timing stuff for pot routines
static INT32 (*pCPUTotalCycles)() = NULL;

void avgdvg_checkhalt()
{
	if (avgdvg_halt_next && pCPUTotalCycles() - avgdvg_halt_next >= last_cyc) {
		avgdvg_halt_next = 0;
		avgdvg_clr_busy(0);
		//bprintf(0, _T("SYNC-HALT @ %d\n"), pCPUTotalCycles());
	}
}

INT32 avgdvg_done(void)
{
	avgdvg_checkhalt();
	return !busy;
}

void avgdvg_go()
{
	INT32 total_length;

	avgdvg_checkhalt();
	/* skip if already busy */
	if (busy)
		return;

	/* count vector updates and mark ourselves busy */
	busy = 1;

	/* DVG case */
	if (vector_engine == USE_DVG)
	{
		total_length = dvg_generate_vector_list();

		avgdvg_halt_next = pCPUTotalCycles();
		last_cyc = (INT32)((float)(((float)1512000 / 1000000000) * 4500) * total_length);
	}

	/* AVG case */
	else
	{
		total_length = avg_generate_vector_list();

		/* for Major Havoc, we need to look for empty frames */
		if (total_length > 1) {
			avgdvg_halt_next = pCPUTotalCycles();
			last_cyc = (INT32)((float)(((float)1512000 / 1000000000) * 2000) * total_length);
			//bprintf(0, _T("last cyc %d    total_length %d\n"), last_cyc, total_length);
		}
		else
		{
			busy = 0;
		}
	}
	vg_flush();
}

void avgdvg_reset()
{
	avgdvg_halt_next = 0;
	avgdvg_clr_busy(0);
	vector_reset();

	nvect = 0;
	has_clip = 0;
}

void avgdvg_scan(INT32 nAction, INT32 *)
{
	SCAN_VAR(avgdvg_halt_next);
	SCAN_VAR(last_cyc);
	SCAN_VAR(busy);
}


/*************************************
 *
 *  Vector generator init
 *
 ************************************/

INT32 avgdvg_init(INT32 vector_type, INT32 xsizemin, INT32 xsize, INT32 ysizemin, INT32 ysize)
{
	INT32 i;

	pCPUTotalCycles = NULL;

	/* 0 vector RAM size is invalid */
	if (vectorram_size == 0)
	{
		bprintf(0, _T("Error: vectorram_size not initialized\n"));
		return 1;
	}

	/* initialize the pages */
	for (i = 0; i < NUM_BANKS; i++)
		vectorbank[i] = vectorram + (i * BANK_SIZE);
	if (vector_type == USE_AVG_MHAVOC || vector_type == USE_AVG_ALPHAONE)
		vectorbank[1] = vectorram + 0x8000; //&memory_region(REGION_CPU1)[0x18000];

	/* set the engine type and validate it */
	vector_engine = vector_type;
	if (vector_engine < AVGDVG_MIN || vector_engine > AVGDVG_MAX)
	{
		bprintf(0, _T("Error: unknown Atari Vector Game Type\n"));
		return 1;
	}

	vectbuf = (vgvector *)BurnMalloc(sizeof(vgvector) * MAXVECT);

	if (!vectbuf)
	{
		bprintf(PRINT_ERROR, _T("Error: Unable to allocate AVG/DVG vector buffer, crashing in 3..2..1...\n"));
		return 1;
	}

	memset(vectbuf, 0, sizeof(vgvector) * MAXVECT);

	/* Star Wars is reverse-endian */
	if (vector_engine == USE_AVG_SWARS)
		flipword = 1;

	/* Quantum may be reverse-endian depending on the platform */
#ifndef LSB_FIRST
	else if (vector_engine==USE_AVG_QUANTUM)
		flipword = 1;
#endif

	/* everyone else is standard */
	else
		flipword = 0;

	/* clear the busy state */
	busy = 0;

	/* compute the min/max values */
	xmin = xsizemin;
	ymin = ysizemin;
	xmax = xsize;
	ymax = ysize;
	width = xmax - xmin;
	height = ymax - ymin;

	/* determine the center points */
	xcenter = ((xmax + xmin) / 2) << 16;
	ycenter = ((ymax + ymin) / 2) << 16;

	/* initialize to no avg flipping */
	flip_x = flip_y = 0;

	/* Tempest and Quantum have X and Y swapped */
	if ((vector_type == USE_AVG_TEMPEST) ||
		(vector_type == USE_AVG_QUANTUM))
		swap_xy = 1;
	else
		swap_xy = 0;

	for (INT32 j = 0; j < 32; j++)
		colorram[j] = j;

	return 0;
}

void avgdvg_init(INT32 type, UINT8 *vectram, INT32 vramromsize, INT32 (*pCPUCyclesCB)(), INT32 w, INT32 h)
{
	vectorram = vectram;
	vectorram_size = vramromsize; // entire allocated ram+romspace size of game!

	vector_init();
	vector_set_scale(w, h);

	avgdvg_init(type, 0, w, 0, h);
	pCPUTotalCycles = pCPUCyclesCB;
	avgdvg_cpu_rate = AVG_DEFAULT_CPU;
}

void avgdvg_set_cycles(INT32 cycles)
{
	avgdvg_cpu_rate = cycles;
}

void avgdvg_exit()
{
	vector_exit();
	BurnFree(vectbuf);
}

// save for later stuff:

#if 0

/*************************************
 *
 *  Video startup
 *
 ************************************/

VIDEO_START( dvg )
{
	return avgdvg_init(USE_DVG);
}


VIDEO_START( avg )
{
	return avgdvg_init(USE_AVG);
}


VIDEO_START( avg_starwars )
{
	return avgdvg_init(USE_AVG_SWARS);
}


VIDEO_START( avg_tempest )
{
	return avgdvg_init(USE_AVG_TEMPEST);
}


VIDEO_START( avg_mhavoc )
{
	return avgdvg_init(USE_AVG_MHAVOC);
}


VIDEO_START( avg_alphaone )
{
	return avgdvg_init(USE_AVG_ALPHAONE);
}


VIDEO_START( avg_bzone )
{
	return avgdvg_init(USE_AVG_BZONE);
}


VIDEO_START( avg_quantum )
{
	return avgdvg_init(USE_AVG_QUANTUM);
}


VIDEO_START( avg_redbaron )
{
	return avgdvg_init(USE_AVG_RBARON);
}



/*************************************
 *
 *  Palette generation
 *
 ************************************/

/* Black and White vector colors for Asteroids, Lunar Lander, Omega Race */
PALETTE_INIT( avg_white )
{
	int i;
	for (i = 0; i < 32; i++)
		colorram[i] = MAKE_RGB(0xff, 0xff, 0xff);
}


/* Basic 8 rgb vector colors for Tempest, Gravitar, Major Havoc etc. */
PALETTE_INIT( avg_multi )
{
	int i;
	for (i = 0; i < 32; i++)
		colorram[i] = VECTOR_COLOR111(i);
}



/*************************************
 *
 *  Color RAM handling
 *
 ************************************/

WRITE8_HANDLER( tempest_colorram_w )
{
	int bit3 = (~data >> 3) & 1;
	int bit2 = (~data >> 2) & 1;
	int bit1 = (~data >> 1) & 1;
	int bit0 = (~data >> 0) & 1;
	int r = bit1 * 0xee + bit0 * 0x11;
	int g = bit3 * 0xee;
	int b = bit2 * 0xee;

	colorram[offset] = MAKE_RGB(r, g, b);
}


WRITE8_HANDLER( mhavoc_colorram_w )
{
	int bit3 = (~data >> 3) & 1;
	int bit2 = (~data >> 2) & 1;
	int bit1 = (~data >> 1) & 1;
	int bit0 = (~data >> 0) & 1;
	int r = bit3 * 0xee + bit2 * 0x11;
	int g = bit1 * 0xee;
	int b = bit0 * 0xee;

	colorram[offset] = MAKE_RGB(r, g, b);
}


WRITE16_HANDLER( quantum_colorram_w )
{
	if (ACCESSING_LSB)
	{
		int bit3 = (~data >> 3) & 1;
		int bit2 = (~data >> 2) & 1;
		int bit1 = (~data >> 1) & 1;
		int bit0 = (~data >> 0) & 1;
		int r = bit3 * 0xee;
		int g = bit1 * 0xee + bit0 * 0x11;
		int b = bit2 * 0xee;

		colorram[offset & 0x0f] = MAKE_RGB(r, g, b);
	}
}


#endif
