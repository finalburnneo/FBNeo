#include "burnint.h"

void pic16c5xDoReset(INT32 type, INT32 *rom, INT32 *ram);
extern INT32 pic16c5xScanCpu(INT32 nAction, INT32* pnMin);

// masks
static INT32 rom_address_mask = 0x7ff;
static INT32 ram_address_mask = 0x07f;

INT32 nPic16c5xCpuType = -1;
static UINT8 *pic16c5x_rom = NULL;
static UINT8 *pic16c5x_ram = NULL;

static UINT8 (*pPic16c5xReadPort)(UINT16 port) = NULL;
static void (*pPic16c5xWritePort)(UINT16 port, UINT8 data) = NULL;

UINT16 pic16c5xFetch(UINT16 address)
{
#if defined FBA_DEBUG
	if (!DebugCPU_PIC16C5XInitted) bprintf(PRINT_ERROR, _T("pic16c5x_read_op called without init\n"));
#endif

	UINT16 *ROM = (UINT16*)pic16c5x_rom;
	
	address &= rom_address_mask;
	
	return ROM[address];
}

UINT8 pic16c5xRead(UINT16 address)
{
#if defined FBA_DEBUG
	if (!DebugCPU_PIC16C5XInitted) bprintf(PRINT_ERROR, _T("pic16c5x_read_byte called without init\n"));
#endif

	address &= ram_address_mask;

	if (nPic16c5xCpuType == 0x16C57 || nPic16c5xCpuType == 0x16C58) {
		if (address >= 0x60 && address <= 0x6f) {
			// mirror
			return pic16c5x_ram[address & 0x0f];
		}
	}
	return pic16c5x_ram[address];
}

void pic16c5xWrite(UINT16 address, UINT8 data)
{
#if defined FBA_DEBUG
	if (!DebugCPU_PIC16C5XInitted) bprintf(PRINT_ERROR, _T("pic16c5x_write_byte called without init\n"));
#endif

	address &= ram_address_mask;

	if (nPic16c5xCpuType == 0x16C57 || nPic16c5xCpuType == 0x16C58) {
		if (address >= 0x60 && address <= 0x6f) {
			// mirror
			pic16c5x_ram[address & 0x0f] = data;
			return;
		}
	}

	pic16c5x_ram[address] = data;
}

UINT8 pic16c5xReadPort(UINT16 port)
{
#if defined FBA_DEBUG
	if (!DebugCPU_PIC16C5XInitted) bprintf(PRINT_ERROR, _T("pic16c5x_read_port called without init\n"));
#endif

	if (pPic16c5xReadPort) {
		return pPic16c5xReadPort(port);
	}

	return 0;
}

void pic16c5xWritePort(UINT16 port, UINT8 data)
{
#if defined FBA_DEBUG
	if (!DebugCPU_PIC16C5XInitted) bprintf(PRINT_ERROR, _T("pic16c5x_write_port called without init\n"));
#endif

	if (pPic16c5xWritePort) {
		pPic16c5xWritePort(port, data);
		return;
	}
}

void pic16c5xSetReadPortHandler(UINT8 (*pReadPort)(UINT16 port))
{
	pPic16c5xReadPort = pReadPort;
}

void pic16c5xSetWritePortHandler(void (*pWritePort)(UINT16 port, UINT8 data))
{
	pPic16c5xWritePort = pWritePort;
}

void pic16c5xReset()
{
#if defined FBA_DEBUG
	if (!DebugCPU_PIC16C5XInitted) bprintf(PRINT_ERROR, _T("pic16c5xReset called without init\n"));
#endif

	pic16c5xDoReset(nPic16c5xCpuType, &rom_address_mask, &ram_address_mask);
}

void pic16c5xInit(INT32 /*nCPU*/, INT32 type, UINT8 *mem)
{
	DebugCPU_PIC16C5XInitted = 1;
	
	nPic16c5xCpuType = type;

	pic16c5xDoReset(type, &rom_address_mask, &ram_address_mask);

	pic16c5x_rom = mem;
	
	pic16c5x_ram = (UINT8*)BurnMalloc(ram_address_mask + 1);
}

void pic16c5xExit()
{
#if defined FBA_DEBUG
	if (!DebugCPU_PIC16C5XInitted) bprintf(PRINT_ERROR, _T("pic16c5xExit called without init\n"));
#endif

	pic16c5x_rom = NULL;
	nPic16c5xCpuType = -1;
	
	if (pic16c5x_ram) {
		BurnFree(pic16c5x_ram);
		pic16c5x_ram = NULL;
	}

	pPic16c5xReadPort = NULL;
	pPic16c5xWritePort = NULL;

	DebugCPU_PIC16C5XInitted = 0;
}

