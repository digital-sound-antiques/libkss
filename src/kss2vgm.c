#include "kss2vgm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DATA_ALLOC_UNIT (1024 * 1024)

static void data_array_write(KSS2VGM_DataArray *array, uint8_t *buf, uint32_t size) {
  while (array->allocated <= array->size + size) {
    array->allocated += DATA_ALLOC_UNIT;
    array->buffer = realloc(array->buffer, array->allocated);
  }
  memcpy(array->buffer + array->size, buf, size);
  array->size += size;
}

static void vgm_putc(KSS2VGM *_this, uint8_t d) { data_array_write(_this->vgm_data, &d, 1); }

static void vgm_write(KSS2VGM *_this, uint8_t *buf, uint32_t size) { data_array_write(_this->vgm_data, buf, size); }

static void ini_write(KSS2VGM *_this, uint8_t *buf, uint32_t size) { data_array_write(_this->ini_data, buf, size); }

static KSS2VGM_DataArray *data_array_new(void) {
  KSS2VGM_DataArray *array = malloc(sizeof(KSS2VGM_DataArray));
  array->buffer = calloc(1, DATA_ALLOC_UNIT);
  array->allocated = DATA_ALLOC_UNIT;
  array->size = 0;
  return array;
}
static void data_array_delete(KSS2VGM_DataArray *array) {
  if (array != NULL) {
    free(array->buffer);
    free(array);
  }
}

static void WORD(uint8_t *buf, uint32_t data) {
  buf[0] = data & 0xff;
  buf[1] = (data & 0xff00) >> 8;
}

static void DWORD(uint8_t *buf, uint32_t data) {
  buf[0] = data & 0xff;
  buf[1] = (data & 0xff00) >> 8;
  buf[2] = (data & 0xff0000) >> 16;
  buf[3] = (data & 0xff000000) >> 24;
}

static void create_vgm_header(KSS2VGM *_this, uint8_t *buf, uint32_t header_size, int8_t volume) {

  uint32_t data_size = _this->ini_data->size + _this->vgm_data->size;

  memset(buf, 0, header_size);
  memcpy(buf, "Vgm ", 4);

  DWORD(buf + 0x04, header_size + data_size - 4);
  DWORD(buf + 0x08, 0x00000170);
  DWORD(buf + 0x0C, _this->use_sng ? 3579545 : 0);  // SN76489
  DWORD(buf + 0x10, _this->use_opll ? 3579545 : 0); // YM2413
  DWORD(buf + 0x14, 0);                             // GD3 offset
  DWORD(buf + 0x18, _this->total_samples);
  DWORD(buf + 0x1C, 0);  // loop_offset
  DWORD(buf + 0x20, 0);  // loop_samples
  DWORD(buf + 0x24, 60); // PAL or NTSC

  WORD(buf + 0x28, 0x0009); // SN76489 feedback
  WORD(buf + 0x2A, 16);     // SN76489 shift register width
  WORD(buf + 0x2B, 0);      // SN76489 flags

  DWORD(buf + 0x34, header_size - 0x34);           // VGM data offset
  DWORD(buf + 0x58, _this->use_opl ? 3579545 : 0); // Y8950
  DWORD(buf + 0x74, _this->use_psg ? 1789773 : 0); // AY8910
  buf[0x78] = 0x00;                                // AY8910 chiptype
  buf[0x79] = 0x00;
  buf[0x7c] = volume;
  DWORD(buf + 0x9C, _this->use_scc_plus ? (0x80000000 | 1789773) : (_this->use_scc ? 1789773 : 0)); // SCC
}

static void write_command(KSS2VGM *_this, uint8_t *buf, uint32_t len) {

  uint32_t d = (_this->total_samples - _this->last_write_clock);
  while (65535 < d) {
    vgm_putc(_this, 0x61);
    vgm_putc(_this, 0xff);
    vgm_putc(_this, 0xff);
    d -= 65535;
  }
  if (0 < d) {
    vgm_putc(_this, 0x61);
    vgm_putc(_this, d & 0xff);
    vgm_putc(_this, (d >> 8) & 0xff);
  }
  _this->last_write_clock = _this->total_samples;
  vgm_write(_this, buf, len);
}

static void write_eos_command(KSS2VGM *_this) {
  uint8_t cmd_buf[1];
  cmd_buf[0] = 0x66;
  write_command(_this, cmd_buf, 1);
}

