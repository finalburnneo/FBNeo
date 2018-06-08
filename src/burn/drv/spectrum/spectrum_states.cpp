#include "spectrum.h"

void SpecLoadSNASnapshot()
{
	UINT16 data;
	UINT16 addr;
	
	data = (SpecSnapshotData[22] << 8) | SpecSnapshotData[21];
	ZetSetAF(0, data);
	
	data = (SpecSnapshotData[14] << 8) | SpecSnapshotData[13];
	ZetSetBC(0, data);
	
	data = (SpecSnapshotData[12] << 8) | SpecSnapshotData[11];
	ZetSetDE(0, data);
	
	data = (SpecSnapshotData[10] << 8) | SpecSnapshotData[9];
	ZetSetHL(0, data);

	data = (SpecSnapshotData[8] << 8) | SpecSnapshotData[7];
	ZetSetAF2(0, data);

	data = (SpecSnapshotData[6] << 8) | SpecSnapshotData[5];
	ZetSetBC2(0, data);

	data = (SpecSnapshotData[4] << 8) | SpecSnapshotData[3];
	ZetSetDE2(0, data);

	data = (SpecSnapshotData[2] << 8) | SpecSnapshotData[1];
	ZetSetHL2(0, data);
	
	data = (SpecSnapshotData[18] << 8) | SpecSnapshotData[17];
	ZetSetIX(0, data);

	data = (SpecSnapshotData[16] << 8) | SpecSnapshotData[15];
	ZetSetIY(0, data);

	data = SpecSnapshotData[20];
	ZetSetR(0, data);

	data = SpecSnapshotData[0];
	ZetSetI(0, data);
	
	data = (SpecSnapshotData[24] << 8) | SpecSnapshotData[23];
	ZetSetSP(0, data);

	data = SpecSnapshotData[25] & 0x03;
	if (data == 3) data = 2;
	ZetSetIM(0, data);
	
	data = SpecSnapshotData[19];
	ZetSetIFF1(0, BIT(data, 0));
	ZetSetIFF2(0, BIT(data, 2));
	
	UINT8 intr = BIT(data, 0) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK;
	if (intr == 0) bprintf(PRINT_IMPORTANT, _T("State load INTR=0\n"));
//	ZetSetIRQLine(0, intr);
	//m_maincpu->set_input_line(INPUT_LINE_HALT, CLEAR_LINE);
		
	/*if (snapsize == SNA48_SIZE)
		// 48K Snapshot
		page_basicrom();
	else
	{
		// 128K Snapshot
		m_port_7ffd_data = snapdata[SNA128_OFFSET + 2];
		logerror ("Port 7FFD:%02X\n", m_port_7ffd_data);
		if (snapdata[SNA128_OFFSET + 3])
		{
			// TODO: page TR-DOS ROM when supported
		}
		update_paging();
	}*/
	
//	if (snapsize == SNA48_SIZE) {
		for (INT32 i = 0; i < 0xc000; i++) SpecZ80Ram[i] = SpecSnapshotData[27 + i];

		addr = ZetSP(0);
		if (addr < 0x4000) {
			data = (SpecZ80Rom[addr + 1] << 8) | SpecZ80Rom[addr + 0];
		} else {
			data = (SpecZ80Ram[addr - 0x4000 + 1] << 8) | SpecZ80Ram[addr - 0x4000 + 0];
		}
		ZetSetPC(0, data);
		
#if 0
		space.write_byte(addr + 0, 0); // It's been reported that zeroing these locations fixes the loading
		space.write_byte(addr + 1, 0); // of a few images that were snapshot at a "wrong" instant
#endif

		addr += 2;
		ZetSetSP(0, addr);

		// Set border color
		data = SpecSnapshotData[26] & 0x07;
		nPortFEData = (nPortFEData & 0xf8) | data;
//	} else {
		// 128K
//	}
}

static INT32 spectrum_identify_z80(UINT32 SnapshotSize)
{
	UINT8 lo, hi, data;
	
	if (SnapshotSize < 30) return SPEC_Z80_SNAPSHOT_INVALID;
	
	lo = SpecSnapshotData[6] & 0x0ff;
	hi = SpecSnapshotData[7] & 0x0ff;
	if (hi || lo) return SPEC_Z80_SNAPSHOT_48K_OLD;

	lo = SpecSnapshotData[30] & 0x0ff;
	hi = SpecSnapshotData[31] & 0x0ff;
	data = SpecSnapshotData[34] & 0x0ff;

	if ((hi == 0) && (lo == 23)) {
		switch (data) {
			case 0:
			case 1: return SPEC_Z80_SNAPSHOT_48K;
			case 2: return SPEC_Z80_SNAPSHOT_SAMRAM;
			case 3:
			case 4: return SPEC_Z80_SNAPSHOT_128K;
			case 128: return SPEC_Z80_SNAPSHOT_TS2068;
		}
	}

	if ((hi == 0) && (lo == 54)) {
		switch (data) {
			case 0:
			case 1:
			case 3: return SPEC_Z80_SNAPSHOT_48K;
			case 2: return SPEC_Z80_SNAPSHOT_SAMRAM;
			case 4:
			case 5:
			case 6: return SPEC_Z80_SNAPSHOT_128K;
			case 128: return SPEC_Z80_SNAPSHOT_TS2068;
		}
	}

	return SPEC_Z80_SNAPSHOT_INVALID;
}

