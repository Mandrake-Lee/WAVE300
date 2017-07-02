
/*
* $Id: mtlk_aocs.c 12949 2012-04-10 08:38:17Z grigorje $
*
* Copyright (c) 2006-2007 Metalink Broadband (Israel)
*
* Proprietary AOCS implementation
*
*/

#include "mtlkinc.h"
#include "mtlk_aocs_propr.h"
#include "mtlk_channels_propr.h"
#include "mtlkerr.h"
#include "aocs.h"
#include "channels.h"
#include "eeprom.h"
#include "mtlk_core_iface.h"
#include "rdlim.h"
#include "mhi_umi.h"
#include "mtlk_eeprom.h"
#include "mtlk_dfs.h"
#include "mhi_umi_propr.h"
#include "mtlk_coreui.h"
#include "mtlk_param_db.h"

#define LOG_LOCAL_GID   GID_AOCS
#define LOG_LOCAL_FID   1

#ifndef INT16_MAX
#define INT16_MAX 0x7fff
#endif

#ifdef MTCFG_DEBUG
#define AOCS_DEBUG
#endif

static const mtlk_ability_id_t _aocs_all_abilities[] = {
#ifdef AOCS_DEBUG
  MTLK_CORE_REQ_SET_AOCS_CL,
#endif
  MTLK_CORE_REQ_GET_AOCS_CFG,
  MTLK_CORE_REQ_SET_AOCS_CFG,
  MTLK_CORE_REQ_GET_AOCS_HISTORY,
  MTLK_CORE_REQ_GET_AOCS_TBL,
  MTLK_CORE_REQ_GET_AOCS_CHANNELS_TBL,
  MTLK_CORE_REQ_GET_AOCS_PENALTIES
};

static const mtlk_ability_id_t _aocs_idle_abilities[] = {
  MTLK_CORE_REQ_GET_AOCS_CFG,
  MTLK_CORE_REQ_SET_AOCS_CFG
};

static const mtlk_ability_id_t _aocs_active_abilities[] = {
  MTLK_CORE_REQ_GET_AOCS_CFG,
  MTLK_CORE_REQ_GET_AOCS_HISTORY,
  MTLK_CORE_REQ_GET_AOCS_TBL,
  MTLK_CORE_REQ_GET_AOCS_CHANNELS_TBL,
  MTLK_CORE_REQ_GET_AOCS_PENALTIES,
#ifdef AOCS_DEBUG
  MTLK_CORE_REQ_SET_AOCS_CL
#endif
};

/*************************************************************************************************
 * AOCS private definitions
 *************************************************************************************************/

#define AOCS_BLOCKED_SEND_TIMEOUT 5000 /* ms */

#define AOCS_NOISY_FREQ_RANGE_MHZ 80

#define AOCS_SCAN_RANK_INVALID      (255)
#define AOCS_CONFIRM_RANK_INVALID   (255)

#define AOCS_UDP_ENABLED(aocs) ((aocs)->config.type == MTLK_AOCST_UDP)

#define MTLK_TCP_AOCS_MIN_THROUGHPUT_THRESHOLD 75000
#define MTLK_TCP_AOCS_MAX_THROUGHPUT_THRESHOLD 3500000

typedef struct _aocs_chnl_tbl_entry_t {
  uint16 channel;
  uint8  sm_required;
} aocs_chnl_tbl_entry_t;

typedef struct _aocs_enum_channels_data_t {
  uint8 reg_domain;
  uint8 is_ht;
  uint8 frequency_band;
  uint8 spectrum_mode;
  uint8 bonding;
} aocs_enum_channels_data_t;

typedef struct _aocs_set_ac_param_t {
  int32 value[NTS_PRIORITIES];
} aocs_set_ac_param_t;

/* AOCS table entry */
typedef struct _mtlk_aocs_table_entry_t {
  /* CB - primary channel, nCB - 20 MHz wide channel */
  mtlk_aocs_channel_data_t *chnl_primary;
  
  /* CB - secondary channel, nCB - NULL */
  mtlk_aocs_channel_data_t *chnl_secondary;
  
  /* TxPower limit: for CB mode TxPower limit is not equal
  to min(TxPower_prim, TxPower_sec), because of HW limits
  influence. Thus, we don't store this value in 20MHz channel's
  data */
  uint16 max_tx_power;
  uint16 tx_power_penalty;

  /* exclude channel from selection: 2.4GHz nCB, 5.2GHz CB while CL exclude */
  BOOL exclude;

  /* was this channel forcibly added to the table - possible on 2.4 GHz band */
  BOOL forcibly_added;

  /* scan rank (metric based on scan results) of the channel */
  uint8 scan_rank;

  /* confirm rank (metric based on actual throughput) of the channel */
  uint8 confirm_rank;
  mtlk_osal_msec_t confirm_rank_update_time;

  /* don't use this channel - exclude from AOCS - debug, fine-tune */
  BOOL dont_use;

  /* TODO: entries below should be moved to Channel Mananger */
  /* channel's last clear check time - 11h, ms */
  mtlk_osal_msec_t time_ms_last_clear_check;

  /* non occupied period, ms */
  mtlk_osal_msec_t time_ms_non_occupied_period;
  
  /* was radar detected */
  BOOL radar_detected;

  /* linked list */
  mtlk_slist_entry_t link_entry;
} mtlk_aocs_table_entry_t;

/* TxPowerPenalty list entry */
typedef struct _mtlk_aocs_tx_penalty_t {
  uint16 freq;
  uint16 penalty;
  
  /* linked list */
  mtlk_slist_entry_t link_entry;
} mtlk_aocs_tx_penalty_t;

/* restricted channel list entry */
typedef struct _mtlk_aocs_restricted_chnl_t {
  uint8 channel;
  
  /* linked list */
  mtlk_slist_entry_t link_entry;
} mtlk_aocs_restricted_chnl_t;

typedef struct _mtlk_aocs_udp_config_t {
  uint16 msdu_threshold_aocs;
  uint16 lower_threshold;
  uint16 threshold_window;
  
  /* MSDU timeout */
  uint32 aocs_window_time_ms;

  /* number of packets that were acknowledged - switch threshold */
  uint32 msdu_per_window_threshold;
  
  /* is MSDU debug enabled */
  BOOL msdu_debug_enabled;
  
  /* access categories for which AOCS shall be enabled */
  BOOL aocs_enabled_tx_ac[NTS_PRIORITIES];
  
  /* access categories for which AOCS shall count RX packets */
  BOOL aocs_enabled_rx_ac[NTS_PRIORITIES];
} mtlk_aocs_udp_config_t;

typedef struct _mtlk_aocs_tcp_config_t {
  uint16 measurement_window;
  uint32 throughput_threshold;
} mtlk_aocs_tcp_config_t;

typedef enum {
  MTLK_AOCST_NONE,
  MTLK_AOCST_UDP,
  MTLK_AOCST_TCP,
  MTLK_AOCST_LAST
} mtlk_aocs_type_e;

/* AOCS configuration */
typedef struct _mtlk_aocs_config_t {
  /* is AOCS enabled */
  mtlk_aocs_type_e type;

  BOOL is_ht;
  uint8 frequency_band;
  uint8 spectrum_mode;
  BOOL is_auto_spectrum;
  mtlk_aocs_wrap_api_t api;
  struct mtlk_scan *scan_data;
  struct _scan_cache_t *cache;
  struct _mtlk_dot11h_t *dot11h;
  mtlk_txmm_t *txmm;
  
  /* rank switch threshold */
  uint8 cfm_rank_sw_threshold;

  /* scan cache aging */
  mtlk_osal_msec_t scan_aging_ms;

  /* confirm rank aging */
  mtlk_osal_msec_t confirm_rank_aging_ms;

  /* alpha filter coefficient, % */
  uint8 alpha_filter_coefficient;

  /* TRUE if TxPowerPenalties are used */
  BOOL use_tx_penalties;
  BOOL disable_sm_channels;

  mtlk_aocs_udp_config_t udp;
  mtlk_aocs_tcp_config_t tcp;
} mtlk_aocs_config_t;

/* AOCS context */
struct _mtlk_aocs_t {
  BOOL initialized;
  mtlk_vap_handle_t vap_handle;
  mtlk_osal_mutex_t watchdog_mutex;
  BOOL              watchdog_started;
  uint16 cur_channel;

#ifndef  MBSS_FORCE_NO_CHANNEL_SWITCH
  /* Number of used REQ BD descriptors */
  uint16 tx_data_nof_used_bds[MAX_USER_PRIORITIES];
#endif

  /* configuration of the module */
  mtlk_aocs_config_t config;

  /* current bonding: upper/lower for CB */
  uint8 bonding;

  /* is 20/40 coexistence active */
  BOOL is_20_40_coex_active;

  /* AOCS table */
  mtlk_slist_t table;

  /* AOCS channel's list */
  mtlk_slist_t channel_list;
  mtlk_osal_spinlock_t lock;
  uint8 weight_ch_load;
  uint8 weight_nof_bss;
  uint8 weight_tx_power;
  uint8 weight_sm_required;

  /* maximum of the tx power for all channels in the OCS table */
  uint16 max_tx_power;

  /* maximum number of BSS on a channel found */
  uint8 max_nof_bss;

  /* is channel switch in progress */
  BOOL ch_sw_in_progress;

  mtlk_aocs_channel_switch_reasons_t last_switch_reason;

  /* whether SM required channels should be considered during channel selection */
  BOOL disable_sm_required;

  /* for the current channel it is a time to wait after radar was detected */
  uint16 channel_availability_check_time;

  /* MSDU threshold tracking timer */
  mtlk_osal_timer_t msdu_timer;

  /* Flag whether timer is scheduled */
  BOOL msdu_timer_running;
  mtlk_atomic_t msdu_counter;

  /* debug variables */
  int8 dbg_non_occupied_period;
  int8 dbg_radar_detection_validity_time;

  /* tx_power_penalty list of channels */
  mtlk_slist_t tx_penalty_list;

  /* restricted channels list */
  mtlk_slist_t restricted_chnl_list;

  /* switch history */
  mtlk_aocs_history_t aocs_history;

  /* effective access categories for which AOCS shall be enabled */
  BOOL aocs_effective_tx_ac[NTS_PRIORITIES];

  /* effective access categories for which AOCS shall count RX packets */
  BOOL aocs_effective_rx_ac[NTS_PRIORITIES];

  /* Minimum and maximum thresholds for AOCS algorithm */
  uint16 lower_threshold[NTS_PRIORITIES];
  uint16 higher_threshold[NTS_PRIORITIES];
  mtlk_osal_timestamp_t lower_threshold_crossed_time[NTS_PRIORITIES];
  mtlk_txmm_msg_t msdu_debug_man_msg;
};

typedef int on_enum_callback_t(mtlk_aocs_t *aocs, aocs_chnl_tbl_entry_t *entry, BOOL force);

static mtlk_aocs_channel_data_t *aocs_find_channel_list (mtlk_aocs_t *aocs, uint16 channel);
static void aocs_change_bonding(mtlk_aocs_t *aocs, uint8 bonding);
static int aocs_update_bss (mtlk_aocs_t *aocs);
static int aocs_select_channel (mtlk_aocs_t *aocs, mtlk_aocs_evt_select_t *channel_data);
static int aocs_get_channel_data (mtlk_aocs_t *aocs, aocs_enum_channels_data_t *data,
  on_enum_callback_t *callback);
static uint8 aocs_get_sm_required (mtlk_aocs_table_entry_t *entry);
static void aocs_update_restricted (mtlk_aocs_t *aocs);
static void aocs_update_tpc (mtlk_aocs_t *aocs);
static void aocs_update_channels_data (mtlk_aocs_t *aocs);
static int aocs_select_channel_by_spectrum (mtlk_aocs_t *aocs, mtlk_aocs_evt_select_t *channel_data);
static void aocs_update_tx_power_penalty (mtlk_aocs_t *aocs);
typedef int aocs_select_20_40_func_t (mtlk_aocs_t *aocs,
  mtlk_aocs_evt_select_t *channel_data);
static BOOL __INLINE aocs_is_entry_disabled(mtlk_aocs_t *aocs, mtlk_aocs_table_entry_t* entry);

static void __INLINE _mtlk_aocs_move_thresholds(mtlk_aocs_t *aocs, uint8 ac, uint16 lower_threshold);
static void __INLINE _mtlk_aocs_lower_threshold_crossed(mtlk_aocs_t *aocs, uint8 ac);

static void __INLINE _mtlk_aocs_start_msdu_timer (mtlk_aocs_t *aocs);
static void __INLINE _mtlk_aocs_stop_msdu_timer (mtlk_aocs_t *aocs);
static uint32 _mtlk_aocs_msdu_tmr (mtlk_osal_timer_t *timer, mtlk_handle_t clb_usr_data);

static void
_mtlk_aocs_enable_idle_abilities(mtlk_aocs_t *aocs)
{
  mtlk_abmgr_enable_ability_set(mtlk_vap_get_abmgr(aocs->vap_handle),
                                _aocs_idle_abilities, ARRAY_SIZE(_aocs_idle_abilities));
}

static void
_mtlk_aocs_disable_idle_abilities(mtlk_aocs_t *aocs)
{
  mtlk_abmgr_disable_ability_set(mtlk_vap_get_abmgr(aocs->vap_handle),
                                 _aocs_idle_abilities, ARRAY_SIZE(_aocs_idle_abilities));
}

static void
_mtlk_aocs_enable_active_abilities(mtlk_aocs_t *aocs)
{
  mtlk_abmgr_enable_ability_set(mtlk_vap_get_abmgr(aocs->vap_handle),
                                _aocs_active_abilities, ARRAY_SIZE(_aocs_active_abilities));
}

static void
_mtlk_aocs_disable_active_abilities(mtlk_aocs_t *aocs)
{
  mtlk_abmgr_disable_ability_set(mtlk_vap_get_abmgr(aocs->vap_handle),
                                 _aocs_active_abilities, ARRAY_SIZE(_aocs_active_abilities));
}

static void
_mtlk_aocs_disable_all_abilities(mtlk_aocs_t *aocs)
{
  mtlk_abmgr_disable_ability_set(mtlk_vap_get_abmgr(aocs->vap_handle),
                                 _aocs_all_abilities, ARRAY_SIZE(_aocs_all_abilities));
}

static int
_mtlk_aocs_register_all_abilities(mtlk_aocs_t *aocs)
{
  return mtlk_abmgr_register_ability_set(mtlk_vap_get_abmgr(aocs->vap_handle),
                                         _aocs_all_abilities, ARRAY_SIZE(_aocs_all_abilities));
}

static void
_mtlk_aocs_unregister_all_abilities(mtlk_aocs_t *aocs)
{
  mtlk_abmgr_unregister_ability_set(mtlk_vap_get_abmgr(aocs->vap_handle),
                                    _aocs_all_abilities, ARRAY_SIZE(_aocs_all_abilities));
}

static __INLINE char *
aocs_bonding_to_str(uint8 bonding)
{
  return bonding == ALTERNATE_UPPER ? "upper" : "lower";
}

static __INLINE int16
aocs_get_alt_channel (int16 channel, uint8 bonding)
{
  if (bonding == ALTERNATE_UPPER)
    channel += 4;
  else
    channel -= 4;
  return channel;
}

static int
aocs_channel_add_to_channels (mtlk_aocs_t *aocs, aocs_chnl_tbl_entry_t *entry, BOOL force)
{
  mtlk_aocs_channel_data_t *new_entry;
  int result = MTLK_ERR_OK;

  ILOG4_D("adding channel %d to the AOCS channel's list", entry->channel);
  new_entry = (mtlk_aocs_channel_data_t *)mtlk_osal_mem_alloc(sizeof(*new_entry), MTLK_MEM_TAG_AOCS_ENTRY);
  if (new_entry == NULL) {
    result = MTLK_ERR_NO_MEM;
    goto FINISH;
  }
  memset(new_entry, 0, sizeof(*new_entry));
  new_entry->stat.channel = entry->channel;
  new_entry->stat.sm_required = entry->sm_required;
  mtlk_slist_push(&aocs->channel_list, &new_entry->link_entry);
FINISH:
  return result;
}

static int
aocs_channel_add_to_table (mtlk_aocs_t *aocs, aocs_chnl_tbl_entry_t *entry, BOOL force)
{
  mtlk_aocs_table_entry_t *new_entry;
  mtlk_aocs_channel_data_t *chnl_entry;
  int result = MTLK_ERR_OK;

  ILOG4_D("adding channel %d to the AOCS table", entry->channel);
  new_entry = (mtlk_aocs_table_entry_t *)mtlk_osal_mem_alloc(sizeof(*new_entry), MTLK_MEM_TAG_AOCS_TABLE_ENTRY1);
  if (new_entry == NULL) {
    result = MTLK_ERR_NO_MEM;
    goto FINISH;
  }
  memset(new_entry, 0, sizeof(*new_entry));
  new_entry->forcibly_added = force;
  /* find corresponding primary channel in channel's list */
  chnl_entry = aocs_find_channel_list(aocs, entry->channel);
  if (NULL == chnl_entry) {
    ELOG_D("Didn't find primary channel %d in AOCS channel's list", entry->channel);
    result = MTLK_ERR_AOCS_FAILED;
    goto FAIL;
  }
  ILOG5_D("Found primary channel %d in channel's list", chnl_entry->stat.channel);
  new_entry->chnl_primary = chnl_entry;
  /* if we are CB then find corresponding secondary channel */
  if (aocs->config.spectrum_mode == SPECTRUM_40MHZ) {
    uint16 alt_channel = chnl_entry->stat.channel;

    alt_channel = aocs_get_alt_channel(alt_channel, aocs->bonding);
    /* find corresponding secondary channel in channel's list */
    chnl_entry = aocs_find_channel_list(aocs, alt_channel);
    if (NULL == chnl_entry) {
      ILOG2_D("Didn't find secondary channel %d in AOCS channel's list, should change upper/lower?", alt_channel);
      result = MTLK_ERR_OK;
      goto FAIL;
    }
    ILOG5_D("Found secondary channel %d in channel's list", chnl_entry->stat.channel);
    new_entry->chnl_secondary = chnl_entry;
  }
  /* add to the AOCS table */
  mtlk_slist_push(&aocs->table, &new_entry->link_entry);
  return result;

FAIL:
  if (new_entry)
    mtlk_osal_mem_free(new_entry);
FINISH:
  return result;
}

