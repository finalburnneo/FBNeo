/*
    sms.c --
    Sega Master System console emulation.
*/
#include "smsshared.h"
#include "z80_intf.h"

/* SMS context */
sms_t sms;

/* Pull-up resistors on data bus */
UINT8 data_bus_pullup   = 0x00;
UINT8 data_bus_pulldown = 0x00;

static UINT8 dummy_write[0xffff];

static UINT8 *korean8kmap8000_9fff, *korean8kmapa000_bfff, *korean8kmap4000_5fff, *korean8kmap6000_7fff;

void __fastcall writemem_mapper_sega(UINT16 offset, UINT8 data)
{
	sms.wram[offset & 0x1fff] = data;

    if(offset >= 0xFFFC)
        sms_mapper_w(offset & 3, data);
}

void __fastcall writemem_mapper_codies(UINT16 offset, UINT8 data)
{
    switch(offset & 0xC000)
    {
        case 0x0000:
            sms_mapper_w(1, data);
            return;
        case 0x4000:
            sms_mapper_w(2, data);
            return;
        case 0x8000:
            sms_mapper_w(3, data);
            return;
    }
}

void __fastcall writemem_mapper_msx(UINT16 offset, UINT8 data)
{
	if (offset <= 0x0003) {
		sms_mapper8k_w(offset & 3, data);
		return;
	}

	if (offset >= 0xc000 /*&& offset <= 0xffff*/)
		sms.wram[offset & 0x1fff] = data;
}

void __fastcall writemem_mapper_none(UINT16 offset, UINT8 data)
{
	if (offset >= 0xc000 /*&& offset <= 0xffff*/)
		sms.wram[offset & 0x1fff] = data;
}

void __fastcall writemem_mapper_korea(UINT16 offset, UINT8 data)
{
	if (offset == 0xA000) {
		sms_mapper_w(3, data);
		return;
	}

	sms.wram[offset & 0x1fff] = data;
}

void __fastcall writemem_mapper_4pak(UINT16 offset, UINT8 data)
{
	switch (offset)	{
		case 0x3FFE: {
			sms_mapper_w(1, data);
			return;
		}
		case 0x7FFF: {
			sms_mapper_w(2, data);
			return;
		}
		case 0xBFFF: {
			sms_mapper_w(3, (cart.fcr[1] & 0x30) + data);
			return;
		}
	}

	sms.wram[offset & 0x1fff] = data;
}

void __fastcall writemem_mapper_xin1(UINT16 offset, UINT8 data)
{
	if (offset == 0xFFFF) {
		sms.wram[0x1fff] = data;
		cart.fcr[0] = data % (cart.pages * 2);
		return;
	}

	sms.wram[offset & 0x1fff] = data;
}

UINT8 __fastcall readmem_mapper_xin1(UINT16 offset) // HiCom Xin1
{
	if(offset >= 0xc000 /*&& offset <= 0xffff*/)
		return sms.wram[offset & 0x1fff];
	else
	if (offset >= 0x8000) {
		return cart.rom[offset & 0x3fff];
	}

	return cart.rom[(cart.fcr[0] * 0x8000) + offset];
}


void __fastcall writemem_mapper_korea8k(UINT16 offset, UINT8 data)
{
	if (offset == 0x4000) {
		sms_mapper8kvirt_w(2, data);
		return;
	}

	if (offset == 0x6000) {
		sms_mapper8kvirt_w(3, data);
		return;
	}

	if (offset == 0x8000) {
		sms_mapper8kvirt_w(0, data);
		return;
	}

	if (offset == 0xA000) {
		sms_mapper8kvirt_w(1, data);
		return;
	}

	if (offset == 0xFFFE) {
		sms_mapper8kvirt_w(2, (data << 1) & 0xFF);
		sms_mapper8kvirt_w(3, (1 + (data << 1)) & 0xFF);
	}
	else if (offset == 0xFFFF) {
		sms_mapper8kvirt_w(0, (data << 1) & 0xFF);
		sms_mapper8kvirt_w(1, (1 + (data << 1)) & 0xFF);
	}

	sms.wram[offset & 0x1fff] = data;
}

