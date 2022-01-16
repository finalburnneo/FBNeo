#ifndef _BURN_ENDIAN_H
#define _BURN_ENDIAN_H

#ifdef LSB_FIRST
typedef union
{
	struct { UINT8 l,h,h2,h3; } b;
	struct { UINT16 l,h; } w;
	UINT32 d;
} PAIR;

#define BURN_ENDIAN_SWAP_INT8(x)				x
#define BURN_ENDIAN_SWAP_INT16(x)				x
#define BURN_ENDIAN_SWAP_INT32(x)				x
#define BURN_ENDIAN_SWAP_INT64(x)				x
#else

typedef union {
  struct { UINT8 h3,h2,h,l; } b;
  struct { UINT16 h,l; } w;
  UINT32 d;
} PAIR;

// Libogc doesn't have intrinsics or ASM macros defined for this
//#if defined(HW_RVL) || defined(GEKKO)
//#define __sthbrx(base,index,value) __asm__ volatile ("sthbrx       %0,%1,%2" : : "r"(value), "b%"(index), "r"(base) : "memory")
//#define __stwbrx(base,index,value) __asm__ volatile ("stwbrx       %0,%1,%2" : : "r"(value), "b%"(index), "r"(base) : "memory")
//#endif

// Xbox 360
#if defined(_XBOX)
#define BURN_ENDIAN_SWAP_INT8(x)				(x^1)
#define BURN_ENDIAN_SWAP_INT16(x)				(_byteswap_ushort(x))
#define BURN_ENDIAN_SWAP_INT32(x)				(_byteswap_ulong(x))
#define BURN_ENDIAN_SWAP_INT64(x)				(_byteswap_uint64(x))
// PS3
#elif defined(__PS3__)
#include <ppu_intrinsics.h>
#include <math.h>
#define BURN_ENDIAN_SWAP_INT8(x)				(x^1)
#define BURN_ENDIAN_SWAP_INT16(x)				({uint16_t tt; __sthbrx(&tt, x); tt;})
#define BURN_ENDIAN_SWAP_INT32(x)				({uint32_t tt; __stwbrx(&tt, x); tt;})
#define NO_64BIT_BYTESWAP
// GC/Wii/WiiU
//#elif defined(HW_RVL)
//#define BURN_ENDIAN_SWAP_INT8(x)				(x^1)
//#define BURN_ENDIAN_SWAP_INT16(x)				({uint16_t tt; __sthbrx(&tt, 0, x); tt;})
//#define BURN_ENDIAN_SWAP_INT32(x)				({uint32_t tt; __stwbrx(&tt, 0, x); tt;})
//#define NO_64BIT_BYTESWAP
// gcc/clang
#elif defined(__GNUC__)
#define BURN_ENDIAN_SWAP_INT8(x)				(x^1)
#define BURN_ENDIAN_SWAP_INT16(x)				(__builtin_bswap16(x))
#define BURN_ENDIAN_SWAP_INT32(x)				(__builtin_bswap32(x))
#define BURN_ENDIAN_SWAP_INT64(x)				(__builtin_bswap64(x))
// Anything else (swap16 & swap32 are kinda broken though)
#else
#define BURN_ENDIAN_SWAP_INT8(x)				(x^1)
#define BURN_ENDIAN_SWAP_INT16(x)				((((x) << 8) & 0xff00) | (((x) >> 8) & 0x00ff))
#define BURN_ENDIAN_SWAP_INT32(x)				((((x) << 24) & 0xff000000) | (((x) << 8) & 0x00ff0000) | (((x) >> 8) & 0x0000ff00) | (((x) >> 24) & 0x000000ff))
#define NO_64BIT_BYTESWAP
#endif

#ifdef NO_64BIT_BYTESWAP
typedef union {
   UINT64 ll;
   struct { UINT32 l, h; } l;
} BYTESWAP_INT64;

static inline UINT64 BURN_ENDIAN_SWAP_INT64(UINT64 x)
{
	BYTESWAP_INT64 r = {0};
	r.l.l = BURN_ENDIAN_SWAP_INT32(x);
	r.l.h = BURN_ENDIAN_SWAP_INT32(x >> 32);
	return r.ll;
}
#endif

#endif

#endif
