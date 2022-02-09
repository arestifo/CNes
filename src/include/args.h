#ifndef CNES_ARGS_H
#define CNES_ARGS_H

#include "nes.h"

// TODO: Refactor some stuff from main and put it in here
typedef struct args {
  int argc;
  char **argv;

  bool cpu_log_output;
  FILE *cpu_logf;
} args_t;

#endif
