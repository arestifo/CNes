#include "../include/mappers.h"
#include "../include/cart.h"
u8 nrom_cpu_read(nes_t *nes, u16 addr) {
  cart_t *cart = nes->cart;

  if (addr >= 0x8000 && addr <= 0xFFFF) {
    if (cart->header.prgrom_n == 1)
      return cart->prg[addr % 0x4000];  // NROM-128 has last 16k mirror the first 16k
    else
      return cart->prg[addr - 0x8000];  // NROM-256
  } else {
    printf("nrom_cpu_read: invalid mapperno read at $%04X\n", addr);
  }
}

u8 nrom_ppu_read(nes_t *nes, u16 addr) {
  return nes->cart->chr[mirror_ppu_addr(addr, nes->mapper->mirror_type)];
}

void nrom_cpu_write(nes_t *nes, u16 addr, u8 val) {
  // NROM has no registers!
  printf("nrom_cpu_write: caught junk write to $%04X=$%02X\n", addr, val);
}

void nrom_ppu_write(nes_t *nes, u16 addr, u8 val) {
  u16 eff_addr = mirror_ppu_addr(addr, nes->mapper->mirror_type);
//  if (eff_addr >= nes->cart->header.chrrom_n * CHRROM_BLOCK_SZ) {
//    printf("nrom_ppu_write: faulty eff_addr=$%04X max_sz=$%04X\n", eff_addr, nes->cart->header.chrrom_n * CHRROM_BLOCK_SZ);
//  }
  nes->cart->chr[eff_addr] = val;
}