#ifndef _DC_FILTER_H_
#define _DC_FILTER_H_
#include "../ksstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _tagDCF
{
  k_uint32 enable;
  double weight;
  double in, out;
} DCF;

DCF *DCF_new(void);
k_int32 DCF_calc(DCF *, k_int32 data);
void DCF_delete(DCF *);
void DCF_reset(DCF *, double rate);
void DCF_disable(DCF *);

#ifdef __cplusplus
}
#endif

#endif
