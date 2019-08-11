#include "sys16.h"
#include "fd1094.h"

#define S16_NUMCACHE 8

static UINT8 *fd1094_key; // the memory region containing key
static UINT16 *fd1094_cpuregion; // the CPU region with encrypted code
static UINT32  fd1094_cpuregionsize; // the size of this region in bytes

UINT16* s24_fd1094_userregion; // a user region where the current decrypted state is put and executed from
static UINT16* fd1094_cacheregion[S16_NUMCACHE]; // a cache region where S16_NUMCACHE states are stored to improve performance
static INT32 fd1094_cached_states[S16_NUMCACHE]; // array of cached state numbers
static INT32 fd1094_current_cacheposition; // current position in cache array

static INT32 fd1094_state;
static INT32 fd1094_selected_state;

static void (*fd1094_callback)(UINT8 *);

static INT32 nFD1094CPU = 0;

/* this function checks the cache to see if the current state is cached,
   if it is then it copies the cached data to the user region where code is
   executed from, if its not cached then it gets decrypted to the current
   cache position using the functions in fd1094.c */
static void fd1094_setstate_and_decrypt(INT32 state)
{
	switch (state & 0x300) {
		case 0x000:
		case FD1094_STATE_RESET:
			fd1094_selected_state = state & 0xff;
			break;
	}

	INT32 nActiveCPU = SekGetActive();

	fd1094_state = state;

	if (nActiveCPU == -1) {
		SekOpen(nFD1094CPU);
	} else {
		if (nActiveCPU != nFD1094CPU) {
			SekClose();
			SekOpen(nFD1094CPU);
		}
	}

	// force a flush of the prefetch cache
	m68k_set_reg(M68K_REG_PREF_ADDR, 0x1000);

	if (nActiveCPU == -1) {
		SekClose();
	} else {
		if (nActiveCPU != nFD1094CPU) {
			SekClose();
			SekOpen(nActiveCPU);
		}
	}

	/* set the FD1094 state ready to decrypt.. */
	state = fd1094_set_state(fd1094_key, state);

	/* first check the cache, if its cached we don't need to decrypt it, just copy */
	for (INT32 i = 0; i < S16_NUMCACHE; i++)
	{
		if (fd1094_cached_states[i] == state)
		{
			/* copy cached state */
			s24_fd1094_userregion = fd1094_cacheregion[i];
			if (nActiveCPU == -1) {
				SekOpen(nFD1094CPU);
				fd1094_callback((UINT8*)s24_fd1094_userregion);
				SekClose();
			} else {
				if (nActiveCPU == nFD1094CPU) {
					fd1094_callback((UINT8*)s24_fd1094_userregion);
				} else {
					SekClose();
					SekOpen(nFD1094CPU);
					fd1094_callback((UINT8*)s24_fd1094_userregion);
					SekClose();
					SekOpen(nActiveCPU);
				}
			}

			return;
		}
	}

	/* mark it as cached (because it will be once we decrypt it) */
	fd1094_cached_states[fd1094_current_cacheposition]=state;

	for (UINT32 addr = 0; addr < fd1094_cpuregionsize / 2; addr++)
	{
		UINT16 dat = fd1094_decode(addr,fd1094_cpuregion[addr], fd1094_key, 0);
		fd1094_cacheregion[fd1094_current_cacheposition][addr] = dat;
	}

	/* copy newly decrypted data to user region */
	s24_fd1094_userregion = fd1094_cacheregion[fd1094_current_cacheposition];
	if (nActiveCPU == -1) {
		SekOpen(nFD1094CPU);
		fd1094_callback((UINT8*)s24_fd1094_userregion);
		SekClose();
	} else {
		if (nActiveCPU == nFD1094CPU) {
			fd1094_callback((UINT8*)s24_fd1094_userregion);
		} else {
			SekClose();
			SekOpen(nFD1094CPU);
			fd1094_callback((UINT8*)s24_fd1094_userregion);
			SekClose();
			SekOpen(nActiveCPU);
		}
	}

	fd1094_current_cacheposition++;

	if (fd1094_current_cacheposition>=S16_NUMCACHE)
	{
#if 1 && defined FBA_DEBUG
		bprintf(PRINT_NORMAL, _T("out of cache, performance may suffer, increase S16_NUMCACHE!\n"));
#endif
		fd1094_current_cacheposition=0;
	}
}

