#include "../include/apu.h"
#include "../include/cpu.h"
#include "../include/util.h"

const u8 LC_LENGTHS[32] = {0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06, 0xA0, 0x08, 0x3C, 0x0A,
                           0x0E, 0x0C, 0x1A, 0x0E, 0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
                           0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E};

const u8 SQUARE_SEQ[4][8] = {{0, 1, 0, 0, 0, 0, 0, 0},
                             {0, 1, 1, 0, 0, 0, 0, 0},
                             {0, 1, 1, 1, 1, 0, 0, 0},
                             {1, 0, 0, 1, 1, 1, 1, 1}};

const u8 TRIANGLE_SEQ[32] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
                             0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

const u16 NOISE_PERIOD[16] = {4, 8, 16, 32, 64, 96, 128, 160, 202,
                              254, 380, 508, 762, 1016, 2034, 4068};

f64 pulse_table[31];
f64 tnd_table[203];

u8 apu_read(nes_t *nes, u16 addr) {
  apu_t *apu = nes->apu;

  // Status register read
  if (addr == 0x4015) {
    apu->frame_interrupt = false;

    u8 retval = (apu->pulse1.lc > 0) | (apu->pulse2.lc > 0) << 1 | (apu->triangle.lc > 0) << 2 |
                (apu->noise.lc > 0) << 3 | (apu->frame_interrupt > 0) << 6;
    return retval;
  } else {
    printf("apu_read: invalid read from $%04X\n", addr);
    exit(EXIT_FAILURE);
  }
}

void apu_write(nes_t *nes, u16 addr, u8 val) {
  apu_t *apu = nes->apu;

  // TODO: Side effects
  switch (addr) {
    case 0x4000:
      // Pulse 1 volume parameters: DDLC NNNN
      apu->pulse1.duty = val >> 6;
      apu->pulse1.lc_disable = val >> 5 & 1;
      apu->pulse1.const_v = val >> 4 & 1;
      apu->pulse1.envelope = val & 0xF;
      break;
    case 0x4001:
      // Pulse 1 sweep unit: EPPP NSSS
      apu->pulse1.sweep_enabled = val >> 7;
      apu->pulse1.sweep_period = val >> 4 & 7;
      apu->pulse1.sweep_neg = val >> 3 & 1;
      apu->pulse1.sweep_shift = val & 7;
      break;
    case 0x4002:
      // Pulse 1 timer low 8 bits
      apu->pulse1.timer &= ~0xFF;
      apu->pulse1.timer |= val;
      break;
    case 0x4003:
      // Pulse 1 length counter load/timer high 3 bits TODO: reset duty and start envelope
      apu->pulse1.timer &= ~(7 << 8);
      apu->pulse1.timer |= (val & 7) << 8;

      apu->pulse1.lc_idx = val >> 3;
      apu->pulse1.lc = LC_LENGTHS[apu->pulse1.lc_idx];
      break;
    case 0x4004:
      // Pulse 2 volume parameters: DDLC NNNN
      apu->pulse2.duty = val >> 6;
      apu->pulse2.lc_disable = val >> 5 & 1;
      apu->pulse2.const_v = val >> 4 & 1;
      apu->pulse2.envelope = val & 0xF;
      break;
    case 0x4005:
      // Pulse 2 sweep unit: EPPP NSSS
      apu->pulse2.sweep_enabled = val >> 7;
      apu->pulse2.sweep_period = val >> 4 & 7;
      apu->pulse2.sweep_neg = val >> 3 & 1;
      apu->pulse2.sweep_shift = val & 7;
      break;
    case 0x4006:
      // Pulse 2 timer low 8 bits
      apu->pulse2.timer &= ~0xFF;
      apu->pulse2.timer |= val;
      break;
    case 0x4007:
      // Pulse 2 length counter load/timer high 3 bits TODO: reset duty and start envelope
      apu->pulse2.timer &= ~(7 << 8);
      apu->pulse2.timer |= (val & 7) << 8;

      apu->pulse2.lc_idx = val >> 3;
      apu->pulse2.lc = LC_LENGTHS[apu->pulse2.lc_idx];
      break;
    case 0x4008:
      // Triangle linear counter control/lc halt and linear counter reload value
      apu->triangle.lc_disable = val >> 7;
      apu->triangle.cnt_reload_val = val & 0x7F;
      break;
    case 0x400A:
      // Triangle timer low 8 bits
      apu->triangle.timer &= ~0xFF;
      apu->triangle.timer |= val;
      break;
    case 0x400B:
      // Triangle length counter load TODO: reload linear counter
      apu->triangle.timer &= ~(7 << 8);
      apu->triangle.timer |= (val & 7) << 8;

      apu->triangle.lc_idx = val >> 3;
      apu->triangle.lc = LC_LENGTHS[apu->triangle.lc_idx];
      break;
    case 0x400C:
      apu->noise.lc_disable = val >> 5 & 1;
      apu->noise.const_v = val >> 4 & 1;
      apu->noise.envelope = val & 0xF;
      break;
    case 0x400E:
      // Noise mode and period
      apu->noise.mode = val >> 7;
      apu->noise.period = val & 0xF;
      break;
    case 0x400F:
      // Noise length counter load TODO: start envelope
      apu->noise.lc_idx = val >> 3;
      apu->noise.lc = LC_LENGTHS[apu->noise.lc_idx];
      break;
    case 0x4010:
      // DMC control flags
      apu->dmc.irq_enable = val >> 7;
      apu->dmc.loop = val >> 6 & 1;
      apu->dmc.freq_idx = val & 0xF;
      break;
    case 0x4011:
      // DMC direct load
      apu->dmc.direct_load = val & 0x7F;
      break;
    case 0x4012:
      // DMC sample address
      apu->dmc.samp_addr = val;
      break;
    case 0x4013:
      // DMC sample length
      apu->dmc.samp_len = val;
    case 0x4015:
      // Status register
      // TODO: DMC stuff
      apu->status.dmc_enable = val >> 4 & 1;
      apu->status.noise_enable = val >> 3 & 1;
      apu->status.triangle_enable = val >> 2 & 1;
      apu->status.pulse2_enable = val >> 1 & 1;
      apu->status.pulse1_enable = val & 1;

      if (!apu->status.noise_enable)
        apu->noise.lc = 0;
      if (!apu->status.triangle_enable)
        apu->triangle.lc = 0;
      if (!apu->status.pulse1_enable)
        apu->pulse1.lc = 0;
      if (!apu->status.pulse2_enable)
        apu->pulse2.lc = 0;
      break;
    case 0x4017:
      // Frame counter
      apu->frame_counter.seq_mode = val >> 7;
      apu->frame_counter.irq_disable = val >> 6 & 1;
      break;
    default:
      printf("apu_write: invalid write to $%04X\n", addr);
      break;
  }
}

