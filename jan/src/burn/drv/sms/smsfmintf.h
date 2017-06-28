
#ifndef _FMINTF_H_
#define _FMINTF_H_

enum {
    SND_EMU2413,        /* Mitsutaka Okazaki's YM2413 emulator */
    SND_YM2413          /* Jarek Burczynski's YM2413 emulator */
};

typedef struct {
    UINT8 latch;
    UINT8 reg[0x40];
} FM_Context;

/* Function prototypes */
void FM_Init(void);
void FM_Shutdown(void);
void FM_Reset(void);
void FM_Update(INT16 **buffer, INT32 length);
void FM_Write(INT32 offset, INT32 data);
#endif /* _FMINTF_H_ */
