
#ifndef _SN76489_H_
#define _SN76489_H_

#define MAX_SN76489     4

/*
    More testing is needed to find and confirm feedback patterns for
    SN76489 variants and compatible chips.
*/
enum feedback_patterns {
    FB_BBCMICRO =   0x8005, /* Texas Instruments TMS SN76489N (original) from BBC Micro computer */
    FB_SC3000   =   0x0006, /* Texas Instruments TMS SN76489AN (rev. A) from SC-3000H computer */
    FB_SEGAVDP  =   0x0009 /* SN76489 clone in Sega's VDP chips (315-5124, 315-5246, 315-5313, Game Gear) */
};

enum volume_modes {
    VOL_TRUNC   =   0,      /* Volume levels 13-15 are identical */
    VOL_FULL    =   1      /* Volume levels 13-15 are unique */
};

enum boost_modes {
    BOOST_OFF   =   0,      /* Regular noise channel volume */
    BOOST_ON    =   1      /* Doubled noise channel volume */
};

enum mute_values {
    MUTE_ALLOFF =   0,      /* All channels muted */
    MUTE_TONE1  =   1,      /* Tone 1 mute control */
    MUTE_TONE2  =   2,      /* Tone 2 mute control */
    MUTE_TONE3  =   4,      /* Tone 3 mute control */
    MUTE_NOISE  =   8,      /* Noise mute control */
    MUTE_ALLON  =   15     /* All channels enabled */
};

typedef struct
{
    /* expose this for inspection/modification for channel muting */
    int Mute;
    int BoostNoise;
    int VolumeArray;
    
    /* Variables */
    float Clock;
    float dClock;
    int PSGStereo;
    int NumClocksForSample;
    int WhiteNoiseFeedback;
    
    /* PSG registers: */
    UINT16 Registers[8];        /* Tone, vol x4 */
    int LatchedRegister;
    UINT16 NoiseShiftRegister;
    INT16 NoiseFreq;            /* Noise channel signal generator frequency */
    
    /* Output calculation variables */
    INT16 ToneFreqVals[4];      /* Frequency register values (counters) */
    INT8 ToneFreqPos[4];        /* Frequency channel flip-flops */
    INT16 Channels[4];          /* Value of each channel, before stereo is applied */
    INT32 IntermediatePos[4];   /* intermediate values used at boundaries between + and - */

} SN76489_Context;

/* Function prototypes */
void SN76489_Init(int which, int PSGClockValue, int SamplingRate);
void SN76489_Reset(int which);
void SN76489_Shutdown(void);
void SN76489_Config(int which, int mute, int boost, int volume, int feedback);
void SN76489_SetContext(int which, uint8 *data);
void SN76489_GetContext(int which, uint8 *data);
uint8 *SN76489_GetContextPtr(int which);
int SN76489_GetContextSize(void);
void SN76489_Write(int which, int data);
void SN76489_GGStereoWrite(int which, int data);
void SN76489_Update(int which, INT16 **buffer, int length);

#endif /* _SN76489_H_ */

