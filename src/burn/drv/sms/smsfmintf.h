
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
void FM_GetContext(UINT8 *data);
void FM_SetContext(UINT8 *data);
INT32 FM_GetContextSize(void);
UINT8 *FM_GetContextPtr(void);
void FM_WriteReg(INT32 reg, INT32 data);

#endif /* _FMINTF_H_ */
