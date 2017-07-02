/*
 * $Id: mtlklist_osdep_defs.h 12609 2012-02-07 16:12:21Z nayshtut $
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *  
 * Linked lists implementation for Linux driver:
 *   1. Singly-linked lists
 *   2. Thread-safe singly-linked lists
 */

#if !defined (SAFE_PLACE_TO_INCLUDE_LIST_OSDEP_DEFS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_LIST_OSDEP_... */

#undef SAFE_PLACE_TO_INCLUDE_LIST_OSDEP_DEFS

#define LOG_LOCAL_GID   GID_MTLK_LIST
#define LOG_LOCAL_FID   0

#define _MTLK_LIST_GET_CONTAINING_RECORD(ptr, type, member) MTLK_CONTAINER_OF(ptr, type, member)

/*
 * 1. Singly-linked lists (stacks)
 */

#ifdef MTCFG_DEBUG
#define MTLK_SLIST_POISON  ((struct _mtlk_slist_entry_t *) 0)
#endif

static __INLINE void
mtlk_slist_init (mtlk_slist_t *slist)
{
  ASSERT(slist != NULL);
  slist->head.next = NULL;
  slist->depth = 0;
}

static __INLINE void
mtlk_slist_cleanup (mtlk_slist_t *slist)
{
  ASSERT(slist != NULL);
  ASSERT(slist->depth == 0);

  mtlk_slist_init(slist);
}

static __INLINE void
mtlk_slist_push (mtlk_slist_t       *slist,
                 mtlk_slist_entry_t *entry)
{
  ASSERT(slist != NULL);
  ASSERT(entry != NULL);
  entry->next = slist->head.next;
  slist->head.next = entry;
  ++slist->depth;
}

static __INLINE mtlk_slist_entry_t *
mtlk_slist_pop (mtlk_slist_t *slist)
{
  mtlk_slist_entry_t *entry;
  ASSERT(slist != NULL);
  entry = slist->head.next;
  if (entry != NULL) {
    slist->head.next = entry->next;
    --slist->depth;
#ifdef MTCFG_DEBUG
    entry->next = MTLK_SLIST_POISON;
#endif
  }
  return entry;
}

static __INLINE mtlk_slist_entry_t *
mtlk_slist_remove_next (mtlk_slist_t       *slist, 
                        mtlk_slist_entry_t *entry)
{
  mtlk_slist_entry_t *entry_next;
  ASSERT(slist != NULL);
  ASSERT(entry != NULL);
  entry_next = entry->next;
  if (entry_next != NULL) {
    entry->next = entry_next->next;
    --slist->depth;
#ifdef MTCFG_DEBUG
    entry_next->next = MTLK_SLIST_POISON;
#endif
  }
  return entry_next;
}

static __INLINE mtlk_slist_entry_t *
mtlk_slist_remove (mtlk_slist_t       *slist,
                   mtlk_slist_entry_t *entry)
{
  mtlk_slist_entry_t *cur;
  mtlk_slist_entry_t *prev = mtlk_slist_begin(slist);

  if (!entry)
    return NULL;
  /* check if the very first element is to be deleted */
  if (prev == entry) {
    mtlk_slist_pop(slist);
  } else {
    while ((cur = mtlk_slist_next(prev)) != entry)
      prev = cur;
    if (cur)
      mtlk_slist_remove_next(slist, prev);
  }
  return entry;
}

static __INLINE uint32
mtlk_slist_size (mtlk_slist_t *slist)
{
  ASSERT(slist != NULL);
  return slist->depth;
}

static __INLINE mtlk_slist_entry_t *
mtlk_slist_head (mtlk_slist_t *slist)
{
  ASSERT(slist != NULL);
  return &slist->head;
}

static __INLINE mtlk_slist_entry_t *
mtlk_slist_begin (mtlk_slist_t *slist)
{
  ASSERT(slist != NULL);
  return slist->head.next;
}

static __INLINE mtlk_slist_entry_t *
mtlk_slist_next (mtlk_slist_entry_t *entry)
{
  ASSERT(entry != NULL);
  return entry->next;
}

static __INLINE void
mtlk_slist_insert_next (mtlk_slist_t *slist, mtlk_slist_entry_t *place, mtlk_slist_entry_t *entry)
{
  ASSERT(slist != NULL);
  ASSERT(place != NULL);
  ASSERT(entry != NULL);
  entry->next = place->next;
  place->next = entry;
  slist->depth++;
}

/*
 * 2. Thread-safe singly-linked lists (stacks)
 */

static __INLINE void
mtlk_lslist_init (mtlk_lslist_t *lslist)
{
  ASSERT(lslist != NULL);
  mtlk_slist_init(&lslist->list);
  mtlk_osal_lock_init(&lslist->lock);
}

static __INLINE void
mtlk_lslist_cleanup (mtlk_lslist_t *lslist)
{
  ASSERT(lslist != NULL);
  mtlk_slist_cleanup(&lslist->list);
  mtlk_osal_lock_cleanup(&lslist->lock);
}

