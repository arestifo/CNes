#include "include/cpu.h"
#include "include/mem.h"
#include "include/util.h"

const int OPERAND_SIZES[] = {2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0};

void cpu_init(struct cpu *cpu, struct cart *cart) {
  // NES power up state
  cpu->a = 0;
  cpu->x = 0;
  cpu->y = 0;
  cpu->cyc = 7;
  cpu->sr = 0x24;
  cpu->sp = 0xFD;

  // Set up CPU memory map
  cpu->mem = nes_malloc(CPU_MEMORY_SZ);

  // Load PRG ROM into CPU memory
  // TODO: Support more than mapper 0 here (bank switching)
  memcpy(cpu->mem + 0x8000, cart->prg_rom,
         PRGROM_BLOCK_SZ * cart->header.prgrom_n);

  // Set the program counter to the right place
  cpu->pc = 0xC000;  // TODO: REMOVE! This is just for nestest
//  cpu->pc = read16(cpu->mem, RESET_VEC);
}

u16 get_addr(struct cpu *cpu, u16 addr, addrmode mode, bool inc_cyc) {
  switch (mode) {
    case ABS:
    case ZP:
      return addr;
    case ABS_IDX_X:
    case ZP_IDX_X:
      return addr + cpu->x;
    case ABS_IDX_Y:
    case ZP_IDX_Y:
      return addr + cpu->y;
    case REL:
      return cpu->pc + ((s8) addr);
    case ABS_IND:
      return read16(cpu, addr, inc_cyc);
    case ZP_IDX_IND:
      return read16(cpu, addr + cpu->x, inc_cyc);
    case ZP_IND_IDX_Y:
      return read16(cpu, addr, inc_cyc) + cpu->y;
    case IMPL_ACCUM:
    case IMM:
    default:
      printf("get_addr: requested addr for invalid addr mode\n");
      return 0;
  }
}

