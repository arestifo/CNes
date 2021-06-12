#include "include/ppu.h"
#include "include/util.h"
#include "include/window.h"
#include "include/cpu.h"
#include "include/cart.h"

static bool write_toggle = false;
static u16 ppu_decode_addr(nes_t *nes, u16 addr);

// Reads in a .pal file as the NES system palette
// TODO: Generate palettes (low priority)
static void ppu_palette_init(nes_t *nes, char *palette_fn) {
  // Palletes are stored as 64 sets of three integers for r, g, and b intensities
  FILE *palette_f;
  SDL_PixelFormat *pixel_fmt;
  color_t pal[PALETTE_SZ];

  // Read in the palette
  palette_f = nes_fopen(palette_fn, "rb");
  nes_fread(pal, sizeof *pal, PALETTE_SZ, palette_f);
  nes_fclose(palette_f);

  // Initialize internal palette from read palette data
  pixel_fmt = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB32);
  for (int i = 0; i < PALETTE_SZ; i++)
    nes->ppu->palette[i] = SDL_MapRGBA(pixel_fmt, pal[i].r, pal[i].g, pal[i].b, 0xFF);

  SDL_FreeFormat(pixel_fmt);
}

bool ppu_rendering_enabled(ppu_t *ppu) {
  return GET_BIT(ppu->regs[PPUMASK], PPUMASK_SHOW_BGR_BIT) &&
         GET_BIT(ppu->regs[PPUMASK], PPUMASK_SHOW_SPR_BIT);
}

// Palette from http://www.firebrandx.com/nespalette.html
void ppu_init(nes_t *nes) {
  ppu_t *ppu;

  ppu = nes->ppu;
  ppu->dot = 0;
  ppu->scanline = 0;
  ppu->fine_x = 0;
  ppu->fine_y = 0;
  ppu->ticks = 0;
  ppu->frameno = 0;
  ppu->vram_addr = 0x0000;

  // Set up PPU address space
  // Load CHR ROM (pattern tables) into PPU $0000-$1FFF
  // TODO: This only works for mapper 0! Add more mappers later
  memcpy(ppu->mem, nes->cart->chr_rom, nes->cart->header.chrrom_n * CHRROM_BLOCK_SZ);

  // Set mirroring type from cart (this will get overridden by mappers that control mirroring)
  // This only applies to static-mirroring mappers
  ppu->mirroring = nes->cart->header.flags6 & 1 ? MT_VERTICAL : MT_HORIZONTAL;

  // Set up system palette
  ppu_palette_init(nes, "../palette/palette.pal");
}

// Palette mirroring
static inline u32 ppu_get_palette_color(nes_t *nes, u8 color_i) {
  // $3F10, $3F14, $3F18, $3F1C are mirrors of $3F00, $3F04, $3F08, $3F0C
  u8 adj_i = color_i;
  if (color_i >= 0x10) {
    if ((color_i & 3) == 0)
      adj_i &= ~0x10;  // Clear bit 4, mirroring the address down by 0x10
  }

  return nes->ppu->palette[adj_i];
}

// Info from https://wiki.nesdev.com/w/index.php/PPU_scrolling
static inline void ppu_increment_scroll_y(ppu_t *ppu) {
  if (ppu_rendering_enabled(ppu)) {
    if (ppu->fine_y < 7) {
      ppu->fine_y++;
    } else {
      ppu->fine_y = 0;

      // Increment coarse y
      u8 coarse_y = (ppu->vram_addr & (0x1F << 5)) >> 5;
      if (coarse_y == 29) {
        coarse_y = 0;
        ppu->vram_addr ^= 0x800;
      } else if (coarse_y == 31) {
        coarse_y = 0;
      } else {
        coarse_y++;
      }

      // Write coarse y back into the VRAM addr
      ppu->vram_addr = (ppu->vram_addr & ~(0x1F << 5)) | (coarse_y << 5);
    }
  }
}

