/*
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 * Cyclic queue
 *
 * Written by: Andrey Fidrya
 *
 */

#ifndef __CQUEUE_H__
#define __CQUEUE_H__

#ifdef MTCFG_DEBUG
#define CQUEUE_DEBUG_DUMP(pqueue, binary) \
  cqueue_debug_dump(#pqueue, pqueue, binary)
#else
#define CQUEUE_DEBUG_DUMP(pqueue, binary)
#endif

typedef struct _cqueue_t
{
  int overflow_state;
  char *data;
  int max_size;
  int begin_idx;
  int end_idx;
  int is_empty;
} cqueue_t;

void cqueue_init (cqueue_t *pqueue);
int cqueue_reset (cqueue_t *pqueue, int max_size);
void cqueue_cleanup (cqueue_t *pqueue);
int cqueue_space_left (cqueue_t *pqueue);
int cqueue_size (cqueue_t *pqueue);
int cqueue_max_size (cqueue_t *pqueue);
int cqueue_empty (cqueue_t *pqueue);
int cqueue_full (cqueue_t *pqueue);
void cqueue_get (cqueue_t *pqueue, int offset, int len, unsigned char *data);
void cqueue_pop_front (cqueue_t *pqueue, int len);
void cqueue_push_back (cqueue_t *pqueue, unsigned char *data, int len);
int cqueue_read (cqueue_t *pqueue, int fd);
int cqueue_write (cqueue_t *pqueue, int fd);
int cqueue_reserve (cqueue_t *pqueue, int max_size);
#ifdef MTCFG_DEBUG
void cqueue_debug_dump (const char *qname, cqueue_t *pqueue, int binary);
#endif // MTCFG_DEBUG
    
#endif // !__CQUEUE_H__

