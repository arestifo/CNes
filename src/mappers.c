#include "include/mappers.h"
#include "include/ppu.h"
#include "include/cart.h"

u8 (*const mapper_cpu_read_fns[4])(nes_t *, u16) = {
    nrom_cpu_read, mmc1_cpu_read, NULL, NULL
};

u8 (*const mapper_ppu_read_fns[4])(nes_t *, u16) = {
    nrom_ppu_read, mmc1_ppu_read, NULL, NULL
};

void (*const mapper_cpu_write_fns[4])(nes_t *, u16, u8) = {
    nrom_cpu_write, mmc1_cpu_write, NULL, NULL
};

void (*const mapper_ppu_write_fns[4])(nes_t *, u16, u8) = {
    nrom_ppu_write, mmc1_ppu_write, NULL, NULL
};

void mapper_init(mapper_t *mapper, cart_t *cart) {
  // Check if we support the cart's mapper
  // There are 255 iNES 1.0 mappers TODO: (low priority) support iNES 2.0
  switch (cart->mapperno) {
    case 0:  // NROM
      printf("mapper_init: using NROM mapper\n");

      // NROM uses fixed mirroring type, set in cart header
      mapper->mirror_type = cart->fixed_mirror ? MT_VERTICAL : MT_HORIZONTAL;
      break;
    default:
      printf("mapper_init: fatal: unsupported mapper %d!\n", cart->mapperno);
      exit(EXIT_FAILURE);
  }

  // Set up the correct mapper function pointers
  mapper->cpu_read = mapper_cpu_read_fns[cart->mapperno];
  mapper->cpu_write = mapper_cpu_write_fns[cart->mapperno];
  mapper->ppu_read = mapper_ppu_read_fns[cart->mapperno];
  mapper->ppu_write = mapper_ppu_write_fns[cart->mapperno];
}

void mapper_destroy(mapper_t *mapper) {
  bzero(mapper, sizeof *mapper);
}

u16 mirror_ppu_addr(u16 addr, mirror_type_t mt) {
  // $3F20-$3FFF are mirrors of $3F00-$3F1F (palette)
  // $3000-$3EFF are mirrors of $2000-$2EFF (nametables)

  if (addr >= 0x3000 && addr <= 0x3EFF) {
    // Nametable mirrored addresses
    // Convert the address by clearing the 12th bit (0x1000)
    addr &= ~0x1000;
  } else if (addr >= 0x3F10 && addr <= 0x3F1F) {
    // Palette mirroring
    if ((addr & 3) == 0)
      addr &= ~0x10;
  } else if (addr >= 0x3F20 && addr <= 0x3FFF) {
    addr = mirror_ppu_addr(PALETTE_BASE + addr % 0x20, mt);
  }

  // PPU mirroring
  switch (mt) {
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
      printf("mirror_ppu_addr: invalid mirroring!\n");
      exit(EXIT_FAILURE);
  }

  return addr;
}