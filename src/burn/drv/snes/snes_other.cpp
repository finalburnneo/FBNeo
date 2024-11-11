
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "snes.h"
#include "cart.h"
#include "ppu.h"
#include "dsp.h"
#include "statehandler.h"

static const int stateVersion = 1;

typedef struct CartHeader {
  // normal header
  uint8_t headerVersion; // 1, 2, 3
  char name[22]; // $ffc0-$ffd4 (max 21 bytes + \0), $ffd4=$00: header V2
  uint8_t speed; // $ffd5.7-4 (always 2 or 3)
  uint8_t type; // $ffd5.3-0
  uint8_t coprocessor; // $ffd6.7-4
  uint8_t chips; // $ffd6.3-0
  uint32_t romSize; // $ffd7 (0x400 << x)
  uint32_t ramSize; // $ffd8 (0x400 << x)
  uint8_t region; // $ffd9 (also NTSC/PAL)
  uint8_t maker; // $ffda ($33: header V3)
  uint8_t version; // $ffdb
  uint16_t checksumComplement; // $ffdc,$ffdd
  uint16_t checksum; // $ffde,$ffdf
  // v2/v3 (v2 only exCoprocessor)
  char makerCode[3]; // $ffb0,$ffb1: (2 chars + \0)
  char gameCode[5]; // $ffb2-$ffb5: (4 chars + \0)
  uint32_t flashSize; // $ffbc (0x400 << x)
  uint32_t exRamSize; // $ffbd (0x400 << x) (used for GSU?)
  uint8_t specialVersion; // $ffbe
  uint8_t exCoprocessor; // $ffbf (if coprocessor = $f)
  // calculated stuff
  int16_t score; // score for header, to see which mapping is most likely
  bool pal; // if this is a rom for PAL regions instead of NTSC
  uint8_t cartType; // calculated type
  bool hasBattery; // battery
} CartHeader;

static void readHeader(const uint8_t* data, int length, int location, CartHeader* header);

bool snes_loadRom(Snes* snes, const uint8_t* data, int length, uint8_t* biosdata, int bioslength) {
  // if smaller than smallest possible, don't load
  if(length < 0x8000) {
    printf("Failed to load rom: rom to small (%d bytes)\n", length);
    return false;
  }
  // check headers
  const int max_headers = 8;
  CartHeader headers[max_headers];
  memset(headers, 0, sizeof(headers));
  for(int i = 0; i < max_headers; i++) {
    headers[i].score = -50;
  }
  if(length >= 0x8000) readHeader(data, length, 0x7fc0, &headers[0]); // lorom
  if(length >= 0x8200) readHeader(data, length, 0x81c0, &headers[1]); // lorom + header
  if(length >= 0x10000) readHeader(data, length, 0xffc0, &headers[2]); // hirom
  if(length >= 0x10200) readHeader(data, length, 0x101c0, &headers[3]); // hirom + header
  if(length >= 0x410000) readHeader(data, length, 0x407fc0, &headers[4]); // exlorom
  if(length >= 0x410200) readHeader(data, length, 0x4081c0, &headers[5]); // exlorom + header
  if(length >= 0x410000) readHeader(data, length, 0x40ffc0, &headers[6]); // exhirom
  if(length >= 0x410200) readHeader(data, length, 0x4101c0, &headers[7]); // exhirom + header
  // see which it is, go backwards to allow picking ExHiROM over HiROM for roms with headers in both spots
  int max = 0;
  int used = 0;
  for(int i = max_headers-1; i >= 0; i--) {
    if(headers[i].score > max) {
      max = headers[i].score;
      used = i;
    }
  }            
  bprintf(0, _T("header used %d\n"), used);
  if(used & 1) {
    // odd-numbered ones are for headered roms
    data += 0x200; // move pointer past header
    length -= 0x200; // and subtract from size
  }
  // check if we can load it
  if(headers[used].cartType > 4) {
    bprintf(0, _T("Failed to load rom: unsupported type (%d)\n"), headers[used].cartType);
    return false;
  }
  // expand to a power of 2
  int newLength = 0x8000;
  while(true) {
    if(length <= newLength) {
      break;
    }
    newLength *= 2;
  }
  uint8_t* newData = BurnMalloc(newLength);
  memcpy(newData, data, length);
  int test = 1;
  while(length != newLength) {
    if(length & test) {
      memcpy(newData + length, newData + length - test, test);
      length += test;
    }
    test *= 2;
  }

  // coprocessor check
  if (headers[used].exCoprocessor == 0x10) {
	  headers[used].cartType = CART_CX4; // cx4
  }

  // -- cart specific config --
  snes->ramFill = 0x00; // default, 00-fill
  if (!strcmp(headers[used].name, "DEATH BRADE") || !strcmp(headers[used].name, "POWERDRIVE")) {
	  snes->ramFill = 0xff; // games prefer 0xff fill
  }
  if (!strcmp(headers[used].name, "ASHITANO JOE") || !strcmp(headers[used].name, "SUCCESS JOE")) {
	  snes->ramFill = 0x3f; // game prefers 0x3f fill
  }
  if (!strcmp(headers[used].name, "PGA TOUR GOLF")) {
	  snes->ramFill = 0x3f;
  }
  if (!strcmp(headers[used].name, "SUPER MARIO KART")) {
	  snes->ramFill = 0x3f; // start always select 2p if 00-fill
  }
  if (!strcmp(headers[used].name, "BATMAN--REVENGE JOKER")) {
	  headers[used].cartType = CART_LOROM; // it's detected as HiROM but actually LoROM (prototype)
  }

  switch (headers[used].coprocessor) {
	  case 2:
		  headers[used].cartType = CART_LOROMOBC1;
		  break;
	  case 3:
		  headers[used].cartType = CART_LOROMSA1;
		  break;
	  case 4:
		  headers[used].cartType = CART_LOROMSDD1;
  }

  switch (bioslength) {
	  case 0x2800: // DSP1-4
		  bprintf(0, _T("-we have dsp bios, lets go!\n"));
		  switch (headers[used].cartType) {
			  case CART_LOROM:
				  headers[used].cartType = CART_LOROMDSP;
				  break;
			  case CART_HIROM:
				  headers[used].cartType = CART_HIROMDSP;
				  break;
		  }
		  break;
	  case 0x11000: // st010/st011
		  bprintf(0, _T("-we have st010/st011 bios, lets go!\n"));
		  headers[used].cartType = CART_LOROMSETA;;
		  break;
  }

  // load it
  const char* typeNames[12] = {"(none)", "LoROM", "HiROM", "ExLoROM", "ExHiROM", "CX4", "LoROM-DSP", "HiROM-DSP", "LoROM-SeTa", "LoROM-SA1", "LoROM-OBC1", "LoROM-SDD1"};
  enum { CART_NONE = 0, CART_LOROM, CART_HIROM, CART_EXLOROM, CART_EXHIROM, CART_CX4, CART_LOROMDSP, CART_HIROMDSP, CART_LOROMSETA, CART_LOROMSA1, CART_LOROMOBC1, CART_LOROMSDD1 };

  bprintf(0, _T("Loaded %S rom (%S)\n"), typeNames[headers[used].cartType], headers[used].pal ? "PAL" : "NTSC");
  bprintf(0, _T("\"%S\"\n"), headers[used].name);

  int bankSize = 0;
  switch (headers[used].cartType) {
	  case CART_HIROM:
	  case CART_EXHIROM:
	  case CART_HIROMDSP:
		  bankSize = 0x10000;
		  break;
	  default:
		  bankSize = 0x8000;
		  break;
  }

  bprintf(
		  0, _T("%dK banks: %d, ramsize: %d%S, coprocessor: %x\n"),
		  bankSize / 1024, newLength / bankSize, headers[used].chips > 0 ? headers[used].ramSize : 0, (headers[used].hasBattery) ? " (battery-backed)" : "", headers[used].exCoprocessor
		 );

  cart_load(
    snes->cart, headers[used].cartType,
    newData, newLength, biosdata, bioslength, headers[used].chips > 0 ? headers[used].ramSize : 0,
    headers[used].hasBattery
  );

  snes_reset(snes, true); // reset after loading
  snes->palTiming = headers[used].pal; // set region
  BurnFree(newData);
  return true;
}

