
#include "burnint.h"
#include "i8039.h" // i8035
#include "sp0250.h"

static UINT8 *program_rom = NULL;
static UINT8 *sample_rom = NULL;

static UINT8 i8035_latch;
static UINT8 i8035_t0;
static UINT8 i8035_t1;
static UINT8 i8035_drq;
static UINT8 i8035_p2;

static void sega_speech_drq_write(INT32 state)
{
	i8035_drq = (state == CPU_IRQSTATUS_ACK) ? 1 : 0;
}

static UINT8 __fastcall sega_speech_read(UINT32 address)
{
	return program_rom[address & 0x7ff];
}

static void __fastcall sega_speech_write_port(UINT32 port, UINT8 data)
{
	if (port < 0x100) {
		sp0250_write(data);
		return;
	}

	switch (port & 0x1ff)
	{
		case I8039_p1:
			if ((data & 0x80) == 0) i8035_t0 = 0;
		return;

		case I8039_p2:
			i8035_p2 = data;
		return;
	}
}

static UINT8 __fastcall sega_speech_read_port(UINT32 port)
{
	if (port < 0x100) {
		return sample_rom[(port & 0xff) + ((i8035_p2 & 0x3f) * 0x100)];
	}

	switch (port & 0x1ff)
	{
		case I8039_p1:
			return i8035_latch;

		case I8039_t0:
			return i8035_t0;

		case I8039_t1:
			return i8035_drq;
	}

	return 0;
}

void sega_speech_reset()
{
	I8039Open(0);
	I8039Reset();
	I8039Close();

	sp0250_reset();

	i8035_latch = 0;
	i8035_t0 = 0;
	i8035_t1 = 0;
	i8035_drq = 0;
	i8035_latch = 0;
	i8035_p2 = 0;
}

void sega_speech_data_write(UINT8 data)
{
	UINT8 old = i8035_latch;
	i8035_latch = data;

	I8039Open(0);
	I8039SetIrqState((data & 0x80) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
	I8039Close();

	if ((old & 0x80) == 0 && (data & 0x80))
		i8035_t0 = 1;
}

void sega_speech_init(UINT8 *program, UINT8 *samples)
{
	sample_rom = samples;
	program_rom = program;

	I8035Init(0);
	I8039Open(0);
	I8039SetProgramReadHandler(sega_speech_read);
	I8039SetCPUOpReadHandler(sega_speech_read);
	I8039SetCPUOpReadArgHandler(sega_speech_read);
	I8039SetIOReadHandler(sega_speech_read_port);
	I8039SetIOWriteHandler(sega_speech_write_port);
	I8039Close();

	sp0250_init(3120000, sega_speech_drq_write, I8039TotalCycles, 208000);
}

void sega_speech_exit()
{
	I8039Exit();

	sp0250_exit();
	sample_rom = NULL;
	program_rom = NULL;
}

void sega_speech_new_frame()
{
	I8039NewFrame();
}

void sega_speech_run(INT32 i, INT32 nInterleave, INT32 *nCyclesTotal, INT32 *nCyclesDone, INT32 which)
{
	I8039Open(0);
	CPU_RUN(which, I8039);
	sp0250_tick();
	I8039Close();
}

void sega_speech_render(INT16 *output, INT32 length)
{
	I8039Open(0);
	sp0250_update(output, length);
	I8039Close();
}

void sega_speech_scan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_VOLATILE)
	{
		I8039Scan(nAction, pnMin);
		sp0250_scan(nAction, pnMin);

		SCAN_VAR(i8035_latch);
		SCAN_VAR(i8035_t0);
		SCAN_VAR(i8035_t1);
		SCAN_VAR(i8035_drq);
		SCAN_VAR(i8035_latch);
		SCAN_VAR(i8035_p2);
	}
}