UINT8 __fastcall readmem_mapper_korea8k(UINT16 offset) // aka Janggun
{
	UINT8 data = 0;

	if(offset >= 0xc000 /*&& offset <= 0xffff*/)
		data = sms.wram[offset & 0x1fff];
	else
	if(/*offset >= 0x0000 &&*/ offset <= 0x3fff)
		data = cart.rom[offset];
	else
	if(offset >= 0x4000 && offset <= 0x5fff)
		data = korean8kmap4000_5fff[offset & 0x1fff];
	else
	if(offset >= 0x6000 && offset <= 0x7fff)
		data = korean8kmap6000_7fff[offset & 0x1fff];
	else
	if(offset >= 0x8000 && offset <= 0x9fff)
		data = korean8kmap8000_9fff[offset & 0x1fff];
	else
	if(offset >= 0xa000 && offset <= 0xbfff)
		data = korean8kmapa000_bfff[offset & 0x1fff];

	/* 16k page */
	UINT8 page = offset >> 14;

	/* $4000-$7FFF and $8000-$BFFF area are protected */
	if (((page == 1) && (cart.fcr[2] & 0x80)) || ((page == 2) && (cart.fcr[0] & 0x80)))	{
		/* bit-swapped value */
		data = (((data >> 7) & 0x01) | ((data >> 5) & 0x02) |
				((data >> 3) & 0x04) | ((data >> 1) & 0x08) |
				((data << 1) & 0x10) | ((data << 3) & 0x20) |
				((data << 5) & 0x40) | ((data << 7) & 0x80));
	}

	return data;
}

void sms_init(void)
{
	ZetInit(0);
	ZetOpen(0);

	/* Default: open bus */
    data_bus_pullup     = 0x00;
    data_bus_pulldown   = 0x00;

	bprintf(0, _T("Cart mapper: "));
    /* Assign mapper */
	if(cart.mapper == MAPPER_CODIES) {
		bprintf(0, _T("Codemasters\n"));
		ZetSetWriteHandler(writemem_mapper_codies);
	}
	else if (cart.mapper == MAPPER_MSX || cart.mapper == MAPPER_MSX_NEMESIS)
	{
		bprintf(0, _T("MSX\n"));
		ZetSetWriteHandler(writemem_mapper_msx);
	}
	else if (cart.mapper == MAPPER_NONE)
	{
		bprintf(0, _T("NONE.\n"));
		ZetSetWriteHandler(writemem_mapper_none);
	}
	else if (cart.mapper == MAPPER_KOREA)
	{
		bprintf(0, _T("Korea\n"));
		ZetSetWriteHandler(writemem_mapper_korea);
	}
	else if (cart.mapper == MAPPER_KOREA8K)
	{
		bprintf(0, _T("Korea 8k\n"));
		ZetSetWriteHandler(writemem_mapper_korea8k);
		ZetSetReadHandler(readmem_mapper_korea8k);
	}
	else if (cart.mapper == MAPPER_4PAK)
	{
		bprintf(0, _T("4PAK All Action\n"));
		ZetSetWriteHandler(writemem_mapper_4pak);
	}
	else if (cart.mapper == MAPPER_XIN1)
	{
		bprintf(0, _T("Hi Com Xin1\n"));
		ZetSetWriteHandler(writemem_mapper_xin1);
		ZetSetReadHandler(readmem_mapper_xin1);
	}
	else {
		bprintf(0, _T("Sega\n"));
		ZetSetWriteHandler(writemem_mapper_sega);
	}

    /* Force SMS (J) console type if FM sound enabled */
    if(sms.use_fm)
	{
		bprintf(0, _T("Emulating FM\n"));
        sms.console = CONSOLE_SMSJ;
        sms.territory = TERRITORY_DOMESTIC;
		sms.display = DISPLAY_NTSC;
    }

    /* Initialize selected console emulation */
    switch(sms.console)
    {
		case CONSOLE_SMS:
			ZetSetOutHandler(sms_port_w);
			ZetSetInHandler(sms_port_r);
            break;

        case CONSOLE_SMSJ:
			ZetSetOutHandler(sms_port_w);
			ZetSetInHandler(sms_port_r);
            break;

        case CONSOLE_SMS2:
			ZetSetOutHandler(sms_port_w);
			ZetSetInHandler(sms_port_r);
            data_bus_pullup = 0xFF;
            break;

        case CONSOLE_GG:
			ZetSetOutHandler(gg_port_w);
			ZetSetInHandler(gg_port_r);
            data_bus_pullup = 0xFF;
            break;

        case CONSOLE_GGMS:
			ZetSetOutHandler(ggms_port_w);
			ZetSetInHandler(ggms_port_r);
            data_bus_pullup = 0xFF;
            break;
	}
	ZetClose();
    sms_reset();
}

