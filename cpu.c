#include "include/cpu.h"
#include "include/mem.h"
#include "include/util.h"
#include "include/cart.h"
#include "include/ppu.h"

const int OPERAND_SIZES[] = {2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0};

void cpu_init(nes_t *nes) {
  cpu_t *cpu;

  // NES power up state
  cpu = nes->cpu;
  cpu->a = 0;
  cpu->x = 0;
  cpu->y = 0;
  cpu->cyc = 7;
  cpu->p = 0x24;
  cpu->sp = 0xFD;
  cpu->irq_pending = false;
  cpu->nmi_pending = false;

  // Load PRG ROM into CPU memory
  // TODO: Support more than mapper 0 here (bank switching)
  memcpy(cpu->mem + 0x8000, nes->cart->prg_rom,
         PRGROM_BLOCK_SZ * nes->cart->header.prgrom_n);

  // Set the program counter to the right place
//  cpu->pc = 0xC000;  // TODO: REMOVE! This is just for nestest
  cpu->pc = cpu_read16(nes, VEC_RESET);
}

u16 resolve_addr(nes_t *nes, u16 addr, addrmode_t mode) {
  u8 low, high;
  cpu_t *cpu;

  cpu = nes->cpu;
  switch (mode) {
    case ABS_IDX_X:
      return addr + cpu->x;
    case ABS_IDX_Y:
      return addr + cpu->y;
    case ABS:
    case ZP:
      return addr;
    case ZP_IDX_X:
      return (addr + cpu->x) & 0xFF;
    case ZP_IDX_Y:
      return (addr + cpu->y) & 0xFF;
    case REL:
      return cpu->pc + ((s8) addr);
    case ABS_IND:
      // The only instruction to use this addrmode_t is JMP, which has a bug
      // where the LSB is read correctly but the MSB is not read from across
      // the page boundary, but rather wrapped around the page
      // For example, JMP ($02FF) *should* read the destination LSB from $02FF
      // and from $0300, but incorrectly reads $02FF and $0200 instead
      if (PAGE_CROSSED(addr, addr + 1)) {
        low = cpu_read8(nes, addr);
        high = cpu_read8(nes, addr & 0xFF00);
        return low | (high << 8);
      }
      return cpu_read16(nes, addr);
    case ZP_IDX_IND:
      low = cpu_read8(nes, (addr + cpu->x) & 0xFF);
      high = cpu_read8(nes, (addr + cpu->x + 1) & 0xFF);

      return low | (high << 8);
    case ZP_IND_IDX_Y:
      low = cpu_read8(nes, addr);
      high = cpu_read8(nes, (addr + 1) & 0xFF);

      return (low | (high << 8)) + cpu->y;
    case IMPL_ACCUM:
    case IMM:
    default:
      printf("resolve_addr: requested vram_addr for invalid vram_addr mode %d\n", mode);
      return 0;
  }
}

static u16
cpu_resolve_addr(nes_t *nes, u16 addr, addrmode_t mode, bool is_write) {
  u8 low, high;
  u16 retval;
  cpu_t *cpu;

  cpu = nes->cpu;
  switch (mode) {
    case ABS:
    case ZP:
    case REL:
      break;
    case ABS_IND:
      cpu->cyc += 2;
      break;
    case ABS_IDX_X:
      if (PAGE_CROSSED(addr, addr + cpu->x) || is_write)
        cpu->cyc++;
      break;
    case ABS_IDX_Y:
      if (PAGE_CROSSED(addr, addr + cpu->y) || is_write)
        cpu->cyc++;
      break;
    case ZP_IDX_X:
    case ZP_IDX_Y:
      cpu->cyc++;
      break;
    case ZP_IDX_IND:
      cpu->cyc += 3;
      break;
    case ZP_IND_IDX_Y:
      low = cpu_read8(nes, addr);
      high = cpu_read8(nes, (addr + 1) & 0xFF);
      retval = low | (high << 8);
      if (PAGE_CROSSED(retval, retval + cpu->y) || is_write)
        cpu->cyc++;
      cpu->cyc += 2;
      return retval + cpu->y;
    case IMPL_ACCUM:
    case IMM:
    default:
      printf("resolve_addr: requested vram_addr for invalid vram_addr mode\n");
      return 0;
  }
  return resolve_addr(nes, addr, mode);
}

