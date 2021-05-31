/*
 * Sega System 32 Multi/Model 1/Model 2 custom PCM chip (315-5560) emulation.
 *
 * by Miguel Angel Horna (ElSemi) for Model 2 Emulator and MAME.
 * Information by R.Belmont and the YMF278B (OPL4) manual.
 *
 * voice registers:
 * 0: Pan
 * 1: Index of sample
 * 2: LSB of pitch (low 2 bits seem unused so)
 * 3: MSB of pitch (ooooppppppppppxx) (o=octave (4 bit signed), p=pitch (10 bits), x=unused?
 * 4: voice control: top bit = 1 for key on, 0 for key off
 * 5: bit 0: 0: interpolate volume changes, 1: direct set volume,
      bits 1-7 = volume attenuate (0=max, 7f=min)
 * 6: LFO frequency + Phase LFO depth
 * 7: Amplitude LFO size
 *
 * The first sample ROM contains a variable length table with 12
 * bytes per instrument/sample. This is very similar to the YMF278B.
 *
 * The first 3 bytes are the offset into the file (big endian).
 * The next 2 are the loop start offset into the file (big endian)
 * The next 2 are the 2's complement of the total sample size (big endian)
 * The next byte is LFO freq + depth (copied to reg 6 ?)
 * The next 3 are envelope params (Attack, Decay1 and 2, sustain level, release, Key Rate Scaling)
 * The next byte is Amplitude LFO size (copied to reg 7 ?)
 *
 * TODO
 * - The YM278B manual states that the chip supports 512 instruments. The MultiPCM probably supports them
 * too but the high bit position is unknown (probably reg 2 low bit). Any game use more than 256?
 *
 */

#include "burnint.h"
#include <math.h>
#include "multipcm.h"

//????
#define MULTIPCM_CLOCKDIV       (180.0)

static signed int LPANTABLE[0x800],RPANTABLE[0x800];

#define FIX(v)  ((UINT32) ((float) (1<<SHIFT)*(v)))

static const int val2chan[] =
{
	0, 1, 2, 3, 4, 5, 6 , -1,
	7, 8, 9, 10,11,12,13, -1,
	14,15,16,17,18,19,20, -1,
	21,22,23,24,25,26,27, -1,
};


#define SHIFT       12


#define MULTIPCM_RATE   44100.0

struct Sample_t
{
	unsigned int Start;
	unsigned int Loop;
	unsigned int End;
	unsigned char AR,DR1,DR2,DL,RR;
	unsigned char KRS;
	unsigned char LFOVIB;
	unsigned char AM;
};

enum STATE {ATTACK,DECAY1,DECAY2,RELEASE};

struct EG_t
{
	int volume; //
	STATE state;
	int step;
	//step vals
	int AR;     //Attack
	int D1R;    //Decay1
	int D2R;    //Decay2
	int RR;     //Release
	int DL;     //Decay level
};

struct LFO_t
{
	unsigned short phase;
	UINT32 phase_step;
	int *table;
	int *scale;
};


struct SLOT
{
	unsigned char Num;
	unsigned char Regs[8];
	int Playing;
	Sample_t *Sample;
	unsigned int Base;
	unsigned int offset;
	unsigned int step;
	unsigned int Pan,TL;
	unsigned int DstTL;
	int TLStep;
	signed int Prev;
	EG_t EG;
	LFO_t PLFO; //Phase lfo
	LFO_t ALFO; //AM lfo
};

struct multi_chip {
	Sample_t Samples[0x200];        //Max 512 samples
	SLOT Slots[28];
	unsigned int CurSlot;
	unsigned int Address;
	unsigned int BankR, BankL;
	float Rate;
	unsigned int ARStep[0x40], DRStep[0x40]; //Envelope step table
	unsigned int FNS_Table[0x400];      //Frequency step table
	int mono;
};

// dink was here! feb 2020
static multi_chip chip;
static UINT8 *m_ROM = NULL;
static INT16 *mixer_buffer_left = NULL;
static INT16 *mixer_buffer_right = NULL;
static INT32 samples_from; // "re"sampler
static INT32 add_to_stream;
static double volume;

/*******************************
        ENVELOPE SECTION
*******************************/

//Times are based on a 44100Hz timebase. It's adjusted to the actual sampling rate on startup

