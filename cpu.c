#include "include/cpu.h"

const int OPERAND_SIZES[] = {2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0};

void cpu_init(struct cpu *cpu, struct cart *cart) {
  // NES power up state
  cpu->a = 0;
  cpu->x = 0;
  cpu->y = 0;
  cpu->cyc = 0;
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

u16 get_addr(struct cpu *cpu, u16 addr, addrmode mode) {
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
      return read16(cpu->mem, addr);
    case ZP_IDX_IND:
      return read16(cpu->mem, addr + cpu->x);
    case ZP_IND_IDX_Y:
      return read16(cpu->mem, addr) + cpu->y;
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
  u8 opcode;
  u16 operand, addr;
  addrmode mode;

  opcode = read8(cpu->mem, cpu->pc);
  mode = get_addrmode(opcode);

  if (OPERAND_SIZES[mode] == 1)
    operand = read8(cpu->mem, cpu->pc + 1);
  else
    operand = read16(cpu->mem, cpu->pc + 1);

  // Output current CPU state for debugging
#ifdef DEBUG
  dump_cpu(cpu, opcode, operand, mode);
#endif
  cpu->pc += OPERAND_SIZES[mode] + 1;  // +1 for the opcode itself
  switch (opcode) {
    case 0x4C:  // JMP
    case 0x6C:
      cpu->pc = get_addr(cpu, operand, mode);
      break;
    default:
      printf("cpu_tick: unsupported op 0x%X\n", opcode);
      exit(EXIT_FAILURE);
  }
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
      addr = get_addr(cpu, operand, mode);
      fprintf(log_f, " ($%04X) = %04X              ", operand, addr);
      break;
    case ABS_IDX_X:
    case ABS_IDX_Y:
      addr = get_addr(cpu, operand, mode);
      fprintf(log_f, " $%04X,%s @ %04X = %02X         ", operand,
              mode == ABS_IDX_X ? "X" : "Y", addr, read8(cpu->mem, addr));
      break;
    case REL:
      // Add two because we haven't advanced the PC when this function is called
      addr = get_addr(cpu, operand, mode) + 2;
      fprintf(log_f, " $%04X                       ", addr);
      break;
    case IMM:
      fprintf(log_f, " #$%02X                        ", operand);
      break;
    case ZP:
      fprintf(log_f, " $%02X = %02X                    ", operand, read8(cpu->mem, operand));
      break;
    case ZP_IDX_X:
    case ZP_IDX_Y:
      addr = get_addr(cpu, operand, mode);
      fprintf(log_f, " $%02X,%s @ %02X = %02X             ", operand,
              mode == ZP_IDX_X ? "X" : "Y", addr, read8(cpu->mem, addr));
      break;
    case ZP_IDX_IND:
      addr = get_addr(cpu, operand, mode);
      fprintf(log_f, " ($%02X,X) @ %02X = %04X = %02X    ", operand,
              operand + cpu->x, addr, read8(cpu->mem, addr));
      break;
    case ZP_IND_IDX_Y:
      addr = get_addr(cpu, operand, mode);
      fprintf(log_f, " ($%02X),Y = %04X @ %04X = %02X  ", operand,
              read16(cpu->mem, operand), addr, read8(cpu->mem, addr));
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

// Gets assembler mnemonic from opcode
char *opcode_tos(u8 opcode) {
  switch (opcode) {
    case 0x69:
    case 0x65:
    case 0x75:
    case 0x6D:
    case 0x7D:
    case 0x79:
    case 0x61:
    case 0x71:
      return "ADC";
    case 0x29:
    case 0x25:
    case 0x35:
    case 0x2D:
    case 0x3D:
    case 0x39:
    case 0x21:
    case 0x31:
      return "AND";
    case 0x0A:
    case 0x06:
    case 0x16:
    case 0x0E:
    case 0x1E:
      return "ASL";
    case 0x90:
      return "BCC";
    case 0xB0:
      return "BCS";
    case 0xF0:
      return "BEQ";
    case 0x24:
    case 0x2C:
      return "BIT";
    case 0x30:
      return "BMI";
    case 0xD0:
      return "BNE";
    case 0x10:
      return "BPL";
    case 0x00:
      return "BRK";
    case 0x50:
      return "BVC";
    case 0x70:
      return "BVS";
    case 0x18:
      return "CLC";
    case 0xD8:
      return "CLD";
    case 0x58:
      return "CLI";
    case 0xB8:
      return "CLV";
    case 0xC9:
    case 0xC5:
    case 0xD5:
    case 0xCD:
    case 0xDD:
    case 0xD9:
    case 0xC1:
    case 0xD1:
      return "CMP";
    case 0xE0:
    case 0xE4:
    case 0xEC:
      return "CPX";
    case 0xC0:
    case 0xC4:
    case 0xCC:
      return "CPY";
    case 0xC6:
    case 0xD6:
    case 0xCE:
    case 0xDE:
      return "DEC";
    case 0xCA:
      return "DEX";
    case 0x88:
      return "DEY";
    case 0x49:
    case 0x45:
    case 0x55:
    case 0x4D:
    case 0x5D:
    case 0x59:
    case 0x41:
    case 0x51:
      return "EOR";
    case 0xE6:
    case 0xF6:
    case 0xEE:
    case 0xFE:
      return "INC";
    case 0xE8:
      return "INX";
    case 0xC8:
      return "INY";
    case 0x4C:
    case 0x6C:
      return "JMP";
    case 0x20:
      return "JSR";
    case 0xA9:
    case 0xA5:
    case 0xB5:
    case 0xAD:
    case 0xBD:
    case 0xB9:
    case 0xA1:
    case 0xB1:
      return "LDA";
    case 0xA2:
    case 0xA6:
    case 0xB6:
    case 0xAE:
    case 0xBE:
      return "LDX";
    case 0xA0:
    case 0xA4:
    case 0xB4:
    case 0xAC:
    case 0xBC:
      return "LDY";
    case 0x4A:
    case 0x46:
    case 0x56:
    case 0x4E:
    case 0x5E:
      return "LSR";
    case 0xEA:
      return "NOP";
    case 0x09:
    case 0x05:
    case 0x15:
    case 0x0D:
    case 0x1D:
    case 0x19:
    case 0x01:
    case 0x11:
      return "ORA";
    case 0x48:
      return "PHA";
    case 0x08:
      return "PHP";
    case 0x68:
      return "PLA";
    case 0x28:
      return "PLP";
    case 0x2A:
    case 0x26:
    case 0x36:
    case 0x2E:
    case 0x3E:
      return "ROL";
    case 0x6A:
    case 0x66:
    case 0x76:
    case 0x6E:
    case 0x7E:
      return "ROR";
    case 0x40:
      return "RTI";
    case 0x60:
      return "RTS";
    case 0xE9:
    case 0xE5:
    case 0xF5:
    case 0xED:
    case 0xFD:
    case 0xF9:
    case 0xE1:
    case 0xF1:
      return "SBC";
    case 0x38:
      return "SEC";
    case 0xF8:
      return "SED";
    case 0x78:
      return "SEI";
    case 0x85:
    case 0x95:
    case 0x8D:
    case 0x9D:
    case 0x99:
    case 0x81:
    case 0x91:
      return "STA";
    case 0x86:
    case 0x96:
    case 0x8E:
      return "STX";
    case 0x84:
    case 0x94:
    case 0x8C:
      return "STY";
    case 0xAA:
      return "TAX";
    case 0xA8:
      return "TAY";
    case 0xBA:
      return "TSX";
    case 0x8A:
      return "TXA";
    case 0x9A:
      return "TXS";
    case 0x98:
      return "TYA";
    default:
      printf("opcode_tos: unknown opcode 0x%02X\n", opcode);
      return "UK?";
  }
}