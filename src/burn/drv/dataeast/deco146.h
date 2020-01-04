// license:BSD-3-Clause
// copyright-holders:David Haywood, Charles MacDonald

#include "bitswap.h"

// handy handlers
void deco146_104_prot_wb(UINT32 region, UINT32 address, UINT8 data);
void deco146_104_prot_ww(UINT32 region, UINT32 address, UINT16 data);
UINT8 deco146_104_prot_rb(UINT32 region, UINT32 address);
UINT16 deco146_104_prot_rw(UINT32 region, UINT32 address);

// raw handlers
void deco_146_104_write_data(UINT16 address, UINT16 data, UINT16 mem_mask, UINT8 &csflags);
UINT16 deco_146_104_read_data(UINT16 address, UINT16 mem_mask, UINT8 &csflags);

// configuration
void deco_146_104_set_port_a_cb(UINT16 (*port_cb)()); // inputs
void deco_146_104_set_port_b_cb(UINT16 (*port_cb)()); // system
void deco_146_104_set_port_c_cb(UINT16 (*port_cb)()); // dips
void deco_146_104_set_soundlatch_cb(void (*port_cb)(UINT16 sl));
void deco_146_104_set_interface_scramble(UINT8 a9, UINT8 a8, UINT8 a7, UINT8 a6, UINT8 a5, UINT8 a4, UINT8 a3,UINT8 a2,UINT8 a1,UINT8 a0);
void deco_146_104_set_interface_scramble_reverse();
void deco_146_104_set_interface_scramble_interleave();
void deco_146_104_set_use_magic_read_address_xor(INT32 use_xor);

// init, etc.
void deco_146_init();
void deco_104_init();
// these 3 autoatically called from deco16Scan(), deco16Reset(), deco16Exit()
void deco_146_104_scan();
void deco_146_104_reset();
void deco_146_104_exit();

extern INT32 deco_146_104_inuse;

#define BLK (0xff)
#define INPUT_PORT_A (-1)
#define INPUT_PORT_B (-2)
#define INPUT_PORT_C (-3)

#define NIB3__ 0xc, 0xd, 0xe, 0xf
#define NIB3R1 0xd, 0xe, 0xf, 0xc
#define NIB3R2 0xe, 0xf, 0xc, 0xd
#define NIB3R3 0xf, 0xc, 0xd, 0xe

#define NIB2__ 0x8, 0x9, 0xa, 0xb
#define NIB2R1 0x9, 0xa, 0xb, 0x8
#define NIB2R2 0xa, 0xb, 0x8, 0x9
#define NIB2R3 0xb, 0x8, 0x9, 0xa

#define NIB1__ 0x4, 0x5, 0x6, 0x7
#define NIB1R1 0x5, 0x6, 0x7, 0x4
#define NIB1R2 0x6, 0x7, 0x4, 0x5
#define NIB1R3 0x7, 0x4, 0x5, 0x6

#define NIB0__ 0x0, 0x1, 0x2, 0x3
#define NIB0R1 0x1, 0x2, 0x3, 0x0
#define NIB0R2 0x2, 0x3, 0x0, 0x1
#define NIB0R3 0x3, 0x0, 0x1, 0x2

#define BLANK_ BLK, BLK, BLK, BLK

struct deco146port_xx
{
	INT32 write_offset;
	UINT8 mapping[16];
	INT32 use_xor;
	INT32 use_nand;
};
