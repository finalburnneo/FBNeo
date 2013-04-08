
#include "burnint.h"

void pic16c5xDoReset(INT32 type, INT32 *rom, INT32 *ram);
extern INT32 pic16c5xScanCpu(INT32 nAction, INT32* pnMin);

// masks
static INT32 rom_address_mask = 0x7ff;
static INT32 ram_address_mask = 0x07f;

INT32 nPic16c5xCpuType = -1;
static UINT8 *pic16c5x_mem = NULL;

INT32 (*pPic16c5xReadPort)(UINT16 port) = NULL;
void (*pPic16c5xWritePort)(UINT16 port, UINT8 data) = NULL;

UINT8 pic16c5x_read_byte(UINT16 address)
{
	address &= rom_address_mask;

	if (address <= rom_address_mask) {
		return pic16c5x_mem[address];
	}

	return 0;
}

void pic16c5x_write_byte(UINT16 address, UINT8 data)
{
	address &= rom_address_mask;

	if (address <= ram_address_mask && (address & 0x30) != 0x20) {
		pic16c5x_mem[address] = data;
		return;
	}
}

UINT8 pic16c5x_read_port(UINT16 port)
{
	if (pPic16c5xReadPort) {
		return pPic16c5xReadPort(port);
	}

	return 0;
}

void pic16c5x_write_port(UINT16 port, UINT8 data)
{
	if (pPic16c5xWritePort) {
		pPic16c5xWritePort(port, data);
		return;
	}
}

void pic16c5xReset()
{
	pic16c5xDoReset(nPic16c5xCpuType, &rom_address_mask, &ram_address_mask);
}

void pic16c5xInit(INT32 type, UINT8 *mem)
{
	nPic16c5xCpuType = type;

	pic16c5xDoReset(type, &rom_address_mask, &ram_address_mask);

	pic16c5x_mem = mem;
}

void pic16c5xExit()
{
	pic16c5x_mem = NULL;
	nPic16c5xCpuType = -1;
}

INT32 pic16c5xScan(INT32 nAction,INT32 */*pnMin*/)
{
	struct BurnArea ba;

	pic16c5xScanCpu(nAction, 0);

	if (nAction & ACB_MEMORY_RAM) {
		ba.Data		= pic16c5x_mem;
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

