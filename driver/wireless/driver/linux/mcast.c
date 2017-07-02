/*
 * $Id: mcast.c 12008 2011-11-24 15:56:37Z nayshtut $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 * 
 * Reliable Multicast routines.
 * 
 *  Originaly written by Artem Migaev & Andriy Tkachuk
 *
 */

#include "mtlkinc.h"

#include <linux/igmp.h>
#include <net/addrconf.h>        // for ipv6_addr_is_multicast()

#include "mcast.h"
#include "mtlk_sq.h"
#include "sq.h"

#if (defined MTLK_DEBUG_IPERF_PAYLOAD_RX) || (defined MTLK_DEBUG_IPERF_PAYLOAD_TX)
#include "iperf_debug.h"
#endif

#define LOG_LOCAL_GID   GID_MCAST
#define LOG_LOCAL_FID   1

#define MCAST_HASH(ip)      (ip & (MCAST_HASH_SIZE - 1))
/* Define this if you want to copy skbuufs (debug only) */

/*****************************************************************************/
/***                        Module definitions                             ***/
/*****************************************************************************/

/* IPv4*/
static int           parse_igmp  (mcast_ctx *mcast, struct igmphdr *igmp_header, sta_entry *sta);
static mcast_sta*    add_ip4_sta (mcast_ctx *mcast, sta_entry *sta, struct in_addr *addr);
static int           del_ip4_sta (mcast_ctx *mcast, sta_entry *sta, struct in_addr *addr);
static mcast_ip4_gr* find_ip4_gr (mcast_ctx *mcast, struct in_addr *addr);
static int           del_ip4_gr  (mcast_ctx *mcast, mcast_ip4_gr *gr);

/* IPv6 */
static int           parse_icmp6 (mcast_ctx *mcast, struct icmp6hdr *icmp6_header, sta_entry *sta);
static mcast_sta*    add_ip6_sta (mcast_ctx *mcast, sta_entry *sta, struct in6_addr *addr);
static int           del_ip6_sta (mcast_ctx *mcast, sta_entry *sta, struct in6_addr *addr);
static mcast_ip6_gr* find_ip6_gr (mcast_ctx *mcast, struct in6_addr *addr);
static int           del_ip6_gr  (mcast_ctx *mcast, mcast_ip6_gr *gr);

/* MLDv2 headers definitions. Taken from <linux/igmp.h> */
struct mldv2_grec {
  uint8  grec_type;
  uint8  grec_auxwords;
  uint16 grec_nsrcs;
  struct in6_addr grec_mca;
  struct in6_addr grec_src[0];
};

struct mldv2_report {
  uint8  type;
  uint8  resv1;
  uint16 csum;
  uint16 resv2;
  uint16 ngrec;
  struct mldv2_grec grec[0];
};

struct mldv2_query {
  uint8  type;
	uint8  code;
	uint16 csum;
  uint16 max_response;
  uint16 resv1;
  struct in6_addr group;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	uint8 qrv:3,
        suppress:1,
        resv2:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
	uint8 resv2:4,
       suppress:1,
       qrv:3;
#else
#error "Please define endianess!"
#endif
  uint8 qqic;
  uint16 nsrcs;
  struct in6_addr srcs[0];
};

/*****************************************************************************/
/***                        API Implementation                             ***/
/*****************************************************************************/


void
mtlk_mc_init (struct nic *nic)
{
  memset(&nic->mcast, 0, sizeof(mcast_ctx));
}

