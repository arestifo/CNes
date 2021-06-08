#include "include/cpu.h"
#include "include/ppu.h"
#include "include/nes.h"
#include "include/util.h"
#include "include/cart.h"
#include "include/window.h"

void nes_init(nes_t *nes, char *cart_fn) {
  nes->cpu = nes_malloc(sizeof *nes->cpu);
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

void controller_update(nes_t *nes) {
  const u8 *k_state = SDL_GetKeyboardState(NULL);
  SET_BIT(nes->ctrl1_sr, 0, k_state[SDL_SCANCODE_L]);  // L = button A
  SET_BIT(nes->ctrl1_sr, 1, k_state[SDL_SCANCODE_K]);  // K = button B
  SET_BIT(nes->ctrl1_sr, 2, k_state[SDL_SCANCODE_G]);  // G = select
  SET_BIT(nes->ctrl1_sr, 3, k_state[SDL_SCANCODE_H]);  // H = start
  SET_BIT(nes->ctrl1_sr, 4, k_state[SDL_SCANCODE_W]);  // W = up
  SET_BIT(nes->ctrl1_sr, 5, k_state[SDL_SCANCODE_S]);  // S = down
  SET_BIT(nes->ctrl1_sr, 6, k_state[SDL_SCANCODE_A]);  // A = left
  SET_BIT(nes->ctrl1_sr, 7, k_state[SDL_SCANCODE_D]);  // D = right
}
