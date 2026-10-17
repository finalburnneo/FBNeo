// dcs 2k, 8k by Romhack w/modifications by dink.

#include "burnint.h"
#include "adsp2100_intf.h"
#include "adsp2100/adsp2100.h"
#include "dcs2k.h"
#include <math.h>

#define MHz(x)  (x * 1000000)
#define ADSP_CLOCK   MHz(10)

#define LOG_ENABLE  0
#define LOG_MEMACC  0

#if LOG_ENABLE
#define dcs_log(...)        bprintf(0, __VA_ARGS__)
#else
#define dcs_log(...)
#endif
// From mame...
#define S1_AUTOBUF_REG		15
#define S1_RFSDIV_REG		16
#define S1_SCLKDIV_REG		17
#define S1_CONTROL_REG		18
#define S0_AUTOBUF_REG		19
#define S0_RFSDIV_REG		20
#define S0_SCLKDIV_REG		21
#define S0_CONTROL_REG		22
#define S0_MCTXLO_REG		23
#define S0_MCTXHI_REG		24
#define S0_MCRXLO_REG		25
#define S0_MCRXHI_REG		26
#define TIMER_SCALE_REG		27
#define TIMER_COUNT_REG		28
#define TIMER_PERIOD_REG	29
#define WAITSTATES_REG		30
#define SYSCONTROL_REG		31

#define LATCH_OUTPUT_EMPTY  0x400
#define LATCH_INPUT_EMPTY   0x800
#define SET_OUTPUT_EMPTY()  nLatchControl |= LATCH_OUTPUT_EMPTY
#define SET_OUTPUT_FULL()   nLatchControl &= ~LATCH_OUTPUT_EMPTY
#define SET_INPUT_EMPTY()   nLatchControl |= LATCH_INPUT_EMPTY
#define SET_INPUT_FULL()    nLatchControl &= ~LATCH_INPUT_EMPTY
// end from mame

static INT32 dcs_type;
static INT32 dcs_mhz;
static double dcs_volume;

static INT32 latch_addr_lo;
static INT32 latch_addr_hi;

static UINT8 *pIntRAM;
static UINT8 *pExtRAM;
static UINT32 *pExtRAM32;
static UINT8 *pDataRAM;
static UINT8 *pDataRAM0;
static UINT8 *pSoundROM;
static UINT32 nSoundSize;
static UINT32 nSoundBanks;

static UINT16 nCurrentBank;
static UINT32 nOutputData;
static UINT32 nInputData;
static UINT16 nLatchControl;
static UINT32 nCtrlReg[32];
static UINT16 nTxIR;
static UINT16 nTxIRBase;
static UINT32 nTxSize;
static UINT32 nTxIncrement;
static UINT8 bGenerateIRQ;

// the autobuffer IRQ is generated every half buffer of ADSP time (as MAME's dcs_irq timer)
static INT32 nIRQPeriod;		// ADSP cycles between two half buffers
static INT32 nIRQCountdown;		// ADSP cycles until the next half buffer

static INT32 sample_rate;  // sample rate of dcs
static INT16 *mixer_buffer;
static INT32 mixer_pos;
static INT32 nRenderAcc;		// remainder of sample_rate * 100 / nBurnFPS
static INT32 bRenderPrimed;
static INT16 nLastSample;

#define MIXER_BUFFER_LEN	10000

void Dcs2kBoot();
static void SetupMemory();
static INT32 RxCallback(INT32 port);
static void TxCallback(INT32 port, INT32 data);
static void TimerCallback(INT32 enable);
static INT32 IRQCallback(INT32 line);

static UINT16 ReadRAM(UINT32 address);
static void WriteRAM(UINT32 address, UINT16 value);
static UINT16 ReadRAM0(UINT32 address);
static void WriteRAM0(UINT32 address, UINT16 value);

static UINT16 ReadRAMBank(UINT32 address);

static UINT16 ReadData(UINT32 address);
static void WriteData(UINT32 address, UINT16 value);
static void SelectDataBank(UINT32 address, UINT16 value);
static UINT16 InputLatch(UINT32 address);
static void OutputLatch(UINT32 address, UINT16 value);
static UINT16 AdspRead(UINT32 address);
static void AdspWrite(UINT32 address, UINT16 value);

