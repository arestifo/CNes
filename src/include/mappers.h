#ifndef CNES_MAPPERS_H
#define CNES_MAPPERS_H

#include "nes.h"

typedef enum mirror_type {
  MT_HORIZONTAL, MT_VERTICAL
} mirror_type_t;

// Supported mappers:
// Maps iNES mapper numbers to mapper functions
// Currently supported:
// 000: NROM
//
// Plans to support:
// 001: MMC1
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

u16 mirror_ppu_addr(u16 addr, mirror_type_t mt);

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

#endif
