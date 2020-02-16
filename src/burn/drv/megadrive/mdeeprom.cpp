/*
 * rarely used EEPROM code
 * (C) notaz, 2007-2009
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 *
 */

#include "burnint.h"
#include "m68000_intf.h"


struct i2c_eep {
	UINT8 eeprom_type;    // eeprom type: 0: 7bit (24C01), 2: 2 addr words (X24C02+), 3: 3 addr words
	UINT8 eeprom_bit_cl;  // bit number for cl
	UINT8 eeprom_bit_in;  // bit number for in
	UINT8 eeprom_bit_out; // bit number for out

	UINT32 last_write;
	UINT16 eeprom_addr;  // EEPROM address register
	UINT8  eeprom_cycle; // EEPROM cycle number
	UINT8  eeprom_slave; // EEPROM slave word for X24C02 and better SRAMs
	UINT8  eeprom_status;
	UINT8  eeprom_wb[2]; // EEPROM latch/write buffer
	UINT8  changed;
	UINT8 *data;		 // pointer to EEPROM block
};

static i2c_eep eeprom;

void EEPROM_init(UINT8 type, UINT8 cl, UINT8 i, UINT8 o, UINT8 *data)
{
	memset(&eeprom, 0, sizeof(eeprom));

	eeprom.eeprom_type   = type; // 0: 7bit (24C01), 2: 2 addr words (X24C02+), 3: 3 addr words
	eeprom.eeprom_bit_cl = cl;
	eeprom.eeprom_bit_in = i;
	eeprom.eeprom_bit_out= o;
	eeprom.data = data;
	eeprom.last_write = 0xffff0000;
}

void EEPROM_init(UINT8 *data)
{
	EEPROM_init(0, 1, 0, 0, data);
}

void EEPROM_scan()
{
	SCAN_VAR(eeprom.last_write);
	SCAN_VAR(eeprom.eeprom_addr);
	SCAN_VAR(eeprom.eeprom_cycle);
	SCAN_VAR(eeprom.eeprom_slave);
	SCAN_VAR(eeprom.eeprom_status);
	SCAN_VAR(eeprom.eeprom_wb);
	SCAN_VAR(eeprom.changed);
	// data block is scanned in megadrive.cpp..
}

// eeprom_status: LA.. s.la (L=pending SCL, A=pending SDA,
//                           s=started, l=old SCL, a=old SDA)
static void EEPROM_write_do(UINT32 d) // ???? ??la (l=SCL, a=SDA)
{
	UINT32 sreg = eeprom.eeprom_status, saddr = eeprom.eeprom_addr;
	UINT32 scyc = eeprom.eeprom_cycle, ssa = eeprom.eeprom_slave;

	//elprintf(EL_EEPROM, "eeprom: scl/sda: %i/%i -> %i/%i, newtime=%i", (sreg&2)>>1, sreg&1, (d&2)>>1, d&1, SekTotalCycles() - eeprom.last_write);
	saddr &= 0x1fff;

	if(sreg & d & 2) {
		// SCL was and is still high..
		if((sreg & 1) && !(d&1)) {
			// ..and SDA went low, means it's a start command, so clear internal addr reg and clock counter
			//elprintf(EL_EEPROM, "eeprom: -start-");
			//saddr = 0;
			scyc = 0;
			sreg |= 8;
		} else if(!(sreg & 1) && (d&1)) {
			// SDA went high == stop command
			//elprintf(EL_EEPROM, "eeprom: -stop-");
			sreg &= ~8;
		}
	}
	else if((sreg & 8) && !(sreg & 2) && (d&2))
	{
		// we are started and SCL went high - next cycle
		scyc++; // pre-increment
		if(eeprom.eeprom_type) {
			// X24C02+
			if((ssa&1) && scyc == 18) {
				scyc = 9;
				saddr++; // next address in read mode
				/*if(eeprom.eeprom_type==2) saddr&=0xff; else*/ saddr&=0x1fff; // mask
			}
			else if(eeprom.eeprom_type == 2 && scyc == 27) scyc = 18;
			else if(scyc == 36) scyc = 27;
		} else {
			// X24C01
			if(scyc == 18) {
				scyc = 9;  // wrap
				if(saddr&1) { saddr+=2; saddr&=0xff; } // next addr in read mode
			}
		}
		//elprintf(EL_EEPROM, "eeprom: scyc: %i", scyc);
	}
	else if((sreg & 8) && (sreg & 2) && !(d&2))
	{
		// we are started and SCL went low (falling edge)
		if(eeprom.eeprom_type) {
			// X24C02+
			if(scyc == 9 || scyc == 18 || scyc == 27); // ACK cycles
			else if( (eeprom.eeprom_type == 3 && scyc > 27) || (eeprom.eeprom_type == 2 && scyc > 18) ) {
				if(!(ssa&1)) {
					// data write
					UINT8 *pm=eeprom.data+saddr;
					*pm <<= 1; *pm |= d&1;
					if(scyc == 26 || scyc == 35) {
						saddr=(saddr&~0xf)|((saddr+1)&0xf); // only 4 (?) lowest bits are incremented
						//elprintf(EL_EEPROM, "eeprom: write done, addr inc to: %x, last byte=%02x", saddr, *pm);
					}
					eeprom.changed = 1;
				}
			} else if(scyc > 9) {
				if(!(ssa&1)) {
					// we latch another addr bit
					saddr<<=1;
					if(eeprom.eeprom_type == 2) saddr&=0xff; else saddr&=0x1fff; // mask
					saddr|=d&1;
					if(scyc==17||scyc==26) {
						//elprintf(EL_EEPROM, "eeprom: addr reg done: %x", saddr);
						if(scyc==17&&eeprom.eeprom_type==2) { saddr&=0xff; saddr|=(ssa<<7)&0x700; } // add device bits too
					}
				}
			} else {
				// slave address
				ssa<<=1; ssa|=d&1;
				//if(scyc==8) elprintf(EL_EEPROM, "eeprom: slave done: %x", ssa);
			}
		} else {
			// X24C01
			if(scyc == 9); // ACK cycle, do nothing
			else if(scyc > 9) {
				if(!(saddr&1)) {
					// data write
					UINT8 *pm=eeprom.data+(saddr>>1);
					*pm <<= 1; *pm |= d&1;
					if(scyc == 17) {
						saddr=(saddr&0xf9)|((saddr+2)&6); // only 2 lowest bits are incremented
						//elprintf(EL_EEPROM, "eeprom: write done, addr inc to: %x, last byte=%02x", saddr>>1, *pm);
					}
					eeprom.changed = 1;
				}
			} else {
				// we latch another addr bit
				saddr<<=1; saddr|=d&1; saddr&=0xff;
				//if(scyc==8) elprintf(EL_EEPROM, "eeprom: addr done: %x", saddr>>1);
			}
		}
	}

	sreg &= ~3; sreg |= d&3; // remember SCL and SDA
	eeprom.eeprom_status  = (UINT8) sreg;
	eeprom.eeprom_cycle = (UINT8) scyc;
	eeprom.eeprom_slave = (UINT8) ssa;
	eeprom.eeprom_addr  = (UINT16)saddr;
}

