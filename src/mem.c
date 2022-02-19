#include "include/mem.h"
#include "include/ppu.h"
#include "include/cpu.h"
#include "include/cart.h"
#include "include/apu.h"
#include "include/mappers.h"
#include "include/util.h"

void cpu_write8(nes_t *nes, u16 addr, u8 val) {
  if (addr <= 0x1FFF) {
    nes->cpu->mem[addr % 0x0800] = val;
  } else if (addr >= 0x2000 && addr <= 0x3FFF) {
    ppu_reg_write(nes, addr % 8, val);
  } else if (addr == CONTROLLER1_PORT) {
//    nes->ctrl1_sr = nes->ctrl1_sr_buf;
    printf("cpu_write8: ctrl1 write val=$%02X ctrl1_sr=$%02X ctrl1_sr_buf=$%02X\n", val, nes->ctrl1_sr,
           nes->ctrl1_sr_buf);
    if (val & 1)
      nes->ctrl1_sr = nes->ctrl1_sr_buf;
  } else if (addr == OAM_DMA_ADDR) {
    // Performs CPU -> PPU OAM DMA. Suspends the CPU for 513 or 514 cycles
    nes->cpu->do_oam_dma = true;
    nes->cpu->oam_dma_base = val << 8;
  } else if (addr >= 0x4000 && addr <= 0x4017) {
    apu_write(nes, addr, val);
  } else if (addr >= 0x4020 && addr <= 0xFFFF) {
    nes->mapper->cpu_write(nes, addr, val);
  }
}

u8 cpu_read8(nes_t *nes, u16 addr) {
  if (addr <= 0x1FFF) {
    // 2KB internal ram
    return nes->cpu->mem[addr % 0x0800];
  } else if (addr >= 0x2000 && addr <= 0x3FFF) {
    // PPU registers ($2000-$2007) are mirrored from $2008-$3FFF
    return ppu_reg_read(nes, addr & 7);
  } else if (addr == CONTROLLER1_PORT) {
    u8 retval = nes->ctrl1_sr & 1;

    // Shift controller SR at most once per instruction
    nes->ctrl1_sr >>= 1;

    printf("cpu_read8: ctrl1 read cpu op=%s pc=$%04X retval=$%02X ctrl1_sr_buf=$%02X ticks=%lu mode=%d op_cyc=%d\n",
           cpu_opcode_tos(nes->cpu->op.code), nes->cpu->pc, retval, nes->ctrl1_sr_buf, nes->cpu->ticks,
           nes->cpu->op.mode, nes->cpu->op.cyc);
    return retval;
  } else if (addr == CONTROLLER2_PORT) {
    // TODO Controller 2 reads (low priority)
  } else if (addr == 0x4015) {
    // APU status register
    return apu_read(nes, addr);
  } else if (addr >= 0x4018 && addr <= 0x401F) {
    crash_and_burn("cpu_read8: reading cpu test mode registers is not supported.\n");
  } else if (addr >= 0x4020 && addr <= 0xFFFF) {
    // Cartridge space; read value from mapper
    return nes->mapper->cpu_read(nes, addr);
  } else {
//    printf("cpu_read8: invalid read from $%04X\n", addr);
//    exit(EXIT_FAILURE);
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
//  printf("cpu_push8: push pc=$%04X val=$%02X base=$%04X\n", nes->cpu->pc, val, STACK_BASE + nes->cpu->sp);
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