static __INLINE void
mtlk_lslist_push (mtlk_lslist_t       *lslist,
                  mtlk_lslist_entry_t *entry)
{
  ASSERT(lslist != NULL);
  mtlk_osal_lock_acquire(&lslist->lock);
  mtlk_slist_push(&lslist->list, entry);
  mtlk_osal_lock_release(&lslist->lock);
}

static __INLINE mtlk_lslist_entry_t*
mtlk_lslist_pop (mtlk_lslist_t *lslist)
{
  mtlk_lslist_entry_t *entry;
  ASSERT(lslist != NULL);
  mtlk_osal_lock_acquire(&lslist->lock);
  entry = mtlk_slist_pop(&lslist->list);
  mtlk_osal_lock_release(&lslist->lock);
  return entry;
}

static __INLINE uint32
mtlk_lslist_size (mtlk_lslist_t *lslist)
{
  return mtlk_slist_size(&lslist->list);
}


/*
 * 3. Doubly-linked lists (looped queues)
 */
#ifdef MTCFG_DEBUG
#define MTLK_DLIST_POISON1  ((struct _mtlk_dlist_entry_t *) 0x00100100)
#define MTLK_DLIST_POISON2  ((struct _mtlk_dlist_entry_t *) 0x00200200)
#endif

static __INLINE void 
__mtlk_dlist_add (mtlk_dlist_t       *dlist,
                  mtlk_dlist_entry_t *entry,
                  mtlk_dlist_entry_t *prev,
                  mtlk_dlist_entry_t *next)
{
  entry->next = next;
  entry->prev = prev;
  next->prev  = entry;
  prev->next  = entry;
  ++dlist->depth;
}

static __INLINE void
mtlk_dlist_init(mtlk_dlist_t *dlist)
{
  ASSERT(dlist != NULL);
  dlist->head.next = &dlist->head;
  dlist->head.prev = &dlist->head;
  dlist->depth = 0;
}

static __INLINE void
mtlk_dlist_cleanup (mtlk_dlist_t *dlist)
{
  ASSERT(dlist != NULL);
  ASSERT(dlist->depth == 0);

  mtlk_dlist_init(dlist);
}

static __INLINE void
mtlk_dlist_push_front (mtlk_dlist_t       *dlist,
                       mtlk_dlist_entry_t *entry)
{
  ASSERT(dlist != NULL);
  ASSERT(entry != NULL);

  __mtlk_dlist_add(dlist,
                   entry, 
                   &dlist->head, 
                   dlist->head.next);
}

static __INLINE mtlk_dlist_entry_t *
mtlk_dlist_pop_front (mtlk_dlist_t *dlist)
{
  mtlk_dlist_entry_t *entry;

  ASSERT(dlist != NULL);

  if (mtlk_dlist_is_empty(dlist))
    return NULL;

  entry = dlist->head.next;

  mtlk_dlist_remove(dlist, entry);

  return entry;
}

static __INLINE void
mtlk_dlist_push_back (mtlk_dlist_t       *dlist,
                      mtlk_dlist_entry_t *entry)
{
  ASSERT(dlist != NULL);
  ASSERT(entry != NULL);

  __mtlk_dlist_add(dlist, 
                   entry, 
                   dlist->head.prev, 
                   &dlist->head);
}

static __INLINE mtlk_dlist_entry_t *
mtlk_dlist_pop_back (mtlk_dlist_t *dlist)
{
  mtlk_dlist_entry_t *entry;

  ASSERT(dlist != NULL);

  if (mtlk_dlist_is_empty(dlist))
    return NULL;

  entry = dlist->head.prev;

  mtlk_dlist_remove(dlist, entry);

  return entry;
}

static __INLINE void
mtlk_dlist_insert_before (mtlk_dlist_t       *dlist,
                          mtlk_dlist_entry_t *cur_entry,
                          mtlk_dlist_entry_t *entry)
{
  ASSERT(dlist != NULL);
  ASSERT(entry != NULL);
  ASSERT(cur_entry != NULL);

  __mtlk_dlist_add(dlist, 
                   entry, 
                   cur_entry->prev, 
                   cur_entry);
}

static __INLINE mtlk_dlist_entry_t *
mtlk_dlist_remove (mtlk_dlist_t       *dlist, 
                   mtlk_dlist_entry_t *entry)
{
  mtlk_ldlist_entry_t *entry_next;

  ASSERT(dlist != NULL);
  ASSERT(entry != NULL);

  if (mtlk_dlist_is_empty(dlist))
    return NULL;
  
  entry_next = entry->next;

  entry->prev->next = entry->next;
  entry->next->prev = entry->prev;
  --dlist->depth;

#ifdef MTCFG_DEBUG
  entry->prev = MTLK_DLIST_POISON1;
  entry->next = MTLK_DLIST_POISON2;
#endif

  return entry_next;
}

static __INLINE mtlk_dlist_entry_t *
mtlk_dlist_head (mtlk_dlist_t *dlist)
{
  ASSERT(dlist != NULL);
  return &dlist->head;
}

