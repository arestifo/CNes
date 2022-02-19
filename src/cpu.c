#include "include/cpu.h"
#include "include/nes.h"
#include "include/mem.h"
#include "include/util.h"
#include "include/ppu.h"
#include "include/apu.h"
#include "include/args.h"

#define OP_FUNC static inline void

// Thanks to https://www.nesdev.org/6502_cpu.txt for great NES 6502 documentation

// TODO: This whole file needs a refactor. I hate seeing nes-> everywhere, plus it makes the code less readable
static bool cpu_get_operand_tick(nes_t *nes, u16 *operand, bool is_read_op);
static void cpu_log_op(nes_t *nes, bool debug_nmi);
static void cpu_branch_op(nes_t *nes, u8 flag_bit, bool branch_if_flag);
static void cpu_pull_reg_op(nes_t *nes, bool reg_a);
static void cpu_push_reg_op(nes_t *nes, bool reg_a);
static void cpu_compare_op(nes_t *nes, u8 val1);
static void cpu_add_op(nes_t *nes, bool subtract);
static void cpu_rmw_op(nes_t *nes, rmw_op_type_t op_type);

// TODO: Implement open-bus behavior. This is also unclean and should probably be behind some interface
u16 addr_bus = 0;
u8 data_bus = 0;

// Current cycle in OAM DMA sequence
u16 oam_dma_byte = 0;
bool oam_dma_read = true;

// Current cycle in interrupt setup sequence
u8 intr_cyc = 0;

// Addressing mode indexed by opcode
addrmode_t cpu_op_addrmodes[CPU_NUM_OPCODES];

// Helper function to set proper CPU negative and zero flags
OP_FUNC cpu_set_nz(nes_t *nes, u8 result) {
  SET_BIT(nes->cpu->p, N_FLAG, (result & 0x80) >> 7);
  SET_BIT(nes->cpu->p, Z_FLAG, !result);
}

// *************************************** CPU instruction handlers ***************************************
OP_FUNC oADC(nes_t *nes) {
  cpu_add_op(nes, false);
}

OP_FUNC oAND(nes_t *nes) {
  u16 addr = 0;
  if (cpu_get_operand_tick(nes, &addr, true)) {
    nes->cpu->a &= cpu_read8(nes, addr);
    cpu_set_nz(nes, nes->cpu->a);
    nes->cpu->fetch_op = true;
  }
}

OP_FUNC oASL(nes_t *nes) {
  cpu_rmw_op(nes, OP_ASL);
}

OP_FUNC oBCC(nes_t *nes) {
  cpu_branch_op(nes, C_FLAG, 0);
}

OP_FUNC oBCS(nes_t *nes) {
  cpu_branch_op(nes, C_FLAG, 1);
}

OP_FUNC oBEQ(nes_t *nes) {
  cpu_branch_op(nes, Z_FLAG, 1);
}

OP_FUNC oBIT(nes_t *nes) {
  cpu_t *cpu = nes->cpu;
  u16 addr = 0;
  if (cpu_get_operand_tick(nes, &addr, false)) {
    u8 val = cpu_read8(nes, addr);
    SET_BIT(cpu->p, Z_FLAG, (cpu->a & val) == 0);

    // Overflow flag is set to bit 6 of memory value
    // Negative flag is set to bit 7 of memory value
    SET_BIT(cpu->p, V_FLAG, GET_BIT(val, 6));
    SET_BIT(cpu->p, N_FLAG, GET_BIT(val, 7));
    cpu->fetch_op = true;
  }
}

OP_FUNC oBMI(nes_t *nes) {
  cpu_branch_op(nes, N_FLAG, 1);
}

OP_FUNC oBNE(nes_t *nes) {
  cpu_branch_op(nes, Z_FLAG, 0);
}

OP_FUNC oBPL(nes_t *nes) {
  cpu_branch_op(nes, N_FLAG, 0);
}

OP_FUNC oBRK(nes_t *nes) {
  nes->cpu->brk = true;
  nes->cpu->fetch_op = true;
}

OP_FUNC oBVC(nes_t *nes) {
  cpu_branch_op(nes, V_FLAG, 0);
}

OP_FUNC oBVS(nes_t *nes) {
  cpu_branch_op(nes, V_FLAG, 1);
}

OP_FUNC oCLC(nes_t *nes) {
  SET_BIT(nes->cpu->p, C_FLAG, 0);
  nes->cpu->fetch_op = true;
}

