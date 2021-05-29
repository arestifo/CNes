#ifndef CNES_MEM_H
#define CNES_MEM_H
#include "nes.h"

void write8(u8 *mem, u16 addr, u8 val);
void write16(u8 *mem, u16 addr, u16 val);
u8   read8(u8 *mem, u16 addr);
u16  read16(u8 *mem, u16 addr);

#endif
