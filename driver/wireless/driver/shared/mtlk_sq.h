/*
* $Id: mtlk_sq.h 11579 2011-08-29 14:31:05Z nayshtut $
*
* Copyright (c) 2006-2008 Metalink Broadband (Israel)
*
*/

#ifndef _MTLK_SENDQUEUE_H_
#define _MTLK_SENDQUEUE_H_

/** 
*\file mtlk_sq.h
*\brief QoS traffic shaper
*/

#include "mtlklist.h"
#include "mtlkdfdefs.h"
#include "mtlkqos.h"
#include "mtlk_clipboard.h"
#include "mtlk_vap_manager.h"

#define LOG_LOCAL_GID   GID_SQ
#define LOG_LOCAL_FID   0

#define SQ_PUT_FRONT  TRUE
#define SQ_PUT_BACK   FALSE

/* queue limits */
typedef struct _mtlk_sq_limits
{
    uint16 global_queue_limit[NTS_PRIORITIES];
    uint16 peer_queue_limit[NTS_PRIORITIES];
} mtlk_sq_limits_t;

/* queue current sizes */
typedef struct _mtlk_sq_qsizes
{
    uint32  qsize[NTS_PRIORITIES];
} mtlk_sq_qsizes_t;

/* send queue statistics */
typedef struct _mtlk_sq_stats
{
    uint32  pkts_pushed[NTS_PRIORITIES];
    uint32  pkts_sent_to_um[NTS_PRIORITIES];
    uint32  pkts_limit_dropped[NTS_PRIORITIES];
} mtlk_sq_stats_t;

typedef struct _mtlk_peer_queue_entry_t {
    mtlk_nbuf_list_t buflist;
    mtlk_dlist_entry_t list_entry; // for mtlk_sq_t.peer_queue membership
} mtlk_peer_queue_entry_t;

typedef struct _mtlk_peer_queue_t {
    mtlk_dlist_t  list; // list of mtlk_peer_queue_entry_t
    mtlk_atomic_t size;       // total size = size(mtlk_peer_queue_entry_t[...].buflist)
} mtlk_peer_queue_t;

typedef struct _mtlk_sq_peer_ctx_t {
    mtlk_peer_queue_entry_t peer_queue_entry[NTS_PRIORITIES];
#define MTLK_SQ_TX_LIMIT_INFINITE -1
#define MTLK_SQ_TX_LIMIT_DEFAULT  64
    uint32          limit_cfg;
    mtlk_atomic_t   limit; // maximum allowed MSDU amount which STA can occupy
    mtlk_atomic_t   used;
    mtlk_sq_stats_t stats;
} mtlk_sq_peer_ctx_t;

/* Opaque structure for send queue. Should not be used directly */
typedef struct _mtlk_sq_t
{
    mtlk_peer_queue_t    peer_queue[NTS_PRIORITIES];              //Main TX queue

    mtlk_sq_peer_ctx_t   broadcast;                               //pseudo-STA for broadcast
                                                                  //and non-reliable multicast
    mtlk_osal_spinlock_t queue_lock;                              //Lock for this object (protects queues)

    volatile uint8       flush_in_progress;                       //Flag indicating whether flush
                                                                  //operation currently in progress
    mtlk_atomic_t        flush_count;                             //Counter of flush requests

    mtlk_df_t*           df;                                      //Pointer to the DF context

    mtlk_sq_stats_t      stats;                                   //send queue statistics
                                                                  //protected by queue_lock
    mtlk_sq_limits_t     limits;                                  //queue limits, 0 - unlimited
                                                                  //protected by queue_lock

    uint8                hw_tx_prohibited; /* Stop data transmit to HW - TX is prohibited */

    MTLK_DECLARE_INIT_STATUS;
    MTLK_DECLARE_INIT_LOOP(DLIST_INIT);                                                                  
    MTLK_DECLARE_START_STATUS;
} mtlk_sq_t;

/* Send Queue status */
typedef struct _mtlk_sq_status
{
  mtlk_sq_stats_t   stats;
  mtlk_sq_qsizes_t  qsizes;
  mtlk_sq_limits_t  limits;
} mtlk_sq_status_t;

/* Send Queue peer status */
typedef struct _mtlk_sq_peer_status
{
  mtlk_sq_stats_t   stats;
  uint32            current_size[NTS_PRIORITIES]; /* current skb-list size */
  uint32            limit;        /* maximum allowed MSDU amount which STA can occupy */
  uint32            used;         /* MSDU used */
  IEEE_ADDR         mac_addr;  /* MAC address of connected station */
} mtlk_sq_peer_status_t;

