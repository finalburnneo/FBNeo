// FB Alpha Wild West C.O.W.-Boys of Moo Mesa / Bucky O'Hare driver module
// Based on MAME driver by R. Belmont, Acho A. Tang

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "konamiic.h"
#include "burn_ym2151.h"
#include "k054539.h"
#include "eeprom.h"

static UINT8 *AllMem;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROMExp0;
static UINT8 *DrvGfxROMExp1;
static UINT8 *DrvSndROM;
static UINT8 *DrvEeprom;
static UINT8 *AllRam;
static UINT8 *Drv68KRAM;
static UINT8 *Drv68KRAM2;
static UINT8 *Drv68KRAM3;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvProtRAM;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static UINT8 *soundlatch;
static UINT8 *soundlatch2;
static UINT8 *soundlatch3;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 layerpri[4];
static INT32 layer_colorbase[4];
static INT32 sprite_colorbase = 0;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvJoy5[16];
static UINT8 DrvReset;
static UINT16 DrvInputs[4];
static UINT8 DrvDips[1];

static INT32 sound_nmi_enable = 0;
static INT32 irq5_timer = 0;
static UINT16 control_data = 0;
static INT32 enable_alpha = 0;
static UINT8 z80_bank;

static UINT16 zmask;

static struct BurnInputInfo MooInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 fire 2"	},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 15,	"p3 start"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy3 + 10,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 11,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 8,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 9,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 12,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 13,	"p3 fire 2"	},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 15,	"p4 start"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 10,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 11,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 8,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 9,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 13,	"p4 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Service 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"service"	},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy5 + 3,	"diagnostics"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Moo)

static struct BurnInputInfo BuckyInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 fire 3"	},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy3 + 15,	"p3 start"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy3 + 10,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy3 + 11,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy3 + 8,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 9,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 12,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 13,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 14,	"p3 fire 3"	},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy4 + 15,	"p4 start"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 10,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 11,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 8,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 9,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 13,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 14,	"p4 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"service"	},
	{"Service 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"service"	},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy5 + 3,	"diagnostics"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Bucky)

static struct BurnDIPInfo MooDIPList[]=
{
	{0x26, 0xff, 0xff, 0x60, NULL			},

	{0   , 0xfe, 0   ,    2, "Sound Output"		},
	{0x26, 0x01, 0x10, 0x10, "Mono"			},
	{0x26, 0x01, 0x10, 0x00, "Stereo"		},

	{0   , 0xfe, 0   ,    2, "Coin Mechanism"	},
	{0x26, 0x01, 0x20, 0x20, "Common"		},
	{0x26, 0x01, 0x20, 0x00, "Independent"		},

	{0   , 0xfe, 0   ,    3, "Number of Players"	},
	{0x26, 0x01, 0xc0, 0xc0, "2"			},
	{0x26, 0x01, 0xc0, 0x40, "3"			},
	{0x26, 0x01, 0xc0, 0x80, "4"			},
};

STDDIPINFO(Moo)

static struct BurnDIPInfo BuckyDIPList[]=
{
	{0x2a, 0xff, 0xff, 0x60, NULL			},

	{0   , 0xfe, 0   ,    2, "Sound Output"		},
	{0x2a, 0x01, 0x10, 0x10, "Mono"			},
	{0x2a, 0x01, 0x10, 0x00, "Stereo"		},

	{0   , 0xfe, 0   ,    2, "Coin Mechanism"	},
	{0x2a, 0x01, 0x20, 0x20, "Common"		},
	{0x2a, 0x01, 0x20, 0x00, "Independent"		},

	{0   , 0xfe, 0   ,    3, "Number of Players"	},
	{0x2a, 0x01, 0xc0, 0xc0, "2"			},
	{0x2a, 0x01, 0xc0, 0x40, "3"			},
	{0x2a, 0x01, 0xc0, 0x80, "4"			},
};

STDDIPINFO(Bucky)

static void moo_objdma()
{
	INT32 num_inactive;
	UINT16 *dst = (UINT16*)K053247Ram;
	UINT16 *src = (UINT16*)DrvSprRAM;

	INT32 counter = 23;

	num_inactive = counter = 256;

	do
	{
		if ((*src & 0x8000) && (*src & zmask))
		{
			memcpy(dst, src, 0x10);
			dst += 8;
			num_inactive--;
		}
		src += 0x80;
	}
	while (--counter);

	if (num_inactive)
	{
		do
		{
			*dst = 0;
			dst += 8;
		}
		while (--num_inactive);
	}
}

static void moo_prot_write(INT32 offset)
{
	UINT16 *m_protram = (UINT16*)DrvProtRAM;

	if ((offset & 0x1e) == 0x18)  // trigger operation
	{
		UINT32 src1 = (m_protram[1] & 0xff) << 16 | m_protram[0];
		UINT32 src2 = (m_protram[3] & 0xff) << 16 | m_protram[2];
		UINT32 dst = (m_protram[5] & 0xff) << 16 | m_protram[4];
		UINT32 length = m_protram[0xf];

		while (length)
		{
			UINT32 a = SekReadWord(src1);
			UINT32 b = SekReadWord(src2);

			SekWriteWord(dst, a + 2 * b);

			src1 += 2;
			src2 += 2;
			dst += 2;
			length--;
		}
	}
}

static inline void sync_sound()
{
	INT32 cycles = (SekTotalCycles() / 2) - ZetTotalCycles();
	if (cycles > 0) {
		ZetRun(cycles);
	}
}

static void __fastcall moo_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffc000) == 0x1a0000) {
		K056832RamWriteWord(address & 0x1fff, data);
		return;
	}

	if ((address & 0xfffff8) == 0x0c2000) {
		K053246Write((address & 0x06) + 0, data >> 8);
		K053246Write((address & 0x06) + 1, data&0xff);
		return;
	}

	if ((address & 0xffffc0) == 0x0c0000) {
		K056832WordWrite(address & 0x3e, data);
		return;
	}

	if ((address & 0xffffe0) == 0x0ca000) {
		K054338WriteWord(address, data);
		return;
	}

	if ((address & 0xffffe0) == 0x0ce000) {
		*((UINT16*)(DrvProtRAM + (address & 0x1e))) = data;
		moo_prot_write(address);
		return;
	}

	if ((address & 0xfffff8) == 0x0d8000) {
		return; // regb
	}
	switch (address)
	{
		case 0x0de000:
			control_data = data;
			K053246_set_OBJCHA_line((data & 0x100) >> 8);
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
		return;
	}
}

