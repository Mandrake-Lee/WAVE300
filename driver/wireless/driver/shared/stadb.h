/*
* $Id: stadb.h 12852 2012-03-19 17:25:17Z laptijev $
*
* Copyright (c) 2006-2007 Metalink Broadband (Israel)
*
* Written by: Dmitry Fleytman
*
*/

#ifndef _MTLK_STADB_H_
#define _MTLK_STADB_H_

#include "mtlk_sq.h"
#include "mtlklist.h"
#include "mtlkqos.h"
#include "addba.h"
#include "rod.h"
#include "mtlkhash_ieee_addr.h"
#include "mtlk_clipboard.h"
#include "mtlkirbd.h"
#include "mtlk_wss.h"
#include "mtlk_analyzer.h"
#include "mtlkwssa_drvinfo.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define LOG_LOCAL_GID   GID_STADB
#define LOG_LOCAL_FID   0

#define DEFAULT_BEACON_INTERVAL (100)

typedef enum
{
  MTLK_STAI_CNT_PACKETS_RECEIVED,                       /* number of packets received */
  MTLK_STAI_CNT_BYTES_RECEIVED,                         /* number of bytes received */
  MTLK_STAI_CNT_PACKETS_SENT,                           /* number of packets transmitted */
  MTLK_STAI_CNT_TX_PACKETS_DISCARDED_FW,                /* number of packets failed to transmit (MAC timeout reached) */
  MTLK_STAI_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD,
  MTLK_STAI_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE,
  MTLK_STAI_CNT_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES,
  MTLK_STAI_CNT_TX_PACKETS_DISCARDED_SQ_OVERFLOW,
  MTLK_STAI_CNT_TX_PACKETS_DISCARDED_EAPOL_FILTER,
  MTLK_STAI_CNT_TX_PACKETS_DISCARDED_DROP_ALL_FILTER,
  MTLK_STAI_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW,
  MTLK_STAI_CNT_BYTES_SENT,    /* Number of bytes sent successfully */
  MTLK_STAI_CNT_FWD_RX_PACKETS,
  MTLK_STAI_CNT_FWD_RX_BYTES,
  MTLK_STAI_CNT_UNICAST_PACKETS_SENT,
  MTLK_STAI_CNT_UNICAST_PACKETS_RECEIVED,
  MTLK_STAI_CNT_PS_MODE_ENTRANCES,
  MTLK_STAI_CNT_TX_PACKETS_DISCARDED_ACM,
  MTLK_STAI_CNT_TX_PACKETS_DISCARDED_EAPOL_CLONED,
  MTLK_STAI_CNT_ADDBA_REQUEST_TX,
  MTLK_STAI_CNT_ADDBA_POSITIVE_RESPONSE_TX,
  MTLK_STAI_CNT_ADDBA_NEGATIVE_RESPONSE_TX,
  MTLK_STAI_CNT_ADDBA_REQUEST_RX,
  MTLK_STAI_CNT_ADDBA_POSITIVE_RESPONSE_RX,
  MTLK_STAI_CNT_ADDBA_NEGATIVE_RESPONSE_RX,
  MTLK_STAI_CNT_ADDBA_UNCONFIRMED_RESPONSE_RX,
  MTLK_STAI_CNT_DELBA_REQUEST_TX,
  MTLK_STAI_CNT_DELBA_SENT_REQUEST_RX,
  MTLK_STAI_CNT_DELBA_RECEIVED_REQUEST_RX,
  MTLK_STAI_CNT_AGGR_OPEN_REQUEST,
  MTLK_STAI_CNT_AGGR_OPEN_CONFIRM,
  MTLK_STAI_CNT_AGGR_CLOSE_REQUEST,
  MTLK_STAI_CNT_AGGR_CLOSE_CONFIRM,
  MTLK_STAI_CNT_LAST
} sta_info_cnt_id_e;

typedef enum {
  MTLK_PCKT_FLTR_ALLOW_ALL,
  MTLK_PCKT_FLTR_ALLOW_802_1X,
  MTLK_PCKT_FLTR_DISCARD_ALL
} mtlk_pckt_filter_e;