int
mtlk_mc_parse (mtlk_core_t* nic, struct sk_buff *skb)
{
  int res = -1;
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(skb);
  struct ethhdr *ether_header;
  sta_entry *sta = mtlk_nbuf_priv_get_src_sta(nbuf_priv);

  // Check that we got packet from the one of connected STAs
  // TODO: Do we need this? Probably this was already checked before...
  if (sta == NULL) {
    WLOG_V("Source STA not found among connected!");
    return -1;
  }

  // TODO: This could also be checked prior to calling mc_parse()
  if (!mtlk_vap_is_ap(nic->vap_handle))
    return -1;

  // Check IP content in MAC header (Ethernet II assumed)
  ether_header = (struct ethhdr *)skb->data;
  switch (ntohs(ether_header->h_proto))
  {
  case ETH_P_IP:
    {
      // Check IGMP content in IP header
      struct iphdr *ip_header = (struct iphdr *)
            ((unsigned long)ether_header + sizeof(struct ethhdr));
      struct igmphdr *igmp_header = (struct igmphdr *)
            ((unsigned long)ip_header + ip_header->ihl * 4);

      if (ip_header->protocol == IPPROTO_IGMP)
        res = parse_igmp(&nic->mcast, igmp_header, sta);
      break;
    }
  case ETH_P_IPV6:
    {
      // Check MLD content in IPv6 header
      struct ipv6hdr *ipv6_header = (struct ipv6hdr *)
            ((unsigned long)ether_header + sizeof(struct ethhdr));
      struct ipv6_hopopt_hdr *hopopt_header = (struct ipv6_hopopt_hdr *)
            ((unsigned long)ipv6_header + sizeof(struct ipv6hdr));
      uint16 *hopopt_data = (uint16 *)
            ((unsigned long)hopopt_header + 2);
      uint16 *rtalert_data = (uint16 *)
            ((unsigned long)hopopt_data + 2);
      struct icmp6hdr *icmp6_header = (struct icmp6hdr *)
            ((unsigned long)hopopt_header + 8 * (hopopt_header->hdrlen + 1));

      if (
          (ipv6_header->nexthdr == IPPROTO_HOPOPTS) &&  // hop-by-hop option in IPv6 packet header
          (htons(*hopopt_data) == 0x0502) &&            // Router Alert option in hop-by-hop option header
          (*rtalert_data == 0) &&                       // MLD presence in Router Alert option header
          (hopopt_header->nexthdr == IPPROTO_ICMPV6)    // ICMPv6 next option in hop-by-hop option header
         )
        res = parse_icmp6(&nic->mcast, icmp6_header, sta);
      break;
    }
  default:
    break;
  }
  return res;
}



/*
 * dst_sta_id determined here only for unicast
 * or reliable multicast transfers
 * */
