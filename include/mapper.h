#ifndef CNES_MAPPER_H
#define CNES_MAPPER_H

#include "nes.h"

// Interface describing a generic mapper
typedef struct mapper {
  // Function pointers for accessing memory
  // PPU memory
  u8   (*ppu_read)(nes_t *nes, u16 addr);
  void (*ppu_write)(nes_t *nes, u16 addr, u8 val);

  // CPU memory
  u8   (*cpu_read)(nes_t *nes, u16 addr);
  void (*cpu_write)(nes_t *nes, u16 addr, u8 val);
} mapper_t;

#endif