static void apu_quarter_frame_tick(apu_t *apu) {
  // TODO: Clock envelopes and triangle linear counter
}

static void apu_half_frame_tick(apu_t *apu) {
  // *********** Clock waveform length counters ***********
  // Pulse 1 & 2
  if (!apu->pulse1.lc_disable && apu->pulse1.lc > 0)
    apu->pulse1.lc--;
  if (!apu->pulse2.lc_disable && apu->pulse2.lc > 0)
    apu->pulse2.lc--;
  if (!apu->noise.lc_disable && apu->noise.lc > 0)
    apu->noise.lc--;
  if (!apu->triangle.lc_disable && apu->triangle.lc > 0)
    apu->triangle.lc--;

  // *********** Clock sweep units ***********
  // TODO: Clock sweep units
}

// Mixes raw channel output into a signed 16-bit sample
static inline s16
apu_mix_audio(u8 pulse1_out, u8 pulse2_out, u8 triangle_out, u8 noise_out, u8 dmc_out) {
  f64 square_out = pulse_table[pulse1_out + pulse2_out];
  f64 tnd_out = tnd_table[3 * triangle_out + 2 * noise_out + dmc_out];

  // Scale sample from [0.0...1.0] to [0...INT16_MAX] TODO: Is 0 the correct lower bound?
//  assert(square_out + tnd_out <= 1.0);
//  assert(square_out + tnd_out >= 0.0);
  return (s16) ((square_out + tnd_out) * INT16_MAX - 1);
}

// Increment APU waveform period counter with proper wrap around
// Pulse channel sequence length is 8, triangle is 32; noise *frequency* is from a 16-entry lookup table
static inline void apu_increment_seq_c(u32 *seq_c, u8 *seq_idx, u32 seq_len, u32 smp_per_sec) {
  if (*seq_c == smp_per_sec - 1) {
    *seq_c = 0;
    *seq_idx = (*seq_idx + 1) % seq_len;
  } else {
    *seq_c += 1;
  }
}