static const double BaseTimes[64]={0,0,0,0,6222.95,4978.37,4148.66,3556.01,3111.47,2489.21,2074.33,1778.00,1555.74,1244.63,1037.19,889.02,
777.87,622.31,518.59,444.54,388.93,311.16,259.32,222.27,194.47,155.60,129.66,111.16,97.23,77.82,64.85,55.60,
48.62,38.91,32.43,27.80,24.31,19.46,16.24,13.92,12.15,9.75,8.12,6.98,6.08,4.90,4.08,3.49,
3.04,2.49,2.13,1.90,1.72,1.41,1.18,1.04,0.91,0.73,0.59,0.50,0.45,0.45,0.45,0.45};
#define AR2DR   14.32833
static signed int lin2expvol[0x400];
static int TLSteps[2];

#define EG_SHIFT    16

static int EG_Update(SLOT *slot)
{
	switch(slot->EG.state)
	{
		case ATTACK:
			slot->EG.volume+=slot->EG.AR;
			if(slot->EG.volume>=(0x3ff<<EG_SHIFT))
			{
				slot->EG.state=DECAY1;
				if(slot->EG.D1R>=(0x400<<EG_SHIFT)) //Skip DECAY1, go directly to DECAY2
					slot->EG.state=DECAY2;
				slot->EG.volume=0x3ff<<EG_SHIFT;
			}
			break;
		case DECAY1:
			slot->EG.volume-=slot->EG.D1R;
			if(slot->EG.volume<=0)
				slot->EG.volume=0;
			if(slot->EG.volume>>EG_SHIFT<=(slot->EG.DL<<(10-4)))
				slot->EG.state=DECAY2;
			break;
		case DECAY2:
			slot->EG.volume-=slot->EG.D2R;
			if(slot->EG.volume<=0)
				slot->EG.volume=0;
			break;
		case RELEASE:
			slot->EG.volume-=slot->EG.RR;
			if(slot->EG.volume<=0)
			{
				slot->EG.volume=0;
				slot->Playing=0;
			}
			break;
		default:
			return 1<<SHIFT;
	}
	return lin2expvol[slot->EG.volume>>EG_SHIFT];
}

static unsigned int Get_RATE(unsigned int *Steps,unsigned int rate,unsigned int val)
{
	int r=4*val+rate;
	if(val==0)
		return Steps[0];
	if(val==0xf)
		return Steps[0x3f];
	if(r>0x3f)
		r=0x3f;
	return Steps[r];
}

static void EG_Calc(SLOT *slot)
{
	int octave=((slot->Regs[3]>>4)-1)&0xf;
	int rate;
	if(octave&8) octave=octave-16;
	if(slot->Sample->KRS!=0xf)
		rate=(octave+slot->Sample->KRS)*2+((slot->Regs[3]>>3)&1);
	else
		rate=0;

	slot->EG.AR=Get_RATE(chip.ARStep,rate,slot->Sample->AR);
	slot->EG.D1R=Get_RATE(chip.DRStep,rate,slot->Sample->DR1);
	slot->EG.D2R=Get_RATE(chip.DRStep,rate,slot->Sample->DR2);
	slot->EG.RR=Get_RATE(chip.DRStep,rate,slot->Sample->RR);
	slot->EG.DL=0xf-slot->Sample->DL;

}

/*****************************
        LFO  SECTION
*****************************/

#define LFO_SHIFT   8


#define LFIX(v) ((unsigned int) ((float) (1<<LFO_SHIFT)*(v)))

//Convert DB to multiply amplitude
#define DB(v)   LFIX(pow(10.0,v/20.0))

//Convert cents to step increment
#define CENTS(v) LFIX(pow(2.0,v/1200.0))

static int PLFO_TRI[256];
static int ALFO_TRI[256];

static const float LFOFreq[8]={0.168f,2.019f,3.196f,4.206f,5.215f,5.888f,6.224f,7.066f};    //Hz;
static const float PSCALE[8]={0.0f,3.378f,5.065f,6.750f,10.114f,20.170f,40.180f,79.307f};   //cents
static const float ASCALE[8]={0.0f,0.4f,0.8f,1.5f,3.0f,6.0f,12.0f,24.0f};                   //DB
static int PSCALES[8][256];
static int ASCALES[8][256];

static void LFO_Init(void)
{
	int i,s;
	for(i=0;i<256;++i)
	{
		int a;  //amplitude
		int p;  //phase

		//Tri
		if(i<128)
			a=255-(i*2);
		else
			a=(i*2)-256;
		if(i<64)
			p=i*2;
		else if(i<128)
			p=255-i*2;
		else if(i<192)
			p=256-i*2;
		else
			p=i*2-511;
		ALFO_TRI[i]=a;
		PLFO_TRI[i]=p;
	}

	for(s=0;s<8;++s)
	{
		float limit=PSCALE[s];
		for(i=-128;i<128;++i)
		{
			PSCALES[s][i+128]=CENTS(((limit*(float) i)/128.0));
		}
		limit=-ASCALE[s];
		for(i=0;i<256;++i)
		{
			ASCALES[s][i]=DB(((limit*(float) i)/256.0));
		}
	}
}