static void __fastcall moo_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffc000) == 0x1a0000) {
		K056832RamWriteByte(address & 0x1fff, data);
		return;
	}

	if ((address & 0xffffc0) == 0x0c0000) {
		K056832ByteWrite(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0x0c2000) {
		K053246Write((address & 0x07) ^ 0, data);
		return;
	}

	if ((address & 0xffffe1) == 0x0cc001) {
		K053251Write((address / 2) & 0xf, data);
		return;
	}

	if ((address & 0xffffe0) == 0x0d0000) {
		// k053252 unimplemented
		return;
	}

	if ((address & 0xfffff8) == 0x0d8000) {
		return; // regb
	}

	switch (address)
	{
		case 0x0d4000:
		case 0x0d4001:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x0d600c:
		case 0x0d600d:
			sync_sound();
			*soundlatch = data;
		return;

		case 0x0d600e:
		case 0x0d600f:
			sync_sound();
			*soundlatch2 = data;
		return;

		case 0x0de000:
			control_data = (control_data & 0x00ff) + (data << 8);
		return;

		case 0x0de001:
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
			control_data = (control_data & 0xff00) + data;
		return;
	}
}

static UINT16 __fastcall moo_main_read_word(UINT32 address)
{
//bprintf (0, _T("MRW: %5.5x\n"), address);

	if ((address & 0xffc000) == 0x1a0000) {
		return K056832RamReadWord(address & 0x1fff);
	}

	if ((address & 0xffe000) == 0x1b0000) {
		return K056832RomWordRead(address);
	}

	switch (address)
	{
		case 0x0c4000:
			sync_sound();
			return K053246Read(1) + (K053246Read(0) << 8);

		case 0x0da000:
			return DrvInputs[2];

		case 0x0da002:
			return DrvInputs[3];

		case 0x0dc000:
			return DrvInputs[0] & 0xff;

		case 0x0dc002:
			return (DrvInputs[1] & 0xf8) | 2 | (EEPROMRead() ? 0x01 : 0);

		case 0x0de000:
			return control_data;
	}

	return 0;
}

static UINT8 __fastcall moo_main_read_byte(UINT32 address)
{
//bprintf (0, _T("MRB: %5.5x\n"), address);

	if ((address & 0xffc000) == 0x1a0000) {
		return K056832RamReadByte(address & 0x1fff);
	}

	if ((address & 0xffe000) == 0x1b0000) {
		return K056832RomWordRead(address) >> ((~address & 1) * 8);
	}

	switch (address)
	{
		case 0x0c4000:
		case 0x0c4001:
			sync_sound();
			return K053246Read(address & 1);

		case 0x0da000:
			return DrvInputs[2] >> 8;

		case 0x0da001:
			return DrvInputs[2];

		case 0x0da002:
			return DrvInputs[3] >> 8;

		case 0x0da003:
			return DrvInputs[3];

		case 0x0dc000:
			return DrvInputs[0] >> 8;

		case 0x0dc001:
			return DrvInputs[0];

		case 0x0dc002:
			return 0;

		case 0x0dc003:
			return ((DrvInputs[1]) & 0xf8) | 2 | (EEPROMRead() ? 0x01 : 0);

		case 0x0d6015:
			return *soundlatch3;

		case 0x0de000:
		case 0x0de001:
			return control_data >> ((~address & 1) * 8);
	}

	return 0;
}

static void __fastcall bucky_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffc000) == 0x180000) {
		K056832RamWriteWord(address & 0x1fff, data);
		return;
	}

	if ((address & 0xfffff8) == 0x0c2000) {
		K053246Write((address & 0x06) + 0, data >> 8);
		K053246Write((address & 0x06) + 1, data&0xff);
		return;
	}

	if ((address & 0xffffc0) == 0x0c0000) {
		K056832WordWrite(address & 0x3e, data);
		return;
	}

	if ((address & 0xffffe0) == 0x0ca000) {
		K054338WriteWord(address, data);
		return;
	}

	if ((address & 0xffffe0) == 0x0ce000) {
		*((UINT16*)(DrvProtRAM + (address & 0x1e))) = data;
		moo_prot_write(address);
		return;
	}

	if ((address & 0xffff00) == 0x0d2000) {
		K054000Write((address/2)&0xff, data);
		return;
	}

	if ((address & 0xfffff8) == 0x0d8000) {
		return; // regb
	}

	switch (address)
	{
		case 0x0de000:
			control_data = data;
			K053246_set_OBJCHA_line((data & 0x100) >> 8);
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
		return;

	}
}

