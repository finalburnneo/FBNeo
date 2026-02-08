// Alpha Denshi 59MC07 sound board device
// Based on MAME device by Acho A. Tang, Nicola Salmoria

#include "burnint.h"
#include "i8085_intf.h"
#include "i8155.h"
#include "ay8910.h"
#include "msm5232.h"
#include "samples.h"
#include "dac.h"
#include "biquad.h"

static UINT8 soundlatch;
static UINT8 dac_latch;
static UINT8 i8155_port_b;
static UINT8 ay_port_a;
static UINT8 ay_port_b;
static INT32 msm_tuning = 0; // default  equites: 34, hvoltage 25
static INT32 msm_rate = 0; // actual rate of msm5232 (derived from msm_tuning) - sample rate: / 16

static float bass_vol, bass_vol_top;
static INT32 bass_mode = 0;
enum { ATTACK = 0, DECAY = 1 };

static float clap, hihat;
static double hihat_vol = 0.0;
static double clap_vol = 0.0;

static BIQ biquad_hihat;
static BIQ biquad_clap;


static void i8155_timer_pulse(INT32 state)
{
	if (!state) {
		i8085RunEnd();
		i8085SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_ACK); // note: nmi is trap
	}
}

static void update_dac()
{
	if (i8155_port_b & 0x01) DACSignedWrite(0, dac_latch);
	if (i8155_port_b & 0x02) DACSignedWrite(1, dac_latch);
}

static void AD59MC07_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff00) == 0xe000) {
		i8155_memory_write(address & 0xff, data);
		return;
	}

	if ((address & 0xfff0) == 0xc080) {
		MSM5232Write(address & 0xf, data);
		return;
	}

	switch (address)
	{
		case 0xc0a0:
		case 0xc0a1:
			AY8910Write(0, ~address & 1, data);
		return;

		case 0xc0b0:
		return; // nop

		case 0xc0c0:
		return; // clap mute?  (see schematic)

		case 0xc0d0:
		case 0xc0e0:
			dac_latch = data << 2;
			update_dac();
		return;

		case 0xc0f8: // nmi drives time for everything
			i8085SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_NONE);
		return;

		case 0xc0f9: // this controls sounds
			i8085SetIRQLine(I8085_RST75_LINE, CPU_IRQSTATUS_ACK);
			i8085SetIRQLine(I8085_RST75_LINE, CPU_IRQSTATUS_NONE);
		return;

		case 0xc0fa: // this controls music
			i8085SetIRQLine(I8085_INTR_LINE, CPU_IRQSTATUS_HOLD);
		return;

		case 0xc0fb:
		return; // nop

		case 0xc0fc: // not actually hooked up to anything
			//sound_prom_address = (sound_prom_address + 1) & 0x1f;
		return;

		case 0xc0fd:
		case 0xc0fe:
		return; // nop

		case 0xc0ff:
			soundlatch = 0;
		return;
	}
	bprintf(0, _T("ad, mw  %x  %x\n"), address, data);
}

static UINT8 AD59MC07_sound_read(UINT16 address)
{
	if ((address & 0xff00) == 0xe000) {
		return i8155_memory_read(address & 0xff);
	}

	switch (address)
	{
		case 0xc000:
			return soundlatch;
	}

	bprintf(0, _T("ad, mr  %x  \n"), address);

	return 0x00;
}

static void AD59MC07_sound_write_port(UINT16 port, UINT8 data)
{
	if ((port & 0xe8) == 0xe0) { // e0 - e7
		i8155_io_write(port & 0x7, data);
		return;
	}
	bprintf(0, _T("ad, bork wp: %x  %x\n"), port, data);
}

static UINT8 AD59MC07_sound_read_port(UINT16 port)
{
	if ((port & 0xe8) == 0xe0) { // e0 - e7
		return i8155_io_read(port & 0x7);
	}
	bprintf(0, _T("ad, bork rp:  %x\n"), port);
	return 0xff;
}

