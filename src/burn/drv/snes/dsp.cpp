
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "dsp.h"
#include "apu.h"
#include "statehandler.h"

static const int rateValues[32] = {
  0, 2048, 1536, 1280, 1024, 768, 640, 512,
  384, 320, 256, 192, 160, 128, 96, 80,
  64, 48, 40, 32, 24, 20, 16, 12,
  10, 8, 6, 5, 4, 3, 2, 1
};

static const int rateOffsets[32] = {
  0, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536,
  0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0, 1040, 536, 0
};

static const int gaussValues[512] = {
  0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000,
  0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x001, 0x002, 0x002, 0x002, 0x002, 0x002,
  0x002, 0x002, 0x003, 0x003, 0x003, 0x003, 0x003, 0x004, 0x004, 0x004, 0x004, 0x004, 0x005, 0x005, 0x005, 0x005,
  0x006, 0x006, 0x006, 0x006, 0x007, 0x007, 0x007, 0x008, 0x008, 0x008, 0x009, 0x009, 0x009, 0x00a, 0x00a, 0x00a,
  0x00b, 0x00b, 0x00b, 0x00c, 0x00c, 0x00d, 0x00d, 0x00e, 0x00e, 0x00f, 0x00f, 0x00f, 0x010, 0x010, 0x011, 0x011,
  0x012, 0x013, 0x013, 0x014, 0x014, 0x015, 0x015, 0x016, 0x017, 0x017, 0x018, 0x018, 0x019, 0x01a, 0x01b, 0x01b,
  0x01c, 0x01d, 0x01d, 0x01e, 0x01f, 0x020, 0x020, 0x021, 0x022, 0x023, 0x024, 0x024, 0x025, 0x026, 0x027, 0x028,
  0x029, 0x02a, 0x02b, 0x02c, 0x02d, 0x02e, 0x02f, 0x030, 0x031, 0x032, 0x033, 0x034, 0x035, 0x036, 0x037, 0x038,
  0x03a, 0x03b, 0x03c, 0x03d, 0x03e, 0x040, 0x041, 0x042, 0x043, 0x045, 0x046, 0x047, 0x049, 0x04a, 0x04c, 0x04d,
  0x04e, 0x050, 0x051, 0x053, 0x054, 0x056, 0x057, 0x059, 0x05a, 0x05c, 0x05e, 0x05f, 0x061, 0x063, 0x064, 0x066,
  0x068, 0x06a, 0x06b, 0x06d, 0x06f, 0x071, 0x073, 0x075, 0x076, 0x078, 0x07a, 0x07c, 0x07e, 0x080, 0x082, 0x084,
  0x086, 0x089, 0x08b, 0x08d, 0x08f, 0x091, 0x093, 0x096, 0x098, 0x09a, 0x09c, 0x09f, 0x0a1, 0x0a3, 0x0a6, 0x0a8,
  0x0ab, 0x0ad, 0x0af, 0x0b2, 0x0b4, 0x0b7, 0x0ba, 0x0bc, 0x0bf, 0x0c1, 0x0c4, 0x0c7, 0x0c9, 0x0cc, 0x0cf, 0x0d2,
  0x0d4, 0x0d7, 0x0da, 0x0dd, 0x0e0, 0x0e3, 0x0e6, 0x0e9, 0x0ec, 0x0ef, 0x0f2, 0x0f5, 0x0f8, 0x0fb, 0x0fe, 0x101,
  0x104, 0x107, 0x10b, 0x10e, 0x111, 0x114, 0x118, 0x11b, 0x11e, 0x122, 0x125, 0x129, 0x12c, 0x130, 0x133, 0x137,
  0x13a, 0x13e, 0x141, 0x145, 0x148, 0x14c, 0x150, 0x153, 0x157, 0x15b, 0x15f, 0x162, 0x166, 0x16a, 0x16e, 0x172,
  0x176, 0x17a, 0x17d, 0x181, 0x185, 0x189, 0x18d, 0x191, 0x195, 0x19a, 0x19e, 0x1a2, 0x1a6, 0x1aa, 0x1ae, 0x1b2,
  0x1b7, 0x1bb, 0x1bf, 0x1c3, 0x1c8, 0x1cc, 0x1d0, 0x1d5, 0x1d9, 0x1dd, 0x1e2, 0x1e6, 0x1eb, 0x1ef, 0x1f3, 0x1f8,
  0x1fc, 0x201, 0x205, 0x20a, 0x20f, 0x213, 0x218, 0x21c, 0x221, 0x226, 0x22a, 0x22f, 0x233, 0x238, 0x23d, 0x241,
  0x246, 0x24b, 0x250, 0x254, 0x259, 0x25e, 0x263, 0x267, 0x26c, 0x271, 0x276, 0x27b, 0x280, 0x284, 0x289, 0x28e,
  0x293, 0x298, 0x29d, 0x2a2, 0x2a6, 0x2ab, 0x2b0, 0x2b5, 0x2ba, 0x2bf, 0x2c4, 0x2c9, 0x2ce, 0x2d3, 0x2d8, 0x2dc,
  0x2e1, 0x2e6, 0x2eb, 0x2f0, 0x2f5, 0x2fa, 0x2ff, 0x304, 0x309, 0x30e, 0x313, 0x318, 0x31d, 0x322, 0x326, 0x32b,
  0x330, 0x335, 0x33a, 0x33f, 0x344, 0x349, 0x34e, 0x353, 0x357, 0x35c, 0x361, 0x366, 0x36b, 0x370, 0x374, 0x379,
  0x37e, 0x383, 0x388, 0x38c, 0x391, 0x396, 0x39b, 0x39f, 0x3a4, 0x3a9, 0x3ad, 0x3b2, 0x3b7, 0x3bb, 0x3c0, 0x3c5,
  0x3c9, 0x3ce, 0x3d2, 0x3d7, 0x3dc, 0x3e0, 0x3e5, 0x3e9, 0x3ed, 0x3f2, 0x3f6, 0x3fb, 0x3ff, 0x403, 0x408, 0x40c,
  0x410, 0x415, 0x419, 0x41d, 0x421, 0x425, 0x42a, 0x42e, 0x432, 0x436, 0x43a, 0x43e, 0x442, 0x446, 0x44a, 0x44e,
  0x452, 0x455, 0x459, 0x45d, 0x461, 0x465, 0x468, 0x46c, 0x470, 0x473, 0x477, 0x47a, 0x47e, 0x481, 0x485, 0x488,
  0x48c, 0x48f, 0x492, 0x496, 0x499, 0x49c, 0x49f, 0x4a2, 0x4a6, 0x4a9, 0x4ac, 0x4af, 0x4b2, 0x4b5, 0x4b7, 0x4ba,
  0x4bd, 0x4c0, 0x4c3, 0x4c5, 0x4c8, 0x4cb, 0x4cd, 0x4d0, 0x4d2, 0x4d5, 0x4d7, 0x4d9, 0x4dc, 0x4de, 0x4e0, 0x4e3,
  0x4e5, 0x4e7, 0x4e9, 0x4eb, 0x4ed, 0x4ef, 0x4f1, 0x4f3, 0x4f5, 0x4f6, 0x4f8, 0x4fa, 0x4fb, 0x4fd, 0x4ff, 0x500,
  0x502, 0x503, 0x504, 0x506, 0x507, 0x508, 0x50a, 0x50b, 0x50c, 0x50d, 0x50e, 0x50f, 0x510, 0x511, 0x511, 0x512,
  0x513, 0x514, 0x514, 0x515, 0x516, 0x516, 0x517, 0x517, 0x517, 0x518, 0x518, 0x518, 0x518, 0x518, 0x519, 0x519
};

