// =============================================================================
//  ST018 (Seta ARMv3 / ARM7TDMI) coprocessor for FBNeo
// =============================================================================

#include "st018.h"
#include "snes.h"
#include "cart.h"
#include "burnint.h"
#include "arm7_intf.h"
#include <string.h>

// -----------------------------------------------------------------------------
//  ST018 bus offsets (ARM side)
// -----------------------------------------------------------------------------
#define ST018_PROM_BASE   0x00000000u   // 128KB on-die program ROM
#define ST018_DROM_BASE   0xa0000000u   // 32KB  on-die data ROM
#define ST018_PRAM_BASE   0xe0000000u   // 16KB  program RAM
#define ST018_MMIO_BASE   0x40000000u   // bridge mailbox window

#define ST018_PROM_SIZE   0x20000       // 128KB
#define ST018_DROM_SIZE   0x08000       // 32KB
#define ST018_PRAM_SIZE   0x04000       // 16KB

#define ST018_BOOT_HOLD   65536

// -----------------------------------------------------------------------------
//  State
// -----------------------------------------------------------------------------
static Snes*  s_snes       = NULL;

static UINT8* s_prom       = NULL;	// 128KB (points into firmware image)
static UINT8* s_drom       = NULL;	// 32KB  (points into firmware image)
static UINT8  s_pram[ST018_PRAM_SIZE];

static INT32  s_haveArm    = 0;		// ARM core successfully initialised
static INT32  s_running    = 0;		// ARM currently clocked (post-boot-hold)

// Host<->ARM bridge (ares ARMDSP::Bridge)
static UINT8  s_c2a_ready;			// CPU -> ARM byte pending
static UINT8  s_c2a_data;
static UINT8  s_a2c_ready;			// ARM -> CPU byte pending
static UINT8  s_a2c_data;
static UINT32 s_timer;				// 24-bit down-counter, ticked each ARM step
static UINT32 s_timerlatch;
static UINT8  s_reset;				// bridge reset line held by the S-CPU
static UINT8  s_ready;				// ARM has passed its boot-hold
static UINT8  s_signal;				// ARM -> CPU signal flag

static UINT32 s_armFreq;
static UINT64 s_clockBase;			// snes->cycles at last catch-up
static UINT64 s_clockFp;			// fixed-point accumulator (armFreq * delta)

// -----------------------------------------------------------------------------
//  Bridge status byte (ares Bridge::status)
// -----------------------------------------------------------------------------
static UINT8 st018_status()
{
	UINT8 data = 0;
	if (s_a2c_ready) data |= 0x01;	// bit0: ARM -> CPU byte ready
	if (s_signal)    data |= 0x04;	// bit2: ARM signalled
	if (s_c2a_ready) data |= 0x08;	// bit3: CPU -> ARM byte ready
	if (s_ready)     data |= 0x80;	// bit7: ARM booted / ready
	return data;
}

// -----------------------------------------------------------------------------
//  ARM-side bridge MMIO  (0x40000000 window)  -  served via Arm7 callbacks
// -----------------------------------------------------------------------------

static UINT32 st018_arm_read_long(UINT32 address)
{
	switch (address & 0xe000003fu) {
		case 0x40000010u:																				// read CPU -> ARM mailbox
			if (s_c2a_ready) {
				s_c2a_ready = 0;
				return s_c2a_data;
			}
			return 0;
		case 0x40000020u:																				// read bridge status
			return st018_status();
	}
	return 0;
}

static void st018_arm_write_long(UINT32 address, UINT32 word)
{
	word &= 0xff;
	switch (address & 0xe000003fu) {
		case 0x40000000u:																				// write ARM -> CPU mailbox
			s_a2c_ready = 1;
			s_a2c_data  = (UINT8)word;
			break;
		case 0x40000010u:																				// raise signal to CPU
			s_signal    = 1;
			break;
		case 0x40000020u: s_timerlatch = (s_timerlatch & 0xffff00u) | ((word & 0xff) <<  0); break;
		case 0x40000024u: s_timerlatch = (s_timerlatch & 0xff00ffu) | ((word & 0xff) <<  8); break;
		case 0x40000028u: s_timerlatch = (s_timerlatch & 0x00ffffu) | ((word & 0xff) << 16); break;
		case 0x4000002cu: s_timer      = s_timerlatch;                                       break;		// arm the down-counter
	}
}

