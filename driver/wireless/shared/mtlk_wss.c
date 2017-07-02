#include "mtlkinc.h"
#include "mtlk_wss.h"

#define LOG_LOCAL_GID   GID_WSS
#define LOG_LOCAL_FID   0

typedef struct _mtlk_wss_stats_entry_t
{
  mtlk_wss_t            *allocator;
  uint32                 stat_id;
  mtlk_wss_cntr_handle_t hcntr;
} __MTLK_IFUNC mtlk_wss_stats_entry_t;

struct _mtlk_wss_t
{
  const mtlk_wss_t        *parent;
  mtlk_wss_stats_entry_t **entries;
  uint32                   nof_entries;
  uint32                   nof_contexts;
#ifdef MTCFG_ENABLE_OBJPOOL
  MTLK_DECLARE_OBJPOOL_CTX(objpool_ctx);
#endif
};

#define MTLK_WSS_INVALID_IDX ((uint32)-1)

static __INLINE mtlk_wss_stats_context_t *
__mtlk_wss_cntr_get_stats_context (mtlk_wss_t * wss, uint32 context_idx)
{
  mtlk_wss_stats_context_t *contexts_arr =
    (mtlk_wss_stats_context_t *)(&wss[1]);

  MTLK_ASSERT(context_idx < wss->nof_contexts);

  return &contexts_arr[context_idx];
}

static void
_mtlk_wss_rollback_contexts (mtlk_wss_t *wss)
{
  if (wss->entries != NULL && (wss->parent == NULL || wss->parent->entries != wss->entries)) {
    uint32 i;

    for (i = 0; i < wss->nof_entries; i++) {
      if (wss->entries[i] && wss->entries[i]->allocator == wss) {
        mtlk_osal_mem_free(wss->entries[i]);
      }
    }

    mtlk_osal_mem_free(wss->entries);
  }

  wss->entries     = NULL;
  wss->nof_entries = 0;

  if (wss->parent != NULL) {
    wss->entries     = wss->parent->entries;
    wss->nof_entries = wss->parent->nof_entries;
  }
}

static uint32
_mtlk_wss_find_entry (const mtlk_wss_t *wss, uint32 stat_id)
{
  uint32 res = MTLK_WSS_INVALID_IDX;
  uint32 i;

  MTLK_ASSERT(wss != NULL);

  for (i = 0; i < wss->nof_entries; i++) {
    if (wss->entries[i]->stat_id == stat_id) {
      res = i;
      break;
    }
  }

  return res;
}

static uint32
_mtlk_wss_get_merged_nof_entries (mtlk_wss_t *wss)
{
  uint32 res = wss->nof_contexts;

  if (wss->parent && wss->parent->nof_entries) {
    uint32 i;

    res += wss->parent->nof_entries;

    for (i = 0; i < wss->nof_contexts; i++) {
      mtlk_wss_stats_context_t *context = __mtlk_wss_cntr_get_stats_context(wss, i);
      if (_mtlk_wss_find_entry(wss->parent, context->stat_id) != MTLK_WSS_INVALID_IDX) {
        --res;
      }
    }

    MTLK_ASSERT(res >= wss->nof_contexts);
  }

  return res;
}

static int
_mtlk_wss_realloc_stats_entry_add_context (mtlk_wss_t *wss, uint32 entry_idx, mtlk_wss_stats_context_t *context)
{
  int                     res                  = MTLK_ERR_UNKNOWN;
  mtlk_wss_stats_entry_t *new_entry            = NULL;
  uint32                  nof_contexts_to_copy = 0;
  uint32                  i;

  MTLK_ASSERT(wss != NULL);
  MTLK_ASSERT(wss->entries != NULL);
  MTLK_ASSERT(wss->nof_entries != 0);
  MTLK_ASSERT(context != NULL);

  if (entry_idx == MTLK_WSS_INVALID_IDX) {
    /* Find an empty cell */
    for (i = 0; i < wss->nof_entries; i++) {
      if (wss->entries[i] == NULL) {
        break;
      }
    }

    MTLK_ASSERT(i < wss->nof_entries);

    entry_idx = i;
  }
  else {
    MTLK_ASSERT(entry_idx < wss->nof_entries);

    nof_contexts_to_copy = wss->entries[entry_idx]->hcntr.nof_contexts;
  }

  new_entry = 
    (mtlk_wss_stats_entry_t *)mtlk_osal_mem_alloc(sizeof(*new_entry) + 
                                                  sizeof(mtlk_wss_stats_context_t *) * nof_contexts_to_copy, /* no need for + 1 as mtlk_wss_stats_entry_t already contains one */
                                                  MTLK_MEM_TAG_WSS);
  if (!new_entry) {
    res = MTLK_ERR_NO_MEM;
    goto FINISH;
  }

  new_entry->allocator          = wss;
  new_entry->stat_id           = context->stat_id;
  new_entry->hcntr.nof_contexts = nof_contexts_to_copy + 1;

  for (i = 0; i < nof_contexts_to_copy; i++) {
    new_entry->hcntr.contexts[i] = wss->entries[entry_idx]->hcntr.contexts[i];
    MTLK_ASSERT(new_entry->hcntr.contexts[i] != NULL);
    MTLK_ASSERT(new_entry->hcntr.contexts[i]->stat_id == context->stat_id);
  }

  new_entry->hcntr.contexts[i] = context;

  wss->entries[entry_idx] = new_entry;
  
  res = MTLK_ERR_OK;

FINISH:
  return res;
}

