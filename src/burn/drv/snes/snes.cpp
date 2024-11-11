
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "snes.h"
#include "cpu.h"
#include "apu.h"
#include "spc.h"
#include "dma.h"
#include "ppu.h"
#include "cart.h"
#include "cx4.h"
#include "input.h"
#include "statehandler.h"

#include "sa1.h" // debug

static void snes_runCycle(Snes* snes);
static void snes_catchupApu(Snes* snes);
static void snes_doAutoJoypad(Snes* snes);
static uint8_t snes_readReg(Snes* snes, uint16_t adr);
static void snes_writeReg(Snes* snes, uint16_t adr, uint8_t val);
static uint8_t snes_rread(Snes* snes, uint32_t adr); // wrapped by read, to set open bus
static int snes_getAccessTime(Snes* snes, uint32_t adr);
static void build_accesstime(Snes* snes);
static void free_accesstime();

static uint8_t *access_time;

Snes* snes_init(void) {
  Snes* snes = (Snes*)BurnMalloc(sizeof(Snes));
  cpu_init(snes); //, snes_cpuRead, snes_cpuWrite, snes_cpuIdle);
  snes->apu = apu_init(snes);
  snes->dma = dma_init(snes);
  ppu_init(snes);
  snes->cart = cart_init(snes);
  snes->input1 = input_init(snes, 1);
  snes->input2 = input_init(snes, 2);
  snes->palTiming = false;

  return snes;
}

void snes_free(Snes* snes) {
  cpu_free();
  apu_free(snes->apu);
  dma_free(snes->dma);
  ppu_free();
  cart_free(snes->cart);
  input_free(snes->input1);
  input_free(snes->input2);
  free_accesstime();
  BurnFree(snes);
}

void snes_reset(Snes* snes, bool hard) {
  cpu_reset(hard);
  apu_reset(snes->apu);
  dma_reset(snes->dma);
  ppu_reset();
  input_reset(snes->input1);
  input_reset(snes->input2);
  cart_reset(snes->cart);
  if(hard) memset(snes->ram, snes->ramFill, sizeof(snes->ram));
  snes->ramAdr = 0;
  snes->hPos = 0;
  snes->vPos = 0;
  snes->frames = 0;
  snes->cycles = 0;
  snes->syncCycle = 0;
  snes->hIrqEnabled = false;
  snes->vIrqEnabled = false;
  snes->nmiEnabled = false;
  snes->hTimer = 0x1ff * 4;
  snes->vTimer = 0x1ff;
  snes->hvTimer = 0;
  snes->inNmi = false;
  snes->irqCondition = false;
  snes->inIrq = false;
  snes->inVblank = false;
  snes->inRefresh = false;
  memset(snes->portAutoRead, 0, sizeof(snes->portAutoRead));
  snes->autoJoyRead = false;
  snes->autoJoyTimer = 0;
  snes->ppuLatch = true;
  snes->multiplyA = 0xff;
  snes->multiplyResult = 0xfe01;
  snes->divideA = 0xffff;
  snes->divideResult = 0x101;
  snes->fastMem = false;
  snes->openBus = 0;

  build_accesstime(snes);

  snes->nextHoriEvent = 16;
}

void snes_handleState(Snes* snes, StateHandler* sh) {
  sh_handleBools(sh,
    &snes->palTiming, &snes->hIrqEnabled, &snes->vIrqEnabled, &snes->nmiEnabled, &snes->inNmi, &snes->irqCondition,
    &snes->inIrq, &snes->inVblank, &snes->autoJoyRead, &snes->ppuLatch, &snes->fastMem, NULL
  );
  sh_handleBytes(sh, &snes->multiplyA, &snes->openBus, NULL);
  sh_handleWords(sh,
    &snes->hPos, &snes->vPos, &snes->hTimer, &snes->vTimer,
    &snes->portAutoRead[0], &snes->portAutoRead[1], &snes->portAutoRead[2], &snes->portAutoRead[3],
    &snes->multiplyResult, &snes->divideA, &snes->divideResult, NULL
  );
  sh_handleInts(sh, &snes->hvTimer, &snes->ramAdr, &snes->frames, &snes->nextHoriEvent, NULL);
  sh_handleLongLongs(sh, &snes->cycles, &snes->syncCycle, &snes->autoJoyTimer, NULL);
  sh_handleByteArray(sh, snes->ram, 0x20000);
  // components
  cpu_handleState(sh);
  dma_handleState(snes->dma, sh);
  ppu_handleState(sh);
  apu_handleState(snes->apu, sh);
  input_handleState(snes->input1, sh);
  input_handleState(snes->input2, sh);
  cart_handleState(snes->cart, sh);
}

