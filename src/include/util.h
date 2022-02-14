#ifndef CNES_UTIL_H
#define CNES_UTIL_H

#include "nes.h"

// Sets bit n of num to x. x must be 0 or 1, else the result is garbage
#define SET_BIT(num, n, x) ((num) = (((num) & ~(1 << (n))) | ((x) << (n))))
#define GET_BIT(num, n) (!!((num) & (1 << (n))))

// Get upper and lower bytes of a u16
#define GET_BYTE_LO(src) ((src) & 0xFF)
#define GET_BYTE_HI(src) (((src) & 0xFF00) >> 8)

#define SET_BYTE_LO(dest, byte) ((dest) = (((dest) & ~0xFF) | (byte)))
#define SET_BYTE_HI(dest, byte) ((dest) = (((dest) & ~0xFF00) | ((byte) << 8)))

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// Math helper functions
i32 ones_complement(i32 num);
i32 twos_complement(i32 num);

// Memory helper functions
void *nes_malloc(size_t sz);
void *nes_calloc(size_t count, size_t sz);

// Filesystem helper functions
FILE *nes_fopen(char *fn, char *mode);
size_t nes_fread(void *ptr, size_t sz, size_t n, FILE *f);
size_t nes_fwrite(void *ptr, size_t sz, size_t n, FILE *f);
int nes_fclose(FILE *f);

// Misc helper functions
char *cpu_opcode_tos(u8 opcode);

// Displays an error message and aborts
void crash_and_burn(const char *msg, ...);

#endif
