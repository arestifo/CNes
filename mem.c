#include "include/mem.h"
#include "include/ppu.h"
#include "include/cpu.h"

void mem_write8(struct nes *nes, u16 addr, u8 val) {
  if (addr >= 0x2000 && addr <= 0x3FFF) {
    // TODO: PPU register writes
  } else {
    nes->cpu->mem[addr] = val;
  }
}

u8 mem_read8(struct nes *nes, u16 addr) {
  if (addr <= 0x1FFF) {
    // 2KB internal ram
    return nes->cpu->mem[addr % 0x0800];
  } else if (addr >= 0x2000 && addr <= 0x3FFF) {
    // PPU registers ($2000-$2007) are mirrored from $2008-$3FFF
    return ppu_read(nes, addr % 8);
  } else if (addr >= 0x4000 && addr <= 0x4017) {
    // TODO: APU registers and PPU OAM DMA
  } else if (addr >= 0x4018 && addr <= 0x401F) {
    printf("cpu_read8: reading cpu test mode registers is not supported.\n");
    exit(EXIT_FAILURE);
  } else if (addr >= 0x4020 && addr <= 0xFFFF) {
    // TODO: PRG ROM, PRG RAM, and mapper registers

    // 16K Mapper 0 ($C000-$FFFF is a mirror of $8000-$BFFF)
    if (addr >= 0x8000 && addr <= 0xFFFF)
      return nes->cpu->mem[0x8000 + (addr % 0x4000)];
    printf("cpu_read8: invalid read from 0x%04X\n", addr); // TODO
  } else {
    printf("cpu_read8: invalid read from 0x%04X\n", addr);
    exit(EXIT_FAILURE);
  }
  return 0;
}

void mem_write16(struct nes *nes, u16 addr, u16 val) {
  u8 low, high;

  low = val & 0x00FF;
  high = (val & 0xFF00) >> 8;

  mem_write8(nes, addr, low);
  mem_write8(nes, addr + 1, high);
}

u16 mem_read16(struct nes *nes, u16 addr) {
  u8 low, high;

  low = mem_read8(nes, addr);
  high = mem_read8(nes, addr + 1);

  return low | (high << 8);
}

void cpu_write8(struct nes *nes, u16 addr, u8 val) {
  nes->cpu->cyc++;

  mem_write8(nes, addr, val);
}

u8 cpu_read8(struct nes *nes, u16 addr) {
  nes->cpu->cyc++;

  return mem_read8(nes, addr);
}

// Write 16-bit value to memory in little-endian format
void cpu_write16(struct nes *nes, u16 addr, u16 val) {
  u8 low, high;

  low = val & 0x00FF;
  high = (val & 0xFF00) >> 8;

  cpu_write8(nes, addr, low);
  cpu_write8(nes, addr + 1, high);
}

// Read 16-bit value from memory stored in little-endian format
u16 cpu_read16(struct nes *nes, u16 addr) {
  u8 low, high;

  low = cpu_read8(nes, addr);
  high = cpu_read8(nes, addr + 1);

  return low | (high << 8);
}

void push8(struct nes *nes, u8 val) {
  nes->cpu->cyc++;

  cpu_write8(nes, STACK_BASE + nes->cpu->sp--, val);
}

void push16(struct nes *nes, u16 val) {
  u8 low, high;

  nes->cpu->cyc++;

  low = val & 0x00FF;
  high = (val & 0xFF00) >> 8;

  cpu_write8(nes, STACK_BASE + nes->cpu->sp--, high);
  cpu_write8(nes, STACK_BASE + nes->cpu->sp--, low);
}

u8 pop8(struct nes *nes) {
  nes->cpu->cyc++;

  return cpu_read8(nes, STACK_BASE + ++nes->cpu->sp);
}

u16 pop16(struct nes *nes) {
  u8 low, high;

  nes->cpu->cyc++;

  low = cpu_read8(nes, STACK_BASE + ++nes->cpu->sp);
  high = cpu_read8(nes, STACK_BASE + ++nes->cpu->sp);

  return low | (high << 8);
}