static void __fastcall bucky_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffc000) == 0x180000) {
		K056832RamWriteByte(address & 0x1fff, data);
		return;
	}

	if ((address & 0xffffc0) == 0x0c0000) {
		K056832ByteWrite(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0x0c2000) {
		K053246Write((address & 0x07) ^ 0, data);
		return;
	}

	if ((address & 0xffffe1) == 0x0cc001) {
		K053251Write((address / 2) & 0xf, data);
		return;
	}

	if ((address & 0xffffe0) == 0x0d0000) {
		// k053252 unimplemented
		return;
	}

	if ((address & 0xffff00) == 0x0d2000) {
		K054000Write((address/2)&0xff, data);
		return;
	}

	if ((address & 0xfffff8) == 0x0d8000) {
		return; // regb
	}

	switch (address)
	{
		case 0x0d4000:
		case 0x0d4001:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x0d600c:
		case 0x0d600d:
			sync_sound();
			*soundlatch = data;
		return;

		case 0x0d600e:
		case 0x0d600f:
			sync_sound();
			*soundlatch2 = data;
		return;

		case 0x0de000:
			control_data = (control_data & 0x00ff) + (data << 8);
		return;

		case 0x0de001:
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
			control_data = (control_data & 0xff00) + data;
		return;
	}
}

static UINT16 __fastcall bucky_main_read_word(UINT32 address)
{
//bprintf (0, _T("MRW: %5.5x\n"), address);

	if ((address & 0xffff00) == 0x0d2000) {
		return K054000Read((address/2)&0xff);
	}

	if ((address & 0xffc000) == 0x180000) {
		return K056832RamReadWord(address & 0x1fff);
	}

	if ((address & 0xffe000) == 0x190000) {
		return K056832RomWordRead(address);
	}

	switch (address)
	{
		case 0x0c4000:
			sync_sound();
			return K053246Read(1) + (K053246Read(0) << 8);

		case 0x0da000:
			return DrvInputs[2];

		case 0x0da002:
			return DrvInputs[3];

		case 0x0dc000:
			return DrvInputs[0] & 0xff;

		case 0x0dc002:
			return (DrvInputs[1] & 0xf8) | 2 | (EEPROMRead() ? 0x01 : 0);

		case 0x0de000:
			return control_data;
	}

	return 0;
}

static UINT8 __fastcall bucky_main_read_byte(UINT32 address)
{
//bprintf (0, _T("MRB: %5.5x\n"), address);

	if ((address & 0xffff00) == 0x0d2000) {
		return K054000Read((address/2)&0xff);
	}

	if ((address & 0xffc000) == 0x180000) {
		return K056832RamReadByte(address & 0x1fff);
	}

	if ((address & 0xffe000) == 0x190000) {
		return K056832RomWordRead(address) >> ((~address & 1) * 8);
	}

	switch (address)
	{
		case 0x0c4000:
		case 0x0c4001:
			sync_sound();
			return K053246Read(address & 1);

		case 0x0da000:
			return DrvInputs[2] >> 8;

		case 0x0da001:
			return DrvInputs[2];

		case 0x0da002:
			return DrvInputs[3] >> 8;

		case 0x0da003:
			return DrvInputs[3];

		case 0x0dc000:
			return DrvInputs[0] >> 8;

		case 0x0dc001:
			return DrvInputs[0];

		case 0x0dc002:
			return 0;

		case 0x0dc003:
			return ((DrvInputs[1]) & 0xf8) | 2 | (EEPROMRead() ? 0x01 : 0);

		case 0x0d6015:
			return *soundlatch3;

		case 0x0de000:
		case 0x0de001:
			return control_data >> ((~address & 1) * 8);
	}

	return 0;
}

