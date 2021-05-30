#ifndef CNES_MEM_H
#define CNES_MEM_H
#include "nes.h"
#include "cpu.h"

// Read/write wrappers for CPU memory
void write8(struct cpu *cpu, u16 addr, u8 val, bool inc_cyc);
void write16(struct cpu *cpu, u16 addr, u16 val, bool inc_cyc);
u8   read8(struct cpu *cpu, u16 addr, bool inc_cyc);
u16  read16(struct cpu *cpu, u16 addr, bool inc_cyc);

// Stack operations
void push8(struct cpu *cpu, u8 val, bool inc_cyc);
void push16(struct cpu *cpu, u16 val, bool inc_cyc);
u8   pop8(struct cpu *cpu, bool inc_cyc);
u16  pop16(struct cpu *cpu, bool inc_cyc);
#endif