static void i8155_pa_write(INT32 port, UINT8 data)
{
	MSM5232SetRoute(((data >> 4) / 15.0), BURN_SND_MSM5232_ROUTE_0);
	MSM5232SetRoute(((data >> 4) / 15.0), BURN_SND_MSM5232_ROUTE_1);
	MSM5232SetRoute(((data >> 4) / 15.0), BURN_SND_MSM5232_ROUTE_2);
	MSM5232SetRoute(((data >> 4) / 15.0), BURN_SND_MSM5232_ROUTE_3);
	MSM5232SetRoute(((data & 0x0f) / 15.0), BURN_SND_MSM5232_ROUTE_4);
	MSM5232SetRoute(((data & 0x0f) / 15.0), BURN_SND_MSM5232_ROUTE_5);
	MSM5232SetRoute(((data & 0x0f) / 15.0), BURN_SND_MSM5232_ROUTE_6);
	MSM5232SetRoute(((data & 0x0f) / 15.0), BURN_SND_MSM5232_ROUTE_7);
}

static void i8155_pb_write(INT32 port, UINT8 data)
{
	i8155_port_b = data;
	update_dac();
}

static void i8155_pc_write(INT32 port, UINT8 data)
{
   // bprintf(0, _T("pc write: %x    fr %d\n"), data, nCurrentFrame);

	const float bass_boost = ((data & 0xf) > 0x00) ? 1.25 : 0.00;
	bass_vol_top = ((data & 0x0f) / 15.0) + bass_boost;
	bass_vol = 0.00;
	bass_mode = ATTACK;

	MSM5232SetRoute(bass_vol + bass_boost, 8);
	if (data & 0x20) {
		MSM5232SetRoute(((data & 0x0f) / 15.0), 9);
	} else {
		MSM5232SetRoute(0.0, 9);
	}
}

static void playex(INT32 sam, double volume, bool checkplay, bool loop)
{
	BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_1, volume, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_2, volume, BURN_SND_ROUTE_BOTH);

	BurnSampleSetLoop(sam, loop);

	if ( (checkplay && BurnSampleGetStatus(sam) == SAMPLE_STOPPED) || !checkplay )
		BurnSamplePlay(sam);
}

static void filter_init()
{
	biquad_hihat.init(FILT_HIGHPASS, msm_rate / 16, 8000.00, 2.2, 0);
	biquad_clap.init(FILT_BANDPASS, msm_rate / 16, 1100.00, 3.4, 7, false);
}

// ad59mc07 notes, - dink feb 2026
// ls123 drives a lp filter for the clap (labled as "cymbal" in user-made schematic)
// essentially giving it an attack & decay stage linked to the filter (filter env. mod.)
static INT32 crash_cutoff = 0;
static INT32 ls123 = 0;

static void clapFilterReset()
{
	ls123 = 0;
	crash_cutoff = 0;
	filter_init();
}

static void clapFilterAdvance()
{
	ls123++;
	crash_cutoff += (ls123 > 30) ? 40 : -40;

	biquad_clap.init(FILT_BANDPASS, msm_rate / 16, 1100.00 + crash_cutoff, 3.4, 7, false);
}

static void update_hat_clap_bass_volumes(bool withbass)
{
	// hihat has no volume control registers! (see schematic)
	hihat_vol = hihat * 0.67;
	clap_vol = clap * (ay_port_b & 3) * 0.77;

	// sometimes we don't want to update bass right away
	if (withbass) MSM5232SetRoute(bass_vol, 8);
}

static INT32 ad59mc07_noise_mixer(INT32 sample_in)
{
	// incoming noise sample -> 2 new channels (hihat, clap)
	//   1: hihat get volume decay and static filter
	//   2: clap gets volume decay and attack/decay envelope modulated filter
	const INT32 hh = biquad_hihat.filter(sample_in) * hihat_vol;
	const INT32 cl = biquad_clap.filter(sample_in) * clap_vol;

	return hh + cl;
}

static void ay8910portAwrite(UINT32 /*offset*/, UINT32 data)
{
	if (AY8910InReset(0)) return; // reset (ignore)

	if (data & ~ay_port_a & 0x80) {
		playex(0, ((data & 0x30) >> 4) * 0.09, 0, 0);
	}

	if (data & ~ay_port_a & 0x08) {
		playex(1, (data & 0x03) * 0.09, 0, 0);
	}

	ay_port_a = data;
}

