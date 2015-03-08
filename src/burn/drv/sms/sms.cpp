/*
    sms.c --
    Sega Master System console emulation.
*/
#include "smsshared.h"
#include "tiles_generic.h"
#include "z80_intf.h"

/* SMS context */
sms_t sms;

/* Pull-up resistors on data bus */
uint8 data_bus_pullup   = 0x00;
uint8 data_bus_pulldown = 0x00;

uint8 dummy_write[0xffff];

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

	sms.wram[offset & 0x1fff] = data;
}

void sms_init(void)
{
	ZetInit(0);
	ZetOpen(0);

	/* Default: open bus */
    data_bus_pullup     = 0x00;
    data_bus_pulldown   = 0x00;

//	bprintf(0, _T("Cart mapper: "));
    /* Assign mapper */
	if(cart.mapper == MAPPER_CODIES) {
//		bprintf(0, _T("codemasters\n"));
		ZetSetWriteHandler(writemem_mapper_codies);
	}
	else if (cart.mapper == MAPPER_MSX || cart.mapper == MAPPER_MSX_NEMESIS)
	{
//		bprintf(0, _T("msx\n"));
		ZetSetWriteHandler(writemem_mapper_msx);
	}
	else {
//		bprintf(0, _T("sega\n"));
		ZetSetWriteHandler(writemem_mapper_sega);
	}

    /* Force SMS (J) console type if FM sound enabled */
    if(sms.use_fm)
    {
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
			ZetSetOutHandler(smsj_port_w);
			ZetSetInHandler(smsj_port_r);
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

        case CONSOLE_GEN:
        case CONSOLE_MD:
			ZetSetOutHandler(md_port_w);
			ZetSetInHandler(md_port_r);
            break;

        case CONSOLE_GENPBC:
        case CONSOLE_MDPBC:
			ZetSetOutHandler(md_port_w);
			ZetSetInHandler(md_port_r);
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
    memset(cart.sram,   0, sizeof(cart.sram));

    sms.paused      = 0x00;
    sms.save        = 0x00;
    sms.fm_detect   = 0x00;
    sms.memctrl     = 0xAB;
    sms.ioctrl      = 0xFF;

	ZetMapMemory(cart.rom + 0x0000, 0x0000, 0x03ff, MAP_ROM);
	ZetMapMemory(cart.rom + 0x0400, 0x0400, 0x3fff, MAP_ROM);
	ZetMapMemory(cart.rom + 0x4000, 0x4000, 0x7fff, MAP_ROM);
	ZetMapMemory(cart.rom + 0x8000, 0x8000, 0xbfff, MAP_ROM);

	if(cart.mapper == MAPPER_CODIES) {
		ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xc000, 0xdfff, MAP_RAM);
		ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xe000, 0xffff, MAP_RAM);
	} else
	if(cart.mapper == MAPPER_SEGA) {
		ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xc000, 0xdfff, MAP_RAM);
		ZetMapMemory((UINT8 *)&dummy_write, 0x0000, 0xbfff, MAP_WRITE);
		ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xe000, 0xffff, MAP_READ);
	} else
	{ // MSX Mapper
		ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xc000, 0xdfff, MAP_RAM);
		ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xe000, 0xffff, MAP_RAM);
	}
	ZetReset();
	ZetClose();

    cart.fcr[0] = 0x00;
    cart.fcr[1] = 0x00;
    cart.fcr[2] = 0x01;
	cart.fcr[3] = 0x00;

	switch (cart.mapper)
	{
		case MAPPER_MSX_NEMESIS: { // WIP!! / won't boot
//			bprintf(0, _T("(nemesis)\n"));
			cart.fcr[2] = 0x00;
			UINT32 poffset = (0x0f) << 13;
			ZetOpen(0);
			ZetMapMemory(cart.rom + poffset, 0x0000, 0x1fff, MAP_READ);
			ZetClose();
		}

	}
}

/*
// Nemesis special case
      if (slot.mapper == MAPPER_MSX_NEMESIS)
      {
        // first 8k page is mapped to last 8k ROM bank
        for (i = 0x00; i < 0x08; i++)
        {
          z80_readmap[i] = &slot.rom[(0x0f << 13) | ((i & 0x07) << 10)];
        }
      }
*/

void sms_mapper8k_w(INT32 address, UINT8 data) // WIP
{
    /* Calculate ROM page index */
	UINT32 poffset = (data % (cart.pages * 2)) << 13;

    /* Save frame control register data */
    cart.fcr[address] = data;

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

    /* Save frame control register data */
    cart.fcr[address] = data;

    switch(address)
    {
        case 0: // page 2
            if(data & 8)
            {
                uint32 offset = (data & 4) ? 0x4000 : 0x0000;
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
uint8 z80_read_unmapped(void)
{
    int pc = ZetGetPC(-1);
    uint8 data;
	pc = (pc - 1) & 0xFFFF;
	data = ZetReadByte(pc);

	return ((data | data_bus_pullup) & ~data_bus_pulldown);
}

void memctrl_w(uint8 data)
{
    sms.memctrl = data;
}

/*--------------------------------------------------------------------------*/
/* Sega Master System port handlers                                         */
/*--------------------------------------------------------------------------*/

void __fastcall sms_port_w(UINT16 port, UINT8 data)
{
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
/* Sega Master System (J) port handlers                                     */
/*--------------------------------------------------------------------------*/

void __fastcall smsj_port_w(UINT16 port, UINT8 data)
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

UINT8 __fastcall smsj_port_r(UINT16 port)
{
    port &= 0xFF;

    if(port == 0xF2 && !(sms.memctrl & 4))
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

/*--------------------------------------------------------------------------*/
/* MegaDrive / Genesis port handlers                                        */
/*--------------------------------------------------------------------------*/

void __fastcall md_port_w(UINT16 port, UINT8 data)
{
    switch(port & 0xC1)
    {
        case 0x00:
            /* No memory control register */
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
            md_vdp_write(port, data);
            return;
    }
}


UINT8 __fastcall md_port_r(UINT16 port)
{
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

