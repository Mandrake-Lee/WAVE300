/*
 * $Id: core.h 12533 2012-02-02 11:24:00Z nayshtut $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Core module definitions
 *
 */
#ifndef __CORE_H__
#define __CORE_H__

/** 
*\file core.h 
*\brief Core module acting as mediator to organize all driver activities
*/

#include "mtlk_vap_manager.h"

struct nic;

#include "mtlkmib.h"

#include "addba.h"
#include "mcast.h"
#include "mtlkqos.h"


#include "mtlkflctrl.h"
#include "aocs.h"
#include "dfs.h"
#include "rod.h"
#include "stadb.h"
#ifdef MTCFG_RF_MANAGEMENT_MTLK
#include "mtlk_rfmgmt.h"
#endif
#include "mtlk_serializer.h"

#include "mtlk_core_iface.h"

#include "l2nat.h"
#include "mtlk_df.h"
#include "mtlk_coc.h"
#include "scan.h"
#include "channels.h"
#include "mib_osdep.h"
#include "core_priv.h"
#include "mtlk_reflim.h"

#include "mtlk_wss.h"

// the sane amount of time dedicated to MAC to perform
// connection or BSS activation
#define CONNECT_TIMEOUT 10000 /* msec */
#define ASSOCIATE_FAILURE_TIMEOUT 3000 /* msec */

#define INVALID_DEACTIVATE_TIMESTAMP ((uint32)-1)

// amount of time - needed by firmware to send vap removal
// indication to the driver.
#define VAP_REMOVAL_TIMEOUT 10000 /* msec */
enum ts_priorities {
  TS_PRIORITY_BE,
  TS_PRIORITY_BG,
  TS_PRIORITY_VIDEO,
  TS_PRIORITY_VOICE,
  TS_PRIORITY_LAST
};

/***************************************************/

typedef struct _wme_class_cfg_t
{
  uint32 cwmin;
  uint32 cwmax;
  uint32 aifsn;
  uint32 txop;
} wme_class_cfg_t;

typedef struct _wme_cfg_t
{
  wme_class_cfg_t wme_class[NTS_PRIORITIES];
} wme_cfg_t;

typedef struct _mtlk_core_cfg_t
{
  mtlk_addba_cfg_t              addba;
  wme_cfg_t                     wme_bss;
  wme_cfg_t                     wme_ap;
  BOOL                          is_hidden_ssid;
} mtlk_core_cfg_t;

#define LOG_LOCAL_GID   GID_CORE
#define LOG_LOCAL_FID   0

typedef enum
{
  MTLK_CORE_CNT_TX_PACKETS_DISCARDED_FW,
  MTLK_CORE_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD,
  MTLK_CORE_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE,
  MTLK_CORE_CNT_PACKETS_SENT,
  MTLK_CORE_CNT_TX_PACKETS_DISCARDED_NO_PEERS,
  MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_ACM,
  MTLK_CORE_CNT_TX_PACKETS_DISCARDED_EAPOL_CLONED,
  MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_DIRECTED,
  MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_MCAST,
  MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES,
  MTLK_CORE_CNT_TX_PACKETS_DISCARDED_SQ_OVERFLOW,
  MTLK_CORE_CNT_TX_PACKETS_DISCARDED_EAPOL_FILTER,
  MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DROP_ALL_FILTER,
  MTLK_CORE_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW,
  MTLK_CORE_CNT_PACKETS_RECEIVED,
  MTLK_CORE_CNT_BYTES_RECEIVED,
  MTLK_CORE_CNT_BYTES_SENT,
  MTLK_CORE_CNT_PAIRWISE_MIC_FAILURE_PACKETS,
  MTLK_CORE_CNT_GROUP_MIC_FAILURE_PACKETS,
  MTLK_CORE_CNT_UNICAST_REPLAYED_PACKETS,
  MTLK_CORE_CNT_MULTICAST_REPLAYED_PACKETS,
  MTLK_CORE_CNT_FWD_RX_PACKETS,
  MTLK_CORE_CNT_FWD_RX_BYTES,
  MTLK_CORE_CNT_UNICAST_PACKETS_SENT,
  MTLK_CORE_CNT_UNICAST_PACKETS_RECEIVED,
  MTLK_CORE_CNT_MULTICAST_PACKETS_SENT,
  MTLK_CORE_CNT_MULTICAST_PACKETS_RECEIVED,
  MTLK_CORE_CNT_BROADCAST_PACKETS_SENT,
  MTLK_CORE_CNT_BROADCAST_PACKETS_RECEIVED,
  MTLK_CORE_CNT_MULTICAST_BYTES_SENT,
  MTLK_CORE_CNT_MULTICAST_BYTES_RECEIVED,
  MTLK_CORE_CNT_BROADCAST_BYTES_SENT,
  MTLK_CORE_CNT_BROADCAST_BYTES_RECEIVED,
  MTLK_CORE_CNT_DAT_FRAMES_RECEIVED,
  MTLK_CORE_CNT_CTL_FRAMES_RECEIVED,
  MTLK_CORE_CNT_MAN_FRAMES_RECEIVED,
  MTLK_CORE_CNT_COEX_EL_RECEIVED,
  MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_REQUESTED,
  MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_GRANTED,
  MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED,
  MTLK_CORE_CNT_CHANNEL_SWITCH_20_TO_40,
  MTLK_CORE_CNT_CHANNEL_SWITCH_40_TO_20,
  MTLK_CORE_CNT_CHANNEL_SWITCH_40_TO_40,
  MTLK_CORE_CNT_AGGR_ACTIVE,
  MTLK_CORE_CNT_REORD_ACTIVE,
  MTLK_CORE_CNT_SQ_DPCS_SCHEDULED,
  MTLK_CORE_CNT_SQ_DPCS_ARRIVED,
  MTLK_CORE_CNT_LAST
} mtlk_core_wss_cnt_id_e;

