#include "include/nes.h"
#include "include/util.h"
#include "include/cpu.h"
#include "include/ppu.h"
#include "include/window.h"

// Entry point of CNES
// Initializes everything and begins ROM execution
FILE *log_f;
static bool g_shutdown = false;

static void keyboard_input(nes_t *nes, SDL_Keycode sc, bool keydown) {
//  assert(keydown == 0 || keydown == 1);
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
    default:
      printf("keyboard_input: invalid input on key%s=%d\n", keydown ? "down" : "up", sc);
      break;
  }

  // Buffer input
  if (keydown)
    SET_BIT(nes->ctrl1_sr_buf, n, 1);
  else
    SET_BIT(nes->ctrl1_sr_buf, n, 0);
}

int main(int argc, char **argv) {
  nes_t nes;
  window_t window;
  SDL_Event event;
  u32 ticks, frame_time;
  f32 ticks_per_frame;
  char *log_fn;
  int i = 0;

  printf("cnes by Alex Restifo starting\n");
  log_fn = "../logs/cnes.log";

  // Init SDL
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    printf("SDL_Init() failed: %s\n", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  // Logging
  remove(log_fn);
  log_f = nes_fopen(log_fn, "w");

  // Read command line arguments
  if (argc != 2) {
    printf("%s: invalid command line arguments.\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // Initialize the NES and display window
  nes_init(&nes, argv[1]);
  window_init(&window);

  // Update the window 60 times per second
  ticks_per_frame = 1000.0 / TARGET_FPS;
  while (!g_shutdown) {
    ticks = SDL_GetTicks();

    // Main event polling loop TODO: Handle window resizing
    // Also update the keyboard array so we can get input
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          g_shutdown = true;
          break;
        case SDL_KEYDOWN:
          keyboard_input(&nes, event.key.keysym.sym, true);
          break;
        case SDL_KEYUP:
          keyboard_input(&nes, event.key.keysym.sym, false);
          break;
      }
    }

    // Run the PPU & CPU until we have a frame ready to render to the screen
    window_update(&window, &nes);

    // Delay for enough time to get our desired FPS
    frame_time = SDL_GetTicks() - ticks;
//    printf("main: frametime=%d (%.2f fps)\n", frame_time, 1000.0 / frame_time);
//    SDL_Delay(MAX(ticks_per_frame - frame_time, 0));
  }

  // Clean up
  window_destroy(&window);
  nes_destroy(&nes);
  nes_fclose(log_f);
  SDL_Quit();

  exit(EXIT_SUCCESS);
}