OP_FUNC oCLD(nes_t *nes) {
  SET_BIT(nes->cpu->p, D_FLAG, 0);
  nes->cpu->fetch_op = true;
}

OP_FUNC oCLI(nes_t *nes) {
  SET_BIT(nes->cpu->p, I_FLAG, 0);
  nes->cpu->fetch_op = true;
}

OP_FUNC oCLV(nes_t *nes) {
  SET_BIT(nes->cpu->p, V_FLAG, 0);
  nes->cpu->fetch_op = true;
}

OP_FUNC oCMP(nes_t *nes) {
  cpu_compare_op(nes, nes->cpu->a);
}

OP_FUNC oCPX(nes_t *nes) {
  cpu_compare_op(nes, nes->cpu->x);
}

OP_FUNC oCPY(nes_t *nes) {
  cpu_compare_op(nes, nes->cpu->y);
}

OP_FUNC oDEC(nes_t *nes) {
  cpu_rmw_op(nes, OP_DEC);
}

OP_FUNC oDEX(nes_t *nes) {
  nes->cpu->x--;
  cpu_set_nz(nes, nes->cpu->x);
  nes->cpu->fetch_op = true;
}

OP_FUNC oDEY(nes_t *nes) {
  nes->cpu->y--;
  cpu_set_nz(nes, nes->cpu->y);
  nes->cpu->fetch_op = true;
}

OP_FUNC oEOR(nes_t *nes) {
  u16 addr = 0;
  if (cpu_get_operand_tick(nes, &addr, true)) {
    nes->cpu->a ^= cpu_read8(nes, addr);
    cpu_set_nz(nes, nes->cpu->a);
    nes->cpu->fetch_op = true;
  }
}

OP_FUNC oINC(nes_t *nes) {
  cpu_rmw_op(nes, OP_INC);
}

OP_FUNC oINX(nes_t *nes) {
  nes->cpu->x++;
  cpu_set_nz(nes, nes->cpu->x);
  nes->cpu->fetch_op = true;
}

OP_FUNC oINY(nes_t *nes) {
  nes->cpu->y++;
  cpu_set_nz(nes, nes->cpu->y);
  nes->cpu->fetch_op = true;
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
  cpu_t *cpu = nes->cpu;
  switch (cpu->op.cyc) {
    case 0:
      // Fetch low absolute addr to address bus
      addr_bus = cpu_read8(nes, cpu->pc++);
      break;
    case 1:
      // I am not sure what this cycle does. Some docs (https://www.nesdev.org/6502_cpu.txt) says it predecrements
      // the stack pointer but this causes wrong results, so just skip it
      break;
    case 2:
      cpu_push8(nes, GET_BYTE_HI(cpu->pc));
      break;
    case 3:
      cpu_push8(nes, GET_BYTE_LO(cpu->pc));
      break;
    case 4:
      // Fetch high absolute addr to PCH and copy low address bus byte to PCL
      SET_BYTE_HI(cpu->pc, cpu_read8(nes, cpu->pc));
      SET_BYTE_LO(cpu->pc, GET_BYTE_LO(addr_bus));
      cpu->fetch_op = true;
      break;
    default:
      crash_and_burn("oJSR: wtf?");
  }
  cpu->op.cyc++;
}

OP_FUNC oLDA(nes_t *nes) {
  u16 addr = 0;
  if (cpu_get_operand_tick(nes, &addr, true)) {
    nes->cpu->a = cpu_read8(nes, addr);
    cpu_set_nz(nes, nes->cpu->a);
    nes->cpu->fetch_op = true;
  }
}

OP_FUNC oLDX(nes_t *nes) {
  u16 addr = 0;
  if (cpu_get_operand_tick(nes, &addr, true)) {
    nes->cpu->x = cpu_read8(nes, addr);
    cpu_set_nz(nes, nes->cpu->x);
    nes->cpu->fetch_op = true;
  }
}

OP_FUNC oLDY(nes_t *nes) {
  u16 addr = 0;
  if (cpu_get_operand_tick(nes, &addr, true)) {
    nes->cpu->y = cpu_read8(nes, addr);
    cpu_set_nz(nes, nes->cpu->y);
    nes->cpu->fetch_op = true;
  }
}

OP_FUNC oLSR(nes_t *nes) {
  cpu_rmw_op(nes, OP_LSR);
}

OP_FUNC oNOP(nes_t *nes) {
  nes->cpu->fetch_op = true;
}