// private statistic counters
struct priv_stats {
  // TX consecutive dropped packets counter
  uint32 tx_cons_drop_cnt;

  // Maximum number of packets dropped consecutively
  uint32 tx_max_cons_drop;

  // Applicable only to STA:
  uint32 sta_session_rx_packets; // Packets received in this session
  uint32 sta_session_tx_packets; // Packets transmitted in this session

  // Dropped Tx packets counters per priority queue
  uint32 ac_dropped_counter[NTS_PRIORITIES];
  uint32 ac_rx_counter[NTS_PRIORITIES];
  uint32 ac_tx_counter[NTS_PRIORITIES];

  // AP forwarding statistics
  uint32 fwd_tx_packets;
  uint32 fwd_tx_bytes;
  uint32 fwd_dropped;

  // Reliable Multicast statistics
  uint32 rmcast_dropped;

  // Used Tx packets per priority queue
  uint32 ac_used_counter[NTS_PRIORITIES];

  // Received BAR frames
  uint32 bars_cnt;

  //trasmitted broadcast/non-reliable multicast packets
  uint32 tx_bcast_nrmcast;

  //number of disconnections
  uint32 num_disconnects;

  /* number of Rx : Wrong nwid/essid */
  uint32 discard_nwi;

  /* Missed beacons/superframe */
  uint32 missed_beacon;

  unsigned long tx_overruns;      /*!< total tx queue overruns */
};

typedef struct {
  uint32 stat[STAT_TOTAL_NUMBER];
} mtlk_mac_stats_t;

/* core to DF UI interface*/
typedef struct _mtlk_core_general_stats_t {
  struct priv_stats   core_priv_stats;
  mtlk_mac_stats_t mac_stat;
  uint32  tx_msdus_free;
  uint32  tx_msdus_usage_peak;
  uint32  bist_check_passed;
  uint16  net_state;
  uint8   max_rssi;
  uint8   noise;
  uint8   channel_load;
  unsigned char bssid[ETH_ALEN];
  uint32  txmm_sent;
  uint32  txmm_cfmd;
  uint32  txmm_peak;
  uint32  txdm_sent;
  uint32  txdm_cfmd;
  uint32  txdm_peak;
  uint32  fw_logger_packets_processed;
  uint32  fw_logger_packets_dropped;

  uint32  tx_packets;       /*!< total packets transmitted */
  uint32  tx_bytes;         /*!< total bytes transmitted */
  uint32  rx_packets;       /*!< total packets received */
  uint32  rx_bytes;         /*!< total bytes received */

  uint32  pairwise_mic_failure_packets;
  uint32  group_mic_failure_packets;
  uint32  unicast_replayed_packets;
  uint32  multicast_replayed_packets;

  uint32  fwd_rx_packets;
  uint32  fwd_rx_bytes;

  // Received data, control and management 802.11 frames from MAC
  uint32 rx_dat_frames;
  uint32 rx_ctl_frames;
  uint32 rx_man_frames;
} mtlk_core_general_stats_t;

struct nic;