static int
aocs_get_channel_data (mtlk_aocs_t *aocs, aocs_enum_channels_data_t *data,
  on_enum_callback_t *callback)
{
  const struct reg_domain_t *domain = NULL;
  const struct reg_class_t *cls;
  int i, j, m;
  uint16 num_of_protocols_in_domains = 0;
  uint8 upper_lower = 0;
  aocs_chnl_tbl_entry_t entry;
  int result = MTLK_ERR_AOCS_FAILED;

  ILOG5_DD("Bonding = %d (0=upper,1=lower), SpectrumMode = %d",
    data->bonding, data->spectrum_mode);
  if ((!data->bonding && data->spectrum_mode) ||
      (!data->spectrum_mode/*if nCB upper/lower not relevant*/)){
    upper_lower = 1;
  }
  domain = mtlk_get_domain(data->reg_domain, &result, &num_of_protocols_in_domains,
    upper_lower, MTLK_CHNLS_DOT11H_CALLER);
  
  if (result != MTLK_ERR_OK) {
    ILOG5_V("error, could not find domain");
    return result;
  }

  for (m = 0; m < num_of_protocols_in_domains; m++) {
    ILOG5_D("domain num_classes %d", domain[m].num_classes);
    for (i = 0; i < domain[m].num_classes; i++) {
      /* run through the domains and do only for the relevant band / HT mode combinaton */
      if (m == MAKE_PROTOCOL_INDEX(data->is_ht, data->frequency_band)) {
        cls = &domain[m].classes[i];
        ILOG5_DDDD("m = %d, Protocol index = %d, num_channels = %d, sm_required=%d", m,
          MAKE_PROTOCOL_INDEX(data->is_ht, data->frequency_band), (int)cls->num_channels,
          cls->sm_required);
        if (cls->sm_required && aocs->config.disable_sm_channels)
          continue;
        for (j = 0; j < cls->num_channels; j++) {
          /* used to get the channel (and related data) from the scan/11d table*/
          ILOG5_D("get channel %d from RD table", cls->channels[j]);
          ILOG5_DD("u8SpectrumMode=%d, spacing=%d", data->spectrum_mode, cls->spacing);
          if ((data->spectrum_mode == 1 && cls->spacing == 40) ||
            (data->spectrum_mode == 0 && cls->spacing == 20)) {
            /*get only channels that meet the bonding configured*/
            entry.channel = cls->channels[j];
            /* if 11h is off - mark all channels as non-sm required */
            if (mtlk_pdb_get_int(mtlk_vap_get_param_db(aocs->vap_handle), PARAM_DB_DFS_RADAR_DETECTION))
              entry.sm_required = cls->sm_required;
            else
              entry.sm_required = FALSE;
            /* call calback function */
            result = callback(aocs, &entry, FALSE);
            if (result != MTLK_ERR_OK) {
              ILOG5_D("Callback returned error code %d", result);
              return result;
            }
          } else
            ILOG5_DD("channel %d spacing = %d",cls->channels[j],cls->spacing);
        }/*for(j..*/
      }/*protocol*/
    }/*for(i..*/
  }/*for (m..*/
  return result;
}

static uint8
aocs_wrap_get_next_bss (mtlk_aocs_t *aocs, bss_data_t *bss_data, unsigned long *timestamp)
{
  mtlk_osal_msec_t cur_time;
  uint8 result = 0;

  cur_time = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  while (mtlk_cache_get_next_bss(aocs->config.cache, bss_data,
    NULL, timestamp)) {
    /* check if aging enabled and this entry has already expired */
    if (aocs->config.scan_aging_ms) {
      if (mtlk_osal_ms_time_diff(cur_time, *timestamp) > aocs->config.scan_aging_ms) {
        ILOG4_DDD("Expired entry on channel %d, diff %lu, aging %lu",
          bss_data->channel, (unsigned long)mtlk_osal_ms_time_diff(cur_time,
          *timestamp), (unsigned long)aocs->config.scan_aging_ms);
        continue;
      }
    }
    /* found and valid */
    result = 1;
    break;
  }
  return result;
}

static int
aocs_configure_mac (mtlk_aocs_t *aocs)
{
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t* man_entry = NULL;
  UMI_SET_CHANNEL_LOAD_VAR *mac_data;
  int result = MTLK_ERR_OK;

  ILOG4_D("Configuring MAC with alpha filter coefficient %d%%",
    aocs->config.alpha_filter_coefficient);
  /* now configure MAC */
  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, aocs->config.txmm, NULL);
  if (man_entry == NULL) {
    ELOG_V("No free man slot available to send CL parameters!");
    result = MTLK_ERR_NO_RESOURCES;
    goto FINISH;
  }
  man_entry->id = UM_MAN_SET_CHANNEL_LOAD_VAR_REQ;
  man_entry->payload_size = sizeof(UMI_SET_CHANNEL_LOAD_VAR);
  mac_data = (UMI_SET_CHANNEL_LOAD_VAR *)man_entry->payload;
  memset(mac_data, 0, sizeof(*mac_data));
  mac_data->uAlphaFilterCoefficient = aocs->config.alpha_filter_coefficient;
  mac_data->uChannelLoadThreshold = 100;
  result = mtlk_txmm_msg_send_blocked(&man_msg, AOCS_BLOCKED_SEND_TIMEOUT);
FINISH:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return result;
}

#define AOCS_FREE_LIST(name, entry_t, list)                                      \
static void                                                                      \
name (mtlk_aocs_t *aocs)                                                         \
{                                                                                \
  mtlk_slist_entry_t *list_entry_to_delete = NULL;                               \
  entry_t *aocs_entry_to_delete = NULL;                                          \
                                                                                 \
  while ((list_entry_to_delete = mtlk_slist_pop(&aocs->list)) != NULL) {         \
    aocs_entry_to_delete = MTLK_LIST_GET_CONTAINING_RECORD(list_entry_to_delete, \
      entry_t, link_entry);                                                      \
    mtlk_osal_mem_free(aocs_entry_to_delete);                                    \
  }                                                                              \
}

AOCS_FREE_LIST(aocs_free_table, mtlk_aocs_table_entry_t, table);
AOCS_FREE_LIST(aocs_free_list, mtlk_aocs_channel_data_t, channel_list);
AOCS_FREE_LIST(aocs_free_tx_penalty, mtlk_aocs_tx_penalty_t, tx_penalty_list);
AOCS_FREE_LIST(aocs_free_restricted_chnl, mtlk_aocs_restricted_chnl_t, restricted_chnl_list);

static BOOL
aocs_is_restricted_chnl (mtlk_aocs_t *aocs, uint16 channel)
{
  mtlk_slist_entry_t *list_entry = NULL;
  mtlk_aocs_restricted_chnl_t *restricted_channel_entry = NULL;

  list_entry = mtlk_slist_begin(&aocs->restricted_chnl_list);
  while (list_entry) {
    restricted_channel_entry = MTLK_LIST_GET_CONTAINING_RECORD(list_entry,
      mtlk_aocs_restricted_chnl_t, link_entry);

    if(restricted_channel_entry->channel == channel)
        return TRUE;
    /* next restricted channel */
    list_entry = mtlk_slist_next(list_entry);
  }

  return FALSE;
}

static void
mtlk_aocs_load_def_penalty (mtlk_aocs_t *aocs)
{
  mtlk_aocs_tx_penalty_t *new_entry;
  int i;

  /* free existing penalties and load defaults */
  aocs_free_tx_penalty(aocs);
  for (i = 0; aocs_default_penalty[i].freq != 0; i++) {
    new_entry = (mtlk_aocs_tx_penalty_t *)mtlk_osal_mem_alloc(sizeof(*new_entry), MTLK_MEM_TAG_AOCS_PENALTY);
    if (new_entry == NULL) {
      ELOG_V("No memory to add new TxPowerPenalty entry");
      break;
    }
    memset(new_entry, 0, sizeof(*new_entry));
    new_entry->freq = aocs_default_penalty[i].freq;
    new_entry->penalty = aocs_default_penalty[i].penalty;
    /* now add to the list */
    mtlk_slist_push(&aocs->tx_penalty_list, &new_entry->link_entry);
    ILOG4_DD("added TxPowerPenalty for freq %d, value %d",
      new_entry->freq, new_entry->penalty);
  }
}

/*****************************************************************************
**
** NAME         mtlk_aocs_init
**
** PARAMETERS   context             AOCS context
**
** RETURNS      MTLK_ERR...
**
** DESCRIPTION  This function initializes AOCS
**
******************************************************************************/
static int
_mtlk_aocs_init (mtlk_aocs_t *aocs, mtlk_aocs_init_t *ini_data, mtlk_vap_handle_t vap_handle)
{
  int result = MTLK_ERR_OK;
  int i;
 
  ILOG4_V("mtlk_aocs_init");
  
  memset(aocs, 0, sizeof(*aocs));

  aocs->vap_handle = vap_handle;
  result = mtlk_osal_lock_init(&aocs->lock);
  if (result != MTLK_ERR_OK) {
    ELOG_D("ERROR: Init: Lock: %d", result);
    goto lock_init_failed;
  }

  result = mtlk_osal_mutex_init(&aocs->watchdog_mutex);
  if (result != MTLK_ERR_OK) {
    ELOG_D("ERROR: Init: Watchdog mutex: %d", result);
    goto mutex_init_failed;
  }

  result = mtlk_txmm_msg_init(&aocs->msdu_debug_man_msg);
  if (result != MTLK_ERR_OK) {
    ELOG_D("ERROR: Init: Man msg: %d", result);
    goto man_msg_init_failed;
  }

  aocs->is_20_40_coex_active = FALSE;
  aocs->config.api = *ini_data->api;
  aocs->config.scan_data = ini_data->scan_data;
  aocs->config.cache = ini_data->cache;
  aocs->config.dot11h = ini_data->dot11h;
  aocs->config.txmm = ini_data->txmm;
  aocs->config.disable_sm_channels = ini_data->disable_sm_channels;
  aocs->bonding = ALTERNATE_UPPER;
  aocs->cur_channel = 0;
  aocs->weight_ch_load = 10;
  aocs->weight_nof_bss = 5;
  aocs->weight_tx_power = 10;
  aocs->weight_sm_required = 0;

  aocs->config.cfm_rank_sw_threshold = 10;
  /* default value is OFF */
  aocs->config.scan_aging_ms = 0;
  /* default value is 5 min (300 sec) */
  aocs->config.confirm_rank_aging_ms = 5 * MTLK_OSAL_MSEC_IN_MIN;

  aocs->config.alpha_filter_coefficient = 80;
  aocs->config.use_tx_penalties = TRUE;
  aocs->config.udp.msdu_threshold_aocs = 160;
  aocs->config.udp.msdu_per_window_threshold = 500;
  aocs->config.udp.aocs_window_time_ms = 100;

  aocs->config.udp.lower_threshold = 1;
  aocs->config.udp.threshold_window = 500;

  aocs->config.udp.aocs_enabled_tx_ac[AC_BE] = TRUE;
  aocs->config.udp.aocs_enabled_tx_ac[AC_BK] = FALSE;
  aocs->config.udp.aocs_enabled_tx_ac[AC_VI] = TRUE;
  aocs->config.udp.aocs_enabled_tx_ac[AC_VO] = FALSE;

  aocs->config.udp.aocs_enabled_rx_ac[AC_BE] = TRUE;
  aocs->config.udp.aocs_enabled_rx_ac[AC_BK] = FALSE;
  aocs->config.udp.aocs_enabled_rx_ac[AC_VI] = TRUE;
  aocs->config.udp.aocs_enabled_rx_ac[AC_VO] = FALSE;

  /* See the SRD-051-441- Optimal Channel Selection Specification.docx, AOCS enable/disable */
  aocs->config.tcp.measurement_window        = 100;         /* msec */
  aocs->config.tcp.throughput_threshold      = 1512 * 500;  /* bytes per window */

  mtlk_aocs_set_type(aocs, MTLK_AOCST_NONE);

  /* these are for debug only */
  aocs->dbg_non_occupied_period = -1;
  aocs->dbg_radar_detection_validity_time = -1;
  aocs->channel_availability_check_time = 0;

  mtlk_aocs_history_init(&aocs->aocs_history);

  /* init lists */
  mtlk_slist_init(&aocs->table);
  mtlk_slist_init(&aocs->channel_list);
  mtlk_slist_init(&aocs->tx_penalty_list);
  mtlk_slist_init(&aocs->restricted_chnl_list);

  mtlk_osal_timer_init(&aocs->msdu_timer, _mtlk_aocs_msdu_tmr, (mtlk_handle_t)aocs);
  aocs->msdu_timer_running = FALSE;

  result = aocs_configure_mac(aocs);
  if (aocs->config.use_tx_penalties)
    mtlk_aocs_load_def_penalty(aocs);

  /* init lower/upper thresholds */
  for(i = 0; i < NTS_PRIORITIES; i ++)
  {
    _mtlk_aocs_move_thresholds(aocs, i, aocs->config.udp.lower_threshold);
    aocs->lower_threshold_crossed_time[i] = 0;
  }

  result = _mtlk_aocs_register_all_abilities(aocs);
  if (MTLK_ERR_OK != result) {
    goto man_msg_init_failed;
  }

  _mtlk_aocs_enable_idle_abilities(aocs);

  aocs->initialized = 1;

  return MTLK_ERR_OK;

man_msg_init_failed:
  mtlk_osal_mutex_cleanup(&aocs->watchdog_mutex);
mutex_init_failed:
  mtlk_osal_lock_cleanup(&aocs->lock);
lock_init_failed:
  return result;
}

/*****************************************************************************
**
** NAME         mtlk_aocs_cleanup
*
** PARAMETERS   context             AOCS context
**
** RETURNS      MTLK_ERR...
**
** DESCRIPTION  This function deinitializes AOCS
**
******************************************************************************/
static void 
_mtlk_aocs_cleanup (mtlk_aocs_t *aocs)
{
  if (aocs->initialized != 1) {
    return;
  }

  _mtlk_aocs_disable_all_abilities(aocs);
  _mtlk_aocs_unregister_all_abilities(aocs);

  mtlk_osal_timer_cleanup(&aocs->msdu_timer);
  aocs_free_tx_penalty(aocs);
  aocs_free_restricted_chnl(aocs);

  mtlk_slist_cleanup(&aocs->table);
  mtlk_slist_cleanup(&aocs->channel_list);
  mtlk_slist_cleanup(&aocs->tx_penalty_list);
  mtlk_slist_cleanup(&aocs->restricted_chnl_list);

  mtlk_aocs_history_clean(&aocs->aocs_history);
  mtlk_txmm_msg_cleanup(&aocs->msdu_debug_man_msg);
  mtlk_osal_mutex_cleanup(&aocs->watchdog_mutex);
  mtlk_osal_lock_cleanup(&aocs->lock);
  memset(aocs, 0, sizeof(*aocs));
}

mtlk_aocs_t* __MTLK_IFUNC
mtlk_aocs_create(mtlk_aocs_init_t *ini_data, mtlk_vap_handle_t vap_handle)
{
  mtlk_aocs_t *aocs;

  MTLK_ASSERT(NULL != ini_data);

  if (NULL == (aocs = mtlk_osal_mem_alloc(sizeof(mtlk_aocs_t), MTLK_MEM_TAG_AOCS))) {
    ELOG_V("Can't allocate AOCS structure");
    goto err_no_mem;
  }
  memset(aocs, 0, sizeof(mtlk_aocs_t));

  if (MTLK_ERR_OK != _mtlk_aocs_init(aocs, ini_data, vap_handle)) {
    ELOG_V("Can't init AOCS structure");
    goto err_init_aocs;
  }
  return aocs;

err_init_aocs:
  mtlk_osal_mem_free(aocs);
err_no_mem:
  return NULL;
}

void __MTLK_IFUNC
mtlk_aocs_delete(mtlk_aocs_t *aocs)
{
  MTLK_ASSERT(NULL != aocs);

  _mtlk_aocs_cleanup(aocs);

  mtlk_osal_mem_free(aocs);
}

void __MTLK_IFUNC
mtlk_aocs_stop(mtlk_aocs_t *aocs)
{
  _mtlk_aocs_disable_active_abilities(aocs);

  mtlk_osal_timer_cancel_sync(&aocs->msdu_timer);
  aocs_free_table(aocs);
  aocs_free_list(aocs);

  _mtlk_aocs_enable_idle_abilities(aocs);
}

/*****************************************************************************
**
** NAME         aocs_find_channel_<list, table>
**
** PARAMETERS   context             AOCS context
**
** RETURNS      .
**
** DESCRIPTION  This function deinitializes AOCS
**
******************************************************************************/
static mtlk_aocs_channel_data_t *
aocs_find_channel_list (mtlk_aocs_t *aocs, uint16 channel)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_channel_data_t *current_entry;

  current_list_entry = mtlk_slist_begin(&aocs->channel_list);
  ILOG5_DP("Looking for channel %d in channel's list (%p)", channel, current_list_entry);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_channel_data_t, link_entry);
    if (current_entry->stat.channel == channel) {
      ILOG5_DP("Found channel %d - %p", current_entry->stat.channel, current_entry);
      return current_entry;
    }
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
  return NULL;
}