void sms_shutdown(void)
{
    /* Nothing to do */
}

void sms_reset(void)
{
	ZetOpen(0);

	/* Clear SMS context */
    memset(&dummy_write, 0, sizeof(dummy_write));
    memset(&sms.wram,    0, sizeof(sms.wram));
    memset(cart.sram,    0, sizeof(cart.sram));

    sms.paused      = 0x00;
    sms.save        = 0x00;
    sms.fm_detect   = 0x00;
    sms.memctrl     = 0xAB;
    sms.ioctrl      = 0xFF;

	if (IS_SMS)
		sms.wram[0] = 0xA8; // BIOS usually sets this. (memory control register)

	cart.fcr[0] = 0x00;
	cart.fcr[1] = 0x00;
	cart.fcr[2] = 0x01;
	cart.fcr[3] = 0x02;

	// Default (0x0000 - 0xbfff) mappings:
	if (cart.mapper == MAPPER_KOREA8K) { // aka Janggun ui Adeul (Kor)
		ZetMapMemory(cart.rom + 0x0000, 0x0000, 0x03ff, MAP_ROM);
		ZetMapMemory(cart.rom + 0x0400, 0x0400, 0x3fff, MAP_ROM);
		// These are mapped manually due to encryption (see readmem_mapper_korea8k()!)
		korean8kmap4000_5fff = cart.rom + 0x4000;
		korean8kmap6000_7fff = cart.rom + 0x6000;
		korean8kmap8000_9fff = cart.rom + 0x8000;
		korean8kmapa000_bfff = cart.rom + 0xa000;
		cart.fcr[2] = 0x00; cart.fcr[3] = 0x00;
	} else
	if (cart.mapper == MAPPER_XIN1) {
		// HiCom Xin1 Carts
		// Nothing here (uses virtual mapping, see readmem/writemem_mapper_xin1())
	} else {
		ZetMapMemory(cart.rom + 0x0000, 0x0000, 0x03ff, MAP_ROM);
		ZetMapMemory(cart.rom + 0x0400, 0x0400, 0x3fff, MAP_ROM);
		ZetMapMemory(cart.rom + 0x4000, 0x4000, 0x7fff, MAP_ROM);
		ZetMapMemory(cart.rom + 0x8000, 0x8000, 0xbfff, MAP_ROM);
	}

	// Work Ram (0xc000 - 0xffff) mappings:
	if(cart.mapper == MAPPER_CODIES || cart.mapper == MAPPER_4PAK) {
		ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xc000, 0xdfff, MAP_RAM);
		ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xe000, 0xffff, MAP_RAM);
	} else
	if(cart.mapper == MAPPER_SEGA || cart.mapper == MAPPER_KOREA8K || cart.mapper == MAPPER_XIN1) {
		ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xc000, 0xdfff, MAP_RAM);
		ZetMapMemory((UINT8 *)&dummy_write, 0x0000, 0xbfff, MAP_WRITE);
		ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xe000, 0xffff, MAP_READ);
	} else
	{ // MSX & Korea Mappers
		ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xc000, 0xdfff, MAP_RAM);
		ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xe000, 0xffff, MAP_RAM);
		memset(&sms.wram[1], 0xf0, sizeof(sms.wram) - 1); // this memory pattern fixes booting of a few korean games
		cart.fcr[2] = 0x00; cart.fcr[3] = 0x00;
	}
	ZetReset();
	ZetClose();

	switch (cart.mapper)
	{
		case MAPPER_MSX_NEMESIS: {
			bprintf(0, _T("(Nemesis-MSX: cart rom-page 0x0f remapped to 0x0000 - 0x1fff)\n"));
			cart.fcr[2] = 0x00; cart.fcr[3] = 0x00;
			UINT32 poffset = (0x0f) << 13;
			ZetOpen(0);
			ZetMapMemory(cart.rom + poffset, 0x0000, 0x1fff, MAP_ROM);
			ZetReset();
			ZetClose();
		}
	}

	if (IS_SMS) {
		// Z80 Stack Pointer set by SMS Bios, fix for Shadow Dancer and Ace of Aces
		// ZetSetSP() Must be called after ZetReset() when the cpu is in a closed state.
		ZetSetSP(0, 0xdff0);
	}
}

