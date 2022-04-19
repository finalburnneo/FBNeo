// license:BSD-3-Clause
// copyright-holders:David Haywood, Luca Elia
/* Serial Flash */

// **FBNeo port note**
// Flash data saved to the device doesn't get saved or loaded, since what
// we're using this for simply uses the flash chips as a ROM source.
// - dink  april 16, 2022

void serflash_init(UINT8 *rom, INT32 length);
void serflash_exit();
void serflash_reset();

void serflash_scan(INT32 nAction, INT32 *pnMin);

void serflash_enab_write(UINT8 data);
void serflash_cmd_write(UINT8 data);
void serflash_data_write(UINT8 data);
void serflash_addr_write(UINT8 data);
UINT8 serflash_io_read();
UINT8 serflash_ready_read();

UINT8 serflash_n3d_flash_read(INT32 offset);
void serflash_n3d_flash_cmd_write(INT32 offset, UINT8 data);
void serflash_n3d_flash_addr_write(INT32 offset, UINT8 data);