static void bankswitch(INT32 data)
{
	z80_bank = data;
	ZetMapMemory(DrvZ80ROM + ((data & 0x0f) * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall moo_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xec00:
			BurnYM2151SelectRegister(data);
		return;

		case 0xec01:
			BurnYM2151WriteRegister(data);
		return;

		case 0xf000:
			*soundlatch3 = data;
		return;

		case 0xf800:
			bankswitch(data);
		return;
	}

	if (address >= 0xe000 && address <= 0xe22f) {
		return K054539Write(0, address & 0x3ff, data);
	}
}

static UINT8 __fastcall moo_sound_read(UINT16 address)
{
	if (address >= 0xe000 && address <= 0xe22f) {
		return K054539Read(0, address & 0x3ff);
	}

	switch (address)
	{
		case 0xec00:
		case 0xec01:
			return BurnYM2151ReadStatus();

		case 0xf002:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch;

		case 0xf003:
			return *soundlatch2;
	}

	return 0;
}

static const eeprom_interface moo_eeprom_interface =
{
	7,			/* address bits */
	8,			/* data bits */
	"011000",		/* read command */
	"011100",		/* write command */
	"0100100000000",	/* erase command */
	"0100000000000",	/* lock command */
	"0100110000000", 	/* unlock command */
	0,
	0
};

static void moo_sprite_callback(INT32 */*code*/, INT32 *color, INT32 *priority)
{
	INT32 pri = (*color & 0x03e0) >> 4;

	if (pri <= layerpri[2])		*priority = 0x00;
	else if (pri <= layerpri[1])	*priority = 0xf0;
	else if (pri <= layerpri[0])	*priority = 0xfc;
	else				*priority = 0xfe;

	*color = sprite_colorbase | (*color & 0x001f);
}

static void moo_tile_callback(INT32 layer, INT32 */*code*/, INT32 *color, INT32 */*flags*/)
{
	*color = layer_colorbase[layer] | (*color >> 2 & 0x0f);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	bankswitch(2);
	ZetClose();

	KonamiICReset();

	BurnYM2151Reset();
	K054539Reset(0);

	EEPROMReset();

	if (EEPROMAvailable() == 0) {
		EEPROMFill(DrvEeprom, 0, 128);
	}

	control_data = 0;
	irq5_timer = 0;

	for (INT32 i = 0; i < 4; i++)
	{
		layer_colorbase[i] = 0;
		layerpri[i] = 0;
	}

	sound_nmi_enable = 0;
	z80_bank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x100000;
	DrvZ80ROM		= Next; Next += 0x040000;

	DrvGfxROM0		= Next; Next += 0x200000;
	DrvGfxROMExp0		= Next; Next += 0x400000;
	DrvGfxROM1		= Next; Next += 0x800000;
	DrvGfxROMExp1		= Next; Next += 0x1000000;

	DrvSndROM		= Next; Next += 0x400000;

	DrvEeprom		= Next; Next += 0x000080;

	konami_palette32	= (UINT32*)Next;
	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	Drv68KRAM2		= Next; Next += 0x010000;
	Drv68KRAM3		= Next; Next += 0x004000;
	DrvSprRAM		= Next; Next += 0x010000;
	DrvPalRAM		= Next; Next += 0x005000;

	DrvZ80RAM		= Next; Next += 0x002000;

	DrvProtRAM		= Next; Next += 0x000020;

	soundlatch		= Next; Next += 0x000001;
	soundlatch2		= Next; Next += 0x000001;
	soundlatch3		= Next; Next += 0x000001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 MooInit()
{
	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x080001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x080000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  5, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  6, 4, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  7, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  8, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004,  9, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006, 10, 8, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 11, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 12, 1)) return 1;

		K053247GfxDecode(DrvGfxROM0, DrvGfxROMExp0, 0x200000);
		K053247GfxDecode(DrvGfxROM1, DrvGfxROMExp1, 0x800000);
	}

	K054338Init();

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x200000, moo_tile_callback);
	K056832SetGlobalOffsets(40, 16);
	K056832SetLayerOffsets(0, -1, 0);
	K056832SetLayerOffsets(1,  3, 0);
	K056832SetLayerOffsets(2,  5, 0);
	K056832SetLayerOffsets(3,  7, 0);

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x7fffff, moo_sprite_callback, 1);
	K053247SetSpriteOffset(-88, -39);

	zmask = 0xffff;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KROM + 0x080000,	0x100000, 0x17ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x180000, 0x18ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x190000, 0x19ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x1c0000, 0x1c1fff, MAP_RAM);
	SekSetWriteWordHandler(0,		moo_main_write_word);
	SekSetWriteByteHandler(0,		moo_main_write_byte);
	SekSetReadWordHandler(0,		moo_main_read_word);
	SekSetReadByteHandler(0,		moo_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(moo_sound_write);
	ZetSetReadHandler(moo_sound_read);
	ZetClose();

	EEPROMInit(&moo_eeprom_interface);

	BurnYM2151Init(4000000);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	K054539Init(0, 48000, DrvSndROM, 0x200000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 0.75, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 0.75, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();

	return 0;
}

static INT32 BuckyInit()
{
	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x080001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x080000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  5, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  6, 4, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  7, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  8, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004,  9, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006, 10, 8, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 11, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x200000, 12, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 13, 1)) return 1;

		K053247GfxDecode(DrvGfxROM0, DrvGfxROMExp0, 0x200000);
		K053247GfxDecode(DrvGfxROM1, DrvGfxROMExp1, 0x800000);
	}

	K054338Init();

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x200000, moo_tile_callback);
	K056832SetGlobalOffsets(40, 16);
	K056832SetLayerOffsets(0, -2, 0);
	K056832SetLayerOffsets(1,  2, 0);
	K056832SetLayerOffsets(2,  4, 0);
	K056832SetLayerOffsets(3,  6, 0);

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x7fffff, moo_sprite_callback, 1);
	K053247SetSpriteOffset(-88, -39);

	zmask = 0x00ff;

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KROM + 0x080000,	0x200000, 0x23ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x080000, 0x08ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x090000, 0x09ffff, MAP_RAM);
	SekMapMemory(Drv68KRAM2,		0x0a0000, 0x0affff, MAP_RAM);
	SekMapMemory(Drv68KRAM3,		0x184000, 0x187fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x1b0000, 0x1b3fff, MAP_RAM);
	SekSetWriteWordHandler(0,		bucky_main_write_word);
	SekSetWriteByteHandler(0,		bucky_main_write_byte);
	SekSetReadWordHandler(0,		bucky_main_read_word);
	SekSetReadByteHandler(0,		bucky_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(moo_sound_write);
	ZetSetReadHandler(moo_sound_read);
	ZetClose();

	EEPROMInit(&moo_eeprom_interface);

	BurnYM2151Init(4000000);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	K054539Init(0, 48000, DrvSndROM, 0x400000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 0.75, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 0.75, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	KonamiICExit();

	SekExit();
	ZetExit();

	EEPROMExit();

	BurnYM2151Exit();
	K054539Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteRecalc()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x2000/2; i+=2)
	{
		INT32 r = pal[i+0] & 0xff;
		INT32 g = pal[i+1] >> 8;
		INT32 b = pal[i+1] & 0xff;

		DrvPalette[i/2] = (r << 16) + (g << 8) + b;
	}
}

