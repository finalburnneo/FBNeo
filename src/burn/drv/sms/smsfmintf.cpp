/*
    fmintf.c --
	Interface to EMU2413 and YM2413 emulators.

	Disabled Feb. 24, 2015, saving for possible future use
*/
#include "smsshared.h"

//static OPLL *opll;
//FM_Context fm_context;

void FM_Init(void)
{
/*    switch(snd.fm_which)
    {
        case SND_EMU2413:
            OPLL_init(snd.fm_clock, snd.sample_rate);
            opll = OPLL_new();
            OPLL_reset(opll);
            OPLL_reset_patch(opll, 0);
            break;

        case SND_YM2413:
            SMSYM2413Init(1, snd.fm_clock, snd.sample_rate);
            SMSYM2413ResetChip(0);
            break;
    }*/
}


void FM_Shutdown(void)
{
/*    switch(snd.fm_which)
    {
        case SND_EMU2413:
            if(opll)
            {
                OPLL_delete(opll);
                opll = NULL;
            }
            OPLL_close();
            break;

        case SND_YM2413:
            SMSYM2413Shutdown();
            break;
    }*/
}


void FM_Reset(void)
{
/*    switch(snd.fm_which)
    {
        case SND_EMU2413:
            OPLL_reset(opll);
            OPLL_reset_patch(opll, 0);
            break;

        case SND_YM2413:
            SMSYM2413ResetChip(0);
            break;
        }*/
}


void FM_Update(int16 **/*buffer*/, int /*length*/)
{
/*    switch(snd.fm_which)
    {
        case SND_EMU2413:
            OPLL_update(opll, buffer, length);
            break;

        case SND_YM2413:
            SMSYM2413UpdateOne(0, buffer, length);
            break;
    }*/
}

void FM_WriteReg(int /*reg*/, int /*data*/)
{
/*    FM_Write(0, reg);
    FM_Write(1, data);*/
}

void FM_Write(int /*offset*/, int /*data*/)
{
/*    if(offset & 1)
        fm_context.reg[ fm_context.latch ] = data;
    else
        fm_context.latch = data;

    switch(snd.fm_which)
    {
        case SND_EMU2413:
            OPLL_write(opll, offset & 1, data);
            break;

        case SND_YM2413:
            SMSYM2413Write(0, offset & 1, data);
            break;
    }*/
}


void FM_GetContext(uint8 */*data*/)
{
//    memcpy(data, &fm_context, sizeof(FM_Context));
}

void FM_SetContext(uint8 */*data*/)
{
/*
	int i;
    uint8 *reg = fm_context.reg;

    memcpy(&fm_context, data, sizeof(FM_Context));

    // If we are loading a save state, we want to update the YM2413 context
    // but not actually write to the current YM2413 emulator.
    if(!snd.enabled || !sms.use_fm)
        return;

    FM_Write(0, 0x0E);
    FM_Write(1, reg[0x0E]);

    for(i = 0x00; i <= 0x07; i++)
    {
        FM_Write(0, i);
        FM_Write(1, reg[i]);
    }

    for(i = 0x10; i <= 0x18; i++)
    {
        FM_Write(0, i);
        FM_Write(1, reg[i]);
    }

    for(i = 0x20; i <= 0x28; i++)
    {
        FM_Write(0, i);
        FM_Write(1, reg[i]);
    }

    for(i = 0x30; i <= 0x38; i++)
    {
        FM_Write(0, i);
        FM_Write(1, reg[i]);
    }

	FM_Write(0, fm_context.latch);
*/
}

int FM_GetContextSize(void)
{
    return 0; //sizeof(FM_Context);
}

uint8 *FM_GetContextPtr(void)
{
	//return (uint8 *)&fm_context;
	return (uint8 *)NULL;
}
