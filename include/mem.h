#ifndef CNES_MEM_H
#define CNES_MEM_H

#include "nes.h"

// cpu_xxx functions deal with CPU memory
// TODO: Call mapper functions
void cpu_write8(nes_t *nes, u16 addr, u8 val);
void cpu_write16(nes_t *nes, u16 addr, u16 val);
u8   cpu_read8(nes_t *nes, u16 addr);
u16  cpu_read16(nes_t *nes, u16 addr);

// Stack operations
void cpu_push8(nes_t *nes, u8 val);
void cpu_push16(nes_t *nes, u16 val);
u8   cpu_pop8(nes_t *nes);
u16  cpu_pop16(nes_t *nes);
#endif
