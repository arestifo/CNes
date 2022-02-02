#ifndef CNES_MAPPER_H
#define CNES_MAPPER_H

#include "nes.h"

// Interface describing a generic mapper
// TODO: Implement NROM like this
typedef struct mapper {
  // A mapper's primary ability is to expand the amount of available PRG/CHR ROM space
  // to allow for better graphics, sound, etc. These functions do mapping for CPU and PPU addresses
  u16 (*ppu_read)(nes_t *nes, u16 addr);
  u16 (*cpu_read)(nes_t *nes, u16 addr);

  void (*mapper_write)(nes_t *nes, u16 addr, u8 val);
} mapper_t;


void mapper_init(nes_t *nes);
void mapper_destroy(mapper_t *mapper);
#endif
