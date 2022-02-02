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

const u8 ENVELOPE_SEQ[16] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

const u16 NOISE_PERIOD[16] = {4, 8, 16, 32, 64, 96, 128, 160, 202,
                              254, 380, 508, 762, 1016, 2034, 4068};

// Lookup tables
u32 pulse_smp_table[UINT16_MAX + 1];
u32 triangle_smp_table[UINT16_MAX + 1];
u32 noise_smp_table[UINT16_MAX + 1];

u32 env_periods[16];
f32 pulse_volume_table[31];
f32 tnd_volume_table[203];

u8 apu_read(nes_t *nes, u16 addr) {
  apu_t *apu = nes->apu;

  // Status register read
  if (addr == 0x4015) {
    apu->frame_interrupt = false;

    u8 retval = !!apu->pulse1.lc | !!apu->pulse2.lc << 1 | !!apu->triangle.lc << 2 |
                !!apu->noise.lc << 3 | !!apu->frame_interrupt << 6;
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

      // Envelope parameters
      apu->pulse1.env.loop = apu->pulse1.lc_disable;
      apu->pulse1.env.disable = val >> 4 & 1;
      apu->pulse1.env.n = val & 0xF;
      break;
    case 0x4001:
      // Pulse 1 sweep unit: EPPP NSSS
      apu->pulse1.sweep.enabled = val >> 7;
      apu->pulse1.sweep.period = (val >> 4 & 7) + 1;
      apu->pulse1.sweep.negate = val >> 3 & 1;
      apu->pulse1.sweep.shift = val & 7;
      apu->pulse1.sweep.reload = true;
      break;
    case 0x4002:
      // Pulse 1 timer low 8 bits
      apu->pulse1.timer &= ~0xFF;
      apu->pulse1.timer |= val;
      break;
    case 0x4003:
      // Pulse 1 length counter load/timer high 3 bits
      apu->pulse1.timer &= ~(7 << 8);
      apu->pulse1.timer |= (val & 7) << 8;

      apu->pulse1.lc_idx = val >> 3;
      if (apu->status.pulse1_enable)
        apu->pulse1.lc = LC_LENGTHS[apu->pulse1.lc_idx];

      // Restart sequence but not divider (seq_c)
      apu->pulse1.seq_idx = 0;
      apu->pulse1.env.env_seq_i = 0;
      apu->pulse1.env.env_c = 0;
      break;
    case 0x4004:
      // Pulse 2 volume parameters: DDLC NNNN
      apu->pulse2.duty = val >> 6;
      apu->pulse2.lc_disable = val >> 5 & 1;

      apu->pulse2.env.loop = apu->pulse2.lc_disable;
      apu->pulse2.env.disable = val >> 4 & 1;
      apu->pulse2.env.n = val & 0xF;
      break;
    case 0x4005:
      // Pulse 2 sweep unit: EPPP NSSS
      apu->pulse2.sweep.enabled = val >> 7;
      apu->pulse2.sweep.period = (val >> 4 & 7) + 1;
      apu->pulse2.sweep.negate = val >> 3 & 1;
      apu->pulse2.sweep.shift = val & 7;
      apu->pulse2.sweep.reload = true;
      break;
    case 0x4006:
      // Pulse 2 timer low 8 bits
      apu->pulse2.timer &= ~0xFF;
      apu->pulse2.timer |= val;
      break;
    case 0x4007:
      // Pulse 2 length counter load/timer high 3 bits
      apu->pulse2.timer &= ~(7 << 8);
      apu->pulse2.timer |= (val & 7) << 8;

      apu->pulse2.lc_idx = val >> 3;
      if (apu->status.pulse2_enable)
        apu->pulse2.lc = LC_LENGTHS[apu->pulse2.lc_idx];

      // Restart sequence but not divider (seq_c)
      apu->pulse2.seq_idx = 0;
      apu->pulse2.env.env_seq_i = 0;
//      apu->pulse2.env.env_c = 0;
      break;
    case 0x4008:
      // Triangle linear counter control/lc halt and linear counter reload value
      apu->triangle.control_flag = val >> 7;
      apu->triangle.linc_reload_val = val & 0x7F;
      break;
    case 0x400A:
      // Triangle timer low 8 bits
      apu->triangle.timer &= ~0xFF;
      apu->triangle.timer |= val;
      break;
    case 0x400B:
      // Triangle length counter load
      apu->triangle.timer &= ~(7 << 8);
      apu->triangle.timer |= (val & 7) << 8;

      apu->triangle.lc_idx = val >> 3;
      if (apu->status.triangle_enable)
        apu->triangle.lc = LC_LENGTHS[apu->triangle.lc_idx];

      apu->triangle.linc_reload = true;
      break;
    case 0x400C:
      // Noise env
      apu->noise.lc_disable = val >> 5 & 1;

      apu->noise.env.loop = apu->noise.lc_disable;
      apu->noise.env.disable = val >> 4 & 1;
      apu->noise.env.n = val & 0xF;
      break;
    case 0x400E:
      // Noise mode and period
      apu->noise.mode = val >> 7;
      apu->noise.period = NOISE_PERIOD[val & 0xF];
      break;
    case 0x400F:
      // Noise length counter load
      apu->noise.lc_idx = val >> 3;
      if (apu->status.noise_enable)
        apu->noise.lc = LC_LENGTHS[apu->noise.lc_idx];
      apu->noise.env.env_seq_i = 0;
      apu->noise.env.env_c = 0;
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
      break;
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

      // Reset divider and and sequencer on every $4017 write
      apu->frame_counter.step = 0;
      apu->frame_counter.divider = 0;
      break;
    default:
      printf("apu_write: invalid write to $%04X\n", addr);
      break;
  }
}

// Mixes raw channel output into a signed 16-bit sample
static inline s16
apu_mix_audio(u8 pulse1_out, u8 pulse2_out, u8 triangle_out, u8 noise_out, u8 dmc_out) {
  f32 square_out = pulse_volume_table[pulse1_out + pulse2_out];
  f32 tnd_out = tnd_volume_table[3 * triangle_out + 2 * noise_out + dmc_out];

  return (s16) ((square_out + tnd_out) * INT16_MAX);
}

// Increment APU waveform period counter with proper wrap around
// Pulse channel sequence length is 8, triangle is 32; noise *frequency* is from a 16-entry lookup table
static inline void
apu_clock_sequence_counter(u32 *seq_c, u8 *seq_idx, u32 seq_len, u32 smp_per_sec) {
  if (*seq_c >= smp_per_sec) {
    *seq_c = 0;
    *seq_idx = (*seq_idx + 1) % seq_len;
  } else {
    *seq_c += 1;
  }
}

static inline void apu_clock_envelope(envelope_t *env) {
  const u32 ENV_PERIOD = env_periods[env->env_seq_i];

  env->env_volume = ENVELOPE_SEQ[env->env_seq_i];
  apu_clock_sequence_counter(&env->env_c, &env->env_seq_i, 16, ENV_PERIOD);
}

// Clock an APU su unit, updating a period `target_pd` with its output
static inline void
apu_clock_sweep_unit(sweep_unit_t *su, u16 *target_pd, s32 (*negate_func)(s32)) {
  // Calculate the new period after shifting
  u16 new_pd = *target_pd >> su->shift;

  if (su->negate)
    new_pd = negate_func(new_pd);

  new_pd += *target_pd;

  // Sweep unit mutes the channel if target_pd < 8 OR new_pd > 0x7FF
  bool muting = *target_pd < 8 || new_pd > 0x7FF;

  // Adjust target period ONLY if the divider's count is zero, su is enabled, and the su unit
  // is not muting the channel.
  if (su->sweep_c == 0 && su->enabled && !muting)
    *target_pd = new_pd;

  // Increment sweep unit divider
  if (su->sweep_c == 0 || su->reload) {
    su->sweep_c = su->period;
    su->reload = false;
  } else {
    su->sweep_c--;
  }
}

static inline u8 apu_get_envelope_volume(envelope_t *env) {
  // If the envelope disable flag is set, the volume is the envelope's n value
  // Else, return the envelope's current volume
  return env->disable ? env->n : env->env_volume;
}

