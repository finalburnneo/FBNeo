// Minimal stand-in for the slice of FBNeo the Neo Geo 68K bankswitch functions
// touch, so those functions can be compiled and driven in isolation.
//
// Everything here is either a verbatim copy of the real definition (types,
// MAP_* values, BURN_ENDIAN_SWAP_INT16 on a little-endian host) or a recorder
// that stands in for a side effect the harness needs to observe
// (SekMapMemory).  Nothing here models emulation behaviour -- the functions
// under test are the real ones, pulled out of the real source by extract.py.
//
// NO ROM OR BIOS DATA IS USED ANYWHERE IN THIS HARNESS.  Every buffer is
// synthetic and every input is generated.

#ifndef FBNEO_BANK_SHIM_H
#define FBNEO_BANK_SHIM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>

typedef uint8_t  UINT8;
typedef int8_t   INT8;
typedef uint16_t UINT16;
typedef int16_t  INT16;
typedef uint32_t UINT32;
typedef int32_t  INT32;
typedef uint64_t UINT64;
typedef int64_t  INT64;

#define __fastcall

// src/burn/burn.h:306-312, verbatim
#define MAP_READ     1
#define MAP_WRITE    2
#define MAP_FETCHOP  4
#define MAP_FETCHARG 8
#define MAP_FETCH    (MAP_FETCHOP | MAP_FETCHARG)
#define MAP_ROM      (MAP_READ | MAP_FETCH)
#define MAP_RAM      (MAP_ROM | MAP_WRITE)

// src/burn/burn_endian.h:21 -- the little-endian arm of the real macro.  The
// harness host is little-endian; kf2k3blaWriteWordBankswitch is the only
// function under test that uses it, and it is exercised on that arm only.
#define BURN_ENDIAN_SWAP_INT16(x) (x)

#define MAX_SLOT 8

// ---------------------------------------------------------------------------
// Globals the functions under test reference

extern UINT8 *Neo68KROMActive;
extern UINT8 *PVCRAM;
extern UINT32 nNeo68KROMBank;
extern UINT32 nCodeSize[MAX_SLOT];
extern INT32  nNeoActiveSlot;

// ---------------------------------------------------------------------------
// SekMapMemory recorder

#define SHIM_MAX_CALLS 64

struct ShimMapCall {
	UINT8 *pMemory;
	UINT32 nStart;
	UINT32 nEnd;
	INT32  nType;
};

extern struct ShimMapCall g_shimCalls[SHIM_MAX_CALLS];
extern int g_shimCallCount;

INT32 SekMapMemory(UINT8 *pMemory, UINT32 nStart, UINT32 nEnd, INT32 nType);
void  SekOpen(INT32 nCPU);
void  SekClose();

// Byte offset of a recorded mapping relative to the ROM base.  Computed on
// uintptr_t so a wildly out-of-range bank (ms5plus can reach 0xFFFF0000) is
// still reported faithfully rather than wrapping.
static inline UINT64 ShimOffset(int i)
{
	return (UINT64)((uintptr_t)g_shimCalls[i].pMemory - (uintptr_t)Neo68KROMActive);
}

static inline UINT64 ShimWindowLen(int i)
{
	return (UINT64)g_shimCalls[i].nEnd - (UINT64)g_shimCalls[i].nStart + 1;
}

void ShimResetCalls();
void ShimInit();          // reserves the ROM address space, once
void ShimSetCodeSize(UINT32 nSize);

#endif // FBNEO_BANK_SHIM_H