static mtlk_aocs_table_entry_t *
aocs_find_channel_table (mtlk_aocs_t *aocs, uint16 primary, uint16 secondary)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *current_entry;

  if (!primary && !secondary)
    return NULL;
  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    if (primary && secondary) {
      /* looking for exact CB channel match */
      if (!current_entry->chnl_secondary)
        goto NEXT;
      if ((current_entry->chnl_primary->stat.channel == primary) &&
        (current_entry->chnl_secondary->stat.channel == secondary))
      return current_entry;

    } else if (primary) {
      /* only primary shall match */
      if (current_entry->chnl_primary->stat.channel == primary)
        return current_entry;
    } else {
      /* only secondary shall match */
      if (!current_entry->chnl_secondary)
        goto NEXT;
      if (current_entry->chnl_secondary->stat.channel == secondary)
        return current_entry;
    }
NEXT:
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
  return NULL;
}

static void
aocs_reset_exclude_flag (mtlk_aocs_t *aocs)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *current_entry;

  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    current_entry->exclude = FALSE;
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
}

static void
aocs_remove_forced_channels (mtlk_aocs_t *aocs)
{
  mtlk_slist_entry_t *current_list_entry, *next_list_entry;
  mtlk_aocs_table_entry_t *current_entry;

  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    if (current_entry->forcibly_added) {
      next_list_entry = mtlk_slist_next(current_list_entry);
      if (current_entry->chnl_secondary) {
        ILOG4_DD("Deleting forcibly added channel (%d, %d)",
          current_entry->chnl_primary->stat.channel,
          current_entry->chnl_secondary->stat.channel);
      } else {
        ILOG4_D("Deleting forcibly added channel %d",
          current_entry->chnl_primary->stat.channel);
      }
      if (mtlk_slist_remove(&aocs->table, current_list_entry)) {
        mtlk_osal_mem_free(current_entry);
        current_list_entry = next_list_entry;
      }
    } else {
      current_list_entry = mtlk_slist_next(current_list_entry);
    }
  }
}

static void
aocs_update_40mhz_intolerant (mtlk_aocs_t *aocs)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_channel_data_t *current_entry;
  mtlk_aocs_table_entry_t *table_entry;

  current_list_entry = mtlk_slist_begin(&aocs->channel_list);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_channel_data_t, link_entry);
    if (!current_entry->stat.forty_mhz_int_affected)
      goto NEXT;
    /* update table */
    table_entry = aocs_find_channel_table(aocs, current_entry->stat.channel, 0);
    if (table_entry == NULL) {
      if (aocs->config.spectrum_mode != SPECTRUM_40MHZ)
        goto NEXT;
      table_entry = aocs_find_channel_table(aocs, 0, current_entry->stat.channel);
      if (table_entry == NULL)
        goto NEXT;
    }
    table_entry->exclude = TRUE;
    ILOG2_D("Excluded intolerant channel %d", current_entry->stat.channel);
NEXT:
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
}

/*****************************************************************************
**
** NAME         aocs_on_radar_detected
**
** PARAMETERS   context
**
** RETURNS      MTLK_ERR...
**
** DESCRIPTION  
**
******************************************************************************/
static int
aocs_on_radar_detected (mtlk_aocs_t *aocs)
{
  mtlk_aocs_table_entry_t *entry;

  /* we expect radar on 5.2MHz band only */
  if (aocs->config.frequency_band != MTLK_HW_BAND_5_2_GHZ) {
    ILOG2_V("Radar on 2.4GHz band???");
    return MTLK_ERR_AOCS_FAILED;
  }
  /* try to find in the AOCS table */ 
  ILOG4_D("Radar detected on current channel %d", aocs->cur_channel);
  entry = aocs_find_channel_table(aocs, aocs->cur_channel, 0);
  if (entry == NULL)
    return MTLK_ERR_AOCS_FAILED;
  /* we expect radar on sm required channel only */
  if (!aocs_get_sm_required(entry)) {
    ILOG2_V("Radar on non-SmRequired channel???");
    return MTLK_ERR_AOCS_FAILED;
  }
  /* channel found */
  entry->radar_detected = TRUE;
  entry->time_ms_last_clear_check = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  return MTLK_ERR_OK;
}

static mtlk_osal_msec_t
aocs_get_non_occupied_ms (mtlk_aocs_t *aocs, mtlk_aocs_table_entry_t *entry)
{
  if (aocs->dbg_non_occupied_period >= 0)
    return aocs->dbg_non_occupied_period * MTLK_OSAL_MSEC_IN_MIN;
  return entry->time_ms_non_occupied_period;
}

static void
aocs_update_non_occupied (mtlk_aocs_t *aocs)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *current_entry;
  mtlk_get_channel_data_t chnl_data;
  uint8 non_occupied_period;
  FREQUENCY_ELEMENT freq_element;

  /* update the table with TxPower values */
  current_list_entry = mtlk_slist_begin(&aocs->table);
  memset(&chnl_data, 0, sizeof(chnl_data));
  chnl_data.spectrum_mode = aocs->config.spectrum_mode;
  chnl_data.bonding = aocs->bonding;
  chnl_data.reg_domain = country_code_to_domain(mtlk_core_get_country_code(mtlk_vap_get_core(aocs->vap_handle)));
  chnl_data.is_ht = aocs->config.is_ht;
  chnl_data.ap = TRUE;
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    chnl_data.channel = current_entry->chnl_primary->stat.channel;
    mtlk_get_channel_data(&chnl_data, &freq_element,
      &non_occupied_period, NULL);
    current_entry->time_ms_non_occupied_period = non_occupied_period * MTLK_OSAL_MSEC_IN_MIN;
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
}

/*****************************************************************************
**
** NAME         aocs_channel_is_valid
**
** PARAMETERS   context
**
** RETURNS      MTLK_ERR...
**
** DESCRIPTION  In case of forced channel, it test the new channel validity
**
******************************************************************************/
static int
aocs_channel_is_valid (mtlk_aocs_t *aocs, uint16 channel, uint8 bonding)
{
  mtlk_aocs_table_entry_t *table_entry = NULL;
  int result = MTLK_ERR_OK;

  if (aocs->config.is_auto_spectrum) {
    result = MTLK_ERR_AOCS_FAILED;
    ELOG_V("Cannot use Auto spectrum with non-Auto channel");
    goto FINISH;
  }
  aocs_change_bonding(aocs, bonding);
  if (aocs->config.spectrum_mode == SPECTRUM_20MHZ) {
    ILOG1_D("Looking for channel(%u)", channel);
    table_entry = aocs_find_channel_table(aocs, channel, 0);
  } else {
    int16 alt_channel = aocs_get_alt_channel(channel, bonding);
 
    ILOG1_DDD("Looking for channel(%u) alt_channel(%u) bonding(%u)", channel, alt_channel, bonding);
    if (alt_channel > 0) 
      table_entry = aocs_find_channel_table(aocs, channel, alt_channel);
  }
  switch (aocs->config.frequency_band) {
  case MTLK_HW_BAND_5_2_GHZ:
  case MTLK_HW_BAND_2_4_GHZ:
    {
      if (table_entry == NULL) {
        ELOG_D("5_2_GHZ:Invalid channel(%u)", channel);
        result = MTLK_ERR_AOCS_FAILED;
        goto FINISH;
      }
      if (aocs_is_entry_disabled(aocs, table_entry)) {
        ELOG_D("5_2_GHZ:Disabled channel(%u)", channel);
        result = MTLK_ERR_AOCS_FAILED;
        goto FINISH;
      }
    }
    break;
  default:
    ELOG_D("Unsupported frequency band: %d", aocs->config.frequency_band);
    result = MTLK_ERR_AOCS_FAILED;
    break;
  }
FINISH:
  return result;
}

/*****************************************************************************
**
** NAME         aocs_channel_is_valid
**
** PARAMETERS   context
**
** RETURNS      MTLK_ERR...
**
** DESCRIPTION  when no channel is available for use, search for a channel
**              with the smallest time to wait
**
******************************************************************************/
static void
aocs_get_loweset_timeout_channel (mtlk_aocs_t *aocs, uint16 *channel)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *current_entry;
  mtlk_osal_msec_t time_avail_ms;
  mtlk_osal_msec_t time_current_ms;
  mtlk_osal_msec_t time_min_ms = (mtlk_osal_msec_t)-1;
  mtlk_osal_msec_t non_occupied_period_ms = 0;

  ILOG2_V("search for the channel having minimal timeout");
  *channel = 0;
  /* go through the list */
  current_list_entry = mtlk_slist_begin(&aocs->table);
  time_current_ms = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    if (!current_entry->dont_use) {
      /* time to wait is:
         time when channel will be availible again:
           time_avail = time_ms_last_clear_check + time_ms_non_occupied_period
         time_to_wait = time_avail - current_time 
         if time_to_wait <= 0 - channel is available - if there are such channels
           then this function shouldn't be called
         if time_to_wait > 0 - channel is a candidate to be selected */

      /* get non occupied period for this channel */
      non_occupied_period_ms = aocs_get_non_occupied_ms(aocs, current_entry);
      time_avail_ms = non_occupied_period_ms + current_entry->time_ms_last_clear_check;
      if (time_avail_ms > time_current_ms) {
        if (time_min_ms > time_avail_ms - time_current_ms) {
          /* channel with minimal time to wait */
          if (current_entry->chnl_primary) {
            time_min_ms = time_avail_ms - time_current_ms;
            *channel = current_entry->chnl_primary->stat.channel;
          }
        }
      }
    }
    /* find next entry */
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
}

static uint8
aocs_get_nof_bss (mtlk_aocs_table_entry_t *entry)
{
  uint8 nof_bss = 0;

  if (entry->chnl_primary)
    nof_bss = nof_bss + entry->chnl_primary->stat.nof_bss;
  if (entry->chnl_secondary)
    nof_bss =nof_bss + entry->chnl_secondary->stat.nof_bss;
  return nof_bss;
}

static uint8
aocs_get_sm_required (mtlk_aocs_table_entry_t *entry)
{
  uint8 sm = 0;

  MTLK_ASSERT(NULL != entry);

  if (entry->chnl_primary)
    sm |= entry->chnl_primary->stat.sm_required ? 1 : 0;
  if (entry->chnl_secondary)
    sm |= entry->chnl_secondary->stat.sm_required ? 1 : 0;
  return sm;
}

static uint8
aocs_get_channel_load (mtlk_aocs_table_entry_t *entry)
{
  uint8 cl = 0;

  if (entry->chnl_primary)
    cl = entry->chnl_primary->stat.channel_load;
  if (entry->chnl_secondary)
    if (entry->chnl_secondary->stat.channel_load > cl)
      cl = entry->chnl_secondary->stat.channel_load;
  return cl;
}

static uint16
aocs_get_tx_power (mtlk_aocs_table_entry_t *entry)
{
  int16 power;

  power = entry->max_tx_power - entry->tx_power_penalty;
  if (power < 0) {
    ILOG4_DD("MaxTxPower from 11d is less then penalty!!! (%d < %d)",
      entry->max_tx_power, entry->tx_power_penalty);
    return entry->max_tx_power;
  }
  return (uint16)power;
}

static void __MTLK_IFUNC
aocs_set_initial_rank(mtlk_aocs_t *aocs)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *current_entry;

  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
        mtlk_aocs_table_entry_t, link_entry);

    current_entry->scan_rank = 0;
    current_entry->confirm_rank = AOCS_CONFIRM_RANK_INVALID;
 
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
}

static void __INLINE
aocs_set_scan_rank_entry(mtlk_aocs_table_entry_t* entry, uint8 rank)
{
  MTLK_ASSERT(NULL != entry);

  /* Update channel load value */
  entry->scan_rank = rank;
  entry->confirm_rank = AOCS_CONFIRM_RANK_INVALID;
}

static void __INLINE
aocs_set_scan_rank(mtlk_aocs_t *aocs, uint16 channel, uint8 rank)
{
  ILOG4_DD("Update scan rank for channel %d, value %d", channel, rank);
  aocs_set_scan_rank_entry(aocs_find_channel_table(aocs, channel, 0), rank);
}

static void
_mtlk_aocs_set_confirm_rank(mtlk_aocs_t *aocs, uint16 channel, uint8 rank)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *current_entry;
  mtlk_aocs_table_entry_t *table_entry;

  ILOG4_DD("Update confirm rank for channel %d, value %d", channel, rank);

  /* HACKHACK primary channel can not act as a key in AOCS table
     It is possible on 2.4ghz band that we have more than one
     channels set with similar primary channels.
     In order to make the module work correctly we have to 
     clear scan rank for all such configurations */
  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
      current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
          mtlk_aocs_table_entry_t, link_entry);
      if ( (current_entry->chnl_primary != NULL) && 
           (current_entry->chnl_primary->stat.channel == channel) )
              aocs_set_scan_rank_entry(current_entry, AOCS_SCAN_RANK_INVALID);
      current_list_entry = mtlk_slist_next(current_list_entry);
  }

  /* Find channel info by channel number */
  table_entry = aocs_find_channel_table(aocs, channel, 0);
  MTLK_ASSERT(NULL != table_entry);

  /* Update channel load value */
  table_entry->scan_rank = AOCS_SCAN_RANK_INVALID;
  table_entry->confirm_rank = rank;
  table_entry->confirm_rank_update_time 
    = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
}

static uint8 __INLINE
aocs_is_cfm_rank_up_to_date(mtlk_aocs_t *aocs, mtlk_aocs_table_entry_t *table_entry)
{
  mtlk_osal_ms_diff_t confirm_rank_age;

  MTLK_ASSERT(NULL != table_entry);
  confirm_rank_age =
    mtlk_osal_ms_time_diff(mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp()),
    table_entry->confirm_rank_update_time);

  return (confirm_rank_age <= aocs->config.confirm_rank_aging_ms);
}

static uint8 __INLINE
aocs_get_confirm_rank(mtlk_aocs_t *aocs, mtlk_aocs_table_entry_t *table_entry)
{
  MTLK_ASSERT(NULL != table_entry);
  return aocs_is_cfm_rank_up_to_date(aocs, table_entry)? 
    table_entry->confirm_rank : AOCS_CONFIRM_RANK_INVALID;
}

static uint8 __INLINE
aocs_get_scan_rank(mtlk_aocs_table_entry_t *table_entry)
{
  MTLK_ASSERT(NULL != table_entry);
  return table_entry->scan_rank;
}

static void
aocs_update_scan_rank (mtlk_aocs_t *aocs, mtlk_aocs_table_entry_t *entry)
{
  uint8 norm_nbss, norm_tx_pow, norm_sm, new_scan_rank;
  int weight_sum;

  MTLK_ASSERT(NULL != entry);
  /* sum up all weights */
  weight_sum = aocs->weight_ch_load + aocs->weight_nof_bss +
    aocs->weight_tx_power + aocs->weight_sm_required;
  if (weight_sum == 0)
    weight_sum = 1;
  norm_nbss = norm_sm = norm_tx_pow = 0;
  /* normalize number of BSS */
  if (aocs->max_nof_bss)
    norm_nbss = (aocs_get_nof_bss(entry) * 100) / aocs->max_nof_bss;
  /* spectrum management required */
  norm_sm = aocs_get_sm_required(entry) * 100;
  /* tx power */
  if (aocs->max_tx_power) {
    /* transmit power is dBm multiplied by 8 */
    norm_tx_pow = (uint8) (((aocs->max_tx_power - aocs_get_tx_power(entry)) * 10) / 8);
    if (norm_tx_pow > 100)
      norm_tx_pow = 100;
  }
  /* calculate the rank of the channel */
  new_scan_rank = (uint8) ((aocs_get_channel_load(entry) * aocs->weight_ch_load + 
    norm_nbss * aocs->weight_nof_bss + norm_tx_pow * aocs->weight_tx_power +
    norm_sm * aocs->weight_sm_required) /
    weight_sum);
  if (entry->chnl_primary)
    ILOG4_DDDDDD("Ch %d rank is %d, BSS %d, SM %d, tx %d, cl = %d", entry->chnl_primary->stat.channel, new_scan_rank,
      norm_nbss, norm_sm, norm_tx_pow, aocs_get_channel_load(entry));
  aocs_set_scan_rank_entry(entry, new_scan_rank);
}

#ifndef MBSS_FORCE_NO_AOCS_INITIAL_SELECTION
void __MTLK_IFUNC
mtlk_aocs_on_bss_data_update(mtlk_aocs_t *aocs, bss_data_t *bss_data)
{
  mtlk_aocs_channel_data_t *entry;
  int16 fc, fs, aff_lo, aff_hi;

  /* TODO: trigger events shall be tracked */
  if (!bss_data->forty_mhz_intolerant)
    return;
  /* update only if scanning */
  if (mtlk_slist_size(&aocs->channel_list) == 0)
    return;
  /* mark affected channels, the formula according to SRD-051-441 is:
     fc = (fp + fs) / 2, where fc - center freq, fp - orimary, fs - secondary
     affected range [fc - 25MHz; fc + 25MHz]
     add +2 to make center freq of the channel
  */
  if (bss_data->spectrum == SPECTRUM_40MHZ) {
    ILOG2_DS("Channel %d, spectum 40MHz, bonding %s",
      bss_data->channel, bss_data->upper_lower == ALTERNATE_LOWER ?
      "lower" : "upper");
  } else {
    ILOG2_D("Channel %d, spectum 20MHz", bss_data->channel);
  }
  if (bss_data->spectrum == SPECTRUM_40MHZ) {
    fs = bss_data->channel;
    if (bss_data->upper_lower == ALTERNATE_LOWER) {
      fs -= 4;
      if (fs < 0)
        fs = 0;
    } else {
      fs += 4;
    }
    fc = ((bss_data->channel + 2 + fs + 2) * 5) / 2;
  } else {
    fc = ((bss_data->channel + 2) * 5) / 2;
  }
  /* affected range in MHz */
  aff_lo = (fc - 25);
  if (aff_lo < 0)
    aff_lo = 0;
  aff_hi = (fc + 25);
  /* convert from MHz to channel */
  aff_lo /= 5;
  aff_hi /= 5;
  ILOG2_DD("Affected channels [%d, %d]", aff_lo, aff_hi);
  /* mark channels - range is in 5MHz wide channels, so use step 1 */
  entry = aocs_find_channel_list(aocs, bss_data->channel);
  if (entry)
    entry->stat.forty_mhz_intolerant = TRUE;
  for (; aff_lo <= aff_hi; aff_lo += 1) {
    entry = aocs_find_channel_list(aocs, aff_lo);
    if (!entry)
      continue;
    ILOG4_D("40MHz intolerance affects channel %d", entry->stat.channel);
    entry->stat.forty_mhz_int_affected = TRUE;
  }
}
#endif

