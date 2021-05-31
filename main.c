#include "include/nes.h"
#include "include/util.h"
#include "include/cpu.h"
#include "include/ppu.h"

// Entry point of CNES
// Initializes everything and begins ROM execution
FILE *log_f;
static bool g_shutdown = false;

int main(int argc, char **argv) {
  struct nes nes;
  char *cart_fn;
  int nextchar;

  printf("cnes v0.1 starting\n");

  // Logging
  remove("../logs/cnes_cpu.log");
  log_f = nes_fopen("../logs/cnes.log", "w");

  // Read command line arguments
  if (argc != 2) {
    printf("%s: invalid command line arguments.\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  cart_fn = argv[1];

  // Initialize everything
  nes_init(&nes, cart_fn);

  // Tick the CPU on each key press and
  while (!g_shutdown) {
    cpu_tick(nes.cpu);

    // Three PPU cycles per CPU cycle
    ppu_tick(nes.ppu);
    ppu_tick(nes.ppu);
    ppu_tick(nes.ppu);
  }

  // Clean up
  nes_destroy(&nes);
  nes_fclose(log_f);
}
