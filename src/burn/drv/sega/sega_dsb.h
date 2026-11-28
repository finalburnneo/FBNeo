// Sega digital sound board (z80 + mpeg decoder), linked to the host by usart 2 of model1_snd

#ifndef SEGA_DSB_H
#define SEGA_DSB_H

#define DSB_CLOCK	4000000

// rom: 0x8000 bytes of z80 code, ram: 0x8000 bytes, mpeg: the mpeg data
void DsbInit(INT32 nCpu, UINT8 *rom, UINT8 *ram, UINT8 *mpeg);
void DsbExit();
void DsbReset();	// the z80 is reset by the driver
void DsbIRQUpdate();	// with the z80 open
void DsbRender(INT16 *out, INT32 len);	// once per frame, even without sound output
void DsbScan(INT32 nAction);

#endif
