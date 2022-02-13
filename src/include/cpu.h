#ifndef CNES_CPU_H
#define CNES_CPU_H

#include "nes.h"

// Flag operations:
// Set:   cpu->p |=  C_MASK
// Clear: cpu->p &= ~C_MASK
// Test:  if (cpu->p & C_MASK)

#define C_FLAG 0
#define Z_FLAG 1
#define I_FLAG 2
#define D_FLAG 3
#define B_FLAG 4
#define U_FLAG 5
#define V_FLAG 6
#define N_FLAG 7

#define C_MASK 0x01
#define Z_MASK 0x02
#define I_MASK 0x04
#define D_MASK 0x08
#define B_MASK 0x10
#define U_MASK 0x20
#define V_MASK 0x40
#define N_MASK 0x80

#define VEC_NMI    0xFFFA
#define VEC_RESET  0xFFFC
#define VEC_IRQ    0xFFFE
#define STACK_BASE 0x0100

// TODO: Put this in the mapper functions
#define CPU_MEM_SZ 0x0800

#define CPU_NUM_OPCODES 0x100

#define OAM_DMA_ADDR     0x4014
#define CONTROLLER1_PORT 0x4016
#define CONTROLLER2_PORT 0x4017

// There are four addressing modes that can incur page cross penalties:
// Absolute,X, Absolute,Y, (Indirect),Y, and Relative
// All instructions using these addressing modes incur page cross penalties
// UNLESS they do a write e.g. ASL, LSR, ROL, ROR, STA, INC, DEC
#define PAGE_CROSSED(a, b) (((a) & 0x0100) != ((b) & 0x0100))

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
} addrmode_t;

// TODO: Consider making a member of struct cpu
typedef struct cpu_op {
  // Opcode, addressing mode, and handling functionof the current operation
  u8 code;
  addrmode_t mode;
  void (*handler)(nes_t *);

  // Keep track of the sub-instruction level ticks to do the proper R/W cycles
  // This is the number of ticks we have spent processing the current opcode *NOT INCLUDING* the original opcode fetch
  // cycle.
  u8 cyc;

  // Did we incur a page cross penalty?
  bool penalty;
  bool do_branch;
} cpu_op_t;

// The NES uses a MOS Technology 6502 CPU with minimal modifications
typedef struct cpu {
  u16 pc;                // Program counter register
  u8  a;                 // Accumulator register
  u8  x;                 // X register
  u8  y;                 // Y register
  u8  sp;                // Stack pointer register
  u8  p;                 // Status register

  u8  mem[CPU_MEM_SZ];   // Pointer to main memory

  // Interrupt flags
  bool nmi;

  // How many CPU cycles have passed since initialization
  u64 ticks;

  // The current opcode being processed
  cpu_op_t op;

  // Should we fetch a new opcode next cycle?
  bool fetch_op;
} cpu_t;

typedef enum interrupt {
  INTR_NMI,  // Non-maskable interrupt
  INTR_IRQ,  // Standard interrupt
  INTR_BRK   // Software-triggered (BRK) interrupt
} interrupt_t;

// Uploads a page of CPU memory to PPU OAM. Suspends the CPU while the transfer is taking place
// TODO: Implement this in a cycle-accurate manner
void cpu_oam_dma(nes_t *nes, u16 cpu_base_addr);

void cpu_init(nes_t *nes);
void cpu_destroy(nes_t *nes);
void cpu_tick(nes_t *nes);
#endif