static INT32 DrvDraw()
{
	DrvPaletteRecalc();

	KonamiClearBitmaps(0);

	static const INT32 K053251_CI[4] = { 1, 2, 3, 4 };
	INT32 layers[3];
	INT32 plane, alpha = 0xff;

	sprite_colorbase = K053251GetPaletteIndex(0);
	layer_colorbase[0] = 0x70;

	for (plane = 1; plane < 4; plane++)
	{
		layer_colorbase[plane] = K053251GetPaletteIndex(K053251_CI[plane]);
	}

	layers[0] = 1;
	layerpri[0] = K053251GetPriority(2);
	layers[1] = 2;
	layerpri[1] = K053251GetPriority(3);
	layers[2] = 3;
	layerpri[2] = K053251GetPriority(4);

	konami_sortlayers3(layers, layerpri);

	if (layerpri[0] < K053251GetPriority(1))
		if (nBurnLayer & (1<<layers[0])) K056832Draw(layers[0], 0, 1);

	if (nBurnLayer & (1<<layers[1])) K056832Draw(layers[1], 0, 2);

	enable_alpha = K054338_read_register(K338_REG_CONTROL) & K338_CTL_MIXPRI;
	alpha = (enable_alpha) ? K054338_set_alpha_level(1) : 255;

	if (alpha > 0)
		if (nBurnLayer & (1<<layers[2])) K056832Draw(layers[2], K056832_SET_ALPHA(alpha), 4);

	if (nSpriteEnable & 1) K053247SpritesRender();

	if (nBurnLayer & 1) K056832Draw(0, 0, 0);

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 4 * sizeof(INT16));
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		DrvInputs[1] = (DrvDips[0] & 0xf0) | ((DrvJoy5[3]) ? 0x00 : 0x08);
	}

	SekNewFrame();
	ZetNewFrame();

	INT32 nInterleave = 120;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 16000000 / 60, 8000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nNext, nCyclesSegment;

		nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[0];
		nCyclesDone[0] += SekRun(nCyclesSegment);

		if (i == (nInterleave - 1)) {
			if (K053246_is_IRQ_enabled()) {
				moo_objdma();
				irq5_timer = 5; // guess
			}

			if (control_data & 0x20) {
				SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
			}
		}

		if (i != (nInterleave - 1)) {
			if (irq5_timer > 0) {
				irq5_timer--;
				if (control_data & 0x800 && (irq5_timer == 0)) {
					SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
				}
			} 
		}

		nCyclesSegment = (SekTotalCycles() / 2) - ZetTotalCycles();
		if (nCyclesSegment > 0) nCyclesDone[1] += ZetRun(nCyclesSegment); // sync sound cpu to main cpu

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
		}
		K054539Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029732;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		ZetScan(nAction);

		BurnYM2151Scan(nAction);
		K054539Scan(nAction);

		KonamiICScan(nAction);

		SCAN_VAR(z80_bank);
		SCAN_VAR(sound_nmi_enable);

		SCAN_VAR(irq5_timer);

		SCAN_VAR(control_data);
		SCAN_VAR(enable_alpha);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(z80_bank);
		ZetClose();
	}

	EEPROMScan(nAction, pnMin);

	return 0;
}



// Wild West C.O.W.-Boys of Moo Mesa (ver EAB)

static struct BurnRomInfo moomesaRomDesc[] = {
	{ "151b01.q5",		0x040000, 0xfb2fa298, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "151eab02.q6",	0x040000, 0x37b30c01, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "151a03.t5",		0x040000, 0xc896d3ea, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "151a04.t6",		0x040000, 0x3b24706a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "151a07.f5",		0x040000, 0xcde247fc, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "151a05.t8",		0x100000, 0xbc616249, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "151a06.t10",		0x100000, 0x38dbcac1, 3 | BRF_GRA },           //  6

	{ "151a10.b8",		0x200000, 0x376c64f1, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "151a11.a8",		0x200000, 0xe7f49225, 4 | BRF_GRA },           //  8
	{ "151a12.b10",		0x200000, 0x4978555f, 4 | BRF_GRA },           //  9
	{ "151a13.a10",		0x200000, 0x4771f525, 4 | BRF_GRA },           // 10

	{ "151a08.b6",		0x200000, 0x962251d7, 5 | BRF_SND },           // 11 K054539 Samples

	{ "moomesa.nv",		0x000080, 0x7bd904a8, 6 | BRF_OPT },           // 12 eeprom data
};

STD_ROM_PICK(moomesa)
STD_ROM_FN(moomesa)

struct BurnDriver BurnDrvMoomesa = {
	"moomesa", NULL, NULL, NULL, "1992",
	"Wild West C.O.W.-Boys of Moo Mesa (ver EAB)\0", NULL, "Konami", "GX151",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, moomesaRomInfo, moomesaRomName, NULL, NULL, MooInputInfo, MooDIPInfo,
	MooInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Wild West C.O.W.-Boys of Moo Mesa (ver UAC)

static struct BurnRomInfo moomesauacRomDesc[] = {
	{ "151c01.q5",		0x040000, 0x10555732, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "151uac02.q6",	0x040000, 0x52ae87b0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "151a03.t5",		0x040000, 0xc896d3ea, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "151a04.t6",		0x040000, 0x3b24706a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "151a07.f5",		0x040000, 0xcde247fc, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "151a05.t8",		0x100000, 0xbc616249, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "151a06.t10",		0x100000, 0x38dbcac1, 3 | BRF_GRA },           //  6