// Gets the addressing mode from an opcode. Based on the chart from:
// https://wiki.nesdev.com/w/index.php/CPU_unofficial_opcodes
addrmode get_addrmode(u8 opcode) {
  u8 mode_i;

  mode_i = opcode % 0x20;
  if (mode_i == 0x00 || mode_i == 0x09 || mode_i == 0x02) {
    if (opcode == 0x20)  // JSR
      return ABS;
    if (opcode != 0x00 && opcode != 0x40 && opcode != 0x60)  // BRK, RTI, RTS
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
  else if (mode_i >= 0x0C && mode_i <= 0x0E) return ABS;
  else if (opcode == 0x6C) return ABS_IND;  // JMP
  return IMPL_ACCUM;
}

void cpu_tick(struct cpu *cpu) {
  // Get opcode at current PC
  u8 opcode, result;
  u16 operand, addr;
  addrmode mode;

  // Output current CPU state for debugging
  // We have to re-do all instruction fetching here because we don't want to
  // increment cycles
#ifdef DEBUG
  u8 debug_opcode;
  u16 debug_operand;
  addrmode debug_mode;

  debug_opcode = read8(cpu, cpu->pc, false);
  debug_mode = get_addrmode(debug_opcode);
  if (OPERAND_SIZES[debug_mode] == 1)
    debug_operand = read8(cpu, cpu->pc + 1, false);
  else if (OPERAND_SIZES[debug_mode] == 2)
    debug_operand = read16(cpu, cpu->pc + 1, false);
  else  // Just to quiet the compiler
    debug_operand = 0;
  dump_cpu(cpu, debug_opcode, debug_operand, debug_mode);
#endif

  opcode = read8(cpu, cpu->pc, true);
  mode = get_addrmode(opcode);

  if (OPERAND_SIZES[mode] == 1)
    operand = read8(cpu, cpu->pc + 1, true);
  else if (OPERAND_SIZES[mode] == 2)
    operand = read16(cpu, cpu->pc + 1, true);

  cpu->pc += OPERAND_SIZES[mode] + 1;  // +1 for the opcode itself
  // TODO: These opcodes were implemented in the order they appeared in nestest :)
  // TODO: rearrange them later for clarity
  switch (opcode) {
    case 0x4C:  // JMP
    case 0x6C:
      cpu->pc = get_addr(cpu, operand, mode, true);
      break;
    case 0xA2:  // LDX
    case 0xA6:
    case 0xB6:
    case 0xAE:
    case 0xBE:
      if (mode == IMM)
        cpu->x = operand;
      else {
        addr = get_addr(cpu, operand, mode, true);
        cpu->x = read8(cpu, addr, true);
      }
      set_nz(cpu, cpu->x);
      break;
    case 0x86:  // STX
    case 0x96:
    case 0x8E:
      addr = get_addr(cpu, operand, mode, true);
      write8(cpu, addr, operand, true);
      break;
    case 0x20:  // JSR
      addr = get_addr(cpu, operand, mode, true);
      push16(cpu, cpu->pc - 1, true);
      cpu->pc = addr;
      break;
    case 0x60:  // RTS
      addr = pop16(cpu, true) + 1;
      cpu->cyc++;  // From adjusting the PC (not sure why JSR doesn't need this)
      cpu->pc = addr;
      break;
    case 0xEA:  // NOP
      cpu->cyc++;
      break;
    case 0x38:  // SEC
      cpu->sr |= C_MASK;
      cpu->cyc++;
      break;
    case 0xF8:  // SED
      cpu->sr |= D_MASK;
      cpu->cyc++;
      break;
    case 0x78:  // SEI
      cpu->sr |= I_MASK;
      cpu->cyc++;
      break;
    case 0x18:  // CLC
      cpu->sr &= ~C_MASK;
      cpu->cyc++;
      break;
    case 0xD8:  // CLD
      cpu->sr &= ~D_MASK;
      cpu->cyc++;
      break;
    case 0x58:  // CLI
      cpu->sr &= ~I_MASK;
      cpu->cyc++;
      break;
    case 0xB8:  // CLV
      cpu->sr &= ~V_MASK;
      cpu->cyc++;
      break;
    case 0x90:  // BCC
      addr = get_addr(cpu, operand, mode, true);
      if (!(cpu->sr & C_MASK)) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0xB0:  // BCS
      addr = get_addr(cpu, operand, mode, true);
      if (cpu->sr & C_MASK) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0xD0:  // BNE
      addr = get_addr(cpu, operand, mode, true);
      if (!(cpu->sr & Z_MASK)) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0xF0:  // BEQ
      addr = get_addr(cpu, operand, mode, true);
      if (cpu->sr & Z_MASK) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0x10:  // BPL
      addr = get_addr(cpu, operand, mode, true);
      if (!(cpu->sr & N_MASK)) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0x30:  // BMI
      addr = get_addr(cpu, operand, mode, true);
      if (cpu->sr & N_MASK) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0x50:  // BVC
      addr = get_addr(cpu, operand, mode, true);
      if (!(cpu->sr & V_MASK)) {
        cpu->pc = addr;
        cpu->cyc++;
      }
      break;
    case 0x70:  // BVS
      addr = get_addr(cpu, operand, mode, true);
      if (cpu->sr & V_MASK) {
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
        addr = get_addr(cpu, operand, mode, true);
        cpu->a = read8(cpu, addr, true);
      }
      set_nz(cpu, cpu->a);
      break;
    case 0x85:  // STA
    case 0x95:
    case 0x8D:
    case 0x9D:
    case 0x99:
    case 0x81:
    case 0x91:
      addr = get_addr(cpu, operand, mode, true);
      write8(cpu, addr, cpu->a, true);
      break;
    case 0x24:  // BIT
    case 0x2C:
      addr = get_addr(cpu, operand, mode, true);
      result = read8(cpu, addr, true) & cpu->a;
      set_nz(cpu, result);

      // Set overflow bit
      if (result & 0x40)
        cpu->sr |= V_MASK;
      else
        cpu->sr &= ~V_MASK;
      break;
    case 0x48:  // PHA
      push8(cpu, cpu->a, true);
      break;
    case 0x08:  // PHP
      push8(cpu, cpu->sr | B_MASK, true);
      break;
    case 0x68:  // PLA
      cpu->a = pop8(cpu, true);
      set_nz(cpu, cpu->a);
      break;
    case 0x28:  // PLP
      // PLP ignores sr bits 4 and 5
      result = pop8(cpu, true) & ~0x30;
      cpu->sr = result;
      break;
    default:
      printf("cpu_tick: unsupported op 0x%02X (%s)\n", opcode,
             opcode_tos(opcode));
      exit(EXIT_FAILURE);
  }
}

// Sets the negative and zero flags based on the result of a computation
// Can't set carry and overflow flag here (i think?) because they are set
// differently depending on the instruction
void set_nz(struct cpu *cpu, u8 result) {
  // Negative flag
  if (result & 0x80)
    cpu->sr |= N_MASK;
  else
    cpu->sr &= ~N_MASK;

  // Zero flag
  if (!result)
    cpu->sr |= Z_MASK;
  else
    cpu->sr &= ~Z_MASK;
}

void cpu_destroy(struct cpu *cpu) {
  free(cpu->mem);
}

void dump_cpu(struct cpu *cpu, u8 opcode, u16 operand, addrmode mode) {
  u8 num_operands, low_op, high_op;
  u16 addr;

  fprintf(log_f, "%04X  ", cpu->pc);
  num_operands = OPERAND_SIZES[mode];
  low_op = operand & 0x00FF;
  high_op = (operand & 0xFF00) >> 8;
  switch (num_operands) {
    case 0:
      fprintf(log_f, "%02X       ", opcode);
      break;
    case 1:
      fprintf(log_f, "%02X %02X    ", opcode, low_op);
      break;
    case 2:
      fprintf(log_f, "%02X %02X %02X ", opcode, low_op, high_op);
      break;
    default:
      printf("dump_cpu: invalid operand count, wtf?\n");
      exit(EXIT_FAILURE);
  }
  fprintf(log_f, " %s", opcode_tos(opcode));
  switch (mode) {
    case ABS:
      fprintf(log_f, " $%04X                       ", operand);
      break;
    case ABS_IND:
      addr = get_addr(cpu, operand, mode, false);
      fprintf(log_f, " ($%04X) = %04X              ", operand, addr);
      break;
    case ABS_IDX_X:
    case ABS_IDX_Y:
      addr = get_addr(cpu, operand, mode, false);
      fprintf(log_f, " $%04X,%s @ %04X = %02X         ", operand,
              mode == ABS_IDX_X ? "X" : "Y", addr, read8(cpu, addr, false));
      break;
    case REL:
      // Add two because we haven't advanced the PC when this function is called
      addr = get_addr(cpu, operand, mode, false) + 2;
      fprintf(log_f, " $%04X                       ", addr);
      break;
    case IMM:
      fprintf(log_f, " #$%02X                        ", operand);
      break;
    case ZP:
      fprintf(log_f, " $%02X = %02X                    ", operand,
              read8(cpu, operand, false));
      break;
    case ZP_IDX_X:
    case ZP_IDX_Y:
      addr = get_addr(cpu, operand, mode, false);
      fprintf(log_f, " $%02X,%s @ %02X = %02X             ", operand,
              mode == ZP_IDX_X ? "X" : "Y", addr, read8(cpu, addr, false));
      break;
    case ZP_IDX_IND:
      addr = get_addr(cpu, operand, mode, false);
      fprintf(log_f, " ($%02X,X) @ %02X = %04X = %02X    ", operand,
              operand + cpu->x, addr, read8(cpu, addr, false));
      break;
    case ZP_IND_IDX_Y:
      addr = get_addr(cpu, operand, mode, false);
      fprintf(log_f, " ($%02X),Y = %04X @ %04X = %02X  ", operand,
              read16(cpu, operand, false), addr, read8(cpu, addr, false));
      break;
    case IMPL_ACCUM:
      fprintf(log_f, "                             ");
      break;
    default:
      printf("dump_cpu: invalid addressing mode, wtf?\n");
      exit(EXIT_FAILURE);
  }

  // Print registers
  // TODO: Add PPU cycles
  fprintf(log_f, "A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3d,%3d CYC:%llu\n",
          cpu->a, cpu->x, cpu->y, cpu->sr, cpu->sp, 0, 0, cpu->cyc);
}