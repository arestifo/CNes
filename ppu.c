#include "include/ppu.h"
#include "include/util.h"
#include "include/window.h"
#include "include/cpu.h"
#include "include/cart.h"

void ppu_init(nes_t *nes) {
  ppu_t *ppu;

  ppu = nes->ppu;
  ppu->x = 0;
  ppu->y = 0;
  ppu->scroll_x = 0;
  ppu->scroll_y = 0;
  ppu->ticks = 0;
  ppu->frameno = 0;
  ppu->vram_addr = 0x0000;

  // Set up PPU address space
  // Load CHR ROM (pattern tables) into PPU $0000-$1FFF
  // TODO: This only works for mapper 0! Add more mapper later
  memcpy(ppu->mem, nes->cart->chr_rom, nes->cart->header.chrrom_n * CHRROM_BLOCK_SZ);
}

static void render_pixel(nes_t *nes, window_t *wnd) {
  // Rendering a pixel consists of rendering both background and sprites
  // TODO: Render sprites
  ppu_t *ppu;
  u32 cur_x, cur_y, final_px;
  u16 nt_base, nt_idx, bgr_pt_base, pt_byte_idx;
  u8 nt_byte, pt_byte_lo, pt_byte_hi;

  ppu = nes->ppu;
  cur_x = ppu->x - 1;
  cur_y = ppu->y - 1;

  // Background rendering
  // Get the current nametable byte
  // Nametables are 32x30 tiles, so we have to find the right tile index
  // TODO: Add mirroring support
  nt_base = 0x2000 + 0x0400 * (ppu->regs[PPUCTRL] & 3);
  nt_idx = ((cur_y >> 3) * 32) + (cur_x >> 3);
  nt_byte = ppu_read(nes, nt_base + nt_idx);

  // nt_byte is a tile index for the background pattern table
  bgr_pt_base = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_BGR_PT_BASE_BIT) ? 0x1000 : 0;

  pt_byte_idx = bgr_pt_base + (nt_byte * 16) + cur_y % 8;
  pt_byte_lo = ppu_read(nes, pt_byte_idx);
  pt_byte_hi = ppu_read(nes, pt_byte_idx + 8);

  // TODO: Add colors
//  final_px = pt_byte_lo | (pt_byte_hi << 8) ? 0xFFFFFFFF : 0;
  final_px = (pt_byte_lo & (1 << (cur_x % 8))) | (pt_byte_hi & (1 << (cur_x % 8))) ? 0xFFFFFFFF : 0;
  wnd->pixels[cur_y][cur_x] = final_px;
//    wnd->pixels[cur_y][cur_x] = 0x000000FF;
}

// Renders a single pixel at the current PPU position
// Also controls timing and issues NMIs to the CPU whe
void ppu_tick(nes_t *nes, window_t *wnd) {
  // TODO: Even and odd frames have slightly different behavior with idle cycles
  static bool even_frame = true;
  ppu_t *ppu;

  ppu = nes->ppu;
  if (ppu->y == 0) {
    // Pre-render scanline
    // Clear NMI flag on the second dot of the pre-render scanline
    if (ppu->x == 1) {
      ppu->frameno++;
      ppu->nmi_occurred = false;
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_VBLANK_BIT, 0);
    }

  } else if (ppu->y >= 1 && ppu->y <= 240) {
    // Visible scanlines
    // Cycle 0 on all visible scanlines is an idle cycle
    if (ppu->x >= 1 && ppu->x <= 256) {
      // Render a visible pixel to the framebuffer
      render_pixel(nes, wnd);
    }
  } else if (ppu->y == 241) {
    // Post-render scanline
    // All visible scanlines have been rendered, frame is ready to be displayed
    wnd->frame_ready = true;
  } else if (ppu->y >= 242 && ppu->y <= 261) {
    // VBlank scanlines
    // Issue NMI at the second dot (x=1) of the first VBlank scanline
    if (ppu->y == 242 && ppu->x == 1) {
      ppu->nmi_occurred = true;
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_VBLANK_BIT, 1);

      if (GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_NMI_ENABLE_BIT))
        nes->cpu->nmi_pending = true;
    }
  }

  // Increment PPU position
  ppu->ticks++;
  ppu->x = ppu->ticks % DOTS_PER_SCANLINE;
  ppu->y = (ppu->ticks / DOTS_PER_SCANLINE) % NUM_SCANLINES;
//  printf("ppu_tick: x=%d y=%d cyc=%llu\n", ppu->x, ppu->y, ppu->ticks);
}

u8 ppu_reg_read(nes_t *nes, ppureg_t reg) {
  ppu_t *ppu;
  u16 temp_addr;
  u8 vram_inc, retval;

  ppu = nes->ppu;
  switch (reg) {
    case PPUSTATUS:
      // Clear vblank bit every PPUSTATUS read
      retval = ppu->regs[PPUSTATUS];
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_VBLANK_BIT, 0);
      return retval;
    case OAMDATA:
      // TODO
    case PPUDATA:
      // Increment VRAM addr by value specified in bit 2 of PPUCTRL
      vram_inc = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_VRAM_INC_BIT) ? 32 : 1;

      temp_addr = ppu->vram_addr;
      ppu->vram_addr += vram_inc;

      // TODO: Implement PPUDATA read buffer (delay)
      return ppu_read(nes, temp_addr);
    default:
      printf("ppu_reg_read: cannot read from ppu reg $%02d\n", reg);
      exit(EXIT_FAILURE);
  }
}

void ppu_reg_write(nes_t *nes, ppureg_t reg, u8 val) {
  // These variables check what "phase" of the write-twice registers (PPUADDR & PPUSCROLL)
  // the PPU is in (which is why they are static -- we want persistent state)
  static bool ppuaddr_written = false;
  static bool ppuscroll_written = false;
  ppu_t *ppu;
  u8 vram_inc;

  ppu = nes->ppu;
  switch (reg) {
    case PPUADDR:
      // The first PPUADDR write is the high byte of VRAM to be accessed, and the second byte
      // is the low byte
      // Clear the vram address if we're writing a new one in
      if (!ppuaddr_written)
        ppu->vram_addr = 0x0000;
      ppu->vram_addr |= ppuaddr_written ? val : (val << 8);
      ppuaddr_written ^= true;  // Toggle ppuaddr_written
      break;
    case PPUCTRL:
      // TODO?
      ppu->regs[PPUCTRL] = val;
      break;
    case PPUMASK:
      // TODO: Add color emphasizing support and show/hide background support
      break;
    case PPUSCROLL:
      if (ppuscroll_written)
        ppu->scroll_y = val;
      else
        ppu->scroll_x = val;
      ppuscroll_written ^= true;
      break;
    case PPUDATA:
      vram_inc = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_VRAM_INC_BIT) ? 32 : 1;
      ppu_write(nes, ppu->vram_addr, val);

      ppu->vram_addr += vram_inc;
      break;
    case OAMADDR:
      // TODO: Implement PPU OAMADDR and OAMDATA registers. Most games don't use them,
      // TODO: so I'm not gonna bother right now
      break;
    default:
      printf("ppu_reg_write: cannot write to ppu reg $%02d\n", reg);
      exit(EXIT_FAILURE);
  }
}

u8 ppu_read(nes_t *nes, u16 addr) {
  return nes->ppu->mem[addr];
}

void ppu_write(nes_t *nes, u16 addr, u8 val) {
  nes->ppu->mem[addr] = val;
}

void ppu_destroy(nes_t *nes) {

}

