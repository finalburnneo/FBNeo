/*##########################################################################

    atarimo.h

    Common motion object management functions for Atari raster games.

##########################################################################*/

#ifndef __ATARIMO__
#define __ATARIMO__


/*##########################################################################
    CONSTANTS
##########################################################################*/

/* maximum number of motion object processors */
#define ATARIMO_MAX				2

/* maximum objects per bank */
#define ATARIMO_MAXPERBANK		1024

/* shift to get to priority in raw data */
#define ATARIMO_PRIORITY_SHIFT	12
#define ATARIMO_PRIORITY_MASK	((~0 << ATARIMO_PRIORITY_SHIFT) & 0xffff)
#define ATARIMO_DATA_MASK		(ATARIMO_PRIORITY_MASK ^ 0xffff)



/*##########################################################################
    TYPES & STRUCTURES
##########################################################################*/

struct rectangle
{
	INT32 min_x;
	INT32 max_x;
	INT32 min_y;
	INT32 max_y;
};

/* callback for special processing */
typedef int (*atarimo_special_cb)(UINT16 *bitmap, const rectangle *clip, int code, int color, int xpos, int ypos, rectangle *mobounds);

/* description for a four-word mask */
struct atarimo_entry
{
	UINT16			data[4];
};

/* description of the motion objects */
struct atarimo_desc
{
	UINT8				gfxindex;			/* index to which gfx system */
	UINT8				banks;				/* number of motion object banks */
	UINT8				linked;				/* are the entries linked? */
	UINT8				split;				/* are the entries split? */
	UINT8				reverse;			/* render in reverse order? */
	UINT8				swapxy;				/* render in swapped X/Y order? */
	UINT8				nextneighbor;		/* does the neighbor bit affect the next object? */
	UINT16				slipheight;			/* pixels per SLIP entry (0 for no-slip) */
	UINT8				slipoffset;			/* pixel offset for SLIPs */
	UINT16				maxlinks;			/* maximum number of links to visit/scanline (0=all) */

	UINT16				palettebase;		/* base palette entry */
	UINT16				maxcolors;			/* maximum number of colors */
	UINT8				transpen;			/* transparent pen index */

	atarimo_entry		linkmask;			/* mask for the link */
	atarimo_entry		gfxmask;			/* mask for the graphics bank */
	atarimo_entry		codemask;			/* mask for the code index */
	atarimo_entry		codehighmask;		/* mask for the upper code index */
	atarimo_entry		colormask;			/* mask for the color */
	atarimo_entry		xposmask;			/* mask for the X position */
	atarimo_entry		yposmask;			/* mask for the Y position */
	atarimo_entry		widthmask;			/* mask for the width, in tiles*/
	atarimo_entry		heightmask;			/* mask for the height, in tiles */
	atarimo_entry		hflipmask;			/* mask for the horizontal flip */
	atarimo_entry		vflipmask;			/* mask for the vertical flip */
	atarimo_entry		prioritymask;		/* mask for the priority */
	atarimo_entry		neighbormask;		/* mask for the neighbor */
	atarimo_entry		absolutemask;		/* mask for absolute coordinates */

	atarimo_entry		specialmask;		/* mask for the special value */
	UINT16				specialvalue;		/* resulting value to indicate "special" */
	atarimo_special_cb	specialcb;			/* callback routine for special entries */
};

/* rectangle list */
struct atarimo_rect_list
{
	int					numrects;
	rectangle *         rect;
};


/*##########################################################################
    FUNCTION PROTOTYPES
##########################################################################*/

/* setup/shutdown */
int atarimo_init(int map, const atarimo_desc *desc);
void atarimo_exit();
UINT16 *atarimo_get_code_lookup(int map, int *size);
UINT8 *atarimo_get_color_lookup(int map, int *size);
UINT8 *atarimo_get_gfx_lookup(int map, int *size);

/* core processing */
UINT16 *atarimo_render(int map, const rectangle *cliprect, atarimo_rect_list *rectlist);

/* atrribute setters */
void atarimo_set_bank(int map, int bank);
void atarimo_set_palettebase(int map, int base);
void atarimo_set_xscroll(int map, int xscroll);
void atarimo_set_yscroll(int map, int yscroll);

/* atrribute getters */
int atarimo_get_bank(int map);
int atarimo_get_palettebase(int map);
int atarimo_get_xscroll(int map);
int atarimo_get_yscroll(int map);

/* write handlers */
//void atarimo_0_spriteram_w(INT32 offset, UINT16 mask, UINT16 data);
//WRITE16_HANDLER( atarimo_0_spriteram_expanded_w );
//WRITE16_HANDLER( atarimo_0_slipram_w ); // just use *atarimo_0_slipram

//void atarimo_1_spriteram_w(INT32 offset, UINT16 mask, UINT16 data);
//WRITE16_HANDLER( atarimo_1_slipram_w ); // just use *atarimo_1_slipram

void atarimo_apply_stain(UINT16 *pf, UINT16 *mo, INT32 x, INT32 /*y*/, INT32 maxx);

/*##########################################################################
    GLOBAL VARIABLES
##########################################################################*/

extern UINT16 *atarimo_0_spriteram;
extern UINT16 *atarimo_0_slipram;

extern UINT16 *atarimo_1_spriteram;
extern UINT16 *atarimo_1_slipram;

// fba-specific routines

void AtariMoWrite(INT32 map, INT32 offset, UINT16 data);
void AtariMoExpandedWrite(INT32 map, INT32 offset, UINT16 data);
void AtariMoInit(INT32 map, const atarimo_desc *desc);
void AtariMoExit();
INT32 AtariMoScan(INT32 nAction, INT32 *pnMin);
void AtariMoRender(INT32 map);
void AtariMoRender(INT32 map, atarimo_rect_list *rectlist);
void AtariMoApplyStain(UINT16 *pf, UINT16 *mo, INT32 x);
#endif
