#include <math.h>
#include <cstdint>
#include "adsp2100_intf.h"
#include "adsp2100/adsp2100.h"
#include "dcs2k.h"
#include <stdio.h>

//31250Hz sample rate
#define MHz(x)  (x * 1000000)
#define ADSP_CLOCK   MHz(10)

#define LOG_ENABLE  0
#define LOG_MEMACC  0

#if LOG_ENABLE
#define dcs_log(...)    fprintf(stdout, "dcs: " __VA_ARGS__); fflush(stdout)
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


template<class T>
class resampler_buffer {
    T *buffer;
    uint64_t head;
    uint64_t tail;
    size_t buffer_size;
    int target_rate;
    int host_rate;
    int t2h_count;
    double factor;
    int unused;
public:
    resampler_buffer(int buf_size) : buffer_size(buf_size) {
        buffer = new T[buffer_size];
        for (size_t i = 0; i < buffer_size; i++)
            buffer[i] = 0;
        head = 0;
        tail = 0;
        target_rate = 0;
        host_rate = 0;
    }
    ~resampler_buffer() {
        delete [] buffer;
    }

    void clear() {
        memset(buffer, 0, sizeof(T) * buffer_size);
    }

    void set_target_rate(int freq) {
        target_rate = freq;
    }

    void set_host_rate(int freq) {
        host_rate = freq;
        factor = static_cast<double>(host_rate) / static_cast<double>(target_rate);
        unused = target_rate * factor;
        t2h_count = target_rate / unused;
    }

    void write(const T *buf, size_t lenght) {
        uint64_t tail_ = tail;
        for (auto i = 0; i < lenght; i++) {
            buffer[tail_ % buffer_size] = buf[i];
            ++tail_;
        }
        tail = tail_;
    }

    T next() {
        T val = buffer[head % buffer_size];
        buffer[head++ % buffer_size] = 0;
        return val;
    }

    T sample() {
        int sum = 0;
        for (int x = 0; x < t2h_count; x++)
            sum += next();
        return sum / t2h_count;
    }

};

#define LATCH_OUTPUT_EMPTY  0x400
#define LATCH_INPUT_EMPTY   0x800
#define SET_OUTPUT_EMPTY()  nLatchControl |= LATCH_OUTPUT_EMPTY
#define SET_OUTPUT_FULL()   nLatchControl &= ~LATCH_OUTPUT_EMPTY
#define SET_INPUT_EMPTY()   nLatchControl |= LATCH_INPUT_EMPTY
#define SET_INPUT_FULL()    nLatchControl &= ~LATCH_INPUT_EMPTY

static UINT8 *pIntRAM;
static UINT8 *pExtRAM;
static UINT32 *pExtRAM32;
static UINT8 *pDataRAM;
static UINT8 *pSoundROM;
static UINT16 nCurrentBank;
static UINT32 nSoundSize;
static UINT32 nSoundBanks;
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
static INT64 nSavedCycles;
static resampler_buffer<INT16> *pSoundBuffer;
extern UINT16 adsp21xx_data_read_word_16le(UINT32 address);

void Dcs2kBoot();
static void SetupMemory();
static int RxCallback(int port);
static void TxCallback(int port, int data);
static void TimerCallback(int enable);
static int IRQCallback(int line);

static void DcsIRQ();

static UINT16 ReadRAM(UINT32 address);
static void WriteRAM(UINT32 address, UINT16 value);

static UINT16 ReadRAMBank(UINT32 address);

static UINT16 ReadData(UINT32 address);
static void WriteData(UINT32 address, UINT16 value);
static void SelectDataBank(UINT32 address, UINT16 value);
static UINT16 InputLatch(UINT32 address);
static void OutputLatch(UINT32 address, UINT16 value);
static UINT16 AdspRead(UINT32 address);
static void AdspWrite(UINT32 address, UINT16 value);