int
mtlk_mc_transmit (mtlk_core_t* nic, struct sk_buff *skb)
{
  mcast_sta *mcsta = NULL;
  int res = MTLK_ERR_OK;
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(skb);
  struct ethhdr *ether_header;
  uint16 ac = mtlk_qos_get_ac_by_tid(skb->priority);
  
#ifdef MTLK_DEBUG_IPERF_PAYLOAD_TX
  debug_ooo_analyze_packet(FALSE, skb, 0);
#endif

  // Save pointer to ethernet header before conversion
  ether_header = (struct ethhdr *)skb->data;

  // Determine destination STA id for unicast and backqueued
  // reliable multicast, if STA not found - drop
  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_UNICAST)) {
    if (mtlk_nbuf_priv_get_dst_sta(nbuf_priv) != NULL)
      goto MTLK_MC_TRANSMIT_NONRMCAST_UPDATE_TX;
    else {
      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_DIRECTED);
      goto MTLK_MC_TRANSMIT_CONSUME;
    }
  }

  // Broadcast transmitted as usual broadcast
  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_BROADCAST))
    goto MTLK_MC_TRANSMIT_NONRMCAST;

  // Perform checks:
  // - AP
  // - Reliable Multicast enabled
  // If failed - transmit as 802.11n generic multicast
  if ((!MTLK_CORE_HOT_PATH_PDB_GET_INT(nic,CORE_DB_CORE_RELIABLE_MCAST)) || (!mtlk_vap_is_ap(nic->vap_handle)))
    goto MTLK_MC_TRANSMIT_NONRMCAST;

  switch (ntohs(ether_header->h_proto))
  {
  case ETH_P_IP:
    {
      mcast_ip4_gr *group = NULL;
      struct in_addr ip_addr;
      struct iphdr *ip_header = (struct iphdr *)
          ((unsigned long)ether_header + sizeof(struct ethhdr));

      ip_addr.s_addr = ntohl(ip_header->daddr);

      ILOG3_D("IPv4: %B", ip_addr.s_addr);

      // Check link local multicast space 224.0.0.0/24
      if ((ip_addr.s_addr & 0xFFFFFF00) == 0xE0000000) {
        goto MTLK_MC_TRANSMIT_NONRMCAST;
      }

      // Drop unknown multicast
      if ((group = find_ip4_gr(&nic->mcast, &ip_addr)) == NULL) {
        mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_MCAST);
        goto MTLK_MC_TRANSMIT_CONSUME;
      }

      // Get first STA in group 
      mcsta = group->mcsta;

      break;
    }
  case ETH_P_IPV6:
    {
      mcast_ip6_gr *group = NULL;
      struct in6_addr ip_addr;
      struct ipv6hdr *ip_header = (struct ipv6hdr *)
          ((unsigned long)ether_header + sizeof(struct ethhdr));

      ip_addr.in6_u.u6_addr32[0] = ntohl(ip_header->daddr.in6_u.u6_addr32[0]);
      ip_addr.in6_u.u6_addr32[1] = ntohl(ip_header->daddr.in6_u.u6_addr32[1]);
      ip_addr.in6_u.u6_addr32[2] = ntohl(ip_header->daddr.in6_u.u6_addr32[2]);
      ip_addr.in6_u.u6_addr32[3] = ntohl(ip_header->daddr.in6_u.u6_addr32[3]);

      ILOG3_K("IPv6: %K", ip_addr.s6_addr);

      // Check link local multicast space FF02::/16
      if ((ip_addr.in6_u.u6_addr32[0] & 0xFF0F0000) == 0xFF020000) {
        goto MTLK_MC_TRANSMIT_NONRMCAST;
      }

      // Drop unknown multicast
      if ((group = find_ip6_gr(&nic->mcast, &ip_addr)) == NULL) {
        mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_MCAST);
        goto MTLK_MC_TRANSMIT_CONSUME;
      }

      // Get first STA in group 
      mcsta = group->mcsta;

      break;
    }

  default:
    goto MTLK_MC_TRANSMIT_NONRMCAST;
  }

  // Now skbuff is reliable multicast packet
  mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_RMCAST);
  if (mtlk_sq_enqueue_clone_begin(nic->sq, ac, SQ_PUT_BACK, skb) != MTLK_ERR_OK) {
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES);
    goto MTLK_MC_TRANSMIT_CONSUME;
  }

  // Transmit packet to all STAs in group
  for (; mcsta != NULL; mcsta = mcsta->next) {
    /* Skip source STA */
    if (mtlk_nbuf_priv_get_src_sta(nbuf_priv) == mcsta->sta)
      continue;

    mtlk_sta_update_tx(mcsta->sta, skb->priority);
    /* enqueue cloned packets to the back of the queue */
    res = mtlk_sq_enqueue_clone(nic->sq, skb, mcsta->sta);
    if (__UNLIKELY(MTLK_ERR_OK != res)) {
      nic->pstats.rmcast_dropped++;
      mtlk_record_xmit_err(nic, skb);
    }
#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
    else if (mtlk_vap_is_ap(nic->vap_handle)) {
      mtlk_aocs_on_tx_msdu_enqued(
          mtlk_core_get_master(nic)->slow_ctx->aocs,
          ac,
          mtlk_sq_get_qsize(nic->sq, ac),
          mtlk_sq_get_limit(nic->sq, ac));
    }
#endif
  }

  mtlk_sq_enqueue_clone_end(nic->sq, skb);

  /* schedule flush of the queue */
  mtlk_sq_schedule_flush(nic);

MTLK_MC_TRANSMIT_CONSUME:
  mtlk_df_nbuf_free(mtlk_vap_manager_get_master_df(mtlk_vap_get_manager(nic->vap_handle)), skb);
  return MTLK_ERR_OK;

MTLK_MC_TRANSMIT_NONRMCAST_UPDATE_TX:
  {
    sta_entry *sta = mtlk_nbuf_priv_get_dst_sta(nbuf_priv);
    MTLK_ASSERT(sta != NULL);
    mtlk_sta_update_tx(sta, skb->priority);
  }
MTLK_MC_TRANSMIT_NONRMCAST:
  /* enqueue packets to the back of the queue */
  res = mtlk_sq_enqueue(nic->sq, ac, SQ_PUT_BACK, skb);
  if (__LIKELY(MTLK_ERR_OK == res)) {
#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
    if (mtlk_vap_is_ap(nic->vap_handle)) {
      mtlk_aocs_on_tx_msdu_enqued(mtlk_core_get_master(nic)->slow_ctx->aocs, ac,
                                  mtlk_sq_get_qsize(nic->sq, ac),
                                  mtlk_sq_get_limit(nic->sq, ac));
    }
#endif
    /* schedule flush of the queue */
    mtlk_sq_schedule_flush(nic);
  } else {
    sta_entry *sta = mtlk_nbuf_priv_get_dst_sta(nbuf_priv);
    mtlk_record_xmit_err(nic, skb);
    if(NULL != sta) {
      mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_SQ_OVERFLOW);
    } else {
      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_SQ_OVERFLOW);
    }
    /*drop buffer*/
    mtlk_df_nbuf_free(mtlk_vap_manager_get_master_df(mtlk_vap_get_manager(nic->vap_handle)), skb);
  }

  return res;
}

