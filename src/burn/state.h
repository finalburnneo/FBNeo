#if !defined(_STATE_H)

#ifdef __cplusplus
 extern "C" {
#endif

#if !defined(__cplusplus) && defined(_XBOX)
#define C_INLINE _inline
#else
#ifndef C_INLINE
#define C_INLINE inline
#endif
#endif

/* Scan driver data */
INT32 BurnAreaScan(INT32 nAction, INT32* pnMin); // burn.cpp

/* ReWind */
void StateRewindInit();
void StateRewindExit();
void StateRewindReInit();
void StateRewindReset();
void StateRewindDoFrame(INT32 bDoRewind, INT32 bDoCancel, INT32 bIsPaused);

/* Run-A-head */
void StateRunAheadInit();
void StateRunAheadExit();
void StateRunAheadSave();
void StateRunAheadLoad();

/* flags to use for nAction */
#define ACB_READ				(1<<0)
#define ACB_WRITE				(1<<1)

#define ACB_MEMORY_ROM			(1<<2)
#define ACB_NVRAM				(1<<3)
#define ACB_MEMCARD				(1<<4)
#define ACB_MEMCARD_ACTION		(1<<14) // Insertion / Removal action
#define ACB_MEMORY_RAM			(1<<5)
#define ACB_DRIVER_DATA			(1<<6)

#define ACB_RUNAHEAD			(1<<7) // for single instance runahead
#define ACB_2RUNAHEAD			(1<<8) // for second instance runahead
#define ACB_NET_OPT				(1<<9) // for netplay

#define ACB_FULLSCAN	(ACB_NVRAM | ACB_MEMCARD | ACB_MEMORY_RAM | ACB_DRIVER_DATA)

#define ACB_ACCESSMASK	(ACB_READ | ACB_WRITE)
#define ACB_TYPEMASK	(ACB_MEMORY_ROM | ACB_NVRAM | ACB_MEMCARD | ACB_MEMORY_RAM | ACB_DRIVER_DATA)

#define ACB_VOLATILE    (ACB_MEMORY_RAM | ACB_DRIVER_DATA)

/* Structure used for area scanning */
struct BurnArea { void *Data; UINT32 nLen; INT32 nAddress; char *szName; };

/* Application-defined callback for processing the area */
extern INT32 (__cdecl *BurnAcb) (struct BurnArea* pba);

/* Scan a small variable or structure */
C_INLINE static void ScanVar(void* pv, INT32 nSize, char* szName)
{
	struct BurnArea ba;
	memset(&ba, 0, sizeof(ba));
	ba.Data   = pv;
	ba.nLen   = nSize;
	ba.szName = szName;
	BurnAcb(&ba);
}

// scan a variable, chunk of memory, pointerless struct, etc.
#define SCAN_VAR(x) ScanVar(&x, sizeof(x), #x)
// scan a memory offset - to safely state-ify pointers (see burn/drv/neogeo/neo_run.cpp or cpu/tlcs900/tlcs900.cpp)
#define SCAN_OFF(x, y, a) { INT32 n = x - y; ScanVar(&n, sizeof(n), #x); if (a & ACB_WRITE) { x = y + n; } }

#ifdef OSD_CPU_H
 /* wrappers for the MAME savestate functions (used by the FM sound cores) */
 void state_save_register_func_postload(void (*pFunction)());

 void state_save_register_INT8(const char* module, INT32 instance, const char* name, INT8* val, unsigned size);
 void state_save_register_UINT8(const char* module, INT32 instance, const char* name, UINT8* val, unsigned size);
 void state_save_register_INT16(const char* module, INT32 instance, const char* name, INT16* val, unsigned size);
 void state_save_register_UINT16(const char* module, INT32 instance, const char* name, UINT16* val, unsigned size);
 void state_save_register_INT32(const char* module, INT32 instance, const char* name, INT32* val, unsigned size);
 void state_save_register_UINT32(const char* module, INT32 instance, const char* name, UINT32* val, unsigned size);

 void state_save_register_int(const char* module, INT32 instance, const char* name, INT32* val);
 void state_save_register_float(const char* module, INT32 instance, const char* name, float* val, unsigned size);
 void state_save_register_double(const char* module, INT32 instance, const char* name, double* val, unsigned size);
#endif

#ifdef __cplusplus
 }
#endif

#define _STATE_H

#endif /* _STATE_H */
