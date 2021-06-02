#ifndef CNES_NES_H
#define CNES_NES_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "SDL.h"
#include "types.h"

#define LOG_OUTPUT

struct nes {
  struct cpu *cpu;
  struct ppu *ppu;
  struct cart *cart;
  struct window *window;
};

void nes_init(struct nes *nes, char *cart_fn);
void nes_destroy(struct nes *nes);
#endif
