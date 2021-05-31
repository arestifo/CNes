#ifndef CNES_MEM_H
#define CNES_MEM_H
#include "nes.h"
#include "cpu.h"

// Raw functions for reading and writing(DO NOT USE)
void cpu_write8(struct cpu *cpu, u16 addr, u8 val, bool inc_cyc);
void cpu_write16(struct cpu *cpu, u16 addr, u16 val, bool inc_cyc);
u8   cpu_read8(struct cpu *cpu, u16 addr, bool inc_cyc);
u16  cpu_read16(struct cpu *cpu, u16 addr, bool inc_cyc);

// Read/write wrappers for CPU memory
void write8(struct cpu *cpu, u16 addr, u8 val);
void write16(struct cpu *cpu, u16 addr, u16 val);
u8   read8(struct cpu *cpu, u16 addr);
u16  read16(struct cpu *cpu, u16 addr);

// Read/write wrappers that don't add cycles to the cpu
void dummy_write8(struct cpu *cpu, u16 addr, u8 val);
void dummy_write16(struct cpu *cpu, u16 addr, u16 val);
u8   dummy_read8(struct cpu *cpu, u16 addr);
u16  dummy_read16(struct cpu *cpu, u16 addr);

// Stack operations
void push8(struct cpu *cpu, u8 val);
void push16(struct cpu *cpu, u16 val);
u8   pop8(struct cpu *cpu);
u16  pop16(struct cpu *cpu);
#endif