static void apu_render_audio(void *userdata, u8 *stream, s32 len) {
  apu_t *apu = (apu_t *) userdata;
  s16 *buffer = (s16 *) stream;

  memset(buffer, 0, len);

  // Audio format is signed 16bit, so we have to render len / 2 samples
  // Sequence should loop one every `sample_rate / tone_hz` samples
  const u32 PULSE1_FREQ = NTSC_CPU_SPEED / (16 * (apu->pulse1.timer + 1));
  const u32 PULSE2_FREQ = NTSC_CPU_SPEED / (16 * (apu->pulse2.timer + 1));
  const u32 TRIANGLE_FREQ = NTSC_CPU_SPEED / (32 * (apu->triangle.timer + 1));

  const u32 PULSE1_SMP_PER_SEQ = apu->audio_spec.freq / PULSE1_FREQ;
  const u32 PULSE2_SMP_PER_SEQ = apu->audio_spec.freq / PULSE2_FREQ;
  const u32 TRIANGLE_SMP_PER_SEQ = apu->audio_spec.freq / TRIANGLE_FREQ;

  u8 pulse1_seq_idx = 0, pulse2_seq_idx = 0, triangle_seq_idx = 0, noise_seq_idx = 0;
  u8 pulse1_out = 0, pulse2_out = 0, triangle_out = 0, noise_out = 0, dmc_out = 0;
  u32 pulse1_seq_c = 0, pulse2_seq_c = 0, triangle_seq_c = 0;
  for (int smp_n = 0; smp_n < len / 2; smp_n++) {
    // **** Pulse 1 synth ****
    // TODO: Envelope and sweep unit
    if (apu->pulse1.lc > 0) {
      pulse1_out = SQUARE_SEQ[apu->pulse1.duty][pulse1_seq_idx];
      apu_increment_seq_c(&pulse1_seq_c, &pulse1_seq_idx, 8, PULSE1_SMP_PER_SEQ);
    }

    // **** Pulse 2 synth ****
    // TODO: Envelope and sweep unit
    if (apu->pulse2.lc > 0) {
      pulse2_out = SQUARE_SEQ[apu->pulse2.duty][pulse2_seq_idx];
      apu_increment_seq_c(&pulse2_seq_c, &pulse2_seq_idx, 8, PULSE2_SMP_PER_SEQ);
    }

    // **** Triangle synth ****
    if (apu->triangle.lc > 0) {
      triangle_out = TRIANGLE_SEQ[triangle_seq_idx];
      apu_increment_seq_c(&triangle_seq_c, &triangle_seq_idx, 32, TRIANGLE_SMP_PER_SEQ);
    }

    // **** Noise 1 synth ****
    // TODO
    noise_out = 8;
    dmc_out = 64;

    // Mix channels together to get the final sample
    buffer[smp_n] = apu_mix_audio(pulse1_out, pulse2_out, triangle_out, noise_out, dmc_out);
  }
}

// Increment APU frame counter. Called approximately 240 times per second
void apu_tick(nes_t *nes) {
  apu_t *apu = nes->apu;

  // Clock frame counter
  const u8 STEPS_IN_SEQ = apu->frame_counter.seq_mode ? 5 : 4;
  if (STEPS_IN_SEQ == 4) {
    // *********** 4-step sequence mode ***********
    // Sequence = [0, 1, 2, 3, 0, 1, 2, 3, ...]
    // Trigger CPU frame IRQ
    if (apu->frame_counter.step == 3 && !apu->frame_counter.irq_disable) {
      apu->frame_interrupt = true;
      // TODO: Fix IRQ :(
//      nes->cpu->irq_pending = true;
    }

    // Trigger half-frame ticks on every odd sequence step
    if (apu->frame_counter.step % 2 == 1)
      apu_half_frame_tick(apu);

    apu_quarter_frame_tick(apu);
  } else {
    // *********** 5-step sequence mode ***********
    // Sequence = [0, 1, 2, 3, 4, 0, 1, 2, 3, 4, ...]
    if (apu->frame_counter.step != 4) {
      // Trigger half-frame ticks on every even sequence step
      if (apu->frame_counter.step % 2 == 0)
        apu_half_frame_tick(apu);

      apu_quarter_frame_tick(apu);
    }
  }

  // Increment current sequence
  if (apu->frame_counter.step == STEPS_IN_SEQ - 1)
    apu->frame_counter.step = 0;
  else
    apu->frame_counter.step++;
}

static void apu_init_lookup_tables() {
  // Approximation of NES DAC mixer from http://nesdev.com/apu_ref.txt
  // **** Pulse channels ****
  for (int i = 0; i < 31; i++) {
    // Need to convert output value [0.0...1.0] to sample [-32768...32767]
    pulse_table[i] = 95.52 / (8128.0 / i + 100);
  }

  // **** Triangle, noise, and DMC channels ****
  for (int i = 0; i < 203; i++) {
    tnd_table[i] = 163.67 / (24329.0 / i + 100);
  }
}

void apu_init(nes_t *nes, s32 sample_rate, u32 buf_len) {
  apu_t *apu = nes->apu;

  apu->pulse1.sweep_c = 0;
  apu->pulse2.sweep_c = 0;
  apu->pulse1.lc = 0;
  apu->pulse2.lc = 0;
  apu->triangle.lc = 0;
  apu->noise.lc = 0;
  apu->frame_counter.step = 0;
  apu->frame_counter.divider = 0;
  apu->frame_interrupt = false;
  apu->ticks = 0;

  // Request audio spec. Init code based on
  // https://stackoverflow.com/questions/10110905/simple-sound-wave-generator-with-sdl-in-c
  SDL_AudioSpec want;
  want.freq = sample_rate;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.callback = apu_render_audio;
  want.userdata = apu;
  want.samples = buf_len;

  if (!(apu->device_id = SDL_OpenAudioDevice(NULL, 0, &want, &apu->audio_spec, 0)))
    printf("apu_init: could not open audio device!\n");

  // Initialize APU output level lookup tables
  apu_init_lookup_tables();

  // Start sound
  SDL_PauseAudioDevice(apu->device_id, 0);
}

void apu_destroy(nes_t *nes) {
  // Stop sound
  SDL_PauseAudioDevice(nes->apu->device_id, 1);

  SDL_CloseAudioDevice(nes->apu->device_id);
}