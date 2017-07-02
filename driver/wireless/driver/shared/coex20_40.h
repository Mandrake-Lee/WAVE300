/*
 * $Id: coex20_40.h 11780 2011-10-19 13:00:19Z bogoslav $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * 20/40 coexistence feature
 * Provides transition between modes (20MHz->20/40MHz and vice versa)
 *
 * The 20/40 coexistence state machine module will export a complete façade-style interface to 
 * all externally accessible functions of the main module and its sub-modules. 
 * Calls that are to be processed by the auxiliary sub-modules rather than the main module itself, 
 * will be forwarded to the actual processor of the request. 
 * In order to implement that and in order to satisfy the internal needs of the state machine module, 
 * the auxiliary sub-modules will export interfaces of their own.
 *
 */
#ifndef __COEX20_40_H__
#define __COEX20_40_H__

#include "mhi_umi.h"
#include "mhi_umi_propr.h"
#include "mtlk_vap_manager.h"
#include "mtlk_wss.h"
#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/* coexistence-related data elements (represented by auxiliary structures) */

typedef enum
{
  CSM_STATE_NOT_STARTED,
  CSM_STATE_20,
  CSM_STATE_20_40,
} eCSM_STATES;

typedef enum
{
  eAO_REGISTER,
  eAO_UNREGISTER,
  eAO_ENABLE,
  eAO_DISABLE,
} eABILITY_OPS;

typedef UMI_COEX_EL mtlk_20_40_coexistence_element;

typedef struct _mtlk_20_40_obss_scan_results
{
  UMI_INTOLERANT_CHANNELS_REPORT    intolerant_channels_report;
  /* TODO insert var's */
} mtlk_20_40_obss_scan_results_t ;/* (see UMI_INTOLERANT_CHANNELS_REPORT definition in the SW -> FW section) */

typedef struct _mtlk_20_40_external_intolerance_info
{
  uint16                channel;
  uint8                 secondary_channel_offset;
  mtlk_osal_timestamp_t timestamp;
  BOOL                  is_ht;
  BOOL                  forty_mhz_intolerant;
} mtlk_20_40_external_intolerance_info_t;

struct _mtlk_get_channel_data_t;

typedef void (*switch_cb_mode_stage1_callback_type)(mtlk_handle_t context,
  struct _mtlk_get_channel_data_t *channel_data, FREQUENCY_ELEMENT *mode_change_params);

typedef void (*switch_cb_mode_stage2_callback_type)(mtlk_handle_t context,
  FREQUENCY_ELEMENT *mode_change_params);

typedef void __MTLK_IFUNC(*send_ce_callback_type)(mtlk_handle_t context,
  UMI_COEX_EL *coexistence_element);

typedef void __MTLK_IFUNC(*send_cmf_callback_type)(mtlk_handle_t context,
  const IEEE_ADDR *sta_addr, const UMI_COEX_FRAME *coexistence_frame);

typedef int __MTLK_IFUNC (*scan_async_callback_type)(mtlk_handle_t context,
  uint8 band, const char* essid);

typedef void __MTLK_IFUNC (*scan_set_bg_callback_type)(mtlk_handle_t context, BOOL is_background);

typedef void __MTLK_IFUNC obss_scan_report_callback_type(mtlk_handle_t context,
  mtlk_20_40_obss_scan_results_t *scan_results);

typedef int __MTLK_IFUNC (*scan_register_obss_cb_callback_type)(mtlk_handle_t context,
  obss_scan_report_callback_type *callback);

typedef void __MTLK_IFUNC (*external_intolerance_enumerator_callback_type)(mtlk_handle_t context, 
  mtlk_20_40_external_intolerance_info_t *external_intolerance_info);

typedef int __MTLK_IFUNC (*enumerate_external_intolerance_info_callback_type)(mtlk_handle_t caller_context,
  mtlk_handle_t core_context, external_intolerance_enumerator_callback_type callback, uint32 expiration_time);

