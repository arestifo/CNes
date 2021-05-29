#include "include/mem.h"

void write8(u8 *mem, u16 addr, u8 val) {

}

u8 read8(u8 *mem, u16 addr) {
  if (addr <= 0x1FFF) {
    // 2KB internal ram
    return mem[addr % 0x0800];
  } else if (addr >= 0x2000 && addr <= 0x3FFF) {
    // TODO: PPU registers and their mirrors
  } else if (addr >= 0x4000 && addr <= 0x4017) {
    // TODO: APU registers
  } else if (addr >= 0x4018 && addr <= 0x401F) {
    printf("read8: reading cpu test mode registers is not supported.\n");
    return 0xFF;
  } else if (addr >= 0x4020 && addr <= 0xFFFF) {
    // TODO: PRG ROM, PRG RAM, and mapper registers
    // TODO: This only works for mapper 0!

    // Mapper 0
    if (addr >= 0x8000 && addr <= 0xFFFF)
      return mem[0x8000 + (addr % 0x4000)];
    printf("read8: invalid read from 0x%x\n", addr); // TODO
  } else {
    printf("read8: invalid read from 0x%x\n", addr);
    exit(EXIT_FAILURE);
  }
  return 0;
}

// Write 16-bit value to memory in little-endian format
void write16(u8 *mem, u16 addr, u16 val) {
  u8 low, high;

  low = val & 0x00FF;
  high = (val & 0xFF00) >> 8;

  write8(mem, addr, low);
  write8(mem, addr + 1, high);
}

// Read 16-bit value from memory stored in little-endian format
u16 read16(u8 *mem, u16 addr) {
  u8 low, high;

  low = read8(mem, addr);
  high = read8(mem, addr + 1);

  return low | (high << 8);
}