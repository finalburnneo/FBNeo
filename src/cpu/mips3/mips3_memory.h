
#ifndef MEMORY
#define MEMORY

#include "mips3_common.h"

namespace mips
{

namespace mem
{

extern void write_byte(addr_t address, uint8_t value);
extern void write_half(addr_t address, uint16_t value);
extern void write_word(addr_t address, uint32_t value);
extern void write_dword(addr_t address, uint64_t value);

extern uint8_t read_byte(addr_t address);
extern uint16_t read_half(addr_t address);
extern uint32_t read_word(addr_t address);
extern uint64_t read_dword(addr_t address);

}

}

#endif // MEMORY