	{ "151a10.b8",		0x200000, 0x376c64f1, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "151a11.a8",		0x200000, 0xe7f49225, 4 | BRF_GRA },           //  8
	{ "151a12.b10",		0x200000, 0x4978555f, 4 | BRF_GRA },           //  9
	{ "151a13.a10",		0x200000, 0x4771f525, 4 | BRF_GRA },           // 10

	{ "151a08.b6",		0x200000, 0x962251d7, 5 | BRF_SND },           // 11 K054539 Samples

	{ "moomesauac.nv",	0x000080, 0xa5cb137a, 6 | BRF_OPT },           // 12 eeprom data
};

STD_ROM_PICK(moomesauac)
STD_ROM_FN(moomesauac)

struct BurnDriver BurnDrvMoomesauac = {
	"moomesauac", "moomesa", NULL, NULL, "1992",
	"Wild West C.O.W.-Boys of Moo Mesa (ver UAC)\0", NULL, "Konami", "GX151",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, moomesauacRomInfo, moomesauacRomName, NULL, NULL, MooInputInfo, MooDIPInfo,
	MooInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Wild West C.O.W.-Boys of Moo Mesa (ver UAB)

static struct BurnRomInfo moomesauabRomDesc[] = {
	{ "151b01.q5",		0x040000, 0xfb2fa298, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "151uab02.q6",	0x040000, 0x3d9f4d59, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "151a03.t5",		0x040000, 0xc896d3ea, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "151a04.t6",		0x040000, 0x3b24706a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "151a07.f5",		0x040000, 0xcde247fc, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "151a05.t8",		0x100000, 0xbc616249, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "151a06.t10",		0x100000, 0x38dbcac1, 3 | BRF_GRA },           //  6

	{ "151a10.b8",		0x200000, 0x376c64f1, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "151a11.a8",		0x200000, 0xe7f49225, 4 | BRF_GRA },           //  8
	{ "151a12.b10",		0x200000, 0x4978555f, 4 | BRF_GRA },           //  9
	{ "151a13.a10",		0x200000, 0x4771f525, 4 | BRF_GRA },           // 10

	{ "151a08.b6",		0x200000, 0x962251d7, 5 | BRF_SND },           // 11 K054539 Samples

	{ "moomesauab.nv",	0x000080, 0xa5cb137a, 6 | BRF_OPT },           // 12 eeprom data
};

STD_ROM_PICK(moomesauab)
STD_ROM_FN(moomesauab)

struct BurnDriver BurnDrvMoomesauab = {
	"moomesauab", "moomesa", NULL, NULL, "1992",
	"Wild West C.O.W.-Boys of Moo Mesa (ver UAB)\0", NULL, "Konami", "GX151",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, moomesauabRomInfo, moomesauabRomName, NULL, NULL, MooInputInfo, MooDIPInfo,
	MooInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Wild West C.O.W.-Boys of Moo Mesa (ver AAB)

static struct BurnRomInfo moomesaaabRomDesc[] = {
	{ "151b01.q5",		0x040000, 0xfb2fa298, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "151aab02.q6",	0x040000, 0x2162d593, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "151a03.t5",		0x040000, 0xc896d3ea, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "151a04.t6",		0x040000, 0x3b24706a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "151a07.f5",		0x040000, 0xcde247fc, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "151a05.t8",		0x100000, 0xbc616249, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "151a06.t10",		0x100000, 0x38dbcac1, 3 | BRF_GRA },           //  6

	{ "151a10.b8",		0x200000, 0x376c64f1, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "151a11.a8",		0x200000, 0xe7f49225, 4 | BRF_GRA },           //  8
	{ "151a12.b10",		0x200000, 0x4978555f, 4 | BRF_GRA },           //  9
	{ "151a13.a10",		0x200000, 0x4771f525, 4 | BRF_GRA },           // 10

	{ "151a08.b6",		0x200000, 0x962251d7, 5 | BRF_SND },           // 11 K054539 Samples

	{ "moomesaaab.nv",	0x000080, 0x7bd904a8, 6 | BRF_OPT },           // 12 eeprom data
};

STD_ROM_PICK(moomesaaab)
STD_ROM_FN(moomesaaab)

struct BurnDriver BurnDrvMoomesaaab = {
	"moomesaaab", "moomesa", NULL, NULL, "1992",
	"Wild West C.O.W.-Boys of Moo Mesa (ver AAB)\0", NULL, "Konami", "GX151",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, moomesaaabRomInfo, moomesaaabRomName, NULL, NULL, MooInputInfo, MooDIPInfo,
	MooInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Wild West C.O.W.-Boys of Moo Mesa (bootleg)

static struct BurnRomInfo moomesablRomDesc[] = {
	{ "moo03.rom",		0x080000, 0xfed6a1cb, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "moo04.rom",		0x080000, 0xec45892a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "moo03.rom",		0x080000, 0xfed6a1cb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "moo04.rom",		0x080000, 0xec45892a, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "moo05.rom",		0x080000, 0x8c045f9c, 3 | BRF_GRA },           //  4 K056832 Characters
	{ "moo07.rom",		0x080000, 0xb9e29f50, 3 | BRF_GRA },           //  5
	{ "moo06.rom",		0x080000, 0x1261aa89, 3 | BRF_GRA },           //  6
	{ "moo08.rom",		0x080000, 0xe6937229, 3 | BRF_GRA },           //  7

