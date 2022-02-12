#include "../include/mappers.h"
#include "../include/cart.h"
#include "../include/ppu.h"
#include "../include/util.h"

// DEBUG INCLUDE
#include "../include/args.h"

u8 mmc1_sr_write_num = 0;
u8 mmc1_sr = 0;

// These are offsets (each of size _banksz) into the CART PRG and CHR rom, used for switching banks
u8 mmc1_prg_bank = 0xF;  // Init value for PRG bank seems to be 15, I got crashes with any other value
u8 mmc1_chr_bank0 = 0;
u8 mmc1_chr_bank1 = 0;

u16 mmc1_prg_banksz = 0x4000;  // 16K PRG ROM window, can be changed to 32K
u16 mmc1_chr_banksz = 0x1000;  // 8K CHR ROM window, can be changed to 4K

//u8 mmc1_prg_bankmode = 0;
u8 mmc1_prg_bankmode = 3;

// TODO: WRAM R/W protection
u8 mmc1_wram_enable = 0;

// Divide cart->prg into 16K chunks
// arr[0] = first chunk, arr[1] = second chunk, etc
static void mmc1_reg_write_helper(nes_t *nes, u8 reg_n, u8 val) {
  switch (reg_n) {
    case 0:
      // ******** Control register ********
      // Mirroring type
      // TODO: One-screen mirroring, this works for now but is hacky
      switch (val & 3) {
        case 0:
        case 1:
          printf("mmc1_reg_write_helper: single-screen mirorring might not work yet");
          break;
        case 2:
          nes->mapper->mirror_type = MT_VERTICAL;
          break;
        case 3:
          nes->mapper->mirror_type = MT_HORIZONTAL;
          break;
      }
      // ******** PRG ROM bank mode ********
      u8 prgrom_bankmode = (val & (3 << 2)) >> 2;
      mmc1_prg_bankmode = prgrom_bankmode;
      if (prgrom_bankmode == 0 || prgrom_bankmode == 1)
        mmc1_prg_banksz = 0x8000;  // 32K
      else
        mmc1_prg_banksz = 0x4000;

      // ******** CHR ROM bank mode (bit 4) ********
      mmc1_chr_banksz = GET_BIT(val, 4) ? 0x1000 : 0x2000;
      break;
    case 1:
      // ******** CHR ROM first bank select register ********
      if (mmc1_chr_banksz == 0x1000) {
        mmc1_chr_bank0 = val & 0x1F;  // Lower 5 bits select the 4K bank
      } else {
        mmc1_chr_bank0 = (val & 0x1F) >> 1;  // Select 8K bank, ignore lowest bit
      }
      break;
    case 2:
      // ******** CHR ROM second bank select register ********
      // This register is irrelevant in 8K CHR mode
      if (mmc1_chr_banksz == 0x1000) {
        mmc1_chr_bank1 = val & 0x1F;
      }
      break;
    case 3:
      // ******** PRG ROM bank select register ********
      if (mmc1_prg_banksz == 0x4000) {
        mmc1_prg_bank = val & 0xF;
      } else {
        // Ignore lower bit in 32K mode
        mmc1_prg_bank = (val & 0xE) >> 1;
      }
      break;
    default:
      printf("mmc1_reg_write_helper: invalid write to mmc1 reg_n $%d", reg_n);
  }
}

