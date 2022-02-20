#ifndef CNES_ARGS_H
#define CNES_ARGS_H

#include "nes.h"

// TODO: Refactor some stuff from main and put it in here
typedef struct args {
  // Cart parameters
  char *cart_fn;

  // CPU logging parameters
  bool cpu_log_output;
  FILE *cpu_logf;

  // APU parameters
  u32 apu_buf_len;
  u32 sample_rate;
} args_t;

void args_init(args_t *args);
void args_destroy(args_t *args);


#endif
