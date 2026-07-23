// =============================================================================
//  FBNeo SNES  -  MSU-1 audio decoders  -  third-party implementation unit
// =============================================================================

// dr_wav: full implementation (nobody else defines it).
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#ifndef STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_IMPLEMENTATION
#endif
#include "stb_vorbis.c"
