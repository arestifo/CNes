#include "include/args.h"
#include "include/util.h"

void args_init(args_t *args) {
  // TODO: Add more config parameters here
  args->cpu_log_output = false;
  if (args->cpu_log_output)
    args->cpu_logf = nes_fopen("../logs/cpu.log", "w");
  else
    args->cpu_logf = NULL;

  // Default APU values
  args->apu_buf_len = 64;
  args->sample_rate = 48000;
}

void args_destroy(args_t *args) {
  if (args->cpu_logf)
    nes_fclose(args->cpu_logf);
  bzero(args, sizeof *args);
}
