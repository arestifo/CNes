#ifndef CNES_PPU_H
#define CNES_PPU_H

#include "nes.h"

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
#define PPUCTRL_SPR_PT_BASE_BIT   3
#define PPUCTRL_BGR_PT_BASE_BIT   4
#define PPUCTRL_SPRITE_SZ_BIT     5
#define PPUCTRL_NMI_ENABLE_BIT    7

#define PPUMASK_SHOW_BGR_LEFT_BIT 1
#define PPUMASK_SHOW_SPR_LEFT_BIT 2
#define PPUMASK_SHOW_BGR_BIT      3
#define PPUMASK_SHOW_SPR_BIT      4

#define PPUSTATUS_VBLANK_BIT      7
#define PPUSTATUS_ZEROHIT_BIT     6
#define PPUSTATUS_OVERFLOW_BIT    5

#define SPRITE_ATTR_PRIORITY_BIT  5
#define SPRITE_ATTR_FLIPH_BIT     6
#define SPRITE_ATTR_FLIPV_BIT     7

#define NUM_PPUREGS  8

// Size of total PPU addressable memory
#define PPU_MEM_SZ   0x4000

// Size of a palette
#define PALETTE_SZ   64
#define PALETTE_BASE 0x3F00

// 262 scanlines per frame: 1 pre-render, 240 visible, 1 post-render, and 20 vblank lines.
// Each scanlines takes 341 PPU cycles to complete, so each scanline "renders" 341 pixels:
// 1 idle pre-render, 256 visible pixels,
#define NUM_SCANLINES     262
#define DOTS_PER_SCANLINE 341

#define OAM_NUM_SPR     64
#define SEC_OAM_NUM_SPR 8

typedef struct color {
  u8 r;
  u8 g;
  u8 b;
} color_t;

typedef struct sprite {
  u8 y_pos;     // Y position of the
  u8 tile_idx;  // Index into the sprite pattern table
  u8 attr;      // Sprite attributes: lower 2 color bits, flipping,
  u8 x_pos;     // X position of the left of the sprite
} sprite_t;

typedef struct ppu {
  // PPU memory
  u8 regs[NUM_PPUREGS];         // PPU internal registers
  u8 mem[PPU_MEM_SZ];           // PPU memory
  sprite_t oam[OAM_NUM_SPR];    // PPU Object Attribute Memory. Stores 64 sprites for the whole frame

  // PPU secondary OAM. Stores 8 sprites for the current scanline
  sprite_t sec_oam[SEC_OAM_NUM_SPR];

  // PPU scanline positions
  u64 frameno;                  // Current PPU frame
  u16 vram_addr;                // Current VRAM address
  u16 x;                        // Current X position (dot within current scanline)
  u16 y;                        // Current Y position (current dot)
  u8  scroll_x;                 // Current scroll X position
  u8  scroll_y;                 // Current scroll Y position
  bool nmi_occurred;            // Whether the PPU is currently generating NMIs or not

  u64 ticks;                    // Number of PPU cycles
  u32 palette[PALETTE_SZ];      // System-wide palette
} ppu_t;

// PPU register access
// These functions can be thought of as an interface between the CPU and PPU
u8 ppu_reg_read(nes_t *nes, ppureg_t reg);
void ppu_reg_write(nes_t *nes, ppureg_t reg, u8 val);

// Internal PPU read functions. Should only be called by internal PPU functions
u8 ppu_read(nes_t *nes, u16 addr);
void ppu_write(nes_t *nes, u16 addr, u8 val);

void ppu_init(nes_t *nes);
void ppu_tick(nes_t *nes, window_t *wnd);
void ppu_destroy(nes_t *nes);

#endif
