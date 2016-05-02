/* KSSPLAY SAMPLE for ANSI C. Play KSS file and write wave file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kssplay.h"

#ifdef EMSCRIPTEN
#include <emscripten.h>
#endif

#define MAX_RATE 96000
#define MAX_PATH 256

static void WORD(char *buf, uint32_t data) {
  buf[0] = data & 0xff ;
  buf[1] = (data & 0xff00) >> 8 ;
}

static void DWORD(char *buf, uint32_t data) {
  buf[0] = data & 0xff ;
  buf[1] = (data & 0xff00) >> 8 ;
  buf[2] = (data & 0xff0000) >> 16 ;
  buf[3] = (data & 0xff000000) >> 24 ;
}

static void chunkID(char *buf, char id[4]){
  buf[0] = id[0] ;
  buf[1] = id[1] ;
  buf[2] = id[2] ;
  buf[3] = id[3] ;
}

static void create_wav_header(char *header, int rate, int nch, int bps)
{
  /* Create WAV header */
  chunkID(header,"RIFF") ;
  DWORD(header+4,0) ;               /* data length * bytePerSample + 36 */
  chunkID(header+8,"WAVE") ;
  chunkID(header+12,"fmt ") ;
  DWORD(header+16,16) ;
  WORD(header+20,1) ;               /* WAVE_FORMAT_PCM */
  WORD(header+22,nch) ;             /* channel 1=mono,2=stereo */
  DWORD(header+24,rate) ;           /* samplesPerSec */
  DWORD(header+28,nch*bps/8*rate) ; /* bytesPerSec */
  WORD(header+32,2) ;               /* blockSize */
  WORD(header+34,bps) ;             /* bitsPerSample */
  chunkID(header+36,"data") ;
  DWORD(header+40,0) ;              /* data length * bytePerSample */
}

#define HLPMSG \
  "Usage: kss2wav FILENAME.KSS <Options>\n" \
  "Options: \n" \
  "  -s<song_num>\n  -p<play_time> (sec)\n  -f<fade_time> (sec)\n  -l<loop_num>\n  -r<play_freq>\n  -q<quality> (0 or 1)\n -o<outfile>\n"