void Dcs2kInit(INT32 dtype, INT32 dmhz)
{
    dcs_log(_T("init\n"));

	dcs_type = dtype;
	dcs_mhz = dmhz;
	dcs_volume = 1.00;

    Adsp2100Init();

    Adsp2100SetIRQCallback(IRQCallback);
    Adsp2100SetRxCallback(RxCallback);
    Adsp2100SetTxCallback(TxCallback);
    Adsp2100SetTimerCallback(TimerCallback);

    pIntRAM = BurnMalloc((0x400 + 0x1000) * sizeof(UINT32)); // need to account for bootrom, rmpgwt uses 0x600
    pExtRAM = BurnMalloc(0x800 * sizeof(UINT32));
    pDataRAM = BurnMalloc(0x200 * sizeof(UINT16));
    pDataRAM0 = BurnMalloc(0x800 * sizeof(UINT16));

	memset(pIntRAM, 0, (0x400 + 0x1000) * sizeof(UINT32));
    memset(pExtRAM, 0, 0x800 * sizeof(UINT32));
    memset(pDataRAM, 0, 0x200 * sizeof(UINT16));
    memset(pDataRAM0, 0, 0x800 * sizeof(UINT16));

    pExtRAM32 = (UINT32*) pExtRAM;

    nCurrentBank = 0;
    nSoundBanks = 0;
    nSoundSize = 0;

    SetupMemory();

	switch (dcs_type) {
		case DCS_2K:
			latch_addr_lo = 0x3400;
			latch_addr_hi = 0x37ff;
		break;

		case DCS_2KU:
			latch_addr_lo = 0x3403;
			latch_addr_hi = 0x3403;
		break;

		case DCS_8K:
			latch_addr_lo = 0x3400;
			latch_addr_hi = 0x3403;
		break;
	}

	mixer_buffer = (INT16*)BurnMalloc(MIXER_BUFFER_LEN * sizeof(INT16));
	mixer_pos = 0;

    nCurrentBank = 0;
}

void Dcs2kExit()
{
    dcs_log(_T("exit\n"));
    Adsp2100Exit();

    BurnFree(pIntRAM);
    BurnFree(pExtRAM);
	BurnFree(pDataRAM);
	BurnFree(pDataRAM0);

	BurnFree(mixer_buffer);
}

void Dcs2kSetVolume(double vol)
{
	dcs_volume = vol;
}

// the ADSP produced about one frame of samples, stretch them to the output length
void Dcs2kRender(INT16 *pSoundBuf, INT32 nSegmentLength)
{
	// exact number of DCS samples in this frame, the remainder carries over
	nRenderAcc += sample_rate * 100;
	INT32 want = nRenderAcc / nBurnFPS;
	nRenderAcc -= want * nBurnFPS;

	// keep a half buffer of jitter in reserve before starting, so the output does not starve
	if (!bRenderPrimed && mixer_pos >= want + (INT32)(nTxSize / 2) + 16) bRenderPrimed = 1;

	INT32 take = bRenderPrimed ? want : 0;
	if (take > mixer_pos) take = mixer_pos;

	if (take <= 0) {
		INT32 v = BURN_SND_CLIP(nLastSample * dcs_volume);
		for (INT32 j = 0; j < nSegmentLength; j++) {
			pSoundBuf[0] = pSoundBuf[1] = v;
			pSoundBuf += 2;
		}
		return;
	}

	// linear interpolation of take input samples over nSegmentLength output samples (dcs 2k/8k is mono)
	for (INT32 j = 0; j < nSegmentLength; j++) {
		INT32 pos = (INT32)(((INT64)j * take << 8) / nSegmentLength);
		INT32 i = pos >> 8, frac = pos & 0xff;
		INT32 a = (i == 0) ? nLastSample : mixer_buffer[i - 1];
		INT32 b = mixer_buffer[i];
		INT32 v = BURN_SND_CLIP(((a * (256 - frac) + b * frac) >> 8) * dcs_volume);
		pSoundBuf[0] = pSoundBuf[1] = v;
		pSoundBuf += 2;
	}

	nLastSample = mixer_buffer[take - 1];
	memmove(&mixer_buffer[0], &mixer_buffer[take], (mixer_pos - take) * sizeof(INT16));
	mixer_pos -= take;

	// latency got too high (e.g. sound output was off for a while): drop the excess
	if (mixer_pos > want * 4) {
		INT32 drop = mixer_pos - want * 2;
		memmove(&mixer_buffer[0], &mixer_buffer[drop], (mixer_pos - drop) * sizeof(INT16));
		mixer_pos -= drop;
	}
}