#define INLINE static inline

INLINE signed int PLFO_Step(LFO_t *LFO)
{
	int p;
	LFO->phase+=LFO->phase_step;
	p=LFO->table[(LFO->phase>>LFO_SHIFT)&0xff];
	p=LFO->scale[p+128];
	return p<<(SHIFT-LFO_SHIFT);
}

INLINE signed int ALFO_Step(LFO_t *LFO)
{
	int p;
	LFO->phase+=LFO->phase_step;
	p=LFO->table[(LFO->phase>>LFO_SHIFT)&0xff];
	p=LFO->scale[p];
	return p<<(SHIFT-LFO_SHIFT);
}

static void LFO_ComputeStep(LFO_t *LFO,UINT32 LFOF,UINT32 LFOS,int ALFO)
{
	float step=(float) LFOFreq[LFOF]*256.0/(float) chip.Rate;
	LFO->phase_step=(unsigned int) ((float) (1<<LFO_SHIFT)*step);
	if(ALFO)
	{
		LFO->table=ALFO_TRI;
		LFO->scale=ASCALES[LFOS];
	}
	else
	{
		LFO->table=PLFO_TRI;
		LFO->scale=PSCALES[LFOS];
	}
}

static void WriteSlot(SLOT *slot,int reg,unsigned char data)
{
	slot->Regs[reg]=data;

	switch(reg)
	{
		case 0: //PANPOT
			slot->Pan=(data>>4)&0xf;
			break;
		case 1: //Sample
			//according to YMF278 sample write causes some base params written to the regs (envelope+lfos)
			//the game should never change the sample while playing.  "sure about that?" -dink
			{
				Sample_t *Sample=chip.Samples+slot->Regs[1];

				slot->Sample=chip.Samples+slot->Regs[1];
				slot->Base=slot->Sample->Start;
				slot->offset=0;
				slot->Prev=0;
				slot->TL=slot->DstTL<<SHIFT;

				EG_Calc(slot);
				slot->EG.state=ATTACK;
				slot->EG.volume=0;

				if(slot->Base>=0x100000)
				{
					if(slot->Pan&8)
						slot->Base=(slot->Base&0xfffff)|(chip.BankL);
					else
						slot->Base=(slot->Base&0xfffff)|(chip.BankR);
				}

				WriteSlot(slot,6,Sample->LFOVIB);
				WriteSlot(slot,7,Sample->AM);
			}
			break;
		case 2: //Pitch
		case 3:
			{
				unsigned int oct=((slot->Regs[3]>>4)-1)&0xf;
				unsigned int pitch=((slot->Regs[3]&0xf)<<6)|(slot->Regs[2]>>2);
				pitch=chip.FNS_Table[pitch];
				if(oct&0x8)
					pitch>>=(16-oct);
				else
					pitch<<=oct;
				slot->step=pitch/chip.Rate;
			}
			break;
		case 4:     //KeyOn/Off (and more?)
			{
				if(data&0x80)       //KeyOn
				{
					slot->Playing=1;
				}
				else
				{
					if(slot->Playing)
					{
						if(slot->Sample->RR!=0xf)
							slot->EG.state=RELEASE;
						else
							slot->Playing=0;
					}
				}
			}
			break;
		case 5: //TL+Interpolation
			{
				slot->DstTL=(data>>1)&0x7f;
				if(!(data&1))   //Interpolate TL
				{
					if((slot->TL>>SHIFT)>slot->DstTL)
						slot->TLStep=TLSteps[0];        //decrease
					else
						slot->TLStep=TLSteps[1];        //increase
				}
				else
					slot->TL=slot->DstTL<<SHIFT;
			}
			break;
		case 6: //LFO freq+PLFO
			{
				if(data)
				{
					LFO_ComputeStep(&(slot->PLFO),(slot->Regs[6]>>3)&7,slot->Regs[6]&7,0);
					LFO_ComputeStep(&(slot->ALFO),(slot->Regs[6]>>3)&7,slot->Regs[7]&7,1);
				}
			}
			break;
		case 7: //ALFO
			{
				if(data)
				{
					LFO_ComputeStep(&(slot->PLFO),(slot->Regs[6]>>3)&7,slot->Regs[6]&7,0);
					LFO_ComputeStep(&(slot->ALFO),(slot->Regs[6]>>3)&7,slot->Regs[7]&7,1);
				}
			}
			break;
	}
}

