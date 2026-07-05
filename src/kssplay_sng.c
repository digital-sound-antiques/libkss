#include "kssplay.h"
#include "kssplay_sng.h"
#include <stdlib.h>
#include <string.h>

/*
 * Native SCC Musixx/SNG playback. The row and effect semantics mirror
 * RoboPlay's MSX SNG player, while audio output is driven through libkss' SCC.
 */

#define SNG_HEADER_OFFSET 0x780
#define SNG_PATTERNS_OFFSET 0x781
#define SNG_PATTERN_DATA_OFFSET 0x7E5
#define SNG_PATTERN_SIZE 0x600
#define SNG_ROW_SIZE 24
#define SNG_ROWS 64
#define SNG_CHANNELS 5
#define SNG_MODULATE_COUNT 3

enum { SNG_DIR_DOWN = 0, SNG_DIR_UP = 1 };

typedef struct {
  uint8_t direction;
  uint8_t value;
} SNG_EFFECT;

typedef struct {
  uint8_t direction;
  uint8_t value;
  uint8_t rest;
} SNG_SLIDE_STATE;

typedef struct {
  uint8_t instrument;
  uint8_t volume;
  uint16_t frequency_work;
  uint16_t frequency_original;
  SNG_EFFECT modulate_work;
  SNG_EFFECT modulate_original;
  SNG_SLIDE_STATE volume_slide;
  SNG_SLIDE_STATE frequency_slide;
  SNG_EFFECT transpose;
  uint8_t command;
  uint8_t value;
} SNG_CHANNEL_STATE;

struct tagKSSPLAY_SNG {
  const uint8_t *data;
  uint32_t size;
  uint8_t speed;
  uint8_t speed_count;
  uint8_t pattern_line;
  uint8_t position;
  uint8_t pattern_number;
  uint8_t modulate_count;
  double sample_accum;
  SNG_CHANNEL_STATE ch[SNG_CHANNELS];
};

static uint16_t sng_u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static const uint8_t *sng_row_ptr(KSSPLAY_SNG *sng) {
  uint32_t off = SNG_PATTERN_DATA_OFFSET + (uint32_t)sng->pattern_number * SNG_PATTERN_SIZE +
                 (uint32_t)sng->pattern_line * SNG_ROW_SIZE;
  if (!sng->data || off + SNG_ROW_SIZE > sng->size)
    return NULL;
  return sng->data + off;
}

static const uint8_t *sng_cell_ptr(const uint8_t *row, int ch) { return ch < 4 ? row + ch * 5 : row + 20; }

static uint16_t sng_cell_frequency(const uint8_t *row, int ch) { return sng_u16(sng_cell_ptr(row, ch)); }
static uint8_t sng_cell_instrument(const uint8_t *row, int ch) { return ch < 4 ? sng_cell_ptr(row, ch)[2] : 0; }
static uint8_t sng_cell_volcmd(const uint8_t *row, int ch) { return ch < 4 ? sng_cell_ptr(row, ch)[3] : sng_cell_ptr(row, ch)[2]; }
static uint8_t sng_cell_value(const uint8_t *row, int ch) { return ch < 4 ? sng_cell_ptr(row, ch)[4] : sng_cell_ptr(row, ch)[3]; }

static void sng_scc_wave(SCC *scc, int ch, const uint8_t *wave) {
  int i;
  int base = ch * 0x20;
  if (ch < 0 || ch >= 4)
    return;
  for (i = 0; i < 32; i++)
    SCC_writeReg(scc, (uint32_t)(base + i), wave[i]);
}

static void sng_scc_frequency(SCC *scc, int ch, uint16_t frequency) {
  SCC_writeReg(scc, (uint32_t)(0xC0 + ch * 2), frequency & 0xFF);
  SCC_writeReg(scc, (uint32_t)(0xC1 + ch * 2), (frequency >> 8) & 0x0F);
}

static void sng_scc_volume(SCC *scc, int ch, uint8_t volume) {
  SCC_writeReg(scc, (uint32_t)(0xD0 + ch), volume & 0x0F);
}

static void sng_next_pattern(KSSPLAY_SNG *sng) {
  uint8_t song_length = sng->data[SNG_HEADER_OFFSET];
  sng->position++;
  if (song_length == 0 || sng->position >= song_length)
    sng->position = 0;
  sng->pattern_number = sng->data[SNG_PATTERNS_OFFSET + sng->position];
  sng->pattern_line = 0;
}

