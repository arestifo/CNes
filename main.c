#include <stdio.h>
#include <SDL.h>
#include "ines.h"

int main(int argc, char **argv) {
  // Validate CMDline input
  if (argc != 2) {
    printf("Invalid command line arguments.\n");
    exit(EXIT_FAILURE);
  }

  // Init SDL
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    printf("SDL initialization failed!\n");
    exit(EXIT_FAILURE);
  }

  // Read cartridge from command line input
  char *cart_fn = argv[1];
  FILE *cart_f;
  if ((cart_f = fopen(cart_fn, "rb")) == NULL) {
    printf("Error opening cartridge file!");
    exit(EXIT_FAILURE);
  }

  // Read in the cartridge and validate its header
  struct nes_cart cart;
  printf("%d\n", sizeof cart);
}