static __INLINE int8
mtlk_dlist_is_empty (mtlk_dlist_t *dlist)
{
  ASSERT(dlist != NULL);

  return (dlist->depth == 0);
}

static __INLINE uint32
mtlk_dlist_size (mtlk_dlist_t *dlist)
{
  ASSERT(dlist != NULL);
  return dlist->depth;
}

static __INLINE mtlk_dlist_entry_t *
mtlk_dlist_next (mtlk_dlist_entry_t *entry)
{
  ASSERT(entry != NULL);
  return entry->next;
}

static __INLINE mtlk_dlist_entry_t *
mtlk_dlist_prev (mtlk_dlist_entry_t *entry)
{
  ASSERT(entry != NULL);
  return entry->prev;
}

static __INLINE void
mtlk_dlist_shift (mtlk_dlist_t *dlist)
{
  ASSERT(dlist != NULL);

  if (dlist->depth > 1) {
    mtlk_dlist_entry_t *entry = mtlk_dlist_pop_front(dlist);
    mtlk_dlist_push_back(dlist, entry);
  }
}

/*
 * 4. Interlocked doubly-linked lists (looped queues)
 */

static __INLINE void
mtlk_ldlist_init (mtlk_ldlist_t *ldlist)
{
  mtlk_dlist_init(&ldlist->list);
  mtlk_osal_lock_init(&ldlist->lock);
}

static __INLINE void
mtlk_ldlist_cleanup (mtlk_ldlist_t *ldlist)
{
  mtlk_dlist_cleanup(&ldlist->list);
  mtlk_osal_lock_cleanup(&ldlist->lock);
}

static __INLINE void
mtlk_ldlist_push_front (mtlk_ldlist_t       *ldlist,
                        mtlk_ldlist_entry_t *entry)
{
  ASSERT(ldlist != NULL);
  mtlk_osal_lock_acquire(&ldlist->lock);
  mtlk_dlist_push_front(&ldlist->list, entry);
  mtlk_osal_lock_release(&ldlist->lock);
}

static __INLINE mtlk_ldlist_entry_t *
mtlk_ldlist_pop_front (mtlk_ldlist_t *ldlist)
{
  mtlk_ldlist_entry_t *entry;

  ASSERT(ldlist != NULL);

  mtlk_osal_lock_acquire(&ldlist->lock);
  entry = mtlk_dlist_pop_front(&ldlist->list);
  mtlk_osal_lock_release(&ldlist->lock);

  return entry;
}

static __INLINE void
mtlk_ldlist_push_back (mtlk_ldlist_t       *ldlist,
                       mtlk_ldlist_entry_t *entry)
{
  ASSERT(ldlist != NULL);

  mtlk_osal_lock_acquire(&ldlist->lock);
  mtlk_dlist_push_back(&ldlist->list, entry);
  mtlk_osal_lock_release(&ldlist->lock);
}

static __INLINE mtlk_ldlist_entry_t *
mtlk_ldlist_pop_back (mtlk_ldlist_t *ldlist)
{
  mtlk_ldlist_entry_t *entry;

  ASSERT(ldlist != NULL);

  mtlk_osal_lock_acquire(&ldlist->lock);
  entry = mtlk_dlist_pop_back(&ldlist->list);
  mtlk_osal_lock_release(&ldlist->lock);

  return entry;
}

static __INLINE int8
mtlk_ldlist_is_empty (mtlk_ldlist_t *ldlist)
{
  return mtlk_dlist_is_empty(&ldlist->list);
}

static __INLINE uint32
mtlk_ldlist_size (mtlk_ldlist_t *ldlist)
{
  return mtlk_dlist_size(&ldlist->list);
}

static __INLINE mtlk_ldlist_entry_t *
mtlk_ldlist_head (mtlk_ldlist_t *ldlist)
{
  mtlk_ldlist_entry_t *entry;

  ASSERT(ldlist != NULL);

  mtlk_osal_lock_acquire(&ldlist->lock);
  entry = mtlk_dlist_head(&ldlist->list);
  mtlk_osal_lock_release(&ldlist->lock);

  return entry;
}

static __INLINE mtlk_ldlist_entry_t *
mtlk_ldlist_next (mtlk_ldlist_t *ldlist, mtlk_ldlist_entry_t *lentry)
{
  mtlk_ldlist_entry_t *entry;

  ASSERT(lentry != NULL);

  mtlk_osal_lock_acquire(&ldlist->lock);
  entry = mtlk_dlist_next(lentry);
  mtlk_osal_lock_release(&ldlist->lock);

  return entry;
}

static __INLINE mtlk_ldlist_entry_t *
mtlk_ldlist_prev (mtlk_ldlist_t *ldlist, mtlk_ldlist_entry_t *lentry)
{
  mtlk_ldlist_entry_t *entry;

  ASSERT(lentry != NULL);

  mtlk_osal_lock_acquire(&ldlist->lock);
  entry = mtlk_dlist_prev(lentry);
  mtlk_osal_lock_release(&ldlist->lock);

  return entry;
}

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID
