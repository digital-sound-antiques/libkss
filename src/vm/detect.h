#ifndef __DETECT_H__
#define __DETECT_H__
#include "../ksstypes.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct __LPDETECT__ {
  k_int32 m_bufsize, m_bufmask;
  k_int32 *m_stream_buf;
  k_int32 *m_time_buf;
  k_int32 m_bidx;
  k_int32 m_blast;                    // 前回チェック時のbidx;
  k_int32 m_wspeed;
  k_int32 m_current_time;
  k_int32 m_loop_start, m_loop_end;
  k_int32 m_empty;
} LPDETECT;

LPDETECT *LPDETECT_new(void);
void LPDETECT_delete(LPDETECT *__this);
void LPDETECT_reset(LPDETECT *__this);
int LPDETECT_write(LPDETECT *__this, k_uint32 adr, k_uint32 val);
int LPDETECT_update(LPDETECT *__this, k_int32 time_in_ms, k_int32 match_second, k_int32 match_interval);
int LPDETECT_count(LPDETECT *__this, k_int32 time_in_ms);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __DETECT_H__ */