static int clamp16(int val);
static int clip16(int val);
static bool dsp_checkCounter(Dsp* dsp, int rate);
static void dsp_cycleChannel(Dsp* dsp, int ch);
static void dsp_handleEcho(Dsp* dsp);
static void dsp_handleGain(Dsp* dsp, int ch);
static void dsp_decodeBrr(Dsp* dsp, int ch);
static int16_t dsp_getSample(Dsp* dsp, int ch);
static void dsp_handleNoise(Dsp* dsp);

Dsp* dsp_init(Apu* apu) {
  Dsp* dsp = (Dsp*)BurnMalloc(sizeof(Dsp));
  dsp->apu = apu;
  return dsp;
}

void dsp_free(Dsp* dsp) {
  BurnFree(dsp);
}

void dsp_reset(Dsp* dsp) {
  memset(dsp->ram, 0, sizeof(dsp->ram));
  dsp->ram[0x7c] = 0xff; // set ENDx
  for(int i = 0; i < 8; i++) {
    dsp->channel[i].pitch = 0;
    dsp->channel[i].pitchCounter = 0;
    dsp->channel[i].pitchModulation = false;
    memset(dsp->channel[i].decodeBuffer, 0, sizeof(dsp->channel[i].decodeBuffer));
    dsp->channel[i].bufferOffset = 0;
    dsp->channel[i].srcn = 0;
    dsp->channel[i].decodeOffset = 0;
    dsp->channel[i].blockOffset = 0;
    dsp->channel[i].brrHeader = 0;
    dsp->channel[i].useNoise = false;
    dsp->channel[i].startDelay = 0;
    memset(dsp->channel[i].adsrRates, 0, sizeof(dsp->channel[i].adsrRates));
    dsp->channel[i].adsrState = 0;
    dsp->channel[i].sustainLevel = 0;
    dsp->channel[i].gainSustainLevel = 0;
    dsp->channel[i].useGain = false;
    dsp->channel[i].gainMode = 0;
    dsp->channel[i].directGain = false;
    dsp->channel[i].gainValue = 0;
    dsp->channel[i].preclampGain = 0;
    dsp->channel[i].gain = 0;
    dsp->channel[i].keyOn = false;
    dsp->channel[i].keyOff = false;
    dsp->channel[i].sampleOut = 0;
    dsp->channel[i].volumeL = 0;
    dsp->channel[i].volumeR = 0;
    dsp->channel[i].echoEnable = false;
  }
  dsp->counter = 0;
  dsp->dirPage = 0;
  dsp->evenCycle = true;
  dsp->mute = true;
  dsp->reset = true;
  dsp->masterVolumeL = 0;
  dsp->masterVolumeR = 0;
  dsp->sampleOutL = 0;
  dsp->sampleOutR = 0;
  dsp->echoOutL = 0;
  dsp->echoOutR = 0;
  dsp->noiseSample = 0x4000;
  dsp->noiseRate = 0;
  dsp->echoWrites = false;
  dsp->echoVolumeL = 0;
  dsp->echoVolumeR = 0;
  dsp->feedbackVolume = 0;
  dsp->echoBufferAdr = 0;
  dsp->echoDelay = 0;
  dsp->echoLength = 0;
  dsp->echoBufferIndex = 0;
  dsp->firBufferIndex = 0;
  memset(dsp->firValues, 0, sizeof(dsp->firValues));
  memset(dsp->firBufferL, 0, sizeof(dsp->firBufferL));
  memset(dsp->firBufferR, 0, sizeof(dsp->firBufferR));
  memset(dsp->sampleBuffer, 0, sizeof(dsp->sampleBuffer));
  dsp->sampleOffset = 0;
  dsp->sampleCount = 0;
  dsp->audioQuePos = 0;
  dsp->audioQueTotal = 0;
  memset(dsp->audioQue, 0, sizeof(dsp->audioQue));
}

