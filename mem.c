#include "include/mem.h"

void mem_write8(u8 *mem, u16 addr, u8 val) {
  if (addr >= 0x2000 && addr <= 0x3FFF) {
    // TODO: PPU register writes
  } else {
    mem[addr] = val;
  }
}

u8 mem_read8(u8 *mem, u16 addr) {
  if (addr <= 0x1FFF) {
    // 2KB internal ram
    return mem[addr % 0x0800];
  } else if (addr >= 0x2000 && addr <= 0x3FFF) {
    // TODO: PPU registers and their mirrors
  } else if (addr >= 0x4000 && addr <= 0x4017) {
    // TODO: APU registers
  } else if (addr >= 0x4018 && addr <= 0x401F) {
    printf("cpu_read8: reading cpu test mode registers is not supported.\n");
    exit(EXIT_FAILURE);
  } else if (addr >= 0x4020 && addr <= 0xFFFF) {
    // TODO: PRG ROM, PRG RAM, and mapper registers

    // 16K Mapper 0 ($C000-$FFFF is a mirror of $8000-$BFFF)
    if (addr >= 0x8000 && addr <= 0xFFFF)
      return mem[0x8000 + (addr % 0x4000)];
    printf("cpu_read8: invalid read from 0x%04X\n", addr); // TODO
  } else {
    printf("cpu_read8: invalid read from 0x%04X\n", addr);
    exit(EXIT_FAILURE);
  }
  return 0;
}

void mem_write16(u8 *mem, u16 addr, u16 val) {
  u8 low, high;

  low = val & 0x00FF;
  high = (val & 0xFF00) >> 8;

  mem_write8(mem, addr, low);
  mem_write8(mem, addr + 1, high);
}

u16 mem_read16(u8 *mem, u16 addr) {
  u8 low, high;

  low = mem_read8(mem, addr);
  high = mem_read8(mem, addr + 1);

  return low | (high << 8);
}

void cpu_write8(struct cpu *cpu, u16 addr, u8 val) {
  cpu->cyc++;

  mem_write8(cpu->mem, addr, val);
}

u8 cpu_read8(struct cpu *cpu, u16 addr) {
  cpu->cyc++;

  return mem_read8(cpu->mem, addr);
}

// Write 16-bit value to memory in little-endian format
void cpu_write16(struct cpu *cpu, u16 addr, u16 val) {
  u8 low, high;

  low = val & 0x00FF;
  high = (val & 0xFF00) >> 8;

  cpu_write8(cpu, addr, low);
  cpu_write8(cpu, addr + 1, high);
}

// Read 16-bit value from memory stored in little-endian format
u16 cpu_read16(struct cpu *cpu, u16 addr) {
  u8 low, high;

  low = cpu_read8(cpu, addr);
  high = cpu_read8(cpu, addr + 1);

  return low | (high << 8);
}

void push8(struct cpu *cpu, u8 val) {
  cpu->cyc++;

  cpu_write8(cpu, STACK_BASE + cpu->sp--, val);
}

void push16(struct cpu *cpu, u16 val) {
  u8 low, high;

  cpu->cyc++;

  low = val & 0x00FF;
  high = (val & 0xFF00) >> 8;

  cpu_write8(cpu, STACK_BASE + cpu->sp--, high);
  cpu_write8(cpu, STACK_BASE + cpu->sp--, low);
}

u8 pop8(struct cpu *cpu) {
  cpu->cyc++;

  return cpu_read8(cpu, STACK_BASE + ++cpu->sp);
}

u16 pop16(struct cpu *cpu) {
  u8 low, high;

  cpu->cyc++;

  low = cpu_read8(cpu, STACK_BASE + ++cpu->sp);
  high = cpu_read8(cpu, STACK_BASE + ++cpu->sp);

  return low | (high << 8);
}