// Gets the addressing mode from an opcode. Based on the chart from:
// https://wiki.nesdev.com/w/index.php/CPU_unofficial_opcodes
// I like this method vs. a simple lookup table because this loosely emulates how the 6502
// internally decodes instructions.
addrmode_t get_addrmode(u8 opcode) {
  u8 mode_i;

  mode_i = opcode % 0x20;
  if (mode_i == 0x00 || mode_i == 0x09 || mode_i == 0x02) {
    if (opcode == 0x20)  // JSR
      return ABS;
    if (opcode != 0x00 && opcode != 0x40 && opcode != 0x60)  // INTR_BRK, RTI, RTS
      return IMM;
  } else if (mode_i >= 0x14 && mode_i <= 0x16) {
    if (opcode == 0x96 || opcode == 0xB6)  // STX, LDX
      return ZP_IDX_Y;
    return ZP_IDX_X;
  } else if (mode_i >= 0x1C && mode_i <= 0x1E) {
    if (opcode == 0xBE)  // LDX
      return ABS_IDX_Y;
    return ABS_IDX_X;
  } else if (mode_i == 0x19) return ABS_IDX_Y;
  else if (mode_i >= 0x04 && mode_i <= 0x06) return ZP;
  else if (mode_i == 0x01) return ZP_IDX_IND;
  else if (mode_i == 0x11) return ZP_IND_IDX_Y;
  else if (mode_i == 0x10) return REL;
  else if (mode_i >= 0x0C && mode_i <= 0x0E && opcode != 0x6C) return ABS;
  else if (opcode == 0x6C) return ABS_IND;  // JMP
  return IMPL_ACCUM;
}

// TODO: If possible, add interrupt hijacking support
// https://wiki.nesdev.com/w/index.php/CPU_interrupts
static void cpu_interrupt(nes_t *nes, interrupt_t type) {
  cpu_t *cpu;
  u16 vec;

  cpu = nes->cpu;
  cpu_push16(nes, cpu->pc);
  if (type == INTR_BRK) {
    SET_BIT(cpu->p, B_BIT, 1);
    vec = VEC_IRQ;
  } else if (type == INTR_IRQ) {
    SET_BIT(cpu->p, B_BIT, 0);
    vec = VEC_IRQ;
  } else if (type == INTR_NMI) {
    SET_BIT(cpu->p, B_BIT, 0);
    vec = VEC_NMI;
  } else {
    printf("cpu_interrupt: invalid interrupt type %d\n", type);
    exit(EXIT_FAILURE);
  }

  // Push processor status and disable interrupts
  cpu_push8(nes, cpu->p);
  SET_BIT(cpu->p, I_BIT, 1);

  // Jump to interrupt handler
  cpu->pc = cpu_read16(nes, vec);
}

