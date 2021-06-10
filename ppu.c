#include "include/ppu.h"
#include "include/util.h"
#include "include/window.h"
#include "include/cpu.h"
#include "include/cart.h"

static bool write_toggle = false;

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

  // Set mirroring type from cart (this will get overridden by mappers that control mirroring)
  // This only applies to static-mirroring mappers
  ppu->mirroring = nes->cart->header.flags6 & 1 ? MT_VERTICAL : MT_HORIZONTAL;

  // Set up system palette
  ppu_palette_init(nes, "../palette/palette.pal");
}

// Palette mirroring
static inline u32 get_palette_color(nes_t *nes, u8 color_i) {
  // $3F10, $3F14, $3F18, $3F1C are mirrors of $3F00, $3F04, $3F08, $3F0C
  u8 adj_i = color_i;
  if (color_i >= 0x10) {
    if ((color_i & 3) == 0)
      adj_i &= ~0x10;  // Clear bit 4, mirroring the address down by 0x10
  }

  return nes->ppu->palette[adj_i];
}

// Renders a single pixel. Run during every PPU visible pixel cycle.
// pixels is an array of ARGB32 values representing the SDL framebuffer we are drawing to
// Information from https://wiki.nesdev.com/w/index.php/PPU_scrolling
static void ppu_render_pixel(nes_t *nes, void *pixels) {
  // Rendering a pixel consists of rendering both background and sprites
  ppu_t *ppu = nes->ppu;
  u16 cur_x = ppu->x - 1;  // [0,255]
  u16 cur_y = ppu->y - 1;  // [0,239]

  // Input to pixel priority multiplexer
  u8 bgr_color_idx = 0;
  u8 spr_color_idx = 0;

  // **************** Background rendering ****************
  bool show_bgr = GET_BIT(ppu->regs[PPUMASK], PPUMASK_SHOW_BGR_BIT);
  u8 coarse_x = cur_x / 8;  // tile x index
  u8 coarse_y = cur_y / 8;  // tile y index
  u8 fine_x = cur_x % 8;    // fine x index within the tile
  u8 fine_y = cur_y % 8;    // fine y index within the tile
  u8 cur_nt = ppu->regs[PPUCTRL] & 3;  // TODO: Remember to increment this properly

  if (show_bgr) {
    // **** Get pattern table and attribute values ****
    // Create an index into the pattern table from value at nametable idx
    ppu->vram_addr = 0x2000 + ((coarse_x & 0x1F) | ((coarse_y & 0x1F) << 5) | (cur_nt << 10));

    // Get the pattern table index from the nametable index
    u8 tile_idx = ppu_read(nes, ppu->vram_addr);
    u8 tile_row = tile_idx / 16;
    u8 tile_col = tile_idx % 16;

    // **** Read two bytes from the pattern table ****
    u16 pt_base = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_BGR_PT_BASE_BIT) ? 0x1000 : 0;

    // Each tile in the pattern table has 16 bytes: 8 for the lower plane (bit 0 of color),
    // 8 for the upper plane (bit 1 of color)
    u16 pt_addr = pt_base + tile_idx * 16 + fine_y;
    u8 pt_val_lo = ppu_read(nes, pt_addr);
    u8 pt_val_hi = ppu_read(nes, pt_addr + 8);

    // **** Extract two pattern table color bits ****
    // We're ANDing with a mask > 1, but we want bit_lo and bit_hi to be in {0..1}
    // The !! "operator" returns 1 if its input > 0 and 0 if its input is 0. It is essentially
    // casts a number to a bool
    u8 pt_bit_lo = !!(pt_val_lo & (0x80 >> fine_x));
    u8 pt_bit_hi = !!(pt_val_hi & (0x80 >> fine_x));
    u8 pt_color_bits = pt_bit_lo | (pt_bit_hi << 1);

    // **** Get attribute table byte ****
    u16 attrib_base = 0x3C0;  // 0b00 1111 000 000;

    // Upper three bits of coarse x, upper three bits of coarse y, attribute offset, nametable
    u16 attrib_addr = ((coarse_x & 0x1C) >> 2) | (((coarse_y & 0x1C) >> 2) << 3) | attrib_base | (cur_nt << 10);
    u8 attrib_val = ppu_read(nes, 0x2000 + attrib_addr);

    // **** Extract attribute table bits ****
    // Each attribute table byte controls four sub-tiles of two bytes each
    // Find the x and y "quadrants" of the current tile
    u8 attrib_quadx = tile_col & 1;
    u8 attrib_quady = tile_row & 1;

    // Get the two attribute bits (colors) from the calculated quadrants
    u8 attrib_idx_shift = 2 * (attrib_quadx | (attrib_quady << 1));
    u8 attrib_color_bits = (attrib_val & (3 << attrib_idx_shift)) >> attrib_idx_shift;

    // Calculate an index into the background palette from the palette and attribute color bits
    u16 bgr_pal_idx = PALETTE_BASE + (pt_color_bits | (attrib_color_bits << 2));

    // Get an index into the system palette from the background palette index
    bgr_color_idx = bgr_pal_idx & 3 ? ppu_read(nes, bgr_pal_idx) : 0;
  }

  // ************** End background rendering **************

  // ****************** Sprite rendering ******************
  // Look through each sprite in the secondary OAM and compare their X-positions to the current PPU X
  bool show_spr = GET_BIT(ppu->regs[PPUMASK], PPUMASK_SHOW_SPR_BIT);
  if (show_spr) {

  }
  // **************** End sprite rendering ****************

  // **************** Pixel multiplexer/display ****************
  u32 final_pixel;

  u8 uni_bgr_color_idx = ppu_read(nes, PALETTE_BASE);
  if (!bgr_color_idx) {
    final_pixel = get_palette_color(nes, uni_bgr_color_idx);
  } else {
    final_pixel = get_palette_color(nes, bgr_color_idx);
  }

  ((u32 *) pixels)[(cur_y * WINDOW_W) + cur_x] = final_pixel;
}

