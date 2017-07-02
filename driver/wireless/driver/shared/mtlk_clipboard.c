/*
 * $Id: mtlk_clipboard.c 10052 2010-12-01 16:43:51Z dmytrof $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Clipboard between Core and DF
 *
 * Originally written by Andrii Tseglytskyi
 *
 */

#include "mtlkinc.h"
#include "mtlk_clipboard.h"
#include "mtlklist.h"

#define LOG_LOCAL_GID   GID_CLIPBOARD
#define LOG_LOCAL_FID   1

/* "clipboard" between DF UI and Core implementation */
struct _mtlk_clpb_t {
  mtlk_dlist_t data_list;
  mtlk_dlist_entry_t *iterator;

  MTLK_DECLARE_INIT_STATUS;
};

struct _mtlk_clpb_entry_t {
  mtlk_dlist_entry_t entry;
  void               *data;
  uint32             size;
};

/* steps for init and cleanup */
MTLK_INIT_STEPS_LIST_BEGIN(clpb)
  MTLK_INIT_STEPS_LIST_ENTRY(clpb, LIST_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(clpb, REWIND)
MTLK_INIT_INNER_STEPS_BEGIN(clpb)
MTLK_INIT_STEPS_LIST_END(clpb);

static void _mtlk_clpb_cleanup(mtlk_clpb_t *clpb)
{
  MTLK_ASSERT(NULL != clpb);
  MTLK_CLEANUP_BEGIN(clpb, MTLK_OBJ_PTR(clpb))
    MTLK_CLEANUP_STEP(clpb, REWIND, MTLK_OBJ_PTR(clpb), 
                      MTLK_NOACTION, ());
    MTLK_CLEANUP_STEP(clpb, LIST_INIT, MTLK_OBJ_PTR(clpb), 
                      mtlk_dlist_cleanup, (&clpb->data_list));
  MTLK_CLEANUP_END(clpb, MTLK_OBJ_PTR(clpb));
}

static int _mtlk_clpb_init(mtlk_clpb_t *clpb)
{
  MTLK_ASSERT(NULL != clpb);
  MTLK_INIT_TRY(clpb, MTLK_OBJ_PTR(clpb))
    MTLK_INIT_STEP_VOID(clpb, LIST_INIT, MTLK_OBJ_PTR(clpb),
                       mtlk_dlist_init, (&clpb->data_list));
    MTLK_INIT_STEP_VOID(clpb, REWIND, MTLK_OBJ_PTR(clpb),
                       mtlk_clpb_enum_rewind, (clpb));
  MTLK_INIT_FINALLY(clpb, MTLK_OBJ_PTR(clpb))    
  MTLK_INIT_RETURN(clpb, MTLK_OBJ_PTR(clpb), _mtlk_clpb_cleanup, (clpb))
}

mtlk_clpb_t* __MTLK_IFUNC
mtlk_clpb_create(void)
{
  mtlk_clpb_t *clpb;

  if (NULL == (clpb = mtlk_osal_mem_alloc(sizeof(mtlk_clpb_t), MTLK_MEM_TAG_CLPB))) {
    ELOG_V("Can't allocate clipboard structure");
    goto err_no_mem;
  }
  memset(clpb, 0, sizeof(mtlk_clpb_t));

  if (MTLK_ERR_OK != _mtlk_clpb_init(clpb)) {
    ELOG_V("Can't init clipboard structure");
    goto err_init_clpb;
  }
  return clpb;

err_init_clpb:
  mtlk_osal_mem_free(clpb);
err_no_mem:
  return NULL;
}

void __MTLK_IFUNC
mtlk_clpb_delete(mtlk_clpb_t *clpb)
{
  MTLK_ASSERT(NULL != clpb);

  mtlk_clpb_purge(clpb);

  mtlk_dlist_cleanup(&clpb->data_list);

  mtlk_osal_mem_free(clpb);
}

void __MTLK_IFUNC
mtlk_clpb_purge(mtlk_clpb_t *clpb)
{
  mtlk_dlist_entry_t *lentry;
  struct _mtlk_clpb_entry_t *clpb_entry;

  MTLK_ASSERT(NULL != clpb);

  while(NULL != (lentry = mtlk_dlist_pop_front(&clpb->data_list))) {
    clpb_entry = MTLK_CONTAINER_OF(lentry, struct _mtlk_clpb_entry_t, entry);
    mtlk_osal_mem_free(clpb_entry->data);
    mtlk_osal_mem_free(clpb_entry);
  }
  mtlk_clpb_enum_rewind(clpb);
}

int __MTLK_IFUNC
mtlk_clpb_push(mtlk_clpb_t *clpb, const void *data, uint32 size)
{
  struct _mtlk_clpb_entry_t *clpb_entry;

  MTLK_ASSERT(NULL != clpb);
  MTLK_ASSERT(NULL != data);

  if (NULL == (clpb_entry = mtlk_osal_mem_alloc(sizeof(struct _mtlk_clpb_entry_t),
                                                MTLK_MEM_TAG_CLPB))) {
    ELOG_V("Can't allocate clipboard entry");
    goto err_no_mem_for_entry;
  }

  if (NULL == (clpb_entry->data = mtlk_osal_mem_alloc(size, MTLK_MEM_TAG_CLPB))) {
    ELOG_V("Can't allocate clipboard data");
    goto err_no_mem_for_data;
  }

  memcpy(clpb_entry->data, data, size);
  clpb_entry->size = size;

  mtlk_dlist_push_back(&clpb->data_list, &clpb_entry->entry);
  return MTLK_ERR_OK;

err_no_mem_for_data:
  mtlk_osal_mem_free(clpb_entry);
err_no_mem_for_entry:
  return MTLK_ERR_NO_MEM;
}

/* API for enumeration of "clipboard" */
void __MTLK_IFUNC
mtlk_clpb_enum_rewind(mtlk_clpb_t *clpb)
{
  MTLK_ASSERT(NULL != clpb);
  clpb->iterator = mtlk_dlist_head(&clpb->data_list);
}

void* __MTLK_IFUNC
mtlk_clpb_enum_get_next(mtlk_clpb_t *clpb, uint32* size)
{
  MTLK_ASSERT(NULL != clpb);

  clpb->iterator = mtlk_dlist_next(clpb->iterator);

  if (mtlk_dlist_head(&clpb->data_list) != clpb->iterator) {
    if(NULL != size)
    {
      *size = MTLK_CONTAINER_OF(clpb->iterator, 
                                struct _mtlk_clpb_entry_t,
                                entry)->size;
    }
    return MTLK_CONTAINER_OF(clpb->iterator, 
                             struct _mtlk_clpb_entry_t,
                             entry)->data;
  }
  
  return NULL;
}

uint32 __MTLK_IFUNC
mtlk_clpb_get_num_of_elements(mtlk_clpb_t *clpb)
{
  MTLK_ASSERT(NULL != clpb);
  return mtlk_dlist_size(&clpb->data_list);
}

