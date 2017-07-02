/*
* $Id: rod.c 12816 2012-03-06 12:48:15Z hatinecs $
*
* Copyright (c) 2006-2007 Metalink Broadband (Israel)
*
* Written by: Dmitry Fleytman
*
*/
#include "mtlkinc.h"

#include "rod.h"
#include "mtlk_df.h"
#include "iperf_debug.h"

#define LOG_LOCAL_GID   GID_ROD
#define LOG_LOCAL_FID   1

#define SEQUENCE_NUMBER_LIMIT           (0x1000)
#define SEQ_DISTANCE(seq1, seq2)       (((seq2) - (seq1) + SEQUENCE_NUMBER_LIMIT) \
                                         % SEQUENCE_NUMBER_LIMIT);

static const uint32 _mtlk_rod_wss_id_map[] =
{
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_TOO_OLD,      /* MTLK_ROD_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD */
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_DUPLICATE,    /* MTLK_ROD_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE */
};

// Retire packet from the traffic stream in slot number
static int
retire_slot (reordering_queue *prod_queue, int slot)
{
  int res = -1;
  mtlk_nbuf_t *nbuf;

  ASSERT(prod_queue->used == 1);

  slot %= prod_queue->window_size;
  nbuf = prod_queue->nbuf[slot];
  ILOG4_DP("slot, %d, skb %p", slot, nbuf);

  if (nbuf != NULL) {
    mtlk_debug_process_iperf_payload_rx(prod_queue,slot);

    mtlk_rod_detect_replay_or_sendup(
        mtlk_vap_get_core(prod_queue->vap_handle), nbuf, prod_queue->rsc);

    prod_queue->nbuf[slot] = NULL;
    prod_queue->last_sent = mtlk_osal_timestamp();
    prod_queue->count--;
    ASSERT(prod_queue->count <= prod_queue->window_size);

    res = slot;
  } else
    prod_queue->stats.lost++;

  return res;
}

int __MTLK_IFUNC
mtlk_create_rod_queue (reordering_queue *prod_queue, mtlk_vap_handle_t vap_handle, mtlk_wss_t *parent_wss, int win_size, int ssn)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(ARRAY_SIZE(_mtlk_rod_wss_id_map)  == MTLK_ROD_CNT_LAST);
  MTLK_ASSERT(ARRAY_SIZE(prod_queue->wss_hcntrs) == MTLK_ROD_CNT_LAST);

  mtlk_osal_lock_acquire(&prod_queue->lock);

  if (prod_queue->used == 1)
  {
    ILOG1_V("TS already exist");
    mtlk_flush_rod_queue(prod_queue);
  }

  prod_queue->wss = mtlk_wss_create(parent_wss, _mtlk_rod_wss_id_map, ARRAY_SIZE(_mtlk_rod_wss_id_map));
  if (NULL == prod_queue->wss) {
    res = MTLK_ERR_NO_MEM;
    goto ERR_CREATE_WSS;
  }

  res = mtlk_wss_cntrs_open(prod_queue->wss, _mtlk_rod_wss_id_map, prod_queue->wss_hcntrs, MTLK_ROD_CNT_LAST);
  if (MTLK_ERR_OK != res) {
    goto ERR_OPEN_COUNTERS;
  }

  prod_queue->used = 1;
  prod_queue->vap_handle = vap_handle;
  prod_queue->count = 0;
  prod_queue->head = ssn;
  prod_queue->window_size = win_size;
  prod_queue->last_sent = mtlk_osal_timestamp();
  memset(&prod_queue->stats, 0, sizeof(reordering_stats));

  mtlk_osal_lock_release(&prod_queue->lock);
  return MTLK_ERR_OK;

ERR_OPEN_COUNTERS:
  mtlk_wss_delete(prod_queue->wss);
ERR_CREATE_WSS:
  mtlk_osal_lock_release(&prod_queue->lock);
  return res;
}

/* move reordering window on N positions retiring slots */
static void
rod_move_window(reordering_queue *prod_queue, int n)
{
  int i;

  ASSERT(prod_queue->used == 1);

  for (i = 0; i < n; i++) {
    retire_slot(prod_queue, prod_queue->head);
    prod_queue->head++; // move window
  }
  prod_queue->head %= SEQUENCE_NUMBER_LIMIT;
}

void __MTLK_IFUNC
mtlk_flush_rod_queue (reordering_queue *prod_queue)
{
    while(prod_queue->count)
    {
        rod_move_window(prod_queue, 1);
    }
}

void
__MTLK_IFUNC mtlk_handle_rod_queue_timer(reordering_queue *prod_queue)
{
  mtlk_osal_lock_acquire(&prod_queue->lock);

  if (prod_queue->used == 1) {
    if (prod_queue->count && mtlk_osal_time_after(mtlk_osal_timestamp(),
        prod_queue->last_sent + mtlk_osal_ms_to_timestamp(ROD_QUEUE_FLUSH_TIMEOUT_MS)))
      mtlk_flush_rod_queue(prod_queue);
  }

  mtlk_osal_lock_release(&prod_queue->lock);
}

