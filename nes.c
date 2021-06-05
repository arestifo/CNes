#include "include/cpu.h"
#include "include/ppu.h"
#include "include/nes.h"
#include "include/util.h"
#include "include/cart.h"
#include "include/window.h"

void nes_init(nes_t *nes, char *cart_fn) {
  // TODO: Why do I use calloc here? The reason has been lost to time
  nes->cpu = nes_calloc(1, sizeof *nes->cpu);
  nes->ppu = nes_malloc(sizeof *nes->ppu);
  nes->cart = nes_malloc(sizeof *nes->cart);

  cart_init(nes->cart, cart_fn);
  cpu_init(nes);
  ppu_init(nes);
}

void nes_destroy(nes_t *nes) {
  ppu_destroy(nes);
  cart_destroy(nes->cart);

  free(nes->cpu);
  free(nes->ppu);
  free(nes->cart);
}