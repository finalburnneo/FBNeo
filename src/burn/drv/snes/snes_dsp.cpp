#include "snes.h"

FILE* sndfile;
UINT8 dspregs[256];
int curdspreg;
UINT8 *spcram;
int ratetable[] = {
		   1 << 30,   2048,   1536,   1280,   1024,    768,    640,    512,
		   384,    320,    256,    192,    160,    128,     96,     80,
			64,     48,     40,     32,     24,     20,     16,     12,
			10,      8,      6,      5,      4,      3,      2,      1
};

#define ATTACK  1
#define DECAY   2
#define SUSTAIN 3
#define RELEASE 4

struct DSP
{
	int pitchcounter[8], pitch[8];
	int volumer[8], volumel[8];
	int sourcenum[8];
	int pos[8];
	UINT16 voiceaddr[8];
	UINT8 dir;
	UINT8 brrctrl[8];
	int brrstat[8];
	int voiceon[8];
	int voiceend[8];

	int evol[8], edelay[8], etype[8];
	UINT8 gain[8], adsr1[8], adsr2[8];
	int adsrstat[8];

	UINT16 noise;
	int noisedelay, noiserate;
	UINT8 non;

	UINT8 envx[8], outx[8];
	UINT8 endx;
};

DSP dsp;
int spcoutput;



void writedsp(UINT16 a, UINT8 v)
{
	int c;
	unsigned long templ;
	if (a & 1)
	{
		dspregs[curdspreg] = v;
		switch (curdspreg)
		{
		case 0x00: case 0x10: case 0x20: case 0x30:
		case 0x40: case 0x50: case 0x60: case 0x70:
			dsp.volumel[curdspreg >> 4] = (int)(signed char)v;
			break;
		case 0x01: case 0x11: case 0x21: case 0x31:
		case 0x41: case 0x51: case 0x61: case 0x71:
			dsp.volumer[curdspreg >> 4] = (int)(signed char)v;
			break;
		case 0x02: case 0x12: case 0x22: case 0x32:
		case 0x42: case 0x52: case 0x62: case 0x72:
			dsp.pitch[curdspreg >> 4] = (dsp.pitch[curdspreg >> 4] & 0x3F00) | v;
			break;
		case 0x03: case 0x13: case 0x23: case 0x33:
		case 0x43: case 0x53: case 0x63: case 0x73:
			dsp.pitch[curdspreg >> 4] = (dsp.pitch[curdspreg >> 4] & 0xFF) | ((v & 0x3F) << 8);
			break;
		case 0x04: case 0x14: case 0x24: case 0x34:
		case 0x44: case 0x54: case 0x64: case 0x74:
			if ((v << 2) != dsp.sourcenum[curdspreg >> 4])
			{
				dsp.sourcenum[curdspreg >> 4] = v << 2;
				dsp.pos[curdspreg >> 4] = 0;
				templ = (dsp.dir << 8) + (v << 2);
				dsp.voiceaddr[curdspreg >> 4] = spcram[templ] | (spcram[templ + 1] << 8);
				//                                printf("Sourcenum select channel %i %02X %03X %04X %04X\n",curdspreg>>4,v,v<<2,(dsp.dir<<8)+(v<<2),dsp.voiceaddr[curdspreg>>4]);
				dsp.brrstat[curdspreg >> 4] = 0;
			}
			break;
		case 0x05: case 0x15: case 0x25: case 0x35:
		case 0x45: case 0x55: case 0x65: case 0x75:
			dsp.adsr1[curdspreg >> 4] = v;
			break;
		case 0x06: case 0x16: case 0x26: case 0x36:
		case 0x46: case 0x56: case 0x66: case 0x76:
			dsp.adsr2[curdspreg >> 4] = v;
			dsp.etype[curdspreg >> 4] = (dsp.gain[curdspreg >> 4] & 0x80) ? 1 : 0;
			dsp.etype[curdspreg >> 4] |= (dsp.adsr1[curdspreg >> 4] & 0x80) ? 2 : 0;
			break;
		case 0x07: case 0x17: case 0x27: case 0x37:
		case 0x47: case 0x57: case 0x67: case 0x77:
			dsp.gain[curdspreg >> 4] = v;
			dsp.etype[curdspreg >> 4] = (dsp.gain[curdspreg >> 4] & 0x80) ? 1 : 0;
			dsp.etype[curdspreg >> 4] |= (dsp.adsr1[curdspreg >> 4] & 0x80) ? 2 : 0;
			break;
		case 0x4C:
			//                        if (v) printf("Voice on %02X\n",v);
			for (c = 0; c < 8; c++)
			{
				if (v & (1 << c))
				{
					dsp.voiceon[c] = 1;
					dsp.voiceaddr[c] = spcram[(dsp.dir << 8) + dsp.sourcenum[c]] | (spcram[(dsp.dir << 8) + dsp.sourcenum[c] + 1] << 8);
					dsp.brrstat[c] = 0;
					dsp.pos[c] = 0;
					dsp.adsrstat[c] = ATTACK;
					dsp.evol[c] = 0;
					dsp.edelay[c] = 8;
					dsp.voiceend[c] = 0;
					dsp.endx &= ~(1 << c);
				}
			}
			break;
		case 0x5C:
			//                        if (v) printf("Voice off %02X\n",v);
			for (c = 0; c < 8; c++)
			{
				if (v & (1 << c))
				{
					dsp.adsrstat[c] = RELEASE;
					dsp.edelay[c] = 1;
					//                                        dsp.voiceon[c]=0;
				}
			}
			break;
		case 0x5D:
			dsp.dir = v;
			break;
		case 0x3D:
			dsp.non = v;
			break;
		case 0x6C:
			dsp.noiserate = v & 0x1F;
			dsp.noisedelay = 1;
			break;
		}
	}
	else
		curdspreg = v & 127;
}