void Dcs2kInit()
{
    dcs_log("init\n");
    Adsp2100Init();

    Adsp2100SetIRQCallback(IRQCallback);
    Adsp2100SetRxCallback(RxCallback);
    Adsp2100SetTxCallback(TxCallback);
    Adsp2100SetTimerCallback(TimerCallback);

    pIntRAM = BurnMalloc(0x400 * sizeof(UINT32));
    pExtRAM = BurnMalloc(0x800 * sizeof(UINT32));
    pDataRAM = BurnMalloc(0x200 * sizeof(UINT16));

    pExtRAM32 = (UINT32*) pExtRAM;

    nCurrentBank = 0;
    nSoundBanks = 0;
    nSoundSize = 0;

    SetupMemory();

    pSoundBuffer = new resampler_buffer<INT16>(31250);
    pSoundBuffer->set_target_rate(31250);
    pSoundBuffer->set_host_rate(nBurnSoundRate);

    nCurrentBank = 0;
}

void Dcs2kExit()
{
    dcs_log("exit\n");
    Adsp2100Exit();
    delete pSoundBuffer;
    BurnFree(pIntRAM);
    BurnFree(pExtRAM);
    BurnFree(pDataRAM);
}


void Dcs2kRender(INT16 *pSoundBuf, INT32 nSegmentLenght)
{
    for (int i = 0; i < nSegmentLenght; i++) {
        INT16 sample = pSoundBuffer->sample();
        pSoundBuf[i * 2 + 0] = sample;
        pSoundBuf[i * 2 + 1] = sample;
    }
}

void Dcs2kRun(int cycles)
{
    const INT64 nEndCycle = nTotalCycles + cycles;
    INT64 nNextCycles = cycles;

    while (nTotalCycles < nEndCycle) {
        // check for next IRQ
        if (nTotalCycles >= nNextIRQCycle) {
            if (bGenerateIRQ) {
                bGenerateIRQ = 0;
                DcsIRQ();
            }
        }
        INT64 next = nNextCycles;
        if ((nTotalCycles + next) >= nNextIRQCycle)
            next = (nTotalCycles + next) - nNextIRQCycle;

        INT64 elapsed = Adsp2100Run(next);
        elapsed -= nSavedCycles;
        nSavedCycles = 0;
        nTotalCycles += elapsed;
        nNextCycles -= elapsed;
    }
}


static UINT32 ReadProgram(UINT32 address)
{
#if LOG_MEMACC
    dcs_log("prg_r %x - %x\n", address);
#endif
    UINT32 *p = (UINT32*)pIntRAM;
    return p[address & 0x3FF];
}

static void WriteProgram(UINT32 address, UINT32 value)
{
#if LOG_MEMACC
    dcs_log("prg_w %x - %x\n", address, value);
#endif
    UINT32 *p = (UINT32*)pIntRAM;
    p[address & 0x3FF] = value & 0xFFFFFF;
}

static UINT32 ReadProgramEXT(UINT32 address)
{
#if LOG_MEMACC
    dcs_log("prg_ext_r %x - %x\n", address, pExtRAM32[address & 0x7FF]);
#endif
    return pExtRAM32[address & 0x7FF];
}

static void WriteProgramEXT(UINT32 address, UINT32 value)
{
    int offset = address & 0x7FF;
#if LOG_MEMACC
    dcs_log("prg_ext_w %x - %x\n", address, value);
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
    Adsp2100MapHandler(2, 0x0800, 0x0FFF, MAP_RAM);
    Adsp2100MapHandler(2, 0x1000, 0x17FF, MAP_RAM);
    Adsp2100MapHandler(2, 0x1800, 0x1FFF, MAP_RAM);

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

    Adsp2100MapDataHandler(1, 0x0000, 0x07FF, MAP_RAM);
    Adsp2100MapDataHandler(1, 0x0800, 0x0FFF, MAP_RAM);
    Adsp2100MapDataHandler(1, 0x1000, 0x17FF, MAP_RAM);
    Adsp2100MapDataHandler(1, 0x1800, 0x1FFF, MAP_RAM);

    Adsp2100MapDataHandler(2, 0x2000, 0x2FFF, MAP_READ);

    Adsp2100MapDataHandler(3, 0x3000, 0x33FF, MAP_WRITE);

    Adsp2100MapDataHandler(4, 0x3400, 0x37FF, MAP_READ | MAP_WRITE);

    Adsp2100MapDataHandler(5, 0x3800, 0x39FF, MAP_READ | MAP_WRITE);

    Adsp2100MapDataHandler(6, 0x3FE0, 0x3FFF, MAP_READ | MAP_WRITE);
}