u8 mmc1_cpu_read(nes_t *nes, u16 addr) {
  cart_t *crt = nes->cart;
  switch (mmc1_prg_bankmode) {
    case 0:
    case 1:
      // 32K mode read
      if (addr >= 0x8000 && addr <= 0xFFFF) {
        u32 offset = addr - 0x8000;
        return crt->prg[mmc1_prg_bank * 0x8000 + offset];
      }
      break;
    case 2:
      // Fix first bank at $8000 and switch 16 KB bank at $C000
      if (addr >= 0x8000 && addr <= 0xBFFF) {
        // Simply point to first block in PRG
        return crt->prg[addr - 0x8000];
      } else if (addr >= 0xC000 && addr <= 0xFFFF) {
        // Switchable bank
        u32 offset = addr - 0xC000;
        return crt->prg[mmc1_prg_bank * 0x4000 + offset];
      }
      break;
    case 3:
      // Fix last bank at $C000 and switch 16 KB bank at $8000
      if (addr >= 0x8000 && addr <= 0xBFFF) {
        // Switchable bank
        u32 offset = addr - 0x8000;
        return crt->prg[mmc1_prg_bank * 0x4000 + offset];
      } else if (addr >= 0xC000 && addr <= 0xFFFF) {
        u32 offset = addr - 0xC000;
        return crt->prg[(crt->header.prgrom_n - 1) * 0x4000 + offset];
      }
      break;
    default:
      printf("mmc1_cpu_read: mmc1_prg_bankmode is invalid=%d\n", mmc1_prg_bankmode);
      exit(EXIT_FAILURE);
  }
  printf("mmc1_cpu_read: something went really wrong\n");
}

u8 mmc1_ppu_read(nes_t *nes, u16 addr) {
  cart_t *crt = nes->cart;

  u16 d_addr = mirror_ppu_addr(addr, nes->mapper->mirror_type);
  if (addr <= 0x3EFF) {
    switch (mmc1_chr_banksz) {
      case 0x1000:
        if (d_addr <= 0x0FFF) {
          return crt->chr[mmc1_chr_bank0 * 0x1000 + d_addr];
        } else if (d_addr >= 0x1000 && d_addr <= 0x1FFF) {
          u32 offset = d_addr - 0x1000;
          return crt->chr[mmc1_chr_bank1 * 0x1000 + offset];
        }
        break;
      case 0x2000:
        if (d_addr <= 0x1FFF) {
          return crt->chr[mmc1_chr_bank0 * 0x2000 + d_addr];
        }
        break;
      default:
        printf("mmc1_ppu_read: invalid CHR banksz, wtf?");
        exit(EXIT_FAILURE);
    }
  }
  return crt->chr[d_addr]; // TODO: ????
//  printf("mmc1_ppu_read: something weird is happening addr=$%04X d_addr=$%04X\n", addr, d_addr);
}

void mmc1_cpu_write(nes_t *nes, u16 addr, u8 val) {
  if (addr >= 0x8000 && addr <= 0xFFFF) {
    if (val & 0x80) {
      // Reset shift register to its initial state
      mmc1_sr_write_num = 0;
      mmc1_sr = 0;

      // control_reg = control_reg | 0x0C, which just affects this register
      mmc1_prg_bankmode = 3;
    } else {
      if (mmc1_sr_write_num == 4) {
        mmc1_sr_write_num = 0;

        // Bits 13 and 14 of the address form an index into the registers
        u8 reg_val = (mmc1_sr >> 1) | ((val & 1) << 4);
        u8 reg_n = (addr & 0x6000) >> 13;

        mmc1_reg_write_helper(nes, reg_n, reg_val);
        mmc1_sr = 0;  // Reset shift register after writing
        if (nes->args->cpu_log_output) {
          fprintf(nes->args->cpu_logf, "mmc1_cpu_write: writing mmc1 reg, reg_val=$%02X, reg_n=%d\n", reg_val, reg_n);
          fprintf(nes->args->cpu_logf, "mmc1_cpu_write: prg_bank=%d chr0_bank=%d chr1_bank=%d\n", mmc1_prg_bank, mmc1_chr_bank0, mmc1_chr_bank1);
        }
      } else {
        // Shift bit 0 of val into the shift register
        SET_BIT(mmc1_sr, 5, val & 1);
        mmc1_sr >>= 1;
        mmc1_sr_write_num++;
      }
    }
  }
}

void mmc1_ppu_write(nes_t *nes, u16 addr, u8 val) {
  // TODO: Is this correct?
  nes->cart->chr[mirror_ppu_addr(addr, nes->mapper->mirror_type)] = val;
}