void resetdsp()
{
	memset(&dsp, 0, sizeof(dsp));
	dsp.noise = 0x4000;
}

UINT8 readdsp(UINT16 a)
{
	if (a & 1)
	{
		switch (curdspreg & 0x7F)
		{
		case 0x08: case 0x18: case 0x28: case 0x38:
		case 0x48: case 0x58: case 0x68: case 0x78:
			return dsp.envx[curdspreg >> 4];
		case 0x09: case 0x19: case 0x29: case 0x39:
		case 0x49: case 0x59: case 0x69: case 0x79:
			return dsp.outx[curdspreg >> 4];
		case 0x7C:
			return dsp.endx;
		}
		return dspregs[curdspreg & 127];
	}
	return curdspreg;
}

INT16 lastsamp[8][2];
int range[8], filter[8];
inline INT16 decodebrr(int v, int c)
{
	INT16 temp = v & 0xF;
	if (temp & 8) temp |= 0xFFF0;
	if (range[c] <= 12) temp <<= range[c];
	else              temp = (temp & 8) ? 0xF800 : 0;
	switch (filter[c])
	{
	case 0: break;
	case 1: temp = temp + lastsamp[c][0] + ((-lastsamp[c][0]) >> 4); break;
	case 2: temp = temp + (lastsamp[c][0] << 1) + ((-((lastsamp[c][0] << 1) + lastsamp[c][0])) >> 5) - lastsamp[c][1] + (lastsamp[c][1] >> 4); break;
	case 3: temp = temp + (lastsamp[c][0] << 1) + ((-(lastsamp[c][0] + (lastsamp[c][0] << 2) + (lastsamp[c][0] << 3))) >> 6) - lastsamp[c][1] + (((lastsamp[c][1] << 1) + lastsamp[c][1]) >> 4); break;
	default:
		return 0;
	}
	lastsamp[c][1] = lastsamp[c][0];
	lastsamp[c][0] = temp;
	return temp;
}

INT16 getbrr(int c)
{
	int temp;
	INT16 sample;
	if (!dsp.voiceon[c]) 
		return 0;

	if (!dsp.brrstat[c])
	{
		dsp.brrstat[c] = 1;
		dsp.brrctrl[c] = spcram[dsp.voiceaddr[c]++];
		range[c] = dsp.brrctrl[c] >> 4;
		filter[c] = (dsp.brrctrl[c] >> 2) & 3;

	}
	if (dsp.brrstat[c] & 1) /*First nibble*/
	{
		temp = spcram[dsp.voiceaddr[c]] >> 4;
		dsp.brrstat[c]++;
		sample = decodebrr(temp, c); //(temp<<(dsp.brrctrl[c]>>4))>>1;
	}
	else
	{
		temp = spcram[dsp.voiceaddr[c]++] & 0xF;
		dsp.brrstat[c]++;
		sample = decodebrr(temp, c);
		if (dsp.brrstat[c] == 17)
		{
			dsp.brrstat[c] = 0;
			if (dsp.brrctrl[c] & 1)
			{
				if (dsp.brrctrl[c] & 2)
				{
					dsp.voiceaddr[c] = spcram[(dsp.dir << 8) + dsp.sourcenum[c] + 2] | (spcram[(dsp.dir << 8) + dsp.sourcenum[c] + 3] << 8);
				}
				else
				{
					dsp.voiceon[c] = 0;
					dsp.endx |= (1 << c);
					//                                        dsp.voiceend[c]=1;
					//                                        dsp.adsrstat[c]=RELEASE;
					//                                        dsp.edelay[c]=1;
				}
			}
		}
	}
	return sample;
}

INT16 dspbuffer[20000];
INT16 dsprealbuffer[8][20000];
int dsppos = 0;
int dspwsel = 0, dsprsel = 0;

int bufferready = 0;
void initdsp()
{
}

int dspqlen = 0;
void refillbuffer()
{
	INT16* p = pBurnSoundOut;
	int c;
	if (!dspqlen) 
		return;

	for (c = 0; c < nBurnSoundLen; c++)
		p[c] = dsprealbuffer[dsprsel][c];
	dsprsel++;
	dsprsel &= 7;
	dspqlen--;
}

void pollsound()
{
	if (bufferready)
	{
		bufferready--;
		refillbuffer();
	}

}

int dspsamples[8];