UINT8 MultiPCMRead()
{
	return 0;
}

void MultiPCMWrite(INT32 offset, UINT8 data)
{
	switch(offset)
	{
		case 0:     //Data write
			WriteSlot(chip.Slots+chip.CurSlot,chip.Address,data);
			break;

		case 1:
			chip.CurSlot=val2chan[data&0x1f];
			break;

		case 2:
			chip.Address=(data>7)?7:data;
			break;
	}
}

/* MAME/M1 access functions */

void MultiPCMSetBank(UINT32 leftoffs, UINT32 rightoffs)
{
	chip.BankL = leftoffs;
	chip.BankR = rightoffs;
}

//	AM_RANGE(0x000000, 0x3fffff) AM_ROM

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void MultiPCMExit()
{
	if (mixer_buffer_left) {
		BurnFree(mixer_buffer_left);
		mixer_buffer_left = NULL;
		mixer_buffer_right = NULL;
		m_ROM = NULL;
	}
}

void MultiPCMScan(INT32 nAction, INT32 *pnMin)
{
	SCAN_VAR(chip);
}

void MultiPCMReset()
{
	for(INT32 i=0; i < 28; i++) {
		chip.Slots[i].Num = i;
		chip.Slots[i].Playing = 0;
	}
}

void MultiPCMSetVolume(double vol)
{
	volume = vol;
}

void MultiPCMInit(INT32 clock, UINT8 *SndROM, INT32 bAdd)
{
	int i;

	memset(&chip, 0, sizeof(chip)); // start fresh

	m_ROM = SndROM;
	chip.Rate=(float) clock / MULTIPCM_CLOCKDIV;
	samples_from = (INT32)((double)((chip.Rate * 100) / nBurnFPS) + 0.5);

	add_to_stream = bAdd;

	mixer_buffer_left = (INT16*)BurnMalloc(2 * sizeof(INT16) * chip.Rate);
	mixer_buffer_right = mixer_buffer_left + ((INT32)chip.Rate);

	volume = 1.00;

	//Volume+pan table
	for(i=0;i<0x800;++i)
	{
		float SegaDB=0;
		float TL;
		float LPAN,RPAN;

		unsigned char iTL=i&0x7f;
		unsigned char iPAN=(i>>7)&0xf;

		SegaDB=(float) iTL*(-24.0)/(float) 0x40;

		TL=pow(10.0,SegaDB/20.0);


		if(iPAN==0x8)
		{
			LPAN=RPAN=0.0;
		}
		else if(iPAN==0x0)
		{
			LPAN=RPAN=1.0;
		}
		else if(iPAN&0x8)
		{
			LPAN=1.0;

			iPAN=0x10-iPAN;

			SegaDB=(float) iPAN*(-12.0)/(float) 0x4;

			RPAN=pow(10.0,SegaDB/20.0);

			if((iPAN&0x7)==7)
				RPAN=0.0;
		}
		else
		{
			RPAN=1.0;

			SegaDB=(float) iPAN*(-12.0)/(float) 0x4;

			LPAN=pow(10.0,SegaDB/20.0);
			if((iPAN&0x7)==7)
				LPAN=0.0;
		}

		TL/=4.0;

		LPANTABLE[i]=FIX((LPAN*TL));
		RPANTABLE[i]=FIX((RPAN*TL));
	}

	//Pitch steps
	for(i=0;i<0x400;++i)
	{
		float fcent=chip.Rate*(1024.0+(float) i)/1024.0;
		chip.FNS_Table[i]=(unsigned int ) ((float) (1<<SHIFT) *fcent);
	}

	//Envelope steps
	for(i=0;i<0x40;++i)
	{
		//Times are based on 44100 clock, adjust to real chip clock
		chip.ARStep[i]=(float) (0x400<<EG_SHIFT)/(BaseTimes[i]*44100.0/(1000.0));
		chip.DRStep[i]=(float) (0x400<<EG_SHIFT)/(BaseTimes[i]*AR2DR*44100.0/(1000.0));
	}
	chip.ARStep[0]=chip.ARStep[1]=chip.ARStep[2]=chip.ARStep[3]=0;
	chip.ARStep[0x3f]=0x400<<EG_SHIFT;
	chip.DRStep[0]=chip.DRStep[1]=chip.DRStep[2]=chip.DRStep[3]=0;

	//TL Interpolation steps
	//lower
	TLSteps[0]=-(float) (0x80<<SHIFT)/(78.2*44100.0/1000.0);
	//raise
	TLSteps[1]=(float) (0x80<<SHIFT)/(78.2*2*44100.0/1000.0);

	//build the linear->exponential ramps
	for(i=0;i<0x400;++i)
	{
		float db=-(96.0-(96.0*(float) i/(float) 0x400));
		lin2expvol[i]=pow(10.0,db/20.0)*(float) (1<<SHIFT);
	}


	for(i=0;i<512;++i)
	{
		UINT8 ptSample[12];

		for (int j = 0; j < 12; j++)
		{
			ptSample[j] = (UINT8)m_ROM[((i*12) + j) & 0x3fffff];
		}

		chip.Samples[i].Start=(ptSample[0]<<16)|(ptSample[1]<<8)|(ptSample[2]<<0);
		chip.Samples[i].Loop=(ptSample[3]<<8)|(ptSample[4]<<0);
		chip.Samples[i].End=0xffff-((ptSample[5]<<8)|(ptSample[6]<<0));
		chip.Samples[i].LFOVIB=ptSample[7];
		chip.Samples[i].DR1=ptSample[8]&0xf;
		chip.Samples[i].AR=(ptSample[8]>>4)&0xf;
		chip.Samples[i].DR2=ptSample[9]&0xf;
		chip.Samples[i].DL=(ptSample[9]>>4)&0xf;
		chip.Samples[i].RR=ptSample[10]&0xf;
		chip.Samples[i].KRS=(ptSample[10]>>4)&0xf;
		chip.Samples[i].AM=ptSample[11];
	}

	LFO_Init();

	MultiPCMReset();
}

