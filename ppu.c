#include "include/ppu.h"
#include "include/util.h"
#include "include/window.h"
#include "include/cpu.h"
#include "include/cart.h"

// Reads in a .pal file as the NES system palette
// TODO: Generate palettes
static void ppu_palette_init(nes_t *nes, char *palette_fn) {
  // Palletes are stored as 64 sets of three integers for r, g, and b intensities
  FILE *palette_f;
  color_t temp_pal[PALETTE_SZ];
  palette_f = nes_fopen(palette_fn, "rb");

  // Read in the palette
  nes_fread(temp_pal, sizeof *temp_pal, PALETTE_SZ, palette_f);
  nes_fclose(palette_f);

  // Initialize internal palette from read palette data
  for (int i = 0; i < PALETTE_SZ; i++)
    nes->ppu->palette[i] = (temp_pal[i].r << 16) | (temp_pal[i].g << 8) | temp_pal[i].b;
}

// Palette from http://www.firebrandx.com/nespalette.html
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
  // TODO: This only works for mapper 0! Add more mappers later
  memcpy(ppu->mem, nes->cart->chr_rom, nes->cart->header.chrrom_n * CHRROM_BLOCK_SZ);

  // Set up system palette
  ppu_palette_init(nes, "../palette/palette.pal");
}

