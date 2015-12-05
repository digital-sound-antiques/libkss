#include "../ksstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _tagFIR
{
  double h[65];
  k_int32 buf[65];
  double Wc;
  k_uint32 M;
} FIR;

FIR *FIR_new(void);
k_int32 FIR_calc(FIR *, k_int32 data);
void FIR_delete(FIR *);
void FIR_reset(FIR *, k_uint32 sam, k_uint32 cut, k_uint32 n);
void FIR_disable(FIR *);

#ifdef __cplusplus
}
#endif
