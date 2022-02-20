#ifndef CNES_WINDOW_H
#define CNES_WINDOW_H

#include "nes.h"

#define WINDOW_W 256
#define WINDOW_H 240

typedef struct window {
  SDL_Window *disp_window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;

  // Set to true when the frame is done rendering
  bool frame_ready;
} window_t;

void window_init(window_t *wnd);
void window_draw_frame(window_t *wnd, nes_t *nes);
void window_destroy(window_t *wnd);

#endif