struct nic_slow_ctx {
  struct nic *nic;
  sta_db stadb;
  hst_db hstdb;

  // configuration
  mtlk_core_cfg_t cfg;

  tx_limit_t  tx_limits;

  /* user configured bonding - used for manual channel selection */
  uint8 bonding;
  // EEPROM data
  mtlk_eeprom_data_t* ee_data;

  struct mtlk_scan   scan;
  scan_cache_t       cache; 
  /* spectrum of the last loaded programming model */
  uint8              last_pm_spectrum;
  /* frequency of the last loaded programming model */
  uint8              last_pm_freq;

  // ADDBA-related
  mtlk_addba_t      addba;
  mtlk_reflim_t     addba_lim_reord;
  mtlk_reflim_t     addba_lim_aggr;

  /*AP - always 11h only
    STA - always 11h, if dot11d_active is set, use 11d table
  */
  //11h-related
  mtlk_dot11h_t* dot11h;

  /* aocs-related */
  mtlk_aocs_t *aocs;

  // 802.11i (security) stuff
  UMI_RSN_IE rsnie;
  uint8 default_key;
  uint8 wep_enabled;
  MIB_WEP_DEF_KEYS wep_keys;
  uint8 wps_in_progress;

  mtlk_aux_pm_related_params_t pm_params;
  // features
  uint8 is_tkip;

  mtlk_coc_t  *coc_mngmt;

#ifdef MTLK_DEBUG_CHARIOT_OOO
  uint16 seq_prev_sent[NTS_PRIORITIES];
#endif

  mtlk_osal_timer_t mac_watchdog_timer;
  uint8 channel_load;
  uint8 noise;
  /* ACL white/black list */
  IEEE_ADDR acl[MAX_ADDRESSES_IN_ACL];
  IEEE_ADDR acl_mask[MAX_ADDRESSES_IN_ACL];

  // This event arises when MAC sends either UMI_CONNECTED (STA)
  // or UMI_BSS_CREATED (AP)
  // Thread, that performs connection/bss_creation, waits for this event before returning.
  // If no such event arises - connect/bss_create process has failed and error
  // is reported to the caller.
  mtlk_osal_event_t connect_event;

  // This event arises when MAC sends MC_MAN_VAP_WAS_REMOVED_IND
  // Thread, that performs VAP deactivation, waits for this event
  mtlk_osal_event_t vap_removed_event;

  mtlk_serializer_t serializer;

  mtlk_irbd_handle_t *stat_irb_handle;

  int mac_stuck_detected_by_sw;

  uint32 deactivate_ts;

  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
};

struct _mtlk_20_40_coexistence_sm;

struct nic {
  mtlk_l2nat_t l2nat;
  struct nic_slow_ctx *slow_ctx;

  struct priv_stats pstats;

  /* MBSS instance ID  - it is assigned by the hypervisor and should be used when communicating with HAL*/
  mtlk_vap_handle_t   vap_handle;

  // reliable multicast context
  mcast_ctx mcast;

  int net_state;

  mtlk_osal_spinlock_t net_state_lock;
  BOOL  is_stopping;
  BOOL  is_iface_stopping;
  BOOL  is_stopped;
  BOOL  aocs_started;

  // 802.11i (security) stuff
  u8 group_cipher;
  u8 group_rsc[4][6]; // Replay Sequence Counters per key index

  /* Flow control object, singleton in Master core */
  mtlk_flctrl_t *hw_tx_flctrl;

  /* Flow control id, per-core */
  mtlk_handle_t flctrl_id;

  /* send queue struct for shared packet scheduler */
  struct _mtlk_sq_t         *sq;

  /* tasklet for "flushing" shared SendQueue on wake */
  struct tasklet_struct     *sq_flush_tasklet;

  /* bands which have already been CB-scanned */
  uint8                     cb_scanned_bands;
#define CB_SCANNED_2_4  0x1
#define CB_SCANNED_5_2  0x2

  struct mtlk_qos           qos;

  /* Should be set to activation result (Failed\Succeeded) before triggering nic->slow_ctx->connect_event*/
  BOOL activation_status;

#ifdef MTCFG_RF_MANAGEMENT_MTLK
  mtlk_rf_mgmt_t           *rf_mgmt;
#endif
  mtlk_txmm_msg_t           txmm_async_eeprom_msgs[MAX_NUM_TX_ANTENNAS]; /* must be moved to EEPROM module ASAP */

  mtlk_core_hot_path_param_handles_t  pdb_hot_path_handles;