static inline void ppu_increment_scroll_x(ppu_t *ppu) {
  if (ppu_rendering_enabled(ppu)) {
    if (ppu->fine_x < 7) {
      ppu->fine_x++;
    } else {
      // Wrap around fine x counter
      ppu->fine_x = 0;

      // Increment coarse x counter
      if ((ppu->vram_addr & 0x1F) == 31) {
        // Coarse x overflow wraps around into the next nametable
        ppu->vram_addr &= ~0x1F;
        ppu->vram_addr ^= 0x400;  // Switch horizontal nametable
      } else {
        ppu->vram_addr++;
      }
    }
  }
}

// Renders a single pixel and returns it as an ARGB32 value. Runs during every PPU cycle.
// pixels is an array of ARGB32 values representing the SDL framebuffer we are drawing to
// Information from https://wiki.nesdev.com/w/index.php/PPU_scrolling
static u32 ppu_render_pixel(nes_t *nes) {
  // Rendering a pixel consists of rendering both background and sprites
  ppu_t *ppu = nes->ppu;
  u16 cur_x = ppu->dot - 1;  // [0,255]
  u16 cur_y = ppu->scanline - 1;  // [0,239]

  // Input to pixel priority multiplexer
  u8 bgr_color_idx = 0;
  u8 spr_color_idx = 0;

  // Sprite rendering metadata
  sprite_t active_spr;
  bool spr_has_priority;

  // **************** Background rendering ****************
  bool show_bgr = GET_BIT(ppu->regs[PPUMASK], PPUMASK_SHOW_BGR_BIT);
  if (show_bgr) {
    // **** Get pattern table and attribute values ****
    // Create an index into the pattern table from value at nametable idx
    u8 coarse_x = ppu->vram_addr & 0x1F;
    u8 coarse_y = (ppu->vram_addr & (0x1F << 5)) >> 5;
//    printf("coarse_x=%d coarse_y=%d\n", coarse_x, coarse_y);
//    u8 cur_nt = (ppu->vram_addr & (3 << 10)) >> 10;
    u8 cur_nt = ppu->regs[PPUCTRL] & 3;
//    u16 tile_idx = 0x2000 | (ppu->vram_addr & 0x0FFF);
    u16 tile_idx = 0x2000 + cur_nt * 1024 + coarse_y * 32 + coarse_x;

    // Get the pattern table index from the nametable index
    u8 pt_idx = ppu_read(nes, tile_idx);

    // **** Read two bytes from the pattern table ****
    u16 pt_base = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_BGR_PT_BASE_BIT) ? 0x1000 : 0;

    // Each tile in the pattern table has 16 bytes: 8 for the lower plane (bit 0 of color),
    // 8 for the upper plane (bit 1 of color)
    u16 pt_addr = pt_base + pt_idx * 16 + ppu->fine_y;

    u8 pt_val_lo = ppu_read(nes, pt_addr);
    u8 pt_val_hi = ppu_read(nes, pt_addr + 8);

    // **** Extract two pattern table color bits ****
    // We're ANDing with a mask > 1, but we want bit_lo and bit_hi to be in {0..1}
    // The !! "operator" returns 1 if its input > 0 and 0 if its input is 0. It is essentially
    // casts a number to a bool
    u8 pixel_mask = 0x80 >> ppu->fine_x;
    u8 pt_color_bits = !!(pt_val_lo & pixel_mask) | (!!(pt_val_hi & pixel_mask) << 1);

    // **** Get attribute table byte ****
    u16 attrib_base = 0x3C0;  // 0b00 1111 000 000;

    // Upper three bits of coarse dot, upper three bits of coarse scanline, attribute offset, nametable
    u8 attrib_x = (coarse_x & 0x1C) >> 2;
    u8 attrib_y = (coarse_y & 0x1C) >> 2;
//    u16 attrib_addr = 0x23C0 | (ppu->vram_addr & 0x0C00) | ((ppu->vram_addr >> 4) & 0x38) | ((ppu->vram_addr >> 2) & 0x07);
    u16 attrib_addr = attrib_x | (attrib_y << 3) | attrib_base | (cur_nt << 10);
    u8 attrib_val = ppu_read(nes, 0x2000 + attrib_addr);

    // **** Extract attribute table bits ****
    // Each attribute table byte controls four sub-tiles of two bytes each
    // Find the dot and scanline "quadrants" of the current tile
    u8 attrib_quadx = coarse_x % 4 >= 2;
    u8 attrib_quady = coarse_y % 4 >= 2;

    // Get the two attribute bits (colors) from the calculated quadrants
    u8 attrib_idx_shift = 2 * (attrib_quadx | (attrib_quady << 1));
    u8 attrib_color_bits = (attrib_val & (3 << attrib_idx_shift)) >> attrib_idx_shift;

    // Calculate an index into the background palette from the palette and attribute color bits
    u16 palette_idx = PALETTE_BASE + (pt_color_bits | (attrib_color_bits << 2));

    // Get an index into the system palette from the background palette index
    bgr_color_idx = palette_idx & 3 ? ppu_read(nes, palette_idx) : 0;
  }

  // ************** End background rendering **************

  // ****************** Sprite rendering ******************
  // Look through each sprite in the secondary OAM and compare their X-positions to the current PPU X
  bool show_spr = GET_BIT(ppu->regs[PPUMASK], PPUMASK_SHOW_SPR_BIT);
  if (show_spr) {
    // Get the sprite to be shown from the secondary OAM.
    // Secondary OAM is initialized at the beginning of every scanline and contains the sprites to
    // be shown on that scanline
    // We count down from 7..0 because that is effectively how the NES does it
    s8 active_spr_i = -1;
    for (int i = SEC_OAM_NUM_SPR - 1; i >= 0; i--) {
      sprite_t cur_spr = ppu->sec_oam[i];
      if (cur_spr.data.tile_idx) {
        if (cur_x >= cur_spr.data.x_pos && cur_x < cur_spr.data.x_pos + 8)
          active_spr_i = i;
      }
    }

    if (active_spr_i >= 0) {
      // **** Get sprite characteristics ****
      active_spr = ppu->sec_oam[active_spr_i];
      u8 spr_fine_y = cur_y - active_spr.data.y_pos;
      u8 spr_fine_x = cur_x - active_spr.data.x_pos;
      bool flip_h = GET_BIT(active_spr.data.attr, SPRITE_ATTR_FLIPH_BIT);
      bool flip_v = GET_BIT(active_spr.data.attr, SPRITE_ATTR_FLIPV_BIT);
      u8 pixel_mask = flip_h ? 1 << spr_fine_x : 0x80 >> spr_fine_x;

      // **** Get two pattern table sprite bytes ****
      u16 pt_base = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_SPR_PT_BASE_BIT) ? 0x1000 : 0;
      u16 pt_addr = pt_base + (active_spr.data.tile_idx * 16) + spr_fine_y;

      u8 pt_val_lo = ppu_read(nes, pt_addr);
      u8 pt_val_hi = ppu_read(nes, pt_addr + 8);

      // **** Get the color bits: two from PT, two from sprite attributes ****
      u8 pt_color_bits = !!(pt_val_lo & pixel_mask) | !!(pt_val_hi & pixel_mask) << 1;
      u8 attrib_color_bits = active_spr.data.attr & 3;
      u16 palette_idx = PALETTE_BASE + (pt_color_bits | (attrib_color_bits << 2) | 0x10);

      // **** Get the final sprite color ****
      spr_color_idx = palette_idx & 3 ? ppu_read(nes, palette_idx) : 0;
      spr_has_priority = !GET_BIT(active_spr.data.attr, SPRITE_ATTR_PRIORITY_BIT);
    }
  }
  // **************** End sprite rendering ****************

  // **************** Pixel multiplexer/display ****************
  u32 final_pixel;
  u8 uni_bgr_color_idx = ppu_read(nes, PALETTE_BASE);
  if (!bgr_color_idx && !spr_color_idx)
    final_pixel = ppu_get_palette_color(nes, uni_bgr_color_idx);
  else if (!bgr_color_idx)
    final_pixel = ppu_get_palette_color(nes, spr_color_idx);
  else if (!spr_color_idx)
    final_pixel = ppu_get_palette_color(nes, bgr_color_idx);
  else {
    // Sprite pixel and background pixel are both opaque; a precondition for spite zero hit
    // detection. Check that here
    // TODO: Don't trigger sprite zero hit when the left-side clipping window is enabled
    if (active_spr.sprite0)
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_ZEROHIT_BIT, 1);
    final_pixel = spr_has_priority ? ppu_get_palette_color(nes, spr_color_idx)
                                   : ppu_get_palette_color(nes, bgr_color_idx);
  }

  return final_pixel;
}

// Does a linear search through OAM to find up to 8 sprites to render for the current scanline
static void ppu_fill_sec_oam(nes_t *nes) {
  // Clear the list of sprites to draw (secondary OAM)
  // Zeroing out the sec_oam array has the benefit of setting the sprite struct valid flags to false
  // So we know which indices of sec_oam are filled
  ppu_t *ppu = nes->ppu;
  memset(ppu->sec_oam, 0, SEC_OAM_NUM_SPR * sizeof *ppu->sec_oam);

  // Search through OAM to find sprites that are in range
  const u8 HEIGHT_OFFSET = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_SPRITE_SZ_BIT) ? 16 : 8;
  u8 sec_oam_i = 0;
  u16 cur_y = ppu->scanline - 1;
  for (u8 i = 0; i < 64 && sec_oam_i < 8; i++) {
    sprite_t cur_spr = ppu->oam[i];

    // Is the sprite in range, and does it already exist in the secondary OAM?
    if (cur_spr.data.tile_idx) {
      if (cur_y >= cur_spr.data.y_pos && cur_y < cur_spr.data.y_pos + HEIGHT_OFFSET) {
        ppu->sec_oam[sec_oam_i++] = cur_spr;
      }
    }
  }
}

// Renders a single pixel at the current PPU position
// Also controls timing and issues NMIs to the CPU on VBlank
void ppu_tick(nes_t *nes, window_t *wnd, void *pixels) {
  // TODO: Even and odd frames have slightly different behavior with idle cycles
  ppu_t *ppu = nes->ppu;

  u16 cur_x = ppu->dot - 1;
  u16 cur_y = ppu->scanline - 1;
  if (ppu->scanline == 0) {
    // Pre-render scanline
    // Clear NMI flag on the second dot of the pre-render scanline
    if (ppu->dot == 1) {
      // All visible scanlines have been rendered, frame is ready to be displayed
      wnd->frame_ready = true;
      ppu->frameno++;
      ppu->nmi_occurred = false;
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_VBLANK_BIT, 0);
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_ZEROHIT_BIT, 0);
    } else if (ppu->dot >= 257 && ppu->dot <= 320) {
      ppu->regs[OAMADDR] = 0x00;
    }
  } else if (ppu->scanline >= 1 && ppu->scanline <= 240) {
    // Visible scanlines
    // Cycle 0 on all visible scanlines is an idle cycle, but we will use it to get the
    // sprites that should be rendered on this scanline
    if (ppu->dot == 0) {
//      ppu->vram_addr &= ~0x1F;
      ppu_fill_sec_oam(nes);
    } else if (ppu->dot >= 1 && ppu->dot <= 256) {
      // Render a visible pixel to the framebuffer
      u32 pixel = ppu_render_pixel(nes);
      ((u32 *) pixels)[(cur_y * WINDOW_W) + cur_x] = pixel;

      // Increment fine/coarse y at the end of every scanline
//      printf("fine_x=%d fine_y=%d\n", ppu->fine_x, ppu->fine_y);
      ppu_increment_scroll_x(ppu);
      if (ppu->dot == 256)
        ppu_increment_scroll_y(ppu);

    } else if (ppu->dot >= 258 && ppu->dot <= 320) {
      // Set OAMADDR to 0
      ppu->regs[OAMADDR] = 0x00;
    }
  } else if (ppu->scanline == 241) {
    // Post-render scanline

  } else if (ppu->scanline >= 242 && ppu->scanline <= 261) {
    // VBlank scanlines
    // Issue NMI at the second dot (dot=1) of the first VBlank scanline
    if (ppu->scanline == 242 && ppu->dot == 1) {
      ppu->nmi_occurred = true;
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_VBLANK_BIT, 1);

      if (GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_NMI_ENABLE_BIT))
        nes->cpu->nmi_pending = true;
    }
  }

  // Increment PPU position
  ppu->ticks++;
  ppu->dot = ppu->ticks % DOTS_PER_SCANLINE;
  ppu->scanline = (ppu->ticks / DOTS_PER_SCANLINE) % NUM_SCANLINES;
}

