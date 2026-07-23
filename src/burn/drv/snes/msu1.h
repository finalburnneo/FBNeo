// =============================================================================
//  FBNeo SNES  -  MSU-1 media coprocessor  -  public interface
// =============================================================================

#ifndef MSU1_H
#define MSU1_H

#define MSU1_DEBUG	0

#include "burnint.h"
#include "statehandler.h"

typedef struct MsuFile {
	void*  ctx;									// opaque backend handle (memory cursor, FILE*, ...)
	UINT8  (*read  )(void* ctx);				// read one byte at cursor, advance cursor
	void   (*seek  )(void* ctx, UINT32 offset);	// set absolute cursor position
	INT32  (*end   )(void* ctx);				// nonzero once cursor is at/past EOF
	UINT32 (*size  )(void* ctx);				// total size in bytes (0 if closed/unknown)
	INT32  (*isOpen)(void* ctx);				// nonzero while a backing store is attached
} MsuFile;

typedef INT32 (*MsuDataOpen)(MsuFile* out);
typedef INT32 (*MsuAudioOpen)(UINT16 track, MsuFile* out);

void snes_msu1_setBackend(MsuDataOpen dataOpen, MsuAudioOpen audioOpen);

// -----------------------------------------------------------------------------
//  Lifecycle  (called from cart.cpp)
// -----------------------------------------------------------------------------
void snes_msu1_init();
void snes_msu1_reset();
void snes_msu1_exit();
void snes_msu1_handleState(StateHandler* sh);

// -----------------------------------------------------------------------------
//  S-CPU bus bridge  (called from the MSU-1 cart read/write path)
// -----------------------------------------------------------------------------
//  MSU-1 decodes the eight I/O ports $2000-$2007 in banks $00-$3f / $80-$bf.
//  address is the full 24-bit bus address; only the low 3 bits select a port.
UINT8 snes_msu1_read(UINT32 address, UINT8 openbus);
void  snes_msu1_write(UINT32 address, UINT8 data);

// -----------------------------------------------------------------------------
//  Audio
// -----------------------------------------------------------------------------
void snes_msu1_mixSamples(INT16* out, INT32 samples, INT32 outRate, INT32 dspMuted);

#endif