void sms_mapper8kvirt_w(INT32 address, UINT8 data)
{
    /* Calculate ROM page index */
	UINT32 poffset = (data % (cart.pages8k)) << 13;

    /* Save frame control register data */
    cart.fcr[address & 3] = data;
	//bprintf(0, _T("pof(%X)a%X = %X,"), poffset, address, data);

	/* 4 x 8k banks */
	switch (address & 3)
	{
		case 0: /* cartridge ROM bank (8k) at $8000-$9FFF */
			korean8kmap8000_9fff = cart.rom + poffset;
			break;

		case 1: /* cartridge ROM bank (8k) at $A000-$BFFF */
			korean8kmapa000_bfff = cart.rom + poffset;
			break;

		case 2: /* cartridge ROM bank (8k) at $4000-$5FFF */
			korean8kmap4000_5fff = cart.rom + poffset;
			break;

		case 3: /* cartridge ROM bank (8k) at $6000-$7FFF */
			korean8kmap6000_7fff = cart.rom + poffset;
			break;
	}
}

void sms_mapper8k_w(INT32 address, UINT8 data)
{
    /* Calculate ROM page index */
	UINT32 poffset = (data % (cart.pages8k)) << 13;

    /* Save frame control register data */
    cart.fcr[address & 3] = data;
	//bprintf(0, _T("pof(%X)a%X = %X,"), poffset, address, data);

	/* 4 x 8k banks */
	switch (address & 3)
	{
		case 0: /* cartridge ROM bank (8k) at $8000-$9FFF */
			ZetMapMemory(cart.rom + poffset, 0x8000, 0x9fff, MAP_ROM);
			break;

		case 1: /* cartridge ROM bank (8k) at $A000-$BFFF */
			ZetMapMemory(cart.rom + poffset, 0xa000, 0xbfff, MAP_ROM);
			break;

		case 2: /* cartridge ROM bank (8k) at $4000-$5FFF */
			ZetMapMemory(cart.rom + poffset, 0x4000, 0x5fff, MAP_ROM);
			break;

		case 3: /* cartridge ROM bank (8k) at $6000-$7FFF */
			ZetMapMemory(cart.rom + poffset, 0x6000, 0x7fff, MAP_ROM);
			break;
	}
}

void sms_mapper_w(INT32 address, UINT8 data)
{
    /* Calculate ROM page index */
	UINT32 poffset = (data % cart.pages) << 14;
   	//bprintf(0, _T("address[%X] pof(%X) data[%X],"), address, poffset, data);

    /* Save frame control register data */
    cart.fcr[address & 3] = data;

    switch(address & 3)
    {
        case 0: // page 2
            if(data & 8)
            {
                UINT32 offset = (data & 4) ? 0x4000 : 0x0000;
                sms.save = 1;
				ZetMapMemory((UINT8 *)&cart.sram + offset, 0x8000, 0xbfff, MAP_RAM);
            }
            else
            {
				poffset = ((cart.fcr[3] % cart.pages) << 14);
				ZetMapMemory(cart.rom + poffset, 0x8000, 0xbfff, MAP_ROM);
				if (cart.mapper == MAPPER_SEGA)
					ZetMapMemory((UINT8 *)&dummy_write, 0x8000, 0xbfff, MAP_WRITE);
            }
            break;

        case 1: // page 0
			ZetMapMemory(cart.rom + poffset, 0x0000, 0x3fff, MAP_ROM);
			if(cart.mapper != MAPPER_CODIES && cart.mapper != MAPPER_4PAK && cart.mapper != MAPPER_XIN1) // first 1k is in the Sega mapper
				ZetMapMemory(cart.rom + 0x0000, 0x0000, 0x03ff, MAP_ROM);
            break;

        case 2: // page 1
			ZetMapMemory(cart.rom + poffset, 0x4000, 0x7fff, MAP_ROM);
            break;

        case 3: // page 2
            if(!(cart.fcr[0] & 0x08))
            {
				ZetMapMemory(cart.rom + poffset, 0x8000, 0xbfff, MAP_ROM);
            }
            break;
    }
}

