#ifndef CNES_NES_MEM_H
#define CNES_NES_MEM_H
#include "types.h"
// All the memory the nes is able to address
u8 mem[0xFFFF];

void write8(u16, u8);
void write16(u16, u16);
u8 read8(u16);
u16 read16(u16);

#endif //CNES_NES_MEM_H