  mtlk_wss_t               *wss;
  mtlk_wss_cntr_handle_t   *wss_hcntrs[MTLK_CORE_CNT_LAST];

  /* 20/40 state machine */
  struct _mtlk_20_40_coexistence_sm *coex_20_40_sm;

  mtlk_txmm_msg_t                     aux_20_40_msg;

  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_INIT_LOOP(TXMM_EEPROM_ASYNC_MSGS_INIT);
  MTLK_DECLARE_START_STATUS;
};

typedef enum
  {
    CFG_INFRA_STATION,
    CFG_ADHOC_STATION,
    CFG_ACCESS_POINT,
    CFG_TEST_MAC,
    CFG_NUM_NET_TYPES
  } CFG_NETWORK_TYPE;

enum bridge_mode {
  BR_MODE_NONE        = 0,
  BR_MODE_WDS         = 1,
  BR_MODE_L2NAT       = 2,
  BR_MODE_MAC_CLONING = 3,
  BR_MODE_LAST
};

int mtlk_xmit (mtlk_core_t* core, struct sk_buff *skb);
void mtlk_record_xmit_err (struct nic *nic, struct sk_buff *skb);

int mtlk_detect_replay_or_sendup(mtlk_core_t* core, struct sk_buff *skb, u8 *rsn);

int mtlk_set_hw_state(mtlk_core_t *nic, int st);

mtlk_handle_t __MTLK_IFUNC mtlk_core_get_tx_limits_handle(mtlk_handle_t nic);

/* Size of this structure must be multiple of sizeof(void*) because   */
/* it is immediately followed by data of the request, and the data,   */
/* generally speaking, must be sizeof(void*)-aligned.                 */
/* There is corresponding assertion in the code.                      */
typedef struct __core_async_exec_t
{
  MTLK_DECLARE_OBJPOOL_CTX(objpool_ctx);

  mtlk_command_t        cmd;
  mtlk_handle_t         receiver;
  uint32                data_size;
  mtlk_core_task_func_t func;
  mtlk_user_request_t  *user_req;
  mtlk_vap_handle_t     vap_handle;
  mtlk_ability_id_t     ability_id;
} _core_async_exec_t;


static __INLINE void
mtlk_core_inc_cnt (mtlk_core_t       *core,
                   mtlk_core_wss_cnt_id_e cnt_id)
{
  MTLK_ASSERT(cnt_id >= 0 && cnt_id < MTLK_CORE_CNT_LAST);

  mtlk_wss_cntr_inc(core->wss_hcntrs[cnt_id]);
}

static __INLINE void
mtlk_core_add_cnt (mtlk_core_t       *core,
                   mtlk_core_wss_cnt_id_e cnt_id,
                   uint32            val)
{
  MTLK_ASSERT(cnt_id >= 0 && cnt_id < MTLK_CORE_CNT_LAST);

  mtlk_wss_cntr_add(core->wss_hcntrs[cnt_id], val);
}


static __INLINE uint8 
mtlk_core_get_last_pm_spectrum(struct nic *nic)
{
  return nic->slow_ctx->last_pm_spectrum;
}

static __INLINE uint8
mtlk_core_get_last_pm_freq(struct nic *nic)
{
  return nic->slow_ctx->last_pm_freq;
}

static __INLINE BOOL
mtlk_core_scan_is_running(struct nic *nic)
{
  if (mtlk_scan_is_initialized(&nic->slow_ctx->scan)) {
    return mtlk_scan_is_running(&nic->slow_ctx->scan);
  }
  return FALSE;
}

mtlk_core_t * __MTLK_IFUNC
mtlk_core_get_master(mtlk_core_t *core);

static __INLINE struct _mtlk_20_40_coexistence_sm *
  mtlk_core_get_coex_sm(mtlk_core_t *core)
{
  return mtlk_core_get_master(core)->coex_20_40_sm;
}

static __INLINE void mtlk_core_set_coex_sm(mtlk_core_t *core, struct _mtlk_20_40_coexistence_sm *coex_sm)
{
  MTLK_ASSERT(mtlk_vap_is_master(core->vap_handle));
  core->coex_20_40_sm = coex_sm;
}

tx_limit_t* __MTLK_IFUNC
mtlk_core_get_tx_limits(mtlk_core_t *core);

int __MTLK_IFUNC mtlk_core_on_channel_switch_done(mtlk_vap_handle_t vap_handle,
                                                  uint16            primary_channel,
                                                  uint8             secondary_channel_offset,
                                                  uint16            reason);
#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif

