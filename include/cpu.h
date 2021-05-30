#ifndef CNES_CPU_H
#define CNES_CPU_H
#include "nes.h"
#include "cart.h"

// Flag operations:
// Set:   cpu->sr |=  C_MASK
// Clear: cpu->sr &= ~C_MASK
// Test:  if (cpu->sr & C_MASK)

#define C_MASK 0x01
#define Z_MASK 0x02
#define I_MASK 0x04
#define D_MASK 0x08
#define B_MASK 0x10
#define U_MASK 0x20
#define V_MASK 0x40
#define N_MASK 0x80

#define NMI_VEC    0xFFFA
#define RESET_VEC  0xFFFC
#define IRQ_VEC    0xFFFE
#define STACK_BASE 0x0100

#define CPU_MEMORY_SZ 0x10000

// The NES uses a MOS Technology 6502 CPU with minimal modifications
struct cpu {
  u8  a;       // Accumulator register
  u8  x;       // X register
  u8  y;       // Y register
  u16 pc;      // Program counter register
  u8  sp;      // Stack pointer register
  u8  sr;      // Status register

  u64 cyc;     // Cycles
  u8  *mem;    // Pointer to main memory
};

// Memory addressing modes
typedef enum addrmode {
  // 16-bit operands
  ABS,          // Absolute
  ABS_IND,      // Absolute indirect
  ABS_IDX_X,    // Absolute indexed with X
  ABS_IDX_Y,    // Absolute indexed with Y

  // 8-bit operands
  REL,          // Relative
  IMM,          // Immediate
  ZP,           // Zero page
  ZP_IDX_X,     // Zero page indexed with X
  ZP_IDX_Y,     // Zero page indexed with Y
  ZP_IDX_IND,   // Zero page indexed indirect
  ZP_IND_IDX_Y, // Zero page indirect indexed with Y

  // Implied, accumulator; no additional operands
  IMPL_ACCUM
} addrmode;

// Array of operand sizes (in bytes), indexed by addressing mode
extern const int OPERAND_SIZES[];
extern FILE *log_f;

// Gets addressing mode from opcode.
// TODO: Use this function to generate a lookup table at program start instead
// TODO: of calling this function every instruction decode cycle
addrmode get_addrmode(u8 opcode);
u16 get_addr(struct cpu *cpu, u16 addr, addrmode mode, bool inc_cyc);
void set_nz(struct cpu *cpu, u8 result);
void cpu_init(struct cpu *cpu, struct cart *cart);
void cpu_destroy(struct cpu *cpu);
void cpu_tick(struct cpu *cpu);

// Debugging util functions
void dump_cpu(struct cpu *cpu, u8 opcode, u16 operand, addrmode mode);
#endif
