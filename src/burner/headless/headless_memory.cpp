// Memory access for cheats, cheat search, and achievements (FinalBurn Neo headless).
// Author: Leonardo Kasperavičius
//
// Built originally to support OpenEmu-Silicon
// (https://github.com/OpenEmu-Silicon/OpenEmu-Silicon).
//
// Thin C wrappers over FBNeo's cheat-engine memory interface (src/burn/cheat.cpp).
// All addresses are in the main CPU (cpu 0) hardware address space — the same
// space FBNeo's own cheat and achievement support uses, so a value found by a
// memory search can be written back by address.
//
// Subject to the FB Neo license (see src/license.txt).

#include "burner.h"
#include "state.h"      // BurnArea, BurnAreaScan, BurnAcb, ACB_FULLSCAN
#include "fbheadless.h"

unsigned int FBHeadlessGetMainMemorySize(void)
{
	return GetMainMemorySize();
}

unsigned int FBHeadlessReadMemoryRange(unsigned int address, unsigned char *out, unsigned int length)
{
	return ReadMemoryRange(address, out, length);
}

// Standard byte order: byte i lives at address+i. littleEndian controls which end
// is the least-significant byte. (FBNeo's own Read/WriteValueAtHardwareAddress use
// an MSB-first, address-decrementing convention — not exposed here to avoid surprises.)
unsigned int FBHeadlessReadMemory(unsigned int address, int size, int littleEndian)
{
	if (!bDrvOkay || size < 1 || size > 4)
		return 0;
	unsigned char b[4] = { 0, 0, 0, 0 };
	ReadMemoryRange(address, b, (unsigned int)size);
	unsigned int value = 0;
	for (int i = 0; i < size; i++) {
		int shift = littleEndian ? (8 * i) : (8 * (size - 1 - i));
		value |= (unsigned int)b[i] << shift;
	}
	return value;
}

int FBHeadlessWriteMemory(unsigned int address, unsigned int value, int size, int littleEndian)
{
	if (!bDrvOkay || size < 1 || size > 4)
		return 0;
	for (int i = 0; i < size; i++) {
		int shift = littleEndian ? (8 * i) : (8 * (size - 1 - i));
		unsigned char byte = (value >> shift) & 0xFF;
		WriteValueAtHardwareAddress(address + i, byte, 1, 1); // single byte, no endian ambiguity
	}
	return 1;
}

int FBHeadlessIsAddressValid(unsigned int address)
{
	return IsHardwareAddressValid(address) ? 1 : 0;
}

// --- Achievements memory map ---
//
// RetroAchievements sets are authored against the flat main-RAM buffer FBNeo's
// libretro port exposes (see CheevosInit / StateGetMainRamAcb in retro_memory.cpp),
// NOT the CPU hardware address space. Find the same RAM area(s) by driver hardware
// type and expose them at the same RA base addresses the libretro port uses.

#define FBH_MAX_RA_REGIONS 10
typedef struct { unsigned int start; unsigned int len; unsigned char *ptr; } FBHRaRegion;
static FBHRaRegion s_raRegions[FBH_MAX_RA_REGIONS];
static int s_raRegionCount = 0;
static int s_raBigEndian = 0; // main CPU multibyte order (68K etc. = big-endian)

static void fbh_ra_add(unsigned int start, unsigned int len, void *ptr)
{
	if (s_raRegionCount >= FBH_MAX_RA_REGIONS || ptr == NULL || len == 0)
		return;
	s_raRegions[s_raRegionCount].start = start;
	s_raRegions[s_raRegionCount].len = len;
	s_raRegions[s_raRegionCount].ptr = (unsigned char *)ptr;
	s_raRegionCount++;
}

