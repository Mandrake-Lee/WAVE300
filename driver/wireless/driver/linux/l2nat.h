/*
 * $Id: l2nat.h 11187 2011-05-27 10:08:55Z Strashko $
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 * L2NAT driver subsystem.
 *
 */

#ifndef __L2NAT_H__
#define __L2NAT_H__

#include "mtlk_clipboard.h"

#define LOG_LOCAL_GID   GID_L2NAT
#define LOG_LOCAL_FID   0

typedef u8 l2nat_bslot_t;

struct l2nat_hash_entry {
  u32               ip;
  u8                mac[ETH_ALEN];
  u16               flags;
#define L2NAT_ENTRY_ACTIVE                        0x1
#define L2NAT_ENTRY_STOP_AGING_TIMER              0x2

  u32               pkts_from;

  unsigned long     last_pkt_timestamp;
  unsigned long     first_pkt_timestamp;

  struct list_head  list;

  struct timer_list timer;
  struct nic   *nic;
};

struct l2nat_hash_entry_stats {
#define L2NAT_BUCKET_STATS_SIZE 9
  u32               ip;
  u8                mac[ETH_ALEN];

  u32               pkts_from;
  unsigned long     last_pkt_timestamp;
  unsigned long     first_pkt_timestamp;
};

struct l2nat_buckets_stats {
#define L2NAT_BUCKET_STATS_SIZE 9
  int               blens[L2NAT_BUCKET_STATS_SIZE];
};


typedef struct _mtlk_l2nat_t{
  uint8 l2nat_flags;
#define L2NAT_NEED_ARP_INFO    0x1
#define L2NAT_GOT_ARP_INFO     0x2
#define L2NAT_DEF_SET_BY_USER  0x4
  uint8  l2nat_default_host[ETH_ALEN];

  spinlock_t                 l2nat_lock;

  struct list_head           l2nat_free_entries;
  struct list_head           l2nat_active_entries;

  /* real entries (ip, mac, stats, list) */
  struct l2nat_hash_entry   *l2nat_hash_entries;

  /* entries indexes, used to enlarge number of possible hash values,
   * to decrease chances of collisions
   */
  l2nat_bslot_t             *l2nat_hash;

  uint32                    l2nat_aging_timeout;

  uint8                     l2nat_mac_for_arp[ETH_ALEN];
  struct in_addr            l2nat_ip_for_arp;
  unsigned long             l2nat_last_arp_sent_timestamp;
  
  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_INIT_LOOP(TIMER_INIT);
} mtlk_l2nat_t;

int mtlk_l2nat_init (mtlk_l2nat_t *mtlk_l2nat, struct nic *nic);
void mtlk_l2nat_cleanup (mtlk_l2nat_t *mtlk_l2nat, struct nic *core);
void mtlk_l2nat_on_rx (struct nic *nic, struct sk_buff *skb);
struct sk_buff * mtlk_l2nat_on_tx (struct nic *nic, struct sk_buff *skb);
int mtlk_l2nat_get_stats(mtlk_core_t *core, mtlk_clpb_t *clpb);
void mtlk_l2nat_clear_table (struct nic *nic);
int mtlk_l2nat_user_set_def_host (struct nic *nic, IEEE_ADDR *mac);
void mtlk_l2nat_get_def_host(struct nic *nic, IEEE_ADDR *mac);

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif /* __L2NAT_H__ */
