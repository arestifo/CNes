#ifndef CNES_NES_H
#define CNES_NES_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "types.h"
#include "cart.h"
#include "cpu.h"
#include "mem.h"
#include "util.h"

#define DEBUG

// TODO: Might not need this struct
struct nes {
  struct cpu *cpu;
  struct ppu *ppu;
  struct cart *cart;
};
#endif