static int
_mtlk_wss_adopt_contexts (mtlk_wss_t *wss)
{
  int                      res             = MTLK_ERR_UNKNOWN;
  uint32                   new_nof_entries = _mtlk_wss_get_merged_nof_entries(wss);
  mtlk_wss_stats_entry_t **new_entries     = NULL;
  uint32                   i;

  new_entries = (mtlk_wss_stats_entry_t **)mtlk_osal_mem_alloc(new_nof_entries * sizeof(new_entries[0]),
                                                               MTLK_MEM_TAG_WSS);
  if (new_entries == NULL) {
    res = MTLK_ERR_NO_MEM;
    goto FINISH;
  }

  memset(new_entries, 0, new_nof_entries * sizeof(new_entries[0]));

  if (wss->parent && wss->parent->nof_entries) {
    /* Copy all reader entries from the parent */
    memcpy(new_entries, wss->parent->entries, wss->parent->nof_entries * sizeof(new_entries[0]));
  }

  wss->entries     = new_entries;
  wss->nof_entries = new_nof_entries;

  for (i = 0; i < wss->nof_contexts; i++) {
    mtlk_wss_stats_context_t *context          = 
      __mtlk_wss_cntr_get_stats_context(wss, i);
    uint32                    reader_entry_idx = 
      (wss->parent != NULL)?_mtlk_wss_find_entry(wss->parent, context->stat_id):MTLK_WSS_INVALID_IDX;

    res = _mtlk_wss_realloc_stats_entry_add_context(wss, reader_entry_idx, context);
    if (res != MTLK_ERR_OK) {
      goto FINISH;
    }
  }

  res = MTLK_ERR_OK;

FINISH:
  if (res != MTLK_ERR_OK) {
    _mtlk_wss_rollback_contexts(wss);
  }

  return res;
}

static void
_mtlk_wss_cleanup (mtlk_wss_t *wss)
{
  MTLK_ASSERT(wss != NULL);

  _mtlk_wss_rollback_contexts(wss);
}

static int
_mtlk_wss_init (mtlk_wss_t *wss, const mtlk_wss_t *parent_wss, const mtlk_wss_stat_id_e *stat_ids, uint32 nof_stat_ids)
{
  int    res = MTLK_ERR_OK;
  uint32 i;

  MTLK_ASSERT(wss != NULL);

  if (parent_wss) {
    wss->parent      = parent_wss;

    wss->entries     = wss->parent->entries;
    wss->nof_entries = wss->parent->nof_entries;
  }

  if (nof_stat_ids) {
    wss->nof_contexts = nof_stat_ids;
    for (i = 0; i < wss->nof_contexts; i++) {
      mtlk_wss_stats_context_t *context = __mtlk_wss_cntr_get_stats_context(wss, i);

      MTLK_ASSERT_STAT_ID(stat_ids[i]);

      context->stat_id = stat_ids[i];
      mtlk_osal_atomic_set(&context->value, 0);
    }

    res = _mtlk_wss_adopt_contexts(wss);
  }

  return res;
}

mtlk_wss_t * __MTLK_IFUNC
#ifndef MTCFG_ENABLE_OBJPOOL
mtlk_wss_create (const mtlk_wss_t *parent_wss, const mtlk_wss_stat_id_e *stat_ids, uint32 nof_stat_ids)
#else
__mtlk_wss_create_objpool (const mtlk_wss_t *parent_wss, const mtlk_wss_stat_id_e *stat_ids, uint32 nof_stat_ids, mtlk_slid_t caller_slid)
#endif
{
  mtlk_wss_t *wss = NULL;

  MTLK_ASSERT(stat_ids != NULL || nof_stat_ids == 0);

  wss = (mtlk_wss_t *)mtlk_osal_mem_alloc(sizeof(*wss) + nof_stat_ids * sizeof(mtlk_wss_stats_context_t), MTLK_MEM_TAG_WSS);
  if (wss != NULL) {
    memset(wss, 0, sizeof(*wss) + nof_stat_ids * sizeof(mtlk_wss_stats_context_t));
    if (_mtlk_wss_init(wss, parent_wss, stat_ids, nof_stat_ids) != MTLK_ERR_OK) {
      mtlk_osal_mem_free(wss);
      wss = NULL;
    }
#ifdef MTCFG_ENABLE_OBJPOOL
    else {
        mtlk_objpool_add_object(&g_objpool, &wss->objpool_ctx, MTLK_WSS_OBJ, caller_slid);
    }
#endif
  }
  return wss;
}

