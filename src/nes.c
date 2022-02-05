#include "include/cpu.h"
#include "include/ppu.h"
#include "include/util.h"
#include "include/cart.h"
#include "include/args.h"
#include "include/mappers.h"
#include "include/apu.h"

void nes_init(nes_t *nes, char *cart_fn) {
  bzero(nes, sizeof *nes);

  nes->cpu    = nes_malloc(sizeof *nes->cpu);
  nes->ppu    = nes_malloc(sizeof *nes->ppu);
  nes->cart   = nes_malloc(sizeof *nes->cart);
  nes->args   = nes_malloc(sizeof *nes->args);
  nes->mapper = nes_malloc(sizeof *nes->mapper);
  nes->apu    = nes_malloc(sizeof *nes->apu);

  cart_init(nes->cart, cart_fn);
  mapper_init(nes->mapper, nes->cart);
  cpu_init(nes);
  ppu_init(nes);
  apu_init(nes, 48000, 64);
}

void nes_destroy(nes_t *nes) {
  apu_destroy(nes);
  ppu_destroy(nes);
  cpu_destroy(nes);
  mapper_destroy(nes->mapper);
  cart_destroy(nes->cart);

  free(nes->cpu);
  free(nes->ppu);
  free(nes->cart);
  free(nes->args);
  free(nes->mapper);
  free(nes->apu);
}
