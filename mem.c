#include "include/mem.h"

void cpu_write8(struct cpu *cpu, u16 addr, u8 val, bool inc_cyc) {
  if (inc_cyc)
    cpu->cyc++;

  cpu->mem[addr] = val;
}

u8 cpu_read8(struct cpu *cpu, u16 addr, bool inc_cyc) {
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
    printf("cpu_read8: reading cpu test mode registers is not supported.\n");
    exit(EXIT_FAILURE);
  } else if (addr >= 0x4020 && addr <= 0xFFFF) {
    // TODO: PRG ROM, PRG RAM, and mapper registers
    // TODO: This only works for *16K* mapper 0!

    // 16K Mapper 0 ($C000-$FFFF is a mirror of $8000-$BFFF)
    if (addr >= 0x8000 && addr <= 0xFFFF)
      return cpu->mem[0x8000 + (addr % 0x4000)];
    printf("cpu_read8: invalid read from 0x%04X\n", addr); // TODO
  } else {
    printf("cpu_read8: invalid read from 0x%04X\n", addr);
    exit(EXIT_FAILURE);
  }
  return 0;
}

// Write 16-bit value to memory in little-endian format
void cpu_write16(struct cpu *cpu, u16 addr, u16 val, bool inc_cyc) {
  u8 low, high;

  low = val & 0x00FF;
  high = (val & 0xFF00) >> 8;

  cpu_write8(cpu, addr, low, inc_cyc);
  cpu_write8(cpu, addr + 1, high, inc_cyc);
}

// Read 16-bit value from memory stored in little-endian format
u16 cpu_read16(struct cpu *cpu, u16 addr, bool inc_cyc) {
  u8 low, high;

  low = cpu_read8(cpu, addr, inc_cyc);
  high = cpu_read8(cpu, addr + 1, inc_cyc);

  return low | (high << 8);
}

inline void write8(struct cpu *cpu, u16 addr, u8 val) {
  cpu_write8(cpu, addr, val, true);
}

inline void write16(struct cpu *cpu, u16 addr, u16 val) {
  cpu_write16(cpu, addr, val, true);
}

inline u8 read8(struct cpu *cpu, u16 addr) {
  return cpu_read8(cpu, addr, true);
}

inline u16 read16(struct cpu *cpu, u16 addr) {
  return cpu_read16(cpu, addr, true);
}

inline void dummy_write8(struct cpu *cpu, u16 addr, u8 val) {
  cpu_write8(cpu, addr, val, false);
}

inline void dummy_write16(struct cpu *cpu, u16 addr, u16 val) {
  cpu_write16(cpu, addr, val, false);
}

inline u8 dummy_read8(struct cpu *cpu, u16 addr) {
  return cpu_read8(cpu, addr, false);
}

inline u16 dummy_read16(struct cpu *cpu, u16 addr) {
  return cpu_read16(cpu, addr, false);
}

void push8(struct cpu *cpu, u8 val) {
  cpu->cyc++;

  write8(cpu, STACK_BASE + cpu->sp--, val);
}

void push16(struct cpu *cpu, u16 val) {
  u8 low, high;

  cpu->cyc++;

  low = val & 0x00FF;
  high = (val & 0xFF00) >> 8;

  write8(cpu, STACK_BASE + cpu->sp--, high);
  write8(cpu, STACK_BASE + cpu->sp--, low);
}

u8 pop8(struct cpu *cpu) {
  cpu->cyc++;

  return read8(cpu, STACK_BASE + ++cpu->sp);
}

u16 pop16(struct cpu *cpu) {
  u8 low, high;

  cpu->cyc++;

  low = read8(cpu, STACK_BASE + ++cpu->sp);
  high = read8(cpu, STACK_BASE + ++cpu->sp);

  return low | (high << 8);
}