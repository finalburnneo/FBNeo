#include "burnint.h"
#include "t11_intf.h"

#define MAX_MEMORY		0x10000
#define MAX_MEMORY_AND	(MAX_MEMORY - 1)
#define PAGE_SHIFT		8
#define PAGE_SIZE		(1 << PAGE_SHIFT)
#define PAGE_COUNT		(MAX_MEMORY >> PAGE_SHIFT)
#define PAGE_BYTE_AND	(PAGE_SIZE - 1)	// byte align
#define PAGE_WORD_AND	(PAGE_SIZE - 2)	// word align

#define READ	0
#define WRITE	1
#define FETCH	2

static UINT8 *membase[3][PAGE_COUNT]; // 0 read, 1, write, 2 opcode

static void (*pWriteWordHandler)(UINT16, UINT16) = NULL;
static void (*pWriteByteHandler)(UINT16, UINT8 ) = NULL;

static UINT16 (*pReadWordHandler)(UINT16) = NULL;
static UINT8  (*pReadByteHandler)(UINT16) = NULL;

extern void t11_set_irq_line(INT32 irqline, INT32 state);

void t11Open(INT32)
{
}
void t11Close()
{
}

static UINT8 t11_read_rom_byte(UINT32 a) // for cheating
{
	return t11ReadByte(a);
}

static void core_set_irq(INT32 /*cpu*/, INT32 irqline, INT32 state)
{
	t11_set_irq_line(irqline, state);
}

cpu_core_config t11Config =
{
	"t11",
	t11Open,
	t11Close,
	t11_read_rom_byte,
	t11_write_rom_byte,
	t11GetActive,
	t11TotalCycles,
	t11NewFrame,
	t11Idle,
	core_set_irq,
	t11Run,
	t11RunEnd,
	t11Reset,
	t11Scan,
	t11Exit,
	MAX_MEMORY,
	0
};

INT32 t11GetActive()
{
	return 0;
}

void t11Exit()
{
	
}

void t11MapMemory(UINT8 *src, UINT16 start, UINT16 finish, INT32 type)
{
	for (UINT32 i = start >> 8; i < (finish >> 8) + 1; i++)
	{
		if (type & (1 <<  READ)) membase[ READ][i] = src + ((i << 8) - (start & ~0xff));
		if (type & (1 << WRITE)) membase[WRITE][i] = src + ((i << 8) - (start & ~0xff));
		if (type & (1 << FETCH)) membase[FETCH][i] = src + ((i << 8) - (start & ~0xff));
	}
}

void t11SetWriteByteHandler(void (*write)(UINT16, UINT8))
{
	pWriteByteHandler = write;
}

void t11SetWriteWordHandler(void (*write)(UINT16, UINT16))
{
	pWriteWordHandler = write;
}

void t11SetReadByteHandler(UINT8 (*read)(UINT16))
{
	pReadByteHandler = read;
}

void t11SetReadWordHandler(UINT16 (*read)(UINT16))
{
	pReadWordHandler = read;
}

void t11WriteByte(UINT16 addr, UINT8 data)
{
	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		membase[WRITE][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
		return;
	}

	if (pWriteByteHandler) {
		pWriteByteHandler(addr, data);
	}
}

void t11WriteWord(UINT16 addr, UINT16 data)
{
	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		*((UINT16*)(membase[WRITE][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND))) = BURN_ENDIAN_SWAP_INT16(data);
		return;
	}

	if (pWriteWordHandler) {
		pWriteWordHandler(addr, data);
	}
}

UINT8 t11ReadByte(UINT16 addr)
{
	if (membase[ READ][addr >> PAGE_SHIFT] != NULL) {
		return membase[READ][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND];
	}

	if (pReadByteHandler) {
		return pReadByteHandler(addr);
	}

	return 0;
}

UINT16 t11ReadWord(UINT16 addr)
{
	if (membase[ READ][addr >> PAGE_SHIFT] != NULL) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(membase[ READ][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND))));
	}

	if (pReadWordHandler) {
		return pReadWordHandler(addr);
	}

	return 0;
}

UINT16 t11FetchWord(UINT16 addr)
{
	if (membase[FETCH][addr >> PAGE_SHIFT] != NULL) {
		return BURN_ENDIAN_SWAP_INT16(*((UINT16*)(membase[FETCH][addr >> PAGE_SHIFT] + (addr & PAGE_WORD_AND))));
	}

	// good enough for now...
	if (pReadWordHandler) {
		return pReadWordHandler(addr);
	}

	return 0;
}


void t11SetIRQLine(INT32 line, INT32 state)
{
	//bprintf(0, _T("t11SetIRQLine(%x,  %x)\n"), line, state);

	if (state == CPU_IRQSTATUS_NONE || state == CPU_IRQSTATUS_ACK)
	{
		t11_set_irq_line(line, state);
	} else {
		bprintf(0, _T("t11SetIRQLine(%d, %d) bad irq line / state?\n"), line, state);
	}
#if 0
	else if (CPU_IRQSTATUS_AUTO)
	{
		t11_set_irq_line(line, CPU_IRQSTATUS_ACK);
		t11Run(0);
		t11_set_irq_line(line, CPU_IRQSTATUS_NONE);
	}
#endif
}

void t11Init(INT32 mode, INT32 (*irqcallback)(INT32))
{
	extern void t11_core_init(INT32 mode, INT32 (*irqcallback)(INT32));

	t11_core_init(mode, irqcallback);

	memset (membase, 0, sizeof(UINT8*) * 3 * 0x100);

	pWriteWordHandler = NULL;
	pWriteByteHandler = NULL;
	pReadWordHandler = NULL;
	pReadByteHandler = NULL;

	CpuCheatRegister(0, &t11Config);
}

// For cheats/etc

void t11_write_rom_byte(UINT32 addr, UINT8 data)
{
	addr &= MAX_MEMORY_AND;

	// write to rom & ram
	if (membase[WRITE][addr >> PAGE_SHIFT] != NULL) {
		membase[WRITE][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
	}

	if (membase[READ][addr >> PAGE_SHIFT] != NULL) {
		membase[READ][addr >> PAGE_SHIFT][addr & PAGE_BYTE_AND] = data;
	}

	if (pWriteByteHandler) {
		pWriteByteHandler(addr, data);
	}
}