static void EEPROM_upd_pending(UINT32 d)
{
	UINT32 d1, sreg = eeprom.eeprom_status;

	sreg &= ~0xc0;

	// SCL
	d1 = (d >> eeprom.eeprom_bit_cl) & 1;
	sreg |= d1 << 7;

	// SDA in
	d1 = (d >> eeprom.eeprom_bit_in) & 1;
	sreg |= d1 << 6;

	eeprom.eeprom_status = (UINT8) sreg;
}

void EEPROM_write16(UINT32 d)
{
	// this diff must be at most 16 for NBA Jam to work
	if (SekTotalCycles() - eeprom.last_write < 16) {
		// just update pending state
		//elprintf(EL_EEPROM, "eeprom: skip because cycles=%i", SekTotalCycles() - eeprom.last_write);
		EEPROM_upd_pending(d);
	} else {
		int srs = eeprom.eeprom_status;
		EEPROM_write_do(srs >> 6); // execute pending
		EEPROM_upd_pending(d);
		if ((srs ^ eeprom.eeprom_status) & 0xc0) // update time only if SDA/SCL changed
			eeprom.last_write = SekTotalCycles();
	}
}

void EEPROM_write8(UINT32 a, UINT8 d)
{
	UINT8 *wb = eeprom.eeprom_wb;
	wb[a & 1] = d;
	EEPROM_write16((wb[0] << 8) | wb[1]);
}

UINT32 EEPROM_read()
{
	UINT32 shift, d;
	UINT32 sreg, saddr, scyc, ssa, interval;

	// flush last pending write
	EEPROM_write_do(eeprom.eeprom_status>>6);

	sreg = eeprom.eeprom_status; saddr = eeprom.eeprom_addr&0x1fff; scyc = eeprom.eeprom_cycle; ssa = eeprom.eeprom_slave;
	interval = SekTotalCycles() - eeprom.last_write;
	d = (sreg>>6)&1; // use SDA as "open bus"

	// NBA Jam is nasty enough to read <before> raising the SCL and starting the new cycle.
	// this is probably valid because data changes occur while SCL is low and data can be read
	// before it's actual cycle begins.
	if (!(sreg&0x80) && interval >= 24) {
		//elprintf(EL_EEPROM, "eeprom: early read, cycles=%i", interval);
		scyc++;
	}

	if (!(sreg & 8)); // not started, use open bus
	else if (scyc == 9 || scyc == 18 || scyc == 27) {
		//elprintf(EL_EEPROM, "eeprom: r ack");
		d = 0;
	} else if (scyc > 9 && scyc < 18) {
		// started and first command word received
		shift = 17-scyc;
		if (eeprom.eeprom_type) {
			// X24C02+
			if (ssa&1) {
				//elprintf(EL_EEPROM, "eeprom: read: addr %02x, cycle %i, reg %02x", saddr, scyc, sreg);
				//if (shift==0) elprintf(EL_EEPROM, "eeprom: read done, byte %02x", eeprom.data[saddr]);
				d = (eeprom.data[saddr]>>shift)&1;
			}
		} else {
			// X24C01
			if (saddr&1) {
				//elprintf(EL_EEPROM, "eeprom: read: addr %02x, cycle %i, reg %02x", saddr>>1, scyc, sreg);
				//if (shift==0) elprintf(EL_EEPROM, "eeprom: read done, byte %02x", eeprom.data[saddr>>1]);
				d = (eeprom.data[saddr>>1]>>shift)&1;
			}
		}
	}

	return (d << eeprom.eeprom_bit_out);
}

UINT32 EEPROM_read8(UINT32 a)
{
	UINT32 d = EEPROM_read();
	if (!(a & 1))
		d >>= 8;
	return d;
}
