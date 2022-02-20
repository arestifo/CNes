#ifndef CNES_NES_H
#define CNES_NES_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <assert.h>

#ifdef WIN32
  #include "SDL2/SDL.h"
#else
  #include "SDL.h"
#endif

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

  // Controller 1 shift registers
  u8 ctrl1_sr;
  u8 ctrl1_sr_buf;

  // Controller 2 shift registers
  u8 ctrl2_sr;
  u8 ctrl2_sr_buf;
} nes_t;

void nes_init(nes_t *nes, args_t *args);
void nes_reset(nes_t *nes);
void nes_destroy(nes_t *nes);

#endif