static uint8 __INLINE
aocs_is_noisy_channel(mtlk_aocs_t *aocs, mtlk_aocs_table_entry_t *table_entry)
{
  return (AOCS_SCAN_RANK_INVALID == aocs_get_scan_rank(table_entry)) &&
    aocs_is_cfm_rank_up_to_date(aocs, table_entry);
}

/*****************************************************************************
**
** NAME         aocs_channel_is_valid
**
** PARAMETERS   context
**
** RETURNS      MTLK_ERR...
**
** DESCRIPTION  when no channel is available for use, search for a channel
**              with the smallest time to wait
**
******************************************************************************/
static void 
aocs_update_channels_data (mtlk_aocs_t *aocs)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *current_entry;

  /* update BSS information */
  aocs_update_bss(aocs);
  if ((SPECTRUM_40MHZ == aocs->config.spectrum_mode) &&
      (FALSE == aocs->is_20_40_coex_active))
  {
    aocs_update_40mhz_intolerant(aocs);
  }
  /* rank the channels */
  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    if(AOCS_SCAN_RANK_INVALID != current_entry->scan_rank) {
      aocs_update_scan_rank(aocs, current_entry);
    }
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
}

static void
aocs_change_bonding (mtlk_aocs_t *aocs, uint8 bonding)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_channel_data_t *channel_swap = NULL;
  mtlk_aocs_table_entry_t *current_entry = NULL;

  if (aocs->config.spectrum_mode != SPECTRUM_40MHZ) {
    return;
  }
  /* check if we really changed bonding inside AOCS*/
  if (bonding == aocs->bonding) {
    return;
  }
  ILOG1_S("Bonding changed to %s", aocs_bonding_to_str(bonding));
  aocs->bonding = bonding;

  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    channel_swap = current_entry->chnl_primary;
    current_entry->chnl_primary = current_entry->chnl_secondary;
    current_entry->chnl_secondary = channel_swap;
    /* next entry */
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
  /* update non-occupied periods */
  aocs_update_non_occupied(aocs);
}

static BOOL __INLINE
aosc_is_in_radar_timeout(mtlk_aocs_t *aocs, mtlk_aocs_table_entry_t *entry)
{
    mtlk_osal_ms_diff_t channel_detection_age;

    if(!entry->radar_detected)
        return FALSE;

    ILOG4_DDDDD("Radar on %d, current %u, last %u, diff %u, non-occup %u",
        entry->chnl_primary->stat.channel, mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp()),
        entry->time_ms_last_clear_check,
        mtlk_osal_ms_time_diff(mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp()),
        entry->time_ms_last_clear_check), aocs_get_non_occupied_ms(aocs, entry));

    channel_detection_age =
        mtlk_osal_ms_time_diff(mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp()),
        entry->time_ms_last_clear_check);

    /* we have radar in the channel - check if non occupied period elapsed */
    return ( channel_detection_age <=
        aocs_get_non_occupied_ms(aocs, entry) ) ? TRUE : FALSE;
}

static BOOL __INLINE
aocs_is_entry_disabled(mtlk_aocs_t *aocs, mtlk_aocs_table_entry_t* entry)
{
  return entry->dont_use || entry->exclude ||
    (aocs->disable_sm_required && aocs_get_sm_required(entry));
};

static BOOL __INLINE
aocs_is_entry_active(mtlk_aocs_t *aocs, mtlk_aocs_table_entry_t* entry)
{
  return !(aocs_is_entry_disabled(aocs, entry) ||
           aosc_is_in_radar_timeout(aocs, entry));
};

static mtlk_aocs_table_entry_t *
aocs_select_best_channel_by_rank(mtlk_aocs_t *aocs, BOOL select_low_freq,
                                 channel_criteria_t *criteria, 
                                 channel_criteria_details_t* criteria_details)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *current_entry = NULL,
                          *best_channel_scan = NULL,
                          *best_channel_confirm = NULL,
                          *result = NULL;
  int clear_channels_count = 0;

  /* iterate over the list of available channels and select the best one */
  current_list_entry = mtlk_slist_begin(&aocs->table);

  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    current_list_entry = mtlk_slist_next(current_list_entry);

    /* don't use the channels that were masked out or excluded from being selected */
    if (!aocs_is_entry_active(aocs, current_entry)) {
        continue;
    }

    if(AOCS_SCAN_RANK_INVALID != aocs_get_scan_rank(current_entry)) {
        if(NULL == best_channel_scan) {
            best_channel_scan = current_entry;
        } else if (aocs_get_scan_rank(current_entry) < aocs_get_scan_rank(best_channel_scan)) {
            best_channel_scan = current_entry;
        } else if (select_low_freq && 
            (aocs_get_scan_rank(current_entry) == aocs_get_scan_rank(best_channel_scan))) {
            best_channel_scan = current_entry;
        }
    }

    if(AOCS_CONFIRM_RANK_INVALID != aocs_get_confirm_rank(aocs, current_entry)) {
        if(NULL == best_channel_confirm) {
            best_channel_confirm = current_entry;
        } else if (aocs_get_confirm_rank(aocs, current_entry) < aocs_get_confirm_rank(aocs, best_channel_confirm)) {
            best_channel_confirm = current_entry;
        } else if (select_low_freq && 
            (aocs_get_confirm_rank(aocs, current_entry) == aocs_get_confirm_rank(aocs, best_channel_confirm))) {
            best_channel_confirm = current_entry;
        }
    }

    if(!aocs_is_noisy_channel(aocs, current_entry))
      clear_channels_count++;
  }

  if(best_channel_scan) {
    /* If we have scan ranks - we always use them */
    result = best_channel_scan;
    *criteria = CHC_SCAN_RANK;
    criteria_details->scan.rank = aocs_get_scan_rank(result);
    goto FINISH;
  } else if( clear_channels_count > 0 ) {
    /* If we have clear channels, i.e. the channels we don't have
       information about, select one of them (random) */
    int random_channel_index;
    ILOG4_V("Selecting random channel...");

    *criteria = CHC_RANDOM;
    /* Random channel index must be less or equal to the number    
       of available clear channels */
    random_channel_index = mtlk_osal_timestamp() % clear_channels_count;

    /* Find a channel with given random index */
    current_list_entry = mtlk_slist_begin(&aocs->table);
    while (random_channel_index >= 0) {
      MTLK_ASSERT(NULL != current_list_entry);
      current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
        mtlk_aocs_table_entry_t, link_entry);
      current_list_entry = mtlk_slist_next(current_list_entry);

      /* don't use the channels that were masked out or excluded from being selected */
      if (!aocs_is_entry_active(aocs, current_entry) 
          || aocs_is_noisy_channel(aocs, current_entry)) {
          continue;
      }
      random_channel_index--;
    }
    result = current_entry;
    goto FINISH;
  } else if( best_channel_confirm ) {
    /* If we have confirm rank(s) for some channel(s) */ 
    /* except current - use minimal confirm rank      */

    /* Check channel switch threshold */
    mtlk_aocs_table_entry_t *current_channel_entry = 
      aocs_find_channel_table(aocs, aocs->cur_channel, 0);
    uint8 current_channel_rank = 
      aocs_get_confirm_rank(aocs, current_channel_entry);
    if((aocs->config.cfm_rank_sw_threshold > 0) && (AOCS_CONFIRM_RANK_INVALID != current_channel_rank) &&
        (current_channel_rank >= 
          aocs_get_confirm_rank(aocs, best_channel_confirm)*aocs->config.cfm_rank_sw_threshold/100))
      result = current_channel_entry;
    else result = best_channel_confirm;

    *criteria = CHC_CONFIRM_RANK;
    criteria_details->confirm.old_rank = aocs_get_confirm_rank(aocs, 
      aocs_find_channel_table(aocs, aocs->cur_channel, 0));
    criteria_details->confirm.new_rank = aocs_get_confirm_rank(aocs, result);
    goto FINISH;
  } else {
    /* If all channels are restricted by used, algorithm may 
       end-up here failing to select the new channel */
    result = NULL;
  }

FINISH:
  MTLK_ASSERT( (NULL == result) || !aocs_is_entry_disabled(aocs, result));
  return result;
}

static int16
aocs_estimate_tx_power_penalty (mtlk_aocs_t *aocs, uint16 channel)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_tx_penalty_t *current_entry, *freq_before, *freq_after;
  int16 before, after, penalty;
  int freq;

  /* convert channel of interest to frequency */
  freq = channel_to_frequency(channel);
  freq_before = freq_after = NULL;
  /* iterate over the list */
  current_list_entry = mtlk_slist_begin(&aocs->tx_penalty_list);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_tx_penalty_t, link_entry);
    if (current_entry->freq > freq) {
      if (freq_after == NULL)
        freq_after = current_entry;
      else if (freq_after->freq > current_entry->freq)
        freq_after = current_entry;
    }
    if (current_entry->freq < freq) {
      if (freq_before == NULL)
        freq_before = current_entry;
      else if (freq_before->freq < current_entry->freq)
        freq_before = current_entry;
    }
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
  before = after = 0;
  if (freq_before) {
    ILOG4_DD("Before: %d, penalty %d", freq_before->freq, freq_before->penalty);
    before = freq_before->penalty;
  }
  if (freq_after) {
    ILOG4_DD("After: %d, penalty %d", freq_after->freq, freq_after->penalty);
    after = freq_after->penalty;
  }
  /* In case that  |penalty_before - penalty_after| <=1 [dB*8]
     then penalty = penalty_before */
  penalty = before - after;
  if (penalty < 0)
    penalty = -penalty;
  if (penalty <= 1)
    penalty = before;
  else
    penalty = (after + before) / 2;
  ILOG4_DD("Estimated TxPowerPenalty for channel %d is %d", channel, penalty);
  return penalty;
}

static int16
aocs_get_tx_power_penalty (mtlk_aocs_t *aocs, uint16 channel)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_tx_penalty_t *current_entry;
  int freq;
  int16 penalty = INT16_MAX;

  /* convert channel of interest to frequency */
  freq = channel_to_frequency(channel);
  ILOG4_DD("Looking for TxPowerPenalty for channel %d (freq %d)", channel, freq);
  /* iterate over the list */
  current_list_entry = mtlk_slist_begin(&aocs->tx_penalty_list);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_tx_penalty_t, link_entry);
    if (current_entry->freq == freq) {
      ILOG4_DDD("Found TxPowerPenalty of %d for channel %d (freq %d)",
        current_entry->penalty, channel, freq);
      penalty = current_entry->penalty;
      break;
    }
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
  return penalty;
}

static void
aocs_update_tx_power_penalty (mtlk_aocs_t *aocs)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *entry;

  /* fill TxPowerPenalty for all channels that have the value defined in 11d */
  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
    entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    if (aocs->config.use_tx_penalties) {
      /* try to read TxPowerPenalty for the primary channel */
      entry->tx_power_penalty = aocs_get_tx_power_penalty(aocs,
        entry->chnl_primary->stat.channel);
    } else {
      /* penalties are not used - use 0 */
      entry->tx_power_penalty = 0;
    }
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
  /* use linear estimation for those channels which don't have TxPowerPenalty defined */
  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
    entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    if (entry->tx_power_penalty == INT16_MAX) {
      ILOG4_D("Use linear estimation for channel %d", entry->chnl_primary->stat.channel);
      entry->tx_power_penalty = aocs_estimate_tx_power_penalty(aocs,
        entry->chnl_primary->stat.channel);
    }
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
}

static void
aocs_disable_noisy_channels (mtlk_aocs_t *aocs, uint16 channel)
{
  int16 start_channel, last_channel, lowest_channel;
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *current_entry;

  /* find the lowest channel */
  /* channel 255 does not exist */
  lowest_channel = 255;
  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    if (current_entry->chnl_primary)
      if (current_entry->chnl_primary->stat.channel < lowest_channel)
        lowest_channel = current_entry->chnl_primary->stat.channel;
    if (current_entry->chnl_secondary)
      if (current_entry->chnl_secondary->stat.channel < lowest_channel)
        lowest_channel = current_entry->chnl_secondary->stat.channel;
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
  /* we have to clear ranks of the channels in +/-80MHz range, each channel is 5MHz wide */
  start_channel = channel - (AOCS_NOISY_FREQ_RANGE_MHZ / 5);
  if (start_channel < lowest_channel)
    start_channel = lowest_channel;
  last_channel = channel + (AOCS_NOISY_FREQ_RANGE_MHZ / 5);
  if (aocs->config.spectrum_mode == SPECTRUM_40MHZ) {
    if (aocs->bonding == ALTERNATE_LOWER)
      start_channel -= 4;
    if (start_channel < lowest_channel)
      start_channel = lowest_channel;
    if (aocs->bonding == ALTERNATE_UPPER)
      last_channel += 4;
  }
  ILOG4_DDDD("Start channel %d, lowest channel %d, last channel %d, current channel %d",
    start_channel, lowest_channel, last_channel, channel);
  for (; start_channel <= last_channel; start_channel += 4) {
    if(start_channel == channel) continue;
    ILOG4_D("Looking for %d", start_channel);
    current_entry = aocs_find_channel_table(aocs, start_channel, 0);
    if (current_entry) {
      if (current_entry->chnl_secondary)
        ILOG4_DDDD("Channel is (%d, %d), scan_rank is %d, cfm_rank is %d",
          current_entry->chnl_primary->stat.channel,
          current_entry->chnl_secondary->stat.channel,
          aocs_get_scan_rank(current_entry),
          aocs_get_confirm_rank(aocs, current_entry));
      else
        ILOG4_DDD("Channel is %d, scan_rank is %d, cfm_rank is %d",
          current_entry->chnl_primary->stat.channel, 
          aocs_get_scan_rank(current_entry),
          aocs_get_confirm_rank(aocs, current_entry));

      _mtlk_aocs_set_confirm_rank(aocs, start_channel, AOCS_CONFIRM_RANK_INVALID);
    }
  }
}

static mtlk_aocs_table_entry_t *
aocs_select_sm_randomly(mtlk_aocs_t *aocs, int cur_channel,
  channel_criteria_t *criteria, channel_criteria_details_t* criteria_details)

{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *current_entry, *cur_entry;
  int num_channels = 0;
  mtlk_aocs_table_entry_t *sm_channels[MAX_CHANNELS];

  *criteria = CHC_RANDOM;
  criteria_details->confirm.old_rank =
  criteria_details->confirm.new_rank = AOCS_CONFIRM_RANK_INVALID;
  memset(sm_channels, 0, sizeof(sm_channels));
  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    if (aocs_is_entry_active(aocs, current_entry) &&
        !aocs_is_noisy_channel(aocs, current_entry)) {
      if (current_entry->chnl_primary->stat.channel != cur_channel &&
          current_entry->chnl_primary->stat.sm_required)
        sm_channels[num_channels++] = current_entry;
    }
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
  if (!num_channels) {
    ILOG2_V("Cannot select randomly SM channel");
    return NULL;
  }
  current_entry = sm_channels[mtlk_osal_timestamp() % num_channels];
  if (current_entry)
    ILOG2_D("Selected randomly SM channel %d", current_entry->chnl_primary->stat.channel);
  else
    ILOG2_V("Unable to select randomly SM channel");
  cur_entry = aocs_find_channel_table(aocs, aocs->cur_channel, 0);
  if (cur_entry)
    criteria_details->confirm.old_rank = aocs_get_confirm_rank(aocs, cur_entry);
  criteria_details->confirm.new_rank = aocs_get_confirm_rank(aocs, current_entry);
  return current_entry;
}

static mtlk_aocs_table_entry_t *
aocs_select_non_sm_best(mtlk_aocs_t *aocs, channel_criteria_t *criteria,
                     channel_criteria_details_t* criteria_details)
{
  mtlk_aocs_table_entry_t *best_channel;
  BOOL disable_sm_required;

  disable_sm_required = aocs->disable_sm_required;
  aocs->disable_sm_required = TRUE;
  best_channel = aocs_select_best_channel_by_rank(aocs, TRUE,
      criteria, criteria_details);
  aocs->disable_sm_required = disable_sm_required;
  if (best_channel)
    ILOG2_D("Selected best non-SM channel %d", best_channel->chnl_primary->stat.channel);
  else
    ILOG2_V("Unable to select best non-SM channel");
  return best_channel;
}

static mtlk_aocs_table_entry_t *
aocs_select_on_radar(mtlk_aocs_t *aocs, channel_criteria_t *criteria,
                     channel_criteria_details_t* criteria_details)
{
  mtlk_aocs_table_entry_t *best_channel;

  best_channel = aocs_select_non_sm_best(aocs, criteria, criteria_details);
  if (best_channel)
    return best_channel;
  return aocs_select_sm_randomly(aocs, aocs->cur_channel, criteria, criteria_details);
}