OP_FUNC oORA(nes_t *nes) {
  u16 addr = 0;
  if (cpu_get_operand_tick(nes, &addr, true)) {
    nes->cpu->a |= cpu_read8(nes, addr);
    cpu_set_nz(nes, nes->cpu->a);
    nes->cpu->fetch_op = true;
  }
}

OP_FUNC oPHA(nes_t *nes) {
  cpu_push_reg_op(nes, true);
}

OP_FUNC oPHP(nes_t *nes) {
  cpu_push_reg_op(nes, false);
}

OP_FUNC oPLA(nes_t *nes) {
  cpu_pull_reg_op(nes, true);
}

OP_FUNC oPLP(nes_t *nes) {
  cpu_pull_reg_op(nes, false);
}

OP_FUNC oROL(nes_t *nes) {
  cpu_rmw_op(nes, OP_ROL);
}

OP_FUNC oROR(nes_t *nes) {
  cpu_rmw_op(nes, OP_ROR);
}

OP_FUNC oRTI(nes_t *nes) {
  cpu_t *cpu = nes->cpu;
  switch (cpu->op.cyc) {
    case 0:
      cpu_read8(nes, cpu->pc);
      break;
    case 1:
      // Pre-inc stack pointer already handled;
      break;
    case 2:
      cpu->p = (cpu_pop8(nes) | U_MASK) & ~B_MASK;
      break;
    case 3:
      SET_BYTE_LO(cpu->pc, cpu_pop8(nes));
      break;
    case 4:
      SET_BYTE_HI(cpu->pc, cpu_pop8(nes));
      cpu->fetch_op = true;
      break;
  }
  cpu->op.cyc++;
}

OP_FUNC oRTS(nes_t *nes) {
  cpu_t *cpu = nes->cpu;
  switch (cpu->op.cyc) {
    case 0:
      // Dummy read next instruction byte;
      cpu_read8(nes, cpu->pc);
      break;
    case 1:
      // This cycle is supposed to pre-increment the stack pointer but that is all handled in
      // cpu_pop8(), so just skip this cycle
      break;
    case 2:
      SET_BYTE_LO(cpu->pc, cpu_pop8(nes));
      break;
    case 3:
      SET_BYTE_HI(cpu->pc, cpu_pop8(nes));
      break;
    case 4:
      cpu->pc++;
      cpu->fetch_op = true;
      break;
  }
  cpu->op.cyc++;
}

OP_FUNC oSBC(nes_t *nes) {
  cpu_add_op(nes, true);
}

OP_FUNC oSEC(nes_t *nes) {
  SET_BIT(nes->cpu->p, C_FLAG, 1);
  nes->cpu->fetch_op = true;
}

OP_FUNC oSED(nes_t *nes) {
  SET_BIT(nes->cpu->p, D_FLAG, 1);
  nes->cpu->fetch_op = true;
}

OP_FUNC oSEI(nes_t *nes) {
  SET_BIT(nes->cpu->p, I_FLAG, 1);
  nes->cpu->fetch_op = true;
}

OP_FUNC oSTA(nes_t *nes) {
  u16 addr = 0;
  if (cpu_get_operand_tick(nes, &addr, false)) {
    cpu_write8(nes, addr, nes->cpu->a);
    nes->cpu->fetch_op = true;
  }
}

OP_FUNC oSTX(nes_t *nes) {
  u16 addr = 0;
  if (cpu_get_operand_tick(nes, &addr, false)) {
    cpu_write8(nes, addr, nes->cpu->x);
    nes->cpu->fetch_op = true;
  }
}

OP_FUNC oSTY(nes_t *nes) {
  u16 addr = 0;
  if (cpu_get_operand_tick(nes, &addr, false)) {
    cpu_write8(nes, addr, nes->cpu->y);
    nes->cpu->fetch_op = true;
  }
}

OP_FUNC oTAX(nes_t *nes) {
  nes->cpu->x = nes->cpu->a;
  cpu_set_nz(nes, nes->cpu->x);
  nes->cpu->fetch_op = true;
}

OP_FUNC oTAY(nes_t *nes) {
  nes->cpu->y = nes->cpu->a;
  cpu_set_nz(nes, nes->cpu->y);
  nes->cpu->fetch_op = true;
}

OP_FUNC oTSX(nes_t *nes) {
  nes->cpu->x = nes->cpu->sp;
  cpu_set_nz(nes, nes->cpu->x);
  nes->cpu->fetch_op = true;
}

OP_FUNC oTXA(nes_t *nes) {
  nes->cpu->a = nes->cpu->x;
  cpu_set_nz(nes, nes->cpu->a);
  nes->cpu->fetch_op = true;
}

