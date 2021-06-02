#ifndef CNES_PPU_H
#define CNES_PPU_H
#include "nes.h"

enum ppuregs {
  PPUCTRL, PPUMASK, PPUSTATUS, OAMADDR, OAMDATA, PPUSCROLL, PPUADDR, PPUDATA
};


#define NUM_PPUREGS 8
#define PPU_MEM_SZ 0x4000
#define OAM_SZ     0x0100

struct ppu {
  u8 *regs;  // PPU internal registers
  u8 *mem;   // PPU memory
  u8 *oam;   // PPU Object Attribute Memory
};

// PPU register access
// reg is an int between 0 and 7 that represents the last digit of the PPU
// register being accessed
u8   ppu_read(struct nes *nes, u8 reg);
void ppu_write(struct nes *nes, u8 reg, u8 val);

void ppu_init(struct nes *nes);
void ppu_tick(struct nes *nes);
void ppu_destroy(struct nes *nes);

#endif