#define DEBUG_CYC 0

void snes_runFrame(Snes* snes) {
#if DEBUG_CYC
  uint32_t apu_cyc_start = apu_cycles(snes->apu);
  uint64_t cpu_cyc_start = snes->cycles;
  bprintf(0, _T("fr. %d: cycles start frame:  %I64u\n"), nCurrentFrame, snes->cycles);
#endif

  while(snes->inVblank) {
    cpu_runOpcode();
  }
  // then run until we are at vblank, or we end up at next frame (DMA caused vblank to be skipped)
  uint32_t frame = snes->frames;
  while(!snes->inVblank && frame == snes->frames) {
    cpu_runOpcode();
  }

#if DEBUG_CYC
  uint32_t apu_cyc_end = apu_cycles(snes->apu);
  uint64_t cpu_cyc_end = snes->cycles;
  bprintf(0, _T("%04d: apu / cpu cycles ran:  %d\t\t%I64u\n"), nCurrentFrame, apu_cyc_end - apu_cyc_start, cpu_cyc_end - cpu_cyc_start);
  bprintf(0, _T("fr. %d: cycles -end- frame:  %I64u\n"), nCurrentFrame, snes->cycles);
#endif
}

void snes_runCycles(Snes* snes, int cycles) {
  for(int i = 0; i < cycles; i += 2) {
    snes_runCycle(snes);
  }
}

void snes_runCycles4(Snes* snes) {
  snes_runCycle(snes);
  snes_runCycle(snes);
}

void snes_runCycles6(Snes* snes) {
  snes_runCycle(snes);
  snes_runCycle(snes);
  snes_runCycle(snes);
}

void snes_runCycles8(Snes* snes) {
  snes_runCycle(snes);
  snes_runCycle(snes);
  snes_runCycle(snes);
  snes_runCycle(snes);
}

void snes_syncCycles(Snes* snes, bool start, int syncCycles) {
  if(start) {
    snes->syncCycle = snes->cycles;
    int count = syncCycles - (snes->cycles % syncCycles);
    snes_runCycles(snes, count);
  } else {
    int count = syncCycles - ((snes->cycles - snes->syncCycle) % syncCycles);
    snes_runCycles(snes, count);
  }
}

int snes_verticalLinecount(Snes* snes) {
	if(!snes->palTiming) {
        // even interlace frame is 263 lines
		return (!ppu_frameInterlace() || !ppu_evenFrame()) ? 262 : 263;
	} else {
		// even interlace frame is 313 lines
		return (!ppu_frameInterlace() || !ppu_evenFrame()) ? 312 : 313;
	}
}