typedef struct _sta_capabilities
{
  uint32 NetModesSupported;
  uint8 MIMOConfigTX;
  uint8 MIMOConfigRX;
  uint8 WMMSupported;
  uint8 CBSupported;
  uint8 SGI20Supported;
  uint8 SGI40Supported;
  uint8 AMPDUMaxLengthExp;
  uint8 AMPDUMinStartSpacing;
  uint8 STBCSupported;
  uint8 LDPCSupported;
  uint8 Vendor;
  uint8 LQLDPCEnabled;
  uint8 BFSupported;
} __MTLK_IDATA sta_capabilities;

typedef struct _sta_info
{
  uint8              cipher;       /* TKIP or CCMP */
  mtlk_pckt_filter_e filter;       /* flag to filter packets */
  BOOL               dot11n_mode;  /* .11n support mode? */
  uint8              net_mode;
  uint16             tx_rate;
  uint8              rx_rate;
  uint8              scp;
  uint8              rssi[NUM_OF_RX_ANT];
  sta_capabilities   capabilities;
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  uint8              rf_mgmt_data; /* RF Management related info */
#endif
  mtlk_peer_analyzer_t sta_analyzer;
} __MTLK_IDATA sta_info;

struct _sta_db; /* TODO: replace it with Param DB/callbacks with opaque pointers */

struct _sta_entry {
  MTLK_HASH_ENTRY_T(ieee_addr) hentry;
  mtlk_atomic_t                ref_cnt;
  sta_info                     info;
  mtlk_osal_spinlock_t         lock;
  mtlk_osal_timer_t            idle_timer;
  mtlk_osal_timestamp_t        timestamp;
  mtlk_osal_timestamp_t        connection_timestamp;
  reordering_queue             rod_queue[NTS_TIDS];  // reordering structures per STA
  mtlk_addba_peer_t            addba_peer;           // addba context
  mtlk_osal_timer_t            keepalive_timer;      // Timer for sending NULL data packets
  mtlk_atomic_t                tx_packets[NTS_TIDS]; // TX packets counters per STA
  BOOL                         aggr_inited[NTS_TIDS]; // aggregation inited flag per STA
  mtlk_sq_peer_ctx_t           sq_peer_ctx;
  /* Description of AP */
  uint16                       beacon_interval;      // AP's beacon interval
  mtlk_osal_timestamp_t        beacon_timestamp;     // AP's last beacon timestamp
  mtlk_vap_handle_t            vap_handle;
  struct _sta_db              *paramdb;
  mtlk_irbd_t                 *irbd;
  mtlk_irbd_handle_t          *stat_irb_handle;
  mtlk_wss_t                  *wss;
  mtlk_wss_cntr_handle_t      *wss_hcntrs[MTLK_STAI_CNT_LAST];
  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
  MTLK_DECLARE_START_LOOP(ROD_QUEs);
} __MTLK_IDATA;

static __INLINE uint32
mtlk_sta_get_cnt (const sta_entry  *sta, 
                  sta_info_cnt_id_e cnt_id)
{
  MTLK_ASSERT(cnt_id >= 0 && cnt_id < MTLK_STAI_CNT_LAST);

  return mtlk_wss_get_stat(sta->wss, cnt_id);
}

static __INLINE uint32
mtlk_sta_get_short_term_rssi(const sta_entry  *sta)
{
  return mtlk_peer_analyzer_get_short_term_rssi(&sta->info.sta_analyzer);
}

static __INLINE uint32
mtlk_sta_get_long_term_rssi(const sta_entry  *sta)
{
  return mtlk_peer_analyzer_get_long_term_rssi(&sta->info.sta_analyzer);
}

static __INLINE uint32
mtlk_sta_get_short_term_tx(const sta_entry  *sta)
{
  return mtlk_peer_analyzer_get_short_term_tx(&sta->info.sta_analyzer);
}

static __INLINE uint32
mtlk_sta_get_long_term_tx(const sta_entry  *sta)
{
  return mtlk_peer_analyzer_get_long_term_tx(&sta->info.sta_analyzer);
}

static __INLINE uint32
mtlk_sta_get_short_term_rx(const sta_entry  *sta)
{
  return mtlk_peer_analyzer_get_short_term_rx(&sta->info.sta_analyzer);
}