void pic16c5xOpen(INT32)
{
#if defined FBA_DEBUG
	if (!DebugCPU_PIC16C5XInitted) bprintf(PRINT_ERROR, _T("pic16c5xOpen called without init\n"));
#endif
}

void pic16c5xClose()
{
#if defined FBA_DEBUG
	if (!DebugCPU_PIC16C5XInitted) bprintf(PRINT_ERROR, _T("pic16c5xClose called without init\n"));
#endif
}

INT32 pic16c5xScan(INT32 nAction)
{
#if defined FBA_DEBUG
	if (!DebugCPU_PIC16C5XInitted) bprintf(PRINT_ERROR, _T("pic16c5xScan called without init\n"));
#endif

	struct BurnArea ba;

	pic16c5xScanCpu(nAction, 0);

	if (nAction & ACB_MEMORY_RAM) {
		ba.Data		= pic16c5x_ram;
		ba.nLen		= ram_address_mask + 1;
		ba.nAddress = 0;
		ba.szName	= "Internal RAM";
		BurnAcb(&ba);
	}

	return 0;
}


static UINT8 asciitohex(UINT8 data)
{
	/* Convert ASCII data to HEX */

	if ((data >= 0x30) && (data < 0x3a)) data -= 0x30;
	data &= 0xdf;			/* remove case sensitivity */
	if ((data >= 0x41) && (data < 0x5b)) data -= 0x37;

	return data;
}

extern void pic16c5x_config(INT32 data);

INT32 BurnLoadPicROM(UINT8 *src, INT32 offset, INT32 len)
{
	UINT8 *PICROM_HEX = (UINT8*)BurnMalloc(len);
	UINT16 *PICROM = (UINT16*)src;
	INT32   offs, data;
	UINT16  src_pos = 0;
	UINT16  dst_pos = 0;
	UINT8   data_hi, data_lo;

	if (BurnLoadRom(PICROM_HEX, offset, 1)) return 1;

	// Convert the PIC16C57 ASCII HEX dump to pure HEX
	do
	{
		if ((PICROM_HEX[src_pos + 0] == ':') &&
			(PICROM_HEX[src_pos + 1] == '1') &&
			(PICROM_HEX[src_pos + 2] == '0'))
			{
			src_pos += 9;

			for (offs = 0; offs < 32; offs += 4)
			{
				data_hi = asciitohex((PICROM_HEX[src_pos + offs + 0]));
				data_lo = asciitohex((PICROM_HEX[src_pos + offs + 1]));
				if ((data_hi <= 0x0f) && (data_lo <= 0x0f)) {
					data =  (data_hi <<  4) | (data_lo << 0);
					data_hi = asciitohex((PICROM_HEX[src_pos + offs + 2]));
					data_lo = asciitohex((PICROM_HEX[src_pos + offs + 3]));

					if ((data_hi <= 0x0f) && (data_lo <= 0x0f)) {
						data |= (data_hi << 12) | (data_lo << 8);
						PICROM[dst_pos] = data;
						dst_pos += 1;
					}
				}
			}
			src_pos += 32;
		}

		/* Get the PIC16C57 Config register data */

		if ((PICROM_HEX[src_pos + 0] == ':') &&
			(PICROM_HEX[src_pos + 1] == '0') &&
			(PICROM_HEX[src_pos + 2] == '2') &&
			(PICROM_HEX[src_pos + 3] == '1'))
			{
				src_pos += 9;

				data_hi = asciitohex((PICROM_HEX[src_pos + 0]));
				data_lo = asciitohex((PICROM_HEX[src_pos + 1]));
				data =  (data_hi <<  4) | (data_lo << 0);
				data_hi = asciitohex((PICROM_HEX[src_pos + 2]));
				data_lo = asciitohex((PICROM_HEX[src_pos + 3]));
				data |= (data_hi << 12) | (data_lo << 8);

				pic16c5x_config(data);
				src_pos = 0x7fff;		/* Force Exit */
		}
		src_pos += 1;
	} while (src_pos < len);		/* 0x2d4c is the size of the HEX rom loaded */

	BurnFree (PICROM_HEX);

	return 0;
}

