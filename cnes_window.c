#include "cnes_window.h"
#include "SDL.h"
void cnes_create_window(void) {
  // Init SDL
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    printf("SDL_Init() failed: %s\n", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  SDL_Window *window = SDL_CreateWindow("CNES", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 256, 240, 0);
  if (!window) {
    printf("CreateWindow() failed: %s\n", SDL_GetError());
  }

  SDL_Surface *window_surface = SDL_GetWindowSurface(window);
  SDL_Surface *framebuf = NULL;
}