static void sng_play_note(KSSPLAY *kssplay) {
  KSSPLAY_SNG *sng = kssplay->sngplay;
  const uint8_t *row = sng_row_ptr(sng);
  int i;
  if (!row)
    return;
  for (i = 0; i < SNG_CHANNELS; i++) {
    uint16_t frequency = sng_cell_frequency(row, i);
    if (frequency) {
      if (sng->ch[i].transpose.direction == SNG_DIR_DOWN)
        frequency = (uint16_t)(frequency + sng->ch[i].transpose.value);
      else
        frequency = (uint16_t)(frequency - sng->ch[i].transpose.value);
      sng->ch[i].frequency_original = frequency;
      sng->ch[i].frequency_work = frequency;
      sng->ch[i].modulate_work = sng->ch[i].modulate_original;
    }
  }
  for (i = 0; i < SNG_CHANNELS; i++)
    sng->ch[i].volume = sng_cell_volcmd(row, i) >> 4;
  for (i = 0; i < SNG_CHANNELS - 1; i++) {
    uint8_t instrument = sng_cell_instrument(row, i);
    if (instrument && instrument != sng->ch[i].instrument) {
      uint32_t off = (uint32_t)(instrument - 1) * 40;
      sng->ch[i].instrument = instrument;
      if (off + 32 <= SNG_HEADER_OFFSET)
        sng_scc_wave(kssplay->vm->scc, i, sng->data + off);
    }
  }
}

static void sng_handle_transpose(KSSPLAY_SNG *sng, int ch, uint8_t direction, uint8_t value) {
  sng->ch[ch].transpose.direction = direction;
  sng->ch[ch].transpose.value = value;
  if (direction == SNG_DIR_DOWN)
    sng->ch[ch].frequency_work = (uint16_t)(sng->ch[ch].frequency_work + value);
  else
    sng->ch[ch].frequency_work = (uint16_t)(sng->ch[ch].frequency_work - value);
}

static void sng_handle_volume_slide(KSSPLAY_SNG *sng, int ch, uint8_t value) {
  sng->ch[ch].volume_slide.direction = (value & 0xF0) ? SNG_DIR_DOWN : SNG_DIR_UP;
  value &= 0x0F;
  sng->ch[ch].volume_slide.value = sng->speed ? (uint8_t)(value / sng->speed) : value;
  sng->ch[ch].volume_slide.rest = sng->speed ? (uint8_t)(value % sng->speed) : 0;
}

static void sng_handle_modulate(KSSPLAY_SNG *sng, int ch, uint8_t direction, uint8_t value) {
  sng->ch[ch].modulate_work.direction = direction;
  sng->ch[ch].modulate_work.value = value;
  sng->ch[ch].modulate_original.direction = direction;
  sng->ch[ch].modulate_original.value = value;
  sng->ch[ch].frequency_work = sng->ch[ch].frequency_original;
}

static void sng_handle_slide(KSSPLAY_SNG *sng, int ch, uint8_t direction, uint8_t value) {
  sng->ch[ch].frequency_slide.direction = direction;
  sng->ch[ch].frequency_slide.value = sng->speed ? (uint8_t)(value / sng->speed) : value;
  sng->ch[ch].frequency_slide.rest = sng->speed ? (uint8_t)(value % sng->speed) : 0;
}

static int sng_handle_command(KSSPLAY_SNG *sng) {
  const uint8_t *row = sng_row_ptr(sng);
  int i;
  int result = 1;
  if (!row)
    return 1;
  for (i = 0; i < SNG_CHANNELS; i++) {
    uint8_t command = sng_cell_volcmd(row, i) & 0x0F;
    uint8_t value = sng_cell_value(row, i);
    sng->ch[i].command = command;
    sng->ch[i].value = value;
    switch (command) {
    case 15:
      if (value)
        sng->speed = (value == 1) ? 2 : value;
      break;
    case 11:
      sng->position = (uint8_t)(value - 1);
      sng_next_pattern(sng);
      result = 0;
      break;
    case 10:
      sng_next_pattern(sng);
      result = 0;
      break;
    case 7:
      sng_handle_transpose(sng, i, SNG_DIR_DOWN, value);
      break;
    case 6:
      sng_handle_transpose(sng, i, SNG_DIR_UP, value);
      break;
    case 5:
      sng_handle_volume_slide(sng, i, value);
      break;
    case 4:
      sng_handle_modulate(sng, i, SNG_DIR_DOWN, value);
      break;
    case 3:
      sng_handle_modulate(sng, i, SNG_DIR_UP, value);
      break;
    case 2:
      sng_handle_slide(sng, i, SNG_DIR_DOWN, value);
      break;
    case 1:
      sng_handle_slide(sng, i, SNG_DIR_UP, value);
      break;
    default:
      break;
    }
  }
  return result;
}

static void sng_do_slide(KSSPLAY_SNG *sng, int ch) {
  uint8_t value = sng->ch[ch].frequency_slide.value;
  if (sng->ch[ch].frequency_slide.rest) {
    value++;
    sng->ch[ch].frequency_slide.rest--;
  }
  if (sng->ch[ch].command == 2) {
    sng->ch[ch].frequency_work = (uint16_t)(sng->ch[ch].frequency_work + value);
    sng->ch[ch].frequency_original = (uint16_t)(sng->ch[ch].frequency_original + value);
  } else {
    sng->ch[ch].frequency_work = (uint16_t)(sng->ch[ch].frequency_work - value);
    sng->ch[ch].frequency_original = (uint16_t)(sng->ch[ch].frequency_original - value);
  }
}

