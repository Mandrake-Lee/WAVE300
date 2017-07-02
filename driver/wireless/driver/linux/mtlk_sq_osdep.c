/*
 * $Id: mtlk_sq_osdep.c 12425 2012-01-09 17:31:06Z nayshtut $
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 */
#include "mtlkinc.h"

#include <linux/module.h>

#include "mtlk_df.h"
#include "mtlk_sq_osdep.h"
#include "mtlk_sq.h"
#include "mcast.h"
#include "stadb.h"
#include "sq.h"
#include "mtlk_coreui.h"

#define LOG_LOCAL_GID   GID_SQ
#define LOG_LOCAL_FID   3

/*************************************************************************************************
 * SQ implementation
 *************************************************************************************************/
static void __INLINE
_mtlk_sq_switch_if_urgent(mtlk_nbuf_t *ppacket, uint16 *ac, BOOL *front)
{
  // Process out-of-band packet
  if (mtlk_nbuf_priv_check_flags(mtlk_nbuf_priv(ppacket), MTLK_NBUFF_URGENT))
  {
    *ac = AC_HIGHEST;
    *front = TRUE;
  }
}

/* called from shared code send queue implementation */
int _mtlk_sq_send_to_hw(mtlk_df_t *df, struct sk_buff *skb, uint16 prio)
{
  int res;
  mtlk_core_t *core = mtlk_vap_get_core(mtlk_nbuf_priv_get_vap_handle(mtlk_nbuf_priv(skb)));

  CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_TX_FLUSH);

  res = mtlk_xmit(core, skb);
  CPU_STAT_END_TRACK(CPU_STAT_ID_TX_FLUSH);

  return res;
}

/* this function is called when the queue needs to be awaken */
void mtlk_sq_schedule_flush(struct nic *nic)
{
  mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_SQ_DPCS_SCHEDULED);
  tasklet_schedule(nic->sq_flush_tasklet);
}

/* tasklet to flush awaken queue */
static void mtlk_sq_flush_tasklet(unsigned long data)
{
  struct nic *nic = (struct nic *)data;

  mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_SQ_DPCS_ARRIVED);

  mtlk_sq_flush(nic->sq);
}

static const mtlk_ability_id_t _sq_abilities[] = {
  MTLK_CORE_REQ_SET_SQ_CFG,
  MTLK_CORE_REQ_GET_SQ_CFG,
  MTLK_CORE_REQ_GET_SQ_STATUS
};

/* called during initialization from core_init */
int sq_init (struct nic *nic)
{
  struct _mtlk_sq_t *sq;
  struct tasklet_struct *ft;
  int i;
  
  int res = MTLK_ERR_UNKNOWN;
  /* these limits are taken from tc script on dongle.
   * maybe it's better to make them settable from userspace.
   */
  struct _mtlk_sq_limits limits = {
    .global_queue_limit = {800, 50, 1100, 100},
    .peer_queue_limit =   {600, 38, 825 , 75 } /* 75% */
  };

  /* allocate send queue struct */
  sq = kmalloc_tag(sizeof(*sq), GFP_KERNEL, MTLK_MEM_TAG_SEND_QUEUE);
  if (!sq) {
    ELOG_V("unable to allocate memory for send queue.");
    res = MTLK_ERR_NO_MEM;
    goto out;
  }
  memset(sq, 0, sizeof(*sq));

  /* allocate flush tasklet struct */
  ft = kmalloc_tag(sizeof(*ft), GFP_KERNEL, MTLK_MEM_TAG_SEND_QUEUE);
  if (!ft) {
    ELOG_V("unable to allocate memory for send queue flush tasklet");
    res = MTLK_ERR_NO_MEM;
    goto err_tasklet;
  }
  tasklet_init(ft, mtlk_sq_flush_tasklet, (unsigned long)nic);

  /* connect allocated memory to "nic" structure */
  nic->sq = sq;
  nic->sq_flush_tasklet = ft;

  /* "create" send queue */
  res = mtlk_sq_init(nic->sq);
  if (MTLK_ERR_OK != res) {
    ELOG_V("SQ inititialization failed");
    goto err_init;
  }

  /* set the limits for queues */
  for(i = 0; i < NTS_PRIORITIES; i++) {
    sq->limits.global_queue_limit[i] = limits.global_queue_limit[i];
    sq->limits.peer_queue_limit[i] = limits.peer_queue_limit[i];
  }

  res = mtlk_abmgr_register_ability_set(mtlk_vap_get_abmgr(nic->vap_handle),
                                        _sq_abilities, ARRAY_SIZE(_sq_abilities));
  if (MTLK_ERR_OK != res) {
    sq_cleanup(nic);
    goto out;
  }

  mtlk_abmgr_enable_ability_set(mtlk_vap_get_abmgr(nic->vap_handle),
                                _sq_abilities, ARRAY_SIZE(_sq_abilities));

  return MTLK_ERR_OK;

err_init:
  /* synchronously disable tasklet */
  tasklet_disable(nic->sq_flush_tasklet);
  kfree_tag(ft);
  nic->sq_flush_tasklet = NULL;

err_tasklet:
  kfree_tag(sq);
  nic->sq = NULL;

out:
  return res;
}