static void ay8910portBwrite(UINT32 /*offset*/, UINT32 data)
{
	if (AY8910InReset(0)) return; // reset (ignore)

	if (data & ~ay_port_b & 0x80) {
		playex(2, ((data & 0x30) >> 4) * 0.09, 0, 0);
	}

	bool has_hat = false;
	bool has_clap = false;

	if (data & ~ay_port_b & 0x04) {
		hihat = 0.10;
		has_hat = true;
	}

	if (data & ~ay_port_b & 0x08) {
		clapFilterReset();
		clap = 0.40;
		has_clap = true;
		//bprintf(0, _T("clap  %d  tc %d\n"), nCurrentFrame, i8085TotalCycles());
	}

	if (~data & 0x40) {
		//bprintf(0, _T("hihat MUTE-- %d   tc %d\n"), nCurrentFrame, i8085TotalCycles());
		has_hat = false;
		hihat = 0.0;
	}
	//if (hat) bprintf(0, _T("Hihat  %d  tc %d\n"), nCurrentFrame, i8085TotalCycles());

	if (has_hat || has_clap) {
		update_hat_clap_bass_volumes(false);
	}

	ay_port_b = data;
}

static void msm_gate(INT32 state)
{
	// note: this is not very helpful
	//bprintf(0, _T("Gate %x    fr %d\n"), state, nCurrentFrame);
}

#define MSM5232_MAX_CLOCK 6144000
#define MSM5232_MIN_CLOCK  214000

static INT32 calc_rate(INT32 percent)
{
	const INT32 rate = MSM5232_MIN_CLOCK + percent * (MSM5232_MAX_CLOCK - MSM5232_MIN_CLOCK) / 100;

	return rate;
}

static void sixtyhz_timer() // called once per frame (possibly not always 60hz)
{
	msm_rate = calc_rate(msm_tuning);
	MSM5232SetClock(msm_rate);

	if (hihat >= 0.01) {
		hihat -= 0.005;
		if (hihat < 0.0) hihat = 0.0;
	} else hihat = 0.0;
	if (clap >= 0.001) {
		clap -= 0.030;
		if (clap < 0.0) clap = 0.0;
		clapFilterAdvance();
	} else clap = 0.0;

	switch (bass_mode) {
		case ATTACK:
			if (bass_vol < bass_vol_top) {
				bass_vol += 0.78; // matches pcb recordings
				if (bass_vol > bass_vol_top) {
					bass_vol = bass_vol_top;
				}
			} else {
				bass_mode = DECAY;
			}
			break;
		case DECAY:
			bass_vol *= 0.92;
			break;
	}

	if (hihat != 0.0) bprintf(0, _T("HH %f\n"), hihat);
	if (clap != 0.0) bprintf(0, _T("CLAP %f\n"), clap);

	update_hat_clap_bass_volumes(true);
}

void AD59MC07Command(UINT8 data)
{
	soundlatch = data;
}

void AD59MC07Reset()
{
	i8085Reset();
	i8155_reset();
	AY8910Reset(0);
	MSM5232Reset();
	BurnSampleReset();
	DACReset();

	biquad_hihat.reset();
	biquad_clap.reset();

	soundlatch = 0;
	dac_latch = 0;
	i8155_port_b = 0;
}

void AD59MC07SetRate(INT32 rate)
{
	if (rate < 1 || rate > 100) {
		bprintf(0, _T("** AD59MC07SetRate(%d): bad value, must be 1-100.  Defaulting to 25.\n"));
		rate = 25;
	}
	msm_tuning = rate;
}

