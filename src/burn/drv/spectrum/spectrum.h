#include "tiles_generic.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "dac.h"
#include "ay8910.h"

#define SPEC_NO_SNAPSHOT			0
#define SPEC_SNAPSHOT_SNA			1
#define SPEC_SNAPSHOT_Z80			2

#define SPEC_Z80_SNAPSHOT_INVALID	0
#define SPEC_Z80_SNAPSHOT_48K_OLD	1
#define SPEC_Z80_SNAPSHOT_48K		2
#define SPEC_Z80_SNAPSHOT_SAMRAM	3
#define SPEC_Z80_SNAPSHOT_128K		4
#define SPEC_Z80_SNAPSHOT_TS2068	5

// d_spectrum.cpp
extern UINT8 *SpecSnapshotData;
extern UINT8 *SpecZ80Rom;
extern UINT8 *SpecVideoRam;
extern UINT8 *SpecZ80Ram;

extern UINT8 nPortFEData;
extern INT32 nPort7FFDData;

void spectrum_UpdateScreenBitmap(bool);
void spectrum_UpdateBorderBitmap();
void spectrum_128_update_memory();

// spectrum_states.cpp
void SpecLoadSNASnapshot();
void SpecLoadZ80Snapshot();