void Dcs2kNewFrame()
{
	Adsp2100NewFrame();
}

UINT32 Dcs2kTotalCycles()
{
	return Adsp2100TotalCycles();
}

static void DcsIRQ__INT();

void Dcs2kRun(INT32 cycles)
{
	while (cycles > 0) {
		if (!bGenerateIRQ) {
			Adsp2100Run(cycles);
			break;
		}

		INT32 seg = (cycles < nIRQCountdown) ? cycles : nIRQCountdown;
		INT32 ran = (seg > 0) ? Adsp2100Run(seg) : 0;
		if (seg > 0 && ran <= 0) ran = seg;		// end of slice requested, don't spin

		cycles -= ran;
		nIRQCountdown -= ran;
		if (nIRQCountdown <= 0) DcsIRQ__INT();
	}
}


static UINT32 ReadProgram(UINT32 address)
{
#if LOG_MEMACC
    dcs_log(_T("prg_r %x - %x\n"), address);
#endif
    UINT32 *p = (UINT32*)pIntRAM;
    return p[address & 0x3FF];
}

static void WriteProgram(UINT32 address, UINT32 value)
{
#if LOG_MEMACC
    dcs_log(_T("prg_w %x - %x\n"), address, value);
#endif
    UINT32 *p = (UINT32*)pIntRAM;
    p[address & 0x3FF] = value & 0xFFFFFF;
}

static UINT32 ReadProgramEXT(UINT32 address)
{
#if LOG_MEMACC
    dcs_log(_T("prg_ext_r %x - %x\n"), address, pExtRAM32[address & 0x7FF]);
#endif
    return pExtRAM32[address & 0x7FF];
}

static void WriteProgramEXT(UINT32 address, UINT32 value)
{
    INT32 offset = address & 0x7FF;
#if LOG_MEMACC
    dcs_log(_T("prg_ext_w %x - %x\n"), address, value);
#endif
    pExtRAM32[offset] = value;
}

static void SetupMemory()
{
    // program address space
    Adsp2100SetReadLongHandler  (1, ReadProgram);
    Adsp2100SetWriteLongHandler (1, WriteProgram);

	Adsp2100SetReadLongHandler  (2, ReadProgramEXT);
    Adsp2100SetWriteLongHandler (2, WriteProgramEXT);
    Adsp2100MapHandler(1, 0x0000, 0x03FF, MAP_RAM);

	switch (dcs_type) {
		case DCS_2K:
		case DCS_2KU:
			Adsp2100MapHandler(2, 0x0800, 0x0FFF, MAP_RAM);
			Adsp2100MapHandler(2, 0x1000, 0x17FF, MAP_RAM);
			Adsp2100MapHandler(2, 0x1800, 0x1FFF, MAP_RAM);
		break;

		case DCS_8K:
			Adsp2100MapHandler(2, 0x0800, 0x1FFF, MAP_RAM);
		break;
	}

    // data address space
    Adsp2100SetReadDataWordHandler  (1, ReadData);
    Adsp2100SetWriteDataWordHandler (1, WriteData);

    Adsp2100SetReadDataWordHandler  (2, ReadRAMBank);

    Adsp2100SetWriteDataWordHandler (3, SelectDataBank);

    Adsp2100SetReadDataWordHandler  (4, InputLatch);
    Adsp2100SetWriteDataWordHandler (4, OutputLatch);

    Adsp2100SetReadDataWordHandler  (5, ReadRAM);
    Adsp2100SetWriteDataWordHandler (5, WriteRAM);

    Adsp2100SetReadDataWordHandler  (6, AdspRead);
    Adsp2100SetWriteDataWordHandler (6, AdspWrite);

	Adsp2100SetReadDataWordHandler  (7, ReadRAM0);
    Adsp2100SetWriteDataWordHandler (7, WriteRAM0);

	switch (dcs_type) {
		case DCS_2K:
		case DCS_2KU:
			Adsp2100MapDataHandler(1, 0x0000, 0x07FF, MAP_RAM);
			Adsp2100MapDataHandler(1, 0x0800, 0x0FFF, MAP_RAM);
			Adsp2100MapDataHandler(1, 0x1000, 0x17FF, MAP_RAM);
			Adsp2100MapDataHandler(1, 0x1800, 0x1FFF, MAP_RAM);
		break;

		case DCS_8K:
			Adsp2100MapDataHandler(1, 0x0800, 0x1FFF, MAP_RAM);
			Adsp2100MapDataHandler(7, 0x0000, 0x07FF, MAP_RAM);
		break;
	}

    Adsp2100MapDataHandler(2, 0x2000, 0x2FFF, MAP_READ);

    Adsp2100MapDataHandler(3, 0x3000, 0x33FF, MAP_WRITE);

    Adsp2100MapDataHandler(4, 0x3400, 0x34FF, MAP_READ | MAP_WRITE);

    Adsp2100MapDataHandler(5, 0x3800, 0x39FF, MAP_READ | MAP_WRITE);

    Adsp2100MapDataHandler(6, 0x3FE0, 0x3FFF, MAP_READ | MAP_WRITE);
}



