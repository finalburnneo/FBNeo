// chip dip
UINT8 cchip_asic_read(UINT32 offset);
void  cchip_asic_write68k(UINT32 offset, UINT16 data);
void  cchip_68k_write(UINT16 address, UINT8 data);
UINT8 cchip_68k_read(UINT16 address);

void  cchip_interrupt();
void  cchip_loadports(UINT8 pa, UINT8 pb, UINT8 pc, UINT8 padc);

void  cchip_init();
void  cchip_exit();
INT32 cchip_run(INT32 cyc);
void  cchip_reset();
INT32 cchip_scan(INT32 nAction);

extern UINT8 *cchip_rom;
extern UINT8 *cchip_eeprom;
extern UINT8 cchip_active;

// Handy macro's.  8/16bit 68k compatible
#define CCHIP_READ(base) \
	if (a >= (0x0000 | base) && a <= (0x07ff | base)) \
		return cchip_68k_read((a & 0x7ff) >> 1); \
	\
	if (a >= (0x0800 | base) && a <= (0x0fff | base)) \
		return cchip_asic_read((a & 0x7ff) >> 1);

#define CCHIP_WRITE(base) \
	if (a >= (0x0000 | base) && a <= (0x07ff | base)) { \
		cchip_68k_write((a & 0x7ff) >> 1, d); \
		return; \
	} \
	if (a >= (0x0800 | base) && a <= (0x0fff | base)) { \
		cchip_asic_write68k((a & 0x7ff) >> 1, d); \
		return; \
    }