typedef int __MTLK_IFUNC (*ability_control_callback_type)(mtlk_handle_t context,
  eABILITY_OPS operation, const uint32* ab_id_list, uint32 ab_id_num);

typedef uint8 __MTLK_IFUNC (*get_reg_domain_callback_type)(mtlk_handle_t context);

typedef uint16 __MTLK_IFUNC (*get_cur_channels_callback_type)(mtlk_handle_t context, int *secondary_channel_offset);

typedef struct _mtlk_20_40_csm_xfaces
{
  mtlk_handle_t                                       context;
  mtlk_vap_handle_t                                   vap_handle;
  switch_cb_mode_stage1_callback_type                 switch_cb_mode_stage1;
  switch_cb_mode_stage2_callback_type                 switch_cb_mode_stage2;
  send_ce_callback_type                               send_ce;
  send_cmf_callback_type                              send_cmf;
  scan_async_callback_type                            scan_async;
  scan_set_bg_callback_type                           scan_set_background;
  scan_register_obss_cb_callback_type                 register_obss_callback;
  enumerate_external_intolerance_info_callback_type   enumerate_external_intolerance_info;
  ability_control_callback_type                       ability_control;
  get_reg_domain_callback_type                        get_reg_domain;
  get_cur_channels_callback_type                      get_cur_channels;
} mtlk_20_40_csm_xfaces_t;

/* interfaces */

struct _mtlk_20_40_coexistence_sm;

/* Initialization and de-initialization interfaces */ 

struct _mtlk_20_40_coexistence_sm* __MTLK_IFUNC mtlk_20_40_create(mtlk_20_40_csm_xfaces_t *xfaces, BOOL is_ap, uint32 max_number_of_connected_stations);

int __MTLK_IFUNC mtlk_20_40_start(struct _mtlk_20_40_coexistence_sm *coex_sm, eCSM_STATES initial_state, mtlk_wss_t *parent_wss);

void __MTLK_IFUNC mtlk_20_40_stop(struct _mtlk_20_40_coexistence_sm *coex_sm);

void __MTLK_IFUNC mtlk_20_40_delete(struct _mtlk_20_40_coexistence_sm *coex_sm);

void __MTLK_IFUNC mtlk_20_40_limit_to_20(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL must_limit);
/* Interfaces for the command line handlers */

void __MTLK_IFUNC mtlk_20_40_enable_feature(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL enable);
/* This function will either enable or disable the 20/40 coexistence feature in general. */

BOOL __MTLK_IFUNC mtlk_20_40_is_feature_enabled(struct _mtlk_20_40_coexistence_sm *coex_sm);
/* This function returns TRUE if the 20/40 coexistence feature is enabled or FALSE otherwise.*/

void __MTLK_IFUNC mtlk_20_40_declare_intolerance(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL intolerant);
/* This function will instruct the state machine whether to declare 20/40 intolerance or not.*/

BOOL __MTLK_IFUNC mtlk_20_40_is_intolerance_declared(struct _mtlk_20_40_coexistence_sm *coex_sm);
/* This function returns TRUE if the STA/AP has declared itself 40 MHz intolerant, or FALSE otherwise. */

void __MTLK_IFUNC mtlk_20_40_sta_force_scan_exemption_request (struct _mtlk_20_40_coexistence_sm *coex_sm, 
  BOOL request_exemption);
/* This interface will allow other modules to instruct the coexistence state machine of a station to request exemption from OBSS scanning. The second parameter (request_exemption) will determine whether the coexistence state machine of the station will generate coexistence elements with OBSS Scanning Exemption Request flag set to one or zero. */

BOOL __MTLK_IFUNC mtlk_20_40_sta_is_scan_exemption_request_forced (struct _mtlk_20_40_coexistence_sm *coex_sm);
/* This function returns TRUE if the station is configured to request exemption from OBSS scanning, or FALSE otherwise. */

