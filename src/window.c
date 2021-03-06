#include "include/window.h"
#include "include/ppu.h"
#include "include/cpu.h"
#include "include/apu.h"

void window_init(window_t *wnd) {
  // Create the main display window
  wnd->disp_window = SDL_CreateWindow("CNES",
                                      SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                                      WINDOW_W, WINDOW_H, SDL_WINDOW_RESIZABLE);
  if (!wnd->disp_window)
    printf("window_init: SDL_CreateWindow() failed: %s\n", SDL_GetError());

  // Create RGB pixel_surface from PPU rendered pixel_surface
  wnd->renderer = SDL_CreateRenderer(wnd->disp_window, -1,
                                     SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
  if (!wnd->renderer)
    printf("window_init: SDL_GetRenderer() failed: %s\n", SDL_GetError());

  // Create texture to render the screen to
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  wnd->texture = SDL_CreateTexture(wnd->renderer, SDL_PIXELFORMAT_ARGB32,
                                   SDL_TEXTUREACCESS_STREAMING, WINDOW_W, WINDOW_H);
  if (!wnd->texture)
    printf("window_init: SDL_CreateTexture() failed: %s\n", SDL_GetError());

  wnd->frame_ready = false;
}

void window_draw_frame(window_t *wnd, nes_t *nes) {
  // One row in the framebuffer is 4 * WINDOW_W bytes long (Framebuffer pixels are ARGB)
  int pitch = 4 * WINDOW_W;
  u32 *pixels = NULL;

  // Grab rendering surface
  SDL_LockTexture(wnd->texture, NULL, (void **) &pixels, &pitch);
  while (!wnd->frame_ready) {
    cpu_tick(nes);

    // Three PPU ticks per CPU cycle
    ppu_tick(nes, wnd, pixels);
    ppu_tick(nes, wnd, pixels);
    ppu_tick(nes, wnd, pixels);

    // APU tick every two CPU cycles
    if (nes->cpu->ticks & 1)
      apu_tick(nes);
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