void dsp_newFrame(Dsp* dsp) {
  dsp->audioQue[dsp->audioQuePos & (MAX_AUDIOQUEUE-1)].count = dsp->sampleCount;
  dsp->audioQue[dsp->audioQuePos & (MAX_AUDIOQUEUE-1)].boundary = dsp->sampleOffset;
  dsp->audioQue[dsp->audioQuePos & (MAX_AUDIOQUEUE-1)].frame = dsp->apu->snes->frames;
  dsp->audioQueTotal = ++dsp->audioQuePos;

  dsp->sampleCount = 0;
}

int dsp_checkAudioQue(Dsp* dsp) {
  return dsp->audioQuePos;
}

void dsp_handleState(Dsp* dsp, StateHandler* sh) {
  sh_handleBools(sh, &dsp->evenCycle, &dsp->mute, &dsp->reset, &dsp->echoWrites, NULL);
  sh_handleBytes(sh, &dsp->noiseRate, &dsp->firBufferIndex, NULL);
  sh_handleBytesS(sh,
    &dsp->masterVolumeL, &dsp->masterVolumeR, &dsp->echoVolumeL, &dsp->echoVolumeR, &dsp->feedbackVolume,
    &dsp->firValues[0], &dsp->firValues[1], &dsp->firValues[2], &dsp->firValues[3], &dsp->firValues[4],
    &dsp->firValues[5], &dsp->firValues[6], &dsp->firValues[7], NULL
  );
  sh_handleWords(sh,
    &dsp->counter, &dsp->dirPage, &dsp->echoBufferAdr, &dsp->echoDelay, &dsp->echoLength, &dsp->echoBufferIndex, &dsp->sampleOffset,
    &dsp->audioQuePos, &dsp->audioQueTotal, NULL
  );
  sh_handleWordsS(sh,
    &dsp->sampleOutL, &dsp->sampleOutR, &dsp->echoOutL, &dsp->echoOutR, &dsp->noiseSample,
    &dsp->firBufferL[0], &dsp->firBufferL[1], &dsp->firBufferL[2], &dsp->firBufferL[3], &dsp->firBufferL[4],
    &dsp->firBufferL[5], &dsp->firBufferL[6], &dsp->firBufferL[7], &dsp->firBufferR[0], &dsp->firBufferR[1],
    &dsp->firBufferR[2], &dsp->firBufferR[3], &dsp->firBufferR[4], &dsp->firBufferR[5], &dsp->firBufferR[6],
    &dsp->firBufferR[7], NULL
  );
  for(int i = 0; i < 8; i++) {
    sh_handleBools(sh,
      &dsp->channel[i].pitchModulation, &dsp->channel[i].useNoise, &dsp->channel[i].useGain, &dsp->channel[i].directGain,
      &dsp->channel[i].keyOn, &dsp->channel[i].keyOff, &dsp->channel[i].echoEnable, NULL
    );
    sh_handleBytes(sh,
      &dsp->channel[i].bufferOffset, &dsp->channel[i].srcn, &dsp->channel[i].blockOffset, &dsp->channel[i].brrHeader,
      &dsp->channel[i].startDelay, &dsp->channel[i].adsrRates[0], &dsp->channel[i].adsrRates[1],
      &dsp->channel[i].adsrRates[2], &dsp->channel[i].adsrRates[3], &dsp->channel[i].adsrState,
      &dsp->channel[i].sustainLevel, &dsp->channel[i].gainSustainLevel, &dsp->channel[i].gainMode, NULL
    );
    sh_handleBytesS(sh, &dsp->channel[i].volumeL, &dsp->channel[i].volumeR, NULL);
    sh_handleWords(sh,
      &dsp->channel[i].pitch, &dsp->channel[i].pitchCounter, &dsp->channel[i].decodeOffset, &dsp->channel[i].gainValue,
      &dsp->channel[i].preclampGain, &dsp->channel[i].gain, NULL
    );
    sh_handleWordsS(sh,
      &dsp->channel[i].decodeBuffer[0], &dsp->channel[i].decodeBuffer[1], &dsp->channel[i].decodeBuffer[2],
      &dsp->channel[i].decodeBuffer[3], &dsp->channel[i].decodeBuffer[4], &dsp->channel[i].decodeBuffer[5],
      &dsp->channel[i].decodeBuffer[6], &dsp->channel[i].decodeBuffer[7], &dsp->channel[i].decodeBuffer[8],
      &dsp->channel[i].decodeBuffer[9], &dsp->channel[i].decodeBuffer[10], &dsp->channel[i].decodeBuffer[11],
      &dsp->channel[i].sampleOut, NULL
    );
  }
  sh_handleByteArray(sh, dsp->ram, 0x80);
  sh_handleByteArray(sh, (uint8_t*)&dsp->audioQue, sizeof(dsp->audioQue));
//  sh_handleByteArray(sh, (UINT8*)&dsp->sampleBuffer[0], 0x800*2*2);
  sh_handleInts(sh, &dsp->sampleCount, NULL);
}

