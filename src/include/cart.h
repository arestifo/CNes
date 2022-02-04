#ifndef CNES_CART_H
#define CNES_CART_H

#include "nes.h"

#define PRGROM_BLOCK_SZ 0x4000
#define CHRROM_BLOCK_SZ 0x2000
#define TRAINER_SZ      0x200
#define INES_MAGIC      "NES\x1a"

// iNES file format information from NESdev wiki
// https://wiki.nesdev.com/w/index.php/INES
typedef struct cart {
  struct {
    u8 magic[4];    // iNES header, should be exactly "NES\x1a"
    u8 prgrom_n;    // Number of 16K PRG ROMs
    u8 chrrom_n;    // Number of 8K CHR ROMs
    u8 flags6;      // Mapper, mirroring, battery, trainer
    u8 flags7;      // Mapper, VS/Playchoice, NES 2.0
    u8 flags8;      // PRG-RAM size (rarely used extension)
    u8 flags9;      // TV system (rarely used extension)
    u8 flags10;     // TV system, PRG-RAM presence (rarely used extension)
    u8 padding[5];  // Extra padding so the header is 16 bytes
  } header;

  // TODO: Maybe clean this stuff up in the future
  u8 mapperno;      // Mapper number this cart uses
  u8 fixed_mirror;  // Fixed mirroring type. This only applies to mappers with a fixed mirroring type
  u8 *prg;          // PRG ROM
  u8 *chr;          // CHR ROM/RAM
} cart_t;

void cart_init(cart_t *cart, char *cart_fn);
void cart_destroy(cart_t *cart);

#endif
