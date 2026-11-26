// Sega Model 1 sound board (also on the original Model 2)

#ifndef MODEL1_SND_H
#define MODEL1_SND_H

#define M1AUDIO_CLOCK	10000000

// usarts: 0 = main board, 1 = sound board, 2 = digital sound board (gets the sound board's output)
#define M1_USART_MAIN	0
#define M1_USART_SOUND	1
#define M1_USART_DSB	2

void M1UsartReset(INT32 n);
void M1UsartControlWrite(INT32 n, UINT8 data);
void M1UsartDataWrite(INT32 n, UINT8 data);
UINT8 M1UsartRead(INT32 n, INT32 offset);
void M1UsartAdvance(INT32 n, INT32 ticks);	// 500KHz ticks
INT32 M1UsartTxRdy(INT32 n);
INT32 M1UsartRxRdy(INT32 n);
void M1UsartSetMainTxHook(void (*hook)(UINT8));
void M1UsartReceive(INT32 n, UINT8 data);
void M1UsartScan(INT32 nAction);

// the sound 68000 is SekInit(nCpu); its BurnTimer is attached to it
void M1AudioInit(INT32 nCpu, UINT8 *rom, UINT8 *ram, UINT8 *pcm0, UINT8 *pcm1, INT32 has_dsb);
void M1AudioExit();
void M1AudioReset();
void M1AudioIRQUpdate();		// the sound cpu must be open
void M1AudioUpdate(INT16 *pSoundBuf, INT32 nSegmentLength);
void M1AudioScan(INT32 nAction, INT32 *pnMin);

#endif
