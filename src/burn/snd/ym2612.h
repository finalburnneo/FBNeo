/*
**
** software implementation of Yamaha FM sound generator (YM2612/YM3438)
**
** Original code (MAME fm.c)
**
** Copyright (C) 2001, 2002, 2003 Jarek Burczynski (bujar at mame dot net)
** Copyright (C) 1998 Tatsuyuki Satoh , MultiArcadeMachineEmulator development
**
** Version 1.4 (final beta) 
**
** Additional code & fixes by Eke-Eke for Genesis Plus GX
**
*/

extern void MDYM2612Init(void);
extern void MDYM2612Exit(void);
extern void MDYM2612Config(unsigned char dac_bits);
extern void MDYM2612Reset(void);
extern void MDYM2612Update(INT16 **buffer, int length);
extern void MDYM2612Write(unsigned int a, unsigned int v);
extern unsigned int MDYM2612Read(void);
extern int MDYM2612LoadContext();
extern int MDYM2612SaveContext();

void BurnMD2612UpdateRequest();
