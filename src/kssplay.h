#ifndef _KSSPLAY_H_
#define _KSSPLAY_H_

#include "filters/filter.h"
#include "filters/rc_filter.h"
#include "filters/dc_filter.h"
#include "kss/kss.h"
#include "vm/vm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KSSPLAY_VOL_BITS 9
#define KSSPLAY_VOL_RANGE (48.0 * 2)
#define KSSPLAY_VOL_STEP (KSSPLAY_VOL_RANGE / (1 << KSSPLAY_VOL_BITS))
#define KSSPLAY_VOL_MAX ((1 << (KSSPLAY_VOL_BITS - 1)) - 1)
#define KSSPLAY_VOL_MIN (-(1 << (KSSPLAY_VOL_BITS - 1)))
#define KSSPLAY_MUTE (1 << KSSPLAY_VOL_BITS)
#define KSSPLAY_VOL_MASK ((1 << KSSPLAY_VOL_BITS) - 1)

typedef struct tagKSSPLAY {
  KSS *kss;

  uint8_t *main_data;
  uint8_t *bank_data;

  VM *vm;

  uint32_t rate;
  uint32_t nch;
  uint32_t bps;

  uint32_t step;
  uint32_t step_rest;
  uint32_t step_left;

  int32_t master_volume;
  int32_t device_volume[EDSC_MAX];
  uint32_t device_mute[EDSC_MAX];
  int32_t device_pan[EDSC_MAX];
  uint32_t device_type[EDSC_MAX];
  FIR *device_fir[2][EDSC_MAX];
  RCF *rcf[2];
  DCF *dcf[2];
  uint32_t lastout[2];

  uint32_t fade;
  uint32_t fade_step;
  uint32_t fade_flag;
  uint32_t silent;
  uint32_t silent_limit;
  int32_t decoded_length;

  uint32_t cpu_speed; /* 0: AutoDetect, 1...N: x1 ... xN */
  uint32_t vsync_freq;

  int scc_disable;
  int opll_stereo;

} KSSPLAY;

enum { KSSPLAY_FADE_NONE = 0, KSSPLAY_FADE_OUT = 1, KSSPLAY_FADE_END = 2 };

/* Create KSSPLAY object */
KSSPLAY *KSSPLAY_new(uint32_t rate, uint32_t nch, uint32_t bps);

/* Set KSS data to KSSPLAY object */
int KSSPLAY_set_data(KSSPLAY *, KSS *kss);

/* Reset KSSPLAY object */
/* cpu_speed = 0:auto 1:3.58MHz 2:7.16MHz ... */
/* scc_type = 0:auto 1: standard 2:enhanced */
void KSSPLAY_reset(KSSPLAY *, uint32_t song, uint32_t cpu_speed);

/* Emulate some seconds and Generate Wave data */
void KSSPLAY_calc(KSSPLAY *, int16_t *buf, uint32_t length);

/* Emulate some seconds */
void KSSPLAY_calc_silent(KSSPLAY *, uint32_t length);

/* Delete KSSPLAY object */
void KSSPLAY_delete(KSSPLAY *);

/* Start fadeout */
void KSSPLAY_fade_start(KSSPLAY *kssplay, uint32_t fade_time);

/* Stop fadeout */
void KSSPLAY_fade_stop(KSSPLAY *kssplay);

int KSSPLAY_read_memory(KSSPLAY *kssplay, uint32_t address);

int KSSPLAY_get_loop_count(KSSPLAY *kssplay);
int KSSPLAY_get_stop_flag(KSSPLAY *kssplay);
int KSSPLAY_get_fade_flag(KSSPLAY *kssplay);

void KSSPLAY_set_opll_patch(KSSPLAY *kssplay, uint8_t *illdata);
void KSSPLAY_set_cpu_speed(KSSPLAY *kssplay, uint32_t cpu_speed);
void KSSPLAY_set_device_lpf(KSSPLAY *kssplay, uint32_t device, uint32_t cutoff);
void KSSPLAY_set_device_mute(KSSPLAY *kssplay, uint32_t devnum, uint32_t mute);
void KSSPLAY_set_device_pan(KSSPLAY *kssplay, uint32_t devnum, int32_t pan);
void KSSPLAY_set_channel_pan(KSSPLAY *kssplay, uint32_t device, uint32_t ch, uint32_t pan);
void KSSPLAY_set_device_quality(KSSPLAY *kssplay, uint32_t device, uint32_t quality);
void KSSPLAY_set_device_volume(KSSPLAY *kssplay, uint32_t devnum, int32_t vol);
void KSSPLAY_set_master_volume(KSSPLAY *kssplay, int32_t vol);
void KSSPLAY_set_device_mute(KSSPLAY *kssplay, uint32_t devnum, uint32_t mute);
void KSSPLAY_set_channel_mask(KSSPLAY *kssplay, uint32_t device, uint32_t mask);
void KSSPLAY_set_device_type(KSSPLAY *kssplay, uint32_t devnum, uint32_t type);
void KSSPLAY_set_silent_limit(KSSPLAY *kssplay, uint32_t time_in_ms);

uint32_t KSSPLAY_get_device_volume(KSSPLAY *kssplay, uint32_t devnum);
void KSSPLAY_get_MGStext(KSSPLAY *kssplay, char *buf, int max);

#ifdef __cplusplus
}
#endif
#endif
