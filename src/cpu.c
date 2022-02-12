#include "include/cpu.h"
#include "include/nes.h"
#include "include/mem.h"
#include "include/util.h"
#include "include/ppu.h"
#include "include/args.h"

// ************Internal CPU functions forward declarations ************
static void cpu_log_op(nes_t *nes, u8 opcode, u16 operand, addrmode_t mode);

// This function is run immediately after getting the opcode and puts the effective operand/address
// on the address line
static u16 cpu_get_operand(nes_t *nes);


// Thanks to https://www.nesdev.org/6502_cpu.txt for great NES 6502 documentation
// This is just here for logging purposes
const int OPERAND_SIZES[] = {2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0};

// TODO: Implement open-bus behavior. This is also unclean and should probably be behind some interface
u16 addr_bus = 0;
u8 data_bus = 0;

// ************ Degbugging ************
u16 log_operand = 0;

// Lookup tables
// Addressing mode indexed by opcode
addrmode_t cpu_op_addrmodes[CPU_NUM_OPCODES];

// Function pointers to CPU instruction handlers
static void oADC(nes_t *nes) {}

static void oAND(nes_t *nes) {}

static void oASL(nes_t *nes) {}

static void oBCC(nes_t *nes) {}

static void oBCS(nes_t *nes) {}

static void oBEQ(nes_t *nes) {}

static void oBIT(nes_t *nes) {}

static void oBMI(nes_t *nes) {}

static void oBNE(nes_t *nes) {}

static void oBPL(nes_t *nes) {}

static void oBRK(nes_t *nes) {}

static void oBVC(nes_t *nes) {}

static void oBVS(nes_t *nes) {}

static void oCLC(nes_t *nes) {}

static void oCLD(nes_t *nes) {}

static void oCLI(nes_t *nes) {}

static void oCLV(nes_t *nes) {}

static void oCMP(nes_t *nes) {}

static void oCPX(nes_t *nes) {}

static void oCPY(nes_t *nes) {}

static void oDEC(nes_t *nes) {}

static void oDEX(nes_t *nes) {}

static void oDEY(nes_t *nes) {}

static void oEOR(nes_t *nes) {}

static void oINC(nes_t *nes) {}

static void oINX(nes_t *nes) {}

static void oINY(nes_t *nes) {}

static void oJMP(nes_t *nes) {
  cpu_t *cpu = nes->cpu;
  switch (cpu->op.addrmode) {
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

static void oJSR(nes_t *nes) {}

static void oLDA(nes_t *nes) {}

static void oLDX(nes_t *nes) {}

static void oLDY(nes_t *nes) {}

static void oLSR(nes_t *nes) {}

static void oNOP(nes_t *nes) {}

static void oORA(nes_t *nes) {}

static void oPHA(nes_t *nes) {}

static void oPHP(nes_t *nes) {}

static void oPLA(nes_t *nes) {}

static void oPLP(nes_t *nes) {}

static void oROL(nes_t *nes) {}

static void oROR(nes_t *nes) {}

static void oRTI(nes_t *nes) {}

static void oRTS(nes_t *nes) {}

static void oSBC(nes_t *nes) {}

static void oSEC(nes_t *nes) {}

static void oSED(nes_t *nes) {}

static void oSEI(nes_t *nes) {}

static void oSTA(nes_t *nes) {}

static void oSTX(nes_t *nes) {}

static void oSTY(nes_t *nes) {}

static void oTAX(nes_t *nes) {}

static void oTAY(nes_t *nes) {}

static void oTSX(nes_t *nes) {}

static void oTXA(nes_t *nes) {}

static void oTXS(nes_t *nes) {}

static void oTYA(nes_t *nes) {}

// TODO: Support unofficial opcodes (low priority)
// Maps opcodes to functions that handle them
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
  cpu->op.addrmode = cpu_op_addrmodes[opcode];
  cpu->op.handler = cpu_op_fns[opcode];
  cpu->op.cyc = 0;
}

// Does an operand fetch cycle for the current op. This is called once per cycle and takes a variable number of cycles
// to complete, depending on the addressing mode. Returns true when the final operand value is calculated and placed
// into `operand`. Returns false and does not set `operand` otherwise.
static bool cpu_get_operand_tick(nes_t *nes, u16 *operand) {
  cpu_t *cpu = nes->cpu;
  // This doesn't include absolute indexed because that is only used in one instruction (JMP)
  // Absolute indexed is handled in oJMP()
  switch (cpu->op.addrmode) {
    case ABS:
      break;
    case ABS_IDX_X:
      break;
    case ABS_IDX_Y:
      break;
    case REL:
      break;
    case IMM:
      break;
    case ZP:
      break;
    case ZP_IDX_X:
      break;
    case ZP_IDX_Y:
      break;
    case ZP_IDX_IND:
      break;
    case ZP_IND_IDX_Y:
      break;
    case IMPL_ACCUM:
      break;
    default:

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
  cpu->ticks = 4;
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

static void cpu_log_op(nes_t *nes, u8 opcode, u16 operand, addrmode_t mode) {
  u8 num_operands, low, high;
  cpu_t *cpu;

  cpu = nes->cpu;
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
    fprintf(log_f, " **** NMI occurred ****");
  }
//  else if (cpu->irq_pending) {
//    fprintf(log_f, " **** IRQ occurred ****");
//  }

  fprintf(log_f, "\n");
}