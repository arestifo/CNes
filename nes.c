#include "include/cpu.h"
#include "include/ppu.h"
#include "include/nes.h"
#include "include/util.h"
#include "include/cart.h"
#include "include/window.h"

void nes_init(struct nes *nes, char *cart_fn) {
  nes->cpu = nes_malloc(sizeof *nes->cpu);
  nes->ppu = nes_malloc(sizeof *nes->ppu);
  nes->cart = nes_malloc(sizeof *nes->cart);
//  nes->window = nes_malloc(sizeof *nes->window);

  cart_init(nes->cart, cart_fn);
  cpu_init(nes);
  ppu_init(nes);
}

void nes_destroy(struct nes *nes) {
  ppu_destroy(nes);
  cart_destroy(nes->cart);

  free(nes->cpu);
  free(nes->ppu);
  free(nes->cart);
//  free(nes->window);
}