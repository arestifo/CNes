#include "include/cart.h"
#include "include/util.h"

static u8 get_mapper(cart_t *cart) {
  u8 low, high;

  low = (cart->header.flags6 & 0xF0) >> 4;
  high = cart->header.flags7 & 0xF0;

  return low | high;
}

void cart_init(cart_t *cart, char *cart_fn) {
  // Open cart file in binary mode
  FILE *cart_f = nes_fopen(cart_fn, "rb");

  // Read in header and validate it
  size_t header_sz = sizeof cart->header;
  nes_fread(&cart->header, 1, header_sz, cart_f);

  // Check header magic number
  if (memcmp(cart->header.magic, INES_MAGIC, strlen(INES_MAGIC)) != 0) {
    printf("cart_init: File specified is not a NES ROM.\n");
    exit(EXIT_FAILURE);
  }

  // Is a trainer present? Bit 3 (mask 0x04) is the trainer present bit
  if (cart->header.flags6 & 0x04) {
    // Just ignore the trainer, advance over it
    fseek(cart_f, TRAINER_SZ, SEEK_CUR);
  }

  // TODO: Support CHR RAM (this is probably mapperno dependent, cart.c is not the right place for it)
  if (cart->header.chrrom_n == 0) {
    printf("cart_init: CHR RAM is not implemented yet.\n");
    exit(EXIT_FAILURE);
  }

  // Read PRG ROM
  cart->prg = nes_malloc(PRGROM_BLOCK_SZ * cart->header.prgrom_n);
  nes_fread(cart->prg, PRGROM_BLOCK_SZ, cart->header.prgrom_n, cart_f);

  // Read CHR ROM
  cart->chr = nes_malloc(CHRROM_BLOCK_SZ * cart->header.chrrom_n);
  nes_fread(cart->chr, CHRROM_BLOCK_SZ, cart->header.chrrom_n, cart_f);

  nes_fclose(cart_f);
  printf("cart_init: loaded cart prgrom=16K*%d chrrom=8K*%d trainer=%s\n",
         cart->header.prgrom_n, cart->header.chrrom_n,
         cart->header.flags6 & 0x04 ? "yes" : "no");

  cart->fixed_mirror = cart->header.flags6 & 1;
  cart->mapperno = get_mapper(cart);
}

void cart_destroy(cart_t *cart) {
  free(cart->prg);
  free(cart->chr);
}