void dsp_cycle(Dsp* dsp) {
  dsp->sampleOutL = 0;
  dsp->sampleOutR = 0;
  dsp->echoOutL = 0;
  dsp->echoOutR = 0;
  for(int i = 0; i < 8; i++) {
    dsp_cycleChannel(dsp, i);
  }
  dsp_handleEcho(dsp); // also applies master volume
  dsp->counter = dsp->counter == 0 ? 30720 - 1 : dsp->counter - 1;
  dsp_handleNoise(dsp);
  dsp->evenCycle = !dsp->evenCycle;
  // handle mute flag
  if(dsp->mute) {
    dsp->sampleOutL = 0;
    dsp->sampleOutR = 0;
  }
  if (bBurnRunAheadFrame == 0) {
    // put final sample in the samplebuffer
    dsp->sampleBuffer[(dsp->sampleOffset   & 0xfff) * 2 + 0] = dsp->sampleOutL;
    dsp->sampleBuffer[(dsp->sampleOffset++ & 0xfff) * 2 + 1] = dsp->sampleOutR;
	dsp->sampleCount++;
  }
}
static int clamp16(int val) {
  return val < -0x8000 ? -0x8000 : (val > 0x7fff ? 0x7fff : val);
}

static int clip16(int val) {
  return (int16_t) (val & 0xffff);
}

static bool dsp_checkCounter(Dsp* dsp, int rate) {
  if(rate == 0) return false;
  return ((dsp->counter + rateOffsets[rate]) % rateValues[rate]) == 0;
}

