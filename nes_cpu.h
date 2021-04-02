#ifndef CNES_NES_CPU_H
#define CNES_NES_CPU_H
#include "types.h"

// The NES uses a MOS Technology 6502 CPU with minimal modifications
struct nes_cpu {
  u8   a;    // Accumulator register
  u8   x;    // X register
  u8   y;    // Y register
  u16  pc;   // Program counter register
  u8   sp;   // Stack pointer register
  struct {   // Status register
    u8 c:1;  // Carry
    u8 z:1;  // Zero
    u8 i:1;  // Interrupt disable
    u8 d:1;  // Decimal mode
    u8 b:1;  // Break on interrupt from BRK instruction
    u8 u:1;  // Unused (should always be set to one)
    u8 v:1;  // Overflow
    u8 n:1;  // Negative
  } sr;
};
#endif //CNES_NES_CPU_H
