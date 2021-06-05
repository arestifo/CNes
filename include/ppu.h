#ifndef CNES_PPU_H
#define CNES_PPU_H

#include "nes.h"

// TODO: Add docs to PPU registers if I get around to it
typedef enum ppureg {
  PPUCTRL,    // $2000: PPU control register (write)
  PPUMASK,    // $2001: PPU mask register (write)
  PPUSTATUS,  // $2002: PPU status register (read)
  OAMADDR,    // $2003: OAM address register (write)
  OAMDATA,    // $2004: OAM data register (read, write)
  PPUSCROLL,  // $2005: PPU scroll position register (write twice)
  PPUADDR,    // $2006: PPU VRAM address register (write twice)
  PPUDATA     // $2007: PPU VRAM data register (read, write)
} ppureg_t;

#define PPUCTRL_VRAM_INC_BIT      2
#define PPUCTRL_NMI_ENABLE_BIT    7

#define PPUMASK_SHOW_BGR_LEFT_BIT 1
#define PPUMASK_SHOW_SPR_LEFT_BIT 2
#define PPUMASK_SHOW_BGR_BIT      3
#define PPUMASK_SHOW_SPR_BIT      4

#define PPUSTATUS_VBLANK_BIT      7
#define PPUSTATUS_ZEROHIT_BIT     6
#define PPUSTATUS_OVERFLOW_BIT    5

#define NUM_PPUREGS 8

// Size of total PPU addressable memory
#define PPU_MEM_SZ  0x4000

// Size of OAM memory
#define OAM_SZ      0x0100

// Size of each nametable
#define NT_SZ       0x0400

// 262 scanlines per frame: 1 pre-render, 240 visible, 1 post-render, and 20 vblank lines.
// Each scanlines takes 341 PPU cycles to complete, so each scanline "renders" 341 pixels:
// 1 idle pre-render, 256 visible pixels,
#define NUM_SCANLINES     262
#define DOTS_PER_SCANLINE 341

typedef struct ppu {
  // PPU memory
  u8 regs[NUM_PPUREGS];   // PPU internal registers
  u8 mem[PPU_MEM_SZ];     // PPU memory
  u8 oam[OAM_SZ];         // PPU Object Attribute Memory

  // PPU scanline positions
  u64 frameno;            // Current PPU frame
  u16 vram_addr;          // Current VRAM address
  u16 x;                  // Current X position (dot within current scanline)
  u16 y;                  // Current Y position (current dot)
  u8  scroll_x;           // Current scroll X position
  u8  scroll_y;           // Current scroll Y position
  bool nmi_occurred;      // Whether the PPU is currently generating NMIs or not

  u64 ticks;              // Number of PPU cycles
} ppu_t;

// PPU register access
// These functions can be thought of as an interface between the CPU and PPU
u8   ppu_reg_read(nes_t *nes, ppureg_t reg);
void ppu_reg_write(nes_t *nes, ppureg_t reg, u8 val);

// Internal PPU read functions. Should only be called by internal PPU functions
u8   ppu_read(nes_t *nes, u16 addr);
void ppu_write(nes_t *nes, u16 addr, u8 val);

void ppu_init(nes_t *nes);
void ppu_tick(nes_t *nes, window_t *wnd);
void ppu_destroy(nes_t *nes);

#endif
