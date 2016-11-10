// FB Alpha pengadvb arcade driver module based on hap's mame driver

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
#include "tms9928a.h"
#include "8255ppi.h"
#include "bitswap.h"

extern "C" {
	#include "ay8910.h"
}
static INT16 *pAY8910Buffer[6];

static UINT8 *AllMem	= NULL;
static UINT8 *MemEnd	= NULL;
static UINT8 *AllRam	= NULL;
static UINT8 *RamEnd	= NULL;
static UINT8 *maincpu	= NULL;
static UINT8 *game = NULL;
static UINT8 *main_mem	= NULL;

static UINT8 DrvInputs[2];
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[1];
static UINT8 DrvReset;
static UINT8 DrvNMI;

static UINT8 msxmode = 0;
static UINT8 mem_map = 0;
static UINT8 mem_banks[4];

static struct BurnInputInfo PengadvbInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Pengadvb)

static struct BurnDIPInfo PengadvbDIPList[]=
{
	{0x08, 0xff, 0xff, 0x00, NULL		},
};

STDDIPINFO(Pengadvb)

static void __fastcall msx_write_port(UINT16 port, UINT8 data)
{
	port &= 0xff;
	switch (port)
	{
		case 0x98:
			TMS9928AWriteVRAM(data);
		return;

		case 0x99:
			TMS9928AWriteRegs(data);
		return;

		case 0xa0:
			AY8910Write(0, 0, data);
		break;

		case 0xa1:
			AY8910Write(0, 1, data);
		break;

		case 0xa8:
		case 0xa9:
		case 0xaa:
		case 0xab:
			ppi8255_w(0, port & 3, data);
		return;
	}

	//bprintf(0, _T("port[%X] data[%X],"), port, data);
}

static UINT8 __fastcall msx_read_port(UINT16 port)
{
	port &= 0xff;

	switch (port)
	{
		case 0x98:
			return TMS9928AReadVRAM();

		case 0x99:
			return TMS9928AReadRegs();

		case 0xa2:
			return AY8910Read(0);

		case 0xa8:
		case 0xa9:
		case 0xaa:
		case 0xab:
		    return ppi8255_r(0, port & 3);
	}

	//bprintf(0, _T("port[%X],"), port);

	return 0;
}

static void mem_map_banks()
{
	int slot_select;

	// page 0
	slot_select = (mem_map >> 0) & 0x03;
	switch(slot_select)
	{
		case 0:
			{
				ZetMapMemory(maincpu, 0x0000, 0x3fff, MAP_READ | MAP_FETCH);
				break;
			}
		case 1:
		case 2:
		case 3:
			{
				ZetUnmapMemory(0x0000, 0x3fff, MAP_READ | MAP_FETCH);
				break;
			}
	}

	// page 1
	slot_select = (mem_map >> 2) & 0x03;
	switch(slot_select)
	{
		case 0:
		{
			ZetMapMemory(maincpu + 0x4000, 0x4000, 0x5fff, MAP_READ | MAP_FETCH);
			ZetMapMemory(maincpu + 0x6000, 0x6000, 0x7fff, MAP_READ | MAP_FETCH);
			break;
		}
		case 1:
		{
			ZetMapMemory(game + mem_banks[0]*0x2000, 0x4000, 0x5fff, MAP_READ | MAP_FETCH);
			ZetMapMemory(game + mem_banks[1]*0x2000, 0x6000, 0x7fff, MAP_READ | MAP_FETCH);
			break;
		}
		case 2:
		case 3:
			{
				ZetUnmapMemory(0x4000, 0x7fff, MAP_READ | MAP_FETCH);
			break;
		}
	}

	// page 2
	slot_select = (mem_map >> 4) & 0x03;
	switch(slot_select)
	{
		case 1:
		{
			ZetMapMemory(game + mem_banks[2]*0x2000, 0x8000, 0x9fff, MAP_READ | MAP_FETCH);
			ZetMapMemory(game + mem_banks[3]*0x2000, 0xa000, 0xbfff, MAP_READ | MAP_FETCH);
			break;
		}
		case 0:
		case 2:
		case 3:
		{
			ZetUnmapMemory(0x8000, 0xbfff, MAP_READ | MAP_FETCH);
			break;
		}
	}

	// page 3
	slot_select = (mem_map >> 6) & 0x03;

	switch(slot_select)
	{
		case 0:
		case 1:
		case 2:
		{
			ZetUnmapMemory(0xc000, 0xffff, MAP_READ | MAP_FETCH);
			break;
		}
		case 3:
		{
			ZetMapMemory(main_mem, 0xc000, 0xffff, MAP_READ | MAP_FETCH);
			break;
		}
	}

}

static UINT8 sg1000_ppi8255_portB_read()
{
	if ((ppi8255_r(0, 2) & 0x0f) == 0)
		return DrvInputs[1];

	return 0xff;

}

static void sg1000_ppi8255_portA_write(UINT8 data)
{
	mem_map = data;
	mem_map_banks();
}

static UINT8 msx_input_mask = 0;

static UINT8 ay8910portAread(UINT32 /*offset*/)
{
	return DrvInputs[0];
}

static void ay8910portBwrite(UINT32 /*offset*/, UINT32 data)
{
	/* PSG reg 15, writes 0 at coin insert, 0xff at boot and game over */
	msx_input_mask = data;
}