static __INLINE uint32
mtlk_sta_get_long_term_rx(const sta_entry  *sta)
{
  return mtlk_peer_analyzer_get_long_term_rx(&sta->info.sta_analyzer);
}

static __INLINE uint32
mtlk_sta_get_retransmissions_number_short(const sta_entry *sta)
{
  return mtlk_peer_analyzer_get_retransmissions_number_short(&sta->info.sta_analyzer);
}

static __INLINE uint8
mtlk_sta_get_cipher (const sta_entry *sta)
{
  return sta->info.cipher;
}

static __INLINE mtlk_pckt_filter_e
mtlk_sta_get_packets_filter (const sta_entry *sta)
{
  return sta->info.filter;
}

static __INLINE BOOL
mtlk_sta_is_dot11n (const sta_entry *sta)
{
  return sta->info.dot11n_mode;
}

static __INLINE uint8
mtlk_sta_get_net_mode (const sta_entry *sta)
{
  return sta->info.net_mode;
}

static __INLINE uint16
mtlk_sta_get_tx_rate (const sta_entry *sta)
{
  return sta->info.tx_rate;
}

__MTLK_IFUNC uint16
mtlk_sta_get_rx_rate (const sta_entry *sta);

/* Get time passed since station connected in seconds */
static __INLINE uint32
mtlk_sta_get_age (const sta_entry *sta)
{
  mtlk_osal_timestamp_diff_t diff = mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), sta->connection_timestamp);
  return mtlk_osal_timestamp_to_ms(diff) / 1000; /* Converted from milliseconds to seconds */
}

static __INLINE void
mtlk_sta_get_rssi (const sta_entry *sta,
                   uint8           *rssi,
                   uint32          size)
{
  MTLK_ASSERT(rssi != NULL);
  MTLK_ASSERT(size == ARRAY_SIZE(sta->info.rssi));

  memcpy(rssi, sta->info.rssi, ARRAY_SIZE(sta->info.rssi));
}

static __INLINE uint8
mtlk_sta_get_max_rssi (const sta_entry *sta)
{
  return MAX(sta->info.rssi[0], MAX(sta->info.rssi[1], sta->info.rssi[2]));
}

#ifdef MTCFG_RF_MANAGEMENT_MTLK
static __INLINE uint8
mtlk_sta_get_rf_mgmt_data (const sta_entry *sta)
{
  return sta->info.rf_mgmt_data;
}
#endif

static __INLINE const IEEE_ADDR *
mtlk_sta_get_addr (const sta_entry *sta)
{
  return MTLK_HASH_VALUE_GET_KEY(sta, hentry);
}

static __INLINE BOOL
mtlk_sta_get_rod_in_use (const sta_entry *sta, uint8 tid)
{
  return mtlk_is_used_rod_queue(&sta->rod_queue[tid])?TRUE:FALSE;
}

static __INLINE BOOL
mtlk_sta_get_rod_stats (const sta_entry *sta, uint8 tid, reordering_stats *s)
{
  const reordering_stats *pstats = mtlk_get_rod_stats(&sta->rod_queue[tid]);

  if (pstats) {
    memcpy(s, pstats, sizeof(*s));
    return TRUE;
  }

  return FALSE;
}

static __INLINE BOOL
mtlk_sta_sq_is_empty (const sta_entry *sta)
{
  return mtlk_sq_is_empty(&sta->sq_peer_ctx);
}

void __MTLK_IFUNC
mtlk_sta_get_peer_stats(const sta_entry* sta, mtlk_wssa_drv_peer_stats_t* stats);
void __MTLK_IFUNC
mtlk_sta_get_peer_capabilities(const sta_entry* sta, mtlk_wssa_drv_peer_capabilities_t* capabilities);

static __INLINE mtlk_addba_peer_t *
mtlk_sta_get_addb_peer (sta_entry *sta)
{
  return &sta->addba_peer;
}

static __INLINE mtlk_sq_peer_ctx_t *
mtlk_sta_get_sq (sta_entry *sta)
{
  return &sta->sq_peer_ctx;
}

/********************************************************
 * WARNING: __mtlk_sta_on_unref_private is private API! *
 *          No one is allowed to use it except the      *
 *          mtlk_sta_decref.                            *
 ********************************************************/
