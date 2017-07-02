#ifndef __MTLK_WSS_H__
#define __MTLK_WSS_H__

/* Wireless State Syndication Module */

#include "mtlkerr.h"
#include "mtlk_osal.h"
#include "mtlk_wss_id.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef struct _mtlk_wss_t       mtlk_wss_t;

#ifndef MTCFG_ENABLE_OBJPOOL
mtlk_wss_t * __MTLK_IFUNC mtlk_wss_create(const mtlk_wss_t *parent_wss, const mtlk_wss_stat_id_e *stat_ids, uint32 nof_stats_ids);
#else
mtlk_wss_t * __MTLK_IFUNC __mtlk_wss_create_objpool(const mtlk_wss_t *parent_wss, const mtlk_wss_stat_id_e *stat_ids, uint32 nof_stat_ids, mtlk_slid_t caller_slid);

#define mtlk_wss_create(parent_wss, stat_ids, nof_stat_ids) \
  __mtlk_wss_create_objpool((parent_wss), (stat_ids), (nof_stat_ids), MTLK_SLID)
#endif

void         __MTLK_IFUNC mtlk_wss_delete(mtlk_wss_t *wss);

uint32       __MTLK_IFUNC mtlk_wss_get_stat(mtlk_wss_t *wss, uint32 stat_id_idx);
void         __MTLK_IFUNC mtlk_wss_reset_stat(mtlk_wss_t *wss, uint32 stat_id_idx);

typedef struct _mtlk_wss_stats_context_t
{
  mtlk_wss_stat_id_e stat_id;
  mtlk_atomic_t      value;
} __MTLK_IFUNC mtlk_wss_stats_context_t;

typedef struct _mtlk_wss_cntr_handle_t
{
  uint32                    nof_contexts;
  mtlk_wss_stats_context_t *contexts[1];
} __MTLK_IDATA mtlk_wss_cntr_handle_t;

mtlk_wss_cntr_handle_t * __MTLK_IFUNC mtlk_wss_cntr_open(mtlk_wss_t *wss, mtlk_wss_stat_id_e stat_id);
void                     __MTLK_IFUNC mtlk_wss_cntr_close(mtlk_wss_t *wss, mtlk_wss_cntr_handle_t *hcntr);

int                      __MTLK_IFUNC mtlk_wss_cntrs_open(mtlk_wss_t               *wss,
                                                          const mtlk_wss_stat_id_e *stat_ids, 
                                                          mtlk_wss_cntr_handle_t  **hctrls,
                                                          uint32                    nof_stats);
void                     __MTLK_IFUNC mtlk_wss_cntrs_close(mtlk_wss_t              *wss,
                                                           mtlk_wss_cntr_handle_t **hctrls,
                                                           uint32                   nof_stats);

static __INLINE void
mtlk_wss_cntr_inc (mtlk_wss_cntr_handle_t *hcntr)
{
  uint32 i = 0;

  for (; i < hcntr->nof_contexts; i++) {
    mtlk_osal_atomic_inc(&hcntr->contexts[i]->value);
  }
}

static __INLINE void
mtlk_wss_cntr_dec (mtlk_wss_cntr_handle_t *hcntr)
{
  uint32 i = 0;

  for (; i < hcntr->nof_contexts; i++) {
    mtlk_osal_atomic_dec(&hcntr->contexts[i]->value);
  }
}

static __INLINE void
mtlk_wss_cntr_add (mtlk_wss_cntr_handle_t *hcntr, uint32 val)
{
  uint32 i = 0;

  for (; i < hcntr->nof_contexts; i++) {
    mtlk_osal_atomic_add(&hcntr->contexts[i]->value, val);
  }
}

static __INLINE void
mtlk_wss_cntr_sub (mtlk_wss_cntr_handle_t *hcntr, uint32 val)
{
  uint32 i = 0;

  for (; i < hcntr->nof_contexts; i++) {
    mtlk_osal_atomic_sub(&hcntr->contexts[i]->value, val);
  }
}

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_WSS_H__ */