int main(int argc, char *argv[])
{
  static char header[46] ;
  char wavefile[MAX_PATH] = "" ;
  short wavebuf[MAX_RATE] ;
  int rate = 44100, nch = 1, bps = 16 ;
  int song_num = 0, play_time = 60, fade_time = 5, loop_num = 1 ;
  int data_length = 0, i, t ;
  int quality = 0;

  KSSPLAY *kssplay ;
  KSS *kss ;
  FILE *fp ;

#ifdef EMSCRIPTEN
  EM_ASM(
    FS.mkdir('/data');
    FS.mount(NODEFS, {root:'.'}, '/data');
  );
#endif

  if(argc<2)
  {
    fprintf(stderr,HLPMSG) ;
    exit(0) ;
  }

  /* Create Default WAV filename */
  for(i=0;argv[1][i]!='\0';i++)
  {
    if(argv[1][i]=='.') break ;
    wavefile[i]=argv[1][i] ;
  }
  wavefile[i] = '\0' ;
  strcat(wavefile, ".wav") ;

  /* Phase parameters */
  for(i=2;i<argc;i++)
  {
    if(argv[i][0]=='-')
    {
      int j ;
      switch(argv[i][1])
      {
      case 'p':
        play_time = atoi(argv[i]+2) ;
        break ;
      case 's':
        song_num = atoi(argv[i]+2) ;
        break ;
      case 'f':
        fade_time = atoi(argv[i]+2) ;
        break ;
      case 'r':
        rate = atoi(argv[i]+2) ;
        break ;
      case 'q':
        quality = atoi(argv[i]+2) ;
        break;
      case 'l':
        loop_num = atoi(argv[i]+2) ;
        if(loop_num==0) loop_num = 256 ;
        break ;
      case 'o':
        for(j=0; argv[i][j+2]; j++) wavefile[j]=argv[i][j+2] ;
        wavefile[j]='\0' ;
        break ;
      default:
        fprintf(stderr,HLPMSG) ;
        exit(1) ;
        break ;
      }
    }
    else
    {
      fprintf(stderr,HLPMSG) ;
      exit(1) ;
    }
  }
  if(rate>MAX_RATE) rate = 44100 ;

  printf("SONG:%02d, PLAYTIME:%d(sec), FADETIME:%d(sec), RATE:%d\n", song_num, play_time, fade_time, rate) ;

  /* If you load following drivers, this program can play MGS, MPK and MuSICA files. */
  /*
  KSS_load_mgsdrv("MGSDRV.COM") ;
  KSS_load_mpk106("MPK.BIN") ;
  KSS_load_mpk103("MPK103.BIN") ;
  KSS_load_kinrou("KINROU5.DRV") ;
  KSS_load_fmbios("FMBIOS.ROM") ;
  */

  if((kss=KSS_load_file(argv[1]))== NULL)
  {
    fprintf(stderr,"FILE READ ERROR!\n") ;
    exit(1) ;
  }

  /* Print title strings */
  printf("[%s]", kss->idstr) ;
  printf("%s\n", kss->title) ;
  if(kss->extra) printf("%s\n", kss->extra) ;

  /* Open WAV file */
  if((fp=fopen(wavefile,"wb"))==NULL)
  {
    fprintf(stderr,"Can't open %s\n", wavefile) ;
    exit(1) ;
  }

  create_wav_header(header, rate, nch, bps) ;

  fwrite(header, sizeof(header), 1, fp) ; /* Write dummy header */

  /* INIT KSSPLAY */
  kssplay = KSSPLAY_new(rate, nch, bps) ;
  KSSPLAY_set_data(kssplay, kss) ;
  KSSPLAY_reset(kssplay, song_num, 0) ;

  KSSPLAY_set_device_quality(kssplay, EDSC_PSG, quality);
  KSSPLAY_set_device_quality(kssplay, EDSC_SCC, quality);
  KSSPLAY_set_device_quality(kssplay, EDSC_OPLL, quality);

  /* Create WAV Data */
  for(t=0; t<play_time; t++)
  {
    int pos ;

    printf("%03d/%03d",t+1,play_time) ; fflush(stdout) ;

    /* Create 1 sec wave block */
    KSSPLAY_calc(kssplay, wavebuf, rate) ;
    
    /* force little endian (for sparc, powerPC, etc...) */
    for(i=0; i<rate; i++) WORD((char *)(wavebuf+i), wavebuf[i]*2) ;

    /* Write 1 sec wave block to file */
    fwrite(wavebuf, sizeof(short), rate, fp) ;
    pos = ftell(fp) ;

    /* Update header */
    data_length += rate ;
    DWORD(header+4, data_length * 2 + 36) ;
    DWORD(header+40, data_length * 2) ;
    fseek(fp, 0, SEEK_SET) ;
    fwrite(header, sizeof(header), 1, fp) ;
    fseek(fp, pos, SEEK_SET) ;

    /* If looped, start fadeout */
    if((KSSPLAY_get_loop_count(kssplay)>=loop_num||
      (play_time-fade_time)<=t+1)&&
      !KSSPLAY_get_fade_flag(kssplay))
    {
      KSSPLAY_fade_start(kssplay, fade_time*1000) ;
    }

    /* If the fade is ended or the play is stopped, break */
    if(KSSPLAY_get_fade_flag(kssplay)==KSSPLAY_FADE_END||KSSPLAY_get_stop_flag(kssplay)) break ;

    printf("\x08\x08\x08\x08\x08\x08\x08") ;
  }

  fclose(fp) ;
  KSSPLAY_delete(kssplay) ;
  KSS_delete(kss) ;

  printf("\n") ;
  return 0 ;
}