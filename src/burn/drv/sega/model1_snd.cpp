// Sega Model 1 sound board (also on the original Model 2)
//  68000 @ 10MHz, YM3438, 2x MultiPCM, linked to the main board by uPD71051 / T82C51 usarts

#include "burnint.h"
#include "m68000_intf.h"
#include "burn_ym2612.h"
#include "multipcm.h"
#include "model1_snd.h"

struct usart_t {
	UINT8 mode;
	UINT8 command;
	UINT8 status;
	UINT8 rx_data;
	UINT8 tx_data;
	UINT8 expect_mode;
	UINT8 tx_busy;
	UINT8 tx_shift;
	INT32 tx_remaining;		// usart clock ticks (500KHz)
	INT32 frame_ticks;
};

#define USART_TXRDY		0x01
#define USART_RXRDY		0x02
#define USART_TXEMPTY	0x04
#define USART_PE		0x08
#define USART_OE		0x10
#define USART_FE		0x20

static usart_t usart[3];		// main board, sound board, digital sound board
static void (*main_tx_hook)(UINT8) = NULL;
static INT32 m1_has_dsb;
static UINT8 mpcm_bank[2];
static INT32 m1_cpu;

// -----------------------------------------------------------------------------
// usarts


static INT32 usart_tx_enabled(INT32 n)
{
	return (usart[n].command & 1);	// cts is tied low on both sides
}

INT32 M1UsartTxRdy(INT32 n)
{
	return usart_tx_enabled(n) && (usart[n].status & USART_TXRDY);
}

INT32 M1UsartRxRdy(INT32 n)
{
	return (usart[n].command & 4) && (usart[n].status & USART_RXRDY);
}