static void iowrite_handler(void *context, uint32_t a, uint32_t d) {

  KSS2VGM *_this = (KSS2VGM *)context;
  uint8_t cmd_buf[4];

  if (a == 0x7c || a == 0xf0) { // YM2413(A)
    _this->use_opll = 1;
    _this->opll_adr = d;
  } else if (a == 0x7d || a == 0xf1) { // YM2413(D)
    cmd_buf[0] = 0x51;
    cmd_buf[1] = _this->opll_adr;
    cmd_buf[2] = d;
    write_command(_this, cmd_buf, 3);
  } else if (a == 0xC0) {
    _this->use_opl = 1;
    _this->opl_adr = d;
    if (d == 0x0f) {
      _this->use_y8950_adpcm = 1;
    }
  } else if (a == 0xC1) {
    cmd_buf[0] = 0x5C;
    cmd_buf[1] = _this->opl_adr;
    cmd_buf[2] = d;
    write_command(_this, cmd_buf, 3);
  } else if (a == 0xa0) { // PSG(A)
    _this->use_psg = 1;
    _this->psg_adr = d;
  } else if (a == 0xa1) { // PSG(D)
    cmd_buf[0] = 0xA0;
    cmd_buf[1] = _this->psg_adr;
    cmd_buf[2] = d;
    write_command(_this, cmd_buf, 3);
  } else if (a == 0x7E || a == 0x7F) { // SN76489
    _this->use_sng = 1;
    cmd_buf[0] = 0x50;
    cmd_buf[1] = d;
    write_command(_this, cmd_buf, 2);
  } else if (a == 0x06) { // GG Stereo
    cmd_buf[0] = 0x4F;
    cmd_buf[1] = d;
    write_command(_this, cmd_buf, 2);
  }
}

static void scc_handler(KSS2VGM *_this, uint32_t a, uint32_t d) {
  uint8_t cmd_buf[4];
  int port = -1, offset = 0;
  a = a & 0xFF;
  if (a <= 0x7F) {
    port = 0; // wave
    offset = a;
  } else if (a <= 0x89) {
    port = 1; // freq
    offset = a - 0x80;
    if (d != 0x00)
      _this->use_scc = 1;
  } else if (a <= 0x8E) {
    port = 2; // volume
    offset = a - 0x8A;
  } else if (a == 0x8F) {
    port = 3; // enable
    offset = 0;
  } else if (a == 0x90 && a <= 0x99) {
    port = 1; // freq
    offset = a - 0x90;
    if (d != 0x00)
      _this->use_scc = 1;
  } else if (a == 0x9A && a <= 0x9E) {
    port = 2; // volume
    offset = a - 0x9A;
  } else if (a == 0x9F) {
    port = 3; // enable
    offset = 0;
  } else if (a <= 0xDF) {
    port = -1;
    offset = 0;
  } else if (a <= 0xFF) {
    port = 5;
    offset = a - 0xe0;
  }

  if (0 <= port) {
    if (_this->scc_initialized == 0) {
      cmd_buf[0] = 0xD2;
      cmd_buf[1] = 3;
      cmd_buf[2] = 0;
      cmd_buf[3] = 0x1f;
      write_command(_this, cmd_buf, 4);
      _this->scc_initialized = 1;
    }
    cmd_buf[0] = 0xD2;
    cmd_buf[1] = port;
    cmd_buf[2] = offset;
    cmd_buf[3] = d;
    write_command(_this, cmd_buf, 4);
  }
}

static void scc_plus_handler(KSS2VGM *_this, uint32_t a, uint32_t d) {
  uint8_t cmd_buf[4];
  int port = -1, offset = 0;
  a = a & 0xFF;
  if (a <= 0x7F) {
    port = 0; // wave[0-4]
    offset = a;
  } else if (a <= 0x9F) {
    port = 4; // wave[5]
    offset = a;
  } else if (a <= 0xA9) {
    port = 1; // freq
    offset = a - 0xA0;
    if (d != 0x00)
      _this->use_scc_plus = 1;
  } else if (0xAA <= a && a <= 0xAE) {
    port = 2; // volume
    offset = a - 0xAA;
  } else if (a == 0xAF) {
    port = 3; // enable
    offset = 0;
  } else if (a == 0xB0 && a <= 0xB9) {
    port = 1; // freq
    offset = a - 0xB0;
    if (d != 0x00)
      _this->use_scc_plus = 1;
  } else if (a == 0xBA && a <= 0xBE) {
    port = 2; // volume
    offset = a - 0xBA;
  } else if (a == 0xBF) {
    port = 3; // enable
    offset = 0;
  } else if (a <= 0xDF) {
    port = 5;
    offset = a - 0xC0;
  } else {
    port = -1;
    offset = 0;
  }

  if (0 <= port) {
    if (_this->scc_initialized == 0) {
      cmd_buf[0] = 0xD2;
      cmd_buf[1] = 3;
      cmd_buf[2] = 0;
      cmd_buf[3] = 0x1f;
      write_command(_this, cmd_buf, 4);
      _this->scc_initialized = 1;
    }
    cmd_buf[0] = 0xD2;
    cmd_buf[1] = port;
    cmd_buf[2] = offset;
    cmd_buf[3] = d;
    write_command(_this, cmd_buf, 4);
  }
}

