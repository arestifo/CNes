#ifndef CNES_NES_H
#define CNES_NES_H

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "SDL.h"
#include "types.h"

typedef struct cpu cpu_t;
typedef struct ppu ppu_t;
typedef struct cart cart_t;
typedef struct window window_t;
typedef struct args args_t;
typedef struct mapper mapper_t;
typedef struct apu apu_t;

typedef struct nes {
  cpu_t *cpu;
  ppu_t *ppu;
  cart_t *cart;
  args_t *args;
  mapper_t *mapper;
  apu_t *apu;

  // Controller information
  bool controllers_polling;
  u8 ctrl1_sr;
  u8 ctrl1_sr_buf;
} nes_t;

void nes_init(nes_t *nes, char *cart_fn);
void nes_destroy(nes_t *nes);

#endif