void
mtlk_mc_drop_sta (struct nic *nic, const unsigned char *mac)
{
  mcast_sta *mcsta, *last_mcsta;
  sta_entry *sta;
  int i;

  // RM option disabled or not AP
  if (!mtlk_vap_is_ap(nic->vap_handle))
    return;

  // IPv4 hash
  for (i = 0; i < MCAST_HASH_SIZE; i++) {
    mcast_ip4_gr *gr, *gr_next;
    for (gr = nic->mcast.mcast_ip4_hash[i]; gr != NULL;) {
      last_mcsta = NULL;
      gr_next = gr->next;
      for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
        sta = mcsta->sta;
        if (0 == mtlk_osal_compare_eth_addresses(mac, mtlk_sta_get_addr(sta)->au8Addr)) {
          if (last_mcsta == NULL)
            gr->mcsta = mcsta->next;
          else
            last_mcsta->next = mcsta->next;
          kfree_tag(mcsta);

          if (gr->mcsta == NULL)
            del_ip4_gr(&nic->mcast, gr);

          break;
        }
        last_mcsta = mcsta;
      }
      gr = gr_next;
    }
  }

  // IPv6 hash
  for (i = 0; i < MCAST_HASH_SIZE; i++) {
    mcast_ip6_gr *gr, *gr_next;
    for (gr = nic->mcast.mcast_ip6_hash[i]; gr != NULL;) {
      last_mcsta = NULL;
      gr_next = gr->next;
      for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
        sta = mcsta->sta;
        if (0 == mtlk_osal_compare_eth_addresses(mac, mtlk_sta_get_addr(sta)->au8Addr)) {
          if (last_mcsta == NULL)
            gr->mcsta = mcsta->next;
          else
            last_mcsta->next = mcsta->next;
          kfree_tag(mcsta);

          if (gr->mcsta == NULL)
            del_ip6_gr(&nic->mcast, gr);

          break;
        }
        last_mcsta = mcsta;
      }
      gr = gr_next;
    }
  }

  ILOG1_Y("STA %Y dropped", mac);

  return;
}



void
mtlk_mc_restore_mac (struct sk_buff *skb)
{
  struct ethhdr *ether_header = (struct ethhdr *)skb->data;

  switch (ntohs(ether_header->h_proto))
  {
  case ETH_P_IP:
    {
      struct in_addr ip_addr;
      struct iphdr *ip_header = (struct iphdr *)
          ((unsigned long)ether_header + sizeof(struct ethhdr));
      
      /* do nothing if destination address is:
       *   - not multicast;
       *   - link local multicast.
       */
      if (!mtlk_osal_ipv4_is_multicast(ip_header->daddr) || 
           mtlk_osal_ipv4_is_local_multicast(ip_header->daddr))
        return;
  
      ip_addr.s_addr = ntohl(ip_header->daddr);

      ether_header->h_dest[0] = 0x01;
      ether_header->h_dest[1] = 0x00;
      ether_header->h_dest[2] = 0x5E;
      ether_header->h_dest[3] = (ip_addr.s_addr >> 16) & 0x7F;
      ether_header->h_dest[4] = (ip_addr.s_addr >> 8) & 0xFF;
      ether_header->h_dest[5] = ip_addr.s_addr & 0xFF;

      break;
    }
  case ETH_P_IPV6:
    {
      struct in6_addr ip_addr;
      struct ipv6hdr *ip_header = (struct ipv6hdr *)
          ((unsigned long)ether_header + sizeof(struct ethhdr));

      /* do nothing if destination address is not multicast */
      if (ipv6_addr_is_multicast(&ip_header->daddr))
        return;

      ip_addr.in6_u.u6_addr32[0] = ntohl(ip_header->daddr.in6_u.u6_addr32[0]);
      ip_addr.in6_u.u6_addr32[1] = ntohl(ip_header->daddr.in6_u.u6_addr32[1]);
      ip_addr.in6_u.u6_addr32[2] = ntohl(ip_header->daddr.in6_u.u6_addr32[2]);
      ip_addr.in6_u.u6_addr32[3] = ntohl(ip_header->daddr.in6_u.u6_addr32[3]);

      // Check link local multicast space FF02::/16
      if ((ip_addr.in6_u.u6_addr32[0] & 0xFF0F0000) == 0xFF020000)
        return;

      ether_header->h_dest[0] = 0x33;
      ether_header->h_dest[1] = 0x33;
      ether_header->h_dest[2] = ip_addr.in6_u.u6_addr8[12];
      ether_header->h_dest[3] = ip_addr.in6_u.u6_addr8[13];
      ether_header->h_dest[4] = ip_addr.in6_u.u6_addr8[14];
      ether_header->h_dest[5] = ip_addr.in6_u.u6_addr8[15];

      break;
    }
  default:
    break;
  }
}