void Dcs2kMapSoundROM(void *ptr, INT32 size)
{
    pSoundROM = (UINT8*)ptr;
    nSoundSize = size;
    nSoundBanks = (nSoundSize / 2) / 0x1000;
    dcs_log(_T("Sound size %x"), size);
    dcs_log(_T("Sound banks %x"), nSoundBanks);
}

void Dcs2kBoot()
{
	UINT8 *buffer = (UINT8*)BurnMalloc(0x2000); // +0x1000 for adsp paging
	UINT16 *base = (UINT16*)pSoundROM + ((nCurrentBank & 0x7FF) * 0x1000);

	for (INT32 i = 0; i < 0x1000; i++)
		buffer[i] = BURN_ENDIAN_SWAP_INT16(base[i]);

	Adsp2100LoadBootROM(buffer, pIntRAM);

	BurnFree(buffer);
}

void Dcs2kReset()
{
    Adsp2100Reset();
    Adsp2100SetIRQLine(ADSP2105_IRQ0, CPU_IRQSTATUS_NONE);
    Adsp2100SetIRQLine(ADSP2105_IRQ1, CPU_IRQSTATUS_NONE);
    Adsp2100SetIRQLine(ADSP2105_IRQ2, CPU_IRQSTATUS_NONE);

    nTxIR = 0;
    nTxIRBase = 0;
    nTxSize = 0;
    nTxIncrement = 0;
    bGenerateIRQ = 0;
    nIRQPeriod = 0;
    nIRQCountdown = 0;

	nCurrentBank = 0;
    nOutputData = 0;
    nInputData = 0;
    nLatchControl = 0;

    for (INT32 i = 0; i < 32; i++)
        nCtrlReg[i] = 0;

    SET_INPUT_EMPTY();
    SET_OUTPUT_EMPTY();

    Dcs2kBoot();

	mixer_pos = 0;
	nRenderAcc = 0;
	bRenderPrimed = 0;
	nLastSample = 0;
	sample_rate = 31250; // default
}

static UINT16 ReadRAMBank(UINT32 address)
{
    UINT16 *p = (UINT16 *) pSoundROM;
    UINT32 offset = (nCurrentBank * 0x1000) + (address & 0xFFF);
#if LOG_MEMACC
    dcs_log(_T("bank_r[%x] %x - %x\n"), nCurrentBank, address & 0xFFF, p[offset]);
#endif
    return BURN_ENDIAN_SWAP_INT16(p[offset]);
}

static UINT16 ReadRAM(UINT32 address)
{
#if LOG_MEMACC
    dcs_log(_T("ram_r %x\n"), address);
#endif
    UINT16 *p = (UINT16 *) pDataRAM;
    return BURN_ENDIAN_SWAP_INT16(p[address & 0x1FF]);
}

static void WriteRAM(UINT32 address, UINT16 value)
{
#if LOG_MEMACC
    dcs_log(_T("ram_w %x, %x\n"), address, value);
#endif
    UINT16 *p = (UINT16 *) pDataRAM;
    p[address & 0x1FF] = BURN_ENDIAN_SWAP_INT16(value);
}

static UINT16 ReadRAM0(UINT32 address)
{
#if LOG_MEMACC
    dcs_log(_T("ram0_r %x\n"), address);
#endif
    UINT16 *p = (UINT16 *) pDataRAM0;
    return BURN_ENDIAN_SWAP_INT16(p[address & 0x7FF]);
}

static void WriteRAM0(UINT32 address, UINT16 value)
{
#if LOG_MEMACC
    dcs_log(_T("ram0_w %x, %x\n"), address, value);
#endif
    UINT16 *p = (UINT16 *) pDataRAM0;
    p[address & 0x7FF] = BURN_ENDIAN_SWAP_INT16(value);
}