void __MTLK_IFUNC
mtlk_wss_delete (mtlk_wss_t *wss)
{
  MTLK_ASSERT(wss != NULL);

  _mtlk_wss_cleanup(wss);
#ifdef MTCFG_ENABLE_OBJPOOL
  mtlk_objpool_remove_object(&g_objpool, &wss->objpool_ctx, MTLK_WSS_OBJ);
#endif
  mtlk_osal_mem_free(wss);
}

uint32 __MTLK_IFUNC
mtlk_wss_get_stat (mtlk_wss_t *wss, uint32 stat_id_idx)
{
  mtlk_wss_stats_context_t *context;

  MTLK_ASSERT(wss != NULL);

  context = __mtlk_wss_cntr_get_stats_context(wss, stat_id_idx);

  return mtlk_osal_atomic_get(&context->value);
}

void __MTLK_IFUNC
mtlk_wss_reset_stat (mtlk_wss_t *wss, uint32 stat_id_idx)
{
  mtlk_wss_stats_context_t *context;

  MTLK_ASSERT(wss != NULL);

  context = __mtlk_wss_cntr_get_stats_context(wss, stat_id_idx);

  mtlk_osal_atomic_set(&context->value, 0);
}

mtlk_wss_cntr_handle_t * __MTLK_IFUNC
mtlk_wss_cntr_open (mtlk_wss_t *wss, mtlk_wss_stat_id_e stat_id)
{
  mtlk_wss_cntr_handle_t *res = NULL;
  uint32                  entry_idx;

  MTLK_ASSERT(wss != NULL);
  MTLK_ASSERT_STAT_ID(stat_id);

  entry_idx = _mtlk_wss_find_entry(wss, stat_id);
  if (entry_idx != MTLK_WSS_INVALID_IDX) {
    res = &wss->entries[entry_idx]->hcntr;
  }

  return (entry_idx == MTLK_WSS_INVALID_IDX)?NULL:&wss->entries[entry_idx]->hcntr;
}

void __MTLK_IFUNC
mtlk_wss_cntr_close (mtlk_wss_t *wss, mtlk_wss_cntr_handle_t *hcntr)
{
  MTLK_ASSERT(wss != NULL);
  MTLK_ASSERT(hcntr != NULL);
}

int __MTLK_IFUNC
mtlk_wss_cntrs_open (mtlk_wss_t               *wss,
                     const mtlk_wss_stat_id_e *stat_ids, 
                     mtlk_wss_cntr_handle_t  **hctrls,
                     uint32                    nof_stats)
{
  int    res = MTLK_ERR_OK;
  uint32 i;

  MTLK_ASSERT(wss != NULL);
  MTLK_ASSERT(hctrls != NULL);
  MTLK_ASSERT(nof_stats != 0);

  memset(hctrls, 0, sizeof(hctrls[0]) * nof_stats);

  for (i = 0; i < nof_stats; i++) {
    hctrls[i] = mtlk_wss_cntr_open(wss, stat_ids[i]);
    if (hctrls[i] == NULL) {
      res = MTLK_ERR_NO_ENTRY;
      break;
    }
  }

  if (res != MTLK_ERR_OK) {
    for (i = 0; i < nof_stats && hctrls[i] != NULL; i++) {
      mtlk_wss_cntr_close(wss, hctrls[i]);
      hctrls[i] = NULL;
    }
  }

  return res;
}

void __MTLK_IFUNC
mtlk_wss_cntrs_close (mtlk_wss_t              *wss,
                      mtlk_wss_cntr_handle_t **hctrls,
                      uint32                   nof_stats)
{
  uint32 i;

  MTLK_ASSERT(wss != NULL);
  MTLK_ASSERT(hctrls != NULL);
  MTLK_ASSERT(nof_stats != 0);

  for (i = 0; i < nof_stats; i++) {
    MTLK_ASSERT(hctrls[i] != NULL);
    mtlk_wss_cntr_close(wss, hctrls[i]);
  }

  memset(hctrls, 0, sizeof(hctrls[0]) * nof_stats);
}
