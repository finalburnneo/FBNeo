//
// FB Alpha Seibu sound hardware module
//
// Based on MAME sources by Bryan McPhail, R. Belmont
// 
// Games using this hardware:
//
//	Dead Angle	2x YM2203 + adpcm -- not implemented
//
// 	Dynamite Duke   1x YM3812 + 1x M6295
//	Toki		1x YM3812 + 1x M6295
//	Raiden          1x YM3812 + 1x M6295
//	Blood Brothers  1x YM3812 + 1x M6295
//	D-Con		1x YM3812 + 1x M6295
//	Legionnaire	1x YM3812 + 1x M6295
//
//	SD Gundam PSK	1x YM2151 + 1x M6295
//	Raiden II	1x YM2151 + 2x M6295
//	Raiden DX	1x YM2151 + 2x M6295
//	Zero Team	1x YM2151 + 2x M6295
//
// 	Cross Shooter	1x YM2151 + ?
//	Cabal		1x YM2151 + adpcm
//

#include "burnint.h"
#include "z80_intf.h"
#include "burn_ym3812.h"
#include "burn_ym2151.h"
#include "burn_ym2203.h"
#include "msm6295.h"
#include "bitswap.h"

static UINT8 main2sub[2];
static UINT8 sub2main[2];
static INT32 main2sub_pending;
static INT32 sub2main_pending;
static INT32 SeibuSoundBank;
static INT32 irq1,irq2;

UINT8 *SeibuZ80DecROM = NULL;
UINT8 *SeibuZ80ROM = NULL;
UINT8 *SeibuZ80RAM = NULL;
INT32 seibu_coin_input;

static INT32 seibu_sndcpu_frequency;
static INT32 seibu_snd_type;
static INT32 is_sdgndmps = 0;

// ADPCM related
static UINT16 adpcmcurrent[2];
static UINT8 adpcmnibble[2];
static UINT16 adpcmend[2];
static UINT8 adpcmplaying[2];
static INT32 adpcmfrequency[2];
static INT32 adpcmsignal[2];
static INT32 adpcmstep[2];