static UINT16 ReadData(UINT32 address)
{
    INT32 offset = (address & 0x7FF);
#if LOG_MEMACC
    dcs_log(_T("dataram_r %x - %x\n"), offset, pExtRAM32[offset] >> 8);
#endif
    return pExtRAM32[offset] >> 8;
}

static void WriteData(UINT32 address, UINT16 value)
{
    INT32 offset = (address & 0x7FF);

    pExtRAM32[offset] &= 0xFF;
    pExtRAM32[offset] |= value << 8;
#if LOG_MEMACC
    dcs_log(_T("dataram_w %x - %x\n"), offset, value);
#endif
}

static void SelectDataBank(UINT32 address, UINT16 value)
{
	static INT32 oldbank = 0;

	nCurrentBank = (value & 0x7FF) % nSoundBanks;
	if (nCurrentBank != oldbank) {
		// this changes so much, it's impossible to log with it..
		//dcs_log(_T("bank_select %X  (mask %X)\n"), nCurrentBank, nSoundBanks);
	}
	oldbank = nCurrentBank;
}

static UINT16 InputLatch(UINT32 address)
{
	if (address >= latch_addr_lo && address <= latch_addr_hi) {
		Adsp2100SetIRQLine(ADSP2105_IRQ2, CPU_IRQSTATUS_NONE);
		SET_INPUT_EMPTY();
		dcs_log(_T("input_latch %x\n"), nInputData);
		return nInputData;
	}
	return 0;
}

static void OutputLatch(UINT32 address, UINT16 value)
{
	if (address >= latch_addr_lo && address <= latch_addr_hi) {
		dcs_log(_T("output_latch %x, %x\n"), address, value);
		SET_OUTPUT_FULL();
		nOutputData = value;
	}
}

static UINT16 AdspRead(UINT32 address)
{
	if (address >= 0x3FE0 && address <= 0x3FFF) {
		dcs_log(_T("adsp_r %x\n"), address);
		return nCtrlReg[address & 0x1F];
	}
	return 0;
}


static void AdspWrite(UINT32 address, UINT16 value)
{
	if (!(address >= 0x3FE0 && address <= 0x3FFF)) return;

	dcs_log(_T("adsp_w %x, %x\n"), address, value);
    nCtrlReg[address & 0x1F] = value;

	switch (address & 0x1F) {
		case S1_AUTOBUF_REG:
			if (~value & 0x0002) {
				bGenerateIRQ = 0;
			}
			break;
		case SYSCONTROL_REG:
			if (value & 0x0200) {
				dcs_log(_T("reboot\n"));
				Adsp2100Reset();
				Dcs2kBoot();
				nCtrlReg[SYSCONTROL_REG] = 0;
			}
			if ((value & 0x0800) == 0) {
				bGenerateIRQ = 0;
			}

			break;
	}
}

void Dcs2kDataWrite(INT32 data)
{
    dcs_log(_T("data_w %x\n"), data);
    Adsp2100SetIRQLine(ADSP2105_IRQ2, CPU_IRQSTATUS_ACK);
    SET_INPUT_FULL();
    nInputData = data;
}

void Dcs2kResetWrite(INT32 data)
{
    dcs_log(_T("reset_w %x\n"), data);
    if (data) {
        Dcs2kReset();
    }
}

INT32 Dcs2kControlRead()
{
//	dcs_log(_T("control_r %x\n"), nLatchControl);
    return nLatchControl;
}

INT32 Dcs2kDataRead()
{
	SET_OUTPUT_EMPTY();
    return nOutputData;
}


static INT32 RxCallback(INT32 port)
{
    dcs_log(_T("rx_sport %d\n"), port);
    return 0;
}

static void ComputeNextIRQCycle()
{
    if (nTxIncrement && sample_rate > 0) {
        // one half buffer: nTxSize / (2 * nTxIncrement) samples
        INT32 count = nTxSize / (2 * nTxIncrement);
        nIRQPeriod = (INT32)(((INT64)dcs_mhz * count) / sample_rate);
        if (nIRQPeriod < 1) nIRQPeriod = 1;
        nIRQCountdown = nIRQPeriod;
        bGenerateIRQ = 1;
        Adsp2100RunEnd();	// stop the current slice, so Dcs2kRun picks up the new period
    }
}

static void TxCallback(INT32 port, INT32 data)
{
    dcs_log(_T("tx_sport p: %d, d: %d\n"), port, data);
    if (port != 1)
        return;

    if (nCtrlReg[SYSCONTROL_REG] & 0x800) {
        if (nCtrlReg[S1_AUTOBUF_REG] & 2) {
            adsp2100_state *adsp = Adsp2100GetState();
            UINT16 src;
            INT32 m, l;

            nTxIR = (nCtrlReg[S1_AUTOBUF_REG] >> 9) & 7;
            m = (nCtrlReg[S1_AUTOBUF_REG] >> 7) & 3;
            m |= nTxIR & 0x04;
            l = nTxIR;

            src = adsp->i[nTxIR];
            nTxIncrement = adsp->m[m];
            nTxSize = adsp->l[l];

            src &= ~0xf;
            adsp->i[nTxIR] = src;
            nTxIRBase = src;

			INT32 sample_rate_old = sample_rate;
			if (nCtrlReg[S1_CONTROL_REG] & 0x4000) {
				sample_rate = (dcs_mhz / (2 * (nCtrlReg[S1_SCLKDIV_REG] + 1))) / 16;
			} else {
				sample_rate = 31250; // external 31.25kHz sample clock
			}

			if (sample_rate != sample_rate_old) {
				bprintf(0, _T("dcs2k: new sample rate %d\n"), sample_rate);
			}

            dcs_log(_T("tx_info base = %x, inc = %x, size = %x\n"), src, nTxIncrement, nTxSize);
            ComputeNextIRQCycle();
            return;
        }
    }
    dcs_log(_T("disable_irq\n"));
    bGenerateIRQ = 0;
}

static void DcsIRQ__INT()
{
	if (!bGenerateIRQ) return;
    adsp2100_state *adsp = Adsp2100GetState();
    INT32 r = adsp->i[nTxIR];

    INT32 count = nTxSize / (2 * (nTxIncrement ? nTxIncrement : 1));

    nIRQCountdown += nIRQPeriod;

    // sound output not being consumed (no pBurnSoundOut)
    if (mixer_pos + count > MIXER_BUFFER_LEN) {
        mixer_pos = 0;
        bRenderPrimed = 0;
    }

    for (INT32 i = 0; i < count; i++) {
        mixer_buffer[mixer_pos++] = adsp21xx_data_read_word_16le(r<<1);
        r += nTxIncrement;
	}

	bool wrapped = false;

    if (r >= (nTxIRBase + nTxSize)) {
        r = nTxIRBase;
        wrapped = true;
    }
    adsp->i[nTxIR] = r;

    if (wrapped) {
		Adsp2100SetIRQLine(ADSP2105_IRQ1, CPU_IRQSTATUS_AUTO);
    }
}

static void TimerCallback(INT32 enable)
{
    dcs_log(_T("timer %d\n"), enable);
}

static INT32 IRQCallback(INT32 line)
{
    dcs_log(_T("irq %d\n"), line);
    return 0;
}

INT32 Dcs2kScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_VOLATILE)
	{
		Adsp2100Scan(nAction);

		ScanVar(pIntRAM,    (0x400 + 0x1000) * sizeof(UINT32),  "DcsIntRAM");
		ScanVar(pExtRAM,    0x800 * sizeof(UINT32),             "DcsExtRAM");
		ScanVar(pDataRAM,   0x200 * sizeof(UINT16),             "DcsDataRAM");
		ScanVar(pDataRAM0,  0x800 * sizeof(UINT16),             "DcsDataRAM0");

		SCAN_VAR(nCurrentBank);
		SCAN_VAR(nOutputData);
		SCAN_VAR(nInputData);
		SCAN_VAR(nLatchControl);
		SCAN_VAR(nCtrlReg);
		SCAN_VAR(nTxIR);
		SCAN_VAR(nTxIRBase);
		SCAN_VAR(nTxSize);
		SCAN_VAR(nTxIncrement);
		SCAN_VAR(bGenerateIRQ);
		SCAN_VAR(nIRQPeriod);
		SCAN_VAR(nIRQCountdown);

		SCAN_VAR(sample_rate);  // sample rate of dcs
		ScanVar(mixer_buffer, MIXER_BUFFER_LEN * sizeof(INT16), "DcsMixerBuffer");
		SCAN_VAR(mixer_pos);
		SCAN_VAR(nRenderAcc);
		SCAN_VAR(bRenderPrimed);
		SCAN_VAR(nLastSample);
	}

	return 0;
}
