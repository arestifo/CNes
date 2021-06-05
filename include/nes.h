#ifndef CNES_NES_H
#define CNES_NES_H

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "SDL.h"
#include "types.h"

#define LOG_OUTPUT

typedef struct cpu cpu_t;
typedef struct ppu ppu_t;
typedef struct cart cart_t;
typedef struct window window_t;

typedef struct nes {
  cpu_t *cpu;
  ppu_t *ppu;
  cart_t *cart;
} nes_t;

void nes_init(nes_t *nes, char *cart_fn);
void nes_destroy(nes_t *nes);
#endif