static void dsp_handleEcho(Dsp* dsp) {
  // increment fir buffer index
  dsp->firBufferIndex++;
  dsp->firBufferIndex &= 0x7;
  // get value out of ram
  uint16_t adr = dsp->echoBufferAdr + dsp->echoBufferIndex;
  int16_t ramSample = dsp->apu->ram[adr] | (dsp->apu->ram[(adr + 1) & 0xffff] << 8);
  dsp->firBufferL[dsp->firBufferIndex] = ramSample >> 1;
  ramSample = dsp->apu->ram[(adr + 2) & 0xffff] | (dsp->apu->ram[(adr + 3) & 0xffff] << 8);
  dsp->firBufferR[dsp->firBufferIndex] = ramSample >> 1;
  // calculate FIR-sum
  int sumL = 0, sumR = 0;
  for(int i = 0; i < 8; i++) {
    sumL += (dsp->firBufferL[(dsp->firBufferIndex + i + 1) & 0x7] * dsp->firValues[i]) >> 6;
    sumR += (dsp->firBufferR[(dsp->firBufferIndex + i + 1) & 0x7] * dsp->firValues[i]) >> 6;
    if(i == 6) {
      // clip to 16-bit before last addition
      sumL = clip16(sumL);
      sumR = clip16(sumR);
    }
  }
  sumL = clamp16(sumL) & ~1;
  sumR = clamp16(sumR) & ~1;
  // apply master volume and modify output with sum
  dsp->sampleOutL = clamp16(((dsp->sampleOutL * dsp->masterVolumeL) >> 7) + ((sumL * dsp->echoVolumeL) >> 7));
  dsp->sampleOutR = clamp16(((dsp->sampleOutR * dsp->masterVolumeR) >> 7) + ((sumR * dsp->echoVolumeR) >> 7));
  // get echo value
  int echoL = clamp16(dsp->echoOutL + clip16((sumL * dsp->feedbackVolume) >> 7)) & ~1;
  int echoR = clamp16(dsp->echoOutR + clip16((sumR * dsp->feedbackVolume) >> 7)) & ~1;
  // write it to ram
  if(dsp->echoWrites) {
    dsp->apu->ram[adr] = echoL & 0xff;
    dsp->apu->ram[(adr + 1) & 0xffff] = echoL >> 8;
    dsp->apu->ram[(adr + 2) & 0xffff] = echoR & 0xff;
    dsp->apu->ram[(adr + 3) & 0xffff] = echoR >> 8;
  }
  // handle indexes
  if(dsp->echoBufferIndex == 0) {
    dsp->echoLength = dsp->echoDelay * 4;
  }
  dsp->echoBufferIndex += 4;
  if(dsp->echoBufferIndex >= dsp->echoLength) {
    dsp->echoBufferIndex = 0;
  }
}