/* Callback for CMP.L instructions (state change) */
static INT32 __fastcall fd1094_cmp_callback(UINT32 val, INT32 reg)
{
	if (reg == 0 && (val & 0x0000ffff) == 0x0000ffff) // ?
	{
		fd1094_setstate_and_decrypt((val & 0xffff0000) >> 16);
	}
	
	return 0;
}

/* Callback when the FD1094 enters interrupt code */
static INT32 __fastcall fd1094_int_callback (INT32 irq)
{
	fd1094_setstate_and_decrypt(FD1094_STATE_IRQ);
	return (0x60+irq*4)/4; // vector address
}

static INT32 __fastcall fd1094_rte_callback (void)
{
	fd1094_setstate_and_decrypt(FD1094_STATE_RTE);

	return 0;
}

void s24_fd1094_kludge_reset_values(void)
{
	INT32 nActiveCPU = SekGetActive();

	for (INT32 i = 0; i < 4; i++) {
		s24_fd1094_userregion[i] = fd1094_decode(i, fd1094_cpuregion[i], fd1094_key, 1);
	}

	if (nActiveCPU == -1) {
		SekOpen(nFD1094CPU);
	} else {
		if (nActiveCPU != nFD1094CPU) {
			SekClose();
			SekOpen(nFD1094CPU);
		}
	}
	fd1094_callback((UINT8*)s24_fd1094_userregion);
	if (nActiveCPU == -1) {
		SekClose();
	} else {
		if (nActiveCPU != nFD1094CPU) {
			SekClose();
			SekOpen(nActiveCPU);
		}
	}
}


/* function, to be called from MACHINE_RESET (every reset) */
void s24_fd1094_machine_init(void)
{
	if (!fd1094_key)
		return;

	fd1094_setstate_and_decrypt(FD1094_STATE_RESET);
	s24_fd1094_kludge_reset_values();

	SekOpen(nFD1094CPU);
	SekSetCmpCallback(fd1094_cmp_callback);
	SekSetRTECallback(fd1094_rte_callback);
	SekSetIrqCallback(fd1094_int_callback);
	SekClose();
}

/* startup function, to be called from DRIVER_INIT (once on startup) */
void s24_fd1094_driver_init(INT32 nCPU, INT32 /*cachesize*/, UINT8 *keybase, UINT8 *codebase, INT32 codebase_len, void (*cb)(UINT8*))
{
	INT32 i;

	nFD1094CPU = nCPU;

	fd1094_cpuregion = (UINT16*)codebase;
	fd1094_cpuregionsize = codebase_len;
	fd1094_callback = cb;

	if (nFD1094CPU >= 2) {
		bprintf(PRINT_ERROR, _T("Invalid CPU called for FD1094 Driver Init\n"));
	}

	fd1094_key = keybase;

	/* punt if no key; this allows us to be called even for non-FD1094 games */
	if (!fd1094_key)
		return;

	for (i = 0; i < S16_NUMCACHE; i++)
	{
		fd1094_cacheregion[i] = (UINT16*)BurnMalloc(fd1094_cpuregionsize);
	}

	/* flush the cached state array */
	for (i = 0; i < S16_NUMCACHE; i++) fd1094_cached_states[i] = -1;

	fd1094_current_cacheposition = 0;
	fd1094_state = -1;
}

void s24_fd1094_exit()
{
	if (!fd1094_key)
		return;

	nFD1094CPU = 0;

	for (INT32 i = 0; i < S16_NUMCACHE; i++) {
		BurnFree(fd1094_cacheregion[i]);
	}

	fd1094_current_cacheposition = 0;
}

void s24_fd1094_scan(INT32 nAction)
{
	if (!fd1094_key)
		return;

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(fd1094_selected_state);
		SCAN_VAR(fd1094_state);

		if (nAction & ACB_WRITE) {
			if (fd1094_state != -1)	{
				INT32 selected_state = fd1094_selected_state;
				INT32 state = fd1094_state;

				s24_fd1094_machine_init();

				fd1094_setstate_and_decrypt(selected_state);
				fd1094_setstate_and_decrypt(state);
			}
		}
	}
}
