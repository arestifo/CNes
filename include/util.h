#ifndef CNES_UTIL_H
#define CNES_UTIL_H

#include "nes.h"

// Sets bit n of num to dot. dot must be 0 or 1, else the result is garbage
#define SET_BIT(num, n, x) ((num) = (((num) & ~(1 << (n))) | ((x) << (n))))
#define GET_BIT(num, n) (!!((num) & (1 << (n))))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// Math helper functions
s32 ones_complement(s32 num);
s32 twos_complement(s32 num);

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

#endif
