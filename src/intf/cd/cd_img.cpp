// FBNeo cd-img, TruRip .ccd/.sub/.img support by Jan Klaassen
// .bin/.cue re-work by dink
// .chd support: compressed CD image backend via libchdr (MAME project, BSD-3-Clause).
// The image IS the .chd file; no CUE parsing required.  Sector layout is
// detected from CHD header hunkbytes; 2048/2352/2448 byte sectors are all
// exposed to upper layers as 2352-byte raw mode-1 sectors.

#include "burner.h"
#include "burnint.h"
#include "cd_chd.h"
#include "cd_img.h"
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <malloc.h>
#define cdimgFseek _fseeki64
#define cdimgFtell _ftelli64
#else
#define cdimgFseek fseeko
#define cdimgFtell ftello
#endif

// cd_img internal functions, independent from interface
#include "cd_img.inc"

static INT32 cdimgGetSettings(InterfaceInfo* pInfo)
{
	return 0;
}

struct CDEmuDo cdimgDo = { cdimgExit, cdimgInit, cdimgStop, cdimgPlay, cdimgLoadSector, cdimgReadDataSector, cdimgReadTOC, cdimgReadQChannel, cdimgSetVolume, cdimgGetCurrentLBA, cdimgGetSoundBuffer, cdimgScan, cdimgGetSettings, _T("raw image CD emulation") };
