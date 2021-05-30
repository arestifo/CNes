#include "include/mem.h"

void write8(struct cpu *cpu, u16 addr, u8 val, bool inc_cyc) {
  // TODO: Add more functionality as we go, it's definitely not this simple...
  if (inc_cyc)
    cpu->cyc++;

  cpu->mem[addr] = val;
}

u8 read8(struct cpu *cpu, u16 addr, bool inc_cyc) {
  if (inc_cyc)
    cpu->cyc++;

  if (addr <= 0x1FFF) {
    // 2KB internal ram
    return cpu->mem[addr % 0x0800];
  } else if (addr >= 0x2000 && addr <= 0x3FFF) {
    // TODO: PPU registers and their mirrors
  } else if (addr >= 0x4000 && addr <= 0x4017) {
    // TODO: APU registers
  } else if (addr >= 0x4018 && addr <= 0x401F) {
    printf("read8: reading cpu test mode registers is not supported.\n");
    return 0xFF;
  } else if (addr >= 0x4020 && addr <= 0xFFFF) {
    // TODO: PRG ROM, PRG RAM, and mapper registers
    // TODO: This only works for *16K* mapper 0!

    // 16K Mapper 0 ($C000-$FFFF is a mirror of $8000-$BFFF)
    if (addr >= 0x8000 && addr <= 0xFFFF)
      return cpu->mem[0x8000 + (addr % 0x4000)];
    printf("read8: invalid read from 0x%04X\n", addr); // TODO
  } else {
    printf("read8: invalid read from 0x%04X\n", addr);
    exit(EXIT_FAILURE);
  }
  return 0;
}

// Write 16-bit value to memory in little-endian format
void write16(struct cpu *cpu, u16 addr, u16 val, bool inc_cyc) {
  u8 low, high;

  low = val & 0x00FF;
  high = (val & 0xFF00) >> 8;

  write8(cpu, addr, low, inc_cyc);
  write8(cpu, addr + 1, high, inc_cyc);
}

// Read 16-bit value from memory stored in little-endian format
u16 read16(struct cpu *cpu, u16 addr, bool inc_cyc) {
  u8 low, high;

  low = read8(cpu, addr, inc_cyc);
  high = read8(cpu, addr + 1, inc_cyc);

  return low | (high << 8);
}

void push8(struct cpu *cpu, u8 val, bool inc_cyc) {
  if (inc_cyc)
    cpu->cyc++;

  write8(cpu, STACK_BASE + cpu->sp--, val, inc_cyc);
}

void push16(struct cpu *cpu, u16 val, bool inc_cyc) {
  u8 low, high;

  if (inc_cyc)
    cpu->cyc++;

  low = val & 0x00FF;
  high = (val & 0xFF00) >> 8;

  write8(cpu, STACK_BASE + cpu->sp--, high, inc_cyc);
  write8(cpu, STACK_BASE + cpu->sp--, low, inc_cyc);
}

u8 pop8(struct cpu *cpu, bool inc_cyc) {
  if (inc_cyc)
    cpu->cyc += 2;

  return read8(cpu, STACK_BASE + ++cpu->sp, inc_cyc);
}

u16 pop16(struct cpu *cpu, bool inc_cyc) {
  u8 low, high;

  if (inc_cyc)
    cpu->cyc += 2;

  low = read8(cpu, STACK_BASE + ++cpu->sp, inc_cyc);
  high = read8(cpu, STACK_BASE + ++cpu->sp, inc_cyc);

  return low | (high << 8);
}