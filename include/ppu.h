#ifndef CNES_PPU_H
#define CNES_PPU_H
#include "nes.h"

#define NUM_PPUREGS 9
#define PPU_MEM_SZ 0x4000
#define OAM_SZ     0x0100

typedef enum {
  PPUCTRL,
  PPUMASK,
  PPUSTATUS,
  OAMADDR,
  OAMDATA,
  PPUSCROLL,
  PPUADDR,
  PPUDATA,
  OAMDMA
} ppureg;

struct ppu {
  u8 *regs;  // PPU internal registers
  u8 *mem;   //
  u8 *oam;
};

// PPU register access
u8   ppu_read(ppureg reg);
void ppu_write(ppureg reg, u8 val);

void ppu_init(struct ppu *ppu, struct nes *nes);
void ppu_tick(struct ppu *ppu);
void ppu_destroy(struct ppu *ppu);

#endif
