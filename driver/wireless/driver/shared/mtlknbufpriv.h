#ifndef __MTLK_NBUF_PRIV_H__
#define __MTLK_NBUF_PRIV_H__

#include "stadb.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define LOG_LOCAL_GID   GID_NBUF
#define LOG_LOCAL_FID   0

#define MTLK_NBUFF_DIRECTED    0x0001   // this unit receiver address (802.11n ADDR1)
#define MTLK_NBUFF_UNICAST     0x0002   // unicast destination address (802.3 DA)
#define MTLK_NBUFF_MULTICAST   0x0004   // multicast destination address (802.3 DA)
#define MTLK_NBUFF_BROADCAST   0x0008   // broadcast destination address (802.3 DA)
#define MTLK_NBUFF_FORWARD     0x0010   // sk_buff should be forwarded
#define MTLK_NBUFF_CONSUME     0x0020   // sk_buff should be consumed by OS
#define MTLK_NBUFF_RMCAST      0x0040   // reliable multicast used
#define MTLK_NBUFF_URGENT      0x0100  // this skb describes urgent data

typedef struct _mtlk_nbuf_priv_extra_t {
  mtlk_atomic_t ref_cnt;
  uint16        ac;
  BOOL          front;
} __MTLK_IDATA mtlk_nbuf_priv_extra_t;

struct mtlk_nbuf_priv_internal
{
  uint8                   rsn_bits;
  uint8                   reserved[1];
  uint16                  flags;

  MTLK_DECLARE_OBJPOOL_CTX(objpool_ctx);

  sta_entry              *dst_sta;    // destination STA (NULL if unknown)
  sta_entry              *src_sta;    // source STA
  uint8                   rsc_buf[8]; // unparsed (TKIP/CCMP) storage for Replay Sequence Counter
  mtlk_vap_handle_t       vap_handle;
  mtlk_nbuf_priv_extra_t *extra;
#ifdef MTCFG_PER_PACKET_STATS
  mtlk_handle_t           stats;
#endif
#ifdef MTLK_DEBUG_CHARIOT_OOO
  uint16                  seq_num;
  uint8                   seq_qos;
#endif
};

typedef struct _mtlk_nbuf_priv_t
{
  uint8 private[sizeof(struct mtlk_nbuf_priv_internal)];
} __MTLK_IDATA mtlk_nbuf_priv_t;

static __INLINE uint16
mtlk_nbuf_priv_check_flags (const mtlk_nbuf_priv_t *priv,
                            uint16                  flags)
{
  const struct mtlk_nbuf_priv_internal *p = 
    (const struct mtlk_nbuf_priv_internal *)priv->private;
  return (p->flags & flags);
}

static __INLINE void
mtlk_nbuf_priv_set_flags (mtlk_nbuf_priv_t *priv,
                          uint16            flags)
{
  struct mtlk_nbuf_priv_internal *p = 
    (struct mtlk_nbuf_priv_internal *)priv->private;
  p->flags |= flags;
}

static __INLINE void
mtlk_nbuf_priv_reset_flags (mtlk_nbuf_priv_t *priv,
                            uint16            flags)
{
  struct mtlk_nbuf_priv_internal *p = 
    (struct mtlk_nbuf_priv_internal *)priv->private;

  p->flags &= ~flags;
}

/* TODO: Why RSC is 8 bytes? Add a protocol related constant. */
static __INLINE uint32
mtlk_nbuf_priv_set_rsc_buf (mtlk_nbuf_priv_t *priv,
                            uint8            *rsc_buf)
{
  struct mtlk_nbuf_priv_internal *p = 
    (struct mtlk_nbuf_priv_internal *)priv->private;

  memcpy(p->rsc_buf, rsc_buf, sizeof(p->rsc_buf));

  return sizeof(p->rsc_buf);
}

static __INLINE uint8
mtlk_nbuf_priv_get_rsc_buf_byte (const mtlk_nbuf_priv_t *priv,
                                 uint32                  idx)
{
  const struct mtlk_nbuf_priv_internal *p = 
    (const struct mtlk_nbuf_priv_internal *)priv->private;

  return p->rsc_buf[idx];
}

