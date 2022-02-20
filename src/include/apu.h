#ifndef CNES_APU_H
#define CNES_APU_H

#include "nes.h"

// NTSC CPU speed in Hz (~1.78 MHz)
#define NTSC_CPU_SPEED 1789773.

typedef struct envelope {
  u8 loop;
  u8 disable;
  u8 n;

  // Envelope divider
  u8 env_volume;
  u8 env_seq_i;
  f64 env_c;
} envelope_t;

typedef struct sweep_unit {
  u8 enabled;
  u8 period;
  u8 negate;
  u8 shift;

  u32 sweep_c;
  bool reload;
} sweep_unit_t;

typedef struct apu {
  // ******************** Pulse channel 1 ********************
  struct {
    // Duty cycle/volume parameters
    u8 duty;
    u8 lc_disable;
    envelope_t env;

    // Sweep unit
    sweep_unit_t sweep;

    // Length counter load value/timer high
    u8 lc_idx;

    // Timer/frequency of output waveform
    u16 timer;
    u8 sweep_c;

    // Length counter
    u8 lc;

    // Where in the output sequence the channel currently is
    u8 seq_idx;
    f64 seq_c;
  } pulse1;

  // ******************** Pulse channel 2 ********************
  struct {
    // Duty cycle/volume parameters
    u8 duty;
    u8 lc_disable;
    envelope_t env;

    // Sweep unit parameters
    sweep_unit_t sweep;

    // Length counter load value/timer high
    u8 lc_idx;

    // Timer/frequency of output waveform
    u16 timer;

    // Sweep unit
    u8 sweep_c;
    u8 lc;

    // Where in the output sequence the channel currently is
    u8 seq_idx;
    f64 seq_c;
  } pulse2;

  // ******************** Triangle channel ********************
  struct {
    // Linear counter flags/parameters
    u8 control_flag;
    u8 linc_reload_val;

    // Length counter load value/timer high
    u8 lc_idx;

    u16 timer;

    // Linear/length counter
    u8 lc;
    u8 linc;
    bool linc_reload;

    // Where in the output sequence the channel currently is
    u8 seq_idx;
    f64 seq_c;
  } triangle;

  // ******************** Noise channel ********************
  struct {
    // Volume parameters
    u8 lc_disable;
    envelope_t env;

    // Noise channel parameters
    u8 mode;
    u8 period;

    // Length counter load value
    u8 lc_idx;

    u8 lc;
    u16 shift_reg;
    f64 seq_c;
  } noise;

  // ******************** DMC channel ********************
  struct {
    u8 irq_enable;
    u8 loop;
    u8 freq_idx;

    u8 direct_load;

    u16 samp_addr;
    u16 samp_len;
  } dmc;


  // ******************** Status register and frame counter  ********************
  struct {
    u8 sr_unused;
    u8 dmc_enable;
    u8 noise_enable;
    u8 triangle_enable;
    u8 pulse2_enable;
    u8 pulse1_enable;
  } status;

  // TODO: Frame counter
  struct {
    u8 seq_mode;
    u8 irq_disable;

    u8 step;
    u32 divider;
  } frame_counter;

  bool frame_interrupt;
  u64 ticks;
  u32 buf_scale_factor;  // Basically, how many samples are generated before being added to the queue

  SDL_AudioDeviceID device_id;
  SDL_AudioSpec audio_spec;
} apu_t;

u8 apu_read(nes_t *nes, u16 addr);
void apu_write(nes_t *nes, u16 addr, u8 val);

void apu_init(nes_t *nes, i32 sample_rate, u32 buf_len);
void apu_tick(nes_t *nes);
void apu_destroy(nes_t *nes);

#endif