void MultiPCMSetMono(INT32 ismono)
{
	chip.mono = (ismono) ? 1 : 0;
}

//-------------------------------------------------
//  sound_stream_update - handle a stream update
//-------------------------------------------------

void MultiPCMUpdate(INT16 *buffer, INT32 samples_len)
{
	INT32 samples = (samples_from * samples_len) / nBurnSoundLen;

	INT16 *lmix = mixer_buffer_left;
	INT16 *rmix = mixer_buffer_right;

	for (INT32 i = 0; i < samples; i++)
	{
		signed int smpl=0;
		signed int smpr=0;
		for (INT32 sl = 0 ;sl < 28; sl++)
		{
			SLOT *slot=chip.Slots+sl;
			if(slot->Playing)
			{
				unsigned int vol=(slot->TL>>SHIFT)|(slot->Pan<<7);
				unsigned int adr=slot->offset>>SHIFT;
				signed int sample;
				unsigned int step=slot->step;
				signed int csample=(signed short) (m_ROM[(slot->Base+adr) & 0x3fffff]<<8);
				signed int fpart=slot->offset&((1<<SHIFT)-1);
				sample=(csample*fpart+slot->Prev*((1<<SHIFT)-fpart))>>SHIFT;

				if(slot->Regs[6]&7) //Vibrato enabled
				{
					step=step*PLFO_Step(&(slot->PLFO));
					step>>=SHIFT;
				}

				slot->offset+=step;
				if(slot->offset>=(slot->Sample->End<<SHIFT))
				{
					slot->offset=slot->Sample->Loop<<SHIFT;
				}
				if(adr^(slot->offset>>SHIFT))
				{
					slot->Prev=csample;
				}

				if((slot->TL>>SHIFT)!=slot->DstTL)
					slot->TL+=slot->TLStep;

				if(slot->Regs[7]&7) //Tremolo enabled
				{
					sample=sample*ALFO_Step(&(slot->ALFO));
					sample>>=SHIFT;
				}

				sample=(sample*EG_Update(slot))>>10;

				smpl+=(LPANTABLE[vol]*sample)>>SHIFT;
				smpr+=(RPANTABLE[vol]*sample)>>SHIFT;
			}
		}

		if (chip.mono) {
			smpl = smpr;
		}

		*lmix++ = BURN_SND_CLIP(smpr); // why are these swapped??
		*rmix++ = BURN_SND_CLIP(smpl);
	}

	// ghetto-tek resampler, #1 in the hood, g!
	lmix = mixer_buffer_left;
	rmix = mixer_buffer_right;

	for (INT32 j = 0; j < samples_len; j++)
	{
		INT32 k = (samples_from * j) / nBurnSoundLen;

		INT32 l = (add_to_stream) ? (lmix[k] * volume) + buffer[0] : (lmix[k] * volume);
		INT32 r = (add_to_stream) ? (rmix[k] * volume) + buffer[1] : (rmix[k] * volume);
		buffer[0] = BURN_SND_CLIP(l);
		buffer[1] = BURN_SND_CLIP(r);
		buffer += 2;
	}
}
