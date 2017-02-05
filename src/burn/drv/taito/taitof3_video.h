extern void TaitoF3DrawCommon(INT32 scanline_start);
extern void TaitoF3VideoInit();
extern void TaitoF3VideoExit();
extern void TaitoF3VideoReset();

extern INT32 f3_game;
extern UINT32 extended_layers;
extern UINT32 sprite_lag;

extern UINT8 *TaitoF3CtrlRAM;
extern UINT8 *TaitoF3PfRAM;
extern UINT8 *TaitoF3LineRAM;

extern UINT16 *bitmap_layer[10];
extern UINT8 *bitmap_flags[10];
extern INT32 bitmap_width[8];
extern UINT32 *output_bitmap;

extern UINT8 *tile_opaque_sp;
extern UINT8 *tile_opaque_pf[8];
extern UINT8 *dirty_tiles;
extern UINT8 dirty_tile_count[10];

extern void (*pPaletteUpdateCallback)(UINT16);
extern UINT8 TaitoF3PalRecalc;

enum {
	RINGRAGE = 0,
	ARABIANM,
	RIDINGF,
	GSEEKER,
	TRSTAR,
	GUNLOCK,
	TWINQIX,
	UNDRFIRE,
	SCFINALS,  // - coin inputs don't work, service coin ok.
	LIGHTBR,
	KAISERKN,
	DARIUSG,
	BUBSYMPH,
	SPCINVDX,
	HTHERO95,
	QTHEATER,
	EACTION2,
	SPCINV95,
	QUIZHUHU,  //  - missing text is normal.
	PBOBBLE2,
	GEKIRIDO,
	KTIGER2,
	BUBBLEM,
	CLEOPATR,
	PBOBBLE3,
	ARKRETRN,
	KIRAMEKI,  // - has extra 68k rom that is banked for sound, not hooked up yet,
	PUCHICAR,
	PBOBBLE4,
	POPNPOP,
	LANDMAKR,
	RECALH,
	COMMANDW,
	TMDRILL
};
