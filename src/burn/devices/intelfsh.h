/*
    Intel Flash ROM emulation
*/

#ifndef _INTELFLASH_H_
#define _INTELFLASH_H_

#define FLASH_CHIPS_MAX	( 56 )

#define FLASH_INTEL_28F016S5 ( 0 )
#define FLASH_SHARP_LH28F400 ( 1 )
#define FLASH_FUJITSU_29F016A ( 2 )
#define FLASH_INTEL_E28F008SA ( 3 )
#define FLASH_INTEL_TE28F160 ( 4 )
#define FLASH_SHARP_LH28F016S ( 5 )

void intelflash_init( int chip, int type, void *data );
UINT32 intelflash_read( int chip, UINT32 address );
void intelflash_write( int chip, UINT32 address, UINT32 value );
void intelflash_write_raw( int chip, UINT32 address, UINT8 value );

INT32 intelflash_scan(INT32 nAction, INT32 *pnMin);
void intelflash_exit();

#endif
