#include "include/nes.h"
#include "include/util.h"
#include "include/cpu.h"
#include "include/ppu.h"
#include "include/window.h"

// Entry point of CNES
// Initializes everything and begins ROM execution
FILE *log_f;
static bool g_shutdown = false;

int main(int argc, char **argv) {
  nes_t nes;
  struct window window;
  SDL_Event event;
  u32 ticks, frame_time;
  f32 ticks_per_frame;
  char *log_fn;

  printf("cnes by Alex Restifo starting\n");
//  log_fn = "/dev/null";
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
  // TODO: This method of frame timing is not very accurate because it puts the
  // TODO: main thread to sleep, leaving it at the mercy of OS scheduling.
  // TODO: Experiment with VSync, SDL_GetPerformanceCounter(), and maybe spin-waiting
  ticks_per_frame = 1000.0 / TARGET_FPS;
  while (!g_shutdown) {
    ticks = SDL_GetTicks();

    // Run the PPU & CPU until we have a frame ready to render to the screen
    window_update(&window, &nes);

    // Main event polling loop
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          g_shutdown = true;
          break;
        case SDL_WINDOWEVENT:
          if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
            // TODO: Handle resizing
          }
          break;
      }
    }

    // Delay for enough time to get our desired FPS
    frame_time = SDL_GetTicks() - ticks;
    printf("frame_time: %dms (%.1f fps)\n", frame_time, 1000.0 / frame_time);
    SDL_Delay(MAX(ticks_per_frame - frame_time, 0));
  }

  // Clean up
  window_destroy(&window);
  nes_destroy(&nes);
  nes_fclose(log_f);
  SDL_Quit();
}