static mtlk_aocs_table_entry_t *
aocs_select_on_init(mtlk_aocs_t *aocs, channel_criteria_t *criteria,
                    channel_criteria_details_t* criteria_details)
{
  mtlk_aocs_table_entry_t *non_sm, *sm;
  channel_criteria_t criteria_sm, criteria_non_sm;
  channel_criteria_details_t criteria_details_sm, criteria_details_non_sm;

  sm = aocs_select_sm_randomly(aocs, aocs->cur_channel, &criteria_sm, &criteria_details_sm);
  non_sm = aocs_select_non_sm_best(aocs, &criteria_non_sm, &criteria_details_non_sm);
  if (!sm && !non_sm)
    return NULL;
  if (!non_sm)
    goto SM_SELECTED;
  if (!sm)
    goto NON_SM_SELECTED;
  /* both channels are available: select best of two via scan ranks */
  if (aocs_get_scan_rank(sm) < aocs_get_scan_rank(non_sm))
    goto SM_SELECTED;
  /* fall through */
NON_SM_SELECTED:
  *criteria = criteria_non_sm;
  *criteria_details = criteria_details_non_sm;
  return non_sm;
SM_SELECTED:
  *criteria = criteria_sm;
  *criteria_details = criteria_details_sm;
  return sm;
}

static int
aocs_select_channel_cb_ncb (mtlk_aocs_t *aocs, mtlk_aocs_evt_select_t *channel_data)
{
  mtlk_aocs_table_entry_t *best_channel = NULL;
  int result = MTLK_ERR_OK;

  /* iterate over the list of available channels and select the best one */
  channel_data->channel = 0;
  channel_data->bonding = ALTERNATE_LOWER;
  if (channel_data->reason == SWR_RADAR_DETECTED) {
    /* At radar detection event: always choose the best channel
       from non-SM channel. If there are no non-SM channel available
       randomize the best SM channel available (without the current channel) */
    best_channel = aocs_select_on_radar(aocs, &channel_data->criteria,
      &channel_data->criteria_details);
  } else if (channel_data->reason == SWR_INITIAL_SELECTION) {
    /* Randomize channel from SM Channels C1, choose the best channel from
       non-SM channels C2, then choose the best from C1 and C2 */
    best_channel = aocs_select_on_init(aocs, &channel_data->criteria,
      &channel_data->criteria_details);
  } else {
    best_channel = aocs_select_best_channel_by_rank(aocs, TRUE, 
        &channel_data->criteria, &channel_data->criteria_details);
  }
  /* check if channel was selected */
  if (best_channel == NULL) {
    ILOG2_V("Unable to select new channel from AOCS table!");
    result = MTLK_ERR_AOCS_FAILED;
    goto FINISH;
  }
  /* now select upper/lower if we are CB */
  /* From SRD: the channel that more BSSs are using as primary,
     should be used as primary by the OCS. If the number of BSSs
     using the upper and lower is the same,
     the lower channel should be the primary) */
  if (aocs->config.spectrum_mode == SPECTRUM_40MHZ) {
    int8 nof_bss;

    if (aocs->config.is_auto_spectrum && best_channel->chnl_primary->stat.nof_bss &&
        best_channel->chnl_secondary->stat.nof_bss) {
      ILOG2_V("both primary and secondary channels have BSS - must not make overlapping");
      result = MTLK_ERR_AOCS_FAILED;
      goto FINISH;
    }
    nof_bss = best_channel->chnl_primary->stat.nof_bss - best_channel->chnl_secondary->stat.nof_bss;
    if (nof_bss == 0)
      channel_data->bonding = ALTERNATE_LOWER;
    if (nof_bss > 0) {
      /* use current bonding as majority of the BSS found are on primary channel */
      channel_data->bonding = aocs->bonding;
    }
    if (nof_bss < 0) {
      /* secondary channel now has more BSS */
      channel_data->bonding = (aocs->bonding == ALTERNATE_UPPER) ? ALTERNATE_LOWER : ALTERNATE_UPPER;
    }
    /* select channel according to bonding selected */
    if (channel_data->bonding != aocs->bonding)
      channel_data->channel = best_channel->chnl_secondary->stat.channel;
    else
      channel_data->channel = best_channel->chnl_primary->stat.channel;
  } else {
    channel_data->channel = best_channel->chnl_primary->stat.channel;
  }
FINISH:
  return result;
}

static int
aocs_auto_select_20_40 (mtlk_aocs_t *aocs, mtlk_aocs_evt_select_t *channel_data,
  aocs_select_20_40_func_t *cb, aocs_select_20_40_func_t *ncb)
{
  int result = MTLK_ERR_OK;

  /* try to select 40MHz first */
  ILOG2_V("Try to select spectrum mode automatically");
  aocs->config.spectrum_mode = SPECTRUM_40MHZ;
  result = cb(aocs, channel_data);
  if (result == MTLK_ERR_OK) {
    ILOG0_V("Selected 40MHz mode");
    goto FINISH;
  }
  /* rebuild AOCS table */
  aocs->config.spectrum_mode = SPECTRUM_20MHZ;
  if (mtlk_aocs_start(aocs, TRUE, aocs->is_20_40_coex_active) != MTLK_ERR_OK) {
    ELOG_V("Failed to rebuild AOCS for selection");
    result = MTLK_ERR_AOCS_FAILED;
    goto FINISH;
  }
  aocs_update_channels_data(aocs);
  result = ncb(aocs, channel_data);
  if (result == MTLK_ERR_OK)
    ILOG0_V("Selected 20MHz mode");
FINISH:
  if (result != MTLK_ERR_OK)
    ELOG_V("Unable to auto select spectrum mode");
  return result;
}

static int
aocs_select_channel_by_spectrum (mtlk_aocs_t *aocs, mtlk_aocs_evt_select_t *channel_data)
{
  int result = MTLK_ERR_OK;

  if (aocs->config.is_auto_spectrum)
    result = aocs_auto_select_20_40(aocs, channel_data,
      aocs_select_channel_cb_ncb,
      aocs_select_channel_cb_ncb);
  else
    result = aocs_select_channel_cb_ncb(aocs, channel_data);
  return result;
}

/*****************************************************************************
**
** NAME         aocs_channel_is_valid
**
** PARAMETERS   context
**
** RETURNS      MTLK_ERR...
**
** DESCRIPTION  when no channel is available for use, search for a channel
**              with the smallest time to wait
**
******************************************************************************/
static int
aocs_build_list (mtlk_aocs_t *aocs, uint8 spectrum_mode, uint8 bonding,
  on_enum_callback_t *callback)
{
  int result = MTLK_ERR_OK;
  aocs_enum_channels_data_t enum_data;

  mtlk_osal_lock_acquire(&aocs->lock);
  /* build AOCS channel's list from 11d tables */
  enum_data.reg_domain = country_code_to_domain(mtlk_core_get_country_code(mtlk_vap_get_core(aocs->vap_handle)));
  enum_data.is_ht = aocs->config.is_ht;
  enum_data.frequency_band = aocs->config.frequency_band;
  /* use 20MHz channels */
  enum_data.spectrum_mode = spectrum_mode;
  enum_data.bonding = bonding;
  result = aocs_get_channel_data(aocs, &enum_data, callback);
  if (result != MTLK_ERR_OK)
    ELOG_D("Failed to buils AOCS channel's list with code %d", result);
  mtlk_osal_lock_release(&aocs->lock);
  return result;
}

static void
aocs_update_tpc (mtlk_aocs_t *aocs)
{
  uint16 max_tx_pow, entry_tx_power;
  uint16 tpc_tx_power;
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *current_entry;

  /* update TxPowerPenalty */
  aocs_update_tx_power_penalty(aocs);
  /* update the table with TxPower values */
  max_tx_pow = 0;
  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    current_entry->max_tx_power = mtlk_calc_tx_power_lim_wrapper((mtlk_handle_t)mtlk_vap_get_core(aocs->vap_handle),
      aocs->config.spectrum_mode, (uint8) current_entry->chnl_primary->stat.channel);


    /* get MaxTxPower from EEPROM/TPC */
    tpc_tx_power = mtlk_get_max_tx_power(mtlk_core_get_eeprom(mtlk_vap_get_core(aocs->vap_handle)),
                                         (uint8)current_entry->chnl_primary->stat.channel);

    ILOG4_DDD("TxPower for channel %d: HW/reg_dom  %d, TPC limit %u",
      current_entry->chnl_primary->stat.channel, current_entry->max_tx_power, tpc_tx_power);

    /* select minimal TxPower allowed */
    if (current_entry->max_tx_power > tpc_tx_power)
      current_entry->max_tx_power = tpc_tx_power;

    /* maximum? */
    entry_tx_power = aocs_get_tx_power(current_entry);
    if (entry_tx_power > max_tx_pow)
        max_tx_pow = entry_tx_power;
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
  aocs->max_tx_power = max_tx_pow;
  ILOG4_DD("Maximum TxPower %d (%d dBm)", max_tx_pow, max_tx_pow / 8);
}

