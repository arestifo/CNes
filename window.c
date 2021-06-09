#include "include/window.h"
#include "include/ppu.h"
#include "include/cpu.h"

void window_init(window_t *wnd) {
  // Create the main display window
  wnd->disp_window = SDL_CreateWindow("CNES",
                                      SDL_WINDOWPOS_UNDEFINED,
                                      SDL_WINDOWPOS_UNDEFINED,
                                      WINDOW_W, WINDOW_H, SDL_WINDOW_RESIZABLE);
  if (!wnd->disp_window)
    printf("window_init: SDL_CreateWindow() failed: %s\n", SDL_GetError());

  // Create RGB pixel_surface from PPU rendered pixel_surface
  wnd->renderer = SDL_CreateRenderer(wnd->disp_window, -1,
                                     SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!wnd->renderer)
    printf("window_init: SDL_GetRenderer() failed: %s\n", SDL_GetError());

  // Create texture to render the screen to
  wnd->texture = SDL_CreateTexture(wnd->renderer, SDL_PIXELFORMAT_ARGB32,
                                   SDL_TEXTUREACCESS_STREAMING, WINDOW_W, WINDOW_H);
  if (!wnd->texture)
    printf("window_init: SDL_CreateTexture() failed: %s\n", SDL_GetError());

  wnd->frame_ready = false;
}

void window_update(window_t *wnd, nes_t *nes) {
  int pitch = 4 * WINDOW_W;
  u32 *pixels = NULL;

  SDL_LockTexture(wnd->texture, NULL, (void **) &pixels, &pitch);
  while (!wnd->frame_ready) {
    cpu_tick(nes);

    // 3 PPU ticks per CPU cycle
    while (nes->ppu->ticks != nes->cpu->cyc * 3) {
      ppu_tick(nes, wnd, pixels);
    }

    // TODO: Decrement APU timers here
  }

  // Draw the screen texture to the screen
  SDL_UnlockTexture(wnd->texture);
  SDL_RenderCopy(wnd->renderer, wnd->texture, NULL, NULL);
  SDL_RenderPresent(wnd->renderer);
  wnd->frame_ready = false;
}

void window_destroy(window_t *wnd) {
  SDL_DestroyTexture(wnd->texture);
  SDL_DestroyRenderer(wnd->renderer);
  SDL_DestroyWindow(wnd->disp_window);
}