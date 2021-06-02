#include "include/window.h"

void window_init(struct window *wnd) {
  wnd->disp_window = SDL_CreateWindow("CNES",
                                      SDL_WINDOWPOS_CENTERED,
                                      SDL_WINDOWPOS_CENTERED,
                                      WINDOW_W, WINDOW_H, 0);
  if (!wnd->disp_window) {
    printf("window_init: CreateWindow() failed: %s\n", SDL_GetError());
  }

  // Create RGB pixel_surface from PPU rendered pixel_surface
  wnd->window_surface = SDL_GetWindowSurface(wnd->disp_window);
  wnd->pixel_surface = SDL_CreateRGBSurfaceFrom(wnd->pixels, WINDOW_W, WINDOW_H,
                                                24, 3 * WINDOW_W, 0x0000FF,
                                                0x00FF00, 0xFF0000, 0);
  if (!wnd->pixel_surface) {
    printf("window_init: CreateRGBSurfaceFrom() failed: %s\n", SDL_GetError());
  }

  // TEMPORARY PIXEL TESTING
  u32 px = 0;
  for (int i = 0; i < WINDOW_H; i++) {
    for (int j = 0; j < WINDOW_W; j++, px++) {
      wnd->pixels[i][j] = px;
    }
  }
}

void window_update(struct window *wnd, struct nes *nes) {
  SDL_BlitSurface(wnd->pixel_surface, NULL, wnd->window_surface, NULL);
  SDL_UpdateWindowSurface(wnd->disp_window);
}

void window_destroy(struct window *wnd) {
  SDL_FreeSurface(wnd->pixel_surface);
  SDL_DestroyWindow(wnd->disp_window);
}