int
mtlk_mc_dump_groups (struct nic *nic, mtlk_clpb_t *clpb)
{
  int                           res = MTLK_ERR_OK;
  int                           idx;
  mcast_sta                     *mcsta;
  sta_entry                     *sta;
  mtlk_mc_igmp_tbl_mac_item_t   igmp_mac_item;


  /* IPv4 hash */
  for (idx = 0; idx < MCAST_HASH_SIZE; idx++) {
    mcast_ip4_gr                  *gr;
    mtlk_mc_igmp_tbl_ipv4_item_t  igmp_ipv4_item;

    for (gr = nic->mcast.mcast_ip4_hash[idx]; gr != NULL; gr = gr->next) {

      /* Store IPv4 MC group address item */
      igmp_ipv4_item.header.type = MTLK_MC_IPV4_ADDR;
      igmp_ipv4_item.addr = gr->ip4_addr;

      res = mtlk_clpb_push(clpb, &igmp_ipv4_item, sizeof(igmp_ipv4_item));
      if (MTLK_ERR_OK != res) {
        goto err_push;
      }

      /* Store MAC addresses of IPv4 MC group members */
      for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {

        sta  = mcsta->sta;
        igmp_mac_item.header.type = MTLK_MC_MAC_ADDR;
        igmp_mac_item.addr = *mtlk_sta_get_addr(sta);

        res = mtlk_clpb_push(clpb, &igmp_mac_item, sizeof(igmp_mac_item));
        if (MTLK_ERR_OK != res) {
          goto err_push;
        }
      }
    }
  }

  /* IPv6 hash */
  for (idx = 0; idx < MCAST_HASH_SIZE; idx++) {
    mcast_ip6_gr                  *gr;
    mtlk_mc_igmp_tbl_ipv6_item_t  igmp_ipv6_item;

    for (gr = nic->mcast.mcast_ip6_hash[idx]; gr != NULL; gr = gr->next) {

      /* Store IPv4 MC group address item */
      igmp_ipv6_item.header.type = MTLK_MC_IPV6_ADDR;
      igmp_ipv6_item.addr = gr->ip6_addr;

      res = mtlk_clpb_push(clpb, &igmp_ipv6_item, sizeof(igmp_ipv6_item));
      if (MTLK_ERR_OK != res) {
        goto err_push;
      }

      /* Store MAC addresses of IPv4 MC group members */
      for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {

        sta  = mcsta->sta;
        igmp_mac_item.header.type = MTLK_MC_MAC_ADDR;
        igmp_mac_item.addr = *mtlk_sta_get_addr(sta);

        res = mtlk_clpb_push(clpb, &igmp_mac_item, sizeof(igmp_mac_item));
        if (MTLK_ERR_OK != res) {
          goto err_push;
        }
      }
    }
  }

  goto finish;

err_push:
   mtlk_clpb_purge(clpb);
finish:
   return res;
}

/*****************************************************************************/
/***                        Local IPv4 functions                           ***/
/*****************************************************************************/


static mcast_sta*
add_ip4_sta (mcast_ctx *mcast, sta_entry *sta, struct in_addr *addr)
{
  mcast_sta *mcsta;
  unsigned int hash = MCAST_HASH(addr->s_addr);
  mcast_ip4_gr *gr;

  // Check link local IPv4 multicast space 224.0.0.0/24 and source station ID
  if (((addr->s_addr & 0xFFFFFF00) == 0xE0000000) || (sta == NULL))
    return NULL;

  gr = find_ip4_gr(mcast, addr);
  if (gr == NULL) {
    gr = kmalloc_tag(sizeof(mcast_ip4_gr), GFP_ATOMIC, MTLK_MEM_TAG_MCAST);
    ASSERT(gr != NULL);

    if (gr == NULL)
      return NULL;
 
    gr->mcsta = NULL;
    gr->ip4_addr = *addr;
    gr->next = mcast->mcast_ip4_hash[hash];
    mcast->mcast_ip4_hash[hash] = gr;
  }

  for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
    if (mcsta->sta == sta)
      return NULL;
  }
  
  mcsta = kmalloc_tag(sizeof(mcast_sta), GFP_ATOMIC, MTLK_MEM_TAG_MCAST);
  ASSERT(mcsta != NULL);

  if (mcsta == NULL)
    return NULL;

  mcsta->next = gr->mcsta;
  mcsta->sta = sta;
  gr->mcsta = mcsta;

  return mcsta;
}