static void snes_runCycle(Snes* snes) {
  snes->cycles += 2;
  if(snes->cart->heavySync) {
	  cart_run(); // run spetzi chippy
  }
  if ((snes->hPos & 2) == 0) {
	  // check for h/v timer irq's every 4 cycles
	  if (snes->hvTimer > 0) {
		  snes->hvTimer -= 2;
		  if (snes->hvTimer == 0) {
//			  bprintf(0, _T("IRQ @  %d,%d\n"),snes->hPos,snes->vPos);
			  snes->inIrq = true;
			  cpu_setIrq(true);
		  }
	  }
	  const bool condition = (
          (snes->vIrqEnabled || snes->hIrqEnabled) &&
		  (snes->vPos == snes->vTimer || !snes->vIrqEnabled) &&
		  (snes->hPos == snes->hTimer || !snes->hIrqEnabled)
	  );
	  if(!snes->irqCondition && condition) {
		  snes->hvTimer = 4;
	  }
	  snes->irqCondition = condition;
  }
  // increment position
  snes->hPos += 2; // must come after irq checks! (hagane, cybernator)
  // handle positional stuff
  if (snes->hPos == snes->nextHoriEvent) {
    switch (snes->hPos) {
      case 16: {
        snes->nextHoriEvent = 22;
        if(snes->vPos == 0) snes->dma->hdmaInitRequested = true;
      } break;
      case 22: {
        snes->nextHoriEvent = 512;
        if(!snes->inVblank && snes->vPos > 0) ppu_latchMode7(snes->vPos);
      } break;
      case 512: {
        snes->nextHoriEvent = 538;
        // render the line halfway of the screen for better compatibility
		if(!snes->inVblank && snes->vPos > 0) ppu_runLine(snes->vPos);
      } break;
      case 538: {
        snes->nextHoriEvent = 1104;
		// +40cycle dram refresh
		snes->inRefresh = true;
		snes_runCycle(snes); snes_runCycle(snes); snes_runCycle(snes); snes_runCycle(snes); snes_runCycle(snes);
		snes_runCycle(snes); snes_runCycle(snes); snes_runCycle(snes); snes_runCycle(snes); snes_runCycle(snes);
		snes_runCycle(snes); snes_runCycle(snes); snes_runCycle(snes); snes_runCycle(snes); snes_runCycle(snes);
		snes_runCycle(snes); snes_runCycle(snes); snes_runCycle(snes); snes_runCycle(snes); snes_runCycle(snes);
		snes->inRefresh = false;
	  } break;
      case 1104: {
        if(!snes->inVblank) snes->dma->hdmaRunRequested = true;
        if(!snes->palTiming) {
          // line 240 of odd frame with no interlace is 4 cycles shorter
          // if((snes->hPos == 1360 && snes->vPos == 240 && !ppu_evenFrame() && !ppu_frameInterlace()) || snes->hPos == 1364) {
			snes->nextHoriEvent = (snes->vPos == 240 && !ppu_evenFrame() && !ppu_frameInterlace()) ? 1360 : 1364;
			//bprintf(0, _T("%d,"),snes->nextHoriEvent);
        } else {
          // line 311 of odd frame with interlace is 4 cycles longer
          // if((snes->hPos == 1364 && (snes->vPos != 311 || ppu_evenFrame() || !ppu_frameInterlace())) || snes->hPos == 1368)
          snes->nextHoriEvent = (snes->vPos != 311 || ppu_evenFrame() || !ppu_frameInterlace()) ? 1364 : 1368;
        }
      } break;
      case 1360:
      case 1364:
      case 1368: { // this is the end (of the h-line)
        snes->nextHoriEvent = 16;

        snes->hPos = 0;
        snes->vPos++;
        if(!snes->palTiming) {
          // even interlace frame is 263 lines
          if((snes->vPos == 262 && (!ppu_frameInterlace() || !ppu_evenFrame())) || snes->vPos == 263) {
			cart_run();
            snes->vPos = 0;
			snes->frames++;
		  }
	    } else {
          // even interlace frame is 313 lines
          if((snes->vPos == 312 && (!ppu_frameInterlace() || !ppu_evenFrame())) || snes->vPos == 313) {
			cart_run();
            snes->vPos = 0;
            snes->frames++;
          }
        }

        // end of hblank, do most vPos-tests
        bool startingVblank = false;
        if(snes->vPos == 0) {
          // end of vblank
          snes->inVblank = false;
          snes->inNmi = false;
          ppu_handleFrameStart();
        } else if(snes->vPos == 225) {
          // ask the ppu if we start vblank now or at vPos 240 (overscan)
          startingVblank = !ppu_checkOverscan();
        } else if(snes->vPos == 240){
          // if we are not yet in vblank, we had an overscan frame, set startingVblank
          if(!snes->inVblank) startingVblank = true;
        }
		if(startingVblank) {
          // catch up the apu at end of emulated frame (we end frame @ start of vblank)
          snes_catchupApu(snes);
          // notify dsp of frame-end, because sometimes dma will extend much further past vblank (or even into the next frame)
          // Megaman X2 (titlescreen animation), Tales of Phantasia (game demo), Actraiser 2 (fade-in @ bootup)
          dsp_newFrame(snes->apu->dsp);

          // we are starting vblank
          ppu_handleVblank();
          snes->inVblank = true;
          snes->inNmi = true;
          if(snes->autoJoyRead) {
            // TODO: this starts a little after start of vblank
            snes->autoJoyTimer = snes->cycles; // 4224;
            snes_doAutoJoypad(snes);
          }
          if(snes->nmiEnabled) {
            cpu_nmi();
          }
        }
      } break;
    }
  }
}

static void snes_catchupApu(Snes* snes) {
  apu_runCycles(snes->apu);
}

