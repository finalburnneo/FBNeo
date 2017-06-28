/*
    Copyright (C) 1998-2004  Charles MacDonald

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "smsshared.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "burn_ym2413.h"

bitmap_t bitmap;
cart_t cart;
input_t input;

/* Run the virtual console emulation for one frame */
void system_frame(INT32 skip_render)
{
    static INT32 iline_table[] = {0xC0, 0xE0, 0xF0};
    INT32 lpf = (sms.display == DISPLAY_NTSC) ? 262 : 313;
    INT32 iline, z80cnt = 0;;
	INT32 nSoundBufferPos = 0;

    /* Debounce pause key */
    if(input.system & INPUT_PAUSE)
    {
        if(!sms.paused)
        {
            sms.paused = 1;

			ZetNmi();
        }
    }
    else
    {
         sms.paused = 0;
    }

	ZetNewFrame();

    text_counter = 0;

    /* End of frame, parse sprites for line 0 on line 261 (VCount=$FF) */
    if(vdp.mode <= 7)
        parse_line(0);

    for(vdp.line = 0; vdp.line < lpf;)
    {
        iline = iline_table[vdp.extended];
		z80cnt = 0;

        if(!skip_render)
        {
            render_line(vdp.line);
        }

        if(vdp.line <= iline)
        {
            vdp.left -= 1;
            if(vdp.left == -1)
            {
                vdp.left = vdp.reg[0x0A];
                vdp.hint_pending = 1;

                if(vdp.reg[0x00] & 0x10)
				{
					/*if (!(ZetTotalCycles() % CYCLES_PER_LINE)) {
						ZetRun(1);
						z80cnt++;
						}*/
					ZetRun(16);
					ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
                }
            }
        }
        else
        {
            vdp.left = vdp.reg[0x0A];
        }

		ZetRun(228 - z80cnt);

        if(vdp.line == iline)
        {
            vdp.status |= 0x80;
            vdp.vint_pending = 1;

            if(vdp.reg[0x01] & 0x20)
            {
				ZetRun(16); // Fixes Zool, Monster Truck Wars, Chicago Syndacite, Terminator 2 (SMS)
                ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
            }
        }

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / lpf;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			if (sms.use_fm)	{
				BurnYM2413Render(pSoundBuf, nSegmentLength);
			} else {
				memset(pSoundBuf, 0, nSegmentLength * 2 * sizeof(INT16));
			}
			SN76496Update(0, pSoundBuf, nSegmentLength);
			
			nSoundBufferPos += nSegmentLength;
		}

        ++vdp.line;

        if(vdp.mode <= 7)
            parse_line(vdp.line);
    }

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			if (sms.use_fm)	{
				BurnYM2413Render(pSoundBuf, nSegmentLength);
			} else {
				memset(pSoundBuf, 0, nSegmentLength * 2 * sizeof(INT16));
			}
			SN76496Update(0, pSoundBuf, nSegmentLength);
		}
	}
}




void system_init(void)
{
    sms_init();
    pio_init();
    vdp_init();
    render_init();
    sound_init();

    sms.save = 0;
}

void system_shutdown(void)
{
#ifdef DEBUG
    INT32 i;

    /*error("PC:%04X\tSP:%04X\n", z80_get_reg(Z80_PC), z80_get_reg(Z80_SP));
    error("AF:%04X\tAF:%04X\n", z80_get_reg(Z80_AF), z80_get_reg(Z80_AF2));
    error("BC:%04X\tBC:%04X\n", z80_get_reg(Z80_BC), z80_get_reg(Z80_BC2));
    error("DE:%04X\tDE:%04X\n", z80_get_reg(Z80_DE), z80_get_reg(Z80_DE2));
    error("HL:%04X\tHL:%04X\n", z80_get_reg(Z80_HL), z80_get_reg(Z80_HL2));
    error("IX:%04X\tIY:%04X\n", z80_get_reg(Z80_IX), z80_get_reg(Z80_IY));

    for(i = 0; i <= 0x0A; i++)
        error("%02X ", vdp.reg[i]);
    error("\n");

    error("MODE:%02X\n", vdp.mode & 7);
    error("PN:%04X\n", vdp.pn);
    error("CT:%04X\n", vdp.ct);
    error("PG:%04X\n", vdp.pg);
    error("SA:%04X\n", vdp.sa);
    error("SG:%04X\n", vdp.sg);

    error("\n");*/
#endif

    sms_shutdown();
    pio_shutdown();
    vdp_shutdown();
    render_shutdown();
    sound_shutdown();
}

void system_reset(void)
{
    sms_reset();
    pio_reset();
    vdp_reset();
    render_reset();
    sound_reset();
//    system_manage_sram(cart.sram, SLOT_CART, SRAM_LOAD);
}

void system_poweron(void)
{
    system_reset();
}

void system_poweroff(void)
{
//    system_manage_sram(cart.sram, SLOT_CART, SRAM_SAVE);
}