static int
del_ip4_sta (mcast_ctx *mcast, sta_entry *sta, struct in_addr *addr)
{
  mcast_sta *mcsta, *last_mcsta = NULL;
  mcast_ip4_gr *gr = find_ip4_gr(mcast, addr);

  if (gr == NULL)
    return 0;

  if (sta == NULL)
    return 0;
  
  for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
    if (mcsta->sta == sta)
      break;
    last_mcsta = mcsta;
  }

  if (mcsta == NULL)
    return 0;

  if (last_mcsta == NULL)
    gr->mcsta = mcsta->next;
  else
    last_mcsta->next = mcsta->next;
  kfree_tag(mcsta);

  if (gr->mcsta == NULL)
    return del_ip4_gr(mcast, gr);

  return 1;
}



static int
del_ip4_gr (mcast_ctx *mcast, mcast_ip4_gr *gr)
{
  unsigned int hash;
  mcast_ip4_gr *last_gr;

  if (gr == NULL)
    return 0;

  hash = MCAST_HASH(gr->ip4_addr.s_addr);

  if (mcast->mcast_ip4_hash[hash] == gr) {
    mcast->mcast_ip4_hash[hash] = gr->next;
  } else {
    for (last_gr = mcast->mcast_ip4_hash[hash];
         last_gr != NULL;
         last_gr = last_gr->next)
    {
      if (last_gr->next == gr) {
        last_gr->next = gr->next;
        break;
      }
    }
    if (last_gr == NULL)
      return 0;
  }
  kfree_tag(gr);

  return 1;
}



static mcast_ip4_gr *
find_ip4_gr (mcast_ctx *mcast, struct in_addr *addr)
{
  unsigned int hash = MCAST_HASH(addr->s_addr);
  mcast_ip4_gr *gr = mcast->mcast_ip4_hash[hash];

  for (; gr != NULL; gr = gr->next)
    if (gr->ip4_addr.s_addr == addr->s_addr)
      break;

  return gr;
}



static int
parse_igmp (mcast_ctx *mcast, struct igmphdr *igmp_header, sta_entry *sta)
{
  uint32 grp_num, src_num;
  struct igmpv3_report *report;
  struct igmpv3_grec *record;
  int i;
  struct in_addr grp_addr;
 
  grp_addr.s_addr = ntohl(igmp_header->group);

  switch (igmp_header->type) {
  case IGMP_HOST_MEMBERSHIP_QUERY:
    break;
  case IGMP_HOST_MEMBERSHIP_REPORT:
    /* fallthrough */
  case IGMPV2_HOST_MEMBERSHIP_REPORT:
    ILOG1_DP("Membership report: IPv4 %B, sta %p", grp_addr.s_addr, sta);
    add_ip4_sta(mcast, sta, &grp_addr);
    break;
  case IGMP_HOST_LEAVE_MESSAGE:
    ILOG1_DP("Leave group report: IPv4 %B, sta %p", grp_addr.s_addr, sta);
    del_ip4_sta(mcast, sta, &grp_addr);
    break;
  case IGMPV3_HOST_MEMBERSHIP_REPORT:
    report = (struct igmpv3_report *)igmp_header;
    grp_num = ntohs(report->ngrec);
    ILOG1_DP("IGMPv3 report: %d record(s), sta %p", grp_num, sta);
    record = report->grec;
    for (i = 0; i < grp_num; i++) {
      src_num = ntohs(record->grec_nsrcs);
      grp_addr.s_addr = ntohl(record->grec_mca);
      ILOG1_D(" *** IPv4 %B", grp_addr.s_addr);
      switch (record->grec_type) {
      case IGMPV3_MODE_IS_INCLUDE:
        /* fallthrough */
      case IGMPV3_CHANGE_TO_INCLUDE:
        ILOG1_D(" --- Mode is include, %d source(s)", src_num);
        // Station removed from the multicast list only if
        // no sources included
        if (src_num == 0)
          del_ip4_sta(mcast, sta, &grp_addr);  
        break;
      case IGMPV3_MODE_IS_EXCLUDE:
        /* fallthrough */
      case IGMPV3_CHANGE_TO_EXCLUDE:
        ILOG1_D(" --- Mode is exclude, %d source(s)", src_num);
        // Station added to the multicast llist no matter
        // how much sources are excluded
        add_ip4_sta(mcast, sta, &grp_addr);
        break;
      case IGMPV3_ALLOW_NEW_SOURCES:
        ILOG1_D(" --- Allow new sources, %d source(s)", src_num);
        // ignore
        break;
      case IGMPV3_BLOCK_OLD_SOURCES:
        ILOG1_D(" --- Block old sources, %d source(s)", src_num);
        // ignore
        break;
      default:  // Unknown IGMPv3 record
        ILOG1_D(" --- Unknown record type %d", record->grec_type);
        break;
      }
      record = (struct igmpv3_grec *)((void *)record +
               sizeof(struct igmpv3_grec) +
               sizeof(struct in_addr) * src_num +
               sizeof(u32) * record->grec_auxwords);
    }
    break;
  default:      // Unknown IGMP message
    ILOG1_D("Unknown IGMP message type %d", igmp_header->type);
    break;
  }
  return 0;
}