static void snes_doAutoJoypad(Snes* snes) {
  memset(snes->portAutoRead, 0, sizeof(snes->portAutoRead));
  // latch controllers
  input_latch(snes->input1, true);
  input_latch(snes->input2, true);
  input_latch(snes->input1, false);
  input_latch(snes->input2, false);
  for(int i = 0; i < 16; i++) {
    uint8_t val = input_read(snes->input1);
    snes->portAutoRead[0] |= ((val & 1) << (15 - i));
    snes->portAutoRead[2] |= (((val >> 1) & 1) << (15 - i));
    val = input_read(snes->input2);
    snes->portAutoRead[1] |= ((val & 1) << (15 - i));
    snes->portAutoRead[3] |= (((val >> 1) & 1) << (15 - i));
  }
}

uint8_t snes_readBBus(Snes* snes, uint8_t adr) {
  if(adr < 0x40) {
    return ppu_read(adr);
  }
  if(adr < 0x80) {
	  snes_catchupApu(snes); // catch up the apu before reading
  //    bprintf(0, _T("SR:%x %x\t\t%I64u\t\tpc: %x\n"), adr&3,snes->apu->outPorts[adr & 0x3],snes->apu->cycles,snes->apu->spc->pc);
    return snes->apu->outPorts[adr & 0x3];
  }
  if(adr == 0x80) {
    uint8_t ret = snes->ram[snes->ramAdr++];
    snes->ramAdr &= 0x1ffff;
    return ret;
  }
  return snes->openBus;
}

void snes_writeBBus(Snes* snes, uint8_t adr, uint8_t val) {
  if(adr < 0x40) {
    ppu_write(adr, val);
    return;
  }
  if(adr < 0x80) {
    snes_catchupApu(snes); // catch up the apu before writing
//	bprintf(0, _T("SW:%x %x\t\t%I64u\t\tpc: %x\n"), adr&3,val,snes->apu->cycles,snes->apu->spc->pc);
    snes->apu->inPorts[adr & 0x3] = val;
    return;
  }
  switch(adr) {
    case 0x80: {
      snes->ram[snes->ramAdr++] = val;
      snes->ramAdr &= 0x1ffff;
      break;
    }
    case 0x81: {
      snes->ramAdr = (snes->ramAdr & 0x1ff00) | val;
      break;
    }
    case 0x82: {
      snes->ramAdr = (snes->ramAdr & 0x100ff) | (val << 8);
      break;
    }
    case 0x83: {
      snes->ramAdr = (snes->ramAdr & 0x0ffff) | ((val & 1) << 16);
      break;
    }
  }
}

static uint8_t snes_readReg(Snes* snes, uint16_t adr) {
  switch(adr) {
    case 0x4210: {
      uint8_t val = 0x2; // CPU version (4 bit)
      val |= snes->inNmi << 7;
      snes->inNmi = false;
      return val | (snes->openBus & 0x70);
    }
    case 0x4211: {
//	bprintf(0, _T("TIMEUP 0x%x @ %d  %d  inirq %x\n"), adr, snes->hPos, snes->vPos,snes->inIrq);
      uint8_t val = snes->inIrq << 7;
      snes->inIrq = false;
      cpu_setIrq(false);
      return val | (snes->openBus & 0x7f);
    }
    case 0x4212: {
      uint8_t val = ((snes->cycles - snes->autoJoyTimer) <= 4224);
      val |= (snes->hPos < 4 || snes->hPos >= 1096) << 6;
      val |= snes->inVblank << 7;
      return val | (snes->openBus & 0x3e);
    }
    case 0x4213: {
      return snes->ppuLatch << 7; // IO-port
    }
    case 0x4214: {
      return snes->divideResult & 0xff;
    }
    case 0x4215: {
      return snes->divideResult >> 8;
    }
    case 0x4216: {
      return snes->multiplyResult & 0xff;
    }
    case 0x4217: {
      return snes->multiplyResult >> 8;
    }
    case 0x4218:
    case 0x421a:
    case 0x421c:
    case 0x421e: {
      return snes->portAutoRead[(adr - 0x4218) / 2] & 0xff;
    }
    case 0x4219:
    case 0x421b:
    case 0x421d:
    case 0x421f: {
      return snes->portAutoRead[(adr - 0x4219) / 2] >> 8;
    }
    default: {
      return snes->openBus;
    }
  }
}