void Dcs2kMapSoundROM(void *ptr, int size)
{
    pSoundROM = (UINT8*)ptr;
    nSoundSize = size;
    nSoundBanks = (nSoundSize / 2) / 0x1000;
    dcs_log("Sound size %x", size);
    dcs_log("Sound banks %x", nSoundBanks);
}

extern UINT32 adsp21xx_read_dword_32le(UINT32 address);
void Dcs2kBoot()
{
    UINT8 buffer[0x1000];
    UINT16 *base;
    int i;

    base = (UINT16*) pSoundROM + ((nCurrentBank & 0x7FF) * 0x1000);

    for (i = 0; i < 0x1000; i++)
        buffer[i] = base[i];
    Adsp2100LoadBootROM(buffer, pIntRAM);
}

void Dcs2kReset()
{
    memset(pIntRAM, 0, 0x400 * sizeof(UINT32));
    memset(pExtRAM, 0, 0x800 * sizeof(UINT32));
    memset(pDataRAM, 0, 0x200 * sizeof(UINT16));
    Adsp2100Reset();
    Adsp2100SetIRQLine(ADSP2105_IRQ0, CLEAR_LINE);
    Adsp2100SetIRQLine(ADSP2105_IRQ1, CLEAR_LINE);
    Adsp2100SetIRQLine(ADSP2105_IRQ2, CLEAR_LINE);

    nTxIR = 0;
    nTxIRBase = 0;
    nTxSize = 0;
    nTxIncrement = 0;
    bGenerateIRQ = 0;

    nTotalCycles = 0ULL;
    nNextIRQCycle = ~0ULL;

    nOutputData = 0;
    nInputData = 0;
    nLatchControl = 0;

    for (int i = 0; i < 32; i++)
        nCtrlReg[i] = 0;

    SET_INPUT_EMPTY();
    SET_OUTPUT_EMPTY();

    Dcs2kBoot();

    pSoundBuffer->clear();
}

static UINT16 ReadRAMBank(UINT32 address)
{
    UINT16 *p = (UINT16 *) pSoundROM;
    unsigned offset = (nCurrentBank * 0x1000) + (address & 0xFFF);
#if LOG_MEMACC
    dcs_log("bank_r[%x] %x - %x\n", nCurrentBank, address & 0xFFF, p[offset]);
#endif
    return p[offset];

}

static UINT16 ReadRAM(UINT32 address)
{
#if LOG_MEMACC
    dcs_log("ram_r %x\n", address);
#endif
    UINT16 *p = (UINT16 *) pDataRAM;
    return p[address & 0x1FF];
}

static void WriteRAM(UINT32 address, UINT16 value)
{
#if LOG_MEMACC
    dcs_log("ram_w %x, %x\n", address, value);
#endif
    UINT16 *p = (UINT16 *) pDataRAM;
    p[address & 0x1FF] = value;
}

static UINT16 ReadData(UINT32 address)
{
    int offset = (address & 0x7FF);
#if LOG_MEMACC
    dcs_log("dataram_r %x - %x\n", offset, pExtRAM32[offset] >> 8);
#endif
    return pExtRAM32[offset] >> 8;
}

static void WriteData(UINT32 address, UINT16 value)
{
    int offset = (address & 0x7FF);
    UINT32 data = value;
    pExtRAM32[offset] &= 0xFF;
    pExtRAM32[offset] |= data << 8;
#if LOG_MEMACC
    dcs_log("dataram_w %x - %x\n", offset, value);
#endif
}