static INT32 diff_lookup[49*16];
static const INT32 index_shift[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static void adpcm_init(); // forward
static void adpcm_reset(); // forward

UINT8 *SeibuADPCMData[2];
INT32 SeibuADPCMDataLen[2];

enum
{
	VECTOR_INIT,
	RST10_ASSERT,
	RST10_CLEAR,
	RST18_ASSERT,
	RST18_CLEAR
};

static void update_irq_lines(INT32 param)
{
	switch(param)
	{
		case VECTOR_INIT:
			irq1 = irq2 = 0xff;
			break;

		case RST10_ASSERT:
			irq1 = 0xd7;
			break;

		case RST10_CLEAR:
			irq1 = 0xff;
			break;

		case RST18_ASSERT:
			irq2 = 0xdf;
			break;

		case RST18_CLEAR:
			irq2 = 0xff;
			break;
	}

	if ((irq1 & irq2) == 0xff) {
		ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
	} else	{
		ZetSetVector(irq1 & irq2);
		ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
	}
}

UINT8 seibu_main_word_read(INT32 offset)
{
#if defined FBA_DEBUG
	if (!DebugDev_SeibuSndInitted) bprintf(PRINT_ERROR, _T("seibu_main_word_read called without init\n"));
#endif

	offset = (offset >> 1) & 7;

	switch (offset)
	{
		case 2:
		case 3:
			return sub2main[offset-2];
		case 5:
			return main2sub_pending ? 1 : 0;
		default:
			return 0xff;
	}
}

void seibu_main_word_write(INT32 offset, UINT8 data)
{
#if defined FBA_DEBUG
	if (!DebugDev_SeibuSndInitted) bprintf(PRINT_ERROR, _T("seibu_main_word_write called without init\n"));
#endif

	offset = (offset >> 1) & 7;

	switch (offset)
	{
		case 0:
		case 1:
			main2sub[offset] = data;
			break;

		case 4:
			//if (is_sdgndmps) update_irq_lines(RST10_ASSERT);
			update_irq_lines(RST18_ASSERT);
			break;

		case 6:
			sub2main_pending = 0;
			main2sub_pending = 1;
			break;

		default:
			break;
	}
}

void seibu_sound_mustb_write_word(INT32 /*offset*/, UINT16 data)
{
#if defined FBA_DEBUG
	if (!DebugDev_SeibuSndInitted) bprintf(PRINT_ERROR, _T("seibu_sound_mustb_write_word called without init\n"));
#endif

	main2sub[0] = data & 0xff;
	main2sub[1] = data >> 8;
	
	update_irq_lines(RST18_ASSERT);
}

static void seibu_z80_bank(INT32 data)
{
	SeibuSoundBank = data & 1;

	ZetMapArea(0x8000, 0xffff, 0, SeibuZ80ROM + 0x10000 + (data & 1) * 0x8000);
	ZetMapArea(0x8000, 0xffff, 2, SeibuZ80ROM + 0x10000 + (data & 1) * 0x8000);
}

void __fastcall seibu_sound_write(UINT16 address, UINT8 data)
{
#if defined FBA_DEBUG
	if (!DebugDev_SeibuSndInitted) bprintf(PRINT_ERROR, _T("seibu_sound_write called without init\n"));
#endif

	switch (address)
	{
		case 0x4000:
			main2sub_pending = 0;
			sub2main_pending = 1;
		return;

		case 0x4001:
			if (!is_sdgndmps)
				update_irq_lines(VECTOR_INIT);
		return;

		case 0x4002:
			// rst10 ack (unused)
		return;

		case 0x4003:
			update_irq_lines(RST18_CLEAR);
		return;

		case 0x4005:
			adpcmcurrent[0] = data << 8;
			adpcmnibble[0] = 4;
		return;

		case 0x4006:
			adpcmend[0] = data << 8;
		return;

		case 0x4007:
			seibu_z80_bank(data);
		return;

		case 0x401a: // raiden2
			if ((seibu_snd_type & 8) == 0) {
				seibu_z80_bank(data);
			} else { // Cabal ADPCM
				if (data < 2) adpcmplaying[0] = data;
			}
		return;

		case 0x4008:
			switch (seibu_snd_type & 3)
			{
				case 0:
					BurnYM3812Write(0, 0, data);
				return;

				case 1:
					BurnYM2151SelectRegister(data);
				return;

				case 2:
					BurnYM2203Write(0, 0, data);
				return;
			}
		return;

		case 0x4009:
			switch (seibu_snd_type & 3)
			{
				case 0:
					BurnYM3812Write(0, 1, data);
				return;

				case 1:
					BurnYM2151WriteRegister(data);
				return;

				case 2:
					BurnYM2203Write(0, 1, data);
				return;
			}
		return;

		case 0x4018:
		case 0x4019:
			sub2main[address & 1] = data;
		return;

		case 0x401b:
			// coin counters
		return;

		case 0x6000:
			if ((seibu_snd_type & 8) == 0) MSM6295Command(0, data);
		return;

		case 0x6002:
			if (seibu_snd_type & 4) MSM6295Command(1, data);
		return;

		case 0x6005:
			adpcmcurrent[1] = data << 8;
			adpcmnibble[1] = 4;
		return;

		case 0x6006:
			adpcmend[1] = data << 8;
		return;

		// type 2
		case 0x6008:
		case 0x6009:
			if (seibu_snd_type == 2) BurnYM2203Write(1, address & 1, data);
		return;

		case 0x601a:
			if (data < 2) adpcmplaying[1] = data;
		return;

	}
}

UINT8 __fastcall seibu_sound_read(UINT16 address)
{
#if defined FBA_DEBUG
	if (!DebugDev_SeibuSndInitted) bprintf(PRINT_ERROR, _T("seibu_sound_read called without init\n"));
#endif

	switch (address)
	{
		case 0x4008:
			switch (seibu_snd_type & 3)
			{
				case 0:
					return BurnYM3812Read(0, 0);

				case 1:
					return BurnYM2151ReadStatus();

				case 2:
					return BurnYM2203Read(0, 0);
			}
			return 0;

		case 0x4009: {
			if ((seibu_snd_type&3)==1) return BurnYM2151ReadStatus();
			if ((seibu_snd_type&3) < 2) return 0;
			return BurnYM2203Read(0, 1);
		}

		case 0x4010:
		case 0x4011:
			return main2sub[address & 1];

		case 0x4012:
			return sub2main_pending ? 1 : 0;

		case 0x4013:
			return seibu_coin_input;

		case 0x6000:
			return (((seibu_snd_type & 8) == 0) ? MSM6295ReadStatus(0) : 0);

		case 0x6002:
			if (seibu_snd_type & 4) return MSM6295ReadStatus(1);
	}

	return 0;
}

static UINT8 decrypt_data(INT32 a,INT32 src)
{
	if ( BIT(a,9)  &  BIT(a,8))             src ^= 0x80;
	if ( BIT(a,11) &  BIT(a,4) &  BIT(a,1)) src ^= 0x40;
	if ( BIT(a,11) & ~BIT(a,8) &  BIT(a,1)) src ^= 0x04;
	if ( BIT(a,13) & ~BIT(a,6) &  BIT(a,4)) src ^= 0x02;
	if (~BIT(a,11) &  BIT(a,9) &  BIT(a,2)) src ^= 0x01;

	if (BIT(a,13) &  BIT(a,4)) src = BITSWAP08(src,7,6,5,4,3,2,0,1);
	if (BIT(a, 8) &  BIT(a,4)) src = BITSWAP08(src,7,6,5,4,2,3,1,0);

	return src;
}

static UINT8 decrypt_opcode(INT32 a,INT32 src)
{
	if ( BIT(a,9)  &  BIT(a,8))             src ^= 0x80;
	if ( BIT(a,11) &  BIT(a,4) &  BIT(a,1)) src ^= 0x40;
	if (~BIT(a,13) & BIT(a,12))             src ^= 0x20;
	if (~BIT(a,6)  &  BIT(a,1))             src ^= 0x10;
	if (~BIT(a,12) &  BIT(a,2))             src ^= 0x08;
	if ( BIT(a,11) & ~BIT(a,8) &  BIT(a,1)) src ^= 0x04;
	if ( BIT(a,13) & ~BIT(a,6) &  BIT(a,4)) src ^= 0x02;
	if (~BIT(a,11) &  BIT(a,9) &  BIT(a,2)) src ^= 0x01;

	if (BIT(a,13) &  BIT(a,4)) src = BITSWAP08(src,7,6,5,4,3,2,0,1);
	if (BIT(a, 8) &  BIT(a,4)) src = BITSWAP08(src,7,6,5,4,2,3,1,0);
	if (BIT(a,12) &  BIT(a,9)) src = BITSWAP08(src,7,6,4,5,3,2,1,0);
	if (BIT(a,11) & ~BIT(a,6)) src = BITSWAP08(src,6,7,5,4,3,2,1,0);

	return src;
}

static void seibu_sound_decrypt(INT32 length)
{
	for (INT32 i = 0; i < length; i++)
	{
		UINT8 src = SeibuZ80ROM[i];

		SeibuZ80ROM[i] = decrypt_data(i,src);
		SeibuZ80DecROM[i] = decrypt_opcode(i,src);
	}
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / seibu_sndcpu_frequency;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	if (nStatus) {
		update_irq_lines(RST10_ASSERT);
	} else {
		update_irq_lines(RST10_CLEAR);
	}
}

static void Drv2151FMIRQHandler(INT32 nStatus)
{
	DrvFMIRQHandler(0, nStatus);
}

static double Drv2203GetTime()
{
	return (double)ZetTotalCycles() / seibu_sndcpu_frequency;
}

void seibu_sound_reset()
{
#if defined FBA_DEBUG
	if (!DebugDev_SeibuSndInitted) bprintf(PRINT_ERROR, _T("seibu_sound_reset called without init\n"));
#endif

	ZetOpen(0);
	ZetReset();
	update_irq_lines(VECTOR_INIT);
	seibu_z80_bank(0); // default banking. (fix for raiden2/dx coin-up)
	ZetClose();

	switch (seibu_snd_type & 3)
	{
		case 0:
			BurnYM3812Reset();
		break;

		case 1:
			BurnYM2151Reset();
		break;

		case 2:
			BurnYM2203Reset();
		break;
	}

	if ((seibu_snd_type & 8) == 0) MSM6295Reset(0);
	if (seibu_snd_type & 4) MSM6295Reset(1);

	memset (main2sub, 0, 2);
	memset (sub2main, 0, 2);
	main2sub_pending = 0;
	sub2main_pending = 0;

	seibu_coin_input = 0;
	//SeibuSoundBank = 0; // set w/ seibu_z80_bank();

	adpcm_reset();
}

void seibu_sound_init(INT32 type, INT32 len, INT32 freq0 /*cpu*/, INT32 freq1 /*ym*/, INT32 freq2 /*oki*/)
{
	DebugDev_SeibuSndInitted = 1;

	seibu_snd_type = type;

	if (len && SeibuZ80DecROM != NULL) {
		seibu_sound_decrypt(len);
	} else {
		SeibuZ80DecROM = SeibuZ80ROM;
	}

	seibu_sndcpu_frequency	= freq0;

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x1fff, 0, SeibuZ80ROM);
	ZetMapArea(0x0000, 0x1fff, 2, SeibuZ80DecROM, SeibuZ80ROM);
	ZetMapArea(0x2000, 0x27ff, 0, SeibuZ80RAM);
	ZetMapArea(0x2000, 0x27ff, 1, SeibuZ80RAM);
	ZetMapArea(0x2000, 0x27ff, 2, SeibuZ80RAM);
	ZetSetWriteHandler(seibu_sound_write);
	ZetSetReadHandler(seibu_sound_read);
	ZetClose();

	switch (seibu_snd_type & 3)
	{
		case 0:
			BurnYM3812Init(1, freq1, &DrvFMIRQHandler, &DrvSynchroniseStream, 0);
			BurnTimerAttachZetYM3812(freq0);
		break;

		case 1:
			BurnYM2151Init(freq1);
			BurnYM2151SetIrqHandler(&Drv2151FMIRQHandler);
			BurnYM2151SetAllRoutes(0.50, BURN_SND_ROUTE_BOTH);
		break;

		case 2:
			BurnYM2203Init(2, freq1, DrvFMIRQHandler, DrvSynchroniseStream, Drv2203GetTime, 0);
			BurnTimerAttachZet(freq0);
		break;
	}

	if ((seibu_snd_type & 8) == 0) {
		MSM6295Init(0, freq2, 1);
		MSM6295SetRoute(0, 0.40, BURN_SND_ROUTE_BOTH);

		if (seibu_snd_type & 4) {
			MSM6295Init(1, freq2, 1);
			MSM6295SetRoute(1, 0.40, BURN_SND_ROUTE_BOTH);
		}
	} else {
		adpcmfrequency[0] = freq2;
		adpcmfrequency[1] = freq2;
		adpcm_init();
	}

	// init kludge for sdgndmps
	if (!strcmp(BurnDrvGetTextA(DRV_NAME), "sdgndmps")) {
		is_sdgndmps = 1;
	}
}

