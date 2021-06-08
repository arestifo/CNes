#include "include/cart.h"
#include "include/util.h"

void cart_init(cart_t *cart, char *cart_fn) {
  FILE *cart_f;
  size_t header_sz;
  bool found;
  int i;
  const int NUM_MAPPERS = 1;
  const int SUPPORTED_MAPPERS[NUM_MAPPERS] = {0};

  // Open cart file
  cart_f = nes_fopen(cart_fn, "rb");

  // Read in header and validate it
  header_sz = sizeof cart->header;
  nes_fread(&cart->header, 1, header_sz, cart_f);

  // Check header magic number
  if (memcmp(cart->header.magic, "NES\x1a", strlen("NES\x1a")) != 0) {
    printf("cart_init: File specified is not a NES ROM.\n");
    exit(EXIT_FAILURE);
  }

  // Get the mapper from the cart
  cart->mapper = get_mapper(cart);
  for (i = 0, found = false; i < NUM_MAPPERS; i++) {
    if (SUPPORTED_MAPPERS[i] == cart->mapper) {
      found = true;
      break;
    }
  }

  if (!found) {
    printf("cart_init: warning: File specified uses an unsupported mapper.\n");
    printf("cart_init: get_mapper=%d\n", cart->mapper);
  }

  // Is a trainer present? Bit 3 (mask 0x04) is the trainer present bit
  if (cart->header.flags6 & 0x04) {
    // Just ignore the trainer, advance over it
    fseek(cart_f, TRAINER_SZ, SEEK_CUR);
  }

  // TODO: Support CHR RAM
//  if (cart->header.chrrom_n == 0) {
//    printf("cart_init: CHR RAM is not implemented yet.\n");
//    exit(EXIT_FAILURE);
//  }

  // Read PRG ROM
  cart->prg_rom = nes_malloc(PRGROM_BLOCK_SZ * cart->header.prgrom_n);
  nes_fread(cart->prg_rom, PRGROM_BLOCK_SZ, cart->header.prgrom_n, cart_f);

  // Read CHR ROM
  cart->chr_rom = nes_malloc(CHRROM_BLOCK_SZ * cart->header.chrrom_n);
  nes_fread(cart->chr_rom, CHRROM_BLOCK_SZ, cart->header.chrrom_n, cart_f);

  printf("Loaded cart prgrom=16K*%d, chrrom=8K*%d, mapper=%d, trainer=%s\n",
         cart->header.prgrom_n, cart->header.chrrom_n, cart->mapper,
         cart->header.flags6 & 0x04 ? "yes" : "no");

  cart->cart_f = cart_f;
}

void cart_destroy(cart_t *cart) {
  free(cart->prg_rom);
  free(cart->chr_rom);
  nes_fclose(cart->cart_f);
}

u8 get_mapper(cart_t *cart) {
  u8 low, high;

  low = (cart->header.flags6 & 0xF0) >> 4;
  high = cart->header.flags7 & 0xF0;

  return low | high;
}