static UINT8 st018_arm_read_byte(UINT32 address)
{
	return (UINT8)st018_arm_read_long(address);
}

static UINT16 st018_arm_read_word(UINT32 address)
{
	return (UINT16)st018_arm_read_long(address);
}

static void st018_arm_write_byte(UINT32 address, UINT8 data)
{
	st018_arm_write_long(address, data);
}

static void st018_arm_write_word(UINT32 address, UINT16 data)
{
	st018_arm_write_long(address, data);
}

// -----------------------------------------------------------------------------
//  S-CPU-side bus bridge  ($3800-$3807, mirrored; a0 ignored -> & 0xff06)
// -----------------------------------------------------------------------------

UINT8 snes_st018_read(UINT32 address, UINT8 openbus)
{
	// keep the ARM caught up so the mailbox reflects its latest writes
	snes_st018_run();

	UINT8 data = 0x00;
	UINT32 reg = address & 0xff06u;

	if (reg == 0x3800) {			// read ARM -> CPU mailbox
		if (s_a2c_ready) {
			s_a2c_ready = 0;
			data = s_a2c_data;
		}
	} else if (reg == 0x3802) {		// acknowledge / clear signal
		s_signal = 0;
	} else if (reg == 0x3804) {		// bridge status
		data = st018_status();
	}

	(void)openbus;
	return data;
}

void snes_st018_write(UINT32 address, UINT8 data)
{
	snes_st018_run();

	UINT32 reg = address & 0xff06u;

	if (reg == 0x3802) {			// write CPU -> ARM mailbox
		s_c2a_ready = 1;
		s_c2a_data  = data;
	} else if (reg == 0x3804) {		// bridge reset line (bit0)
		data &= 1;
		if (!s_reset && data) {
			// rising edge: (re)start the ARM
			snes_st018_reset();
		}
		s_reset = data;
	}
}

// -----------------------------------------------------------------------------
//  Catch-up run  (GSU-style: advance ARM clocks up to the S-CPU cycle counter)
// -----------------------------------------------------------------------------

void snes_st018_run()
{
	if (!s_haveArm) return;

	// While the S-CPU holds the reset line, the ARM is frozen (ares boot()).
	if (s_reset) { s_clockBase = s_snes->cycles; return; }

	UINT64 now   = s_snes->cycles;
	UINT64 delta = (now >= s_clockBase) ? (now - s_clockBase) : 0;
	s_clockBase  = now;

	// S-CPU cycle counter runs at the same master clock we clock the ARM with,
	// so one S-CPU cycle == one ARM clock owed.
	s_clockFp += delta;
	if (s_clockFp == 0) return;

	Arm7Open(0);

	if (!s_running) {
		if (s_clockFp >= ST018_BOOT_HOLD) {
			s_clockFp -= ST018_BOOT_HOLD;
			s_ready   = 1;
			s_running = 1;
		} else {
			Arm7Close();	// still within the boot hold; keep accumulating
			return;
		}
	}

	UINT32 owed = (s_clockFp > 0x3fffffffu) ? 0x3fffffffu : (UINT32)s_clockFp;
	if (owed > 0) {
		if (s_timer) {
			s_timer = (s_timer > owed) ? (s_timer - owed) : 0;
		}
		Arm7Run((INT32)owed);
	}
	s_clockFp = 0;

	Arm7Close();
}

// -----------------------------------------------------------------------------
//  Lifecycle
// -----------------------------------------------------------------------------