void __MTLK_IFUNC
mtlk_clear_rod_queue (reordering_queue *prod_queue)
{
  mtlk_osal_lock_acquire(&prod_queue->lock);

  if (prod_queue->used != 1)
  {
    ILOG3_V("TS do not exist");
    goto end;
  }

  mtlk_flush_rod_queue(prod_queue);
  prod_queue->used = 0;

  mtlk_wss_cntrs_close(prod_queue->wss, prod_queue->wss_hcntrs, ARRAY_SIZE(prod_queue->wss_hcntrs));
  mtlk_wss_delete(prod_queue->wss);

end:
  mtlk_osal_lock_release(&prod_queue->lock);
}

// Retire packets in order from head until an empty slot is reached
static int
retire_rod_queue(reordering_queue *rq)
{
  int n=0;
  int loc = rq->head % rq->window_size;
  while (rq->nbuf[loc] != NULL) {
    retire_slot(rq, loc);
    loc++;
    if (loc >= (int)rq->window_size)
        loc = 0;
    rq->head++;
    n++;
  }
  rq->head %= SEQUENCE_NUMBER_LIMIT;
  return n;
}

static void
_mtlk_rod_inc_cnt (reordering_queue  *prod_queue,
                   rod_info_cnt_id_e cnt_id)
{
  MTLK_ASSERT(cnt_id >= 0 && cnt_id < MTLK_ROD_CNT_LAST);

  mtlk_wss_cntr_inc(prod_queue->wss_hcntrs[cnt_id]);
}

void __MTLK_IFUNC
mtlk_reorder_packet (reordering_queue *prod_queue, mtlk_core_t* nic,
                     int seq, mtlk_nbuf_t *nbuf)
{
  uint32 diff;
  int loc;
    
  mtlk_osal_lock_acquire(&prod_queue->lock);

  if (prod_queue->used == 0)
  {
    // legacy packets (i.e. not aggregated)
    mtlk_rod_detect_replay_or_sendup(nic,
                                     nbuf, prod_queue->rsc);
    goto end;
  }

  ASSERT(seq >= 0 && seq < SEQUENCE_NUMBER_LIMIT);

  diff = SEQ_DISTANCE(prod_queue->head, seq);
  ASSERT(diff < SEQUENCE_NUMBER_LIMIT);

  if (diff > SEQUENCE_NUMBER_LIMIT/2) {
    // This is a packet that has been already seen in the past(?)
    ILOG3_DDDD("too old packet, seq %d, head %u, diff %d > %d",
        seq, prod_queue->head, diff, SEQUENCE_NUMBER_LIMIT/2);
    ++prod_queue->stats.too_old;
    _mtlk_rod_inc_cnt(prod_queue, MTLK_ROD_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD);
    mtlk_df_nbuf_free(mtlk_vap_manager_get_master_df(mtlk_vap_get_manager(prod_queue->vap_handle)), nbuf);
    goto end;
  }

  if (diff >= prod_queue->window_size) {
    int delta;
    ILOG3_DDDD("packet outside of the window, seq %d, head %u, diff %d >= %u",
        seq, prod_queue->head, diff, prod_queue->window_size);
    ++prod_queue->stats.overflows;
    delta = diff - prod_queue->window_size + 1;
    rod_move_window(prod_queue, delta);
    diff = SEQ_DISTANCE(prod_queue->head, seq);
    ASSERT(diff == prod_queue->window_size - 1);
  }

  // Insert the skb into the reorder array
  // at the location of the sequence number.
  loc= seq % prod_queue->window_size;
  if (prod_queue->nbuf[loc] == NULL) {
    prod_queue->nbuf[loc]= nbuf;
    prod_queue->count++;
    ASSERT(prod_queue->count <= prod_queue->window_size);
    ILOG3_PDD("packet %p queued to slot %d, total queued %u",
        nbuf, loc, prod_queue->count);
    ++prod_queue->stats.queued;
  } else {
    mtlk_df_nbuf_free(mtlk_vap_manager_get_master_df(mtlk_vap_get_manager(prod_queue->vap_handle)), nbuf);
    ILOG3_PD("duplicate packet %p dropped, slot %d", nbuf, loc);
    ++prod_queue->stats.duplicate;
    _mtlk_rod_inc_cnt(prod_queue, MTLK_ROD_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE);
    goto end;
  }

  retire_rod_queue(prod_queue);

end:
  mtlk_osal_lock_release(&prod_queue->lock);
}

int __MTLK_IFUNC
mtlk_rod_process_bar(reordering_queue* prod_queue, uint16 ssn)
{
  int diff, res = 0;
    
  mtlk_osal_lock_acquire(&prod_queue->lock);

  if (prod_queue->used == 0) {
    ELOG_V("Received BAR for wrong TID (no BA agreement)");
    res = -1;
    goto end;
  }
  ASSERT(ssn < SEQUENCE_NUMBER_LIMIT);

  diff = SEQ_DISTANCE(prod_queue->head, ssn);
  if (diff > SEQUENCE_NUMBER_LIMIT/2) {
    /* sequence number is in the past */
    ILOG3_DDDD("Nothing to free - SSN is in the past: seq %d, head %u, diff %d > %d",
          ssn, prod_queue->head, diff, SEQUENCE_NUMBER_LIMIT/2);
    goto end;
  }

  // Move reordering window to new received SSN
  ILOG3_DD("BAR: SSN %d, head %u", ssn, prod_queue->head);
  rod_move_window(prod_queue, diff);

  retire_rod_queue(prod_queue);

end:
  mtlk_osal_lock_release(&prod_queue->lock);

  return res;
}