static __INLINE void
mtlk_nbuf_priv_set_rcn_bits (mtlk_nbuf_priv_t *priv,
                             uint8             rcn_bits)
{
  struct mtlk_nbuf_priv_internal *p = 
    (struct mtlk_nbuf_priv_internal *)priv->private;

  p->rsn_bits = rcn_bits;
}

static __INLINE uint8
mtlk_nbuf_priv_get_rcn_bits (const mtlk_nbuf_priv_t *priv)
{
  const struct mtlk_nbuf_priv_internal *p = 
    (const struct mtlk_nbuf_priv_internal *)priv->private;

  return p->rsn_bits;
}

static __INLINE void
mtlk_nbuf_priv_set_vap_handle (mtlk_nbuf_priv_t   *priv,
                               mtlk_vap_handle_t vap_handle)
{
  struct mtlk_nbuf_priv_internal *p = 
    (struct mtlk_nbuf_priv_internal *)priv->private;

  p->vap_handle = vap_handle;
}

static __INLINE mtlk_vap_handle_t
mtlk_nbuf_priv_get_vap_handle (const mtlk_nbuf_priv_t *priv)
{
  const struct mtlk_nbuf_priv_internal *p = 
    (const struct mtlk_nbuf_priv_internal *)priv->private;

  return p->vap_handle;
}

static __INLINE int
mtlk_nbuf_priv_extra_create_and_attach (mtlk_nbuf_priv_t *priv,
                                        uint16            ac,
                                        BOOL              front)
{
  struct mtlk_nbuf_priv_internal *p = 
    (struct mtlk_nbuf_priv_internal *)priv->private;

  p->extra = mtlk_osal_mem_alloc(sizeof(*p->extra),
                                 MTLK_MEM_TAG_SEND_QUEUE_CLONE);
  if (p->extra != NULL) {
    mtlk_osal_atomic_set(&p->extra->ref_cnt, 1);

    p->extra->ac = ac;
    p->extra->front = front;

    return MTLK_ERR_OK;
  }

  return MTLK_ERR_NO_MEM;
}

static __INLINE void
mtlk_nbuf_priv_extra_attach (mtlk_nbuf_priv_t *priv_from, 
                             mtlk_nbuf_priv_t *priv_to)
{
  struct mtlk_nbuf_priv_internal *p_from = 
    (struct mtlk_nbuf_priv_internal *)priv_from->private;
  struct mtlk_nbuf_priv_internal *p_to = 
    (struct mtlk_nbuf_priv_internal *)priv_to->private;

  p_to->extra = p_from->extra;
  mtlk_osal_atomic_inc(&p_from->extra->ref_cnt);
}

static __INLINE BOOL
mtlk_nbuf_priv_extra_detach (mtlk_nbuf_priv_t *priv)
{
  struct mtlk_nbuf_priv_internal *p;

  MTLK_ASSERT(NULL != priv);

  p = (struct mtlk_nbuf_priv_internal *) priv->private;

  MTLK_ASSERT(NULL != p);
  MTLK_ASSERT(NULL != p->extra);

  if (mtlk_osal_atomic_dec(&p->extra->ref_cnt) == 0) {
    mtlk_osal_mem_free(p->extra);
    p->extra = NULL;
    return TRUE;
  }

  return FALSE;
}

static __INLINE uint16
mtlk_nbuf_priv_extra_get_ac (const mtlk_nbuf_priv_t *priv)
{
  const struct mtlk_nbuf_priv_internal *p = 
    (const struct mtlk_nbuf_priv_internal *)priv->private;

  return p->extra->ac;
}

static __INLINE BOOL
mtlk_nbuf_priv_extra_get_front (const mtlk_nbuf_priv_t *priv)
{
  const struct mtlk_nbuf_priv_internal *p = 
    (const struct mtlk_nbuf_priv_internal *)priv->private;

  return p->extra->front;
}

/****************************************************************
 * NOTE: DF is responsible to initiate/cleanup NBUF's private 
 *       data prior to passing NBUF to CORE or OS.
 *       Thus, CORE is always working with initialized NBUF's 
 *       private data and MUST not call these functions.
 ****************************************************************/
static __INLINE void
mtlk_nbuf_priv_init (mtlk_nbuf_priv_t *priv)
{
  memset(priv, 0, sizeof(*priv));
}

static __INLINE void
mtlk_nbuf_priv_cleanup (mtlk_nbuf_priv_t *priv)
{
  const struct mtlk_nbuf_priv_internal *p = 
    (const struct mtlk_nbuf_priv_internal *)priv->private;

  if (p->src_sta) {
    mtlk_sta_decref(p->src_sta); /* De-reference by packet*/
  }
  if (p->dst_sta) {
    mtlk_sta_decref(p->dst_sta); /* De-reference by packet*/
  }

#ifndef DONT_PERFORM_PRIV_CLEANUP
  memset(priv, 0, sizeof(*priv));
#endif /* DONT_PERFORM_PRIV_CLEANUP */
}
/****************************************************************/

static __INLINE sta_entry *
mtlk_nbuf_priv_get_src_sta (mtlk_nbuf_priv_t *priv)
{
  struct mtlk_nbuf_priv_internal *p = 
    (struct mtlk_nbuf_priv_internal *)priv->private;

  return p->src_sta;
}

static __INLINE void
mtlk_nbuf_priv_set_src_sta (mtlk_nbuf_priv_t *priv,
                            sta_entry        *sta)
{
  struct mtlk_nbuf_priv_internal *p = 
    (struct mtlk_nbuf_priv_internal *)priv->private;

  MTLK_ASSERT(p->src_sta == NULL);

  p->src_sta = sta;
  if (p->src_sta) {
    mtlk_sta_incref(p->src_sta); /* Reference by packet*/
  }
}

static __INLINE sta_entry *
mtlk_nbuf_priv_get_dst_sta (mtlk_nbuf_priv_t *priv)
{
  struct mtlk_nbuf_priv_internal *p = 
    (struct mtlk_nbuf_priv_internal *)priv->private;

  return p->dst_sta;
}

static __INLINE void
mtlk_nbuf_priv_set_dst_sta (mtlk_nbuf_priv_t *priv,
                            sta_entry        *sta)
{
  struct mtlk_nbuf_priv_internal *p = 
    (struct mtlk_nbuf_priv_internal *)priv->private;

  MTLK_ASSERT(p->dst_sta == NULL);

  p->dst_sta = sta;
  if (p->dst_sta) {
    mtlk_sta_incref(p->dst_sta); /* Reference by packet*/
  }
}

static __INLINE void
__mtlk_nbuf_priv_clean (mtlk_nbuf_priv_t *clone_priv)
{
  struct mtlk_nbuf_priv_internal *clone_p = 
    (struct mtlk_nbuf_priv_internal *)clone_priv->private;

  clone_p->extra   = NULL;
  clone_p->dst_sta = NULL;
  clone_p->src_sta = NULL;
}

static __INLINE void
__mtlk_nbuf_priv_clone (mtlk_nbuf_priv_t *priv,
                        mtlk_nbuf_priv_t *clone_priv)
{
  struct mtlk_nbuf_priv_internal *p = 
    (struct mtlk_nbuf_priv_internal *)priv->private;
  struct mtlk_nbuf_priv_internal *clone_p = 
    (struct mtlk_nbuf_priv_internal *)clone_priv->private;

  clone_p->extra   = NULL;
  /* The VAP index should be passed through all cloned SKBs */
  clone_p->vap_handle = p->vap_handle;

  if(NULL != p->dst_sta)
  {
    clone_p->dst_sta = p->dst_sta;
    mtlk_sta_incref(clone_p->dst_sta);
  }

  if(NULL != p->src_sta)
  {
    clone_p->src_sta = p->src_sta;
    mtlk_sta_incref(clone_p->src_sta);
  }
}

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#ifdef MTCFG_PER_PACKET_STATS
#include "mtlknbufstats.h"
#endif

#endif /* __MTLK_NBUF_PRIV_H__ */
