#include "include/window.h"
#include "include/ppu.h"
#include "include/cpu.h"

void window_init(struct window *wnd) {
  wnd->disp_window = SDL_CreateWindow("CNES",
                                      SDL_WINDOWPOS_UNDEFINED,
                                      SDL_WINDOWPOS_UNDEFINED,
                                      WINDOW_W, WINDOW_H, 0);
  if (!wnd->disp_window)
    printf("window_init: CreateWindow() failed: %s\n", SDL_GetError());

  // Create RGB pixel_surface from PPU rendered pixel_surface
  wnd->window_surface = SDL_GetWindowSurface(wnd->disp_window);
  wnd->pixel_surface = SDL_CreateRGBSurfaceFrom(wnd->pixels, WINDOW_W, WINDOW_H,
                                                24, 3 * WINDOW_W, 0x0000FF,
                                                0x00FF00, 0xFF0000, 0);
  if (!wnd->pixel_surface)
    printf("window_init: CreateRGBSurfaceFrom() failed: %s\n", SDL_GetError());
}

void window_update(struct window *wnd, nes_t *nes) {
  while (!wnd->frame_ready) {
    cpu_tick(nes);

    // 3 PPU ticks per CPU cycle
    while ((nes->ppu->ticks != nes->cpu->cyc * 3)/* || !wnd->frame_ready */) {
      ppu_tick(nes);
    }
  }

  // Update the window surface and show it on the screen
  SDL_BlitSurface(wnd->pixel_surface, NULL, wnd->window_surface, NULL);
  SDL_UpdateWindowSurface(wnd->disp_window);
}

void window_destroy(struct window *wnd) {
  SDL_FreeSurface(wnd->pixel_surface);
  SDL_DestroyWindow(wnd->disp_window);
}