void M1AudioIRQUpdate()
{
	SekSetIRQLine(2, M1UsartRxRdy(1) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void usart_check_tx_start(INT32 n)
{
	usart_t *u = &usart[n];

	if (usart_tx_enabled(n) && !u->tx_busy && !(u->status & USART_TXRDY)) {
		u->tx_shift = u->tx_data;
		u->tx_busy = 1;
		u->tx_remaining = u->frame_ticks;
		u->status &= ~USART_TXEMPTY;
		u->status |= USART_TXRDY;
	}
}

void M1UsartReset(INT32 n)
{
	usart_t *u = &usart[n];

	memset(u, 0, sizeof(usart_t));
	u->status = USART_TXEMPTY | USART_TXRDY;
	u->expect_mode = 1;
	u->frame_ticks = 10;
}

static void usart_receive(INT32 n, UINT8 ch)
{
	usart_t *u = &usart[n];

	if (!(u->command & 4)) return;	// receiver disabled

	u->rx_data = ch;
	if (u->status & USART_RXRDY) u->status |= USART_OE;
	u->status |= USART_RXRDY;
}

// another sound board (model 2a's scsp) on the main usart
void M1UsartSetMainTxHook(void (*hook)(UINT8))
{
	main_tx_hook = hook;
}

void M1UsartReceive(INT32 n, UINT8 data)
{
	usart_receive(n, data);
}

void M1UsartControlWrite(INT32 n, UINT8 data)
{
	usart_t *u = &usart[n];

	if (u->expect_mode) {
		u->mode = data;
		u->expect_mode = 0;

		INT32 factor = 1;
		switch (data & 3) {
			case 2: factor = 16; break;
			case 3: factor = 64; break;
		}
		INT32 bits = 1 + (((data >> 2) & 3) + 5) + ((data >> 4) & 1);
		INT32 half_stop = 2;
		switch (data & 0xc0) {
			case 0x80: half_stop = 3; break;
			case 0xc0: half_stop = 4; break;
		}
		u->frame_ticks = (factor * (bits * 2 + half_stop)) / 2;
		return;
	}

	if (data & 0x10) u->status &= ~(USART_PE | USART_OE | USART_FE);

	if (data & 0x40) {
		u->expect_mode = 1;
		data = 0x40;
	}

	u->command = data;
	usart_check_tx_start(n);
}

void M1UsartDataWrite(INT32 n, UINT8 data)
{
	usart_t *u = &usart[n];

	u->tx_data = data;
	u->status &= ~USART_TXRDY;
	usart_check_tx_start(n);
}

UINT8 M1UsartRead(INT32 n, INT32 offset)
{
	usart_t *u = &usart[n];

	if (offset) return 0x80 | u->status;	// dsr not connected

	u->status &= ~USART_RXRDY;
	return u->rx_data;
}

void M1UsartAdvance(INT32 n, INT32 ticks)
{
	usart_t *u = &usart[n];

	if (!u->tx_busy) return;

	u->tx_remaining -= ticks;

	while (u->tx_busy && u->tx_remaining <= 0) {
		INT32 late = -u->tx_remaining;

		// the sound board output also goes to the digital sound board, whose own output is not connected
		if (n == 0) {
			if (main_tx_hook) main_tx_hook(u->tx_shift);
			else usart_receive(1, u->tx_shift);
		}
		if (n == 1) {
			usart_receive(0, u->tx_shift);
			if (m1_has_dsb) usart_receive(2, u->tx_shift);
		}
		u->tx_busy = 0;

		if (!(u->status & USART_TXRDY) && usart_tx_enabled(n)) {
			usart_check_tx_start(n);
			u->tx_remaining -= late;
		} else {
			u->status |= USART_TXEMPTY;
		}
	}
}


// -----------------------------------------------------------------------------
// sound board

static void mpcm_bank_w(INT32 chip, UINT16 data)
{
	mpcm_bank[chip] = data & 3;
	MultiPCMSetBank(chip, mpcm_bank[chip] * 0x100000, mpcm_bank[chip] * 0x100000);
}

static void __fastcall snd_write_byte(UINT32 address, UINT8 data)
{
	switch (address & 0xffffff) {
		case 0xc20001:
			M1UsartDataWrite(1, data);
			return;

		case 0xc20003:
			M1UsartControlWrite(1, data);
			return;

		case 0xc40001:
		case 0xc40003:
		case 0xc40005:
		case 0xc40007:
			MultiPCMWrite(0, (address >> 1) & 3, data);
			return;

		case 0xc50000:
		case 0xc50001:
			mpcm_bank_w(0, data);
			return;

		case 0xc60001:
		case 0xc60003:
		case 0xc60005:
		case 0xc60007:
			MultiPCMWrite(1, (address >> 1) & 3, data);
			return;

		case 0xc70000:
		case 0xc70001:
			mpcm_bank_w(1, data);
			return;

		case 0xd00001:
		case 0xd00003:
		case 0xd00005:
		case 0xd00007:
			BurnYM3438Write(0, (address >> 1) & 3, data);
			return;
	}
}

static void __fastcall snd_write_word(UINT32 address, UINT16 data)
{
	switch (address & 0xfffffe) {
		case 0xc50000:
			mpcm_bank_w(0, data);
			return;

		case 0xc70000:
			mpcm_bank_w(1, data);
			return;
	}

	snd_write_byte((address & 0xfffffe) | 1, data & 0xff);
}

static UINT8 __fastcall snd_read_byte(UINT32 address)
{
	switch (address & 0xffffff) {
		case 0xc20001:
		case 0xc20003: {
			UINT8 r = M1UsartRead(1, (address >> 1) & 1);
			M1AudioIRQUpdate();
			return r;
		}

		case 0xc40001:
		case 0xc40003:
		case 0xc40005:
		case 0xc40007:
			return MultiPCMRead(0);

		case 0xc60001:
		case 0xc60003:
		case 0xc60005:
		case 0xc60007:
			return MultiPCMRead(1);

		case 0xd00001:
		case 0xd00003:
		case 0xd00005:
		case 0xd00007:
			return BurnYM3438Read(0, (address >> 1) & 3);
	}

	return 0;
}

static UINT16 __fastcall snd_read_word(UINT32 address)
{
	return snd_read_byte((address & 0xfffffe) | 1);
}

static void DrvFMIRQHandler(INT32, INT32)
{
	// not connected
}


void M1AudioInit(INT32 nCpu, UINT8 *rom, UINT8 *ram, UINT8 *pcm0, UINT8 *pcm1, INT32 has_dsb)
{
	m1_cpu = nCpu;
	m1_has_dsb = has_dsb;

	SekInit(nCpu, 0x68000);
	SekOpen(nCpu);
	SekMapMemory(rom,				0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(rom + 0x20000,		0x080000, 0x09ffff, MAP_ROM);	// upper rom socket mirror
	SekMapMemory(ram,				0xf00000, 0xf0ffff, MAP_RAM);
	SekSetWriteWordHandler(0,		snd_write_word);
	SekSetWriteByteHandler(0,		snd_write_byte);
	SekSetReadWordHandler(0,		snd_read_word);
	SekSetReadByteHandler(0,		snd_read_byte);
	SekClose();

	BurnYM3438Init(1, 8000000, &DrvFMIRQHandler, 0);
	BurnTimerAttach(&SekConfig, M1AUDIO_CLOCK);
	BurnYM3438SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	// 10MHz / 224 per sample (the core divides by 180)
	MultiPCMInit(0, (INT32)(10000000.0 * 180.0 / 224.0), pcm0, 1);
	MultiPCMInit(1, (INT32)(10000000.0 * 180.0 / 224.0), pcm1, 1);
	MultiPCMSetVolume(0, 0.50);
	MultiPCMSetVolume(1, 0.50);
}

void M1AudioExit()
{
	BurnYM3438Exit();
	MultiPCMExit();
}

void M1AudioReset()
{
	SekOpen(m1_cpu);
	SekReset();
	BurnYM3438Reset();
	SekClose();

	MultiPCMReset();
	mpcm_bank_w(0, 0);
	mpcm_bank_w(1, 0);
}

void M1AudioUpdate(INT16 *pSoundBuf, INT32 nSegmentLength)
{
	BurnYM3438Update(pSoundBuf, nSegmentLength);
	MultiPCMUpdate(pSoundBuf, nSegmentLength);
}

void M1UsartScan(INT32 nAction)
{
	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(usart);
	}
}

void M1AudioScan(INT32 nAction, INT32 *pnMin)
{
	BurnYM3438Scan(nAction, pnMin);
	MultiPCMScan(nAction, pnMin);

	if (nAction & ACB_DRIVER_DATA) {
		SCAN_VAR(mpcm_bank);
	}

	if (nAction & ACB_WRITE) {
		MultiPCMSetBank(0, mpcm_bank[0] * 0x100000, mpcm_bank[0] * 0x100000);
		MultiPCMSetBank(1, mpcm_bank[1] * 0x100000, mpcm_bank[1] * 0x100000);
	}
}