static void SpecWriteByte(UINT32 addr, UINT8 data)
{
	if (addr >= 0x4000) {
		SpecZ80Ram[addr - 0x4000] = data;
	}
}

static void update_paging()
{
	// this will be different for +2/+3
	ZetOpen(0);
	spectrum_128_update_memory();
	ZetClose();
}

static void z80_decompress_block(UINT8 *source, UINT32 dest, UINT16 size)
{
	UINT8 ch;
	INT32 i;
	
	do {
		ch = source[0];

		// either start 0f 0x0ed, 0x0ed, xx yy or single 0x0ed
		if (ch == 0x0ed) {
			if (source[1] == 0x0ed)	{
				// 0x0ed, 0x0ed, xx yy - repetition
				UINT8 count;
				UINT8 data;

				count = source[2];

				if (count == 0)	return;

				data = source[3];

				source += 4;

				if (count > size) count = size;

				size -= count;

				for (i = 0; i < count; i++)	{
					SpecWriteByte(dest, data);
					dest++;
				}
			} else {
				// single 0x0ed
				SpecWriteByte(dest, ch);
				dest++;
				source++;
				size--;
			}
		} else {
			// not 0x0ed
			SpecWriteByte(dest, ch);
			dest++;
			source++;
			size--;
		}
	}
	while (size > 0);
}