static void snes_writeReg(Snes* snes, uint16_t adr, uint8_t val) {
  //bprintf(0, _T("write 0x%x %x @ %d  %d\n"), adr, val, snes->hPos, snes->vPos);
  switch(adr) {
    case 0x4200: {
      snes->autoJoyRead = val & 0x1;
      if(!snes->autoJoyRead) snes->autoJoyTimer = 0;
      snes->hIrqEnabled = val & 0x10;
      snes->vIrqEnabled = val & 0x20;
      if(!snes->hIrqEnabled && !snes->vIrqEnabled) {
        snes->inIrq = false;
        cpu_setIrq(false);
      }
      // if nmi is enabled while inNmi is still set, immediately generate nmi
      if(!snes->nmiEnabled && (val & 0x80) && snes->inNmi) {
        cpu_nmi();
      }
	  snes->nmiEnabled = val & 0x80;
	  cpu_setIntDelay(); // nmi is delayed by 1 opcode
      break;
    }
    case 0x4201: {
      if(!(val & 0x80) && snes->ppuLatch) {
        // latch the ppu h/v registers
        ppu_latchHV();
      }
      snes->ppuLatch = val & 0x80;
      break;
    }
    case 0x4202: {
      snes->multiplyA = val;
      break;
    }
    case 0x4203: {
      snes->multiplyResult = snes->multiplyA * val;
      break;
    }
    case 0x4204: {
      snes->divideA = (snes->divideA & 0xff00) | val;
      break;
    }
    case 0x4205: {
      snes->divideA = (snes->divideA & 0x00ff) | (val << 8);
      break;
    }
    case 0x4206: {
      if(val == 0) {
        snes->divideResult = 0xffff;
        snes->multiplyResult = snes->divideA;
      } else {
        snes->divideResult = snes->divideA / val;
		snes->multiplyResult = snes->divideA % val;
      }
      break;
    }
    case 0x4207: {
		//snes->hTimer = (snes->hTimer & 0x100) | val;
		snes->hTimer = (snes->hTimer & 0x400) | (val << 2);
		//bprintf(0, _T("hTimer.l  %x  both %x\n"), val<<2, snes->hTimer);
      break;
    }
    case 0x4208: {
		//snes->hTimer = (snes->hTimer & 0x0ff) | ((val & 1) << 8);
		snes->hTimer = (snes->hTimer & 0x03fc) | ((val & 1) << 10);
		//bprintf(0, _T("hTimer.h  %x  both %x\n"), (val&1)<<10, snes->hTimer);
      break;
    }
    case 0x4209: {
      snes->vTimer = (snes->vTimer & 0x100) | val;
	  //bprintf(0, _T("vTimer.l  %x  both %x\n"), val, snes->vTimer);
      break;
    }
    case 0x420a: {
      snes->vTimer = (snes->vTimer & 0x0ff) | ((val & 1) << 8);
	  //bprintf(0, _T("vTimer.h  %x  both %x\n"), ((val & 1) << 8), snes->vTimer);
      break;
    }
    case 0x420b: {
      dma_startDma(snes->dma, val, false);
      break;
    }
    case 0x420c: {
      dma_startDma(snes->dma, val, true);
      break;
    }
    case 0x420d: {
	  snes->fastMem = val & 0x1;
	  //bprintf(0, _T("fastMem %x\n"), val);
	  break;
    }
    default: {
      break;
    }
  }
}

static uint8_t snes_rread(Snes* snes, uint32_t adr) {
  const uint8_t bank = adr >> 16;
  adr &= 0xffff;
  if(bank == 0x7e || bank == 0x7f) {
    return snes->ram[((bank & 1) << 16) | adr]; // ram
  }
  if(bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) {
    if(adr < 0x2000) {
      return snes->ram[adr]; // ram mirror
    }
    if(adr >= 0x2100 && adr < 0x2200) {
      return snes_readBBus(snes, adr & 0xff); // B-bus
    }
    if(adr == 0x4016) {
      return input_read(snes->input1) | (snes->openBus & 0xfc);
    }
    if(adr == 0x4017) {
      return input_read(snes->input2) | (snes->openBus & 0xe0) | 0x1c;
    }
    if(adr >= 0x4200 && adr < 0x4220) {
      return snes_readReg(snes, adr); // internal registers
    }
    if(adr >= 0x4300 && adr < 0x4380) {
      return dma_read(snes->dma, adr); // dma registers
    }
  }
  // read from cart
  return cart_read(snes->cart, bank, adr);
}