static void SelectDataBank(UINT32 address, UINT16 value)
{
    dcs_log("bank_select %x\n", value);
    nCurrentBank = value & 0x7FF;
}

static UINT16 InputLatch(UINT32 address)
{
    Adsp2100SetIRQLine(ADSP2105_IRQ2, CLEAR_LINE);
    SET_INPUT_EMPTY();
    dcs_log("input_latch %x\n", nInputData);
    return nInputData;
}

static void OutputLatch(UINT32 address, UINT16 value)
{
    dcs_log("output_latch %x, %x\n", address, value);
    SET_OUTPUT_FULL();
    nOutputData = value;
}

static UINT16 AdspRead(UINT32 address)
{
    dcs_log("adsp_r %x\n", address);
    return nCtrlReg[address & 0x1F];
}


static void AdspWrite(UINT32 address, UINT16 value)
{
    dcs_log("adsp_w %x, %x\n", address, value);
    nCtrlReg[address & 0x1F] = value;

    switch (address & 0x1F) {
    case SYSCONTROL_REG:
        if (value & 0x0200) {
            dcs_log("reboot\n");
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

void Dcs2kDataWrite(int data)
{
    dcs_log("data_w %x\n", data);
    SET_INPUT_FULL();
    Adsp2100SetIRQLine(ADSP2105_IRQ2, ASSERT_LINE);
    nInputData = data;
}

void Dcs2kResetWrite(int data)
{
    dcs_log("reset_w %x\n", data);
    if (data) {
        Dcs2kReset();
    }
}

int Dcs2kDataRead()
{
    dcs_log("data_r %x\n", nLatchControl);
    return nLatchControl;
}


static int RxCallback(int port)
{
    dcs_log("rx_sport %d\n", port);
    return 0;
}

static void ComputeNextIRQCycle()
{
    if (nTxIncrement) {
        adsp2100_state *adsp = Adsp2100GetState();
        nNextIRQCycle = (nTotalCycles + adsp->icount) + 76800/2;
        bGenerateIRQ = 1;
        nSavedCycles = adsp->icount;
        // stop CPU
        adsp->icount = 1;
    }

}

static void TxCallback(int port, int data)
{
    dcs_log("tx_sport p: %d, d: %d\n", port, data);
    if (port != 1)
        return;

    if (nCtrlReg[SYSCONTROL_REG] & 0x800) {
        if (nCtrlReg[S1_AUTOBUF_REG] & 2) {
            adsp2100_state *adsp = Adsp2100GetState();
            UINT16 src;
            int	m, l;

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

            dcs_log("tx_info base = %x, inc = %x, size = %x\n", src, nTxIncrement, nTxSize);
            ComputeNextIRQCycle();
            return;
        }
    }
    dcs_log("disable_irq\n");
    bGenerateIRQ = 0;
    nNextIRQCycle = ~0ULL;
}

static void DcsIRQ()
{
    adsp2100_state *adsp = Adsp2100GetState();
    int r = adsp->i[nTxIR];

    int count = nTxSize / 2;
    INT16 buffer[0x400];


    for (int i = 0; i < count; i++) {
        buffer[i] = adsp21xx_data_read_word_16le(r<<1);
        r += nTxIncrement;
    }

    pSoundBuffer->write(buffer, count);

    bool wrapped = false;

    if (r >= (nTxIRBase + nTxSize)) {
        r = nTxIRBase;
        wrapped = true;
    }
    adsp->i[nTxIR] = r;

    nNextIRQCycle = nTotalCycles + 76800/2;
    bGenerateIRQ = 1;

    if (wrapped) {
        Adsp2100SetIRQLine(ADSP2105_IRQ1, ASSERT_LINE);
        Adsp2100Run(1);
        Adsp2100SetIRQLine(ADSP2105_IRQ1, CLEAR_LINE);
        nTotalCycles++;
    }
}

static void TimerCallback(int enable)
{
    dcs_log("timer %d\n", enable);
}

static int IRQCallback(int line)
{
    dcs_log("irq %d\n", line);
    return 0;
}