static int 
aocs_enable_mac_algorithm (mtlk_aocs_t *aocs, BOOL enable)
{
  int               res       = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_AOCS_CFG     *aocs_cfg;

  ILOG2_S("AOCS TCP watchdog becomes %s", enable?"enabled":"disabled");

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, aocs->config.txmm, NULL);
  if (!man_entry) {
    ELOG_V("Can't send AOCS TCP msg due to the lack of MAN_MSG");
    res = MTLK_ERR_NO_RESOURCES;
    goto end;
  }

  man_entry->id           = UM_MAN_AOCS_REQ;
  man_entry->payload_size = sizeof(*aocs_cfg);

  aocs_cfg = (UMI_AOCS_CFG *)man_entry->payload;

  memset(aocs_cfg, 0, sizeof(*aocs_cfg));

  if (enable) {
    MTLK_ASSERT(aocs->config.tcp.throughput_threshold >= MTLK_TCP_AOCS_MIN_THROUGHPUT_THRESHOLD && 
                aocs->config.tcp.throughput_threshold <= MTLK_TCP_AOCS_MAX_THROUGHPUT_THRESHOLD);

    aocs_cfg->u16Command           = HOST_TO_MAC16(MAC_AOCS_ALG_ENABLE);
    aocs_cfg->u16MeasurementWindow = HOST_TO_MAC16(aocs->config.tcp.measurement_window);
    aocs_cfg->u32ThrouhhputTH      = HOST_TO_MAC32(aocs->config.tcp.throughput_threshold);
  }
  else {
    aocs_cfg->u16Command           = HOST_TO_MAC16(MAC_AOCS_DISABLE);
  }

  res = mtlk_txmm_msg_send_blocked(&man_msg, AOCS_BLOCKED_SEND_TIMEOUT);
  if (res < MTLK_ERR_OK) {
    ELOG_V("Can't send AOCS request to MAC, timed-out");
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
aocs_on_tcp_ind (mtlk_aocs_t *aocs, UMI_AOCS_IND *data, uint32 data_size)
{
  uint8  rank = 0;
  int    i;
  uint32 sum = 0;

  MTLK_ASSERT(data_size >= sizeof(UMI_AOCS_IND));

  for (i = 0; i < MAX_USER_PRIORITIES; i++) {
    data->au32Throughput_TX[i] = MAC_TO_HOST32(data->au32Throughput_TX[i]);
    data->au32Throughput_RX[i] = MAC_TO_HOST32(data->au32Throughput_RX[i]);
    ILOG4_DDD("AOCS TCP IND: dl#%d = tx:%u rx:%u", 
         i, data->au32Throughput_TX[i], data->au32Throughput_RX[i]);

    sum += data->au32Throughput_TX[i] + data->au32Throughput_RX[i];
  }
  
  /* Caclulate the rank according to 
   * SRD-051-441- Optimal Channel Selection Specification.docx, 4.5.3.1. Pressure Rank:
   * (throughput_threshold - all_throughput)/throughput_threshold normalized to 100
   */
  rank = (uint8)(100 - (100 * sum)/aocs->config.tcp.throughput_threshold);

  ILOG2_DDD("AOCS TCP IND: rank=%d (sum=%u thrsh=%u)", 
       rank, sum, aocs->config.tcp.throughput_threshold);

  mtlk_osal_lock_acquire(&aocs->lock);
  _mtlk_aocs_set_confirm_rank(aocs, aocs->cur_channel, rank);
  aocs_optimize_channel(aocs, SWR_MAC_PRESSURE_TEST);
  mtlk_osal_lock_release(&aocs->lock);
}

int __MTLK_IFUNC
mtlk_aocs_start (mtlk_aocs_t *aocs, BOOL keep_chnl_info, BOOL is_20_40_coexistence_active)
{
  int result = MTLK_ERR_OK;

  aocs->is_20_40_coex_active = is_20_40_coexistence_active;
  _mtlk_aocs_disable_idle_abilities(aocs);

  /* build channel's list first - use 20MHz channels */
  aocs_free_table(aocs);
  if (!keep_chnl_info) {
    aocs_free_list(aocs);
    /*Bonding is not relevant here, so use Upper*/
    result = aocs_build_list(aocs, SPECTRUM_20MHZ, ALTERNATE_UPPER, aocs_channel_add_to_channels);
    if (result != MTLK_ERR_OK)
      goto FINISH;
  }

  result = aocs_build_list(aocs, aocs->config.spectrum_mode, aocs->bonding,
                           aocs_channel_add_to_table);
  if (result != MTLK_ERR_OK)
    goto FINISH;
  /* update restricted channels now */
  aocs_update_restricted(aocs);
  /* update the table with TPC values */
  aocs_update_tpc(aocs);
  /* update non-occupied periods */
  aocs_update_non_occupied(aocs);
  /* set initial ranks */
  aocs_set_initial_rank(aocs);
  /* table created */
  /* Disable SM required channels selection */
  mtlk_aocs_disable_smrequired(aocs);

  _mtlk_aocs_enable_active_abilities(aocs);

FINISH:
  return result;
}

static int
_mtlk_aocs_start_preconfigured_watchdog (mtlk_aocs_t *aocs)
{
  int res = MTLK_ERR_UNKNOWN;

  switch (aocs->config.type) {
  case MTLK_AOCST_TCP:
    /* Switching on the TCP AOCS */
    res = aocs_enable_mac_algorithm(aocs, TRUE);
    break;
  case MTLK_AOCST_UDP:
  case MTLK_AOCST_NONE:
  default:
    /* Nothing to do */
    res = MTLK_ERR_OK;
    break;
  }
    
  return res;
}

static void
_mtlk_aocs_stop_preconfigured_watchdog (mtlk_aocs_t *aocs)
{
  switch (aocs->config.type) {
  case MTLK_AOCST_TCP:
    /* Switching off the TCP AOCS */
    aocs_enable_mac_algorithm(aocs, FALSE);
    break;
  case MTLK_AOCST_UDP:
  case MTLK_AOCST_NONE:
  default:
    /* Nothing to do */
    break;
  }   
}

int __MTLK_IFUNC
mtlk_aocs_start_watchdog (mtlk_aocs_t *aocs)
{
  int res = MTLK_ERR_OK;

  mtlk_osal_mutex_acquire(&aocs->watchdog_mutex);
  if (!aocs->watchdog_started) {
    res = _mtlk_aocs_start_preconfigured_watchdog(aocs);
    aocs->watchdog_started = (res == MTLK_ERR_OK);
  }
  mtlk_osal_mutex_release(&aocs->watchdog_mutex);

  return res;
}

void __MTLK_IFUNC
mtlk_aocs_stop_watchdog (mtlk_aocs_t *aocs)
{
  mtlk_osal_mutex_acquire(&aocs->watchdog_mutex);
  if (aocs->watchdog_started) {
    _mtlk_aocs_stop_preconfigured_watchdog(aocs);
    aocs->watchdog_started = FALSE;
  }
  mtlk_osal_mutex_release(&aocs->watchdog_mutex);
}

/*****************************************************************************
**
** NAME         aocs_channel_is_valid
**
** PARAMETERS   context
**
** RETURNS      MTLK_ERR...
**
** DESCRIPTION  when no channel is available for use, search for a channel
**              with the smallest time to wait
**
******************************************************************************/
int __MTLK_IFUNC
mtlk_aocs_update_cl_on_scan_cfm (mtlk_aocs_t *aocs, void *data)
{
  int i;
  uint16 load, channel;
  mtlk_aocs_channel_data_t *entry;
  mtlk_aocs_table_entry_t *table_entry;
  FREQUENCY_ELEMENT *freq_element = ((UMI_SCAN*)data)->aChannelParams;

  mtlk_osal_lock_acquire(&aocs->lock);
  ILOG5_V("Update channel load");
  /* go through the array of frequency element, stop if empty
   * element or number of the element is > UMI_MAX_CHANNELS_PER_SCAN_REQ */
  for (i = 0; i < UMI_MAX_CHANNELS_PER_SCAN_REQ; i++) {
    channel = MAC_TO_HOST16(freq_element[i].u16Channel);
    /* check if valid channel */
    if (channel == 0)
      break;
    entry = aocs_find_channel_list(aocs, channel);
    if (entry == NULL)
      continue;
    load = MAC_TO_HOST16(freq_element[i].u16ChannelLoad);
    ILOG5_DD("CFM channel %d, load is %d", channel, load);
    entry->stat.channel_load = (uint8) load;
    
    table_entry = aocs_find_channel_table(aocs, channel, 0);
    if(NULL != (table_entry = aocs_find_channel_table(aocs, channel, 0)))
        aocs_update_scan_rank(aocs, table_entry);
  }
  mtlk_osal_lock_release(&aocs->lock);
  return 0;
}

static void __INLINE
aocs_do_channel_switch(mtlk_aocs_t *aocs, mtlk_aocs_evt_select_t *event_data)
{
  /* initiate channel switch */
  aocs->config.dot11h->event = MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL;

  aocs->config.dot11h->cfg.u8IsHT = aocs->config.is_ht;
  aocs->config.dot11h->cfg.u8FrequencyBand = aocs->config.frequency_band;
  aocs->config.dot11h->cfg.u8SpectrumMode = aocs->config.spectrum_mode;
  aocs->config.dot11h->cfg.u8Bonding = aocs->bonding;

  /*
   * We should reset channel to auto for proper channel switching and Core notification
   */
  event_data->channel = 0;

  /**
   * mtlk_aocs_indicate_event(MTLK_AOCS_EVENT_SELECT_CHANNEL) is called from
   * mtlk_dot11h_initiate_channel_switch.
   * Notification about changing the channel, bounding and spectrum will be sent to core
   * from mtlk_aocs_indicate_event(MTLK_AOCS_EVENT_SELECT_CHANNEL).
   **/
  mtlk_dot11h_initiate_channel_switch(aocs->config.dot11h, event_data, TRUE);
}

void __MTLK_IFUNC
aocs_optimize_channel(mtlk_aocs_t *aocs, mtlk_aocs_channel_switch_reasons_t reason)
{
  mtlk_aocs_evt_select_t channel_data = {0};
  
  channel_data.reason = reason;

  /* If channel switch is currently in progress */
  /* we will not try to initiate another switch */
  if (aocs->ch_sw_in_progress) {
      ILOG2_V("Previous channel switch hasn't completed yet");
      return;
  }

  /* Initiate channel switch */

  /* Disable channels next to the channel we are leaving */
  aocs_disable_noisy_channels(aocs, aocs->cur_channel);

  /* Select the optimal channel */
  if (MTLK_ERR_OK != aocs_select_channel(aocs, &channel_data)) {
      ILOG2_V("Failed to select a new channel");
      return;
  }

  if (channel_data.channel == aocs->cur_channel) {
      ILOG4_V("Current channel is still the best one");
      return;
  }

  aocs_do_channel_switch(aocs, &channel_data);
};

void __MTLK_IFUNC
mtlk_aocs_update_cl (mtlk_aocs_t *aocs, uint16 channel, uint8 channel_load)
{
  mtlk_aocs_channel_data_t *channel_info;

  mtlk_osal_lock_acquire(&aocs->lock);

  ILOG4_DD("Update load for channel %d, value %d%%", channel, channel_load);

  /* Find channel info by channel number */
  channel_info = aocs_find_channel_list(aocs, channel);
  if (channel_info == NULL)
    goto FINISH;

  /* Update channel load value */
  channel_info->stat.channel_load = channel_load;

FINISH:
  mtlk_osal_lock_release(&aocs->lock);
}

/*****************************************************************************
**
** NAME         aocs_channel_is_valid
**
** PARAMETERS   context
**
** RETURNS      MTLK_ERR...
**
** DESCRIPTION  when no channel is available for use, search for a channel
**              with the smallest time to wait
**
******************************************************************************/
static int
aocs_update_bss (mtlk_aocs_t *aocs)
{
  bss_data_t bss_data;
  int result = MTLK_ERR_OK;
  mtlk_aocs_channel_data_t *entry;
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_table_entry_t *table_entry;
  uint8 nof_bss;
  unsigned long scan_cache_timestamp;

  current_list_entry = mtlk_slist_begin(&aocs->channel_list);
  while (current_list_entry) {
    entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_channel_data_t, link_entry);
    entry->stat.nof_bss = 0;
    entry->stat.num_20mhz_bss = entry->stat.num_40mhz_bss = 0;
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
  /* now update from scan cache */
  mtlk_cache_rewind(aocs->config.cache);
  while (aocs_wrap_get_next_bss(aocs, &bss_data, &scan_cache_timestamp)) {
    /* scan cache entry is ok - update */
    entry = aocs_find_channel_list(aocs, bss_data.channel);
    if (entry == NULL)
      continue;
    entry->stat.nof_bss++;
    if (bss_data.spectrum == SPECTRUM_40MHZ) {
      entry->stat.num_40mhz_bss++;
      ILOG4_DDSDDS("Ch %d, nof_bss %d, add %s, total %d, %dMHz, %s bonding", bss_data.channel,
        entry->stat.nof_bss, bss_data.essid, entry->stat.nof_bss,
        bss_data.spectrum == SPECTRUM_40MHZ ? 40 : 20,
        aocs_bonding_to_str(bss_data.upper_lower));
    } else {
      entry->stat.num_20mhz_bss++;
      ILOG4_DDSDD("Ch %d, nof_bss %d, add %s, total %d, %dMHz", bss_data.channel,
        entry->stat.nof_bss, bss_data.essid, entry->stat.nof_bss,
        bss_data.spectrum == SPECTRUM_40MHZ ? 40 : 20);
    }
  }
  /* update maximum number of BSS */
  aocs->max_nof_bss = 0;
  current_list_entry = mtlk_slist_begin(&aocs->table);
  while (current_list_entry) {
    table_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_table_entry_t, link_entry);
    nof_bss = aocs_get_nof_bss(table_entry);
    if (nof_bss > aocs->max_nof_bss)
      aocs->max_nof_bss = nof_bss;
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
  ILOG4_D("Maximum #BSS %d", aocs->max_nof_bss);
  return result;
}
int  __MTLK_IFUNC
mtlk_aocs_get_history(mtlk_aocs_t *aocs, mtlk_clpb_t *clpb)
{
  return mtlk_aocshistory_get_history(&aocs->aocs_history, clpb);
}

int __MTLK_IFUNC
mtlk_aocs_get_table (mtlk_aocs_t *aocs, mtlk_clpb_t *clpb)
{
  mtlk_slist_entry_t *list_head;
  mtlk_slist_entry_t *list_entry;
  mtlk_aocs_table_entry_t *entry;
  uint16 chnl_primary, chnl_secondary, tx_power;
  mtlk_aocs_table_stat_entry_t stat_entry;
  int res = MTLK_ERR_UNKNOWN;

  MTLK_ASSERT(NULL != aocs);
  MTLK_ASSERT(NULL != clpb);

  mtlk_slist_foreach(&aocs->table, list_entry, list_head) {
    entry = MTLK_LIST_GET_CONTAINING_RECORD(list_entry,
                                            mtlk_aocs_table_entry_t,
                                            link_entry);
    chnl_primary = chnl_secondary = 0;
    if (entry->chnl_primary) {
      chnl_primary = entry->chnl_primary->stat.channel;
    }
    if (entry->chnl_secondary) {
      chnl_secondary = entry->chnl_secondary->stat.channel;
    }

    tx_power = aocs_get_tx_power(entry);
    
    stat_entry.time_ms_non_occupied_period = entry->time_ms_non_occupied_period;
    stat_entry.time_ms_last_clear_check = entry->time_ms_last_clear_check;
    stat_entry.radar_detected = entry->radar_detected;
    stat_entry.is_in_radar_timeout = aosc_is_in_radar_timeout(aocs, entry);
    stat_entry.dont_use = entry->dont_use;
    stat_entry.exclude = entry->exclude;
    stat_entry.channel_primary = chnl_primary;
    stat_entry.channel_secondary = chnl_secondary;
    stat_entry.tx_power = tx_power;
    stat_entry.max_tx_power = entry->max_tx_power;
    stat_entry.tx_power_penalty = entry->tx_power_penalty;
    stat_entry.channel_load = aocs_get_channel_load(entry);
    stat_entry.scan_rank = aocs_get_scan_rank(entry);
    stat_entry.confirm_rank = aocs_get_confirm_rank(aocs, entry);
    stat_entry.nof_bss = aocs_get_nof_bss(entry);
    stat_entry.sm = aocs_get_sm_required(entry);
    stat_entry.is_noisy = aocs_is_noisy_channel(aocs, entry);

    if (MTLK_ERR_OK != (res = mtlk_clpb_push(clpb, &stat_entry, sizeof(stat_entry)))) {
      goto err_push;
    }
  }
  return MTLK_ERR_OK;

err_push:
  mtlk_clpb_purge(clpb);
  return res;
}

int __MTLK_IFUNC
mtlk_aocs_get_channels (mtlk_aocs_t *aocs, mtlk_clpb_t *clpb)
{
  mtlk_slist_entry_t *list_head;
  mtlk_slist_entry_t *list_entry;
  mtlk_aocs_channel_data_t *entry;
  int res = MTLK_ERR_UNKNOWN;

  MTLK_ASSERT(NULL != aocs);
  MTLK_ASSERT(NULL != clpb);

  mtlk_slist_foreach(&aocs->channel_list, list_entry, list_head) {
    entry = MTLK_LIST_GET_CONTAINING_RECORD(list_entry,
      mtlk_aocs_channel_data_t, link_entry);

    if (MTLK_ERR_OK != (res = mtlk_clpb_push(clpb, &entry->stat, sizeof(entry->stat)))) {
      goto err_push;
    }
  }
  return MTLK_ERR_OK;

err_push:
  mtlk_clpb_purge(clpb);
  return res;
}

int __MTLK_IFUNC
mtlk_aocs_get_penalties (mtlk_aocs_t *aocs, mtlk_clpb_t *clpb)
{
  mtlk_slist_entry_t *list_head;
  mtlk_slist_entry_t *list_entry;
  mtlk_aocs_tx_penalty_t *entry;
  mtlk_aocs_penalties_stat_entry_t stat_entry;
  int res = MTLK_ERR_UNKNOWN;

  MTLK_ASSERT(NULL != aocs);
  MTLK_ASSERT(NULL != clpb);

  mtlk_slist_foreach(&aocs->tx_penalty_list, list_entry, list_head) {
    entry = MTLK_LIST_GET_CONTAINING_RECORD(list_entry,
      mtlk_aocs_tx_penalty_t, link_entry);

    stat_entry.freq = entry->freq;
    stat_entry.penalty = entry->penalty;
    
    if (MTLK_ERR_OK != (res = mtlk_clpb_push(clpb, &stat_entry, sizeof(stat_entry)))) {
      goto err_push;
    }
  }
  return MTLK_ERR_OK;

err_push:
  mtlk_clpb_purge(clpb);
  return res;
}

int __MTLK_IFUNC
mtlk_aocs_get_msdu_threshold(mtlk_aocs_t *aocs)
{
  return aocs->config.udp.msdu_threshold_aocs;
}

int __MTLK_IFUNC
mtlk_aocs_set_msdu_threshold(mtlk_aocs_t *aocs, uint32 value)
{
  aocs->config.udp.msdu_threshold_aocs = value;
  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_aocs_get_lower_threshold(mtlk_aocs_t *aocs)
{
  return aocs->config.udp.lower_threshold;
}

int __MTLK_IFUNC
mtlk_aocs_set_lower_threshold(mtlk_aocs_t *aocs, uint32 value)
{
  aocs->config.udp.lower_threshold = value;
  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_aocs_get_threshold_window(mtlk_aocs_t *aocs)
{
  return aocs->config.udp.threshold_window;
}

int __MTLK_IFUNC
mtlk_aocs_set_threshold_window(mtlk_aocs_t *aocs, uint32 value)
{
  aocs->config.udp.threshold_window = value;
  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_aocs_get_weight(mtlk_aocs_t *aocs, mtlk_aocs_weight_e index)
{
  int weight;

  switch (index) {
  case AOCS_WEIGHT_IDX_CL:
    weight = aocs->weight_ch_load;
    break;
  case AOCS_WEIGHT_IDX_TX:
    weight = aocs->weight_nof_bss;
    break;
  case AOCS_WEIGHT_IDX_BSS:
    weight = aocs->weight_tx_power;
    break;
  case AOCS_WEIGHT_IDX_SM:
    weight = aocs->weight_sm_required;
    break;
  default:
    weight = 0;
    break;
  }
  return weight;
}

int __MTLK_IFUNC
mtlk_aocs_set_weight(mtlk_aocs_t *aocs, mtlk_aocs_weight_e index, int32 weight)
{
  int result = MTLK_ERR_OK;
 
  if ((weight < 0) || (weight > 255)) {
    result = MTLK_ERR_PARAMS;
    goto FINISH;
  }
  switch (index) {
  case AOCS_WEIGHT_IDX_CL:
    aocs->weight_ch_load = (unsigned char)weight;
    break;
  case AOCS_WEIGHT_IDX_TX:
    aocs->weight_nof_bss = (unsigned char)weight;
    break;
  case AOCS_WEIGHT_IDX_BSS:
    aocs->weight_tx_power = (unsigned char)weight;
    break;
  case AOCS_WEIGHT_IDX_SM:
    aocs->weight_sm_required = (unsigned char)weight;
    break;
  default:
    result = MTLK_ERR_PARAMS;
    break;
  }
FINISH:
  return result;
}

int __MTLK_IFUNC
mtlk_aocs_get_cfm_rank_sw_threshold(mtlk_aocs_t *aocs)
{
  return aocs->config.cfm_rank_sw_threshold;
}

int __MTLK_IFUNC
mtlk_aocs_set_cfm_rank_sw_threshold(mtlk_aocs_t *aocs, uint8 value)
{
  int result = MTLK_ERR_OK;

  if (value <= 100)
    aocs->config.cfm_rank_sw_threshold = value;
  else
    result = MTLK_ERR_PARAMS;
  return result;
}

int __MTLK_IFUNC
mtlk_aocs_get_scan_aging(mtlk_aocs_t *aocs)
{
  return aocs->config.scan_aging_ms / MTLK_OSAL_MSEC_IN_MIN;
}

int __MTLK_IFUNC
mtlk_aocs_set_scan_aging(mtlk_aocs_t *aocs, int value)
{
  int result = MTLK_ERR_OK;

  if (value < 0)
    result = MTLK_ERR_PARAMS;
  else
    aocs->config.scan_aging_ms = value * MTLK_OSAL_MSEC_IN_MIN;
  return result;
}

int __MTLK_IFUNC
mtlk_aocs_get_confirm_rank_aging(mtlk_aocs_t *aocs)
{
  return aocs->config.confirm_rank_aging_ms / MTLK_OSAL_MSEC_IN_MIN;
}

int __MTLK_IFUNC
mtlk_aocs_set_confirm_rank_aging(mtlk_aocs_t *aocs, int value)
{
  int result = MTLK_ERR_OK;

  if (value < 0)
    result = MTLK_ERR_PARAMS;
  else
    aocs->config.confirm_rank_aging_ms = value * MTLK_OSAL_MSEC_IN_MIN;
  return result;
}


int __MTLK_IFUNC
mtlk_aocs_get_afilter(mtlk_aocs_t *aocs)
{
  return aocs->config.alpha_filter_coefficient;
}

int __MTLK_IFUNC
mtlk_aocs_set_afilter(mtlk_aocs_t *aocs, uint8 value)
{
  int result = MTLK_ERR_OK;

  if (value <= 100) {
    aocs->config.alpha_filter_coefficient = value;
    result = aocs_configure_mac(aocs);
  } else
    result = MTLK_ERR_PARAMS;
  return result;
}

int __MTLK_IFUNC
mtlk_aocs_get_penalty_enabled(mtlk_aocs_t *aocs)
{
  return aocs->config.use_tx_penalties;
}

int __MTLK_IFUNC
mtlk_aocs_set_penalty_enabled(mtlk_aocs_t *aocs, BOOL value)
{
  aocs->config.use_tx_penalties = value ? TRUE : FALSE;
  ILOG2_S("Power penalties are %s", aocs->config.use_tx_penalties ? "enabled" : "disabled");
  /* update tx power penalties list */
  aocs_update_tx_power_penalty(aocs);
  /* update AOCS table */
  aocs_update_channels_data(aocs);
  return MTLK_ERR_OK;
}

/****************************************************************************
* TCP AOCS algorithm related stuff
****************************************************************************/
int __MTLK_IFUNC
mtlk_aocs_set_measurement_window (mtlk_aocs_t *aocs, uint16 val)
{
  if (aocs->config.tcp.measurement_window != val &&
      mtlk_aocs_get_type(aocs) == MTLK_AOCST_TCP) {
    return MTLK_ERR_NOT_READY;
  }

  aocs->config.tcp.measurement_window = val;
  return MTLK_ERR_OK;
}

uint16 __MTLK_IFUNC
mtlk_aocs_get_measurement_window (mtlk_aocs_t *aocs)
{
  return aocs->config.tcp.measurement_window;
}

int __MTLK_IFUNC
mtlk_aocs_set_troughput_threshold (mtlk_aocs_t *aocs, uint32 val)
{
  if (val < MTLK_TCP_AOCS_MIN_THROUGHPUT_THRESHOLD ||
      val > MTLK_TCP_AOCS_MAX_THROUGHPUT_THRESHOLD) {
    return MTLK_ERR_PARAMS;
  }

  if (aocs->config.tcp.throughput_threshold != val &&
      mtlk_aocs_get_type(aocs) == MTLK_AOCST_TCP) {
      return MTLK_ERR_NOT_READY; 
  }

  aocs->config.tcp.throughput_threshold = val;
  return MTLK_ERR_OK;
}

uint32 __MTLK_IFUNC
mtlk_aocs_get_troughput_threshold (mtlk_aocs_t *aocs)
{
  return aocs->config.tcp.throughput_threshold;
}

uint8 __MTLK_IFUNC
mtlk_aocs_get_spectrum_mode(mtlk_aocs_t *aocs)
{
  return aocs->config.spectrum_mode;
}

void __MTLK_IFUNC
mtlk_aocs_set_spectrum_mode(mtlk_aocs_t *aocs, uint8 spectrum_mode)
{
  aocs->config.spectrum_mode = spectrum_mode;
}

uint16 __MTLK_IFUNC
mtlk_aocs_get_cur_channel(mtlk_aocs_t *aocs)
{
  return aocs->cur_channel;
}

void __MTLK_IFUNC
mtlk_aocs_set_dbg_non_occupied_period(mtlk_aocs_t *aocs, int8 dbg_non_occupied_period)
{
  aocs->dbg_non_occupied_period = dbg_non_occupied_period;
}

int8 __MTLK_IFUNC
mtlk_aocs_get_dbg_non_occupied_period(mtlk_aocs_t *aocs)
{
  return aocs->dbg_non_occupied_period;
}

void __MTLK_IFUNC
mtlk_aocs_set_config_is_ht(mtlk_aocs_t *aocs, BOOL is_ht)
{
  aocs->config.is_ht = is_ht;
}

void __MTLK_IFUNC
mtlk_aocs_set_config_frequency_band(mtlk_aocs_t *aocs, uint8 frequency_band)
{
  aocs->config.frequency_band = frequency_band;
  if (MTLK_HW_BAND_2_4_GHZ == aocs->config.frequency_band) {
    ILOG0_V("Overriding AOCS type for 2.4 GHz (NONE)");
    mtlk_aocs_set_type(aocs, MTLK_AOCST_NONE);
  }
}

uint16 __MTLK_IFUNC
mtlk_aocs_get_channel_availability_check_time(mtlk_aocs_t *aocs)
{
  return aocs->channel_availability_check_time;
}

int __MTLK_IFUNC
mtlk_aocs_get_win_time(mtlk_aocs_t *aocs)
{
  return aocs->config.udp.aocs_window_time_ms;
}

int __MTLK_IFUNC
mtlk_aocs_set_win_time(mtlk_aocs_t *aocs, uint32 value)
{
  aocs->config.udp.aocs_window_time_ms = value;
  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_aocs_get_msdu_win_thr(mtlk_aocs_t *aocs)
{
  return aocs->config.udp.msdu_per_window_threshold;
}

int __MTLK_IFUNC
mtlk_aocs_set_msdu_win_thr(mtlk_aocs_t *aocs, uint32 value)
{
  aocs->config.udp.msdu_per_window_threshold = value;
  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_aocs_get_msdu_debug_enabled(mtlk_aocs_t *aocs)
{
  return aocs->config.udp.msdu_debug_enabled;
}

int __MTLK_IFUNC
mtlk_aocs_set_msdu_debug_enabled(mtlk_aocs_t *aocs, uint32 value)
{
  aocs->config.udp.msdu_debug_enabled = value == 0 ? FALSE : TRUE;
  return MTLK_ERR_OK;
}

static void
aocs_update_effective_ac(mtlk_aocs_t *aocs)
{
  int i;
  for(i = 0; i < NTS_PRIORITIES; i++)
  {
    aocs->aocs_effective_tx_ac[i] = 
        aocs->config.udp.aocs_enabled_tx_ac[i] && AOCS_UDP_ENABLED(aocs);
    aocs->aocs_effective_rx_ac[i] = 
        aocs->config.udp.aocs_enabled_rx_ac[i] && AOCS_UDP_ENABLED(aocs);
  }
}

int __MTLK_IFUNC
mtlk_aocs_get_type(mtlk_aocs_t *aocs)
{
  return aocs->config.type;
}

int __MTLK_IFUNC
mtlk_aocs_set_type (mtlk_aocs_t *aocs, uint32 value)
{
  int res = MTLK_ERR_UNKNOWN;

#ifdef MBSS_FORCE_NO_CHANNEL_SWITCH
  if (value != MTLK_AOCST_NONE) 
#else
  if (value >= MTLK_AOCST_LAST) 
#endif
  {
    res = MTLK_ERR_PARAMS;
    goto end;
  }

  if (MTLK_HW_BAND_2_4_GHZ == aocs->config.frequency_band && value != MTLK_AOCST_NONE) {
    WLOG_V("AOCS isn't supported at 2.4 GHz");
    res = MTLK_ERR_PARAMS;
    goto end;
  }

  if (aocs->config.type == (mtlk_aocs_type_e)value) {
    res = MTLK_ERR_OK;
    goto end;
  }

  res = MTLK_ERR_OK;
  mtlk_osal_mutex_acquire(&aocs->watchdog_mutex);
  if (aocs->watchdog_started) {
    /* Stop previously started watchdog */
    _mtlk_aocs_stop_preconfigured_watchdog(aocs);
  }
   
  /* Set new watchdog type */
  aocs->config.type = (mtlk_aocs_type_e)value;
  /* Update UDP watchdog limits */
  aocs_update_effective_ac(aocs);

  if (aocs->watchdog_started) {
    /* Start requested watchdog */
    res = _mtlk_aocs_start_preconfigured_watchdog(aocs);
  }
  mtlk_osal_mutex_release(&aocs->watchdog_mutex);

end:
  return res;
}

BOOL mtlk_aocs_is_type_none(mtlk_aocs_t *aocs)
{
  MTLK_ASSERT(NULL != aocs);
  return (BOOL)(MTLK_AOCST_NONE == mtlk_aocs_get_type(aocs));
}

void __MTLK_IFUNC
mtlk_aocs_get_restricted_ch(mtlk_aocs_t *aocs, uint8 *restr_chnl)
{
  mtlk_slist_entry_t *list_entry;
  mtlk_slist_entry_t *list_head;
  mtlk_aocs_restricted_chnl_t *current_entry;
  uint32 i=0;

  memset(restr_chnl, MTLK_CHANNEL_NOT_USED, MAX_CHANNELS*sizeof(uint8));
  /* iterate over the restricted channel list */
  mtlk_slist_foreach(&aocs->restricted_chnl_list, list_entry, list_head) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(list_entry,
      mtlk_aocs_restricted_chnl_t, link_entry);

    restr_chnl[i] = current_entry->channel;

    if (MAX_CHANNELS == ++i) {
      return;
    }
  }
}

void __MTLK_IFUNC
mtlk_aocs_get_msdu_tx_ac(mtlk_aocs_t *aocs, mtlk_aocs_ac_t *ac)
{
  mtlk_osal_mutex_acquire(&aocs->watchdog_mutex);
  memcpy(ac->ac, aocs->config.udp.aocs_enabled_tx_ac, NTS_PRIORITIES*sizeof(BOOL));
  mtlk_osal_mutex_release(&aocs->watchdog_mutex);
}

void __MTLK_IFUNC
mtlk_aocs_get_msdu_rx_ac(mtlk_aocs_t *aocs, mtlk_aocs_ac_t *ac)
{
  mtlk_osal_mutex_acquire(&aocs->watchdog_mutex);
  memcpy(ac->ac, aocs->config.udp.aocs_enabled_rx_ac, NTS_PRIORITIES*sizeof(BOOL));
  mtlk_osal_mutex_release(&aocs->watchdog_mutex);
}

static int
_mtlk_aocs_set_msdu_ac(mtlk_aocs_ac_t *ac, BOOL *ac_values_array)
{
  int i, result = MTLK_ERR_OK;

  for (i = 0; i < NTS_PRIORITIES; i++) {
    if (AC_NOT_USED == ac->ac[i]) {
      continue;
    }
    ac_values_array[i] = ac->ac[i];
  }
  
  return result;
}

int __MTLK_IFUNC
mtlk_aocs_set_msdu_tx_ac(mtlk_aocs_t *aocs, mtlk_aocs_ac_t *ac)
{
  int res = _mtlk_aocs_set_msdu_ac(ac, aocs->config.udp.aocs_enabled_tx_ac);
  if(MTLK_ERR_OK == res)
    aocs_update_effective_ac(aocs);
  return res;
}

int __MTLK_IFUNC
mtlk_aocs_set_msdu_rx_ac(mtlk_aocs_t *aocs, mtlk_aocs_ac_t *ac)
{
  int res = _mtlk_aocs_set_msdu_ac(ac, aocs->config.udp.aocs_enabled_rx_ac);
  if(MTLK_ERR_OK == res)
    aocs_update_effective_ac(aocs);
  return res;
}

static void
aocs_update_restricted (mtlk_aocs_t *aocs)
{
  mtlk_slist_entry_t *list_entry_table;
  mtlk_aocs_table_entry_t *entry_table;

  ILOG4_V("Update restricted channels in the table");
  /* iterate over AOCS table:
   * - look up for each channel from AOCS table in restricted channels list
   * - if found update the AOCS table (dont_use = TRUE) */
  list_entry_table = mtlk_slist_begin(&aocs->table);
  while (list_entry_table) {
    entry_table = MTLK_LIST_GET_CONTAINING_RECORD(list_entry_table,
      mtlk_aocs_table_entry_t, link_entry);
    entry_table->dont_use = FALSE;
    if (aocs_is_restricted_chnl(aocs, entry_table->chnl_primary->stat.channel)) {
      entry_table->dont_use = TRUE;
    } else if ((NULL != entry_table->chnl_secondary) &&
               aocs_is_restricted_chnl(aocs, entry_table->chnl_secondary->stat.channel)) {
      entry_table->dont_use = TRUE;
    }

    if (entry_table->dont_use) {
      if (NULL != entry_table->chnl_secondary) {
        ILOG2_DD("Restricting channel %d (%d) usage",
          entry_table->chnl_primary->stat.channel, entry_table->chnl_secondary->stat.channel);
      } else {
        ILOG2_D("Restricting channel %d usage", entry_table->chnl_primary->stat.channel);
      }
    }

    list_entry_table = mtlk_slist_next(list_entry_table);
  }
}

void __MTLK_IFUNC
mtlk_aocs_set_restricted_ch(mtlk_aocs_t *aocs, uint8 *restr_chnl)
{
  uint32 i;
  mtlk_aocs_restricted_chnl_t *new_entry;

  /* free restricted channel list */
  aocs_free_restricted_chnl(aocs);

  for (i=0; i<MAX_CHANNELS; i++) {
    if (MTLK_CHANNEL_NOT_USED == restr_chnl[i]) {
      break;
    }

    /* add channel to the list */
    new_entry = (mtlk_aocs_restricted_chnl_t *)mtlk_osal_mem_alloc(sizeof(*new_entry),
                                                                   MTLK_MEM_TAG_AOCS_RESTR_CHNL);
    if (new_entry == NULL) {
      ELOG_V("No memory to add new resticted channel entry");
      goto FINISH;
    }

    memset(new_entry, 0, sizeof(*new_entry));
    new_entry->channel = restr_chnl[i];
    
    /* now add to the list */
    mtlk_slist_push(&aocs->restricted_chnl_list, &new_entry->link_entry);

    ILOG4_D("added new restricted channel %d", new_entry->channel);
  }
FINISH:
  /* now update the table */
  aocs_update_restricted(aocs);
}

static mtlk_slist_entry_t *
aocs_find_tx_power_penalty (mtlk_aocs_t *aocs, uint16 freq)
{
  mtlk_slist_entry_t *current_list_entry;
  mtlk_aocs_tx_penalty_t *current_entry;

  /* iterate over the list */
  current_list_entry = mtlk_slist_begin(&aocs->tx_penalty_list);
  while (current_list_entry) {
    current_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry,
      mtlk_aocs_tx_penalty_t, link_entry);
    if (current_entry->freq == freq)
      return current_list_entry;
    current_list_entry = mtlk_slist_next(current_list_entry);
  }
  return NULL;
}

int __MTLK_IFUNC
mtlk_aocs_set_tx_penalty (mtlk_aocs_t *aocs, int32 *v, int nof_ints)
{
  mtlk_slist_entry_t *existing_list_entry;
  mtlk_aocs_tx_penalty_t *list_entry;
  int32 freq, penalty;
  int result = MTLK_ERR_OK;

  /* check if we have 2 ints passed to us: v[0] - freq, v[1] - dBm */
  if (nof_ints != MTLK_AOCS_PENALTIES_BUFSIZE) {
    result = MTLK_ERR_PARAMS;
    goto FINISH;
  }
  freq = v[0];
  penalty = v[1];
  if (penalty < 0) {
    /* remove existing penalty from the list */
    existing_list_entry = aocs_find_tx_power_penalty(aocs, (uint16) freq);
    if (existing_list_entry == NULL) {
      ILOG2_D("Entry freq %d not found", freq);
      result = MTLK_ERR_PARAMS;
      goto FINISH;
    }
    if (mtlk_slist_remove(&aocs->tx_penalty_list, existing_list_entry)) {
      list_entry = MTLK_LIST_GET_CONTAINING_RECORD(existing_list_entry,
        mtlk_aocs_tx_penalty_t, link_entry);
      mtlk_osal_mem_free(list_entry);
      goto FINISH;
    }
  }

  /* try to find freq in the list */
  existing_list_entry = aocs_find_tx_power_penalty(aocs, (uint16)freq);
  if (existing_list_entry) {
    list_entry = MTLK_LIST_GET_CONTAINING_RECORD(existing_list_entry,
      mtlk_aocs_tx_penalty_t, link_entry);

    list_entry->penalty = (uint16) penalty;
    ILOG4_DD("updated TxPowerPenalty for freq %d, value %d",
      list_entry->freq, list_entry->penalty);
  } else {
    list_entry = (mtlk_aocs_tx_penalty_t *)mtlk_osal_mem_alloc(sizeof(*list_entry),
      MTLK_MEM_TAG_AOCS_PENALTY);
    if (list_entry == NULL) {
      ELOG_V("No memory to add new TxPowerPenalty entry");
      result = MTLK_ERR_NO_MEM;
      goto FINISH;
    }
    memset(list_entry, 0, sizeof(*list_entry));
    list_entry->freq = (uint16) freq;
    list_entry->penalty = (uint16) penalty;
    /* now add to the list */
    mtlk_slist_push(&aocs->tx_penalty_list, &list_entry->link_entry);
    ILOG4_DD("added TxPowerPenalty for freq %d, value %d",
      list_entry->freq, list_entry->penalty);
  }
FINISH:
  return result;
}

static int
aocs_select_channel (mtlk_aocs_t *aocs, mtlk_aocs_evt_select_t *channel_data)
{
  int result = MTLK_ERR_OK;

  /* reset exclude flag if it was set before */
  aocs_reset_exclude_flag(aocs);
  /* remove all forcibly added channels */
  aocs_remove_forced_channels(aocs);
  /* update channel data */
  aocs_update_channels_data(aocs);
  if (channel_data->channel > 0) {
    /* a2k: use user configured bonding */
    channel_data->bonding = mtlk_core_get_bonding(mtlk_vap_get_core(aocs->vap_handle));
    result = aocs_channel_is_valid(aocs, channel_data->channel, channel_data->bonding);
  } else {
    /* make sure we will not try to change bonding if it is not appropriate */
    channel_data->bonding = aocs->bonding;

    switch (aocs->config.frequency_band) {
    case MTLK_HW_BAND_5_2_GHZ:
    case MTLK_HW_BAND_2_4_GHZ:
      result = aocs_select_channel_by_spectrum(aocs, channel_data);
      /* in case no channel could be selected - choose best of the worst */
      if (result == MTLK_ERR_AOCS_FAILED) {
        aocs_get_loweset_timeout_channel(aocs, &channel_data->channel);
        if (channel_data->channel) {
          /* Restore bonding configuration */
          channel_data->bonding = aocs->bonding;
          channel_data->criteria = CHC_LOWEST_TIMEOUT;
          result = MTLK_ERR_OK;
        } else {
          result = MTLK_ERR_AOCS_FAILED;
        }
      }
      break;
    default:
      ELOG_D("Unsupported frequency band: %d", aocs->config.frequency_band);
      result = MTLK_ERR_AOCS_FAILED;
      break;
    }
  }
  if (result == MTLK_ERR_OK) {
    if (aocs->config.spectrum_mode == SPECTRUM_40MHZ)
      ILOG4_DS("Channel %d, bonding %s", channel_data->channel, aocs_bonding_to_str(channel_data->bonding));
    else
      ILOG4_D("Channel %d", channel_data->channel);
  }
  return result;
}

static void
aocs_add_to_history(mtlk_aocs_t *aocs, uint16 new_channel, 
                    mtlk_aocs_channel_switch_reasons_t reason, channel_criteria_t criteria,
                    channel_criteria_details_t *criteria_details)
{
  mtlk_sw_info_t switch_info;

  aocs->last_switch_reason = reason;
  /* Add entry to the channel switch history */
  switch_info.primary_channel = new_channel;
  if (aocs->config.spectrum_mode == SPECTRUM_40MHZ) {
      mtlk_aocs_table_entry_t* entry =  aocs_find_channel_table(aocs, new_channel, 0);
      MTLK_ASSERT(NULL != entry);
      MTLK_ASSERT(NULL != entry->chnl_secondary);
      switch_info.secondary_channel = entry->chnl_secondary->stat.channel;
  } else switch_info.secondary_channel = 0; /* No secondary channel */

  switch_info.reason = reason;
  switch_info.criteria = criteria;
  switch_info.criteria_details = *criteria_details;
  mtlk_aocs_history_add(&aocs->aocs_history, &switch_info);
};
/*****************************************************************************
**
** NAME         mtlk_aocs_indicate_event
**
** PARAMETERS   aocs                AOCS context
**              event               AOCS event
**              data                data of the event
**
** RETURNS      MTLK_ERR...
**
** DESCRIPTION  This function is an AOCS state machine entry - handles events
**
******************************************************************************/
int __MTLK_IFUNC
mtlk_aocs_indicate_event (mtlk_aocs_t *aocs, mtlk_aocs_event_e event, void *data, uint32 data_size)
{
  int result = MTLK_ERR_OK;

  switch (event) {
  case MTLK_AOCS_EVENT_RADAR_DETECTED:
    /* radar was detected on current channel */
    result = aocs_on_radar_detected(aocs);
    break;
  case MTLK_AOCS_EVENT_SELECT_CHANNEL:
    {
      mtlk_aocs_evt_select_t *aocs_data = (mtlk_aocs_evt_select_t *)data;

      /* if HT is not enabled, then do not allow selection of 40MHz channels */
      if (!aocs->config.is_ht) {
        WLOG_V("Cannot auto select channel because HT is off: forcing 20MHz selection");
        aocs->config.is_auto_spectrum = FALSE;
        aocs->config.spectrum_mode = SPECTRUM_20MHZ;
      }
      /* request to select a new channel or to validate specified */
      result = aocs_select_channel(aocs, aocs_data);
#if 0
      /* if we had a non-zero channel and were not able to validate it then
         try to select new channel automatically */
      if ( (channel > 0) && (result != MTLK_ERR_OK)) {
        ILOG0_D("Forcing automatic channel selection as channel %d is not valid",
          channel);
        aocs_data->channel = 0;
        if (aocs->config.is_auto_spectrum) {
          /* rebuild table as it is 20MHz now */
          aocs->config.spectrum_mode = SPECTRUM_40MHZ;
          if (mtlk_aocs_start(aocs, TRUE, aocs->is_20_40_coex_active) != MTLK_ERR_OK) {
            ELOG_V("Failed to rebuild AOCS for selection");
            result = MTLK_ERR_AOCS_FAILED;
            break;
          }
        }
        aocs_update_channels_data(aocs);
        result = aocs_select_channel(aocs, aocs_data);
      }
#endif
      if (result == MTLK_ERR_OK) {
        if (aocs->config.spectrum_mode == SPECTRUM_40MHZ) {
          aocs_change_bonding(aocs, aocs_data->bonding);
          ILOG2_DS("Selected channel %d, %s bonding", aocs_data->channel, aocs_bonding_to_str(aocs_data->bonding));
        } else {
          ILOG2_D("Selected channel %d", aocs_data->channel);
        }
        /* change current channel */
        aocs->cur_channel = aocs_data->channel;
        /* notify about the new bonding */
        if ((NULL != aocs->config.api.on_bonding_change) &&
            (mtlk_core_get_bonding(mtlk_vap_get_core(aocs->vap_handle)) != aocs->bonding) ) {
          aocs->config.api.on_bonding_change(aocs->vap_handle, aocs->bonding);
        }
        /* notify about the new channel */
        if (NULL != aocs->config.api.on_channel_change) {
          aocs->config.api.on_channel_change(aocs->vap_handle, aocs->cur_channel);
        }
        /* notify about the new spectrum */
        if ((NULL != aocs->config.api.on_spectrum_change) && aocs->config.is_auto_spectrum) {
          aocs->config.api.on_spectrum_change(aocs->vap_handle, aocs->config.spectrum_mode);
        }
        /* we have selected a channel, so auto is no more applicable */
        aocs->config.is_auto_spectrum = FALSE;
      } else {
        ILOG2_V("AOCS failed to select/validate channel");
        result = MTLK_ERR_AOCS_FAILED;
      }
    }
    break;
  case MTLK_AOCS_EVENT_INITIAL_SELECTED:
    {
      mtlk_aocs_evt_select_t *aocs_data = (mtlk_aocs_evt_select_t *)data;
      ILOG4_V("Initial channel selected");
      /* Add channel switch to history */
      aocs_add_to_history(aocs, aocs_data->channel, aocs_data->reason, 
        aocs_data->criteria, &aocs_data->criteria_details);
    }
    break;
  case MTLK_AOCS_EVENT_SWITCH_STARTED:
    {
      mtlk_aocs_evt_select_t *aocs_data = (mtlk_aocs_evt_select_t *)data;
      ILOG4_V("Started channel switch");

      /* Add channel switch to history */
      aocs_add_to_history(aocs, aocs_data->channel, aocs_data->reason, 
        aocs_data->criteria, &aocs_data->criteria_details);

      aocs->ch_sw_in_progress = TRUE;

      /* timer will be restarted again in case we still have big channel load */
      _mtlk_aocs_stop_msdu_timer(aocs);
    }
    break;
  case MTLK_AOCS_EVENT_SWITCH_DONE:
    {
      int i;
      mtlk_aocs_evt_switch_t *aocs_data = (mtlk_aocs_evt_switch_t *)data;
      /* channel switch done */
      ILOG4_D("Channel switch done with code %d", aocs_data->status);

      aocs->ch_sw_in_progress = FALSE;
      /* timer will be restarted again in case we still have big channel load */
      _mtlk_aocs_stop_msdu_timer(aocs);

      /* update lower thresholds */
      mtlk_osal_lock_acquire(&aocs->lock);
      for(i = 0; i < NTS_PRIORITIES; i++)
      {
        _mtlk_aocs_move_thresholds(aocs, i, aocs_data->sq_used[i]);
        _mtlk_aocs_lower_threshold_crossed(aocs, i);
      }
      mtlk_osal_lock_release(&aocs->lock);
      mtlk_core_abilities_enable_vap_ops(aocs->vap_handle);
    }
    break;
  case MTLK_AOCS_EVENT_TCP_IND:
    aocs_on_tcp_ind(aocs, (UMI_AOCS_IND *)data, data_size);
    break;
  default:
    break;
  }
  return result;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
notify_clb(mtlk_handle_t clb_usr_data, mtlk_txmm_data_t* data, mtlk_txmm_clb_reason_e reason)
{
  UMI_GENERIC_MAC_REQUEST* msg = (UMI_GENERIC_MAC_REQUEST *)data->payload;

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
    ELOG_D("Reason for TXMM callback is %d", reason);
  else if (msg->retStatus != UMI_OK)
    ELOG_D("Status is %d", msg->retStatus);

  return MTLK_TXMM_CLBA_FREE;
}

static int
_mtlk_aocs_notify_mac(mtlk_aocs_t *aocs, uint32 req_type, uint32 data)
{
  UMI_GENERIC_MAC_REQUEST* req;
  int result = MTLK_ERR_OK;
  mtlk_txmm_data_t *man_entry = NULL;

  if (!aocs->config.udp.msdu_debug_enabled)
    goto FINISH;

  man_entry = mtlk_txmm_msg_get_empty_data(&aocs->msdu_debug_man_msg, aocs->config.txmm);
  if (NULL == man_entry) {
    result = MTLK_ERR_AOCS_FAILED;
    goto FINISH;
  }
  man_entry->id           = UM_MAN_GENERIC_MAC_REQ;
  man_entry->payload_size = sizeof(*req);
  req                   = (UMI_GENERIC_MAC_REQUEST *)man_entry->payload;
  memset(req, 0, sizeof(*req));
  req->data[0]          = HOST_TO_MAC32(data);
  req->opcode           = HOST_TO_MAC32(req_type);
  req->size             = HOST_TO_MAC32(sizeof(uint32));
  req->action           = HOST_TO_MAC32(MT_REQUEST_SET);
  /* we don't care if the message will be delivered */
  if (mtlk_txmm_msg_send(&aocs->msdu_debug_man_msg, notify_clb, HANDLE_T(NULL), TXMM_DEFAULT_TIMEOUT) < MTLK_ERR_OK) {
    result = MTLK_ERR_AOCS_FAILED;
    goto FINISH;
  }
  result = MTLK_ERR_OK;

FINISH:
  return result;
}

static void __INLINE
_mtlk_aocs_notify_mac_timer(mtlk_aocs_t *aocs, int timer_event)
{
    if (aocs->config.udp.msdu_debug_enabled) {
        _mtlk_aocs_notify_mac(aocs, timer_event, mtlk_osal_atomic_get(&aocs->msdu_counter));
    }
}

static uint8 __INLINE
_mtlk_aocs_calc_confirm_rank(int msdu_confirm_threshold, int msdu_confirmed)
{
    return ((msdu_confirm_threshold - msdu_confirmed)*100) 
        / (msdu_confirm_threshold);
};

static uint32
_mtlk_aocs_msdu_tmr (mtlk_osal_timer_t *timer,
               mtlk_handle_t      clb_usr_data)
{
  mtlk_aocs_t *aocs = (mtlk_aocs_t *)clb_usr_data;
  int msdu_counter;

  MTLK_UNREFERENCED_PARAM(timer);

  _mtlk_aocs_notify_mac_timer(aocs, MAC_OCS_TIMER_TIMEOUT);
  aocs->msdu_timer_running = FALSE;

  msdu_counter = mtlk_osal_atomic_get(&aocs->msdu_counter);
  if (msdu_counter < aocs->config.udp.msdu_per_window_threshold) {
    ILOG2_DDD("Channel switch on MSDU ack: channel: %d acked %d thr %d",
      aocs->cur_channel, msdu_counter, aocs->config.udp.msdu_per_window_threshold);

    mtlk_osal_lock_acquire(&aocs->lock);
    _mtlk_aocs_set_confirm_rank(aocs, aocs->cur_channel,
    _mtlk_aocs_calc_confirm_rank(aocs->config.udp.msdu_per_window_threshold, msdu_counter));

    aocs_optimize_channel(aocs, SWR_LOW_THROUGHPUT);

    mtlk_osal_lock_release(&aocs->lock);
    goto FINISH;
  }

 FINISH:
  mtlk_osal_atomic_set(&aocs->msdu_counter, 0);
  return 0;
}

BOOL __MTLK_IFUNC
mtlk_aocs_set_auto_spectrum(mtlk_aocs_t *aocs, uint8 spectrum)
{
  aocs->config.is_auto_spectrum =
    (spectrum != SPECTRUM_20MHZ) && (spectrum != SPECTRUM_40MHZ);
  return aocs->config.is_auto_spectrum;
}

static void __INLINE /* HOTPATH */
_mtlk_aocs_start_msdu_timer (mtlk_aocs_t *aocs)
{
  if(!aocs->msdu_timer_running) {
    aocs->msdu_timer_running = TRUE;
    mtlk_osal_timer_set(&aocs->msdu_timer, aocs->config.udp.aocs_window_time_ms);
    _mtlk_aocs_notify_mac_timer(aocs, MAC_OCS_TIMER_START);
  }
}

static void __INLINE /*HOTPATH*/
_mtlk_aocs_stop_msdu_timer (mtlk_aocs_t *aocs)
{
  if(aocs->msdu_timer_running) {
    _mtlk_aocs_notify_mac_timer(aocs, MAC_OCS_TIMER_STOP);
    mtlk_osal_timer_cancel(&aocs->msdu_timer);
    aocs->msdu_timer_running = FALSE;
  }
}

#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
static void __INLINE /*HOTPATH*/
_mtlk_aocs_chk_msdu_threshold(mtlk_aocs_t *aocs)
{
  int i;
  uint32 nof_msdu_used = 0;
  const uint16* nof_msdu_used_by_ac = aocs->tx_data_nof_used_bds;

  for(i = 0; i < NTS_PRIORITIES; i++) {
    MTLK_ASSERT( (0 == aocs->aocs_effective_tx_ac[i]) ||
               (1 == aocs->aocs_effective_tx_ac[i]) );
    nof_msdu_used += nof_msdu_used_by_ac[i] * aocs->aocs_effective_tx_ac[i];
  }

  if (nof_msdu_used < aocs->config.udp.msdu_threshold_aocs) {
    _mtlk_aocs_stop_msdu_timer(aocs);
  } else if (!aocs->ch_sw_in_progress) {
    /* If channel switch is in progress we don't need to start the timer */
    /* If timer is already running we don't need to restart it */
    _mtlk_aocs_start_msdu_timer(aocs);
  }
}
#endif

static void __INLINE
_mtlk_aocs_move_thresholds(mtlk_aocs_t *aocs, uint8 ac, uint16 lower_threshold)
{
  aocs->lower_threshold[ac] = lower_threshold;
  aocs->higher_threshold[ac] = lower_threshold + aocs->config.udp.threshold_window;
}

static void __INLINE
_mtlk_aocs_lower_threshold_crossed(mtlk_aocs_t *aocs, uint8 ac)
{
  aocs->lower_threshold_crossed_time[ac] = mtlk_osal_timestamp();
  mtlk_osal_atomic_set(&aocs->msdu_counter, 0);
}

static void __INLINE /*HOTPATH*/
__aocs_chk_sqsize_threshold(mtlk_aocs_t *aocs, uint16 sq_used, 
                            uint8 ac)
{
  if (sq_used == aocs->lower_threshold[ac]) {
    _mtlk_aocs_lower_threshold_crossed(aocs, ac);
  } else if (sq_used == aocs->higher_threshold[ac]) {
     mtlk_osal_ms_diff_t time_diff =
       mtlk_osal_ms_time_diff(mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp()),
       mtlk_osal_timestamp_to_ms(aocs->lower_threshold_crossed_time[ac]));
     uint16 msdu_per_window = (time_diff != 0) 
       ? (mtlk_osal_atomic_get(&aocs->msdu_counter) * aocs->config.udp.aocs_window_time_ms) / time_diff
       : 0;

     _mtlk_aocs_move_thresholds(aocs, ac, aocs->higher_threshold[ac]);
     _mtlk_aocs_lower_threshold_crossed(aocs, ac);

     if(msdu_per_window < aocs->config.udp.msdu_per_window_threshold) {
       /* update confirm rank */
       _mtlk_aocs_set_confirm_rank(aocs, aocs->cur_channel,
       _mtlk_aocs_calc_confirm_rank(aocs->config.udp.msdu_per_window_threshold, msdu_per_window));

       /* initiate channel switch */
       aocs_optimize_channel(aocs, SWR_HIGH_SQ_LOAD);
       return;
     }
  }
}

#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
void __MTLK_IFUNC /*HOTPATH*/
mtlk_aocs_on_tx_msdu_sent(mtlk_aocs_t *aocs, uint8 ac,
                          uint16 sq_size_limit, uint16 sq_used)
{
  int new_lower_threshold;

  MTLK_ASSERT(ac < NTS_PRIORITIES);

  if (!aocs->aocs_effective_tx_ac[ac]) {
    return;
  }

  mtlk_osal_lock_acquire(&aocs->lock);

  /* If upper threshold is above the send queue capacity,
  AOCS uses timer-based algorithm */
  if( aocs->higher_threshold[ac] > sq_size_limit ) {
    _mtlk_aocs_chk_msdu_threshold(aocs);
  }

  new_lower_threshold = 
    MAX((int) aocs->lower_threshold[ac] - (int) aocs->config.udp.threshold_window, 
       (int) aocs->config.udp.lower_threshold);

  if( ((int) sq_used < new_lower_threshold) && 
      (aocs->lower_threshold[ac] > new_lower_threshold) ) {
      _mtlk_aocs_move_thresholds(aocs, ac, new_lower_threshold);
  }

  mtlk_osal_lock_release(&aocs->lock);
}

void __MTLK_IFUNC /*HOTPATH*/
mtlk_aocs_on_tx_msdu_enqued(mtlk_aocs_t *aocs, uint16 ac, 
                            uint16 sq_size, uint16 sq_size_limit)
{
  MTLK_ASSERT(ac < NTS_PRIORITIES);

  if (!aocs->aocs_effective_tx_ac[ac]) {
    return;
  }

  /* If upper threshold is below the send queue capacity,
  AOCS uses SendQueue-based algorithm */
  mtlk_osal_lock_acquire(&aocs->lock);

  if( aocs->higher_threshold[ac] <= sq_size_limit ) {
    __aocs_chk_sqsize_threshold(aocs, sq_size, ac);
  }

  mtlk_osal_lock_release(&aocs->lock);
}

void __MTLK_IFUNC /*HOTPATH*/
mtlk_aocs_on_tx_msdu_returned (mtlk_aocs_t *aocs, uint8 ac)
{
  MTLK_ASSERT(ac < NTS_PRIORITIES);
  if(aocs->aocs_effective_tx_ac[ac]) {
    mtlk_osal_atomic_inc(&aocs->msdu_counter);
  }
}

void __MTLK_IFUNC /*HOTPATH*/
mtlk_aocs_on_rx_msdu (mtlk_aocs_t *aocs, uint8 ac)
{
  MTLK_ASSERT(ac < NTS_PRIORITIES);
  if(aocs->aocs_effective_rx_ac[ac]) {
    mtlk_osal_atomic_inc(&aocs->msdu_counter);
  }
}
#endif

void __MTLK_IFUNC 
mtlk_aocs_enable_smrequired(mtlk_aocs_t *aocs)
{
  aocs->disable_sm_required = FALSE;
}

void __MTLK_IFUNC 
mtlk_aocs_disable_smrequired(mtlk_aocs_t *aocs)
{
  aocs->disable_sm_required = TRUE;
}

BOOL __MTLK_IFUNC
mtlk_aocs_is_smrequired_disabled(mtlk_aocs_t *aocs)
{     
  return aocs->disable_sm_required;
}

mtlk_aocs_channel_switch_reasons_t __MTLK_IFUNC
mtlk_aocs_get_last_switch_reason(mtlk_aocs_t *aocs)
{
  return aocs->last_switch_reason;
}

#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
void __MTLK_IFUNC
mtlk_aocs_msdu_tx_inc_nof_used(mtlk_aocs_t *aocs, uint8 ac)
{
  aocs->tx_data_nof_used_bds[ac]++;
  ILOG4_DD("-> core->tx_data_nof_used_bds[%d] = %d", ac, aocs->tx_data_nof_used_bds[ac]);
}

void __MTLK_IFUNC
mtlk_aocs_msdu_tx_dec_nof_used(mtlk_aocs_t *aocs, uint8 ac)
{
  aocs->tx_data_nof_used_bds[ac]--;
  ILOG4_DD("<- core->tx_data_nof_used_bds[%d] = %d", ac, aocs->tx_data_nof_used_bds[ac]);
}
#endif

#ifdef AOCS_DEBUG
void mtlk_aocs_debug_update_cl(mtlk_aocs_t *aocs, uint32 cl)
{
  MTLK_ASSERT(NULL != aocs);

  if ((cl < 0) || (cl > 100)) {
    ELOG_V("Channel load must be in [0, 100] range");
  } else {
    mtlk_aocs_update_cl(aocs, aocs->cur_channel, cl);
  }

  mtlk_osal_lock_acquire(&aocs->lock);
  aocs_optimize_channel(aocs, SWR_CHANNEL_LOAD_CHANGED);
  mtlk_osal_lock_release(&aocs->lock);
}
#endif