void snes_write(Snes* snes, uint32_t adr, uint8_t val) {
  snes->openBus = val;
  snes->adrBus = adr;
  const uint8_t bank = adr >> 16;
  adr &= 0xffff;
  if(bank == 0x7e || bank == 0x7f) {
    snes->ram[((bank & 1) << 16) | adr] = val; // ram
  }
  if(bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) {
    if(adr < 0x2000) {
      snes->ram[adr] = val; // ram mirror
    }
    if(adr >= 0x2100 && adr < 0x2200) {
      snes_writeBBus(snes, adr & 0xff, val); // B-bus
    }
    if(adr == 0x4016) {
      input_latch(snes->input1, val & 1); // input latch
      input_latch(snes->input2, val & 1);
    }
    if(adr >= 0x4200 && adr < 0x4220) {
      snes_writeReg(snes, adr, val); // internal registers
    }
    if(adr >= 0x4300 && adr < 0x4380) {
      dma_write(snes->dma, adr, val); // dma registers
    }
  }
  // write to cart
  cart_write(snes->cart, bank, adr, val);
}

#if 0
static int snes_getAccessTime(Snes* snes, uint32_t adr) {
  uint8_t bank = adr >> 16;
  adr &= 0xffff;
  if((bank < 0x40 || (bank >= 0x80 && bank < 0xc0)) && adr < 0x8000) {
    // 00-3f,80-bf:0-7fff
    if(adr < 0x2000 || adr >= 0x6000) return 8; // 0-1fff, 6000-7fff
    if(adr < 0x4000 || adr >= 0x4200) return 6; // 2000-3fff, 4200-5fff
    return 12; // 4000-41ff
  }
  // 40-7f,co-ff:0000-ffff, 00-3f,80-bf:8000-ffff
  return (snes->fastMem && bank >= 0x80) ? 6 : 8; // depends on setting in banks 80+
}
#endif

static inline int snes_getAccessTime(Snes* snes, uint32_t adr)
{
	if ((adr & 0x408000) == 0)
	{
  		adr &= 0xffff;
		// 00-3f,80-bf:0-7fff
		if(adr < 0x2000 || adr >= 0x6000) return 8; // 0-1fff, 6000-7fff
		if(adr < 0x4000 || adr >= 0x4200) return 6; // 2000-3fff, 4200-5fff
		return 12; // 4000-41ff
	}

	// 40-7f,co-ff:0000-ffff, 00-3f,80-bf:8000-ffff
	return (snes->fastMem && (adr & 0x800000)) ? 6 : 8; // depends on setting in banks 80+
}

static void build_accesstime(Snes* snes) {
  if (access_time == NULL) {
	access_time = (uint8_t *)BurnMalloc(0x1000000 * 2);
  }
  snes->fastMem = 0;
  for (int i = 0; i < 0x1000000; i++) {
	  access_time[i] = snes_getAccessTime(snes, i);
  }
  snes->fastMem = 1;
  for (int i = 0x1000000; i < 0x2000000; i++) {
	  access_time[i] = snes_getAccessTime(snes, i & 0xffffff);
  }
  snes->fastMem = 0;
}

static void free_accesstime() {
  BurnFree(access_time);
}

uint8_t snes_read(Snes* snes, uint32_t adr) {
  snes->adrBus = adr;
  uint8_t val = snes_rread(snes, adr);
  snes->openBus = val;
  return val;
}

void snes_cpuIdle(void* mem, bool waiting) {
  Snes* snes = (Snes*) mem;
  dma_handleDma(snes->dma, 6);
  snes_runCycles6(snes);
}

uint8_t snes_cpuRead(void* mem, uint32_t adr) {
  Snes* snes = (Snes*) mem;
  const int cycles = access_time[adr + (snes->fastMem << 24)];
//  const int cycles = snes_getAccessTime(snes, adr);
  dma_handleDma(snes->dma, cycles);
  snes->adrBus = adr;
  snes_runCycles(snes, cycles - 4);
  const uint8_t rv = snes_read(snes, adr);
  snes_runCycles4(snes);
  return rv;
}

void snes_cpuWrite(void* mem, uint32_t adr, uint8_t val) {
  Snes* snes = (Snes*) mem;
  const int cycles = access_time[adr + (snes->fastMem << 24)];
  //const int cycles = snes_getAccessTime(snes, adr);
  dma_handleDma(snes->dma, cycles);
  snes->adrBus = adr;
  snes_runCycles(snes, cycles);
  snes_write(snes, adr, val);
}

// debugging

void snes_runCpuCycle(Snes* snes) {
  cpu_runOpcode();
}

void snes_runSpcCycle(Snes* snes) {
  // TODO: apu catchup is not aware of this, SPC runs extra cycle(s)
  spc_runOpcode(snes->apu->spc);
}
