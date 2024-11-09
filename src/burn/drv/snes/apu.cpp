
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "apu.h"
#include "snes.h"
#include "spc.h"
#include "dsp.h"
#include "statehandler.h"

static uint8_t bootRom[0x40]; // algorithmically constructed

static const uint8_t iplDeltas[0x40] = {
  0x18, 0x45, 0xeb, 0x61, 0x1c, 0x9a, 0xdc, 0x06, 0xa1, 0x26, 0x15, 0x07, 0x89, 0x96, 0xb8, 0xe2,
  0xf5, 0xe1, 0x1e, 0xf1, 0xeb, 0x0a, 0xd2, 0xc0, 0xf7, 0x83, 0x7e, 0x60, 0x93, 0x40, 0x15, 0x46,
  0x18, 0xf4, 0xdc, 0x7a, 0x31, 0x3d, 0x2d, 0x64, 0x30, 0xf8, 0x00, 0xcc, 0xa3, 0x67, 0x67, 0x23,
  0xdc, 0xa2, 0x2f, 0xd7, 0xa6, 0x06, 0xd3, 0x84, 0xc4, 0xdc, 0xb8, 0x7f, 0x02, 0x86, 0x47, 0x6a,
};

static const double apuCyclesPerMaster = (32040 * 32) / (1364 * 262 * 60.0);
static const double apuCyclesPerMasterPal = (32040 * 32) / (1364 * 312 * 50.0);

static void apu_cycle(Apu* apu);

static uint8_t ipl_lfsr(uint32_t posTo, int32_t arrayPos) {
  uint32_t seed = 0xa5; // it's magic! (tm)
  uint32_t pos = 0;
  do {
    if ((pos ^ arrayPos) == posTo) break;
    seed = (seed * 1664525 + 1013904223) % 256;
    pos++;
  } while (1);
  return seed;
}

static void ipl_create() {
  uint8_t prev = 0;
  for (int i = 0; i < 0x40; i++) {
    prev = (prev - iplDeltas[i]) & 0xff;
    bootRom[i] = ipl_lfsr(prev, i);
    prev = iplDeltas[i];
  }
}

Apu* apu_init(Snes* snes) {
  Apu* apu = (Apu*)BurnMalloc(sizeof(Apu));
  apu->snes = snes;
  apu->spc = spc_init(apu, apu_spcRead, apu_spcWrite, apu_spcIdle);
  apu->dsp = dsp_init(apu);
  ipl_create();

  return apu;
}

void apu_free(Apu* apu) {
  spc_free(apu->spc);
  dsp_free(apu->dsp);
  BurnFree(apu);
}

uint64_t apu_cycles(Apu* apu) {
  return apu->cycles;
}

void apu_reset(Apu* apu) {
  // TODO: hard reset for apu
  spc_reset(apu->spc, true);
  dsp_reset(apu->dsp);
  memset(apu->ram, 0, sizeof(apu->ram));
  apu->dspAdr = 0;
  apu->romReadable = true;
  apu->cycles = 0;
  memset(apu->inPorts, 0, sizeof(apu->inPorts));
  memset(apu->outPorts, 0, sizeof(apu->outPorts));
  for(int i = 0; i < 3; i++) {
    apu->timer[i].cycles = 0;
    apu->timer[i].divider = 0;
    apu->timer[i].target = 0;
    apu->timer[i].counter = 0;
    apu->timer[i].enabled = false;
  }
}

void apu_handleState(Apu* apu, StateHandler* sh) {
  sh_handleBools(sh, &apu->romReadable, NULL);
  sh_handleBytes(sh,
    &apu->dspAdr, &apu->inPorts[0], &apu->inPorts[1], &apu->inPorts[2], &apu->inPorts[3], &apu->inPorts[4],
    &apu->inPorts[5], &apu->outPorts[0], &apu->outPorts[1], &apu->outPorts[2], &apu->outPorts[3], NULL
  );
  sh_handleLongLongs(sh, &apu->cycles, NULL);
  for(int i = 0; i < 3; i++) {
    sh_handleBools(sh, &apu->timer[i].enabled, NULL);
    sh_handleBytes(sh, &apu->timer[i].cycles, &apu->timer[i].divider, &apu->timer[i].target, &apu->timer[i].counter, NULL);
  }
  sh_handleByteArray(sh, apu->ram, 0x10000);
  // components
  spc_handleState(apu->spc, sh);
  dsp_handleState(apu->dsp, sh);
}

