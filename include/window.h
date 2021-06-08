#ifndef CNES_WINDOW_H
#define CNES_WINDOW_H

#include "nes.h"

#define WINDOW_W 256
#define WINDOW_H 240

#define TARGET_FPS 120

typedef struct window {
  SDL_Window *disp_window;
  SDL_Surface *pixel_surface;
  SDL_Surface *window_surface;

  // 256x240 RGB buffer of pixel_surface to be drawn on the screen
  u32 pixels[WINDOW_H][WINDOW_W];

  // Set to true when the frame is done rendering
  bool frame_ready;
} window_t;

void window_init(window_t *wnd);
void window_update(window_t *wnd, nes_t *nes);
void window_destroy(window_t *wnd);

#endif