	{ "151a10",		0x200000, 0x376c64f1, 4 | BRF_GRA },           //  8 K053247 Sprites
	{ "151a11",		0x200000, 0xe7f49225, 4 | BRF_GRA },           //  9
	{ "151a12",		0x200000, 0x4978555f, 4 | BRF_GRA },           // 10
	{ "151a13",		0x200000, 0x4771f525, 4 | BRF_GRA },           // 11

	{ "moo01.rom",	0x080000, 0x3311338a, 5 | BRF_SND },           // 12 MSM6295 Samples
	{ "moo02.rom",	0x080000, 0x2cf3a7c6, 5 | BRF_SND },           // 13

	{ "moo.nv",		0x000080, 0x7bd904a8, 6 | BRF_OPT },           // 14 eeprom data
};

STD_ROM_PICK(moomesabl)
STD_ROM_FN(moomesabl)

static INT32 moomesablInit()
{
	return 1;
}

struct BurnDriverD BurnDrvMoomesabl = {
	"moomesabl", "moomesa", NULL, NULL, "1992",
	"Wild West C.O.W.-Boys of Moo Mesa (bootleg)\0", NULL, "bootleg", "GX151",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, moomesablRomInfo, moomesablRomName, NULL, NULL, MooInputInfo, MooDIPInfo,
	moomesablInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 224, 4, 3
};


// Bucky O'Hare (ver EAB)

static struct BurnRomInfo buckyRomDesc[] = {
	{ "173eab01.q5",	0x040000, 0x7785ac8a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "173eab02.q6",	0x040000, 0x9b45f122, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "173a03.t5",		0x020000, 0xcd724026, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "173a04.t6",		0x020000, 0x7dd54d6f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "173a07.f5",		0x040000, 0x4cdaee71, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "173a05.t8",		0x100000, 0xd14333b4, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "173a06.t10",		0x100000, 0x6541a34f, 3 | BRF_GRA },           //  6

	{ "173a10.b8",		0x200000, 0x42fb0a0c, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "173a11.a8",		0x200000, 0xb0d747c4, 4 | BRF_GRA },           //  8
	{ "173a12.b10",		0x200000, 0x0fc2ad24, 4 | BRF_GRA },           //  9
	{ "173a13.a10",		0x200000, 0x4cf85439, 4 | BRF_GRA },           // 10

	{ "173a08.b6",		0x200000, 0xdcdded95, 5 | BRF_SND },           // 11 K054539 Samples
	{ "173a09.a6",		0x200000, 0xc93697c4, 5 | BRF_SND },           // 12

	{ "bucky.nv",		0x000080, 0x6a5986f3, 6 | BRF_OPT },           // 13 eeprom data
};

STD_ROM_PICK(bucky)
STD_ROM_FN(bucky)

struct BurnDriver BurnDrvBucky = {
	"bucky", NULL, NULL, NULL, "1992",
	"Bucky O'Hare (ver EAB)\0", NULL, "Konami", "GX173",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, buckyRomInfo, buckyRomName, NULL, NULL, BuckyInputInfo, BuckyDIPInfo,
	BuckyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 224, 4, 3
};


// Bucky O'Hare (ver EA)

static struct BurnRomInfo buckyeaRomDesc[] = {
	{ "2.d5",		0x040000, 0xe18518a6, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "3.d6",		0x040000, 0x45ef9545, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "173a03.t5",		0x020000, 0xcd724026, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "173a04.t6",		0x020000, 0x7dd54d6f, 1 | BRF_PRG | BRF_ESS }, //  3
	
	{ "173a07.f5",		0x040000, 0x4cdaee71, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "173a05.t8",		0x100000, 0xd14333b4, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "173a06.t10",		0x100000, 0x6541a34f, 3 | BRF_GRA },           //  6

	{ "173a10.b8",		0x200000, 0x42fb0a0c, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "173a11.a8",		0x200000, 0xb0d747c4, 4 | BRF_GRA },           //  8
	{ "173a12.b10",		0x200000, 0x0fc2ad24, 4 | BRF_GRA },           //  9
	{ "173a13.a10",		0x200000, 0x4cf85439, 4 | BRF_GRA },           // 10

	{ "173a08.b6",		0x200000, 0xdcdded95, 5 | BRF_SND },           // 11 K054539 Samples
	{ "173a09.a6",		0x200000, 0xc93697c4, 5 | BRF_SND },           // 12

	{ "bucky.nv",		0x000080, 0x6a5986f3, 6 | BRF_OPT },           // 13 eeprom data
};

STD_ROM_PICK(buckyea)
STD_ROM_FN(buckyea)

struct BurnDriver BurnDrvBuckyea = {
	"buckyea", "bucky", NULL, NULL, "1992",
	"Bucky O'Hare (ver EA)\0", NULL, "Konami", "GX173",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, buckyeaRomInfo, buckyeaRomName, NULL, NULL, BuckyInputInfo, BuckyDIPInfo,
	BuckyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 224, 4, 3
};


// Bucky O'Hare (ver JAA)

static struct BurnRomInfo buckyjaaRomDesc[] = {
	{ "173_ja_a01.05",	0x040000, 0x0a32bde7, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "173_ja_a02.06",	0x040000, 0x3e6f3955, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "173a03.t5",		0x020000, 0xcd724026, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "173a04.t6",		0x020000, 0x7dd54d6f, 1 | BRF_PRG | BRF_ESS }, //  3
	