/* called when exiting from core_delete */
void sq_cleanup (struct nic *nic)
{
  MTLK_ASSERT(NULL != nic->sq);
  MTLK_ASSERT(NULL != nic->sq_flush_tasklet);

  mtlk_abmgr_disable_ability_set(mtlk_vap_get_abmgr(nic->vap_handle),
                                 _sq_abilities, ARRAY_SIZE(_sq_abilities));
  mtlk_abmgr_unregister_ability_set(mtlk_vap_get_abmgr(nic->vap_handle),
                                    _sq_abilities, ARRAY_SIZE(_sq_abilities));

  /* "release" the send queue */
  mtlk_sq_cleanup(nic->sq);

  /* synchronously disable tasklet */
  tasklet_disable(nic->sq_flush_tasklet);
  
  /* deallocate memory */
  kfree_tag(nic->sq);
  kfree_tag(nic->sq_flush_tasklet);
}


static int
mtlk_xmit_sq_enqueue (mtlk_sq_t *sq, struct sk_buff *skb, struct net_device *dev, uint16 access_category, BOOL front)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(skb);
  mtlk_sq_peer_ctx_t *sq_peer_ctx;
  sta_entry          *dst_sta = mtlk_nbuf_priv_get_dst_sta(nbuf_priv);

  if (dst_sta != NULL) {
    sq_peer_ctx = &dst_sta->sq_peer_ctx;
  } else {
    sq_peer_ctx = NULL;
  }

  /* try to enqueue packet into the "send queue" */
  return mtlk_sq_enqueue_pkt(sq, sq_peer_ctx, skb, access_category, front);
}

void mtlk_xmit_sq_flush (struct nic *nic)
{
  mtlk_sq_flush(nic->sq);
}


void sq_get_limits(struct nic *nic, int32 *limits, uint8 num_to_write)
{
  int i;

  /* copy limits to destination buffer */
  for (i = 0; i < num_to_write; i++)
    limits[i] = mtlk_sq_get_limit(nic->sq, i);
}

void sq_get_peer_limits(struct nic *nic, int32 *ratio, uint8 num_to_write)
{
  mtlk_sq_t *sq = nic->sq;
  int i;

  mtlk_osal_lock_acquire(&sq->queue_lock);

  for (i = 0; i < num_to_write; i++)
    ratio[i] = (100*sq->limits.peer_queue_limit[i])/sq->limits.global_queue_limit[i];

  mtlk_osal_lock_release(&sq->queue_lock);
}

int sq_set_limits(struct nic *nic, int32 *global_queue_limit, int num)
{
  mtlk_sq_t *sq = nic->sq;
  int i;

  /* accept only the exact number */
  if (num != NTS_PRIORITIES)
    return MTLK_ERR_PARAMS;

  for (i = 0; i < NTS_PRIORITIES; i++) {
    if (global_queue_limit[i] <= 0) {
      return MTLK_ERR_PARAMS;
    }
  }

  mtlk_osal_lock_acquire(&sq->queue_lock);

  for (i = 0; i < NTS_PRIORITIES; i++) {
    int ratio = (100*sq->limits.peer_queue_limit[i])/sq->limits.global_queue_limit[i];
    sq->limits.global_queue_limit[i] = global_queue_limit[i];
    sq->limits.peer_queue_limit[i] = (ratio*global_queue_limit[i])/100;
  }

  mtlk_osal_lock_release(&sq->queue_lock);

  return MTLK_ERR_OK;
}

