#ifndef CNES_MEM_H
#define CNES_MEM_H
#include "nes.h"
#include "cpu.h"

void mem_write8(u8 *mem, u16 addr, u8 val);
u8   mem_read8(u8 *mem, u16 addr);
void mem_write16(u8 *mem, u16 addr, u16 val);
u16  mem_read16(u8 *mem, u16 addr);

void cpu_write8(struct cpu *cpu, u16 addr, u8 val);
u8   cpu_read8(struct cpu *cpu, u16 addr);
void cpu_write16(struct cpu *cpu, u16 addr, u16 val);
u16  cpu_read16(struct cpu *cpu, u16 addr);

// Stack operations
void push8(struct cpu *cpu, u8 val);
void push16(struct cpu *cpu, u16 val);
u8   pop8(struct cpu *cpu);
u16  pop16(struct cpu *cpu);
#endif