static void sng_do_volume_slide(KSSPLAY_SNG *sng, int ch) {
  uint8_t value = sng->ch[ch].volume_slide.value;
  if (sng->ch[ch].volume_slide.rest) {
    value++;
    sng->ch[ch].volume_slide.rest--;
  }
  if (sng->ch[ch].volume_slide.direction == SNG_DIR_DOWN) {
    sng->ch[ch].volume = (sng->ch[ch].volume > value) ? (uint8_t)(sng->ch[ch].volume - value) : 0;
  } else {
    sng->ch[ch].volume = (uint8_t)(sng->ch[ch].volume + value);
    if (sng->ch[ch].volume > 15)
      sng->ch[ch].volume = 15;
  }
}

static void sng_update(KSSPLAY *kssplay) {
  KSSPLAY_SNG *sng = kssplay->sngplay;
  int i;
  if (!sng)
    return;
  sng->speed_count--;
  if (!sng->speed_count) {
    do {
      sng_play_note(kssplay);
    } while (!sng_handle_command(sng));
    sng->speed_count = sng->speed ? sng->speed : 1;
    sng->pattern_line++;
    if (sng->pattern_line == SNG_ROWS)
      sng_next_pattern(sng);
  }
  for (i = 0; i < SNG_CHANNELS; i++) {
    if (sng->ch[i].command == 1 || sng->ch[i].command == 2)
      sng_do_slide(sng, i);
    else if (sng->ch[i].command == 5)
      sng_do_volume_slide(sng, i);
  }
  sng->modulate_count--;
  if (!sng->modulate_count) {
    sng->modulate_count = SNG_MODULATE_COUNT;
    for (i = 0; i < SNG_CHANNELS; i++) {
      if (sng->ch[i].modulate_work.value) {
        if (sng->ch[i].modulate_work.direction == SNG_DIR_DOWN) {
          sng->ch[i].frequency_work = (uint16_t)(sng->ch[i].frequency_work + sng->ch[i].modulate_work.value);
          sng->ch[i].modulate_work.direction = SNG_DIR_UP;
        } else {
          sng->ch[i].frequency_work = (uint16_t)(sng->ch[i].frequency_work - sng->ch[i].modulate_work.value);
          sng->ch[i].modulate_work.direction = SNG_DIR_DOWN;
        }
      }
    }
  }
  for (i = 0; i < SNG_CHANNELS; i++) {
    sng_scc_frequency(kssplay->vm->scc, i, sng->ch[i].frequency_work);
    sng_scc_volume(kssplay->vm->scc, i, sng->ch[i].volume);
  }
}

void KSSPLAY_SNG_process(KSSPLAY *kssplay) {
  KSSPLAY_SNG *sng = kssplay->sngplay;
  if (!sng)
    return;
  if (sng->sample_accum <= 0.0) {
    sng_update(kssplay);
    sng->sample_accum += (double)kssplay->rate / 50.0;
  }
  sng->sample_accum -= 1.0;
}

void KSSPLAY_SNG_reset(KSSPLAY *kssplay) {
  int i;
  KSSPLAY_SNG *sng;
  if (!kssplay->sngplay) {
    kssplay->sngplay = malloc(sizeof(KSSPLAY_SNG));
  }
  sng = kssplay->sngplay;
  if (!sng)
    return;
  memset(sng, 0, sizeof(*sng));
  sng->data = kssplay->kss->data;
  sng->size = kssplay->kss->size;
  sng->speed = 8;
  sng->speed_count = 1;
  sng->modulate_count = 1;
  sng->position = 0xFF;
  for (i = 0; i < SNG_CHANNELS; i++) {
    sng->ch[i].transpose.direction = SNG_DIR_DOWN;
    sng->ch[i].modulate_work.direction = SNG_DIR_DOWN;
    sng->ch[i].modulate_original.direction = SNG_DIR_DOWN;
    sng->ch[i].volume_slide.direction = SNG_DIR_DOWN;
    sng->ch[i].frequency_slide.direction = SNG_DIR_DOWN;
  }
  SCC_reset(kssplay->vm->scc);
  SCC_set_type(kssplay->vm->scc, SCC_STANDARD);
  SCC_writeReg(kssplay->vm->scc, 0xE1, 0x1F);
  sng_next_pattern(sng);
}

void KSSPLAY_SNG_delete(KSSPLAY *kssplay) {
  free(kssplay->sngplay);
  kssplay->sngplay = NULL;
}
