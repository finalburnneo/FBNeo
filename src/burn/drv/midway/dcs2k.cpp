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
static INT32 dcs_icycles;
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
static UINT64 nNextIRQCycle;
static UINT64 nTotalCycles;
static UINT8 bGenerateIRQ;

static INT32 samples_from; // samples per frame
static INT32 sample_rate;  // sample rate of dcs
static INT16 *mixer_buffer;
static INT32 mixer_pos;

void Dcs2kBoot();
static void SetupMemory();
static INT32 RxCallback(INT32 port);
static void TxCallback(INT32 port, INT32 data);
static void TimerCallback(INT32 enable);
static INT32 IRQCallback(INT32 line);

void DcsIRQ();

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

	mixer_buffer = (INT16*)BurnMalloc(44800 * 2);
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

void Dcs2kRender(INT16 *pSoundBuf, INT32 nSegmentLength)
{
	// dcs provides 240 samples every call to DcsIRQ__INT()
	// theory: check 3x a frame w/DcsCheckIRQ(), if mixer_pos gets below
	// samples_from - run IRQ to get more samples. -dink oct.29, 2020

	if (mixer_pos == 0) { // nothing to output
		memset(pSoundBuf, 0, nSegmentLength * 2 * sizeof(INT16));
		return;
	}

	for (INT32 j = 0; j < nSegmentLength; j++)
	{
		INT32 k = (samples_from * j) / nBurnSoundLen;

		INT32 l = mixer_buffer[k];
		INT32 r = mixer_buffer[k]; // dcs 2, 8k is mono :) (saved incase later we add stereo dcs)
		pSoundBuf[0] = BURN_SND_CLIP(l * dcs_volume);
		pSoundBuf[1] = BURN_SND_CLIP(r * dcs_volume);
		pSoundBuf += 2;
	}

	if (mixer_pos >= samples_from) {
		memmove(&mixer_buffer[0], &mixer_buffer[samples_from], (mixer_pos - samples_from) * sizeof(INT16));

		mixer_pos = mixer_pos - samples_from;

		//bprintf(0, _T("dcs2k: samples_from %d  extra samples: %d\n"), samples_from, mixer_pos);

		if (mixer_pos > 10000) {
			bprintf(0, _T("dcs2k: overrun!\n"));
			mixer_pos = 0;
		}
	} else {
		//bprintf(0, _T("dcs2k: underrun.  mixer_pos %d  samples %d\n"), mixer_pos, samples_from);
		mixer_pos = 0;
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

void Dcs2kRun(INT32 cycles)
{
	Adsp2100Run(cycles);

#if 0
	// we use line-based irq timing now! ignore this (saving just-incase)
	const INT64 nEndCycle = Adsp2100TotalCycles() + cycles;
    INT64 nNextCycles = cycles;

	while (Adsp2100TotalCycles() < nEndCycle) {
        // check for next IRQ
        if (Adsp2100TotalCycles() >= nNextIRQCycle) {
            if (bGenerateIRQ) {
                bGenerateIRQ = 1;
                DcsIRQ();
            }
        }
        INT64 next = nNextCycles;
        if ((Adsp2100TotalCycles() + next) >= nNextIRQCycle)
            next = (Adsp2100TotalCycles() + next) - nNextIRQCycle;

		INT64 elapsed = Adsp2100Run(next);
        nNextCycles -= elapsed;
	}
#endif
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

    nTotalCycles = 0ULL;
    nNextIRQCycle = ~0ULL;

	nCurrentBank = 0;
    nOutputData = 0;
    nInputData = 0;
    nLatchControl = 0;

    for (INT32 i = 0; i < 32; i++)
        nCtrlReg[i] = 0;

    SET_INPUT_EMPTY();
    SET_OUTPUT_EMPTY();

    Dcs2kBoot();

	dcs_icycles = ((dcs_mhz * 100) / nBurnFPS) / 2;

	mixer_pos = 0;
	sample_rate = 31250; // default
	samples_from = (INT32)((double)((sample_rate * 100) / nBurnFPS) + 0.5);
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
				nNextIRQCycle = ~0ULL;
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
				nNextIRQCycle = ~0ULL;
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
    if (nTxIncrement) {
        //adsp2100_state *adsp = Adsp2100GetState();
        //nNextIRQCycle = (nTotalCycles + adsp->icount) + dcs_icycles; // half of frame.
		nNextIRQCycle = Adsp2100TotalCycles() + dcs_icycles; // half of frame.
        bGenerateIRQ = 1;
        // stop CPU to recalculate next irq
		//Adsp2100RunEnd();
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

            src -= nTxIncrement;
            adsp->i[nTxIR] = src;
            nTxIRBase = src;

			INT32 sample_rate_old = sample_rate;
			sample_rate = (dcs_mhz / (2 * (nCtrlReg[S1_SCLKDIV_REG] + 1))) / 16;

			if (sample_rate != sample_rate_old) {
				bprintf(0, _T("dcs2k: new sample rate %d\n"), sample_rate);

				// re-calculate the #samples per frame
				samples_from = (INT32)((double)((sample_rate * 100) / nBurnFPS) + 0.5);
			}

            dcs_log(_T("tx_info base = %x, inc = %x, size = %x\n"), src, nTxIncrement, nTxSize);
            ComputeNextIRQCycle();
            return;
        }
    }
    dcs_log(_T("disable_irq\n"));
    bGenerateIRQ = 0;
    nNextIRQCycle = ~0ULL;
}

static void DcsIRQ__INT()
{
	if (!bGenerateIRQ) return;
    adsp2100_state *adsp = Adsp2100GetState();
    INT32 r = adsp->i[nTxIR];

    INT32 count = nTxSize / 2;

    for (INT32 i = 0; i < count; i++) {
        mixer_buffer[mixer_pos++] = adsp21xx_data_read_word_16le(r<<1);
        r += nTxIncrement;
	}
	// bprintf(0, _T("mixer_pos %d [%04x %04x %04x %04x %04x %04x %04x %04x]\n"), mixer_pos, mixer_buffer[0], mixer_buffer[0], mixer_buffer[1], mixer_buffer[2], mixer_buffer[3], mixer_buffer[4+0], mixer_buffer[4+0], mixer_buffer[4+1], mixer_buffer[4+2], mixer_buffer[4+3]);

	bool wrapped = false;

    if (r >= (nTxIRBase + nTxSize)) {
        r = nTxIRBase;
        wrapped = true;
    }
    adsp->i[nTxIR] = r;

    nNextIRQCycle = Adsp2100TotalCycles() + dcs_icycles;
    bGenerateIRQ = 1;

    if (wrapped) {
        Adsp2100SetIRQLine(ADSP2105_IRQ1, CPU_IRQSTATUS_ACK);
        Adsp2100Run(1);
        Adsp2100SetIRQLine(ADSP2105_IRQ1, CPU_IRQSTATUS_NONE);
    }
}

void DcsCheckIRQ()
{
	if (pBurnSoundOut == NULL)
		mixer_pos = 0;

	if (mixer_pos < samples_from) {
		DcsIRQ__INT();
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
		SCAN_VAR(nNextIRQCycle);
		SCAN_VAR(nTotalCycles);
		SCAN_VAR(bGenerateIRQ);

		SCAN_VAR(samples_from); // samples per frame
		SCAN_VAR(sample_rate);  // sample rate of dcs
		ScanVar(mixer_buffer, 10000 * sizeof(INT16), "DcsMixerBuffer");
		SCAN_VAR(mixer_pos);
	}

	return 0;
}
