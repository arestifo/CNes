#include <stdio.h>
#include <stdlib.h>
#include "nes_cart.h"

// Entry point of CNES
// Initializes everything and begins ROM execution
int main(int argc, char **argv) {
  // Validate CMDline input
  if (argc != 2) {
    printf("Invalid command line arguments.\n");
    exit(EXIT_FAILURE);
  }
  char *cart_fn = argv[1];

  // Read in the cartridge and validate its header
  struct nes_cart cart;
  read_cart(&cart, cart_fn);

  // start_ppu
  // start_cpu
  // create_cnes_window
}