static void apu_render_audio(apu_t *apu) {
  const u32 BYTES_PER_SAMPLE = apu->audio_spec.channels * sizeof(s16);

  // Sequence should loop once every `sample_rate / tone_hz` samples
  // TODO: implement this with lookup tables
  const u32 PULSE1_FREQ = NTSC_CPU_SPEED / (apu->pulse1.timer + 1) / 2;
  const u32 PULSE2_FREQ = NTSC_CPU_SPEED / (apu->pulse2.timer + 1) / 2;
  const u32 TRIANGLE_FREQ = NTSC_CPU_SPEED / (apu->triangle.timer + 1);
  const u32 NOISE_FREQ = NTSC_CPU_SPEED / apu->noise.period;
//  printf("pulse1 freq is %d Hz smprate is %d Hz\n", PULSE1_FREQ, apu->audio_spec.freq);

  const u32 PULSE1_SMP_PER_SEQ = apu->audio_spec.freq / PULSE1_FREQ;
  const u32 PULSE2_SMP_PER_SEQ = apu->audio_spec.freq / PULSE2_FREQ;
  const u32 TRIANGLE_SMP_PER_SEQ = apu->audio_spec.freq / TRIANGLE_FREQ;
  const u32 NOISE_SMP_PER_SEQ = apu->audio_spec.freq / NOISE_FREQ;
  // **** Render num_samples worth of audio data and queue it ****
  u8 pulse1_out = 0, pulse2_out = 0, triangle_out = 0, noise_out = 0, dmc_out = 0;
//  printf("p1 seq_c=%d, p2 seq_c=%d, t seq_c=%d, n seq_c=%d\n", apu->pulse1.seq_c, apu->pulse2.seq_c, apu->triangle.seq_c, apu->noise.seq_c);
  // TODO: Adjust the audio buffer scaling factor dynamically when a buffer underrun is detected
  while (SDL_GetQueuedAudioSize(apu->device_id) < apu->audio_spec.freq / apu->buf_scale_factor) {
    // **** Pulse 1 synth ****
    if (apu->pulse1.lc > 0 && apu->pulse1.timer > 7 && apu->status.pulse1_enable) {
      const u8 PULSE1_VOLUME = apu_get_envelope_volume(&apu->pulse1.env);
      pulse1_out = SQUARE_SEQ[apu->pulse1.duty][apu->pulse1.seq_idx] * PULSE1_VOLUME;

      apu_clock_sequence_counter(&apu->pulse1.seq_c, &apu->pulse1.seq_idx, 8, PULSE1_SMP_PER_SEQ);
    }

    // **** Pulse 2 synth ****
    if (apu->pulse2.lc > 0 && apu->pulse2.timer > 7 && apu->status.pulse2_enable) {
      const u8 PULSE2_VOLUME = apu_get_envelope_volume(&apu->pulse2.env);
      pulse2_out = SQUARE_SEQ[apu->pulse2.duty][apu->pulse2.seq_idx] * PULSE2_VOLUME;

      apu_clock_sequence_counter(&apu->pulse2.seq_c, &apu->pulse2.seq_idx, 8, PULSE2_SMP_PER_SEQ);
    }

    // **** Triangle synth ****
    if (apu->triangle.lc > 0 && apu->triangle.linc > 0 && apu->status.triangle_enable) {
      triangle_out = TRIANGLE_SEQ[apu->triangle.seq_idx];

      apu_clock_sequence_counter(&apu->triangle.seq_c, &apu->triangle.seq_idx, 32, TRIANGLE_SMP_PER_SEQ);
    }

    // **** Noise synth ****
    if (apu->noise.lc > 0 && apu->status.noise_enable) {
      if (apu->noise.seq_c >= NOISE_SMP_PER_SEQ) {
        apu->noise.seq_c = 0;

        // Shift noise shift register
        const u8 FEEDBACK_BIT_NUM = apu->noise.mode ? 6 : 1;
        u8 feedback_bit = !!((apu->noise.shift_reg & 1) ^ GET_BIT(apu->noise.shift_reg, FEEDBACK_BIT_NUM));
        apu->noise.shift_reg >>= 1;
        SET_BIT(apu->noise.shift_reg, 14, feedback_bit);
      } else {
        apu->noise.seq_c++;
      }

      // Increment noise counter and shift noise shift register
      if (!(apu->noise.shift_reg & 1)) {
        noise_out = apu_get_envelope_volume(&apu->noise.env);
      }
    }

    // Mix channels together to get the final sample
//    s16 final_sample = apu_mix_audio(pulse1_out, pulse2_out, triangle_out, noise_out, dmc_out);
    s16 final_sample = apu_mix_audio(pulse1_out, pulse2_out, triangle_out, 0, dmc_out);
    SDL_QueueAudio(apu->device_id, &final_sample, BYTES_PER_SAMPLE);
  }
}