void __MTLK_IFUNC
__mtlk_sta_on_unref_private(sta_entry *sta);
/********************************************************/

//#define STA_REF_DBG

#ifndef STA_REF_DBG
static __INLINE void
mtlk_sta_incref (sta_entry  *sta)
{
  mtlk_osal_atomic_inc(&sta->ref_cnt);
}

static __INLINE void
mtlk_sta_decref (sta_entry *sta)
{
  uint32 ref_cnt = mtlk_osal_atomic_dec(&sta->ref_cnt);

  if (ref_cnt == 0) {
    __mtlk_sta_on_unref_private(sta);
  }
}
#else
#define mtlk_sta_incref(sta) __mtlk_sta_incref_dbg(MTLK_SLID, (sta))

static __INLINE void
__mtlk_sta_incref_dbg (mtlk_slid_t slid, sta_entry *sta)
{
  uint32 ref_cnt = mtlk_osal_atomic_inc(&sta->ref_cnt);
  ILOG0_DDDD("STA referenced from G:%d F:%d L:%d. The new refcount is %d",
            mtlk_slid_get_gid(ainfo->allocator_slid),
            mtlk_slid_get_fid(ainfo->allocator_slid),
            mtlk_slid_get_lid(ainfo->allocator_slid),
            ref_cnt);
}

#define mtlk_sta_decref(sta) __mtlk_sta_decref_dbg(MTLK_SLID, (sta))

static __INLINE void
__mtlk_sta_decref_dbg (const char *f, int l, sta_entry *sta)
{
  uint32 ref_cnt = mtlk_osal_atomic_dec(&sta->ref_cnt);
  ILOG0_DDDD("STA dereferenced from G:%d F:%d L:%d. The new refcount is %d",
            mtlk_slid_get_gid(ainfo->allocator_slid),
            mtlk_slid_get_fid(ainfo->allocator_slid),
            mtlk_slid_get_lid(ainfo->allocator_slid),
            ref_cnt);

  if (ref_cnt == 0) {
    __mtlk_sta_on_unref_private(sta);
  }
}
#endif /* STA_REF_DBG */

void __MTLK_IFUNC
mtlk_sta_on_packet_sent(sta_entry *sta, mtlk_nbuf_t *nbuf, uint32 retransmissions);

void __MTLK_IFUNC
mtlk_sta_on_frame_arrived(sta_entry *sta, uint8 rssi);

typedef enum
{
  MTLK_TX_DISCARDED_FW                = MTLK_STAI_CNT_TX_PACKETS_DISCARDED_FW,
  MTLK_TX_DISCARDED_DRV_NO_RESOURCES  = MTLK_STAI_CNT_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES,
  MTLK_TX_DISCARDED_SQ_OVERFLOW       = MTLK_STAI_CNT_TX_PACKETS_DISCARDED_SQ_OVERFLOW,
  MTLK_TX_DISCARDED_EAPOL_FILTER      = MTLK_STAI_CNT_TX_PACKETS_DISCARDED_EAPOL_FILTER,
  MTLK_TX_DISCARDED_DROP_ALL_FILTER   = MTLK_STAI_CNT_TX_PACKETS_DISCARDED_DROP_ALL_FILTER,
  MTLK_TX_DISCARDED_TX_QUEUE_OVERFLOW = MTLK_STAI_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW,
  MTLK_TX_DISCARDED_DRV_ACM           = MTLK_STAI_CNT_TX_PACKETS_DISCARDED_ACM,
  MTLK_TX_DISCARDED_EAPOL_CLONED      = MTLK_STAI_CNT_TX_PACKETS_DISCARDED_EAPOL_CLONED
} mtlk_tx_drop_reasons_e;

void __MTLK_IFUNC
mtlk_sta_on_packet_dropped(sta_entry *sta, mtlk_tx_drop_reasons_e reason);

void __MTLK_IFUNC
mtlk_sta_on_packet_indicated(sta_entry *sta, mtlk_nbuf_t *nbuf);

void __MTLK_IFUNC
mtlk_sta_on_rx_packet_forwarded(sta_entry *sta, mtlk_nbuf_t *nbuf);

