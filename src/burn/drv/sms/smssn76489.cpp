/* 
    SN76489 emulation
    by Maxim in 2001 and 2002
    converted from my original Delphi implementation

    I'm a C newbie so I'm sure there are loads of stupid things
    in here which I'll come back to some day and redo

    Includes:
    - Super-high quality tone channel "oversampling" by calculating fractional positions on transitions
    - Noise output pattern reverse engineered from actual SMS output
    - Volume levels taken from actual SMS output

    07/08/04  Charles MacDonald
    Modified for use with SMS Plus:
    - Added support for multiple PSG chips.
    - Added reset/config/update routines.
    - Added context management routines.
    - Removed SN76489_GetValues().
    - Removed some unused variables.
*/

#include "smsshared.h"

#define NoiseInitialState   0x8000  /* Initial state of shift register */
#define PSG_CUTOFF          0x6     /* Value below which PSG does not output */

/* These values are taken from a real SMS2's output */
static const int PSGVolumeValues[2][16] = {
    {892,892,892,760,623,497,404,323,257,198,159,123,96,75,60,0}, /* I can't remember why 892... :P some scaling I did at some point */
	{892,774,669,575,492,417,351,292,239,192,150,113,80,50,24,0}
};

static SN76489_Context SN76489[MAX_SN76489];

void SN76489_Init(int which, int PSGClockValue, int SamplingRate)
{
    SN76489_Context *p = &SN76489[which];
    p->dClock=(float)PSGClockValue/16/SamplingRate;
    SN76489_Config(which, MUTE_ALLON, BOOST_ON, VOL_FULL, FB_SEGAVDP);
    SN76489_Reset(which);
}

void SN76489_Reset(int which)
{
    SN76489_Context *p = &SN76489[which];
    int i;

    p->PSGStereo = 0xFF;

    for(i = 0; i <= 3; i++)
    {
        /* Initialise PSG state */
        p->Registers[2*i] = 1;         /* tone freq=1 */
        p->Registers[2*i+1] = 0xf;     /* vol=off */
        p->NoiseFreq = 0x10;

        /* Set counters to 0 */
        p->ToneFreqVals[i] = 0;

        /* Set flip-flops to 1 */
        p->ToneFreqPos[i] = 1;

        /* Set intermediate positions to do-not-use value */
        p->IntermediatePos[i] = LONG_MIN;
    }

    p->LatchedRegister=0;

    /* Initialise noise generator */
    p->NoiseShiftRegister=NoiseInitialState;

    /* Zero clock */
    p->Clock=0;

}

void SN76489_Shutdown(void)
{
}

void SN76489_Config(int which, int mute, int boost, int volume, int feedback)
{
    SN76489_Context *p = &SN76489[which];

    p->Mute = mute;
    p->BoostNoise = boost;
    p->VolumeArray = volume;
    p->WhiteNoiseFeedback = feedback;
}

void SN76489_SetContext(int which, uint8 *data)
{
    memcpy(&SN76489[which], data, sizeof(SN76489_Context));
}

void SN76489_GetContext(int which, uint8 *data)
{
    memcpy(data, &SN76489[which], sizeof(SN76489_Context));
}

uint8 *SN76489_GetContextPtr(int which)
{
    return (uint8 *)&SN76489[which];
}

int SN76489_GetContextSize(void)
{
    return sizeof(SN76489_Context);
}

void SN76489_Write(int which, int data)
{
    SN76489_Context *p = &SN76489[which];

	if (data&0x80) {
        /* Latch/data byte  %1 cc t dddd */
        p->LatchedRegister=((data>>4)&0x07);
        p->Registers[p->LatchedRegister]=
            (p->Registers[p->LatchedRegister] & 0x3f0)    /* zero low 4 bits */
            | (data&0xf);                           /* and replace with data */
	} else {
        /* Data byte        %0 - dddddd */
        if (!(p->LatchedRegister%2)&&(p->LatchedRegister<5))
            /* Tone register */
            p->Registers[p->LatchedRegister]=
                (p->Registers[p->LatchedRegister] & 0x00f)    /* zero high 6 bits */
                | ((data&0x3f)<<4);                     /* and replace with data */
		else
            /* Other register */
            p->Registers[p->LatchedRegister]=data&0x0f;       /* Replace with data */
    }
    switch (p->LatchedRegister) {
	case 0:
	case 2:
    case 4: /* Tone channels */
        if (p->Registers[p->LatchedRegister]==0) p->Registers[p->LatchedRegister]=1;    /* Zero frequency changed to 1 to avoid div/0 */
		break;
    case 6: /* Noise */
        p->NoiseShiftRegister=NoiseInitialState;   /* reset shift register */
        p->NoiseFreq=0x10<<(p->Registers[6]&0x3);     /* set noise signal generator frequency */
		break;
    }
}

void SN76489_GGStereoWrite(int which, int data)
{
    SN76489_Context *p = &SN76489[which];
    p->PSGStereo=data;
}