static INT32 __cdecl fbh_ra_acb(struct BurnArea *pba)
{
	if (!pba->szName)
		return 0;

	switch (BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) {
		case HARDWARE_CAPCOM_CPS1:
		case HARDWARE_CAPCOM_CPS1_QSOUND:
		case HARDWARE_CAPCOM_CPS1_GENERIC:
		case HARDWARE_CAPCOM_CPSCHANGER:
		case HARDWARE_CAPCOM_CPS2:
			if (strcmp(pba->szName, "CpsRamFF") == 0) fbh_ra_add(0, pba->nLen, pba->Data);
			return 0;
		case HARDWARE_CAPCOM_CPS3:
			if (strcmp(pba->szName, "Main RAM") == 0) fbh_ra_add(0, pba->nLen, pba->Data);
			return 0;
		case HARDWARE_SNK_NEOCD:
			if (strcmp(pba->szName, "68K program RAM") == 0) fbh_ra_add(0x00000000, pba->nLen, pba->Data);
			if (strcmp(pba->szName, "Memory card") == 0)     fbh_ra_add(0x00800000, pba->nLen, pba->Data);
			return 0;
		case HARDWARE_SNK_NEOGEO:
		case HARDWARE_IGS_PGM:
			if (strcmp(pba->szName, "68K RAM") == 0) fbh_ra_add(0, pba->nLen, pba->Data);
			return 0;
		case HARDWARE_PSIKYO:
			if (strcmp(pba->szName, "All RAM") == 0 || strcmp(pba->szName, "68K RAM") == 0) fbh_ra_add(0, pba->nLen, pba->Data);
			return 0;
		case HARDWARE_CAVE_68K_Z80:
		case HARDWARE_CAVE_68K_ONLY:
			if (strcmp(pba->szName, "RAM") == 0 || strcmp(pba->szName, "68K RAM") == 0) fbh_ra_add(0, pba->nLen, pba->Data);
			return 0;
		case HARDWARE_SEGA_MEGADRIVE:
			if (strcmp(pba->szName, "RAM") == 0)    fbh_ra_add(0x00FF0000, pba->nLen, pba->Data);
			if (strcmp(pba->szName, "NV RAM") == 0) fbh_ra_add(0x00000000, pba->nLen, pba->Data);
			return 0;
		case HARDWARE_SEGA_MASTER_SYSTEM:
		case HARDWARE_SEGA_GAME_GEAR:
			if (strcmp(pba->szName, "sms") == 0) fbh_ra_add(0, pba->nLen, pba->Data);
			return 0;
		case HARDWARE_NES:
		case HARDWARE_FDS:
		case HARDWARE_NVS:
			if (strcmp(pba->szName, "CPU Ram") == 0)  fbh_ra_add(0x00000000, pba->nLen, pba->Data);
			if (strcmp(pba->szName, "Work Ram") == 0) fbh_ra_add(0x00006000, pba->nLen, pba->Data);
			return 0;
		case HARDWARE_SNK_NGP:
		case HARDWARE_SNK_NGPC:
			if (strcmp(pba->szName, "Main Ram") == 0)   fbh_ra_add(0x00004000, pba->nLen, pba->Data);
			if (strcmp(pba->szName, "Shared Ram") == 0) fbh_ra_add(0x00007000, pba->nLen, pba->Data);
			return 0;
		case HARDWARE_TOAPLAN_RAIZING:
			if (strcmp(pba->szName, "All Ram") == 0 || strcmp(pba->szName, "All RAM") == 0 || strcmp(pba->szName, "RAM") == 0) fbh_ra_add(0, pba->nLen, pba->Data);
			return 0;
		default:
			// Most other drivers expose main RAM as "All Ram" / "All RAM".
			if (strcmp(pba->szName, "All Ram") == 0 || strcmp(pba->szName, "All RAM") == 0) fbh_ra_add(0, pba->nLen, pba->Data);
			return 0;
	}
}

// Rebuild the achievement memory map. Call after a driver loads or resets, since
// DrvInit reallocates the RAM buffers (the cached pointers would otherwise dangle).
void FBHeadlessCheevosInit(void)
{
	INT32 nMin = 0;
	s_raRegionCount = 0;
	s_raBigEndian = 0;
	if (!bDrvOkay)
		return;
	s_raBigEndian = GetMainMemoryBigEndian();
	BurnAcb = fbh_ra_acb;
	BurnAreaScan(ACB_FULLSCAN, &nMin);
}

unsigned int FBHeadlessReadAchievementMemory(unsigned int address, unsigned char *out, unsigned int length)
{
	if (out == NULL)
		return 0;

	unsigned int i;
	for (i = 0; i < length; i++) {
		unsigned int a = address + i;
		int found = 0;
		for (int r = 0; r < s_raRegionCount; r++) {
			if (a >= s_raRegions[r].start && a < s_raRegions[r].start + s_raRegions[r].len) {
				out[i] = s_raRegions[r].ptr[a - s_raRegions[r].start];
				found = 1;
				break;
			}
		}
		if (!found)
			return i; // stop at the first unmapped address
	}
	return length;
}

// Write a value into the achievement memory map (used to apply cheats). Bytes land
// at address+i for littleEndian, or MSB-first for big-endian. Returns 1 if anything
// was written. Direct buffer writes — the CPU sees them next frame (plain RAM).
int FBHeadlessWriteAchievementMemory(unsigned int address, unsigned int value, int size, int littleEndian)
{
	if (size < 1 || size > 4)
		return 0;
	int wrote = 0;
	for (int i = 0; i < size; i++) {
		unsigned int a = littleEndian ? (address + i) : (address + (size - 1 - i));
		unsigned char byte = (value >> (8 * i)) & 0xFF;
		for (int r = 0; r < s_raRegionCount; r++) {
			if (a >= s_raRegions[r].start && a < s_raRegions[r].start + s_raRegions[r].len) {
				s_raRegions[r].ptr[a - s_raRegions[r].start] = byte;
				wrote = 1;
				break;
			}
		}
	}
	return wrote;
}

// Enumerate the mapped regions so the host can present cheat-search memory.
int FBHeadlessGetMemoryRegionCount(void)
{
	return s_raRegionCount;
}

int FBHeadlessGetMemoryRegion(int index, unsigned int *start, unsigned int *length, int *bigEndian)
{
	if (index < 0 || index >= s_raRegionCount)
		return 0;
	if (start)     *start = s_raRegions[index].start;
	if (length)    *length = s_raRegions[index].len;
	if (bigEndian) *bigEndian = s_raBigEndian;
	return 1;
}