void apu_runCycles(Apu* apu) {
  uint64_t sync_to = (uint64_t)apu->snes->cycles * (apu->snes->palTiming ? apuCyclesPerMasterPal : apuCyclesPerMaster);
  while (apu->cycles < sync_to) {
    spc_runOpcode(apu->spc);
  }
}

static void apu_cycle(Apu* apu) {
  if((apu->cycles & 0x1f) == 0) {
    // every 32 cycles
    dsp_cycle(apu->dsp);
  }

  // handle timers
  for(int i = 0; i < 3; i++) {
    if(apu->timer[i].cycles == 0) {
      apu->timer[i].cycles = i == 2 ? 16 : 128;
      if(apu->timer[i].enabled) {
        apu->timer[i].divider++;
        if(apu->timer[i].divider == apu->timer[i].target) {
          apu->timer[i].divider = 0;
          apu->timer[i].counter++;
          apu->timer[i].counter &= 0xf;
        }
      }
    }
    apu->timer[i].cycles--;
  }

  apu->cycles++;
}

uint8_t apu_read(Apu* apu, uint16_t adr) {
  switch(adr) {
    case 0xf0:
    case 0xf1:
    case 0xfa:
    case 0xfb:
    case 0xfc: {
      return 0;
    }
    case 0xf2: {
      return apu->dspAdr;
    }
    case 0xf3: {
      return dsp_read(apu->dsp, apu->dspAdr & 0x7f);
    }
    case 0xf4:
    case 0xf5:
    case 0xf6:
    case 0xf7:
    case 0xf8:
    case 0xf9: {
      return apu->inPorts[adr - 0xf4];
    }
    case 0xfd:
    case 0xfe:
    case 0xff: {
      uint8_t ret = apu->timer[adr - 0xfd].counter;
      apu->timer[adr - 0xfd].counter = 0;
      return ret;
    }
  }
  if(apu->romReadable && adr >= 0xffc0) {
    return bootRom[adr - 0xffc0];
  }
  return apu->ram[adr];
}

void apu_write(Apu* apu, uint16_t adr, uint8_t val) {
  switch(adr) {
    case 0xf0: {
      break; // test register
    }
    case 0xf1: {
      for(int i = 0; i < 3; i++) {
        if(!apu->timer[i].enabled && (val & (1 << i))) {
          apu->timer[i].divider = 0;
          apu->timer[i].counter = 0;
        }
        apu->timer[i].enabled = val & (1 << i);
      }
      if(val & 0x10) {
        apu->inPorts[0] = 0;
        apu->inPorts[1] = 0;
      }
      if(val & 0x20) {
        apu->inPorts[2] = 0;
        apu->inPorts[3] = 0;
      }
      apu->romReadable = val & 0x80;
      break;
    }
    case 0xf2: {
      apu->dspAdr = val;
      break;
    }
    case 0xf3: {
      if(apu->dspAdr < 0x80) dsp_write(apu->dsp, apu->dspAdr, val);
      break;
    }
    case 0xf4:
    case 0xf5:
    case 0xf6:
    case 0xf7: {
      apu->outPorts[adr - 0xf4] = val;
      break;
    }
    case 0xf8:
    case 0xf9: {
      apu->inPorts[adr - 0xf4] = val;
      break;
    }
    case 0xfa:
    case 0xfb:
    case 0xfc: {
      apu->timer[adr - 0xfa].target = val;
      break;
    }
  }
  apu->ram[adr] = val;
}

uint8_t apu_spcRead(void* mem, uint16_t adr) {
  Apu* apu = (Apu*) mem;
  apu_cycle(apu);
  return apu_read(apu, adr);
}

void apu_spcWrite(void* mem, uint16_t adr, uint8_t val) {
  Apu* apu = (Apu*) mem;
  apu_cycle(apu);
  apu_write(apu, adr, val);
}

void apu_spcIdle(void* mem, bool waiting) {
  Apu* apu = (Apu*) mem;
  apu_cycle(apu);
}
