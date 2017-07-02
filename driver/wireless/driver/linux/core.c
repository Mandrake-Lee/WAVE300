
/*
 * $Id: core.c 12816 2012-03-06 12:48:15Z hatinecs $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Core functionality
 *
 */
#include "mtlkinc.h"

#include "core_priv.h"
#include "core.h"
#include "mtlkhal.h"
#include "mtlk_coreui.h"
#include "drvver.h"
#include "mhi_mac_event.h"
#include "mtlk_packets.h"
#include "mtlkparams.h"
#include "nlmsgs.h"
#include "aocs.h"
#include "aocshistory.h"
#include "sq.h"
#include "mtlk_snprintf.h"
#include "eeprom.h"
#include "dfs.h"
#include "bitrate.h"
#include "mtlk_fast_mem.h"
#include "mtlk_gpl_helper.h"
#include "wpa.h"
#include "mtlkaux.h"
#include "mtlk_param_db.h"
#include "mtlkwssa_drvinfo.h"
#include "coex20_40.h"
#include "mtlk_wssd.h"

#define DEFAULT_NUM_TX_ANTENNAS NUM_TX_ANTENNAS_GEN3
#define DEFAULT_NUM_RX_ANTENNAS (3)

#define DEFAULT_MAX_STAs_SUPPORTED (32)
#define DEFAULT_MAX_VAPs_SUPPORTED (5)

#define LOG_LOCAL_GID   GID_CORE
#define LOG_LOCAL_FID   4

#ifdef MTCFG_DEBUG
#define AOCS_DEBUG
#endif

/* acessors for configuration data passed from DF to Core */
#define MTLK_CFG_START_CHEK_ITEM_AND_CALL() do{
#define MTLK_CFG_END_CHEK_ITEM_AND_CALL() }while(0);

#define MTLK_CFG_CHECK_ITEM_AND_CALL(obj,name,func,func_args,func_res) \
  if(1==(obj)->name##_filled){func_res=func func_args;if(MTLK_ERR_OK != func_res)break;}

#define MTLK_CFG_CHECK_ITEM_AND_CALL_VOID MTLK_CFG_GET_ITEM_BY_FUNC_VOID

#define MTLK_CFG_CHECK_ITEM_AND_CALL_EX(obj,name,func,func_args,func_res,etalon_res) \
  if(1==(obj)->name##_filled) {\
    func_res=func func_args;\
    if(etalon_res != func_res) {\
      func_res = MTLK_ERR_UNKNOWN;\
      break;\
    }else {\
      func_res=MTLK_ERR_OK;\
    }\
  }

#define MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(obj,name,func,mibid,retdata,core) \
  if (!mtlk_mib_request_is_allowed(core->vap_handle, mibid)) { \
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(obj, name, func,(mtlk_vap_get_txmm(mtlk_core_get_master(core)->vap_handle), mibid,(retdata))); \
  } else { \
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(obj, name, func,(mtlk_vap_get_txmm(core->vap_handle), mibid,(retdata))); \
  } \
  ILOG2_DDDD("CID-%04x: Read Mib 0x%x, Val 0x%x, Vap 0x%x", mtlk_vap_get_oid(core->vap_handle), mibid, *(retdata), mtlk_vap_get_id(core->vap_handle));

static int
_mtlk_core_on_peer_disconnect(mtlk_core_t *core,
                             sta_entry   *sta,
                             uint16       reason);

typedef enum __mtlk_core_async_priorities_t
{
  _MTLK_CORE_PRIORITY_MAINTENANCE = 0,
  _MTLK_CORE_PRIORITY_NETWORK,
  _MTLK_CORE_PRIORITY_INTERNAL,
  _MTLK_CORE_PRIORITY_USER,
  _MTLK_CORE_NUM_PRIORITIES
} _mtlk_core_async_priorities_t;

#define DEFAULT_TX_POWER        "17"
#define SCAN_CACHE_AGEING (3600) /* 1 hour */

static const IEEE_ADDR EMPTY_MAC_ADDR = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
static const IEEE_ADDR EMPTY_MAC_MASK = { {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} };

#define STRING_VERSION_SIZE 1024
char mtlk_version_string[STRING_VERSION_SIZE] = "Driver version: " DRV_VERSION "\nMAC/PHY versions:\n";

/* driver is halted - in case if was not initialized yet
   or after critical error */
#define NET_STATE_HALTED         (1 << 0)
/* driver is initializing */
#define NET_STATE_IDLE           (1 << 1)
/* driver has been initialized */
#define NET_STATE_READY          (1 << 2)
/* activation request was sent - waiting for CFM */
#define NET_STATE_ACTIVATING     (1 << 3)
/* got connection event */
#define NET_STATE_CONNECTED      (1 << 4)
/* disconnect started */
#define NET_STATE_DISCONNECTING  (1 << 5)

static const uint32 _mtlk_core_wss_id_map[] =
{
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_FW,                                  /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_FW                                         */
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_TOO_OLD,                         /* MTLK_CORE_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD */
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_DUPLICATE,                       /* MTLK_CORE_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE */
  MTLK_WWSS_WLAN_STAT_ID_PACKETS_SENT,                                             /* MTLK_CORE_CNT_PACKETS_SENT                                                    */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_NO_PEERS,                        /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_NO_PEERS                                   */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_ACM,                             /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_ACM                                    */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_EAPOL_CLONED,                    /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_EAPOL_CLONED                               */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_DIRECTED,    /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_DIRECTED           */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_MCAST,       /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_MCAST              */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES,                    /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES                           */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_SQ_OVERFLOW,                     /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_SQ_OVERFLOW                                */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_EAPOL_FILTER,                    /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_EAPOL_FILTER                               */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_DROP_ALL_FILTER,                 /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DROP_ALL_FILTER                            */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_TX_QUEUE_OVERFLOW,               /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW                          */
  MTLK_WWSS_WLAN_STAT_ID_PACKETS_RECEIVED,                                         /* MTLK_CORE_CNT_PACKETS_RECEIVED                                                */
  MTLK_WWSS_WLAN_STAT_ID_BYTES_RECEIVED,                                           /* MTLK_CORE_CNT_BYTES_RECEIVED                                                  */
  MTLK_WWSS_WLAN_STAT_ID_BYTES_SENT,                                                /* MTLK_CORE_CNT_BYTES_SENT                                                      */
  MTLK_WWSS_WLAN_STAT_ID_PAIRWISE_MIC_FAILURE_PACKETS,                              /* MTLK_CORE_CNT_PAIRWISE_MIC_FAILURE_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_GROUP_MIC_FAILURE_PACKETS,                                 /* MTLK_CORE_CNT_GROUP_MIC_FAILURE_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_REPLAYED_PACKETS,                                  /* MTLK_CORE_CNT_UNICAST_REPLAYED_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_REPLAYED_PACKETS,                                /* MTLK_CORE_CNT_MULTICAST_REPLAYED_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_FWD_RX_PACKETS,                                            /* MTLK_CORE_CNT_FWD_RX_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_FWD_RX_BYTES,                                              /* MTLK_CORE_CNT_FWD_RX_BYTES */
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_PACKETS_SENT,                                      /* MTLK_CORE_CNT_UNICAST_PACKETS_SENT */
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_PACKETS_RECEIVED,                                  /* MTLK_CORE_CNT_UNICAST_PACKETS_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_PACKETS_SENT,                                    /* MTLK_CORE_CNT_MULTICAST_PACKETS_SENT */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_PACKETS_RECEIVED,                                /* MTLK_CORE_CNT_MULTICAST_PACKETS_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_PACKETS_SENT,                                    /* MTLK_CORE_CNT_BROADCAST_PACKETS_SENT */
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_PACKETS_RECEIVED,                                /* MTLK_CORE_CNT_BROADCAST_PACKETS_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_BYTES_SENT,                                      /* MTLK_CORE_CNT_MULTICAST_BYTES_SENT */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_BYTES_RECEIVED,                                  /* MTLK_CORE_CNT_MULTICAST_BYTES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_BYTES_SENT,                                      /* MTLK_CORE_CNT_BROADCAST_BYTES_SENT */
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_BYTES_RECEIVED,                                  /* MTLK_CORE_CNT_BROADCAST_BYTES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_DAT_FRAMES_RECEIVED,                                       /* MTLK_CORE_CNT_DAT_FRAMES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_CTL_FRAMES_RECEIVED,                                       /* MTLK_CORE_CNT_CTL_FRAMES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_MAN_FRAMES_RECEIVED,                                       /* MTLK_CORE_CNT_MAN_FRAMES_RECEIVED */

  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_RECEIVED,                                     /* MTLK_CORE_CNT_COEX_EL_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_REQUESTED,                      /* MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_REQUESTED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_GRANTED,                        /* MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_GRANTED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED,                /* MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_20_TO_40,                               /* MTLK_CORE_CNT_CHANNEL_SWITCH_20_TO_40 */
  MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_40_TO_20,                               /* MTLK_CORE_CNT_CHANNEL_SWITCH_40_TO_20 */
  MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_40_TO_40,                               /* MTLK_CORE_CNT_CHANNEL_SWITCH_40_TO_40 */

  MTLK_WWSS_WLAN_STAT_ID_AGGR_ACTIVE,                                               /* MTLK_CORE_CNT_AGGR_ACTIVE  */
  MTLK_WWSS_WLAN_STAT_ID_REORD_ACTIVE,                                              /* MTLK_CORE_CNT_REORD_ACTIVE */
  MTLK_WWSS_WLAN_STAT_ID_SQ_DPCS_SCHEDULED,                                         /* MTLK_CORE_CNT_SQ_DPCS_SCHEDULED */
  MTLK_WWSS_WLAN_STAT_ID_SQ_DPCS_ARRIVED,                                           /* MTLK_CORE_CNT_SQ_DPCS_ARRIVED */
};

/* API between Core and HW */
static int
_mtlk_core_start (mtlk_vap_handle_t vap_handle);
static int
_mtlk_core_release_tx_data (mtlk_vap_handle_t vap_handle, const mtlk_core_release_tx_data_t *data);
static int
_mtlk_core_handle_rx_data (mtlk_vap_handle_t vap_handle, mtlk_core_handle_rx_data_t *data);
static void
_mtlk_core_handle_rx_ctrl (mtlk_vap_handle_t vap_handle, uint32 id, void *payload, uint32 payload_buffer_size);
static int
_mtlk_core_get_prop (mtlk_vap_handle_t vap_handle, mtlk_core_prop_e prop_id, void* buffer, uint32 size);
static int
_mtlk_core_set_prop (mtlk_vap_handle_t vap_handle, mtlk_core_prop_e prop_id, void *buffer, uint32 size);
static void
_mtlk_core_stop (mtlk_vap_handle_t vap_handle);
static void
_mtlk_core_prepare_stop(mtlk_vap_handle_t vap_handle);

static mtlk_core_vft_t const core_vft = {
  _mtlk_core_start,
  _mtlk_core_release_tx_data,
  _mtlk_core_handle_rx_data,
  _mtlk_core_handle_rx_ctrl,
  _mtlk_core_get_prop,
  _mtlk_core_set_prop,
  _mtlk_core_stop,
  _mtlk_core_prepare_stop
};

/* API between Core and DF UI */
static int  __MTLK_IFUNC
_mtlk_core_get_ant_gain(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_aocs_table(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_aocs_history(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_aocs_channels(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_aocs_penalties(mtlk_handle_t hcore, const void* data, uint32 data_size);
#ifdef AOCS_DEBUG
static int __MTLK_IFUNC
_mtlk_core_get_aocs_debug_update_cl(mtlk_handle_t hcore, const void* data, uint32 data_size);
#endif /* AOCS_DEBUG */
static int __MTLK_IFUNC
_mtlk_core_get_hw_limits(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_reg_limits(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_ee_caps(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int
_mtlk_core_get_stadb_sta_list(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_l2nat_stats(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_sq_status(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_set_mac_assert(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_mc_igmp_tbl(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_bcl_mac_data_get (mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_bcl_mac_data_set (mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_range_info_get (mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_connect_sta(mtlk_handle_t hcore, const void* data, uint32 data_size);

static int __MTLK_IFUNC
handleDisconnectMe(mtlk_handle_t core_object, const void *payload, uint32 size);
static int __MTLK_IFUNC
_mtlk_core_ap_disconnect_sta(mtlk_handle_t hcore, const void* data, uint32 data_size);

static int __MTLK_IFUNC
_mtlk_core_start_scanning(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_scanning_res(mtlk_handle_t hcore, const void* data, uint32 data_size);

static int __MTLK_IFUNC
_mtlk_core_set_wep_enc_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_wep_enc_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_set_auth_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_auth_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_set_genie_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_enc_ext_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_set_enc_ext_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);

static int
_mtlk_core_get_status(mtlk_handle_t hcore, const void* data, uint32 data_size);

/* Core utilities */
static void
_mtlk_core_bswap_bcl_request(UMI_BCL_REQUEST *req, BOOL hdr_only);
static int
_mtlk_core_set_channel(mtlk_core_t *core, uint16 channel);
static void
mtlk_core_configuration_dump(mtlk_core_t *core);
static int
mtlk_core_set_acl(struct nic *nic, IEEE_ADDR *mac, IEEE_ADDR *mac_mask);
static int
mtlk_core_del_acl(struct nic *nic, IEEE_ADDR *mac);
static int
mtlk_core_set_gen_ie(struct nic *nic, u8 *ie, u16 ie_len, u8 ie_type);
static int
mtlk_core_set_bonding(mtlk_core_t *core, uint8 bonding);
static uint32
mtlk_core_get_available_bitrates (struct nic *nic);
static int
mtlk_core_update_network_mode(mtlk_core_t* nic, uint8 net_mode);


static void
_mtlk_core_country_code_set_default(mtlk_core_t* core);
static void
_mtlk_core_sta_country_code_update_on_connect(mtlk_core_t* core, uint8 country_code);
static void
_mtlk_core_sta_country_code_set_default_on_activate(mtlk_core_t* core);
static int
_mtlk_core_set_country_from_ui(mtlk_core_t *core, char *val);
static int
  _mtlk_core_set_is_dot11d(mtlk_core_t *core, BOOL is_dot11d);
static void
  _mtlk_core_switch_cb_mode_stage1_callback(mtlk_handle_t context, mtlk_get_channel_data_t *channel_data, FREQUENCY_ELEMENT *mode_change_params);
static void 
  _mtlk_core_switch_cb_mode_stage2_callback(mtlk_handle_t context, FREQUENCY_ELEMENT *mode_change_params);
static void
  _mtlk_core_send_ce_callback(mtlk_handle_t context, UMI_COEX_EL *coexistence_element);
static void
  _mtlk_core_send_cmf_callback(mtlk_handle_t context, const IEEE_ADDR *sta_addr, const UMI_COEX_FRAME *coexistence_frame);
static int
  _mtlk_core_scan_async_callback(mtlk_handle_t context, uint8 band, const char* essid);
static void
  _mtlk_core_scan_set_bg_callback(mtlk_handle_t context, BOOL is_background);
static int
  _mtlk_core_scan_register_obss_cb_callback(mtlk_handle_t context, obss_scan_report_callback_type *callback);
static int _mtlk_core_enumerate_external_intolerance_info_callback(mtlk_handle_t caller_context,
  mtlk_handle_t core_context, external_intolerance_enumerator_callback_type callback, uint32 expiration_time);
static int _mtlk_core_ability_control_callback(mtlk_handle_t context,
  eABILITY_OPS operation, const uint32* ab_id_list, uint32 ab_id_num);
static uint8 _mtlk_core_get_reg_domain_callback(mtlk_handle_t context);
static uint16 _mtlk_core_get_cur_channels_callback(mtlk_handle_t context,
  int *secondary_channel_offset);
static int _mtlk_core_create_20_40(struct nic* nic);
static void _mtlk_core_delete_20_40(struct nic* nic);
static BOOL _mtlk_core_is_20_40_active(struct nic* nic);
static void _mtlk_core_notify_ap_of_station_connection(struct nic *nic, const IEEE_ADDR  *addr, const UMI_RSN_IE *rsn_ie,
  BOOL supports_20_40, BOOL received_scan_exemption, BOOL is_intolerant, BOOL is_legacy);
static void _mtlk_core_notify_ap_of_station_disconnection(struct nic *nic, const IEEE_ADDR  *addr);
static int _mtlk_core_set_wep (struct nic *nic, int wep_enabled);


static __INLINE uint32
_mtlk_core_get_cnt (mtlk_core_t       *core,
                    mtlk_core_wss_cnt_id_e cnt_id)
{
  MTLK_ASSERT(cnt_id >= 0 && cnt_id < MTLK_CORE_CNT_LAST);

  return mtlk_wss_get_stat(core->wss, cnt_id);
}

static __INLINE void
_mtlk_core_on_mic_failure (mtlk_core_t       *core,
                           mtlk_df_ui_mic_fail_type_t mic_fail_type)
{
  MTLK_ASSERT((MIC_FAIL_PAIRWISE == mic_fail_type) || (MIC_FAIL_GROUP== mic_fail_type));
  switch(mic_fail_type) {
  case MIC_FAIL_PAIRWISE:
    mtlk_core_inc_cnt(core, MTLK_CORE_CNT_PAIRWISE_MIC_FAILURE_PACKETS);
    break;
  case MIC_FAIL_GROUP:
    mtlk_core_inc_cnt(core, MTLK_CORE_CNT_GROUP_MIC_FAILURE_PACKETS);
    break;
  default:
    WLOG_V("Wrong type of pairwise packet");
    break;
  }
}

static __INLINE BOOL
_mtlk_core_has_connections(mtlk_core_t *core)
{
  return !mtlk_stadb_is_empty(&core->slow_ctx->stadb);
};

/* ======================================================*/
/* Core internal wrapper for asynchronous execution.     */
/* Uses serializer, command can't be tracked/canceled,   */
/* allocated on heap and deleted by completion callback. */
static void 
_mtlk_core_async_clb(mtlk_handle_t user_context)
{
  int res = MTLK_ERR_BUSY;
  _core_async_exec_t *ctx = (_core_async_exec_t *) user_context;

  if (_mtlk_abmgr_is_ability_enabled(mtlk_vap_get_abmgr(ctx->vap_handle),
                                     ctx->ability_id))
  {
    res = ctx->func(ctx->receiver, &ctx[1], ctx->data_size);
  }
  else
  {
    WLOG_D("Requested ability 0x%X is disabled or never was registered", ctx->ability_id);
  }

  if(NULL != ctx->user_req)
    mtlk_df_ui_req_complete(ctx->user_req, res);
}

static void 
_mtlk_core_async_compl_clb(serializer_result_t res,
                           mtlk_command_t* command, 
                           mtlk_handle_t completion_ctx)
{
  _core_async_exec_t *ctx = (_core_async_exec_t *) completion_ctx;

  mtlk_command_cleanup(&ctx->cmd);
  mtlk_osal_mem_free(ctx);
}

static int
_mtlk_core_execute_async_ex (struct nic *nic, mtlk_ability_id_t ability_id, mtlk_handle_t receiver, 
                             mtlk_core_task_func_t func, const void *data, size_t size, 
                             _mtlk_core_async_priorities_t priority,
                             mtlk_user_request_t *req,
                             mtlk_slid_t issuer_slid)
{
  int res;
  _core_async_exec_t *ctx;

  MTLK_ASSERT(0 == sizeof(_core_async_exec_t) % sizeof(void*));

  ctx = mtlk_osal_mem_alloc(sizeof(_core_async_exec_t) + size,
                            MTLK_MEM_TAG_ASYNC_CTX);
  if(NULL == ctx)
  {
    ELOG_D("CID-%04x: Failed to allocate execution context object", mtlk_vap_get_oid(nic->vap_handle));
    return MTLK_ERR_NO_MEM;
  }

  ctx->receiver     = receiver;
  ctx->data_size    = size;
  ctx->func         = func;
  ctx->user_req     = req;
  ctx->vap_handle   = nic->vap_handle;
  ctx->ability_id   = ability_id;
  memcpy(&ctx[1], data, size);

  res = mtlk_command_init(&ctx->cmd, _mtlk_core_async_clb, HANDLE_T(ctx), issuer_slid);
  if(MTLK_ERR_OK != res)
  {
    mtlk_osal_mem_free(ctx);
    ELOG_D("CID-%04x: Failed to initialize command object", mtlk_vap_get_oid(nic->vap_handle));
    return res;
  }

  res = mtlk_serializer_enqueue(&nic->slow_ctx->serializer, priority,
                                &ctx->cmd, _mtlk_core_async_compl_clb, 
                                HANDLE_T(ctx));
  if(MTLK_ERR_OK != res)
  {
    mtlk_osal_mem_free(ctx);
    ELOG_DD("CID-%04x: Failed to enqueue command object (error: %d)", mtlk_vap_get_oid(nic->vap_handle), res);
    return res;
  }

  return res;
}

#define _mtlk_core_execute_async(nic, ability_id, receiver, func, data, size, priority, req) \
  _mtlk_core_execute_async_ex((nic), (ability_id), (receiver), (func), (data), (size), (priority), (req), MTLK_SLID)

int __MTLK_IFUNC mtlk_core_schedule_internal_task_ex (struct nic *nic, 
                                                      mtlk_handle_t object, 
                                                      mtlk_core_task_func_t func, 
                                                      const void *data, size_t size,
                                                      mtlk_slid_t issuer_slid)
{
  return _mtlk_core_execute_async_ex(nic, MTLK_ABILITY_NONE, object, func, data, size, 
                                     _MTLK_CORE_PRIORITY_INTERNAL, NULL, issuer_slid);
}

/*! Function for scheduling serialized task on demand of HW module activities
    Sends message confirmation for message object specified

    \param   nic              Pointer to the core object
    \param   object           Handle of receiver object
    \param   func             Task callback
    \param   data             Pointer to the data buffer provided by caller
    \param   data_size        Size of data buffer provided by caller

*/
int __MTLK_IFUNC mtlk_core_schedule_hw_task(struct nic *nic,
                                            mtlk_handle_t object, 
                                            mtlk_core_task_func_t func, 
                                            const void *data, size_t size,
                                            mtlk_slid_t issuer_slid)
{
  return _mtlk_core_execute_async_ex(nic, MTLK_ABILITY_NONE, object, func, data, size, 
                                     _MTLK_CORE_PRIORITY_NETWORK, NULL, issuer_slid);
}
/* ======================================================*/

/* ======================================================*/
/* Function for processing HW tasks                      */

typedef enum __mtlk_hw_task_type_t
{
  SYNCHRONOUS,
  SERIALIZABLE
} _mtlk_core_task_type_t;

static void
_mtlk_process_hw_task_ex (mtlk_core_t* nic,
                          _mtlk_core_task_type_t task_type, mtlk_core_task_func_t task_func,
                          mtlk_handle_t object, const void* data, uint32 data_size, mtlk_slid_t issuer_slid)
{
    if(SYNCHRONOUS == task_type)
    {
      task_func(object, data, data_size);
    }
    else
    {
      if(MTLK_ERR_OK != mtlk_core_schedule_hw_task(nic, object,
                                                   task_func, data, data_size, issuer_slid))
      {
        ELOG_DP("CID-%04x: Hardware task schedule for callback 0x%p failed.", mtlk_vap_get_oid(nic->vap_handle), task_func);
      }
    }
}

#define _mtlk_process_hw_task(nic, task_type, task_func, object, data, data_size) \
  _mtlk_process_hw_task_ex((nic), (task_type), (task_func), (object), (data), (data_size), MTLK_SLID)

static void
_mtlk_process_user_task_ex (mtlk_core_t* nic, mtlk_user_request_t *req,
                            _mtlk_core_task_type_t task_type, mtlk_ability_id_t ability_id,
                            mtlk_core_task_func_t task_func,
                            mtlk_handle_t object, mtlk_clpb_t* data, mtlk_slid_t issuer_slid)
{
    if(SYNCHRONOUS == task_type)
    {
      int res = MTLK_ERR_BUSY;
      /* chek is ability enabled for execution */
      if (_mtlk_abmgr_is_ability_enabled(mtlk_vap_get_abmgr(nic->vap_handle), ability_id)) {
        res = task_func(object, &data, sizeof(mtlk_clpb_t*));
      }
      else
      {
        WLOG_D("Requested ability 0x%X is disabled or never was registered", ability_id);
      }


      mtlk_df_ui_req_complete(req, res);
    }
    else
    {
      int result = _mtlk_core_execute_async_ex(nic, ability_id, object, task_func,
                                               &data, sizeof(data), _MTLK_CORE_PRIORITY_USER, req,
                                               issuer_slid);

      if(MTLK_ERR_OK != result)
      {
        ELOG_DPD("CID-%04x: User task schedule for callback 0x%p failed (error %d).",
                 mtlk_vap_get_oid(nic->vap_handle), task_func, result);
        mtlk_df_ui_req_complete(req, result);
      }
    }
}

#define _mtlk_process_user_task(nic, req, task_type, ability_id, task_func, object, data) \
  _mtlk_process_user_task_ex((nic), (req), (task_type), (ability_id), (task_func), (object), (data), MTLK_SLID)

/* ======================================================*/

static int __MTLK_IFUNC
handleDisconnectSta(mtlk_handle_t core_object, const void *payload, uint32 size);
static void cleanup_on_disconnect(struct nic *nic);

char* mtlk_core_get_version_str(void)
{
  return mtlk_version_string;
}

static __INLINE mtlk_df_t*
_mtlk_core_get_master_df(const mtlk_core_t* core)
{
  return mtlk_vap_manager_get_master_df(mtlk_vap_get_manager(core->vap_handle));
}

mtlk_eeprom_data_t* __MTLK_IFUNC
mtlk_core_get_eeprom(mtlk_core_t* core)
{
  return mtlk_core_get_master(core)->slow_ctx->ee_data;
}

mtlk_dot11h_t* __MTLK_IFUNC
mtlk_core_get_dfs(mtlk_core_t* core)
{
  if (mtlk_vap_is_slave_ap(core->vap_handle)) {
    return mtlk_core_get_master(core)->slow_ctx->dot11h;
  }

  return core->slow_ctx->dot11h;
}

static char *
mtlk_net_state_to_string(uint32 state)
{
  switch (state) {
  case NET_STATE_HALTED:
    return "NET_STATE_HALTED";
  case NET_STATE_IDLE:
    return "NET_STATE_IDLE";
  case NET_STATE_READY:
    return "NET_STATE_READY";
  case NET_STATE_ACTIVATING:
    return "NET_STATE_ACTIVATING";
  case NET_STATE_CONNECTED:
    return "NET_STATE_CONNECTED";
  case NET_STATE_DISCONNECTING:
    return "NET_STATE_DISCONNECTING";
  default:
    break;
  }
  ILOG1_D("Unknown state 0x%04X", state);
  return "NET_STATE_UNKNOWN";
}

mtlk_hw_state_e
mtlk_core_get_hw_state (mtlk_core_t *nic)
{
  mtlk_hw_state_e hw_state = MTLK_HW_STATE_LAST;

  mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_PROP_STATE, &hw_state, sizeof(hw_state));
  return hw_state;
}

int
mtlk_set_hw_state (mtlk_core_t *nic, int st)
{
  mtlk_hw_state_e ost;
  mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_PROP_STATE, &ost, sizeof(ost));
  ILOG1_DD("%i -> %i", ost, st);
  return mtlk_vap_get_hw_vft(nic->vap_handle)->set_prop(nic->vap_handle, MTLK_HW_PROP_STATE, &st, sizeof(st));
}

uint16 __MTLK_IFUNC mtlk_core_get_sq_size(mtlk_core_t *nic, uint16 access_category)
{
  return mtlk_sq_get_qsize( nic->sq, access_category);
}

static int
mtlk_core_set_net_state(mtlk_core_t *core, uint32 new_state)
{
  uint32 allow_mask;
  mtlk_hw_state_e hw_state;
  int result = MTLK_ERR_OK;

  mtlk_osal_lock_acquire(&core->net_state_lock);
  if (new_state == NET_STATE_HALTED) {
    if (core->net_state != NET_STATE_HALTED) {
      mtlk_hw_state_e hw_state = mtlk_core_get_hw_state(core); 
      ELOG_DD("CID-%04x: Going to net state HALTED (net_state=%d)", mtlk_vap_get_oid(core->vap_handle), core->net_state);
      if (hw_state != MTLK_HW_STATE_EXCEPTION && 
          hw_state != MTLK_HW_STATE_APPFATAL) { 
          ELOG_DD("CID-%04x: Asserting FW: hw_state=%d", mtlk_vap_get_oid(core->vap_handle), hw_state);
          mtlk_vap_get_hw_vft(core->vap_handle)->set_prop(core->vap_handle, MTLK_HW_RESET, NULL, 0);
      } 
      core->net_state = NET_STATE_HALTED;
    }
    goto FINISH;
  }
  /* allow transition from NET_STATE_HALTED to NET_STATE_IDLE
     while in hw state MTLK_HW_STATE_READY */
  hw_state = mtlk_core_get_hw_state(core);
  if ((hw_state != MTLK_HW_STATE_READY) && (hw_state != MTLK_HW_STATE_UNLOADING) &&
      (new_state != NET_STATE_IDLE)) {
    ELOG_DD("CID-%04x: Wrong hw_state=%d", mtlk_vap_get_oid(core->vap_handle), hw_state);
    result = MTLK_ERR_HW;
    goto FINISH;
  }
  allow_mask = 0;
  switch (new_state) {
  case NET_STATE_IDLE:
    allow_mask = NET_STATE_HALTED; /* on core_start */
    break;
  case NET_STATE_READY:
    allow_mask = NET_STATE_IDLE | NET_STATE_ACTIVATING |
      NET_STATE_DISCONNECTING;
    break;
  case NET_STATE_ACTIVATING:
    allow_mask = NET_STATE_READY; 
    break;
  case NET_STATE_DISCONNECTING:
    allow_mask = NET_STATE_CONNECTED;
    break;
  case NET_STATE_CONNECTED:
    allow_mask = NET_STATE_ACTIVATING;
    break;
  default:
    break;
  }
  /* check mask */ 
  if (core->net_state & allow_mask) {
    ILOG1_SS("Going from %s to %s",
      mtlk_net_state_to_string(core->net_state),
      mtlk_net_state_to_string(new_state));
    core->net_state = new_state;
  } else {
    ILOG1_SS("Failed to change state from %s to %s",
      mtlk_net_state_to_string(core->net_state),
      mtlk_net_state_to_string(new_state));
    result = MTLK_ERR_WRONG_CONTEXT; 
  }
FINISH:
  mtlk_osal_lock_release(&core->net_state_lock);
  return result;
}

static int
mtlk_core_get_net_state(mtlk_core_t *core)
{
  uint32 net_state;
  mtlk_hw_state_e hw_state;

  hw_state = mtlk_core_get_hw_state(core);
  if (hw_state != MTLK_HW_STATE_READY && hw_state != MTLK_HW_STATE_UNLOADING) {
    net_state = NET_STATE_HALTED;
    goto FINISH;
  }
  net_state = core->net_state;
FINISH:
  return net_state;
}

static int __MTLK_IFUNC
check_mac_watchdog (mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry;
  UMI_MAC_WATCHDOG *mac_watchdog;
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(0 == size);
  MTLK_UNREFERENCED_PARAM(payload);
  MTLK_UNREFERENCED_PARAM(size);
  MTLK_ASSERT(FALSE == mtlk_vap_is_slave_ap(nic->vap_handle));

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txdm(nic->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_DBG_MAC_WATCHDOG_REQ;
  man_entry->payload_size = sizeof(UMI_MAC_WATCHDOG);

  mac_watchdog = (UMI_MAC_WATCHDOG *)man_entry->payload;
  mac_watchdog->u16Timeout =
      HOST_TO_MAC16(MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_TIMEOUT_MS));

  res = mtlk_txmm_msg_send_blocked(&man_msg,
          MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_TIMEOUT_MS));
  if (res == MTLK_ERR_OK) {
    switch (mac_watchdog->u8Status) {
    case UMI_OK:
      break;
    case UMI_MC_BUSY:
      break;
    case UMI_TIMEOUT:
      res = MTLK_ERR_UMI;
      break;
    default:
      res = MTLK_ERR_UNKNOWN;
      break;
    }
  }
  mtlk_txmm_msg_cleanup(&man_msg);

END:
  if (res != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: MAC watchdog error %d, resetting", mtlk_vap_get_oid(nic->vap_handle), res);
    mtlk_vap_get_hw_vft(nic->vap_handle)->set_prop(nic->vap_handle, MTLK_HW_RESET, NULL, 0);
  } else {
    if (MTLK_ERR_OK !=
        mtlk_osal_timer_set(&nic->slow_ctx->mac_watchdog_timer,
                            MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_PERIOD_MS))) {
      ELOG_D("CID-%04x: Cannot schedule MAC watchdog timer, resetting", mtlk_vap_get_oid(nic->vap_handle));
      mtlk_vap_get_hw_vft(nic->vap_handle)->set_prop(nic->vap_handle, MTLK_HW_RESET, NULL, 0);
    }
  }

  return MTLK_ERR_OK;
}

static uint32
mac_watchdog_timer_handler (mtlk_osal_timer_t *timer, mtlk_handle_t data)
{
  int err;
  struct nic *nic = (struct nic *)data;

  err = _mtlk_core_execute_async(nic, MTLK_ABILITY_NONE, HANDLE_T(nic), check_mac_watchdog,
                                 NULL, 0, _MTLK_CORE_PRIORITY_MAINTENANCE, NULL);

  if (err != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't schedule MAC WATCHDOG task (err=%d)", mtlk_vap_get_oid(nic->vap_handle), err);
  }

  return 0;
}

static int
get_cipher (mtlk_core_t* core, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);

  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_DIRECTED)) {
    MTLK_ASSERT(mtlk_nbuf_priv_get_src_sta(nbuf_priv) != NULL);
    return mtlk_sta_get_cipher(mtlk_nbuf_priv_get_src_sta(nbuf_priv));
  } else {
    return core->group_cipher;
  }
}

/* Get Received Security Counter buffer */
static int
get_rsc_buf(mtlk_core_t* core, mtlk_nbuf_t *nbuf, int off)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  u8 cipher = get_cipher(core, nbuf);
 
  if (cipher == IW_ENCODE_ALG_TKIP || cipher == IW_ENCODE_ALG_CCMP) {
    return mtlk_nbuf_priv_set_rsc_buf(nbuf_priv, nbuf->data + off);
  } else {
    return 0;
  }
}

static int
_mtlk_core_mac_reset_stats (mtlk_core_t *nic)
{
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry;
  int res = MTLK_ERR_OK;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txdm(nic->vap_handle), NULL);
  if (man_entry) {
    UMI_RESET_STATISTICS *pstats = (UMI_RESET_STATISTICS *)man_entry->payload;

    man_entry->id           = UM_DBG_RESET_STATISTICS_REQ;
    man_entry->payload_size = sizeof(*pstats);
    pstats->u16Status       = 0;

    res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
    if (res != MTLK_ERR_OK) {
      ELOG_D("CID-%04x: MAC Reset Stat sending timeout", mtlk_vap_get_oid(nic->vap_handle));
    }

    mtlk_txmm_msg_cleanup(&man_msg);
  }
  else {
    ELOG_D("CID-%04x: Can't reset statistics due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
  }

  return res;
}

static void __MTLK_IFUNC 
clean_all_sta_on_disconnect_sta_clb (mtlk_handle_t    usr_ctx, 
                                     const sta_entry *sta)
{
  struct nic      *nic  = HANDLE_T_PTR(struct nic, usr_ctx);
  const IEEE_ADDR *addr = mtlk_sta_get_addr(sta);

  ILOG1_Y("Station %Y disconnected", addr->au8Addr);
  mtlk_df_ui_notify_node_disconect(mtlk_vap_get_df(nic->vap_handle), addr->au8Addr);
  mtlk_hstdb_remove_all_by_sta(&nic->slow_ctx->hstdb, sta);
}

static void
_mtlk_core_clean_vap_tx_on_disconnect(struct nic *nic)
{
  mtlk_sq_peer_ctx_cancel_all_packets_for_vap(nic->sq, &nic->sq->broadcast, nic->vap_handle);
}

static void
clean_all_sta_on_disconnect (struct nic *nic)
{
  BOOL wait_all_packets_confirmed;

  wait_all_packets_confirmed = (mtlk_core_get_net_state(nic) != NET_STATE_HALTED);

  mtlk_stadb_disconnect_all(&nic->slow_ctx->stadb, 
                            clean_all_sta_on_disconnect_sta_clb,
                            HANDLE_T(nic),
                            wait_all_packets_confirmed);

  _mtlk_core_clean_vap_tx_on_disconnect(nic);
}

static void
cleanup_on_disconnect (struct nic *nic)
{
  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    unsigned char bssid[ETH_ALEN];
    mtlk_pdb_get_mac(
            mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID, bssid);
    // Drop BSSID persistance in BSS cache.
    // We've been disconnected from BSS, thus we don't know 
    // whether it's alive or not.
    mtlk_cache_set_persistent(&nic->slow_ctx->cache, 
                              bssid,
                              FALSE);
    /* rollback network mode */
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_NET_MODE_CUR, mtlk_core_get_network_mode_cfg(nic));
  }

  if (!_mtlk_core_has_connections(nic)) {
    if (mtlk_core_get_net_state(nic) == NET_STATE_DISCONNECTING) {
      mtlk_pdb_set_mac(mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID,
                       mtlk_osal_eth_zero_addr);
      mtlk_core_set_net_state(nic, NET_STATE_READY);
    }
    _mtlk_core_clean_vap_tx_on_disconnect(nic);
  }
}

static int
_mtlk_core_send_disconnect_req_blocked (struct nic *nic, const IEEE_ADDR *addr, uint16 reason)
{
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_DISCONNECT   *psUmiDisconnect;
  int               net_state = mtlk_core_get_net_state(nic);
  int               res = MTLK_ERR_OK;
  IEEE_ADDR         addr_to_send;

  if (((mtlk_vap_is_ap(nic->vap_handle) && addr == NULL) || !mtlk_vap_is_ap(nic->vap_handle)) && (net_state != NET_STATE_HALTED)) {
    /* check if we can disconnect in current net state */
    res = mtlk_core_set_net_state(nic, NET_STATE_DISCONNECTING);
    if (res != MTLK_ERR_OK) {
      goto FINISH;
    }
  }

  memset(&addr_to_send, 0, sizeof(addr_to_send));

  if (addr == NULL && !mtlk_vap_is_ap(nic->vap_handle)) {
    mtlk_pdb_get_mac(
            mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID, addr_to_send.au8Addr);
  }
  else if (addr) {
    addr_to_send = *addr;
  }

#ifdef PHASE_3
  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    /* stop cache updation on STA. */
    mtlk_stop_cache_update(nic);
  }
#endif
  if (mtlk_vap_is_ap(nic->vap_handle) && addr == NULL) {
    clean_all_sta_on_disconnect(nic);
  } 
  else {
    sta_entry *sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, addr_to_send.au8Addr);
    if (sta == NULL) {
      ILOG1_Y("STA not found during cleanup: %Y", &addr_to_send);
    } else {

      mtlk_stadb_remove_sta(&nic->slow_ctx->stadb, sta);
      mtlk_hstdb_remove_all_by_sta(&nic->slow_ctx->hstdb, sta);
      /*Send indication if STA has been disconnected from AP*/
      _mtlk_core_on_peer_disconnect(nic, sta, reason);
      mtlk_sta_decref(sta); /* De-reference of find */
    }
  }

  if (net_state == NET_STATE_HALTED) {
    /* Do not send anything to halted MAC or if STA hasn't been connected */
    res = MTLK_ERR_OK;
    goto FINISH;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), &res);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send DISCONNECT request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_DISCONNECT_REQ;
  man_entry->payload_size = sizeof(UMI_DISCONNECT);
  psUmiDisconnect         = (UMI_DISCONNECT *)man_entry->payload;

  psUmiDisconnect->u16Status = UMI_OK;
  psUmiDisconnect->sStationID = addr_to_send;

  mtlk_dump(5, psUmiDisconnect, sizeof(UMI_DISCONNECT), "dump of UMI_DISCONNECT:");

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send DISCONNECT request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), res);
    goto FINISH;
  }

  cleanup_on_disconnect(nic);

  if (mtlk_vap_is_ap(nic->vap_handle)) {
    ILOG1_YD("Station %Y disconnected (status %u)", 
      &addr_to_send, MAC_TO_HOST16(psUmiDisconnect->u16Status));
    _mtlk_core_notify_ap_of_station_disconnection(nic, &addr_to_send);
  } else {
    ILOG1_YD("Disconnected from AP %Y (status %u)", 
      &addr_to_send, MAC_TO_HOST16(psUmiDisconnect->u16Status));
    mtlk_df_ui_notify_disassociation(mtlk_vap_get_df(nic->vap_handle));
  }

  /* update disconnections statistics */
  nic->pstats.num_disconnects++;

  if (MAC_TO_HOST16(psUmiDisconnect->u16Status) != UMI_OK) {
    WLOG_DYD("CID-%04x: Station %Y disconnect failed in FW (status=%u)", mtlk_vap_get_oid(nic->vap_handle),
        &addr_to_send, MAC_TO_HOST16(psUmiDisconnect->u16Status));
    res = MTLK_ERR_MAC;
    goto FINISH;
  }

  res = MTLK_ERR_OK;

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static int
clear_group_key(struct nic *nic)
{
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_CLEAR_KEY    *umi_cl_key;
  int res;

  ILOG1_D("CID-%04x: Clear group key", mtlk_vap_get_oid(nic->vap_handle));

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), NULL);
  if (!man_entry) {
    return -ENOMEM;
  }

  umi_cl_key = (UMI_CLEAR_KEY*) man_entry->payload;
  memset(umi_cl_key, 0, sizeof(*umi_cl_key));
  man_entry->id = UM_MAN_CLEAR_KEY_REQ;
  man_entry->payload_size = sizeof(*umi_cl_key);

  umi_cl_key->u16KeyType = cpu_to_le16(UMI_RSN_GROUP_KEY);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) 
    ELOG_DD("CID-%04x: mtlk_mm_send_blocked failed: %i", mtlk_vap_get_oid(nic->vap_handle), res);

  mtlk_txmm_msg_cleanup(&man_msg);

  return (res == MTLK_ERR_OK ? 0 : -EFAULT);
}

static void __MTLK_IFUNC
  _mtlk_core_trigger_connect_complete_event(mtlk_core_t *nic, BOOL success)
{
  nic->activation_status = success;
  mtlk_osal_event_set(&nic->slow_ctx->connect_event);
}

static int
reset_security_stuff(struct nic *nic)
{
  int res = MTLK_ERR_OK;

  memset(&nic->slow_ctx->rsnie, 0, sizeof(nic->slow_ctx->rsnie));
  if (mtlk_vap_is_ap(nic->vap_handle))
    clear_group_key(nic);
  if ((res = mtlk_set_mib_rsn(mtlk_vap_get_txmm(nic->vap_handle), 0)) != MTLK_ERR_OK)
    ELOG_D("CID-%04x: Failed to reset RSN", mtlk_vap_get_oid(nic->vap_handle));
  if (nic->slow_ctx->wep_enabled) {
    /* Disable WEP encryption */
    res = _mtlk_core_set_wep(nic, FALSE);
    nic->slow_ctx->wep_enabled = FALSE;
  }
  nic->slow_ctx->default_key = 0;
  nic->slow_ctx->wps_in_progress = FALSE;

  return res;
}

BOOL
can_disconnect_now(struct nic *nic)
{
  return !mtlk_core_scan_is_running(nic);
}

/* This interface can be used if we need to disconnect while in 
 * atomic context (for example, when disconnecting from a timer).
 * Disconnect process requires blocking function calls, so we
 * have to schedule a work.
 */

struct mtlk_core_disconnect_sta_data
{
  IEEE_ADDR          addr;
  uint16             reason;
  int               *res;
  mtlk_osal_event_t *done_evt;
};

static void
_mtlk_core_schedule_disconnect_sta (struct nic *nic, const IEEE_ADDR *addr, 
                                    uint16 reason, int *res, mtlk_osal_event_t *done_evt)
{
  int err;
  struct mtlk_core_disconnect_sta_data data;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(addr != NULL);

  memset(&data, 0, sizeof(data));

  data.addr     = *addr;
  data.reason   = reason;
  data.res      = res;
  data.done_evt = done_evt;

  err = _mtlk_core_execute_async(nic, MTLK_ABILITY_NONE, HANDLE_T(nic), handleDisconnectSta, &data, 
                                 sizeof(data), _MTLK_CORE_PRIORITY_NETWORK, NULL);
  if (err != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't schedule DISCONNECT STA (err=%d)", mtlk_vap_get_oid(nic->vap_handle), err);
  }
}

/***************************************************************************
 * Disconnecting routines for STA
 ***************************************************************************/
/* This interface can be used if we need to disconnect while in
 * atomic context (for example, when disconnecting from a timer).
 * Disconnect process requires blocking function calls, so we
 * have to schedule a work.
 */
static void
_mtlk_core_schedule_disconnect_me (struct nic *nic, uint16 reason)
{
  int err;
  MTLK_ASSERT(nic != NULL);

  err = _mtlk_core_execute_async(nic, MTLK_ABILITY_NONE, HANDLE_T(nic), handleDisconnectMe, &reason, sizeof(reason),
                                 _MTLK_CORE_PRIORITY_NETWORK, NULL);

  if (err != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't schedule DISCONNECT (err=%d)", mtlk_vap_get_oid(nic->vap_handle), err);
  }
}

static int __MTLK_IFUNC
_mtlk_core_disconnect_sta(mtlk_core_t *nic, uint16 reason)
{
  uint32 net_state;

  net_state = mtlk_core_get_net_state(nic);
  if ((net_state != NET_STATE_CONNECTED) &&
      (net_state != NET_STATE_HALTED)) { /* allow disconnect for clean up */
    ILOG0_DS("CID-%04x: disconnect in invalid state %s", mtlk_vap_get_oid(nic->vap_handle),
          mtlk_net_state_to_string(net_state));
    return MTLK_ERR_NOT_READY;
  }

  if (!can_disconnect_now(nic)) {
    _mtlk_core_schedule_disconnect_me(nic, reason);
    return MTLK_ERR_OK;
  }

  reset_security_stuff(nic);

  return _mtlk_core_send_disconnect_req_blocked(nic, NULL, reason);
}

static int __MTLK_IFUNC
_mtlk_core_hanle_disconnect_sta_req(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  return _mtlk_core_disconnect_sta(HANDLE_T_PTR(mtlk_core_t, hcore), FM_STATUSCODE_USER_REQUEST);
}

static int __MTLK_IFUNC
handleDisconnectMe(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);

  MTLK_ASSERT(sizeof(uint16) == size);

  ILOG2_V("Handling disconnect request");
  _mtlk_core_disconnect_sta(nic, *((uint16*)payload));

  return MTLK_ERR_OK;
}

/*****************************************************************************
**
** NAME         mtlk_send_null_packet
**
** PARAMETERS   nic           Card context
**              sta                 Destination STA
**
** RETURNS      none
**
** DESCRIPTION  Function used to send NULL packets from STA to AP in order
**              to support AutoReconnect feature (NULL packets are treated as
**              keepalive data)
**
******************************************************************************/
static void
mtlk_send_null_packet (struct nic *nic, sta_entry *sta)
{
  mtlk_hw_send_data_t data;
  int                 ires = MTLK_ERR_UNKNOWN;
  mtlk_nbuf_t         *nbuf;
  mtlk_nbuf_priv_t  *nbuf_priv;

  // Transmit only if connected
  if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED)
    return;

  // STA only ! Transmit only if data was not stopped (by 11h)
  if (mtlk_dot11h_is_data_stop(mtlk_core_get_dfs(nic)) == 1) {
    goto END;
  }

  // XXX, klogg: revisit
  // Use NULL sk_buffer (really UGLY)
  nbuf = mtlk_df_nbuf_alloc(_mtlk_core_get_master_df(nic), 0);
  if (!nbuf) {
    ILOG1_V("there is no free network buffer to send NULL packet");
    goto END;
  }
  nbuf_priv = mtlk_nbuf_priv(nbuf);
  mtlk_nbuf_priv_set_dst_sta(nbuf_priv, sta);
  mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_UNICAST);

  memset(&data, 0, sizeof(data));

  data.msg = mtlk_vap_get_hw_vft(nic->vap_handle)->get_msg_to_send(nic->vap_handle, NULL);
  // Check free MSDUs
  if (!data.msg) {
    ILOG1_V("there is no free msg to send NULL packet");
    mtlk_df_nbuf_free(_mtlk_core_get_master_df(nic), nbuf);
    goto END;
  }

  ILOG3_P("got from hw msg %p", data.msg);

  data.nbuf            = nbuf;
  data.size            = 0;
  data.rcv_addr        = mtlk_sta_get_addr(sta);
  data.wds             = 0;
  data.access_category = UMI_USE_DCF;
  data.encap_type      = ENCAP_TYPE_ILLEGAL;
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  data.rf_mgmt_data    = mtlk_sta_get_rf_mgmt_data(sta);
#endif

  ires = mtlk_vap_get_hw_vft(nic->vap_handle)->send_data(nic->vap_handle, &data);
  if (__LIKELY(ires == MTLK_ERR_OK)) {
#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
    if (mtlk_vap_is_ap(nic->vap_handle)) {
      mtlk_aocs_msdu_tx_inc_nof_used(mtlk_core_get_master(nic)->slow_ctx->aocs, AC_BE);
    }
#endif
  }
  else {
    WLOG_DD("CID-%04x: hw_send (NULL) failed with Err#%d", mtlk_vap_get_oid(nic->vap_handle), ires);
    mtlk_vap_get_hw_vft(nic->vap_handle)->release_msg_to_send(nic->vap_handle, data.msg);
  }

END:
  return;
}



/*****************************************************************************
**
** DESCRIPTION  Determines if packet should be passed to upper layer and/or
**              forwarded to BSS. Sets nbuf flags.
**
** 1. Unicast (to us)   -> pass to upper layer.
** 2. Broadcast packet  -> pass to upper layer AND forward to BSS.
** 3. Multicast packet  -> forward to BSS AND check if AP is in multicast group,
**                           if so - pass to upper layer.
** 4. Unicast (not to us) -> if STA found in connected list - forward to BSS.
**
******************************************************************************/
static mtlk_nbuf_priv_t *
analyze_rx_packet (mtlk_core_t* nic, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  struct ethhdr *ether_header = (struct ethhdr *)nbuf->data;
  int bridge_mode;

  bridge_mode = MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE);

  // check if device in PROMISCUOUS mode
  if (mtlk_df_ui_is_promiscuous(mtlk_vap_get_df(nic->vap_handle)))
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_CONSUME);
    
  // check if destination MAC is our address
  if (0 == MTLK_CORE_HOT_PATH_PDB_CMP_MAC(nic, CORE_DB_CORE_MAC_ADDR, ether_header->h_dest))
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_CONSUME);

  // check if destination MAC is broadcast address
  else if (mtlk_osal_eth_is_broadcast(ether_header->h_dest)) {
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_BROADCAST | MTLK_NBUFF_CONSUME);
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_BROADCAST_PACKETS_RECEIVED);
    mtlk_core_add_cnt(nic, MTLK_CORE_CNT_BROADCAST_BYTES_RECEIVED, mtlk_df_nbuf_get_data_length(nbuf));
    if (mtlk_vap_is_ap(nic->vap_handle) && MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_AP_FORWARDING))
      mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_FORWARD);
  }
  // check if destination MAC is multicast address
  else if (mtlk_osal_eth_is_multicast(ether_header->h_dest)) {
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_MULTICAST);
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_MULTICAST_PACKETS_RECEIVED);
    mtlk_core_add_cnt(nic, MTLK_CORE_CNT_MULTICAST_BYTES_RECEIVED, mtlk_df_nbuf_get_data_length(nbuf));

    if (mtlk_vap_is_ap(nic->vap_handle) && MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_AP_FORWARDING))
      mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_FORWARD);

    //check if we subscribed to all multicast
    if(mtlk_df_ui_check_is_mc_group_member(mtlk_vap_get_df(nic->vap_handle), ether_header->h_dest))
      mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_CONSUME);
  }
  // check if destination MAC is unicast address of connected STA
  else {
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_UNICAST);
    if (mtlk_vap_is_ap(nic->vap_handle) && MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_AP_FORWARDING)) {
      // Search of DESTINATION MAC ADDRESS of RECEIVED packet
      sta_entry *dst_sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ether_header->h_dest);
      if ((NULL == dst_sta) && (BR_MODE_WDS == bridge_mode) ) {
        dst_sta = mtlk_hstdb_find_sta(&nic->slow_ctx->hstdb, 
                                      ether_header->h_dest);
      }

      if (dst_sta != NULL) {
        mtlk_nbuf_priv_set_dst_sta(nbuf_priv, dst_sta);
        mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_FORWARD);
        mtlk_sta_decref(dst_sta); /* De-reference of find */
      }
    }
  }

  return nbuf_priv;
}



/*****************************************************************************
**
** NAME         mtlk_core_handle_tx_data
**
** PARAMETERS   nic                 Pointer to the core object
**              nbuf                 Skbuff to transmit
**
** DESCRIPTION  Entry point for TX buffers
**
******************************************************************************/
void __MTLK_IFUNC
mtlk_core_handle_tx_data(mtlk_core_t* nic, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_priv_t *nbuf_priv;
  struct ethhdr *ether_header = (struct ethhdr *)nbuf->data;
  uint16 ac;
  int bridge_mode;

  CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_TX_OS);

  bridge_mode = MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE);

  /* Transmit only if connected to someone */
  if (unlikely(!_mtlk_core_has_connections(nic))) {
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_NO_PEERS);
    goto ERR;
  }

  /* get private fields */
  nbuf_priv = mtlk_nbuf_priv(nbuf);

#if defined(MTCFG_PER_PACKET_STATS) && defined(MTCFG_TSF_TIMER_ACCESS_ENABLED)
  mtlk_nbuf_priv_stats_set(nbuf_priv, MTLK_NBUF_STATS_TS_SQ_IN, mtlk_hw_get_timestamp(nic->vap_handle));
#endif

  /* store vap index in private data, will be retrieved when SQ sends packet to HW */
  mtlk_nbuf_priv_set_vap_handle(nbuf_priv, nic->vap_handle);

  /* analyze (and probably adjust) packet's priority and AC */
  ac = mtlk_qos_get_ac(&nic->qos, nbuf);
  if (unlikely(ac == AC_INVALID)) {
    sta_entry *sta = mtlk_nbuf_priv_get_dst_sta(nbuf_priv);
    if (sta) {
      mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_DRV_ACM);
    } else {
      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_ACM);
    }
    goto ERR;
  }

  /* check frame urgency (currently for the 802.1X packets only) */
  if (mtlk_wlan_pkt_is_802_1X(ether_header->h_proto))
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_URGENT);

  switch (bridge_mode)
  {
  case BR_MODE_MAC_CLONING: /* MAC cloning stuff */
    /* these frames will generate MIC failures on AP and they have
     * no meaning in MAC Cloning mode - so we just drop them silently */
    if (!mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_URGENT) && 
         (0 != MTLK_CORE_HOT_PATH_PDB_CMP_MAC(nic, CORE_DB_CORE_MAC_ADDR, ether_header->h_source))) {
      sta_entry *sta = mtlk_nbuf_priv_get_dst_sta(nbuf_priv);
      if (sta) {
        mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_EAPOL_CLONED);
      } else {
        mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_EAPOL_CLONED);
      }
      goto ERR;
    }
    break;

  case BR_MODE_L2NAT: /* L2NAT stuff */
    /* call the hook */
    nbuf = mtlk_l2nat_on_tx(nic, nbuf);

    /* update ethernet header & nbuf_priv pointers */
    ether_header = (struct ethhdr *)nbuf->data;
    nbuf_priv = mtlk_nbuf_priv(nbuf);

    if (!mtlk_vap_is_ap(nic->vap_handle) &&
        (0 != MTLK_CORE_HOT_PATH_PDB_CMP_MAC(nic, CORE_DB_CORE_MAC_ADDR, ether_header->h_source))) {
      mtlk_hstdb_update_default_host(&nic->slow_ctx->hstdb, ether_header->h_source);
    }
    break;

  case BR_MODE_WDS: /* WDS stuff */
    if (!mtlk_vap_is_ap(nic->vap_handle) &&
        (0 != MTLK_CORE_HOT_PATH_PDB_CMP_MAC(nic, CORE_DB_CORE_MAC_ADDR, ether_header->h_source))) {
      sta_entry *sta = mtlk_stadb_get_ap(&nic->slow_ctx->stadb);
      if (sta) {
        /* update or add host in case 802.3 SA is not ours */
        mtlk_hstdb_update_host(&nic->slow_ctx->hstdb, ether_header->h_source, sta);
        mtlk_sta_decref(sta); /* De-reference of get_ap */
      }
    }
    break;

  default: /* no bridgung */
    break;
  }

  ILOG4_Y("802.3 tx DA: %Y", ether_header->h_dest);
  ILOG4_Y("802.3 tx SA: %Y", ether_header->h_source);

  /* check frame destination */
  if (mtlk_osal_eth_is_broadcast(ether_header->h_dest)) {
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_BROADCAST);
  }
  else if (mtlk_osal_eth_is_multicast(ether_header->h_dest)) {
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_MULTICAST);
  }
  else {
    sta_entry * dst_sta = NULL;
    /* On STA we have only AP connected at id 0, so we do not need
     * to perform search of destination id */
    if (!mtlk_vap_is_ap(nic->vap_handle)) {
      dst_sta = mtlk_stadb_get_ap(&nic->slow_ctx->stadb); 
    /* On AP we need to find destination STA id in database of peers
     * (both registered HOSTs and connected STAs) */
    } else {
      dst_sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ether_header->h_dest);
      if (dst_sta == NULL && bridge_mode == BR_MODE_WDS) {
        dst_sta = mtlk_hstdb_find_sta(&nic->slow_ctx->hstdb, 
                                      ether_header->h_dest);
      }
    }

    if (dst_sta) {
      mtlk_nbuf_priv_set_dst_sta(nbuf_priv, dst_sta);
      mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_UNICAST);
      mtlk_sta_decref(dst_sta); /* De-reference of find or get_ap */
    }
    else {
      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_DIRECTED);
      ILOG3_Y("Unknown destination (%Y)", ether_header->h_dest);
      goto ERR;
    }
  }

  mtlk_mc_transmit(nic, nbuf);

  CPU_STAT_END_TRACK(CPU_STAT_ID_TX_OS);
  return;

ERR:
  mtlk_df_nbuf_free(_mtlk_core_get_master_df(nic), nbuf);
  CPU_STAT_END_TRACK(CPU_STAT_ID_TX_OS);
}

static int
parse_and_get_replay_counter(mtlk_nbuf_t *nbuf, u8 *rc, u8 cipher)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);

  ASSERT(rc != NULL);

  if (cipher != IW_ENCODE_ALG_TKIP && cipher != IW_ENCODE_ALG_CCMP)
    return 1;

  rc[0] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 7);
  rc[1] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 6);
  rc[2] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 5);
  rc[3] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 4);

  if (cipher == IW_ENCODE_ALG_TKIP) {
    rc[4] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 0);
    rc[5] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 2);
  } else if (cipher == IW_ENCODE_ALG_CCMP) {
    rc[4] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 1);
    rc[5] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 0);
  }

  return 0;
}

static int
detect_replay(mtlk_core_t *core, mtlk_nbuf_t *nbuf, u8 *last_rc)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  sta_entry *sta;
  u8 cipher;
  int res;
  u8 rc[6]; // replay counter
  u8 rsn_bits = mtlk_nbuf_priv_get_rcn_bits(nbuf_priv);
  u8 nfrags;  // number of fragments this MSDU consisted of
  struct ethhdr *ether_header = NULL;

  sta = mtlk_nbuf_priv_get_src_sta(nbuf_priv);
  ASSERT(sta != NULL);

  cipher = get_cipher(core, nbuf);

  if (cipher != IW_ENCODE_ALG_TKIP && cipher != IW_ENCODE_ALG_CCMP)
    return 0;

  res = parse_and_get_replay_counter(nbuf, rc, cipher);
  ASSERT(res == 0);

  ILOG3_DDDDDDDDDDDD("last RSC %02x%02x%02x%02x%02x%02x, got RSC %02x%02x%02x%02x%02x%02x",
   last_rc[0], last_rc[1], last_rc[2], last_rc[3], last_rc[4], last_rc[5],
   rc[0], rc[1], rc[2], rc[3], rc[4], rc[5]);

  res = memcmp(rc, last_rc, sizeof(rc));

  if (res <= 0) {
    ILOG2_YDDDDDDDDDDDD("replay detected from %Y last RSC %02x%02x%02x%02x%02x%02x, got RSC %02x%02x%02x%02x%02x%02x",
        mtlk_sta_get_addr(sta)->au8Addr,
        last_rc[0], last_rc[1], last_rc[2], last_rc[3], last_rc[4], last_rc[5],
        rc[0], rc[1], rc[2], rc[3], rc[4], rc[5]);

    ether_header = (struct ethhdr *)nbuf->data;
    if(mtlk_osal_eth_is_multicast(ether_header->h_source) || mtlk_osal_eth_is_broadcast(ether_header->h_source)) {
      mtlk_core_inc_cnt(core, MTLK_CORE_CNT_MULTICAST_REPLAYED_PACKETS);
    }
    else {
      mtlk_core_inc_cnt(core, MTLK_CORE_CNT_UNICAST_REPLAYED_PACKETS);
    }

    return 1;
  }

  ILOG3_D("rsn bits 0x%02x", rsn_bits);

  if (rsn_bits & UMI_NOTIFICATION_MIC_FAILURE) {

    WLOG_DY("CID-%04x: MIC failure from %Y", mtlk_vap_get_oid(core->vap_handle), mtlk_sta_get_addr(sta)->au8Addr);

    if(IW_ENCODE_ALG_TKIP == cipher)
    {
      mtlk_df_ui_mic_fail_type_t mic_fail_type =
          mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_DIRECTED) ? MIC_FAIL_PAIRWISE : MIC_FAIL_GROUP;

      mtlk_df_ui_notify_mic_failure(
          mtlk_vap_get_df(core->vap_handle),
          mtlk_sta_get_addr(sta)->au8Addr,
          mic_fail_type);

      _mtlk_core_on_mic_failure(core, mic_fail_type);
    }

    return 2;
  }

  nfrags = (rsn_bits & 0xf0) >> 4;
  if (nfrags) {
    uint64 u64buf = 0;
    char *p = (char*)&u64buf + sizeof(u64buf)-sizeof(rc);
    memcpy(p, rc, sizeof(rc));
    u64buf = be64_to_cpu(u64buf);
    u64buf += nfrags;
    u64buf = cpu_to_be64(u64buf);
    memcpy(rc, p, sizeof(rc));
  }

  memcpy(last_rc, rc, sizeof(rc));

  return 0;
}

#define SEQUENCE_NUMBER_LIMIT                    (0x1000)
#define SEQ_DISTANCE(seq1, seq2) (((seq2) - (seq1) + SEQUENCE_NUMBER_LIMIT) \
                                    % SEQUENCE_NUMBER_LIMIT)

// Send Packet to the OS's protocol stack
// (or forward)
static void
send_up (mtlk_core_t* nic, mtlk_nbuf_t *nbuf)
{
  uint32 net_state;
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  sta_entry *sta = mtlk_nbuf_priv_get_src_sta(nbuf_priv);
  int bridge_mode;

  bridge_mode = MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE);

  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_FORWARD)) {

    /* Clone received packet for forwarding */
    mtlk_nbuf_t *cl_nbuf = NULL;
    uint16 ac;

    CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_TX_FWD);

    /* Count rxed data bo be forwarded */
    mtlk_sta_on_rx_packet_forwarded(sta, nbuf);

    /* analyze (and probably adjust) packet's priority and AC */
    ac = mtlk_qos_get_ac(&nic->qos, nbuf);
    if (unlikely(ac == AC_INVALID)) {
      if (sta) {
        mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_DRV_ACM);
      } else {
        mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_ACM);
      }
    } else
      cl_nbuf = mtlk_df_nbuf_clone_with_priv(_mtlk_core_get_master_df(nic), nbuf);

    if (likely(cl_nbuf != NULL)) {
      mtlk_mc_transmit(nic, cl_nbuf);
    } else
      nic->pstats.fwd_dropped++;

    CPU_STAT_END_TRACK(CPU_STAT_ID_TX_FWD);

  }

  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_CONSUME)) {
    struct ethhdr *ether_header = (struct ethhdr *)nbuf->data;
#if defined MTLK_DEBUG_IPERF_PAYLOAD_RX
    //check if it is an iperf's packet we use to debug
    mtlk_iperf_payload_t *iperf = debug_ooo_is_iperf_pkt((uint8*) ether_header);
    if (iperf != NULL) {
      iperf->ts.tag_tx_to_os = htonl(debug_iperf_priv.tag_tx_to_os);
      debug_iperf_priv.tag_tx_to_os++;
    }
#endif

    ILOG3_Y("802.3 rx DA: %Y", nbuf->data);
    ILOG3_Y("802.3 rx SA: %Y", nbuf->data+ETH_ALEN);

    ILOG3_D("packet protocol %04x", ntohs(ether_header->h_proto));

    /* NOTE: Operations below can change packet payload, so it seems that we
     * may need to perform skb_unshare for the packte. But they are available
     * only for station, which does not perform packet forwarding on RX, so
     * packet cannot be cloned (flag MTLK_NBUFF_FORWARD is unset). In future this may
     * change (everything changes...) so we will need to unshare here */
    if (!mtlk_vap_is_ap(nic->vap_handle) && (bridge_mode != BR_MODE_WDS))
      mtlk_mc_restore_mac(nbuf);
    switch (bridge_mode)
    {
    case BR_MODE_MAC_CLONING:
      if (mtlk_wlan_pkt_is_802_1X(ether_header->h_proto)) {
        mtlk_osal_copy_eth_addresses(nbuf->data,
            mtlk_df_ui_get_mac_addr(mtlk_vap_get_df(nic->vap_handle)));
        ILOG2_Y("MAC Cloning enabled, DA set to %Y", nbuf->data);
      }
      break;
    case BR_MODE_L2NAT:
      mtlk_l2nat_on_rx(nic, nbuf);
      break;
    default:
      break;
    }

    net_state = mtlk_core_get_net_state(nic);
    if (net_state != NET_STATE_CONNECTED) {
      mtlk_df_nbuf_free(_mtlk_core_get_master_df(nic), nbuf);
      if (net_state != NET_STATE_DISCONNECTING)
        WLOG_DD("CID-%04x: Data rx in not connected state (current state is %d), dropped", mtlk_vap_get_oid(nic->vap_handle), net_state);
      return;
    }

#ifdef MTLK_DEBUG_CHARIOT_OOO
    /* check out-of-order */
    {
      int diff, seq_prev;

      seq_prev = nic->seq_prev_sent[nbuf_priv->seq_qos];
      //ILOG2_DD("qos %d sn %u\n", nbuf_priv->seq_qos, nbuf_priv->seq_num);
      diff = SEQ_DISTANCE(seq_prev, nbuf_priv->seq_num);
      if (diff > SEQUENCE_NUMBER_LIMIT / 2)
        ILOG2_DDD("ooo: qos %u prev = %u, cur %u\n", nbuf_priv->seq_qos, seq_prev, nbuf_priv->seq_num);
      nic->seq_prev_sent[nbuf_priv->seq_qos] = nbuf_priv->seq_num;
    }
#endif

    // Count only packets sent to OS
    mtlk_sta_on_packet_indicated(sta, nbuf);

    mtlk_df_ui_indicate_rx_data(mtlk_vap_get_df(nic->vap_handle), nbuf);

    ILOG3_PD("nbuf %p, rx_packets %lu", nbuf, _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_PACKETS_RECEIVED));
  } else {
    mtlk_df_nbuf_free(_mtlk_core_get_master_df(nic), nbuf);
    ILOG3_P("nbuf %p dropped - consumption is disabled", nbuf);
  }
}

int
mtlk_detect_replay_or_sendup(mtlk_core_t* core, mtlk_nbuf_t *nbuf, u8 *rsn)
{
  int res=0;
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);

  if (mtlk_nbuf_priv_get_src_sta(nbuf_priv) != NULL)
    res = detect_replay(core, nbuf, rsn);

  if (res != 0) {
    mtlk_df_nbuf_free(_mtlk_core_get_master_df(core), nbuf);
  } else {
    send_up(core, nbuf);
  }

  return res;
}

static uint8
_mtlk_core_get_antennas_cnt(mtlk_core_t *core, mtlk_pdb_id_t id_array)
{
  mtlk_pdb_size_t size = MTLK_NUM_ANTENNAS_BUFSIZE;
  uint8 count = 0;
  uint8 val_array[MTLK_NUM_ANTENNAS_BUFSIZE];
  
  if (MTLK_ERR_OK != MTLK_CORE_PDB_GET_BINARY(core, id_array, val_array, &size))
  {
    MTLK_ASSERT(0); /* Can not retrieve antennas configuration from PDB */
    return 0;
  }
    
  for (count = 0; (0 != val_array[count]) && (count < (MTLK_NUM_ANTENNAS_BUFSIZE - 1)); count++);

  return count;

}

int16 __MTLK_IFUNC
mtlk_calc_tx_power_lim_wrapper(mtlk_handle_t usr_data, int8 spectrum_mode, uint8 channel)
{
    struct nic* nic = (struct nic*)usr_data;
    
    return mtlk_calc_tx_power_lim(&nic->slow_ctx->tx_limits, 
                                  channel,
                                  country_code_to_domain(mtlk_core_get_country_code(nic)),
                                  spectrum_mode,
                                  nic->slow_ctx->bonding,
                                  _mtlk_core_get_antennas_cnt(nic, PARAM_DB_CORE_TX_ANTENNAS));
}

int16 __MTLK_IFUNC
mtlk_scan_calc_tx_power_lim_wrapper(mtlk_handle_t usr_data, int8 spectrum_mode, uint8 reg_domain, uint8 channel)
{
    struct nic* nic = (struct nic*)usr_data;
    
    return mtlk_calc_tx_power_lim(&nic->slow_ctx->tx_limits, 
                                  channel,
                                  reg_domain,
                                  spectrum_mode,
                                  nic->slow_ctx->bonding,
                                  _mtlk_core_get_antennas_cnt(nic, PARAM_DB_CORE_TX_ANTENNAS));
}

int16 __MTLK_IFUNC
mtlk_get_antenna_gain_wrapper(mtlk_handle_t usr_data, uint8 channel)
{
    struct nic* nic = (struct nic*)usr_data;
    
    return mtlk_get_antenna_gain(&nic->slow_ctx->tx_limits, channel);
}

int __MTLK_IFUNC
mtlk_reload_tpc_wrapper (uint8 channel, mtlk_handle_t usr_data)
{
    struct nic* nic = (struct nic*)usr_data;
    
    return mtlk_reload_tpc(MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE),
        nic->slow_ctx->bonding,
        channel,
        mtlk_vap_get_txmm(nic->vap_handle),
        nic->txmm_async_eeprom_msgs,
        ARRAY_SIZE(nic->txmm_async_eeprom_msgs),
        mtlk_core_get_eeprom(nic));
}

void
mtlk_record_xmit_err(struct nic *nic, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);

  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_FORWARD)) {
/*    TODO: GS: nic->core_stats.tx_dropped++; check it. It's taken from MAC statistic for output
  } else  {*/
    nic->pstats.fwd_dropped++;
  }

  if (++nic->pstats.tx_cons_drop_cnt > nic->pstats.tx_max_cons_drop)
    nic->pstats.tx_max_cons_drop = nic->pstats.tx_cons_drop_cnt;
}

/*****************************************************************************
**
** NAME         mtlk_xmit
**
** PARAMETERS   nbuf                 Skbuff to transmit
**              dev                 Device context
**
** RETURNS      Skbuff transmission status
**
** DESCRIPTION  This function called to perform packet transmission.
**
******************************************************************************/
int
mtlk_xmit (mtlk_core_t* nic, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  int res = MTLK_ERR_PKT_DROPPED; 
  unsigned short ac = mtlk_qos_get_ac_by_tid(nbuf->priority);
  sta_entry *sta = NULL;
  uint32 ntx_free = 0;
  mtlk_hw_send_data_t data;
  const IEEE_ADDR *rcv_addr = NULL;
  IEEE_ADDR bssid;
  struct ethhdr *ether_header = (struct ethhdr *)nbuf->data;
  mtlk_pckt_filter_e sta_filter_stored;
  int bridge_mode;

  memset(&data, 0, sizeof(data));

  mtlk_dump(5, nbuf->data, nbuf->len, "nbuf->data received from OS");

  bridge_mode = MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE);

  /* For AP's unicast and reliable multicast if destination STA
   * id is known - get sta entry, otherwise - drop packet */
  if (__LIKELY(mtlk_nbuf_priv_check_flags(nbuf_priv, 
                                          MTLK_NBUFF_UNICAST | MTLK_NBUFF_RMCAST))) {
    sta = mtlk_nbuf_priv_get_dst_sta(nbuf_priv);
    if (__UNLIKELY(sta == NULL)) {
      ELOG_DY("CID-%04x: Destination STA (%Y) is not known!", mtlk_vap_get_oid(nic->vap_handle),
           ether_header->h_dest);
      goto tx_skip;
    }
  }
  /* In STA mode all packets are unicast -
   * so we always have peer entry in this mode */
  else if (!mtlk_vap_is_ap(nic->vap_handle)) {
    sta = mtlk_stadb_get_ap(&nic->slow_ctx->stadb); 
    if (__LIKELY(sta != NULL)) {
      mtlk_nbuf_priv_set_dst_sta(nbuf_priv, sta);
      mtlk_sta_decref(sta); /* De-reference of get_ap
                             * We may dereference STA since 
                             * it is referenced by packet now.
                             */
    }
  }

  if (NULL != sta) {
    sta_filter_stored = mtlk_sta_get_packets_filter(sta);
    if (MTLK_PCKT_FLTR_ALLOW_ALL == sta_filter_stored) {
      /* all packets are allowed */
      ;
    }
    else if ((MTLK_PCKT_FLTR_ALLOW_802_1X == sta_filter_stored) &&
              mtlk_wlan_pkt_is_802_1X(MTLK_ETH_GET_ETHER_TYPE(nbuf->data))) {
      /* only 802.1x packets are allowed */
      ;
    }
    else {
      if (MTLK_PCKT_FLTR_ALLOW_802_1X == sta_filter_stored)
        mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_EAPOL_FILTER);
      else
        mtlk_sta_on_packet_dropped(sta,  MTLK_TX_DISCARDED_DROP_ALL_FILTER);

      goto tx_skip;
    }
   }

  data.msg = mtlk_vap_get_hw_vft(nic->vap_handle)->get_msg_to_send(nic->vap_handle, &ntx_free);
  if (ntx_free == 0) { // Do not accept packets from OS anymore
    ILOG2_V("mtlk_xmit 0, call mtlk_flctrl_stop");
    mtlk_flctrl_stop_data(nic->hw_tx_flctrl, nic->flctrl_id);
  }

  if (!data.msg) {
    if(NULL != sta)
      mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_TX_QUEUE_OVERFLOW);
    else
      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW);

    ++nic->pstats.ac_dropped_counter[ac];
    nic->pstats.tx_overruns++;
    goto tx_skip;
  }

  ++nic->pstats.ac_used_counter[ac];

  ILOG4_P("got from hw_msg %p", data.msg);

  /* check if wds should be used:
   *  - WDS option must be enabled
   *  - destination STA must be known (i.e. unicast being sent)
   *  - 802.3 DA is not equal to destination STA's MAC address
   */
  if ((bridge_mode == BR_MODE_WDS) &&
      (sta != NULL) &&
      memcmp(mtlk_sta_get_addr(sta)->au8Addr, ether_header->h_dest, ETH_ALEN))
    data.wds = 1;

  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    /* always AP's MAC address on the STA */
    MTLK_CORE_HOT_PATH_PDB_GET_MAC(nic, PARAM_DB_CORE_BSSID, bssid.au8Addr);
    rcv_addr = &bssid;

  } else if (data.wds || mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_RMCAST)) {
    /* use RA on AP when WDS enabled or Reliable Multicast used */
    rcv_addr = mtlk_sta_get_addr(sta);

  } else {
    /* 802.3 DA on AP when WDS not needed */
    rcv_addr = (IEEE_ADDR *)ether_header->h_dest;
  }
  ILOG3_Y("RA: %Y", rcv_addr->au8Addr);

  data.nbuf            = nbuf;
  data.size            = nbuf->len;
  data.rcv_addr        = rcv_addr;
  data.access_category = (uint8)nbuf->priority;
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  data.rf_mgmt_data    = sta?mtlk_sta_get_rf_mgmt_data(sta):MTLK_RF_MGMT_DATA_DEFAULT;
#endif

  if (ntohs(ether_header->h_proto) <= ETH_DATA_LEN) {
    data.encap_type = ENCAP_TYPE_8022;
  } else {
    switch (ether_header->h_proto) {
    case __constant_htons(ETH_P_AARP):
    case __constant_htons(ETH_P_IPX):
      data.encap_type = ENCAP_TYPE_STT;
      break;
    default:
      data.encap_type = ENCAP_TYPE_RFC1042;
      break;
    }
  }

#ifdef MTCFG_PER_PACKET_STATS
  mtlk_nbuf_priv_stats_set(nbuf_priv, MTLK_NBUF_STATS_DATA_SIZE, data.size);
#endif
#if defined(MTCFG_PER_PACKET_STATS) && defined(MTCFG_TSF_TIMER_ACCESS_ENABLED)
  mtlk_nbuf_priv_stats_set(nbuf_priv, MTLK_NBUF_STATS_TS_FW_IN, mtlk_hw_get_timestamp(nic->vap_handle));
#endif

  res = mtlk_vap_get_hw_vft(nic->vap_handle)->send_data(mtlk_nbuf_priv_get_vap_handle(nbuf_priv), &data);
  if (__LIKELY(res == MTLK_ERR_OK)) {
#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
    if(mtlk_vap_is_ap(nic->vap_handle)) {
      mtlk_core_t *master_core = mtlk_core_get_master(nic);
      mtlk_aocs_msdu_tx_inc_nof_used(master_core->slow_ctx->aocs, ac);
      mtlk_aocs_on_tx_msdu_sent(master_core->slow_ctx->aocs,
                                ac,
                                mtlk_sq_get_limit(master_core->sq, ac),
                                mtlk_sq_get_qsize(master_core->sq, ac));
    }
#endif
    mtlk_df_ui_notify_tx_start(mtlk_vap_get_df(nic->vap_handle));
  }
  else {
    if(NULL != sta)
      mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_TX_QUEUE_OVERFLOW);
    else
      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW);
    goto tx_skip; /* will also release msg */
  }

  nic->pstats.sta_session_tx_packets++;
  nic->pstats.ac_tx_counter[ac]++;

  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_FORWARD)) {
    nic->pstats.fwd_tx_packets++;
    nic->pstats.fwd_tx_bytes += nbuf->len;
  }

  // reset consecutive drops counter
  nic->pstats.tx_cons_drop_cnt = 0;

  return MTLK_ERR_OK;

tx_skip:
  if (data.msg) {
    mtlk_vap_get_hw_vft(nic->vap_handle)->release_msg_to_send(nic->vap_handle, data.msg);
  }

  mtlk_record_xmit_err(nic, nbuf);

  return res;
}

static int
set_80211d_mibs (struct nic *nic, uint8 spectrum, uint16 channel)
{
  uint8 country_domain =
      country_code_to_domain(mtlk_core_get_country_code(nic));

  uint8 mitigation = mtlk_get_channel_mitigation(
      country_domain,
      mtlk_core_get_is_ht_cur(nic),
      spectrum,
      channel);
  
  ILOG3_DS("Setting MIB_COUNTRY for 0x%x domain with HT %s", country_domain, mtlk_core_get_is_ht_cur(nic) ? "enabled" : "disabled");
  mtlk_set_mib_value_uint8(mtlk_vap_get_txmm(nic->vap_handle), MIB_SM_MITIGATION_FACTOR, mitigation);
  mtlk_set_mib_value_uint8(mtlk_vap_get_txmm(nic->vap_handle), MIB_USER_POWER_SELECTION,
      MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_POWER_SELECTION));
  return mtlk_set_country_mib(mtlk_vap_get_txmm(nic->vap_handle), 
                              country_domain,
                              mtlk_core_get_is_ht_cur(nic),
                              mtlk_core_get_freq_band_cur(nic),
                              mtlk_vap_is_ap(nic->vap_handle),
                              country_code_to_country(mtlk_core_get_country_code(nic)),
                              mtlk_core_get_dot11d(nic));
}

#ifdef DEBUG_WPS
static char test_wps_ie0[] = {0xdd, 7, 0x00, 0x50, 0xf2, 0x04, 1, 2, 3};
static char test_wps_ie1[] = {0xdd, 7, 0x00, 0x50, 0xf2, 0x04, 4, 5, 6};
static char test_wps_ie2[] = {0xdd, 7, 0x00, 0x50, 0xf2, 0x04, 7, 8, 9};
#endif

static int
_mtlk_core_perform_initial_scan(mtlk_core_t *core, uint16 channel, BOOL rescan_exempted)
{
  MTLK_ASSERT(NULL != core);

  if (rescan_exempted && channel != 0) {
    ILOG0_DD("CID-%04x: in Rescan Exemption period (ch=%d): skipping the Initial Scan", mtlk_vap_get_oid(core->vap_handle),
             channel);
  }
  else if (channel == 0 || !mtlk_aocs_is_type_none(core->slow_ctx->aocs) || mtlk_20_40_is_feature_enabled(core->coex_20_40_sm)) {
    ILOG0_DDD("CID-%04x: AOCS is ON (ch=%d type=%d): doing the Initial Scan", mtlk_vap_get_oid(core->vap_handle),
      channel, mtlk_aocs_get_type(core->slow_ctx->aocs));

    if (mtlk_aocs_get_spectrum_mode(core->slow_ctx->aocs) == SPECTRUM_40MHZ) {
      ILOG0_D("CID-%04x: Initial scan started SPECTRUM_40MHZ", mtlk_vap_get_oid(core->vap_handle));
      /* perform CB scan to collect CB calibration data */
      if (mtlk_scan_sync(&core->slow_ctx->scan, mtlk_core_get_freq_band_cfg(core), SPECTRUM_40MHZ) != MTLK_ERR_OK) {
        ELOG_D("CID-%04x: Initial scan failed SPECTRUM_40MHZ", mtlk_vap_get_oid(core->vap_handle));
        return MTLK_ERR_SCAN_FAILED;
      }
    }

    ILOG0_D("CID-%04x: Initial scan started SPECTRUM_20MHZ", mtlk_vap_get_oid(core->vap_handle));
    if (mtlk_scan_sync(&core->slow_ctx->scan, mtlk_core_get_freq_band_cfg(core), SPECTRUM_20MHZ) != MTLK_ERR_OK) {
      ELOG_D("CID-%04x: Initial scan failed SPECTRUM_20MHZ", mtlk_vap_get_oid(core->vap_handle));
      return MTLK_ERR_SCAN_FAILED;
    }
  }
  else {
    ILOG0_DDDD("CID-%04x: AOCS is completely OFF (ch=%d type=%d 20/40=%d): skipping the Initial Scan", mtlk_vap_get_oid(core->vap_handle),
      channel, mtlk_aocs_get_type(core->slow_ctx->aocs), mtlk_20_40_is_feature_enabled(core->coex_20_40_sm));
  }
  return MTLK_ERR_OK;
}

static void
_mtlk_mbss_undo_preactivate (struct nic *nic)
{
  if (nic->aocs_started) {
    mtlk_aocs_stop(nic->slow_ctx->aocs);
    mtlk_aocs_stop_watchdog(nic->slow_ctx->aocs);
    nic->aocs_started = FALSE;
  }
}

static void
_mtlk_core_mbss_set_last_deactivate_ts (struct nic *nic)
{
  uint32 ts;

  MTLK_ASSERT(nic != NULL);

  ts = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  if (ts == INVALID_DEACTIVATE_TIMESTAMP) {
    ++ts;
  }

  mtlk_core_get_master(nic)->slow_ctx->deactivate_ts = ts;
}

static BOOL
_mtlk_core_mbss_is_rescan_exempted (struct nic *nic)
{
  BOOL res = FALSE;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(mtlk_vap_is_master(nic->vap_handle) == TRUE);
  MTLK_ASSERT(mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle)) == 0);
  MTLK_ASSERT(mtlk_vap_is_ap(nic->vap_handle) == TRUE);

  if (mtlk_aocs_is_type_none(nic->slow_ctx->aocs) == FALSE) {
    ILOG0_D("Re-Scan cannot be exempted (AOCS type = %d)", mtlk_aocs_get_type(nic->slow_ctx->aocs));
  }
  else if (_mtlk_core_is_20_40_active(nic) == TRUE) {
    ILOG0_V("Re-Scan cannot be exempted (20/40 is active)");
  }
  else {
    uint32 rescan_exemption_interval =
      MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_UP_RESCAN_EXEMPTION_TIME);

    ILOG3_DD("i=%u dts=%u", rescan_exemption_interval, nic->slow_ctx->deactivate_ts);

    if (rescan_exemption_interval &&
        nic->slow_ctx->deactivate_ts != INVALID_DEACTIVATE_TIMESTAMP) {
      uint32 now_ts = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
      uint32 delta  = (uint32)(now_ts - nic->slow_ctx->deactivate_ts);

      res = (delta < rescan_exemption_interval);

      ILOG3_DDD("rescan_exempted=%u nts=%u delta=%u", res, now_ts, delta);
    }
  }

  return res;
}

static int
_mtlk_mbss_send_preactivate (struct nic *nic, BOOL rescan_exempted)
{
  mtlk_get_channel_data_t params;
  uint8                   u8SwitchMode;
  mtlk_txmm_data_t        *man_entry = NULL;
  mtlk_txmm_msg_t         man_msg;
  int                     result = MTLK_ERR_OK;
  int                     channel;
  UMI_MBSS_PRE_ACTIVATE     *pPreActivate;
  UMI_MBSS_PRE_ACTIVATE_HDR *pPreActivateHeader;
  mtlk_aocs_evt_select_t  aocs_data;
  uint8                   ap_scan_band_cfg;
  int                     actual_spectrum_mode;
  int                     prog_model_spectrum_mode;

  MTLK_ASSERT(NULL != nic);


  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send PRE_ACTIVATE request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_MBSS_PRE_ACTIVATE_REQ;
  man_entry->payload_size = mtlk_get_umi_mbss_pre_activate_size ();

  memset(man_entry->payload, 0, man_entry->payload_size);
  pPreActivate = (UMI_MBSS_PRE_ACTIVATE *)(man_entry->payload);

  /*get data from Regulatory table:
    Availability Check Time,
    Scan Type
  */
  /************************************************************************/
  /* Add aocs initialization + loading of programming module                                                                     */
  /************************************************************************/
  actual_spectrum_mode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE);
  if ((actual_spectrum_mode == SPECTRUM_40MHZ) || (actual_spectrum_mode == SPECTRUM_AUTO)) {
    if (_mtlk_core_is_20_40_active(nic)) {
      actual_spectrum_mode = SPECTRUM_40MHZ;
      MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, actual_spectrum_mode);
      mtlk_aocs_set_auto_spectrum(nic->slow_ctx->aocs, SPECTRUM_40MHZ);
    }
  }
  mtlk_aocs_set_spectrum_mode(nic->slow_ctx->aocs, actual_spectrum_mode);
  ILOG0_D("CID-%04x: Pre-activation started with parameters:", mtlk_vap_get_oid(nic->vap_handle));
  mtlk_core_configuration_dump(nic);

  mtlk_dot11h_set_event(mtlk_core_get_dfs(nic), MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL);

  mtlk_aocs_set_config_is_ht(nic->slow_ctx->aocs, mtlk_core_get_is_ht_cur(nic));

  mtlk_aocs_set_config_frequency_band(nic->slow_ctx->aocs, mtlk_core_get_freq_band_cur(nic));

  /* build the AOCS channel's list now */
  if (nic->aocs_started == FALSE) {
    ILOG0_D("CID-%04x: Start AOCS", mtlk_vap_get_oid(nic->vap_handle));
    if (mtlk_aocs_start(nic->slow_ctx->aocs, FALSE, _mtlk_core_is_20_40_active(nic)) != MTLK_ERR_OK) {
      ELOG_D("CID-%04x: Failed to prepare AOCS for selection", mtlk_vap_get_oid(nic->vap_handle));
      result = MTLK_ERR_AOCS_FAILED;
      goto FINISH;
    }
    nic->aocs_started = TRUE;
  }

  /* now we have to perform an AP scan and update
   * the table after we have scan results. Do scan only in one band */
  ap_scan_band_cfg = mtlk_core_get_freq_band_cfg(nic);
  ILOG0_DD("CID-%04x: ap_scan_band_cfg = %d", mtlk_vap_get_oid(nic->vap_handle), ap_scan_band_cfg);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CFG,
      ((ap_scan_band_cfg == MTLK_HW_BAND_2_4_GHZ) ? MTLK_HW_BAND_2_4_GHZ : MTLK_HW_BAND_5_2_GHZ) );

  /* now select or validate the channel */
  channel = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR);
  /* TODO: WAVE300_SW-100: remove this after FW fix */
  /* Do not perform initial Scan in case this present */
#ifndef MBSS_FORCE_NO_AOCS_INITIAL_SELECTION
  result = _mtlk_core_perform_initial_scan(nic, channel, rescan_exempted);
  if (MTLK_ERR_OK != result) {
    goto FINISH;
  }
#endif

  aocs_data.channel = channel;
  aocs_data.reason = SWR_INITIAL_SELECTION;
  aocs_data.criteria = CHC_USERDEF;
  /* On initial channel selection we may use SM required channels */
  mtlk_aocs_enable_smrequired(nic->slow_ctx->aocs);
  result = mtlk_aocs_indicate_event(nic->slow_ctx->aocs, MTLK_AOCS_EVENT_SELECT_CHANNEL,
    (void*)&aocs_data, sizeof(aocs_data));
  /* After initial channel was selected we must never use sm required channels */
  mtlk_aocs_disable_smrequired(nic->slow_ctx->aocs);
  if (result == MTLK_ERR_OK)
    mtlk_aocs_indicate_event(nic->slow_ctx->aocs, MTLK_AOCS_EVENT_INITIAL_SELECTED,
      (void *)&aocs_data, sizeof(aocs_data));
  /* restore all after AP scan */
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CFG, ap_scan_band_cfg);

  /*
   * at this point spectrum may be changed by AOCS
   */

  /* after AOCS initial scan we must reload progmodel */
  prog_model_spectrum_mode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE);
  if (_mtlk_core_is_20_40_active(nic)) {
    prog_model_spectrum_mode = SPECTRUM_40MHZ;
    nic->slow_ctx->pm_params.u8SpectrumMode = prog_model_spectrum_mode;
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE, prog_model_spectrum_mode);
  }
  if (mtlk_progmodel_load(mtlk_vap_get_txmm(nic->vap_handle), nic, mtlk_core_get_freq_band_cfg(nic),
      prog_model_spectrum_mode) != 0) {
    ELOG_D("CID-%04x: Error while downloading progmodel files", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_UNKNOWN;
    goto FINISH;
  }
    
  mtlk_mib_update_pm_related_mibs(nic, &nic->slow_ctx->pm_params);
  mtlk_set_pm_related_params(mtlk_vap_get_txmm(nic->vap_handle), &nic->slow_ctx->pm_params);
  /* ----------------------------------- */
  /* restore MIB values in the MAC as they were before the AP scanning */
  if (mtlk_set_mib_values(nic) != 0) {
    ELOG_D("CID-%04x: Failed to set MIB values", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_UNKNOWN;
    goto FINISH;
  }

  /* now check AOCS result - here all state is already restored */
  if (result != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: aocs did not find available channel", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_AOCS_FAILED;
    goto FINISH;
  }
  /* update channel now */
  channel = aocs_data.channel;
  nic->slow_ctx->scan.last_channel = channel;
  MTLK_ASSERT(mtlk_aocs_get_cur_channel(nic->slow_ctx->aocs) == channel);

  /************************************************************************/
  /* End of aocs initialization.                                                                     */
  /************************************************************************/

  pPreActivateHeader = &pPreActivate->sHdr;
  pPreActivateHeader->u16Status = HOST_TO_MAC16(UMI_OK);
  pPreActivateHeader->u8_40mhzIntolerant = mtlk_20_40_is_intolerance_declared(mtlk_core_get_coex_sm(nic));
  pPreActivateHeader->u8_CoexistenceEnabled = _mtlk_core_is_20_40_active(nic);
  params.reg_domain = country_code_to_domain(mtlk_core_get_country_code(nic));
  params.is_ht = mtlk_core_get_is_ht_cur(nic);
  params.ap = mtlk_vap_is_ap(nic->vap_handle);
  params.spectrum_mode = actual_spectrum_mode;
  params.bonding = nic->slow_ctx->bonding;
  params.channel = channel;
  if ((pPreActivateHeader->u8_CoexistenceEnabled == TRUE) &&
      (pPreActivateHeader->u8_40mhzIntolerant == FALSE) &&
      (actual_spectrum_mode == SPECTRUM_40MHZ)) {
        uint8 secondary_channel_offset;
        switch ((signed char)params.bonding) {
          case ALTERNATE_LOWER:
            secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCB;
            break;
          case ALTERNATE_UPPER:
            secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCA;
            break;
          case ALTERNATE_NONE:
          default:
            secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCN;
            MTLK_ASSERT(0);
            break;
        }
        if (mtlk_20_40_is_20_40_operation_permitted(mtlk_core_get_coex_sm(nic),
                                                    params.channel,
                                                    secondary_channel_offset) == FALSE) {
        /* The channel pair found by the AOCS is not suitable for the 40 MHz operation */
          params.bonding = ALTERNATE_NONE;
          nic->slow_ctx->bonding = ALTERNATE_NONE;
          actual_spectrum_mode = SPECTRUM_20MHZ;
          MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, SPECTRUM_20MHZ);
          mtlk_20_40_set_intolerance_at_first_scan_flag(mtlk_core_get_coex_sm(nic), TRUE, FALSE /* must acquire the lock */);
          ILOG2_DD("The channel pair found by the AOCS (primary = %d, offset = %d) is not suitable for the 40 MHz operation\n", params.channel, secondary_channel_offset);
          ILOG2_V("Sending a 20 MHz pre-activation request");
        }
  }

  u8SwitchMode = mtlk_get_chnl_switch_mode(actual_spectrum_mode, nic->slow_ctx->bonding, 0);
  mtlk_channels_fill_mbss_pre_activate_req_ext_data(&params, nic, u8SwitchMode, man_entry->payload);

  set_80211d_mibs(nic, actual_spectrum_mode, channel);

  /* Set TPC calibration MIBs */
  mtlk_reload_tpc_wrapper(channel, HANDLE_T(nic));

  ILOG0_DD("CID-%04x: mtlk_txmm_msg_send_blocked, MsgId = 0x%x", mtlk_vap_get_oid(nic->vap_handle), man_entry->id);
  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send PRE_ACTIVATE request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (MAC_TO_HOST16(pPreActivateHeader->u16Status) != UMI_OK) {
    ELOG_DD("CID-%04x: Error returned for PRE_ACTIVATE request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), MAC_TO_HOST16(pPreActivateHeader->u16Status));
    result = MTLK_ERR_UMI;
    goto FINISH;
  }

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  if (result == MTLK_ERR_OK) {
    ILOG0_D("CID-%04x: _mtlk_mbss_send_preactivate returned successfully", mtlk_vap_get_oid(nic->vap_handle));
  }
  else {
    ELOG_D("CID-%04x: _mtlk_mbss_send_preactivate returned with error", mtlk_vap_get_oid(nic->vap_handle));
  }

  return result;
}

int
mtlk_mbss_send_vap_add (struct nic *nic)
{
  mtlk_txmm_data_t        *man_entry = NULL;
  mtlk_txmm_msg_t         man_msg;
  int                     result = MTLK_ERR_OK;
  UMI_VAP_DB_OP           *pAddRequest;


  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send ADD VAP request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_VAP_DB_REQ;
  man_entry->payload_size = sizeof (UMI_VAP_DB_OP);


  pAddRequest = (UMI_VAP_DB_OP *)(man_entry->payload);
  pAddRequest->u16Status = HOST_TO_MAC16(UMI_OK);
  pAddRequest->u8OperationCode = VAP_OPERATION_ADD;
  pAddRequest->u8VAPIdx = mtlk_vap_get_id(nic->vap_handle);

  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send ADD VAP request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (MAC_TO_HOST16(pAddRequest->u16Status) != UMI_OK) {
    ELOG_DD("CID-%04x: Error returned for VAP ADD request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), MAC_TO_HOST16(pAddRequest->u16Status));
    result = MTLK_ERR_UMI;
    goto FINISH;
  }
  
FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return result;
}

int
mtlk_mbss_send_vap_activate (struct nic *nic)
{
  mtlk_txmm_data_t* man_entry=NULL;
  mtlk_txmm_msg_t activate_msg;
  int essid_len;
  UMI_ACTIVATE_VAP *areq;
  int result = MTLK_ERR_OK;


  ILOG0_D("CID-%04x: Entering mtlk_mbss_send_vap_activate", mtlk_vap_get_oid(nic->vap_handle));

#ifdef DEBUG_WPS
  mtlk_core_set_gen_ie(nic, test_wps_ie0, sizeof(test_wps_ie0), 0);
  mtlk_core_set_gen_ie(nic, test_wps_ie2, sizeof(test_wps_ie2), 2);
#endif
  // Start activation request
  ILOG0_D("CID-%04x: Start activation", mtlk_vap_get_oid(nic->vap_handle));

  man_entry = mtlk_txmm_msg_init_with_empty_data(&activate_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL)
  {
    ELOG_D("CID-%04x: Can't send ACTIVATE request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    return result;
  }

  man_entry->id           = UM_MAN_ACTIVATE_VAP_REQ;
  man_entry->payload_size = sizeof (UMI_ACTIVATE_VAP);

  areq = (UMI_ACTIVATE_VAP*)(man_entry->payload);
  memset(areq, 0, sizeof(UMI_ACTIVATE_VAP));

  essid_len = sizeof(areq->sSSID.acESSID);
  result = mtlk_pdb_get_string(mtlk_vap_get_param_db(nic->vap_handle),
                               PARAM_DB_CORE_ESSID, areq->sSSID.acESSID, &essid_len);
  if (MTLK_ERR_OK != result) {
    ELOG_D("CID-%04x: ESSID parameter has wrong length", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }
  essid_len = strlen(areq->sSSID.acESSID);

  if (0 == essid_len) {
    ELOG_D("CID-%04x: ESSID is not set", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  mtlk_pdb_get_mac(
      mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_MAC_ADDR, areq->sBSSID.au8Addr);
  areq->sSSID.u8Length = essid_len;
  areq->isHiddenBssID = nic->slow_ctx->cfg.is_hidden_ssid;
  areq->u16BSStype = HOST_TO_MAC16(CFG_ACCESS_POINT);
  areq->u16Status = HOST_TO_MAC16(UMI_OK);

  ASSERT(sizeof(areq->sRSNie) == sizeof(nic->slow_ctx->rsnie));
  memcpy(&areq->sRSNie, &nic->slow_ctx->rsnie, sizeof(areq->sRSNie));

  if (mtlk_core_set_net_state(nic, NET_STATE_ACTIVATING) != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: Failed to switch core to state ACTIVATING", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  nic->activation_status = FALSE;

  mtlk_osal_event_reset(&nic->slow_ctx->connect_event);

  mtlk_dump(1, areq, sizeof(UMI_ACTIVATE_VAP), "dump of UMI_ACTIVATE_VAP:");
  ILOG0_D("CID-%04x: UMI_ACTIVATE_VAP", mtlk_vap_get_oid(nic->vap_handle));
  ILOG0_DY("CID-%04x: \tsBSSID = %Y", mtlk_vap_get_oid(nic->vap_handle), areq->sBSSID.au8Addr);
  ILOG0_DS("CID-%04x: \tsSSID = %s", mtlk_vap_get_oid(nic->vap_handle), areq->sSSID.acESSID);
  ILOG0_DD("CID-%04x: \tHidden = %d", mtlk_vap_get_oid(nic->vap_handle), areq->isHiddenBssID);
  mtlk_dump(0, areq->sRSNie.au8RsnIe, sizeof(areq->sRSNie.au8RsnIe), "\tsRSNie = "); 
  

  ILOG0_DDS("CID-%04x: mtlk_txmm_msg_send, MsgId = 0x%x, ESSID = %s", mtlk_vap_get_oid(nic->vap_handle), man_entry->id, areq->sSSID.acESSID);

  result = mtlk_txmm_msg_send_blocked(&activate_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Cannot send activate request due to TXMM err#%d", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (areq->u16Status != UMI_OK && areq->u16Status != UMI_ALREADY_ENABLED)
  {
    WLOG_DD("CID-%04x: Activate VAP request failed with code %d", mtlk_vap_get_oid(nic->vap_handle), areq->u16Status);
    result = MTLK_ERR_UNKNOWN;
    goto FINISH;
  }

  /* now wait and handle connection event if any */
  ILOG4_V("Timestamp before network status wait...");

  /* TODO: WAVE300_SW-100: remove this after FW fix */
#ifdef MBSS_FORCE_VAP_ACTIVATION_SUCCEEDED
  nic->activation_status = TRUE;
  result = MTLK_ERR_OK;
#else
  nic->activation_status = FALSE;
  result = mtlk_osal_event_wait(&nic->slow_ctx->connect_event, CONNECT_TIMEOUT);
#endif


  if (result == MTLK_ERR_TIMEOUT) {
    /* MAC is dead? Either fix MAC or increase timeout */
    ELOG_D("CID-%04x: Timeout reached while waiting for BSS activation event", mtlk_vap_get_oid(nic->vap_handle));
    mtlk_core_set_net_state(nic, NET_STATE_HALTED);
    goto CLEANUP;
  } else if(nic->activation_status) {
    mtlk_core_set_net_state(nic, NET_STATE_CONNECTED);
    if (mtlk_vap_is_master_ap(nic->vap_handle)) {
      result = mtlk_aocs_start_watchdog(nic->slow_ctx->aocs);
      if (result != MTLK_ERR_OK) {
        ELOG_DD("CID-%04x: Can't start AOCS watchdog: %d", mtlk_vap_get_oid(nic->vap_handle), result);
        mtlk_core_set_net_state(nic, NET_STATE_HALTED);
        goto CLEANUP;
      }
    }
  } else {
    mtlk_core_set_net_state(nic, NET_STATE_READY);
    goto CLEANUP;
  }

FINISH:
  if (result != MTLK_ERR_OK &&
    mtlk_core_get_net_state(nic) != NET_STATE_READY)
    mtlk_core_set_net_state(nic, NET_STATE_READY);

CLEANUP:
  mtlk_txmm_msg_cleanup(&activate_msg);
  return result;
}

static int
_mtlk_core_send_vap_deactivate_req_blocked(struct nic *nic)
{
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_DEACTIVATE_VAP  *psUmiDeactivate;
  int               net_state = mtlk_core_get_net_state(nic);
  int               result = MTLK_ERR_OK;
  uint8             mac_addr[ETH_ALEN];

  if (net_state != NET_STATE_HALTED) {
    result = mtlk_core_set_net_state(nic, NET_STATE_DISCONNECTING);
    if (result != MTLK_ERR_OK) {
      goto FINISH;
    }
  }

  clean_all_sta_on_disconnect(nic);
  if (net_state == NET_STATE_HALTED) {
    /* Do not send anything to halted MAC or if STA hasn't been connected */
    result = MTLK_ERR_OK;
    goto FINISH;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send DISCONNECT request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_DEACTIVATE_VAP_REQ;
  man_entry->payload_size = sizeof(UMI_DEACTIVATE_VAP);
  psUmiDeactivate         = (UMI_DEACTIVATE_VAP *)man_entry->payload;

  psUmiDeactivate->u16Status = HOST_TO_MAC16(UMI_OK);

  mtlk_dump(2, psUmiDeactivate, sizeof(UMI_DEACTIVATE_VAP), "dump of UMI_DEACTIVATE:");

  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send DEACTIVATE request to VAP (err=%d)", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (MAC_TO_HOST16(psUmiDeactivate->u16Status) != UMI_OK) {
    WLOG_DD("CID-%04x: Deactivation failed in FW (status=%u)", mtlk_vap_get_oid(nic->vap_handle),
      MAC_TO_HOST16(psUmiDeactivate->u16Status));
    result = MTLK_ERR_MAC;
    goto FINISH;
  }

  /* update disconnections statistics */
  nic->pstats.num_disconnects++;

  mtlk_pdb_get_mac(
      mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_MAC_ADDR, mac_addr);

  ILOG1_YD("Station %Y disconnected (status %u)", 
    mac_addr, MAC_TO_HOST16(MAC_TO_HOST16(psUmiDeactivate->u16Status)));

  result = MTLK_ERR_OK;

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return result;
}

static int
_mtlk_mbss_deactivate_vap(mtlk_core_t *nic)
{
  int res = MTLK_ERR_OK;

  mtlk_osal_event_reset(&nic->slow_ctx->vap_removed_event);
  res = _mtlk_core_send_vap_deactivate_req_blocked(nic);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Timeout reached while waiting for VAP deactivation - Asserting FW", mtlk_vap_get_oid(nic->vap_handle));
    goto finish;
  }

  ILOG1_V("Waiting for VAP removal event...");
  res = mtlk_osal_event_wait(&nic->slow_ctx->vap_removed_event,
                             VAP_REMOVAL_TIMEOUT);
  if(MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Timeout reached while waiting for VAP deletion indication from firmware - Asserting FW", mtlk_vap_get_oid(nic->vap_handle));
  }

finish:
  if (MTLK_ERR_OK != res) {
    res = MTLK_ERR_FW;
    mtlk_core_set_net_state(nic, NET_STATE_HALTED);
  }
  cleanup_on_disconnect(nic);

  return res;
}

int
mtlk_mbss_send_vap_delete (struct nic *nic)
{
  mtlk_txmm_data_t        *man_entry = NULL;
  mtlk_txmm_msg_t         man_msg;
  int                     result = MTLK_ERR_OK;
  UMI_VAP_DB_OP           *pRemoveRequest;


  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send Remove VAP request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_VAP_DB_REQ;
  man_entry->payload_size = sizeof (UMI_VAP_DB_OP);


  pRemoveRequest = (UMI_VAP_DB_OP *)(man_entry->payload);
  pRemoveRequest->u16Status = MTLK_ERR_OK;
  pRemoveRequest->u8OperationCode = VAP_OPERATION_DEL;
  pRemoveRequest->u8VAPIdx = mtlk_vap_get_id(nic->vap_handle);

  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send Remove VAP request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (pRemoveRequest->u16Status != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Error returned for VAP REMOVE request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), pRemoveRequest->u16Status);
    result = MTLK_ERR_UMI;
    goto FINISH;
  }

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return result;
}

static int
_mtlk_mbss_send_preactivate_if_needed(mtlk_core_t *core, BOOL rescan_exempted)
{
  int result = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != core);

  if (0 == mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(core->vap_handle))) {
    if (mtlk_vap_is_master_ap(core->vap_handle)) {
      result = _mtlk_mbss_send_preactivate(core, rescan_exempted);
    }
    else {
      result = _mtlk_mbss_send_preactivate(mtlk_core_get_master(core), rescan_exempted);
    }
  }
  return result;
}

static BOOL
_mtlk_core_mbss_check_activation_params (struct nic *nic)
{
  BOOL res = FALSE;
  int  essid_len;
  char essid[MIB_ESSID_LENGTH+1];

  MTLK_ASSERT(NULL != nic);

  essid_len = sizeof(essid);
  if (MTLK_ERR_OK != mtlk_pdb_get_string(mtlk_vap_get_param_db(nic->vap_handle),
                                         PARAM_DB_CORE_ESSID, essid, &essid_len)) {
    ELOG_D("CID-%04x: ESSID parameter has wrong length", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }
  essid[sizeof(essid) - 1] = '\0';
  essid_len = strlen(essid);

  if (0 == essid_len) {
    ELOG_D("CID-%04x: ESSID is not set", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  res = TRUE;

FINISH:
  return res;
}

int
mtlk_mbss_init (struct nic *nic, BOOL rescan_exempted)
{
  int result = MTLK_ERR_PARAMS;

  if (_mtlk_core_mbss_check_activation_params(nic) != TRUE) {
    ELOG_D("CID-%04x: mtlk_mbss_init: call to _mtlk_core_mbss_check_activation_params returned with error", mtlk_vap_get_oid(nic->vap_handle));
    goto error_params;
  }
  ILOG0_D("CID-%04x: _mtlk_core_mbss_check_activation_params returned successfully", mtlk_vap_get_oid(nic->vap_handle));

  result = _mtlk_mbss_send_preactivate_if_needed (nic, rescan_exempted);
  if (result != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: mtlk_mbss_init: call to _mtlk_mbss_send_preactivate_if_needed returned with error", mtlk_vap_get_oid(nic->vap_handle));
    goto error_preactivation;
  }
  ILOG0_D("CID-%04x: _mtlk_mbss_send_preactivate_if_needed returned successfully", mtlk_vap_get_oid(nic->vap_handle));

  result = mtlk_mbss_send_vap_activate (nic);
  if (result != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: mtlk_mbss_init: call to mtlk_mbss_send_vap_activate returned with error", mtlk_vap_get_oid(nic->vap_handle));
    goto error_activation;
  }
  ILOG0_D("CID-%04x: mtlk_mbss_send_vap_activate returned successfully", mtlk_vap_get_oid(nic->vap_handle));

  return MTLK_ERR_OK;

error_preactivation:
error_activation:
  ILOG0_D("CID-%04x: before _mtlk_mbss_undo_preactivate", mtlk_vap_get_oid(nic->vap_handle));
  if ((0 == mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle))))
  {
    _mtlk_mbss_undo_preactivate(mtlk_core_get_master(nic));
  }
  ILOG0_D("CID-%04x: after _mtlk_mbss_undo_preactivate", mtlk_vap_get_oid(nic->vap_handle));

error_params:
  return result;
}

static int
mtlk_send_activate (struct nic *nic)
{
  mtlk_txmm_data_t* man_entry=NULL;
  mtlk_txmm_msg_t activate_msg;
  int channel, bss_type, essid_len;
  UMI_ACTIVATE_HDR *areq;
  int result = MTLK_ERR_OK;
  uint8 u8SwitchMode;
  BOOL aocs_started = FALSE;
  mtlk_get_channel_data_t params;

  MTLK_ASSERT(!mtlk_vap_is_ap(nic->vap_handle));

  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    bss_type = CFG_INFRA_STATION;
  } else {
    bss_type = CFG_ACCESS_POINT;
  }

#ifdef DEBUG_WPS
  mtlk_core_set_gen_ie(nic, test_wps_ie0, sizeof(test_wps_ie0), 0);
  mtlk_core_set_gen_ie(nic, test_wps_ie2, sizeof(test_wps_ie2), 2);
#endif
  // Start activation request
  ILOG2_D("CID-%04x: Start activation", mtlk_vap_get_oid(nic->vap_handle));

  man_entry = mtlk_txmm_msg_init_with_empty_data(&activate_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL)
  {
    ELOG_D("CID-%04x: Can't send ACTIVATE request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    return result;
  }
  
  man_entry->id           = UM_MAN_ACTIVATE_REQ;
  man_entry->payload_size = mtlk_get_umi_activate_size();

  areq = mtlk_get_umi_activate_hdr(man_entry->payload);
  memset(areq, 0, sizeof(UMI_ACTIVATE_HDR));

  channel = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR);
  /* for AP channel 0 means "use AOCS", but for STA channel must be set
     implicitly - we cannot send 0 to MAC in activation request */
  if ((channel == 0) && !mtlk_vap_is_master_ap(nic->vap_handle)) {
    ELOG_D("CID-%04x: Channel must be specified for station or Virtual AP", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  essid_len = sizeof(areq->sSSID.acESSID);
  result = mtlk_pdb_get_string(mtlk_vap_get_param_db(nic->vap_handle),
                               PARAM_DB_CORE_ESSID, areq->sSSID.acESSID, &essid_len);
  if (MTLK_ERR_OK != result) {
    ELOG_D("CID-%04x: ESSID parameter has wrong length", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }
  essid_len = strlen(areq->sSSID.acESSID);

  if (0 == essid_len) {
    ELOG_D("CID-%04x: ESSID is not set", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  /* Do not allow to activate if BSSID isn't set for the STA. Probably it
   * is worth to not allow this on AP as well? */
  if (!mtlk_vap_is_ap(nic->vap_handle) &&
      (0 == MTLK_CORE_HOT_PATH_PDB_CMP_MAC(nic, PARAM_DB_CORE_BSSID, mtlk_osal_eth_zero_addr))) {
    ELOG_D("CID-%04x: BSSID is not set", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    _mtlk_core_sta_country_code_set_default_on_activate(nic);
  }

  mtlk_pdb_get_mac(
      mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID, areq->sBSSID.au8Addr);
  areq->sSSID.u8Length = essid_len;
  areq->u16RestrictedChannel = cpu_to_le16(channel);
  areq->u16BSStype = cpu_to_le16(bss_type);
  areq->isHiddenBssID = nic->slow_ctx->cfg.is_hidden_ssid;

  ILOG0_D("CID-%04x: Pre-activation started with parameters:", mtlk_vap_get_oid(nic->vap_handle));
  mtlk_core_configuration_dump(nic);

  if (mtlk_vap_is_master_ap(nic->vap_handle)) {
    mtlk_aocs_set_spectrum_mode(nic->slow_ctx->aocs, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE));
  }

  mtlk_dot11h_set_event(mtlk_core_get_dfs(nic), MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL);
  
  if (mtlk_vap_is_master_ap(nic->vap_handle)) {
    mtlk_aocs_set_config_is_ht(nic->slow_ctx->aocs, mtlk_core_get_is_ht_cur(nic));
    mtlk_aocs_set_config_frequency_band(nic->slow_ctx->aocs, mtlk_core_get_freq_band_cur(nic));
  }

  if (mtlk_vap_is_master_ap(nic->vap_handle)) {
    mtlk_aocs_evt_select_t aocs_data;
    uint8 ap_scan_band_cfg;

    /* build the AOCS channel's list now */
    if (mtlk_aocs_start(nic->slow_ctx->aocs, FALSE, _mtlk_core_is_20_40_active(nic)) != MTLK_ERR_OK) {
      ELOG_D("CID-%04x: Failed to prepare AOCS for selection", mtlk_vap_get_oid(nic->vap_handle));
      result = MTLK_ERR_AOCS_FAILED;
      goto FINISH;
    }

    aocs_started = TRUE;

    /* now we have to perform an AP scan and update
     * the table after we have scan results. Do scan only in one band */
    ap_scan_band_cfg = mtlk_core_get_freq_band_cfg(nic);
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CFG,
        ((ap_scan_band_cfg == MTLK_HW_BAND_2_4_GHZ) ? MTLK_HW_BAND_2_4_GHZ : MTLK_HW_BAND_5_2_GHZ) );

    result = _mtlk_core_perform_initial_scan(nic, channel, FALSE);
    if (MTLK_ERR_OK != result) {
      goto FINISH;
    }

    /* now select or validate the channel */
    aocs_data.channel = channel;
    aocs_data.reason = SWR_INITIAL_SELECTION;
    aocs_data.criteria = CHC_USERDEF;
    /* On initial channel selection we may use SM required channels */
    mtlk_aocs_enable_smrequired(nic->slow_ctx->aocs);
    result = mtlk_aocs_indicate_event(nic->slow_ctx->aocs, MTLK_AOCS_EVENT_SELECT_CHANNEL,
      (void*)&aocs_data, sizeof(aocs_data));
    /* After initial channel was selected we must never use sm required channels */
    mtlk_aocs_disable_smrequired(nic->slow_ctx->aocs);
    if (result == MTLK_ERR_OK)
      mtlk_aocs_indicate_event(nic->slow_ctx->aocs, MTLK_AOCS_EVENT_INITIAL_SELECTED,
        (void *)&aocs_data, sizeof(aocs_data));
    /* restore all after AP scan */
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CFG, ap_scan_band_cfg );

    /*
     * at this pint spectrum may be changed by AOCS
     */

    /* after AOCS initial scan we must reload progmodel */
    if (!_mtlk_core_is_20_40_active(nic)) {
      if (mtlk_progmodel_load(mtlk_vap_get_txmm(nic->vap_handle), nic, mtlk_core_get_freq_band_cfg(nic),
          MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE)) != 0) {
        ELOG_D("CID-%04x: Error while downloading progmodel files", mtlk_vap_get_oid(nic->vap_handle));
        result = MTLK_ERR_UNKNOWN;
        goto FINISH;
      }
    }
    
    mtlk_mib_update_pm_related_mibs(nic, &nic->slow_ctx->pm_params);
    mtlk_set_pm_related_params(mtlk_vap_get_txmm(nic->vap_handle), &nic->slow_ctx->pm_params);
    /* ----------------------------------- */
    /* restore MIB values in the MAC as they were before the AP scanning */
    if (mtlk_set_mib_values(nic) != 0) {
      ELOG_D("CID-%04x: Failed to set MIB values", mtlk_vap_get_oid(nic->vap_handle));
      result = MTLK_ERR_UNKNOWN;
      goto FINISH;
    }
    /* now check AOCS result - here all state is already restored */
    if (result != MTLK_ERR_OK) {
      ELOG_D("CID-%04x: aocs did not find available channel", mtlk_vap_get_oid(nic->vap_handle));
      result = MTLK_ERR_AOCS_FAILED;
      goto FINISH;
    }
    /* update channel now */
    channel = aocs_data.channel;
    nic->slow_ctx->scan.last_channel = channel;
    MTLK_ASSERT(mtlk_aocs_get_cur_channel(nic->slow_ctx->aocs) == channel);
  }

  u8SwitchMode = mtlk_get_chnl_switch_mode(MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE), nic->slow_ctx->bonding, 0);
 
  /*get data from Regulatory table:
    Availability Check Time,
    Scan Type
  */

  ILOG2_DD("CurrentSpectrumMode = %d\n"
          "RFSpectrumMode = %d", 
          MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE),
          MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE));
 
  params.reg_domain = country_code_to_domain(mtlk_core_get_country_code(nic));
  params.is_ht = mtlk_core_get_is_ht_cur(nic);
  params.ap = mtlk_vap_is_ap(nic->vap_handle);
  params.spectrum_mode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE);
  params.bonding = nic->slow_ctx->bonding;
  params.channel = channel;

  mtlk_channels_fill_activate_req_ext_data(&params, nic, u8SwitchMode, man_entry->payload);

  /*TODO- add SmRequired to 11d table !!*/
  /*
  ILOG1_SSDY("activating (mode:%s, essid:\"%s\", chan:%d, bssid %Y)...",
     bss_type_str, nic->slow_ctx->essid, channel, nic->slow_ctx->bssid);
  */

  /*********************** END NEW **********************************/
  
  set_80211d_mibs(nic, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE), channel);

  /* Set TPC calibration MIBs */
  mtlk_reload_tpc_wrapper(channel, HANDLE_T(nic));

  ASSERT(sizeof(areq->sRSNie) == sizeof(nic->slow_ctx->rsnie));
  memcpy(&areq->sRSNie, &nic->slow_ctx->rsnie, sizeof(areq->sRSNie));

  if (mtlk_core_set_net_state(nic, NET_STATE_ACTIVATING) != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: Failed to switch core to state ACTIVATING", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  nic->activation_status = FALSE;
  mtlk_osal_event_reset(&nic->slow_ctx->connect_event);

  mtlk_dump(3, areq, sizeof(UMI_ACTIVATE_HDR), "dump of UMI_ACTIVATE_HDR:");

  result = mtlk_txmm_msg_send_blocked(&activate_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Cannot send activate request due to TXMM err#%d", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (areq->u16Status != UMI_OK && areq->u16Status != UMI_ALREADY_ENABLED)
  {
    WLOG_DD("CID-%04x: Activate VAP request failed with code %d", mtlk_vap_get_oid(nic->vap_handle), areq->u16Status);
    result = MTLK_ERR_UNKNOWN;
    goto FINISH;
  }

  /* now wait and handle connection event if any */
  ILOG4_V("Timestamp before network status wait...");

  result = mtlk_osal_event_wait(&nic->slow_ctx->connect_event, CONNECT_TIMEOUT);

  if (result == MTLK_ERR_TIMEOUT) {
    /* MAC is dead? Either fix MAC or increase timeout */
    ELOG_DS("CID-%04x: Timeout reached while waiting for %s event - Asserting FW", mtlk_vap_get_oid(nic->vap_handle),
        mtlk_vap_is_ap(nic->vap_handle) ? "BSS creation" : "connection");
    mtlk_vap_get_hw_vft(nic->vap_handle)->set_prop(nic->vap_handle, MTLK_HW_RESET, NULL, 0);
    goto CLEANUP;
  } else if(nic->activation_status) {
    mtlk_core_set_net_state(nic, NET_STATE_CONNECTED);
    if (mtlk_vap_is_master_ap(nic->vap_handle)) {
      result = mtlk_aocs_start_watchdog(nic->slow_ctx->aocs);
      if (result != MTLK_ERR_OK) {
        ELOG_DD("CID-%04x: Can't start AOCS watchdog: %d", mtlk_vap_get_oid(nic->vap_handle), result);
        mtlk_core_set_net_state(nic, NET_STATE_HALTED);
        goto CLEANUP;
      }
    }
  } else {
    ELOG_D("CID-%04x: Activate failed. Switch to NET_STATE_READY", mtlk_vap_get_oid(nic->vap_handle));
    mtlk_core_set_net_state(nic, NET_STATE_READY);
    goto CLEANUP;
  }

FINISH:
  if (result != MTLK_ERR_OK &&
      mtlk_core_get_net_state(nic) != NET_STATE_READY)
      mtlk_core_set_net_state(nic, NET_STATE_READY);

CLEANUP:
  mtlk_txmm_msg_cleanup(&activate_msg);

  if (result != MTLK_ERR_OK && aocs_started)
    mtlk_aocs_stop(nic->slow_ctx->aocs);

  return result;
}

static int __MTLK_IFUNC
_mtlk_core_connect_sta(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  int res = MTLK_ERR_OK;
  bss_data_t bss_found;
  int freq;
  mtlk_aux_pm_related_params_t pm_params;
  uint8 *addr;
  uint32 addr_size;
  uint32 new_spectrum_mode;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  addr = mtlk_clpb_enum_get_next(clpb, &addr_size);
  MTLK_ASSERT(NULL != addr);

  if (mtlk_vap_is_ap(nic->vap_handle)) {
    res = MTLK_ERR_NOT_SUPPORTED;
    goto end_activation;
  }

  if (  (mtlk_core_get_net_state(nic) != NET_STATE_READY)
      || mtlk_core_scan_is_running(nic)
      || mtlk_core_is_stopping(nic)) {
    ILOG1_V("Can't connect to AP - unappropriated state");
    res = MTLK_ERR_NOT_READY;
    goto end_activation;
  }

  if (mtlk_cache_find_bss_by_bssid(&nic->slow_ctx->cache, addr, &bss_found, NULL) == 0) {
    ILOG1_V("Can't connect to AP - unknown BSS");
    res = MTLK_ERR_PARAMS;
    goto end_activation;
  } 
  /* update regulation limits for the BSS */
  if (mtlk_core_get_dot11d(nic)) {
    if (!bss_found.country_ie) {
      struct country_ie_t *country_ie;

      /* AP we are connecting to has no Country IE - use from the first found */
      country_ie = mtlk_cache_find_first_country_ie(&nic->slow_ctx->cache,
        mtlk_core_get_country_code(nic));
      if (country_ie) {
        ILOG1_V("Updating regulation limits from the first found BSS's country IE");
        mtlk_update_reg_limit_table(HANDLE_T(nic), country_ie, bss_found.power);
        mtlk_osal_mem_free(country_ie);
      }
    } else {
      ILOG1_V("Updating regulation limits from the current BSS's country IE");
      mtlk_update_reg_limit_table(HANDLE_T(nic), bss_found.country_ie, bss_found.power);
    }

    _mtlk_core_sta_country_code_update_on_connect(nic, bss_found.country_code);
  }

  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR, bss_found.channel);
  /* save ESSID */
  res = mtlk_pdb_set_string(mtlk_vap_get_param_db(nic->vap_handle),
                            PARAM_DB_CORE_ESSID, bss_found.essid);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: Can't store ESSID (err=%d)", mtlk_vap_get_oid(nic->vap_handle), res);
    goto end_activation;
  }
  /* save BSSID so we can use it on activation */
  mtlk_pdb_set_mac(mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID, addr);
  /* set bonding according to the AP */
  nic->slow_ctx->bonding = bss_found.upper_lower;

  /* set current frequency band */
  freq = channel_to_band(bss_found.channel);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CUR, freq);

  /* set current HT capabilities */
  if (BSS_IS_WEP_ENABLED(&bss_found) && nic->slow_ctx->wep_enabled) {
    /* no HT is allowed for WEP connections */
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_IS_HT_CUR, FALSE);
  }
  else {
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_IS_HT_CUR,
        (mtlk_core_get_is_ht_cfg(nic) && bss_found.is_ht));
  }

  /* set TKIP */
  nic->slow_ctx->is_tkip = 0;
  if (mtlk_core_get_is_ht_cur(nic) && nic->slow_ctx->rsnie.au8RsnIe[0]) {
    struct wpa_ie_data d;
    ASSERT(nic->slow_ctx->rsnie.au8RsnIe[1]+2 <= sizeof(nic->slow_ctx->rsnie));
    if (wpa_parse_wpa_ie(nic->slow_ctx->rsnie.au8RsnIe,
      nic->slow_ctx->rsnie.au8RsnIe[1] + 2, &d) < 0) {
        ELOG_D("CID-%04x: Can not parse WPA/RSN IE", mtlk_vap_get_oid(nic->vap_handle));
        res = MTLK_ERR_PARAMS;
        goto end_activation;
    }
    if (d.pairwise_cipher == UMI_RSN_CIPHER_SUITE_TKIP) {
      WLOG_D("CID-%04x: Connection in HT mode with TKIP is prohibited by standard, trying non-HT mode...", mtlk_vap_get_oid(nic->vap_handle));
      MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_IS_HT_CUR, FALSE);
      nic->slow_ctx->is_tkip = 1;
    }
  }

  /* for STA spectrum mode should be set according to our own HT capabilities */
  if (mtlk_core_get_is_ht_cur(nic) == 0) {
    /* e.g. if we connect to A/N AP, but STA is A then we should select 20MHz  */
    new_spectrum_mode = SPECTRUM_20MHZ;
  } else {
    new_spectrum_mode = bss_found.spectrum;

    if (SPECTRUM_40MHZ == bss_found.spectrum) {
      uint32 sta_force_spectrum_mode =
          MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_STA_FORCE_SPECTRUM_MODE);
      /* force set spectrum mode */
      if (   (SPECTRUM_20MHZ == sta_force_spectrum_mode)
          || (   (SPECTRUM_AUTO == sta_force_spectrum_mode)
              && (MTLK_HW_BAND_2_4_GHZ == mtlk_core_get_freq_band_cur(nic)))) {

        new_spectrum_mode = SPECTRUM_20MHZ;
      }
    }
  }
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, new_spectrum_mode);
  ILOG1_DS("CID-%04x: Set SpectrumMode: %s MHz", mtlk_vap_get_oid(nic->vap_handle), new_spectrum_mode ? "40": "20");

  /* previously set network mode shouldn't be overridden,
   * but in case it "MTLK_HW_BAND_BOTH" it need to be recalculated, this value is not
   * acceptable for MAC! */
  if(MTLK_HW_BAND_BOTH == net_mode_to_band(mtlk_core_get_network_mode_cur(nic))) {
     MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_NET_MODE_CUR,
         get_net_mode(freq, mtlk_core_get_is_ht_cur(nic)));
  }

  /* perform CB scan, but only once per band */
  if (!(nic->cb_scanned_bands & 
    (freq == MTLK_HW_BAND_2_4_GHZ ? CB_SCANNED_2_4 : CB_SCANNED_5_2)))
  {
    /* perform CB scan to collect CB calibration data */
    if ((res = mtlk_scan_sync(&nic->slow_ctx->scan, freq, SPECTRUM_40MHZ)) != MTLK_ERR_OK) {
      res=  MTLK_ERR_NOT_READY;
      goto end_activation;
    }

    /* add the band to CB scanned ones */
    nic->cb_scanned_bands |= (freq == MTLK_HW_BAND_2_4_GHZ ? CB_SCANNED_2_4 : CB_SCANNED_5_2);
  }

  if (!_mtlk_core_is_20_40_active(nic)) {
    res = mtlk_progmodel_load(mtlk_vap_get_txmm(nic->vap_handle), nic, freq,
             MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE));
    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: Can't configure progmodel for connection (err=%d)", mtlk_vap_get_oid(nic->vap_handle), res);
      goto end_activation;
    }
  }

  res = mtlk_aux_pm_related_params_set_bss_based(
    mtlk_vap_get_txmm(nic->vap_handle),
    &bss_found,
    mtlk_core_get_network_mode_cur(nic),
    MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE),
    &pm_params);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: Can't set PM related params (err=%d)", mtlk_vap_get_oid(nic->vap_handle), res);
    goto end_activation;
  }

  mtlk_mib_update_pm_related_mibs(nic, &pm_params);

  if (mtlk_send_activate(nic) != MTLK_ERR_OK) {
    res = MTLK_ERR_NOT_READY;
  }

end_activation:
  if (MTLK_ERR_OK != res) {
    /* rollback network mode */
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_NET_MODE_CUR, mtlk_core_get_network_mode_cfg(nic));
  }
  return res;
}

static uint32
_mtlk_core_get_max_stas_supported_by_fw (mtlk_core_t *nic)
{
  uint32 res = DEFAULT_MAX_STAs_SUPPORTED;

  if (mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_FW_CAPS_MAX_STAs, &res, sizeof(res)) != MTLK_ERR_OK) {
    WLOG_V("Cannot get MAX STAs supported by FW. Forcing default");
    res = DEFAULT_MAX_STAs_SUPPORTED;
  }

  ILOG1_D("MAX STAs supported by FW: %d", res);

  return res;
}

static uint32
_mtlk_core_get_max_vaps_supported_by_fw (mtlk_core_t *nic)
{
  uint32 res = DEFAULT_MAX_VAPs_SUPPORTED;

  if (mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_FW_CAPS_MAX_VAPs, &res, sizeof(res)) != MTLK_ERR_OK) {
    WLOG_V("Cannot get MAX VAPs supported by FW. Forcing default");
    res = DEFAULT_MAX_VAPs_SUPPORTED;
  }

  ILOG1_D("MAX VAPs supported by FW: %d", res);

  return res;
}

static BOOL __MTLK_IFUNC
_mtlk_core_sta_inactivity_on (mtlk_handle_t    usr_data,
                              const sta_entry *sta)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, usr_data);

  MTLK_UNREFERENCED_PARAM(sta);

  return mtlk_flctrl_is_data_flowing(nic->hw_tx_flctrl);
}

static void __MTLK_IFUNC
_mtlk_core_on_sta_inactive (mtlk_handle_t    usr_data,
                            const sta_entry *sta)
{
  struct nic      *nic = HANDLE_T_PTR(struct nic, usr_data);
  const IEEE_ADDR *addr = mtlk_sta_get_addr(sta);

  ILOG1_Y("Disconnecting %Y due to data timeout", addr);

  if (mtlk_vap_is_ap(nic->vap_handle))
    _mtlk_core_schedule_disconnect_sta(nic, addr, FM_STATUSCODE_INACTIVITY, NULL, NULL);
  else
    _mtlk_core_schedule_disconnect_me(nic, FM_STATUSCODE_INACTIVITY);
}

static void __MTLK_IFUNC
_mtlk_core_on_sta_keepalive (mtlk_handle_t  usr_data,
                             sta_entry     *sta)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, usr_data);

  mtlk_send_null_packet(nic, sta);
}

static int __MTLK_IFUNC
_mtlk_core_get_ap_capabilities (mtlk_handle_t hcore, 
                                const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_card_capabilities_t card_capabilities;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(mtlk_vap_is_master_ap(nic->vap_handle));
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  MTLK_CFG_SET_ITEM(&card_capabilities, max_stas_supported, _mtlk_core_get_max_stas_supported_by_fw(nic));
  MTLK_CFG_SET_ITEM(&card_capabilities, max_vaps_supported, _mtlk_core_get_max_vaps_supported_by_fw(nic));

  return mtlk_clpb_push(clpb, &card_capabilities, sizeof(card_capabilities));
}

static int
_mtlk_core_ap_set_initial_channel (mtlk_core_t *nic, BOOL *rescan_exempted)
{
  int                     res = MTLK_ERR_UNKNOWN;
  mtlk_get_channel_data_t param;
  uint16                  channel;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(rescan_exempted != NULL);
  MTLK_ASSERT(mtlk_vap_is_master(nic->vap_handle) == TRUE);
  MTLK_ASSERT(mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle)) == 0);
  MTLK_ASSERT(mtlk_vap_is_ap(nic->vap_handle) == TRUE);

  *rescan_exempted = _mtlk_core_mbss_is_rescan_exempted(nic);

  if (0 == mtlk_core_get_country_code(nic)) {
    ELOG_D("CID-%04x: Failed to open - Country not specified", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto end;
  }

  param.reg_domain          = country_code_to_domain(mtlk_core_get_country_code(nic));
  param.is_ht               = mtlk_core_get_is_ht_cfg(nic);
  param.ap                  = mtlk_vap_is_ap(nic->vap_handle);
  param.bonding             = nic->slow_ctx->bonding;
  param.spectrum_mode       = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE);
  param.frequency_band      = mtlk_core_get_freq_band_cfg(nic);
  param.disable_sm_channels = mtlk_eeprom_get_disable_sm_channels(mtlk_core_get_eeprom(nic));

  channel = (*rescan_exempted == TRUE && MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CFG) == 0)?
      MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR):
      MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CFG);

  if (   (0 != channel)
      && (MTLK_ERR_OK != mtlk_check_channel(&param, channel)) ) {
    ELOG_DD("CID-%04x: Channel (%i) is not supported in current configuration.",
        mtlk_vap_get_oid(nic->vap_handle), channel);
    mtlk_core_configuration_dump(nic);
    res = MTLK_ERR_PARAMS;
    goto end;
  }

  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR, channel);

  if (   (CFG_BASIC_RATE_SET_LEGACY == MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_BASIC_RATE_SET))
      && (MTLK_HW_BAND_2_4_GHZ != mtlk_core_get_freq_band_cfg(nic))) {
    ILOG1_D("CID-%04x: Forcing BasicRateSet to default", mtlk_vap_get_oid(nic->vap_handle));
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_BASIC_RATE_SET, CFG_BASIC_RATE_SET_DEFAULT);
  }

  res = MTLK_ERR_OK;

end:
  return res;
}

static int __MTLK_IFUNC
_mtlk_core_activate(mtlk_handle_t hcore, 
                    const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  int res;
  sta_db_cfg_t sta_db_cfg;
  BOOL  rescan_exempted = FALSE;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  ILOG1_D("CID-%04x: open interface", mtlk_vap_get_oid(nic->vap_handle));

  if (  (mtlk_core_get_net_state(nic) != NET_STATE_READY)
      || mtlk_core_scan_is_running(nic)
      || mtlk_core_is_stopping(nic)) {
    ELOG_D("CID-%04x: Failed to open - inappropriate state", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto end;
  }

  if (mtlk_vap_is_ap(nic->vap_handle) &&
      mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle)) == 0) {
    res = _mtlk_core_ap_set_initial_channel(mtlk_core_get_master(nic), &rescan_exempted);
    if (res != MTLK_ERR_OK) {
      ELOG_D("Cannot set initial channel (%d)", res);
      goto end;
    }
  }

  mtlk_set_mib_values(nic);
  mtlk_addba_reconfigure(&nic->slow_ctx->addba, &nic->slow_ctx->cfg.addba);

  if (mtlk_vap_is_ap(nic->vap_handle))
  {
    res =  mtlk_mbss_init(nic, rescan_exempted);
    if ((res == MTLK_ERR_OK) &&
        _mtlk_core_is_20_40_active(nic) &&
        0 == mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle))) {
          eCSM_STATES mode_to_start = CSM_STATE_20;
          if (MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE) == SPECTRUM_40MHZ) {
            mode_to_start = CSM_STATE_20_40;
          }
          res = mtlk_20_40_start(mtlk_core_get_coex_sm(nic), mode_to_start, nic->wss);
          /* Note: even though the coexistence state machine is started, it doesn't mean it 
             will eventually try to jump to 40 MHz. The SM remembers the user-chosen spectrum
             mode and will refrain from moving to 40 MHz if the user limited it to 20 MHz */
    }
    if (MTLK_ERR_OK != res)
    {
      ELOG_D("CID-%04x: Failed to activate the core", mtlk_vap_get_oid(nic->vap_handle));
      goto end;
    }
  }
  else
  {
    if (0 < mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle)))
    {
      ELOG_D("CID-%04x: STA has been already activated", mtlk_vap_get_oid(nic->vap_handle));
      res = MTLK_ERR_PROHIB;
      goto end;
    }
    res = mtlk_send_activate(nic);
    /* don't handle error code */
    res = MTLK_ERR_OK;
  }
  mtlk_vap_manager_notify_vap_activated(mtlk_vap_get_manager(nic->vap_handle));

  /* interface is up - start timers */
  sta_db_cfg.api.usr_data          = HANDLE_T(nic);
  sta_db_cfg.api.sta_inactivity_on = _mtlk_core_sta_inactivity_on;
  sta_db_cfg.api.on_sta_inactive   = _mtlk_core_on_sta_inactive;
  sta_db_cfg.api.on_sta_keepalive  = _mtlk_core_on_sta_keepalive;

  sta_db_cfg.addba        = &nic->slow_ctx->addba;
  sta_db_cfg.sq           = nic->sq;
  sta_db_cfg.max_nof_stas = _mtlk_core_get_max_stas_supported_by_fw(nic);
  sta_db_cfg.parent_wss   = nic->wss;
  mtlk_stadb_start(&nic->slow_ctx->stadb, &sta_db_cfg);
  nic->is_stopped = FALSE;
end:
  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_deactivate(mtlk_handle_t hcore, 
                      const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  int net_state = mtlk_core_get_net_state(nic);
  int res = MTLK_ERR_OK;
  int deactivate_res  = MTLK_ERR_OK;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(0 != mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle)));

  if ((FALSE == nic->is_iface_stopping) && !mtlk_vap_is_slave_ap(nic->vap_handle)) {
    scan_terminate(&nic->slow_ctx->scan);
  }

  mtlk_osal_lock_acquire(&nic->net_state_lock);
  nic->is_iface_stopping = TRUE;
  mtlk_osal_lock_release(&nic->net_state_lock);

  if (!can_disconnect_now(nic)) {
     res = MTLK_ERR_NOT_READY;
     goto FINISH;
  }

  if ((net_state == NET_STATE_CONNECTED) ||
      (net_state == NET_STATE_HALTED)) /* for cleanup after exception */  {

    /* disconnect STA(s) */
    reset_security_stuff(nic);

    if (mtlk_vap_is_ap (nic->vap_handle)) 
    {
      deactivate_res = _mtlk_mbss_deactivate_vap(nic);
      /* do undo_preactivate, if last active vap is left only */
      if ((1 == mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle))))
      {
         _mtlk_mbss_undo_preactivate(mtlk_core_get_master(nic));
      }
    }
    else
    {
      /* Deactivate STA*/
      _mtlk_core_send_disconnect_req_blocked(nic, NULL, FM_STATUSCODE_USER_REQUEST);
    }
  }

  mtlk_stadb_stop(&nic->slow_ctx->stadb);
  /* Clearing cache */
  mtlk_cache_clear(&nic->slow_ctx->cache);

  mtlk_osal_lock_acquire(&nic->net_state_lock);
  nic->is_iface_stopping = FALSE;
  nic->is_stopped = TRUE;
  mtlk_osal_lock_release(&nic->net_state_lock);

  mtlk_vap_manager_notify_vap_deactivated(mtlk_vap_get_manager(nic->vap_handle));
  ILOG1_D("CID-%04x: interface is stopped", mtlk_vap_get_oid(nic->vap_handle));

  if ((0 == mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle))))
  {
    mtlk_20_40_stop(mtlk_core_get_coex_sm(nic));
  }

  if (mtlk_vap_is_master_ap (nic->vap_handle)) { // re-enable in case we disabled during channel switch
    mtlk_core_abilities_enable_vap_ops(nic->vap_handle);
  }

FINISH:
  _mtlk_core_mbss_set_last_deactivate_ts(nic);

  /*
    If deactivate_res indicates an error - we must make sure
    that the close function will not reiterate. Therefore, we return
    specific error code in this case. 
  */
  if (MTLK_ERR_OK != deactivate_res)
    res = deactivate_res;
  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

int __MTLK_IFUNC
_mtlk_core_set_mac_addr (mtlk_core_t *nic, const char *mac)
{
  int res = MTLK_ERR_UNKNOWN;

  /* Validate MAC address */
  if (!mtlk_osal_is_valid_ether_addr(mac)) {
    ELOG_DY("CID-%04x: The MAC %Y is invalid", mtlk_vap_get_oid(nic->vap_handle), mac);
    res = MTLK_ERR_PARAMS;
    goto FINISH;
  }

#ifdef MTCFG_LINDRV_HW_AHBG35
  // WLN-37 Same wireless MAC addresses configured for AP and STA
  // Temporary workaround for static EEPROM side effect
  ((char*)mac)[5] += mtlk_vap_is_ap(nic->vap_handle) ? 0x26 : 0x28;
#endif

  if (!mtlk_vap_is_slave_ap(nic->vap_handle)) {
    MIB_VALUE uValue = {0};
    /* Try to send value to the MAC */
    mtlk_osal_copy_eth_addresses(uValue.au8ListOfu8.au8Elements, mac);

    res = mtlk_set_mib_value_raw(mtlk_vap_get_txmm(nic->vap_handle), 
                                 MIB_IEEE_ADDRESS, &uValue);
    if (res != MTLK_ERR_OK) {
      ELOG_D("CID-%04x: Can't set MIB_IEEE_ADDRESS", mtlk_vap_get_oid(nic->vap_handle));
      goto FINISH;
    } 
  }

  mtlk_pdb_set_mac(mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_MAC_ADDR, mac);
  ILOG1_DY("CID-%04x: New MAC: %Y", mtlk_vap_get_oid(nic->vap_handle), mac);
  
  res = MTLK_ERR_OK;
  
FINISH:
  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_mac_addr_wrapper (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  char* mac_addr;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  mac_addr = mtlk_clpb_enum_get_next(clpb, NULL);
  MTLK_ASSERT(NULL != mac_addr);

  return _mtlk_core_set_mac_addr(nic, mac_addr);
}

static int __MTLK_IFUNC
_mtlk_core_get_mac_addr (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  uint8 mac_addr[ETH_ALEN];

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mtlk_pdb_get_mac(mtlk_vap_get_param_db(core->vap_handle), PARAM_DB_CORE_MAC_ADDR, mac_addr);
  return mtlk_clpb_push(clpb, mac_addr, sizeof(mac_addr));
}

static void
_mtlk_core_reset_stats_internal(mtlk_core_t *core)
{
  if (mtlk_vap_is_ap(core->vap_handle)) {
    mtlk_stadb_reset_cnts(&core->slow_ctx->stadb);
  }

  memset(&core->pstats, 0, sizeof(core->pstats));
}

static int __MTLK_IFUNC
_mtlk_core_reset_stats (mtlk_handle_t hcore, 
                        const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if (mtlk_core_get_net_state(nic) != NET_STATE_HALTED)
  {
    ELOG_D("CID-%04x: Can not reset stats when core is active", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
  }
  else
  {
    _mtlk_core_reset_stats_internal(nic);
    res = _mtlk_core_mac_reset_stats(nic);
  }

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_l2nat_clear_table(mtlk_handle_t hcore,
                             const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_UNREFERENCED_PARAM(data);

  mtlk_l2nat_clear_table(nic);

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_addba_cfg (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  uint32 i;
  mtlk_addba_cfg_entity_t addba_cfg_entity;
  mtlk_addba_cfg_t *addba_cfg = &(HANDLE_T_PTR(mtlk_core_t, hcore)->slow_ctx->cfg.addba);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&addba_cfg_entity, 0, sizeof(addba_cfg_entity));

  for (i=0; i<MTLK_ADDBA_NOF_TIDs; i++) {
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], use_aggr, addba_cfg->tid[i].use_aggr);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], accept_aggr, addba_cfg->tid[i].accept_aggr);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], max_nof_packets, addba_cfg->tid[i].max_nof_packets);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], max_nof_bytes, addba_cfg->tid[i].max_nof_bytes);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], timeout_interval, addba_cfg->tid[i].timeout_interval);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], min_packet_size_in_aggr, addba_cfg->tid[i].min_packet_size_in_aggr);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], addba_timeout, addba_cfg->tid[i].addba_timeout);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], aggr_win_size, addba_cfg->tid[i].aggr_win_size);
  }

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &addba_cfg_entity, sizeof(addba_cfg_entity));
  }

  return res;
}

static int
_mtlk_core_verify_assign_aggr_max_num_of_packets(uint16 in_max_nof_packets, uint16 *dst_max_nof_packets, uint32 tid)
{
  uint32 res = MTLK_ERR_OK;
  uint16 ac = mtlk_qos_get_ac_by_tid(tid);
  static uint8 max_nof_packets[] = 
  {
    NO_MAX_PACKETS_AGG_SUPPORTED_BE,
    NO_MAX_PACKETS_AGG_SUPPORTED_BK,
    NO_MAX_PACKETS_AGG_SUPPORTED_VI,
    NO_MAX_PACKETS_AGG_SUPPORTED_VO
  };

  MTLK_ASSERT(ARRAY_SIZE(max_nof_packets) == NTS_PRIORITIES);
  MTLK_ASSERT(ac < ARRAY_SIZE(max_nof_packets));

  if (in_max_nof_packets > max_nof_packets[ac]) {
    WLOG_DDD ("Invalid max aggr: %d %d %d", tid, in_max_nof_packets, max_nof_packets[ac]);
    res = MTLK_ERR_PARAMS;
  }
  MTLK_ASSERT(dst_max_nof_packets != NULL);

  if (res == MTLK_ERR_OK) {
    *dst_max_nof_packets = in_max_nof_packets;
  }
  return res;
}

static int
_mtlk_core_check_and_set_addba_cfg_field(mtlk_addba_cfg_entity_t *src_addba, mtlk_addba_cfg_t *dst_addba)
{
  uint32 i;
  uint32 res = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != src_addba);
  MTLK_ASSERT(NULL != dst_addba);

  for (i=0; i<MTLK_ADDBA_NOF_TIDs; i++) {
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], use_aggr, dst_addba->tid[i].use_aggr);
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], accept_aggr, dst_addba->tid[i].accept_aggr);
    MTLK_CFG_GET_ITEM_BY_FUNC(&src_addba->tid[i], max_nof_packets, _mtlk_core_verify_assign_aggr_max_num_of_packets, (src_addba->tid[i].max_nof_packets, &dst_addba->tid[i].max_nof_packets, i), res);
    if (res != MTLK_ERR_OK) {
      return res;  
    }
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], max_nof_bytes, dst_addba->tid[i].max_nof_bytes);
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], timeout_interval, dst_addba->tid[i].timeout_interval);
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], min_packet_size_in_aggr, dst_addba->tid[i].min_packet_size_in_aggr);
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], addba_timeout, dst_addba->tid[i].addba_timeout);
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], aggr_win_size, dst_addba->tid[i].aggr_win_size);
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_addba_cfg (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  uint32 res;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_addba_cfg_entity_t *addba_cfg = NULL;
  uint32 addba_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  addba_cfg = mtlk_clpb_enum_get_next(clpb, &addba_cfg_size);

  MTLK_ASSERT(NULL != addba_cfg);
  MTLK_ASSERT(sizeof(*addba_cfg) == addba_cfg_size);

  res = _mtlk_core_check_and_set_addba_cfg_field(addba_cfg, &core->slow_ctx->cfg.addba);

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_wme_cfg(mtlk_clpb_t *clpb, wme_cfg_t *src_wme_cfg)
{
  uint32 res = MTLK_ERR_OK;
  uint32 i;
  mtlk_wme_cfg_entity_t wme_cfg_entity;

  MTLK_ASSERT(NULL != clpb);
  MTLK_ASSERT(NULL != src_wme_cfg);

  memset(&wme_cfg_entity, 0, sizeof(wme_cfg_entity));

  for (i=0; i<NTS_PRIORITIES; i++) {
    MTLK_CFG_SET_ITEM(&wme_cfg_entity.wme_class[i], cwmin, src_wme_cfg->wme_class[i].cwmin);
    MTLK_CFG_SET_ITEM(&wme_cfg_entity.wme_class[i], cwmax, src_wme_cfg->wme_class[i].cwmax);
    MTLK_CFG_SET_ITEM(&wme_cfg_entity.wme_class[i], aifsn, src_wme_cfg->wme_class[i].aifsn);
    MTLK_CFG_SET_ITEM(&wme_cfg_entity.wme_class[i], txop, src_wme_cfg->wme_class[i].txop);
  }
  
  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &wme_cfg_entity, sizeof(wme_cfg_entity));
  }
  
  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_wme_bss_cfg (mtlk_handle_t hcore, 
                            const void* data, uint32 data_size)
{
  wme_cfg_t *wme_bss_cfg = &(HANDLE_T_PTR(mtlk_core_t, hcore)->slow_ctx->cfg.wme_bss);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  return _mtlk_core_get_wme_cfg(clpb, wme_bss_cfg);
}

static int __MTLK_IFUNC
_mtlk_core_get_wme_ap_cfg (mtlk_handle_t hcore, 
                           const void* data, uint32 data_size)
{
  wme_cfg_t *wme_ap_cfg = &(HANDLE_T_PTR(mtlk_core_t, hcore)->slow_ctx->cfg.wme_ap);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  return _mtlk_core_get_wme_cfg(clpb, wme_ap_cfg);
}

static int
_mtlk_core_check_and_set_wme_cfg_field(mtlk_wme_cfg_entity_t *src_wme, wme_cfg_t *dst_wme)
{
  uint32 i;

  MTLK_ASSERT(NULL != src_wme);
  MTLK_ASSERT(NULL != dst_wme);

  for (i=0; i<NTS_PRIORITIES; i++) {
    MTLK_CFG_GET_ITEM(&src_wme->wme_class[i], cwmin, dst_wme->wme_class[i].cwmin);
    MTLK_CFG_GET_ITEM(&src_wme->wme_class[i], cwmax, dst_wme->wme_class[i].cwmax);
    MTLK_CFG_GET_ITEM(&src_wme->wme_class[i], aifsn, dst_wme->wme_class[i].aifsn);
    MTLK_CFG_GET_ITEM(&src_wme->wme_class[i], txop, dst_wme->wme_class[i].txop);
  }

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_set_wme_bss_cfg (mtlk_handle_t hcore, 
                            const void* data, uint32 data_size)
{
  uint32 res;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_wme_cfg_entity_t *wme_cfg = NULL;
  uint32 wme_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  wme_cfg = mtlk_clpb_enum_get_next(clpb, &wme_cfg_size);

  MTLK_ASSERT(NULL != wme_cfg);
  MTLK_ASSERT(sizeof(*wme_cfg) == wme_cfg_size);

  res = _mtlk_core_check_and_set_wme_cfg_field(wme_cfg, &core->slow_ctx->cfg.wme_bss);

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_set_wme_ap_cfg (mtlk_handle_t hcore, 
                           const void* data, uint32 data_size)
{
  uint32 res;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_wme_cfg_entity_t *wme_cfg = NULL;
  uint32 wme_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  wme_cfg = mtlk_clpb_enum_get_next(clpb, &wme_cfg_size);

  MTLK_ASSERT(NULL != wme_cfg);
  MTLK_ASSERT(sizeof(*wme_cfg) == wme_cfg_size);

  res = _mtlk_core_check_and_set_wme_cfg_field(wme_cfg, &core->slow_ctx->cfg.wme_ap);

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_aocs_cfg (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_aocs_cfg_t aocs_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_aocs_t *aocs = core->slow_ctx->aocs;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&aocs_cfg, 0, sizeof(aocs_cfg));

  MTLK_CFG_SET_ITEM(&aocs_cfg, weight_ch_load, mtlk_aocs_get_weight(aocs, AOCS_WEIGHT_IDX_CL));
  MTLK_CFG_SET_ITEM(&aocs_cfg, weight_nof_bss, mtlk_aocs_get_weight(aocs, AOCS_WEIGHT_IDX_BSS));
  MTLK_CFG_SET_ITEM(&aocs_cfg, weight_tx_power, mtlk_aocs_get_weight(aocs, AOCS_WEIGHT_IDX_TX));
  MTLK_CFG_SET_ITEM(&aocs_cfg, weight_sm_required, mtlk_aocs_get_weight(aocs, AOCS_WEIGHT_IDX_SM));
  MTLK_CFG_SET_ITEM(&aocs_cfg, scan_aging_ms, mtlk_aocs_get_scan_aging(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, confirm_rank_aging_ms, mtlk_aocs_get_confirm_rank_aging(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, cfm_rank_sw_threshold, mtlk_aocs_get_cfm_rank_sw_threshold(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, alpha_filter_coefficient, mtlk_aocs_get_afilter(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, bonding, mtlk_core_get_bonding(core));
  MTLK_CFG_SET_ITEM(&aocs_cfg, use_tx_penalties, mtlk_aocs_get_penalty_enabled(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, udp_aocs_window_time_ms, mtlk_aocs_get_win_time(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, udp_lower_threshold, mtlk_aocs_get_lower_threshold(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, udp_threshold_window, mtlk_aocs_get_threshold_window(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, udp_msdu_debug_enabled, mtlk_aocs_get_msdu_debug_enabled(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, type, mtlk_aocs_get_type(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, udp_msdu_per_window_threshold, mtlk_aocs_get_msdu_win_thr(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, udp_msdu_threshold_aocs, mtlk_aocs_get_msdu_threshold(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, tcp_measurement_window, mtlk_aocs_get_measurement_window(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, tcp_throughput_threshold, mtlk_aocs_get_troughput_threshold(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, dbg_non_occupied_period, mtlk_aocs_get_dbg_non_occupied_period(aocs));
  MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(&aocs_cfg, restricted_channels, mtlk_aocs_get_restricted_ch,
                                        (aocs, aocs_cfg.restricted_channels));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&aocs_cfg, msdu_tx_ac, mtlk_aocs_get_msdu_tx_ac, (aocs, &aocs_cfg.msdu_tx_ac));
  
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&aocs_cfg, msdu_rx_ac, mtlk_aocs_get_msdu_rx_ac, (aocs, &aocs_cfg.msdu_rx_ac));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &aocs_cfg, sizeof(aocs_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_aocs_cfg (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_aocs_t *aocs = core->slow_ctx->aocs;
  mtlk_aocs_cfg_t *aocs_cfg = NULL;
  uint32 aocs_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  aocs_cfg = mtlk_clpb_enum_get_next(clpb, &aocs_cfg_size);

  MTLK_ASSERT(NULL != aocs_cfg);
  MTLK_ASSERT(sizeof(*aocs_cfg) == aocs_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, weight_ch_load, mtlk_aocs_set_weight,
                               (aocs, AOCS_WEIGHT_IDX_CL, aocs_cfg->weight_ch_load), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, weight_tx_power, mtlk_aocs_set_weight,
                               (aocs, AOCS_WEIGHT_IDX_TX, aocs_cfg->weight_tx_power), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, weight_nof_bss, mtlk_aocs_set_weight,
                               (aocs, AOCS_WEIGHT_IDX_BSS, aocs_cfg->weight_nof_bss), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, weight_sm_required, mtlk_aocs_set_weight,
                               (aocs, AOCS_WEIGHT_IDX_SM, aocs_cfg->weight_sm_required), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, scan_aging_ms, mtlk_aocs_set_scan_aging,
                               (aocs, aocs_cfg->scan_aging_ms), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, confirm_rank_aging_ms, mtlk_aocs_set_confirm_rank_aging,
                               (aocs, aocs_cfg->confirm_rank_aging_ms), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, cfm_rank_sw_threshold, mtlk_aocs_set_cfm_rank_sw_threshold,
                               (aocs, aocs_cfg->cfm_rank_sw_threshold), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, alpha_filter_coefficient, mtlk_aocs_set_afilter,
                               (aocs, aocs_cfg->alpha_filter_coefficient), res);

  /* TODO: GS: Move it to Core cfg */
  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, bonding, mtlk_core_set_bonding,
                               (core, aocs_cfg->bonding), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, use_tx_penalties, mtlk_aocs_set_penalty_enabled,
                               (aocs, aocs_cfg->use_tx_penalties), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, udp_aocs_window_time_ms, mtlk_aocs_set_win_time,
                               (aocs, aocs_cfg->udp_aocs_window_time_ms), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, udp_lower_threshold, mtlk_aocs_set_lower_threshold,
                               (aocs, aocs_cfg->udp_lower_threshold), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, udp_threshold_window, mtlk_aocs_set_threshold_window,
                               (aocs, aocs_cfg->udp_threshold_window), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, udp_msdu_debug_enabled, mtlk_aocs_set_msdu_debug_enabled,
                               (aocs, aocs_cfg->udp_msdu_debug_enabled), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, type, mtlk_aocs_set_type,
                               (aocs, aocs_cfg->type), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, udp_msdu_per_window_threshold, mtlk_aocs_set_msdu_win_thr,
                               (aocs, aocs_cfg->udp_msdu_per_window_threshold), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, udp_msdu_threshold_aocs, mtlk_aocs_set_msdu_threshold,
                               (aocs, aocs_cfg->udp_msdu_threshold_aocs), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, tcp_measurement_window, mtlk_aocs_set_measurement_window,
                               (aocs, aocs_cfg->tcp_measurement_window), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, tcp_throughput_threshold, mtlk_aocs_set_troughput_threshold,
                               (aocs, aocs_cfg->tcp_throughput_threshold), res);

  MTLK_CFG_GET_ITEM_BY_FUNC_VOID(aocs_cfg, dbg_non_occupied_period, mtlk_aocs_set_dbg_non_occupied_period,
                                (aocs, aocs_cfg->dbg_non_occupied_period));

  MTLK_CFG_GET_ITEM_BY_FUNC_VOID(aocs_cfg, restricted_channels, mtlk_aocs_set_restricted_ch,
                                   (aocs, aocs_cfg->restricted_channels));

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, msdu_tx_ac, mtlk_aocs_set_msdu_tx_ac,
                               (aocs, &aocs_cfg->msdu_tx_ac), res);
  
  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, msdu_rx_ac, mtlk_aocs_set_msdu_rx_ac,
                               (aocs, &aocs_cfg->msdu_rx_ac), res);
  
  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, penalties, mtlk_aocs_set_tx_penalty,
                               (aocs, aocs_cfg->penalties, MTLK_AOCS_PENALTIES_BUFSIZE), res);

MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_dot11h_ap_cfg (mtlk_handle_t hcore, 
                              const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_11h_ap_cfg_t dot11h_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  memset(&dot11h_cfg, 0, sizeof(dot11h_cfg));

  MTLK_CFG_SET_ITEM(&dot11h_cfg, debugChannelSwitchCount,
                     mtlk_dot11h_get_dbg_channel_switch_count(mtlk_core_get_dfs(core)));

  MTLK_CFG_SET_ITEM(&dot11h_cfg, debugChannelAvailabilityCheckTime,
                     mtlk_dot11h_get_dbg_channel_availability_check_time(mtlk_core_get_dfs(core)));

  MTLK_CFG_SET_ITEM(&dot11h_cfg, next_channel,
                    mtlk_dot11h_get_debug_next_channel(mtlk_core_get_dfs(core)));

  if (mtlk_vap_is_master_ap(core->vap_handle)) {
    MTLK_CFG_SET_ITEM(&dot11h_cfg, enable_sm_required,
                      !mtlk_aocs_is_smrequired_disabled(core->slow_ctx->aocs));
  }
  else {
    MTLK_CFG_SET_ITEM(&dot11h_cfg, enable_sm_required, FALSE);
  }

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &dot11h_cfg, sizeof(dot11h_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_dot11h_cfg (mtlk_handle_t hcore, 
                           const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_11h_cfg_t dot11h_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  memset(&dot11h_cfg, 0, sizeof(dot11h_cfg));

  MTLK_CFG_SET_ITEM(&dot11h_cfg, radar_detect,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_DFS_RADAR_DETECTION));

  MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(&dot11h_cfg, status, mtlk_dot11h_status,
                                        (mtlk_core_get_dfs(core), dot11h_cfg.status, MTLK_DOT11H_STATUS_BUFSIZE));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &dot11h_cfg, sizeof(dot11h_cfg));
  }

  return res;
}

static int
_mtlk_core_debug_emulate_radar_detection(mtlk_core_t *core, uint16 channel)
{
  MTLK_ASSERT(mtlk_vap_is_master_ap(core->vap_handle));
  if (mtlk_core_get_net_state(core) != NET_STATE_CONNECTED) {
    WLOG_D("CID-%04x: Can't switch channel - inappropriate state", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_UNKNOWN;
  }

  return mtlk_dot11h_debug_event(mtlk_core_get_dfs(core), MTLK_DFS_EVENT_RADAR_DETECTED, channel);
}

static int
_mtlk_core_debug_switch_channel(mtlk_core_t *core, uint16 channel)
{
  MTLK_ASSERT(mtlk_vap_is_master_ap(core->vap_handle));
  if (mtlk_core_get_net_state(core) != NET_STATE_CONNECTED) {
    WLOG_D("CID-%04x: Can't switch channel - inappropriate state", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_UNKNOWN;
  }

  return mtlk_dot11h_debug_event(mtlk_core_get_dfs(core), MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL, channel);
}

static int
_mtlk_core_set_sm_required(mtlk_core_t *core, BOOL enable_sm_required)
{
  MTLK_ASSERT(mtlk_vap_is_master_ap(core->vap_handle));

  if (TRUE == enable_sm_required) {
    mtlk_aocs_enable_smrequired(core->slow_ctx->aocs);
  } else {
    mtlk_aocs_disable_smrequired(core->slow_ctx->aocs);
  }
  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_set_dot11h_ap_cfg (mtlk_handle_t hcore, 
                              const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_11h_ap_cfg_t *dot11h_cfg = NULL;
  uint32 dot11h_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  dot11h_cfg = mtlk_clpb_enum_get_next(clpb, &dot11h_cfg_size);

  MTLK_ASSERT(NULL != dot11h_cfg);
  MTLK_ASSERT(sizeof(*dot11h_cfg) == dot11h_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()

  MTLK_CFG_CHECK_ITEM_AND_CALL(dot11h_cfg, channel_emu, _mtlk_core_debug_emulate_radar_detection,
                               (core, dot11h_cfg->channel_emu), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(dot11h_cfg, channel_switch, _mtlk_core_debug_switch_channel,
                               (core, dot11h_cfg->channel_switch), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(dot11h_cfg, enable_sm_required, _mtlk_core_set_sm_required,
                               (core, dot11h_cfg->enable_sm_required), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(dot11h_cfg, debugChannelSwitchCount, mtlk_dot11h_set_dbg_channel_switch_count,
                                    (mtlk_core_get_dfs(core), dot11h_cfg->debugChannelSwitchCount));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(dot11h_cfg, debugChannelAvailabilityCheckTime, mtlk_dot11h_set_dbg_channel_availability_check_time,
                                    (mtlk_core_get_dfs(core), dot11h_cfg->debugChannelAvailabilityCheckTime));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(dot11h_cfg, next_channel, mtlk_dot11h_set_debug_next_channel,
                                    (mtlk_core_get_dfs(core), dot11h_cfg->next_channel));

MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_set_dot11h_cfg (mtlk_handle_t hcore, 
                           const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_11h_cfg_t *dot11h_cfg = NULL;
  uint32 dot11h_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  dot11h_cfg = mtlk_clpb_enum_get_next(clpb, &dot11h_cfg_size);

  MTLK_ASSERT(NULL != dot11h_cfg);
  MTLK_ASSERT(sizeof(*dot11h_cfg) == dot11h_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()

  #ifdef MBSS_FORCE_NO_CHANNEL_SWITCH
    if (0 != dot11h_cfg->radar_detect) {
      res = MTLK_ERR_NOT_SUPPORTED;
      break;
    }
  #endif

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(dot11h_cfg, radar_detect, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_DFS_RADAR_DETECTION, dot11h_cfg->radar_detect));

MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int
_mtlk_core_get_antennas(mtlk_core_t *core, mtlk_pdb_id_t id_array, char *val)
{
  mtlk_pdb_size_t size = MTLK_NUM_ANTENNAS_BUFSIZE;
  int err = MTLK_ERR_OK;
  uint8 i = 0;
  uint8 val_array[MTLK_NUM_ANTENNAS_BUFSIZE];
  
  err = MTLK_CORE_PDB_GET_BINARY(core, id_array, val_array, &size);
  if (MTLK_ERR_OK != err)
  {
    MTLK_ASSERT(MTLK_ERR_OK == err); /* Can not retrieve antennas configuration from PDB */
    return err;
  }

  memset(val, '0', MTLK_NUM_ANTENNAS_BUFSIZE);
  val[MTLK_NUM_ANTENNAS_BUFSIZE - 1] = '\0';
  
  /* convert integer array in to the zero terminated character array */
  for (i = 0; i < (MTLK_NUM_ANTENNAS_BUFSIZE - 1); i++)
  {
    if (0 == val_array[i])
      break;
    
    val[i] = val_array[i] + '0';
  }
  
  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_get_mibs_cfg (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_mibs_cfg_t mibs_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&mibs_cfg, 0, sizeof(mibs_cfg));

  MTLK_CFG_SET_ITEM(&mibs_cfg, calibr_algo_mask, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_CALIBRATION_ALGO_MASK));
  MTLK_CFG_SET_ITEM(&mibs_cfg, power_increase, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_POWER_INCREASE));
  MTLK_CFG_SET_ITEM(&mibs_cfg, short_cyclic_prefix, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX));
  MTLK_CFG_SET_ITEM(&mibs_cfg, short_preamble_option, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_PREAMBLE));
  MTLK_CFG_SET_ITEM(&mibs_cfg, short_slot_time_option, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_SLOT_TIME));
  MTLK_CFG_SET_ITEM(&mibs_cfg, tx_power, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_TX_POWER));

  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, acl_mode, mtlk_get_mib_acl_mode,
                                     MIB_ACL_MODE, &mibs_cfg.acl_mode, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, current_tx_antenna, mtlk_get_mib_value_uint16, 
                                     MIB_CURRENT_TX_ANTENNA, &mibs_cfg.current_tx_antenna, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, beacon_period, mtlk_get_mib_value_uint16,
                                     MIB_BEACON_PERIOD, &mibs_cfg.beacon_period, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, disconnect_on_nacks_weight, mtlk_get_mib_value_uint16,
                                     MIB_DISCONNECT_ON_NACKS_WEIGHT, &mibs_cfg.disconnect_on_nacks_weight, core);

  if (mtlk_core_scan_is_running(core)) {
    ILOG1_D("%CID-%04x: Request eliminated due to running scan", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto err_get;
  } else {
    MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, long_retry_limit, mtlk_get_mib_value_uint16,
                                       MIB_LONG_RETRY_LIMIT, &mibs_cfg.long_retry_limit, core);
    MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, short_retry_limit, mtlk_get_mib_value_uint16,
                                       MIB_SHORT_RETRY_LIMIT, &mibs_cfg.short_retry_limit, core);
    MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, tx_msdu_lifetime, mtlk_get_mib_value_uint16,
                                       MIB_TX_MSDU_LIFETIME, &mibs_cfg.tx_msdu_lifetime, core);
    MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, rts_threshold, mtlk_get_mib_value_uint16,
                                       MIB_RTS_THRESHOLD, &mibs_cfg.rts_threshold, core);
  }

  MTLK_CFG_SET_ITEM(&mibs_cfg, sm_enable, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SM_ENABLE));

  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, advanced_coding_supported, mtlk_get_mib_value_uint8,
                                     MIB_ADVANCED_CODING_SUPPORTED, &mibs_cfg.advanced_coding_supported, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, overlapping_protect_enabled, mtlk_get_mib_value_uint8,
                                     MIB_OVERLAPPING_PROTECTION_ENABLE, &mibs_cfg.overlapping_protect_enabled, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, ofdm_protect_method, mtlk_get_mib_value_uint8,
                                     MIB_OFDM_PROTECTION_METHOD, &mibs_cfg.ofdm_protect_method, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, ht_method, mtlk_get_mib_value_uint8,
                                     MIB_HT_PROTECTION_METHOD, &mibs_cfg.ht_method, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, dtim_period, mtlk_get_mib_value_uint8,
                                     MIB_DTIM_PERIOD, &mibs_cfg.dtim_period, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, receive_ampdu_max_len, mtlk_get_mib_value_uint8,
                                     MIB_RECEIVE_AMPDU_MAX_LENGTH, &mibs_cfg.receive_ampdu_max_len, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, cb_databins_per_symbol, mtlk_get_mib_value_uint8,
                                     MIB_CB_DATABINS_PER_SYMBOL, &mibs_cfg.cb_databins_per_symbol, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, use_long_preamble_for_multicast, mtlk_get_mib_value_uint8,
                                     MIB_USE_LONG_PREAMBLE_FOR_MULTICAST, &mibs_cfg.use_long_preamble_for_multicast, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, use_space_time_block_code, mtlk_get_mib_value_uint8,
                                     MIB_USE_SPACE_TIME_BLOCK_CODE, &mibs_cfg.use_space_time_block_code, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, online_calibr_algo_mask, mtlk_get_mib_value_uint8,
                                     MIB_ONLINE_CALIBRATION_ALGO_MASK, &mibs_cfg.online_calibr_algo_mask, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, disconnect_on_nacks_enable, mtlk_get_mib_value_uint8,
                                     MIB_DISCONNECT_ON_NACKS_ENABLE, &mibs_cfg.disconnect_on_nacks_enable, core);

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&mibs_cfg, tx_antennas, _mtlk_core_get_antennas,
                                 (core, PARAM_DB_CORE_TX_ANTENNAS, mibs_cfg.tx_antennas));
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&mibs_cfg, rx_antennas, _mtlk_core_get_antennas,
                                 (core, PARAM_DB_CORE_RX_ANTENNAS, mibs_cfg.rx_antennas));

err_get:
  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &mibs_cfg, sizeof(mibs_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_country_cfg (mtlk_handle_t hcore, 
                            const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_country_cfg_t country_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&country_cfg, 0, sizeof(country_cfg));

  /* TODO: This check must be dropped in favor of abilities */
  if (mtlk_core_scan_is_running(core)) {
    ILOG1_D("%CID-%04x: Request eliminated due to running scan", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto err_get;
  }

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&country_cfg, country, strncpy,
                                  (country_cfg.country, country_code_to_country(mtlk_core_get_country_code(core)), MTLK_CHNLS_COUNTRY_BUFSIZE));

err_get:
  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &country_cfg, sizeof(country_cfg));
  }

  return res;
}

static int
_mtlk_core_set_bss_base_rate(mtlk_core_t *core, uint32 val)
{
  MTLK_ASSERT(mtlk_vap_is_ap(core->vap_handle));

  if (mtlk_core_get_net_state(core) != NET_STATE_READY) {
    return MTLK_ERR_NOT_READY;
  }

  if ((val != CFG_BASIC_RATE_SET_DEFAULT)
      && (val != CFG_BASIC_RATE_SET_EXTRA)
      && (val != CFG_BASIC_RATE_SET_LEGACY)) {
    return MTLK_ERR_PARAMS;
  }

  if ((val == CFG_BASIC_RATE_SET_LEGACY)
      && (MTLK_HW_BAND_2_4_GHZ != mtlk_core_get_freq_band_cfg(core))) {
    return MTLK_ERR_PARAMS;
  }

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_BASIC_RATE_SET, val);
  return MTLK_ERR_OK;
}

static int
_mtlk_core_set_antennas(mtlk_core_t *core, mtlk_pdb_id_t id_array, char *val)
{
  int err = MTLK_ERR_OK;
  uint8 val_array[MTLK_NUM_ANTENNAS_BUFSIZE];
  uint8 count = 0;

  if (0 != val[MTLK_NUM_ANTENNAS_BUFSIZE - 1])
    return MTLK_ERR_VALUE;
  
  memset (val_array, 0, sizeof (val_array));

  /* convert zero terminated character array in to the integer array */
  for (count = 0; count < (MTLK_NUM_ANTENNAS_BUFSIZE - 1); count++)
  {
    if (val[count] < '0' || val[count] > '3')
      return MTLK_ERR_VALUE;

    val_array[count] = val[count] - '0';
    if (0 == val_array[count])
      break;
  }

  err = MTLK_CORE_PDB_SET_BINARY(core, id_array, val_array, MTLK_NUM_ANTENNAS_BUFSIZE);
  if (MTLK_ERR_OK != err)
  {
    ILOG2_V("Can not save antennas configuration in to the PDB");
    return err;
  }

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_set_mibs_cfg (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_txmm_t *txmm = mtlk_vap_get_txmm(core->vap_handle);

  mtlk_mibs_cfg_t *mibs_cfg = NULL;
  uint32 mibs_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mibs_cfg = mtlk_clpb_enum_get_next(clpb, &mibs_cfg_size);

  MTLK_ASSERT(NULL != mibs_cfg);
  MTLK_ASSERT(sizeof(*mibs_cfg) == mibs_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, acl_mode, mtlk_set_mib_acl_mode,
                                  (txmm, MIB_ACL_MODE, mibs_cfg->acl_mode), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, calibr_algo_mask, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_CALIBRATION_ALGO_MASK, mibs_cfg->calibr_algo_mask));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, power_increase, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_POWER_INCREASE, mibs_cfg->power_increase));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, short_cyclic_prefix, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX, mibs_cfg->short_cyclic_prefix));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, short_preamble_option, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_SHORT_PREAMBLE, mibs_cfg->short_preamble_option));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, short_slot_time_option, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_SHORT_SLOT_TIME, mibs_cfg->short_slot_time_option));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, tx_power, MTLK_CORE_PDB_SET_INT,
                                  (core, PARAM_DB_CORE_TX_POWER, mibs_cfg->tx_power));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, current_tx_antenna, mtlk_set_mib_value_uint16,
                               (txmm, MIB_CURRENT_TX_ANTENNA, mibs_cfg->current_tx_antenna), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, beacon_period, mtlk_set_mib_value_uint16,
                               (txmm, MIB_BEACON_PERIOD, mibs_cfg->beacon_period), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, disconnect_on_nacks_weight, mtlk_set_mib_value_uint16,
                               (txmm, MIB_DISCONNECT_ON_NACKS_WEIGHT, mibs_cfg->disconnect_on_nacks_weight), res);

  if (mtlk_core_scan_is_running(core)) {
    ILOG1_D("CID-%04x: Request eliminated due to running scan", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    break;
  } else {
    MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, short_retry_limit, mtlk_set_mib_value_uint16,
                                 (txmm, MIB_SHORT_RETRY_LIMIT, mibs_cfg->short_retry_limit), res);

    MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, long_retry_limit, mtlk_set_mib_value_uint16,
                                 (txmm, MIB_LONG_RETRY_LIMIT, mibs_cfg->long_retry_limit), res);

    MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, tx_msdu_lifetime, mtlk_set_mib_value_uint16,
                                 (txmm, MIB_TX_MSDU_LIFETIME, mibs_cfg->tx_msdu_lifetime), res);

    MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, rts_threshold, mtlk_set_mib_value_uint16,
                                    (txmm, MIB_RTS_THRESHOLD, mibs_cfg->rts_threshold), res);
  }

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, sm_enable, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_SM_ENABLE, mibs_cfg->sm_enable));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, advanced_coding_supported, mtlk_set_mib_value_uint8,
                               (txmm, MIB_ADVANCED_CODING_SUPPORTED, mibs_cfg->advanced_coding_supported), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, overlapping_protect_enabled, mtlk_set_mib_value_uint8,
                               (txmm, MIB_OVERLAPPING_PROTECTION_ENABLE, mibs_cfg->overlapping_protect_enabled), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, ofdm_protect_method, mtlk_set_mib_value_uint8,
                               (txmm, MIB_OFDM_PROTECTION_METHOD, mibs_cfg->ofdm_protect_method), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, ht_method, mtlk_set_mib_value_uint8,
                               (txmm, MIB_HT_PROTECTION_METHOD, mibs_cfg->ht_method), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, dtim_period, mtlk_set_mib_value_uint8,
                               (txmm, MIB_DTIM_PERIOD, mibs_cfg->dtim_period), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, receive_ampdu_max_len, mtlk_set_mib_value_uint8,
                               (txmm, MIB_RECEIVE_AMPDU_MAX_LENGTH, mibs_cfg->receive_ampdu_max_len), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, cb_databins_per_symbol, mtlk_set_mib_value_uint8,
                               (txmm, MIB_CB_DATABINS_PER_SYMBOL, mibs_cfg->cb_databins_per_symbol), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, use_long_preamble_for_multicast, mtlk_set_mib_value_uint8,
                               (txmm, MIB_USE_LONG_PREAMBLE_FOR_MULTICAST, mibs_cfg->use_long_preamble_for_multicast), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, use_space_time_block_code, mtlk_set_mib_value_uint8,
                               (txmm, MIB_USE_SPACE_TIME_BLOCK_CODE, mibs_cfg->use_space_time_block_code), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, online_calibr_algo_mask, mtlk_set_mib_value_uint8,
                               (txmm, MIB_ONLINE_CALIBRATION_ALGO_MASK, mibs_cfg->online_calibr_algo_mask), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, disconnect_on_nacks_enable, mtlk_set_mib_value_uint8,
                               (txmm, MIB_DISCONNECT_ON_NACKS_ENABLE, mibs_cfg->disconnect_on_nacks_enable), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, tx_antennas, _mtlk_core_set_antennas,
                               (core, PARAM_DB_CORE_TX_ANTENNAS, mibs_cfg->tx_antennas), res);
  
  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, rx_antennas, _mtlk_core_set_antennas,
                               (core, PARAM_DB_CORE_RX_ANTENNAS, mibs_cfg->rx_antennas), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_set_country_cfg (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_country_cfg_t *country_cfg = NULL;
  uint32 country_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  country_cfg = mtlk_clpb_enum_get_next(clpb, &country_cfg_size);

  MTLK_ASSERT(NULL != country_cfg);
  MTLK_ASSERT(sizeof(*country_cfg) == country_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()

  if (mtlk_core_scan_is_running(core)) {
    ILOG1_D("CID-%04x: Request eliminated due to running scan", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    break;
  }

  MTLK_CFG_CHECK_ITEM_AND_CALL(country_cfg, country, _mtlk_core_set_country_from_ui,
                               (core, country_cfg->country), res);

MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_l2nat_cfg (mtlk_handle_t hcore, 
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_l2nat_cfg_t l2nat_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&l2nat_cfg, 0, sizeof(l2nat_cfg));

  MTLK_CFG_SET_ITEM(&l2nat_cfg, aging_timeout, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_L2NAT_AGING_TIMEOUT));
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&l2nat_cfg, address, mtlk_l2nat_get_def_host, (core, &l2nat_cfg.address));
                                  
  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &l2nat_cfg, sizeof(l2nat_cfg));
  }

  return res;
}

static int
_mtlk_core_set_l2nat_aging_timeout(mtlk_core_t *core, int32 timeval)
{
  if (timeval < 0) {
    ELOG_DD("CID-%04x: Wrong value for aging timeout: %d", mtlk_vap_get_oid(core->vap_handle), timeval);
    return MTLK_ERR_PARAMS;
  }
  if (timeval > 0 && timeval < 60) {
    ELOG_D("CID-%04x: The lowest timeout allowed is 60 seconds.", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_PARAMS;
  }

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_L2NAT_AGING_TIMEOUT, timeval);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_set_l2nat_cfg (mtlk_handle_t hcore, 
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_l2nat_cfg_t *l2nat_cfg = NULL;
  uint32 l2nat_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  l2nat_cfg = mtlk_clpb_enum_get_next(clpb, &l2nat_cfg_size);

  MTLK_ASSERT(NULL != l2nat_cfg);
  MTLK_ASSERT(sizeof(*l2nat_cfg) == l2nat_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
   MTLK_CFG_CHECK_ITEM_AND_CALL(l2nat_cfg, aging_timeout, _mtlk_core_set_l2nat_aging_timeout,
                               (core, l2nat_cfg->aging_timeout), res);
   MTLK_CFG_CHECK_ITEM_AND_CALL(l2nat_cfg, address, mtlk_l2nat_user_set_def_host,
                               (core, &l2nat_cfg->address), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_dot11d_cfg (mtlk_handle_t hcore, 
                           const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_dot11d_cfg_t dot11d_cfg;

  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&dot11d_cfg, 0, sizeof(dot11d_cfg));

  MTLK_CFG_SET_ITEM(&dot11d_cfg, is_dot11d, mtlk_core_get_dot11d(core));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &dot11d_cfg, sizeof(dot11d_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_dot11d_cfg (mtlk_handle_t hcore, 
                           const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_dot11d_cfg_t *dot11d_cfg = NULL;
  uint32 dot11d_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  dot11d_cfg = mtlk_clpb_enum_get_next(clpb, &dot11d_cfg_size);

  MTLK_ASSERT(NULL != dot11d_cfg);
  MTLK_ASSERT(sizeof(*dot11d_cfg) == dot11d_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
   MTLK_CFG_CHECK_ITEM_AND_CALL(dot11d_cfg, is_dot11d, _mtlk_core_set_is_dot11d,
                               (core, dot11d_cfg->is_dot11d), res);
   MTLK_CFG_CHECK_ITEM_AND_CALL(dot11d_cfg, should_reset_tx_limits, mtlk_reset_tx_limit_tables,
                               (&core->slow_ctx->tx_limits), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_mac_wdog_cfg (mtlk_handle_t hcore, 
                             const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_mac_wdog_cfg_t mac_wdog_cfg;

  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&mac_wdog_cfg, 0, sizeof(mac_wdog_cfg));

  MTLK_CFG_SET_ITEM(&mac_wdog_cfg, mac_watchdog_timeout_ms,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_TIMEOUT_MS));
  MTLK_CFG_SET_ITEM(&mac_wdog_cfg, mac_watchdog_period_ms,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_PERIOD_MS));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &mac_wdog_cfg, sizeof(mac_wdog_cfg));
  }

  return res;
}

static int
_mtlk_core_set_mac_wdog_timeout(mtlk_core_t *core, uint16 value)
{
  if (value < 1000) {
    return MTLK_ERR_PARAMS;
  }
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_TIMEOUT_MS, value);
  return MTLK_ERR_OK;
}

static int
_mtlk_core_set_mac_wdog_period(mtlk_core_t *core, uint32 value)
{
  if (0 == value) {
    return MTLK_ERR_PARAMS;
  }
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_PERIOD_MS, value);
  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_set_mac_wdog_cfg (mtlk_handle_t hcore, 
                             const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_mac_wdog_cfg_t *mac_wdog_cfg = NULL;
  uint32 mac_wdog_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mac_wdog_cfg = mtlk_clpb_enum_get_next(clpb, &mac_wdog_cfg_size);

  MTLK_ASSERT(NULL != mac_wdog_cfg);
  MTLK_ASSERT(sizeof(*mac_wdog_cfg) == mac_wdog_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
   MTLK_CFG_CHECK_ITEM_AND_CALL(mac_wdog_cfg, mac_watchdog_timeout_ms, _mtlk_core_set_mac_wdog_timeout,
                               (core, mac_wdog_cfg->mac_watchdog_timeout_ms), res);
   MTLK_CFG_CHECK_ITEM_AND_CALL(mac_wdog_cfg, mac_watchdog_period_ms, _mtlk_core_set_mac_wdog_period,
                               (core, mac_wdog_cfg->mac_watchdog_period_ms), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_stadb_cfg (mtlk_handle_t hcore, 
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_stadb_cfg_t stadb_cfg;

  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&stadb_cfg, 0, sizeof(stadb_cfg));

  MTLK_CFG_SET_ITEM(&stadb_cfg, sta_keepalive_timeout, core->slow_ctx->stadb.sta_keepalive_timeout);
  MTLK_CFG_SET_ITEM(&stadb_cfg, keepalive_interval, core->slow_ctx->stadb.keepalive_interval);
  MTLK_CFG_SET_ITEM(&stadb_cfg, aggr_open_threshold, core->slow_ctx->stadb.aggr_open_threshold);

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &stadb_cfg, sizeof(stadb_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_stadb_cfg (mtlk_handle_t hcore, 
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_stadb_cfg_t *stadb_cfg = NULL;
  uint32 stadb_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  stadb_cfg = mtlk_clpb_enum_get_next(clpb, &stadb_cfg_size);

  MTLK_ASSERT(NULL != stadb_cfg);
  MTLK_ASSERT(sizeof(*stadb_cfg) == stadb_cfg_size);

  MTLK_CFG_GET_ITEM(stadb_cfg, sta_keepalive_timeout,
                    core->slow_ctx->stadb.sta_keepalive_timeout);
  MTLK_CFG_GET_ITEM(stadb_cfg, keepalive_interval,
                    core->slow_ctx->stadb.keepalive_interval);
  MTLK_CFG_GET_ITEM(stadb_cfg, aggr_open_threshold,
                    core->slow_ctx->stadb.aggr_open_threshold);

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_sq_cfg (mtlk_handle_t hcore, 
                       const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_sq_cfg_t sq_cfg;

  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&sq_cfg, 0, sizeof(sq_cfg));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&sq_cfg, sq_limit, sq_get_limits,
                                (core, sq_cfg.sq_limit, NTS_PRIORITIES));
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&sq_cfg, peer_queue_limit, sq_get_peer_limits,
                                (core, sq_cfg.peer_queue_limit, NTS_PRIORITIES));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &sq_cfg, sizeof(sq_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_sq_cfg (mtlk_handle_t hcore, 
                       const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_sq_cfg_t *sq_cfg = NULL;
  uint32 sq_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  sq_cfg = mtlk_clpb_enum_get_next(clpb, &sq_cfg_size);

  MTLK_ASSERT(NULL != sq_cfg);
  MTLK_ASSERT(sizeof(*sq_cfg) == sq_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(sq_cfg, sq_limit, sq_set_limits,
                              (core, sq_cfg->sq_limit, NTS_PRIORITIES), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL(sq_cfg, peer_queue_limit, sq_set_peer_limits,
                              (core, sq_cfg->peer_queue_limit, NTS_PRIORITIES), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static uint8
_mtlk_core_get_spectrum_mode(mtlk_core_t *core)
{
  if (mtlk_vap_is_ap(core->vap_handle)) {
    return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE);
  }

  return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_STA_FORCE_SPECTRUM_MODE);
}

static int
_mtlk_core_get_force_rate (mtlk_core_t          *core,
                           mtlk_core_rate_cfg_t *rate_cfg,
                           uint16                object_id)
{
  uint16 forced_rate_idx = 0; /* prevent compiler from complaining */

  switch (object_id) {
  case MIB_HT_FORCE_RATE:
    forced_rate_idx = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_HT_FORCED_RATE_SET);
    break;
  case MIB_LEGACY_FORCE_RATE:
    forced_rate_idx = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_LEGACY_FORCED_RATE_SET);
    break;
  default:
    MTLK_ASSERT(0);
    break;
  }

  return mtlk_bitrate_idx_to_rates(forced_rate_idx,
                                   MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE),
                                   MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX),
                                   &rate_cfg->int_rate,
                                   &rate_cfg->frac_rate);
}

static void
_mtlk_core_get_countries_supported(mtlk_core_t *core, mtlk_gen_core_country_name_t *countries)
{
  MTLK_ASSERT(NULL != core);
  MTLK_ASSERT(NULL != countries);

  memset(countries, 0, sizeof(mtlk_gen_core_country_name_t)*MAX_COUNTRIES);

  get_all_countries_for_domain(country_code_to_domain(mtlk_core_get_country_code(core)), countries, MAX_COUNTRIES);
}

static int
_mtlk_core_get_channel (mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  /* Retrieve PARAM_DB_CORE_CHANNEL_CUR channel in case if there are active VAPs
   * Master VAP can be in NET_STATE_READY, but Slave VAP can be in NET_STATE_CONNECTED,
   * therefore PARAM_DB_CORE_CHANNEL_CUR channel, belonged to Master VAP has correct value */
  if ((NET_STATE_CONNECTED == mtlk_core_get_net_state(core)) || (0 != mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(core->vap_handle))))
    return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_CHANNEL_CUR);
  else
    return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_CHANNEL_CFG);
}

static int
_mtlk_core_set_up_rescan_exemption_time_sec (mtlk_core_t *core, uint32 value)
{
  if (value == MAX_UINT32) {
    ;
  }
  else if (value < MAX_UINT32/ MSEC_PER_SEC) {
    value *= MSEC_PER_SEC;
  }
  else {
    /* In this case, the TS (which is measured in ms) can wrap around. */
    return MTLK_ERR_PARAMS;
  }

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_UP_RESCAN_EXEMPTION_TIME, value);

  return MTLK_ERR_OK;
}

static uint32
_mtlk_core_get_up_rescan_exemption_time_sec (mtlk_core_t *core)
{
  uint32 res = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_UP_RESCAN_EXEMPTION_TIME);

  if (res != MAX_UINT32) {
    res /= MSEC_PER_SEC;
  }

  return res;
}


static void
_mtlk_master_core_get_core_cfg (mtlk_core_t *core, 
                         mtlk_gen_core_cfg_t* pCore_Cfg)
{
  MTLK_CFG_SET_ITEM(pCore_Cfg, bridge_mode,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_BRIDGE_MODE));
  MTLK_CFG_SET_ITEM(pCore_Cfg, dbg_sw_wd_enable,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_MAC_SOFT_RESET_ENABLE));
  MTLK_CFG_SET_ITEM(pCore_Cfg, reliable_multicast,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_RELIABLE_MCAST));
  MTLK_CFG_SET_ITEM(pCore_Cfg, up_rescan_exemption_time,
                    _mtlk_core_get_up_rescan_exemption_time_sec(core));
  MTLK_CFG_SET_ITEM(pCore_Cfg, spectrum_mode,
                    _mtlk_core_get_spectrum_mode(core));
  MTLK_CFG_SET_ITEM(pCore_Cfg, channel,
                    _mtlk_core_get_channel(core));
  MTLK_CFG_SET_ITEM(pCore_Cfg, bonding, mtlk_core_get_bonding(core));
  MTLK_CFG_SET_ITEM(pCore_Cfg, frequency_band_cur, mtlk_core_get_freq_band_cur(core));
}

static void
_mtlk_slave_core_get_core_cfg (mtlk_core_t *core, 
                         mtlk_gen_core_cfg_t* pCore_Cfg)
{
  _mtlk_master_core_get_core_cfg(mtlk_core_get_master(core), pCore_Cfg);
}

static int __MTLK_IFUNC
_mtlk_core_get_core_cfg (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_gen_core_cfg_t* pcore_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  uint32 num_macs = 0;
  uint32 i;
  uint32 str_size;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  pcore_cfg = mtlk_osal_mem_alloc(sizeof(mtlk_gen_core_cfg_t), MTLK_MEM_TAG_CORE_CFG);
  if(NULL == pcore_cfg) {
    ELOG_D("CID-%04x: Cannot allocate memory for core configuration data", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_MEM;
    goto err_return;
  }
  memset(pcore_cfg, 0, sizeof(*pcore_cfg));

  MTLK_CFG_SET_ITEM(pcore_cfg, ap_forwarding,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_AP_FORWARDING));

  if (NET_STATE_CONNECTED == mtlk_core_get_net_state(core)) {
    MTLK_CFG_SET_ITEM(pcore_cfg, net_mode,
                      mtlk_core_get_network_mode_cur(core));
    MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(pcore_cfg, bssid, mtlk_pdb_get_mac,
        (mtlk_vap_get_param_db(core->vap_handle), PARAM_DB_CORE_BSSID, pcore_cfg->bssid) );
  } else {
    MTLK_CFG_SET_ITEM(pcore_cfg, net_mode,
                      mtlk_core_get_network_mode_cfg(core));
    MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(pcore_cfg, bssid, memset,
                      (pcore_cfg->bssid, 0, sizeof(pcore_cfg->bssid)) );
  }

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(pcore_cfg, countries_supported, _mtlk_core_get_countries_supported,
                                (core, pcore_cfg->countries_supported));

  for (i = 0, num_macs = 0; i < MAX_ADDRESSES_IN_ACL; i++) {
    if (mtlk_osal_is_zero_address(core->slow_ctx->acl[i].au8Addr)) {
      continue;
    }
    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(pcore_cfg, macs_to_set, memcpy,
                                   (pcore_cfg->macs_to_set[num_macs].au8Addr, core->slow_ctx->acl[i].au8Addr, sizeof(IEEE_ADDR)))
    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(pcore_cfg, mac_mask, memcpy,
                                   (pcore_cfg->mac_mask[num_macs++].au8Addr, core->slow_ctx->acl_mask[i].au8Addr, sizeof(IEEE_ADDR)))
  }
  
  MTLK_CFG_SET_ITEM(pcore_cfg, num_macs_to_set, num_macs);

  str_size = sizeof(pcore_cfg->nickname);
  MTLK_CFG_SET_ITEM_BY_FUNC(pcore_cfg, nickname, mtlk_pdb_get_string,
                            (mtlk_vap_get_param_db(core->vap_handle),
                            PARAM_DB_CORE_NICK_NAME,
                            pcore_cfg->nickname, &str_size), res);

  MTLK_CFG_SET_ITEM_BY_FUNC(pcore_cfg, essid, mtlk_pdb_get_string,
                            (mtlk_vap_get_param_db(core->vap_handle),
                            PARAM_DB_CORE_ESSID,
                            pcore_cfg->essid, &str_size), res );

  MTLK_CFG_SET_ITEM(pcore_cfg, is_hidden_ssid, core->slow_ctx->cfg.is_hidden_ssid);

  if (!mtlk_vap_is_slave_ap(core->vap_handle)) {
    _mtlk_master_core_get_core_cfg(core, pcore_cfg);
  } else {
    _mtlk_slave_core_get_core_cfg(core, pcore_cfg);
  }

err_return:
  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, pcore_cfg, sizeof(*pcore_cfg));
  }
  if(NULL != pcore_cfg) {
    mtlk_osal_mem_free(pcore_cfg);
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_master_specific_cfg (mtlk_handle_t hcore, 
                                    const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_master_core_cfg_t master_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&master_cfg, 0, sizeof(master_cfg));

  MTLK_CFG_SET_ITEM_BY_FUNC(&master_cfg, legacy_force_rate, _mtlk_core_get_force_rate,
                            (core, &master_cfg.legacy_force_rate, MIB_LEGACY_FORCE_RATE), res);
  if (res != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Cannot get Legacy force rate (err=%d)", mtlk_vap_get_oid(core->vap_handle), res);
    goto err_return;
  }

  MTLK_CFG_SET_ITEM_BY_FUNC(&master_cfg, ht_force_rate, _mtlk_core_get_force_rate,
                            (core, &master_cfg.ht_force_rate, MIB_HT_FORCE_RATE), res);
  if (res != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Cannot get HT force rate (err=%d)", mtlk_vap_get_oid(core->vap_handle), res);
    goto err_return;
  }

  MTLK_CFG_SET_ITEM(&master_cfg, power_selection,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_POWER_SELECTION));

err_return:
  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &master_cfg, sizeof(master_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_master_ap_specific_cfg (mtlk_handle_t hcore, 
                                       const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_master_ap_core_cfg_t master_ap_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&master_ap_cfg, 0, sizeof(master_ap_cfg));

  MTLK_CFG_SET_ITEM(&master_ap_cfg, bss_rate, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_BASIC_RATE_SET));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &master_ap_cfg, sizeof(master_ap_cfg));
  }

  return res;
}

static int
_mtlk_core_set_bridge_mode(mtlk_core_t *core, uint8 mode)
{
  /* allow bridge mode change only if not connected */
  if (NET_STATE_CONNECTED == core->net_state) {
    ELOG_DD("CID-%04x: Cannot change bridge mode to (%d) while connected.", mtlk_vap_get_oid(core->vap_handle), mode);
    return MTLK_ERR_PARAMS;
  }
  
  /* check for only allowed values */
  if (mode >= BR_MODE_LAST) {
    ELOG_DD("CID-%04x: Unsupported bridge mode value: %d.", mtlk_vap_get_oid(core->vap_handle), mode);
    return MTLK_ERR_PARAMS;
  }
  
  /* on AP only NONE and WDS allowed */
  if (mtlk_vap_is_ap(core->vap_handle) && mode != BR_MODE_NONE && mode != BR_MODE_WDS) {
    ELOG_DD("CID-%04x: Unsupported (on AP) bridge mode value: %d.", mtlk_vap_get_oid(core->vap_handle), mode);
    return MTLK_ERR_PARAMS;
  }
  
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_BRIDGE_MODE, mode);
  ILOG1_DD("CID-%04x: bridge_mode set to %u", mtlk_vap_get_oid(core->vap_handle), mode);
  
  return MTLK_ERR_OK;
}

static int
_mtlk_core_set_spectrum_mode(mtlk_core_t *core, uint8 mode)
{
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  /* mode:
   * for AP: 20, 40, auto;
   * for STA: 20, 40,
   *          auto - use 20MHz mode for 2.4G band and
   *                 use 40MHz mode for 5G band */
  if (mode != SPECTRUM_20MHZ && mode != SPECTRUM_40MHZ && mode != SPECTRUM_AUTO) {
    ELOG_D("CID-%04x: Invalid value", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_PARAMS;
  }

  if (mtlk_vap_is_ap(core->vap_handle)) {
    BOOL is_auto_spectrum;

    mtlk_20_40_limit_to_20(mtlk_core_get_coex_sm(core), (mode == SPECTRUM_20MHZ) ? TRUE : FALSE);

    if (!is_ht_net_mode(mtlk_core_get_network_mode_cfg(core)) && (mode != SPECTRUM_20MHZ)) {
      return MTLK_ERR_PARAMS;
    }

    is_auto_spectrum = mtlk_aocs_set_auto_spectrum(core->slow_ctx->aocs, mode);
    
    if (is_auto_spectrum) {
      mode = SPECTRUM_40MHZ;
    }

    MTLK_ASSERT(mtlk_core_get_freq_band_cfg(core) != MTLK_HW_BAND_BOTH);
    if (_mtlk_core_is_20_40_active(core)) {
      MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, SPECTRUM_40MHZ);
      MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE, SPECTRUM_40MHZ);
    } 
    else {
      MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, mode);
      MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE, mode);
    }

    
  } else {
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_STA_FORCE_SPECTRUM_MODE, mode);
  }
  return MTLK_ERR_OK;
}

static int
_mtlk_core_update_network_mode(mtlk_core_t *core, uint8 mode)
{
  if(mtlk_core_scan_is_running(core)) {
    ELOG_D("CID-%04x: Cannot set network mode while scan is running", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_BUSY;
  }

  return mtlk_core_update_network_mode(core, mode);
}

static int
_mtlk_core_set_force_rate (mtlk_core_t *core, mtlk_core_rate_cfg_t *rate_cfg, uint16 u16ObjectID)
{
  uint16 forced_rate_idx;
  int res = MTLK_ERR_OK;
  int tmp_int_rate = 0;
  int tmp_frac_rate = 0;
  uint16 array_index = BITRATE_LAST+1;

  MTLK_CFG_GET_ITEM(rate_cfg, int_rate, tmp_int_rate);
  MTLK_CFG_GET_ITEM(rate_cfg, frac_rate, tmp_frac_rate);
  MTLK_CFG_GET_ITEM(rate_cfg, array_idx, array_index);

  /* check is fate requested by index in array */
  if (array_index <= BITRATE_LAST) {
    forced_rate_idx = array_index;
  }
  else {
    res = mtlk_bitrate_rates_to_idx(tmp_int_rate,
                                    tmp_frac_rate,
                                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE),
                                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX),
                                    &forced_rate_idx);
  }

  if (res != MTLK_ERR_OK) {
    return res;
  }
  if (forced_rate_idx == NO_RATE) {
    goto apply;
  }
  if (!((1 << forced_rate_idx) & mtlk_core_get_available_bitrates(core))) {
    ILOG0_D("CID-%04x: Rate doesn't fall into list of supported rates for current network mode", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_PARAMS;
  }

apply:

  switch (u16ObjectID) {
    case MIB_LEGACY_FORCE_RATE:
      MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_LEGACY_FORCED_RATE_SET, forced_rate_idx);
      break;
    case MIB_HT_FORCE_RATE:
      MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_HT_FORCED_RATE_SET, forced_rate_idx);
      break;
    default:
      break;
  }

  mtlk_mib_set_forced_rates(core);

  return MTLK_ERR_OK;
}

static void
_mtlk_core_set_power_selection (mtlk_core_t *core, int8 power_selection)
{
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_POWER_SELECTION, power_selection);

  mtlk_mib_set_power_selection(core);
}

static __INLINE int
_mtlk_core_set_nickname_by_cfg(mtlk_core_t *core, mtlk_gen_core_cfg_t *core_cfg)
{
  int res = mtlk_pdb_set_string(mtlk_vap_get_param_db(core->vap_handle),
                                PARAM_DB_CORE_NICK_NAME,
                                core_cfg->nickname);
  if (MTLK_ERR_OK == res) {
    ILOG2_DS("CID-%04x: Set NICKNAME to \"%s\"", mtlk_vap_get_oid(core->vap_handle),
        core_cfg->nickname);
  }
  return res;
}

static __INLINE int
_mtlk_core_set_essid_by_cfg(mtlk_core_t *core, mtlk_gen_core_cfg_t *core_cfg)
{
  int res = mtlk_pdb_set_string(mtlk_vap_get_param_db(core->vap_handle),
                            PARAM_DB_CORE_ESSID, core_cfg->essid);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: Can't store ESSID (err=%d)", mtlk_vap_get_oid(core->vap_handle), res);
  } else {
    ILOG2_DS("CID-%04x: Set ESSID to \"%s\"", mtlk_vap_get_oid(core->vap_handle), core_cfg->essid);
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_add_acl (mtlk_core_t *core, mtlk_gen_core_cfg_t *core_cfg)
{
  int i, res = MTLK_ERR_OK;

  for (i=0; i < core_cfg->num_macs_to_set; i++) {
    MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, macs_to_set, mtlk_core_set_acl,
                                (core, &core_cfg->macs_to_set[i], core_cfg->mac_mask_filled ? &core_cfg->mac_mask[i] : NULL), res);
  }
  if (MTLK_ERR_OK == res) {
    res = mtlk_set_mib_acl(mtlk_vap_get_txmm(core->vap_handle), core->slow_ctx->acl, core->slow_ctx->acl_mask);
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_remove_acl (mtlk_core_t *core, mtlk_gen_core_cfg_t *core_cfg)
{
  int i, res = MTLK_ERR_OK;

  for (i=0; i < core_cfg->num_macs_to_del; i++) {
    MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, macs_to_del, mtlk_core_del_acl,
                                (core, &core_cfg->macs_to_del[i]), res);
  }
  if (MTLK_ERR_OK == res) {
    res = mtlk_set_mib_acl(mtlk_vap_get_txmm(core->vap_handle), core->slow_ctx->acl, core->slow_ctx->acl_mask);
  }

  return res;
}


static int __MTLK_IFUNC
_mtlk_core_set_core_cfg (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_gen_core_cfg_t *core_cfg = NULL;
  uint32 core_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  core_cfg = mtlk_clpb_enum_get_next(clpb, &core_cfg_size);

  MTLK_ASSERT(NULL != core_cfg);
  MTLK_ASSERT(sizeof(*core_cfg) == core_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  
  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, bridge_mode, _mtlk_core_set_bridge_mode,
                              (core, core_cfg->bridge_mode), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(core_cfg, reliable_multicast, mtlk_pdb_set_int,
                                    (mtlk_vap_get_param_db(core->vap_handle),
                                    PARAM_DB_CORE_RELIABLE_MCAST,
                                    !!core_cfg->reliable_multicast));

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, up_rescan_exemption_time, _mtlk_core_set_up_rescan_exemption_time_sec,
                               (core, core_cfg->up_rescan_exemption_time), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(core_cfg, ap_forwarding, mtlk_pdb_set_int,
                                    (mtlk_vap_get_param_db(core->vap_handle),
                                    PARAM_DB_CORE_AP_FORWARDING,
                                    !!core_cfg->ap_forwarding));
  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, spectrum_mode, _mtlk_core_set_spectrum_mode,
                              (mtlk_core_get_master(core), core_cfg->spectrum_mode), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, net_mode, _mtlk_core_update_network_mode,
                              (core, core_cfg->net_mode), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, num_macs_to_set, _mtlk_core_add_acl, (core, core_cfg), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, num_macs_to_del, _mtlk_core_remove_acl, (core, core_cfg), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, nickname, _mtlk_core_set_nickname_by_cfg, (core, core_cfg), res);
  MTLK_CFG_GET_ITEM_BY_FUNC_VOID(core_cfg, essid, _mtlk_core_set_essid_by_cfg, (core, core_cfg));
  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, channel, _mtlk_core_set_channel,
                              (mtlk_core_get_master(core), core_cfg->channel), res);
  MTLK_CFG_GET_ITEM(core_cfg, is_hidden_ssid, core->slow_ctx->cfg.is_hidden_ssid);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_set_master_specific_cfg (mtlk_handle_t hcore, 
                                    const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_master_core_cfg_t *master_cfg = NULL;
  uint32 master_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  master_cfg = mtlk_clpb_enum_get_next(clpb, &master_cfg_size);

  MTLK_ASSERT(NULL != master_cfg);
  MTLK_ASSERT(sizeof(*master_cfg) == master_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(master_cfg, legacy_force_rate, _mtlk_core_set_force_rate,
                              (core, &master_cfg->legacy_force_rate, MIB_LEGACY_FORCE_RATE), res);
                              
  MTLK_CFG_CHECK_ITEM_AND_CALL(master_cfg, ht_force_rate, _mtlk_core_set_force_rate,
                              (core, &master_cfg->ht_force_rate, MIB_HT_FORCE_RATE), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(master_cfg, power_selection, _mtlk_core_set_power_selection,
                                   (core, master_cfg->power_selection));

MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_set_master_ap_specific_cfg (mtlk_handle_t hcore, 
                                       const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_master_ap_core_cfg_t *master_ap_cfg = NULL;
  uint32 master_ap_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  master_ap_cfg = mtlk_clpb_enum_get_next(clpb, &master_ap_cfg_size);

  MTLK_ASSERT(NULL != master_ap_cfg);
  MTLK_ASSERT(sizeof(*master_ap_cfg) == master_ap_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(master_ap_cfg, bss_rate, _mtlk_core_set_bss_base_rate,
                               (core, master_ap_cfg->bss_rate), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_eeprom_cfg (mtlk_handle_t hcore, 
                           const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_eeprom_cfg_t* eeprom_cfg = mtlk_osal_mem_alloc(sizeof(mtlk_eeprom_cfg_t), MTLK_MEM_TAG_EEPROM);
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if(NULL == eeprom_cfg) {
    return MTLK_ERR_NO_MEM;
  }

  memset(eeprom_cfg, 0, sizeof(mtlk_eeprom_cfg_t));

  if (core->is_stopped) {
    MTLK_CFG_SET_ITEM(eeprom_cfg, is_if_stopped, TRUE);

    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(eeprom_cfg, eeprom_data, mtlk_eeprom_get_cfg ,
                                   (mtlk_core_get_eeprom(core), &eeprom_cfg->eeprom_data));

    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(eeprom_cfg, eeprom_raw_data, mtlk_eeprom_get_raw_cfg ,
                                   (mtlk_vap_get_txmm(core->vap_handle),
                                    eeprom_cfg->eeprom_raw_data,
                                    sizeof(eeprom_cfg->eeprom_raw_data)));
  }
  else {
    MTLK_CFG_SET_ITEM(eeprom_cfg, is_if_stopped, FALSE)
  }
  
  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, eeprom_cfg, sizeof(mtlk_eeprom_cfg_t));
  }

  mtlk_osal_mem_free(eeprom_cfg);

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_hstdb_cfg (mtlk_handle_t hcore, 
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_hstdb_cfg_t hstdb_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&hstdb_cfg, 0, sizeof(hstdb_cfg));

  MTLK_CFG_SET_ITEM(&hstdb_cfg, wds_host_timeout, core->slow_ctx->hstdb.wds_host_timeout);
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&hstdb_cfg, address, mtlk_hstdb_get_local_mac, 
                                 (&core->slow_ctx->hstdb, hstdb_cfg.address.au8Addr));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &hstdb_cfg, sizeof(hstdb_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_hstdb_cfg (mtlk_handle_t hcore, 
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_hstdb_cfg_t *hstdb_cfg = NULL;
  uint32 hstdb_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  hstdb_cfg = mtlk_clpb_enum_get_next(clpb, &hstdb_cfg_size);

  MTLK_ASSERT(NULL != hstdb_cfg);
  MTLK_ASSERT(sizeof(*hstdb_cfg) == hstdb_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_GET_ITEM(hstdb_cfg, wds_host_timeout, core->slow_ctx->hstdb.wds_host_timeout);
  MTLK_CFG_CHECK_ITEM_AND_CALL(hstdb_cfg, address, mtlk_hstdb_set_local_mac, 
                               (&core->slow_ctx->hstdb, hstdb_cfg->address.au8Addr), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_scan_cfg (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_scan_cfg_t scan_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&scan_cfg, 0, sizeof(scan_cfg));

  MTLK_CFG_SET_ITEM(&scan_cfg, cache_expire, core->slow_ctx->cache.cache_expire);
  MTLK_CFG_SET_ITEM(&scan_cfg, channels_per_chunk_limit,
                    mtlk_scan_get_per_chunk_limit(&core->slow_ctx->scan));
  MTLK_CFG_SET_ITEM(&scan_cfg, pause_between_chunks,
                    mtlk_scan_get_pause_between_chunks(&core->slow_ctx->scan));
  MTLK_CFG_SET_ITEM(&scan_cfg, is_background_scan,
                    mtlk_scan_is_background_scan_enabled(&core->slow_ctx->scan));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&scan_cfg, essid, mtlk_scan_get_essid,
                                (&core->slow_ctx->scan, scan_cfg.essid));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &scan_cfg, sizeof(scan_cfg));
  }

  return res;
}

static int 
_mtlk_core_set_scan_cfg (mtlk_handle_t hcore, 
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_scan_cfg_t *scan_cfg = NULL;
  uint32 scan_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  scan_cfg = mtlk_clpb_enum_get_next(clpb, &scan_cfg_size);

  MTLK_ASSERT(NULL != scan_cfg);
  MTLK_ASSERT(sizeof(*scan_cfg) == scan_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_GET_ITEM(scan_cfg, cache_expire, core->slow_ctx->cache.cache_expire);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(scan_cfg, channels_per_chunk_limit, mtlk_scan_set_per_chunk_limit,
                              (&core->slow_ctx->scan, scan_cfg->channels_per_chunk_limit));
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(scan_cfg, pause_between_chunks, mtlk_scan_set_pause_between_chunks,
                              (&core->slow_ctx->scan, scan_cfg->pause_between_chunks));
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(scan_cfg, is_background_scan, mtlk_scan_set_is_background_scan_enabled,
                              (&core->slow_ctx->scan, scan_cfg->is_background_scan));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(scan_cfg, essid, mtlk_scan_set_essid,
                                   (&core->slow_ctx->scan, scan_cfg->essid));
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int 
_mtlk_core_set_hw_data_cfg (mtlk_handle_t hcore, 
                            const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_hw_data_cfg_t *hw_data_cfg = NULL;
  uint32 hw_data_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  hw_data_cfg = mtlk_clpb_enum_get_next(clpb, &hw_data_cfg_size);

  MTLK_ASSERT(NULL != hw_data_cfg);
  MTLK_ASSERT(sizeof(*hw_data_cfg) == hw_data_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(hw_data_cfg, hw_cfg, mtlk_set_hw_limit,
                              (&core->slow_ctx->tx_limits, &hw_data_cfg->hw_cfg), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL(hw_data_cfg, ant, mtlk_set_ant_gain,
                              (&core->slow_ctx->tx_limits, &hw_data_cfg->ant), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL(hw_data_cfg, power_limit, mtlk_set_power_limit,
                              (core, &hw_data_cfg->power_limit), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_qos_cfg (mtlk_handle_t hcore, 
                        const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_qos_cfg_t qos_cfg;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&qos_cfg, 0, sizeof(qos_cfg));

  MTLK_CFG_SET_ITEM(&qos_cfg, map, mtlk_qos_get_map());

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &qos_cfg, sizeof(qos_cfg));
  }

  return res;
}

static int 
_mtlk_core_set_qos_cfg (mtlk_handle_t hcore, 
                        const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_qos_cfg_t *qos_cfg = NULL;
  uint32 qos_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  qos_cfg = mtlk_clpb_enum_get_next(clpb, &qos_cfg_size);

  MTLK_ASSERT(NULL != qos_cfg);
  MTLK_ASSERT(sizeof(*qos_cfg) == qos_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(qos_cfg, map, mtlk_qos_set_map,
                              (qos_cfg->map), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_coc_cfg (mtlk_handle_t hcore, 
                        const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_coc_mode_cfg_t coc_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&coc_cfg, 0, sizeof(coc_cfg));

  MTLK_CFG_SET_ITEM(&coc_cfg, is_enabled, mtlk_coc_low_power_mode_get(core->slow_ctx->coc_mngmt));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &coc_cfg, sizeof(coc_cfg));
  }

  return res;
}

static int
_mtlk_core_set_coc_power_mode(mtlk_core_t *core, BOOL enable)
{
  if (NET_STATE_CONNECTED != mtlk_core_get_net_state(core)) {
    ELOG_D("CID-%04x: Cannot set CoC power mode while not connected", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_NOT_READY;
  }
  else if (mtlk_core_scan_is_running(core)) {
    ELOG_D("CID-%04x: Cannot set CoC power mode while scan is running", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_BUSY;
  }
  else if ( MTLK_ERR_OK !=
        (FALSE != enable
        ? mtlk_coc_low_power_mode_enable(core->slow_ctx->coc_mngmt)
        : mtlk_coc_high_power_mode_enable(core->slow_ctx->coc_mngmt)) )
  {
    return MTLK_ERR_UNKNOWN;
  }
  return MTLK_ERR_OK;
}

static int 
_mtlk_core_set_coc_cfg (mtlk_handle_t hcore, 
                        const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_coc_mode_cfg_t *coc_cfg = NULL;
  uint32 coc_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  coc_cfg = mtlk_clpb_enum_get_next(clpb, &coc_cfg_size);

  MTLK_ASSERT(NULL != coc_cfg);
  MTLK_ASSERT(sizeof(*coc_cfg) == coc_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(coc_cfg, is_enabled, _mtlk_core_set_coc_power_mode,
                              (core, coc_cfg->is_enabled), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}


static int __MTLK_IFUNC
_mtlk_core_mbss_add_vap (mtlk_handle_t hcore, uint32 vap_index)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_vap_handle_t slave_vap_handle;

  ILOG0_D("CID-%04x: Got PRM_ID_VAP_ADD", mtlk_vap_get_oid(core->vap_handle));
  res = mtlk_vap_manager_create_vap(mtlk_vap_get_manager(core->vap_handle), &slave_vap_handle, vap_index);
  if (res != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: Can't add VAP", mtlk_vap_get_oid(core->vap_handle));
  }
  else if ((res = mtlk_vap_start(slave_vap_handle, TRUE)) != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't start VAP#%d", mtlk_vap_get_oid(core->vap_handle), vap_index);
    mtlk_vap_delete(slave_vap_handle);
  }
  else {
    ILOG0_DD("CID-%04x: VAP#%d added", mtlk_vap_get_oid(core->vap_handle), vap_index);
  }

  return res;
}

static int
_mtlk_core_mbss_del_vap (mtlk_handle_t hcore, uint32 vap_index)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_vap_handle_t vap_handle;
  int target_core_state;

  res = mtlk_vap_manager_get_vap_handle(
      mtlk_vap_get_manager(core->vap_handle), vap_index, &vap_handle);
  if (MTLK_ERR_OK != res ) {
    ELOG_DD("CID-%04x: VAP#%d doesnt exist", mtlk_vap_get_oid(core->vap_handle), vap_index);
    res = MTLK_ERR_PARAMS;
    goto func_ret;
  }

  if (mtlk_vap_is_master(vap_handle)) {
    ELOG_D("CID-%04x: Can't remove Master VAP", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_PARAMS;
    goto func_ret;
  }

  target_core_state = mtlk_core_get_net_state(mtlk_vap_get_core(vap_handle));
  if ( 0 == ((NET_STATE_READY|NET_STATE_IDLE|NET_STATE_HALTED) & target_core_state) ) {
    ILOG1_D("CID-%04x:: Invalid card state - request rejected", mtlk_vap_get_oid(vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto func_ret;
  }

  ILOG0_DD("CID-%04x: Deleting VAP#%d", mtlk_vap_get_oid(core->vap_handle), vap_index);
  mtlk_vap_stop(vap_handle, MTLK_VAP_SLAVE_INTERFACE);
  mtlk_vap_delete(vap_handle);
  res = MTLK_ERR_OK;

func_ret:
  return res;
}

static int __MTLK_IFUNC
_mtlk_core_add_vap (mtlk_handle_t hcore, 
                    const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_mbss_cfg_t *mbss_cfg;
  uint32 mbss_cfg_size = sizeof(mtlk_mbss_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mbss_cfg = mtlk_clpb_enum_get_next(clpb, &mbss_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_CHECK_ITEM_AND_CALL(mbss_cfg, added_vap_index, _mtlk_core_mbss_add_vap,
                                 (hcore, mbss_cfg->added_vap_index), res);
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_del_vap (mtlk_handle_t hcore, 
                    const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_mbss_cfg_t *mbss_cfg;
  uint32 mbss_cfg_size = sizeof(mtlk_mbss_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mbss_cfg = mtlk_clpb_enum_get_next(clpb, &mbss_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_CHECK_ITEM_AND_CALL(mbss_cfg, deleted_vap_index, _mtlk_core_mbss_del_vap,
                                 (hcore, mbss_cfg->deleted_vap_index), res);
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));

  return res;
}

static int
_mtlk_core_set_mbss_vap_limits (mtlk_handle_t hcore, uint32 min_limit, uint32 max_limit)
{
  uint32 res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  UMI_LIMITS_VAP_OPERATE *vap_operate;

  if (min_limit != MTLK_MBSS_VAP_LIMIT_DEFAULT && 
    max_limit != MTLK_MBSS_VAP_LIMIT_DEFAULT &&
    min_limit > max_limit) {
      ELOG_V("maximum limit is lower than minimum limit value");
      res = MTLK_ERR_PARAMS;
      goto FINISH;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto FINISH;
  }

  MTLK_ASSERT(man_entry->payload != NULL);

  man_entry->id = UM_MAN_VAP_LIMITS_REQ;
  man_entry->payload_size = sizeof(UMI_LIMITS_VAP_OPERATE);
  vap_operate = (UMI_LIMITS_VAP_OPERATE *)(man_entry->payload);
  vap_operate->u8MaxLimit = max_limit;
  vap_operate->u8MinLimit = min_limit;
  vap_operate->u8OperationCode = VAP_OPERATIONS_LIMITS_SET;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Can't set limits, timed-out", mtlk_vap_get_oid(core->vap_handle));
    goto RELEASE_MAN_ENTRY;
  }

  switch (vap_operate->u8Status) {
  case UMI_OK:
    res = MTLK_ERR_OK;
    break;
  case UMI_NOT_SUPPORTED:
    res = MTLK_ERR_NOT_SUPPORTED;
    break;
  case UMI_BAD_PARAMETER:
    res = MTLK_ERR_PARAMS;
    break;
  case UMI_BSS_ALREADY_ACTIVE:
    res = MTLK_ERR_NOT_READY;
    break;
  default:
    MTLK_ASSERT(FALSE);
  }

RELEASE_MAN_ENTRY:
  mtlk_txmm_msg_cleanup(&man_msg);

FINISH:
  return res;
}


static int
mtlk_core_get_mbss_vap_limits (mtlk_handle_t hcore, const void *data)
{
  int res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  UMI_LIMITS_VAP_OPERATE *vap_operate;
  mtlk_mbss_vap_limit_data_cfg_t *vap_limits = (mtlk_mbss_vap_limit_data_cfg_t *)data;

  MTLK_ASSERT(vap_limits != NULL);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto FINISH;
  }

  MTLK_ASSERT(man_entry->payload != NULL);

  man_entry->id = UM_MAN_VAP_LIMITS_REQ;
  man_entry->payload_size = sizeof(UMI_LIMITS_VAP_OPERATE);
  vap_operate = (UMI_LIMITS_VAP_OPERATE *)man_entry->payload;
  vap_operate->u8OperationCode = VAP_OPERATION_LIMITS_QUERY;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Can't set limits, timed-out", mtlk_vap_get_oid(core->vap_handle));
    goto SEND_COMPLETED;
  }

  switch (vap_operate->u8Status) {
  case UMI_OK:
    res = MTLK_ERR_OK;
    break;
  case UMI_NOT_SUPPORTED:
    res = MTLK_ERR_NOT_SUPPORTED;
    goto SEND_COMPLETED;
    break;
  case UMI_BAD_PARAMETER:
    res = MTLK_ERR_PARAMS;
    goto SEND_COMPLETED;
    break;
  default:
    MTLK_ASSERT(FALSE);
  }
  vap_limits->lower_limit = vap_operate->u8MinLimit;
  vap_limits->upper_limit = vap_operate->u8MaxLimit;


SEND_COMPLETED:
  mtlk_txmm_msg_cleanup(&man_msg);
FINISH:
  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_mbss_vars (mtlk_handle_t hcore, 
                          const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_mbss_cfg_t mbss_cfg;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  memset(&mbss_cfg, 0, sizeof(mbss_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_SET_ITEM_BY_FUNC(&mbss_cfg, vap_limits, mtlk_core_get_mbss_vap_limits,
                             (hcore, &mbss_cfg.vap_limits), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &mbss_cfg, sizeof(mbss_cfg));
  }

  return res;
}

typedef struct  
{
  int          res;
  mtlk_clpb_t *clpb;
} mtlk_core_get_serializer_info_enum_ctx_t;

static BOOL __MTLK_IFUNC
__mtlk_core_get_serializer_info_enum_clb (mtlk_serializer_t    *szr,
                                          const mtlk_command_t *command,
                                          BOOL                  is_current,
                                          mtlk_handle_t         enum_ctx)
{
  mtlk_core_get_serializer_info_enum_ctx_t *ctx = 
    HANDLE_T_PTR(mtlk_core_get_serializer_info_enum_ctx_t, enum_ctx);
  mtlk_serializer_command_info_t cmd_info;

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_SET_ITEM(&cmd_info, is_current, is_current);
    MTLK_CFG_SET_ITEM(&cmd_info, priority, mtlk_command_get_priority(command));
    MTLK_CFG_SET_ITEM(&cmd_info, issuer_slid, mtlk_command_get_issuer_slid(command));
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  ctx->res = mtlk_clpb_push(ctx->clpb, &cmd_info, sizeof(cmd_info));

  return (ctx->res == MTLK_ERR_OK)?TRUE:FALSE;
}

static int __MTLK_IFUNC
_mtlk_core_get_serializer_info (mtlk_handle_t hcore, 
                                const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_core_get_serializer_info_enum_ctx_t ctx;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  ctx.clpb = *(mtlk_clpb_t **) data;
  ctx.res  = MTLK_ERR_OK;

  mtlk_serializer_enum_commands(&nic->slow_ctx->serializer, __mtlk_core_get_serializer_info_enum_clb, HANDLE_T(&ctx));

  return ctx.res;
}

static int __MTLK_IFUNC
_mtlk_core_set_mbss_vars (mtlk_handle_t hcore, 
                          const void* data, uint32 data_size)
{

  uint32 res = MTLK_ERR_OK;
  mtlk_mbss_cfg_t *mbss_cfg;
  uint32 mbss_cfg_size = sizeof(mtlk_mbss_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mbss_cfg = mtlk_clpb_enum_get_next(clpb, &mbss_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_CHECK_ITEM_AND_CALL(mbss_cfg, vap_limits, _mtlk_core_set_mbss_vap_limits,
                                 (hcore, mbss_cfg->vap_limits.lower_limit, mbss_cfg->vap_limits.upper_limit), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()


  res = mtlk_clpb_push(clpb, &res, sizeof(res));

  return res;
}
/* 20/40 coexistence */
static int __MTLK_IFUNC
_mtlk_core_get_coex_20_40_mode_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_coex_20_40_mode_cfg_t coex_cfg;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&coex_cfg, 0, sizeof(coex_cfg));


  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_SET_ITEM(&coex_cfg, coexistence_mode, 
      mtlk_20_40_is_feature_enabled(mtlk_core_get_coex_sm(core)));

    MTLK_CFG_SET_ITEM(&coex_cfg, intolerance_mode, 
      mtlk_20_40_is_intolerance_declared(mtlk_core_get_coex_sm(core)));

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &coex_cfg, sizeof(coex_cfg));
  }

  return res;
}


//*******1
static int __MTLK_IFUNC
  _mtlk_core_get_coex_20_40_exm_req_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_coex_20_40_exm_req_cfg_t coex_cfg;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&coex_cfg, 0, sizeof(coex_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_SET_ITEM(&coex_cfg, exemption_req, 
      mtlk_20_40_sta_is_scan_exemption_request_forced(mtlk_core_get_coex_sm(core)));

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &coex_cfg, sizeof(coex_cfg));
  }

  return res;
}

//*****2
static int __MTLK_IFUNC
  _mtlk_core_get_coex_20_40_times_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_coex_20_40_times_cfg_t coex_cfg;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&coex_cfg, 0, sizeof(coex_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_SET_ITEM(&coex_cfg, delay_factor, 
      mtlk_20_40_get_transition_delay_factor(mtlk_core_get_coex_sm(core)));

    MTLK_CFG_SET_ITEM(&coex_cfg, obss_scan_interval, 
      mtlk_20_40_get_scan_interval(mtlk_core_get_coex_sm(core)));

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &coex_cfg, sizeof(coex_cfg));
  }

  return res;
}


static int __MTLK_IFUNC
  _mtlk_core_set_coex_20_40_mode_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_coex_20_40_mode_cfg_t *coex_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  uint32 coex_cfg_size = sizeof(mtlk_coex_20_40_mode_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  coex_cfg = mtlk_clpb_enum_get_next(clpb, &coex_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_GET_ITEM_BY_FUNC_VOID(coex_cfg, coexistence_mode, 
      mtlk_20_40_enable_feature, (mtlk_core_get_coex_sm(core), coex_cfg->coexistence_mode));
    MTLK_CFG_GET_ITEM_BY_FUNC_VOID(coex_cfg, intolerance_mode, 
      mtlk_20_40_declare_intolerance, (mtlk_core_get_coex_sm(core), coex_cfg->intolerance_mode));

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}


//*****1s
static int __MTLK_IFUNC
  _mtlk_core_set_coex_20_40_exm_req_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_coex_20_40_exm_req_cfg_t *coex_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  uint32 coex_cfg_size = sizeof(mtlk_coex_20_40_exm_req_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  coex_cfg = mtlk_clpb_enum_get_next(clpb, &coex_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_GET_ITEM_BY_FUNC_VOID(coex_cfg, exemption_req, 
        mtlk_20_40_sta_force_scan_exemption_request, (mtlk_core_get_coex_sm(core), coex_cfg->exemption_req));

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}


//*****2s
static int __MTLK_IFUNC
  _mtlk_core_set_coex_20_40_times_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_coex_20_40_times_cfg_t *coex_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  uint32 coex_cfg_size = sizeof(mtlk_coex_20_40_times_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  coex_cfg = mtlk_clpb_enum_get_next(clpb, &coex_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, delay_factor, 
      mtlk_20_40_set_transition_delay_factor, (mtlk_core_get_coex_sm(core), coex_cfg->delay_factor), res);
    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, obss_scan_interval, 
      mtlk_20_40_set_scan_interval, (mtlk_core_get_coex_sm(core), coex_cfg->obss_scan_interval), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

/* End of: 20/40 coexistence */

static int
_mtlk_core_set_fw_led_gpio_cfg (mtlk_core_t *core, const mtlk_fw_led_gpio_cfg_t *fw_led_gpio_cfg)
{
  int res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_CONFIG_GPIO  *umi_gpio_cfg = NULL;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(fw_led_gpio_cfg != NULL);
  MTLK_ASSERT(mtlk_vap_is_slave_ap(core->vap_handle) == FALSE);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_MAN_CONFIG_GPIO_REQ;
  man_entry->payload_size = sizeof(*umi_gpio_cfg);

  umi_gpio_cfg = (UMI_CONFIG_GPIO *)man_entry->payload;

  umi_gpio_cfg->uDisableTestbus = fw_led_gpio_cfg->disable_testbus;
  umi_gpio_cfg->uActiveGpios    = fw_led_gpio_cfg->active_gpios;
  umi_gpio_cfg->bLedPolarity    = fw_led_gpio_cfg->led_polatity;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Cannot send UM_MAN_CONFIG_GPIO_REQ to the FW (err=%d)", res);
    goto END;
  }

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_FW_LED_GPIO_DISABLE_TESTBUS, fw_led_gpio_cfg->disable_testbus);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_FW_LED_GPIO_ACTIVE_GPIOs, fw_led_gpio_cfg->active_gpios);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_FW_LED_GPIO_LED_POLARITY, fw_led_gpio_cfg->led_polatity);

END:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static int
_mtlk_core_get_fw_led_gpio_cfg (mtlk_core_t *core, mtlk_fw_led_gpio_cfg_t *fw_led_gpio_cfg)
{
  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(fw_led_gpio_cfg != NULL);

  fw_led_gpio_cfg->disable_testbus = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_FW_LED_GPIO_DISABLE_TESTBUS);
  fw_led_gpio_cfg->active_gpios    = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_FW_LED_GPIO_ACTIVE_GPIOs);
  fw_led_gpio_cfg->led_polatity    = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_FW_LED_GPIO_LED_POLARITY);

  return MTLK_ERR_OK;
}

static int
_mtlk_core_set_fw_led_state (mtlk_core_t *core, const mtlk_fw_led_state_t *fw_led_state)
{
  int res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_SET_LED      *umi_led_state = NULL;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(fw_led_state != NULL);
  MTLK_ASSERT(mtlk_vap_is_slave_ap(core->vap_handle) == FALSE);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_MAN_SET_LED_REQ;
  man_entry->payload_size = sizeof(*umi_led_state);

  umi_led_state = (UMI_SET_LED *)man_entry->payload;

  umi_led_state->u8BasebLed  = fw_led_state->baseb_led;
  umi_led_state->u8LedStatus = fw_led_state->led_state;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Cannot send UM_MAN_SET_LED_REQ to the FW (err=%d)", res);
    goto END;
  }

END:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

static int
_mtlk_core_set_fw_led_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_UNKNOWN;
  mtlk_fw_led_cfg_t *fw_led_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  uint32 fw_led_cfg_size = sizeof(*fw_led_cfg);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  fw_led_cfg = mtlk_clpb_enum_get_next(clpb, &fw_led_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_CHECK_ITEM_AND_CALL(fw_led_cfg, gpio_cfg, 
                                _mtlk_core_set_fw_led_gpio_cfg,
                                (core, &fw_led_cfg->gpio_cfg), res);

    MTLK_CFG_CHECK_ITEM_AND_CALL(fw_led_cfg, led_state, 
                                _mtlk_core_set_fw_led_state,
                                (core, &fw_led_cfg->led_state), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int
_mtlk_core_get_fw_led_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_UNKNOWN;
  mtlk_fw_led_cfg_t fw_led_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  memset(&fw_led_cfg, 0, sizeof(fw_led_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_SET_ITEM_BY_FUNC(&fw_led_cfg, gpio_cfg,
                              _mtlk_core_get_fw_led_gpio_cfg, (core, &fw_led_cfg.gpio_cfg), res);
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &fw_led_cfg, sizeof(fw_led_cfg));
  }

  return res;
}


static int
_mtlk_core_stop_lm(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);

  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), &res);
  if (!man_entry) {
    ELOG_D("CID-%04x: Can't stop lower MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  man_entry->id           = UM_LM_STOP_REQ;
  man_entry->payload_size = 0;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Can't stop lower MAC, timed-out", mtlk_vap_get_oid(core->vap_handle));
  }

  mtlk_txmm_msg_cleanup(&man_msg);
end:
  return res;
}

static int
_mtlk_core_mac_calibrate(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);

  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), &res);
  if (!man_entry) {
    ELOG_D("CID-%04x: Can't calibrate due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  man_entry->id           = UM_PER_CHANNEL_CALIBR_REQ;
  man_entry->payload_size = 0;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Can't calibrate, timed-out", mtlk_vap_get_oid(core->vap_handle));
  }

  mtlk_txmm_msg_cleanup(&man_msg);
end:
  return res;
}

static int
_mtlk_core_get_iw_generic(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  UMI_GENERIC_MAC_REQUEST *req_df_cfg = NULL;
  UMI_GENERIC_MAC_REQUEST *pdata = NULL;
  uint32 req_df_cfg_size;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), &res);
  if (!man_entry) {
    ELOG_D("CID-%04x: Can't send request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  req_df_cfg = mtlk_clpb_enum_get_next(clpb, &req_df_cfg_size);

  MTLK_ASSERT(NULL != req_df_cfg);
  MTLK_ASSERT(sizeof(*req_df_cfg) == req_df_cfg_size);

  pdata = (UMI_GENERIC_MAC_REQUEST*)man_entry->payload;
  man_entry->id           = UM_MAN_GENERIC_MAC_REQ;
  man_entry->payload_size = sizeof(*pdata);

  pdata->opcode=  cpu_to_le32(req_df_cfg->opcode);
  pdata->size=  cpu_to_le32(req_df_cfg->size);
  pdata->action=  cpu_to_le32(req_df_cfg->action);
  pdata->res0=  cpu_to_le32(req_df_cfg->res0);
  pdata->res1=  cpu_to_le32(req_df_cfg->res1);
  pdata->res2=  cpu_to_le32(req_df_cfg->res2);
  pdata->retStatus=  cpu_to_le32(req_df_cfg->retStatus);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Can't send generic request to MAC, timed-out", mtlk_vap_get_oid(core->vap_handle));
    goto err_send;
  }

  /* Send response to DF user */
  pdata->opcode=  cpu_to_le32(pdata->opcode);
  pdata->size=  cpu_to_le32(pdata->size);
  pdata->action=  cpu_to_le32(pdata->action);
  pdata->res0=  cpu_to_le32(pdata->res0);
  pdata->res1=  cpu_to_le32(pdata->res1);
  pdata->res2=  cpu_to_le32(pdata->res2);
  pdata->retStatus=  cpu_to_le32(pdata->retStatus);

  res = mtlk_clpb_push(clpb, pdata, sizeof(*pdata));

err_send:
  mtlk_txmm_msg_cleanup(&man_msg);
end:
  return res;
}

static int
_mtlk_core_set_leds_cfg(mtlk_core_t *core, UMI_SET_LED *leds_cfg)
{
  int res = MTLK_ERR_OK;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_SET_LED* pdata = NULL;

  MTLK_ASSERT(NULL != leds_cfg);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), &res);
  if (!man_entry) {
    ELOG_D("CID-%04x: No man entry to set MAC leds cfg (GPIO)", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  pdata = (UMI_SET_LED*)man_entry->payload;
  man_entry->id           = UM_MAN_SET_LED_REQ;
  man_entry->payload_size = sizeof(*leds_cfg);

  pdata->u8BasebLed = leds_cfg->u8BasebLed;
  pdata->u8LedStatus = leds_cfg->u8LedStatus;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: MAC Control GPIO set timeout", mtlk_vap_get_oid(core->vap_handle));
  }

  mtlk_txmm_msg_cleanup(&man_msg);

end:
  return res;
}

static int
_mtlk_core_ctrl_mac_gpio(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  UMI_SET_LED *leds_cfg = NULL;
  uint32 leds_cfg_size;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  leds_cfg = mtlk_clpb_enum_get_next(clpb, &leds_cfg_size);

  MTLK_ASSERT(NULL != leds_cfg);
  MTLK_ASSERT(sizeof(*leds_cfg) == leds_cfg_size);

  return _mtlk_core_set_leds_cfg(core, leds_cfg);
}

static int
_mtlk_core_gen_dataex_get_connection_stats (mtlk_core_t *core,
                                            WE_GEN_DATAEX_REQUEST *preq,
                                            mtlk_clpb_t *clpb)
{
  int res = MTLK_ERR_OK;
  WE_GEN_DATAEX_CONNECTION_STATUS dataex_conn_status;
  WE_GEN_DATAEX_RESPONSE resp;
  int nof_connected;
  const sta_entry *sta = NULL;
  mtlk_stadb_iterator_t iter;

  memset(&resp, 0, sizeof(resp));
  memset(&dataex_conn_status, 0, sizeof(dataex_conn_status));

  resp.ver = WE_GEN_DATAEX_PROTO_VER;
  resp.status = WE_GEN_DATAEX_SUCCESS;
  resp.datalen = sizeof(WE_GEN_DATAEX_CONNECTION_STATUS);

  if (preq->datalen < resp.datalen) {
    return MTLK_ERR_NO_MEM;
  }

  memset(&dataex_conn_status, 0, sizeof(WE_GEN_DATAEX_CONNECTION_STATUS));

  nof_connected = 0;

  sta = mtlk_stadb_iterate_first(&core->slow_ctx->stadb, &iter);
  if (sta) {
    do {
      WE_GEN_DATAEX_DEVICE_STATUS dataex_dev_status;
      dataex_dev_status.u32RxCount    = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_PACKETS_RECEIVED);
      dataex_dev_status.u32TxCount    = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_PACKETS_SENT);

      resp.datalen += sizeof(WE_GEN_DATAEX_DEVICE_STATUS);
      if (preq->datalen < resp.datalen) {
        res = MTLK_ERR_NO_MEM;
        break;
      }

      res = mtlk_clpb_push(clpb, &dataex_dev_status, sizeof(WE_GEN_DATAEX_DEVICE_STATUS));
      if (MTLK_ERR_OK != res) { 
        break;
      }

      nof_connected++;

      sta = mtlk_stadb_iterate_next(&iter);
    } while (sta);
    mtlk_stadb_iterate_done(&iter);
  }

  if (MTLK_ERR_OK == res) {
    dataex_conn_status.u32NumOfConnections = nof_connected;
    res = mtlk_clpb_push(clpb, &dataex_conn_status, sizeof(WE_GEN_DATAEX_CONNECTION_STATUS));
  }

  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &resp, sizeof(resp));
  }
  return res;
}

static int
_mtlk_core_gen_dataex_get_status (mtlk_core_t *core,
                                  WE_GEN_DATAEX_REQUEST *preq,
                                  mtlk_clpb_t *clpb)
{
  int res = MTLK_ERR_OK;
  const sta_entry *sta;
  WE_GEN_DATAEX_RESPONSE resp;
  WE_GEN_DATAEX_STATUS status;
  mtlk_stadb_iterator_t iter;

  memset(&resp, 0, sizeof(resp));
  memset(&status, 0, sizeof(status));

  if (preq->datalen < sizeof(status)) {
    resp.status = WE_GEN_DATAEX_DATABUF_TOO_SMALL;
    resp.datalen = sizeof(status);
    goto end;
  }

  resp.ver = WE_GEN_DATAEX_PROTO_VER;
  resp.status = WE_GEN_DATAEX_SUCCESS;
  resp.datalen = sizeof(status);

  memset(&status, 0, sizeof(status));

  status.security_on = 0;
  status.wep_enabled = 0;

  sta = mtlk_stadb_iterate_first(&core->slow_ctx->stadb, &iter);
  if (sta) {
    do {
      /* Check global WEP enabled flag only if some STA connected */
      if ((mtlk_sta_get_cipher(sta) != IW_ENCODE_ALG_NONE) || core->slow_ctx->wep_enabled) {
        status.security_on = 1;
        if (core->slow_ctx->wep_enabled) {
          status.wep_enabled = 1;
        }
        break;
      }
      sta = mtlk_stadb_iterate_next(&iter);
    } while (sta);
    mtlk_stadb_iterate_done(&iter);
  }

  status.scan_started = mtlk_core_scan_is_running(core);
  if (!mtlk_vap_is_slave_ap(core->vap_handle) && (mtlk_scan_is_initialized(&core->slow_ctx->scan))) {
    status.frequency_band = mtlk_core_get_freq_band_cur(core);
  }
  else {
    status.frequency_band = MTLK_HW_BAND_NONE;
  }
  status.link_up = (mtlk_core_get_net_state(core) == NET_STATE_CONNECTED) ? 1 : 0;

  res = mtlk_clpb_push(clpb, &status, sizeof(status));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &resp, sizeof(resp));
  }

end:
  return res;
}

static int
_mtlk_core_gen_dataex_send_mac_leds (mtlk_core_t *core,
                                     mtlk_core_ui_gen_data_t *req,
                                     mtlk_clpb_t *clpb)
{
  int res = 0;
  WE_GEN_DATAEX_RESPONSE resp;
  UMI_SET_LED leds_cfg;
  WE_GEN_DATAEX_LED *leds_status = NULL;

  MTLK_ASSERT(NULL != req);

  memset(&resp, 0, sizeof(resp));
  memset(&leds_cfg, 0, sizeof(leds_cfg));

  leds_status = &req->leds_status;

  resp.ver = WE_GEN_DATAEX_PROTO_VER;
  if (req->request.datalen < sizeof(WE_GEN_DATAEX_LED)) {
    resp.status = WE_GEN_DATAEX_DATABUF_TOO_SMALL;
    resp.datalen = sizeof(WE_GEN_DATAEX_LED);
    goto end;
  }

  ILOG2_DD("u8BasebLed = %d, u8LedStatus = %d", req->leds_status.u8BasebLed,
    req->leds_status.u8LedStatus);

  resp.datalen = sizeof(leds_status);

  leds_cfg.u8LedStatus = req->leds_status.u8LedStatus;
  leds_cfg.u8BasebLed  = req->leds_status.u8BasebLed;

  res = _mtlk_core_set_leds_cfg(core, &leds_cfg);
  if (MTLK_ERR_OK != res) {
    resp.status = WE_GEN_DATAEX_FAIL;
  }
  else {
    resp.status = WE_GEN_DATAEX_SUCCESS;
  }
 
  res = mtlk_clpb_push(clpb, &resp, sizeof(resp));
 
end:
  return res;
}

static int
_mtlk_core_gen_data_exchange(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_ui_gen_data_t *req = NULL;
  uint32 req_size;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  
  req = mtlk_clpb_enum_get_next(clpb, &req_size);

  MTLK_ASSERT(NULL != req);
  MTLK_ASSERT(sizeof(*req) == req_size);

  switch (req->request.cmd_id) {
    case WE_GEN_DATAEX_CMD_CONNECTION_STATS:
      res = _mtlk_core_gen_dataex_get_connection_stats(core, &req->request, clpb);
      break;
    case WE_GEN_DATAEX_CMD_STATUS:
      res = _mtlk_core_gen_dataex_get_status(core, &req->request, clpb);
      break;
    case WE_GEN_DATAEX_CMD_LEDS_MAC:
      res = _mtlk_core_gen_dataex_send_mac_leds(core, req, clpb);
      break;
    default:
      MTLK_ASSERT(0);
   }

  return res;
}

uint32
mtlk_core_get_available_bitrates (struct nic *nic)
{
  uint8 net_mode;
  uint32 mask = 0;

  /* Get all needed MIBs */
  if (nic->net_state == NET_STATE_CONNECTED)
    net_mode =  mtlk_core_get_network_mode_cur(nic);
  else
    net_mode = mtlk_core_get_network_mode_cfg(nic);
  mask = get_operate_rate_set(net_mode);
  ILOG3_D("Configuration mask: 0x%08x", mask);

  return mask;
}

int
mtlk_core_set_gen_ie (struct nic *nic, u8 *ie, u16 ie_len, u8 ie_type)
{
  int res = 0;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry;
  UMI_GENERIC_IE   *ie_req;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, 
                                                 mtlk_vap_get_txmm(nic->vap_handle),
                                                 NULL);
  if (!man_entry) {
    ELOG_D("CID-%04x: No man entry available", mtlk_vap_get_oid(nic->vap_handle));
    res = -EAGAIN;
    goto end;
  }

  man_entry->id = UM_MAN_SET_IE_REQ;
  man_entry->payload_size = sizeof(UMI_GENERIC_IE);
  ie_req = (UMI_GENERIC_IE*) man_entry->payload;
  memset(ie_req, 0, sizeof(*ie_req));

  ie_req->u8Type = ie_type;

  if (ie_len > sizeof(ie_req->au8IE)) {
    ELOG_DDD("CID-%04x: invalid IE length (%i > %i)", mtlk_vap_get_oid(nic->vap_handle),
        ie_len, (int)sizeof(ie_req->au8IE));
    res = -EINVAL;
    goto end;
  }
  if (ie && ie_len)
    memcpy(ie_req->au8IE, ie, ie_len);
  ie_req->u16Length = cpu_to_le16(ie_len);
  
  if (mtlk_txmm_msg_send_blocked(&man_msg, 
                                 MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: cannot set IE to MAC", mtlk_vap_get_oid(nic->vap_handle));
    res = -EINVAL;
  }
  
end:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);
    
  return res;
} 

static int
mtlk_core_is_band_supported(mtlk_core_t *nic, unsigned band)
{
  if (band == MTLK_HW_BAND_BOTH && mtlk_vap_is_ap(nic->vap_handle)) // AP can't be dual-band
    return MTLK_ERR_UNKNOWN;

  return mtlk_eeprom_is_band_supported(mtlk_core_get_eeprom(nic), band);
}

int
mtlk_core_update_network_mode(mtlk_core_t* nic, uint8 net_mode)
{
  mtlk_core_t *core = NULL;

  MTLK_ASSERT(NULL != nic);

  /* Update network mode for Master Core */
  if(mtlk_vap_is_slave_ap(nic->vap_handle)) {
    core = mtlk_core_get_master(nic);
    MTLK_ASSERT(NULL != core);
  }
  else {
    core = nic;
  }

  if (mtlk_core_is_band_supported(core, net_mode_to_band(net_mode)) != MTLK_ERR_OK) {
    if (net_mode_to_band(net_mode) == MTLK_HW_BAND_BOTH) {
      /*
       * Just in case of single-band hardware
       * continue to use `default' frequency band,
       * which is de facto correct.
       */
      ELOG_D("CID-%04x: dualband isn't supported", mtlk_vap_get_oid(core->vap_handle));
      return MTLK_ERR_OK;
    } else {
      ELOG_DS("CID-%04x: %s band isn't supported", mtlk_vap_get_oid(core->vap_handle),
              mtlk_eeprom_band_to_string(net_mode_to_band(net_mode)));
      return MTLK_ERR_NOT_SUPPORTED;
    }
  }
  if (is_ht_net_mode(net_mode) && core->slow_ctx->wep_enabled) {
    if (mtlk_vap_is_ap(core->vap_handle)) {
      ELOG_DS("CID-%04x: AP: %s network mode isn't supported for WEP", mtlk_vap_get_oid(core->vap_handle),
           net_mode_to_string(net_mode));
      return MTLK_ERR_NOT_SUPPORTED;
    }
    else if (!is_mixed_net_mode(net_mode)) {
      ELOG_DS("CID-%04x: STA: %s network mode isn't supported for WEP", mtlk_vap_get_oid(core->vap_handle),
           net_mode_to_string(net_mode));
      return MTLK_ERR_NOT_SUPPORTED;
    }
  }
  ILOG1_S("Set Network Mode to %s", net_mode_to_string(net_mode));
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_NET_MODE_CUR, net_mode);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_NET_MODE_CFG, net_mode);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_FREQ_BAND_CUR, net_mode_to_band(net_mode));
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_FREQ_BAND_CFG, net_mode_to_band(net_mode));
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_IS_HT_CUR, is_ht_net_mode(net_mode));
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_IS_HT_CFG, is_ht_net_mode(net_mode));

  if (!is_ht_net_mode(net_mode)) {
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE, SPECTRUM_20MHZ);
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, SPECTRUM_20MHZ);
    WLOG_DS("CID-%04x: 20MHz spectrum forced for %s", mtlk_vap_get_oid(core->vap_handle), net_mode_to_string(net_mode));
  }

  /* The set of supported bands may be changed by this request.           */
  /* Scan cache to be cleared to throw out BSS from unsupported now bands */
  mtlk_cache_clear(&core->slow_ctx->cache);
  return MTLK_ERR_OK;
}

int
mtlk_handle_bar (mtlk_handle_t context, uint8 *ta, uint8 tid, uint16 ssn)
{
  struct nic *nic = (struct nic *)context;
  int res;
  sta_entry *sta = NULL;

  nic->pstats.bars_cnt++;

  if (tid >= NTS_TIDS) {
    ELOG_DD("CID-%04x: Received BAR with wrong TID (%u)", mtlk_vap_get_oid(nic->vap_handle), tid);
    return -1;
  }

  sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ta);
  if (sta == NULL) {
    ELOG_DY("CID-%04x: Received BAR from unknown peer %Y", mtlk_vap_get_oid(nic->vap_handle), ta);
    return -1;
  }

  ILOG2_YD("Received BAR from %Y TID %u", ta, tid);

  res = mtlk_sta_process_bar(sta, tid, ssn);

  mtlk_sta_decref(sta); /* De-reference of find */

  if(res != 0) {
    ELOG_DYD("CID-%04x: Failed to process BAR (STA %Y TID %u)", mtlk_vap_get_oid(nic->vap_handle), ta, tid);
  }
  
  return res;
}

#ifdef MTCFG_RF_MANAGEMENT_MTLK
mtlk_rf_mgmt_t* __MTLK_IFUNC
mtlk_get_rf_mgmt (mtlk_handle_t context)
{
  struct nic *nic = (struct nic *)context;

  return nic->rf_mgmt;
}
#endif

static void
mtlk_parse_a_msdu(mtlk_core_t* nic, mtlk_nbuf_t *nbuf, int a_msdu_len)
{
  int subpacket_len, pad;
  struct ethhdr *ether_header;
  mtlk_nbuf_t *sub_nbuf;

  ILOG5_D("Parsing A-MSDU: length %d", a_msdu_len);

  while (a_msdu_len) {
    ether_header = (struct ethhdr *)nbuf->data;
    subpacket_len = ntohs(ether_header->h_proto) + sizeof(struct ethhdr);

    ILOG5_D("A-MSDU subpacket: length = %d", subpacket_len);

    /* check if we need to clone a packet */
    a_msdu_len -= subpacket_len;
    ASSERT(a_msdu_len >= 0);

    if (!a_msdu_len) {
      sub_nbuf = nbuf;
      pad = 0;
    } else {
      sub_nbuf = mtlk_df_nbuf_clone_no_priv(_mtlk_core_get_master_df(nic), nbuf);
      /* skip padding */
      pad = (4 - (subpacket_len & 0x03)) & 0x03;
      a_msdu_len -= pad;
      if (sub_nbuf) {
        mtlk_nbuf_priv_set_src_sta(mtlk_nbuf_priv(sub_nbuf), 
                                   mtlk_nbuf_priv_get_src_sta(mtlk_nbuf_priv(nbuf)));
      }
      else {
        WLOG_D("Cannot clone A-MSDU (len=%d)", a_msdu_len);
      }
      mtlk_df_nbuf_pull(nbuf, subpacket_len + pad);
    }

    if (sub_nbuf) {
      /* cut everything after data */
      mtlk_df_nbuf_trim(sub_nbuf, subpacket_len);
      /* for A-MSDU case we need to skip LLC/SNAP header */
      memmove(sub_nbuf->data + sizeof(mtlk_snap_hdr_t) + sizeof(mtlk_llc_hdr_t),
        ether_header, ETH_ALEN * 2);
      mtlk_df_nbuf_pull(sub_nbuf, sizeof(mtlk_snap_hdr_t) + sizeof(mtlk_llc_hdr_t));
      /* pass extracted subpacket to the OS */
      analyze_rx_packet(nic, sub_nbuf);
      mtlk_mc_parse(nic, sub_nbuf);
      send_up(nic, sub_nbuf);
    }
  }
}

/*
 * Definitions and macros below are used only for the packet's header transformation
 * For more information, please see following documents:
 *   - IEEE 802.1H standard
 *   - IETF RFC 1042
 *   - IEEE 802.11n standard draft 5 Annex M
 * */

#define _8021H_LLC_HI4BYTES             0xAAAA0300
#define _8021H_LLC_LO2BYTES_CONVERT     0x0000
#define RFC1042_LLC_LO2BYTES_TUNNEL     0x00F8

/* Default ISO/IEC conversion
 * we need to keep full LLC header and store packet length in the T/L subfield */
#define _8021H_CONVERT(ether_header, nbuf, data_offset) \
  data_offset -= sizeof(struct ethhdr); \
  ether_header = (struct ethhdr *)(nbuf->data + data_offset); \
  ether_header->h_proto = htons(nbuf->len - data_offset - sizeof(struct ethhdr))

/* 802.1H encapsulation
 * we need to remove LLC header except the 'type' field */
#define _8021H_DECAPSULATE(ether_header, nbuf, data_offset) \
  data_offset -= sizeof(struct ethhdr) - (sizeof(mtlk_snap_hdr_t) + sizeof(mtlk_llc_hdr_t)); \
  ether_header = (struct ethhdr *)(nbuf->data + data_offset)

static int
handle_rx_ind (mtlk_core_t *nic, mtlk_nbuf_t *nbuf, uint16 msdulen,
               const MAC_RX_ADDITIONAL_INFO_T *mac_rx_info)
{
  int res = MTLK_ERR_OK; /* Do not free nbuf */
  int off;
  mtlk_nbuf_priv_t *nbuf_priv;
  unsigned char fromDS, toDS;
  uint16 seq = 0, frame_ctl;
  uint16 frame_subtype;
  uint16 priority = 0, qos = 0;
  unsigned int a_msdu = 0;
  unsigned char *cp, *addr1, *addr2;
  sta_entry *sta = NULL;
  uint8 key_idx;
  int bridge_mode;

  ILOG4_V("Rx indication");
  bridge_mode = MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE);;

  // Set the size of the nbuff data
  
  if (msdulen > mtlk_df_nbuf_get_tail_room_size(nbuf)) {
    ELOG_DDD("CID-%04x: msdulen > nbuf size ->> %d > %d", mtlk_vap_get_oid(nic->vap_handle),
          msdulen,
          mtlk_df_nbuf_get_tail_room_size(nbuf));
  }

  mtlk_df_nbuf_put(nbuf, msdulen);

  // Get pointer to private area
  nbuf_priv = mtlk_nbuf_priv(nbuf);

  /* store vap index in private data,
   * Each packet must be aligned with the corresponding VAP */
  mtlk_nbuf_priv_set_vap_handle(nbuf_priv, nic->vap_handle);

/*


802.11n data frame from AP:

        |----------------------------------------------------------------|
 Bytes  |  2   |  2    |  6  |  6  |  6  |  2  | 6?  | 2?  | 0..2312 | 4 |
        |------|-------|-----|-----|-----|-----|-----|-----|---------|---|
 Descr. | Ctl  |Dur/ID |Addr1|Addr2|Addr3| Seq |Addr4| QoS |  Frame  |fcs|
        |      |       |     |     |     | Ctl |     | Ctl |  data   |   |
        |----------------------------------------------------------------|
Total: 28-2346 bytes

Existance of Addr4 in frame is optional and depends on To_DS From_DS flags.
Existance of QoS_Ctl is also optional and depends on Ctl flags.
(802.11n-D1.0 describes also HT Control (0 or 4 bytes) field after QoS_Ctl
but we don't support this for now.)

Interpretation of Addr1/2/3/4 depends on To_DS From_DS flags:

To DS From DS   Addr1   Addr2   Addr3   Addr4
---------------------------------------------
0       0       DA      SA      BSSID   N/A
0       1       DA      BSSID   SA      N/A
1       0       BSSID   SA      DA      N/A
1       1       RA      TA      DA      SA


frame data begins with 8 bytes of LLC/SNAP:

        |-----------------------------------|
 Bytes  |  1   |   1  |  1   |    3   |  2  |
        |-----------------------------------|
 Descr. |        LLC         |     SNAP     |
        |-----------------------------------+
        | DSAP | SSAP | Ctrl |   OUI  |  T  |
        |-----------------------------------|
        |  AA  |  AA  |  03  | 000000 |     |
        |-----------------------------------|

From 802.11 data frame that we receive from MAC we are making
Ethernet DIX (II) frame.

Ethernet DIX (II) frame format:

        |------------------------------------------------------|
 Bytes  |  6  |  6  | 2 |         46 - 1500               |  4 |
        |------------------------------------------------------|
 Descr. | DA  | SA  | T |          Data                   | FCS|
        |------------------------------------------------------|

So we overwrite 6 bytes of LLC/SNAP with SA.

*/

  ILOG4_V("Munging IEEE 802.11 header to be Ethernet DIX (II), irrevesible!");
  cp = (unsigned char *) nbuf->data;

  mtlk_dump(4, cp, 64, "dump of recvd .11n packet");

  // Chop the last four bytes (FCS)
  mtlk_df_nbuf_trim(nbuf, nbuf->len-4);

  frame_ctl = mtlk_wlan_pkt_get_frame_ctl(cp);
  addr1 = WLAN_GET_ADDR1(cp);
  addr2 = WLAN_GET_ADDR2(cp);

  ILOG4_D("frame control - %04x", frame_ctl);

  /* Try to find source MAC of transmitter */
  sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, addr2);

  /*
  Excerpts from "IEEE P802.11e/D13.0, January 2005" p.p. 22-23
  Type          Subtype     Description
  -------------------------------------------------------------
  00 Management 0000        Association request
  00 Management 0001        Association response
  00 Management 0010        Reassociation request
  00 Management 0011        Reassociation response
  00 Management 0100        Probe request
  00 Management 0101        Probe response
  00 Management 0110-0111   Reserved
  00 Management 1000        Beacon
  00 Management 1001        Announcement traffic indication message (ATIM)
  00 Management 1010        Disassociation
  00 Management 1011        Authentication
  00 Management 1100        Deauthentication
  00 Management 1101        Action
  00 Management 1101-1111   Reserved
  01 Control    0000-0111   Reserved
  01 Control    1000        Block Acknowledgement Request (BlockAckReq)
  01 Control    1001        Block Acknowledgement (BlockAck)
  01 Control    1010        Power Save Poll (PS-Poll)
  01 Control    1011        Request To Send (RTS)
  01 Control    1100        Clear To Send (CTS)
  01 Control    1101        Acknowledgement (ACK)
  01 Control    1110        Contention-Free (CF)-End
  01 Control    1111        CF-End + CF-Ack
  10 Data       0000        Data
  10 Data       0001        Data + CF-Ack
  10 Data       0010        Data + CF-Poll
  10 Data       0011        Data + CF-Ack + CF-Poll
  10 Data       0100        Null function (no data)
  10 Data       0101        CF-Ack (no data)
  10 Data       0110        CF-Poll (no data)
  10 Data       0111        CF-Ack + CF-Poll (no data)
  10 Data       1000        QoS Data
  10 Data       1001        QoS Data + CF-Ack
  10 Data       1010        QoS Data + CF-Poll
  10 Data       1011        QoS Data + CF-Ack + CF-Poll
  10 Data       1100        QoS Null (no data)
  10 Data       1101        Reserved
  10 Data       1110        QoS CF-Poll (no data)
  10 Data       1111        QoS CF-Ack + CF-Poll (no data)
  11 Reserved   0000-1111   Reserved
  */

  // FIXME: ADD DEFINITIONS!!!!
  // XXX, klogg: see frame.h

  switch(WLAN_FC_GET_TYPE(frame_ctl))
  {
  case IEEE80211_FTYPE_DATA:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_DAT);
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_DAT_FRAMES_RECEIVED);
    // Normal data
    break;
  case IEEE80211_FTYPE_MGMT:
    mtlk_process_man_frame(HANDLE_T(nic), sta, &nic->slow_ctx->scan, &nic->slow_ctx->cache,
        nic->slow_ctx->aocs, nbuf->data, nbuf->len, mac_rx_info);
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_MAN_FRAMES_RECEIVED);

    frame_subtype = (frame_ctl & FRAME_SUBTYPE_MASK) >> FRAME_SUBTYPE_SHIFT;
    ILOG4_D("Subtype is %d", frame_subtype);

    if ((frame_subtype == MAN_TYPE_BEACON && !mtlk_vap_is_ap(nic->vap_handle)) ||
        frame_subtype == MAN_TYPE_PROBE_RES ||
        frame_subtype == MAN_TYPE_PROBE_REQ) {

      // Workaraund for WPS (wsc-1.7.0) - send channel instead of the Dur/ID field */
      *(uint16 *)(nbuf->data + 2) = mac_rx_info->u8Channel;

      mtlk_nl_send_brd_msg(nbuf->data, nbuf->len, GFP_ATOMIC,
                             NETLINK_SIMPLE_CONFIG_GROUP, NL_DRV_CMD_MAN_FRAME);
    }

    res = MTLK_ERR_NOT_IN_USE;
    goto end;
  case IEEE80211_FTYPE_CTL:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_CTL);
    mtlk_process_ctl_frame(HANDLE_T(nic), nbuf->data, nbuf->len);
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_CTL_FRAMES_RECEIVED);
    res = MTLK_ERR_NOT_IN_USE; /* Free nbuf */
    goto end;
  default:
    ILOG2_D("Unknown header type, frame_ctl %04x", frame_ctl);
    res = MTLK_ERR_NOT_IN_USE; /* Free nbuf */
    goto end;
  }

  ILOG4_Y("802.11n rx TA: %Y", addr2);
  ILOG4_Y("802.11n rx RA: %Y", addr1);

  if (sta == NULL) {
    ILOG2_V("SOURCE of RX packet not found!");
    res = MTLK_ERR_NOT_IN_USE; /* Free nbuf */
    goto end;
  }
  mtlk_sta_update_rx_rate(sta, mac_rx_info);
  mtlk_nbuf_priv_set_src_sta(nbuf_priv, sta);

  /* Peers are updated on any data packet including NULL */
  mtlk_sta_on_frame_arrived(sta, mac_rx_info->MaxRssi);

  seq = mtlk_wlan_pkt_get_seq(cp);
  ILOG3_D("seq %d", seq);

  if (WLAN_FC_IS_NULL_PKT(frame_ctl)) {
    ILOG3_D("Null data packet, frame ctl - 0x%04x", frame_ctl);
    res = MTLK_ERR_NOT_IN_USE; /* Free nbuf */
    goto end;
  }

  off = mtlk_wlan_get_hdrlen(frame_ctl);
  ILOG3_D("80211_hdrlen - %d", off);
  if (WLAN_FC_IS_QOS_PKT(frame_ctl)) {
    u16 qos_ctl = mtlk_wlan_pkt_get_qos_ctl(cp, off);
    priority = WLAN_QOS_GET_PRIORITY(qos_ctl);
    a_msdu = WLAN_QOS_GET_MSDU(qos_ctl);
  }

  qos = mtlk_qos_get_ac_by_tid(priority);
#ifdef MTLK_DEBUG_CHARIOT_OOO
  nbuf_priv->seq_qos = qos;
#endif

#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
  if (mtlk_vap_is_ap(nic->vap_handle)) {
    mtlk_aocs_on_rx_msdu(mtlk_core_get_master(nic)->slow_ctx->aocs, qos);
  }
#endif

  fromDS = WLAN_FC_GET_FROMDS(frame_ctl);
  toDS   = WLAN_FC_GET_TODS(frame_ctl);
  ILOG3_DD("FromDS %d, ToDS %d", fromDS, toDS);

  /* Check if packet was directed to us */
  if (0 == MTLK_CORE_HOT_PATH_PDB_CMP_MAC(nic, CORE_DB_CORE_MAC_ADDR, addr1)) {
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_DIRECTED);
  }
  
  nic->pstats.ac_rx_counter[qos]++;
  nic->pstats.sta_session_rx_packets++;

  mtlk_nbuf_priv_set_rcn_bits(nbuf_priv, mac_rx_info->u8RSN);

  /* data offset should account also security info if it is there */
  off += get_rsc_buf(nic, nbuf, off);
  /* See CCMP/TKIP frame header in standard - 8.3.2.2/8.3.3.2 clause */
  key_idx = (mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 3) & 0300) >> 6;

  /* process regular MSDU */
  if (likely(!a_msdu)) {
    struct ethhdr *ether_header;
    /* Raw LLC header split into 3 parts to make processing more convenient */
    struct llc_hdr_raw_t {
      uint32 hi4bytes;
      uint16 lo2bytes;
      uint16 ether_type;
    } *llc_hdr_raw = (struct llc_hdr_raw_t *)(nbuf->data + off);

    if (llc_hdr_raw->hi4bytes == __constant_htonl(_8021H_LLC_HI4BYTES)) {
      switch (llc_hdr_raw->lo2bytes) {
      case __constant_htons(_8021H_LLC_LO2BYTES_CONVERT):
        switch (llc_hdr_raw->ether_type) {
        /* AppleTalk and IPX encapsulation - integration service STT (see table M.1) */
        case __constant_htons(ETH_P_AARP):
        case __constant_htons(ETH_P_IPX):
          _8021H_CONVERT(ether_header, nbuf, off);
          break;
        /* default encapsulation
         * TODO: make sure it will be the shortest path */
        default:
          _8021H_DECAPSULATE(ether_header, nbuf, off);
          break;
        }
        break;
      case __constant_htons(RFC1042_LLC_LO2BYTES_TUNNEL):
        _8021H_DECAPSULATE(ether_header, nbuf, off);
        break;
      default:
        _8021H_CONVERT(ether_header, nbuf, off);
        break;
      }
    } else {
      _8021H_CONVERT(ether_header, nbuf, off);
    }
    mtlk_df_nbuf_pull(nbuf, off);
    ether_header = (struct ethhdr *)nbuf->data;

    /* save SRC/DST MAC adresses from 802.11 header to 802.3 header */
    mtlk_wlan_get_mac_addrs(cp, fromDS, toDS, ether_header->h_source, ether_header->h_dest);

    /* Check if packet received from WDS HOST
     * (HOST mac address is not equal to sender's MAC address) */
    if ((bridge_mode == BR_MODE_WDS) &&
        memcmp(ether_header->h_source, mtlk_sta_get_addr(sta)->au8Addr, ETH_ALEN)) {

      /* On AP we need to update HOST's entry in database of registered
       * HOSTs behid connected STAs */
      if (mtlk_vap_is_ap(nic->vap_handle)) {
        mtlk_hstdb_update_host(&nic->slow_ctx->hstdb, ether_header->h_source, sta);

      /* On STA we search if this HOST registered in STA's database of
       * connected HOSTs (that are behind this STA) */
      } else if (mtlk_hstdb_find_sta(&nic->slow_ctx->hstdb, ether_header->h_source) != NULL) {
          ILOG4_V("Packet from our own host received from AP");
          res = MTLK_ERR_NOT_IN_USE; /* Free nbuf */
          goto end;
      }
    }

#ifdef MTLK_DEBUG_IPERF_PAYLOAD_RX
    debug_ooo_analyze_packet(TRUE, nbuf, seq);
#endif

    analyze_rx_packet(nic, nbuf);
    mtlk_mc_parse(nic, nbuf);

    /* try to reorder packet */
    if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_DIRECTED) &&
        (mtlk_vap_is_ap(nic->vap_handle) || WLAN_FC_IS_QOS_PKT(frame_ctl))) {
      mtlk_sta_reorder_packet(sta, priority, seq, nbuf);
    } else {
      mtlk_detect_replay_or_sendup(nic, nbuf, nic->group_rsc[key_idx]);
    }

  /* process A-MSDU */
  } else {
    uint8 *rsc;

    /* for A-MSDU there cannot be any kind of reordering so we will do
     * replay analysis only for the header and not for each subpacket */
    if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_DIRECTED))
      rsc = mtlk_sta_get_rsc(sta, priority);
    else
      rsc = nic->group_rsc[key_idx];
    if (detect_replay(nic, nbuf, rsc) != 0) {
      res = MTLK_ERR_NOT_IN_USE; /* Free nbuf */
      goto end;
    }

    mtlk_df_nbuf_pull(nbuf, off);
    /* parse A-MSDU and send all subpackets to OS */
    mtlk_parse_a_msdu(nic, nbuf, nbuf->len);
  }

end:
  if (sta) {
    mtlk_sta_decref(sta); /* De-reference of find */
  }
  return res;
}

static int __MTLK_IFUNC
_handle_bss_created(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);
  MTLK_ASSERT(size == sizeof(UMI_NETWORK_EVENT));

  ILOG1_Y("Network created, BSSID %Y", ((UMI_NETWORK_EVENT *)payload)->sBSSID.au8Addr);
  _mtlk_core_trigger_connect_complete_event(nic, TRUE);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_bss_connecting(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  MTLK_UNREFERENCED_PARAM(core_object);
  MTLK_UNREFERENCED_PARAM(payload);
  MTLK_UNREFERENCED_PARAM(size);
  ILOG1_V("Connecting to network...");

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_bss_connected(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  MTLK_UNREFERENCED_PARAM(core_object);
  MTLK_ASSERT(size == sizeof(UMI_NETWORK_EVENT));

  ILOG1_Y("Network created, BSSID %Y", ((UMI_NETWORK_EVENT *)payload)->sBSSID.au8Addr);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_bss_failed(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);
  const UMI_NETWORK_EVENT *network_event = (const UMI_NETWORK_EVENT *)payload;
  uint16 reason;
  MTLK_ASSERT(size == sizeof(UMI_NETWORK_EVENT));

  reason = network_event->u8Reason;
  WLOG_DD("CID-%04x: Failed to create/connect to network, reason %d",
           mtlk_vap_get_oid(nic->vap_handle), reason);

  // AP is dead? Force user to rescan to see this BSS again
  if (reason == UMI_BSS_JOIN_FAILED) {
    MTLK_ASSERT(!mtlk_vap_is_ap(nic->vap_handle));
    mtlk_cache_remove_bss_by_bssid(&nic->slow_ctx->cache, 
                                   network_event->sBSSID.au8Addr);
  }

  _mtlk_core_trigger_connect_complete_event(nic, FALSE);

  return MTLK_ERR_OK;
};

static int __MTLK_IFUNC
_mtlk_handle_unknown_network_ind(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  MTLK_ASSERT(size == sizeof(uint16));

  ELOG_DD("CID-%04x: Unrecognised network event %d",
      mtlk_vap_get_oid(HANDLE_T_PTR(mtlk_core_t, core_object)->vap_handle), *(uint16 *)payload);
  return MTLK_ERR_OK;
}

static void
_handle_network_event (struct nic *nic, UMI_NETWORK_EVENT *psNetwork)
{
  uint16 id = MAC_TO_HOST16(psNetwork->u16BSSstatus);

  switch (id)
  {
  case UMI_BSS_CREATED:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_bss_created,
      HANDLE_T(nic), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_CONNECTING:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_bss_connecting,
                          HANDLE_T(nic), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_CONNECTED:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_bss_connected,
                          HANDLE_T(nic), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_FAILED:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_bss_failed,
                          HANDLE_T(nic), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_CHANNEL_SWITCH_DONE:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, mtlk_dot11h_handle_channel_switch_done,
                              HANDLE_T(mtlk_core_get_dfs(nic)), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_CHANNEL_PRE_SWITCH_DONE:
    MTLK_ASSERT(!mtlk_vap_is_slave_ap(nic->vap_handle));
    _mtlk_process_hw_task(nic, SYNCHRONOUS, mtlk_dot11h_handle_channel_pre_switch_done,
                              HANDLE_T(mtlk_core_get_dfs(nic)), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_CHANNEL_SWITCH_NORMAL:
  case UMI_BSS_CHANNEL_SWITCH_SILENT:
    MTLK_ASSERT(!mtlk_vap_is_slave_ap(nic->vap_handle));
    _mtlk_process_hw_task(nic, SYNCHRONOUS, mtlk_dot11h_handle_channel_switch_ind,
                              HANDLE_T(mtlk_core_get_dfs(nic)), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_RADAR_NORM:
  case UMI_BSS_RADAR_HOP:
    MTLK_ASSERT(!mtlk_vap_is_slave_ap(nic->vap_handle));
    _mtlk_process_hw_task(nic, SYNCHRONOUS, mtlk_dot11h_handle_radar_ind,
                              HANDLE_T(mtlk_core_get_dfs(nic)), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  default:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_unknown_network_ind,
                          HANDLE_T(nic), &id, sizeof(uint16));
    break;
  }
}

static int
_mtlk_core_set_wep_key_blocked (struct nic      *nic, 
                                const IEEE_ADDR *addr)
{
  int               res             = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry       = NULL;
  uint16            default_key_idx = nic->slow_ctx->default_key;
  UMI_SET_KEY      *umi_key;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, 
                                                 mtlk_vap_get_txmm(nic->vap_handle), 
                                                 &res);
  if (!man_entry) {
    ELOG_DD("CID-%04x: No man entry available (res = %d)", mtlk_vap_get_oid(nic->vap_handle), res);
    goto end;
  }

  umi_key = (UMI_SET_KEY*)man_entry->payload;
  memset(umi_key, 0, sizeof(*umi_key));

  man_entry->id           = UM_MAN_SET_KEY_REQ;
  man_entry->payload_size = sizeof(*umi_key);

  umi_key->u16CipherSuite     = HOST_TO_MAC16(UMI_RSN_CIPHER_SUITE_WEP40);
  umi_key->sStationID         = *addr;
  if (mtlk_osal_eth_is_broadcast(addr->au8Addr))
    umi_key->u16KeyType = cpu_to_le16(UMI_RSN_GROUP_KEY);
  else
    umi_key->u16KeyType = cpu_to_le16(UMI_RSN_PAIRWISE_KEY);
  umi_key->u16DefaultKeyIndex = HOST_TO_MAC16(default_key_idx);

  memcpy(umi_key->au8Tk1, 
         nic->slow_ctx->wep_keys.sKey[default_key_idx].au8KeyData, 
         nic->slow_ctx->wep_keys.sKey[default_key_idx].u8KeyLength);

  mtlk_dump(4, umi_key, sizeof(*umi_key), "dump of UMI_SET_KEY");

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: mtlk_mm_send failed: %d", mtlk_vap_get_oid(nic->vap_handle), res);
    goto end;
  }

  umi_key->u16Status = MAC_TO_HOST16(umi_key->u16Status);

  if (umi_key->u16Status != UMI_OK) {
    ELOG_DYD("CID-%04x: %Y: status is %d", mtlk_vap_get_oid(nic->vap_handle), addr, umi_key->u16Status);
    res = MTLK_ERR_MAC;
    goto end;
  }

  res = MTLK_ERR_OK;

end:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

static void
_mtlk_core_notify_ap_of_station_connection(struct nic       *nic, 
                                           const IEEE_ADDR  *addr,
                                           const UMI_RSN_IE *rsn_ie,
                                           BOOL supports_20_40,
                                           BOOL received_scan_exemption,
                                           BOOL is_intolerant,
                                           BOOL is_legacy)
{

  const uint8 *rsnie     = rsn_ie->au8RsnIe;
  uint8        rsnie_id  = rsnie[0];

  /*
   * WARNING! When there is RSN IE, we send address
   * in IWEVREGISTERED event not in wrqu.addr.sa_data as usual,
   * but in extra along with RSN IE data.
   * Why? Because iwreq_data is union and there is no other way
   * to send address and RSN IE in one event.
   * IWEVREGISTERED event is handled in hostAPd only
   * so there might be not any collessions with such non-standart
   * implementation of it.
   */
  mtlk_dump(4, rsnie, sizeof(rsn_ie->au8RsnIe), "dump of RSNIE:");

  mtlk_20_40_register_station(mtlk_core_get_coex_sm(nic), addr, supports_20_40, received_scan_exemption, is_intolerant, is_legacy);

  if (rsnie_id) {
    mtlk_df_ui_notify_secure_node_connect(
        mtlk_vap_get_df(nic->vap_handle),
        addr->au8Addr, rsnie, rsnie[1] + 2);
  } else {
    mtlk_df_ui_notify_node_connect(mtlk_vap_get_df(nic->vap_handle), addr->au8Addr);
  }
}

static void
  _mtlk_core_notify_ap_of_station_disconnection(struct nic       *nic, 
                                                const IEEE_ADDR  *addr)
{
  mtlk_20_40_unregister_station(mtlk_core_get_coex_sm(nic), addr);
  mtlk_df_ui_notify_node_disconect(mtlk_vap_get_df(nic->vap_handle), addr->au8Addr);
}

static void
_mtlk_core_connect_sta_blocked (struct nic       *nic, 
                                const UMI_CONNECTION_EVENT *psConnect,
                                BOOL              reconnect)
{
  sta_entry *sta;
  const IEEE_ADDR  *addr = &psConnect->sStationID;
  const IEEE_ADDR  *prev_bssid = &psConnect->sPrevBSSID;
  const UMI_RSN_IE *rsn_ie = &psConnect->sRSNie;
  BOOL              is_ht_capable = psConnect->u8HTmode?TRUE:FALSE;
  BOOL              intolerant = FALSE;

  MTLK_ASSERT(mtlk_vap_is_ap(nic->vap_handle) || !_mtlk_core_has_connections(nic));

  sta = mtlk_stadb_add_sta(&nic->slow_ctx->stadb, addr->au8Addr, 
                           (mtlk_core_get_is_ht_cur(nic) && is_ht_capable));
  if (sta == NULL)
    goto end;

  if (mtlk_vap_is_ap(nic->vap_handle))
  {
    sta_capabilities sta_capabilities;

    if (!reconnect) {
      ILOG1_YS("Station %Y (%sN) has connected", addr,
           is_ht_capable ? "" : "non-");
    }
    else {
      ILOG1_YSY("Station %Y (%sN) has reconnected. Previous BSS was %Y",
          addr, is_ht_capable ? "" : "non-",
          prev_bssid);
    }

    if (nic->slow_ctx->wep_enabled && !nic->slow_ctx->wps_in_progress) {
      mtlk_sta_set_cipher(sta, IW_ENCODE_ALG_WEP);
      _mtlk_core_set_wep_key_blocked(nic, addr);
    }

    if ((psConnect->fortyMHzIntolerant == TRUE) || (psConnect->twentyMHzBssWidthRequest == TRUE)) {
      intolerant = TRUE;
    }

    _mtlk_core_notify_ap_of_station_connection(nic, addr, rsn_ie, psConnect->twentyFortyBssCoexistenceManagementSupport, psConnect->obssScanningExemptionGrant, intolerant, !is_ht_capable);

    mtlk_frame_process_sta_capabilities(&sta_capabilities,
        psConnect->sPeersCapabilities.u8LantiqProprietry,
        MAC_TO_HOST16(psConnect->sPeersCapabilities.u16HTCapabilityInfo),
        psConnect->sPeersCapabilities.AMPDU_Parameters,
        MAC_TO_HOST32(psConnect->sPeersCapabilities.tx_bf_capabilities),
        psConnect->boWMEsupported,
        MAC_TO_HOST32(psConnect->u32SupportedRates),
        mtlk_core_get_freq_band_cur(nic));

    mtlk_sta_set_capabilities(sta, &sta_capabilities);
  }
  else  // STA
  {
    mtlk_pdb_set_mac(mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID,
                     addr->au8Addr);

    if (!reconnect) {
      ILOG1_YS("connected to %Y (%sN)",
                       addr, 
                       is_ht_capable ? "" : "non-");
      nic->pstats.sta_session_rx_packets = 0;
      nic->pstats.sta_session_tx_packets = 0;
    }
    else {
      ILOG1_YY("connected to %Y, previous BSSID was %Y",
                      addr,
                      prev_bssid);
    }

    mtlk_df_ui_notify_association(mtlk_vap_get_df(nic->vap_handle), addr->au8Addr);
    
#ifdef PHASE_3
    mtlk_start_cache_update(nic);
#endif

    // make BSS we've connected to persistent in cache until we're disconnected
    mtlk_cache_set_persistent(&nic->slow_ctx->cache, 
                              addr->au8Addr,
                              TRUE);
  }

  if (nic->slow_ctx->rsnie.au8RsnIe[0]) {
    /* In WPA/WPA security start ADDBA after key is set */
    mtlk_sta_set_packets_filter(sta, MTLK_PCKT_FLTR_ALLOW_802_1X);
    ILOG1_Y("%Y: turn on 802.1x filtering due to RSN", mtlk_sta_get_addr(sta));
  } else if (!mtlk_vap_is_ap(nic->vap_handle) && nic->slow_ctx->wps_in_progress) {
    mtlk_sta_set_packets_filter(sta, MTLK_PCKT_FLTR_ALLOW_802_1X);
    ILOG1_Y("%Y: turn on 802.1x filtering due to WPS", mtlk_sta_get_addr(sta));
  } else {
    mtlk_sta_on_security_negotiated(sta);
  }

  mtlk_qos_reset_acm_bits(&nic->qos);

  mtlk_sta_decref(sta); /* De-reference by creator */

end:
  return;
}

static int
_mtlk_core_disconnect_sta_blocked (struct nic       *nic, 
                                   const IEEE_ADDR  *addr,
                                   uint16            reason)

{
  uint32      net_state  = mtlk_core_get_net_state(nic);

  MTLK_ASSERT(addr != NULL);

  if (net_state != NET_STATE_CONNECTED) {
    ILOG1_YD("Failed to connect to %Y for reason %d", addr, reason);

    if (reason == UMI_BSS_JOIN_FAILED) {
      /* AP is dead? Force user to rescan to see this BSS again */
      mtlk_cache_remove_bss_by_bssid(&nic->slow_ctx->cache, addr->au8Addr);
    }

    _mtlk_core_trigger_connect_complete_event(nic, FALSE);
  } 
  else {
    if (mtlk_vap_is_ap(nic->vap_handle)) {
      ILOG1_YD("STA %Y disconnected for reason %d", addr, reason);
    } else {
      ILOG1_YD("Disconnected from BSS %Y for reason %d", addr, reason);

      if (!mtlk_vap_is_slave_ap(nic->vap_handle)) {
        /* We could have BG scan doing right now.
           It must be terminated because scan module
           is not ready for background scan to normal scan
           mode switch on the fly.
         */
        scan_terminate(&nic->slow_ctx->scan);

        /* Since we disconnected switch scan mode to normal */
        mtlk_scan_set_background(&nic->slow_ctx->scan, FALSE);
      }
    }
  }
  
  /* send disconnect request */
  return _mtlk_core_send_disconnect_req_blocked(nic, addr, reason);
}


static int __MTLK_IFUNC
_mtlk_core_ap_disconnect_sta(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  int res = MTLK_ERR_PARAMS;
  uint8 *addr;
  uint32 addr_size;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    res = MTLK_ERR_NOT_SUPPORTED;
    goto finish;
  }

  addr = mtlk_clpb_enum_get_next(clpb, &addr_size);
  if (NULL == addr) {
    goto finish;
  }

  if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED) {
    ILOG1_Y("STA (%Y), not connected - request rejected", (const IEEE_ADDR *)addr);
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  res = _mtlk_core_disconnect_sta_blocked(nic,
                                          (const IEEE_ADDR *)addr,
                                           FM_STATUSCODE_USER_REQUEST);
finish:
  return res;
}

static int __MTLK_IFUNC
_mtlk_core_ap_disconnect_all (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  int res = MTLK_ERR_PARAMS;
  const sta_entry *sta = NULL;
  mtlk_stadb_iterator_t iter;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(mtlk_vap_is_ap(nic->vap_handle));
  MTLK_UNREFERENCED_PARAM(data);
  MTLK_UNREFERENCED_PARAM(data_size);

  if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED) {
    ILOG1_V("AP is down - request rejected");
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  sta = mtlk_stadb_iterate_first(&nic->slow_ctx->stadb, &iter);
  if (sta) {
    do {
      res = _mtlk_core_disconnect_sta_blocked(nic,
                                              mtlk_sta_get_addr(sta),
                                              FM_STATUSCODE_USER_REQUEST);
      if (res != MTLK_ERR_OK) {
        ELOG_YD("STA (%Y) disconnection failed (%d)", mtlk_sta_get_addr(sta), res);
        break;
      }
      sta = mtlk_stadb_iterate_next(&iter);
    } while (sta);
    mtlk_stadb_iterate_done(&iter);
  }

finish:
  return res;
}

static int _mtlk_core_on_association_event(mtlk_vap_handle_t vap_handle,
                                           IEEE_ADDR         mac_addr,
                                           uint16            status)
{
  mtlk_association_event_t assoc_event;

  memset(&assoc_event, 0, sizeof(mtlk_association_event_t));
  mtlk_osal_copy_eth_addresses((uint8*)&assoc_event.mac_addr, mac_addr.au8Addr);
  assoc_event.status = status;

  return mtlk_wssd_send_event(mtlk_vap_get_irbd(vap_handle),
                              MTLK_WSSA_DRV_EVENT_ASSOCIATION,
                              &assoc_event,
                              sizeof(mtlk_association_event_t));
}

static int _mtlk_core_on_authentication_event(mtlk_vap_handle_t vap_handle,
                                              IEEE_ADDR         mac_addr,
                                              uint16            auth_type,
                                              uint16            status)
{
  mtlk_authentication_event_t auth_event;

  memset(&auth_event, 0, sizeof(mtlk_authentication_event_t));

  mtlk_osal_copy_eth_addresses((uint8*)&auth_event.mac_addr, mac_addr.au8Addr);
  auth_event.auth_type = auth_type;
  auth_event.status = status;

  return mtlk_wssd_send_event(mtlk_vap_get_irbd(vap_handle),
                              MTLK_WSSA_DRV_EVENT_AUTHENTICATION,
                              &auth_event,
                              sizeof(mtlk_authentication_event_t));
}

static int _mtlk_core_on_peer_disconnect(mtlk_core_t *core,
                                        sta_entry   *sta,
                                        uint16       reason)
{
  mtlk_disconnection_event_t disconnect_event;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(sta != NULL);

  memset(&disconnect_event, 0, sizeof(disconnect_event));

  memcpy(&disconnect_event.mac_addr, mtlk_sta_get_addr(sta), sizeof(disconnect_event.mac_addr));

  disconnect_event.reason = reason;

  if((FM_STATUSCODE_AGED_OUT            == reason)   ||
     (FM_STATUSCODE_INACTIVITY          == reason)   ||
     (FM_STATUSCODE_USER_REQUEST        == reason)   ||
     (FM_STATUSCODE_PEER_PARAMS_CHANGED == reason))
  {
    disconnect_event.initiator = MTLK_DI_THIS_SIDE;
  }
  else disconnect_event.initiator = MTLK_DI_OTHER_SIDE;

  mtlk_sta_get_peer_stats(sta, &disconnect_event.peer_stats);
  mtlk_sta_get_peer_capabilities(sta, &disconnect_event.peer_capabilities);

  return mtlk_wssd_send_event(mtlk_vap_get_irbd(core->vap_handle),
                              MTLK_WSSA_DRV_EVENT_DISCONNECTION,
                              &disconnect_event,
                              sizeof(disconnect_event));
}

static int __MTLK_IFUNC
_handle_sta_connection_event (mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);
  const UMI_CONNECTION_EVENT *psConnect = 
    (const UMI_CONNECTION_EVENT *)payload;

  MTLK_ASSERT(size == sizeof(*psConnect));

  mtlk_dump(5, psConnect, sizeof(UMI_CONNECTION_EVENT), "UMI_CONNECTION_EVENT:");

  if (nic->is_stopped) {
    ILOG5_V("Connection event while core is down");
    return MTLK_ERR_OK; // do not process
  }

  switch (MAC_TO_HOST16(psConnect->u16Event)) {
  case UMI_CONNECTED:
    _mtlk_core_connect_sta_blocked(nic, psConnect, FALSE);
    if (FM_STATUSCODE_ASSOCIATED == MAC_TO_HOST16(psConnect->u16Reason)) {
      _mtlk_core_on_association_event(nic->vap_handle, psConnect->sStationID,
                                      FM_STATUSCODE_SUCCESSFUL);
    }
    break;
  case UMI_RECONNECTED:
    _mtlk_core_connect_sta_blocked(nic, psConnect, TRUE);
    break;
  case UMI_DISCONNECTED:
    _mtlk_core_disconnect_sta_blocked(nic, &psConnect->sStationID, 
      MAC_TO_HOST16(psConnect->u16FailReason));
    if (UMI_BSS_ASSOC_FAILED == MAC_TO_HOST16(psConnect->u16Reason)) {
      _mtlk_core_on_association_event(nic->vap_handle, psConnect->sStationID,
                                      MAC_TO_HOST16(psConnect->u16FailReason));
    }
    break;
  case UMI_AUTHENTICATION:
    _mtlk_core_on_authentication_event(nic->vap_handle, psConnect->sStationID,
                                       MAC_TO_HOST16(psConnect->u16Reason),
                                       MAC_TO_HOST16(psConnect->u16FailReason));
    break;
  case UMI_ASSOCIATION:
    MTLK_ASSERT(UMI_BSS_ASSOC_FAILED == MAC_TO_HOST16(psConnect->u16Reason));
    _mtlk_core_on_association_event(nic->vap_handle,
                                    psConnect->sStationID,
                                    MAC_TO_HOST16(psConnect->u16FailReason));
    break;
  default:
    ELOG_DD("CID-%04x: Unrecognized connection event %d", mtlk_vap_get_oid(nic->vap_handle), MAC_TO_HOST16(psConnect->u16Event));
    break;
  }

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_fw_connection_event_indication(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);

  if(!mtlk_vap_is_ap(nic->vap_handle)) {
    _mtlk_core_trigger_connect_complete_event(nic, TRUE);
  }
  _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_sta_connection_event,
                        core_object, payload, size);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_vap_removed_ind (mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);

  MTLK_UNREFERENCED_PARAM(payload);
  ILOG1_V("VAP removal indication received");
  mtlk_osal_event_set(&nic->slow_ctx->vap_removed_event);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
handleDisconnectSta (mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);
  int                                   res;
  struct mtlk_core_disconnect_sta_data *data =
    (struct mtlk_core_disconnect_sta_data *)payload;

  MTLK_ASSERT(size == sizeof(*data));

  res = _mtlk_core_disconnect_sta_blocked(nic, &data->addr, data->reason);
  if (data->res) {
    *data->res = res;
  }
  if (data->done_evt) {
    mtlk_osal_event_set(data->done_evt);
  }

  return MTLK_ERR_OK;
}

static int
_handle_dynamic_param_ind(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  UMI_DYNAMIC_PARAM_TABLE *param_table = (UMI_DYNAMIC_PARAM_TABLE *) data;
  int i;

  MTLK_ASSERT(sizeof(UMI_DYNAMIC_PARAM_TABLE) == data_size);

  for (i = 0; i < NTS_PRIORITIES; i++)
    ILOG5_DD("Set ACM bit for priority %d: %d", i, param_table->ACM_StateTable[i]);

  mtlk_qos_set_acm_bits(&nic->qos, param_table->ACM_StateTable);
  return MTLK_ERR_OK;
}

static int
_handle_security_alert_ind(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  UMI_SECURITY_ALERT *usa = (UMI_SECURITY_ALERT*)data;
  MTLK_ASSERT(sizeof(UMI_SECURITY_ALERT) == data_size);

  switch (usa->u16EventCode) {
  case UMI_RSN_EVENT_TKIP_MIC_FAILURE:
    {
      mtlk_df_ui_mic_fail_type_t mic_fail_type =
          (UMI_RSN_PAIRWISE_KEY == usa->u16KeyType) ? MIC_FAIL_PAIRWISE : MIC_FAIL_GROUP;

      mtlk_df_ui_notify_mic_failure(
          mtlk_vap_get_df(nic->vap_handle), usa->sStationID.au8Addr, mic_fail_type);

      _mtlk_core_on_mic_failure(nic, mic_fail_type);
    }
    break;
  }
  return MTLK_ERR_OK;
}

extern const char *mtlk_drv_info[];

static void 
mtlk_print_drv_info (void) {
#if (RTLOG_MAX_DLEVEL >= 1)
  int i = 0;
  ILOG1_V("*********************************************************");
  ILOG1_V("* Driver Compilation Details:");
  ILOG1_V("*********************************************************");
  while (mtlk_drv_info[i]) {
    ILOG1_S("* %s", mtlk_drv_info[i]);
    i++;
  }
  ILOG1_V("*********************************************************");
#endif
}

/* steps for init and cleanup */
MTLK_INIT_STEPS_LIST_BEGIN(core_slow_ctx)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, EEPROM)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, DFS)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, SERIALIZER)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, SET_NIC_CFG)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, WATCHDOG_TIMER_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, CONNECT_EVENT_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, VAP_REMOVED_EVENT_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, STADB_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, HSTDB_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(core_slow_ctx)
MTLK_INIT_STEPS_LIST_END(core_slow_ctx);

static void __MTLK_IFUNC
_mtlk_slow_ctx_cleanup(struct nic_slow_ctx *slow_ctx, struct nic* nic)
{
  MTLK_ASSERT(NULL != slow_ctx);
  MTLK_ASSERT(NULL != nic);

  MTLK_CLEANUP_BEGIN(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))
    
    MTLK_CLEANUP_STEP(core_slow_ctx, HSTDB_INIT, MTLK_OBJ_PTR(slow_ctx), 
                      mtlk_hstdb_cleanup, (&slow_ctx->hstdb));
    
    MTLK_CLEANUP_STEP(core_slow_ctx, STADB_INIT, MTLK_OBJ_PTR(slow_ctx), 
                      mtlk_stadb_cleanup, (&slow_ctx->stadb));
    
    MTLK_CLEANUP_STEP(core_slow_ctx, VAP_REMOVED_EVENT_INIT, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_osal_event_cleanup, (&slow_ctx->vap_removed_event));

    MTLK_CLEANUP_STEP(core_slow_ctx, CONNECT_EVENT_INIT, MTLK_OBJ_PTR(slow_ctx), 
                      mtlk_osal_event_cleanup, (&slow_ctx->connect_event));

    MTLK_CLEANUP_STEP(core_slow_ctx, WATCHDOG_TIMER_INIT, MTLK_OBJ_PTR(slow_ctx), 
                      mtlk_osal_timer_cleanup, (&slow_ctx->mac_watchdog_timer));

    MTLK_CLEANUP_STEP(core_slow_ctx, SET_NIC_CFG, MTLK_OBJ_PTR(slow_ctx), 
                      MTLK_NOACTION, ());

    MTLK_CLEANUP_STEP(core_slow_ctx, SERIALIZER, MTLK_OBJ_PTR(slow_ctx), 
                      mtlk_serializer_cleanup, (&slow_ctx->serializer));

    MTLK_CLEANUP_STEP(core_slow_ctx, DFS, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_dfs_delete, (slow_ctx->dot11h) )

    MTLK_CLEANUP_STEP(core_slow_ctx, EEPROM, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_eeprom_delete, (slow_ctx->ee_data) )

  MTLK_CLEANUP_END(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx));
}

static int __MTLK_IFUNC
_mtlk_slow_ctx_init(struct nic_slow_ctx *slow_ctx, struct nic* nic)
{
  MTLK_ASSERT(NULL != slow_ctx);
  MTLK_ASSERT(NULL != nic);

  memset(slow_ctx, 0, sizeof(struct nic_slow_ctx));
  slow_ctx->nic = nic;

  MTLK_INIT_TRY(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))

    MTLK_INIT_STEP_EX_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, EEPROM, MTLK_OBJ_PTR(slow_ctx),
                         mtlk_eeprom_create, (),
                         slow_ctx->ee_data, NULL != slow_ctx->ee_data, MTLK_ERR_NO_MEM);

    MTLK_INIT_STEP_EX_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, DFS, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_dfs_create, (),
                      slow_ctx->dot11h, NULL != slow_ctx->dot11h, MTLK_ERR_NO_MEM);

    MTLK_INIT_STEP(core_slow_ctx, SERIALIZER, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_serializer_init, (&slow_ctx->serializer, _MTLK_CORE_NUM_PRIORITIES));

    MTLK_INIT_STEP_VOID(core_slow_ctx, SET_NIC_CFG, MTLK_OBJ_PTR(slow_ctx),
                        mtlk_mib_set_nic_cfg, (nic));

    MTLK_INIT_STEP_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, WATCHDOG_TIMER_INIT, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_osal_timer_init, (&slow_ctx->mac_watchdog_timer,
                                             mac_watchdog_timer_handler,
                                             HANDLE_T(nic)));

    MTLK_INIT_STEP(core_slow_ctx, CONNECT_EVENT_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_osal_event_init, (&slow_ctx->connect_event));

    MTLK_INIT_STEP(core_slow_ctx, VAP_REMOVED_EVENT_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_osal_event_init, (&slow_ctx->vap_removed_event));

    MTLK_INIT_STEP(core_slow_ctx, STADB_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_stadb_init, (&slow_ctx->stadb, nic->vap_handle));
    
    MTLK_INIT_STEP(core_slow_ctx, HSTDB_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_hstdb_init, (&slow_ctx->hstdb, nic->vap_handle));

    slow_ctx->last_pm_spectrum = -1;
    slow_ctx->last_pm_freq = MTLK_HW_BAND_NONE;

    /* Initialize WEP keys */
    slow_ctx->wep_keys.sKey[0].u8KeyLength =
    slow_ctx->wep_keys.sKey[1].u8KeyLength =
    slow_ctx->wep_keys.sKey[2].u8KeyLength =
    slow_ctx->wep_keys.sKey[3].u8KeyLength =
    MIB_WEP_KEY_WEP1_LENGTH;

    nic->slow_ctx->tx_limits.num_tx_antennas = DEFAULT_NUM_TX_ANTENNAS;
    nic->slow_ctx->tx_limits.num_rx_antennas = DEFAULT_NUM_RX_ANTENNAS;
    
    nic->slow_ctx->deactivate_ts = INVALID_DEACTIVATE_TIMESTAMP;

  MTLK_INIT_FINALLY(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))
  MTLK_INIT_RETURN(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx), _mtlk_slow_ctx_cleanup, (nic->slow_ctx, nic))
}

static int
_mtlk_core_sq_init(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);
  /* in case if it is Virtual AP, we get Master SQ from outside, otherwise, create it */
  if (mtlk_vap_is_slave_ap(core->vap_handle)) {
    mtlk_core_t *master_nic = mtlk_core_get_master(core);
    core->sq               = master_nic->sq;
    core->sq_flush_tasklet = master_nic->sq_flush_tasklet;
    return MTLK_ERR_OK;
  }
  return sq_init(core);
}

static void
_mtlk_core_sq_cleanup(mtlk_core_t *core)
{
  /* do nothing in case of slave core */
  if (mtlk_vap_is_slave_ap(core->vap_handle)) {
    core->sq               = NULL;
    core->sq_flush_tasklet = NULL;
  }
  else {
    sq_cleanup(core);
  }
}

static void
_mtlk_core_hw_flctrl_delete(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  if(!mtlk_vap_is_slave_ap(core->vap_handle)) {
    MTLK_ASSERT(core->hw_tx_flctrl != NULL);
    mtlk_flctrl_cleanup(core->hw_tx_flctrl);
    mtlk_osal_mem_free(core->hw_tx_flctrl);
  }
  core->hw_tx_flctrl = NULL;
}

static void  __MTLK_IFUNC
_mtlk_core_hw_flctrl_start_data (mtlk_handle_t ctx)
{
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, ctx);

  mtlk_sq_tx_enable(core->sq);
  mtlk_sq_schedule_flush(core);
}

static void __MTLK_IFUNC
_mtlk_core_hw_flctrl_stop_data (mtlk_handle_t ctx)
{
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, ctx);

  mtlk_sq_tx_disable(core->sq);
}

static int
_mtlk_core_hw_flctrl_create(mtlk_core_t *core)
{
  int result = MTLK_ERR_OK;
  MTLK_ASSERT(NULL != core);

  /* Allocate and init Flow Control object only for Master AP or STA */
  if(!mtlk_vap_is_slave_ap(core->vap_handle)) {
    mtlk_flctrl_api_t hw_flctrl_cfg;

    core->hw_tx_flctrl = mtlk_osal_mem_alloc(sizeof(mtlk_flctrl_t), MTLK_MEM_TAG_FLCTRL);
    if (NULL == core->hw_tx_flctrl) {
      result = MTLK_ERR_NO_MEM;
      goto err_flctrl_alloc;
    }

    hw_flctrl_cfg.ctx        = HANDLE_T(core);
    hw_flctrl_cfg.start_data = _mtlk_core_hw_flctrl_start_data;
    hw_flctrl_cfg.stop_data  = _mtlk_core_hw_flctrl_stop_data;
    result = mtlk_flctrl_init(core->hw_tx_flctrl, &hw_flctrl_cfg);
    if (MTLK_ERR_OK != result) {
      goto err_flctrl_init;
    }
  }
  else {
    /* In case of Slave AP, use Master AP Flow Control Object */
    core->hw_tx_flctrl = mtlk_core_get_master(core)->hw_tx_flctrl;
  }

  return MTLK_ERR_OK;

err_flctrl_init:
  mtlk_osal_mem_free(core->hw_tx_flctrl);
  core->hw_tx_flctrl = NULL;
err_flctrl_alloc:
  return result;
}

/* steps for init and cleanup */
MTLK_INIT_STEPS_LIST_BEGIN(core)
  MTLK_INIT_STEPS_LIST_ENTRY(core, CORE_PDB_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, SLOW_CTX_ALLOC)
  MTLK_INIT_STEPS_LIST_ENTRY(core, SLOW_CTX_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, SQ_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, L2NAT_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, NET_STATE_LOCK_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, QOS_INIT)
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  MTLK_INIT_STEPS_LIST_ENTRY(core, RF_MGMT_CREATE)
#endif
  MTLK_INIT_STEPS_LIST_ENTRY(core, FLCTRL_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(core, TXMM_EEPROM_ASYNC_MSGS_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, COEX_20_40_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, AUX_20_40_MSG_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(core)
MTLK_INIT_STEPS_LIST_END(core);

static void __MTLK_IFUNC
_mtlk_core_cleanup(struct nic* nic)
{
  int i;

  MTLK_ASSERT(NULL != nic);
  MTLK_CLEANUP_BEGIN(core, MTLK_OBJ_PTR(nic))    

    MTLK_CLEANUP_STEP(core, AUX_20_40_MSG_INIT, MTLK_OBJ_PTR(nic),
                      mtlk_txmm_msg_cleanup, (&nic->aux_20_40_msg))

    MTLK_CLEANUP_STEP(core, COEX_20_40_INIT, MTLK_OBJ_PTR(nic),
                      _mtlk_core_delete_20_40, (nic));

    for (i = 0; i < ARRAY_SIZE(nic->txmm_async_eeprom_msgs); i++) {
      MTLK_CLEANUP_STEP_LOOP(core, TXMM_EEPROM_ASYNC_MSGS_INIT, MTLK_OBJ_PTR(nic),
                             mtlk_txmm_msg_cleanup, (&nic->txmm_async_eeprom_msgs[i]));
    }

    MTLK_CLEANUP_STEP(core, FLCTRL_CREATE, MTLK_OBJ_PTR(nic),
                      _mtlk_core_hw_flctrl_delete, (nic))

#ifdef MTCFG_RF_MANAGEMENT_MTLK
    MTLK_CLEANUP_STEP(core, RF_MGMT_CREATE, MTLK_OBJ_PTR(nic),
                      mtlk_rf_mgmt_delete, (nic->rf_mgmt))
#endif

    MTLK_CLEANUP_STEP(core, QOS_INIT, MTLK_OBJ_PTR(nic),
                      mtlk_qos_cleanup, (&nic->qos, nic->vap_handle));

    MTLK_CLEANUP_STEP(core, NET_STATE_LOCK_INIT, MTLK_OBJ_PTR(nic), 
                      mtlk_osal_lock_cleanup, (&nic->net_state_lock));

    MTLK_CLEANUP_STEP(core, L2NAT_INIT, MTLK_OBJ_PTR(nic), 
                      mtlk_l2nat_cleanup, (&nic->l2nat, nic));

    MTLK_CLEANUP_STEP(core, SQ_INIT, MTLK_OBJ_PTR(nic), 
                      _mtlk_core_sq_cleanup, (nic));

    MTLK_CLEANUP_STEP(core, SLOW_CTX_INIT, MTLK_OBJ_PTR(nic), 
                      _mtlk_slow_ctx_cleanup, (nic->slow_ctx, nic));

    MTLK_CLEANUP_STEP(core, SLOW_CTX_ALLOC, MTLK_OBJ_PTR(nic), 
                      kfree_tag, (nic->slow_ctx));

    MTLK_CLEANUP_STEP(core, CORE_PDB_INIT, MTLK_OBJ_PTR(nic),
        mtlk_core_pdb_fast_handles_close, (nic->pdb_hot_path_handles));

  MTLK_CLEANUP_END(core, MTLK_OBJ_PTR(nic));
}

static int __MTLK_IFUNC
_mtlk_core_init(struct nic* nic, mtlk_vap_handle_t vap_handle, mtlk_df_t*   df)
{
  int txem_cnt = 0;

  MTLK_ASSERT(NULL != nic);

  MTLK_INIT_TRY(core, MTLK_OBJ_PTR(nic))
    /* set initial net state */
    nic->net_state   = NET_STATE_HALTED;
    nic->vap_handle  = vap_handle;

    MTLK_INIT_STEP(core, CORE_PDB_INIT, MTLK_OBJ_PTR(nic),
        mtlk_core_pdb_fast_handles_open, (mtlk_vap_get_param_db(nic->vap_handle), nic->pdb_hot_path_handles));

    MTLK_INIT_STEP_EX(core, SLOW_CTX_ALLOC, MTLK_OBJ_PTR(nic), 
                      kmalloc_tag, (sizeof(struct nic_slow_ctx), GFP_KERNEL, MTLK_MEM_TAG_CORE),
                      nic->slow_ctx, NULL != nic->slow_ctx, MTLK_ERR_NO_MEM);

    MTLK_INIT_STEP(core, SLOW_CTX_INIT, MTLK_OBJ_PTR(nic),
                   _mtlk_slow_ctx_init, (nic->slow_ctx, nic));

    MTLK_INIT_STEP(core, SQ_INIT, MTLK_OBJ_PTR(nic),
                   _mtlk_core_sq_init, (nic));

    MTLK_INIT_STEP(core, L2NAT_INIT, MTLK_OBJ_PTR(nic),
                   mtlk_l2nat_init, (&nic->l2nat, nic));

    MTLK_INIT_STEP(core, NET_STATE_LOCK_INIT, MTLK_OBJ_PTR(nic),
                   mtlk_osal_lock_init, (&nic->net_state_lock));

    MTLK_INIT_STEP(core, QOS_INIT, MTLK_OBJ_PTR(nic),
                   mtlk_qos_init, (&nic->qos, nic->vap_handle));

#ifdef MTCFG_RF_MANAGEMENT_MTLK
    MTLK_INIT_STEP_EX(core, RF_MGMT_CREATE, MTLK_OBJ_PTR(nic),
                      mtlk_rf_mgmt_create, (), nic->rf_mgmt,
                      nic->rf_mgmt != NULL, MTLK_ERR_UNKNOWN);
#endif

    MTLK_INIT_STEP(core, FLCTRL_CREATE, MTLK_OBJ_PTR(nic),
                   _mtlk_core_hw_flctrl_create, (nic))

    for (txem_cnt = 0; txem_cnt < ARRAY_SIZE(nic->txmm_async_eeprom_msgs); txem_cnt++) {
      MTLK_INIT_STEP_LOOP(core, TXMM_EEPROM_ASYNC_MSGS_INIT, MTLK_OBJ_PTR(nic),
                          mtlk_txmm_msg_init, (&nic->txmm_async_eeprom_msgs[txem_cnt]));
    }

    MTLK_INIT_STEP_IF(mtlk_vap_is_master(nic->vap_handle), core, COEX_20_40_INIT, MTLK_OBJ_PTR(nic),
                      _mtlk_core_create_20_40, (nic));

    MTLK_INIT_STEP_IF(mtlk_vap_is_master(nic->vap_handle), core, AUX_20_40_MSG_INIT, MTLK_OBJ_PTR(nic),
      mtlk_txmm_msg_init, (&nic->aux_20_40_msg));
    
    nic->is_stopped = TRUE;

  MTLK_INIT_FINALLY(core, MTLK_OBJ_PTR(nic))    
  MTLK_INIT_RETURN(core, MTLK_OBJ_PTR(nic), _mtlk_core_cleanup, (nic))
}

static int _mtlk_core_create_20_40(struct nic* nic)
{
  mtlk_20_40_csm_xfaces_t xfaces;
  struct _mtlk_20_40_coexistence_sm *coex_sm;
  BOOL is_ap;
  uint32 max_number_of_connected_stations;
  int ret_val = MTLK_ERR_NO_MEM;
  
  xfaces.context = (mtlk_handle_t)nic;
  xfaces.vap_handle = nic->vap_handle;
  xfaces.switch_cb_mode_stage1 = &_mtlk_core_switch_cb_mode_stage1_callback;
  xfaces.switch_cb_mode_stage2 = &_mtlk_core_switch_cb_mode_stage2_callback;
  xfaces.send_ce = &_mtlk_core_send_ce_callback;
  xfaces.send_cmf = &_mtlk_core_send_cmf_callback;
  xfaces.scan_async = &_mtlk_core_scan_async_callback;
  xfaces.scan_set_background = &_mtlk_core_scan_set_bg_callback;
  xfaces.register_obss_callback = &_mtlk_core_scan_register_obss_cb_callback;
  xfaces.enumerate_external_intolerance_info = &_mtlk_core_enumerate_external_intolerance_info_callback;
  xfaces.ability_control = &_mtlk_core_ability_control_callback;
  xfaces.get_reg_domain = &_mtlk_core_get_reg_domain_callback;
  xfaces.get_cur_channels = &_mtlk_core_get_cur_channels_callback;

  is_ap = mtlk_vap_is_ap(nic->vap_handle);
  if (is_ap) {
    max_number_of_connected_stations = _mtlk_core_get_max_stas_supported_by_fw(nic);
  } else {
    max_number_of_connected_stations = 0;
  }
  coex_sm = mtlk_20_40_create(&xfaces, is_ap, max_number_of_connected_stations);

  if (coex_sm) {
    mtlk_core_set_coex_sm(nic, coex_sm);
    ret_val = MTLK_ERR_OK;
  }
  
  return ret_val;
}

static void _mtlk_core_delete_20_40(struct nic* nic)
{
  struct _mtlk_20_40_coexistence_sm *coex_sm = mtlk_core_get_coex_sm(nic);
  if (coex_sm)
  {
    mtlk_20_40_delete(coex_sm);
    mtlk_core_set_coex_sm(nic, NULL);
  }
}

static BOOL _mtlk_core_is_20_40_active(struct nic* nic)
{
  BOOL res = (mtlk_20_40_is_feature_enabled(mtlk_core_get_coex_sm(nic)) && (mtlk_core_get_freq_band_cfg(nic) == MTLK_HW_BAND_2_4_GHZ)) ?
             TRUE :
             FALSE;
  return res;
}

mtlk_core_api_t* __MTLK_IFUNC
mtlk_core_api_create (mtlk_vap_handle_t vap_handle, mtlk_df_t*   df)
{
  mtlk_core_api_t *core_api;

  mtlk_print_drv_info();
  
  core_api = mtlk_fast_mem_alloc(MTLK_FM_USER_CORE, sizeof(mtlk_core_api_t));
  if(NULL == core_api) {
    return NULL;
  }

  memset(core_api, 0, sizeof(mtlk_core_api_t));

  /* initialize function table */
  core_api->vft = &core_vft;

  core_api->obj = mtlk_fast_mem_alloc(MTLK_FM_USER_CORE, sizeof(mtlk_core_t));
  if(NULL == core_api->obj) {
    mtlk_fast_mem_free(core_api);
    return NULL;
  }

  memset(core_api->obj, 0, sizeof(mtlk_core_t));

  if (MTLK_ERR_OK != _mtlk_core_init(core_api->obj, vap_handle, df)) {
    mtlk_fast_mem_free(core_api->obj);
    mtlk_fast_mem_free(core_api);
    return NULL;
}

  return core_api;
}

static int
mtlk_core_master_set_default_cfg(struct nic *nic)
{
  uint8 freq_band_cfg = MTLK_HW_BAND_NONE;
  uint8 net_mode;
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(nic->vap_handle));

  if (mtlk_core_is_band_supported(nic, MTLK_HW_BAND_BOTH) == MTLK_ERR_OK) {
    freq_band_cfg = MTLK_HW_BAND_BOTH;
  } else if (mtlk_core_is_band_supported(nic, MTLK_HW_BAND_5_2_GHZ) == MTLK_ERR_OK) {
    freq_band_cfg = MTLK_HW_BAND_5_2_GHZ;
  } else if (mtlk_core_is_band_supported(nic, MTLK_HW_BAND_2_4_GHZ) == MTLK_ERR_OK) {
    freq_band_cfg = MTLK_HW_BAND_2_4_GHZ;
  } else {
    ELOG_D("CID-%04x: None of the bands is supported", mtlk_vap_get_oid(nic->vap_handle));
    return MTLK_ERR_UNKNOWN;
  }

  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CFG, freq_band_cfg);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CUR, freq_band_cfg);

  /* for Master AP and STA calculate network_mode */
  net_mode = get_net_mode(mtlk_core_get_freq_band_cfg(nic), mtlk_core_get_is_ht_cfg(nic));
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_NET_MODE_CUR, net_mode);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_NET_MODE_CFG, net_mode);

  return MTLK_ERR_OK;
}

static int
_mtlk_core_process_antennas_configuration(mtlk_core_t *nic)
{
  int err = MTLK_ERR_OK;
  uint8 tx_val_array[MTLK_NUM_ANTENNAS_BUFSIZE] = {1, 2, 3, 0};
  uint8 rx_val_array[MTLK_NUM_ANTENNAS_BUFSIZE] = {1, 2, 3, 0};
  
  uint8 num_tx_antennas = nic->slow_ctx->tx_limits.num_tx_antennas;
  uint8 num_rx_antennas = nic->slow_ctx->tx_limits.num_rx_antennas;

  /* determine number of TX antennas */
  if (2 == num_tx_antennas)
  {
    tx_val_array[2] = 0;
  }
  else if (3 != num_tx_antennas)
  {
    MTLK_ASSERT(!"Wrong number of TX antennas");
    return MTLK_ERR_UNKNOWN;
  }

  /* determine number of RX antennas */
  if (2 == num_rx_antennas)
  {
    rx_val_array[2] = 0;
  }
  else if (3 != num_rx_antennas)
  {
    MTLK_ASSERT(!"Wrong number of RX antennas");
    return MTLK_ERR_UNKNOWN;
  }

  err = MTLK_CORE_PDB_SET_BINARY(nic, PARAM_DB_CORE_TX_ANTENNAS, tx_val_array, MTLK_NUM_ANTENNAS_BUFSIZE);
  if (MTLK_ERR_OK != err)
  {
    ILOG2_V("Can not save TX antennas configuration in to the PDB");
    return err;
  }
  
  err = MTLK_CORE_PDB_SET_BINARY(nic, PARAM_DB_CORE_RX_ANTENNAS, rx_val_array, MTLK_NUM_ANTENNAS_BUFSIZE);
  if (MTLK_ERR_OK != err)
  {
    ILOG2_V("Can not save RX antennas confgiration in to the PDB");
    return err;
  }
  
  return MTLK_ERR_OK;
}

static void
_mtlk_core_aocs_on_channel_change(mtlk_vap_handle_t vap_handle, int channel)
{
  mtlk_pdb_set_int(mtlk_vap_get_param_db(vap_handle), PARAM_DB_CORE_CHANNEL_CUR, channel);
  ILOG3_D("Channel changed to %d", channel);
}

static void
_mtlk_core_aocs_on_bonding_change(mtlk_vap_handle_t vap_handle, uint8 bonding)
{
  mtlk_core_set_bonding(mtlk_vap_get_core(vap_handle), bonding);
}

static void
_mtlk_core_aocs_on_spectrum_change(mtlk_vap_handle_t vap_handle, int spectrum)
{
    mtlk_core_t *nic = mtlk_vap_get_core(vap_handle);
    mtlk_pdb_t  *pdb = mtlk_vap_get_param_db(vap_handle);

    if (mtlk_pdb_get_int(pdb, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE) != spectrum) {
        ILOG0_DS("CID-%04x: Spectrum changed to %s0MHz", mtlk_vap_get_oid(vap_handle), spectrum == SPECTRUM_20MHZ ? "2" : "4");
    }

    mtlk_pdb_set_int(pdb, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, spectrum);

    mtlk_dot11h_set_spectrum_mode(mtlk_core_get_dfs(nic), spectrum);

    /*
    * 40MHz spectrum should be selected only if HighThroughput is enabled.
    * Refer WLS-1602 for further information.
    */
    MTLK_ASSERT((spectrum == 0) || mtlk_core_get_is_ht_cfg(nic));
}

MTLK_START_STEPS_LIST_BEGIN(core_slow_ctx)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, SERIALIZER_START)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, EEPROM_READ)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, SET_MAC_MAC_ADDR)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, PARSE_EE_DATA)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, CACHE_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, INIT_TX_LIMIT_TABLES)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, PROCESS_ANTENNA_CFG)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, PROCESS_COC)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, AOCS_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, SCAN_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, DOT11H_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, ADDBA_AGGR_LIM_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, ADDBA_REORD_LIM_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, ADDBA_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, SET_DEFAULT_BAND)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, WATCHDOG_TIMER_START)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, SERIALIZER_ACTIVATE)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, CORE_STAT_REQ_HANDLER)
MTLK_START_INNER_STEPS_BEGIN(core_slow_ctx)
MTLK_START_STEPS_LIST_END(core_slow_ctx);

static void
_mtlk_core_get_wlan_stats(mtlk_core_t* core, mtlk_wssa_drv_wlan_stats_t* stats)
{
  stats->RxPacketsDiscardedDrvTooOld                        = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD);
  stats->RxPacketsDiscardedDrvDuplicate                     = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE);
  stats->TxPacketsDiscardedDrvNoPeers                       = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_NO_PEERS);
  stats->TxPacketsDiscardedDrvACM                           = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_ACM);
  stats->TxPacketsDiscardedEapolCloned                      = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_EAPOL_CLONED);
  stats->TxPacketsDiscardedDrvUnknownDestinationDirected    = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_DIRECTED);
  stats->TxPacketsDiscardedDrvUnknownDestinationMcast       = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_MCAST);
  stats->TxPacketsDiscardedDrvNoResources                   = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES);
  stats->TxPacketsDiscardedDrvSQOverflow                    = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_SQ_OVERFLOW);
  stats->TxPacketsDiscardedDrvEAPOLFilter                   = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_EAPOL_FILTER);
  stats->TxPacketsDiscardedDrvDropAllFilter                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DROP_ALL_FILTER);
  stats->TxPacketsDiscardedDrvTXQueueOverflow               = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW);
  stats->RxPacketsSucceeded                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_PACKETS_RECEIVED);
  stats->RxBytesSucceeded                                   = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BYTES_RECEIVED);
  stats->TxPacketsSucceeded                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_PACKETS_SENT);
  stats->TxBytesSucceeded                                   = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BYTES_SENT);
  stats->PairwiseMICFailurePackets                          = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_PAIRWISE_MIC_FAILURE_PACKETS);
  stats->GroupMICFailurePackets                             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_GROUP_MIC_FAILURE_PACKETS);
  stats->UnicastReplayedPackets                             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_UNICAST_REPLAYED_PACKETS);
  stats->MulticastReplayedPackets                           = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MULTICAST_REPLAYED_PACKETS);
  stats->FwdRxPackets                                       = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_FWD_RX_PACKETS);
  stats->FwdRxBytes                                         = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_FWD_RX_BYTES);
  stats->UnicastPacketsSent                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_UNICAST_PACKETS_SENT);
  stats->UnicastPacketsReceived                             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_UNICAST_PACKETS_RECEIVED);
  stats->MulticastPacketsSent                               = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MULTICAST_PACKETS_SENT);
  stats->MulticastPacketsReceived                           = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MULTICAST_PACKETS_RECEIVED);
  stats->BroadcastPacketsSent                               = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BROADCAST_PACKETS_SENT);
  stats->BroadcastPacketsReceived                           = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BROADCAST_PACKETS_RECEIVED);
  stats->MulticastBytesSent                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MULTICAST_BYTES_SENT);
  stats->MulticastBytesReceived                             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MULTICAST_BYTES_RECEIVED);
  stats->BroadcastBytesSent                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BROADCAST_BYTES_SENT);
  stats->BroadcastBytesReceived                             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BROADCAST_BYTES_RECEIVED);
  stats->DATFramesReceived                                  = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_DAT_FRAMES_RECEIVED);
  stats->CTLFramesReceived                                  = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_CTL_FRAMES_RECEIVED);
  stats->MANFramesReceived                                  = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MAN_FRAMES_RECEIVED);

  stats->CoexElReceived                                     = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_COEX_EL_RECEIVED);
  stats->ScanExRequested                                    = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_REQUESTED);
  stats->ScanExGranted                                      = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_GRANTED);
  stats->ScanExGrantCancelled                               = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED);
  stats->SwitchChannel20To40                                = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_CHANNEL_SWITCH_20_TO_40);
  stats->SwitchChannel40To20                                = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_CHANNEL_SWITCH_40_TO_20);
  stats->SwitchChannel40To40                                = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_CHANNEL_SWITCH_40_TO_40);

  stats->AggrActive                                         = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_AGGR_ACTIVE);
  stats->ReordActive                                        = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_REORD_ACTIVE);
}

static void __MTLK_IFUNC
_mtlk_core_stat_handle_request(mtlk_irbd_t       *irbd,
                               mtlk_handle_t      context,
                               const mtlk_guid_t *evt,
                               void              *buffer,
                               uint32            *size)
{
  struct nic_slow_ctx  *slow_ctx = HANDLE_T_PTR(struct nic_slow_ctx, context);
  mtlk_wssa_info_hdr_t *hdr = (mtlk_wssa_info_hdr_t *) buffer;

  MTLK_UNREFERENCED_PARAM(evt);

  if(sizeof(mtlk_wssa_info_hdr_t) > *size)
    return;

  if(MTIDL_SRC_DRV == hdr->info_source)
  {
    switch(hdr->info_id)
    {
    case MTLK_WSSA_DRV_STATUS_WLAN:
      {
        if(sizeof(mtlk_wssa_drv_wlan_stats_t) + sizeof(mtlk_wssa_info_hdr_t) > *size)
        {
          hdr->processing_result = MTLK_ERR_BUF_TOO_SMALL;
        }
        else
        {
          _mtlk_core_get_wlan_stats(slow_ctx->nic, (mtlk_wssa_drv_wlan_stats_t*) &hdr[1]);
          hdr->processing_result = MTLK_ERR_OK;
          *size = sizeof(mtlk_wssa_drv_wlan_stats_t) + sizeof(mtlk_wssa_info_hdr_t);
        }
      }
      break;
    default:
      {
        hdr->processing_result = MTLK_ERR_NO_ENTRY;
        *size = sizeof(mtlk_wssa_info_hdr_t);
      }
    }
  }
  else
  {
    hdr->processing_result = MTLK_ERR_NO_ENTRY;
    *size = sizeof(mtlk_wssa_info_hdr_t);
  }
}

static void __MTLK_IFUNC
_mtlk_slow_ctx_stop(struct nic_slow_ctx *slow_ctx, struct nic* nic)
{
  MTLK_ASSERT(NULL != slow_ctx);
  MTLK_ASSERT(NULL != nic);

  MTLK_STOP_BEGIN(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))
    MTLK_STOP_STEP(core_slow_ctx, CORE_STAT_REQ_HANDLER, MTLK_OBJ_PTR(slow_ctx), 
                   mtlk_wssd_unregister_request_handler, (mtlk_vap_get_irbd(nic->vap_handle), slow_ctx->stat_irb_handle));

    MTLK_STOP_STEP(core_slow_ctx, SERIALIZER_ACTIVATE, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_serializer_stop, (&slow_ctx->serializer))

    MTLK_STOP_STEP(core_slow_ctx, WATCHDOG_TIMER_START, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_osal_timer_cancel_sync, (&slow_ctx->mac_watchdog_timer))

    MTLK_STOP_STEP(core_slow_ctx, SET_DEFAULT_BAND, MTLK_OBJ_PTR(slow_ctx),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core_slow_ctx, ADDBA_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_addba_cleanup, (&slow_ctx->addba))

    MTLK_STOP_STEP(core_slow_ctx, ADDBA_REORD_LIM_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_reflim_cleanup, (&slow_ctx->addba_lim_reord))

    MTLK_STOP_STEP(core_slow_ctx, ADDBA_AGGR_LIM_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_reflim_cleanup, (&slow_ctx->addba_lim_aggr))

    MTLK_STOP_STEP(core_slow_ctx, DOT11H_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_dot11h_cleanup, (mtlk_core_get_dfs(nic)))

    MTLK_STOP_STEP(core_slow_ctx, SCAN_INIT, MTLK_OBJ_PTR(slow_ctx),
                    mtlk_scan_cleanup, (&slow_ctx->scan))

    MTLK_STOP_STEP(core_slow_ctx, AOCS_INIT, MTLK_OBJ_PTR(slow_ctx),
                    mtlk_aocs_delete, (slow_ctx->aocs))

    MTLK_STOP_STEP(core_slow_ctx, PROCESS_COC, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_coc_delete, (slow_ctx->coc_mngmt))

    MTLK_STOP_STEP(core_slow_ctx, PROCESS_ANTENNA_CFG, MTLK_OBJ_PTR(slow_ctx),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core_slow_ctx, INIT_TX_LIMIT_TABLES, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_cleanup_tx_limit_tables, (&slow_ctx->tx_limits))

    MTLK_STOP_STEP(core_slow_ctx, CACHE_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_cache_cleanup, (&slow_ctx->cache))

    MTLK_STOP_STEP(core_slow_ctx, PARSE_EE_DATA, MTLK_OBJ_PTR(slow_ctx),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core_slow_ctx, SET_MAC_MAC_ADDR, MTLK_OBJ_PTR(slow_ctx),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core_slow_ctx, EEPROM_READ, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_clean_eeprom_data, (mtlk_core_get_eeprom(nic), mtlk_vap_get_abmgr(nic->vap_handle)))

    MTLK_STOP_STEP(core_slow_ctx, SERIALIZER_START, MTLK_OBJ_PTR(slow_ctx),
                   MTLK_NOACTION, ())
  MTLK_STOP_END(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))
}

static int __MTLK_IFUNC
_mtlk_slow_ctx_start(struct nic_slow_ctx *slow_ctx, struct nic* nic)
{
  int cache_param;
  struct mtlk_scan_config scan_cfg;
  mtlk_aocs_wrap_api_t aocs_api;
  mtlk_aocs_init_t aocs_ini_data;
  mtlk_dot11h_wrap_api_t dot11h_api;
  mtlk_coc_cfg_t  coc_cfg;
  mtlk_txmm_t *txmm = mtlk_vap_get_txmm(nic->vap_handle);
  mtlk_reflim_t *addba_lim_aggr  = NULL;
  mtlk_reflim_t *addba_lim_reord = NULL;

  MTLK_ASSERT(NULL != slow_ctx);
  MTLK_ASSERT(NULL != nic);

  MTLK_START_TRY(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))
    MTLK_START_STEP(core_slow_ctx, SERIALIZER_START, MTLK_OBJ_PTR(slow_ctx),
                    mtlk_serializer_start, (&slow_ctx->serializer))

    MTLK_START_STEP_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, EEPROM_READ, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_eeprom_read_and_parse, (mtlk_core_get_eeprom(nic), txmm, mtlk_vap_get_abmgr(nic->vap_handle)))

    MTLK_START_STEP_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, SET_MAC_MAC_ADDR, MTLK_OBJ_PTR(slow_ctx),
                       _mtlk_core_set_mac_addr, (nic, mtlk_eeprom_get_nic_mac_addr(mtlk_core_get_eeprom(nic))) )

    MTLK_START_STEP_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, PARSE_EE_DATA, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_eeprom_check_ee_data, (mtlk_core_get_eeprom(nic), txmm, mtlk_vap_is_ap(nic->vap_handle)))
    
    _mtlk_core_country_code_set_default(nic);

    if (mtlk_vap_is_ap(nic->vap_handle)) {
      cache_param = 0;
    } else {
      cache_param = SCAN_CACHE_AGEING;
    }

    MTLK_START_STEP(core_slow_ctx, CACHE_INIT, MTLK_OBJ_PTR(slow_ctx),
                    mtlk_cache_init, (&slow_ctx->cache, cache_param))
    
    MTLK_START_STEP_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, INIT_TX_LIMIT_TABLES, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_init_tx_limit_tables,
                       (&slow_ctx->tx_limits,
                       MAC_TO_HOST16( mtlk_eeprom_get_vendor_id(mtlk_core_get_eeprom(nic)) ),
                       MAC_TO_HOST16( mtlk_eeprom_get_device_id(mtlk_core_get_eeprom(nic)) ),
                       mtlk_eeprom_get_nic_type(mtlk_core_get_eeprom(nic)),
                       mtlk_eeprom_get_nic_revision(mtlk_core_get_eeprom(nic)) ) )

    MTLK_START_STEP_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, PROCESS_ANTENNA_CFG, MTLK_OBJ_PTR(slow_ctx),
                       _mtlk_core_process_antennas_configuration, (nic));

    coc_cfg.hw_antenna_cfg.num_tx_antennas = nic->slow_ctx->tx_limits.num_tx_antennas;
    coc_cfg.hw_antenna_cfg.num_rx_antennas = nic->slow_ctx->tx_limits.num_rx_antennas;
    coc_cfg.txmm = txmm;
    coc_cfg.core = nic;

    MTLK_START_STEP_EX_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, PROCESS_COC, MTLK_OBJ_PTR(slow_ctx),
                          mtlk_coc_create, (&coc_cfg),
                          slow_ctx->coc_mngmt, slow_ctx->coc_mngmt != NULL, MTLK_ERR_NO_MEM);

    aocs_api.on_channel_change = _mtlk_core_aocs_on_channel_change;
    aocs_api.on_bonding_change = _mtlk_core_aocs_on_bonding_change;
    aocs_api.on_spectrum_change = _mtlk_core_aocs_on_spectrum_change;

    aocs_ini_data.api = &aocs_api;
    aocs_ini_data.scan_data = &slow_ctx->scan;
    aocs_ini_data.cache = &slow_ctx->cache;
    aocs_ini_data.dot11h = mtlk_core_get_dfs(nic);
    aocs_ini_data.txmm = txmm;
    aocs_ini_data.disable_sm_channels = mtlk_eeprom_get_disable_sm_channels(mtlk_core_get_eeprom(nic));

    slow_ctx->aocs = NULL;

    MTLK_START_STEP_EX_IF(mtlk_vap_is_master_ap(nic->vap_handle), core_slow_ctx, AOCS_INIT, MTLK_OBJ_PTR(slow_ctx),
                          mtlk_aocs_create, (&aocs_ini_data, nic->vap_handle),
                          slow_ctx->aocs, slow_ctx->aocs != NULL, MTLK_ERR_NO_MEM)

    scan_cfg.txmm = txmm;
    scan_cfg.aocs = slow_ctx->aocs;
    scan_cfg.hw_tx_flctrl = nic->hw_tx_flctrl;
    scan_cfg.bss_cache = &slow_ctx->cache;

    MTLK_START_STEP_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, SCAN_INIT, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_scan_init, (&slow_ctx->scan, scan_cfg, nic->vap_handle))
    
    dot11h_api.aocs         = slow_ctx->aocs;
    dot11h_api.txmm         = txmm;
    dot11h_api.hw_tx_flctrl = nic->hw_tx_flctrl;

    MTLK_START_STEP_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, DOT11H_INIT, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_dot11h_init, (mtlk_core_get_dfs(nic), NULL, &dot11h_api, nic->vap_handle))

    MTLK_START_STEP_VOID_IF(mtlk_vap_is_master(nic->vap_handle), core_slow_ctx, ADDBA_AGGR_LIM_INIT, MTLK_OBJ_PTR(slow_ctx),
                            mtlk_reflim_init, (&slow_ctx->addba_lim_aggr, MTLK_ADDBA_DEF_MAX_AGGR_SUPPORTED))

    MTLK_START_STEP_VOID_IF(mtlk_vap_is_master(nic->vap_handle), core_slow_ctx, ADDBA_REORD_LIM_INIT, MTLK_OBJ_PTR(slow_ctx),
                            mtlk_reflim_init, (&slow_ctx->addba_lim_reord, MTLK_ADDBA_DEF_MAX_REORD_SUPPORTED))

    if (mtlk_vap_is_master(nic->vap_handle)) {
      addba_lim_aggr  = &slow_ctx->addba_lim_aggr;
      addba_lim_reord = &slow_ctx->addba_lim_reord;
    }
    else {
      /* Limits are shared for all the VAPs on same HW */
      addba_lim_aggr  = &mtlk_core_get_master(nic)->slow_ctx->addba_lim_aggr;
      addba_lim_reord = &mtlk_core_get_master(nic)->slow_ctx->addba_lim_reord;
    }

    MTLK_START_STEP(core_slow_ctx, ADDBA_INIT, MTLK_OBJ_PTR(slow_ctx),
                    mtlk_addba_init, (&slow_ctx->addba, txmm, addba_lim_aggr, addba_lim_reord, &slow_ctx->cfg.addba, nic->vap_handle))
    
    MTLK_START_STEP_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, SET_DEFAULT_BAND, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_core_master_set_default_cfg, (nic))
    
    MTLK_START_STEP_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, WATCHDOG_TIMER_START, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_osal_timer_set,
                       (&slow_ctx->mac_watchdog_timer,
                       MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_PERIOD_MS)))

    MTLK_START_STEP_VOID(core_slow_ctx, SERIALIZER_ACTIVATE, MTLK_OBJ_PTR(slow_ctx),
                         MTLK_NOACTION, ())

    MTLK_START_STEP_EX(core_slow_ctx, CORE_STAT_REQ_HANDLER, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_wssd_register_request_handler, (mtlk_vap_get_irbd(nic->vap_handle),
                                                            _mtlk_core_stat_handle_request, HANDLE_T(slow_ctx)),
                       slow_ctx->stat_irb_handle, slow_ctx->stat_irb_handle != NULL, MTLK_ERR_UNKNOWN);

  MTLK_START_FINALLY(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))
  MTLK_START_RETURN(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx), _mtlk_slow_ctx_stop, (slow_ctx, nic))
}

MTLK_START_STEPS_LIST_BEGIN(core)
  MTLK_START_STEPS_LIST_ENTRY(core, WSS_CREATE)
  MTLK_START_STEPS_LIST_ENTRY(core, WSS_HCTNRs)
  MTLK_START_STEPS_LIST_ENTRY(core, SET_NET_STATE_IDLE)
  MTLK_START_STEPS_LIST_ENTRY(core, FLCTRL_ID_REGISTER)
  MTLK_START_STEPS_LIST_ENTRY(core, SLOW_CTX_START)
  MTLK_START_STEPS_LIST_ENTRY(core, DF_USER_SET_MAC_ADDR)
  MTLK_START_STEPS_LIST_ENTRY(core, RESET_STATS)
  MTLK_START_STEPS_LIST_ENTRY(core, SQ_START)
  MTLK_START_STEPS_LIST_ENTRY(core, MC_INIT)
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  MTLK_START_STEPS_LIST_ENTRY(core, RF_MGMT_START)
#endif
  MTLK_START_STEPS_LIST_ENTRY(core, ADD_VAP)
  MTLK_START_STEPS_LIST_ENTRY(core, ABILITIES_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core, SET_NET_STATE_READY)
MTLK_START_INNER_STEPS_BEGIN(core)
MTLK_START_STEPS_LIST_END(core);

static void
_mtlk_core_stop (mtlk_vap_handle_t vap_handle)
{
  mtlk_core_t *nic = mtlk_vap_get_core (vap_handle);
  mtlk_hw_state_e hw_state = mtlk_core_get_hw_state(nic);
  int i;

  ILOG0_D("CID-%04x: stop", mtlk_vap_get_oid(vap_handle));

  /*send RMMOD event to application*/
  if ((hw_state != MTLK_HW_STATE_EXCEPTION) && 
      (hw_state != MTLK_HW_STATE_APPFATAL)) {
    ILOG4_V("RMMOD send event");
    mtlk_df_ui_notify_notify_rmmod(1);
  }

  flush_scheduled_work();

  MTLK_STOP_BEGIN(core, MTLK_OBJ_PTR(nic))
    MTLK_STOP_STEP(core, SET_NET_STATE_READY, MTLK_OBJ_PTR(nic),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core, ABILITIES_INIT, MTLK_OBJ_PTR(nic),
                   mtlk_core_abilities_unregister, (nic))

    MTLK_STOP_STEP(core, ADD_VAP, MTLK_OBJ_PTR(nic),
                   mtlk_mbss_send_vap_delete, (nic))

#ifdef MTCFG_RF_MANAGEMENT_MTLK
    MTLK_STOP_STEP(core, RF_MGMT_START, MTLK_OBJ_PTR(nic),
                   mtlk_rf_mgmt_stop, (nic->rf_mgmt))
#endif

    MTLK_STOP_STEP(core, MC_INIT, MTLK_OBJ_PTR(nic),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core, SQ_START, MTLK_OBJ_PTR(nic),
                   mtlk_sq_stop, (nic->sq));

    MTLK_STOP_STEP(core, RESET_STATS, MTLK_OBJ_PTR(nic),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core, DF_USER_SET_MAC_ADDR, MTLK_OBJ_PTR(nic),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core, SLOW_CTX_START, MTLK_OBJ_PTR(nic),
                   _mtlk_slow_ctx_stop, (nic->slow_ctx, nic))

    MTLK_STOP_STEP(core, FLCTRL_ID_REGISTER, MTLK_OBJ_PTR(nic),
                   mtlk_flctrl_unregister, (nic->hw_tx_flctrl, nic->flctrl_id))

    nic->flctrl_id = 0;

    MTLK_STOP_STEP(core, SET_NET_STATE_IDLE, MTLK_OBJ_PTR(nic),
                   MTLK_NOACTION, ())
    MTLK_STOP_STEP(core, WSS_HCTNRs, MTLK_OBJ_PTR(nic),
                   mtlk_wss_cntrs_close, (nic->wss, nic->wss_hcntrs, ARRAY_SIZE(nic->wss_hcntrs)))
    MTLK_STOP_STEP(core, WSS_CREATE, MTLK_OBJ_PTR(nic),
                   mtlk_wss_delete, (nic->wss));
  MTLK_STOP_END(core, MTLK_OBJ_PTR(nic))

  for (i = 0; i < ARRAY_SIZE(nic->txmm_async_eeprom_msgs); i++) {
    mtlk_txmm_msg_cancel(&nic->txmm_async_eeprom_msgs[i]);
  }
}

static int
_mtlk_core_start (mtlk_vap_handle_t vap_handle)
{
  mtlk_core_t       *nic = mtlk_vap_get_core (vap_handle);
  uint8             mac_addr[ETH_ALEN];
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  mtlk_rf_mgmt_cfg_t rf_mgmt_cfg = {0};
#endif

  ILOG0_DS("CID-%04x: %s", mtlk_vap_get_oid(vap_handle), mtlk_version_string);

  MTLK_ASSERT(ARRAY_SIZE(nic->wss_hcntrs) == MTLK_CORE_CNT_LAST);
  MTLK_ASSERT(ARRAY_SIZE(_mtlk_core_wss_id_map) == MTLK_CORE_CNT_LAST);

  MTLK_START_TRY(core, MTLK_OBJ_PTR(nic))
    MTLK_START_STEP_EX(core, WSS_CREATE, MTLK_OBJ_PTR(nic),
                       mtlk_wss_create, (mtlk_vap_manager_get_hw_wss(mtlk_vap_get_manager(vap_handle)), _mtlk_core_wss_id_map, ARRAY_SIZE(_mtlk_core_wss_id_map)),
                       nic->wss, nic->wss != NULL, MTLK_ERR_NO_MEM);

    MTLK_START_STEP(core, WSS_HCTNRs, MTLK_OBJ_PTR(nic),
                    mtlk_wss_cntrs_open, (nic->wss, _mtlk_core_wss_id_map, nic->wss_hcntrs, MTLK_CORE_CNT_LAST));
    MTLK_START_STEP(core, SET_NET_STATE_IDLE, MTLK_OBJ_PTR(nic),
                    mtlk_core_set_net_state, (nic, NET_STATE_IDLE))

    nic->flctrl_id = 0;

    MTLK_START_STEP(core, FLCTRL_ID_REGISTER, MTLK_OBJ_PTR(nic),
                    mtlk_flctrl_register, (nic->hw_tx_flctrl, &nic->flctrl_id))

    MTLK_START_STEP(core, SLOW_CTX_START, MTLK_OBJ_PTR(nic),
                    _mtlk_slow_ctx_start, (nic->slow_ctx, nic))

    mtlk_pdb_get_mac(
        mtlk_vap_get_param_db(vap_handle), PARAM_DB_CORE_MAC_ADDR, mac_addr);

    MTLK_START_STEP_VOID(core, DF_USER_SET_MAC_ADDR, MTLK_OBJ_PTR(nic),
                         mtlk_df_ui_set_mac_addr, (mtlk_vap_get_df(vap_handle), mac_addr))

    MTLK_START_STEP_VOID(core, RESET_STATS, MTLK_OBJ_PTR(nic),
                         _mtlk_core_reset_stats_internal, (nic))

    /* The Master DF is always used here */
    MTLK_START_STEP_IF(!mtlk_vap_is_slave_ap(vap_handle), core, SQ_START, MTLK_OBJ_PTR(nic),
                       mtlk_sq_start, (nic->sq, mtlk_vap_get_df(vap_handle)) );

    MTLK_START_STEP_VOID(core, MC_INIT, MTLK_OBJ_PTR(nic),
                         mtlk_mc_init, (nic))

#ifdef MTCFG_RF_MANAGEMENT_MTLK
    rf_mgmt_cfg.txmm  = mtlk_vap_get_txmm(vap_handle);
    rf_mgmt_cfg.stadb = &nic->slow_ctx->stadb;
    rf_mgmt_cfg.irbd  = mtlk_vap_get_irbd(vap_handle);
    rf_mgmt_cfg.context = HANDLE_T(nic);
    rf_mgmt_cfg.device_is_busy = mtlk_core_is_device_busy;

    MTLK_START_STEP(core, RF_MGMT_START, MTLK_OBJ_PTR(nic),
                    mtlk_rf_mgmt_start, (nic->rf_mgmt, &rf_mgmt_cfg));
#endif

    MTLK_START_STEP_IF(mtlk_vap_is_ap(vap_handle),
                       core, ADD_VAP, MTLK_OBJ_PTR(nic),
                       mtlk_mbss_send_vap_add, (nic))

    MTLK_START_STEP(core, ABILITIES_INIT, MTLK_OBJ_PTR(nic),
                    mtlk_core_abilities_register, (nic))

    MTLK_START_STEP(core, SET_NET_STATE_READY, MTLK_OBJ_PTR(nic),
                    mtlk_core_set_net_state, (nic, NET_STATE_READY))

  MTLK_START_FINALLY(core, MTLK_OBJ_PTR(nic))
  MTLK_START_RETURN(core, MTLK_OBJ_PTR(nic), _mtlk_core_stop, (vap_handle))
}

static int
_mtlk_core_release_tx_data (mtlk_vap_handle_t vap_handle, const mtlk_core_release_tx_data_t *data)
{
  int res = MTLK_ERR_UNKNOWN;  
  mtlk_core_t *nic = mtlk_vap_get_core (vap_handle);
  mtlk_nbuf_t *nbuf = data->nbuf;
  unsigned short qos = 0;
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  sta_entry *sta = NULL; /* NOTE: nbuf is referencing STA, so it is safe to use this STA 
                          * while nbuf isn't released. */
  mtlk_sq_peer_ctx_t *sq_ppeer = NULL;

  /* All the packets on AP and non BC packets on STA */
  if (mtlk_vap_is_ap(nic->vap_handle) ||
     mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_UNICAST | MTLK_NBUFF_RMCAST)) {
    sta = mtlk_nbuf_priv_get_dst_sta(nbuf_priv);
  }

#if defined(MTCFG_PER_PACKET_STATS) && defined (MTCFG_TSF_TIMER_ACCESS_ENABLED)
  mtlk_nbuf_priv_stats_set(nbuf_priv, MTLK_NBUF_STATS_TS_FW_OUT, mtlk_hw_get_timestamp(vap_handle));
#endif

  // check if NULL packet confirmed
  if (data->size == 0) {
    ILOG9_V("Confirmation for NULL nbuf");
    goto FINISH;
  }

  qos = mtlk_qos_get_ac_by_tid(data->access_category);
  
  if ((qos != (uint16)-1) && (nic->pstats.ac_used_counter[qos] > 0))
    --nic->pstats.ac_used_counter[qos];

  res = MTLK_ERR_OK;

FINISH:
  if (data->resources_free) {
    if (__UNLIKELY(!mtlk_flctrl_is_data_flowing(nic->hw_tx_flctrl))) {
      ILOG2_V("mtlk_flctrl_wake on OS TX queue wake");
      mtlk_flctrl_start_data(nic->hw_tx_flctrl, nic->flctrl_id);
    } else {
      mtlk_sq_schedule_flush(nic);
    }
  }

  // If confirmed (or failed) unicast packet to known STA
  if (NULL != sta ) {
    /* this is unicast or reliable multicast being transmitted */
    sq_ppeer = &sta->sq_peer_ctx;

    if (__LIKELY(data->status == UMI_OK)) {
      /* Update STA's timestamp on successful (confirmed by ACK) TX */
      mtlk_sta_on_packet_sent(sta, nbuf, data->nof_retries);

#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
      if (mtlk_vap_is_ap(vap_handle) && (qos != (uint16)-1)) {
        mtlk_aocs_on_tx_msdu_returned(mtlk_core_get_master(nic)->slow_ctx->aocs, qos);
      }
#endif
    } else {
      mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_FW);
    }
  } else {
    MTLK_ASSERT(FALSE == mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_UNICAST | MTLK_NBUFF_RMCAST));
    /* this should be broadcast or non-reliable multicast packet */
    if (__LIKELY(data->status == UMI_OK)) {
      if(mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_MULTICAST)) {
        mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_MULTICAST_PACKETS_SENT);
        mtlk_core_add_cnt(nic, MTLK_CORE_CNT_MULTICAST_BYTES_SENT, mtlk_df_nbuf_get_data_length(nbuf));
      }
      else if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_BROADCAST)) {
        mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_BROADCAST_PACKETS_SENT);
        mtlk_core_add_cnt(nic, MTLK_CORE_CNT_BROADCAST_BYTES_SENT, mtlk_df_nbuf_get_data_length(nbuf));
      }

      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_PACKETS_SENT);
      mtlk_core_add_cnt(nic, MTLK_CORE_CNT_BYTES_SENT, mtlk_df_nbuf_get_data_length(nbuf));
      nic->pstats.tx_bcast_nrmcast++;
    } else {
      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_FW);
    }
  }

  if (data->size != 0) {
    mtlk_sq_on_tx_cfm(nic->sq, sq_ppeer);
  }

#if defined(MTCFG_PRINT_PER_PACKET_STATS)
  mtlk_nbuf_priv_stats_dump(nbuf_priv);
#endif

  /* Release net buffer
   * WARNING: we can't do it before since we use STA referenced by this packet on FINISH.
   */
  mtlk_df_nbuf_free(_mtlk_core_get_master_df(nic), nbuf);

  /* update used Tx MSDUs counter */
#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
  if (qos != (uint16)-1) {
    if(mtlk_vap_is_ap(vap_handle)) {
      mtlk_aocs_msdu_tx_dec_nof_used(mtlk_core_get_master(nic)->slow_ctx->aocs, qos);
    }
  }
#endif

  return res;
}

static int
_mtlk_core_handle_rx_data (mtlk_vap_handle_t vap_handle, mtlk_core_handle_rx_data_t *data)
{
  mtlk_nbuf_t *nbuf = data->nbuf;
  ASSERT(nbuf != 0);

  mtlk_df_nbuf_put(nbuf, data->offset);
  mtlk_df_nbuf_pull(nbuf, data->offset);

  return handle_rx_ind(mtlk_vap_get_core (vap_handle), nbuf, (uint16)data->size, data->info);
}

static int __MTLK_IFUNC
_handle_pm_update_event(mtlk_handle_t object, const void *data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  const UMI_PM_UPDATE *pm_update = (const UMI_PM_UPDATE *)data;
  sta_entry *sta;

  MTLK_ASSERT(sizeof(UMI_PM_UPDATE) == data_size);

  ILOG2_DY("Power management mode changed to %d for %Y",
        pm_update->newPowerMode, pm_update->sStationID.au8Addr);

  sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, pm_update->sStationID.au8Addr);
  if (sta == NULL) {
    ILOG2_Y("PM update event received from STA %Y which is not known",
          pm_update->sStationID.au8Addr);
    return MTLK_ERR_OK;
  }

  mtlk_sta_set_pm_enabled(sta, pm_update->newPowerMode == UMI_STATION_IN_PS);
  mtlk_sta_decref(sta); /* De-reference of find */

  if (pm_update->newPowerMode == UMI_STATION_ACTIVE)
    mtlk_sq_schedule_flush(nic);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_logger_init_failed_event(mtlk_handle_t object, const void *data, uint32 data_size)
{
  MTLK_UNREFERENCED_PARAM(object);
  MTLK_UNREFERENCED_PARAM(data);
  MTLK_UNREFERENCED_PARAM(data_size);

  MTLK_ASSERT(0 == data_size);

  ELOG_V("Firmware log will be unavailable due to firmware logger init failure");

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_aocs_tcp_event(mtlk_handle_t core_object, const void *data, uint32 data_size)
{
  mtlk_core_t* core = HANDLE_T_PTR(mtlk_core_t, core_object);

  MTLK_UNREFERENCED_PARAM(data_size);

  if (mtlk_vap_is_master_ap(core->vap_handle)) {
    mtlk_aocs_indicate_event(core->slow_ctx->aocs,
                             MTLK_AOCS_EVENT_TCP_IND, (void*) data, data_size);
  }

  return MTLK_ERR_OK;
}

/* TODO: Looks like CLI won't work in case of silent build */
#ifndef MTCFG_SILENT
static const char
mtlk_mac_event_prefix[] = "MAC event"; // don't change this - used from CLI
#endif

static int __MTLK_IFUNC
_mtlk_process_mac_hang(mtlk_core_t* nic, mtlk_hw_state_e hw_state, uint32 fw_cpu)
{
  mtlk_set_hw_state(nic, hw_state);
  mtlk_core_set_net_state(nic, NET_STATE_HALTED);
  nic->slow_ctx->mac_stuck_detected_by_sw = 0;
  WLOG_DD("CID-%04x: MAC Hang detected, event = %d", mtlk_vap_get_oid(nic->vap_handle), hw_state);
  mtlk_df_ui_notify_notify_fw_hang(mtlk_vap_get_df(nic->vap_handle), fw_cpu, hw_state);

  return MTLK_ERR_OK;
}

static int
_mtlk_process_fw_log_buffers_on_exception(mtlk_core_t *nic, LOG_BUF_DESC_ON_FATAL* buffers)
{
#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
  int i;
  for(i = 0; (i < LOGGER_NUM_OF_BUFFERS); i++)
  {
    if ((0 != buffers[i].pBufStart) && (0 != buffers[i].bufLength)) {
      mtlk_core_fw_log_buffer_t buffer;
      buffer.addr = MAC_TO_HOST32(buffers[i].pBufStart);
      buffer.length = MAC_TO_HOST32(buffers[i].bufLength);
      mtlk_vap_get_hw_vft(nic->vap_handle)->set_prop(nic->vap_handle, MTLK_HW_FW_LOG_BUFFER, &buffer, sizeof(buffer));
    }
  }
#else
  MTLK_UNREFERENCED_PARAM(nic);
  MTLK_UNREFERENCED_PARAM(buffers);
#endif
  return MTLK_ERR_OK;
}

static int
_mtlk_process_mac_fatal_log (mtlk_core_t *nic, APP_FATAL *app_fatal)
{
#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
  mtlk_log_event_t log_event;

  log_event.timestamp = MAC_TO_HOST32(app_fatal->uTimeStamp);
  log_event.info = LOG_MAKE_INFO(0,                                  /* version */
                                 MAC_TO_HOST32(app_fatal->OriginId), /* firmware OID */
                                 MAC_TO_HOST32(app_fatal->GroupId)); /* firmware GID */
  log_event.info_ex = LOG_MAKE_INFO_EX(MAC_TO_HOST32(app_fatal->FileId),             /* firmware FID */
                                       MAC_TO_HOST32(app_fatal->uCauseRegOrLineNum), /* firmware LID */
                                       0,                                            /* data size */
                                       MAC_TO_HOST32(app_fatal->FWinterface));       /* firmware wlanif */

  mtlk_vap_get_hw_vft(nic->vap_handle)->set_prop(nic->vap_handle, MTLK_HW_LOG, 
                                                 &log_event, sizeof(log_event) + 0 /* data size */);
#else
  MTLK_UNREFERENCED_PARAM(nic);
  MTLK_UNREFERENCED_PARAM(app_fatal);
#endif
  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_mac_exception(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  APP_FATAL *app_fatal = (APP_FATAL*)data;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);

  MTLK_ASSERT(sizeof(APP_FATAL) == data_size);

  _mtlk_process_fw_log_buffers_on_exception(nic, app_fatal->LogBufDescOnFatal);
  _mtlk_process_mac_hang(nic, MTLK_HW_STATE_EXCEPTION, MAC_TO_HOST32(app_fatal->uLmOrUm));

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_mac_exception_sync(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  APP_FATAL *app_fatal = (APP_FATAL*)data;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);

  MTLK_ASSERT(sizeof(APP_FATAL) == data_size);

  WLOG_DSDSDDDD("CID-%04x: %s [on wlan%u]: From %s : MAC exception: Cause 0x%x, EPC 0x%x, Status 0x%x, TS 0x%x",
    mtlk_vap_get_oid(nic->vap_handle),
    mtlk_mac_event_prefix,
    MAC_TO_HOST32(app_fatal->FWinterface),
    app_fatal->uLmOrUm == 0 ? "lower" : "upper",
    MAC_TO_HOST32(app_fatal->uCauseRegOrLineNum),
    MAC_TO_HOST32(app_fatal->uEpcReg),
    MAC_TO_HOST32(app_fatal->uStatusReg),
    MAC_TO_HOST32(app_fatal->uTimeStamp));

  _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_mac_exception, HANDLE_T(nic), data, data_size);
  
  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_mac_fatal(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  APP_FATAL *app_fatal = (APP_FATAL*)data;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);

  MTLK_ASSERT(sizeof(APP_FATAL) == data_size);

  _mtlk_process_fw_log_buffers_on_exception(nic, app_fatal->LogBufDescOnFatal);
  _mtlk_process_mac_fatal_log(nic, app_fatal);
  _mtlk_process_mac_hang(nic, MTLK_HW_STATE_APPFATAL, MAC_TO_HOST32(app_fatal->uLmOrUm));

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_mac_fatal_sync(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  APP_FATAL *app_fatal = (APP_FATAL*)data;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);

  MTLK_ASSERT(sizeof(APP_FATAL) == data_size);

  WLOG_DSDSDDDD("CID-%04x: %s [on wlan%u]: From %s : MAC fatal error: [GroupID: %u, FileID: %u, Line: %u], TS 0x%x",
    mtlk_vap_get_oid(nic->vap_handle),
    mtlk_mac_event_prefix,
    MAC_TO_HOST32(app_fatal->FWinterface),
    app_fatal->uLmOrUm == 0 ? "lower" : "upper",
    MAC_TO_HOST32(app_fatal->GroupId),
    MAC_TO_HOST32(app_fatal->FileId),
    MAC_TO_HOST32(app_fatal->uCauseRegOrLineNum),
    MAC_TO_HOST32(app_fatal->uTimeStamp));

  _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_mac_fatal, HANDLE_T(nic), data, data_size);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_eeprom_failure(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  MTLK_ASSERT(sizeof(EEPROM_FAILURE_EVENT) == data_size);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_eeprom_failure_sync(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);

  MTLK_ASSERT(sizeof(EEPROM_FAILURE_EVENT) == data_size);

  WLOG_DSD("CID-%04x: %s: EEPROM failure : Code %d", mtlk_vap_get_oid(HANDLE_T_PTR(mtlk_core_t, object)->vap_handle),
    mtlk_mac_event_prefix,
    ((EEPROM_FAILURE_EVENT*) data)->u8ErrCode);

  _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_eeprom_failure, HANDLE_T(nic), data, data_size);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_generic_event(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  MTLK_ASSERT(sizeof(GENERIC_EVENT) == data_size);

  ILOG0_DSD("CID-%04x: %s: Generic data: size %u", mtlk_vap_get_oid(HANDLE_T_PTR(mtlk_core_t, object)->vap_handle),
        mtlk_mac_event_prefix,
        le32_to_cpu(((GENERIC_EVENT*) data)->u32dataLength));
  mtlk_dump(0, &((GENERIC_EVENT*) data)->u8data, GENERIC_DATA_SIZE, "Generic MAC data");

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_algo_failure(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  CALIBR_ALGO_EVENT* calibr_event = (CALIBR_ALGO_EVENT*) data;
  MTLK_ASSERT(sizeof(CALIBR_ALGO_EVENT) == data_size);

  if (le32_to_cpu(calibr_event->u32calibrAlgoType) == 
        MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CALIBRATION_ALGO_MASK))
  {
    WLOG_DSDD("CID-%04x: %s: Algo calibration failure: algo type %u, error code %u", mtlk_vap_get_oid(nic->vap_handle),
         mtlk_mac_event_prefix,
         le32_to_cpu(calibr_event->u32calibrAlgoType),
         le32_to_cpu(calibr_event->u32ErrCode));
  }
  else
  {
    ILOG0_DSDD("CID-%04x: %s: Online calibration scheduler: algo type %u, state %u", mtlk_vap_get_oid(nic->vap_handle),
          mtlk_mac_event_prefix,
          le32_to_cpu(calibr_event->u32calibrAlgoType),
          le32_to_cpu(calibr_event->u32ErrCode));
  }

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_dummy_event(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  MTLK_ASSERT(sizeof(DUMMY_EVENT) == data_size);

  ILOG0_DSDDDDDDDD("CID-%04x: %s: Dummy event : %u %u %u %u %u %u %u %u", mtlk_vap_get_oid(HANDLE_T_PTR(mtlk_core_t, object)->vap_handle),
        mtlk_mac_event_prefix,
        MAC_TO_HOST32(((DUMMY_EVENT*) data)->u32Dummy1),
        MAC_TO_HOST32(((DUMMY_EVENT*) data)->u32Dummy2),
        MAC_TO_HOST32(((DUMMY_EVENT*) data)->u32Dummy3),
        MAC_TO_HOST32(((DUMMY_EVENT*) data)->u32Dummy4),
        MAC_TO_HOST32(((DUMMY_EVENT*) data)->u32Dummy5),
        MAC_TO_HOST32(((DUMMY_EVENT*) data)->u32Dummy6),
        MAC_TO_HOST32(((DUMMY_EVENT*) data)->u32Dummy7),
        MAC_TO_HOST32(((DUMMY_EVENT*) data)->u32Dummy8));

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_unknown_event(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  MTLK_ASSERT(sizeof(uint32) == data_size);

  ILOG0_DSD("CID-%04x: %s: unknown MAC event id %u", mtlk_vap_get_oid(HANDLE_T_PTR(mtlk_core_t, object)->vap_handle),
        mtlk_mac_event_prefix, *(uint32*)data);

  return MTLK_ERR_OK;
}

static void __MTLK_IFUNC
_mtlk_handle_mac_event(mtlk_core_t         *nic,
                       MAC_EVENT           *event)
{
  uint32 event_id = MAC_TO_HOST32(event->u32EventID) & 0xff;

  switch(event_id)
  {
  case EVENT_EXCEPTION:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _mtlk_handle_mac_exception_sync,
                          HANDLE_T(nic), &event->u.sAppFatalEvent, sizeof(APP_FATAL));
    break;
  case EVENT_EEPROM_FAILURE:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _mtlk_handle_eeprom_failure_sync,
                          HANDLE_T(nic), &event->u.sEepromEvent, sizeof(EEPROM_FAILURE_EVENT));
    break;
  case EVENT_APP_FATAL:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _mtlk_handle_mac_fatal_sync,
                          HANDLE_T(nic), &event->u.sAppFatalEvent, sizeof(APP_FATAL));
    break;
  case EVENT_GENERIC_EVENT:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_generic_event,
                          HANDLE_T(nic), &event->u.sGenericData, sizeof(GENERIC_EVENT));
    break;
  case EVENT_CALIBR_ALGO_FAILURE:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_algo_failure,
                          HANDLE_T(nic), &event->u.sCalibrationEvent, sizeof(CALIBR_ALGO_EVENT));
    break;
  case EVENT_DUMMY:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_dummy_event,
                          HANDLE_T(nic), &event->u.sDummyEvent, sizeof(DUMMY_EVENT));
    break;
  default:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_unknown_event,
                          HANDLE_T(nic), &event_id, sizeof(uint32));
    break;
  }
}

static int __MTLK_IFUNC
_mtlk_handle_unknown_ind_type(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  MTLK_ASSERT(sizeof(uint32) == data_size);

  ILOG0_DD("CID-%04x:Unknown MAC indication type %u", mtlk_vap_get_oid(HANDLE_T_PTR(mtlk_core_t, object)->vap_handle),
        *(uint32*)data);

  return MTLK_ERR_OK;
}

static void
_mtlk_core_handle_rx_ctrl (mtlk_vap_handle_t    vap_handle,
                          uint32               id,
                          void                *payload,
                          uint32               payload_buffer_size)
{
  mtlk_core_t *nic = mtlk_vap_get_core (vap_handle);

  MTLK_ASSERT(NULL != nic);

  switch(id)
  {
  case MC_MAN_DYNAMIC_PARAM_IND:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_dynamic_param_ind,
                          HANDLE_T(nic), payload, sizeof(UMI_DYNAMIC_PARAM_TABLE));
    break;
  case MC_MAN_MAC_EVENT_IND:
    _mtlk_handle_mac_event(nic, (MAC_EVENT*)payload);
    break;
  case MC_MAN_NETWORK_EVENT_IND:
    _handle_network_event(nic, (UMI_NETWORK_EVENT*)payload);
    break;
  case MC_MAN_CONNECTION_EVENT_IND:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_fw_connection_event_indication,
                          HANDLE_T(nic), payload, sizeof(UMI_CONNECTION_EVENT));
    break;
  case MC_MAN_VAP_WAS_REMOVED_IND:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_vap_removed_ind,
                          HANDLE_T(nic), payload, sizeof(UMI_DEACTIVATE_VAP));
    break;
  case MC_MAN_SECURITY_ALERT_IND:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_security_alert_ind,
                          HANDLE_T(nic), payload, sizeof(UMI_SECURITY_ALERT));
    break;
  case MC_MAN_PM_UPDATE_IND:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_pm_update_event,
                          HANDLE_T(nic), payload, sizeof(UMI_PM_UPDATE));
    break;
  case MC_MAN_AOCS_IND:
      _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_aocs_tcp_event, 
                            HANDLE_T(nic), payload, payload_buffer_size);
    break;
  case MC_DBG_LOGGER_INIT_FAILD_IND:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_logger_init_failed_event,
                          HANDLE_T(nic), payload, 0);
    break;
  default:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_unknown_ind_type,
                          HANDLE_T(nic), &id, sizeof(uint32));
    break;
  }
}

void __MTLK_IFUNC
mtlk_core_handle_tx_ctrl (mtlk_core_t         *nic,
                          mtlk_user_request_t *req,
                          uint32               id,
                          mtlk_clpb_t         *data)
{
#define _MTLK_CORE_REQ_MAP_START(req_id)                                                \
  switch (req_id) {

#define _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(req_id, func)                                \
  case (req_id):                                                                        \
    _mtlk_process_user_task(nic, req, SERIALIZABLE, req_id, func, HANDLE_T(nic), data); \
    break;

#define _MTLK_CORE_HANDLE_REQ_SYNCHRONOUS(req_id, func)                                 \
  case (req_id):                                                                        \
  _mtlk_process_user_task(nic, req, SYNCHRONOUS, req_id, func, HANDLE_T(nic), data);    \
  break;

#define _MTLK_CORE_REQ_MAP_END()                                                        \
    default:                                                                            \
      MTLK_ASSERT(FALSE);                                                               \
  }

  MTLK_ASSERT(NULL != nic);
  MTLK_ASSERT(NULL != req);
  MTLK_ASSERT(NULL != data);

  _MTLK_CORE_REQ_MAP_START(id)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AP_CAPABILITIES,       _mtlk_core_get_ap_capabilities)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_ACTIVATE_OPEN,             _mtlk_core_activate)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_CONNECT_STA,               _mtlk_core_connect_sta)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_DISCONNECT_STA,            _mtlk_core_hanle_disconnect_sta_req)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_AP_DISCONNECT_STA,         _mtlk_core_ap_disconnect_sta)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_AP_DISCONNECT_ALL,         _mtlk_core_ap_disconnect_all)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_DEACTIVATE,                _mtlk_core_deactivate)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_START_SCANNING,            _mtlk_core_start_scanning);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_SCANNING_RES,          _mtlk_core_get_scanning_res);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_MAC_ADDR,              _mtlk_core_set_mac_addr_wrapper);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_MAC_ADDR,              _mtlk_core_get_mac_addr);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_STATUS,                _mtlk_core_get_status);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_RESET_STATS,               _mtlk_core_reset_stats);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_ADDBA_CFG,             _mtlk_core_get_addba_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_ADDBA_CFG,             _mtlk_core_set_addba_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_WME_BSS_CFG,           _mtlk_core_get_wme_bss_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_WME_BSS_CFG,           _mtlk_core_set_wme_bss_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_WME_AP_CFG,            _mtlk_core_get_wme_ap_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_WME_AP_CFG,            _mtlk_core_set_wme_ap_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AOCS_CFG,              _mtlk_core_get_aocs_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_AOCS_CFG,              _mtlk_core_set_aocs_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_DOT11H_CFG,            _mtlk_core_get_dot11h_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_DOT11H_CFG,            _mtlk_core_set_dot11h_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_DOT11H_AP_CFG,         _mtlk_core_get_dot11h_ap_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_DOT11H_AP_CFG,         _mtlk_core_set_dot11h_ap_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_L2NAT_CLEAR_TABLE,         _mtlk_core_l2nat_clear_table);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_ANTENNA_GAIN,          _mtlk_core_get_ant_gain);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_MIBS_CFG,              _mtlk_core_get_mibs_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_MIBS_CFG,              _mtlk_core_set_mibs_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COUNTRY_CFG,           _mtlk_core_get_country_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COUNTRY_CFG,           _mtlk_core_set_country_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_L2NAT_CFG,             _mtlk_core_get_l2nat_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_L2NAT_CFG,             _mtlk_core_set_l2nat_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_DOT11D_CFG,            _mtlk_core_get_dot11d_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_DOT11D_CFG,            _mtlk_core_set_dot11d_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_MAC_WATCHDOG_CFG,      _mtlk_core_get_mac_wdog_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_MAC_WATCHDOG_CFG,      _mtlk_core_set_mac_wdog_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_STADB_CFG,             _mtlk_core_get_stadb_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_STADB_CFG,             _mtlk_core_set_stadb_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_SQ_CFG,                _mtlk_core_get_sq_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_SQ_CFG,                _mtlk_core_set_sq_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_CORE_CFG,              _mtlk_core_get_core_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_CORE_CFG,              _mtlk_core_set_core_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_MASTER_CFG,            _mtlk_core_get_master_specific_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_MASTER_CFG,            _mtlk_core_set_master_specific_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_MASTER_AP_CFG,         _mtlk_core_get_master_ap_specific_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_MASTER_AP_CFG,         _mtlk_core_set_master_ap_specific_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_EEPROM_CFG,            _mtlk_core_get_eeprom_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_HSTDB_CFG,             _mtlk_core_get_hstdb_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_HSTDB_CFG,             _mtlk_core_set_hstdb_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_SCAN_CFG,              _mtlk_core_get_scan_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_SCAN_CFG,              _mtlk_core_set_scan_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_HW_DATA_CFG,           _mtlk_core_set_hw_data_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_QOS_CFG,               _mtlk_core_get_qos_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_QOS_CFG,               _mtlk_core_set_qos_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COC_CFG,               _mtlk_core_get_coc_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COC_CFG,               _mtlk_core_set_coc_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AOCS_TBL,              _mtlk_core_get_aocs_table);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AOCS_CHANNELS_TBL,     _mtlk_core_get_aocs_channels);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AOCS_HISTORY,          _mtlk_core_get_aocs_history);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AOCS_PENALTIES,        _mtlk_core_get_aocs_penalties);
#ifdef AOCS_DEBUG
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_AOCS_CL,               _mtlk_core_get_aocs_debug_update_cl);
#endif /* AOCS_DEBUG */
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_HW_LIMITS,             _mtlk_core_get_hw_limits);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_REG_LIMITS,            _mtlk_core_get_reg_limits);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_STOP_LM,                   _mtlk_core_stop_lm);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_MAC_CALIBRATE,             _mtlk_core_mac_calibrate);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_IW_GENERIC,            _mtlk_core_get_iw_generic);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_CTRL_MAC_GPIO,             _mtlk_core_ctrl_mac_gpio);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GEN_DATA_EXCHANGE,         _mtlk_core_gen_data_exchange);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_EE_CAPS,               _mtlk_core_get_ee_caps);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_L2NAT_STATS,           _mtlk_core_get_l2nat_stats);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_SQ_STATUS,             _mtlk_core_get_sq_status);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_MAC_ASSERT,            _mtlk_core_set_mac_assert);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_MC_IGMP_TBL,           _mtlk_core_get_mc_igmp_tbl);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_BCL_MAC_DATA,          _mtlk_core_bcl_mac_data_get);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_BCL_MAC_DATA,          _mtlk_core_bcl_mac_data_set);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_RANGE_INFO,            _mtlk_core_range_info_get);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_STADB_STATUS,          _mtlk_core_get_stadb_sta_list);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_WEP_ENC_CFG,           _mtlk_core_set_wep_enc_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_WEP_ENC_CFG,           _mtlk_core_get_wep_enc_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_AUTH_CFG,              _mtlk_core_set_auth_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AUTH_CFG,              _mtlk_core_get_auth_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_GENIE_CFG,             _mtlk_core_set_genie_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_ENCEXT_CFG,            _mtlk_core_get_enc_ext_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_ENCEXT_CFG,            _mtlk_core_set_enc_ext_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_MBSS_ADD_VAP,              _mtlk_core_add_vap);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_MBSS_DEL_VAP,              _mtlk_core_del_vap);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_MBSS_SET_VARS,             _mtlk_core_set_mbss_vars);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_MBSS_GET_VARS,             _mtlk_core_get_mbss_vars);
    _MTLK_CORE_HANDLE_REQ_SYNCHRONOUS(MTLK_CORE_REQ_GET_SERIALIZER_INFO,        _mtlk_core_get_serializer_info);
    /* 20/40 coexistence feature */
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COEX_20_40_MODE_CFG,   _mtlk_core_set_coex_20_40_mode_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COEX_20_40_MODE_CFG,   _mtlk_core_get_coex_20_40_mode_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COEX_20_40_EXM_REQ_CFG,_mtlk_core_set_coex_20_40_exm_req_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COEX_20_40_EXM_REQ_CFG,_mtlk_core_get_coex_20_40_exm_req_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COEX_20_40_TIMES_CFG,  _mtlk_core_set_coex_20_40_times_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COEX_20_40_TIMES_CFG,  _mtlk_core_get_coex_20_40_times_cfg);

    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_FW_LED_CFG,            _mtlk_core_set_fw_led_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_FW_LED_CFG,            _mtlk_core_get_fw_led_cfg);

  _MTLK_CORE_REQ_MAP_END()

#undef _MTLK_CORE_REQ_MAP_START
#undef _MTLK_CORE_HANDLE_REQ_SERIALIZABLE
#undef _MTLK_CORE_REQ_MAP_END
}

/*****************************************************************************
**
** NAME         get_firmware_version
**
** PARAMETERS   fname               Firmware file
**              data                Buffer for processing
**              size                Size of buffer
**
** RETURNS      none
**
** DESCRIPTION  Extract firmware version string of firmware file <fname> to
**              global variable <mtlk_version_string> from buffer <data> of
**              size <size>. If given <fname> already processed - skip parsing
**
******************************************************************************/
static void 
get_firmware_version (struct nic      *nic, 
                      const char      *fname, 
                      const char      *data, 
                      unsigned long    size)
{
  static const char MAC_VERSION_SIGNATURE[] = "@@@ VERSION INFO @@@";
  const char *border = data + size;

  if (strstr(mtlk_version_string, fname)) return;

  data = mtlk_osal_str_memchr(data, '@', border - data);
  while (data) {
    if (memcmp(data, MAC_VERSION_SIGNATURE, strlen(MAC_VERSION_SIGNATURE)) == 0) {
      char *v = mtlk_version_string + strlen(mtlk_version_string);
      sprintf(v, "%s: %s\n", fname, data);
      break;
    }
    data = mtlk_osal_str_memchr(data + 1, '@', border - data - 1);
  }
}

static int
_mtlk_core_get_prop (mtlk_vap_handle_t vap_handle, mtlk_core_prop_e prop_id, void* buffer, uint32 size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;

  switch (prop_id) {
  case MTLK_CORE_PROP_MAC_SW_RESET_ENABLED:
    if (buffer && size == sizeof(uint32))
    {
      uint32 *mac_sw_reset_enabled = (uint32 *)buffer;

      *mac_sw_reset_enabled = MTLK_CORE_PDB_GET_INT(mtlk_vap_get_core (vap_handle), PARAM_DB_CORE_MAC_SOFT_RESET_ENABLE);
      res = MTLK_ERR_OK;
    }
  break;
  default:
    break;
  }
  return res;
}

static int
_mtlk_core_set_prop (mtlk_vap_handle_t vap_handle,
                    mtlk_core_prop_e  prop_id, 
                    void             *buffer, 
                    uint32            size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_core_t *nic = mtlk_vap_get_core (vap_handle);

  switch (prop_id)
  {
  case MTLK_CORE_PROP_FIRMWARE_BIN_BUFFER:
    if (buffer && size == sizeof(mtlk_core_firmware_file_t))
    {
      mtlk_core_firmware_file_t *fw_buffer = (mtlk_core_firmware_file_t *)buffer;

      get_firmware_version(nic, 
                           fw_buffer->fname, 
                           fw_buffer->content.buffer, 
                           fw_buffer->content.size);

      res = MTLK_ERR_OK;
    }
    break;
  case MTLK_CORE_PROP_MAC_STUCK_DETECTED:
    if (buffer && size == sizeof(uint32))
    {
      uint32 *cpu_no = (uint32 *)buffer;
      nic->slow_ctx->mac_stuck_detected_by_sw = 1;
      mtlk_set_hw_state(nic, MTLK_HW_STATE_APPFATAL);
      mtlk_core_set_net_state(nic, NET_STATE_HALTED);
      mtlk_df_ui_notify_notify_fw_hang(mtlk_vap_get_df(nic->vap_handle), *cpu_no, MTLK_HW_STATE_APPFATAL);
    }
    break;
  default:
    break;
  }

  return res;
}

void __MTLK_IFUNC
mtlk_core_api_delete (mtlk_core_api_t *core_api)
{
  _mtlk_core_cleanup(core_api->obj);
  mtlk_fast_mem_free(core_api->obj);
  mtlk_fast_mem_free(core_api);
}

void
mtlk_find_and_update_ap(mtlk_handle_t context, uint8 *addr, bss_data_t *bss_data)
{
  uint8 channel, is_ht;
  struct nic *nic = (struct nic *)context;
  int lost_beacons;
  sta_entry *sta = NULL;

  /* No updates in not connected state of for non-STA or during scan*/
  if (mtlk_vap_is_ap(nic->vap_handle) ||
      (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED) ||
      mtlk_core_scan_is_running(nic))
    return;

  /* Check wrong AP */
  sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, addr);
  ILOG4_YP("Trying to find AP %Y, PTR is %p", addr, sta);
  if (sta == NULL) {
    nic->pstats.discard_nwi++;
    return;
  }

  /* Read setings for checks */
  is_ht = mtlk_core_get_is_ht_cur(nic);
  channel = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR);

  /* Check channel change */
  if (bss_data->channel != channel) {
    ILOG0_DYDD("CID-%04x: AP %Y changed its channel! (%u -> %u)", mtlk_vap_get_oid(nic->vap_handle), addr, channel, bss_data->channel);
    goto DISCONNECT;
  }

  /* Check HT capabilities change (only if HT is allowed in configuration) */
  if (mtlk_core_get_is_ht_cfg(nic) && !nic->slow_ctx->is_tkip && !nic->slow_ctx->wep_enabled &&
      (!!bss_data->is_ht != is_ht)) {
    ILOG0_DYS("CID-%04x: AP %Y changed its HT capabilities! (%s)", mtlk_vap_get_oid(nic->vap_handle),
        addr, is_ht ? "HT -> non-HT" : "non-HT -> HT");
    goto DISCONNECT;
  }

  /* Update lost beacons */
  lost_beacons = mtlk_sta_update_beacon_interval(sta, bss_data->beacon_interval);
  mtlk_sta_decref(sta); /* De-reference of find */
  nic->pstats.missed_beacon += lost_beacons;
  return;

DISCONNECT:
  mtlk_sta_decref(sta); /* De-reference of find */
  ILOG1_Y("Disconnecting AP %Y due to changed parameters", addr);
  _mtlk_core_schedule_disconnect_me(nic, FM_STATUSCODE_PEER_PARAMS_CHANGED);
}

static signed int
find_acl_entry (IEEE_ADDR *list, IEEE_ADDR *mac, signed int *free_entry)
{
  int i;
  signed int idx;

  idx = -1;
  for (i = 0; i < MAX_ADDRESSES_IN_ACL; i++) {
    if (0 == mtlk_osal_compare_eth_addresses(mac->au8Addr, list[i].au8Addr)) {
      idx = i;
      break;
    }
  }
  if (NULL == free_entry)
	return idx;
  /* find first free entry */
  *free_entry = -1;
  for (i = 0; i < MAX_ADDRESSES_IN_ACL; i++) {
    if (mtlk_osal_is_zero_address(list[i].au8Addr)) {
      *free_entry = i;
      break;
    }
  }
  return idx;
}

int
mtlk_core_set_acl(struct nic *nic, IEEE_ADDR *mac, IEEE_ADDR *mac_mask)
{
  signed int idx, free_idx;
  IEEE_ADDR addr_tmp;

  if (mtlk_osal_is_zero_address(mac->au8Addr)) {
    ILOG2_V("Upload ACL list");
    return MTLK_ERR_OK;
  }

  /* Check pair MAC/MAC-mask consistency : MAC == (MAC & MAC-mask) */
  if (NULL != mac_mask) {
    mtlk_osal_eth_apply_mask(addr_tmp.au8Addr, mac->au8Addr, mac_mask->au8Addr);
    if (0 != mtlk_osal_compare_eth_addresses(addr_tmp.au8Addr, mac->au8Addr)) {
      WLOG_V("The ACL rule addition failed: "
           "The specified mask parameter is invalid. (MAC & MAC-Mask) != MAC.");
      return MTLK_ERR_PARAMS;
    }
  }

  idx = find_acl_entry(nic->slow_ctx->acl, mac, &free_idx);
  if (idx >= 0) {
    /* already on the list */
    WLOG_YD("MAC %Y is already on the ACL list at %d", mac->au8Addr, idx);
    return MTLK_ERR_OK;
  }
  if (free_idx < 0) {
    /* list is full */
    WLOG_V("ACL list is full");
    return MTLK_ERR_NO_RESOURCES;
  }
  /* add new entry */
  nic->slow_ctx->acl[free_idx] = *mac;
  if (NULL != mac_mask) {
    nic->slow_ctx->acl_mask[free_idx] = *mac_mask;
  } else {
    nic->slow_ctx->acl_mask[free_idx] = EMPTY_MAC_MASK;
  }

  ILOG2_YD("Added %Y to the ACL list at %d", mac->au8Addr, free_idx);
  return MTLK_ERR_OK;
}

int
mtlk_core_del_acl(struct nic *nic, IEEE_ADDR *mac)
{
  signed int idx;

  if (mtlk_osal_is_zero_address(mac->au8Addr)) {
    ILOG2_V("Delete ACL list");
    memset(nic->slow_ctx->acl, 0, sizeof(nic->slow_ctx->acl));
    return MTLK_ERR_OK;
  }
  idx = find_acl_entry(nic->slow_ctx->acl, mac, NULL);
  if (idx < 0) {
    /* not found on the list */
    WLOG_Y("MAC %Y is not on the ACL list", mac->au8Addr);
    return MTLK_ERR_PARAMS;
  }
  /* del entry */
  nic->slow_ctx->acl[idx] = EMPTY_MAC_ADDR;
  nic->slow_ctx->acl_mask[idx] = EMPTY_MAC_MASK;

  ILOG5_YD("Cleared %Y from the ACL list at %d", mac->au8Addr, idx);
  return MTLK_ERR_OK;
}

mtlk_handle_t __MTLK_IFUNC
mtlk_core_get_tx_limits_handle(mtlk_handle_t nic)
{
  return HANDLE_T(&(((struct nic*)nic)->slow_ctx->tx_limits));
}

int __MTLK_IFUNC
_mtlk_core_get_aocs_history(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_aocs_get_history(nic->slow_ctx->aocs, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_aocs_table(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_aocs_get_table(nic->slow_ctx->aocs, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_aocs_channels(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_aocs_get_channels(nic->slow_ctx->aocs, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_aocs_penalties(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_aocs_get_penalties(nic->slow_ctx->aocs, clpb);

  return res;
}

#ifdef AOCS_DEBUG
int __MTLK_IFUNC
_mtlk_core_get_aocs_debug_update_cl(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  uint32 clpb_data_size;
  void* clpb_data;
  uint32 cl;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  clpb_data = mtlk_clpb_enum_get_next(clpb, &clpb_data_size);
  MTLK_ASSERT(NULL != clpb_data);
  MTLK_ASSERT(sizeof(uint32) == clpb_data_size);
  cl = *(uint32*) clpb_data;

  mtlk_aocs_debug_update_cl(nic->slow_ctx->aocs, cl);

  return res;
}
#endif /*AOCS_DEBUG*/

int __MTLK_IFUNC
_mtlk_core_get_hw_limits(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_channels_get_hw_limits(&nic->slow_ctx->tx_limits, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_reg_limits(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_channels_get_reg_limits(&nic->slow_ctx->tx_limits, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_ant_gain(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_channels_get_ant_gain(&nic->slow_ctx->tx_limits, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_l2nat_stats(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_l2nat_get_stats(nic, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_sq_status(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  BOOL get_peers_status;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  get_peers_status = (NET_STATE_CONNECTED == mtlk_core_get_net_state(nic));

  res = mtlk_sq_get_status(nic->sq, clpb, get_peers_status);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_set_mac_assert(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  uint32 clpb_data_size;
  void* clpb_data;
  int assert_type;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  clpb_data = mtlk_clpb_enum_get_next(clpb, &clpb_data_size);
  MTLK_ASSERT(NULL != clpb_data);
  MTLK_ASSERT(sizeof(int) == clpb_data_size);
  assert_type = *(uint32*) clpb_data;

  ILOG0_DD("CID-%04x: Rise MAC assert (type=%d)", mtlk_vap_get_oid(nic->vap_handle), assert_type);

  switch (assert_type) {
    case MTLK_CORE_UI_ASSERT_TYPE_FW_LMIPS:
    case MTLK_CORE_UI_ASSERT_TYPE_FW_UMIPS:
     {
      uint32 mips_no = (assert_type == MTLK_CORE_UI_ASSERT_TYPE_FW_UMIPS)?UMIPS:LMIPS;
      res     = mtlk_vap_get_hw_vft(nic->vap_handle)->set_prop(nic->vap_handle, MTLK_HW_DBG_ASSERT_FW, &mips_no, sizeof(mips_no));

      if (res != MTLK_ERR_OK) {
        ELOG_DDD("CID-%04x: Can't assert FW MIPS#%d (res=%d)", mtlk_vap_get_oid(nic->vap_handle), mips_no, res);
      }
    }
    break;

    case MTLK_CORE_UI_ASSERT_TYPE_DRV_DIV0:
    {
      volatile int do_bug = 0;
      do_bug = 1/do_bug;
      ILOG0_D("do_bug = %d", do_bug); /* To avoid compilation optimization */
      res = MTLK_ERR_OK;
    }
    break;

    case MTLK_CORE_UI_ASSERT_TYPE_DRV_BLOOP:
      while (1) {;}
      break;

    case MTLK_CORE_UI_ASSERT_TYPE_NONE:
    case MTLK_CORE_UI_ASSERT_TYPE_LAST:
    default:
      ILOG0_DD("CID-%04x: Unsupported assert type: %d", mtlk_vap_get_oid(nic->vap_handle), assert_type);
      res = MTLK_ERR_NOT_SUPPORTED;
      break;
  };

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_mc_igmp_tbl(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_mc_dump_groups(nic, clpb);

  return res;
}

int
_mtlk_core_get_stadb_sta_list(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_ui_get_stadb_status_req_t *get_stadb_status_req;
  uint32 size;
  hst_db *hstdb = NULL;
  uint8 group_cipher = FALSE;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ( 0 == (mtlk_core_get_net_state(nic) & (NET_STATE_HALTED | NET_STATE_CONNECTED)) ) {
    mtlk_clpb_purge(clpb);
    return res;
  }

  get_stadb_status_req = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == get_stadb_status_req) || (sizeof(*get_stadb_status_req) != size) ) {
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  if (get_stadb_status_req->get_hostdb) {
    hstdb = &nic->slow_ctx->hstdb;
  }

  if (get_stadb_status_req->use_cipher) {
    group_cipher = nic->group_cipher;
  }

  mtlk_clpb_purge(clpb);
  res = mtlk_stadb_get_stat(&nic->slow_ctx->stadb, hstdb, clpb, group_cipher);

finish:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_ee_caps(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_eeprom_get_caps(mtlk_core_get_eeprom(nic), clpb);

  return res;
}

mtlk_core_t * __MTLK_IFUNC
mtlk_core_get_master (mtlk_core_t *core)
{
  MTLK_ASSERT(core != NULL);

  return mtlk_vap_manager_get_master_core(mtlk_vap_get_manager(core->vap_handle));
}

uint8 __MTLK_IFUNC mtlk_core_is_device_busy(mtlk_handle_t context)
{

    mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, context);
    return (  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_DFS_RADAR_DETECTION)
           && !mtlk_dot11h_can_switch_now(mtlk_core_get_dfs(nic)));
}

tx_limit_t* __MTLK_IFUNC
mtlk_core_get_tx_limits(mtlk_core_t *core)
{
  return &core->slow_ctx->tx_limits;
}

void
mtlk_core_configuration_dump(mtlk_core_t *core)
{
  ILOG0_DS("CID-%04x: Country             : %s", mtlk_vap_get_oid(core->vap_handle), country_code_to_country(mtlk_core_get_country_code(core)));
  ILOG0_DD("CID-%04x: Domain              : %u", mtlk_vap_get_oid(core->vap_handle), country_code_to_domain(mtlk_core_get_country_code(core)));
  ILOG0_DS("CID-%04x: Network mode        : %s", mtlk_vap_get_oid(core->vap_handle), net_mode_to_string(mtlk_core_get_network_mode_cfg(core)));
  ILOG0_DS("CID-%04x: Band                : %s", mtlk_vap_get_oid(core->vap_handle), mtlk_eeprom_band_to_string(net_mode_to_band(mtlk_core_get_network_mode_cfg(core))));
  ILOG0_DS("CID-%04x: Prog Model Spectrum : %s MHz", mtlk_vap_get_oid(core->vap_handle), MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE) ? "40": "20");
  ILOG0_DS("CID-%04x: Selected Spectrum   : %s MHz", mtlk_vap_get_oid(core->vap_handle), MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE) ? "40": "20");
  ILOG0_DD("CID-%04x: Bonding             : %u", mtlk_vap_get_oid(core->vap_handle),  core->slow_ctx->bonding); /* "Lower"(1) : "Upper"(0)) */
  ILOG0_DS("CID-%04x: HT mode             : %s", mtlk_vap_get_oid(core->vap_handle),  mtlk_core_get_is_ht_cur(core) ? "enabled" : "disabled");
  ILOG0_DS("CID-%04x: SM enabled          : %s", mtlk_vap_get_oid(core->vap_handle),
      mtlk_eeprom_get_disable_sm_channels(mtlk_core_get_eeprom(core)) ? "disabled" : "enabled");
}


static void
_mtlk_core_prepare_stop(mtlk_vap_handle_t vap_handle)
{
  mtlk_core_t *nic = mtlk_vap_get_core (vap_handle);

  ILOG1_V("Core prepare stopping....");
  mtlk_osal_lock_acquire(&nic->net_state_lock);
  nic->is_stopping = TRUE;
  mtlk_osal_lock_release(&nic->net_state_lock);

  if (mtlk_vap_is_slave_ap(vap_handle)) {
    return;
  }

  if (NET_STATE_HALTED == mtlk_core_get_net_state(nic)) {
    if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
      scan_complete(&nic->slow_ctx->scan);
    }
  } else {
    scan_terminate_and_wait_completion(&nic->slow_ctx->scan);
  }
}

BOOL __MTLK_IFUNC
mtlk_core_is_stopping(mtlk_core_t *core)
{
  return (core->is_stopping || core->is_iface_stopping);
}

void
_mtlk_core_bswap_bcl_request (UMI_BCL_REQUEST *req, BOOL hdr_only)
{
  int i;

  req->Size    = cpu_to_le32(req->Size);
  req->Address = cpu_to_le32(req->Address);
  req->Unit    = cpu_to_le32(req->Unit);

  if (!hdr_only) {
    for (i = 0; i < ARRAY_SIZE(req->Data); i++) {
      req->Data[i] = cpu_to_le32(req->Data[i]);
    }
  }
}

int __MTLK_IFUNC
_mtlk_core_bcl_mac_data_get (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t* man_entry = NULL;
  int exception;
  mtlk_hw_state_e hw_state;
  UMI_BCL_REQUEST* preq;
  BOOL f_bswap_data = TRUE;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  /* Get BCL request from CLPB */
  preq = mtlk_clpb_enum_get_next(clpb, NULL);
  if (NULL == preq) {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  /* Check MAC state */
  hw_state = mtlk_core_get_hw_state(core);
  exception = (((hw_state == MTLK_HW_STATE_EXCEPTION) ||
                (hw_state == MTLK_HW_STATE_APPFATAL)) &&
               !core->slow_ctx->mac_stuck_detected_by_sw);

  /* if Core got here preq->Unit field wiath value greater or equal to BCL_UNIT_MAX -
   * the Core should not convert result data words in host format. */
  if (preq->Unit >= BCL_UNIT_MAX) {
    preq->Unit -= BCL_UNIT_MAX; /*Restore original field value*/
    f_bswap_data = FALSE;
  }

  ILOG2_SDDDD("Getting BCL over %s unit(%d) address(0x%x) size(%u) (%x)",
       exception ? "io" : "txmm",
       (int)preq->Unit,
       (unsigned int)preq->Address,
       (unsigned int)preq->Size,
       (unsigned int)preq->Data[0]);

  if (exception)
  {
    /* MAC is halted - send BCL request through IO */
    _mtlk_core_bswap_bcl_request(preq, TRUE);

    res = mtlk_vap_get_hw_vft(core->vap_handle)->get_prop(core->vap_handle, MTLK_HW_BCL_ON_EXCEPTION, preq, sizeof(*preq));

    if (MTLK_ERR_OK != res) {
      ELOG_D("CID-%04x: Can't get BCL", mtlk_vap_get_oid(core->vap_handle));
      goto err_push;
    }
  }
  else
  {
    /* MAC is in normal state - send BCL request through TXMM */
    man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
    if (!man_entry) {
      ELOG_D("CID-%04x: Can't send Get BCL request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
      res = MTLK_ERR_NO_RESOURCES;
      goto err_push;
    }

    _mtlk_core_bswap_bcl_request(preq, TRUE);

    memcpy((UMI_BCL_REQUEST*)man_entry->payload, preq, sizeof(*preq));

    man_entry->id           = UM_MAN_QUERY_BCL_VALUE;
    man_entry->payload_size = sizeof(*preq);

    res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);

    if (MTLK_ERR_OK != res) {
      ELOG_D("CID-%04x: Can't send Get BCL request to MAC, timed-out", mtlk_vap_get_oid(core->vap_handle));
      goto err_push;
    }

    /* Copy back results */
    memcpy(preq, (UMI_BCL_REQUEST*)man_entry->payload, sizeof(*preq));

    mtlk_txmm_msg_cleanup(&man_msg);
  }

  /* Send back results */
  _mtlk_core_bswap_bcl_request(preq, !f_bswap_data);

  mtlk_dump(3, preq, sizeof(*preq), "dump of the UM_MAN_QUERY_BCL_VALUE");

  res = mtlk_clpb_push(clpb, preq, sizeof(*preq));
  if (MTLK_ERR_OK != res) {
    goto err_push;
  }

  goto finish;

err_push:
  mtlk_clpb_purge(clpb);
finish:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_bcl_mac_data_set (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t* man_entry = NULL;
  int exception;
  mtlk_hw_state_e hw_state;
  UMI_BCL_REQUEST* preq = NULL;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  /* Read Set BCL request from CLPB */
  preq = mtlk_clpb_enum_get_next(clpb, NULL);
  if (NULL == preq) {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  /* Check MAC state */
  hw_state = mtlk_core_get_hw_state(core);
  exception = (((hw_state == MTLK_HW_STATE_EXCEPTION) ||
                (hw_state == MTLK_HW_STATE_APPFATAL)) &&
               !core->slow_ctx->mac_stuck_detected_by_sw);

  ILOG2_SDDDD("Setting BCL over %s unit(%d) address(0x%x) size(%u) (%x)",
       exception ? "io" : "txmm",
       (int)preq->Unit,
       (unsigned int)preq->Address,
       (unsigned int)preq->Size,
       (unsigned int)preq->Data[0]);

  mtlk_dump(3, preq, sizeof(*preq), "dump of the UM_MAN_SET_BCL_VALUE");

  if (exception)
  {
    /* MAC is halted - send BCL request through IO */
    _mtlk_core_bswap_bcl_request(preq, FALSE);

    res = mtlk_vap_get_hw_vft(core->vap_handle)->set_prop(core->vap_handle, MTLK_HW_BCL_ON_EXCEPTION, preq, sizeof(*preq));

    if (MTLK_ERR_OK != res) {
      ELOG_D("CID-%04x: Can't set BCL", mtlk_vap_get_oid(core->vap_handle));
      goto finish;
    }
  }
  else
  {
    /* MAC is in normal state - send BCL request through TXMM */
     man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
     if (!man_entry) {
       ELOG_D("CID-%04x: Can't send Set BCL request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
       res = MTLK_ERR_NO_RESOURCES;
       goto finish;
     }

     _mtlk_core_bswap_bcl_request(preq, FALSE);

     memcpy((UMI_BCL_REQUEST*)man_entry->payload, preq, sizeof(*preq));

     man_entry->id           = UM_MAN_SET_BCL_VALUE;
     man_entry->payload_size = sizeof(*preq);

     res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);

     if (MTLK_ERR_OK != res) {
       ELOG_D("CID-%04x: Can't send Set BCL request to MAC, timed-out", mtlk_vap_get_oid(core->vap_handle));
       goto finish;
     }

     mtlk_txmm_msg_cleanup(&man_msg);
  }

finish:
  return res;
}

int
_mtlk_core_set_channel(mtlk_core_t *core, uint16 channel)
{
  int res = MTLK_ERR_OK;
  mtlk_get_channel_data_t param;

  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  if (0 == channel) {
    ILOG1_D("CID-%04x: Enable channel autoselection", mtlk_vap_get_oid(core->vap_handle));

  } else if (0 == mtlk_core_get_country_code(core)) {
    WLOG_DD("CID-%04x: Set channel to %i. (AP Workaround due to invalid Driver parameters setting at BSP startup)", mtlk_vap_get_oid(core->vap_handle),
             channel);

  } else {
    /* Check if channel is supported in current configuration */
    param.reg_domain = country_code_to_domain(mtlk_core_get_country_code(core));
    param.is_ht = mtlk_core_get_is_ht_cfg(core);
    param.ap = mtlk_vap_is_ap(core->vap_handle);
    param.bonding = core->slow_ctx->bonding;
    param.spectrum_mode = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE);
    param.frequency_band = mtlk_core_get_freq_band_cfg(core);
    param.disable_sm_channels = mtlk_eeprom_get_disable_sm_channels(mtlk_core_get_eeprom(core));

    if (MTLK_ERR_OK == mtlk_check_channel(&param, channel)) {
      ILOG1_DD("CID-%04x: Set channel to %i", mtlk_vap_get_oid(core->vap_handle), channel);

    } else {
      WLOG_DD("CID-%04x: Channel (%i) is not supported in current configuration.", mtlk_vap_get_oid(core->vap_handle),
               channel);
      mtlk_core_configuration_dump(core);
      res = MTLK_ERR_PARAMS;
    }
  }

  if (MTLK_ERR_OK == res) {
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_CHANNEL_CFG, channel);
  }

  return res;
}

static int
_mtlk_general_core_range_info_get (mtlk_core_t* nic, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_ui_range_info_t range_info;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(nic->vap_handle));

  /* Get supported bitrates */
  {
    int avail = mtlk_core_get_available_bitrates(nic);
    int32 short_cyclic_prefix = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX);
    int num_bitrates; /* Index in table returned to userspace */
    int value; /* Bitrate's value */
    int i; /* Bitrate index */
    int k, l; /* Counters, used for sorting */

    /* Array of bitrates is sorted and consist of only unique elements */
    num_bitrates = 0;
    for (i = BITRATE_FIRST; i <= BITRATE_LAST; i++) {
      if ((1 << i) & avail) {
        value = mtlk_bitrate_get_value(i, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE), short_cyclic_prefix);
        range_info.bitrates[num_bitrates] = value;
        k = num_bitrates;
        while (k && (range_info.bitrates[k-1] >= value)) k--; /* Position found */
        if ((k == num_bitrates) || (range_info.bitrates[k] != value)) {
          for (l = num_bitrates; l > k; l--)
            range_info.bitrates[l] = range_info.bitrates[l-1];
          range_info.bitrates[k] = value;
          num_bitrates++;
        }
      }
    }

    range_info.num_bitrates = num_bitrates;
  }

  /* Get supported channels */
  {
    uint8 band;
    mtlk_get_channel_data_t param;

    if (0 == mtlk_core_get_country_code(nic)) {
       WLOG_D("CID-%04x: Country is not selected. Channels information is not available.",
               mtlk_vap_get_oid(nic->vap_handle));
       goto finish;
    }

    if (!mtlk_vap_is_ap(nic->vap_handle) && (NET_STATE_CONNECTED == mtlk_core_get_net_state(nic)) )
      band = mtlk_core_get_freq_band_cur(nic);
    else
      band = mtlk_core_get_freq_band_cfg(nic);

    param.reg_domain = country_code_to_domain(mtlk_core_get_country_code(nic));
    param.is_ht = mtlk_core_get_is_ht_cfg(nic);
    param.ap = mtlk_vap_is_ap(nic->vap_handle);
    param.bonding = nic->slow_ctx->bonding;
    param.spectrum_mode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE);
    param.frequency_band = band;
    param.disable_sm_channels = mtlk_eeprom_get_disable_sm_channels(mtlk_core_get_eeprom(nic));

    range_info.num_channels = mtlk_get_avail_channels(&param, range_info.channels);
  }

finish:
  res = mtlk_clpb_push(clpb, &range_info, sizeof(range_info));
  if (MTLK_ERR_OK != res) {
    mtlk_clpb_purge(clpb);
  }

  return res;
}

static int
_mtlk_slave_core_range_info_get (mtlk_core_t* nic, const void* data, uint32 data_size)
{
  return (_mtlk_general_core_range_info_get (mtlk_core_get_master (nic), data, data_size));
}

int __MTLK_IFUNC
_mtlk_core_range_info_get (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  if (!mtlk_vap_is_slave_ap (core->vap_handle)) {
    return _mtlk_general_core_range_info_get (core, data, data_size);
  }
  else
    return _mtlk_slave_core_range_info_get (core, data, data_size);
}

int __MTLK_IFUNC
_mtlk_core_start_scanning(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_BUSY;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  char *essid = NULL;
  int net_state;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  net_state = mtlk_core_get_net_state(nic);

  /* allow scanning in net states ready and connected only */
  if ((net_state & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    WLOG1_DS("CID-%04x: Cannot start scanning in state %s", mtlk_vap_get_oid(nic->vap_handle), mtlk_net_state_to_string(net_state));
    goto CANNOT_SCAN;
  }
  if ((net_state == NET_STATE_CONNECTED) &&
      !mtlk_scan_is_background_scan_enabled(&nic->slow_ctx->scan)) {
    WLOG1_D("CID-%04x: BG scan is off - cannot start scanning", mtlk_vap_get_oid(nic->vap_handle));
    goto CANNOT_SCAN;
  }

  /* Although we won't start scan when the previous hasn't completed yet anyway
   * (scan module prohibits this) -
   * we need to return 0 in this case to indicate the scan
   * has been started successfully.
   * Otherwise wpa_supplicant will wait for scan completion for 3 seconds only
   * (which is not enough for us in the majority of cases to finish scan)
   * and iwlist will simply report error to the user.
   * If we return 0 - they start polling us for scan results, which is
   * an expected behavior.
   */
  res = MTLK_ERR_OK;
  if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
    ILOG1_D("CID-%04x: Scan in progress - cannot start scanning", mtlk_vap_get_oid(nic->vap_handle));
    goto CANNOT_SCAN;
  }
  if (mtlk_core_is_stopping(nic)) {
    WLOG1_D("CID-%04x: Core is being stopped - cannot start scanning", mtlk_vap_get_oid(nic->vap_handle));
    goto CANNOT_SCAN;
  }

  /* iwlist wlan0 scan <ESSID> */
  essid = mtlk_clpb_enum_get_next(clpb, NULL);

  /* Perform scanning */
  if (net_state == NET_STATE_CONNECTED)
    mtlk_scan_set_background(&nic->slow_ctx->scan, TRUE);
  else
    mtlk_scan_set_background(&nic->slow_ctx->scan, FALSE);
  res = mtlk_scan_async(&nic->slow_ctx->scan, mtlk_core_get_freq_band_cfg(nic), essid);

CANNOT_SCAN:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_scanning_res(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int         res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  bss_data_t  bss_found;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
    ILOG1_D("CID-%04x: Can't get scan results - scan in progress", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }
  if (mtlk_core_is_stopping(nic)) {
    WLOG1_D("CID-%04x: Core is being stopped - cannot start scanning", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_IN_USE;
    goto finish;
  }

  mtlk_cache_rewind(&nic->slow_ctx->cache);
  while (mtlk_cache_get_next_bss(&nic->slow_ctx->cache, &bss_found, NULL, NULL)) {
    res = mtlk_clpb_push(clpb, &bss_found, sizeof(bss_found));
    if (MTLK_ERR_OK != res) {
      goto err_push;
    }
  }

  goto finish;

err_push:
  mtlk_clpb_purge(clpb);
finish:
  return res;
}

static int
_mtlk_core_set_wep (struct nic *nic, int wep_enabled)
{
  int res = MTLK_ERR_NOT_SUPPORTED;

  if ((!wep_enabled && !nic->slow_ctx->wep_enabled) ||
      (wep_enabled && nic->slow_ctx->wep_enabled)) { /* WEB state is not changed */
    res = MTLK_ERR_OK;
    goto end;
  }

  if (wep_enabled && is_ht_net_mode(mtlk_core_get_network_mode_cfg(nic))) {
    if (mtlk_vap_is_ap(nic->vap_handle)) {
      ELOG_DS("CID-%04x: AP: Can't set WEP for HT Network Mode (%s)", mtlk_vap_get_oid(nic->vap_handle),
           net_mode_to_string(mtlk_core_get_network_mode_cfg(nic)));
      goto end;
    }
    else if (!is_mixed_net_mode(mtlk_core_get_network_mode_cfg(nic))) {
      ELOG_DS("CID-%04x: STA: Can't set WEP for HT-only Network Mode (%s)", mtlk_vap_get_oid(nic->vap_handle),
        net_mode_to_string(mtlk_core_get_network_mode_cfg(nic)));
      goto end;
    }
  }

  res = mtlk_set_mib_value_uint8(mtlk_vap_get_txmm(nic->vap_handle),
                                 MIB_PRIVACY_INVOKED,
                                 wep_enabled);
  if (res != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Failed to enable WEP encryption (err=%d)", mtlk_vap_get_oid(nic->vap_handle), res);
    goto end;
  }
  nic->slow_ctx->wep_enabled = wep_enabled;
  if (wep_enabled) {
    ILOG1_V("WEP encryption enabled");
  }else {
    ILOG1_V("WEP encryption disabled");
  }

end:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_set_wep_enc_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_ui_enc_cfg_t   *enc_cfg;
  uint32                    size;
  mtlk_txmm_t              *txmm = mtlk_vap_get_txmm(nic->vap_handle);
  IEEE_ADDR                 addr;
  uint8 tx_key;
  BOOL   key_updated = FALSE;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ((mtlk_core_get_net_state(nic) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1_D("CID-%04x: Invalid card state - request rejected", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }
  if (mtlk_core_scan_is_running(nic)) {
    ILOG1_D("CID-%04x: Can't set WEP configuration - scan in progress", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  enc_cfg = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == enc_cfg) || (sizeof(*enc_cfg) != size) ) {
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  if (!enc_cfg->wep_enabled && nic->slow_ctx->wep_enabled) {
    /* Disable WEP encryption */
    res = _mtlk_core_set_wep(nic, FALSE);
    if (MTLK_ERR_OK != res) {
      ELOG_D("CID-%04x: Failed to disable WEP encryption", mtlk_vap_get_oid(nic->vap_handle));
      goto finish;
    }

    res = mtlk_set_mib_value_uint8(txmm, MIB_AUTHENTICATION_PREFERENCE,
                                   MIB_AUTHENTICATION_OPEN_SYSTEM);
    if (MTLK_ERR_OK != res) {
      ELOG_D("CID-%04x: Failed to switch access policy to 'Open system'", mtlk_vap_get_oid(nic->vap_handle));
      goto finish;
    }
    ILOG1_V("Access policy switched to 'Open system'");
    goto finish;
  }

  res = mtlk_get_mib_value_uint8(txmm, MIB_WEP_DEFAULT_KEYID, &tx_key);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Unable to get WEP TX key index", mtlk_vap_get_oid(nic->vap_handle));
    goto finish;
  }

  if (0 <= enc_cfg->authentication) {
    res = mtlk_set_mib_value_uint8(txmm, MIB_AUTHENTICATION_PREFERENCE, enc_cfg->authentication);
    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: Failed to switch access policy to %i", mtlk_vap_get_oid(nic->vap_handle), enc_cfg->authentication);
      goto finish;
    }
    ILOG1_D("Access policy switched to %i", enc_cfg->authentication);
  }

  /* Validate and adjust key index
   *
   * Up to 4 WEP keys supported.
   * WE enumerate WEP keys from 1 to N, and 0 - is current TX key.
   */
  if (enc_cfg->update_current_key) {
    enc_cfg->key_id = tx_key;
  }

  if ((enc_cfg->wep_keys.sKey[enc_cfg->key_id].u8KeyLength == 0) &&
      (tx_key != enc_cfg->key_id)) {

    /* If WEP key not given - TX key index may be changed to requested */
    res = mtlk_set_mib_value_uint8(txmm, MIB_WEP_DEFAULT_KEYID, enc_cfg->key_id);
    if (MTLK_ERR_OK != res) {
      ELOG_D("CID-%04x: Unable to set WEP TX key index", mtlk_vap_get_oid(nic->vap_handle));
      goto finish;
    }
    nic->slow_ctx->default_key = enc_cfg->key_id;
    ILOG1_D("Set WEP TX key index to %i", enc_cfg->key_id);
    key_updated = TRUE;

  } else if (0 < enc_cfg->wep_keys.sKey[enc_cfg->key_id].u8KeyLength) {
    /* Set WEP key */
    MIB_WEP_DEF_KEYS wep_keys;

    memcpy(&wep_keys, &nic->slow_ctx->wep_keys, sizeof(wep_keys));

    wep_keys.sKey[enc_cfg->key_id].u8KeyLength =
        enc_cfg->wep_keys.sKey[enc_cfg->key_id].u8KeyLength;

    memcpy(wep_keys.sKey[enc_cfg->key_id].au8KeyData,
           enc_cfg->wep_keys.sKey[enc_cfg->key_id].au8KeyData,
           enc_cfg->wep_keys.sKey[enc_cfg->key_id].u8KeyLength);

    res = mtlk_set_mib_value_raw(txmm, MIB_WEP_DEFAULT_KEYS, (MIB_VALUE*)&wep_keys);
    if (res == MTLK_ERR_OK) {
      nic->slow_ctx->wep_keys = wep_keys;
      ILOG2_D("Successfully set WEP key #%i", enc_cfg->key_id);
      mtlk_dump(2, wep_keys.sKey[enc_cfg->key_id].au8KeyData, wep_keys.sKey[enc_cfg->key_id].u8KeyLength, "");
    } else {
      ELOG_D("CID-%04x: Failed to set WEP key", mtlk_vap_get_oid(nic->vap_handle));
      goto finish;
    }
  }

  res = _mtlk_core_set_wep(nic, enc_cfg->wep_enabled);
  if (MTLK_ERR_OK != res) {
    ELOG_V("Failed to set WEP to Core");
    goto finish;
  }

   /* Update RSN group key */
  memset(addr.au8Addr, 0xff, sizeof(addr.au8Addr));
  res = _mtlk_core_set_wep_key_blocked(nic, &addr);
  if (MTLK_ERR_OK != res) {
    ELOG_V("Failed to update group WEP key");
    goto finish;
  }

  if (key_updated) {
    res = mtlk_clpb_push(clpb, enc_cfg, sizeof(*enc_cfg));
    if (MTLK_ERR_OK != res) {
      mtlk_clpb_purge(clpb);
      goto finish;
    }
  }

finish:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_wep_enc_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_ui_enc_cfg_t    enc_cfg;
  mtlk_txmm_t               *txmm = mtlk_vap_get_txmm(nic->vap_handle);
  uint8                     tmp;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ((mtlk_core_get_net_state(nic) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1_D("CID-%04x: Invalid card state - request rejected", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }
  if (mtlk_core_scan_is_running(nic)) {
    ILOG1_D("CID-%04x: Can't get WEP configuration - scan in progress", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  memset(&enc_cfg, 0, sizeof(enc_cfg));

  enc_cfg.wep_enabled = nic->slow_ctx->wep_enabled;

  if (nic->slow_ctx->wep_enabled) {
    /* Report access policy */
    res = mtlk_get_mib_value_uint8(txmm, MIB_AUTHENTICATION_PREFERENCE, &tmp);
    if (res != MTLK_ERR_OK) {
      ELOG_D("CID-%04x: Failed to read WEP access policy", mtlk_vap_get_oid(nic->vap_handle));
      goto finish;
    }
    enc_cfg.authentication = tmp;

    enc_cfg.key_id = nic->slow_ctx->default_key;

    enc_cfg.wep_keys = nic->slow_ctx->wep_keys;

  }

  res = mtlk_clpb_push(clpb, &enc_cfg, sizeof(enc_cfg));
  if (MTLK_ERR_OK != res) {
    mtlk_clpb_purge(clpb);
  }

finish:
  return res;
}


int __MTLK_IFUNC
_mtlk_core_set_auth_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_ui_auth_cfg_t   *auth_cfg;
  uint32                    size;
  mtlk_txmm_t               *txmm = mtlk_vap_get_txmm(nic->vap_handle);

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ((mtlk_core_get_net_state(nic) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1_D("CID-%04x: Invalid card state - request rejected", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_is_stopping(nic)) {
    ILOG1_D("CID-%04x: Can't set AUTH configuration - core is stopping", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_scan_is_running(nic)) {
    ILOG1_D("CID-%04x: Can't set AUTH configuration - scan in progress", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  auth_cfg = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == auth_cfg) || (sizeof(*auth_cfg) != size) ) {
    ELOG_D("CID-%04x: Failed to get AUTH configuration parameters from CLPB", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  if (0 <= auth_cfg->rsn_enabled) {
    res = mtlk_set_mib_rsn(txmm, auth_cfg->rsn_enabled);
    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: Failed to switch RSN state to %i", mtlk_vap_get_oid(nic->vap_handle), auth_cfg->rsn_enabled);
      goto finish;
    }
    ILOG1_DD("CID-%04x: RSN switched to %i", mtlk_vap_get_oid(nic->vap_handle), auth_cfg->rsn_enabled);
  }

  if (0 <= auth_cfg->wep_enabled) {
    res = _mtlk_core_set_wep(nic, auth_cfg->wep_enabled);
    if (MTLK_ERR_OK != res) {
      goto finish;
    }
  }

  if (0 <= auth_cfg->authentication) {
    res = mtlk_set_mib_value_uint8(txmm, MIB_AUTHENTICATION_PREFERENCE, auth_cfg->authentication);
    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: Failed to switch access policy to %i", mtlk_vap_get_oid(nic->vap_handle), auth_cfg->authentication);
      goto finish;
    }
    ILOG1_DD("CID-%04x: Access policy switched to %i", mtlk_vap_get_oid(nic->vap_handle), auth_cfg->authentication);
  }

finish:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_auth_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_ui_auth_state_t auth_state;
  sta_entry                 *sta;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ((mtlk_core_get_net_state(nic) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1_D("CID-%04x: Invalid card state - request rejected", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_is_stopping(nic)) {
    ILOG1_D("CID-%04x: Can't set AUTH configuration - core is stopping", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_scan_is_running(nic)) {
    ILOG1_D("CID-%04x: Can't set AUTH configuration - scan in progress", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  memset(&auth_state, 0, sizeof(auth_state));

  auth_state.rsnie = nic->slow_ctx->rsnie;
  auth_state.group_cipher = nic->group_cipher;
  auth_state.cipher_pairwise = -1;

  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    sta = mtlk_stadb_get_ap(&nic->slow_ctx->stadb);
    if (!sta) {
      res = MTLK_ERR_PARAMS;
      goto finish;
    }

    auth_state.cipher_pairwise = mtlk_sta_get_cipher(sta);
    mtlk_sta_decref(sta); /* De-reference of get_ap */
  }

  res = mtlk_clpb_push(clpb, &auth_state, sizeof(auth_state));
  if (MTLK_ERR_OK != res) {
    mtlk_clpb_purge(clpb);
  }

finish:
  return res;
}

static void __MTLK_IFUNC
_mtlk_core_set_wps_in_progress(mtlk_core_t *core, uint8 wps_in_progress)
{
  core->slow_ctx->wps_in_progress = wps_in_progress;
  if (wps_in_progress)
    ILOG1_D("CID-%04x: WPS in progress", mtlk_vap_get_oid(core->vap_handle));
  else
    ILOG1_D("CID-%04x: WPS stopped", mtlk_vap_get_oid(core->vap_handle));
}


int __MTLK_IFUNC
_mtlk_core_set_genie_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_ui_genie_cfg_t  *genie_cfg;
  uint32                    size;
  mtlk_txmm_t               *txmm = mtlk_vap_get_txmm(core->vap_handle);

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ((mtlk_core_get_net_state(core) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1_D("CID-%04x:: Invalid card state - request rejected", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_is_stopping(core)) {
    ILOG1_D("CID-%04x: Can't set GEN_IE configuration - core is stopping", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_scan_is_running(core)) {
    ILOG1_D("CID-%04x: Can't set GEN_IE configuration - scan in progress", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  genie_cfg = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == genie_cfg) || (sizeof(*genie_cfg) != size) ) {
    ELOG_D("CID-%04x: Failed to get GEN_IE configuration parameters from CLPB", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(genie_cfg, wps_in_progress,
        _mtlk_core_set_wps_in_progress, (core, genie_cfg->wps_in_progress));

    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(genie_cfg, rsnie_reset,
        memset, (&core->slow_ctx->rsnie, 0, sizeof(core->slow_ctx->rsnie)));

    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(genie_cfg, rsnie,
        memcpy, (&core->slow_ctx->rsnie, &genie_cfg->rsnie, sizeof(core->slow_ctx->rsnie)));

    MTLK_CFG_CHECK_ITEM_AND_CALL(genie_cfg, gen_ie_set, mtlk_core_set_gen_ie,
       (core, genie_cfg->gen_ie, genie_cfg->gen_ie_len, genie_cfg->gen_ie_type), res);

    MTLK_CFG_CHECK_ITEM_AND_CALL(genie_cfg, rsn_enabled, mtlk_set_mib_rsn,
                                (txmm, genie_cfg->rsn_enabled), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

finish:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_enc_ext_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_txmm_t               *txmm = mtlk_vap_get_txmm(nic->vap_handle);
  mtlk_txmm_msg_t           man_msg;
  mtlk_txmm_data_t          *man_entry = NULL;
  UMI_GROUP_PN              *umi_gpn;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if (0 != (mtlk_core_get_net_state(nic) & (NET_STATE_HALTED | NET_STATE_IDLE))) {
    ILOG1_D("CID-%04x: Invalid card state - request rejected", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_scan_is_running(nic)) {
    ILOG1_D("CID-%04x: Can't get WEP configuration - scan in progress", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, txmm, NULL);
  if (NULL == man_entry) {
    ELOG_D("CID-%04x: No man entry available", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
    goto finish;
  }

  umi_gpn = (UMI_GROUP_PN*)man_entry->payload;
  memset(umi_gpn, 0, sizeof(UMI_GROUP_PN));

  man_entry->id           = UM_MAN_GET_GROUP_PN_REQ;
  man_entry->payload_size = sizeof(UMI_GROUP_PN);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Timeout expired while waiting for CFM from MAC", mtlk_vap_get_oid(nic->vap_handle));
    goto finish;
  }

  umi_gpn->u16Status = le16_to_cpu(umi_gpn->u16Status);
  if (UMI_OK != umi_gpn->u16Status) {
    ELOG_DD("CID-%04x: GET_GROUP_PN_REQ failed: %u", mtlk_vap_get_oid(nic->vap_handle), umi_gpn->u16Status);
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  res = mtlk_clpb_push(clpb, umi_gpn, sizeof(*umi_gpn));
  if (MTLK_ERR_OK != res) {
    mtlk_clpb_purge(clpb);
  }

finish:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static void
_mtlk_core_set_rx_seq(struct nic *nic, uint16 idx, uint8* rx_seq)
{
  nic->group_rsc[idx][0] = rx_seq[5];
  nic->group_rsc[idx][1] = rx_seq[4];
  nic->group_rsc[idx][2] = rx_seq[3];
  nic->group_rsc[idx][3] = rx_seq[2];
  nic->group_rsc[idx][4] = rx_seq[1];
  nic->group_rsc[idx][5] = rx_seq[0];
}


int __MTLK_IFUNC
_mtlk_core_set_enc_ext_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_txmm_msg_t           man_msg;
  mtlk_txmm_data_t          *man_entry = NULL;
  mtlk_txmm_t               *txmm = mtlk_vap_get_txmm(core->vap_handle);

  mtlk_core_ui_encext_cfg_t *encext_cfg;
  uint32                    size;
  UMI_SET_KEY               *umi_key;
  uint16                    alg_type = IW_ENCODE_ALG_NONE;
  uint16                    key_len = 0;
  sta_entry                 *sta = NULL;
  mtlk_pckt_filter_e        sta_filter_stored = MTLK_PCKT_FLTR_ALLOW_ALL;


  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ((mtlk_core_get_net_state(core) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1_D("CID-%04x: Invalid card state - request rejected", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_is_stopping(core)) {
    ILOG1_D("CID-%04x: Can't set ENC_EXT configuration - core is stopping", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_scan_is_running(core)) {
    ILOG1_D("CID-%04x: Can't set ENC_EXT configuration - scan in progress", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  encext_cfg = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == encext_cfg) || (sizeof(*encext_cfg) != size) ) {
    ELOG_D("CID-%04x: Failed to get ENC_EXT configuration parameters from CLPB", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  /* Prepare UMI message */
  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, txmm, NULL);
  if (!man_entry) {
    ELOG_D("CID-%04x: No man entry available", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
    goto finish;
  }

  umi_key = (UMI_SET_KEY*)man_entry->payload;
  memset(umi_key, 0, sizeof(*umi_key));

  man_entry->id           = UM_MAN_SET_KEY_REQ;
  man_entry->payload_size = sizeof(*umi_key);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_GET_ITEM(encext_cfg, alg_type, alg_type);

    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(encext_cfg, sta_addr, memcpy,
        (umi_key->sStationID.au8Addr, encext_cfg->sta_addr.au8Addr, sizeof(umi_key->sStationID.au8Addr)));

    MTLK_CFG_GET_ITEM(encext_cfg, key_idx, umi_key->u16DefaultKeyIndex);

    /* Set default key */
    MTLK_CFG_CHECK_ITEM_AND_CALL(encext_cfg, default_key_idx,
        mtlk_set_mib_value_uint8, (txmm, MIB_WEP_DEFAULT_KEYID, encext_cfg->default_key_idx), res);

    MTLK_CFG_GET_ITEM(encext_cfg, default_key_idx, core->slow_ctx->default_key);

    MTLK_CFG_GET_ITEM(encext_cfg, key_len, key_len);

    /* Set WEP key if needed */
    if (alg_type == IW_ENCODE_ALG_WEP ||
       (alg_type == IW_ENCODE_ALG_NONE && core->slow_ctx->wep_enabled))
          // IW_ENCODE_ALG_NONE - reset keys
    {
      /* Set WEP key */
      MIB_WEP_DEF_KEYS wep_keys;

      memcpy(&wep_keys, &core->slow_ctx->wep_keys, sizeof(wep_keys));

      wep_keys.sKey[umi_key->u16DefaultKeyIndex].u8KeyLength = key_len;

      memset(wep_keys.sKey[umi_key->u16DefaultKeyIndex].au8KeyData, 0,
             sizeof(wep_keys.sKey[umi_key->u16DefaultKeyIndex].u8KeyLength));

      if (0 < key_len) {
        MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(encext_cfg, key, memcpy,
            (wep_keys.sKey[umi_key->u16DefaultKeyIndex].au8KeyData, encext_cfg->key, key_len));
      }

      res = mtlk_set_mib_value_raw(txmm, MIB_WEP_DEFAULT_KEYS, (MIB_VALUE*)&wep_keys);
      if (res == MTLK_ERR_OK) {
        core->slow_ctx->wep_keys = wep_keys;
        ILOG1_D("Successfully set WEP key #%i", umi_key->u16DefaultKeyIndex);
        mtlk_dump(1, wep_keys.sKey[umi_key->u16DefaultKeyIndex].au8KeyData,
              wep_keys.sKey[umi_key->u16DefaultKeyIndex].u8KeyLength, "");
      } else {
        ELOG_D("CID-%04x: Failed to set WEP key", mtlk_vap_get_oid(core->vap_handle));
        goto finish;
      }
    }

    /* Enable disable WEP */
    res = _mtlk_core_set_wep(core, encext_cfg->wep_enabled);
    if (MTLK_ERR_OK != res) goto finish;

    /* Extract UNI key */
    /* The key has been copied into au8Tk1 array with UMI_RSN_TK1_LEN size.
     * But key can have UMI_RSN_TK1_LEN+UMI_RSN_TK2_LEN size - so
     * actually second part of key is copied into au8Tk2 array */
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(encext_cfg, key, memcpy,
              (umi_key->au8Tk1, encext_cfg->key, key_len));


    if (mtlk_osal_eth_is_broadcast(umi_key->sStationID.au8Addr))
    {
      umi_key->u16KeyType = cpu_to_le16(UMI_RSN_GROUP_KEY);
      core->group_cipher = alg_type;

      memset(core->group_rsc[umi_key->u16DefaultKeyIndex], 0, sizeof(core->group_rsc[0]));

      MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(encext_cfg, rx_seq,
          _mtlk_core_set_rx_seq, (core, umi_key->u16DefaultKeyIndex, encext_cfg->rx_seq));
    } else {
      umi_key->u16KeyType = cpu_to_le16(UMI_RSN_PAIRWISE_KEY);
    }

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  if (MTLK_ERR_OK != res) {
    goto finish;
  }

  switch (alg_type) {
  case IW_ENCODE_ALG_NONE:
    /* IW_ENCODE_ALG_NONE - reset keys */
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_NONE);
    break;
  case IW_ENCODE_ALG_WEP:
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_WEP40);
    break;
  case IW_ENCODE_ALG_TKIP:
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_TKIP);
    break;
  case IW_ENCODE_ALG_CCMP:
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_CCMP);
    break;
  }

  /* ant, 13 Apr 07: replay detection is performed by driver,
   * so MAC does not need this.
  if (ext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID)
    memcpy(umi_key->au8RxSeqNum, ext->rx_seq, ARRAY_SIZE(umi_key->au8RxSeqNum));
    */
  /* ant: DO NOT SET THIS: supplicant is doing the job for us
   * (the job of swapping 16 bytes of umi_key->au8Tk2 in TKIP)
   * (umi_key->au8Tk2 is used in TKIP only)
  if (mtlk_vap_is_ap(nic->vap_handle))
    umi_key->u16StationRole = cpu_to_le16(UMI_RSN_AUTHENTICATOR);
  else
    umi_key->u16StationRole = cpu_to_le16(UMI_RSN_SUPPLICANT);
    */

  if (cpu_to_le16(UMI_RSN_PAIRWISE_KEY) == umi_key->u16KeyType) {
    sta = mtlk_stadb_find_sta(&core->slow_ctx->stadb, umi_key->sStationID.au8Addr);
    if (NULL == sta) {
    /* Supplicant reset keys for AP from which we just were disconnected */
      ILOG1_Y("There is no connection with %Y", umi_key->sStationID.au8Addr);
      goto finish;
    }

    mtlk_sta_set_cipher(sta, alg_type);
    mtlk_sta_zero_rod_reply_counters(sta);

    if (0 == key_len) {
      mtlk_sta_set_packets_filter(sta, MTLK_PCKT_FLTR_ALLOW_802_1X);
      ILOG1_Y("%Y: turn on 802.1x filtering", mtlk_sta_get_addr(sta));

    } else /*key_len != 0*/ if (!mtlk_vap_is_ap(core->vap_handle)) {
      /* Don't set the key until msg4 is sent to MAC */
      while (!mtlk_sta_sq_is_empty(sta))
        msleep(100);
    }
  }

  umi_key->u16DefaultKeyIndex = cpu_to_le16(umi_key->u16DefaultKeyIndex);
  if (0 < key_len) {
    umi_key->au8TxSeqNum[0] = 1;
  }

  /* Set key/configure MAC */
  for (;;) {
    /* if WEP is configured then:
     * - new key should be set in MAC
     * - if deafult_key != new_key
     * ---- default key should be set in MAC*/
    uint16 status;
    /* Save UMI message */
    UMI_SET_KEY stored_umi_key = *(UMI_SET_KEY*)man_entry->payload;
    mtlk_txmm_data_t stored_man_entry = *man_entry;

    mtlk_dump(3, umi_key, sizeof(UMI_SET_KEY), "dump of UMI_SET_KEY msg:");

    if (NULL != sta) {
      sta_filter_stored  = mtlk_sta_get_packets_filter(sta);
      mtlk_sta_set_packets_filter(sta, MTLK_PCKT_FLTR_DISCARD_ALL);

      /* drop all packets in sta sendqueue */
      mtlk_sq_peer_ctx_cancel_all_packets(core->sq, mtlk_sta_get_sq(sta));

      /* wait till all messages to MAC to be confirmed */
      mtlk_sq_wait_all_packets_confirmed(mtlk_sta_get_sq(sta));
    }

    res = mtlk_txmm_msg_send_blocked(&man_msg,
                                     MTLK_MM_BLOCKED_SEND_TIMEOUT);

    if (NULL != sta) {
      /* restore previous state of sta packets filter */
      mtlk_sta_set_packets_filter(sta, sta_filter_stored);
    }

    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: mtlk_mm_send_blocked failed: %i", mtlk_vap_get_oid(core->vap_handle), res);
      goto finish;
    }

    status = le16_to_cpu(umi_key->u16Status);
    if (UMI_OK != status) {
      res = MTLK_ERR_UNKNOWN;
      switch (status) {
      case UMI_NOT_SUPPORTED:
        WLOG_D("CID-%04x: SIOCSIWENCODEEXT: RSN mode is disabled or an unsupported cipher suite was selected.", mtlk_vap_get_oid(core->vap_handle));
        res = MTLK_ERR_NOT_SUPPORTED;
        break;
      case UMI_STATION_UNKNOWN:
        WLOG_D("CID-%04x: SIOCSIWENCODEEXT: Unknown station was selected.", mtlk_vap_get_oid(core->vap_handle));
        break;
      default:
        WLOG_DDD("CID-%04x: invalid status of last msg %04x sending to MAC - %i", mtlk_vap_get_oid(core->vap_handle),
            man_entry->id, status);
      }
      goto finish;
    }

    if ((IW_ENCODE_ALG_WEP == alg_type) &&
        (umi_key->u16DefaultKeyIndex != cpu_to_le16(core->slow_ctx->default_key))) {

      ILOG1_D("reset tx key according to default key idx %i", core->slow_ctx->default_key);
      /* restore UMI message*/
      *(UMI_SET_KEY*)man_entry->payload = stored_umi_key;
      *man_entry = stored_man_entry;

      /* update UMI message*/
      umi_key->u16DefaultKeyIndex = cpu_to_le16(core->slow_ctx->default_key);
      memcpy(umi_key->au8Tk1,
          core->slow_ctx->wep_keys.sKey[core->slow_ctx->default_key].au8KeyData,
          core->slow_ctx->wep_keys.sKey[core->slow_ctx->default_key].u8KeyLength);
      /*Send UMI message*/
      continue;
    }
    break;
  }

  if (NULL != sta) {
    /* Now we have already set the keys =>
       we can start ADDBA and disable filter if required */
    mtlk_sta_on_security_negotiated(sta);

    if (key_len) {
      mtlk_sta_set_packets_filter(sta, MTLK_PCKT_FLTR_ALLOW_ALL);
      ILOG1_Y("%Y: turn off 802.1x filtering", mtlk_sta_get_addr(sta));
    }
  }

finish:
  if (NULL != sta) {
    mtlk_sta_decref(sta); /* De-reference of find */
  }

  if (NULL != man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

static int
_mtlk_core_mac_get_channel_stats(mtlk_core_t *nic, mtlk_core_general_stats_t *general_stats)
{
  int                       res = MTLK_ERR_OK;
  mtlk_txmm_msg_t           man_msg;
  mtlk_txmm_data_t          *man_entry = NULL;
  UMI_GET_CHANNEL_STATUS    *pchannel_stats= NULL;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(general_stats != NULL);

  if (NET_STATE_HALTED == mtlk_core_get_net_state(nic)) { /* Do nothing if halted */
    goto finish;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), NULL);
  if (NULL == man_entry) {
    ELOG_D("CID-%04x: No man entry available", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
    goto finish;
  }

  man_entry->id = UM_MAN_GET_CHANNEL_STATUS_REQ;
  man_entry->payload_size = sizeof(UMI_GET_CHANNEL_STATUS);

  pchannel_stats = (UMI_GET_CHANNEL_STATUS *)man_entry->payload;
  memset(pchannel_stats, 0, sizeof(UMI_GET_CHANNEL_STATUS));

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: MAC Get Channel Status sending failure (%i)", mtlk_vap_get_oid(nic->vap_handle), res);
    goto finish;
  }

  general_stats->noise = pchannel_stats->u8GlobalNoise;
  general_stats->channel_load = pchannel_stats->u8ChannelLoad;

finish:
  if (NULL != man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}


static int
_mtlk_core_mac_get_peers_stats(mtlk_core_t *nic, mtlk_core_general_stats_t *general_stats)
{
  int                       res = MTLK_ERR_OK;
  mtlk_txmm_msg_t           man_msg;
  mtlk_txmm_data_t          *man_entry = NULL;
  UMI_GET_PEERS_STATUS      *ppeers_stats= NULL;
  uint8                     device_index = 0;
  uint8                     idx;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(general_stats != NULL);

  if (NET_STATE_HALTED == mtlk_core_get_net_state(nic)) { /* Do nothing if halted */
    goto finish;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), NULL);
  if (NULL == man_entry) {
    ELOG_D("CID-%04x: No man entry available", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
    goto finish;
  }

  do {
    man_entry->id = UM_MAN_GET_PEERS_STATUS_REQ;
    man_entry->payload_size = sizeof(UMI_GET_PEERS_STATUS);

    ppeers_stats = (UMI_GET_PEERS_STATUS *)man_entry->payload;
    memset(ppeers_stats, 0, sizeof(UMI_GET_PEERS_STATUS));
    ppeers_stats->u8DeviceIndex = device_index;

    res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: MAC Get Peers Status sending failure (%i)", mtlk_vap_get_oid(nic->vap_handle), res);
      break;
    }

    for (idx = 0; idx < ppeers_stats->u8NumOfDeviceStatus; idx++) {
      sta_entry *sta = mtlk_stadb_find_sta(
                            &nic->slow_ctx->stadb,
                            (unsigned char*)&ppeers_stats->sDeviceStatus[idx].sMacAdd);

      if (sta != NULL) {
        mtlk_sta_update_fw_related_info(sta, &ppeers_stats->sDeviceStatus[idx]);
        mtlk_sta_decref(sta); /* De-reference of find */
      }
    }

    device_index = ppeers_stats->u8DeviceIndex;

  } while (0 != device_index);

finish:
  if (NULL != man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static int
_mtlk_core_mac_get_stats(mtlk_core_t *nic, mtlk_core_general_stats_t *general_stats)
{
  int                       res = MTLK_ERR_OK;
  mtlk_txmm_msg_t           man_msg;
  mtlk_txmm_data_t          *man_entry = NULL;
  UMI_GET_STATISTICS        *mac_stats;
  int                       idx;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(general_stats != NULL);

  if (NET_STATE_HALTED == mtlk_core_get_net_state(nic)) { /* Do nothing if halted */
    goto finish;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txdm(nic->vap_handle), NULL);
  if (!man_entry) {
    ELOG_D("CID-%04x: Can't get statistics due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
    goto finish;
  }

  mac_stats = (UMI_GET_STATISTICS *)man_entry->payload;

  man_entry->id           = UM_DBG_GET_STATISTICS_REQ;
  man_entry->payload_size = sizeof(*mac_stats);
  mac_stats->u16Status       = 0;
  mac_stats->u16Ident        = HOST_TO_MAC16(GET_ALL_STATS);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: MAC Get Stat sending failure (%i)", mtlk_vap_get_oid(nic->vap_handle), res);
  } else if (UMI_OK != le16_to_cpu(mac_stats->u16Status)) {
    ELOG_DD("CID-%04x: MAC Get Stat failure (%u)", mtlk_vap_get_oid(nic->vap_handle), le16_to_cpu(mac_stats->u16Status));
    res = MTLK_ERR_MAC;
  } else {
    for (idx = 0; idx < STAT_TOTAL_NUMBER; idx++) {
      general_stats->mac_stat.stat[idx] = le32_to_cpu(mac_stats->sStats.au32Statistics[idx]);
    }
  }

finish:
  if (NULL != man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}


int
_mtlk_core_get_status(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_general_stats_t general_stats;
  mtlk_txmm_stats_t         txm_stats;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&general_stats, 0, sizeof(general_stats));

  if(mtlk_vap_is_master(nic->vap_handle)) {
    /* Fill Core private statistic fields*/
    general_stats.core_priv_stats = nic->pstats;
    general_stats.tx_packets = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_PACKETS_SENT);
    general_stats.tx_bytes = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_BYTES_SENT);
    general_stats.rx_packets = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_PACKETS_RECEIVED);
    general_stats.rx_bytes = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_BYTES_RECEIVED);
    general_stats.unicast_replayed_packets = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_UNICAST_REPLAYED_PACKETS);
    general_stats.multicast_replayed_packets = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_MULTICAST_REPLAYED_PACKETS);
    general_stats.fwd_rx_packets = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_FWD_RX_PACKETS);
    general_stats.fwd_rx_bytes = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_FWD_RX_BYTES);
    general_stats.rx_dat_frames = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_DAT_FRAMES_RECEIVED);
    general_stats.rx_ctl_frames = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_CTL_FRAMES_RECEIVED);
    general_stats.rx_man_frames = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_MAN_FRAMES_RECEIVED);

    /* HW status fields */
    mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_FREE_TX_MSGS,
        &general_stats.tx_msdus_free, sizeof(general_stats.tx_msdus_free));
    mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_TX_MSGS_USED_PEAK,
        &general_stats.tx_msdus_usage_peak, sizeof(general_stats.tx_msdus_usage_peak));
    mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_BIST,
        &general_stats.bist_check_passed, sizeof(general_stats.bist_check_passed));
    mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_FW_BUFFERS_PROCESSED,
      &general_stats.fw_logger_packets_processed, sizeof(general_stats.fw_logger_packets_processed));
    mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_FW_BUFFERS_DROPPED,
      &general_stats.fw_logger_packets_dropped, sizeof(general_stats.fw_logger_packets_dropped));

    mtlk_txmm_get_stats(mtlk_vap_get_txmm(nic->vap_handle), &txm_stats);
    general_stats.txmm_sent = txm_stats.nof_sent;
    general_stats.txmm_cfmd = txm_stats.nof_cfmed;
    general_stats.txmm_peak = txm_stats.used_peak;

    mtlk_txmm_get_stats(mtlk_vap_get_txdm(nic->vap_handle), &txm_stats);
    general_stats.txdm_sent = txm_stats.nof_sent;
    general_stats.txdm_cfmd = txm_stats.nof_cfmed;
    general_stats.txdm_peak = txm_stats.used_peak;

    /* Get MAC statistic */
    res = _mtlk_core_mac_get_stats(nic, &general_stats);
    if (MTLK_ERR_OK != res) {
      goto finish;
    }

    /* Get MAC channel information for master only*/
    res = _mtlk_core_mac_get_channel_stats(nic, &general_stats);
    if (MTLK_ERR_OK != res) {
      goto finish;
    }

    /* Fill core status fields */
    general_stats.net_state = mtlk_core_get_net_state(nic);

    mtlk_pdb_get_mac(
        mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID, general_stats.bssid);

    if (!mtlk_vap_is_ap(nic->vap_handle) && (mtlk_core_get_net_state(nic) == NET_STATE_CONNECTED)) {
      sta_entry *sta = mtlk_stadb_get_ap(&nic->slow_ctx->stadb);

      if (NULL != sta) {
        general_stats.max_rssi = mtlk_sta_get_max_rssi(sta);

        mtlk_sta_decref(sta);  /* De-reference of get_ap */
      }
    }
  }

  /* Get MAC peers information */
  res = _mtlk_core_mac_get_peers_stats(nic, &general_stats);
  if (MTLK_ERR_OK != res) {
    goto finish;
  }

  /* Return Core status & statistic data */
  res = mtlk_clpb_push(clpb, &general_stats, sizeof(general_stats));
  if (MTLK_ERR_OK != res) {
    mtlk_clpb_purge(clpb);
  }

finish:
  return res;
}


int
mtlk_core_set_bonding(mtlk_core_t *core, uint8 bonding)
{
  int result = MTLK_ERR_OK;

  if ((bonding == ALTERNATE_UPPER) || (bonding == ALTERNATE_LOWER))
  {
    ILOG3_S("Bonding changed to %s", bonding == ALTERNATE_UPPER ? "upper" : "lower");
    core->slow_ctx->bonding = core->slow_ctx->pm_params.u8UpperLowerChannel = bonding;
  } else {
    result = MTLK_ERR_PARAMS;
  }

  return result;
}

uint8 __MTLK_IFUNC
mtlk_core_get_bonding(mtlk_core_t *core)
{
  return core->slow_ctx->bonding;
}

BOOL __MTLK_IFUNC
mtlk_core_net_state_is_connected(uint16 net_state)
{
  return ((NET_STATE_CONNECTED == net_state) ? TRUE:FALSE);
}

uint8 __MTLK_IFUNC
mtlk_core_get_country_code(mtlk_core_t *core)
{
  return MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_COUNTRY_CODE);
}

static void
_mtlk_core_country_code_set_default(mtlk_core_t* core)
{
  uint8  country_code;

  if (mtlk_vap_is_master_ap(core->vap_handle)) {
    country_code = mtlk_eeprom_get_country_code(mtlk_core_get_eeprom(core));
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_COUNTRY_CODE, country_code);
    ILOG1_DSD("CID-%04x: Country is set to (on init): %s(0x%02x)",
        mtlk_vap_get_oid(core->vap_handle), country_code_to_country(country_code), country_code);
  }
  /* - Do nothing for slave VAP*/
  /* - Do nothing for STA (default value - 0)*/
}

static void
_mtlk_core_sta_country_code_set_default_on_activate(mtlk_core_t* core)
{
  uint8  country_code = mtlk_core_get_country_code(core);
  if (country_code == 0) {
     /*TODO: GS: Hide country_code processing */
    /* we must set up at least something */
    country_code = mtlk_eeprom_get_country_code(mtlk_core_get_eeprom(core));
    if (0 == country_code) {
      MTLK_CORE_PDB_SET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_COUNTRY_CODE, country_to_country_code("US"));
      WLOG_DSD("CID-%04x: Country isn't set. Set it to default (on activate): %s(0x%02x)",
          mtlk_vap_get_oid(core->vap_handle), "US", country_to_country_code("US"));
    }
  }
}

static void
_mtlk_core_sta_country_code_update_on_connect(mtlk_core_t* core, uint8 country_code)
{
  if (country_code) {
    MTLK_CORE_PDB_SET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_COUNTRY_CODE, country_code);
    ILOG1_DSD("CID-%04x: Country has been adopted (on connect): %s(0x%02x)",
        mtlk_vap_get_oid(core->vap_handle), country_code_to_country(country_code), country_code);
  }
}

void __MTLK_IFUNC
mtlk_core_sta_country_code_update_from_bss(mtlk_core_t* core, uint8 country_code)
{
  if ( mtlk_core_get_dot11d(core) /*802.11d mode on*/ &&
      (0 != country_code) /*802.11d becon with country IE recieved */ &&
      (0 == mtlk_core_get_country_code(core)) /*Country hasn't been set yet*/) {

    MTLK_CORE_PDB_SET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_COUNTRY_CODE, country_code);
    ILOG1_DSD("CID-%04x: Country has been adopted (from bss): %s(0x%02x)",
        mtlk_vap_get_oid(core->vap_handle), country_code_to_country(country_code), country_code);

    mtlk_scan_schedule_rescan(&mtlk_core_get_master(core)->slow_ctx->scan);
  }
}

static int
_mtlk_core_set_country_from_ui(mtlk_core_t *core, char *val)
{
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  if (mtlk_core_get_net_state(core) != NET_STATE_READY) {
    return MTLK_ERR_BUSY;
  }

  if (mtlk_vap_is_ap(core->vap_handle) && mtlk_eeprom_get_country_code(mtlk_core_get_eeprom(core))) {
    ILOG1_D("CID-%04x: Can't change Country. It's read-only parameter.", mtlk_vap_get_oid(core->vap_handle));
    ILOG1_DSD("CID-%04x: Current Country value: %s(0x%02x)",
        mtlk_vap_get_oid(core->vap_handle), country_code_to_country(mtlk_core_get_country_code(core)), mtlk_core_get_country_code(core));
    return MTLK_ERR_NOT_SUPPORTED;
  }

  if (!mtlk_vap_is_ap(core->vap_handle) && mtlk_core_get_dot11d(core)) {
    ILOG1_D("CID-%04x: Can't change Country until 802.11d extension is enabled.", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_NOT_SUPPORTED;
  }

  if (strncmp(val, "??", 2) && country_to_domain(val) == 0) {
    return MTLK_ERR_VALUE;
  }

  ILOG1_DSD("CID-%04x: Country is set to (from ui): %s(0x%02x)",
      mtlk_vap_get_oid(core->vap_handle), val, country_to_country_code(val));

  MTLK_CORE_PDB_SET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_COUNTRY_CODE, country_to_country_code(val));
  return MTLK_ERR_OK;
}

BOOL __MTLK_IFUNC mtlk_core_get_dot11d(mtlk_core_t *core)
{
  return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_DOT11D_ENABLED);
}

static int
_mtlk_core_set_is_dot11d(mtlk_core_t *core, BOOL is_dot11d)
{
  uint8  country_code = 0;

  if (NET_STATE_READY != mtlk_core_get_net_state(core)) {
    return MTLK_ERR_NOT_READY;
  }

  /* set country code */
  if (!mtlk_vap_is_ap(core->vap_handle)) {
    /* Switched on */
    if (is_dot11d && !mtlk_core_get_dot11d(core)) {
      country_code = 0;
    }
    /* Switched off */
    else if(!is_dot11d && mtlk_core_get_dot11d(core)) {
      country_code = mtlk_eeprom_get_country_code(mtlk_core_get_eeprom(core));
      if (0 == country_code) {

        country_code = country_to_country_code("US");
      }
    }

    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_COUNTRY_CODE, country_code);
  }

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_DOT11D_ENABLED, !!is_dot11d);

  return MTLK_ERR_OK;
}

uint8
mtlk_core_get_freq_band_cur(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_FREQ_BAND_CUR);
}

uint8
mtlk_core_get_freq_band_cfg(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_FREQ_BAND_CFG);
}

uint8
mtlk_core_get_network_mode_cur(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_NET_MODE_CUR);
}

uint8
mtlk_core_get_network_mode_cfg(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_NET_MODE_CFG);
}

void __MTLK_IFUNC
mtlk_core_notify_scan_complete(mtlk_vap_handle_t vap_handle)
{
  mtlk_df_ui_notify_scan_complete(mtlk_vap_get_df(vap_handle));
}

uint8
mtlk_core_get_is_ht_cur(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_IS_HT_CUR);
}

uint8
mtlk_core_get_is_ht_cfg(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_IS_HT_CFG);
}

/* 20/40 state machine */
static mtlk_txmm_clb_action_e _mtlk_channel_switched_notification_callback(mtlk_handle_t clb_usr_data,
                                                                           mtlk_txmm_data_t*      data, 
                                                                           mtlk_txmm_clb_reason_e reason)
{
  MTLK_UNREFERENCED_PARAM(clb_usr_data);
  MTLK_UNREFERENCED_PARAM(data);
  MTLK_UNREFERENCED_PARAM(reason);
  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e _mtlk_send_cmf_notification_callback(mtlk_handle_t clb_usr_data,
                                                                   mtlk_txmm_data_t*      data, 
                                                                   mtlk_txmm_clb_reason_e reason)
{
  MTLK_UNREFERENCED_PARAM(clb_usr_data);
  MTLK_UNREFERENCED_PARAM(data);
  MTLK_UNREFERENCED_PARAM(reason);
  return MTLK_TXMM_CLBA_FREE;
}


static void _mtlk_core_switch_cb_mode_stage1_callback(mtlk_handle_t context, mtlk_get_channel_data_t *channel_data, FREQUENCY_ELEMENT *mode_change_params)
{
  mtlk_core_t *core;
  uint8 secondary_channel_offset;

  core = (mtlk_core_t*)context;

  channel_data->channel = mode_change_params->u16Channel;
  /* u16Channel field of mode_change_params is not yet converted to MAC endianness */
  secondary_channel_offset = (mode_change_params->u8SwitchMode & UMI_CHANNEL_SW_MODE_SC_MASK);
  channel_data->reg_domain = country_code_to_domain(mtlk_core_get_country_code(core));
  ILOG2_DDD("\n"
            "Primary channel = %d\n"
            "Secondary channel offset = %d\n"
            "Regulatory domain = %d\n",
            channel_data->channel,
            secondary_channel_offset,
            channel_data->reg_domain);

  channel_data->is_ht = mtlk_core_get_is_ht_cur(core);
  channel_data->ap = mtlk_vap_is_ap(core->vap_handle);
  if (secondary_channel_offset == UMI_CHANNEL_SW_MODE_SCN)
  {
    channel_data->spectrum_mode = SPECTRUM_20MHZ;
    channel_data->bonding = ALTERNATE_NONE;
  }
  else
  {
    channel_data->spectrum_mode = SPECTRUM_40MHZ;
    if (secondary_channel_offset == UMI_CHANNEL_SW_MODE_SCA)
    {
      channel_data->bonding = ALTERNATE_UPPER;
    }
    else
    {
      channel_data->bonding = ALTERNATE_LOWER;
    }
  }
  channel_data->frequency_band = mtlk_core_get_freq_band_cfg(core);
  channel_data->disable_sm_channels = mtlk_eeprom_get_disable_sm_channels(mtlk_core_get_eeprom(core));
}

static void _mtlk_core_switch_cb_mode_stage2_callback(mtlk_handle_t context, FREQUENCY_ELEMENT *mode_change_params)
{
  mtlk_core_t *core;
  mtlk_txmm_t *txmm;
  mtlk_txmm_data_t* tx_data;
  uint8 channel = (uint8)MAC_TO_HOST16(mode_change_params->u16Channel);
  /* u16Channel of mode_change_params is already converted to MAC endianness */

  core = (mtlk_core_t*)context;
  mode_change_params->u32SwitchType = HOST_TO_MAC32(0); //Normal
  mode_change_params->i16CbTransmitPowerLimit = HOST_TO_MAC16(mtlk_calc_tx_power_lim_wrapper(HANDLE_T(core),1, channel)); 
  mode_change_params->i16nCbTransmitPowerLimit = HOST_TO_MAC16(mtlk_calc_tx_power_lim_wrapper(HANDLE_T(core),1, channel));
  mode_change_params->i16AntennaGain = HOST_TO_MAC16(mtlk_get_antenna_gain_wrapper(HANDLE_T(core), channel));
  mode_change_params->u16ChannelLoad = HOST_TO_MAC16(0);

  ILOG0_DDD("\nCurrent Primary Channel = %d\n"
    "About to switch channel with following parameters:\n"
    "Primary Channel = %d\n"
    "Secondary Channel offset = %d\n",
    _mtlk_core_get_channel(core),
    MAC_TO_HOST16(mode_change_params->u16Channel),
    (mode_change_params->u8SwitchMode & UMI_CHANNEL_SW_MODE_SC_MASK));
  ILOG2_DDDDDDDDDD("ChannelAvailabilityCheckTime = %d\n"
    "ScanType = %d\n"
    "ChannelSwitchCount = %d\n"
    "SwitchMode = %x\n"
    "SmRequired = %d\n"                 
    "CbTransmitPowerLimit = %d\n"
    "nCbTransmitPowerLimit = %d\n"
    "AntennaGain = %d\n"
    "ChannelLoad = %d\n"
    "SwitchType = %d\n",
    MAC_TO_HOST16(mode_change_params->u16ChannelAvailabilityCheckTime),
    mode_change_params->u8ScanType,                    
    mode_change_params->u8ChannelSwitchCount,          
    mode_change_params->u8SwitchMode,                 
    mode_change_params->u8SmRequired,                 
    MAC_TO_HOST16(mode_change_params->i16CbTransmitPowerLimit),
    MAC_TO_HOST16(mode_change_params->i16nCbTransmitPowerLimit),
    MAC_TO_HOST16(mode_change_params->i16AntennaGain),
    MAC_TO_HOST16(mode_change_params->u16ChannelLoad),               
    MAC_TO_HOST32(mode_change_params->u32SwitchType));
  txmm = mtlk_vap_get_txmm(core->vap_handle);
  tx_data = mtlk_txmm_msg_get_empty_data(&core->aux_20_40_msg, txmm);
  if (tx_data != NULL) {
    memcpy(tx_data->payload, mode_change_params, sizeof(*mode_change_params));
    tx_data->id           = UM_SET_CHAN_REQ;
    tx_data->payload_size = sizeof(*mode_change_params);
    if (mtlk_txmm_msg_send(&core->aux_20_40_msg, 
                       _mtlk_channel_switched_notification_callback,
                       context,
                       TXMM_DEFAULT_TIMEOUT) == MTLK_ERR_OK) 
    {
      uint8 secondary_channel_offset = (mode_change_params->u8SwitchMode & UMI_CHANNEL_SW_MODE_SC_MASK);
      uint8 new_spectrum_mode;
      uint8 new_bonding;
      
      
      ILOG2_V("Channel switch message sent to FW ok");
      mtlk_core_abilities_disable_vap_ops(core->vap_handle);
      if (secondary_channel_offset == UMI_CHANNEL_SW_MODE_SCN) {
        new_spectrum_mode = SPECTRUM_20MHZ;
        new_bonding = ALTERNATE_NONE;
      }
      else {
        new_spectrum_mode = SPECTRUM_40MHZ;
        if (secondary_channel_offset == UMI_CHANNEL_SW_MODE_SCA) {
          new_bonding = ALTERNATE_UPPER;
        }
        else {
          new_bonding = ALTERNATE_LOWER;
        }
      }
      MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, new_spectrum_mode);
      mtlk_aocs_set_spectrum_mode(core->slow_ctx->aocs, new_spectrum_mode);
      core->slow_ctx->pm_params.u8UpperLowerChannel = new_bonding;
      mtlk_core_set_bonding(core, new_bonding);
      _mtlk_core_set_channel(core, MAC_TO_HOST16(mode_change_params->u16Channel));
    }
    else {
      ELOG_V("UM_SET_CHAN_REQ sending failed");
    }
  }
  else {
    ELOG_V("Can't get MAN entry to send UM_SET_CHAN_REQ");
  }
}

static void _mtlk_core_send_ce_callback(mtlk_handle_t context, UMI_COEX_EL *coexistence_element)
{
  ILOG2_V("called");
}

static void _mtlk_core_send_cmf_callback(mtlk_handle_t context, const IEEE_ADDR *sta_addr, const UMI_COEX_FRAME *coexistence_frame)
{
  mtlk_core_t *core;
  mtlk_txmm_t *txmm;
  mtlk_txmm_data_t* tx_data;

  ILOG2_V("called");
  core = (mtlk_core_t*)context;
  txmm = mtlk_vap_get_txmm(core->vap_handle);
  tx_data = mtlk_txmm_msg_get_empty_data(&core->aux_20_40_msg, txmm);
  if (tx_data != NULL) {
    memcpy(tx_data->payload, coexistence_frame, sizeof(*coexistence_frame));
    tx_data->id           = UM_MAN_SEND_COEX_FRAME_REQ;
    tx_data->payload_size = sizeof(*coexistence_frame);
    if (mtlk_txmm_msg_send(&core->aux_20_40_msg, 
      _mtlk_send_cmf_notification_callback,
      context,
      TXMM_DEFAULT_TIMEOUT) != MTLK_ERR_OK) {
        ILOG2_V("Error sending coexistence management frame");
    }
  }
}

static int _mtlk_core_scan_async_callback(mtlk_handle_t context, uint8 band, const char* essid)
{
  ILOG2_V("called");
  return MTLK_ERR_OK;
}

static void _mtlk_core_scan_set_bg_callback(mtlk_handle_t context, BOOL is_background)
{
  ILOG2_V("called");
}

static int _mtlk_core_scan_register_obss_cb_callback(mtlk_handle_t context,
  obss_scan_report_callback_type *callback)
{
  ILOG2_V("called");
  return MTLK_ERR_OK;
}

static int _mtlk_core_enumerate_external_intolerance_info_callback(mtlk_handle_t caller_context,
  mtlk_handle_t core_context, external_intolerance_enumerator_callback_type callback, uint32 expiration_time)
{
  bss_data_t bss_data;
  mtlk_core_t *core;
  uint32 original_cache_expire_time;
  int i = 0;

  ILOG2_V("called");
  core = (mtlk_core_t*)core_context;
  original_cache_expire_time = mtlk_cache_get_expiration_time(&core->slow_ctx->cache);
  if (original_cache_expire_time < expiration_time)
  {
    mtlk_cache_set_expiration_time(&core->slow_ctx->cache, expiration_time);
  }
  mtlk_cache_rewind(&core->slow_ctx->cache);
  ILOG2_V("Accessing cache data .........");
  /* update channels array from cache (received bss's) */
  while (mtlk_cache_get_next_bss(&core->slow_ctx->cache, &bss_data, NULL, NULL))
  {
    mtlk_20_40_external_intolerance_info_t intolerance_info;
    ILOG2_D("Cache data No. %d:", i);
    ILOG2_D("Channel = %d", bss_data.channel);
    ILOG2_D("Sec.channel offset = %d", bss_data.secondary_channel_offset);
    ILOG2_D("Spectrum = %d", bss_data.spectrum);
    ILOG2_D("Is HT = %d", bss_data.is_ht);
    ILOG2_D("40MHz Intolerant = %d",bss_data.forty_mhz_intolerant);
    
    ++i;
    intolerance_info.channel = bss_data.channel;
    intolerance_info.secondary_channel_offset = bss_data.secondary_channel_offset;
    intolerance_info.timestamp = bss_data.received_timestamp;
    intolerance_info.is_ht = bss_data.is_ht;
    intolerance_info.forty_mhz_intolerant = bss_data.forty_mhz_intolerant;
    (*callback)(caller_context, &intolerance_info);
  }
  if(i == 0)
    ILOG2_V("Processing cache data ended with no results - cache is empty!");
  /* restore cache expire time  */
  if (original_cache_expire_time < expiration_time)
  {
    mtlk_cache_set_expiration_time(&core->slow_ctx->cache, original_cache_expire_time);
  }
  return MTLK_ERR_OK;
}

static int _mtlk_core_ability_control_callback(mtlk_handle_t context,
  eABILITY_OPS operation, const uint32* ab_id_list, uint32 ab_id_num)
{
  mtlk_core_t *core;
  int ret_val = MTLK_ERR_OK;

  ILOG2_V("called");

  core = (mtlk_core_t*)context;
  switch (operation)
  {
    case eAO_REGISTER:
      ret_val = mtlk_abmgr_register_ability_set(mtlk_vap_get_abmgr(core->vap_handle), (mtlk_ability_id_t*)ab_id_list, ab_id_num);
      break;
    case eAO_UNREGISTER:
      mtlk_abmgr_unregister_ability_set(mtlk_vap_get_abmgr(core->vap_handle), (mtlk_ability_id_t*)ab_id_list, ab_id_num);
      break;
    case eAO_ENABLE:
      mtlk_abmgr_enable_ability_set(mtlk_vap_get_abmgr(core->vap_handle), (mtlk_ability_id_t*)ab_id_list, ab_id_num);
      break;
    case eAO_DISABLE:
      mtlk_abmgr_disable_ability_set(mtlk_vap_get_abmgr(core->vap_handle), (mtlk_ability_id_t*)ab_id_list, ab_id_num);
      break;
  }

  return ret_val;
}

static uint8 _mtlk_core_get_reg_domain_callback(mtlk_handle_t context)
{
  mtlk_core_t *core;

  ILOG2_V("called");

  core = (mtlk_core_t*)context;
  return country_code_to_domain(mtlk_core_get_country_code(core));
}

static uint16 _mtlk_core_get_cur_channels_callback(mtlk_handle_t context, int *secondary_channel_offset)
{
  mtlk_core_t *core;

  ILOG2_V("called");

  core = (mtlk_core_t*)context;
  if (mtlk_aocs_get_spectrum_mode(core->slow_ctx->aocs))
  {
    if (mtlk_core_get_bonding(core) == ALTERNATE_UPPER)
    {
      *secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCA;
    }
    else
    {
      *secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCB;
    }
  }
  else
  {
    *secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCN;
  }

  return _mtlk_core_get_channel(core);
}

int __MTLK_IFUNC mtlk_core_on_channel_switch_done(mtlk_vap_handle_t vap_handle,
                                                  uint16            primary_channel,
                                                  uint8             secondary_channel_offset,
                                                  uint16            reason)
{
  mtlk_channel_switched_event_t switch_data;

  memset(&switch_data, 0, sizeof(mtlk_channel_switched_event_t));

  switch_data.primary_channel   = primary_channel;
  switch_data.secondary_channel = mtlk_channels_get_secondary_channel_no_by_offset(primary_channel, secondary_channel_offset);

  switch(reason)
  {
  case SWR_LOW_THROUGHPUT:
  case SWR_HIGH_SQ_LOAD:
  case SWR_CHANNEL_LOAD_CHANGED:
  case SWR_MAC_PRESSURE_TEST:
    switch_data.reason = WSSA_SWR_OPTIMIZATION;
    break;
  case SWR_RADAR_DETECTED:
    switch_data.reason = WSSA_SWR_RADAR;
    break;
  case SWR_INITIAL_SELECTION:
    switch_data.reason = WSSA_SWR_USER;
    break;
  case SWR_AP_SWITCHED:
    switch_data.reason = WSSA_SWR_AP_SWITCHED;
    break;
  case SWR_UNKNOWN:
  default:
    switch_data.reason = WSSA_SWR_UNKNOWN;
    break;
  }

  return mtlk_wssd_send_event(mtlk_vap_get_irbd(vap_handle),
                              MTLK_WSSA_DRV_EVENT_CH_SWITCH,
                              &switch_data,
                              sizeof(mtlk_channel_switched_event_t));
}