void SpecLoadZ80Snapshot()
{
	INT32 z80_type = 0;
	UINT8 lo, hi, data;
	
	struct BurnRomInfo ri;
	ri.nType = 0;
	ri.nLen = 0;
	BurnDrvGetRomInfo(&ri, 0);
	UINT32 SnapshotSize = ri.nLen;
	
	z80_type = spectrum_identify_z80(SnapshotSize);
	
	bprintf(PRINT_IMPORTANT, _T("Snapshot Type %i\n"), z80_type);
	
	switch (z80_type) {
		case SPEC_Z80_SNAPSHOT_INVALID:	return;
		
		case SPEC_Z80_SNAPSHOT_48K_OLD:
		case SPEC_Z80_SNAPSHOT_48K: {
			//if (!strcmp(machine().system().name,"ts2068")) logerror("48K .Z80 file in TS2068\n");
			break;
		}
		
		case SPEC_Z80_SNAPSHOT_128K: {
			/*if (m_port_7ffd_data == -1)
			{
				logerror("Not a 48K .Z80 file\n");
				return;
			}
			if (!strcmp(machine().system().name,"ts2068"))
			{
				logerror("Not a TS2068 .Z80 file\n");
				return;
			}*/
			break;
		}
		
		case SPEC_Z80_SNAPSHOT_TS2068: {
			//if (strcmp(machine().system().name,"ts2068"))	logerror("Not a TS2068 machine\n");
			break;
		}
		case SPEC_Z80_SNAPSHOT_SAMRAM: {
			return;
		}
	}
	
	hi = SpecSnapshotData[0] & 0x0ff;
	lo = SpecSnapshotData[1] & 0x0ff;
	ZetSetAF(0, (hi << 8) | lo);
	
	lo = SpecSnapshotData[2] & 0x0ff;
	hi = SpecSnapshotData[3] & 0x0ff;
	ZetSetBC(0, (hi << 8) | lo);
	
	lo = SpecSnapshotData[4] & 0x0ff;
	hi = SpecSnapshotData[5] & 0x0ff;
	ZetSetHL(0, (hi << 8) | lo);

	lo = SpecSnapshotData[8] & 0x0ff;
	hi = SpecSnapshotData[9] & 0x0ff;
	ZetSetSP(0, (hi << 8) | lo);

	ZetSetI(0, (SpecSnapshotData[10] & 0x0ff));

	data = (SpecSnapshotData[11] & 0x07f) | ((SpecSnapshotData[12] & 0x01) << 7);
	ZetSetR(0, data);

	nPortFEData = (nPortFEData & 0xf8) | ((SpecSnapshotData[12] & 0x0e) >> 1);

	lo = SpecSnapshotData[13] & 0x0ff;
	hi = SpecSnapshotData[14] & 0x0ff;
	ZetSetDE(0, (hi << 8) | lo);

	lo = SpecSnapshotData[15] & 0x0ff;
	hi = SpecSnapshotData[16] & 0x0ff;
	ZetSetBC2(0, (hi << 8) | lo);

	lo = SpecSnapshotData[17] & 0x0ff;
	hi = SpecSnapshotData[18] & 0x0ff;
	ZetSetDE2(0, (hi << 8) | lo);

	lo = SpecSnapshotData[19] & 0x0ff;
	hi = SpecSnapshotData[20] & 0x0ff;
	ZetSetHL2(0, (hi << 8) | lo);

	hi = SpecSnapshotData[21] & 0x0ff;
	lo = SpecSnapshotData[22] & 0x0ff;
	ZetSetAF2(0, (hi << 8) | lo);

	lo = SpecSnapshotData[23] & 0x0ff;
	hi = SpecSnapshotData[24] & 0x0ff;
	ZetSetIY(0, (hi << 8) | lo);

	lo = SpecSnapshotData[25] & 0x0ff;
	hi = SpecSnapshotData[26] & 0x0ff;
	ZetSetIX(0, (hi << 8) | lo);
	
	if (SpecSnapshotData[27] == 0)	{
		ZetSetIFF1(0, 0);
	} else {
		ZetSetIFF1(0, 1);
	}

	//ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
	//m_maincpu->set_input_line(INPUT_LINE_HALT, CLEAR_LINE);
	
	if (SpecSnapshotData[28] != 0) {
		data = 1;
	} else {
		data = 0;
	}
	ZetSetIFF2(0, data);

	ZetSetIM(0, (SpecSnapshotData[29] & 0x03));
	
	if (z80_type == SPEC_Z80_SNAPSHOT_48K_OLD) {
		lo = SpecSnapshotData[6] & 0x0ff;
		hi = SpecSnapshotData[7] & 0x0ff;
		ZetSetPC(0, (hi << 8) | lo);

		//page_basicrom();

		if ((SpecSnapshotData[12] & 0x020) == 0)	{
			bprintf(PRINT_NORMAL, _T("Not compressed\n"));
			//for (i = 0; i < 49152; i++)	space.write_byte(i + 16384, SpecSnapshotData[30 + i]);
		} else {
			z80_decompress_block(SpecSnapshotData + 30, 16384, 49152);
		}
	} else {
		UINT8 *pSource;
		INT32 header_size;

		header_size = 30 + 2 + ((SpecSnapshotData[30] & 0x0ff) | ((SpecSnapshotData[31] & 0x0ff) << 8));

		lo = SpecSnapshotData[32] & 0x0ff;
		hi = SpecSnapshotData[33] & 0x0ff;
		ZetSetPC(0, (hi << 8) | lo);

//		if ((z80_type == SPEC_Z80_SNAPSHOT_128K) || ((z80_type == SPEC_Z80_SNAPSHOT_TS2068) && !strcmp(machine().system().name,"ts2068")))
		if (z80_type == SPEC_Z80_SNAPSHOT_128K) {
			for (INT32 i = 0; i < 16; i++) {
				AY8910Write(0, 0, i);
				AY8910Write(0, 1, SpecSnapshotData[39 + i]);
			}
			AY8910Write(0, 0, SpecSnapshotData[38]);
		}

		pSource = SpecSnapshotData + header_size;

		if (z80_type == SPEC_Z80_SNAPSHOT_48K) {
			// Ensure 48K Basic ROM is used
			//page_basicrom();
		}

		do {
			UINT16 length;
			UINT8 page;
			INT32 Dest = 0;

			length = (pSource[0] & 0x0ff) | ((pSource[1] & 0x0ff) << 8);
			page = pSource[2];
			
			if (z80_type == SPEC_Z80_SNAPSHOT_48K || z80_type == SPEC_Z80_SNAPSHOT_TS2068) {
				switch (page) {
					case 4: Dest = 0x08000; break;
					case 5: Dest = 0x0c000; break;
					case 8: Dest = 0x04000; break;
					default: Dest = 0; break;
				}
			} else {
				if ((page >= 3) && (page <= 10)) {
					Dest = ((page - 3) * 0x4000) + 0x4000;
				} else {
					// Other values correspond to ROM pages
					Dest = 0x0;
				}
			}

			if (Dest != 0) {
				if (length == 0x0ffff) {
					bprintf(PRINT_NORMAL, _T("Not compressed\n"));

					//for (i = 0; i < 16384; i++) space.write_byte(i + Dest, pSource[i]);
				} else {
					z80_decompress_block(&pSource[3], Dest, 16384);
				}
			}

			// go to next block
			pSource += (3 + length);
		}
		while ((pSource - SpecSnapshotData) < SnapshotSize);

		if ((nPort7FFDData != -1) && (z80_type != SPEC_Z80_SNAPSHOT_48K)) {
			// Set up paging
			nPort7FFDData = (SpecSnapshotData[35] & 0x0ff);
			update_paging();
		}
		/*if ((z80_type == SPEC_Z80_SNAPSHOT_48K) && !strcmp(machine().system().name,"ts2068"))
		{
			m_port_f4_data = 0x03;
			m_port_ff_data = 0x00;
			ts2068_update_memory();
		}
		if (z80_type == SPEC_Z80_SNAPSHOT_TS2068 && !strcmp(machine().system().name,"ts2068"))
		{
			m_port_f4_data = snapdata[35];
			m_port_ff_data = snapdata[36];
			ts2068_update_memory();
		}*/
	}
}