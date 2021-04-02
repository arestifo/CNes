#ifndef CNES_NES_CART_H
#define CNES_NES_CART_H
#include "types.h"
// iNES file format information from NESdev wiki
// https://wiki.nesdev.com/w/index.php/INES
struct nes_cart {
  struct {
    u8 magic[4];   // iNES header, should be exactly "NES\x1a"
    u8 prgrom_sz;  // Size of PRG ROM in 16K units
    u8 chrrom_sz;  // Size of CHR ROM in 8K units
    u8 flags6;     // Mapper, mirroring, battery, trainer
    u8 flags7;     // Mapper, VS/Playchoice, NES 2.0
    u8 flags8;     // PRG-RAM size (rarely used extension)
    u8 flags9;     // TV system (rarely used extension)
    u8 flags10;    // TV system, PRG-RAM presence (rarely used extension)
    u8 padding[5]; // Extra padding so the header is 16 bytes
  } header;
  FILE *cart_f;    // FILE pointer for cart file
};

void read_cart(struct nes_cart *, char *);
#endif  // CNES_NES_CART_H
