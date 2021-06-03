#include "include/ppu.h"
#include "include/util.h"
#include "include/window.h"

void ppu_init(struct nes *nes) {

}

// Renders a single pixel at the current PPU position
void ppu_tick(struct nes *nes) {

}

u8 ppu_read(struct nes *nes, u8 reg) {
  struct ppu *ppu;
  u8 retval;

  ppu = nes->ppu;
  switch (reg) {
    case 2:  // PPUSTATUS
      // Clear vblank bit every PPUSTATUS read
      SET_BIT(ppu->regs[PPUSTATUS], PPUSTATUS_VBLANK_BIT, 0);
      return ppu->regs[PPUSTATUS];
    case 4:  // OAMDATA
      // TODO
    case 7:  // PPUDATA
      // TODO
    default:
      printf("ppu_read: cannot read from ppu reg $%02d\n", reg);
      exit(EXIT_FAILURE);
  }
}

void ppu_write(struct nes *nes, u8 reg, u8 val) {

}

void ppu_destroy(struct nes *nes) {

}

