#include "include/cpu.h"
#include "include/ppu.h"
#include "include/nes.h"
#include "include/util.h"
#include "include/cart.h"

void nes_init(struct nes *nes, char *cart_fn) {
  nes->cpu = nes_malloc(sizeof(struct cpu));
  nes->ppu = nes_malloc(sizeof(struct ppu));
  nes->cart = nes_malloc(sizeof(struct cart));

  cart_init(nes->cart, cart_fn);
  cpu_init(nes->cpu, nes);
  ppu_init(nes->ppu, nes);
}

void nes_destroy(struct nes *nes) {
  free(nes->cpu);
  free(nes->ppu);
  free(nes->cart);
}