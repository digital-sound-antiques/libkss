/*
 * kssinfo - read a music file, detect its type, and print its title.
 *
 * Exercises KSS_check_type() and KSS_bin2kss() (which builds the KSS
 * object and fills in the title). Handy for
 * batch-checking a corpus, e.g. verifying OPX detection:
 *
 *   kssinfo *.opx
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/kss/kss.h"

static const char *type_name(uint8_t type) {
  switch (type) {
  case KSSDATA:
    return "KSS";
  case MGSDATA:
    return "MGS";
  case MBMDATA:
    return "MBM";
  case MPK106DATA:
    return "MPK106";
  case MPK103DATA:
    return "MPK103";
  case BGMDATA:
    return "BGM";
  case OPXDATA:
    return "OPX";
  case FMDATA:
    return "FM";
  default:
    return "UNKNOWN";
  }
}

static uint8_t *read_file(const char *path, uint32_t *size_out) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL)
    return NULL;

  fseek(fp, 0, SEEK_END);
  long len = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (len < 0) {
    fclose(fp);
    return NULL;
  }

  uint8_t *buf = malloc(len > 0 ? (size_t)len : 1);
  if (buf == NULL) {
    fclose(fp);
    return NULL;
  }

  size_t got = fread(buf, 1, (size_t)len, fp);
  fclose(fp);
  *size_out = (uint32_t)got;
  return buf;
}

static void process(const char *path) {
  uint32_t size = 0;
  uint8_t *data = read_file(path, &size);
  if (data == NULL) {
    printf("%-40s  %-8s  %s\n", path, "ERROR", "(cannot read file)");
    return;
  }

  int type = KSS_check_type(data, size, path);

  const char *title = "";
  KSS *kss = KSS_bin2kss(data, size, path);
  if (kss != NULL)
    title = KSS_get_title(kss);

  printf("%-40s  %-8s  %s\n", path, type_name((uint8_t)type), title);

  if (kss != NULL)
    KSS_delete(kss);
  free(data);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <file> [<file> ...]\n", argv[0]);
    fprintf(stderr, "  Detect the type of each input file and print its title.\n");
    return 1;
  }

  for (int i = 1; i < argc; i++)
    process(argv[i]);

  return 0;
}