static void vdp_interrupt(INT32 state)
{
	ZetSetIRQLine(0, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	TMS9928AReset();
	mem_map = 0;
	mem_banks[0] = mem_banks[1] = mem_banks[2] = mem_banks[3] = 0;
	mem_map_banks();
	ZetClose();

	AY8910Reset(0);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	maincpu		= Next; Next += 0x020000;
	game		= Next; Next += 0x020000;

	AllRam			= Next;

	main_mem		= Next; Next += 0x010400;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static void __fastcall msx_write(UINT16 address, UINT8 data)
{
	if (address >= 0xc000)
	{
		int slot_select = (mem_map >> 6) & 0x03;

		if ( slot_select == 3 )
		{
			main_mem[address - 0xc000] = data;
		}
	}
	else
	{
		switch(address)
		{
			case 0x4000: mem_banks[0] = data; mem_map_banks(); break;
			case 0x6000: mem_banks[1] = data; mem_map_banks(); break;
			case 0x8000: mem_banks[2] = data; mem_map_banks(); break;
			case 0xa000: mem_banks[3] = data; mem_map_banks(); break;
		}
	}
	//bprintf(0, _T("a[%X] d[%X],"), address, data);
}

static UINT8 __fastcall msx_read(UINT16 /*address*/)
{
	//bprintf(0, _T("a[%X],"), address);
	return 0;
}

static void pengadvb_decrypt(UINT8 *mem, INT32 memsize)
{
	UINT8 *buf;
	int i;

	// data lines swap
	for ( i = 0; i < memsize; i++ )
	{
		mem[i] = BITSWAP08(mem[i],7,6,5,3,4,2,1,0);
	}

	// address line swap
	buf = (UINT8 *)malloc(memsize);
	memcpy(buf, mem, memsize);
	for ( i = 0; i < memsize; i++ )
	{
		mem[i] = buf[BITSWAP24(i,23,22,21,20,19,18,17,16,15,14,13,5,11,10,9,8,7,6,12,4,3,2,1,0)];
	}
	free(buf);
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(maincpu, 0, 1)) return 1;

		if (msxmode) {
			if (BurnLoadRom(game + 0x00000, 1, 1)) return 1;
		} else {
			if (BurnLoadRom(game + 0x00000, 1, 1)) return 1;
			if (BurnLoadRom(game + 0x08000, 2, 1)) return 1;
			if (BurnLoadRom(game + 0x10000, 3, 1)) return 1;
			if (BurnLoadRom(game + 0x18000, 4, 1)) return 1;
			pengadvb_decrypt(game, 0x20000);
			//FILE * f = fopen("c:\\penga.rom", "wb+");
			//fwrite(game, 1, 0x20000, f);
			//fclose(f);
		}

		pengadvb_decrypt(maincpu, 0x8000);
	}

	ZetInit(0);
	ZetOpen(0);

	ZetSetOutHandler(msx_write_port);
	ZetSetInHandler(msx_read_port);
	ZetSetWriteHandler(msx_write);
	ZetSetReadHandler(msx_read);
	ZetClose();

	AY8910Init(0, 3579545/2, nBurnSoundRate, ay8910portAread, NULL, NULL, ay8910portBwrite);
	AY8910SetAllRoutes(0, 0.50, BURN_SND_ROUTE_BOTH);

	TMS9928AInit(TMS99x8A, 0x4000, 0, 0, vdp_interrupt);

	ppi8255_init(1);
	//PPI0PortReadA	= sg1000_ppi8255_portA_read;
	PPI0PortReadB	= sg1000_ppi8255_portB_read;
	//PPI0PortReadC	= sg1000_ppi8255_portC_read;
	PPI0PortWriteA	= sg1000_ppi8255_portA_write;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	TMS9928AExit();
	ZetExit();
	AY8910Exit(0);
	ppi8255_exit();

	BurnFree (AllMem);
	AllMem = NULL;

	msxmode = 0;

	return 0;
}

static INT32 DrvFrame()
{
	static UINT8 lastnmi = 0;

	if (DrvReset) {
		DrvDoReset();
	}

	{ // Compile Inputs
		memset (DrvInputs, 0xff, 2);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			if (i==6 || i==7)
				DrvInputs[1] ^= (DrvJoy1[i] & 1) << i;
			else
				DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 3579545 / 60 };
	INT32 nCyclesDone[1] = { 0 };
	INT32 nSoundBufferPos = 0;

    ZetOpen(0);

	if (DrvNMI && !lastnmi) {
		ZetNmi();
		lastnmi = DrvNMI;
	} else lastnmi = DrvNMI;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);

		TMS9928AScanline(i);

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
			nSoundBufferPos += nSegmentLength;
		}
	}

	ZetClose();

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
		}
	}

	if (pBurnDraw) {
		TMS9928ADraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);
		TMS9928AScan(nAction, pnMin);
	}

	return 0;
}


// Penguin Adventure (bootleg of MSX version)

static struct BurnRomInfo pengadvbRomDesc[] = {
	{ "rom.u5",	0x8000, 0xd21950d2, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu

	{ "rom.u7",	0x8000, 0xd4b4a4a4, 2 | BRF_GRA },           //  1 game
	{ "rom.u8",	0x8000, 0xeada2232, 2 | BRF_GRA },           //  2
	{ "rom.u9",	0x8000, 0x6478c561, 2 | BRF_GRA },           //  3
	{ "rom.u10",	0x8000, 0x5c48360f, 2 | BRF_GRA },           //  4
};

STD_ROM_PICK(pengadvb)
STD_ROM_FN(pengadvb)

struct BurnDriver BurnDrvPengadvb = {
	"pengadvb", NULL, NULL, NULL, "1988",
	"Penguin Adventure (bootleg of MSX version)\0", NULL, "bootleg (Screen) / Konami", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, pengadvbRomInfo, pengadvbRomName, NULL, NULL, PengadvbInputInfo, PengadvbDIPInfo,
	DrvInit, DrvExit, DrvFrame, TMS9928ADraw, DrvScan, NULL, TMS9928A_PALETTE_SIZE,
	272, 216, 4, 3
};
