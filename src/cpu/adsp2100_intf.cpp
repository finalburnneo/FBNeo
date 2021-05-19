#include "adsp2100/adsp2100.h"
#include "adsp2100_intf.h"

//#define xlog(...)   fprintf(stdout, "dcs: " __VA_ARGS__); fflush(stdout)
#define xlog(...)

#define ENABLE_TRACE    0

#define ADDR_BITS       16
#define PAGE_SIZE       0x100
#define PAGE_SHIFT      8
#define PAGE_MASK       0xFF
#define PAGE_COUNT      (1 << (ADDR_BITS - PAGE_SHIFT))
#define PAGE_WADD       (PAGE_COUNT)
#define ADSP_MAXHANDLER 10
#define PFN(x)          ((x >> PAGE_SHIFT) & 0xFF)

struct Adsp2100MemoryMap
{
    UINT8 *PrgMap[PAGE_COUNT * 2];
    UINT8 *DataMap[PAGE_COUNT * 2];

    pAdsp2100ReadLongHandler prgReadLong[ADSP_MAXHANDLER];
    pAdsp2100WriteLongHandler prgWriteLong[ADSP_MAXHANDLER];
    pAdsp2100ReadWordHandler dataReadWord[ADSP_MAXHANDLER];
    pAdsp2100WriteWordHandler dataWriteWord[ADSP_MAXHANDLER];
};

static Adsp2100MemoryMap *pMemMap;
static adsp2100_state *pADSP;

static pAdsp2100RxCallback pRxCallback;
static pAdsp2100TxCallback pTxCallback;
static pAdsp2100TimerCallback pTimerCallback;

#if ENABLE_TRACE
static FILE *pTrace;
#endif

static unsigned short DefReadWord(unsigned int a) { xlog("DefReadWord %x\n", a); return 0; }
static unsigned int DefReadLong(unsigned int a) { xlog("DefReadLong %x\n", a); return 0; }

static void DefWriteWord(unsigned int a, unsigned short value) { xlog("DefWriteWord %x - %x\n", a, value);  }
static void DefWriteLong(unsigned int a, unsigned int value) { xlog("DefWriteLog %x - %x\n", a, value); }

static void ResetMemoryMap()
{
    for (int page = 0; page < PAGE_COUNT; page++) {
        pMemMap->PrgMap[page] = (unsigned char*) 0;
        pMemMap->PrgMap[PAGE_WADD + page] = (unsigned char*) 0;
        pMemMap->DataMap[page] = (unsigned char*) 0;
        pMemMap->DataMap[PAGE_WADD + page] = (unsigned char*) 0;

    }

    for (int i = 0; i < ADSP_MAXHANDLER; i++) {
        pMemMap->prgReadLong[i] = DefReadLong;
        pMemMap->prgWriteLong[i] = DefWriteLong;

        pMemMap->dataWriteWord[i] = DefWriteWord;
        pMemMap->dataReadWord[i] = DefReadWord;
    }
}

static int RxCallback(adsp2100_state *adsp, int port)
{
    if (pRxCallback)
        return pRxCallback(port);
    return 0;
}

static void TxCallback(adsp2100_state *adsp, int port, int data)
{
    if (pTxCallback)
        pTxCallback(port, data);
}

static void TimerCallback(adsp2100_state *adsp, int enable)
{
    if (pTimerCallback)
        pTimerCallback(enable);
}


int Adsp2100Init()
{
    pMemMap = new Adsp2100MemoryMap;
    ResetMemoryMap();
    pADSP = (adsp2100_state*) BurnMalloc(sizeof(adsp2100_state));
    adsp2105_init(pADSP, NULL);
    pADSP->sport_rx_callback = RxCallback;
    pADSP->sport_tx_callback = TxCallback;
    pADSP->timer_fired = TimerCallback;

    pTimerCallback = NULL;
    pTxCallback = NULL;
    pRxCallback = NULL;

#if ENABLE_TRACE
    pTrace = fopen("adsp21xx.txt", "w");
#endif
    return 0;
}

int Adsp2100Exit()
{
    adsp21xx_exit(pADSP);
    BurnFree(pADSP);
    delete pMemMap;
    pMemMap = NULL;
#if ENABLE_TRACE
    fclose(pTrace);
#endif
    return 0;
}