static void dsp_cycleChannel(Dsp* dsp, int ch) {
  // handle pitch counter
  int pitch = dsp->channel[ch].pitch;
  if(ch > 0 && dsp->channel[ch].pitchModulation) {
    pitch += ((dsp->channel[ch - 1].sampleOut >> 5) * pitch) >> 10;
  }
  // get current brr header and get sample address
  dsp->channel[ch].brrHeader = dsp->apu->ram[dsp->channel[ch].decodeOffset];
  uint16_t samplePointer = dsp->dirPage + 4 * dsp->channel[ch].srcn;
  if(dsp->channel[ch].startDelay == 0) samplePointer += 2;
  uint16_t sampleAdr = dsp->apu->ram[samplePointer] | (dsp->apu->ram[(samplePointer + 1) & 0xffff] << 8);
  // handle starting of sample
  if(dsp->channel[ch].startDelay > 0) {
    if(dsp->channel[ch].startDelay == 5) {
      // first keyed on
      dsp->channel[ch].decodeOffset = sampleAdr;
      dsp->channel[ch].blockOffset = 1;
      dsp->channel[ch].bufferOffset = 0;
      dsp->channel[ch].brrHeader = 0;
    }
    dsp->channel[ch].gain = 0;
    dsp->channel[ch].startDelay--;
    dsp->channel[ch].pitchCounter = 0;
    if(dsp->channel[ch].startDelay > 0 && dsp->channel[ch].startDelay < 4) {
      dsp->channel[ch].pitchCounter = 0x4000;
    }
    pitch = 0;
  }
  // get sample
  int sample = 0;
  if(dsp->channel[ch].useNoise) {
    sample = clip16(dsp->noiseSample * 2);
  } else {
    sample = dsp_getSample(dsp, ch);
  }
  sample = ((sample * dsp->channel[ch].gain) >> 11) & ~1;
  // handle reset and release
  if(dsp->reset || (dsp->channel[ch].brrHeader & 0x03) == 1) {
    dsp->channel[ch].adsrState = 3; // go to release
    dsp->channel[ch].gain = 0;
  }
  // handle keyon/keyoff
  if(dsp->evenCycle) {
    if(dsp->channel[ch].keyOff) {
      dsp->channel[ch].adsrState = 3; // go to release
    }
    if(dsp->channel[ch].keyOn) {
      dsp->channel[ch].startDelay = 5;
      dsp->channel[ch].adsrState = 0; // go to attack
      dsp->channel[ch].keyOn = false;
      dsp->ram[0x7c] &= ~(1 << ch); // clear ENDx
    }
  }
  // handle envelope
  if(dsp->channel[ch].startDelay == 0) {
    dsp_handleGain(dsp, ch);
  }
  // decode new brr samples if needed and update offsets
  if(dsp->channel[ch].pitchCounter >= 0x4000) {
    dsp_decodeBrr(dsp, ch);
    if(dsp->channel[ch].blockOffset >= 7) {
      if(dsp->channel[ch].brrHeader & 0x1) {
        dsp->channel[ch].decodeOffset = sampleAdr;
        dsp->ram[0x7c] |= 1 << ch; // set ENDx
      } else {
        dsp->channel[ch].decodeOffset += 9;
      }
      dsp->channel[ch].blockOffset = 1;
    } else {
      dsp->channel[ch].blockOffset += 2;
    }
  }
  // update pitch counter
  dsp->channel[ch].pitchCounter &= 0x3fff;
  dsp->channel[ch].pitchCounter += pitch;
  if(dsp->channel[ch].pitchCounter > 0x7fff) dsp->channel[ch].pitchCounter = 0x7fff;
  // set outputs
  dsp->ram[(ch << 4) | 8] = dsp->channel[ch].gain >> 4;
  dsp->ram[(ch << 4) | 9] = sample >> 8;
  dsp->channel[ch].sampleOut = sample;
  dsp->sampleOutL = clamp16(dsp->sampleOutL + ((sample * dsp->channel[ch].volumeL) >> 7));
  dsp->sampleOutR = clamp16(dsp->sampleOutR + ((sample * dsp->channel[ch].volumeR) >> 7));
  if(dsp->channel[ch].echoEnable) {
    dsp->echoOutL = clamp16(dsp->echoOutL + ((sample * dsp->channel[ch].volumeL) >> 7));
    dsp->echoOutR = clamp16(dsp->echoOutR + ((sample * dsp->channel[ch].volumeR) >> 7));
  }
}

static void dsp_handleGain(Dsp* dsp, int ch) {
  int newGain = dsp->channel[ch].gain;
  int rate = 0;
  // handle gain mode
  if(dsp->channel[ch].adsrState == 3) { // release
    rate = 31;
    newGain -= 8;
  } else {
    if(!dsp->channel[ch].useGain) {
      rate = dsp->channel[ch].adsrRates[dsp->channel[ch].adsrState];
      switch(dsp->channel[ch].adsrState) {
        case 0: newGain += rate == 31 ? 1024 : 32; break; // attack
        case 1: newGain -= ((newGain - 1) >> 8) + 1; break; // decay
        case 2: newGain -= ((newGain - 1) >> 8) + 1; break; // sustain
      }
    } else {
      if(!dsp->channel[ch].directGain) {
        rate = dsp->channel[ch].adsrRates[3];
        switch(dsp->channel[ch].gainMode) {
          case 0: newGain -= 32; break; // linear decrease
          case 1: newGain -= ((newGain - 1) >> 8) + 1; break; // exponential decrease
          case 2: newGain += 32; break; // linear increase
          case 3: newGain += (dsp->channel[ch].preclampGain < 0x600) ? 32 : 8; break; // bent increase
        }
      } else { // direct gain
        rate = 31;
        newGain = dsp->channel[ch].gainValue;
      }
    }
  }
  // use sustain level according to mode
  int sustainLevel = dsp->channel[ch].useGain ? dsp->channel[ch].gainSustainLevel : dsp->channel[ch].sustainLevel;
  if(dsp->channel[ch].adsrState == 1 && (newGain >> 8) == sustainLevel) {
    dsp->channel[ch].adsrState = 2; // go to sustain
  }
  // store pre-clamped gain (for bent increase)
  dsp->channel[ch].preclampGain = newGain & 0xffff;
  // clamp gain
  if(newGain < 0 || newGain > 0x7ff) {
    newGain = newGain < 0 ? 0 : 0x7ff;
    if(dsp->channel[ch].adsrState == 0) {
      dsp->channel[ch].adsrState = 1; // go to decay
    }
  }
  // store new value
  if(dsp_checkCounter(dsp, rate)) dsp->channel[ch].gain = newGain;
}