void seibu_sound_exit()
{
#if defined FBA_DEBUG
	if (!DebugDev_SeibuSndInitted) bprintf(PRINT_ERROR, _T("seibu_sound_exit called without init\n"));
#endif

	if (!DebugDev_SeibuSndInitted) return;

	switch (seibu_snd_type & 3)
	{
		case 0:
			BurnYM3812Exit();
		break;

		case 1:
			BurnYM2151Exit();
		break;

		case 2:
			BurnYM2203Exit();
		break;
	}

	ZetExit();

	if ((seibu_snd_type & 8) == 0) MSM6295Exit(0);
	if (seibu_snd_type & 4) MSM6295Exit(1);

	MSM6295ROM = NULL;

	SeibuZ80DecROM = NULL;
	SeibuZ80ROM = NULL;
	SeibuZ80RAM = NULL;
	seibu_sndcpu_frequency = 0;
	is_sdgndmps = 0;
	
	DebugDev_SeibuSndInitted = 0;
}

static void adpcm_init()
{
	/* nibble to bit map */
	static const INT32 nbl2bit[16][4] =
	{
		{ 1, 0, 0, 0}, { 1, 0, 0, 1}, { 1, 0, 1, 0}, { 1, 0, 1, 1},
		{ 1, 1, 0, 0}, { 1, 1, 0, 1}, { 1, 1, 1, 0}, { 1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1},
		{-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1}
	};

	INT32 step, nib;

	/* loop over all possible steps */
	for (step = 0; step <= 48; step++)
	{
		/* compute the step value */
		INT32 stepval = (INT32)(floor (16.0 * pow (11.0 / 10.0, (double)step)));

		/* loop over all nibbles and compute the difference */
		for (nib = 0; nib < 16; nib++)
		{
			diff_lookup[step*16 + nib] = nbl2bit[nib][0] *
				(stepval   * nbl2bit[nib][1] +
				 stepval/2 * nbl2bit[nib][2] +
				 stepval/4 * nbl2bit[nib][3] +
				 stepval/8);
		}
	}

	adpcm_reset();
}

