#include "../include/apu.h"
#include "../include/cpu.h"

const u8 LC_LENGTHS[32] = {0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06, 0xA0, 0x08, 0x3C, 0x0A,
                           0x0E, 0x0C, 0x1A, 0x0E, 0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
                           0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E};

const u8 SQUARE_SEQ[4][8] = {{0, 1, 0, 0, 0, 0, 0, 0}, {0, 1, 1, 0, 0, 0, 0, 0},
                            {0, 1, 1, 1, 1, 0, 0, 0}, {1, 0, 0, 1, 1, 1, 1, 1}};

const u8 TRIANGLE_SEQ[32] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
                             0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

u8 apu_read(nes_t *nes, u16 addr) {
  apu_t *apu = nes->apu;

  // Status register read
  if (addr == 0x4015) {
    apu->frame_interrupt = false;

    u8 retval = (apu->pulse1_lc > 0) | (apu->pulse2_lc > 0) << 1 | (apu->triangle_lc > 0) << 2 |
                (apu->noise_lc > 0) << 3 | (apu->frame_interrupt > 0) << 6;
    return retval;
  } else {
    printf("apu_read: invalid read from $%04X\n", addr);
  }

  return nes->apu->reg.bytes[addr - 0x4000];
}

void apu_write(nes_t *nes, u16 addr, u8 val) {
  apu_t *apu = nes->apu;

  apu->reg.bytes[addr - 0x4000] = val;

  // TODO: Side effects
  switch (addr) {
    case 0x4003:
      // Pulse 1 length counter load TODO: reset duty and start envelope
      apu->pulse1_lc = LC_LENGTHS[apu->reg.pulse1.lc_idx];
      break;
    case 0x4007:
      // Pulse 2 length counter load TODO: reset duty and start envelope
      apu->pulse2_lc = LC_LENGTHS[apu->reg.pulse2.lc_idx];
      break;
    case 0x400B:
      // Triangle length counter load TODO: reload linear counter
      apu->triangle_lc = LC_LENGTHS[apu->reg.triangle.lc_idx];
      break;
    case 0x400F:
      // Noise length counter load TODO: start envelope
      apu->noise_lc = LC_LENGTHS[apu->reg.noise.lc_idx];
      break;
    case 0x4015:
      // Status register
      // TODO: DMC stuff
      if (!apu->reg.status.noise_enable)
        apu->noise_lc = 0;
      if (!apu->reg.status.triangle_enable)
        apu->triangle_lc = 0;
      if (!apu->reg.status.pulse1_enable)
        apu->pulse1_lc = 0;
      if (!apu->reg.status.pulse2_enable)
        apu->pulse2_lc = 0;
      break;
    case 0x4017:
      // Frame counter
      break;
    default:
      break;
  }
}

static void quarter_frame_tick(apu_t *apu) {
  // TODO: Clock envelopes and triangle linear counter
}

static void half_frame_tick(apu_t *apu) {
  // *********** Clock waveform length counters ***********
  // Pulse 1 & 2
  if (!apu->reg.pulse1.lc_disable && apu->pulse1_lc > 0)
    apu->pulse1_lc--;
  if (!apu->reg.pulse2.lc_disable && apu->pulse2_lc > 0)
    apu->pulse2_lc--;
  if (!apu->reg.noise.lc_disable && apu->noise_lc > 0)
    apu->noise_lc--;
  if (!apu->reg.triangle.lc_disable && apu->triangle_lc > 0)
    apu->triangle_lc--;

  // *********** Clock sweep units ***********
  // TODO: Clock sweep units
}

// Increment APU frame counter. Called approximately 240 times per second
void apu_tick(nes_t *nes) {
  apu_t *apu = nes->apu;

  // Clock frame counter
  const u8 STEPS_IN_SEQ = apu->reg.frame_counter.seq_mode ? 5 : 4;
  if (STEPS_IN_SEQ == 4) {
    // *********** 4-step sequence mode ***********
    // Sequence = [0, 1, 2, 3, 0, 1, 2, 3, ...]
    // Trigger CPU frame IRQ
    if (apu->frame_counter_step == 3 && !apu->reg.frame_counter.irq_disable) {
      apu->frame_interrupt = true;
      // TODO: Fix IRQ :(
//      nes->cpu->irq_pending = true;
    }

    // Trigger half-frame ticks on every odd sequence step
    if (apu->frame_counter_step % 2 == 1)
      half_frame_tick(apu);

    quarter_frame_tick(apu);
  } else {
    // *********** 5-step sequence mode ***********
    // Sequence = [0, 1, 2, 3, 4, 0, 1, 2, 3, 4, ...]
    if (apu->frame_counter_step != 4) {
      // Trigger half-frame ticks on every even sequence step
      if (apu->frame_counter_step % 2 == 0)
        half_frame_tick(apu);

      quarter_frame_tick(apu);
    }
  }

  // Increment current sequence
  if (apu->frame_counter_step == STEPS_IN_SEQ - 1)
    apu->frame_counter_step = 0;
  else
    apu->frame_counter_step++;
}

static void apu_fill_buffer(void *apu, Uint8 *stream, int len) {

}

void apu_init(nes_t *nes, s32 sample_rate, u32 buf_len) {
  apu_t *apu = nes->apu;

  // There are 24 memory-mapped APU registers spanning $4000-$4017 (this inclues 21 accessible
  // addresses and three padding bytes; there is nothing at $4009, $400D, and $4014)
  // Since we're directly writing and indexing the APU registers, the register structure needs to
  // be the correct size
  assert(sizeof apu->reg == 24);
  memset(&apu->reg, 0, sizeof apu->reg);

  apu->pulse1_sweep_c = 0;
  apu->pulse2_sweep_c = 0;
  apu->pulse1_lc = 0;
  apu->pulse2_lc = 0;
  apu->triangle_lc = 0;
  apu->noise_lc = 0;
  apu->frame_counter_step = 0;
  apu->frame_counter_div = 0;
  apu->frame_interrupt = false;
  apu->ticks = 0;

  // Request audio spec. Init code based on
  // https://stackoverflow.com/questions/10110905/simple-sound-wave-generator-with-sdl-in-c
  SDL_AudioSpec want;
  want.freq = sample_rate;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = buf_len;

  SDL_AudioSpec have;
  if (!(apu->device_id = SDL_OpenAudioDevice(NULL, 0, &want, &apu->audio_spec, 0)))
    printf("apu_init: could not open audio device!\n");

  // Start sound
  SDL_PauseAudio(0);
}

void apu_destroy(nes_t *nes) {
  // Stop sound
  SDL_PauseAudio(1);
  SDL_CloseAudio();
}