/*****************************************************************************/
/***                        Local IPv6 functions                           ***/
/*****************************************************************************/


static mcast_sta *
add_ip6_sta (mcast_ctx *mcast, sta_entry *sta, struct in6_addr *addr)
{
  mcast_sta *mcsta;
  unsigned int hash = MCAST_HASH(addr->in6_u.u6_addr32[3]);
  mcast_ip6_gr *gr;

  // Check link local multicast space FF02::/16 and source station ID
  if (((addr->in6_u.u6_addr32[0] & 0xFF0F0000) == 0xFF020000) || (sta == NULL))
    return NULL;
 
  gr = find_ip6_gr(mcast, addr); 
  if (gr == NULL) {
    gr = kmalloc_tag(sizeof(mcast_ip6_gr), GFP_ATOMIC, MTLK_MEM_TAG_MCAST);
    ASSERT(gr != NULL);

    if (gr == NULL)
      return NULL;
 
    gr->mcsta = NULL;
    gr->ip6_addr = *addr;
    gr->next = mcast->mcast_ip6_hash[hash];
    mcast->mcast_ip6_hash[hash] = gr;
  }

  for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
    if (mcsta->sta == sta)
      return NULL;
  }
  
  mcsta = kmalloc_tag(sizeof(mcast_sta), GFP_ATOMIC, MTLK_MEM_TAG_MCAST);
  ASSERT(mcsta != NULL);

  if (mcsta == NULL)
    return NULL;

  mcsta->next = gr->mcsta;
  mcsta->sta = sta;
  gr->mcsta = mcsta;

  return mcsta;
}



static int
del_ip6_sta (mcast_ctx *mcast, sta_entry *sta, struct in6_addr *addr)
{
  mcast_sta *mcsta, *last_mcsta = NULL;
  mcast_ip6_gr *gr = find_ip6_gr(mcast, addr);

  if (gr == NULL)
    return 0;

  if (sta == NULL)
    return 0;
  
  for (mcsta = gr->mcsta; mcsta != NULL; mcsta = mcsta->next) {
    if (mcsta->sta == sta)
      break;
    last_mcsta = mcsta;
  }

  if (mcsta == NULL)
    return 0;

  if (last_mcsta == NULL)
    gr->mcsta = mcsta->next;
  else
    last_mcsta->next = mcsta->next;
  kfree_tag(mcsta);

  if (gr->mcsta == NULL)
    return del_ip6_gr(mcast, gr);

  return 1;
}



static int
del_ip6_gr (mcast_ctx *mcast, mcast_ip6_gr *gr)
{
  unsigned int hash;
  mcast_ip6_gr *last_gr;

  if (gr == NULL)
    return 0;

  hash = MCAST_HASH(gr->ip6_addr.in6_u.u6_addr32[3]);

  if (mcast->mcast_ip6_hash[hash] == gr) {
    mcast->mcast_ip6_hash[hash] = gr->next;
  } else {
    for (last_gr = mcast->mcast_ip6_hash[hash];
         last_gr != NULL;
         last_gr = last_gr->next)
    {
      if (last_gr->next == gr) {
        last_gr->next = gr->next;
        break;
      }
    }
    if (last_gr == NULL)
      return 0;
  }
  kfree_tag(gr);

  return 1;
}