static void adpcm_reset()
{
	for (INT32 i = 0; i < 2; i++) {
		adpcmcurrent[i] = 0;
		adpcmnibble[i] = 0;
		adpcmend[i] = 0;
		adpcmplaying[i] = 0;
		adpcmsignal[i] = -2;
		adpcmstep[i] = 0;
	}
}

static INT32 adpcm_clock(INT32 chip, UINT8 nibble)
{
	adpcmsignal[chip] += diff_lookup[adpcmstep[chip] * 16 + (nibble & 15)];

	if (adpcmsignal[chip] > 2047)
		adpcmsignal[chip] = 2047;
	else if (adpcmsignal[chip] < -2048)
		adpcmsignal[chip] = -2048;

	adpcmstep[chip] += index_shift[nibble & 7];
	if (adpcmstep[chip] > 48)
		adpcmstep[chip] = 48;
	else if (adpcmstep[chip] < 0)
		adpcmstep[chip] = 0;

	return adpcmsignal[chip];
}

static void adpcm_update(INT32 chip, INT16 *pbuf, INT32 samples)
{
	UINT8 *adpcmrom = SeibuADPCMData[chip];
	INT32 val, samp, i;

	while (adpcmplaying[chip] && samples > 0)
	{
		val = (adpcmrom[adpcmcurrent[chip]] >> adpcmnibble[chip]) & 0xf;

		adpcmnibble[chip] ^= 4;
		if (adpcmnibble[chip] == 4)
		{
			adpcmcurrent[chip]++;
			if (adpcmcurrent[chip] >= adpcmend[chip])
				adpcmplaying[chip] = 0;
		}

		samp = ((adpcm_clock(chip, val) << 4));

		for (i = 0; (i < 6) && (samples > 0); i++) { // slow it down
			*pbuf = BURN_SND_CLIP(*pbuf + samp);
			pbuf++;
			*pbuf = BURN_SND_CLIP(*pbuf + samp);
			pbuf++;
			samples--;
		}
	}
}

