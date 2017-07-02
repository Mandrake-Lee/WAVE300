#ifndef __MTLK_AOCS_H__
#define __MTLK_AOCS_H__

#include "mtlk_osal.h"

#include "mhi_umi.h"
#include "mtlklist.h"

#include "txmm.h"
#include "frame.h"

#include "aocshistory.h"
#include "mtlk_clipboard.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

#ifdef MTCFG_DEBUG
#define AOCS_DEBUG
#endif

#define MTLK_AOCS_PENALTIES_BUFSIZE 2

typedef enum _mtlk_aocs_weight_e {
  AOCS_WEIGHT_IDX_CL,
  AOCS_WEIGHT_IDX_TX,
  AOCS_WEIGHT_IDX_BSS,
  AOCS_WEIGHT_IDX_SM,
  AOCS_WEIGHT_IDX_LAST
} mtlk_aocs_weight_e;

typedef struct _mtlk_aocs_channels_stat_t {
  /* affected by 40MHz intolerance */
  BOOL   forty_mhz_int_affected;
  BOOL   forty_mhz_intolerant;
  BOOL   sm_required;
  uint16 channel;
  uint8  nof_bss;
  uint8  channel_load;
  uint8  num_20mhz_bss;
  uint8  num_40mhz_bss;
} __MTLK_IDATA mtlk_aocs_channels_stat_t;

/* channel's list entry */
typedef struct _mtlk_aocs_channel_data_t {
  mtlk_aocs_channels_stat_t stat;
  mtlk_osal_timestamp_t time_cl;

  /* linked list */
  mtlk_slist_entry_t link_entry;
} __MTLK_IDATA mtlk_aocs_channel_data_t;

/* AOCS supported events */
typedef enum _mtlk_aocs_event_e {
  /* on radar detected */
  MTLK_AOCS_EVENT_RADAR_DETECTED,
  /* request to select a new channel */
  MTLK_AOCS_EVENT_SELECT_CHANNEL,
  /* channel switch started */
  MTLK_AOCS_EVENT_SWITCH_STARTED,
  /* channel switch done */
  MTLK_AOCS_EVENT_SWITCH_DONE,
  /* initial channel selected */
  MTLK_AOCS_EVENT_INITIAL_SELECTED,
  /* MAC TCP AOCS indication received */
  MTLK_AOCS_EVENT_TCP_IND,
  MTLK_AOCS_EVENT_LAST
} mtlk_aocs_event_e;

/* on channel select event data */
typedef struct _mtlk_aocs_evt_select_t {
  uint16             channel;
  uint8              bonding;
  mtlk_aocs_channel_switch_reasons_t   reason;
  channel_criteria_t criteria;
  channel_criteria_details_t criteria_details;
} mtlk_aocs_evt_select_t;

/* API */
typedef struct _mtlk_aocs_wrap_api_t {
  /* card's context */
  void (__MTLK_IDATA *on_channel_change)(mtlk_vap_handle_t vap_handle, int channel);
  void (__MTLK_IDATA *on_bonding_change)(mtlk_vap_handle_t vap_handle, uint8 bonding);
  void (__MTLK_IDATA *on_spectrum_change)(mtlk_vap_handle_t vap_handle, int spectrum);
} __MTLK_IDATA mtlk_aocs_wrap_api_t;

struct mtlk_scan;
struct _mtlk_dot11h_t;
struct _scan_cache_t;

typedef struct _mtlk_aocs_init_t {
  mtlk_aocs_wrap_api_t *api;
  struct mtlk_scan *scan_data;
  struct _scan_cache_t *cache;
  struct _mtlk_dot11h_t *dot11h;
  mtlk_txmm_t *txmm;
  BOOL disable_sm_channels;
} mtlk_aocs_init_t;

typedef struct {
  mtlk_osal_msec_t time_ms_non_occupied_period;
  mtlk_osal_msec_t time_ms_last_clear_check;
  BOOL   radar_detected;
  BOOL   is_in_radar_timeout;
  BOOL   dont_use;
  BOOL   exclude;
  uint16 channel_primary;
  uint16 channel_secondary;
  uint16 tx_power;
  uint16 max_tx_power;
  uint16 tx_power_penalty;
  uint8  channel_load;
  uint8  scan_rank;
  uint8  confirm_rank;
  uint8  nof_bss;
  uint8  sm;
  uint8  is_noisy;
} __MTLK_IDATA mtlk_aocs_table_stat_entry_t;

typedef struct {
  uint16 freq;
  uint16 penalty;
} __MTLK_IDATA mtlk_aocs_penalties_stat_entry_t;

typedef struct _mtlk_aocs_t mtlk_aocs_t;