static void ppu_render_pixel(nes_t *nes, window_t *wnd) {
  // Rendering a pixel consists of rendering both background and sprites
  ppu_t *ppu = nes->ppu;
  u16 cur_x = ppu->x - 1;
  u16 cur_y = ppu->y - 1;
  u8 bgr_color = 0;
  u8 spr_color = 0;
  u32 final_px = 0;
  bool spr_has_priority;

  // ******** Background rendering ********
  // Get the current nametable byte
  // Nametables are 32x30 tiles, so we have to find the right tile index
  // TODO: Add mirroring support
  u8 x_tile_idx = cur_x % 8;
  u8 y_tile_idx = cur_y % 8;
  bool show_bgr = GET_BIT(ppu->regs[PPUMASK], PPUMASK_SHOW_BGR_BIT);
  if (show_bgr) {
    u16 nt_base = 0x2000 + 0x0400 * (ppu->regs[PPUCTRL] & 3);
    u16 tile_idx = ((cur_y / 8) << 5) + (cur_x / 8);
    u8 bgr_tile = ppu_read(nes, nt_base + tile_idx);

    // bgr_tile is a tile index for the background pattern table
    u16 bgr_pt_base = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_BGR_PT_BASE_BIT) ? 0x1000 : 0;

    // Fetch the two pattern table bytes from the index we got from the nametables
    u16 bg_ptable_idx = bgr_pt_base + (bgr_tile << 4) + y_tile_idx;
    u8 bg_color_lo = ppu_read(nes, bg_ptable_idx);
    u8 bg_color_hi = ppu_read(nes, bg_ptable_idx + 8);
    u8 bg_pal_idx_lo = !!(bg_color_lo & (0x80 >> x_tile_idx)) |
                       !!(bg_color_hi & (0x80 >> x_tile_idx)) << 1;

    // ***** Attribute table fetch *****
    u16 attr_base = nt_base + 0x3C0;

    // Attribute table bytes consist of four tiles with two bits of color information each
    u16 attr_idx = attr_base + (8 * (cur_y / 32)) + (cur_x / 32);
    u8 attr_byte = ppu_read(nes, attr_idx);

    // Get the "quadrant" of the attribute byte the current PPU position is at
    u8 attr_quadr_x = (cur_x % 16) >= 8;
    u8 attr_quadr_y = (cur_y % 16) >= 8;
    u8 pal_shift = 2 * (attr_quadr_x | (attr_quadr_y << 1));
    u8 bg_pal_idx_hi = (attr_byte & (0x3 << pal_shift)) >> pal_shift;

    // Palette indices are 5 bits since there are 64 colors
    u8 bgr_pal_idx = bg_pal_idx_lo | (bg_pal_idx_hi << 2);

    // Get an index into the system palette from the upper two bits specified in the attribute table
    // and the lower two bits specified in the pattern table
    bgr_color = ppu_read(nes, PALETTE_BASE + bgr_pal_idx);
  }
  // **** End background rendering ****

  // ******** Sprite rendering ********
  // Look through each sprite in the secondary OAM and compare their X-positions to the current PPU X
  bool show_spr = GET_BIT(ppu->regs[PPUMASK], PPUMASK_SHOW_SPR_BIT);
  if (show_spr) {
    s8 active_spr_i = -1;
    for (int i = 0; i < SEC_OAM_NUM_SPR; i++) {
      sprite_t cur_spr = ppu->sec_oam[i];
      if (cur_spr.tile_idx) {
        if (cur_x >= cur_spr.x_pos && cur_x < cur_spr.x_pos + 8) {
          active_spr_i = i;
          break;
        }
      }
    }

    if (active_spr_i >= 0) {
      sprite_t active_spr = ppu->sec_oam[active_spr_i];

      u16 spr_pt_base = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_SPR_PT_BASE_BIT) ? 0x1000 : 0;
      u16 spr_ptable_idx = spr_pt_base + (active_spr.tile_idx << 4) + y_tile_idx;
      u8 spr_color_lo = ppu_read(nes, spr_ptable_idx);
      u8 spr_color_hi = ppu_read(nes, spr_ptable_idx + 8);
      u8 spr_pal_idx_lo = !!(spr_color_lo & (0x80 >> x_tile_idx)) |
          !!(spr_color_hi & (0x80 >> x_tile_idx)) << 1;

      u8 spr_pal_idx_hi = active_spr.attr & 3;
      u8 spr_pal_idx = spr_pal_idx_lo | (spr_pal_idx_hi << 2) | 0x10;

      spr_color = ppu_read(nes, PALETTE_BASE + spr_pal_idx);
      spr_has_priority = !GET_BIT(active_spr.attr, SPRITE_ATTR_PRIORITY_BIT);
//      if (active_spr.x_pos && active_spr.y_pos) {
//        printf("*****************************\n");
//        printf("render_pixel: sec_oam_i=%d spr pix x=%d y=%d tile=%02X attr=%02X\n",
//               active_spr_i, active_spr.x_pos, active_spr.y_pos, active_spr.tile_idx,
//               active_spr.attr);
//      }
    }
  }

  u8 uni_bgr_color = ppu_read(nes, PALETTE_BASE);
  if (!bgr_color && !spr_color)
    final_px = ppu->palette[uni_bgr_color];
  else if (bgr_color && !spr_color)
    final_px = ppu->palette[bgr_color];
  else
    final_px = spr_has_priority ? ppu->palette[spr_color] : ppu->palette[bgr_color];

  wnd->pixels[cur_y][cur_x] = final_px;
}

// Does a linear search through OAM to find up to 8 sprites to render for the current scanline
static void ppu_fill_sec_oam(nes_t *nes) {
  // Clear the list of sprites to draw (secondary OAM)
  // Zeroing out the sec_oam array has the benefit of setting the sprite struct valid flags to false
  // So we know which indices of sec_oam are filled
  ppu_t *ppu = nes->ppu;
  assert(SEC_OAM_NUM_SPR * sizeof *ppu->sec_oam == 32);
  memset(ppu->sec_oam, 0, SEC_OAM_NUM_SPR * sizeof *ppu->sec_oam);

  // Search through OAM to find sprites that are in range
  const u8 HEIGHT_OFFSET = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_SPRITE_SZ_BIT) ? 16 : 8;
  u8 sec_oam_i = 0;
  u16 cur_y = ppu->y - 1;
  for (u8 i = 0; i < 64 && sec_oam_i < 8; i++) {
    sprite_t cur_spr = ppu->oam[i];

    // Is the sprite in range, and does it already exist in the secondary OAM?
    if (cur_spr.tile_idx) {
      if (cur_y >= cur_spr.y_pos && cur_y < cur_spr.y_pos + HEIGHT_OFFSET) {
        ppu->sec_oam[sec_oam_i++] = cur_spr;
      }
    }
  }
}

// Renders a single pixel at the current PPU position
// Also controls timing and issues NMIs to the CPU on VBlank
void ppu_tick(nes_t *nes, window_t *wnd) {
  // TODO: Even and odd frames have slightly different behavior with idle cycles
  ppu_t *ppu = nes->ppu;

  if (ppu->y == 0) {
    // Pre-render scanline
    // Clear NMI flag on the second dot of the pre-render scanline
    if (ppu->x == 1) {
      ppu->frameno++;
      ppu->nmi_occurred = false;
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_VBLANK_BIT, 0);
    } else if (ppu->x >= 257 && ppu->x <= 320) {
      ppu->regs[OAMADDR] = 0x00;
    }

  } else if (ppu->y >= 1 && ppu->y <= 240) {
    // Visible scanlines
    // Cycle 0 on all visible scanlines is an idle cycle, but we will use it to get the
    // sprites that should be rendered on this scanline
    if (ppu->x == 0)
      ppu_fill_sec_oam(nes);

    if (ppu->x >= 1 && ppu->x <= 256) {
      // Render a visible pixel to the framebuffer
      ppu_render_pixel(nes, wnd);
    } else if (ppu->x >= 257 && ppu->x <= 320) {
      // Set OAMADDR to 0
      ppu->regs[OAMADDR] = 0x00;
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
  ppu_t *ppu = nes->ppu;

  u8 vram_inc, retval;
  switch (reg) {
    case PPUSTATUS:
      // Clear vblank bit every PPUSTATUS read
      retval = ppu->regs[PPUSTATUS];
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_VBLANK_BIT, 0);
      return retval;
    case PPUDATA:
      // Increment VRAM addr by value specified in bit 2 of PPUCTRL
      vram_inc = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_VRAM_INC_BIT) ? 32 : 1;

      u16 temp_addr = ppu->vram_addr;
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
  ppu_t *ppu = nes->ppu;
  u8 vram_inc;

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
      ppu->regs[PPUCTRL] = val;

      // TODO: Support 8x16 sprites (high priority)
      if (GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_SPRITE_SZ_BIT))
        printf("ppu_reg_write: WARNING: 8x16 sprites are not supported yet.\n");
      break;
    case PPUMASK:
      // TODO: Add color emphasizing support and show/hide background support
      ppu->regs[PPUMASK] = val;
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
      ppu->regs[OAMADDR] = val;
      break;
    default:
      printf("ppu_reg_write: cannot write to ppu reg $%02d\n", reg);
      exit(EXIT_FAILURE);
  }
}

u8 ppu_read(nes_t *nes, u16 addr) {
  // $3F20-$3FFF are mirrors of $3F00-$3F1F (palette)
  // $3000-$3EFF are mirrors of $2000-$2EFF (nametables)
  // Convert the address by clearing the 12th bit (0x1000)
  if (addr >= 0x3000 && addr <= 0x3EFF)
    addr &= ~0x1000;
  else if (addr >= 0x3F20 && addr <= 0x3FFF)
    addr = PALETTE_BASE + addr % 0x20;

  return nes->ppu->mem[addr];
}

void ppu_write(nes_t *nes, u16 addr, u8 val) {
  if (addr >= 0x3000 && addr <= 0x3EFF)
    addr &= ~(1 << 12);
  else if (addr >= 0x3F20 && addr <= 0x3FFF)
    addr = PALETTE_BASE + addr % 0x20;

  nes->ppu->mem[addr] = val;
}

void ppu_destroy(nes_t *nes) {

}