void snes_st018_init(void* mem, UINT8* bios, INT32 biosSize)
{
	s_snes = (Snes*)mem;

	if (bios == NULL || biosSize < (ST018_PROM_SIZE + ST018_DROM_SIZE)) {
		bprintf(0, _T("st018: missing/short firmware (need %x, have %x) - disabled\n"),
			ST018_PROM_SIZE + ST018_DROM_SIZE, biosSize);
		s_haveArm = 0;
		return;
	}

	if (s_haveArm) {
		Arm7Exit();
		s_haveArm = 0;
	} else {
		memset(s_pram, 0, sizeof(s_pram));
	}

	s_prom = bios;							// 0x00000..0x1ffff
	s_drom = bios + ST018_PROM_SIZE;		// 0x20000..0x27fff

	s_armFreq = s_snes->palTiming ? 21281372u : 21477272u;

	Arm7Init(0);
	Arm7Open(0);
	Arm7MapMemory(s_prom, ST018_PROM_BASE, ST018_PROM_BASE + ST018_PROM_SIZE - 1, MAP_ROM);
	// high-bit regions fold to 31-bit space (see header); map at the folded base
	Arm7MapMemory(s_drom, ST018_DROM_BASE & 0x7fffffffu,
		(ST018_DROM_BASE & 0x7fffffffu) + ST018_DROM_SIZE - 1, MAP_ROM);
	Arm7MapMemory(s_pram, ST018_PRAM_BASE & 0x7fffffffu,
		(ST018_PRAM_BASE & 0x7fffffffu) + ST018_PRAM_SIZE - 1, MAP_RAM);
	Arm7SetReadByteHandler( st018_arm_read_byte);
	Arm7SetReadWordHandler( st018_arm_read_word);
	Arm7SetReadLongHandler( st018_arm_read_long);
	Arm7SetWriteByteHandler(st018_arm_write_byte);
	Arm7SetWriteWordHandler(st018_arm_write_word);
	Arm7SetWriteLongHandler(st018_arm_write_long);
	Arm7Close();

	s_haveArm = 1;
	bprintf(0, _T("st018: init  prom %x  drom %x  pram %x  arm %u Hz\n"),
		ST018_PROM_SIZE, ST018_DROM_SIZE, ST018_PRAM_SIZE, s_armFreq);
}

void snes_st018_reset()
{
	if (!s_haveArm) return;

	Arm7Open(0);
	Arm7Reset();
	Arm7Close();

	s_running    = 0;
	s_ready      = 0;
	s_signal     = 0;
	s_timer      = 0;
	s_timerlatch = 0;
	s_c2a_ready  = 0;
	s_c2a_data   = 0;
	s_a2c_ready  = 0;
	s_a2c_data   = 0;

	s_clockBase  = (s_snes != NULL) ? s_snes->cycles : 0;
	s_clockFp    = 0;
}

void snes_st018_exit()
{
	if (s_haveArm) {
		Arm7Exit();
		s_haveArm = 0;
	}
	s_snes = NULL;
	s_prom = NULL;
	s_drom = NULL;
}

// -----------------------------------------------------------------------------
//  Save-state
// -----------------------------------------------------------------------------
void snes_st018_handleState(StateHandler* sh)
{
	// program RAM + bridge mailbox/timer/flags
	sh_handleByteArray(sh, s_pram, ST018_PRAM_SIZE);
	sh_handleBytes(sh,
		&s_c2a_ready, &s_c2a_data, &s_a2c_ready, &s_a2c_data,
		&s_reset, &s_ready, &s_signal,
		(UINT8*)&s_running, NULL);
	sh_handleInts(sh, &s_timer, &s_timerlatch, NULL);

	if (s_haveArm) {
		void* ctx = NULL;
		INT32 ctxSize = 0;
		Arm7Open(0);
		arm7_get_context(&ctx, &ctxSize);
		if (ctx != NULL && ctxSize > 0) {
			sh_handleByteArray(sh, (UINT8*)ctx, ctxSize);
		}
		Arm7Close();
	}

	// Re-anchor the catch-up clock base on load.
	if (!sh->saving) {
		s_clockBase = (s_snes != NULL) ? s_snes->cycles : 0;
		s_clockFp   = 0;
	}
}