mtlk_aocs_t* __MTLK_IFUNC mtlk_aocs_create (mtlk_aocs_init_t *ini_data, mtlk_vap_handle_t vap_handle);
void __MTLK_IFUNC mtlk_aocs_delete (mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_start (mtlk_aocs_t *aocs, BOOL keep_chnl_info, BOOL is_20_40_coexistence_active);
void __MTLK_IFUNC mtlk_aocs_stop (mtlk_aocs_t *aocs);

int __MTLK_IFUNC mtlk_aocs_start_watchdog (mtlk_aocs_t *aocs);
void __MTLK_IFUNC mtlk_aocs_stop_watchdog (mtlk_aocs_t *aocs);

int __MTLK_IFUNC mtlk_aocs_indicate_event (mtlk_aocs_t *aocs,
  mtlk_aocs_event_e event, void *data, uint32 data_size);
int __MTLK_IFUNC mtlk_aocs_channel_in_validity_time(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_update_cl_on_scan_cfm (mtlk_aocs_t *aocs, void* data);
void __MTLK_IFUNC mtlk_aocs_update_cl (mtlk_aocs_t *aocs, uint16 channel, uint8 channel_load);
void __MTLK_IFUNC aocs_optimize_channel(mtlk_aocs_t *aocs, mtlk_aocs_channel_switch_reasons_t reason);

int __MTLK_IFUNC mtlk_aocs_get_history(mtlk_aocs_t *aocs, mtlk_clpb_t *clpb);
int __MTLK_IFUNC mtlk_aocs_get_table (mtlk_aocs_t *aocs, mtlk_clpb_t *clpb);
int __MTLK_IFUNC mtlk_aocs_get_channels (mtlk_aocs_t *aocs, mtlk_clpb_t *clpb);
int __MTLK_IFUNC mtlk_aocs_get_penalties (mtlk_aocs_t *aocs, mtlk_clpb_t *clpb);

int __MTLK_IFUNC mtlk_aocs_get_weight(mtlk_aocs_t *aocs, mtlk_aocs_weight_e index);
int __MTLK_IFUNC mtlk_aocs_set_weight(mtlk_aocs_t *aocs, mtlk_aocs_weight_e index, int32 weight);

int __MTLK_IFUNC mtlk_aocs_get_cfm_rank_sw_threshold(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_cfm_rank_sw_threshold(mtlk_aocs_t *aocs, uint8 value);
int __MTLK_IFUNC mtlk_aocs_get_scan_aging(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_scan_aging(mtlk_aocs_t *aocs, int value);
int __MTLK_IFUNC mtlk_aocs_get_confirm_rank_aging(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_confirm_rank_aging(mtlk_aocs_t *aocs, int value);
int __MTLK_IFUNC mtlk_aocs_get_afilter(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_afilter(mtlk_aocs_t *aocs, uint8 value);
int __MTLK_IFUNC mtlk_aocs_get_penalty_enabled(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_penalty_enabled(mtlk_aocs_t *aocs, BOOL value);

#ifdef AOCS_DEBUG
void mtlk_aocs_debug_update_cl(mtlk_aocs_t *aocs, uint32 cl);
#endif

int __MTLK_IFUNC mtlk_aocs_get_msdu_threshold(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_msdu_threshold(mtlk_aocs_t *aocs, uint32 value);
int __MTLK_IFUNC mtlk_aocs_get_lower_threshold(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_lower_threshold(mtlk_aocs_t *aocs, uint32 value);
int __MTLK_IFUNC mtlk_aocs_get_threshold_window(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_threshold_window(mtlk_aocs_t *aocs, uint32 value);

int __MTLK_IFUNC mtlk_aocs_get_win_time(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_win_time(mtlk_aocs_t *aocs, uint32 value);
int __MTLK_IFUNC mtlk_aocs_get_msdu_win_thr(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_msdu_win_thr(mtlk_aocs_t *aocs, uint32 value);
int __MTLK_IFUNC mtlk_aocs_get_msdu_debug_enabled(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_msdu_debug_enabled(mtlk_aocs_t *aocs, uint32 value);
int __MTLK_IFUNC mtlk_aocs_get_type(mtlk_aocs_t *aocs);
int __MTLK_IFUNC mtlk_aocs_set_type(mtlk_aocs_t *aocs, uint32 value);
BOOL __MTLK_IFUNC mtlk_aocs_is_type_none(mtlk_aocs_t *aocs);

struct _mtlk_aocs_ac_t;
void __MTLK_IFUNC mtlk_aocs_get_restricted_ch(mtlk_aocs_t *aocs, uint8 *restr_chnl);
void __MTLK_IFUNC mtlk_aocs_set_restricted_ch(mtlk_aocs_t *aocs, uint8 *restr_chnl);
size_t __MTLK_IFUNC mtlk_aocs_get_tx_penalty(mtlk_handle_t handle, char *buffer);
int __MTLK_IFUNC mtlk_aocs_set_tx_penalty (mtlk_aocs_t *aocs, int32 *v, int nof_ints);
void __MTLK_IFUNC mtlk_aocs_get_msdu_tx_ac(mtlk_aocs_t *aocs, struct _mtlk_aocs_ac_t *ac);
int __MTLK_IFUNC mtlk_aocs_set_msdu_tx_ac(mtlk_aocs_t *aocs, struct _mtlk_aocs_ac_t *ac);
void __MTLK_IFUNC mtlk_aocs_get_msdu_rx_ac(mtlk_aocs_t *aocs, struct _mtlk_aocs_ac_t *ac);
int __MTLK_IFUNC mtlk_aocs_set_msdu_rx_ac(mtlk_aocs_t *aocs, struct _mtlk_aocs_ac_t *ac);

#ifndef MBSS_FORCE_NO_AOCS_INITIAL_SELECTION
void __MTLK_IFUNC mtlk_aocs_on_bss_data_update(mtlk_aocs_t *aocs, bss_data_t *bss_data);
#endif

BOOL __MTLK_IFUNC mtlk_aocs_set_auto_spectrum(mtlk_aocs_t *aocs, uint8 spectrum);

/* NOTE: aocs_effective_tx_ac[ac] and aocs_effective_rx_ac[ac] become 0 when
 * some of the following conditions are true:
 *  - AOCS is disabled 
 *  - a non-UDP AOCS algorithm is enabled (for example, TCP)
 *  - UDP AOCS algotrithm for this AC is disabled by user
 */

#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
void __MTLK_IFUNC /*HOTPATH*/
mtlk_aocs_on_tx_msdu_sent(mtlk_aocs_t *aocs, uint8 ac,
                          uint16 sq_size_limit, uint16 sq_used);

void __MTLK_IFUNC /*HOTPATH*/
mtlk_aocs_on_tx_msdu_enqued(mtlk_aocs_t *aocs, uint16 ac, 
                            uint16 sq_size, uint16 sq_size_limit);

void __MTLK_IFUNC /*HOTPATH*/
mtlk_aocs_on_tx_msdu_returned (mtlk_aocs_t *aocs, uint8 ac);

void __MTLK_IFUNC /*HOTPATH*/
mtlk_aocs_on_rx_msdu (mtlk_aocs_t *aocs, uint8 ac);
#endif

void __MTLK_IFUNC 
mtlk_aocs_enable_smrequired(mtlk_aocs_t *aocs);

void __MTLK_IFUNC 
mtlk_aocs_disable_smrequired(mtlk_aocs_t *aocs);

BOOL __MTLK_IFUNC
mtlk_aocs_is_smrequired_disabled(mtlk_aocs_t *aocs);

mtlk_aocs_channel_switch_reasons_t __MTLK_IFUNC
mtlk_aocs_get_last_switch_reason(mtlk_aocs_t *aocs);

/****************************************************************************
 * TCP AOCS algorithm related stuff
 ****************************************************************************/
int __MTLK_IFUNC
mtlk_aocs_set_measurement_window (mtlk_aocs_t *aocs, uint16 val);

uint16 __MTLK_IFUNC
mtlk_aocs_get_measurement_window (mtlk_aocs_t *aocs);

int __MTLK_IFUNC
mtlk_aocs_set_troughput_threshold (mtlk_aocs_t *aocs, uint32 val);

uint32 __MTLK_IFUNC
mtlk_aocs_get_troughput_threshold (mtlk_aocs_t *aocs);

uint8 __MTLK_IFUNC
mtlk_aocs_get_spectrum_mode(mtlk_aocs_t *aocs);

void __MTLK_IFUNC
mtlk_aocs_set_spectrum_mode(mtlk_aocs_t *aocs, uint8 spectrum_mode);

uint16 __MTLK_IFUNC
mtlk_aocs_get_cur_channel(mtlk_aocs_t *aocs);

void __MTLK_IFUNC
mtlk_aocs_set_dbg_non_occupied_period(mtlk_aocs_t *aocs, int8 dbg_non_occupied_period);

int8 __MTLK_IFUNC
mtlk_aocs_get_dbg_non_occupied_period(mtlk_aocs_t *aocs);

void __MTLK_IFUNC
mtlk_aocs_set_config_is_ht(mtlk_aocs_t *aocs, BOOL is_ht);

void __MTLK_IFUNC
mtlk_aocs_set_config_frequency_band(mtlk_aocs_t *aocs, uint8 frequency_band);

uint16 __MTLK_IFUNC
mtlk_aocs_get_channel_availability_check_time(mtlk_aocs_t *aocs);

#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
void __MTLK_IFUNC
mtlk_aocs_msdu_tx_inc_nof_used(mtlk_aocs_t *aocs, uint8 ac);

void __MTLK_IFUNC
mtlk_aocs_msdu_tx_dec_nof_used(mtlk_aocs_t *aocs, uint8 ac);
#endif

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif
