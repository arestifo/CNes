#ifndef CNES_MEM_H
#define CNES_MEM_H

#include "nes.h"

// cpu_xxx functions deal with CPU memory
void cpu_write8(struct nes *nes, u16 addr, u8 val);
void cpu_write16(struct nes *nes, u16 addr, u16 val);
u8   cpu_read8(struct nes *nes, u16 addr);
u16  cpu_read16(struct nes *nes, u16 addr);

// Stack operations
void cpu_push8(struct nes *nes, u8 val);
void cpu_push16(struct nes *nes, u16 val);
u8   cpu_pop8(struct nes *nes);
u16  cpu_pop16(struct nes *nes);
#endif