static int16_t dsp_getSample(Dsp* dsp, int ch) {
  int pos = (dsp->channel[ch].pitchCounter >> 12) + dsp->channel[ch].bufferOffset;
  int offset = (dsp->channel[ch].pitchCounter >> 4) & 0xff;
  int16_t news = dsp->channel[ch].decodeBuffer[(pos + 3) % 12];
  int16_t olds = dsp->channel[ch].decodeBuffer[(pos + 2) % 12];
  int16_t olders = dsp->channel[ch].decodeBuffer[(pos + 1) % 12];
  int16_t oldests = dsp->channel[ch].decodeBuffer[pos % 12];
  int out = (gaussValues[0xff - offset] * oldests) >> 11;
  out += (gaussValues[0x1ff - offset] * olders) >> 11;
  out += (gaussValues[0x100 + offset] * olds) >> 11;
  out = clip16(out) + ((gaussValues[offset] * news) >> 11);
  return clamp16(out) & ~1;
}

static void dsp_decodeBrr(Dsp* dsp, int ch) {
  int shift = dsp->channel[ch].brrHeader >> 4;
  int filter = (dsp->channel[ch].brrHeader & 0xc) >> 2;
  int bOff = dsp->channel[ch].bufferOffset;
  int old = dsp->channel[ch].decodeBuffer[bOff == 0 ? 11 : bOff - 1] >> 1;
  int older = dsp->channel[ch].decodeBuffer[bOff == 0 ? 10 : bOff - 2] >> 1;
  uint8_t curByte = 0;
  for(int i = 0; i < 4; i++) {
    int s = 0;
    if(i & 1) {
      s = curByte & 0xf;
    } else {
      curByte = dsp->apu->ram[(dsp->channel[ch].decodeOffset + dsp->channel[ch].blockOffset + (i >> 1)) & 0xffff];
      s = curByte >> 4;
    }
    if(s > 7) s -= 16;
    if(shift <= 0xc) {
      s = (s << shift) >> 1;
    } else {
      s = (s >> 3) << 12;
    }
    switch(filter) {
      case 1: s += old + (-old >> 4); break;
      case 2: s += 2 * old + ((3 * -old) >> 5) - older + (older >> 4); break;
      case 3: s += 2 * old + ((13 * -old) >> 6) - older + ((3 * older) >> 4); break;
    }
    dsp->channel[ch].decodeBuffer[bOff + i] = clamp16(s) * 2; // cuts off bit 15
    older = old;
    old = dsp->channel[ch].decodeBuffer[bOff + i] >> 1;
  }
  dsp->channel[ch].bufferOffset += 4;
  if(dsp->channel[ch].bufferOffset >= 12) dsp->channel[ch].bufferOffset = 0;
}

static void dsp_handleNoise(Dsp* dsp) {
  if(dsp_checkCounter(dsp, dsp->noiseRate)) {
    int bit = (dsp->noiseSample & 1) ^ ((dsp->noiseSample >> 1) & 1);
    dsp->noiseSample = ((dsp->noiseSample >> 1) & 0x3fff) | (bit << 14);
  }
}

uint8_t dsp_read(Dsp* dsp, uint8_t adr) {
  return dsp->ram[adr];
}