OP_FUNC oTXS(nes_t *nes) {
  nes->cpu->sp = nes->cpu->x;
  nes->cpu->fetch_op = true;
}

OP_FUNC oTYA(nes_t *nes) {
  nes->cpu->a = nes->cpu->y;
  cpu_set_nz(nes, nes->cpu->a);
  nes->cpu->fetch_op = true;
}
// ********************************************************************************************************

// TODO: Support unofficial opcodes (low priority)
// Maps opcodes to functions that handle them
// I'm putting this here because I don't want to forward declare all the op functions :)
void (*const cpu_op_handlers[CPU_NUM_OPCODES])(nes_t *nes) = {
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

// Returns true when this branch op is done executing
static void cpu_branch_op(nes_t *nes, u8 flag_bit, bool branch_if_flag) {
  cpu_t *cpu = nes->cpu;
  switch (cpu->op.cyc) {
    case 0:
      // Fetch operand
      data_bus = cpu_read8(nes, cpu->pc++);

      // Are we taking the branch?
      if (GET_BIT(cpu->p, flag_bit) == branch_if_flag) {
        break;
      }

      // If not branching, we're done. Fetch a new op
      cpu->fetch_op = true;
      break;
    case 1: {
      // We're taking the branch, calculate the new PC. If it crosses a page boundary we need another
      // cycle to fix it
      if (PAGE_CROSSED(cpu->pc, cpu->pc + (i8) data_bus)) {
        break;
      }

      cpu->pc = cpu->pc + (i8) data_bus;
      cpu->fetch_op = true;
      break;
    }
    case 2:
      // Page cross penalty
      cpu->pc = cpu->pc + (i8) data_bus;
      cpu->fetch_op = true;
      break;
  }
  cpu->op.cyc++;
}

// Execute PLA or PLP, if reg_a is true we do PLA else PLP
static void cpu_pull_reg_op(nes_t *nes, bool reg_a) {
  cpu_t *cpu = nes->cpu;
  switch (cpu->op.cyc) {
    case 0:
      // Dummy read instruction byte
      cpu_read8(nes, cpu->pc);
      break;
    case 1:
      // Increment SP, already handled by cpu_pop8
      break;
    case 2:
      if (reg_a) {
        cpu->a = cpu_pop8(nes);
        cpu_set_nz(nes, cpu->a);
      } else {
        cpu->p = (cpu_pop8(nes) | U_MASK) & ~B_MASK;
      }
      cpu->fetch_op = true;
      break;
  }
  cpu->op.cyc++;
}

static void cpu_push_reg_op(nes_t *nes, bool reg_a) {
  cpu_t *cpu = nes->cpu;
  switch (cpu->op.cyc) {
    case 0:
      // Dummy read instruction byte
      cpu_read8(nes, cpu->pc);
      break;
    case 1:
      cpu_push8(nes, reg_a ? cpu->a : (cpu->p | B_MASK));
      cpu->fetch_op = true;
      break;
  }
  cpu->op.cyc++;
}

static void cpu_compare_op(nes_t *nes, u8 val1) {
  cpu_t *cpu = nes->cpu;
  u16 addr = 0;

  // TODO: Can compare ops incur page cross penalties? I believe they can
  if (cpu_get_operand_tick(nes, &addr, true)) {
    u8 val2 = cpu_read8(nes, addr);
    SET_BIT(cpu->p, C_FLAG, val1 >= val2);
    cpu_set_nz(nes, val1 - val2);
    cpu->fetch_op = true;
  }
}

static void cpu_rmw_modify(nes_t *nes, u8 *val, rmw_op_type_t op_type) {
  cpu_t *cpu = nes->cpu;
  u8 old_carry;
  switch (op_type) {
    case OP_ASL:
      // Set carry to old bit 7
      SET_BIT(cpu->p, C_FLAG, GET_BIT(*val, 7));
      *val <<= 1;
      break;
    case OP_LSR:
      SET_BIT(cpu->p, C_FLAG, *val & C_MASK);
      *val >>= 1;
      break;
    case OP_ROL:
      old_carry = cpu->p & C_MASK;
      SET_BIT(cpu->p, C_FLAG, GET_BIT(*val, 7));
      *val <<= 1;
      *val |= old_carry;
      break;
    case OP_ROR:
      old_carry = cpu->p & C_MASK;
      SET_BIT(cpu->p, C_FLAG, *val & C_MASK);
      *val >>= 1;
      *val |= old_carry << 7;
      break;
    case OP_INC:
      *val += 1;
      break;
    case OP_DEC:
      *val -= 1;
      break;
  }
  cpu_set_nz(nes, *val);
}

static void cpu_rmw_op(nes_t *nes, rmw_op_type_t op_type) {
  cpu_t *cpu = nes->cpu;

  if (cpu->op.mode == IMPL_ACCUM) {
    cpu_rmw_modify(nes, &cpu->a, op_type);
    cpu->fetch_op = true;
    return;
  }

  if (cpu->op.rmw_did_read) {
    // Do modify and write
    switch (cpu->op.cyc) {
      case 0:
        // Write original value back to address and do the transformation
        cpu_write8(nes, addr_bus, data_bus);
        cpu_rmw_modify(nes, &data_bus, op_type);
        break;
      case 1:
        // Write new value
        cpu_write8(nes, addr_bus, data_bus);
        cpu->fetch_op = true;
        break;
      default:
        crash_and_burn("cpu_rmw_op: something weird is happening");
    }
    cpu->op.cyc++;
  } else {
    // RMW ops do not incur page crossing penalties
    if (cpu_get_operand_tick(nes, &addr_bus, false)) {
      data_bus = cpu_read8(nes, addr_bus);
      cpu->op.rmw_did_read = true;
      cpu->op.cyc = 0;
      return;
    }
  }
}

static void cpu_add_op(nes_t *nes, bool subtract) {
  cpu_t *cpu = nes->cpu;
  u16 addr = 0;
  if (cpu_get_operand_tick(nes, &addr, true)) {
    u8 val = cpu_read8(nes, addr);

    // Subtraction is implemented by simply negating the operand
    if (subtract)
      val = ~val;
    u8 old_carry = cpu->p & C_MASK;

    // Check for overflow. This happens when the signs of the operands are "incompatible":
    // 1) Adding two positive nums (bit 7 clear) results in a negative num (bit 7 set)
    // 2) Adding two negative nums results in a positive num
    // TODO: I'll clean this up eventually...
    u8 v_cond = ((cpu->a & 0x80) && (val & 0x80) && !((cpu->a + val + old_carry) & 0x80)) ||
                (!(cpu->a & 0x80) && !(val & 0x80) && ((cpu->a + val + old_carry) & 0x80));
    SET_BIT(cpu->p, V_FLAG, v_cond);

    SET_BIT(cpu->p, C_FLAG, cpu->a + val + old_carry > 0xFF);
    cpu->a += val + old_carry;
    cpu_set_nz(nes, cpu->a);
    cpu->fetch_op = true;
  }
}

static void cpu_set_op(cpu_t *cpu, u8 opcode) {
  cpu->op.code = opcode;
  cpu->op.mode = cpu_op_addrmodes[opcode];
  cpu->op.handler = cpu_op_handlers[opcode];
  cpu->op.rmw_did_read = false;
  cpu->op.cyc = 0;
}

// Does an operand fetch cycle for the current op. This is called once per cycle and takes a variable number of cycles
// to complete, depending on the addressing mode. Returns true when the final operand address is calculated and placed
// into `operand`. Returns false and does not set `operand` otherwise.
static bool cpu_get_operand_tick(nes_t *nes, u16 *operand, bool is_read_op) {
  cpu_t *cpu = nes->cpu;
  // This doesn't include absolute indexed because that is only used in one instruction (JMP)
  // Absolute indexed is handled in oJMP()
  switch (cpu->op.mode) {
    case ABS:
      if (cpu->op.cyc == 0) {
        SET_BYTE_LO(addr_bus, cpu_read8(nes, cpu->pc++));
        cpu->op.cyc++;
        return false;
      } else if (cpu->op.cyc == 1) {
        SET_BYTE_HI(addr_bus, cpu_read8(nes, cpu->pc++));
        cpu->op.cyc++;
        return false;
      } else {
        *operand = addr_bus;
        return true;
      }
    case ABS_IDX_X:
    case ABS_IDX_Y:
      switch (cpu->op.cyc) {
        case 0:
          SET_BYTE_LO(addr_bus, cpu_read8(nes, cpu->pc++));
          cpu->op.cyc++;
          return false;
        case 1:
          SET_BYTE_HI(addr_bus, cpu_read8(nes, cpu->pc++));
          cpu->op.cyc++;
          return false;
        case 2: {
          u16 old_addr_bus = addr_bus;
          u8 inc_val = cpu->op.mode == ABS_IDX_X ? cpu->x : cpu->y;

          // TODO: This dummy read is faithful to how the 6502 implements absolute indexed addressing but causes
          // TODO: problems with controller reading. Find a way to re-enable this
//          cpu_read8(nes, (addr_bus & ~0xFF) | ((addr_bus + inc_val) & 0xFF));

          addr_bus += inc_val;
          if (!is_read_op || PAGE_CROSSED(old_addr_bus, addr_bus)) {
            cpu->op.cyc++;
            return false;
          }

          *operand = addr_bus;
          return true;
        }
        case 3:
          // Page cross penalty cycle
          *operand = addr_bus;
          return true;
      }
    case IMM:
      *operand = cpu->pc++;
      return true;
    case ZP:
      // Clear upper address line bits for zero-page indexing
      if (cpu->op.cyc == 0) {
        addr_bus = cpu_read8(nes, cpu->pc++);
        cpu->op.cyc++;
        return false;
      } else {
        *operand = addr_bus;
        return true;
      }
    case ZP_IDX_X:
    case ZP_IDX_Y:
      switch (cpu->op.cyc) {
        case 0:
          // Fetch zero page address
          addr_bus = cpu_read8(nes, cpu->pc++);
          cpu->op.cyc++;
          return false;
        case 1:
          // Add index reg to ZP address
          // Zero-page dummy reads are free from side effects so we can leave it here
          cpu_read8(nes, addr_bus);

          u8 inc_val = cpu->op.mode == ZP_IDX_X ? cpu->x : cpu->y;
          addr_bus = (addr_bus + inc_val) & 0xFF;

          cpu->op.cyc++;
          return false;
        case 2:
          *operand = addr_bus;
          return true;
      }
    case ZP_IDX_IND:
      switch (cpu->op.cyc) {
        case 0:
          // Fetch pointer address
          data_bus = cpu_read8(nes, cpu->pc++);
          cpu->op.cyc++;
          return false;
        case 1:
          // Dummy read from pointer, then add X to it
          cpu_read8(nes, data_bus);
          // This properly wraps around to the correct zero-page address since data_bus is a u8
          data_bus += cpu->x;
          cpu->op.cyc++;
          return false;
        case 2:
          // Fetch effective address low
          SET_BYTE_LO(addr_bus, cpu_read8(nes, data_bus));
          cpu->op.cyc++;
          return false;
        case 3:
          // Fetch effective address high
          SET_BYTE_HI(addr_bus, cpu_read8(nes, (data_bus + 1) & 0xFF));
          cpu->op.cyc++;
          return false;
        case 4:
          *operand = addr_bus;
          return true;
        default:
          crash_and_burn("cpu_get_operand_tick: ZP_IDX_IND isn't working properly");
      }
    case ZP_IND_IDX_Y:
      switch (cpu->op.cyc) {
        case 0:
          // Fetch pointer from zero page
          data_bus = cpu_read8(nes, cpu->pc++);
          cpu->op.cyc++;
          return false;
        case 1:
          // Fetch effective address low
          SET_BYTE_LO(addr_bus, cpu_read8(nes, data_bus));
          cpu->op.cyc++;
          return false;
        case 2:
          // Fetch effective address high
          SET_BYTE_HI(addr_bus, cpu_read8(nes, (data_bus + 1) & 0xFF));
          cpu->op.cyc++;
          return false;
        case 3: {
          u16 old_addr_bus = addr_bus;
          // TODO: This is also a potentially problematic dummy read
//          cpu_read8(nes, (addr_bus & ~0xFF) | ((addr_bus + cpu->y) & 0xFF));

          addr_bus += cpu->y;
          if (!is_read_op || PAGE_CROSSED(old_addr_bus, addr_bus)) {
            cpu->op.cyc++;
            return false;
          }

          *operand = addr_bus;
          return true;
        }
        case 4:
          // Page cross penalty cycle
          *operand = addr_bus;
          return true;
      }
    case IMPL_ACCUM:
      // Read next instruction byte and throw it away
      cpu_read8(nes, cpu->pc);
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
  else if (mode_i >= 0x0C && mode_i <= 0x0E && opcode != 0x6C) return ABS;
  else if (opcode == 0x6C) return ABS_IND;  // JMP
  return IMPL_ACCUM;
}

static void cpu_init_tables() {
  for (int i = 0; i < CPU_NUM_OPCODES; i++)
    cpu_op_addrmodes[i] = get_addrmode(i);
}

// Returns true when interrupt sequence has finished
static bool cpu_handle_interrupt(nes_t *nes, interrupt_t intr_type) {
  cpu_t *cpu = nes->cpu;
//  printf("handle irq type=%d cyc=%d\n", intr_type, intr_cyc);
  switch (intr_cyc) {
    case 0:
      // Read next instruction byte and throw it away
      cpu_read8(nes, cpu->pc);
      break;
    case 1:
      // Push PCH on stack
      cpu_push8(nes, GET_BYTE_HI(cpu->pc));
      break;
    case 2:
      // Push PCL on stack
      cpu_push8(nes, GET_BYTE_LO(cpu->pc));
      break;
    case 3:
      switch (intr_type) {
        case INTR_NMI:
          addr_bus = VEC_NMI;
          cpu_push8(nes, cpu->p & ~B_MASK);
          break;
        case INTR_IRQ:
          addr_bus = VEC_IRQ;
          cpu_push8(nes, cpu->p & ~B_MASK);
          break;
        case INTR_BRK:
          addr_bus = VEC_IRQ;
          cpu_push8(nes, cpu->p | B_MASK);
          break;
      }
      break;
    case 4:
      // Fetch PCL and set I flag
      SET_BYTE_LO(cpu->pc, cpu_read8(nes, addr_bus));
      if (intr_type != INTR_NMI)
        SET_BIT(cpu->p, I_FLAG, 1);
      break;
    case 5:
      // Fetch PCH
      SET_BYTE_HI(cpu->pc, cpu_read8(nes, addr_bus + 1));
      intr_cyc = 0;
      cpu->fetch_op = true;
      return true;
    default:
      crash_and_burn("cpu_handle_interrupt: invalid interrupt cycle\n");
  }
  intr_cyc++;
  return false;
}

static bool cpu_do_oam_dma(nes_t *nes) {
  cpu_t *cpu = nes->cpu;
  ppu_t *ppu = nes->ppu;

//  // Alternate read/write cycles
//  oam_dma_read = !oam_dma_read;

  // Read a page of memory starting at cpu->oam_dma_base into PPU OAM
  if (oam_dma_byte < 256) {
    if (oam_dma_read) {
      // Read byte to be placed into OAM
      data_bus = cpu_read8(nes, cpu->oam_dma_base + oam_dma_byte);
//      printf("oam dma read, dma_base=$%04X dma_byte=$%02X data=$%02X\n", cpu->oam_dma_base, oam_dma_byte, data_bus);
    } else {
//      printf("oam dma write, dma_base=$%04X dma_byte=$%02X data=$%02X\n", cpu->oam_dma_base, oam_dma_byte, data_bus);
      // Write byte to OAM. There are four bytes per sprite, so calculate the index into the sprite array
      u8 attr_idx = oam_dma_byte % 4;
      u8 sprite_idx = oam_dma_byte >> 2;
      if (attr_idx == 0)
        ppu->oam[sprite_idx].data.y_pos = data_bus;
      else if (attr_idx == 1)
        ppu->oam[sprite_idx].data.tile_idx = data_bus;
      else if (attr_idx == 2)
        ppu->oam[sprite_idx].data.attr = data_bus;
      else if (attr_idx == 3)
        ppu->oam[sprite_idx].data.x_pos = data_bus;
      ppu->oam[sprite_idx].sprite0 = sprite_idx == 0;
      oam_dma_byte++;
    }

    // Alternate read/write cycles
    oam_dma_read = !oam_dma_read;
    return false;
  } else if (oam_dma_byte == 256) {
    // Wait one more cycle to synchronize timing
    oam_dma_byte++;
    return false;
  } else {
    // We're done here, reset all the OAM counters to their default values
    oam_dma_byte = 0;
    oam_dma_read = true;
    cpu->do_oam_dma = false;
    cpu->oam_dma_base = 0;
    return true;
  }
}

// Accurately emulates a single CPU cycle
void cpu_tick(nes_t *nes) {
  cpu_t *cpu = nes->cpu;

  if (cpu->fetch_op) {
    // Check if we need to do OAM DMA
    if (cpu->do_oam_dma)
      if (!cpu_do_oam_dma(nes)) goto done;

    // Check for pending NMI
    if (cpu->nmi) {
      if (!cpu_handle_interrupt(nes, INTR_NMI)) goto done;
      cpu->nmi = false;

      if (nes->args->cpu_log_output) {
        cpu_log_op(nes, true);
      }
    } else {
      // Normal instruction sequence
      if (nes->args->cpu_log_output) {
        cpu_log_op(nes, false);
      }
    }

    cpu_set_op(cpu, cpu_read8(nes, cpu->pc++));
    cpu->fetch_op = false;
  } else {
    if (!cpu->op.handler)
      crash_and_burn("cpu_tick: unsupported opcode $%02X\n", cpu->op.code);
    cpu->op.handler(nes);
  }

  done:
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
  cpu->pc = cpu_read16(nes, VEC_RESET);
  cpu->fetch_op = true;
}

void cpu_destroy(nes_t *nes) {
  bzero(nes->cpu, sizeof *nes->cpu);
}

static void cpu_log_op(nes_t *nes, bool debug_nmi) {
  const int OPERAND_SIZES[] = {2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0};
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
      crash_and_burn("cpu_log_op: invalid operand count, wtf?\n");
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
    case ABS_IND: {
      u8 lo = cpu_read8(nes, operand);
      u8 hi = cpu_read8(nes, ((operand + 1) & 0xFF) | (operand & ~0xFF));
      u16 fin = lo | (hi << 8);
      fprintf(log_f, " ($%04X) = %04X              ", operand, fin);
      break;
    }
    case ABS_IDX_X:
    case ABS_IDX_Y: {
      u8 inc = mode == ABS_IDX_X ? cpu->x : cpu->y;
      fprintf(log_f, " $%04X,%s @ %04X = %02X         ", operand,
              mode == ABS_IDX_X ? "X" : "Y", (operand + inc) & 0xFFFF, cpu_read8(nes, operand + inc));
      break;
    }
    case REL:
      // Add two because we haven't advanced the PC when this function is called
      fprintf(log_f, " $%04X                       ", nes->cpu->pc + (i8) operand + 2);
      break;
    case IMM:
      fprintf(log_f, " #$%02X                        ", operand);
      break;
    case ZP:
      fprintf(log_f, " $%02X = %02X                    ", operand,
              cpu_read8(nes, operand));
      break;
    case ZP_IDX_X:
    case ZP_IDX_Y: {
      u8 inc_val = mode == ZP_IDX_X ? cpu->x : cpu->y;
      fprintf(log_f, " $%02X,%s @ %02X = %02X             ", operand,
              mode == ZP_IDX_X ? "X" : "Y", (operand + inc_val) & 0xFF, cpu_read8(nes, (operand + inc_val) & 0xFF));
      break;
    }
    case ZP_IDX_IND: {
      u16 zp_addr = (operand + cpu->x) & 0xFF;
      u8 at_zp_lo = cpu_read8(nes, zp_addr);
      u8 at_zp_hi = cpu_read8(nes, (zp_addr + 1) & 0xFF);
      u16 fin_zp = at_zp_lo | (at_zp_hi << 8);
      fprintf(log_f, " ($%02X,X) @ %02X = %04X = %02X    ", operand,
              (operand + cpu->x) & 0xFF, fin_zp, cpu_read8(nes, fin_zp));
      break;
    }
    case ZP_IND_IDX_Y:
      low = cpu_read8(nes, operand);
      high = cpu_read8(nes, (operand + 1) & 0xFF);
      u16 ind_addr = low | (high << 8);
      fprintf(log_f, " ($%02X),Y = %04X @ %04X = %02X  ", operand,
              ind_addr, (ind_addr + cpu->y) & 0xFFFF, cpu_read8(nes, ind_addr + cpu->y));
      break;
    case IMPL_ACCUM:
      // For some dumb reason, accumulator arithmetic instructions have "A" as
      // the operand...
      // OP_LSR OP_ASL OP_ROL OP_ROR
      if (opcode == 0x4A || opcode == 0x0A || opcode == 0x2A || opcode == 0x6A)
        fprintf(log_f, " A                           ");
      else
        fprintf(log_f, "                             ");
      break;
  }

  // Print registers
  fprintf(log_f,
          "A:%02X X:%02X Y:%02X P:%02X SP:%02X PPU:%3u,%3u CYC:%lu",
          cpu->a, cpu->x, cpu->y, cpu->p, cpu->sp, nes->ppu->scanline,
          nes->ppu->dot, cpu->ticks);

  // Mark where interrupts occur
  if (cpu->do_oam_dma) {
    fprintf(log_f, " OAM_DMA! ");
  }
  if (debug_nmi) {
    fprintf(log_f, " NMI!");
  }
//  else if (cpu->irq_pending) {
//    fprintf(log_f, " IRQ!");
//  }

  fprintf(log_f, "\n");
//  fflush(log_f);
}