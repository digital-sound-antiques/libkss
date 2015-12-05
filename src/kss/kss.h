#ifndef _KSSOBJ_H_
#define _KSSOBJ_H_
#include "../ksstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {KSS_MSX=0, KSS_SEGA=1} ;

#define KSS_TITLE_MAX 256
#define KSS_HEADER_SIZE 0x20

typedef struct tagKSSINFO {
  int song;
  int type;
  char title[KSS_TITLE_MAX];
  int time_in_ms;
  int fade_in_ms;
} KSSINFO;

typedef struct tagKSS
{
  k_uint32 type ;
  k_uint32 stop_detectable ; 
  k_uint32 loop_detectable ; 
  k_uint8 title[KSS_TITLE_MAX] ; /* the title */
  k_uint8 idstr[8] ; /* ID */
  k_uint8 *data ;    /* the KSS binary */
  k_uint32 size ;    /* the size of KSS binary */
  k_uint8 *extra ;   /* Infomation text for KSS info dialog */


  int kssx ; /* 0:KSCC, 1:KSSX */
  int mode ; /* 0:MSX 1:SMS 2:GG */

  int fmpac ;
  int fmunit ;
  int sn76489 ;
  int ram_mode ;
  int msx_audio ;
  int stereo ;
  int pal_mode ;

  int DA8_enable ;
  
  k_uint16 load_adr ;
  k_uint16 load_len ;
  k_uint16 init_adr ;
  k_uint16 play_adr ;

  k_uint8 bank_offset ;
  k_uint8 bank_num ;
  k_uint8 bank_mode ;
  k_uint8 extra_size ;
  k_uint8 device_flag ;
  k_uint8 trk_min ;
  k_uint8 trk_max ;
  k_uint8 vol[EDSC_MAX] ;

  k_uint16 info_num;
  KSSINFO *info;

} KSS ;

KSS *KSS_new(k_uint8 *data, k_uint32 size) ;
void KSS_delete(KSS *kss) ;

enum {KSSDATA, MGSDATA, MBMDATA, MPK106DATA, MPK103DATA, BGMDATA, OPXDATA, FMDATA, KSS_TYPE_UNKNOWN} ;

int KSS_check_type(k_uint8 *data, k_uint32 size, const char *filename) ;

int KSS_load_mgsdrv(const char *filename) ;
int KSS_set_mgsdrv(const k_uint8 *data, k_uint32 size) ;
int KSS_load_kinrou(const char *filename) ;
int KSS_set_kinrou(const k_uint8 *data, k_uint32 size) ;
int KSS_load_mpk106(const char *filename) ;
int KSS_set_mpk106(const k_uint8 *data, k_uint32 size) ;
int KSS_load_mpk103(const char *filename) ;
int KSS_set_mpk103(const k_uint8 *data, k_uint32 size) ;
int KSS_load_opxdrv(const char *filename) ;
int KSS_set_opxdrv(const k_uint8 *data, k_uint32 size) ;
int KSS_load_fmbios(const char *filename) ;
int KSS_set_fmbios(const k_uint8 *data, k_uint32 size) ;
int KSS_load_mbmdrv(const char *filename) ;
int KSS_set_mbmdrv(const k_uint8 *data, k_uint32 size) ;

void KSS_get_info_mgsdata(KSS *, k_uint8 *data, k_uint32 size);
void KSS_get_info_bgmdata(KSS *, k_uint8 *data, k_uint32 size);
void KSS_get_info_mpkdata(KSS *, k_uint8 *data, k_uint32 size);
void KSS_get_info_opxdata(KSS *, k_uint8 *data, k_uint32 size);
void KSS_get_info_kssdata(KSS *, k_uint8 *data, k_uint32 size);
void KSS_get_info_mbmdata(KSS *, k_uint8 *data, k_uint32 size);

void KSS_make_header(k_uint8 *header, k_uint16 load_adr, k_uint16 load_size, k_uint16 init_adr, k_uint16 play_adr);

KSS *KSS_mgs2kss(k_uint8 *data, k_uint32 size) ;
KSS *KSS_bgm2kss(k_uint8 *data, k_uint32 size) ;
KSS *KSS_opx2kss(k_uint8 *data, k_uint32 size) ;
KSS *KSS_mpk1032kss(k_uint8 *data, k_uint32 size) ;
KSS *KSS_mpk1062kss(k_uint8 *data, k_uint32 size) ;
KSS *KSS_kss2kss(k_uint8 *data, k_uint32 size) ;
KSS *KSS_mbm2kss(const k_uint8 *data, k_uint32 size) ;
void KSS_set_mbmparam(int m, int, int s) ;
KSS *KSS_bin2kss(k_uint8 *data, k_uint32 size, const char *filename) ;
int KSS_load_mbk(const char *filename) ;
int KSS_autoload_mbk(const char *mbmfile, const char *extra_path, const char *dummy_file);

int KSS_isMGSdata(k_uint8 *data, k_uint32 size);
int KSS_isBGMdata(k_uint8 *data, k_uint32 size);
int KSS_isMPK106data(k_uint8 *data, k_uint32 size);
int KSS_isMPK103data(k_uint8 *data, k_uint32 size);
int KSS_isOPXdata(k_uint8 *data, k_uint32 size);

KSS *KSS_load_file(char *fn) ;

void KSS_info2pls(KSS *kss, char *base, int index, char *buf, int buflen, int playtime, int fadetime);

#ifdef __cplusplus
}
#endif

#endif