void AD59MC07Init(UINT8 *rom)
{
	i8085Init();
	i8085Open(0);
	i8085MapMemory(rom,		0x0000, 0xbfff, MAP_ROM);
	i8085SetWriteHandler(AD59MC07_sound_write);
	i8085SetReadHandler(AD59MC07_sound_read);
	i8085SetOutHandler(AD59MC07_sound_write_port);
	i8085SetInHandler(AD59MC07_sound_read_port);
	i8085SetCallback(i8155_timers_run);
	i8085Close();

	i8155_init();
	i8155_set_to_write_cb(i8155_timer_pulse);
	i8155_set_pa_write_cb(i8155_pa_write);
	i8155_set_pb_write_cb(i8155_pb_write);
	i8155_set_pc_write_cb(i8155_pc_write);

	AY8910Init(0, 6144000 / 4, 0);
	AY8910SetPorts(0, NULL, NULL, ay8910portAwrite, ay8910portBwrite);
	AY8910SetAllRoutes(0, 0.31, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(i8085TotalCycles, 6144000/2);

	// MSM clock
	msm_tuning = 25; // default value
	msm_rate = calc_rate(msm_tuning);

	//  fbprintf(0, _T("MSM5232 Rate: %d  @  %d%%\n"), rate, 34);

	MSM5232Init(msm_rate, 1); // clock is variable!
	MSM5232SetBuffered(i8085TotalCycles, 6144000/2);
	MSM5232SetCapacitors(0.47e-6, 0.47e-6, 0.47e-6, 0.47e-6, 0.47e-6, 0.47e-6, 0.47e-6, 0.47e-6);
	MSM5232SetRoute(0.32, BURN_SND_MSM5232_ROUTE_0);
	MSM5232SetRoute(0.47, BURN_SND_MSM5232_ROUTE_1);
	MSM5232SetRoute(0.70, BURN_SND_MSM5232_ROUTE_2);
	MSM5232SetRoute(0.70, BURN_SND_MSM5232_ROUTE_3);
	MSM5232SetRoute(0.32, BURN_SND_MSM5232_ROUTE_4);
	MSM5232SetRoute(0.47, BURN_SND_MSM5232_ROUTE_5);
	MSM5232SetRoute(0.70, BURN_SND_MSM5232_ROUTE_6);
	MSM5232SetRoute(0.70, BURN_SND_MSM5232_ROUTE_7);
	MSM5232SetRoute(0.70, 8);
	MSM5232SetRoute(0.70, 9);
	MSM5232SetRoute(1.00, 10);  // noise: volume is modulated in ad59mc07_noise_mixer()
	MSM5232SetGateCallback(msm_gate);
	MSM5232SetNoiseMixerCallback(ad59mc07_noise_mixer);

	filter_init();

	BurnSampleInit(1);
	BurnSampleSetBuffered(i8085TotalCycles, 6144000/2);
	BurnSampleSetAllRoutesAllSamples(0.21, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, i8085TotalCycles, 6144000/2);
	DACInit(1, 0, 1, i8085TotalCycles, 6144000/2);
	DACSetRoute(0, 0.35, BURN_SND_ROUTE_BOTH);
	DACSetRoute(1, 0.35, BURN_SND_ROUTE_BOTH);
}

void AD59MC07Exit()
{
	AY8910Exit(0);
	BurnSampleExit();
	DACExit();
	MSM5232Exit();
	i8085Exit();
	i8155_exit();
	msm_tuning = 0;
	msm_rate = 0;
}

INT32 AD59MC07Run(INT32 cycles)
{
	return i8085Run(cycles);
}

void AD59MC07NewFrame()
{
	sixtyhz_timer();

	i8085NewFrame();
}

void AD59MC07Update(INT16 *output, INT32 length)
{
    AY8910Render(output, length);
	MSM5232Update(output, length);
	BurnSampleRender(output, length);
	DACUpdate(output, length);
}

INT32 AD59MC07Scan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_VOLATILE) {
		i8085Scan(nAction);
		i8155_scan();
		AY8910Scan(nAction, pnMin);
		MSM5232Scan(nAction, pnMin);
		BurnSampleScan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(soundlatch);
		SCAN_VAR(dac_latch);
		SCAN_VAR(i8155_port_b);
		SCAN_VAR(ay_port_a);
		SCAN_VAR(ay_port_b);
		SCAN_VAR(clap);
		SCAN_VAR(hihat);

		SCAN_VAR(bass_vol);
		SCAN_VAR(bass_vol_top);
		SCAN_VAR(bass_mode);
		SCAN_VAR(msm_tuning);
		SCAN_VAR(msm_rate);

		SCAN_VAR(hihat_vol);
		SCAN_VAR(clap_vol);

		biquad_hihat.scan();
		biquad_clap.scan();
	}

	return 0;
}