/* Read unmapped memory */
UINT8 z80_read_unmapped(void)
{
    INT32 pc = ZetGetPC(-1);
    UINT8 data;
	pc = (pc - 1) & 0xFFFF;
	data = ZetReadByte(pc);

	return ((data | data_bus_pullup) & ~data_bus_pulldown);
}

void memctrl_w(UINT8 data)
{
    sms.memctrl = data;
}

/*--------------------------------------------------------------------------*/
/* Sega Master System port handlers                                         */
/*--------------------------------------------------------------------------*/

void __fastcall sms_port_w(UINT16 port, UINT8 data)
{
    port &= 0xFF;

    if(port >= 0xF0)
    {
        switch(port)
        {
            case 0xF0:
                fmunit_write(0, data);
                return;

            case 0xF1:
                fmunit_write(1, data);
                return;

            case 0xF2:
                fmunit_detect_w(data);
                return;
        }
    }

    switch(port & 0xC1)
    {
        case 0x00:
            memctrl_w(data);
            return;

        case 0x01:
            ioctrl_w(data);
            return;

        case 0x40:
        case 0x41:
            psg_write(data);
            return;

        case 0x80:
        case 0x81:
            vdp_write(port, data);
            return;

        case 0xC0:
        case 0xC1:
            return;
    }
}

UINT8 __fastcall sms_port_r(UINT16 port)
{
    port &= 0xFF;

    if(port == 0xF2 && sms.use_fm)
        return fmunit_detect_r();

    switch(port & 0xC0)
    {
        case 0x00:
            return z80_read_unmapped();

        case 0x40:
            return vdp_counter_r(port);

        case 0x80:
            return vdp_read(port);

        case 0xC0:
            return input_r(port);
    }

    /* Just to please the compiler */
    return 0;
}



/*--------------------------------------------------------------------------*/
/* Game Gear port handlers                                                  */
/*--------------------------------------------------------------------------*/

void __fastcall gg_port_w(UINT16 port, UINT8 data)
{
    port &= 0xFF;

    if(port <= 0x06) {
        sio_w(port, data);
        return;
    }

    switch(port & 0xC1)
    {
        case 0x00:
            memctrl_w(data);
            return;

        case 0x01:
            ioctrl_w(data);
            return;

        case 0x40:
        case 0x41:
            psg_write(data);
            return;

        case 0x80:
        case 0x81:
            gg_vdp_write(port, data);
            return;
    }
}


UINT8 __fastcall gg_port_r(UINT16 port)
{
    port &= 0xFF;

    if(port <= 0x06)
        return sio_r(port);

    switch(port & 0xC0)
    {
        case 0x00:
            return z80_read_unmapped();

        case 0x40:
            return vdp_counter_r(port);

        case 0x80:
            return vdp_read(port);

        case 0xC0:
            switch(port)
            {
                case 0xC0:
                case 0xC1:
                case 0xDC:
                case 0xDD:
                    return input_r(port);
            }
            return z80_read_unmapped();
    }

    /* Just to please the compiler */
    return 0;
}

/*--------------------------------------------------------------------------*/
/* Game Gear (MS) port handlers                                             */
/*--------------------------------------------------------------------------*/

void __fastcall ggms_port_w(UINT16 port, UINT8 data)
{
    port &= 0xFF;

    switch(port & 0xC1)
    {
        case 0x00:
            memctrl_w(data);
            return;

        case 0x01:
            ioctrl_w(data);
            return;

        case 0x40:
        case 0x41:
            psg_write(data);
            return;

        case 0x80:
        case 0x81:
            gg_vdp_write(port, data);
            return;
    }
}

UINT8 __fastcall ggms_port_r(UINT16 port)
{
    port &= 0xFF;

    switch(port & 0xC0)
    {
        case 0x00:
            return z80_read_unmapped();

        case 0x40:
            return vdp_counter_r(port);

        case 0x80:
            return vdp_read(port);

        case 0xC0:
            switch(port)
            {
                case 0xC0:
                case 0xC1:
                case 0xDC:
                case 0xDD:
                    return input_r(port);
            }
            return z80_read_unmapped();
    }

    /* Just to please the compiler */
    return 0;
}
