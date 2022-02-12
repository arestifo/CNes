#include "include/cpu.h"
#include "include/nes.h"
#include "include/mem.h"
#include "include/util.h"
#include "include/ppu.h"
#include "include/args.h"

#define OP_FUNC static inline void

// ************Internal CPU functions forward declarations ************
static void cpu_log_op(nes_t *nes);

// This function is run immediately after getting the opcode and puts the effective operand/address
// into operand
static bool cpu_get_operand_tick(nes_t *nes, u16 *operand);

// Thanks to https://www.nesdev.org/6502_cpu.txt for great NES 6502 documentation
// This is just here for logging purposes
const int OPERAND_SIZES[] = {2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0};

// TODO: Implement open-bus behavior. This is also unclean and should probably be behind some interface
u16 addr_bus = 0;
u8 data_bus = 0;

// Lookup tables
// Addressing mode indexed by opcode
addrmode_t cpu_op_addrmodes[CPU_NUM_OPCODES];

static void cpu_set_nz(nes_t *nes, u8 result) {
  SET_BIT(nes->cpu->p, N_BIT, (result & 0x80) >> 7);
  SET_BIT(nes->cpu->p, Z_BIT, !result);
}

// Function pointers to CPU instruction handlers
OP_FUNC oADC(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oAND(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oASL(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oBCC(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oBCS(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oBEQ(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oBIT(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oBMI(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oBNE(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oBPL(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oBRK(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oBVC(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oBVS(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oCLC(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oCLD(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oCLI(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oCLV(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oCMP(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oCPX(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oCPY(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oDEC(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oDEX(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oDEY(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oEOR(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oINC(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oINX(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oINY(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oJMP(nes_t *nes) {
  cpu_t *cpu = nes->cpu;
  switch (cpu->op.mode) {
    case ABS:
      assert(cpu->op.cyc == 0 || cpu->op.cyc == 1);
      if (cpu->op.cyc == 0) {
        SET_BYTE_LO(addr_bus, cpu_read8(nes, cpu->pc++));
        cpu->op.cyc++;
      } else {
        SET_BYTE_HI(addr_bus, cpu_read8(nes, cpu->pc));
        cpu->pc = addr_bus;
        cpu->fetch_op = true; // Done with this instruction
      }
      break;
    case ABS_IND:
      // This is the only instruction that utilizes absolute indirect addressing so just handle it here
      // instead of the generic read cycle handler
      assert(cpu->op.cyc >= 0 && cpu->op.cyc <= 3);
      if (cpu->op.cyc == 0) {
        // Fetch pointer lo
        SET_BYTE_LO(addr_bus, cpu_read8(nes, cpu->pc++));
        cpu->op.cyc++;
      } else if (cpu->op.cyc == 1) {
        // Fetch pointer hi
        SET_BYTE_HI(addr_bus, cpu_read8(nes, cpu->pc++));
        cpu->op.cyc++;
      } else if (cpu->op.cyc == 2) {
        // Fetch *pointer lo to PC, addr_bus now contains pointer
        SET_BYTE_LO(cpu->pc, cpu_read8(nes, addr_bus));
        cpu->op.cyc++;
      } else {
        // Final cycle, set PC hi byte to *(pointer + 1)
        // The pointer increment does not cross page boundaries, so basically don't increment the upper byte
        // of PC
        u16 hi_addr = ((addr_bus + 1) & 0xFF) | (addr_bus & ~0xFF);
        SET_BYTE_HI(cpu->pc, cpu_read8(nes, hi_addr));
        cpu->fetch_op = true;
      }
      break;
    default:
      crash_and_burn("oJMP: invalid JMP addressing mode");
  }
}

OP_FUNC oJSR(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oLDA(nes_t *nes) {
  u16 addr;
  if (cpu_get_operand_tick(nes, &addr)) {
    nes->cpu->fetch_op = true;
    nes->cpu->a = cpu_read8(nes, addr);
    cpu_set_nz(nes, nes->cpu->a);
  }
}

OP_FUNC oLDX(nes_t *nes) {
  u16 addr;
  if (cpu_get_operand_tick(nes, &addr)) {
    nes->cpu->fetch_op = true;
    nes->cpu->x = cpu_read8(nes, addr);
    cpu_set_nz(nes, nes->cpu->x);
  }
}

OP_FUNC oLDY(nes_t *nes) {
  u16 addr;
  if (cpu_get_operand_tick(nes, &addr)) {
    nes->cpu->fetch_op = true;
    nes->cpu->y = cpu_read8(nes, addr);
    cpu_set_nz(nes, nes->cpu->y);
  }
}

OP_FUNC oLSR(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oNOP(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oORA(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oPHA(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oPHP(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oPLA(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oPLP(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oROL(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oROR(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oRTI(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oRTS(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oSBC(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oSEC(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oSED(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oSEI(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oSTA(nes_t *nes) {
  u16 addr;
  if (cpu_get_operand_tick(nes, &addr)) {
    nes->cpu->fetch_op = true;
    cpu_write8(nes, addr, nes->cpu->a);
  }
}

OP_FUNC oSTX(nes_t *nes) {
  u16 addr;
  if (cpu_get_operand_tick(nes, &addr)) {
    nes->cpu->fetch_op = true;
    cpu_write8(nes, addr, nes->cpu->x);
  }
}

OP_FUNC oSTY(nes_t *nes) {
  u16 addr;
  if (cpu_get_operand_tick(nes, &addr)) {
    nes->cpu->fetch_op = true;
    cpu_write8(nes, addr, nes->cpu->y);
  }
}

OP_FUNC oTAX(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oTAY(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oTSX(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oTXA(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oTXS(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

OP_FUNC oTYA(nes_t *nes) {
  crash_and_burn("Instruction not implemented");
}

// TODO: Support unofficial opcodes (low priority)
// Maps opcodes to functions that handle them
// I'm putting this here because I don't want to forward declare all the op functions :)
void (*const cpu_op_fns[CPU_NUM_OPCODES])(nes_t *nes) = {
    oBRK, oORA, NULL, NULL, NULL, oORA, oASL, NULL, oPHP, oORA, oASL, NULL, NULL, oORA, oASL, NULL,
    oBPL, oORA, NULL, NULL, NULL, oORA, oASL, NULL, oCLC, oORA, NULL, NULL, NULL, oORA, oASL, NULL,
    oJSR, oAND, NULL, NULL, oBIT, oAND, oROL, NULL, oPLP, oAND, oROL, NULL, oBIT, oAND, oROL, NULL,
    oBMI, oAND, NULL, NULL, NULL, oAND, oROL, NULL, oSEC, oAND, NULL, NULL, NULL, oAND, oROL, NULL,
    oRTI, oEOR, NULL, NULL, NULL, oEOR, oLSR, NULL, oPHA, oEOR, oLSR, NULL, oJMP, oEOR, oLSR, NULL,
    oBVC, oEOR, NULL, NULL, NULL, oEOR, oLSR, NULL, oCLI, oEOR, NULL, NULL, NULL, oEOR, oLSR, NULL,
    oRTS, oADC, NULL, NULL, NULL, oADC, oROR, NULL, oPLA, oADC, oROR, NULL, oJMP, oADC, oROR, NULL,
    oBVS, oADC, NULL, NULL, NULL, oADC, oROR, NULL, oSEI, oADC, NULL, NULL, NULL, oADC, oROR, NULL,
    NULL, oSTA, NULL, NULL, oSTY, oSTA, oSTX, NULL, oDEY, NULL, oTXA, NULL, oSTY, oSTA, oSTX, NULL,
    oBCC, oSTA, NULL, NULL, oSTY, oSTA, oSTX, NULL, oTYA, oSTA, oTXS, NULL, NULL, oSTA, NULL, NULL,
    oLDY, oLDA, oLDX, NULL, oLDY, oLDA, oLDX, NULL, oTAY, oLDA, oTAX, NULL, oLDY, oLDA, oLDX, NULL,
    oBCS, oLDA, NULL, NULL, oLDY, oLDA, oLDX, NULL, oCLV, oLDA, oTSX, NULL, oLDY, oLDA, oLDX, NULL,
    oCPY, oCMP, NULL, NULL, oCPY, oCMP, oDEC, NULL, oINY, oCMP, oDEX, NULL, oCPY, oCMP, oDEC, NULL,
    oBNE, oCMP, NULL, NULL, NULL, oCMP, oDEC, NULL, oCLD, oCMP, NULL, NULL, NULL, oCMP, oDEC, NULL,
    oCPX, oSBC, NULL, NULL, oCPX, oSBC, oINC, NULL, oINX, oSBC, oNOP, NULL, oCPX, oSBC, oINC, NULL,
    oBEQ, oSBC, NULL, NULL, NULL, oSBC, oINC, NULL, oSED, oSBC, NULL, NULL, NULL, oSBC, oINC, NULL
};

static void cpu_set_op(cpu_t *cpu, u8 opcode) {
  cpu->op.code = opcode;
  cpu->op.mode = cpu_op_addrmodes[opcode];
  cpu->op.handler = cpu_op_fns[opcode];
  cpu->op.cyc = 0;
}

// Does an operand fetch cycle for the current op. This is called once per cycle and takes a variable number of cycles
// to complete, depending on the addressing mode. Returns true when the final operand address is calculated and placed
// into `operand`. Returns false and does not set `operand` otherwise.
static bool cpu_get_operand_tick(nes_t *nes, u16 *operand) {
  cpu_t *cpu = nes->cpu;
  // This doesn't include absolute indexed because that is only used in one instruction (JMP)
  // Absolute indexed is handled in oJMP()
  switch (cpu->op.mode) {
    case ABS:
      assert(cpu->op.cyc == 0 || cpu->op.cyc == 1);
      if (cpu->op.cyc == 0) {
        SET_BYTE_LO(addr_bus, cpu->pc++);
        cpu->op.cyc++;
        return false;
      } else {
        SET_BYTE_HI(addr_bus, cpu->pc++);
        *operand = addr_bus;
        cpu->op.cyc++;
        return true;
      }
    case ABS_IDX_X:
      crash_and_burn("cpu_get_operand_tick: unimplemented addressing mode ABS_IDX_X");
    case ABS_IDX_Y:
      crash_and_burn("cpu_get_operand_tick: unimplemented addressing mode ABS_IDX_Y");
    case REL:
      crash_and_burn("cpu_get_operand_tick: unimplemented addressing mode REL");
    case IMM:
      *operand = cpu->pc++;
      return true;
    case ZP:
      crash_and_burn("cpu_get_operand_tick: unimplemented addressing mode ZP");
    case ZP_IDX_X:
      crash_and_burn("cpu_get_operand_tick: unimplemented addressing mode ZP_IDX_X");
    case ZP_IDX_Y:
      crash_and_burn("cpu_get_operand_tick: unimplemented addressing mode ZP_IDX_Y");
    case ZP_IDX_IND:
      crash_and_burn("cpu_get_operand_tick: unimplemented addressing mode ZP_IDX_IND");
    case ZP_IND_IDX_Y:
      crash_and_burn("cpu_get_operand_tick: unimplemented addressing mode ZP_IND_IDX_Y");
    case IMPL_ACCUM:
      // Read data byte and throw it away
      cpu_read8(nes, cpu->pc++);
      return true;
    default:
      crash_and_burn("cpu_get_operand_tick: unimplemented addressing mode");
      break;
  }
}

// Gets the addressing mode from an opcode. Based on the chart from:
// https://wiki.nesdev.com/w/index.php/CPU_unofficial_opcodes
// I like this method vs. a simple lookup table because this loosely emulates how the 6502
// internally decodes instructions.
static addrmode_t get_addrmode(u8 opcode) {
  u8 mode_i = opcode % 0x20;
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

static void cpu_init_tables() {
  for (int i = 0; i < CPU_NUM_OPCODES; i++)
    cpu_op_addrmodes[i] = get_addrmode(i);
}

// Accurately emulates a single CPU cycle
void cpu_tick(nes_t *nes) {
  cpu_t *cpu = nes->cpu;

  if (cpu->fetch_op) {
    if (nes->args->cpu_log_output)
      cpu_log_op(nes);

    cpu_set_op(cpu, cpu_read8(nes, cpu->pc++));
    cpu->fetch_op = false;
  } else {
    cpu->op.handler(nes);
  }

  cpu->ticks++;
}

void cpu_init(nes_t *nes) {
  // NES power up state
  cpu_t *cpu = nes->cpu;

  bzero(cpu, sizeof *cpu);
  cpu->a = 0;
  cpu->x = 0;
  cpu->y = 0;

  // TODO: The mystery of starting ticks == 7... that's what nestest starts at but I want to derive this value.
  cpu->ticks = 7;
  cpu->p = 0x24;
  cpu->sp = 0xFD;
  cpu->nmi = false;

  // Initialize lookup tables
  cpu_init_tables();

  // Set PC to value at reset vector
//  cpu->pc = cpu_read16(nes, VEC_RESET);
  // TODO: This is just for nestest
  cpu->pc = 0xC000;
  cpu->fetch_op = true;
}

void cpu_destroy(nes_t *nes) {
  bzero(nes->cpu, sizeof *nes->cpu);
}

static void cpu_log_op(nes_t *nes) {
  u8 num_operands, low, high;
  cpu_t *cpu = nes->cpu;

  // Get operand for logging purposes
  u8 opcode = cpu_read8(nes, cpu->pc);
  addrmode_t mode = get_addrmode(opcode);
  u8 dbg_operand_sz = OPERAND_SIZES[mode];
  u16 operand = 0;
  if (dbg_operand_sz == 2)
    operand = cpu_read16(nes, cpu->pc + 1);
  else if (dbg_operand_sz == 1)
    operand = cpu_read8(nes, cpu->pc + 1);

  FILE *log_f = nes->args->cpu_logf;

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
      printf("cpu_log_op: invalid operand count, wtf?\n");
      exit(EXIT_FAILURE);
  }
  fprintf(log_f, " %s", cpu_opcode_tos(opcode));
  switch (mode) {
    case ABS:
      // JSR and JMP absolute shouldn't display val @ address
      if (opcode == 0x20 || opcode == 0x4C)
        fprintf(log_f, " $%04X                       ", operand);
      else {
        // We don't want to access reg that have side effects, like PPU reg, OAM DMA,
        if (!(operand >= 0x2000 && operand <= 0x3FFF) && !(operand >= 0x4000 && operand <= 0x4020))
          fprintf(log_f, " $%04X = %02X                  ", operand, cpu_read8(nes, operand));
        else
          fprintf(log_f, " $%04X                       ", operand);
      }
      break;
    case ABS_IND:
      fprintf(log_f, " ($%04X) = %04X              ", operand, addr_bus);
      break;
    case ABS_IDX_X:
    case ABS_IDX_Y:
      fprintf(log_f, " $%04X,%s @ %04X = %02X         ", operand,
              mode == ABS_IDX_X ? "X" : "Y", addr_bus, cpu_read8(nes, addr_bus));
      break;
    case REL:
      // Add two because we haven't advanced the PC when this function is called
      fprintf(log_f, " $%04X                       ", addr_bus);
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
      fprintf(log_f, " $%02X,%s @ %02X = %02X             ", operand,
              mode == ZP_IDX_X ? "X" : "Y", addr_bus, cpu_read8(nes, addr_bus));
      break;
    case ZP_IDX_IND:
      fprintf(log_f, " ($%02X,X) @ %02X = %04X = %02X    ", operand,
              (operand + cpu->x) & 0xFF, addr_bus, cpu_read8(nes, addr_bus));
      break;
    case ZP_IND_IDX_Y:
      low = cpu_read8(nes, operand);
      high = cpu_read8(nes, (operand + 1) & 0xFF);
      fprintf(log_f, " ($%02X),Y = %04X @ %04X = %02X  ", operand,
              low | (high << 8), addr_bus, cpu_read8(nes, addr_bus));
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
      printf("cpu_log_op: invalid addressing mode, wtf?\n");
      exit(EXIT_FAILURE);
  }

  // Print registers
  fprintf(log_f,
          "A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3u,%3u CYC:%lu",
          cpu->a, cpu->x, cpu->y, cpu->p, cpu->sp, nes->ppu->scanline,
          nes->ppu->dot, cpu->ticks);

  // Mark where interrupts occur
  if (cpu->nmi) {
    fprintf(log_f, " NMI!");
  }
//  else if (cpu->irq_pending) {
//    fprintf(log_f, " IRQ!");
//  }

  fprintf(log_f, "\n");
  fflush(log_f);
}