#ifndef SAFE_PLACE_TO_INCLUDE_DF_NBUF_IMPL
#error "You shouldn't include this file directly!"
#endif

#include "hw_mmb.h"

#define LOG_LOCAL_GID   GID_NBUF
#define LOG_LOCAL_FID   1

/**********************************************************************
 * DF Network Buffer API OS depended implementation
 * - and -
 * DF Network Buffers lists (doubly linked) OS depended implementation
 **********************************************************************
 *
 * Since current driver implementation doesn't set NETIF_F_SG and NETIF_F_FRAGLIST
 * feature flags, kernel network subsystem linearizes all buffers
 * before calling driver's transmit function, therefore standard
 * memory access API (like memcpy()) can be used with DF Network Buffer.
 *
 **********************************************************************/

static __INLINE mtlk_nbuf_priv_t *
mtlk_nbuf_priv(mtlk_nbuf_t *nbuf)
{
  return (mtlk_nbuf_priv_t *)nbuf->cb;
}

static __INLINE mtlk_nbuf_t *
_mtlk_df_nbuf_alloc_osdep (mtlk_df_t *df, uint32 size)
{
  mtlk_nbuf_t *nbuf = dev_alloc_skb(size);
  if (nbuf) {
    mtlk_nbuf_priv_init(mtlk_nbuf_priv(nbuf));
  }
  return nbuf;
}

static __INLINE void
_mtlk_df_nbuf_free_osdep (mtlk_df_t *df, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_priv_cleanup(mtlk_nbuf_priv(nbuf));
  dev_kfree_skb(nbuf);
}

static __INLINE mtlk_nbuf_t *
_mtlk_df_nbuf_clone_no_priv_osdep (mtlk_df_t *df, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_t *res = skb_clone(nbuf, GFP_ATOMIC);

  if (res) {
    __mtlk_nbuf_priv_clean(mtlk_nbuf_priv(res));
    /* The VAP index should be passed through all cloned SKBs */
    mtlk_nbuf_priv_set_vap_handle(
        mtlk_nbuf_priv(res),
        mtlk_nbuf_priv_get_vap_handle(mtlk_nbuf_priv(nbuf)));
  }

  return res;
}

static __INLINE mtlk_nbuf_t *
_mtlk_df_nbuf_clone_with_priv_osdep (mtlk_df_t *df, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_t *res = skb_clone(nbuf, GFP_ATOMIC);

  if (res) {
    __mtlk_nbuf_priv_clone(mtlk_nbuf_priv(nbuf), 
                           mtlk_nbuf_priv(res));
  }

  return res;
}

static __INLINE void *
mtlk_df_nbuf_get_virt_addr (mtlk_nbuf_t *nbuf)
{
  return nbuf->data;
}

typedef enum
{
  MTLK_DATA_FROM_DEVICE = PCI_DMA_FROMDEVICE,
  MTLK_DATA_TO_DEVICE = PCI_DMA_TODEVICE
} mtlk_data_direction_e;

static __INLINE uint32
mtlk_map_to_phys_addr (mtlk_df_t       *df,
                       void            *buffer,
                       uint32           size,
                       mtlk_data_direction_e direction)
{
  return dma_map_single(mtlk_bus_drv_get_device(
                             mtlk_vap_manager_get_bus_drv(mtlk_df_get_vap_manager(df))),
                        buffer, size, direction);
}

static __INLINE void
mtlk_unmap_phys_addr (mtlk_df_t       *df,
                      uint32           addr,
                      uint32           size,
                      mtlk_data_direction_e direction)
{
  dma_unmap_single(mtlk_bus_drv_get_device(
                        mtlk_vap_manager_get_bus_drv(mtlk_df_get_vap_manager(df))),
                   addr, size, direction);
}

#define mtlk_df_nbuf_map_to_phys_addr(df, nbuf, size, direction) \
  mtlk_map_to_phys_addr(df, nbuf->data, size, direction)

#define  mtlk_df_nbuf_unmap_phys_addr(df, nbuf, addr, size, direction) \
  mtlk_unmap_phys_addr(df, addr, size, direction)

static __INLINE void 
mtlk_df_nbuf_reserve (mtlk_nbuf_t *nbuf, uint32 len)
{
  skb_reserve(nbuf, len);
}

static __INLINE void *
mtlk_df_nbuf_put (mtlk_nbuf_t *nbuf, uint32 len)
{
  return skb_put(nbuf, len);
}

static __INLINE void
mtlk_df_nbuf_trim (mtlk_nbuf_t *nbuf, uint32 len)
{
  skb_trim(nbuf, len);
}

static __INLINE void *
mtlk_df_nbuf_pull (mtlk_nbuf_t *nbuf, uint32 len)
{
  return skb_pull(nbuf, len);
}

static __INLINE uint32
mtlk_df_nbuf_get_tail_room_size (mtlk_nbuf_t *nbuf)
{
  return skb_end_pointer(nbuf) - skb_tail_pointer(nbuf);
}

