#include "include/args.h"
#include "include/util.h"

void args_init(args_t *args) {
  // TODO: Add more config parameters here
  args->cpu_log_output = false;
  if (args->cpu_log_output)
    args->cpu_logf = nes_fopen("../logs/cpu.log", "w");
  else
    args->cpu_logf = NULL;

  // Query default audio device sample rate. If it can't be found, use a default value
  const u16 default_buf_len = 64;
  const i32 default_sample_rate = 48000;
  args->apu_buf_len = default_buf_len;

  char *default_device_name;
  SDL_AudioSpec default_spec;

  if (SDL_GetDefaultAudioInfo(&default_device_name, &default_spec, 0) == 0) {
    args->sample_rate = default_spec.freq;

    printf("args_init: found audio device \"%s\" with sample_rate=%d\n", default_device_name, args->sample_rate);

    SDL_free(default_device_name);
  } else {
    // Default APU values
    args->sample_rate = default_sample_rate;

    printf("args_init: error getting default audio device: %s\n", SDL_GetError());
    printf("args_init: using default parameters: sample_rate=%d\n", args->sample_rate);
  }
}

void args_destroy(args_t *args) {
  if (args->cpu_logf)
    nes_fclose(args->cpu_logf);
  memset(args, 0, sizeof *args);
}
