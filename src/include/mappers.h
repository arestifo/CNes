#ifndef CNES_MAPPERS_H
#define CNES_MAPPERS_H

#include "nes.h"

// There are 2^16 possible PPU addresses and four mirroring modes
#define PPU_ADDR_CACHE_SIZE ((1 << 16) * 4)

typedef enum mirror_type {
  MT_HORIZONTAL, MT_VERTICAL, MT_1SCR_A, MT_1SCR_B
} mirror_type_t;

// There are four PPU address mirroring types and 2^16 addresses, so we can build a lookup table on startup to
// cache the mirrored addresses and not have to compute them every time
typedef struct ppu_cache {
    mirror_type_t mtype;
    u16 addr;
} ppu_cache_t;

// Supported mappers:
// Maps iNES mapper numbers to mapper functions
// Currently supported:
// 000: NROM
// 001: MMC1
// 007: ANROM
//
// Plans to support:
// 002: UxROM
// 003: CNROM
// 004: MMC3
typedef struct mapper {
  // A mapper's primary ability is to expand the amount of available PRG/CHR ROM space
  // to allow for better graphics, sound, etc. These functions do mapping for CPU and PPU addresses
  u8 (*cpu_read)(nes_t *nes, u16 addr);
  u8 (*ppu_read)(nes_t *nes, u16 addr);

  void (*cpu_write)(nes_t *nes, u16 addr, u8 val);
  void (*ppu_write)(nes_t *nes, u16 addr, u8 val);

  mirror_type_t mirror_type;
} mapper_t;

void mapper_init(mapper_t *mapper, cart_t *cart);
void mapper_destroy(mapper_t *mapper);

u16 mapper_ppu_addr(u16 addr, mirror_type_t mt);
void mapper_init_ppu_cache(u16 *cache_arr);

// ******** Mapper-specific R/W functions ********
// **** NROM ****
u8 nrom_cpu_read(nes_t *nes, u16 addr);
u8 nrom_ppu_read(nes_t *nes, u16 addr);

void nrom_cpu_write(nes_t *nes, u16 addr, u8 val);
void nrom_ppu_write(nes_t *nes, u16 addr, u8 val);

// **** MMC1 ****
u8 mmc1_cpu_read(nes_t *nes, u16 addr);
u8 mmc1_ppu_read(nes_t *nes, u16 addr);

void mmc1_cpu_write(nes_t *nes, u16 addr, u8 val);
void mmc1_ppu_write(nes_t *nes, u16 addr, u8 val);

// **** AxROM ****
u8 axrom_cpu_read(nes_t *nes, u16 addr);
u8 axrom_ppu_read(nes_t *nes, u16 addr);

void axrom_cpu_write(nes_t *nes, u16 addr, u8 val);
void axrom_ppu_write(nes_t *nes, u16 addr, u8 val);

#endif