void Adsp2100Reset()
{
    adsp21xx_reset(pADSP);
}

int Adsp2100Run(int cycles)
{
    return adsp21xx_execute(pADSP, cycles);
}

int Adsp2100TotalCycles()
{
	return adsp21xx_total_cycles(pADSP);
}

void Adsp2100NewFrame()
{
	adsp21xx_new_frame(pADSP);
}

void Adsp2100RunEnd()
{
	adsp21xx_stop_execute(pADSP);
}

void Adsp2100Scan(INT32 nAction)
{
	adsp21xx_scan(pADSP, nAction);
}

void Adsp2100SetRxCallback(pAdsp2100RxCallback cb)
{
    pRxCallback = cb;
}

void Adsp2100SetTxCallback(pAdsp2100TxCallback cb)
{
    pTxCallback = cb;
}

void Adsp2100SetTimerCallback(pAdsp2100TimerCallback cb)
{
    pTimerCallback = cb;
}

void Adsp2100SetIRQCallback(int (*cb)(int))
{
    if (pADSP)
        pADSP->irq_callback = cb;
}

void Adsp2100SetIRQLine(int line, int state)
{
	switch (state) {
		case CPU_IRQSTATUS_AUTO:
		case CPU_IRQSTATUS_HOLD:
			adsp21xx_set_irq_line(pADSP, line, CPU_IRQSTATUS_ACK);
			adsp21xx_set_irq_line(pADSP, line, CPU_IRQSTATUS_NONE);
			break;
		default:
			adsp21xx_set_irq_line(pADSP, line, state);
			break;
	}
}


int Adsp2100LoadBootROM(void *src, void *dst)
{
    adsp2105_load_boot_data((UINT8*)src, (UINT32*)dst);
    return 0;
}

int Adsp2100MapMemory(unsigned char* pMemory, unsigned int nStart, unsigned int nEnd, int nType)
{
    const int maxPages = (PFN(nEnd) - PFN(nStart)) + 1;

    int page = PFN(nStart);
    for (int i = 0; i < maxPages; i++, page++) {

        if (nType & MAP_READ)
            pMemMap->PrgMap[page] = pMemory + (PAGE_SIZE * i);

        if (nType & MAP_WRITE)
            pMemMap->PrgMap[PAGE_WADD + page] = pMemory + (PAGE_SIZE * i);
    }
    return 0;
}


int Adsp2100MapHandler(uintptr_t nHandler, unsigned int nStart, unsigned int nEnd, int nType)
{
    const int maxPages = (PFN(nEnd) - PFN(nStart)) + 1;

    int page = PFN(nStart);
    for (int i = 0; i < maxPages; i++, page++) {

        if (nType & MAP_READ)
            pMemMap->PrgMap[page] = (UINT8*) nHandler;

        if (nType & MAP_WRITE)
            pMemMap->PrgMap[PAGE_WADD + page] = (UINT8*) nHandler;
    }
    return 0;
}



int Adsp2100MapData(unsigned char* pMemory, unsigned int nStart, unsigned int nEnd, int nType)
{
    const int maxPages = (PFN(nEnd) - PFN(nStart)) + 1;

    int page = PFN(nStart);
    for (int i = 0; i < maxPages; i++, page++) {

        if (nType & MAP_READ)
            pMemMap->DataMap[page] = pMemory + (PAGE_SIZE * i);

        if (nType & MAP_WRITE)
            pMemMap->DataMap[PAGE_WADD + page] = pMemory + (PAGE_SIZE * i);
    }
    return 0;
}


int Adsp2100MapDataHandler(uintptr_t nHandler, unsigned int nStart, unsigned int nEnd, int nType)
{
    const int maxPages = (PFN(nEnd) - PFN(nStart)) + 1;

    int page = PFN(nStart);
    for (int i = 0; i < maxPages; i++, page++) {

        if (nType & MAP_READ)
            pMemMap->DataMap[page] = (UINT8*) nHandler;

        if (nType & MAP_WRITE)
            pMemMap->DataMap[PAGE_WADD + page] = (UINT8*) nHandler;
    }
    return 0;
}