static mcast_ip6_gr*
find_ip6_gr (mcast_ctx *mcast, struct in6_addr *addr)
{
  unsigned int hash = MCAST_HASH(addr->in6_u.u6_addr32[3]);
  mcast_ip6_gr *gr = mcast->mcast_ip6_hash[hash];

  for (; gr != NULL; gr = gr->next)
    if (ipv6_addr_equal(&gr->ip6_addr, addr))
      break;

  return gr;
}



static int
parse_icmp6 (mcast_ctx *mcast, struct icmp6hdr *icmp6_header, sta_entry *sta)
{
  uint32 grp_num, src_num;
  int i;
  struct mldv2_report *report;
  struct mldv2_grec *record;
  struct in6_addr grp_addr, *addr;
 
  addr = (struct in6_addr *)
    ((unsigned long)icmp6_header + sizeof(struct icmp6hdr));
  grp_addr.in6_u.u6_addr32[0] = ntohl(addr->in6_u.u6_addr32[0]);
  grp_addr.in6_u.u6_addr32[1] = ntohl(addr->in6_u.u6_addr32[1]);
  grp_addr.in6_u.u6_addr32[2] = ntohl(addr->in6_u.u6_addr32[2]);
  grp_addr.in6_u.u6_addr32[3] = ntohl(addr->in6_u.u6_addr32[3]);

  switch (icmp6_header->icmp6_type)
  {
  case ICMPV6_MGM_QUERY:
    break;
  case ICMPV6_MGM_REPORT:
    ILOG1_KP("MLD membership report: IPv6 %K, sta %p", grp_addr.s6_addr, sta);
    add_ip6_sta(mcast, sta, &grp_addr);
    break;
  case ICMPV6_MGM_REDUCTION:
    ILOG1_KP("MLD membership done: IPv6 %K, sta %p", grp_addr.s6_addr, sta);
    del_ip6_sta(mcast, sta, &grp_addr);
    break;
  case ICMPV6_MLD2_REPORT:
    report = (struct mldv2_report *)icmp6_header;
    grp_num = ntohs(report->ngrec);
    ILOG1_DP("MLDv2 report: %d record(s), sta %p", grp_num, sta);
    record = report->grec;
    for (i = 0; i < grp_num; i++) {
      src_num = ntohs(record->grec_nsrcs);
      addr = (struct in6_addr *)
        ((unsigned long)icmp6_header + sizeof(struct icmp6hdr));
      grp_addr.in6_u.u6_addr32[0] = ntohl(record->grec_mca.in6_u.u6_addr32[0]);
      grp_addr.in6_u.u6_addr32[1] = ntohl(record->grec_mca.in6_u.u6_addr32[1]);
      grp_addr.in6_u.u6_addr32[2] = ntohl(record->grec_mca.in6_u.u6_addr32[2]);
      grp_addr.in6_u.u6_addr32[3] = ntohl(record->grec_mca.in6_u.u6_addr32[3]);
      ILOG1_K(" *** IPv6 %K", grp_addr.s6_addr);
      switch (record->grec_type) {
      case MLD2_MODE_IS_INCLUDE:
        /* fallthrough */
      case MLD2_CHANGE_TO_INCLUDE:
        ILOG1_D(" --- Mode is include, %d source(s)", src_num);
        // Station removed from the multicast list only if
        // no sources included
        if (src_num == 0)
          del_ip6_sta(mcast, sta, &grp_addr);  
        break;
      case MLD2_MODE_IS_EXCLUDE:
        /* fallthrough */
      case MLD2_CHANGE_TO_EXCLUDE:
        ILOG1_D(" --- Mode is exclude, %d source(s)", src_num);
        // Station added to the multicast llist no matter
        // how much sources are excluded
        add_ip6_sta(mcast, sta, &grp_addr);
        break;
      case MLD2_ALLOW_NEW_SOURCES:
        ILOG1_D(" --- Allow new sources, %d source(s)", src_num);
        // ignore
        break;
      case MLD2_BLOCK_OLD_SOURCES:
        ILOG1_D(" --- Block old sources, %d source(s)", src_num);
        // ignore
        break;
      default:  // Unknown MLDv2 record
        ILOG1_D(" --- Unknown record type %d", record->grec_type);
        break;
      }
      record = (struct mldv2_grec *)((void *)record +
               sizeof(struct mldv2_grec) +
               sizeof(struct in6_addr) * src_num +
               sizeof(u32) * record->grec_auxwords);
    }
    break;
  default:
    ILOG1_V("Unknown ICMPv6/MLD message");
    break;
  }
  return 0;
}

