/*
 * $Id: coex20_40.h 11780 2011-10-19 13:00:19Z bogoslav $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * 20/40 coexistence feature - private portion
 *
 * This file defines the private portion of the 20/40 coexistence state
 * machine's interface meant only for the state machine itself and its
 * child modules
 */

#ifndef __COEX20_40CHLD_H__
#define __COEX20_40CHLD_H__

#ifndef COEX_20_40_C
#error This file can only be included from one of the 20/40 coexistence implementation (.c) files
#endif

#include "coex20_40.h"
#include "coexlve.h"
#include "scexempt.h"
#include "coexfrgen.h"
#include "cbsmgr.h"
#include "mtlkstartup.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define CE2040_DEFAULT_TRANSITION_DELAY         4
#define CE2040_DEFAULT_SCAN_INTERVAL            60
#define CE2040_NUMBER_OF_CHANNELS_IN_2G4_BAND   14
#define CE2040_FIRST_CHANNEL_NUMBER_IN_2G4_BAND 1


/* statistics */
typedef enum{
  MTLK_COEX_20_40_NOF_COEX_EL_RECEIVED,                            /* received coexistence element */
  MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_REQUESTED,            /* received coexistence element with SCAN_EXEMPTION_REQUEST bit = 1 */
  MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_GRANTED,              /* sent coexistence element with SCAN_EXEMPTION_GRANT bit = 1 */
  MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED,      /* sent coexistence element with SCAN_EXEMPTION_GRANT bit = 0 */
  MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_20_TO_40,                     /* switch channel message sent to FW (20MHz to 40MHz) */
  MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_20,                     /* switch channel message sent to FW (40MHz to 20MHz) */
  MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_40,                     /* switch channel message sent to FW (40MHz to 40MHz) */
  
  MTLK_COEX_20_40_CNT_LAST
} coex_20_40_info_cnt_id_e;


typedef struct _mtlk_coex_intolerant_detection
{
  BOOL                              intolerant_detected;
  mtlk_osal_timestamp_t             intolerant_detection_ts;
} mtlk_coex_intolerant_detection_t;

typedef struct _mtlk_coex_intolerant_channels_db
{
  BOOL                              primary;
  mtlk_osal_timestamp_t             primary_detection_ts;
  BOOL                              secondary;
  mtlk_osal_timestamp_t             secodnary_detection_ts;
  BOOL                              intolerant;
  mtlk_osal_timestamp_t             intolerant_detection_ts;
} mtlk_coex_intolerant_channels_db_t;

typedef struct _mtlk_coex_intolerant_db
{
  mtlk_coex_intolerant_detection_t      general_intolerance_data;
  mtlk_coex_intolerant_channels_db_t    channels_list[CE2040_NUMBER_OF_CHANNELS_IN_2G4_BAND];
  MTLK_DECLARE_INIT_STATUS;
} mtlk_coex_intolerant_db_t;

/* state machine */
typedef struct _mtlk_20_40_coexistence_sm
{
  mtlk_osal_spinlock_t                  lock;
  BOOL                                  coexistence_mode;
  BOOL                                  intolerance_mode;
  BOOL                                  limited_to_20;
  BOOL                                  exemption_req;
  BOOL                                  intolerance_detected_at_first_scan;
  BOOL                                  is_ap;
  uint8                                 delay_factor;
  uint32                                obss_scan_interval;
  mtlk_osal_timer_t                     transition_timer;
  mtlk_coex_intolerant_db_t             intolerance_db;
  mtlk_atomic_t                         current_csm_state;
  mtlk_20_40_csm_xfaces_t               xfaces;

  /* sub-modules */
  mtlk_local_variable_evaluator         coexlve;
  mtlk_cb_switch_manager                cbsm;
  mtlk_scan_exemption_policy_manager_t  ap_scexmpt;
  mtlk_scan_reqresp_manager_t           sta_scexmpt;
  mtlk_coex_frame_gen                   frgen;

  /* statistics */
  mtlk_wss_t                            *wss;
  mtlk_wss_cntr_handle_t                *wss_hcntrs[MTLK_COEX_20_40_CNT_LAST];

  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
} __MTLK_IDATA mtlk_20_40_coexistence_sm_t;

/* Interfaces for the child modules */
void __MTLK_IFUNC mtlk_20_40_perform_idb_update(mtlk_20_40_coexistence_sm_t *coex_sm);
uint32 __MTLK_IFUNC mtlk_20_40_calc_transition_timer(mtlk_20_40_coexistence_sm_t *coex_sm);
BOOL __MTLK_IFUNC mtlk_20_40_is_coex_el_intolerant_bit_detected(mtlk_20_40_coexistence_sm_t *coex_sm);

/* statistics - promote counter function */
void __MTLK_IFUNC mtlk_coex_20_40_inc_cnt(mtlk_20_40_coexistence_sm_t *coex_sm, coex_20_40_info_cnt_id_e cnt_id);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif
