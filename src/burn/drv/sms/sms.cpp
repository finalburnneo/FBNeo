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

UINT8 __fastcall ZetReadProg(UINT32 a); // lazy.

uint8 dummy_write[0x2000];

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
        case 0xC000:
			//cpu_writemap[offset >> 10][offset & 0x03FF] = data;
			sms.wram[offset & 0x1fff] = data;    // maybe..
            return;
    }

}

void sms_init(void)
{
	ZetInit(0);
	ZetOpen(0);

	/* Default: open bus */
    data_bus_pullup     = 0x00;
    data_bus_pulldown   = 0x00;

    /* Assign mapper */
	if(cart.mapper == MAPPER_CODIES)
		ZetSetWriteHandler(writemem_mapper_codies);
	else
		ZetSetWriteHandler(writemem_mapper_sega);

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
			//bprintf(0, _T("sms!\n"));
            //smscpu_writeport16 = sms_port_w;
            //smscpu_readport16 = sms_port_r;
            break;

        case CONSOLE_SMSJ:
            //smscpu_writeport16 = smsj_port_w;
            //smscpu_readport16 = smsj_port_r;
            break;
  
        case CONSOLE_SMS2:
            //smscpu_writeport16 = sms_port_w;
            //smscpu_readport16 = sms_port_r;
            data_bus_pullup = 0xFF;
            break;

        case CONSOLE_GG:
            //smscpu_writeport16 = gg_port_w;
            //smscpu_readport16 = gg_port_r;
            data_bus_pullup = 0xFF;
            break;

        case CONSOLE_GGMS:
            //smscpu_writeport16 = ggms_port_w;
            //smscpu_readport16 = ggms_port_r;
            data_bus_pullup = 0xFF;
            break;

        case CONSOLE_GEN:
        case CONSOLE_MD:
            //smscpu_writeport16 = md_port_w;
            //smscpu_readport16 = md_port_r;
            break;

        case CONSOLE_GENPBC:
        case CONSOLE_MDPBC:
            //smscpu_writeport16 = md_port_w;
            //smscpu_readport16 = md_port_r;
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
    memset(dummy_write, 0, sizeof(dummy_write));
    memset(sms.wram,    0, sizeof(sms.wram));
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
	ZetMapMemory((UINT8 *)&dummy_write, 0x0000, 0xbfff, MAP_WRITE);
	ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory((UINT8 *)&sms.wram + 0x0000, 0xe000, 0xffff, MAP_READ);
	ZetReset();
	ZetClose();

    cart.fcr[0] = 0x00;
    cart.fcr[1] = 0x00;
    cart.fcr[2] = 0x01;
    cart.fcr[3] = 0x00;
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
	data = ZetReadProg(pc);
	//bprintf(0, _T("Read unmapped: %X data %X.\n"), pc, data);

	return ((data | data_bus_pullup) & ~data_bus_pulldown);
}

void memctrl_w(uint8 data)
{
    sms.memctrl = data;
}

/*--------------------------------------------------------------------------*/
/* Sega Master System port handlers                                         */
/*--------------------------------------------------------------------------*/

void _fastcall sms_port_w(UINT16 port, UINT8 data)
{   //bprintf(0, _T("pw %X %X,"), port, data);
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

UINT8 _fastcall sms_port_r(UINT16 port)
{   //bprintf(0, _T("pr %X,"), port);
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
    return -1;
}


/*--------------------------------------------------------------------------*/
/* Sega Master System (J) port handlers                                     */
/*--------------------------------------------------------------------------*/

void smsj_port_w(UINT16 port, UINT8 data)
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

UINT8 smsj_port_r(UINT16 port)
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
    return -1;
}



/*--------------------------------------------------------------------------*/
/* Game Gear port handlers                                                  */
/*--------------------------------------------------------------------------*/

void gg_port_w(UINT16 port, UINT8 data)
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


UINT8 gg_port_r(UINT16 port)
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
    return -1;
}

/*--------------------------------------------------------------------------*/
/* Game Gear (MS) port handlers                                             */
/*--------------------------------------------------------------------------*/

void ggms_port_w(UINT16 port, UINT8 data)
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

UINT8 ggms_port_r(UINT16 port)
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
    return -1;
}

/*--------------------------------------------------------------------------*/
/* MegaDrive / Genesis port handlers                                        */
/*--------------------------------------------------------------------------*/

void md_port_w(UINT16 port, UINT8 data)
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


UINT8 md_port_r(UINT16 port)
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
    return -1;
}