static void apu_quarter_frame_tick(apu_t *apu) {
  // *********** Clock triangle linear counter ***********
  if (apu->triangle.linc_reload) {
    apu->triangle.linc = apu->triangle.linc_reload_val;
  } else {
    if (apu->triangle.linc > 0)
      apu->triangle.linc--;
  }

  if (!apu->triangle.control_flag)
    apu->triangle.linc_reload = false;

  // *********** Clock envelopes ***********
  apu_clock_envelope(&apu->pulse1.env);
  apu_clock_envelope(&apu->pulse2.env);
  apu_clock_envelope(&apu->noise.env);

  // *********** Render and queue some audio ***********
  apu_render_audio(apu);
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
  if (!apu->triangle.control_flag && apu->triangle.lc > 0)
    apu->triangle.lc--;

  // *********** Clock sweep units ***********
  apu_clock_sweep_unit(&apu->pulse1.sweep, &apu->pulse1.timer, ones_complement);
  apu_clock_sweep_unit(&apu->pulse2.sweep, &apu->pulse2.timer, twos_complement);
}

// Increment APU frame counter. Called approximately 240 times per second
void apu_tick(nes_t *nes) {
  static u32 last_ticks = 0;
  apu_t *apu = nes->apu;

  // Clock frame counter
  const u8 STEPS_IN_SEQ = apu->frame_counter.seq_mode == 1 ? 5 : 4;
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
    if (apu->frame_counter.step & 1)
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

//  printf("apu_tick took %dms\n", SDL_GetTicks() - last_ticks);
  last_ticks = SDL_GetTicks();
}

static void apu_init_lookup_tables(apu_t *apu) {
  // *************** APU mixer lookup tables ***************
  // Approximation of NES DAC mixer from http://nesdev.com/apu_ref.txt
  // **** Pulse channels ****
  for (int i = 0; i < 31; i++)
    pulse_volume_table[i] = 95.52 / (8128.0 / i + 100);

  // **** Triangle, noise, and DMC channels ****
  for (int i = 0; i < 203; i++)
    tnd_volume_table[i] = 163.67 / (24329.0 / i + 100);

  // *************** APU envelope lookup tables ***************
  // There are 16 possible envelope periods (env->n is a 4-bit value)
  for (int i = 0; i < 16; i++)
    env_periods[i] = NTSC_CPU_SPEED / (i + 1);

  // *************** APU frequency lookup tables ***************
  // TODO: Make these work :(
  pulse_smp_table[0] = apu->audio_spec.freq / NTSC_CPU_SPEED;
  triangle_smp_table[0] = apu->audio_spec.freq / NTSC_CPU_SPEED;
  noise_smp_table[0] = apu->audio_spec.freq / NTSC_CPU_SPEED;
  for (int i = 1; i < UINT16_MAX + 1; i++) {
    pulse_smp_table[i] = apu->audio_spec.freq / NTSC_CPU_SPEED / (i + 1) / 2;
    triangle_smp_table[i] = apu->audio_spec.freq / NTSC_CPU_SPEED / (i + 1);
    noise_smp_table[i] = apu->audio_spec.freq / NTSC_CPU_SPEED / i;
  }
}

void apu_init(nes_t *nes, s32 sample_rate, u32 buf_len) {
  apu_t *apu = nes->apu;

  // Initialize all APU state to zero
  bzero(apu, sizeof *apu);

  apu->noise.shift_reg = 1;
  apu->noise.period = NOISE_PERIOD[0];
  apu->buf_scale_factor = 8;

  // Request audio spec. Init code based on
  // https://stackoverflow.com/questions/10110905/simple-sound-wave-generator-with-sdl-in-c
  SDL_AudioSpec want;
  want.freq = sample_rate;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.callback = NULL;
  want.userdata = NULL;
  want.samples = buf_len;

  if (!(apu->device_id = SDL_OpenAudioDevice(NULL, 0, &want, &apu->audio_spec, 0)))
    printf("apu_init: could not open audio device!\n");

  // Initialize APU output level lookup tables
  apu_init_lookup_tables(apu);

  // Start sound
  SDL_PauseAudioDevice(apu->device_id, 0);
}

void apu_destroy(nes_t *nes) {
  // Stop sound
  SDL_PauseAudioDevice(nes->apu->device_id, 1);

  SDL_CloseAudioDevice(nes->apu->device_id);
}