int sq_set_peer_limits(struct nic *nic, int32 *ratio, int num)
{
  mtlk_sq_t *sq = nic->sq;
  const int min_peer_queue_size_ratio = 5;
  const int max_peer_queue_size_ratio = 100;
  int i;

  /* accept only the exact number */
  if (num != NTS_PRIORITIES)
    return MTLK_ERR_PARAMS;

  for (i = 0; i < NTS_PRIORITIES; i++) {
    if (ratio[i] < min_peer_queue_size_ratio) {
      return MTLK_ERR_PARAMS;
    }
    if (ratio[i] > max_peer_queue_size_ratio) {
      return MTLK_ERR_PARAMS;
    }
  }

  mtlk_osal_lock_acquire(&sq->queue_lock);

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sq->limits.peer_queue_limit[i] = MAX(1, (ratio[i]*sq->limits.global_queue_limit[i])/100);
  }

  mtlk_osal_lock_release(&sq->queue_lock);

  return MTLK_ERR_OK;
}

int mtlk_sq_enqueue_clone_begin (mtlk_sq_t *sq, uint16 ac, BOOL front, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  int               res       = MTLK_ERR_UNKNOWN;

  _mtlk_sq_switch_if_urgent(nbuf, &ac, &front);

  res = mtlk_nbuf_priv_extra_create_and_attach(nbuf_priv,
                                               ac,
                                               front);
  if (res == MTLK_ERR_OK) {
    mtlk_osal_atomic_inc(&sq->peer_queue[ac].size);
  }

  return res;
}

int mtlk_sq_enqueue_clone(mtlk_sq_t *sq, mtlk_nbuf_t *nbuf, sta_entry *clone_dst_sta)
{
  int               res;
  mtlk_nbuf_priv_t *nbuf_priv;
  mtlk_nbuf_priv_t *clone_nbuf_priv;
  mtlk_nbuf_t      *clone_nbuf;

  MTLK_ASSERT(NULL != sq);

  clone_nbuf = mtlk_df_nbuf_clone_no_priv(sq->df, nbuf);

  if (!clone_nbuf) {
    mtlk_sta_on_packet_dropped(clone_dst_sta, MTLK_TX_DISCARDED_DRV_NO_RESOURCES);
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  nbuf_priv       = mtlk_nbuf_priv(nbuf);
  clone_nbuf_priv = mtlk_nbuf_priv(clone_nbuf);

  mtlk_nbuf_priv_set_dst_sta(clone_nbuf_priv, clone_dst_sta);

  mtlk_nbuf_priv_extra_attach(nbuf_priv, clone_nbuf_priv);
  res = mtlk_xmit_sq_enqueue(sq,
                             clone_nbuf, 
                             clone_nbuf->dev, 
                             mtlk_nbuf_priv_extra_get_ac(nbuf_priv), 
                             mtlk_nbuf_priv_extra_get_front(nbuf_priv));
  if (unlikely(res != MTLK_ERR_OK)) {
    mtlk_sta_on_packet_dropped(clone_dst_sta, MTLK_TX_DISCARDED_SQ_OVERFLOW);
    mtlk_nbuf_priv_extra_detach(clone_nbuf_priv);
    mtlk_df_nbuf_free(sq->df, clone_nbuf);
  }

end:
  return res;
}

void mtlk_sq_enqueue_clone_end(mtlk_sq_t *sq, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  uint16               ac        =
    mtlk_nbuf_priv_extra_get_ac(nbuf_priv);
  BOOL                 last_ref  = 
    mtlk_nbuf_priv_extra_detach(nbuf_priv);

  if (last_ref) {
    mtlk_osal_atomic_dec(&sq->peer_queue[ac].size);
  }
}

/* Be aware! This function is for SQ internal usage only. 
 * You will never know from the outside the exact queue 
 * to which SQ has placed the packet (due to the "urgent" packets).
 */ 
void _mtlk_sq_release_packet(mtlk_sq_t *sq, uint16 ac, struct sk_buff *skb)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(skb);

  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_RMCAST)) {
    mtlk_sq_enqueue_clone_end(sq, skb);
  } else {
    mtlk_osal_atomic_dec(&sq->peer_queue[ac].size);
  }
}

int mtlk_sq_enqueue(mtlk_sq_t *sq, uint16 ac, BOOL front, struct sk_buff *skb)
{
  int res;

  _mtlk_sq_switch_if_urgent(skb, &ac, &front);

  mtlk_osal_atomic_inc(&sq->peer_queue[ac].size);

  res = mtlk_xmit_sq_enqueue(sq, skb, skb->dev, ac, front);
  if (unlikely(res != MTLK_ERR_OK)) {
    mtlk_osal_atomic_dec(&sq->peer_queue[ac].size);
  }

  return res;
}