// Does a linear search through OAM to find up to 8 sprites to render for the current scanline
static void ppu_fill_sec_oam(nes_t *nes) {
  // Clear the list of sprites to draw (secondary OAM)
  // Zeroing out the sec_oam array has the benefit of setting the sprite struct valid flags to false
  // So we know which indices of sec_oam are filled
  ppu_t *ppu = nes->ppu;
  assert(SEC_OAM_NUM_SPR * sizeof ppu->sec_oam->data == 32);
  memset(ppu->sec_oam, 0, SEC_OAM_NUM_SPR * sizeof ppu->sec_oam->data);

  // Search through OAM to find sprites that are in range
  const u8 HEIGHT_OFFSET = GET_BIT(ppu->regs[PPUCTRL], PPUCTRL_SPRITE_SZ_BIT) ? 16 : 8;
  u8 sec_oam_i = 0;
  u16 cur_y = ppu->y - 1;
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

  if (ppu->y == 0) {
    // Pre-render scanline
    // Clear NMI flag on the second dot of the pre-render scanline
    if (ppu->x == 1) {
      // All visible scanlines have been rendered, frame is ready to be displayed
      wnd->frame_ready = true;
      ppu->frameno++;
      ppu->nmi_occurred = false;
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_VBLANK_BIT, 0);
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_ZEROHIT_BIT, 0);
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
      ppu_render_pixel(nes, pixels);
    } else if (ppu->x >= 257 && ppu->x <= 320) {
      // Set OAMADDR to 0
      ppu->regs[OAMADDR] = 0x00;
    }
  } else if (ppu->y == 241) {
    // Post-render scanline

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
//  printf("ppu_tick: x=%d y=%d ticks=%llu\n", ppu->x, ppu->y, ppu->ticks);
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

  switch (reg) {
    case PPUADDR:
      // The first PPUADDR write is the high byte of VRAM to be accessed, and the second byte
      // is the low byte
      // Clear the vram address if we're writing a new one in
      if (!write_toggle)
        ppu->vram_addr = 0x0000;
      ppu->vram_addr |= write_toggle ? val : (val << 8);
      write_toggle ^= true;  // Toggle ppuaddr_written
      break;
    case PPUSCROLL:
      if (write_toggle)
        ppu->scroll_y = val;
      else
        ppu->scroll_x = val;
      write_toggle ^= true;
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

  // Other mirrored addresses
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
//    case MT_VERTICAL:
      // Clear bit 10 to mirror down
      if ((addr >= 0x2400 && addr <= 0x27FF) || (addr >= 0x2C00 && addr <= 0x2FFF))
        addr &= ~0x400;
      break;
    case MT_VERTICAL:
//    case MT_HORIZONTAL:
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

u8 ppu_read(nes_t *nes, u16 addr) {
  u16 decoded_addr = ppu_decode_addr(nes, addr);

  return nes->ppu->mem[decoded_addr];
}

void ppu_write(nes_t *nes, u16 addr, u8 val) {
  u16 decoded_addr = ppu_decode_addr(nes, addr);

  nes->ppu->mem[decoded_addr] = val;
}

void ppu_destroy(nes_t *nes) {
  // TODO?
}

