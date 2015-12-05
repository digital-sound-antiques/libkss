#ifndef _RC_FILTER_H_
#define _RC_FILTER_H_
#include "../ksstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _tagRCF
{
  k_uint32 enable;
  double out;
  double a;
} RCF;

RCF *RCF_new(void);
k_int32 RCF_calc(RCF *, k_int32 data);
void RCF_delete(RCF *);
void RCF_reset(RCF *, double rate, double R, double C);
void RCF_disable(RCF *);

#ifdef __cplusplus
}
#endif

#endif