bool snes_isPal(Snes* snes) {
  return snes->palTiming;
}

void snes_setButtonState(Snes* snes, int player, int button, int pressed, int device) {
	// set key in controller
    Input* input = (player == 1) ? snes->input1 : snes->input2;
	uint32_t *c_state = &input->currentState;

	input_setType(input, device);

	if(pressed) {
      *c_state |= 1 << button;
    } else {
      *c_state &= ~(1 << button);
    }

	if (device == DEVICE_SUPERSCOPE) {
		static uint8_t button8_9[2] = { 0, 0 };

		switch (button) {
			case 8:
			case 9:
				button8_9[button & 1] = pressed;
				break;
			case 11: // last button
				*c_state |= 0xff00;
				if (*c_state & SCOPE_FIRE || *c_state & SCOPE_CURSOR) {
					ppu_latchScope(button8_9[0], button8_9[1]);
				}
				break;
		}
	}
}

void snes_setMouseState(Snes* snes, int player, int16_t x, int16_t y, uint8_t buttonA, uint8_t buttonB) {
  Input* input = (player == 1) ? snes->input1 : snes->input2;
  input_setType(input, DEVICE_MOUSE);
  input_setMouse(input, x, y, buttonA, buttonB);
}

void snes_setPixels(Snes* snes, uint8_t* pixelData, int height) {
  // size is 4 (rgba) * 512 (w) * 480 (h)
  ppu_putPixels(pixelData, height);
}

void snes_setSamples(Snes* snes, int16_t* sampleData, int samplesPerFrame) {
  // size is 2 (int16) * 2 (stereo) * samplesPerFrame
  // sets samples in the sampleData
  dsp_getSamples(snes->apu->dsp, sampleData, samplesPerFrame);
}

int snes_saveBattery(Snes* snes, uint8_t* data) {
  int size = 0;
  cart_handleBattery(snes->cart, true, data, &size);
  return size;
}

bool snes_loadBattery(Snes* snes, uint8_t* data, int size) {
  return cart_handleBattery(snes->cart, false, data, &size);
}

int snes_saveState(Snes* snes, uint8_t* data) {
  StateHandler* sh = sh_init(true, NULL, 0);
  uint32_t id = 0x4653534c; // 'LSSF' LakeSnes State File
  uint32_t version = stateVersion;
  sh_handleInts(sh, &id, &version, &version, NULL); // second version to be overridden by length
  cart_handleTypeState(snes->cart, sh);
  // save data
  snes_handleState(snes, sh);
  // store
  sh_placeInt(sh, 8, sh->offset);
  if(data != NULL) memcpy(data, sh->data, sh->offset);
  int size = sh->offset;
  sh_free(sh);
  return size;
}

bool snes_loadState(Snes* snes, uint8_t* data, int size) {
  StateHandler* sh = sh_init(false, data, size);
  uint32_t id = 0, version = 0, length = 0;
  sh_handleInts(sh, &id, &version, &length, NULL);
  bool cartMatch = cart_handleTypeState(snes->cart, sh);
  if(id != 0x4653534c || version != stateVersion || length != size || !cartMatch) {
    sh_free(sh);
    return false;
  }
  // load data
  snes_handleState(snes, sh);
  // finish
  sh_free(sh);
  return true;
}

