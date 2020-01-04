// Atari EE-ROM (X2212), impl. by dink

#include "burnint.h"
#include "x2212.h"

#define X2212_DEBUG     0

#define X2212_MAX       4
#define X2212_SIZE      0x100

#define X2212_IDLE      ( 0 )
#define X2212_STORE     (1 << 0)
#define X2212_RECALL    (1 << 1)
#define X2212_AUTOSTORE  (1 << 16)

struct x2212_chip {
	UINT8 *eerom;
	UINT8 *sram;
	UINT32 mode;
};

static struct x2212_chip x2212_chips[X2212_MAX];
static INT32 x2212_chipnum = 0;

UINT8 x2212_read(INT32 chip, UINT16 offset)
{
	return (x2212_chips[chip].sram[offset & 0xff] & 0x0f) | 0xf0;
}

void x2212_write(INT32 chip, UINT16 offset, UINT8 data)
{
	x2212_chips[chip].sram[offset & 0xff] = data & 0x0f;
}

static void store_internal(INT32 chip)
{
	memcpy(x2212_chips[chip].eerom, x2212_chips[chip].sram, X2212_SIZE);
}

void x2212_store(INT32 chip, INT32 state)
{
	if (state && (~x2212_chips[chip].mode & X2212_STORE)) {
		store_internal(chip);
		if (X2212_DEBUG) bprintf(0, _T("X2212 chip %d: store.\n"), chip);
	}

	x2212_chips[chip].mode = (x2212_chips[chip].mode & ~X2212_STORE) | ((state & 1) ? X2212_STORE : 0);
}

void x2212_recall(INT32 chip, INT32 state)
{
	if (state && (~x2212_chips[chip].mode & X2212_RECALL)) {
		memcpy(x2212_chips[chip].sram, x2212_chips[chip].eerom, X2212_SIZE);
		if (X2212_DEBUG) bprintf(0, _T("X2212 chip %d: recall.\n"), chip);
	}

	x2212_chips[chip].mode = (x2212_chips[chip].mode & ~X2212_RECALL) | ((state & 1) ? X2212_RECALL : 0);
}

void x2212_reset()
{
	for (INT32 i = 0; i < x2212_chipnum; i++) {
		memset(x2212_chips[i].sram,  0xff, X2212_SIZE);
		x2212_chips[i].mode = X2212_IDLE | (x2212_chips[i].mode & X2212_AUTOSTORE);
	}
}

void x2212_init(INT32 num_chips)
{
	x2212_chipnum = num_chips & 0xff;

	for (INT32 i = 0; i < x2212_chipnum; i++) {
		x2212_chips[i].eerom = (UINT8*)BurnMalloc(X2212_SIZE);
		x2212_chips[i].sram  = (UINT8*)BurnMalloc(X2212_SIZE);
		memset(x2212_chips[i].eerom, 0xff, X2212_SIZE);
		memset(x2212_chips[i].sram,  0xff, X2212_SIZE);

		if (num_chips & X2212_AUTOSTORE) {
			x2212_chips[i].mode = X2212_AUTOSTORE;
		}
	}

	x2212_reset();
}

void x2212_init_autostore(INT32 num_chips)
{
	x2212_init(num_chips | X2212_AUTOSTORE);
}

void x2212_exit()
{
	for (INT32 i = 0; i < x2212_chipnum; i++) {
		BurnFree(x2212_chips[i].eerom);
		BurnFree(x2212_chips[i].sram);
		x2212_chips[i].mode = X2212_IDLE;
	}

	x2212_chipnum = 0;
}

void x2212_scan(INT32 nAction, INT32 *pnMin)
{
	for (INT32 i = 0; i < x2212_chipnum; i++) {
		if (nAction & ACB_VOLATILE) {

			if (X2212_DEBUG) bprintf(0, _T("X2212 chip %d: scan volatile.\n"), i);

			ScanVar(x2212_chips[i].sram, X2212_SIZE, "X2212 SRAM");
			SCAN_VAR(x2212_chips[i].mode);
		}

		if (nAction & ACB_NVRAM) {
			if ((nAction & ACB_READ) && (x2212_chips[i].mode & X2212_AUTOSTORE)) {
				if (X2212_DEBUG) bprintf(0, _T("X2212 chip %d: NVRAM Write Auto-Store.\n"), i);
				store_internal(i);
			}

			if (X2212_DEBUG) bprintf(0, _T("X2212 chip %d: scan NVRAM.\n"), i);
			ScanVar(x2212_chips[i].eerom, X2212_SIZE, "X2212 EEROM");
		}
	}
}