void cpu_tick(nes_t *nes) {
  // Get opcode at current PC
  u8 opcode, old_carry;
  u16 operand, addr, result;
  addrmode_t mode;
  bool v_cond;
  cpu_t *cpu;

  // Output current CPU state for debugging
  // We have to re-do all instruction fetching here because we don't want to
  // increment cycles
  cpu = nes->cpu;
#ifdef LOG_OUTPUT
  u8 debug_opcode;
  u16 debug_operand;
  addrmode_t debug_mode;

  debug_opcode = cpu_read8(nes, cpu->pc);
  debug_mode = get_addrmode(debug_opcode);
  if (OPERAND_SIZES[debug_mode] == 1)
    debug_operand = cpu_read8(nes, cpu->pc + 1);
  else if (OPERAND_SIZES[debug_mode] == 2)
    debug_operand = cpu_read16(nes, cpu->pc + 1);
  else  // Just to quiet the compiler
    debug_operand = 0;
  dump_cpu(nes, debug_opcode, debug_operand, debug_mode);
#endif

  // Check for interrupts
  if (cpu->irq_pending) {
    cpu_interrupt(nes, INTR_IRQ);
    cpu->irq_pending = false;
  }
  if (cpu->nmi_pending) {
    cpu_interrupt(nes, INTR_NMI);
    cpu->nmi_pending = false;
  }

  opcode = cpu_read8(nes, cpu->pc);
  mode = get_addrmode(opcode);

  if (OPERAND_SIZES[mode] == 1)
    operand = cpu_read8(nes, cpu->pc + 1);
  else if (OPERAND_SIZES[mode] == 2)
    operand = cpu_read16(nes, cpu->pc + 1);

  cpu->pc += OPERAND_SIZES[mode] + 1;  // +1 for the opcode itself
  cpu->cyc += OPERAND_SIZES[mode] + 1;
  switch (opcode) {
    case 0x4C:  // JMP
    case 0x6C:
      cpu->pc = cpu_resolve_addr(nes, operand, mode, false);
      break;
    case 0xA2:  // LDX
    case 0xA6:
    case 0xB6:
    case 0xAE:
    case 0xBE:
      if (mode == IMM)
        cpu->x = operand;
      else {
        addr = cpu_resolve_addr(nes, operand, mode, false);
        cpu->x = cpu_read8(nes, addr);
        cpu->cyc++;
      }
      cpu_set_nz(nes, cpu->x);
      break;
    case 0x20:  // JSR
      addr = cpu_resolve_addr(nes, operand, mode, false);
      cpu_push16(nes, cpu->pc - 1);
      cpu->pc = addr;
      cpu->cyc += 3;
      break;
    case 0x60:  // RTS
      addr = cpu_pop16(nes) + 1;
      cpu->pc = addr;
      cpu->cyc += 5;  // From adjusting the PC (not sure why JSR doesn't need this)
      break;
    case 0xEA:  // NOP
      cpu->cyc++;
      break;
    case 0x38:  // SEC
      SET_BIT(cpu->p, C_BIT, 1);
      cpu->cyc++;
      break;
    case 0xF8:  // SED
      SET_BIT(cpu->p, D_BIT, 1);
      cpu->cyc++;
      break;
    case 0x78:  // SEI
      SET_BIT(cpu->p, I_BIT, 1);
      cpu->cyc++;
      break;
    case 0x18:  // CLC
      SET_BIT(cpu->p, C_BIT, 0);
      cpu->cyc++;
      break;
    case 0xD8:  // CLD
      SET_BIT(cpu->p, D_BIT, 0);
      cpu->cyc++;
      break;
    case 0x58:  // CLI
      SET_BIT(cpu->p, I_BIT, 0);
      cpu->cyc++;
      break;
    case 0xB8:  // CLV
      SET_BIT(cpu->p, V_BIT, 0);
      cpu->cyc++;
      break;
    case 0x90:  // BCC
      addr = cpu_resolve_addr(nes, operand, mode, false);
      if (!(cpu->p & C_MASK)) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0xB0:  // BCS
      addr = cpu_resolve_addr(nes, operand, mode, false);
      if (cpu->p & C_MASK) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0xD0:  // BNE
      addr = cpu_resolve_addr(nes, operand, mode, false);
      if (!(cpu->p & Z_MASK)) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0xF0:  // BEQ
      addr = cpu_resolve_addr(nes, operand, mode, false);
      if (cpu->p & Z_MASK) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0x10:  // BPL
      addr = cpu_resolve_addr(nes, operand, mode, false);
      if (!(cpu->p & N_MASK)) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0x30:  // BMI
      addr = cpu_resolve_addr(nes, operand, mode, false);
      if (cpu->p & N_MASK) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0x50:  // BVC
      addr = cpu_resolve_addr(nes, operand, mode, false);
      if (!(cpu->p & V_MASK)) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0x70:  // BVS
      addr = cpu_resolve_addr(nes, operand, mode, false);
      if (cpu->p & V_MASK) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0xA9:  // LDA
    case 0xA5:
    case 0xB5:
    case 0xAD:
    case 0xBD:
    case 0xB9:
    case 0xA1:
    case 0xB1:
      if (mode == IMM)
        cpu->a = operand;
      else {
        addr = cpu_resolve_addr(nes, operand, mode, false);
        cpu->a = cpu_read8(nes, addr);
        cpu->cyc++;
      }
      cpu_set_nz(nes, cpu->a);
      break;
    case 0xA0:  // LDY
    case 0xA4:
    case 0xB4:
    case 0xAC:
    case 0xBC:
      if (mode == IMM) {
        cpu->y = operand;
      } else {
        addr = cpu_resolve_addr(nes, operand, mode, false);
        cpu->y = cpu_read8(nes, addr);
        cpu->cyc++;
      }
      cpu_set_nz(nes, cpu->y);
      break;
    case 0x85:  // STA
    case 0x95:
    case 0x8D:
    case 0x9D:
    case 0x99:
    case 0x81:
    case 0x91:
      addr = cpu_resolve_addr(nes, operand, mode, true);
      cpu_write8(nes, addr, cpu->a);
      cpu->cyc++;
      break;
    case 0x86:  // STX
    case 0x96:
    case 0x8E:
      addr = cpu_resolve_addr(nes, operand, mode, true);
      cpu_write8(nes, addr, cpu->x);
      cpu->cyc++;
      break;
    case 0x84:  // STY
    case 0x94:
    case 0x8C:
      addr = cpu_resolve_addr(nes, operand, mode, true);
      cpu_write8(nes, addr, cpu->y);
      cpu->cyc++;
      break;
    case 0x24:  // BIT: n and z flag behavior is slightly different so we can't
    case 0x2C:  // use cpu_set_nz()
      addr = cpu_resolve_addr(nes, operand, mode, false);
      result = cpu_read8(nes, addr);

      // Set flags
      SET_BIT(cpu->p, Z_BIT, !(result & cpu->a));
      SET_BIT(cpu->p, V_BIT, (result & V_MASK) >> V_BIT);
      SET_BIT(cpu->p, N_BIT, (result & N_MASK) >> N_BIT);
      cpu->cyc++;
      break;
    case 0x48:  // PHA
      cpu_push8(nes, cpu->a);
      cpu->cyc += 2;
      break;
    case 0x08:  // PHP
      cpu_push8(nes, cpu->p | B_MASK);
      cpu->cyc += 2;
      break;
    case 0x68:  // PLA
      cpu->a = cpu_pop8(nes);
      cpu_set_nz(nes, cpu->a);
      cpu->cyc += 3;
      break;
    case 0x28:  // PLP
      result = cpu_pop8(nes) | U_MASK;
      cpu->p = result & ~B_MASK;
      cpu->cyc += 3;
      break;
    case 0x29:  // AND
    case 0x25:
    case 0x35:
    case 0x2D:
    case 0x3D:
    case 0x39:
    case 0x21:
    case 0x31:
      if (mode == IMM)
        cpu->a &= operand;
      else {
        addr = cpu_resolve_addr(nes, operand, mode, false);
        cpu->a &= cpu_read8(nes, addr);
        cpu->cyc++;
      }
      cpu_set_nz(nes, cpu->a);
      break;
    case 0x09:  // ORA
    case 0x05:
    case 0x15:
    case 0x0D:
    case 0x1D:
    case 0x19:
    case 0x01:
    case 0x11:
      if (mode == IMM)
        cpu->a |= operand;
      else {
        addr = cpu_resolve_addr(nes, operand, mode, false);
        cpu->a |= cpu_read8(nes, addr);
        cpu->cyc++;
      }
      cpu_set_nz(nes, cpu->a);
      break;
    case 0x49:  // EOR
    case 0x45:
    case 0x55:
    case 0x4D:
    case 0x5D:
    case 0x59:
    case 0x41:
    case 0x51:
      if (mode == IMM)
        cpu->a ^= operand;
      else {
        addr = cpu_resolve_addr(nes, operand, mode, false);
        cpu->a ^= cpu_read8(nes, addr);
        cpu->cyc++;
      }
      cpu_set_nz(nes, cpu->a);
      break;
    case 0xC9:  // CMP
    case 0xC5:
    case 0xD5:
    case 0xCD:
    case 0xDD:
    case 0xD9:
    case 0xC1:
    case 0xD1:
      if (mode == IMM)
        result = operand;
      else {
        addr = cpu_resolve_addr(nes, operand, mode, false);
        result = cpu_read8(nes, addr);
        cpu->cyc++;
      }
      cpu_set_nz(nes, cpu->a - result);

      // Set carry bit
      SET_BIT(cpu->p, C_BIT, cpu->a >= result);
      break;
    case 0xE0:  // CPX
    case 0xE4:
    case 0xEC:
      if (mode == IMM)
        result = operand;
      else {
        addr = cpu_resolve_addr(nes, operand, mode, false);
        result = cpu_read8(nes, addr);
        cpu->cyc++;
      }
      cpu_set_nz(nes, cpu->x - result);

      // Set carry bit
      SET_BIT(cpu->p, C_BIT, cpu->x >= result);
      break;
    case 0xC0:  // CPY
    case 0xC4:
    case 0xCC:
      if (mode == IMM)
        result = operand;
      else {
        addr = cpu_resolve_addr(nes, operand, mode, false);
        result = cpu_read8(nes, addr);
        cpu->cyc++;
      }
      cpu_set_nz(nes, cpu->y - result);

      // Set carry bit
      SET_BIT(cpu->p, C_BIT, cpu->y >= result);
      break;
    case 0x69:  // ADC
    case 0x65:
    case 0x75:
    case 0x6D:
    case 0x7D:
    case 0x79:
    case 0x61:
    case 0x71:
      if (mode == IMM)
        result = operand;
      else {
        addr = cpu_resolve_addr(nes, operand, mode, false);
        result = cpu_read8(nes, addr);
        cpu->cyc++;
      }

      // Overflow detection
      // If neg + neg = pos, or pos + pos = neg
      // TODO: I really gotta clean up the overflow condition check
      old_carry = cpu->p & C_MASK;
      v_cond = ((cpu->a & 0x80) && (result & 0x80) && !((cpu->a + result + old_carry) & 0x80)) ||
               (!(cpu->a & 0x80) && !(result & 0x80) && ((cpu->a + result + old_carry) & 0x80));
      SET_BIT(cpu->p, V_BIT, v_cond);

      // Carry flag
      SET_BIT(cpu->p, C_BIT, cpu->a + result + old_carry > 0xFF);

      cpu->a += result + old_carry;
      cpu_set_nz(nes, cpu->a);
      break;
    case 0xE9:  // SBC
    case 0xE5:
    case 0xF5:
    case 0xED:
    case 0xFD:
    case 0xF9:
    case 0xE1:
    case 0xF1:
      // TODO: This is just a copy paste. Gross!
      if (mode == IMM)
        result = ~operand;
      else {
        addr = cpu_resolve_addr(nes, operand, mode, false);
        result = ~cpu_read8(nes, addr);
        cpu->cyc++;
      }

      // Overflow detection
      // If neg + neg = pos, or pos + pos = neg
      old_carry = cpu->p & C_MASK;
      v_cond = ((cpu->a & 0x80) && (result & 0x80) && !((cpu->a + result + old_carry) & 0x80)) ||
               (!(cpu->a & 0x80) && !(result & 0x80) && ((cpu->a + result + old_carry) & 0x80));
      SET_BIT(cpu->p, V_BIT, v_cond);

      // Carry flag checking uses 0xFFFF since result is a u16
      SET_BIT(cpu->p, C_BIT, cpu->a + result + old_carry > 0xFFFF);

      cpu->a += result + old_carry;
      cpu_set_nz(nes, cpu->a);
      break;
    case 0xE8:  // INX
      cpu->x++;
      cpu->cyc++;
      cpu_set_nz(nes, cpu->x);
      break;
    case 0xC8:  // INY
      cpu->y++;
      cpu->cyc++;
      cpu_set_nz(nes, cpu->y);
      break;
    case 0xCA:  // DEX
      cpu->x--;
      cpu->cyc++;
      cpu_set_nz(nes, cpu->x);
      break;
    case 0x88:  // DEY
      cpu->y--;
      cpu->cyc++;
      cpu_set_nz(nes, cpu->y);
      break;
    case 0xAA:  // TAX
      cpu->x = cpu->a;
      cpu->cyc++;
      cpu_set_nz(nes, cpu->x);
      break;
    case 0xA8:  // TAY
      cpu->y = cpu->a;
      cpu->cyc++;
      cpu_set_nz(nes, cpu->y);
      break;
    case 0xBA:  // TSX
      cpu->x = cpu->sp;
      cpu->cyc++;
      cpu_set_nz(nes, cpu->x);
      break;
    case 0x8A:  // TXA
      cpu->a = cpu->x;
      cpu->cyc++;
      cpu_set_nz(nes, cpu->a);
      break;
    case 0x9A:  // TXS
      cpu->sp = cpu->x;
      cpu->cyc++;
      break;
    case 0x98:  // TYA
      cpu->a = cpu->y;
      cpu->cyc++;
      cpu_set_nz(nes, cpu->a);
      break;
    case 0x40:  // RTI
      result = cpu_pop8(nes) | U_MASK;
      cpu->p = result & ~B_MASK;
      cpu->pc = cpu_pop16(nes);
      cpu->cyc += 5;
      break;
    case 0x4A:  // LSR
    case 0x46:
    case 0x56:
    case 0x4E:
    case 0x5E:
      if (mode == IMPL_ACCUM) {
        SET_BIT(cpu->p, C_BIT, cpu->a & C_MASK);
        cpu->a >>= 1;
        cpu->cyc++;
        cpu_set_nz(nes, cpu->a);
      } else {
        addr = cpu_resolve_addr(nes, operand, mode, true);
        result = cpu_read8(nes, addr);
        SET_BIT(cpu->p, C_BIT, result & C_MASK);

        // This looks confusing, but actually RMW instructions on the 6502
        // have a "dummy" write, and this emulates that.
        cpu_write8(nes, addr, result);
        result >>= 1;

        cpu_write8(nes, addr, result);
        cpu_set_nz(nes, result);
        cpu->cyc += 3;
      }
      break;
    case 0x0A:  // ASL
    case 0x06:
    case 0x16:
    case 0x0E:
    case 0x1E:
      if (mode == IMPL_ACCUM) {
        SET_BIT(cpu->p, C_BIT, (cpu->a & 0x80) >> 7);
        cpu->a <<= 1;
        cpu->cyc++;
        cpu_set_nz(nes, cpu->a);
      } else {
        addr = cpu_resolve_addr(nes, operand, mode, true);
        result = cpu_read8(nes, addr);
        SET_BIT(cpu->p, C_BIT, (result & 0x80) >> 7);

        cpu_write8(nes, addr, result);
        result <<= 1;

        cpu_write8(nes, addr, result);
        cpu_set_nz(nes, result);
        cpu->cyc += 3;
      }
      break;
    case 0x6A:  // ROR
    case 0x66:
    case 0x76:
    case 0x6E:
    case 0x7E:
      old_carry = cpu->p & C_MASK;
      if (mode == IMPL_ACCUM) {
        SET_BIT(cpu->p, C_BIT, cpu->a & C_MASK);
        cpu->a >>= 1;
        cpu->a |= old_carry << 7;
        cpu->cyc++;
        cpu_set_nz(nes, cpu->a);
      } else {
        addr = cpu_resolve_addr(nes, operand, mode, true);
        result = cpu_read8(nes, addr);
        SET_BIT(cpu->p, C_BIT, result & C_MASK);

        cpu_write8(nes, addr, result);
        result >>= 1;
        result |= old_carry << 7;

        cpu_write8(nes, addr, result);
        cpu_set_nz(nes, result);
        cpu->cyc += 3;
      }
      break;
    case 0x2A:  // ROL
    case 0x26:
    case 0x36:
    case 0x2E:
    case 0x3E:
      old_carry = cpu->p & C_MASK;
      if (mode == IMPL_ACCUM) {
        SET_BIT(cpu->p, C_BIT, (cpu->a & 0x80) >> 7);
        cpu->a <<= 1;
        cpu->a |= old_carry;
        cpu->cyc++;
        cpu_set_nz(nes, cpu->a);
      } else {
        addr = cpu_resolve_addr(nes, operand, mode, true);
        result = cpu_read8(nes, addr);
        SET_BIT(cpu->p, C_BIT, (result & 0x80) >> 7);

        cpu_write8(nes, addr, result);
        result <<= 1;
        result |= old_carry;

        cpu_write8(nes, addr, result);
        cpu_set_nz(nes, result);
        cpu->cyc += 3;
      }
      break;
    case 0xE6:  // INC
    case 0xF6:
    case 0xEE:
    case 0xFE:
      addr = cpu_resolve_addr(nes, operand, mode, true);
      result = cpu_read8(nes, addr);

      cpu_write8(nes, addr, result);
      result++;

      cpu_set_nz(nes, result);
      cpu_write8(nes, addr, result);
      cpu->cyc += 3;
      break;
    case 0xC6:  // DEC
    case 0xD6:
    case 0xCE:
    case 0xDE:
      addr = cpu_resolve_addr(nes, operand, mode, true);
      result = cpu_read8(nes, addr);

      cpu_write8(nes, addr, result);
      result--;

      cpu_set_nz(nes, result);
      cpu_write8(nes, addr, result);
      cpu->cyc += 3;
      break;
    case 0x00:  // BRK
      cpu_interrupt(nes, INTR_BRK);
      break;
    default:
      printf("cpu_tick $%04X: unsupported op 0x%02X (%s)\n",
             cpu->pc - (OPERAND_SIZES[mode] + 1), opcode, opcode_tos(opcode));
      exit(EXIT_FAILURE);
  }
}

