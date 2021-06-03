#ifndef CNES_PPU_H
#define CNES_PPU_H

#include "nes.h"

typedef enum ppureg {
  PPUCTRL, PPUMASK, PPUSTATUS, OAMADDR, OAMDATA, PPUSCROLL, PPUADDR, PPUDATA
} ppureg_t;

#define PPUSTATUS_VBLANK_BIT   7
#define PPUSTATUS_ZEROHIT_BIT  6
#define PPUSTATUS_OVERFLOW_BIT 5

#define NUM_PPUREGS 8
#define PPU_MEM_SZ 0x4000
#define OAM_SZ     0x0100

// 262 scanlines per frame: 1 pre-render, 240 visible, 1 post-render, and 20 vblank lines.
// Each scanlines takes 341 PPU cycles to complete, so each scanline "renders" 341 pixels:
// 1 idle pre-render, 256 visible pixel,
#define NUM_SCANLINES 262
#define SCANLINE_CYC  341

struct ppu {
  u8 regs[NUM_PPUREGS];   // PPU internal registers
  u8 mem[PPU_MEM_SZ];     // PPU memory
  u8 oam[OAM_SZ];         // PPU Object Attribute Memory

  u64 cyc;                // PPU cycles TODO: maybe don't need b/c ppu.x & ppu.y
  u32 x;                  // Current X position (position within current scanline)
  u32 y;                  // Current Y position (current scanline)
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