void __MTLK_IFUNC 
mtlk_sta_update_tx(sta_entry *sta, uint16 tid);

int __MTLK_IFUNC
mtlk_sta_update_beacon_interval(sta_entry *sta, uint16 beacon_interval);

void __MTLK_IFUNC
mtlk_sta_set_pm_enabled(sta_entry *sta, BOOL enabled);

void __MTLK_IFUNC
mtlk_sta_set_capabilities(sta_entry *sta, const sta_capabilities *capabilities);

static __INLINE void
mtlk_sta_update_rx_rate(sta_entry *sta, const MAC_RX_ADDITIONAL_INFO_T *mac_rx_info)
{
  /* According to WAVE300_SW-1154, the sCP is only valid when a legacy packet or
   * a last packet in aggregation arrives.
   * Thus, we only update RX Rate and SCP fields in these cases.
   * For other packets the rate cannot be calculated.
   */
  if (MTLK_BFIELD_GET(mac_rx_info->u8HwInfo, HW_INFO_AGGR) == 0 || /* Legacy packet */
      MTLK_BFIELD_GET(mac_rx_info->u8HwInfo, HW_INFO_AGGR_LAST) != 0) /* Last in aggregation */
  {
    sta->info.rx_rate = mac_rx_info->u8RxRate;
    sta->info.scp     = MTLK_BFIELD_GET(mac_rx_info->u8HwInfo, HW_INFO_SCP);
  }
}

void __MTLK_IFUNC
mtlk_sta_update_fw_related_info(sta_entry *sta, const DEVICE_STATUS *fw_info);

static __INLINE void
mtlk_sta_set_cipher (sta_entry *sta, 
                     uint8      cipher)
{
  sta->info.cipher = cipher;
}

void __MTLK_IFUNC
mtlk_sta_set_packets_filter(sta_entry         *sta, 
                            mtlk_pckt_filter_e filter_type);


#ifdef MTCFG_RF_MANAGEMENT_MTLK
static __INLINE void
mtlk_sta_set_rf_mgmt_data (sta_entry *sta, uint8 rf_mgmt_data)
{
  sta->info.rf_mgmt_data = rf_mgmt_data;
}
#endif

void __MTLK_IFUNC
mtlk_sta_zero_rod_reply_counters(sta_entry *sta);

static __INLINE int __MTLK_IFUNC
mtlk_sta_process_bar (sta_entry *sta, uint8 tid, uint16 ssn)
{
  return mtlk_rod_process_bar(&sta->rod_queue[tid], ssn);
}

static __INLINE uint8 *
mtlk_sta_get_rsc (sta_entry *sta, uint8 tid)
{
  return sta->rod_queue[tid].rsc;
}

static __INLINE void __MTLK_IFUNC
mtlk_sta_on_security_negotiated(sta_entry *sta)
{
  MTLK_ASSERT(sta != NULL);
}

struct nic;

typedef struct  
{
  mtlk_handle_t   usr_data;
  BOOL            (__MTLK_IFUNC *sta_inactivity_on)(mtlk_handle_t    usr_data,
                                                    const sta_entry *sta);
  void            (__MTLK_IFUNC *on_sta_inactive)(mtlk_handle_t    usr_data,
                                                  const sta_entry *sta);
  void            (__MTLK_IFUNC *on_sta_keepalive)(mtlk_handle_t    usr_data,
                                                   sta_entry *sta);
} __MTLK_IDATA sta_db_wrap_api_t;

typedef struct _sta_db_cfg_t
{
  sta_db_wrap_api_t api;
  mtlk_addba_t     *addba;
  mtlk_sq_t        *sq;
  uint32            max_nof_stas;
  mtlk_wss_t       *parent_wss;
} __MTLK_IDATA sta_db_cfg_t;

typedef struct _sta_db
{
  mtlk_osal_timer_t    flush_timer;
  mtlk_osal_timer_t    iter_addba_timer;
  mtlk_hash_t          hash;
  mtlk_osal_spinlock_t lock;
  uint32               hash_cnt;
  uint32               sta_keepalive_timeout;
  uint32               keepalive_interval;
  uint32               aggr_open_threshold;
  sta_db_cfg_t         cfg;
  mtlk_vap_handle_t    vap_handle;
  mtlk_wss_t          *wss;
  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
} __MTLK_IDATA sta_db;

