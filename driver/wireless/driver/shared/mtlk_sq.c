/*
* $Id: mtlk_sq.c 12609 2012-02-07 16:12:21Z nayshtut $
*
* Copyright (c) 2006-2008 Metalink Broadband (Israel)
*
*/
#include "mtlkinc.h"

#include "mtlk_sq.h"
#include "mtlk_df.h"
#include "mtlkhal.h"
#include "mtlkqos.h"
#include "mtlk_sq_osdep.h"

#define LOG_LOCAL_GID   GID_SQ
#define LOG_LOCAL_FID   1

MTLK_INIT_STEPS_LIST_BEGIN(sq)
  MTLK_INIT_STEPS_LIST_ENTRY(sq, LOCK_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(sq, DLIST_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(sq, PEER_CTX_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(sq)
MTLK_INIT_STEPS_LIST_END(sq);

MTLK_START_STEPS_LIST_BEGIN(sq)
MTLK_START_INNER_STEPS_BEGIN(sq)
MTLK_START_STEPS_LIST_END(sq);

int __MTLK_IFUNC
mtlk_sq_init(mtlk_sq_t *pqueue)
{
  int i;

  MTLK_ASSERT( pqueue  != NULL );

  MTLK_INIT_TRY(sq, MTLK_OBJ_PTR(pqueue))
    MTLK_INIT_STEP(sq, LOCK_INIT, MTLK_OBJ_PTR(pqueue), 
                   mtlk_osal_lock_init, (&pqueue->queue_lock));
                   
    for(i = 0; i < NTS_PRIORITIES; i++)
    {
      mtlk_osal_atomic_set(&pqueue->peer_queue[i].size, 0);
      MTLK_INIT_STEP_VOID_LOOP(sq, DLIST_INIT, MTLK_OBJ_PTR(pqueue), 
                               mtlk_dlist_init, (&pqueue->peer_queue[i].list));
    }                 

    MTLK_INIT_STEP_VOID(sq, PEER_CTX_INIT, MTLK_OBJ_PTR(pqueue), 
                   mtlk_sq_peer_ctx_init, (pqueue, &pqueue->broadcast, MTLK_SQ_TX_LIMIT_DEFAULT));

    pqueue->flush_in_progress = 0;

    mtlk_osal_atomic_set(&pqueue->flush_count, 0);

    memset(&pqueue->stats, 0, sizeof(pqueue->stats));
    /* set unlimited queue lengths */
    memset(&pqueue->limits, 0, sizeof(pqueue->limits));
    
  MTLK_INIT_FINALLY(sq, MTLK_OBJ_PTR(pqueue))
  MTLK_INIT_RETURN(sq, MTLK_OBJ_PTR(pqueue), mtlk_sq_cleanup, (pqueue));    
}

void __MTLK_IFUNC
mtlk_sq_cleanup(mtlk_sq_t *pqueue)
{
  int i;

  MTLK_ASSERT( pqueue != NULL );

  MTLK_CLEANUP_BEGIN(sq, MTLK_OBJ_PTR(pqueue))
    MTLK_CLEANUP_STEP(sq, PEER_CTX_INIT, MTLK_OBJ_PTR(pqueue),
                      mtlk_sq_peer_ctx_cleanup, (pqueue, &pqueue->broadcast));
    for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(pqueue), DLIST_INIT) > 0; i++) {
      MTLK_CLEANUP_STEP_LOOP(sq, DLIST_INIT, MTLK_OBJ_PTR(pqueue),
                             mtlk_dlist_cleanup, (&pqueue->peer_queue[i].list));
    }

    MTLK_CLEANUP_STEP(sq, LOCK_INIT, MTLK_OBJ_PTR(pqueue),
                      mtlk_osal_lock_cleanup, (&pqueue->queue_lock));
  MTLK_CLEANUP_END(sq, MTLK_OBJ_PTR(pqueue));

}

int __MTLK_IFUNC
mtlk_sq_start (mtlk_sq_t *pqueue,
               mtlk_df_t *df)
{
  MTLK_ASSERT( pqueue != NULL );
  MTLK_ASSERT( df != NULL );
  MTLK_ASSERT( pqueue->df == NULL );

  pqueue->df = df;

  MTLK_START_TRY(sq, MTLK_OBJ_PTR(pqueue))
  MTLK_START_FINALLY(sq, MTLK_OBJ_PTR(pqueue))
  MTLK_START_RETURN(sq, MTLK_OBJ_PTR(pqueue), mtlk_sq_stop, (pqueue));
}

void __MTLK_IFUNC
mtlk_sq_stop (mtlk_sq_t *pqueue)
{
  MTLK_ASSERT( pqueue != NULL );
  MTLK_ASSERT( pqueue->df != NULL );

  MTLK_STOP_BEGIN(sq, MTLK_OBJ_PTR(pqueue))
  MTLK_STOP_END(sq, MTLK_OBJ_PTR(pqueue))

  pqueue->df = NULL;
}

void __MTLK_IFUNC
mtlk_sq_peer_ctx_init(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer, uint32 limit)
{
    int i;

    ASSERT( pqueue != NULL );
    ASSERT( ppeer != NULL );

    memset(ppeer, 0, sizeof(mtlk_sq_peer_ctx_t));
    mtlk_osal_atomic_set(&ppeer->limit, limit);
    mtlk_osal_atomic_set(&ppeer->used, 0);
    ppeer->limit_cfg = limit; 

    mtlk_osal_lock_acquire(&pqueue->queue_lock);
    for (i = 0; i < NTS_PRIORITIES; i++)
    {
        mtlk_df_nbuf_list_init(&ppeer->peer_queue_entry[i].buflist);
        mtlk_dlist_push_front(&pqueue->peer_queue[i].list, &ppeer->peer_queue_entry[i].list_entry);
    }
    mtlk_osal_lock_release(&pqueue->queue_lock);
}

void __MTLK_IFUNC
mtlk_sq_peer_ctx_cleanup(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer)
{
    mtlk_nbuf_list_entry_t *nbuflist_entry;
    mtlk_nbuf_t *ppacket;
    int i;

    ASSERT( pqueue != NULL );
    ASSERT( ppeer != NULL );


    mtlk_osal_lock_acquire(&pqueue->queue_lock);

    for (i = 0; i < NTS_PRIORITIES; i++)
    {
        mtlk_dlist_remove(&pqueue->peer_queue[i].list, &ppeer->peer_queue_entry[i].list_entry);

        while (!mtlk_df_nbuf_list_is_empty(&ppeer->peer_queue_entry[i].buflist))
        {
            nbuflist_entry = mtlk_df_nbuf_list_pop_front(&ppeer->peer_queue_entry[i].buflist);
            ppacket = mtlk_df_nbuf_get_by_list_entry(nbuflist_entry);
            _mtlk_sq_release_packet(pqueue, i, ppacket);
            mtlk_df_nbuf_free(pqueue->df, ppacket);
        }

        mtlk_df_nbuf_list_cleanup(&ppeer->peer_queue_entry[i].buflist);
    }

    mtlk_osal_lock_release(&pqueue->queue_lock);
}

void __MTLK_IFUNC
mtlk_sq_peer_ctx_cancel_all_packets(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer)
{
    mtlk_nbuf_list_entry_t *nbuflist_entry;
    mtlk_nbuf_t *ppacket;
    int i;

    MTLK_ASSERT(NULL != pqueue);
    MTLK_ASSERT(NULL != ppeer);

    mtlk_osal_lock_acquire(&pqueue->queue_lock);
    for (i = 0; i < NTS_PRIORITIES; i++) {
       while (!mtlk_df_nbuf_list_is_empty(&ppeer->peer_queue_entry[i].buflist)) {
         nbuflist_entry = mtlk_df_nbuf_list_pop_front(&ppeer->peer_queue_entry[i].buflist);
         ppacket = mtlk_df_nbuf_get_by_list_entry(nbuflist_entry);
         _mtlk_sq_release_packet(pqueue, i, ppacket);
         mtlk_df_nbuf_free(pqueue->df, ppacket);
       }
     }
     mtlk_osal_lock_release(&pqueue->queue_lock);
}

void __MTLK_IFUNC
mtlk_sq_peer_ctx_cancel_all_packets_for_vap(mtlk_sq_t *pqueue, mtlk_sq_peer_ctx_t *ppeer, mtlk_vap_handle_t vap_handle)
{
  mtlk_nbuf_list_entry_t *nbuflist_entry, *nbuflist_head;
  mtlk_nbuf_t *ppacket;
  int i;

  MTLK_ASSERT(NULL != pqueue);
  MTLK_ASSERT(NULL != ppeer);

  mtlk_osal_lock_acquire(&pqueue->queue_lock);
  for (i = 0; i < NTS_PRIORITIES; i++) {
    nbuflist_head = mtlk_df_nbuf_list_head(&ppeer->peer_queue_entry[i].buflist);
    nbuflist_entry = mtlk_df_nbuf_list_next(nbuflist_head);

    while (nbuflist_entry != nbuflist_head) {
      ppacket = mtlk_df_nbuf_get_by_list_entry(nbuflist_entry);

      /* Pick up next nbuflist_entry here, not at the end of the loop,
       * because in case we release packet, we are not able to retrieve next
       * entry from released memory */
      nbuflist_entry = mtlk_df_nbuf_list_next(nbuflist_entry);

      /* release packet, belonged to specified VAP */
      if(vap_handle == mtlk_nbuf_priv_get_vap_handle(mtlk_nbuf_priv(ppacket))) {
        mtlk_df_nbuf_list_remove_entry(&ppeer->peer_queue_entry[i].buflist, ppacket);
        _mtlk_sq_release_packet(pqueue, i, ppacket);
        mtlk_df_nbuf_free(pqueue->df, ppacket);
      }
    }
  }
  mtlk_osal_lock_release(&pqueue->queue_lock);
}

int __MTLK_IFUNC
mtlk_sq_enqueue_pkt(mtlk_sq_t          *pqueue,
                    mtlk_sq_peer_ctx_t *ppeer,
                    mtlk_nbuf_t      *ppacket,
                    uint16             access_category,
                    BOOL               front)
{
    int res = MTLK_ERR_OK;

    ASSERT( pqueue != NULL );
    ASSERT( ppacket != NULL );
    ASSERT( access_category < NTS_PRIORITIES );

    // Use broadcast pseudo-STA if ppeer isn't specified
    if (ppeer == NULL)
    {
        ppeer = &pqueue->broadcast;
    }

    mtlk_osal_lock_acquire(&pqueue->queue_lock);

    // Queue size could exceed configured limit---
    // in case if packet is enqueue on the front of queue
    if (!front && 
        /* total nof packets limit reached or */
        (mtlk_osal_atomic_get(&pqueue->peer_queue[access_category].size) >= pqueue->limits.global_queue_limit[access_category] ||
        /* per-peer nof packets limit reached */
         mtlk_df_nbuf_list_size(&ppeer->peer_queue_entry[access_category].buflist) >= pqueue->limits.peer_queue_limit[access_category])
        )
    {
        pqueue->stats.pkts_limit_dropped[access_category]++;
        ppeer->stats.pkts_limit_dropped[access_category]++;
        res = MTLK_ERR_NO_RESOURCES;
        goto out;
    }

    if (front)
        mtlk_df_nbuf_list_push_front(&ppeer->peer_queue_entry[access_category].buflist,
            mtlk_df_nbuf_get_list_entry(ppacket));
    else
        mtlk_df_nbuf_list_push_back(&ppeer->peer_queue_entry[access_category].buflist,
            mtlk_df_nbuf_get_list_entry(ppacket));

    pqueue->stats.pkts_pushed[access_category]++;
    ppeer->stats.pkts_pushed[access_category]++;

out:
    mtlk_osal_lock_release(&pqueue->queue_lock);
    return res;
}

/*! 
\fn      static int process_packets_by_ac(mtlk_sq_t *pqueue, uint16 access_category, mtlk_completion_ctx_t *pcompletion_ctx)
\brief   Internal function that sends as much packets of given AC as possible from send queue to HW.

\param   pqueue Pointer to send queue structure
\param   access_category Access category of packets
\param   pcompletion_ctx Completion context used to free sent packets

\return MTLK_ERR_OK if packet was sent successfully
\return MTLK_ERR_NO_RESOURCES if some packets were not sent due to lack of HW resources
*/

static int
process_packets_by_ac(mtlk_sq_t             *pqueue, 
                      uint16                 access_category)
{
    int    res = MTLK_ERR_OK;
    uint32 packets_sent;

    do {
      mtlk_dlist_entry_t *entry;
      mtlk_dlist_entry_t *head;

      packets_sent = 0;

      mtlk_dlist_foreach(&pqueue->peer_queue[access_category].list, entry, head)
      {
        mtlk_peer_queue_entry_t *ppeer_queue_entry =
            MTLK_CONTAINER_OF(entry, mtlk_peer_queue_entry_t, list_entry);
        mtlk_sq_peer_ctx_t *ppeer =
            MTLK_CONTAINER_OF(ppeer_queue_entry, mtlk_sq_peer_ctx_t, peer_queue_entry[access_category]);

        if (!mtlk_df_nbuf_list_is_empty(&ppeer_queue_entry->buflist))
        {
          mtlk_nbuf_t *ppacket;

          if (mtlk_osal_atomic_get(&ppeer->used) >= mtlk_osal_atomic_get(&ppeer->limit)) {
              break;
          }

          ppacket = mtlk_df_nbuf_get_by_list_entry(
              mtlk_df_nbuf_list_pop_front(&ppeer_queue_entry->buflist));

          /* check the flow control "stopped" flag */
          if(__UNLIKELY(pqueue->hw_tx_prohibited)) {
            res = MTLK_ERR_PROHIB;
          } else {
            res = _mtlk_sq_send_to_hw(pqueue->df, ppacket, mtlk_df_nbuf_get_priority(ppacket));
          }

          switch (res)
          {
          case MTLK_ERR_OK:
              _mtlk_sq_release_packet(pqueue, access_category, ppacket);
              mtlk_osal_atomic_inc(&ppeer->used);
              pqueue->stats.pkts_sent_to_um[access_category]++;
              ppeer->stats.pkts_sent_to_um[access_category]++;
              break;
          case MTLK_ERR_PROHIB:
              /* fall through */
          case MTLK_ERR_NO_RESOURCES:
              mtlk_df_nbuf_list_push_front(&ppeer_queue_entry->buflist, mtlk_df_nbuf_get_list_entry(ppacket));
              return MTLK_ERR_PROHIB;
          case MTLK_ERR_PKT_DROPPED:
              _mtlk_sq_release_packet(pqueue, access_category, ppacket);
              mtlk_df_nbuf_free(pqueue->df, ppacket);
              break;
          default:
              WLOG_V("Unknown failure upon HW TX request");
              _mtlk_sq_release_packet(pqueue, access_category, ppacket);
              mtlk_df_nbuf_free(pqueue->df, ppacket);
              return MTLK_ERR_UNKNOWN;
          }

          ++packets_sent;
        }
      }
    } while (packets_sent);

    mtlk_dlist_shift(&pqueue->peer_queue[access_category].list);

    return MTLK_ERR_OK;
}

// Following is a rationale and algorithm description of mtlk_sq_flush function. 
// So long as queue processing may be initiated by two different threads 
// in parallel - thread used to send data packets and thread used to process 
// MAC confirmations - all data structures of '''QoS Traffic Shaper''' must 
// be protected by synchronization primitives.
// 
// This leads to serialization of above threads when they call '''QoS Traffic Shaper''' 
// and performance degradation, basically because of concurrent invocations of queues 
// processing logic that takes majority of algorithm time. Moreover, 
// when MAC confirmation thread is blocked on some synchronization primitive adapter 
// is unable to return packets and this leads to further serialization of driver and MAC threads.
// 
// In order to avoid this scenario following locking strategy was implemented: 	 	 
// 
// 1. Queues processing logic operates with two additional variables: 	 	 
// 	* Flag to indicate that currently queues being flushed in concurrent thread (''InProgress flag''), 	 	 
// 	* Counter of queues flush requests (''Requests counter''); 	 	 
// 2. Before acquiring '''QoS Traffic Shaper''' lock, queues processing logic checks value of ''InProgress flag''.
// 3. If flag is raised, processing logic increments ''Requests counter'' and quits. 	 	 
// 4. If flag not raised, queues processing logic acquires lock, raises ''InProgress flag'', remembers value of 
//    ''Requests counter'' and starts processing of queues;
// 5. After queues processing finished, queues processing logic lowers ''InProgress flag'' to avoid 
//    ''signal loss'' effect, checks whether ''Requests counter'' value equals to remembered one. 
//    If not equals - code raises ''InProgress flag'' and repeats queues processing, else releases lock and quits;

#define PACKETS_IN_MAC_DURING_PM 3

void __MTLK_IFUNC
mtlk_sq_set_pm_enabled(mtlk_sq_peer_ctx_t *ppeer, BOOL enabled)
{
  uint32 val = enabled?
                 MIN(PACKETS_IN_MAC_DURING_PM, ppeer->limit_cfg) : 
                 ppeer->limit_cfg;

  mtlk_osal_atomic_set(&ppeer->limit, val);
}

void __MTLK_IFUNC
mtlk_sq_flush(mtlk_sq_t *pqueue)
{
    uint32 initial_flush_count;

    ASSERT( pqueue != NULL );

    if(pqueue->flush_in_progress)
    {
        mtlk_osal_atomic_inc(&pqueue->flush_count);
        return;
    }

    pqueue->flush_in_progress = 1;

    mtlk_osal_lock_acquire(&pqueue->queue_lock);

    do 
    {
        uint16 i;
        initial_flush_count = mtlk_osal_atomic_get(&pqueue->flush_count);
        pqueue->flush_in_progress = 1;

        //Send packets to MAC, higher priorities go first
        for (i = NTS_PRIORITIES; i > 0; i--)
        {
          int res;
          CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_TX_SQ_FLUSH);
          res = process_packets_by_ac(pqueue, mtlk_get_ac_by_number(i - 1));
          CPU_STAT_END_TRACK(CPU_STAT_ID_TX_SQ_FLUSH);
          if (MTLK_ERR_OK != res)
            break;
        }

        pqueue->flush_in_progress = 0;
    } 
    while(initial_flush_count != mtlk_osal_atomic_get(&pqueue->flush_count));

    mtlk_osal_lock_release(&pqueue->queue_lock);
}