void dsp_write(Dsp* dsp, uint8_t adr, uint8_t val) {
  int ch = adr >> 4;
  switch(adr) {
    case 0x00: case 0x10: case 0x20: case 0x30: case 0x40: case 0x50: case 0x60: case 0x70: {
      dsp->channel[ch].volumeL = val;
      break;
    }
    case 0x01: case 0x11: case 0x21: case 0x31: case 0x41: case 0x51: case 0x61: case 0x71: {
      dsp->channel[ch].volumeR = val;
      break;
    }
    case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52: case 0x62: case 0x72: {
      dsp->channel[ch].pitch = (dsp->channel[ch].pitch & 0x3f00) | val;
      break;
    }
    case 0x03: case 0x13: case 0x23: case 0x33: case 0x43: case 0x53: case 0x63: case 0x73: {
      dsp->channel[ch].pitch = ((dsp->channel[ch].pitch & 0x00ff) | (val << 8)) & 0x3fff;
      break;
    }
    case 0x04: case 0x14: case 0x24: case 0x34: case 0x44: case 0x54: case 0x64: case 0x74: {
      dsp->channel[ch].srcn = val;
      break;
    }
    case 0x05: case 0x15: case 0x25: case 0x35: case 0x45: case 0x55: case 0x65: case 0x75: {
      dsp->channel[ch].adsrRates[0] = (val & 0xf) * 2 + 1;
      dsp->channel[ch].adsrRates[1] = ((val & 0x70) >> 4) * 2 + 16;
      dsp->channel[ch].useGain = (val & 0x80) == 0;
      break;
    }
    case 0x06: case 0x16: case 0x26: case 0x36: case 0x46: case 0x56: case 0x66: case 0x76: {
      dsp->channel[ch].adsrRates[2] = val & 0x1f;
      dsp->channel[ch].sustainLevel = (val & 0xe0) >> 5;
      break;
    }
    case 0x07: case 0x17: case 0x27: case 0x37: case 0x47: case 0x57: case 0x67: case 0x77: {
      dsp->channel[ch].directGain = (val & 0x80) == 0;
      dsp->channel[ch].gainMode = (val & 0x60) >> 5;
      dsp->channel[ch].adsrRates[3] = val & 0x1f;
      dsp->channel[ch].gainValue = (val & 0x7f) * 16;
      dsp->channel[ch].gainSustainLevel = (val & 0xe0) >> 5;
      break;
    }
    case 0x0c: {
      dsp->masterVolumeL = val;
      break;
    }
    case 0x1c: {
      dsp->masterVolumeR = val;
      break;
    }
    case 0x2c: {
      dsp->echoVolumeL = val;
      break;
    }
    case 0x3c: {
      dsp->echoVolumeR = val;
      break;
    }
    case 0x4c: {
      for(int i = 0; i < 8; i++) {
        dsp->channel[i].keyOn = val & (1 << i);
      }
      break;
    }
    case 0x5c: {
      for(int i = 0; i < 8; i++) {
        dsp->channel[i].keyOff = val & (1 << i);
      }
      break;
    }
    case 0x6c: {
      dsp->reset = val & 0x80;
      dsp->mute = val & 0x40;
      dsp->echoWrites = (val & 0x20) == 0;
      dsp->noiseRate = val & 0x1f;
      break;
    }
    case 0x7c: {
      val = 0; // any write clears ENDx
      break;
    }
    case 0x0d: {
      dsp->feedbackVolume = val;
      break;
    }
    case 0x2d: {
      for(int i = 0; i < 8; i++) {
        dsp->channel[i].pitchModulation = val & (1 << i);
      }
      break;
    }
    case 0x3d: {
      for(int i = 0; i < 8; i++) {
        dsp->channel[i].useNoise = val & (1 << i);
      }
      break;
    }
    case 0x4d: {
      for(int i = 0; i < 8; i++) {
        dsp->channel[i].echoEnable = val & (1 << i);
      }
      break;
    }
    case 0x5d: {
      dsp->dirPage = val << 8;
      break;
    }
    case 0x6d: {
      dsp->echoBufferAdr = val << 8;
      break;
    }
    case 0x7d: {
      dsp->echoDelay = (val & 0xf) * 512; // 2048-byte steps, stereo sample is 4 bytes
      break;
    }
    case 0x0f: case 0x1f: case 0x2f: case 0x3f: case 0x4f: case 0x5f: case 0x6f: case 0x7f: {
      dsp->firValues[ch] = val;
      break;
    }
  }
  dsp->ram[adr] = val;
}

void dsp_getSamples(Dsp* dsp, int16_t* sampleData, int samplesPerFrame) {
  // resample from 534 / 641 samples per frame to wanted value

  if (dsp->audioQuePos < 1) {
    bprintf(0, _T("---[DSP: no queue'd audio! d'oh!\n"));
    return;
  }
  dsp->audioQuePos--;
  int quePos = ((dsp->audioQueTotal-1) - dsp->audioQuePos) & (MAX_AUDIOQUEUE-1);
  if (sampleData == NULL || samplesPerFrame == 0) return;

  int wantedSamples = dsp->audioQue[quePos].count; // (dsp->apu->snes->palTiming ? 641.0 : 534.0);
  if (dsp->audioQueTotal > 1) {
    bprintf(0, _T("rendering que#/queTotal/frame: %d / %d / %d   sams  %d   current frame %d\n"), quePos, dsp->audioQueTotal, dsp->audioQue[quePos].frame, wantedSamples, dsp->apu->snes->frames);
  }

  double adder = (float)wantedSamples / samplesPerFrame;
  double location = dsp->audioQue[quePos].boundary - wantedSamples;
  for(int i = 0; i < samplesPerFrame; i++) {
    sampleData[i * 2 + 0] = dsp->sampleBuffer[(((int) location) & 0xfff) * 2 + 0];
    sampleData[i * 2 + 1] = dsp->sampleBuffer[(((int) location) & 0xfff) * 2 + 1];
    location += adder;
  }
}
