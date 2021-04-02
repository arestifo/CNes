#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nes_cart.h"

void read_cart(struct nes_cart *cart, char *cart_fn) {
  // Open cart file
  FILE *cart_f;
  if ((cart_f = fopen(cart_fn, "rb")) == NULL) {
    printf("read_cart(): Error opening cartridge file.\n");
    exit(EXIT_FAILURE);
  }

  // Read in header and validate it
  size_t header_sz = sizeof cart->header;
  if (fread(&cart->header, 1, header_sz, cart_f) != header_sz) {
    printf("read_cart(): Error reading cart header");
    exit(EXIT_FAILURE);
  }

  // Check header magic number
  if (memcmp(cart->header.magic, "NES\x1a", 4) != 0) {
    printf("read_cart(): file specified is not a NES ROM!");
    exit(EXIT_FAILURE);
  }
  cart->cart_f = cart_f;
}