int __MTLK_IFUNC mtlk_20_40_set_transition_delay_factor (struct _mtlk_20_40_coexistence_sm *coex_sm, 
  uint8 delay_factor);
/* The delay factor is the minimum number of scans that the AP must perform (by itself or by an associated station) before it makes a decision to move from 20 to 20/40 CB mode. */

int __MTLK_IFUNC mtlk_20_40_get_transition_delay_factor (struct _mtlk_20_40_coexistence_sm *coex_sm);
/* The function returns the current transition delay factor. */

int __MTLK_IFUNC mtlk_20_40_set_scan_interval (struct _mtlk_20_40_coexistence_sm *coex_sm,
  uint32 scan_interval);
/* The scan interval is the interval in seconds between two consecutive OBSS scans. */

int __MTLK_IFUNC mtlk_20_40_get_scan_interval (struct _mtlk_20_40_coexistence_sm *coex_sm);
/*The function returns the current interval between two consecutive OBSS scans. */


/* Additional interfaces */
void __MTLK_IFUNC mtlk_20_40_ap_process_coexistence_element (struct _mtlk_20_40_coexistence_sm *coex_sm, 
  const mtlk_20_40_coexistence_element *coex_el, const IEEE_ADDR *src_addr);

void __MTLK_IFUNC mtlk_20_40_sta_process_coexistence_element (struct _mtlk_20_40_coexistence_sm *coex_sm, 
  mtlk_20_40_coexistence_element *coex_el);

void __MTLK_IFUNC mtlk_20_40_ap_process_obss_scan_results (struct _mtlk_20_40_coexistence_sm *coex_sm,
  UMI_INTOLERANT_CHANNEL_DESCRIPTOR *intolerant_channels_descriptor);
/* This function will be called by the frame parser of on AP when an OBSS scan results are received from one of the associated stations. */

void __MTLK_IFUNC mtlk_20_40_ap_notify_non_ht_beacon_received (struct _mtlk_20_40_coexistence_sm *coex_sm, uint16 channel);

void __MTLK_IFUNC mtlk_20_40_ap_notify_intolerant_or_legacy_station_connected (struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL dont_lock);

void __MTLK_IFUNC mtlk_20_40_ap_notify_last_40_incapable_station_disconnected(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL dont_lock);

void __MTLK_IFUNC mtlk_20_40_sta_process_obss_scan_results (struct _mtlk_20_40_coexistence_sm *coex_sm, 
  mtlk_20_40_obss_scan_results_t *obss_scan_results);
/* This function will be called by the scan manager when it is ready to pass the scan results to the coexistence state machine that, in turn, will report them to the associated AP. */

void __MTLK_IFUNC mtlk_20_40_sta_notify_switch_to_20_mode (struct _mtlk_20_40_coexistence_sm *coex_sm,
  int channel);

void __MTLK_IFUNC mtlk_20_40_sta_notify_switch_to_40_mode (struct _mtlk_20_40_coexistence_sm *coex_sm,
  int primary_channel, int secondary_channel);

void __MTLK_IFUNC mtlk_20_40_register_station (struct _mtlk_20_40_coexistence_sm *coex_sm,
  const IEEE_ADDR *sta_addr, BOOL supports_coexistence, BOOL exempt, BOOL intolerant, BOOL legacy);

void __MTLK_IFUNC mtlk_20_40_unregister_station (struct _mtlk_20_40_coexistence_sm *coex_sm, 
  const IEEE_ADDR *sta_addr);

BOOL __MTLK_IFUNC mtlk_20_40_is_20_40_operation_permitted(struct _mtlk_20_40_coexistence_sm *coex_sm,
  uint16 primary_channel, uint8 secondary_channel_offset);

void __MTLK_IFUNC mtlk_20_40_set_intolerance_at_first_scan_flag(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL intolerant, BOOL dont_lock);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif
