
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#include "statehandler.h"

static void sh_writeByte(StateHandler* sh, uint8_t val);
static uint8_t sh_readByte(StateHandler* sh);

StateHandler* sh_init(bool saving, const uint8_t* data, int size) {
  StateHandler* sh = (StateHandler*)BurnMalloc(sizeof(StateHandler));
  sh->saving = saving;
  sh->offset = 0;
  if(!saving) {
    sh->data = BurnMalloc(size);
    memcpy(sh->data, data, size);
    sh->allocSize = size;
  } else {
    sh->data = BurnMalloc(512 * 1024);
	sh->allocSize = 512 * 1024;
  }
  return sh;
}

void sh_free(StateHandler* sh) {
  BurnFree(sh->data);
  BurnFree(sh);
}

static void sh_writeByte(StateHandler* sh, uint8_t val) {
  if(sh->offset >= sh->allocSize) {
    // realloc
    sh->data = BurnRealloc(sh->data, sh->allocSize * 2);
    sh->allocSize *= 2;
  }
  sh->data[sh->offset++] = val;
}

static uint8_t sh_readByte(StateHandler* sh) {
  if(sh->offset >= sh->allocSize) {
	  bprintf(0, _T("offset >= allocSize!  %x   %x\n"), sh->offset, sh->allocSize);
	  // reading above data (should never happen)
    return 0;
  }
  return sh->data[sh->offset++];
}

void sh_handleBools(StateHandler* sh, ...) {
  va_list args;
  va_start(args, sh);
  while(true) {
    bool* v = va_arg(args, bool*);
    if(v == NULL) break;
    if(sh->saving) {
      sh_writeByte(sh, *v ? 1 : 0);
    } else {
      *v = sh_readByte(sh) > 0 ? true : false;
    }
  }
  va_end(args);
}

void sh_handleBytes(StateHandler* sh, ...) {
  va_list args;
  va_start(args, sh);
  while(true) {
    uint8_t* v = va_arg(args, uint8_t*);
    if(v == NULL) break;
    if(sh->saving) {
      sh_writeByte(sh, *v);
    } else {
      *v = sh_readByte(sh);
    }
  }
  va_end(args);
}

void sh_handleBytesS(StateHandler* sh, ...) {
  va_list args;
  va_start(args, sh);
  while(true) {
    int8_t* v = va_arg(args, int8_t*);
    if(v == NULL) break;
    if(sh->saving) {
      sh_writeByte(sh, (uint8_t) *v);
    } else {
      *v = (int8_t) sh_readByte(sh);
    }
  }
  va_end(args);
}

void sh_handleWords(StateHandler* sh, ...) {
  va_list args;
  va_start(args, sh);
  while(true) {
    uint16_t* v = va_arg(args, uint16_t*);
    if(v == NULL) break;
    if(sh->saving) {
      uint16_t val = *v;
      for(int i = 0; i < 16; i += 8) sh_writeByte(sh, (val >> i) & 0xff);
    } else {
      uint16_t val = 0;
      for(int i = 0; i < 16; i += 8) val |= sh_readByte(sh) << i;
      *v = val;
    }
  }
  va_end(args);
}

void sh_handleWordsS(StateHandler* sh, ...) {
  va_list args;
  va_start(args, sh);
  while(true) {
    int16_t* v = va_arg(args, int16_t*);
    if(v == NULL) break;
    if(sh->saving) {
      uint16_t val = (uint16_t) *v;
      for(int i = 0; i < 16; i += 8) sh_writeByte(sh, (val >> i) & 0xff);
    } else {
      uint16_t val = 0;
      for(int i = 0; i < 16; i += 8) val |= sh_readByte(sh) << i;
      *v = (int16_t) val;
    }
  }
  va_end(args);
}

void sh_handleLongLongs(StateHandler* sh, ...) {
  va_list args;
  va_start(args, sh);
  while(true) {
    uint64_t* v = va_arg(args, uint64_t*);
    if(v == NULL) break;
    if(sh->saving) {
      uint64_t val = *v;
	  for(int i = 0; i < 64; i += 8) sh_writeByte(sh, (val >> i) & 0xff);
    } else {
      uint64_t val = 0;
      for(int i = 0; i < 64; i += 8) val |= (uint64_t)sh_readByte(sh) << i;
      *v = val;
    }
  }
  va_end(args);
}

