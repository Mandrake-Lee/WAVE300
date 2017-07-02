/*
 * $Id: mcast.h 11609 2011-09-07 10:32:44Z fleytman $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 *  Originaly written by Artem Migaev
 *
 */

#ifndef _MCAST_H_
#define _MCAST_H_

#include "stadb.h"

/***************************************************/

#define MCAST_HASH_SIZE   (64)  // must be power of 2

typedef struct _mcast_sta {
  struct _mcast_sta *next;
  sta_entry         *sta;
} mcast_sta;

typedef struct _mcast_ip4_gr {
  struct _mcast_ip4_gr *next;
  struct in_addr       ip4_addr;
  mcast_sta            *mcsta;
} mcast_ip4_gr;

typedef struct _mcast_ip6_gr {
  struct _mcast_ip6_gr *next;
  struct in6_addr      ip6_addr;
  mcast_sta            *mcsta;
} mcast_ip6_gr;

typedef struct _mcast_ctx {
  mcast_ip4_gr *mcast_ip4_hash[MCAST_HASH_SIZE];
  mcast_ip6_gr *mcast_ip6_hash[MCAST_HASH_SIZE];
} mcast_ctx;

/* IGMP table definition for DF UI */

typedef enum {
  MTLK_MC_IPV4_ADDR,
  MTLK_MC_IPV6_ADDR,
  MTLK_MC_MAC_ADDR
} mtlk_mc_igmp_tbl_row_type_e;

typedef struct _mtlk_mc_igmp_tbl_item {
  mtlk_mc_igmp_tbl_row_type_e type;
} mtlk_mc_igmp_tbl_item_t;

typedef struct _mtlk_mc_igmp_tbl_ipv4_item {
  mtlk_mc_igmp_tbl_item_t header;
  struct in_addr          addr;
} mtlk_mc_igmp_tbl_ipv4_item_t;

typedef struct _mtlk_mc_igmp_tbl_ipv6_item {
  mtlk_mc_igmp_tbl_item_t header;
  struct in6_addr         addr;
} mtlk_mc_igmp_tbl_ipv6_item_t;

typedef struct _mtlk_mc_igmp_tbl_mac_item {
  mtlk_mc_igmp_tbl_item_t header;
  IEEE_ADDR               addr;
} mtlk_mc_igmp_tbl_mac_item_t;

#include "core.h"
#include "mtlk_clipboard.h"

#define LOG_LOCAL_GID   GID_MCAST
#define LOG_LOCAL_FID   0

int  mtlk_mc_parse (mtlk_core_t* nic, struct sk_buff *skb);
int  mtlk_mc_transmit (mtlk_core_t* nic, struct sk_buff *skb);
void mtlk_mc_drop_sta (struct nic *nic, const unsigned char *mac);
int  mtlk_mc_dump_groups (struct nic *nic, mtlk_clpb_t *clpb);
void mtlk_mc_restore_mac (struct sk_buff *skb);
void mtlk_mc_init (struct nic *nic);

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif // _MCAST_H_


