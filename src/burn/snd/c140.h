/* C140.h */

#define C140_MAX_VOICE 24

enum
{
	C140_TYPE_SYSTEM2,
	C140_TYPE_SYSTEM21,
	C140_TYPE_ASIC219
};


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

struct C140_VOICE
{
	long    ptoffset;
	long    pos;
	long    key;
	//--work
	long    lastdt;
	long    prevdt;
	long    dltdt;
	//--reg
	long    rvol;
	long    lvol;
	long    frequency;
	long    bank;
	long    mode;

	long    sample_start;
	long    sample_end;
	long    sample_loop;
};


// ======================> c140_device

void c140_init(INT32 clock, INT32 devtype, UINT8 *c140_rom);
void c140_exit();
void c140_reset();
void c140_scan();

void c140_update(INT16 *outputs, int samples_len);

UINT8 c140_read(UINT16 offset);
void c140_write(UINT16 offset, UINT8 data);

void c140_set_base(void *base);