// Sets the negative and zero flags based on the result of a computation
// Can't set carry and overflow flag here (i think?) because they are set
// differently depending on the instruction
inline void cpu_set_nz(nes_t *nes, u8 result) {
  SET_BIT(nes->cpu->p, N_BIT, (result & 0x80) >> 7);
  SET_BIT(nes->cpu->p, Z_BIT, !result);
}

void dump_cpu(nes_t *nes, u8 opcode, u16 operand, addrmode_t mode) {
  u8 num_operands, low, high;
  u16 addr;
  u64 ppu_cyc;
  cpu_t *cpu;

  cpu = nes->cpu;
  fprintf(log_f, "%04X  ", cpu->pc);
  num_operands = OPERAND_SIZES[mode];
  low = operand & 0x00FF;
  high = (operand & 0xFF00) >> 8;
  switch (num_operands) {
    case 0:
      fprintf(log_f, "%02X       ", opcode);
      break;
    case 1:
      fprintf(log_f, "%02X %02X    ", opcode, low);
      break;
    case 2:
      fprintf(log_f, "%02X %02X %02X ", opcode, low, high);
      break;
    default:
      printf("dump_cpu: invalid operand count, wtf?\n");
      exit(EXIT_FAILURE);
  }
  fprintf(log_f, " %s", opcode_tos(opcode));
  switch (mode) {
    case ABS:
      // JSR and JMP absolute shouldn't display val @ address
      if (opcode == 0x20 || opcode == 0x4C)
        fprintf(log_f, " $%04X                       ", operand);
      else {
        // Don't access PPU regs
        if (!(operand >= 0x2000 && operand <= 0x3FFF))
          fprintf(log_f, " $%04X = %02X                  ", operand, cpu_read8(nes, operand));
        else
          fprintf(log_f, " $%04X                       ", operand);
      }
      break;
    case ABS_IND:
      addr = resolve_addr(nes, operand, mode);
      fprintf(log_f, " ($%04X) = %04X              ", operand, addr);
      break;
    case ABS_IDX_X:
    case ABS_IDX_Y:
      addr = resolve_addr(nes, operand, mode);
      fprintf(log_f, " $%04X,%s @ %04X = %02X         ", operand,
              mode == ABS_IDX_X ? "X" : "Y", addr, cpu_read8(nes, addr));
      break;
    case REL:
      // Add two because we haven't advanced the PC when this function is called
      addr = resolve_addr(nes, operand, mode) + 2;
      fprintf(log_f, " $%04X                       ", addr);
      break;
    case IMM:
      fprintf(log_f, " #$%02X                        ", operand);
      break;
    case ZP:
      fprintf(log_f, " $%02X = %02X                    ", operand,
              cpu_read8(nes, operand));
      break;
    case ZP_IDX_X:
    case ZP_IDX_Y:
      addr = resolve_addr(nes, operand, mode);
      fprintf(log_f, " $%02X,%s @ %02X = %02X             ", operand,
              mode == ZP_IDX_X ? "X" : "Y", addr, cpu_read8(nes, addr));
      break;
    case ZP_IDX_IND:
      addr = resolve_addr(nes, operand, mode);
      fprintf(log_f, " ($%02X,X) @ %02X = %04X = %02X    ", operand,
              (operand + cpu->x) & 0xFF, addr, cpu_read8(nes, addr));
      break;
    case ZP_IND_IDX_Y:
      addr = resolve_addr(nes, operand, mode);
      low = cpu_read8(nes, operand);
      high = cpu_read8(nes, (operand + 1) & 0xFF);
      fprintf(log_f, " ($%02X),Y = %04X @ %04X = %02X  ", operand,
              low | (high << 8), addr, cpu_read8(nes, addr));
      break;
    case IMPL_ACCUM:
      // For some dumb reason, accumulator arithmetic instructions have "A" as
      // the operand...
      // LSR ASL ROL ROR
      if (opcode == 0x4A || opcode == 0x0A || opcode == 0x2A || opcode == 0x6A)
        fprintf(log_f, " A                           ");
      else
        fprintf(log_f, "                             ");
      break;
    default:
      printf("dump_cpu: invalid addressing mode, wtf?\n");
      exit(EXIT_FAILURE);
  }

  // Print registers
  fprintf(log_f,
          "A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3u,%3u CYC:%llu",
          cpu->a, cpu->x, cpu->y, cpu->p, cpu->sp, nes->ppu->y,
          nes->ppu->x, cpu->cyc);

  // Mark where interrupts occur
  if (cpu->pc == cpu_read16(nes, VEC_NMI)) {
    fprintf(log_f, " **** NMI occurred ****");
  } else if (cpu->pc == cpu_read16(nes, VEC_IRQ)) {
    fprintf(log_f, " **** IRQ occurred ****");
  }

  fprintf(log_f, "\n");
}