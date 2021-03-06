#include "include/nes.h"
#include "include/util.h"
#include "include/cpu.h"
#include "include/ppu.h"
#include "include/window.h"
#include "include/args.h"

static void keyboard_input(nes_t *nes, SDL_Keycode sc, bool keydown) {
  u8 n;
  switch (sc) {
    case SDLK_l:  // L = button A
      n = 0;
      break;
    case SDLK_k:  // K = button B
      n = 1;
      break;
    case SDLK_g:  // G = select
      n = 2;
      break;
    case SDLK_h:  // H = start
      n = 3;
      break;
    case SDLK_w:  // W = up
      n = 4;
      break;
    case SDLK_s:  // S = down
      n = 5;
      break;
    case SDLK_a:  // A = left
      n = 6;
      break;
    case SDLK_d:  // D = right
      n = 7;
      break;
    case SDLK_r:  // R = reset;
      nes_reset(nes);
      return;
    default:
      printf("keyboard_input: invalid input on key%s=%d\n", keydown ? "down" : "up", sc);
      return;
  }

  // Buffer input
  if (keydown)
    SET_BIT(nes->ctrl1_sr_buf, n, 1);
  else
    SET_BIT(nes->ctrl1_sr_buf, n, 0);
}

int main(int argc, char **argv) {
  printf("cnes by Alex Restifo\n");

  // Init SDL
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    crash_and_burn("SDL_Init() failed: %s\n", SDL_GetError());

  // Read command line arguments
  // TODO: Usage string
  if (argc != 2)
    crash_and_burn("Invalid command line arguments.\n");

  // Initialize the NES and display window
  nes_t nes;
  args_t args;
  window_t window;

  // Use default starting values
  args_init(&args);
  args.cart_fn = argv[1];

  nes_init(&nes, &args);
  window_init(&window);

  // TODO: Make this configurable
  SDL_SetWindowSize(window.disp_window, 2 * WINDOW_W, 2 * WINDOW_H);

  bool is_running = true;
  u32 last_ticks = SDL_GetTicks();
  SDL_Event event;
  while (is_running) {
    // Main event polling loop
    // Also update the keyboard array so we can get input
    if (SDL_GetTicks() - last_ticks > 1000 / 60.) {
      last_ticks = SDL_GetTicks();

      while (SDL_PollEvent(&event)) {
        switch (event.type) {
          case SDL_QUIT:
            is_running = false;
            break;
          case SDL_KEYDOWN:
            keyboard_input(&nes, event.key.keysym.sym, true);
            break;
          case SDL_KEYUP:
            keyboard_input(&nes, event.key.keysym.sym, false);
            break;
        }
      }

      // Generate a frame and display it
      window_draw_frame(&window, &nes);
    } else {
      SDL_Delay(1);
    }
  }

  // Clean up
  nes_fclose(nes.args->cpu_logf);

  window_destroy(&window);
  nes_destroy(&nes);
  SDL_Quit();

  return 0;
}

#ifdef WIN32
#include "Windows.h"

//-----------------------------------------------------------------
int CALLBACK WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
//-----------------------------------------------------------------
{
    return main(__argc, __argv);
}
#endif // #ifdef WIN32