int __MTLK_IFUNC
mtlk_sq_get_status(mtlk_sq_t *pqueue, mtlk_clpb_t *clpb, BOOL get_peers_status)
{
    int                   idx;
    mtlk_dlist_entry_t    *entry;
    mtlk_dlist_entry_t    *head;
    mtlk_sq_status_t      status;
    mtlk_sq_peer_status_t peer_status;
    int                   res = MTLK_ERR_UNKNOWN;

    ASSERT( pqueue != NULL );

    mtlk_osal_lock_acquire(&pqueue->queue_lock);

    /* Get SQ status */
    status.stats = pqueue->stats;
    status.limits = pqueue->limits;

    for(idx = 0; idx < NTS_PRIORITIES; idx++) {
      status.qsizes.qsize[idx] = mtlk_osal_atomic_get(&pqueue->peer_queue[idx].size);
    }

    res = mtlk_clpb_push(clpb, &status, sizeof(status));
    if (MTLK_ERR_OK != res) {
      goto err_push;
    }

    if (!get_peers_status) {
      /* Skip SQ peers */
      goto finish;
    }

    /* Get SQ peers status */
    mtlk_dlist_foreach(&pqueue->peer_queue[AC_BE].list, entry, head) {

       mtlk_peer_queue_entry_t *ppeer_queue_entry =
         MTLK_CONTAINER_OF(entry, mtlk_peer_queue_entry_t, list_entry);
       mtlk_sq_peer_ctx_t *ppeer =
         MTLK_CONTAINER_OF(ppeer_queue_entry, mtlk_sq_peer_ctx_t, peer_queue_entry[AC_BE]);

       peer_status.limit = mtlk_osal_atomic_get(&ppeer->limit);
       if (&pqueue->broadcast != ppeer)  {
         sta_entry *sta =
           MTLK_CONTAINER_OF(ppeer, sta_entry, sq_peer_ctx);
         peer_status.mac_addr = *mtlk_sta_get_addr(sta);
       }
       else
         memset (&peer_status.mac_addr, 0xFF, sizeof (IEEE_ADDR));
       peer_status.used = mtlk_osal_atomic_get(&ppeer->used);
       peer_status.stats = ppeer->stats;
       for(idx = 0; idx < NTS_PRIORITIES; idx++) {
         peer_status.current_size[idx] = mtlk_df_nbuf_list_size(&ppeer->peer_queue_entry[idx].buflist);
       }

       res = mtlk_clpb_push(clpb, &peer_status, sizeof(peer_status));
       if (MTLK_ERR_OK != res) {
         goto err_push;
       }
    }

    goto finish;

err_push:
   mtlk_clpb_purge(clpb);
finish:
   mtlk_osal_lock_release(&pqueue->queue_lock);
   return res;
}