static __INLINE int
mtlk_df_nbuf_read (mtlk_nbuf_t *nbuf, 
                   uint32       offset, 
                   uint32       length, 
                   uint8       *destination)
{
  ASSERT(nbuf != NULL);

  if (unlikely(skb_copy_bits(nbuf, 
                             offset, 
                             destination, 
                             length)))
      return MTLK_ERR_UNKNOWN;

  return MTLK_ERR_OK;
}

/*
 * Actually, the right function here should be
 * skb_strore_bits(), but it's not supported in older kernels.
 */
static __INLINE int
mtlk_df_nbuf_write (mtlk_nbuf_t *nbuf, 
                    uint32       offset, 
                    uint32       length, 
                    uint8       *source)
{
  ASSERT(nbuf != NULL);

  if (unlikely((offset + length) > nbuf->len))
    return MTLK_ERR_UNKNOWN;

  memcpy(nbuf->data + offset, source, length);
  return MTLK_ERR_OK;
}

/* Again, skb is assumed to be linearized (i.e. skb->data_len == 0) */
static __INLINE uint32
mtlk_df_nbuf_get_data_length (mtlk_nbuf_t *nbuf)
{
  ASSERT(nbuf != NULL);
  return nbuf->len;
}

static __INLINE uint16
mtlk_df_nbuf_get_priority (mtlk_nbuf_t *nbuf)
{
  ASSERT(nbuf != NULL);
  return nbuf->priority;
}

static __INLINE void
mtlk_df_nbuf_set_priority (mtlk_nbuf_t *nbuf, 
                           uint16       priority)
{
  ASSERT(nbuf != NULL);
  nbuf->priority = priority;
}

static __INLINE void
mtlk_df_nbuf_list_init (mtlk_nbuf_list_t *pbuflist)
{
  ASSERT(pbuflist != NULL);
  skb_queue_head_init(pbuflist);
}

static __INLINE void
mtlk_df_nbuf_list_cleanup (mtlk_nbuf_list_t *pbuflist)
{
  ASSERT(pbuflist != NULL);
  ASSERT(skb_queue_empty(pbuflist));
  skb_queue_head_init(pbuflist);
}

static __INLINE void
mtlk_df_nbuf_list_push_front (mtlk_nbuf_list_t       *pbuflist,
                              mtlk_nbuf_list_entry_t *pentry)
{
  ASSERT(pbuflist != NULL);
  ASSERT(pentry != NULL);
  __skb_queue_head(pbuflist, pentry);
}

static __INLINE mtlk_nbuf_list_entry_t *
mtlk_df_nbuf_list_pop_front (mtlk_nbuf_list_t *pbuflist)
{
  ASSERT(pbuflist != NULL);
  return __skb_dequeue(pbuflist);
}

static __INLINE mtlk_nbuf_list_entry_t *
mtlk_df_nbuf_list_remove_entry (mtlk_nbuf_list_t       *pbuflist,
                                mtlk_nbuf_list_entry_t *pentry)
{
  struct sk_buff *ret_skb;
  ASSERT(pbuflist != NULL);
  ASSERT(pentry != NULL);
  ret_skb = pentry->next;
  __skb_unlink(pentry, pbuflist);
  return ret_skb;
}

static __INLINE void
mtlk_df_nbuf_list_push_back (mtlk_nbuf_list_t       *pbuflist,
                             mtlk_nbuf_list_entry_t *pentry)
{
  ASSERT(pbuflist != NULL);
  ASSERT(pentry != NULL);
  __skb_queue_tail(pbuflist, pentry);
}

static __INLINE mtlk_nbuf_list_entry_t *
mtlk_df_nbuf_list_head (mtlk_nbuf_list_t *pbuflist)
{
  ASSERT(pbuflist != NULL);
  return (mtlk_nbuf_list_entry_t*)pbuflist;
}

static __INLINE mtlk_nbuf_list_entry_t *
mtlk_df_nbuf_list_next (mtlk_nbuf_list_entry_t *pentry)
{
  ASSERT(pentry != NULL);
  return pentry->next;
}

static __INLINE int8
mtlk_df_nbuf_list_is_empty (mtlk_nbuf_list_t *pbuflist)
{
  ASSERT(pbuflist != NULL);
  return skb_queue_empty(pbuflist);
}

static __INLINE uint32
mtlk_df_nbuf_list_size (mtlk_nbuf_list_t* pbuflist)
{
  ASSERT(pbuflist != NULL);
  return skb_queue_len(pbuflist);
}

static __INLINE mtlk_nbuf_list_entry_t *
mtlk_df_nbuf_get_list_entry (mtlk_nbuf_t *nbuf)
{
  ASSERT(nbuf != NULL);
  return nbuf;
}

static __INLINE mtlk_nbuf_t *
mtlk_df_nbuf_get_by_list_entry (mtlk_nbuf_list_entry_t *pentry)
{
  ASSERT(pentry != NULL);
  return pentry;
}

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID
