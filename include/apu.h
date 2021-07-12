#ifndef CNES_APU_H
#define CNES_APU_H

#include "nes.h"

// NTSC CPU speed (~1.78 MHz)
#define NTSC_CPU_SPEED 1789773

// Number of CPU ticks per 1/240th of a second. This is the basis of all APU timing
#define NTSC_TICKS_PER_SEQ (NTSC_CPU_SPEED / 240)

typedef struct apu {
  // ******************** Pulse channel 1 ********************
  struct {
    // Duty cycle/volume parameters
    u8 duty: 2;
    u8 lc_disable: 1;
    u8 const_v: 1;
    u8 envelope: 4;

    // Sweep unit parameters
    u8 sweep_enabled: 1;
    u8 sweep_period: 3;
    u8 sweep_neg: 1;
    u8 sweep_shift: 3;

    // Length counter load value/timer high
    u8 lc_idx: 5;

    // Timer/frequency of output waveform
    u16 timer: 11;

    // Sweep unit
    u8 sweep_c;

    // Length counter
    u8 lc;

    // Where in the output sequence the channel currently is
    u8 seq_idx;
    u32 seq_c;
  } pulse1;

  // ******************** Pulse channel 2 ********************
  struct {
    // Duty cycle/volume parameters
    u8 duty: 2;
    u8 lc_disable: 1;
    u8 const_v: 1;
    u8 envelope: 4;

    // Sweep unit parameters
    u8 sweep_enabled: 1;
    u8 sweep_period: 3;
    u8 sweep_neg: 1;
    u8 sweep_shift: 3;

    // Length counter load value/timer high
    u8 lc_idx: 5;

    // Timer/frequency of output waveform
    u16 timer: 11;

    // Sweep unit
    u8 sweep_c;
    u8 lc;

    // Where in the output sequence the channel currently is
    u8 seq_idx;
    u32 seq_c;
  } pulse2;

  // ******************** Triangle channel ********************
  struct {
    // Linear counter flags/parameters
    u8 control_flag: 1;
    u8 linc_reload_val: 7;

    // Length counter load value/timer high
    u8 lc_idx: 5;

    u16 timer: 11;

    // Linear/length counter
    u8 lc;
    u8 linc;
    bool linc_reload;

    // Where in the output sequence the channel currently is
    u8 seq_idx;
    u32 seq_c;
  } triangle;

  // ******************** Noise channel ********************
  struct {
    // Volume parameters
    u8 r1_unused: 2;
    u8 lc_disable: 1;
    u8 const_v: 1;
    u8 envelope: 4;

    // Noise channel parameters
    u8 mode: 1;
    u8 r2_unused: 3;
    u8 period: 4;

    // Length counter load value
    u8 lc_idx: 5;
    u8 r3_unused: 3;

    u8 lc;
    u8 shift_reg;
  } noise;

  // ******************** DMC channel ********************
  struct {
    u8 irq_enable: 1;
    u8 loop: 1;
    u8 r1_unused: 2;
    u8 freq_idx: 4;

    u8 r2_unused: 1;
    u8 direct_load: 7;

    u16 samp_addr;
    u16 samp_len;
  } dmc;


  // ******************** Status register and frame counter  ********************
  struct {
    u8 sr_unused: 3;
    u8 dmc_enable: 1;
    u8 noise_enable: 1;
    u8 triangle_enable: 1;
    u8 pulse2_enable: 1;
    u8 pulse1_enable: 1;
  } status;

  // TODO: Frame counter
  struct {
    u8 seq_mode: 1;
    u8 irq_disable: 1;
    u8 r1_unused: 6;

    u8 step;
    u32 divider;
  } frame_counter;

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
