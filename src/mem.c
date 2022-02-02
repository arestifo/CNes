#include "../include/mem.h"
#include "../include/ppu.h"
#include "../include/cpu.h"
#include "../include/cart.h"
#include "../include/apu.h"

// Split PRG and CHR ROM into 4K pages and swap their respective pointers around
void cpu_write8(nes_t *nes, u16 addr, u8 val) {
  if (addr >= 0x2000 && addr <= 0x3FFF) {
    ppu_reg_write(nes, addr % 8, val);
  } else if (addr == CONTROLLER1_PORT) {
    if (val & 1) {
      nes->ctrl1_sr = nes->ctrl1_sr_buf;
      nes->controllers_polling = true;
    } else {
      nes->controllers_polling = false;
    }
  } else if (addr == OAM_DMA_ADDR) {
    // Performs CPU -> PPU OAM DMA. Suspends the CPU for 513 or 514 cycles
    cpu_oam_dma(nes, val << 8);
  } else if (addr >= 0x4000 && addr <= 0x4017) {
    apu_write(nes, addr, val);
  } else {
    nes->cpu->mem[addr] = val;
  }
}

u8 cpu_read8(nes_t *nes, u16 addr) {
  cpu_t *cpu = nes->cpu;

  if (addr <= 0x1FFF) {
    // 2KB internal ram
    return cpu->mem[addr % 0x0800];
  } else if (addr >= 0x2000 && addr <= 0x3FFF) {
    // PPU registers ($2000-$2007) are mirrored from $2008-$3FFF
    return ppu_reg_read(nes, addr & 7);
  } else if (addr == CONTROLLER1_PORT) {
    u8 retval = nes->ctrl1_sr;

    nes->ctrl1_sr >>= 1;

    return (retval & 1) | 0x40;
  } else if (addr == CONTROLLER2_PORT) {
    // TODO Controller 2 reads (low priority)
  } else if (addr == 0x4015) {
    // APU status register
    return apu_read(nes, addr);
  } else if (addr >= 0x4018 && addr <= 0x401F) {
    printf("cpu_read8: reading cpu test mode registers is not supported.\n");
    exit(EXIT_FAILURE);
  } else if (addr >= 0x4020 && addr <= 0xFFFF) {
    // TODO: PRG ROM, PRG RAM, and mapper registers

    // TODO: THIS IS NOT EXTENSIBLE! USE MAPPER FUNCTIONS INSTEAD
    if (addr >= 0x8000 && addr <= 0xFFFF) {
      if (nes->cart->header.prgrom_n == 1)
        return cpu->mem[0x8000 + (addr % 0x4000)];  // NROM-128 has last 16k mirror the first 16k
      else
        return cpu->mem[addr];  // NROM-256
    }
    printf("cpu_read8: invalid read from $%04X\n", addr); // TODO
  } else {
    printf("cpu_read8: invalid read from $%04X\n", addr);
    exit(EXIT_FAILURE);
  }
  return 0;
}

// Write 16-bit value to memory in little-endian format
void cpu_write16(nes_t *nes, u16 addr, u16 val) {
  u8 lo = val & 0x00FF;
  u8 hi = (val & 0xFF00) >> 8;

  cpu_write8(nes, addr, lo);
  cpu_write8(nes, addr + 1, hi);
}

// Read 16-bit value from memory stored in little-endian format
u16 cpu_read16(nes_t *nes, u16 addr) {
  u8 lo = cpu_read8(nes, addr);
  u8 hi = cpu_read8(nes, addr + 1);

  return lo | (hi << 8);
}

void cpu_push8(nes_t *nes, u8 val) {
  cpu_write8(nes, STACK_BASE + nes->cpu->sp--, val);
}

void cpu_push16(nes_t *nes, u16 val) {
  u8 lo = val & 0x00FF;
  u8 hi = (val & 0xFF00) >> 8;

  cpu_write8(nes, STACK_BASE + nes->cpu->sp--, hi);
  cpu_write8(nes, STACK_BASE + nes->cpu->sp--, lo);
}

u8 cpu_pop8(nes_t *nes) {
  return cpu_read8(nes, STACK_BASE + ++nes->cpu->sp);
}

u16 cpu_pop16(nes_t *nes) {
  u8 lo = cpu_read8(nes, STACK_BASE + ++nes->cpu->sp);
  u8 hi = cpu_read8(nes, STACK_BASE + ++nes->cpu->sp);

  return lo | (hi << 8);
}