void SN76489_Update(int which, INT16 **buffer, int length)
{
    SN76489_Context *p = &SN76489[which];
    int i, j;

    for(j = 0; j < length; j++)
    {
        for (i=0;i<=2;++i)
            if (p->IntermediatePos[i]!=LONG_MIN)
                p->Channels[i]=(p->Mute >> i & 0x1)*PSGVolumeValues[p->VolumeArray][p->Registers[2*i+1]]*p->IntermediatePos[i]/65536;
            else
                p->Channels[i]=(p->Mute >> i & 0x1)*PSGVolumeValues[p->VolumeArray][p->Registers[2*i+1]]*p->ToneFreqPos[i];
    
        p->Channels[3]=(short)((p->Mute >> 3 & 0x1)*PSGVolumeValues[p->VolumeArray][p->Registers[7]]*(p->NoiseShiftRegister & 0x1));
    
        if (p->BoostNoise) p->Channels[3]<<=1; /* Double noise volume to make some people happy */
    
        buffer[0][j] =0;
        buffer[1][j] =0;
        for (i=0;i<=3;++i) {
            buffer[0][j] +=(p->PSGStereo >> (i+4) & 0x1)*p->Channels[i]; /* left */
            buffer[1][j] +=(p->PSGStereo >>  i    & 0x1)*p->Channels[i]; /* right */
        }
    
        p->Clock+=p->dClock;
        p->NumClocksForSample=(int)p->Clock;  /* truncates */
        p->Clock-=p->NumClocksForSample;  /* remove integer part */
        /* Looks nicer in Delphi... */
        /*  Clock:=Clock+p->dClock; */
        /*  NumClocksForSample:=Trunc(Clock); */
        /*  Clock:=Frac(Clock); */
    
        /* Decrement tone channel counters */
        for (i=0;i<=2;++i)
            p->ToneFreqVals[i]-=p->NumClocksForSample;
     
        /* Noise channel: match to tone2 or decrement its counter */
        if (p->NoiseFreq==0x80) p->ToneFreqVals[3]=p->ToneFreqVals[2];
        else p->ToneFreqVals[3]-=p->NumClocksForSample;
    
        /* Tone channels: */
        for (i=0;i<=2;++i) {
            if (p->ToneFreqVals[i]<=0) {   /* If it gets below 0... */
                if (p->Registers[i*2]>PSG_CUTOFF) {
                    /* Calculate how much of the sample is + and how much is - */
                    /* Go to floating point and include the clock fraction for extreme accuracy :D */
                    /* Store as long int, maybe it's faster? I'm not very good at this */
                    p->IntermediatePos[i]=(long)((p->NumClocksForSample-p->Clock+2*p->ToneFreqVals[i])*p->ToneFreqPos[i]/(p->NumClocksForSample+p->Clock)*65536);
                    p->ToneFreqPos[i]=-p->ToneFreqPos[i]; /* Flip the flip-flop */
                } else {
                    p->ToneFreqPos[i]=1;   /* stuck value */
                    p->IntermediatePos[i]=LONG_MIN;
                }
                p->ToneFreqVals[i]+=p->Registers[i*2]*(p->NumClocksForSample/p->Registers[i*2]+1);
            } else p->IntermediatePos[i]=LONG_MIN;
        }
    
        /* Noise channel */
        if (p->ToneFreqVals[3]<=0) {   /* If it gets below 0... */
            p->ToneFreqPos[3]=-p->ToneFreqPos[3]; /* Flip the flip-flop */
            if (p->NoiseFreq!=0x80)            /* If not matching tone2, decrement counter */
                p->ToneFreqVals[3]+=p->NoiseFreq*(p->NumClocksForSample/p->NoiseFreq+1);
            if (p->ToneFreqPos[3]==1) {    /* Only once per cycle... */
                int Feedback;
                if (p->Registers[6]&0x4) { /* White noise */
                    /* Calculate parity of fed-back bits for feedback */
                    switch (p->WhiteNoiseFeedback) {
                        /* Do some optimised calculations for common (known) feedback values */
                    case 0x0006:    /* SC-3000      %00000110 */
                    case 0x0009:    /* SMS, GG, MD  %00001001 */
                        /* If two bits fed back, I can do Feedback=(nsr & fb) && (nsr & fb ^ fb) */
                        /* since that's (one or more bits set) && (not all bits set) */
    /* which one?         Feedback=((p->NoiseShiftRegister&p->WhiteNoiseFeedback) && (p->NoiseShiftRegister&p->WhiteNoiseFeedback^p->WhiteNoiseFeedback)); */
                        Feedback=((p->NoiseShiftRegister&p->WhiteNoiseFeedback) && ((p->NoiseShiftRegister&p->WhiteNoiseFeedback)^p->WhiteNoiseFeedback));
                        break;
                    case 0x8005:    /* BBC Micro */
                        /* fall through :P can't be bothered to think too much */
                    default:        /* Default handler for all other feedback values */
                        Feedback=p->NoiseShiftRegister&p->WhiteNoiseFeedback;
                        Feedback^=Feedback>>8;
                        Feedback^=Feedback>>4;
                        Feedback^=Feedback>>2;
                        Feedback^=Feedback>>1;
                        Feedback&=1;
                        break;
                    }
                } else      /* Periodic noise */
                    Feedback=p->NoiseShiftRegister&1;
    
                p->NoiseShiftRegister=(p->NoiseShiftRegister>>1) | (Feedback<<15);
    
    /* Original code: */
    /*          p->NoiseShiftRegister=(p->NoiseShiftRegister>>1) | ((p->Registers[6]&0x4?((p->NoiseShiftRegister&0x9) && (p->NoiseShiftRegister&0x9^0x9)):p->NoiseShiftRegister&1)<<15); */
            }
        }
    }
}

