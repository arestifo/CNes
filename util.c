#include "include/util.h"

void *nes_malloc(size_t sz) {
  void *ret;

  if ((ret = malloc(sz)) == NULL) {
    perror("nes_malloc");
    exit(EXIT_FAILURE);
  }

  return ret;
}

void *nes_calloc(size_t count, size_t sz) {
  void *ret;

  if ((ret = calloc(count, sz)) == NULL) {
    perror("nes_calloc");
    exit(EXIT_FAILURE);
  }

  return ret;
}

FILE *nes_fopen(char *fn, char *mode) {
  FILE *f;

  if ((f = fopen(fn, mode)) == NULL) {
    perror("nes_fopen");
    exit(EXIT_FAILURE);
  }

  return f;
}

size_t nes_fread(void *ptr, size_t sz, size_t n, FILE *f) {
  size_t bytesread;

  if ((bytesread = fread(ptr, sz, n, f)) != n) {
    printf("nes_fread: read size inconsistent, read=%zu n=%zu sz=%zu expected=%lu\n", bytesread, n, sz, n);
  }

  return bytesread;
}

size_t nes_fwrite(void *ptr, size_t sz, size_t n, FILE *f) {
  size_t byteswritten;

  if ((byteswritten = fwrite(ptr, sz, n, f)) != n) {
    printf("nes_fwrite: write size inconsistent, wtf?\n");
  }

  return byteswritten;
}

int nes_fclose(FILE *f) {
  int retval;

  if ((retval = fclose(f)) != 0) {
    perror("nes_fclose");
  }

  return retval;
}