	{ "173a07.f5",		0x040000, 0x4cdaee71, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "173a05.t8",		0x100000, 0xd14333b4, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "173a06.t10",		0x100000, 0x6541a34f, 3 | BRF_GRA },           //  6

	{ "173a10.b8",		0x200000, 0x42fb0a0c, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "173a11.a8",		0x200000, 0xb0d747c4, 4 | BRF_GRA },           //  8
	{ "173a12.b10",		0x200000, 0x0fc2ad24, 4 | BRF_GRA },           //  9
	{ "173a13.a10",		0x200000, 0x4cf85439, 4 | BRF_GRA },           // 10

	{ "173a08.b6",		0x200000, 0xdcdded95, 5 | BRF_SND },           // 11 K054539 Samples
	{ "173a09.a6",		0x200000, 0xc93697c4, 5 | BRF_SND },           // 12

	{ "buckyja.nv",		0x000080, 0x2f280a74, 6 | BRF_OPT },           // 13 eeprom data
};

STD_ROM_PICK(buckyjaa)
STD_ROM_FN(buckyjaa)

struct BurnDriver BurnDrvBuckyjaa = {
	"buckyjaa", "bucky", NULL, NULL, "1992",
	"Bucky O'Hare (ver JAA)\0", NULL, "Konami", "GX173",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, buckyjaaRomInfo, buckyjaaRomName, NULL, NULL, BuckyInputInfo, BuckyDIPInfo,
	BuckyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 224, 4, 3
};


// Bucky O'Hare (ver UAB)

static struct BurnRomInfo buckyuabRomDesc[] = {
	{ "173uab01.q5",	0x040000, 0xdcaecca0, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "173uab02.q6",	0x040000, 0xe3c856a6, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "173a03.t5",		0x020000, 0xcd724026, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "173a04.t6",		0x020000, 0x7dd54d6f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "173a07.f5",		0x040000, 0x4cdaee71, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "173a05.t8",		0x100000, 0xd14333b4, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "173a06.t10",		0x100000, 0x6541a34f, 3 | BRF_GRA },           //  6

	{ "173a10.b8",		0x200000, 0x42fb0a0c, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "173a11.a8",		0x200000, 0xb0d747c4, 4 | BRF_GRA },           //  8
	{ "173a12.b10",		0x200000, 0x0fc2ad24, 4 | BRF_GRA },           //  9
	{ "173a13.a10",		0x200000, 0x4cf85439, 4 | BRF_GRA },           // 10

	{ "173a08.b6",		0x200000, 0xdcdded95, 5 | BRF_SND },           // 11 K054539 Samples
	{ "173a09.a6",		0x200000, 0xc93697c4, 5 | BRF_SND },           // 12

	{ "buckyuab.nv",	0x000080, 0xa5cb137a, 6 | BRF_OPT },           // 13 eeprom data
};

STD_ROM_PICK(buckyuab)
STD_ROM_FN(buckyuab)

struct BurnDriver BurnDrvBuckyuab = {
	"buckyuab", "bucky", NULL, NULL, "1992",
	"Bucky O'Hare (ver UAB)\0", NULL, "Konami", "GX173",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, buckyuabRomInfo, buckyuabRomName, NULL, NULL, BuckyInputInfo, BuckyDIPInfo,
	BuckyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 224, 4, 3
};


// Bucky O'Hare (ver AAB)

static struct BurnRomInfo buckyaabRomDesc[] = {
	{ "173aab01.q5",	0x040000, 0x9193e89f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "173aab02.q6",	0x040000, 0x2567f3eb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "173a03.t5",		0x020000, 0xcd724026, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "173a04.t6",		0x020000, 0x7dd54d6f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "173a07.f5",		0x040000, 0x4cdaee71, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "173a05.t8",		0x100000, 0xd14333b4, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "173a06.t10",		0x100000, 0x6541a34f, 3 | BRF_GRA },           //  6

	{ "173a10.b8",		0x200000, 0x42fb0a0c, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "173a11.a8",		0x200000, 0xb0d747c4, 4 | BRF_GRA },           //  8
	{ "173a12.b10",		0x200000, 0x0fc2ad24, 4 | BRF_GRA },           //  9
	{ "173a13.a10",		0x200000, 0x4cf85439, 4 | BRF_GRA },           // 10

	{ "173a08.b6",		0x200000, 0xdcdded95, 5 | BRF_SND },           // 11 K054539 Samples
	{ "173a09.a6",		0x200000, 0xc93697c4, 5 | BRF_SND },           // 12

	{ "buckyaab.nv",	0x000080, 0x6a5986f3, 6 | BRF_OPT },           // 13 eeprom data
};

STD_ROM_PICK(buckyaab)
STD_ROM_FN(buckyaab)

struct BurnDriver BurnDrvBuckyaab = {
	"buckyaab", "bucky", NULL, NULL, "1992",
	"Bucky O'Hare (ver AAB)\0", NULL, "Konami", "GX173",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, buckyaabRomInfo, buckyaabRomName, NULL, NULL, BuckyInputInfo, BuckyDIPInfo,
	BuckyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	384, 224, 4, 3
};