static __INLINE void 
mtlk_sta_reorder_packet (sta_entry *sta, uint8 tid, uint16 seq, mtlk_nbuf_t *nbuf)
{
  reordering_queue *rod_queue = &sta->rod_queue[tid];
  mtlk_rod_queue_set_last_rx_time(rod_queue, mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp()));
  mtlk_reorder_packet(rod_queue, mtlk_vap_get_core(sta->vap_handle), seq, nbuf);
}

int __MTLK_IFUNC
mtlk_stadb_init(sta_db *stadb, mtlk_vap_handle_t vap_handle);

int __MTLK_IFUNC
mtlk_stadb_start(sta_db *stadb, const sta_db_cfg_t *cfg);

void __MTLK_IFUNC
mtlk_stadb_stop(sta_db *stadb);

void __MTLK_IFUNC
mtlk_stadb_cleanup(sta_db *stadb);

sta_entry * __MTLK_IFUNC
mtlk_stadb_add_sta(sta_db *stadb, const unsigned char *mac,
                   BOOL dot11n_mode);
void __MTLK_IFUNC
mtlk_stadb_remove_sta(sta_db *stadb, sta_entry *sta);

#ifndef STA_REF_DBG
static __INLINE sta_entry *
mtlk_stadb_find_sta (sta_db *stadb, const unsigned char *mac)
{
  sta_entry                    *sta = NULL;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  mtlk_osal_lock_acquire(&stadb->lock);
  h = mtlk_hash_find_ieee_addr(&stadb->hash, (IEEE_ADDR *)mac);
  if (h) {
    sta = MTLK_CONTAINER_OF(h, sta_entry, hentry);
    mtlk_sta_incref(sta); /* Reference by caller */
  }
  mtlk_osal_lock_release(&stadb->lock);

  return sta;
}
#else
#define mtlk_stadb_find_sta(stadb, mac) \
  __mtlk_stadb_find_sta_dbg(__FUNCTION__, __LINE__, (stadb), (mac))

static __INLINE sta_entry *
__mtlk_stadb_find_sta_dbg (const char *f, int l, sta_db *stadb, const unsigned char *mac)
{
  sta_entry                    *sta = NULL;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;
    
  mtlk_osal_lock_acquire(&stadb->lock);
  h = mtlk_hash_find_ieee_addr(&stadb->hash, (IEEE_ADDR *)mac);
  if (h) {
    sta = MTLK_CONTAINER_OF(h, sta_entry, hentry);
    __mtlk_sta_incref_dbg(f, l, sta); /* Reference by caller */
  }
  mtlk_osal_lock_release(&stadb->lock);

  return sta;
}
#endif

static __INLINE sta_entry *
mtlk_stadb_get_ap (sta_db *stadb)
{
  sta_entry                    *sta = NULL;
  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  mtlk_osal_lock_acquire(&stadb->lock);
  h = mtlk_hash_enum_first_ieee_addr(&stadb->hash, &e);
  if (h) {
    sta = MTLK_CONTAINER_OF(h, sta_entry, hentry);
    mtlk_sta_incref(sta); /* Reference by caller */
  }
  mtlk_osal_lock_release(&stadb->lock);
  
  return sta;
}

typedef struct  
{
  sta_entry **arr;
  uint32      size;
  uint32      idx;
  sta_db     *stadb;
} __MTLK_IDATA mtlk_stadb_iterator_t;

struct _hst_db; /* TODO: replace it with Param DB/callbacks with opaque pointers */

typedef enum {
  STAT_ID_STADB = 1,
  STAT_ID_HSTDB
} __MTLK_IDATA mtlk_stadb_stat_id_e;

typedef struct {
  uint8             used;
  reordering_stats  reord_stat;
} __MTLK_IDATA mtlk_stadb_reordering_stat_t;

typedef struct {
  IEEE_ADDR addr;
  uint32    sta_rx_packets;
  uint32    sta_tx_packets;
  uint32    sta_rx_dropped;
  uint8     network_mode;
  uint16    tx_rate;
  mtlk_stadb_reordering_stat_t reordering_stats[NTS_TIDS];
} __MTLK_IDATA mtlk_stadb_general_stat_t;

