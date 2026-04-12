// pgm2.h - PGM2 (IGS036) platform header for FBNeo
// Covers orleg2, kov2nl, kov3

#pragma once

#include "tiles_generic.h"
#include "arm7/arm7core.h"
#include "arm9_intf.h"
#include "ymz770.h"

// ---------------------------------------------------------------------------
// Memory pointers (allocated in pgm2_run.cpp)
// ---------------------------------------------------------------------------
extern UINT8 *Pgm2IntROM;          // IGS036 internal boot ROM (16 KB at 0x00000000)
extern INT32  Pgm2IntROMLen;
extern UINT8 *Pgm2ArmROM;          // ARM9 code flash (external, at 0x10000000)
extern UINT8 *Pgm2ArmRAM;          // Main RAM (512 KB at 0x20000000)
extern UINT8 *Pgm2SprROM;          // Sprite mask + colour ROM
extern UINT8 *Pgm2TileROM;         // Tilemap/text graphics ROM
extern UINT8 *Pgm2SndROM;          // YMZ774 sample ROM
extern UINT8 *Pgm2PalRAM;          // (legacy, NULL — palette in VidRAM sub-regions)
extern UINT8 *Pgm2VidRAM;          // Combined video RAM block (0x130000 bytes)
extern UINT8 *Pgm2SharedRAM;       // (legacy, NULL — inside VidRAM)
extern UINT8 *Pgm2ExtRAM;          // Battery SRAM (64 KB at 0x02000000)
extern UINT8 *Pgm2Cards[4];          // IC card images (0x108 bytes each, 4 player slots)

extern INT32  Pgm2ArmROMLen;
extern INT32  Pgm2BgROMLen;
extern INT32  Pgm2SprROMLen;
extern INT32  Pgm2TileROMLen;
extern INT32  Pgm2SndROMLen;

// Sprite layout offsets (set by ROM loader, used by pgm2_draw.cpp)
extern INT32  Pgm2MaskROMOffset;   // byte offset in SprROM where mask data starts
extern INT32  Pgm2MaskROMLen;      // total mask data bytes
extern INT32  Pgm2ColourROMOffset; // byte offset in SprROM where colour data starts

// ---------------------------------------------------------------------------
// VidRAM sub-region pointers (set after allocation)
// Match MAME pgm2_map():
//   0x30000000  Sprite OAM        8 KB  (32-bit words, 4 per sprite)
//   0x30020000  BG videoram       8 KB  (32-bit tile entries)
//   0x30040000  FG videoram      24 KB  (32-bit tile entries, 8x8 text)
//   0x30060000  Sprite palette   16 KB  (32-bit xRGB888, 0x1000 entries)
//   0x30080000  BG palette        8 KB  (32-bit xRGB888, 0x800 entries)
//   0x300A0000  TX palette        2 KB  (32-bit xRGB888, 0x200 entries)
//   0x300C0000  Sprite zoom table 512 B (32-bit entries)
//   0x300E0000  Line scroll RAM   1 KB
//   0x30100000  Shared RAM       256 B  (MCU comms)
//   0x30120000  Video registers   64 B
// ---------------------------------------------------------------------------
extern UINT8  *Pgm2SpOAM;      // sprite OAM (raw 32-bit words, little-endian in hardware)
extern UINT8  *Pgm2BgVRAM;     // BG tilemap entries
extern UINT8  *Pgm2FgVRAM;     // FG (text) tilemap entries
extern UINT32 *Pgm2SpPal;      // sprite palette (xRGB888)
extern UINT32 *Pgm2BgPal;      // BG palette (xRGB888)
extern UINT32 *Pgm2TxPal;      // TX/FG palette (xRGB888)
extern UINT32 *Pgm2SpZoom;     // sprite zoom lookup table
extern UINT8  *Pgm2LineRAM;    // per-line X scroll
extern UINT8  *Pgm2SharedRAM2; // MCU shared RAM
extern UINT32 *Pgm2VideoRegs;  // video control registers
extern UINT16 Pgm2VideoReg14Lo;
extern UINT16 Pgm2VideoReg14Hi;
//   VideoRegs[0] = bgscroll (0x30120000) — lo16: X, hi16: Y
//   VideoRegs[2] = fgscroll (0x30120008) — lo16: X, hi16: Y
//   VideoRegs[3] = vidmode  (0x3012000C) — bits[17:16]: 0=320x240, 1=448x224, 2=512x240

// ---------------------------------------------------------------------------
// Input / misc
// ---------------------------------------------------------------------------
extern UINT8 Pgm2InputPort0[32];
extern UINT8 Pgm2InputPort1[32];
extern UINT8 Pgm2Dip[1];
extern UINT8 Pgm2Reset;

// ---------------------------------------------------------------------------
// Callbacks for game-specific init / reset / scan (set by d_pgm2.cpp)
// ---------------------------------------------------------------------------
extern void (*pPgm2InitCallback)();
extern void (*pPgm2ResetCallback)();
extern INT32 (*pPgm2ScanCallback)(INT32 nAction, INT32 *pnMin);

// ---------------------------------------------------------------------------
// Shared platform functions
// ---------------------------------------------------------------------------
INT32 pgm2Init();
INT32 pgm2Exit();
INT32 pgm2Frame();
INT32 pgm2DoReset();
INT32 pgm2Scan(INT32 nAction, INT32 *pnMin);
void pgm2SetSpeedhack(UINT32 id, UINT32 addr, UINT32 pc1, UINT32 pc2 = 0, UINT32 pc3 = 0, UINT32 pc4 = 0);
void pgm2EnableKov3Module(const UINT8 *key, const UINT8 *sum, UINT32 addrXor, UINT16 dataXor);
void pgm2DisableKov3Module();
void pgm2SetStorageRomIndices(INT32 cardRomIndex, INT32 sramRomIndex);
void pgm2SetArmRomIndex(INT32 armRomIndex);
void pgm2SetCardRomIndex(INT32 slot, INT32 index);
void pgm2SetMaxCardSlots(INT32 count);
INT32 pgm2GetCardRomTemplate(UINT8* buffer, INT32 maxSize);
void pgm2SetRamRomBoard(INT32 ramSize);
void pgm2SetRefreshRate(double hz);

// Per-slot card state (exported for memcard UI)
extern INT32  Pgm2MaxCardSlots;
extern INT32  Pgm2ActiveCardSlot;
extern bool   Pgm2CardInserted[4];

extern UINT8  CardlessHack;

// pgm2_draw.cpp
void pgm2InitDraw();
void pgm2ExitDraw();
INT32 pgm2DoDraw();
void pgm2SnapshotOam();     // call at vblank to snapshot sprite OAM
void pgm2SnapshotScroll();  // call at vblank (before IRQ) to snapshot BG/FG scroll + lineram
void pgm2ScanDraw(INT32 nAction);  // save/restore OAM double-buffer + scroll snapshot
