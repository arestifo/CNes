#include "include/mappers.h"
#include "include/ppu.h"
#include "include/cart.h"
#include "include/util.h"

u8 (*const mapper_cpu_read_fns[8])(nes_t *, u16) = {
        nrom_cpu_read, mmc1_cpu_read, NULL, NULL, NULL, NULL, NULL, axrom_cpu_read
};

u8 (*const mapper_ppu_read_fns[8])(nes_t *, u16) = {
        nrom_ppu_read, mmc1_ppu_read, NULL, NULL, NULL, NULL, NULL, axrom_ppu_read
};

void (*const mapper_cpu_write_fns[8])(nes_t *, u16, u8) = {
        nrom_cpu_write, mmc1_cpu_write, NULL, NULL, NULL, NULL, NULL, axrom_cpu_write
};

void (*const mapper_ppu_write_fns[8])(nes_t *, u16, u8) = {
        nrom_ppu_write, mmc1_ppu_write, NULL, NULL, NULL, NULL, NULL, axrom_ppu_write
};

// Stores all possible PPU addresses. This way, we don't have to calculate the mirrored address on every PPU read
u16 ppu_addr_cache[PPU_ADDR_CACHE_SIZE];

char *map_str(u8 mapno) {
  switch (mapno) {
    case 0:
      return "NROM";
    case 1:
      return "MMC1";
    case 7:
      return "AxROM";
  }
  return NULL;
}

void mapper_init(mapper_t *mapper, cart_t *cart) {
  // Check if we support the cart's mapper
  // There are 255 iNES 1.0 mappers TODO: (low priority) support iNES 2.0

  // Set fixed mirroring type for mappers that don't control it. This gets overwritten by mappers that control
  // mirroring themselves (MMC1, MMC3, etc.)
  mapper->mirror_type = cart->fixed_mirror ? MT_VERTICAL : MT_HORIZONTAL;

  char *mapstr;
  if ((mapstr = map_str(cart->mapno)) == NULL) {
    crash_and_burn("mapper_init: fatal: unsupported mapper %d!\n", cart->mapno);
  }
  printf("mapper_init: using %s mapper (%d)", mapstr, cart->mapno);

  // Set up the correct mapper function pointers
  mapper->cpu_read = mapper_cpu_read_fns[cart->mapno];
  mapper->cpu_write = mapper_cpu_write_fns[cart->mapno];
  mapper->ppu_read = mapper_ppu_read_fns[cart->mapno];
  mapper->ppu_write = mapper_ppu_write_fns[cart->mapno];

  // Initialize the PPU address cache
  mapper_init_ppu_cache(ppu_addr_cache);
}

void mapper_destroy(mapper_t *mapper) {
  bzero(mapper, sizeof *mapper);
}

u16 mapper_init_ppu_cache_helper(u16 addr, mirror_type_t mt) {
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
    addr = mapper_init_ppu_cache_helper(PALETTE_BASE + addr % 0x20, mt);
  }

  // Nametable mirroring
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
    case MT_1SCR_A:
      if (addr >= 0x2000 && addr <= 0x2FFF)
        addr &= ~0xC00;
      break;
    case MT_1SCR_B:
      if (addr >= 0x2000 && addr <= 0x2FFF)
        addr &= ~0xC00;
      addr += 0x400;
      break;
    default:
      crash_and_burn("mapper_ppu_addr: invalid mirroring!\n");
  }

  return addr;
}

void mapper_init_ppu_cache(u16 *cache_arr) {
  bzero(ppu_addr_cache, PPU_ADDR_CACHE_SIZE);

  for (u8 mtype = 0; mtype < 4; mtype++) {
    for (u32 baseaddr = 0; baseaddr < 65536; baseaddr++) {
      cache_arr[mtype * 65536 + baseaddr] = mapper_init_ppu_cache_helper(baseaddr, mtype);
    }
  }
}


u16 mapper_ppu_addr(u16 addr, mirror_type_t mt) {
  // Get the effective mirrored address from the PPU address cache
  return ppu_addr_cache[mt * 65536 + addr];
}