typedef struct {
  IEEE_ADDR addr;
} __MTLK_IDATA mtlk_hstdb_stat_t;

typedef struct {
  mtlk_stadb_stat_id_e    type;
  union {
    mtlk_stadb_general_stat_t general_stat;
    mtlk_hstdb_stat_t hstdb_stat;
  } u;
}__MTLK_IDATA mtlk_stadb_stat_t;

int __MTLK_IFUNC
mtlk_stadb_get_stat(sta_db *stadb, struct _hst_db *hstdb, mtlk_clpb_t *clpb, uint8 group_cipher);

const sta_entry * __MTLK_IFUNC
mtlk_stadb_iterate_first(sta_db *stadb, mtlk_stadb_iterator_t *iter);
const sta_entry * __MTLK_IFUNC
mtlk_stadb_iterate_next(mtlk_stadb_iterator_t *iter);
void __MTLK_IFUNC
mtlk_stadb_iterate_done(mtlk_stadb_iterator_t *iter);

void __MTLK_IFUNC
mtlk_stadb_reset_cnts(sta_db *stadb);

typedef void (__MTLK_IFUNC * mtlk_stadb_disconnect_sta_clb_f)(mtlk_handle_t usr_ctx, const sta_entry *sta);

void __MTLK_IFUNC
mtlk_stadb_disconnect_all(sta_db *stadb,
    mtlk_stadb_disconnect_sta_clb_f clb,
    mtlk_handle_t usr_ctx,
    BOOL wait_all_packets_confirmed);

void __MTLK_IFUNC
mtlk_stadb_iterate_addba(sta_db *stadb);

BOOL __MTLK_IFUNC
mtlk_stadb_is_empty(sta_db *stadb);

typedef struct _hst_db
{
  mtlk_hash_t          hash;
  mtlk_osal_spinlock_t lock;
  uint32               hash_cnt;
  uint32               wds_host_timeout;
  mtlk_vap_handle_t    vap_handle;
  /* default host related */
  uint8                default_host[ETH_ALEN];
  uint8                local_mac[ETH_ALEN];
  MTLK_DECLARE_INIT_STATUS;
} __MTLK_IDATA hst_db;

int __MTLK_IFUNC
mtlk_hstdb_init(hst_db *hstdb, mtlk_vap_handle_t vap_handle);

void __MTLK_IFUNC
mtlk_hstdb_cleanup(hst_db *hstdb);

sta_entry * __MTLK_IFUNC
mtlk_hstdb_find_sta(hst_db* hstdb, const unsigned char *mac);

void __MTLK_IFUNC
mtlk_hstdb_update_host(hst_db *hstdb, const unsigned char *mac, 
                       sta_entry *sta);

void __MTLK_IFUNC
mtlk_hstdb_update_default_host(hst_db* hstdb, const unsigned char *mac);

int __MTLK_IFUNC
mtlk_hstdb_remove_all_by_sta(hst_db *hstdb, const sta_entry *sta);

static __INLINE uint8 *
mtlk_hstdb_get_default_host (hst_db* hstdb)
{
  if (mtlk_osal_is_valid_ether_addr(hstdb->default_host))
    return hstdb->default_host;
  return NULL;
}

int __MTLK_IFUNC mtlk_hstdb_set_local_mac(hst_db              *hstdb,
                                          const unsigned char *mac);
void __MTLK_IFUNC mtlk_hstdb_get_local_mac(hst_db        *hstdb,
                                           unsigned char *mac);

typedef struct  
{
  IEEE_ADDR *addr;
  uint32     size;
  uint32     idx;
} __MTLK_IDATA mtlk_hstdb_iterator_t;

const IEEE_ADDR * __MTLK_IFUNC
mtlk_hstdb_iterate_first(hst_db *hstdb, const sta_entry *sta, mtlk_hstdb_iterator_t *iter);
const IEEE_ADDR * __MTLK_IFUNC
mtlk_hstdb_iterate_next(mtlk_hstdb_iterator_t *iter);
void __MTLK_IFUNC
mtlk_hstdb_iterate_done(mtlk_hstdb_iterator_t *iter);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif /* !_MTLK_STADB_H_ */
