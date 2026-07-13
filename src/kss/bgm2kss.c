#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kss.h"

static uint8_t KINROU[8192] = {
#if !defined(EXCLUDE_DRIVER_ALL) && !defined(EXCLUDE_DRIVER_KINROU)
#include "drivers/kinrou5.h"
#endif
};
static uint32_t kinrou_size = sizeof(KINROU);

static uint8_t kinrou_init[0x100] = {
    0xCD, 0x20, 0x60,       /* CALL	6020H */
    0x3E, 0x3F,             /* LD	A,3FH */
    0x32, 0x10, 0x60,       /* LD	(6010H),A	; OPLL SLOT */
    0x32, 0x11, 0x60,       /* LD	(6011H),A	; SCC SLOT */
    0x21, 0x07, 0x80,       /* LD HL,A007H ; DATA ADDRESS */
    0xED, 0x5B, 0x01, 0x80, /* LD	DE,(A001H) ; COMPILE ADDRESS */
    /* INIT FLAG WORK */
    0x3E, 0x00,             /* LD	A,0 ; LOOP NUMBER */
    0xCD, 0x26, 0x60,       /* CALL	6026H */
    0xAF, 0xCD, 0x38, 0x60, /* CALL	6038H	*/

    0x3E, 0x7F, /* LD  A,0FFH */
    0xD3, 0x40, /* OUT (040H),A */
    0xAF,       /* XOR A */
    0xD3, 0x41, /* OUT (041H),A ; PLAY FLAG  */
    0xD3, 0x42, /* OUT (042H),A ; LOOP COUNT */

    0xC9 /* RET */
};

static uint8_t kinrou_play[0x100] = {
    0xCD, 0x29, 0x60, /* CALL PLAY */
    0xCD, 0x32, 0x60, /* CALL PLAYCHK */
    0xE6, 0x01,       /* AND 01H */
    0xAF,             /* XOR A */
    0xD3, 0x41,       /* OUT (041H),A ; STOP FLAG */
    0x2B,             /* DEC HL */
    0x7D,             /* LD A,L */
    0xD3, 0x42,       /* OUT (042H),A ; LOOP COUNT */
    0xC9              /* RET */
};

int KSS_isBGMdata(uint8_t *data, uint32_t size) {
  // improved bgm detection by Kaens (see https://github.com/Kaens/audio1sg)

#define rU16(x) (data[x] | (data[x + 1] << 8))     // uint16 LE read helper
#define within(x, l, r) ((l) <= (x) && (x) <= (r)) // a shortcut

  // MSX binary check
  if (data[0] != 0xFE || size < 7 + 1 + 17 * 2) // BIN header + loop byte + 17 pointers to channel flows
    return 0;

  // A BSAVE binary cannot be larger than its 7-byte header plus 65535 bytes of
  // data (the maximum the MSX-BASIC BSAVE command can write), so anything bigger
  // cannot be a BGM.
  if (size > 7 + 65535)
    return 0;

  const uint16_t base = rU16(1),  // RAM base address
      sz = 8 + rU16(3) - base,    // calculated=expected file size
      Mp = sz > size ? size : sz; // maximum offset

  if (base && !within(base, 0x4000, 0xFF80)) return 0;

  int32_t p = 7 + rU16(5) - base, // entrypoint as file offset — for starters
      y = 20;                     // y for "yes it's a bgm": confidence

  if (!within(p, 0, size)) return 0;

  uint16_t mp = p + 1 + 17 * 2; // minimum offset

  if (sz <= mp || size <= mp) return 0;
  if (sz > size) y -= 15;
  else y += 5; // calcsize should fit in the filesize, but we can detect a corrupt file
  if (!within(sz, 80, 16384)) y -= 10; // suspicious calcsize
  if (data[p] > 1) y -= 20;
  else y += 5; // unusual loop value, but also a decent check

  // metadata tag presence check
  if (size >= 0x60 && !strncmp((const char *)(data + 0x50), "BTO", 3))
    y += 30; // auspicious occurrence! don't just trust it though, it's only 3 ASCII chars
  if (y <= 0) return 0; // confidence break-off

  // 17 channel pointer read & check
  uint16_t chp[17];
  int32_t t;
  uint8_t i, ch = 0;

  for (i = 0, ++p; i < 17 && y > 0 && p < Mp; ++i, p += 2) {
    t = rU16(p);
    if (!t) continue;
    t += 7 - base; // normalize the offset
    if (within(t, mp, sz - 1)) y += 3;
    else {
      y -= 15;
      continue;
    } // offsets must fit (may not mean much). Maybe a couple are broken
    chp[ch++] = t;
    if (p + ch * 4 > mp) mp = p + ch * 4; // a new minimum pointer value, yay
  }
  if (!ch || y <= 0) return 0; // at least 1 channel must be present (I saw 2 at least)

  // per-channel block list, pointers
  uint16_t chc, totalblk = 0; // a channel's block counter
  uint8_t FFencountered = 0;  // gotta find at least

  for (i = 0; i < ch && y > 0; ++i) {
    p = chp[i];
    t = rU16(p); // select the next channel's block list

    // check each pointer to a block (u16 ptr, u8 repetitions)
    for (chc = 0; y > 0 && p < Mp; ++chc) {
      if (!t) {
        p += 2;
        break;
      } // zero pointer has no repetitions, break off to the next channel
      t += 7 - base; // normalise offset
      if (within(t, mp, Mp - 1)) {
        y += 10; // these pointers are 2nd-stage so worth more points
        if (!data[p + 2]) y -= 10; // 0 times to play is suspicious, 1+ is expected
        if (data[t - 1] == 0xFF) {
          // the end-of-track marker from a previous block is a good sanity check!
          y += 15;
          FFencountered = 1;
        }
      } else y -= 15; // corrupt pointer?
      p += 3;
      t = rU16(p);
    }
    totalblk += chc;
    if (!chc) y -= 15; // no blocks in an active channel is suspicious
  }
  if (totalblk > 1 && !FFencountered) y -= 50;

  // it's super unlikely there's a file with just one channel or two channels pointing at the same block
  return p <= Mp && y > 0;
}