// -------------------------------------------------------------------
// Program address space
// -------------------------------------------------------------------
int Adsp2100SetReadLongHandler(int i, pAdsp2100ReadLongHandler pHandler)
{
    if (i >= ADSP_MAXHANDLER)
        return 1;
    pMemMap->prgReadLong[i] = pHandler;
    return 0;
}

int Adsp2100SetWriteLongHandler(int i, pAdsp2100WriteLongHandler pHandler)
{
    if (i >= ADSP_MAXHANDLER)
        return 1;
    pMemMap->prgWriteLong[i] = pHandler;
    return 0;
}

// -------------------------------------------------------------------
// Data address space
// -------------------------------------------------------------------

int Adsp2100SetReadDataWordHandler(int i, pAdsp2100ReadWordHandler pHandler)
{
    if (i >= ADSP_MAXHANDLER)
        return 1;
    pMemMap->dataReadWord[i] = pHandler;
    return 0;
}

int Adsp2100SetWriteDataWordHandler(int i, pAdsp2100WriteWordHandler pHandler)
{
    if (i >= ADSP_MAXHANDLER)
        return 1;
    pMemMap->dataWriteWord[i] = pHandler;
    return 0;
}

// ============================================================================
//                       MEMORY ACCESSORS
// ============================================================================

template<typename T>
inline T fast_read(uint8_t *ptr, unsigned adr) {
    return *((T*)  ((uint8_t*) ptr + (adr & PAGE_MASK)));
}

template<typename T>
inline void fast_write(uint8_t *xptr, unsigned adr, T value) {
    T *ptr = ((T*)  ((uint8_t*) xptr + (adr & PAGE_MASK)));
    *ptr = value;
}

UINT16 adsp21xx_data_read_word_16le(UINT32 address)
{
//    address &= 0xFFFF;
    address >>= 1;
    address &= 0x3FFF;

    UINT8 *pr = pMemMap->DataMap[PFN(address)];
    if ((uintptr_t)pr >= ADSP_MAXHANDLER) {
        return BURN_ENDIAN_SWAP_INT16(fast_read<uint16_t>(pr, address));
    }
    return pMemMap->dataReadWord[(uintptr_t)pr](address);
}

void adsp21xx_data_write_word_16le(UINT32 address, UINT16 data)
{
//    address &= 0xFFFF;
    address >>= 1;
    address &= 0x3FFF;

    UINT8 *pr = pMemMap->DataMap[PAGE_WADD + PFN(address)];
    if ((uintptr_t)pr >= ADSP_MAXHANDLER) {
        fast_write<uint16_t>(pr, address, BURN_ENDIAN_SWAP_INT16(data));
        return;
    }
    pMemMap->dataWriteWord[(uintptr_t)pr](address, data);
}

//static UINT32 last_pc = 0xFFFFFFFF;
extern int adsp21xx_dasm(char *buffer, UINT8 *oprom);
UINT32 adsp21xx_read_dword_32le(UINT32 address)
{
//    address &= 0xFFFF;
    address >>= 2;
    address &= 0x3FFF;

    UINT32 value = 0;

    UINT8 *pr = pMemMap->PrgMap[PFN(address)];
    if ((uintptr_t)pr >= ADSP_MAXHANDLER) {
        value = BURN_ENDIAN_SWAP_INT32(fast_read<uint32_t>(pr, address));
    } else
        value = pMemMap->prgReadLong[(uintptr_t)pr](address);
#if ENABLE_TRACE
    if (last_pc != pADSP->pc) {
        char buffer[1024];
        adsp21xx_dasm(buffer, (UINT8*)&value);
        fprintf(pTrace, "%04X: %s\n", address>>2, buffer);
        last_pc = pADSP->pc;
    }
#endif
    return value;
}

void adsp21xx_write_dword_32le(UINT32 address, UINT32 data)
{
//    address &= 0xFFFF;
    address >>= 2;
    address &= 0x3FFF;

    UINT8 *pr = pMemMap->PrgMap[PAGE_WADD + PFN(address)];
    if ((uintptr_t)pr >= ADSP_MAXHANDLER) {
        fast_write<uint32_t>(pr, address, BURN_ENDIAN_SWAP_INT32(data));
        return;
    }
    pMemMap->prgWriteLong[(uintptr_t)pr](address, data);
}


adsp2100_state *Adsp2100GetState()
{
    return pADSP;
}