static void memwrite_handler(void *context, uint32_t a, uint32_t d) {
  KSS2VGM *_this = context;
  if (0x9800 <= a && a <= 0x98FF) {
    scc_handler(_this, a, d);
  } else if (0xB800 <= a && a <= 0xB8FF) {
    scc_plus_handler(_this, a, d);
  }
}

static uint8_t y8950_adpcm_init[15] = {
    0x67, 0x66,             // Data Block Command
    0x88,                   // Block Type: Y8950 DELTA PCM ROM/RAM
    0x08, 0x00, 0x00, 0x00, // Size of The Block
    0x00, 0x80, 0x00, 0x00, // ROM/RAM Image Size
    0x00, 0x00, 0x00, 0x00  // Start Offset of Data
};

static KSS2VGM_Result *build_vgm(KSS2VGM *_this, int volume) {

  uint8_t vgm_header[0x100];
  int vgm_header_size = sizeof(vgm_header);
  uint32_t vgm_size = vgm_header_size + _this->ini_data->size + _this->vgm_data->size;
  uint8_t *vgm = malloc(vgm_size);

  KSS2VGM_Result *res = calloc(sizeof(KSS2VGM_Result), 1);
  res->vgm = vgm;
  res->vgm_size = vgm_size;

  uint8_t *wp = vgm;
  create_vgm_header(_this, vgm_header, vgm_header_size, volume);
  memcpy(wp, vgm_header, vgm_header_size);
  wp += vgm_header_size;

  if (_this->ini_data->size != 0) {
    memcpy(wp, _this->ini_data->buffer, _this->ini_data->size);
    wp += _this->ini_data->size;
  }

  memcpy(wp, _this->vgm_data->buffer, _this->vgm_data->size);

  return res;
}

KSS2VGM *KSS2VGM_new(void) {
  KSS2VGM *obj = calloc(sizeof(KSS2VGM), 1);
  return obj;
}

static void reset(KSS2VGM *_this) {
  data_array_delete(_this->ini_data);
  data_array_delete(_this->vgm_data);
  memset(_this, 0, sizeof(KSS2VGM));
  _this->ini_data = data_array_new();
  _this->vgm_data = data_array_new();
}

KSS2VGM_Result *KSS2VGM_kss2vgm(KSS2VGM *_this, KSS *kss, int play_time, int song_num, int loop_num, int volume) {

  reset(_this);

  KSSPLAY *kssplay = KSSPLAY_new(44100, 1, 16);
  KSSPLAY_set_iowrite_handler(kssplay, _this, iowrite_handler);
  KSSPLAY_set_memwrite_handler(kssplay, _this, memwrite_handler);
  KSSPLAY_set_data(kssplay, kss);
  KSSPLAY_reset(kssplay, song_num, 0);

  int i, t;
  for (t = 0; t < play_time; t++) {
    for (i = 0; i < 44100; i++) {
      KSSPLAY_calc_silent(kssplay, 1);
      _this->total_samples++;
      if (KSSPLAY_get_stop_flag(kssplay)) {
        break;
      }
      if ((loop_num > 0 && KSSPLAY_get_loop_count(kssplay) >= loop_num)) {
        break;
      }
    }
  }

  KSSPLAY_delete(kssplay);

  write_eos_command(_this);

  if (_this->use_y8950_adpcm) {
    ini_write(_this, y8950_adpcm_init, 15);
  }

  return build_vgm(_this, volume);
}

void KSS2VGM_delete(KSS2VGM *_this) {
  if (_this != NULL) {
    data_array_delete(_this->ini_data);
    data_array_delete(_this->vgm_data);
    free(_this);
  }
}

void KSS2VGM_Result_delete(KSS2VGM_Result *obj) {
  if (obj != NULL) {
    free(obj->vgm);
    free(obj);
  }
}
