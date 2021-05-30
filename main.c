#include "include/nes.h"
#include "include/util.h"
#include "include/cpu.h"

// Entry point of CNES
// Initializes everything and begins ROM execution
FILE *log_f;
static bool g_shutdown = false;

int main(int argc, char **argv) {
  struct cart cart;
  struct cpu cpu;
  char *cart_fn;
  int nextchar;

  printf("cnes v0.1 starting\n");

  // Logging TODO: This isn't the cleanest way of doing this
  remove("../logs/cnes_cpu.log");
  log_f = nes_fopen("../logs/cnes_cpu.log", "w");
//  log_f = stdout;

  // Read command line arguments
  if (argc != 2) {
    printf("%s: invalid command line arguments.\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  cart_fn = argv[1];

  // Initialize everything
  cart_init(&cart, cart_fn);
  cpu_init(&cpu, &cart);

  // Tick the CPU on each key press
  while (!g_shutdown) {
    cpu_tick(&cpu);
  }

  // Clean up
  cart_destroy(&cart);
  cpu_destroy(&cpu);
  nes_fclose(log_f);
}