void polldsp()
{
	int c;
	int sample=0;
	INT16 s;
	short totalsamplel = 0, totalsampler = 0;
	for (c = 0; c < 8; c++)
	{
		dsp.pitchcounter[c] += dsp.pitch[c];
		if (dsp.pitchcounter[c] < 0)// || dsp.voiceend[c])
			sample = dspsamples[c];
		else while (dsp.pitchcounter[c] >= 0 && dsp.pitch[c])
		{
			s = (INT16)getbrr(c);
			sample = (int)s;
			dspsamples[c] = sample;
			dsp.pitchcounter[c] -= 0x1000;
		}
		if (dsp.non & (1 << c))
		{
			sample = dsp.noise & 0x3FFF;
			if (dsp.noise & 0x4000)
				sample |= 0xFFFF8000;
		}
		sample *= dsp.evol[c];
		sample >>= 11;
		dsp.outx[c] = sample >> 8;
		if (dsp.volumel[c]) totalsamplel += (((sample * dsp.volumel[c]) >> 7) >> 3);
		if (dsp.volumer[c]) totalsampler += (((sample * dsp.volumer[c]) >> 7) >> 3);
		dsp.edelay[c]--;
		if (dsp.edelay[c] <= 0)
		{
			if (dsp.adsrstat[c] == RELEASE)
			{
				dsp.edelay[c] = 1;
				dsp.evol[c] -= 8;
			}
			else switch (dsp.etype[c])
			{
			case 0: /*Direct Gain*/
				dsp.evol[c] = (dsp.gain[c] & 0x7F) << 4;
				break;
			case 1: /*Gain*/
				switch ((dsp.gain[c] >> 5) & 3)
				{
				case 0: /*Linear decrease*/
					dsp.evol[c] -= 32;
					break;
				case 1: /*Exponential decrease*/
					dsp.evol[c] -= ((dsp.evol[c] - 1) >> 8) + 1;
					break;
				case 2: /*Linear increase*/
					dsp.evol[c] += 32;
					break;
				case 3: /*Bent increase*/
					dsp.evol[c] += (dsp.evol[c] < 0x600) ? 32 : 8;
					break;
				}
				dsp.edelay[c] = ratetable[dsp.gain[c] & 0x1F];
				break;
			case 2: case 3: /*ADSR*/
//                                if (c==7) printf("ADSR state now %i\n",dsp.adsrstat[c]);
				switch (dsp.adsrstat[c])
				{
				case ATTACK:
					if ((dsp.adsr1[c] & 0xF) == 0xF)
					{
						dsp.evol[c] += 1024;
						dsp.edelay[c] = 1;
					}
					else
					{
						dsp.evol[c] += 32;
						dsp.edelay[c] = ratetable[((dsp.adsr1[c] & 0xF) << 1) | 1];
					}
					if (dsp.evol[c] >= 0x7FF)
					{
						dsp.evol[c] = 0x7FF;
						dsp.adsrstat[c] = DECAY;
					}
					break;
				case DECAY:
					dsp.evol[c] -= ((dsp.evol[c] - 1) >> 8) + 1;
					if (dsp.evol[c] <= (dsp.adsr2[c] & 0xE0) << 3)
					{
						dsp.evol[c] = (dsp.adsr2[c] & 0xE0) << 3;
						dsp.adsrstat[c] = SUSTAIN;
					}
					dsp.edelay[c] = ratetable[((dsp.adsr1[c] & 0x70) >> 2) | 1];
					break;
				case SUSTAIN:
					dsp.evol[c] -= ((dsp.evol[c] - 1) >> 8) + 1;
					//                                        printf("Evol %i now %i\n",c,dsp.evol[c]);
					dsp.edelay[c] = ratetable[dsp.adsr2[c] & 0x1F];
					//                                        printf("edelay now %i %02X %02X %02X\n",dsp.edelay[c],dsp.adsr1[c],dsp.adsr2[c],dsp.gain[c]);
					break;
				case RELEASE:
					dsp.edelay[c] = 1;
					dsp.evol[c] -= 8;
					break;
				}
				break;
			}
			if (dsp.evol[c] > 0x7FF) dsp.evol[c] = 0x7FF;
			if (dsp.evol[c] < 0) dsp.evol[c] = 0;
			dsp.envx[c] = (dsp.evol[c] >> 4);
		}
	}
	dsp.noisedelay--;
	if (dsp.noisedelay <= 0)
	{
		dsp.noisedelay += ratetable[dsp.noiserate];
		dsp.noise = (dsp.noise >> 1) | (((dsp.noise << 14) ^ (dsp.noise << 13)) & 0x4000);
	}
	dspbuffer[dsppos++] = totalsamplel;
	dspbuffer[dsppos++] = totalsampler;
	if (dsppos >= (nBurnSoundLen))
	{
		if (dspqlen == 8)
		{
			dspwsel--;
			dspwsel &= 7;
			dspqlen--;
		}
		memcpy(dsprealbuffer[dspwsel], dspbuffer, nBurnSoundLen * 2);
		dspqlen++;
		dspwsel++;
		dspwsel &= 7;
		dsppos = 0;
		bufferready++;
		
	}
	spctotal++;
}
