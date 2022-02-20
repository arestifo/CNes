#include "include/cpu.h"
#include "include/ppu.h"
#include "include/util.h"
#include "include/cart.h"
#include "include/args.h"
#include "include/mappers.h"
#include "include/apu.h"

void nes_init(nes_t *nes, args_t *args) {
  bzero(nes, sizeof *nes);

  nes->args   = args;
  nes->cpu    = nes_malloc(sizeof *nes->cpu);
  nes->ppu    = nes_malloc(sizeof *nes->ppu);
  nes->cart   = nes_malloc(sizeof *nes->cart);
  nes->mapper = nes_malloc(sizeof *nes->mapper);
  nes->apu    = nes_malloc(sizeof *nes->apu);

  cart_init(nes->cart, args->cart_fn);
  mapper_init(nes->mapper, nes->cart);
  cpu_init(nes);
  ppu_init(nes);
  apu_init(nes, args->sample_rate, args->apu_buf_len);
}

void nes_reset(nes_t *nes) {
  // Reset the nes and restart ROM execution
  cpu_init(nes);
  ppu_init(nes);

  apu_destroy(nes);
  apu_init(nes, nes->args->sample_rate, nes->args->apu_buf_len);
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
  free(nes->mapper);
  free(nes->apu);
}
