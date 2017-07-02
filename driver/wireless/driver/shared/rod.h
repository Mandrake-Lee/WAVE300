/*
 * $Id: rod.h 11832 2011-10-26 13:39:48Z fleytman $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Dmitry Fleytman
 *
 */

#ifndef _MTLK_ROD_H_
#define _MTLK_ROD_H_

#include "rod_osdep.h"
#include "mtlk_osal.h"
#include "mtlkdfdefs.h"
#include "mtlk_vap_manager.h"
#include "mtlk_wss.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define LOG_LOCAL_GID   GID_ROD
#define LOG_LOCAL_FID   0

// this is a maximum reordering window
#define MAX_REORD_WINDOW 64
#define MIN_REORD_WINDOW 16

#define ROD_QUEUE_FLUSH_TIMEOUT_MS (1000)

typedef enum
{
  MTLK_ROD_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD,
  MTLK_ROD_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE,
  MTLK_ROD_CNT_LAST
} rod_info_cnt_id_e;

typedef struct _reordering_stats
{
    uint32 too_old;
    uint32 duplicate;
    uint32 queued;
    uint32 overflows;
    uint32 lost;
} __MTLK_IDATA reordering_stats;

typedef struct _reordering_queue
{
    mtlk_vap_handle_t         vap_handle;
    uint32                    used;         /* Queue is active */
    uint32                    count;        /* count of allocated slots */
    uint32                    head;         /* Sequence number of head */
    uint32                    window_size;  /* <= REORD_WINDOW */
    mtlk_osal_timestamp_t     last_sent;    /* Timestamp of the last send operation */
    mtlk_nbuf_t              *nbuf[MAX_REORD_WINDOW];
    mtlk_osal_spinlock_t      lock;
    mtlk_osal_msec_t          last_rx_time; /* Last receive time in milliseconds */
    reordering_stats          stats;
    uint8                     rsc[6];       /* Replay Sequence Counter */
    mtlk_wss_t                *wss;
    mtlk_wss_cntr_handle_t    *wss_hcntrs[MTLK_ROD_CNT_LAST];
} __MTLK_IDATA reordering_queue;

int __MTLK_IFUNC
mtlk_create_rod_queue (reordering_queue *prod_queue, mtlk_vap_handle_t  vap_handle,  mtlk_wss_t *parent_wss, int win_size, int ssn);

void __MTLK_IFUNC
mtlk_flush_rod_queue (reordering_queue *prod_queue);

void __MTLK_IFUNC
mtlk_clear_rod_queue (reordering_queue *prod_queue);

static __INLINE int
mtlk_init_rod_queue(reordering_queue *prod_queue)
{
    return mtlk_osal_lock_init(&prod_queue->lock);
}

static __INLINE void
mtlk_release_rod_queue (reordering_queue *prod_queue)
{
    mtlk_osal_lock_cleanup(&prod_queue->lock);
}

void __MTLK_IFUNC
mtlk_reorder_packet (reordering_queue *prod_queue, mtlk_core_t* nic,
                     int seq, mtlk_nbuf_t *nbuf);

int __MTLK_IFUNC
mtlk_rod_process_bar(reordering_queue* prod_queue, uint16 ssn);

void
__MTLK_IFUNC mtlk_handle_rod_queue_timer(reordering_queue *prod_queue);

static __INLINE const reordering_stats* 
mtlk_get_rod_stats(const reordering_queue* prod_queue)
{
    //Is it safe to touch reordering queue without lock?
    return (prod_queue->used == 0) ? NULL : &prod_queue->stats;
}

static __INLINE int
mtlk_is_used_rod_queue(const reordering_queue* prod_queue)
{
    return prod_queue->used;
}

static __INLINE mtlk_osal_msec_t
mtlk_rod_queue_get_last_rx_time(reordering_queue* prod_queue)
{
    return prod_queue->last_rx_time;
}

static __INLINE void
mtlk_rod_queue_set_last_rx_time(reordering_queue* prod_queue, mtlk_osal_msec_t msec)
{
    prod_queue->last_rx_time = msec;
}

static __INLINE void
mtlk_rod_queue_clear_reply_counter(reordering_queue* prod_queue)
{
    memset(prod_queue->rsc, 0, sizeof(prod_queue->rsc));
};

//OS-dependent function
//Should be implemented in rod_osdep.c
int __MTLK_IFUNC
mtlk_rod_detect_replay_or_sendup(mtlk_core_t* core, mtlk_nbuf_t *nbuf, uint8 *rsn);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif /* !_MTLK_ROD_H_ */