/*! 
\fn      int __MTLK_IFUNC mtlk_sq_init(mtlk_sq_t *pqueue)
\brief   Initializes send queue. 

\param   pqueue Pointer to user-allocated memory for send queue structure
*/

int __MTLK_IFUNC
mtlk_sq_init(mtlk_sq_t      *pqueue);

/*! 
\fn      void __MTLK_IFUNC mtlk_sq_cleanup(mtlk_sq_t *pqueue)
\brief   Frees resources used by send queue. 

\param   pqueue Pointer to send queue structure
*/

void __MTLK_IFUNC
mtlk_sq_cleanup(mtlk_sq_t *pqueue);

/*! 
\fn      int __MTLK_IFUNC mtlk_sq_start(mtlk_sq_t *pqueue, mtlk_df_t *df)
\brief   Starts send queue. 

\param   pqueue Pointer to user-allocated memory for send queue structure
\param   df     Pointer to DF object
*/

int __MTLK_IFUNC
mtlk_sq_start(mtlk_sq_t *pqueue,
              mtlk_df_t *df);

/*! 
\fn      void __MTLK_IFUNC mtlk_sq_stop(mtlk_sq_t *pqueue)
\brief   Stops send queue. 

\param   pqueue Pointer to send queue structure
*/

void __MTLK_IFUNC
mtlk_sq_stop(mtlk_sq_t *pqueue);

/*! 
\fn      int __MTLK_IFUNC mtlk_sq_enqueue_pkt(mtlk_sq_t *pqueue, mtlk_nbuf_t *ppacket, uint16 *paccess_category, BOOL front)
\brief   Places packet to send queue. 

\param   pqueue           [IN] Pointer to send queue structure
\param   ppacket          [IN] Pointer to the packet to be enqueued
\param   paccess_category [IN] AC of the queue where packet was placed. If packet is urgent - ignored.
\param   front            [IN] Insert the packet into the front of the queue

\return  MTLK_ERR_UNKNOWN In case of error
\return  MTLK_ERR_OK When succeeded
*/

/*!
\fn      void __MTLK_IFUNC mtlk_sq_peer_ctx_init(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer, uint32 resource)
\brief   Init SendQueue peer ctx

\param   pqueue SendQueue ctx
\param   ppeer SendQueue peer ctx
\param   resource maximum allowed MSDU amount which peer can occupy
*/

void __MTLK_IFUNC
mtlk_sq_peer_ctx_init(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer, uint32 resource);

/*!
\fn      void __MTLK_IFUNC mtlk_sq_peer_ctx_cleanup(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer)
\brief   Cleanup SendQueue peer ctx

\param   pqueue SendQueue ctx
\param   ppeer SendQueue peer ctx
*/

void __MTLK_IFUNC
mtlk_sq_peer_ctx_cleanup(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer);

/*!
\fn      void __MTLK_IFUNC mtlk_sq_peer_ctx_cancel_all_packets(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer)
\brief   Releases packets peer ctx

\param   pqueue Pointer to send queue structure
\param   ppeer SendQueue peer ctx
*/
void __MTLK_IFUNC
mtlk_sq_peer_ctx_cancel_all_packets(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer);

/*!
\fn      void __MTLK_IFUNC mtlk_sq_peer_ctx_cancel_all_packets_for_vap(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer, mtlk_vap_handle_t vap_handle)
\brief   Releases packets for specified VAP peer ctx

\param   pqueue Pointer to send queue structure
\param   ppeer SendQueue peer ctx
\param   vap_index VAP index
*/
void __MTLK_IFUNC
mtlk_sq_peer_ctx_cancel_all_packets_for_vap(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer, mtlk_vap_handle_t vap_handle);

int __MTLK_IFUNC
mtlk_sq_enqueue_pkt(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer, mtlk_nbuf_t *ppacket, uint16 access_category, BOOL front);

/*! 
\fn      void __MTLK_IFUNC mtlk_sq_flush(mtlk_sq_t *pqueue)
\brief   Processes send queue contents and sends as much packets as possible to the HW.

\param   pqueue Pointer to send queue structure
*/

void __MTLK_IFUNC
mtlk_sq_flush(mtlk_sq_t *pqueue);

/*! 
\fn      mtlk_sq_get_status
\brief   Copies send queue statistics and status information to the CLPB.

\param   pqueue Pointer to send queue structure
\param   clpb   Pointer to the CLPB
\param   get_peers_status   if TRUE - Retrieve SQ Peers status if FALSE - skip SQ Peers processing

\return  MTLK_ERR_OK when succeeded or error code otherwise
*/

int __MTLK_IFUNC
mtlk_sq_get_status(mtlk_sq_t *pqueue, mtlk_clpb_t *clpb, BOOL get_peers_status);

