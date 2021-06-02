#include "include/nes.h"
#include "include/util.h"
#include "include/cpu.h"
#include "include/ppu.h"
#include "include/window.h"

// Entry point of CNES
// Initializes everything and begins ROM execution
FILE *log_f;
static bool g_shutdown = false;

int main(int argc, char **argv) {
  struct nes nes;
  struct window window;
  char *cart_fn;
  int nextchar;

  printf("cnes v0.1 starting\n");

  // Init SDL
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    printf("SDL_Init() failed: %s\n", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  // Logging
  remove("../logs/cnes_cpu.log");
  log_f = nes_fopen("../logs/cnes.log", "w");

  // Read command line arguments
  if (argc != 2) {
    printf("%s: invalid command line arguments.\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  cart_fn = argv[1];

  // Initialize the NES and window
  nes_init(&nes, cart_fn);
  window_init(&window);

  // Update the window 60 times per second
  while (!g_shutdown) {
    window_update(&window, &nes);
//    cpu_tick(&nes);
//
//    // Three PPU cycles per CPU cycle
//    ppu_tick(&nes);
//    ppu_tick(&nes);
//    ppu_tick(&nes);
  }

  // Clean up
  window_destroy(&window);
  nes_destroy(&nes);
  nes_fclose(log_f);
  SDL_Quit();
}
