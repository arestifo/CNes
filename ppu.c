#include "include/ppu.h"
#include "include/util.h"

void ppu_init(struct nes *nes) {
  struct ppu *ppu;

  ppu = nes->ppu;
}

void ppu_tick(struct nes *nes) {

}

u8 ppu_read(struct nes *nes, u8 reg) {
  u8 retval;
  struct ppu *ppu;

  ppu = nes->ppu;
  switch (reg) {
    case 2:  // PPUSTATUS
      retval = ppu->regs[PPUSTATUS];

      // Clear vblank bit every PPUSTATUS read
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS, 0);
      break;
    case 4:  // OAMDATA
      // TODO
      break;
    case 7:  // PPUDATA
      // TODO
      break;
    default:
      printf("ppu_read: cannot read from ppu reg $%02d\n", reg);
      exit(EXIT_FAILURE);
  }
}

void ppu_write(struct nes *nes, u8 reg, u8 val) {

}

void ppu_destroy(struct nes *nes) {

}