int KSS_set_kinrou(const uint8_t *data, uint32_t size) {
  if (size > 8192)
    return 1;
  memcpy(KINROU, data, size);
  return 0;
}

int KSS_load_kinrou(const char *kinrou) {
  FILE *fp;

  if ((fp = fopen(kinrou, "rb")) == NULL)
    return 1;

  fseek(fp, 0, SEEK_END);
  kinrou_size = ftell(fp);

  if (kinrou_size > 8192) {
    fclose(fp);
    return 1;
  }

  fseek(fp, 0, SEEK_SET);
  fread(KINROU, 1, kinrou_size, fp);

  fclose(fp);

  return 0;
}

void KSS_get_info_bgmdata(KSS *kss, uint8_t *data, uint32_t size) {
  static char extra[256];
  uint32_t offset, i;

  if (size > 0x60 && !strncmp((const char *)(data + 0x50), "BTO", 3)) {
    strcpy((char *)kss->idstr, "BGM:BTO");
    offset = data[0x5B] + (data[0x5C] << 8) - (data[0x59] + (data[0x5a] << 8)) + 7;
    for (i = 0; i < 255; i++) {
      if ((offset + i) >= size)
        break;
      if (data[offset + i] < 0x20)
        break;
      kss->title[i] = data[offset + i];
    }
    kss->title[i] = '\0';

    offset = data[0x5D] + (data[0x5E] << 8) - (data[0x59] + (data[0x5a] << 8)) + 7;
    for (i = 0; i < 255; i++) {
      if ((offset + i) >= size)
        break;
      if (data[offset + i] < 0x20)
        break;
      extra[i] = data[offset + i];
    }
    extra[i] = '\0';
    kss->extra = malloc(strlen(extra) + 1);
    if (kss->extra != NULL)
      strcpy((char *)kss->extra, extra);
  } else {
    strcpy((char *)kss->idstr, "BGM");
    kss->title[0] = '\0';
    kss->extra = NULL;
  }

  kss->loop_detectable = 1;
  kss->stop_detectable = 1;
}

KSS *KSS_bgm2kss(uint8_t *data, uint32_t size) {
  KSS *kss;
  uint8_t *buf;
  uint16_t load_adr = 0x6000, load_size = 0x8000 + size - load_adr;
  uint16_t init_adr = 0x7E00, play_adr = 0x7F00;

  if ((size < 16) || (kinrou_size == 0))
    return NULL;

  if ((buf = malloc(load_size + KSS_HEADER_SIZE)) == NULL)
    return NULL;

  KSS_make_header(buf, load_adr, load_size, init_adr, play_adr);
  buf[0x0F] = 0x01;
  memcpy(buf + KSS_HEADER_SIZE, KINROU + 0x07, kinrou_size - 7);
  memcpy(buf + KSS_HEADER_SIZE + 0x1E00, kinrou_init, 0x100);
  memcpy(buf + KSS_HEADER_SIZE + 0x1F00, kinrou_play, 0x100);
  memcpy(buf + KSS_HEADER_SIZE + 0x8000 - load_adr, data, size);

  kss = KSS_new(buf, (load_size + KSS_HEADER_SIZE));

  free(buf);
  return kss;
}
