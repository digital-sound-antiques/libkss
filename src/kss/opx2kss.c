#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kss.h"

#ifdef EMBED_OPXDRV
static k_uint8 OPXDRV[16384] =
{
#include "../drivers/opx4kss.h"
} ;
static k_uint32 opxdrv_size = sizeof(OPXDRV) ;
#else
static k_uint32 opxdrv_size ;
static k_uint8 OPXDRV[16384];
#endif

#ifdef EMBED_FMBIOS
static k_uint8 FMBIOS[16384] =
{
#include "fmbios.h"
} ;
static k_uint32 fmbios_size = sizeof(FMBIOS) ;
#else
static k_uint32 fmbios_size ;
static k_uint8 FMBIOS[16384];
#endif

int KSS_isOPXdata(k_uint8 *data, k_uint32 size)
{
  if((128+32<size)&&/*data[0x7B]==0x0D&&data[0x7C]==0x0A&&*/data[0x7D]==0x1A) return 1 ;
  else return 0 ;
}

int KSS_set_fmbios(const k_uint8 *data, k_uint32 size)
{
  if(size>16384) return 1 ;
  memcpy(FMBIOS, data, size) ;
  return 0 ;
}

int KSS_load_fmbios(const char *filename)
{
  FILE *fp ;

  if((fp=fopen(filename,"rb"))==NULL) return 1 ;

  fmbios_size = 16384 ;
  fread(FMBIOS,1,fmbios_size,fp) ;
  
  fclose(fp) ;

  return 0 ;
}

int KSS_set_opxdrv(const k_uint8 *data, k_uint32 size)
{
  if(size>16384) return 1 ;
  memcpy(OPXDRV,data,size) ;
  return 0 ;
}

int KSS_load_opxdrv(const char *opxdrv)
{
  FILE *fp ;

  if((fp=fopen(opxdrv,"rb"))==NULL) return 1 ;

  fseek(fp,0,SEEK_END) ;
  opxdrv_size = ftell(fp) ;

  if(opxdrv_size>16384)
  {
    fclose(fp) ;
    return 1 ;
  }

  fseek(fp,0,SEEK_SET) ;
  fread(OPXDRV,1,opxdrv_size,fp) ;
  
  fclose(fp) ;

  return 0 ;
}

void KSS_get_info_opxdata(KSS *kss, k_uint8 *data, k_uint32 size)
{
  int i ;

  strcpy((char *)kss->idstr,"OPX") ;
  if(size<128)
  {
    strcpy((char *)kss->title,"Not a opx file.") ;
    return ;
  }

  memcpy(kss->title, data, 53) ;
  for(i=52 ; i>=0 ; i--)
  {
    if(kss->title[i]!=0x20) break ;
    kss->title[i] = '\0' ;
  }
  kss->title[53] = '\0' ;

  kss->extra = malloc(55+15+1) ;
  if(kss->extra!=NULL)
  {
    memcpy(kss->extra + 0 , data + 55      , 55) ;
    memcpy(kss->extra + 55, data + 55 + 55 , 15) ;
    kss->extra[55+15] = '\0' ;
  }

  kss->loop_detectable = 1 ;
  kss->stop_detectable = 1 ;
}

#define OFFSET(b,l,x) (b + KSS_HEADER_SIZE + x - l)

KSS *KSS_opx2kss(k_uint8 *data, k_uint32 size)
{
  KSS *kss ;
  int i ;

  k_uint8 *buf ;
  k_uint16 load_adr = 0x0100, load_size = 0x8000 + size - load_adr ;
  k_uint16 init_adr = 0x0106, play_adr = 0x0103 ;

  if(size<16||opxdrv_size==0) return NULL ;

  if((buf=malloc( load_size + KSS_HEADER_SIZE ))==NULL) return NULL ;

  KSS_make_header(buf, load_adr, load_size, init_adr, play_adr) ;
  memcpy(OFFSET(buf,load_adr,0x0100), OPXDRV, opxdrv_size) ;

  if(fmbios_size)
    memcpy(OFFSET(buf,load_adr,0x4000), FMBIOS, fmbios_size) ;
  else
  {
    memset(OFFSET(buf, load_adr, 0x4000), 0, 0x4000) ;
    memcpy(OFFSET(buf,load_adr,0x401C), "OPLL", 4) ;
    for(i=0;i<62;i++)
    {
      memcpy(OFFSET(buf,load_adr,0x62EC+i*32), 
        "Pia34567"
        "\x00\x00\x0e\x00\x00\x00\x00\x00"
        "\x22\x1B\xf0\x0f\x00\x00\x00\x00"
        "\x21\x00\xf0\x0f\x00\x00\x00\x00",
        32) ;
    }
  }
  memcpy(OFFSET(buf,load_adr,0x8000), data, size) ;

  kss = KSS_new(buf , ( load_size + KSS_HEADER_SIZE ) ) ;

  free(buf) ;
  return kss ;
}
