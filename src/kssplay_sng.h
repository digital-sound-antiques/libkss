#ifndef _KSSPLAY_SNG_H_
#define _KSSPLAY_SNG_H_

typedef struct tagKSSPLAY KSSPLAY;
typedef struct tagKSSPLAY_SNG KSSPLAY_SNG;

void KSSPLAY_SNG_reset(KSSPLAY *kssplay);
void KSSPLAY_SNG_process(KSSPLAY *kssplay);
void KSSPLAY_SNG_delete(KSSPLAY *kssplay);

#endif