void seibu_sound_update(INT16 *pbuf, INT32 nLen)
{
#if defined FBA_DEBUG
	if (!DebugDev_SeibuSndInitted) bprintf(PRINT_ERROR, _T("seibu_sound_update called without init\n"));
#endif

	switch (seibu_snd_type & 3)
	{
		case 0:
			BurnYM3812Update(pbuf, nLen);
		break;

		case 1:
			BurnYM2151Render(pbuf, nLen);
		break;

		case 2:
			BurnYM2203Update(pbuf, nLen);
		break;
	}

	if ((seibu_snd_type & 8) == 0) {
		MSM6295Render(0, pbuf, nLen);

		if (seibu_snd_type & 4) 
			MSM6295Render(1, pbuf, nLen);
	}
}

void seibu_sound_update_cabal(INT16 *pbuf, INT32 nLen)
{
#if defined FBA_DEBUG
	if (!DebugDev_SeibuSndInitted) bprintf(PRINT_ERROR, _T("seibu_sound_update called without init\n"));
#endif

	adpcm_update(0, pbuf, nLen);
	adpcm_update(1, pbuf, nLen);
}

void seibu_sound_scan(INT32 *pnMin, INT32 nAction)
{
#if defined FBA_DEBUG
	if (!DebugDev_SeibuSndInitted) bprintf(PRINT_ERROR, _T("seibu_sound_scan called without init\n"));
#endif

	if (nAction & ACB_VOLATILE)
	{		
		ZetScan(nAction);

		ZetOpen(0);
		switch (seibu_snd_type & 3)
		{
			case 0:
				BurnYM3812Scan(nAction, pnMin);
			break;
	
			case 1:
				BurnYM2151Scan(nAction);
			break;
	
			case 2:
				BurnYM2203Scan(nAction, pnMin);
			break;
		}
		ZetClose();
		
		if ((seibu_snd_type & 8) == 0) {
			MSM6295Scan(0, nAction);
			if (seibu_snd_type & 4) {
				MSM6295Scan(1, nAction);
			}
		}

		SCAN_VAR(main2sub[0]);
		SCAN_VAR(main2sub[1]);
		SCAN_VAR(sub2main[0]);
		SCAN_VAR(sub2main[1]);
		SCAN_VAR(main2sub_pending);
		SCAN_VAR(sub2main_pending);
		SCAN_VAR(SeibuSoundBank);
		SCAN_VAR(irq1);
		SCAN_VAR(irq2);

		SCAN_VAR(adpcmcurrent);
		SCAN_VAR(adpcmnibble);
		SCAN_VAR(adpcmend);
		SCAN_VAR(adpcmplaying);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		seibu_z80_bank(SeibuSoundBank);
		ZetClose();
	}
}
