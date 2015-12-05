#ifndef _KSSTYPES_H_
#define _KSSTYPES_H_

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char k_uint8 ;
typedef signed char k_int8 ;

typedef unsigned short k_uint16 ;
typedef signed short k_int16 ;

typedef unsigned int k_uint32 ;
typedef signed int k_int32 ;

enum EmuDeviceSerialCode
{
  EDSC_PSG=0,
  EDSC_SCC=1,
  EDSC_OPLL=2,
  EDSC_OPL=3,
  EDSC_MAX=4,
} ;

enum { KSS_16K, KSS_8K } ;

#ifdef __cplusplus
}
#endif
#endif
