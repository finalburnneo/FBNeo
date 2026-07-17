// =============================================================================
//  FBNeo SNES  -  SuperFX (GSU) coprocessor  -  public interface
// =============================================================================
//  Experimental GSU coprocessor core for the FBNeo LakeSnes SNES driver.
//
//  The emulation logic (81-instruction GSU core, the plot/rpix raster
//  pipeline, ROM/RAM buffer timing, cache window and MMIO map) is a clean-room
//  C re-implementation derived from the ares emulator's open-source GSU /
//  SuperFX modules:
//        ares/component/processor/gsu/*
//        ares/sfc/coprocessor/superfx/*
//  ares is distributed under the ISC licence; see license.txt for the upstream
//  copyright and licence text.  No proprietary or reverse-engineered material
//  is used - everything here is reconstructed from public, open-source code.
//
//  This is NOT a line-for-line transliteration.  ares' C++ object model
//  (Processor::GSU + SuperFX : Thread, virtual dispatch, nall BitField
//  registers, libco coroutine scheduling, its own bus/serializer) has been
//  fully dismantled and re-expressed against FBNeo conventions:
//    * every ares C++ class collapses into one flat C state struct,
//      SnesGsuState - no templates, namespaces or member functions;
//    * a single gsu.cpp / gsu.h pair replaces ares' multi-file chip layering;
//    * all buses, interrupts and the raster path bind to FBNeo's native
//      Snes bus, cpu_setIrq() hook and StateHandler save-state API;
//    * every symbol uses FBNeo's snes_gsu_* / SNES_GSU_* naming and the
//      UINT8/INT8/UINT16/... integer typedefs.
//
//  FBNeo mount / bridge glue (c) 2026 (see license.txt).
// =============================================================================

#ifndef GSU_H
#define GSU_H

#include "burnint.h"
#include "statehandler.h"

typedef struct Snes Snes;

// -- lifecycle (called from cart.cpp) --------------------------------------
//  mem		: Snes* context
//  rom		: linear cart ROM, romSize bytes
//  ram		: SuperFX save/work RAM (battery), ramSize bytes (64KB / 128KB)
//  hirom	: 0 = GSU-1 window set, 1 = GSU-2 extended window set
//  oscillator : GSU master-clock oscillator in Hz (21.44 MHz for GSU-1/GSU-2;
//               pass 0 to fall back to the S-CPU clock, used by MARIO CHIP 1).
//               ares encodes this per-board in its game DB; FBNeo passes it in
//               from snes_other.cpp so the GSU thread runs at the correct rate
//               relative to the S-CPU (NTSC 21.477 / PAL 21.281 MHz).
void snes_gsu_init(void* mem, UINT8* rom, INT32 romSize, UINT8* ram, INT32 ramSize, INT32 hirom, UINT32 oscillator);
void snes_gsu_reset();
void snes_gsu_exit();
void snes_gsu_run();
void snes_gsu_handleState(StateHandler* sh);

// -- S-CPU cartridge bus bridge (called from cart_readSuperFX / write) ------
// Full 24-bit cart address (bank << 16 | adr); decodes MMIO / ROM / Save-RAM.
UINT8 snes_gsu_cart_read(UINT32 address);
void  snes_gsu_cart_write(UINT32 address, UINT8 data);

#endif