static void readHeader(const uint8_t* data, int length, int location, CartHeader* header) {
  // read name, TODO: non-ASCII names?
  for(int i = 0; i < 21; i++) {
    uint8_t ch = data[location + i];
    if(ch >= 0x20 && ch < 0x7f) {
      header->name[i] = ch;
    } else {
      header->name[i] = '.';
    }
  }
  header->name[21] = 0;
  // clean name (strip end space)
  int slen = strlen(header->name);
  while (slen > 0 && header->name[slen-1] == ' ') {
	  header->name[slen-1] = '\0';
	  slen--;
  }
  // read rest
  header->speed = data[location + 0x15] >> 4;
  header->type = data[location + 0x15] & 0xf;
  header->coprocessor = data[location + 0x16] >> 4;
  header->chips = data[location + 0x16] & 0xf;
  bprintf(0, _T("cart type, copro, chips: %x, %x, %x\n"), header->type, header->coprocessor, header->chips);
  header->hasBattery = (header->chips == 0x02 || header->chips == 0x05 || header->chips == 0x06);
  header->romSize = 0x400 << data[location + 0x17];
  header->ramSize = 0x400 << data[location + 0x18];
  header->region = data[location + 0x19];
  header->maker = data[location + 0x1a];
  header->version = data[location + 0x1b];
  header->checksumComplement = (data[location + 0x1d] << 8) + data[location + 0x1c];
  header->checksum = (data[location + 0x1f] << 8) + data[location + 0x1e];
  // read v3 and/or v2
  header->headerVersion = 1;
  if(header->maker == 0x33) {
    header->headerVersion = 3;
    // maker code
    for(int i = 0; i < 2; i++) {
      uint8_t ch = data[location - 0x10 + i];
      if(ch >= 0x20 && ch < 0x7f) {
        header->makerCode[i] = ch;
      } else {
        header->makerCode[i] = '.';
      }
    }
    header->makerCode[2] = 0;
    // game code
    for(int i = 0; i < 4; i++) {
      uint8_t ch = data[location - 0xe + i];
      if(ch >= 0x20 && ch < 0x7f) {
        header->gameCode[i] = ch;
      } else {
        header->gameCode[i] = '.';
      }
    }
    header->gameCode[4] = 0;
    header->flashSize = 0x400 << data[location - 4];
    header->exRamSize = 0x400 << data[location - 3];
    header->specialVersion = data[location - 2];
    header->exCoprocessor = data[location - 1];
  } else if(data[location + 0x14] == 0) {
    header->headerVersion = 2;
    header->exCoprocessor = data[location - 1];
  }
  //bprintf(0, _T("ramsize %x  exramsize %x\n"),header->ramSize, header->exRamSize);
  //bprintf(0, _T("exCoprocessor %x\n"), header->exCoprocessor);
  // get region
  header->pal = (header->region >= 0x2 && header->region <= 0xc) || header->region == 0x11;
  header->cartType = location < 0x9000 ? CART_LOROM : CART_HIROM;
  if(location > 0x400000) {
	  // Ex...
	  if (location == 0x407fc0 || location == 0x4081c0) {
		  header->cartType = CART_EXLOROM; // ExLoROM
	  } else {
		  header->cartType = CART_EXHIROM; // ExHiROM
	  }
  }
  // get score
  // TODO: check name, maker/game-codes (if V3) for ASCII, more vectors,
  //   more first opcode, rom-sizes (matches?), type (matches header location?)
  int score = 0;
  score += (header->speed == 2 || header->speed == 3) ? 5 : -4;
  score += (header->type <= 3 || header->type == 5) ? 5 : -2;
  score += (header->coprocessor <= 5 || header->coprocessor >= 0xe) ? 5 : -2;
  score += (header->chips <= 6 || header->chips == 9 || header->chips == 0xa) ? 5 : -2;
  score += (header->region <= 0x14) ? 5 : -2;
  score += (header->checksum + header->checksumComplement == 0xffff) ? 8 : -6;
  uint16_t resetVector = data[location + 0x3c] | (data[location + 0x3d] << 8);
  score += (resetVector >= 0x8000) ? 8 : -20;
  // check first opcode after reset
  int opcodeLoc = location + 0x40 - 0x8000 + (resetVector & 0x7fff);
  uint8_t opcode = 0xff;
  if(opcodeLoc < length) {
    opcode = data[opcodeLoc];
  } else {
    score -= 14;
  }
  if(opcode == 0x78 || opcode == 0x18) {
    // sei, clc (for clc:xce)
    score += 6;
  }
  if(opcode == 0x4c || opcode == 0x5c || opcode == 0x9c) {
    // jmp abs, jml abl, stz abs
    score += 3;
  }
  if(opcode == 0x00 || opcode == 0xff || opcode == 0xdb) {
    // brk, sbc alx, stp
    score -= 6;
  }
  header->score = score;
}
