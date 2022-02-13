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
    printf("nes_fopen: %s\n", fn);
    exit(EXIT_FAILURE);
  }

  return f;
}

size_t nes_fread(void *ptr, size_t sz, size_t n, FILE *f) {
  size_t bytesread;

  if ((bytesread = fread(ptr, sz, n, f)) != n) {
    printf("nes_fread: read size inconsistent, read=%zu n=%zu sz=%zu expected=%lu\n", bytesread, n,
           sz, n);
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

i32 ones_complement(i32 num) {
  return -num - 1;
}

i32 twos_complement(i32 num) {
  return -num;
}

void crash_and_burn(const char *msg) {
  printf("%s\n", msg);
  exit(EXIT_FAILURE);
}

// Gets assembler mnemonic from opcode
char *cpu_opcode_tos(u8 opcode) {
  switch (opcode) {
    case 0x69:
    case 0x65:
    case 0x75:
    case 0x6D:
    case 0x7D:
    case 0x79:
    case 0x61:
    case 0x71:
      return "ADC";
    case 0x29:
    case 0x25:
    case 0x35:
    case 0x2D:
    case 0x3D:
    case 0x39:
    case 0x21:
    case 0x31:
      return "AND";
    case 0x0A:
    case 0x06:
    case 0x16:
    case 0x0E:
    case 0x1E:
      return "ASL";
    case 0x90:
      return "BCC";
    case 0xB0:
      return "BCS";
    case 0xF0:
      return "BEQ";
    case 0x24:
    case 0x2C:
      return "BIT";
    case 0x30:
      return "BMI";
    case 0xD0:
      return "BNE";
    case 0x10:
      return "BPL";
    case 0x00:
      return "BRK";
    case 0x50:
      return "BVC";
    case 0x70:
      return "BVS";
    case 0x18:
      return "CLC";
    case 0xD8:
      return "CLD";
    case 0x58:
      return "CLI";
    case 0xB8:
      return "CLV";
    case 0xC9:
    case 0xC5:
    case 0xD5:
    case 0xCD:
    case 0xDD:
    case 0xD9:
    case 0xC1:
    case 0xD1:
      return "CMP";
    case 0xE0:
    case 0xE4:
    case 0xEC:
      return "CPX";
    case 0xC0:
    case 0xC4:
    case 0xCC:
      return "CPY";
    case 0xC6:
    case 0xD6:
    case 0xCE:
    case 0xDE:
      return "DEC";
    case 0xCA:
      return "DEX";
    case 0x88:
      return "DEY";
    case 0x49:
    case 0x45:
    case 0x55:
    case 0x4D:
    case 0x5D:
    case 0x59:
    case 0x41:
    case 0x51:
      return "EOR";
    case 0xE6:
    case 0xF6:
    case 0xEE:
    case 0xFE:
      return "INC";
    case 0xE8:
      return "INX";
    case 0xC8:
      return "INY";
    case 0x4C:
    case 0x6C:
      return "JMP";
    case 0x20:
      return "JSR";
    case 0xA9:
    case 0xA5:
    case 0xB5:
    case 0xAD:
    case 0xBD:
    case 0xB9:
    case 0xA1:
    case 0xB1:
      return "LDA";
    case 0xA2:
    case 0xA6:
    case 0xB6:
    case 0xAE:
    case 0xBE:
      return "LDX";
    case 0xA0:
    case 0xA4:
    case 0xB4:
    case 0xAC:
    case 0xBC:
      return "LDY";
    case 0x4A:
    case 0x46:
    case 0x56:
    case 0x4E:
    case 0x5E:
      return "LSR";
    case 0xEA:
      return "NOP";
    case 0x09:
    case 0x05:
    case 0x15:
    case 0x0D:
    case 0x1D:
    case 0x19:
    case 0x01:
    case 0x11:
      return "ORA";
    case 0x48:
      return "PHA";
    case 0x08:
      return "PHP";
    case 0x68:
      return "PLA";
    case 0x28:
      return "PLP";
    case 0x2A:
    case 0x26:
    case 0x36:
    case 0x2E:
    case 0x3E:
      return "ROL";
    case 0x6A:
    case 0x66:
    case 0x76:
    case 0x6E:
    case 0x7E:
      return "ROR";
    case 0x40:
      return "RTI";
    case 0x60:
      return "RTS";
    case 0xE9:
    case 0xE5:
    case 0xF5:
    case 0xED:
    case 0xFD:
    case 0xF9:
    case 0xE1:
    case 0xF1:
      return "SBC";
    case 0x38:
      return "SEC";
    case 0xF8:
      return "SED";
    case 0x78:
      return "SEI";
    case 0x85:
    case 0x95:
    case 0x8D:
    case 0x9D:
    case 0x99:
    case 0x81:
    case 0x91:
      return "STA";
    case 0x86:
    case 0x96:
    case 0x8E:
      return "STX";
    case 0x84:
    case 0x94:
    case 0x8C:
      return "STY";
    case 0xAA:
      return "TAX";
    case 0xA8:
      return "TAY";
    case 0xBA:
      return "TSX";
    case 0x8A:
      return "TXA";
    case 0x9A:
      return "TXS";
    case 0x98:
      return "TYA";
    default:
      printf("cpu_opcode_tos: unknown opcode 0x%02X\n", opcode);
      return "UNK";
  }
}