void sh_handleInts(StateHandler* sh, ...) {
  va_list args;
  va_start(args, sh);
  while(true) {
    uint32_t* v = va_arg(args, uint32_t*);
    if(v == NULL) break;
    if(sh->saving) {
      uint32_t val = *v;
      for(int i = 0; i < 32; i += 8) sh_writeByte(sh, (val >> i) & 0xff);
    } else {
      uint32_t val = 0;
      for(int i = 0; i < 32; i += 8) val |= sh_readByte(sh) << i;
      *v = val;
    }
  }
  va_end(args);
}

void sh_handleIntsS(StateHandler* sh, ...) {
  va_list args;
  va_start(args, sh);
  while(true) {
    int32_t* v = va_arg(args, int32_t*);
    if(v == NULL) break;
    if(sh->saving) {
      uint32_t val = (uint32_t) *v;
      for(int i = 0; i < 32; i += 8) sh_writeByte(sh, (val >> i) & 0xff);
    } else {
      uint32_t val = 0;
      for(int i = 0; i < 32; i += 8) val |= sh_readByte(sh) << i;
      *v = (int32_t) val;
    }
  }
  va_end(args);
}

void sh_handleFloats(StateHandler* sh, ...) {
  va_list args;
  va_start(args, sh);
  while(true) {
    float* v = va_arg(args, float*);
    if(v == NULL) break;
    if(sh->saving) {
      uint8_t valData[4] = {};
      *((float*) valData) = *v;
      uint32_t val = *((uint32_t*) valData);
      for(int i = 0; i < 32; i += 8) sh_writeByte(sh, (val >> i) & 0xff);
    } else {
      uint32_t val = 0;
      for(int i = 0; i < 32; i += 8) val |= sh_readByte(sh) << i;
      uint8_t valData[4] = {};
      *((uint32_t*) valData) = val;
      *v = *((float*) valData);
    }
  }
  va_end(args);
}

void sh_handleDoubles(StateHandler* sh, ...) {
  va_list args;
  va_start(args, sh);
  while(true) {
    double* v = va_arg(args, double*);
    if(v == NULL) break;
    if(sh->saving) {
      uint8_t valData[8] = {};
      *((double*) valData) = *v;
      uint64_t val = *((uint64_t*) valData);
	  for(int i = 0; i < 64; i += 8) sh_writeByte(sh, (val >> i) & 0xff);
    } else {
      uint64_t val = 0;
      for(int i = 0; i < 64; i += 8) val |= (uint64_t)sh_readByte(sh) << i;
      uint8_t valData[8] = {};
      *((uint64_t*) valData) = val;
      *v = *((double*) valData);
    }
  }
  va_end(args);
}

void sh_handleByteArray(StateHandler* sh, uint8_t* data, int size) {
  for(int i = 0; i < size; i++) {
    if(sh->saving) {
      sh_writeByte(sh, data[i]);
    } else {
      data[i] = sh_readByte(sh);
    }
  }
}

void sh_handleWordArray(StateHandler* sh, uint16_t* data, int size) {
  for(int i = 0; i < size; i++) {
    if(sh->saving) {
      sh_writeByte(sh, data[i] & 0xff);
      sh_writeByte(sh, (data[i] >> 8) & 0xff);
    } else {
      data[i] = sh_readByte(sh);
      data[i] |= sh_readByte(sh) << 8;
    }
  }
}

void sh_placeInt(StateHandler* sh, int location, uint32_t value) {
  while(sh->offset < location + 4) sh_writeByte(sh, 0);
  sh->data[location] = value & 0xff;
  sh->data[location + 1] = (value >> 8) & 0xff;
  sh->data[location + 2] = (value >> 16) & 0xff;
  sh->data[location + 3] = (value >> 24) & 0xff;
}
