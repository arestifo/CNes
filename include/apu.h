#ifndef CNES_APU_H
#define CNES_APU_H

#include "nes.h"

#define CPU_TICKS_PER_SEQ 7457

typedef struct apu {
  union {
    struct {
      // ******************** Pulse channel 1 ********************
      struct {
        // Duty cycle/volume parameters
        u8 duty: 2;
        u8 lc_disable: 1;
        u8 const_v: 1;
        u8 envelope: 4;

        // Sweep unit parameters
        u8 enabled: 1;
        u8 period: 3;
        u8 negative: 1;
        u8 shift_c: 3;

        // Timer low
        u8 timer_lo;

        // Length counter load value/timer high
        u8 lc_idx: 5;
        u8 timer_hi: 3;
      } pulse1;

      // ******************** Pulse channel 2 ********************
      struct {
        // Duty cycle/volume parameters
        u8 duty: 2;
        u8 lc_disable: 1;
        u8 const_v: 1;
        u8 envelope: 4;

        // Sweep unit parameters
        u8 enabled: 1;
        u8 period: 3;
        u8 negative: 1;
        u8 shift_c: 3;

        // Timer low
        u8 timer_lo;

        // Length counter load value/timer high
        u8 lc_idx: 5;
        u8 timer_hi: 3;
      } pulse2;

      // ******************** Triangle channel ********************
      struct {
        // Linear counter flags/parameters
        u8 lc_disable: 1;
        u8 cnt_reload_val: 7;

        // $4009 is unused for APU triangle so include this padding for indexing purposes
        u8 padding_4009;

        // Timer low
        u8 timer_lo;

        // Length counter load value/timer high
        u8 lc_idx: 5;
        u8 timer_hi: 3;
      } triangle;

      // ******************** Noise channel ********************
      struct {
        // Volume parameters
        u8 r1_unused: 2;
        u8 lc_disable: 1;
        u8 const_v: 1;
        u8 envelope: 4;

        u8 padding_400D;  // $400D is unused in APU noise channel

        // Noise channel parameters
        u8 mode: 1;
        u8 r2_unused: 3;
        u8 period: 4;

        // Length counter load value
        u8 lc_idx: 5;
        u8 r3_unused: 3;
      } noise;

      // ******************** DMC channel ********************
      struct {
        u8 irq_enable: 1;
        u8 loop: 1;
        u8 r1_unused: 2;
        u8 freq_idx: 4;

        u8 r2_unused: 1;
        u8 direct_load: 7;

        u8 samp_addr;
        u8 samp_len;
      } dmc;

      u8 padding_4014;

      // ******************** Status register and frame counter  ********************
      struct {
        u8 sr_unused: 3;
        u8 dmc_enable: 1;
        u8 noise_enable: 1;
        u8 triangle_enable: 1;
        u8 pulse2_enable: 1;
        u8 pulse1_enable: 1;
      } status;

      u8 padding_4016;

      // TODO: Frame counter
      struct {
        u8 seq_mode: 1;
        u8 irq_disable: 1;
        u8 r1_unused: 6;
      } frame_counter;
    };

    u8 bytes[24];
  } reg;

  u8 pulse1_sweep_c;
  u8 pulse2_sweep_c;
  u8 pulse1_lc;
  u8 pulse2_lc;
  u8 triangle_lc;
  u8 noise_lc;
  u8 frame_counter_step;
  u32 frame_counter_div;

  bool frame_interrupt;
  u64 ticks;

  SDL_AudioDeviceID device_id;
  SDL_AudioSpec audio_spec;
} apu_t;

u8 apu_read(nes_t *nes, u16 addr);
void apu_write(nes_t *nes, u16 addr, u8 val);

void apu_init(nes_t *nes, s32 sample_rate, u32 buf_len);
void apu_tick(nes_t *nes);
void apu_destroy(nes_t *nes);

#endif