/*! 
\fn      static uint16 __INLINE mtlk_sq_get_limit(mtlk_sq_t *pqueue, uint8 ac)
\brief   Returns queue limit for given access category.

\param   pqueue  Pointer to send queue structure
\param   ac      Access category for which limit is being queried
*/

static uint16 __INLINE
mtlk_sq_get_limit(mtlk_sq_t *pqueue, uint8 ac)
{
    MTLK_ASSERT(NULL != pqueue);
    MTLK_ASSERT(ac < NTS_PRIORITIES);

    return pqueue->limits.global_queue_limit[ac];
}

/*! 
\fn      static uint16 __INLINE mtlk_sq_get_qsize(mtlk_sq_t *pqueue, uint8 ac)
\brief   Returns queue size for given access category.

\param   pqueue  Pointer to send queue structure
\param   ac      Access category for which queue size is being queried
*/

static uint16 __INLINE
mtlk_sq_get_qsize(mtlk_sq_t *pqueue, uint8 ac)
{
    ASSERT(pqueue != NULL);
    ASSERT(ac < NTS_PRIORITIES);

    return (uint16)mtlk_osal_atomic_get(&pqueue->peer_queue[ac].size);
}

/*!
=fn      void __MTLK_IFUNC mtlk_sq_on_tx_cfm(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer)
\brief   Request to free TX resouce for ppeer

\param   pqueue Pointer to send queue structure
\param   ppeer SendQueue peer ctx
*/
static void __INLINE
mtlk_sq_on_tx_cfm(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer)
{
    ASSERT( pqueue != NULL );

    if (ppeer == NULL) {
      ppeer = &pqueue->broadcast;
    }

    mtlk_osal_atomic_dec(&ppeer->used);
}

/*!
=fn      void __MTLK_IFUNC mtlk_sq_is_empty(mtlk_sq_peer_ctx_t *ppeer)
\brief   Returns true if the queue is empty

\param   ppeer SendQueue peer ctx
*/
static BOOL __INLINE
mtlk_sq_is_empty(const mtlk_sq_peer_ctx_t *ppeer)
{
    ASSERT (ppeer != NULL);
    return (mtlk_osal_atomic_get(&ppeer->used) == 0);
}

/*!
=fn      void __MTLK_IFUNC mtlk_sq_wait_all_packets_confirmed(mtlk_sq_peer_ctx_t *ppeer)
\brief   Wait until MAC confirmed all packets

\param   ppeer SendQueue peer ctx
*/
static void __INLINE
mtlk_sq_wait_all_packets_confirmed(mtlk_sq_peer_ctx_t *ppeer)
{
  while (TRUE != mtlk_sq_is_empty(ppeer)) {
    mtlk_osal_msleep(10);
  }
}

int mtlk_sq_enqueue_clone_begin (mtlk_sq_t *sq, uint16 ac, BOOL front, mtlk_nbuf_t *nbuf);
int mtlk_sq_enqueue_clone(mtlk_sq_t *sq, mtlk_nbuf_t *nbuf, sta_entry *clone_dst_sta);
void mtlk_sq_enqueue_clone_end(mtlk_sq_t *sq, mtlk_nbuf_t *nbuf);
void _mtlk_sq_release_packet(mtlk_sq_t *sq, uint16 ac, struct sk_buff *skb);
int mtlk_sq_enqueue(mtlk_sq_t *sq, uint16 ac, BOOL front, struct sk_buff *skb);
void __MTLK_IFUNC mtlk_sq_set_pm_enabled(mtlk_sq_peer_ctx_t *ppeer, BOOL pm_enabled);

/*!
  \brief   Enable TX data transmitting to HW

  \param   pqueue Pointer to send queue structure
*/
static void __INLINE
mtlk_sq_tx_enable(mtlk_sq_t *pqueue)
{
    ASSERT (NULL != pqueue);
    pqueue->hw_tx_prohibited = FALSE;
}

/*!
  \brief   Disable TX data transmitting to HW

  \param   pqueue Pointer to send queue structure
*/
static void __INLINE
mtlk_sq_tx_disable(mtlk_sq_t *pqueue)
{
    ASSERT (NULL != pqueue);
    pqueue->hw_tx_prohibited = TRUE;
}

/*!
  \brief   Get SQ TX state

  \param   pqueue Pointer to send queue structure
*/
static uint8 __INLINE
mtlk_sq_is_tx_enabled(const mtlk_sq_t *pqueue)
{
    ASSERT (NULL != pqueue);
    return (pqueue->hw_tx_prohibited);
}

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif //_MTLK_SENDQUEUE_H_
