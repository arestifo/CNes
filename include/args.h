#ifndef CNES_ARGS_H
#define CNES_ARGS_H

#include "nes.h"

// TODO: Refactor some stuff from main and put it in here
typedef struct args {
  bool ppu_log_output;
  bool cpu_log_output;

  FILE *cpu_logf;
  FILE *ppu_logf;

  int argc;
  char **argv;
} args_t;

#endif
