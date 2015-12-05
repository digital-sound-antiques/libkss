#include <stdio.h>
#include <stdlib.h>
#include "dc_filter.h"

DCF* DCF_new()
{
  return malloc(sizeof(DCF));
}

k_int32 DCF_calc(DCF *dcf, k_int32 data)
{
  if(dcf->enable)
  {
    dcf->out = dcf->weight * ( dcf->out + data - dcf->in );
    dcf->in = data;
    return (k_int32)dcf->out;
  }
  else
  {
    return data;
  }
}

void DCF_disable(DCF *dcf)
{
  dcf->enable = 0;
}

void DCF_delete(DCF *dcf)
{
  free(dcf);
}

void DCF_reset(DCF *dcf, double rate)
{
  /** R:47K, C:1uF, Cutoff: 2pi*R*C Hz */
  dcf->weight = 1.0/((1.0/rate)/(1.0*(1.0e-06)*47000)+1.0);
  dcf->in = dcf->out = 0;
}
