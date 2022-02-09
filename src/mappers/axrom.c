#include "../include/mappers.h"
#include "../include/cart.h"
#include "../include/ppu.h"
#include "../include/util.h"

u8 axrom_prg_bank = 0;

u8 axrom_cpu_read(nes_t *nes, u16 addr) {
  if (addr >= 0x8000 && addr <= 0xFFFF) {
    u16 offset = addr - 0x8000;
    return nes->cart->prg[0x8000 * axrom_prg_bank + offset];
  }
  printf("axrom_cpu_read: ??\n");
}

u8 axrom_ppu_read(nes_t *nes, u16 addr) {
  return nes->cart->chr[mirror_ppu_addr(addr, nes->mapper->mirror_type)];
}

void axrom_cpu_write(nes_t *nes, u16 addr, u8 val) {
  // Single register: $8000-$FFFF 32K PRG ROM select
  if (addr >= 0x8000 && addr <= 0xFFFF) {
    axrom_prg_bank = val & 0x7;
//    nes->mapper->mirror_type = GET_BIT(val, 4) ? MT_1SCR_A : MT_1SCR_B;
    nes->mapper->mirror_type = MT_1SCR_A;
  }
}

void axrom_ppu_write(nes_t *nes, u16 addr, u8 val) {
  nes->cart->chr[mirror_ppu_addr(addr, nes->mapper->mirror_type)] = val;
}