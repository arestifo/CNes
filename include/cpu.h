#ifndef CNES_CPU_H
#define CNES_CPU_H

#include "nes.h"

// Flag operations:
// Set:   cpu->p |=  C_MASK
// Clear: cpu->p &= ~C_MASK
// Test:  if (cpu->p & C_MASK)

#define C_BIT 0
#define Z_BIT 1
#define I_BIT 2
#define D_BIT 3
#define B_BIT 4
#define U_BIT 5
#define V_BIT 6
#define N_BIT 7

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

#define CPU_MEM_SZ 0x10000

#define OAM_DMA_ADDR     0x4014
#define CONTROLLER1_PORT 0x4016
#define CONTROLLER2_PORT 0x4017

// There are four addressing modes that can incur page cross penalties:
// Absolute,X, Absolute,Y, (Indirect),Y, and Relative
// All instructions using these addressing modes incur page cross penalties
// UNLESS they do a write e.g. ASL, LSR, ROL, ROR, STA, INC, DEC
#define PAGE_CROSSED(a, b) (((a) & 0x0100) != ((b) & 0x0100))

// The NES uses a MOS Technology 6502 CPU with minimal modifications
typedef struct cpu {
  u16 pc;                // Program counter register
  u8  a;                 // Accumulator register
  u8  x;                 // X register
  u8  y;                 // Y register
  u8  sp;                // Stack pointer register
  u8  p;                 // Status register

  u64 ticks;               // Cycles
  u8  mem[CPU_MEM_SZ];   // Pointer to main memory

  // Interrupt flags
  bool nmi_pending;
  bool irq_pending;
} cpu_t;

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

typedef enum interrupt {
  INTR_NMI,  // Non-maskable interrupt
  INTR_IRQ,  // Standard interrupt
  INTR_BRK   // Software-triggered (BRK) interrupt
} interrupt_t;

// Gets addressing mode from opcode.
// TODO: Use this function to generate a lookup table at program start instead
// TODO: of calling this function every instruction decode cycle
addrmode_t get_addrmode(u8 opcode);
u16 resolve_addr(nes_t *nes, u16 addr, addrmode_t mode);
void cpu_set_nz(nes_t *nes, u8 result);
void cpu_init(nes_t *nes);
void cpu_destroy(nes_t *nes);
void cpu_tick(nes_t *nes);
void cpu_oam_dma(nes_t *nes, u16 cpu_base_addr);

// Debugging util functions
void dump_cpu(nes_t *nes, u8 opcode, u16 operand, addrmode_t mode);

#endif