u8 ppu_reg_read(nes_t *nes, ppureg_t reg) {
  ppu_t *ppu = nes->ppu;

  u8 vram_inc, retval;
  switch (reg) {
    case PPUSTATUS:
      // Clear vblank bit every PPUSTATUS read
      retval = ppu->regs[PPUSTATUS];
      write_toggle = false;
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_VBLANK_BIT, 0);
      return retval;
    case PPUDATA:
      // Increment VRAM addr by value specified in bit 2 of PPUCTRL
      vram_inc = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_VRAM_INC_BIT) ? 32 : 1;
      static u8 read_buf;

      u16 temp_addr = ppu->vram_addr;
      ppu->vram_addr += vram_inc;

      // PPUDATA read buffer
      retval = read_buf;
      read_buf = ppu_read(nes, temp_addr);

      return retval;
    default:
      printf("ppu_reg_read: cannot read from ppu reg $%02d\n", reg);
      exit(EXIT_FAILURE);
  }
}

void ppu_reg_write(nes_t *nes, ppureg_t reg, u8 val) {
  // These variables check what "phase" of the write-twice registers (PPUADDR & PPUSCROLL)
  // the PPU is in (which is why they are static -- we want persistent state)
  ppu_t *ppu = nes->ppu;
  u8 vram_inc;
  static u16 temp_vram_addr = 0;

  switch (reg) {
    case PPUSCROLL:  // $2005
      if (!write_toggle) {
        // Write the fine scroll x (coarse is calculated in render_pixel)
        ppu->fine_x = val % 8;
      } else {
        // Write the fine scroll y
        ppu->fine_y = val % 8;
      }
      write_toggle ^= true;

//      static int i = 0;
//      printf("ppu_write: i=%d PPUSCROLL write $%02X\n", i++, val);

      break;
    case PPUADDR:  // $2006
      // The first PPUADDR write is the high byte of VRAM to be accessed, and the second byte
      // is the low byte
      // Clear the vram address if we're writing a new one in
      temp_vram_addr &= write_toggle ? ~0x00FF : ~0xFF00;
      temp_vram_addr |= write_toggle ? val : (val << 8);

      if (write_toggle)
        ppu->vram_addr = temp_vram_addr;

      write_toggle ^= true;  // Toggle ppuaddr_written
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

static u16 ppu_decode_addr(nes_t *nes, u16 addr) {
  ppu_t *ppu = nes->ppu;

  // $3F20-$3FFF are mirrors of $3F00-$3F1F (palette)
  // $3000-$3EFF are mirrors of $2000-$2EFF (nametables)

  // Nametable mirrored addresses
  if (addr >= 0x3000 && addr <= 0x3EFF) {
    // Convert the address by clearing the 12th bit (0x1000)
    addr &= ~0x1000;
  } else if (addr >= 0x3F10 && addr <= 0x3F1F) {
    if ((addr & 3) == 0)
      addr &= ~0x10;
  } else if (addr >= 0x3F20 && addr <= 0x3FFF) {
    addr = ppu_decode_addr(nes, PALETTE_BASE + addr % 0x20);
  }

  // PPU mirroring
  switch (ppu->mirroring) {
    case MT_HORIZONTAL:
      // Clear bit 10 to mirror down
      if ((addr >= 0x2400 && addr <= 0x27FF) || (addr >= 0x2C00 && addr <= 0x2FFF))
        addr &= ~0x400;
      break;
    case MT_VERTICAL:
      // Clear bit 11 to mirror down
      if ((addr >= 0x2800 && addr <= 0x2BFF) || (addr >= 0x2C00 && addr <= 0x2FFF))
        addr &= ~0x800;
      break;
    default:
      printf("ppu_decode_addr: invalid mirroring!\n");
      exit(EXIT_FAILURE);
  }

  return addr;
}

inline u8 ppu_read(nes_t *nes, u16 addr) {
  u16 decoded_addr = ppu_decode_addr(nes, addr);

  return nes->ppu->mem[decoded_addr];
}

inline void ppu_write(nes_t *nes, u16 addr, u8 val) {
  u16 decoded_addr = ppu_decode_addr(nes, addr);

  nes->ppu->mem[decoded_addr] = val;
}

void ppu_destroy(nes_t *nes) {
  // TODO?
}

