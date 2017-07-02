/*
 * $Id: coex20_40.c 12232 2011-12-20 16:03:42Z kashani $
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

#include "mtlkinc.h"

#define COEX_20_40_C
// This define is necessary for the submodules' .h files to compile successfully
#include "coex20_40priv.h"
#include "mtlk_param_db.h"
#include "mtlk_coreui.h"

static int _mtlk_20_40_init(mtlk_20_40_coexistence_sm_t *coex_sm, mtlk_20_40_csm_xfaces_t *xfaces, BOOL is_ap, uint32 max_number_of_connected_stations);
static void _mtlk_20_40_cleanup(mtlk_20_40_coexistence_sm_t *coex_sm);
static void _mtlk_20_40_check_if_can_remain_40_move_if_not(mtlk_20_40_coexistence_sm_t *coex_sm);
static void _mtlk_20_40_check_if_can_move_to_40_move_if_so(mtlk_20_40_coexistence_sm_t *coex_sm);
static int _mtlk_20_40_register_ability_set(mtlk_20_40_coexistence_sm_t *coex_sm, const uint32* ab_id_list, uint32 ab_id_num);
static int _mtlk_20_40_unregister_ability_set(mtlk_20_40_coexistence_sm_t *coex_sm, const mtlk_ability_id_t* ab_id_list, uint32 ab_id_num);
static int _mtlk_20_40_enable_ability_set(mtlk_20_40_coexistence_sm_t *coex_sm, const uint32* ab_id_list, uint32 ab_id_num);
static int _mtlk_20_40_disable_ability_set(mtlk_20_40_coexistence_sm_t *coex_sm, const uint32* ab_id_list, uint32 ab_id_num);
static BOOL _mtlk_coex_20_40_intolerant_db_update_data_from_frame(mtlk_20_40_coexistence_sm_t *coex_sm, UMI_INTOLERANT_CHANNEL_DESCRIPTOR *descriptor, mtlk_osal_timestamp_t rec_ts);
static void _mtlk_coex_20_40_external_intolerance_enumerator_callback(mtlk_handle_t context, mtlk_20_40_external_intolerance_info_t *external_intolerance_info);
static BOOL _mtlk_20_40_find_available_channel_pair(mtlk_20_40_coexistence_sm_t *coex_sm, uint16 *primary_channel, int *secondary_channel_offset, int forced_secondary_channel_offset);
static uint32 _mtlk_transition_delay_timeout_callback(mtlk_osal_timer_t *timer, mtlk_handle_t clb_usr_data);
static const uint32*_mtlk_20_40_init_get_abilities (mtlk_20_40_coexistence_sm_t *coex_sm, uint32 *nof_abilities);
static void _mtlk_20_40_set_limit_to_20(mtlk_20_40_coexistence_sm_t *coex_sm, BOOL limit_flag);

#define LOG_LOCAL_GID   GID_COEX
#define LOG_LOCAL_FID   0

#define MIN_TRANSITION_DELAY_FACTOR  1
#define MAX_TRANSITION_DELAY_FACTOR  100
#define DEFAULT_TRANSITION_DELAY_FACTOR  4
#define MIN_OBSS_SCAN_INTERVAL  0
#define MAX_OBSS_SCAN_INTERVAL  300
#define DEFAULT_OBSS_SCAN_INTERVAL  300
#define SECONDARY_CHANNEL_OFFSET_JUMP 5
#define SECONDARY_CHANNEL_OFFSET_JUMP_FOR_LAST_CHANNEL 4
#define LAST_CHANNEL 14


/* Initialization & cleanup */

MTLK_INIT_STEPS_LIST_BEGIN(coex_sm)
  MTLK_INIT_STEPS_LIST_ENTRY(coex_sm, TRANSITION_TIMER)
  MTLK_INIT_STEPS_LIST_ENTRY(coex_sm, IDB_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(coex_sm, REGISTER_ABILITIES)
  MTLK_INIT_STEPS_LIST_ENTRY(coex_sm, LVE_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(coex_sm, CBSM_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(coex_sm, AP_SCEXMPT_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(coex_sm, STA_SCEXMPT_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(coex_sm, CEFG_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(coex_sm, ENABLE_ABILITIES)
MTLK_INIT_INNER_STEPS_BEGIN(coex_sm)
MTLK_INIT_STEPS_LIST_END(coex_sm);

MTLK_START_STEPS_LIST_BEGIN(coex_sm)
  MTLK_START_STEPS_LIST_ENTRY(coex_sm, DISABLE_INACTIVE_ABILITIES)
  MTLK_START_STEPS_LIST_ENTRY(coex_sm, WSS_NODE)
  MTLK_START_STEPS_LIST_ENTRY(coex_sm, WSS_HCNTRs)
  MTLK_START_STEPS_LIST_ENTRY(coex_sm, TRANSITION_TIMER)
  MTLK_START_STEPS_LIST_ENTRY(coex_sm, SET_STATE)
MTLK_START_INNER_STEPS_BEGIN(coex_sm)
MTLK_START_STEPS_LIST_END(coex_sm);

static const uint32 _mtlk_coex_20_40_wss_id_map[] = 
{
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_RECEIVED,                        /* MTLK_COEX_20_40_NOF_COEX_EL_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_REQUESTED,        /* MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_REQUESTED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_GRANTED,          /* MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_GRANTED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED,  /* MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_20_TO_40,                 /* MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_20_TO_40 */
  MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_40_TO_20,                 /* MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_20 */
  MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_40_TO_40                  /* MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_40 */
};

static const uint32 _mtlk_20_40_ap_abilities[] =
{
  MTLK_CORE_REQ_GET_COEX_20_40_MODE_CFG,
  MTLK_CORE_REQ_SET_COEX_20_40_MODE_CFG,
  MTLK_CORE_REQ_GET_COEX_20_40_TIMES_CFG,
  MTLK_CORE_REQ_SET_COEX_20_40_TIMES_CFG,
};

static const uint32 _mtlk_20_40_sta_abilities[] =
{
  MTLK_CORE_REQ_GET_COEX_20_40_MODE_CFG,
  MTLK_CORE_REQ_SET_COEX_20_40_MODE_CFG,
  MTLK_CORE_REQ_GET_COEX_20_40_TIMES_CFG,
  MTLK_CORE_REQ_SET_COEX_20_40_TIMES_CFG,
  MTLK_CORE_REQ_GET_COEX_20_40_EXM_REQ_CFG,
  MTLK_CORE_REQ_SET_COEX_20_40_EXM_REQ_CFG
};

static const uint32 _mtlk_20_40_inactive_only_abilities[] = 
{
  MTLK_CORE_REQ_SET_COEX_20_40_MODE_CFG
};

static const uint32 *
  _mtlk_20_40_init_get_abilities (mtlk_20_40_coexistence_sm_t *coex_sm, uint32 *nof_abilities)
{
  const mtlk_ability_id_t *abilities     = NULL;

  MTLK_ASSERT(coex_sm != NULL);
  MTLK_ASSERT(nof_abilities != NULL);

  if (coex_sm->is_ap) 
  {
    abilities      = _mtlk_20_40_ap_abilities;
    *nof_abilities = ARRAY_SIZE(_mtlk_20_40_ap_abilities);
  }
  else 
  {
    abilities      = _mtlk_20_40_sta_abilities;
    *nof_abilities = ARRAY_SIZE(_mtlk_20_40_sta_abilities);
  }

  return abilities;
}

mtlk_20_40_coexistence_sm_t *__MTLK_IFUNC mtlk_20_40_create(mtlk_20_40_csm_xfaces_t *xfaces, BOOL is_ap, uint32 max_number_of_connected_stations)
{
  mtlk_20_40_coexistence_sm_t *coex_sm = mtlk_osal_mem_alloc(sizeof(mtlk_20_40_coexistence_sm_t), MTLK_MEM_TAG_COEX_20_40);
  
  MTLK_ASSERT(xfaces != NULL); 

  if (coex_sm != NULL)
  {
    if (_mtlk_20_40_init(coex_sm, xfaces, is_ap, max_number_of_connected_stations) != MTLK_ERR_OK)
    {
      mtlk_osal_mem_free(coex_sm);
      coex_sm = NULL;
    }
  }
  else
  {
    ELOG_V("ERROR: coex_sm memory allocation failed!");
  }

  return coex_sm;
}

static int _mtlk_20_40_init(mtlk_20_40_coexistence_sm_t *coex_sm, mtlk_20_40_csm_xfaces_t *xfaces, BOOL is_ap, uint32 max_number_of_connected_stations)
{
  const mtlk_core_tx_req_id_t *abilities     = NULL;
  uint32                       nof_abilities = 0;
  int                          i;
  mtlk_osal_timestamp_t        cur_ts;

  MTLK_ASSERT(NULL != coex_sm);
  MTLK_ASSERT(NULL != xfaces);
  MTLK_ASSERT(NULL != xfaces->switch_cb_mode_stage1);
  MTLK_ASSERT(NULL != xfaces->switch_cb_mode_stage2);
  MTLK_ASSERT(NULL != xfaces->send_ce);
  MTLK_ASSERT(NULL != xfaces->send_cmf);
  MTLK_ASSERT(NULL != xfaces->scan_async);
  MTLK_ASSERT(NULL != xfaces->scan_set_background);
  MTLK_ASSERT(NULL != xfaces->register_obss_callback);
  MTLK_ASSERT(NULL != xfaces->enumerate_external_intolerance_info);
  MTLK_ASSERT(NULL != xfaces->ability_control);
  MTLK_ASSERT(NULL != xfaces->get_reg_domain);
  MTLK_ASSERT(NULL != xfaces->get_cur_channels);

  coex_sm->is_ap = is_ap;
  abilities = _mtlk_20_40_init_get_abilities(coex_sm, &nof_abilities);

  MTLK_ASSERT(abilities != NULL);

  coex_sm->coexistence_mode = FALSE;
  coex_sm->intolerance_mode = FALSE;
  _mtlk_20_40_set_limit_to_20(coex_sm, FALSE);
  coex_sm->intolerance_detected_at_first_scan = FALSE;
  coex_sm->exemption_req = FALSE;
  coex_sm->delay_factor = CE2040_DEFAULT_TRANSITION_DELAY;
  coex_sm->obss_scan_interval = CE2040_DEFAULT_SCAN_INTERVAL;
  coex_sm->xfaces = *xfaces;
  mtlk_osal_atomic_set(&coex_sm->current_csm_state, CSM_STATE_NOT_STARTED);

  MTLK_INIT_TRY(coex_sm, MTLK_OBJ_PTR(coex_sm))
    MTLK_INIT_STEP(coex_sm, TRANSITION_TIMER, MTLK_OBJ_PTR(coex_sm),
                        mtlk_osal_timer_init, (&coex_sm->transition_timer, 
                        _mtlk_transition_delay_timeout_callback, (mtlk_handle_t)coex_sm));
    MTLK_INIT_STEP(coex_sm, IDB_LOCK, MTLK_OBJ_PTR(coex_sm),
                        mtlk_osal_lock_init, (&coex_sm->lock));
    MTLK_INIT_STEP(coex_sm, REGISTER_ABILITIES, MTLK_OBJ_PTR(coex_sm),
                        _mtlk_20_40_register_ability_set, (coex_sm, abilities, nof_abilities));
    MTLK_INIT_STEP(coex_sm, LVE_INIT, MTLK_OBJ_PTR(coex_sm),
                        mtlk_coex_lve_init, (&coex_sm->coexlve, coex_sm, &coex_sm->xfaces));
    MTLK_INIT_STEP(coex_sm, CBSM_INIT, MTLK_OBJ_PTR(coex_sm),
                        mtlk_cbsm_init, (&coex_sm->cbsm, coex_sm, &coex_sm->xfaces));
    MTLK_INIT_STEP_IF(coex_sm->is_ap, coex_sm, AP_SCEXMPT_INIT, MTLK_OBJ_PTR(coex_sm),
                        mtlk_sepm_init, (&coex_sm->ap_scexmpt, coex_sm, &coex_sm->xfaces, max_number_of_connected_stations));
    MTLK_INIT_STEP_IF(!coex_sm->is_ap, coex_sm, STA_SCEXMPT_INIT, MTLK_OBJ_PTR(coex_sm),
                        mtlk_srrm_init, (&coex_sm->sta_scexmpt, coex_sm, &coex_sm->xfaces));
    MTLK_INIT_STEP(coex_sm, CEFG_INIT, MTLK_OBJ_PTR(coex_sm),
                        mtlk_cefg_init, (&coex_sm->frgen, coex_sm, &coex_sm->xfaces));
    MTLK_INIT_STEP(coex_sm, ENABLE_ABILITIES, MTLK_OBJ_PTR(coex_sm),
                   _mtlk_20_40_enable_ability_set, (coex_sm, abilities, nof_abilities));
  
  cur_ts = mtlk_osal_timestamp();
  coex_sm->intolerance_db.general_intolerance_data.intolerant_detected = FALSE;
  coex_sm->intolerance_db.general_intolerance_data.intolerant_detection_ts = cur_ts;
  for(i = 0 ; i < CE2040_NUMBER_OF_CHANNELS_IN_2G4_BAND; i++)
  {
    coex_sm->intolerance_db.channels_list[i].intolerant = FALSE;
    coex_sm->intolerance_db.channels_list[i].intolerant_detection_ts = cur_ts;
    coex_sm->intolerance_db.channels_list[i].primary = FALSE;
    coex_sm->intolerance_db.channels_list[i].primary_detection_ts = cur_ts;
    coex_sm->intolerance_db.channels_list[i].secondary = FALSE;
    coex_sm->intolerance_db.channels_list[i].secodnary_detection_ts = cur_ts;
  }

  MTLK_INIT_FINALLY(coex_sm, MTLK_OBJ_PTR(coex_sm))
  MTLK_INIT_RETURN(coex_sm, MTLK_OBJ_PTR(coex_sm), mtlk_20_40_delete, (coex_sm))
}

int __MTLK_IFUNC mtlk_20_40_start( mtlk_20_40_coexistence_sm_t *coex_sm, eCSM_STATES initial_state, mtlk_wss_t *parent_wss)
{
  BOOL start_timer = FALSE;

  MTLK_ASSERT(NULL != coex_sm);
  MTLK_ASSERT(NULL != parent_wss);

  /* currently, the state machine will always be started in CSM_STATE_20 mode */
  ILOG2_D("initial state = %d", initial_state);

  MTLK_START_TRY(coex_sm, MTLK_OBJ_PTR(coex_sm))
    MTLK_START_STEP_VOID(coex_sm, DISABLE_INACTIVE_ABILITIES, MTLK_OBJ_PTR(coex_sm),
                         _mtlk_20_40_disable_ability_set, (coex_sm, _mtlk_20_40_inactive_only_abilities, ARRAY_SIZE(_mtlk_20_40_inactive_only_abilities)));
    MTLK_START_STEP_EX(coex_sm, WSS_NODE, MTLK_OBJ_PTR(coex_sm),
      mtlk_wss_create, (parent_wss, _mtlk_coex_20_40_wss_id_map, ARRAY_SIZE(_mtlk_coex_20_40_wss_id_map)),
      coex_sm->wss, coex_sm->wss != NULL, MTLK_ERR_NO_MEM);
    MTLK_START_STEP(coex_sm, WSS_HCNTRs, MTLK_OBJ_PTR(coex_sm),
      mtlk_wss_cntrs_open, (coex_sm->wss, _mtlk_coex_20_40_wss_id_map, coex_sm->wss_hcntrs, MTLK_COEX_20_40_CNT_LAST));
    start_timer = ((coex_sm->intolerance_detected_at_first_scan == FALSE) &&
                   (coex_sm->limited_to_20 == FALSE) &&
                   (initial_state == CSM_STATE_20) &&
                   (coex_sm->coexistence_mode == TRUE) &&
                   (coex_sm->intolerance_mode == FALSE));
    MTLK_START_STEP_IF(start_timer, coex_sm, TRANSITION_TIMER, MTLK_OBJ_PTR(coex_sm),
                       mtlk_osal_timer_set, (&coex_sm->transition_timer, mtlk_20_40_calc_transition_timer(coex_sm)));
    if(start_timer)
    {
      ILOG2_V("Transition timer is set, since coexistence feature started");
    }
    MTLK_START_STEP_VOID(coex_sm, SET_STATE, MTLK_OBJ_PTR(coex_sm),
                         mtlk_osal_atomic_set, (&coex_sm->current_csm_state, (uint32)initial_state));
  MTLK_START_FINALLY(coex_sm, MTLK_OBJ_PTR(coex_sm))
  MTLK_START_RETURN(coex_sm, MTLK_OBJ_PTR(coex_sm), mtlk_20_40_stop, (coex_sm))
}

void __MTLK_IFUNC mtlk_20_40_stop( mtlk_20_40_coexistence_sm_t *coex_sm )
{
  MTLK_ASSERT(NULL != coex_sm);

  if (mtlk_osal_atomic_get(&coex_sm->current_csm_state) != CSM_STATE_NOT_STARTED)
  {
    MTLK_STOP_BEGIN(coex_sm, MTLK_OBJ_PTR(coex_sm))
      MTLK_STOP_STEP(coex_sm, SET_STATE, MTLK_OBJ_PTR(coex_sm),
                     mtlk_osal_atomic_set, (&coex_sm->current_csm_state, CSM_STATE_NOT_STARTED));
      MTLK_STOP_STEP(coex_sm, TRANSITION_TIMER, MTLK_OBJ_PTR(coex_sm),
                     mtlk_osal_timer_cancel_sync, (&coex_sm->transition_timer));
      MTLK_STOP_STEP(coex_sm, WSS_HCNTRs, MTLK_OBJ_PTR(coex_sm),
                     mtlk_wss_cntrs_close, (coex_sm->wss, coex_sm->wss_hcntrs, ARRAY_SIZE(coex_sm->wss_hcntrs)));
      MTLK_STOP_STEP(coex_sm, WSS_NODE, MTLK_OBJ_PTR(coex_sm),
                     mtlk_wss_delete, (coex_sm->wss));
      ILOG2_V("Transition timer has been canceled, since coexistence feature stopped");
      MTLK_STOP_STEP(coex_sm, DISABLE_INACTIVE_ABILITIES, MTLK_OBJ_PTR(coex_sm),
                     _mtlk_20_40_enable_ability_set, (coex_sm, _mtlk_20_40_inactive_only_abilities,
                     ARRAY_SIZE(_mtlk_20_40_inactive_only_abilities)));
    MTLK_STOP_END(coex_sm, MTLK_OBJ_PTR(coex_sm))
  }
}

void __MTLK_IFUNC mtlk_20_40_delete(mtlk_20_40_coexistence_sm_t *coex_sm)
{
  MTLK_ASSERT(coex_sm != NULL);

  _mtlk_20_40_cleanup(coex_sm);
  mtlk_osal_mem_free(coex_sm);
}

static void _mtlk_20_40_cleanup(mtlk_20_40_coexistence_sm_t *coex_sm)
{
  const mtlk_core_tx_req_id_t *abilities     = NULL;
  uint32                       nof_abilities = 0;

  MTLK_ASSERT(coex_sm != NULL);

  abilities = _mtlk_20_40_init_get_abilities(coex_sm, &nof_abilities);

  MTLK_ASSERT(abilities != NULL);

  MTLK_CLEANUP_BEGIN(coex_sm, MTLK_OBJ_PTR(coex_sm))
    MTLK_CLEANUP_STEP(coex_sm, ENABLE_ABILITIES, MTLK_OBJ_PTR(coex_sm),
      _mtlk_20_40_disable_ability_set, (coex_sm, abilities, nof_abilities));
    MTLK_CLEANUP_STEP(coex_sm, CEFG_INIT, MTLK_OBJ_PTR(coex_sm),
      mtlk_cefg_cleanup, (&coex_sm->frgen));
    MTLK_CLEANUP_STEP(coex_sm, STA_SCEXMPT_INIT, MTLK_OBJ_PTR(coex_sm),
      mtlk_srrm_cleanup, (&coex_sm->sta_scexmpt));
    MTLK_CLEANUP_STEP(coex_sm, AP_SCEXMPT_INIT, MTLK_OBJ_PTR(coex_sm),
      mtlk_sepm_cleanup, (&coex_sm->ap_scexmpt));
    MTLK_CLEANUP_STEP(coex_sm, CBSM_INIT, MTLK_OBJ_PTR(coex_sm),
      mtlk_cbsm_cleanup, (&coex_sm->cbsm));
    MTLK_CLEANUP_STEP(coex_sm, LVE_INIT, MTLK_OBJ_PTR(coex_sm),
      mtlk_coex_lve_cleanup, (&coex_sm->coexlve));
    MTLK_CLEANUP_STEP(coex_sm, REGISTER_ABILITIES, MTLK_OBJ_PTR(coex_sm),
      _mtlk_20_40_unregister_ability_set, (coex_sm, abilities, nof_abilities));
    MTLK_CLEANUP_STEP(coex_sm, IDB_LOCK, MTLK_OBJ_PTR(coex_sm),
      mtlk_osal_lock_cleanup, (&coex_sm->lock));
    MTLK_CLEANUP_STEP(coex_sm, TRANSITION_TIMER, MTLK_OBJ_PTR(coex_sm),
      mtlk_osal_timer_cleanup, (&coex_sm->transition_timer));
  MTLK_CLEANUP_END(coex_sm, MTLK_OBJ_PTR(coex_sm))
}

void __MTLK_IFUNC mtlk_20_40_limit_to_20(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL must_limit)
{
  MTLK_ASSERT(NULL != coex_sm);

  _mtlk_20_40_set_limit_to_20(coex_sm, must_limit);
  if (must_limit)
  {
    mtlk_osal_timer_cancel_sync(&coex_sm->transition_timer);
    ILOG2_V("Transition timer has been canceled, since limit_to_20 flag is up");
    if (mtlk_osal_atomic_get(&coex_sm->current_csm_state) == CSM_STATE_20_40)
    {
      mtlk_osal_atomic_set(&coex_sm->current_csm_state, CSM_STATE_20);
      mtlk_cbsm_switch_to_20_mode(&coex_sm->cbsm, mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CUR));
      mtlk_coex_20_40_inc_cnt(coex_sm, MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_20);
    }
  }
}

/* External functional interfaces */

void __MTLK_IFUNC mtlk_20_40_enable_feature(mtlk_20_40_coexistence_sm_t *coex_sm, BOOL enable_flag)
{
  MTLK_ASSERT(coex_sm != NULL);

  coex_sm->coexistence_mode = enable_flag;
}

BOOL __MTLK_IFUNC mtlk_20_40_is_feature_enabled(mtlk_20_40_coexistence_sm_t *coex_sm)
{
  MTLK_ASSERT(coex_sm != NULL);

  return coex_sm->coexistence_mode;
}

void __MTLK_IFUNC mtlk_20_40_declare_intolerance(mtlk_20_40_coexistence_sm_t *coex_sm, BOOL intolerant)
{
  MTLK_ASSERT(coex_sm != NULL);

  coex_sm->intolerance_mode = intolerant;
}

BOOL __MTLK_IFUNC mtlk_20_40_is_intolerance_declared(mtlk_20_40_coexistence_sm_t *coex_sm)
{
  MTLK_ASSERT(coex_sm != NULL);

  return coex_sm->intolerance_mode;
}

void __MTLK_IFUNC mtlk_20_40_sta_force_scan_exemption_request (mtlk_20_40_coexistence_sm_t *coex_sm, 
  BOOL request_exemption)
{
  MTLK_ASSERT(coex_sm != NULL);

  coex_sm->exemption_req = request_exemption;
}

BOOL __MTLK_IFUNC mtlk_20_40_sta_is_scan_exemption_request_forced (mtlk_20_40_coexistence_sm_t *coex_sm)
{
  MTLK_ASSERT(coex_sm != NULL);

  return coex_sm->exemption_req;
}

int __MTLK_IFUNC mtlk_20_40_set_transition_delay_factor (mtlk_20_40_coexistence_sm_t *coex_sm, 
  uint8 delay_factor)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(coex_sm != NULL);

  if ((delay_factor >= MIN_TRANSITION_DELAY_FACTOR) && (delay_factor <= MAX_TRANSITION_DELAY_FACTOR))
  {
    coex_sm->delay_factor = delay_factor;
  } 
  else
  {
    res = MTLK_ERR_VALUE;
  }
  return res;
}

int __MTLK_IFUNC mtlk_20_40_get_transition_delay_factor (mtlk_20_40_coexistence_sm_t *coex_sm)
{
  MTLK_ASSERT(coex_sm != NULL);

  return coex_sm->delay_factor;
}

int __MTLK_IFUNC mtlk_20_40_set_scan_interval (mtlk_20_40_coexistence_sm_t *coex_sm,
  uint32 scan_interval)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(coex_sm != NULL);

  if((scan_interval >= MIN_OBSS_SCAN_INTERVAL) && (scan_interval <= MAX_OBSS_SCAN_INTERVAL))
  {
    coex_sm->obss_scan_interval = scan_interval;
  }
  else
  {
    res = MTLK_ERR_VALUE;
  }

  return res;
}

int __MTLK_IFUNC mtlk_20_40_get_scan_interval (mtlk_20_40_coexistence_sm_t *coex_sm)
{
  MTLK_ASSERT(coex_sm != NULL);

  return coex_sm->obss_scan_interval;
}

void __MTLK_IFUNC mtlk_20_40_ap_process_coexistence_element (mtlk_20_40_coexistence_sm_t *coex_sm, 
  const mtlk_20_40_coexistence_element *coex_el, const IEEE_ADDR *src_addr)
{
  MTLK_ASSERT(coex_sm != NULL);
  MTLK_ASSERT(coex_el != NULL);
  MTLK_ASSERT(src_addr != NULL);

  if (mtlk_osal_atomic_get(&coex_sm->current_csm_state) == CSM_STATE_NOT_STARTED) 
  {
    ILOG3_V("Coexistence Element received, eCSM is CSM_STATE_NOT_STARTED");
    return;
  }

  mtlk_osal_lock_acquire(&coex_sm->lock);
  mtlk_coex_20_40_inc_cnt(coex_sm, MTLK_COEX_20_40_NOF_COEX_EL_RECEIVED);
  
  ILOG3_V("COEX_EL:");
  ILOG3_D("\t InformationRequest           = %d", coex_el->u8InformationRequest);
  ILOG3_D("\t TwentyMhzBSSWidthRequest     = %d", coex_el->u8FortyMhzIntolerant);
  ILOG3_D("\t FortyMhzIntolerant           = %d", coex_el->u8TwentyMhzBSSWidthRequest);
  ILOG3_D("\t OBSSScanningExemptionRequest = %d", coex_el->u8OBSSScanningExemptionRequest);
  ILOG3_D("\t OBSSScanningExemptionGrant   = %d", coex_el->u8OBSSScanningExemptionGrant);
  ILOG3_Y("\t Sender IEEE Address          = %Y", src_addr);
  
  if((mtlk_20_40_is_feature_enabled(coex_sm)) && 
     (MTLK_HW_BAND_2_4_GHZ == channel_to_band(mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CUR))))
  {
    if ((coex_el->u8FortyMhzIntolerant) || (coex_el->u8TwentyMhzBSSWidthRequest))
    {
      mtlk_sepm_register_station_intolerance (&coex_sm->ap_scexmpt, src_addr);
      /* If the source of this coexistence element is one of the connected stations,
         the database will be updated and will prevent working in 40 MHz mode until
         the reporting station disconnects from the AP */

      coex_sm->intolerance_db.general_intolerance_data.intolerant_detected = TRUE;
      coex_sm->intolerance_db.general_intolerance_data.intolerant_detection_ts = mtlk_osal_timestamp();
      switch (mtlk_osal_atomic_get(&coex_sm->current_csm_state))
      {
      case CSM_STATE_20:
        if ((coex_sm->limited_to_20 == FALSE) &&
            (coex_sm->intolerance_detected_at_first_scan == FALSE))
        {
          mtlk_osal_timer_set(&coex_sm->transition_timer, mtlk_20_40_calc_transition_timer(coex_sm));
          ILOG2_V("Transition timer is set, since coexistence element which limits AP to 20MHz processed");
        }
        break;

      case CSM_STATE_20_40:
        mtlk_osal_atomic_set(&coex_sm->current_csm_state, CSM_STATE_20);
        mtlk_cbsm_switch_to_20_mode(&coex_sm->cbsm, mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CUR));
        mtlk_coex_20_40_inc_cnt(coex_sm, MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_20);
        if ((coex_sm->limited_to_20 == FALSE) &&
            (coex_sm->intolerance_detected_at_first_scan == FALSE))
        {
          mtlk_osal_timer_set(&coex_sm->transition_timer, mtlk_20_40_calc_transition_timer(coex_sm));
          ILOG2_V("Transition timer is set, since coexistence element which limits AP to 20MHz processed (moved to 20MHz)");
        }
        break;

      default:
        ELOG_V("Bad eCSM_STATE in coex_sm");
        MTLK_ASSERT(0);
        break;
      }
    }// of: if ((coex_el->u8FortyMhzIntolerant) || (coex_el->u8TwentyMhzBSSWidthRequest)) 
    else
    {
      switch (mtlk_osal_atomic_get(&coex_sm->current_csm_state))
      {
      case CSM_STATE_20:
        if ((coex_sm->limited_to_20 == FALSE) &&
            (coex_sm->intolerance_detected_at_first_scan == FALSE))
        {
          uint16 primary_channel = 0;
          int secondary_channel_offset = 0;
          if (_mtlk_20_40_find_available_channel_pair(coex_sm, &primary_channel, &secondary_channel_offset, UMI_CHANNEL_SW_MODE_SCN /* no forcing */))
          {
            mtlk_osal_atomic_set(&coex_sm->current_csm_state, CSM_STATE_20_40);
            mtlk_cbsm_switch_to_40_mode(&coex_sm->cbsm, primary_channel, secondary_channel_offset);
            mtlk_coex_20_40_inc_cnt(coex_sm, MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_20_TO_40);
            mtlk_osal_timer_cancel_sync(&coex_sm->transition_timer);
            ILOG2_V("Transition timer has been canceled, since moved to 40MHz spectrum");
          }
        }
        break;

      case CSM_STATE_20_40:
        // do nothing 
        break;

      default:
        ELOG_V("Bad eCSM_STATE in coex_sm");
        MTLK_ASSERT(0);
        break;
      }
    }
    if (coex_el->u8OBSSScanningExemptionRequest)
    {
      mtlk_sepm_process_exemption_request(&coex_sm->ap_scexmpt, src_addr);
    }
  }

  mtlk_osal_lock_release(&coex_sm->lock);
}

void __MTLK_IFUNC mtlk_20_40_sta_process_coexistence_element (mtlk_20_40_coexistence_sm_t *coex_sm, 
  mtlk_20_40_coexistence_element *coex_el)
{
  MTLK_ASSERT(0);
  // The function has yet to be implemented

  MTLK_ASSERT(coex_sm != NULL);
  MTLK_ASSERT(coex_el != NULL);

  mtlk_coex_20_40_inc_cnt(coex_sm, MTLK_COEX_20_40_NOF_COEX_EL_RECEIVED);

  mtlk_osal_lock_acquire(&coex_sm->lock);
  mtlk_osal_lock_release(&coex_sm->lock);
}

void __MTLK_IFUNC mtlk_20_40_ap_process_obss_scan_results (mtlk_20_40_coexistence_sm_t *coex_sm,
  UMI_INTOLERANT_CHANNEL_DESCRIPTOR *intolerant_channels_descriptor)
{
  int i;

  MTLK_ASSERT(coex_sm != NULL);
  MTLK_ASSERT(intolerant_channels_descriptor != NULL);

  if (mtlk_osal_atomic_get(&coex_sm->current_csm_state) == CSM_STATE_NOT_STARTED) 
  {
    ILOG3_V("OBSS scan result  received, eCSM is CSM_STATE_NOT_STARTED");
    return;
  }

  mtlk_osal_lock_acquire(&coex_sm->lock);
  
  ILOG2_DD("Operating Class = %d, NumberOfIntolerantChannels = %d",
           intolerant_channels_descriptor->u8OperatingClass, 
           intolerant_channels_descriptor->u8NumberOfIntolerantChannels);

  for(i = 0 ; i < intolerant_channels_descriptor->u8NumberOfIntolerantChannels ; i++)
  {
    ILOG2_D("Intolerant channel detected on channel %d\n", intolerant_channels_descriptor->u8IntolerantChannels[i]);
  }

  if(mtlk_20_40_is_feature_enabled(coex_sm) && 
      (MTLK_HW_BAND_2_4_GHZ == channel_to_band(mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CUR))))
  {
    BOOL cur_primary_channel_intolerant = FALSE;
    if(intolerant_channels_descriptor->u8NumberOfIntolerantChannels > 0)
    {
      cur_primary_channel_intolerant = _mtlk_coex_20_40_intolerant_db_update_data_from_frame(coex_sm, intolerant_channels_descriptor, mtlk_osal_timestamp());
    }
    switch (mtlk_osal_atomic_get(&coex_sm->current_csm_state))
    {
      case CSM_STATE_20:
        if ((coex_sm->limited_to_20 == FALSE) &&
            (coex_sm->intolerance_detected_at_first_scan == FALSE))
        {
          if (cur_primary_channel_intolerant)
          {
            mtlk_osal_timer_set(&coex_sm->transition_timer, mtlk_20_40_calc_transition_timer(coex_sm));
            ILOG2_V("Transition timer is set, since OBSS scan results which limits AP to 20MHz processed");
          }
          else
          {
            _mtlk_20_40_check_if_can_move_to_40_move_if_so(coex_sm);
          }
        }
        break;

      case CSM_STATE_20_40:
        _mtlk_20_40_check_if_can_remain_40_move_if_not(coex_sm);
        break;

      default:
        ELOG_V("Bad eCSM_STATE in coex_sm");
        break;
    }
  }

  mtlk_osal_lock_release(&coex_sm->lock);
}

void __MTLK_IFUNC mtlk_20_40_ap_notify_non_ht_beacon_received (struct _mtlk_20_40_coexistence_sm *coex_sm, uint16 channel)
{
  MTLK_ASSERT(NULL != coex_sm);

  if (mtlk_osal_atomic_get(&coex_sm->current_csm_state) == CSM_STATE_NOT_STARTED) 
  {
    ILOG3_V("Non-HT beacon received, eCSM is CSM_STATE_NOT_STARTED");
    return;
  }

  ILOG2_D("Non HT beacon received on channel %d", channel);
  mtlk_osal_lock_acquire(&coex_sm->lock);

  if ((mtlk_20_40_is_feature_enabled(coex_sm)) && 
    (MTLK_HW_BAND_2_4_GHZ == channel_to_band(mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CUR))))
  {
    if (mtlk_osal_atomic_get(&coex_sm->current_csm_state) == CSM_STATE_20_40)
    {
      _mtlk_20_40_check_if_can_remain_40_move_if_not(coex_sm);
    }
    else if (mtlk_osal_atomic_get(&coex_sm->current_csm_state) == CSM_STATE_20)
    {
      if (channel == mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CUR))
      {
        if ((coex_sm->limited_to_20 == FALSE) &&
            (coex_sm->intolerance_detected_at_first_scan == FALSE))
        {
          mtlk_osal_timer_set(&coex_sm->transition_timer, mtlk_20_40_calc_transition_timer(coex_sm));
          ILOG2_V("Transition timer is set, since a non-HT beacon was received");
        }
      }
      else
      {
        _mtlk_20_40_check_if_can_move_to_40_move_if_so(coex_sm);
      }
    }
  }

  mtlk_osal_lock_release(&coex_sm->lock);
}

void __MTLK_IFUNC mtlk_20_40_ap_notify_intolerant_or_legacy_station_connected (struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL dont_lock)
{
  MTLK_ASSERT(NULL != coex_sm);

  if (mtlk_osal_atomic_get(&coex_sm->current_csm_state) == CSM_STATE_NOT_STARTED) 
  {
    ILOG3_V("ERROR: eCSM is CSM_STATE_NOT_STARTED");
    return;
  }

  ILOG2_V("An intolerant or legacy station has connected to the AP");

  if (dont_lock == FALSE)
  {
    mtlk_osal_lock_acquire(&coex_sm->lock);
  }

  if ((mtlk_20_40_is_feature_enabled(coex_sm)) && 
    (MTLK_HW_BAND_2_4_GHZ == channel_to_band(mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CUR))))
  {
    if (mtlk_osal_atomic_get(&coex_sm->current_csm_state) == CSM_STATE_20_40)
    {
      int secondary_channel_offset = 0;
      uint16 primary_channel = (*coex_sm->xfaces.get_cur_channels)(coex_sm->xfaces.context, &secondary_channel_offset);
      mtlk_osal_atomic_set(&coex_sm->current_csm_state, CSM_STATE_20);
      mtlk_cbsm_switch_to_20_mode(&coex_sm->cbsm, primary_channel);
      /* We are switching to the 20 MHz mode, but we're not going to set the timer until all legacy stations
         disconnect from the AP */
      mtlk_coex_20_40_inc_cnt(coex_sm, MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_20);
    }
  }

  if (dont_lock == FALSE)
  {
    mtlk_osal_lock_release(&coex_sm->lock);
  }
}

void __MTLK_IFUNC mtlk_20_40_ap_notify_last_40_incapable_station_disconnected(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL dont_lock)
{
  MTLK_ASSERT(NULL != coex_sm);

  if (mtlk_osal_atomic_get(&coex_sm->current_csm_state) == CSM_STATE_NOT_STARTED) 
  {
    ILOG3_V("ERROR: eCSM is CSM_STATE_NOT_STARTED");
    return;
  }

  ILOG2_V("The last legacy station has disconnected from the AP");

  if (dont_lock == FALSE)
  {
    mtlk_osal_lock_acquire(&coex_sm->lock);
  }

  if ((mtlk_20_40_is_feature_enabled(coex_sm)) && 
    (MTLK_HW_BAND_2_4_GHZ == channel_to_band(mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CUR))))
  {
    if (mtlk_osal_atomic_get(&coex_sm->current_csm_state) == CSM_STATE_20)
    {
      if ((coex_sm->limited_to_20 == FALSE) &&
          (coex_sm->intolerance_detected_at_first_scan == FALSE))
      {
        mtlk_osal_timer_set(&coex_sm->transition_timer, mtlk_20_40_calc_transition_timer(coex_sm));
        ILOG2_V("Transition timer is set, since the last legacy station has disconnected");
      }
    }
  }

  if (dont_lock == FALSE)
  {
    mtlk_osal_lock_release(&coex_sm->lock);
  }
}

BOOL __MTLK_IFUNC mtlk_20_40_is_20_40_operation_permitted(struct _mtlk_20_40_coexistence_sm *coex_sm,
  uint16 primary_channel, uint8 secondary_channel_offset)
{
  BOOL ret_val;
  uint16 secondary_channel;
  int reg_domain;
  MTLK_ASSERT(NULL != coex_sm);

  mtlk_osal_lock_acquire(&coex_sm->lock);

  secondary_channel = 0;
  reg_domain = (*coex_sm->xfaces.get_reg_domain)(coex_sm->xfaces.context);
  mtlk_channels_find_secondary_channel_no(reg_domain, primary_channel, secondary_channel_offset, &secondary_channel);
  ret_val = (BOOL) mtlk_coex_lve_evaluate(&coex_sm->coexlve,
                                          LVT_20_40_OPERATION_PERMITTED, 
                                          primary_channel, 
                                          secondary_channel);
  mtlk_osal_lock_release(&coex_sm->lock);

  return ret_val;
}

void __MTLK_IFUNC mtlk_20_40_set_intolerance_at_first_scan_flag(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL intolerant, BOOL dont_lock)
{
  MTLK_ASSERT(NULL != coex_sm);

  if (dont_lock == FALSE)
  {
    mtlk_osal_lock_acquire(&coex_sm->lock);
  }

  if (coex_sm->intolerance_detected_at_first_scan != intolerant)
  {
    if (coex_sm->intolerance_detected_at_first_scan == TRUE)
    {
      if (coex_sm->limited_to_20 == FALSE)
      {
        if ((mtlk_20_40_is_feature_enabled(coex_sm)) && 
            (MTLK_HW_BAND_2_4_GHZ == channel_to_band(mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CUR))) &&
            (mtlk_osal_atomic_get(&coex_sm->current_csm_state) == CSM_STATE_20))
        {   /* The timer was not set previously because the first scan had detected an intolerance of some kind.
               we have an external source of intolerance-related information, so we will start periodically
               checking whether we can move to 40 MHz */
            mtlk_osal_timer_set(&coex_sm->transition_timer, mtlk_20_40_calc_transition_timer(coex_sm));
        }
      }
    }

    coex_sm->intolerance_detected_at_first_scan = intolerant;
  }

  if (dont_lock == FALSE)
  {
    mtlk_osal_lock_release(&coex_sm->lock);
  }
}

static void _mtlk_20_40_check_if_can_remain_40_move_if_not(mtlk_20_40_coexistence_sm_t *coex_sm)
{
  uint16 primary_channel = 0;
  uint16 secondary_channel = 0;
  int secondary_channel_offset = 0;
  uint8 reg_domain = 0;

  MTLK_ASSERT(NULL != coex_sm);

  primary_channel = (*coex_sm->xfaces.get_cur_channels)(coex_sm->xfaces.context, &secondary_channel_offset);
  reg_domain = (*coex_sm->xfaces.get_reg_domain)(coex_sm->xfaces.context);
  if (mtlk_channels_find_secondary_channel_no(reg_domain, primary_channel, secondary_channel_offset, &secondary_channel) &&
      !mtlk_coex_lve_evaluate(&coex_sm->coexlve,
                              LVT_20_40_OPERATION_PERMITTED, 
                              primary_channel, 
                              secondary_channel))
  { /* We can not stay on the current channel pair */
    if (_mtlk_20_40_find_available_channel_pair(coex_sm, &primary_channel, &secondary_channel_offset, UMI_CHANNEL_SW_MODE_SCN /* no forcing */))
    { /* A different suitable channel pair exists, we'll move to it */
      mtlk_cbsm_switch_to_40_mode(&coex_sm->cbsm, primary_channel, secondary_channel_offset);
      mtlk_coex_20_40_inc_cnt(coex_sm, MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_40);
    }
    else
    { /* We'll have to switch to 20 MHz mode */
      mtlk_osal_atomic_set(&coex_sm->current_csm_state, CSM_STATE_20);
      primary_channel = (*coex_sm->xfaces.get_cur_channels)(coex_sm->xfaces.context, &secondary_channel_offset);
      mtlk_cbsm_switch_to_20_mode(&coex_sm->cbsm, primary_channel);
      mtlk_coex_20_40_inc_cnt(coex_sm, MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_20);
      if ((coex_sm->limited_to_20 == FALSE) &&
          (coex_sm->intolerance_detected_at_first_scan == FALSE))
      {
        mtlk_osal_timer_set(&coex_sm->transition_timer, mtlk_20_40_calc_transition_timer(coex_sm));
        ILOG2_V("Transition timer is set, since no available channel pair is found (moved to 20MHz)");
      }
    }
  }
}

static void _mtlk_20_40_check_if_can_move_to_40_move_if_so(mtlk_20_40_coexistence_sm_t *coex_sm)
{
  uint16 primary_channel = 0;
  int secondary_channel_offset = 0;

  MTLK_ASSERT(NULL != coex_sm);
  
  if (coex_sm->intolerance_detected_at_first_scan == FALSE)
  {
    int secondary_channel_offset_candidate = UMI_CHANNEL_SW_MODE_SCN /* no forcing */;
    int configured_primary_channel = mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CFG);
    if (configured_primary_channel != 0 /* a channel or a channel pair are forced by the user */)
    {
      int secondary_channel_offset = 0;
      uint16 cur_primary_channel = (*coex_sm->xfaces.get_cur_channels)(coex_sm->xfaces.context, &secondary_channel_offset);
      if (cur_primary_channel == configured_primary_channel)
      { /* We're still using the user-defined primary channel */
        secondary_channel_offset_candidate = secondary_channel_offset;
      }
    }
    if (_mtlk_20_40_find_available_channel_pair(coex_sm, &primary_channel, &secondary_channel_offset, secondary_channel_offset_candidate))
    {
      mtlk_osal_atomic_set(&coex_sm->current_csm_state, CSM_STATE_20_40);
      mtlk_cbsm_switch_to_40_mode(&coex_sm->cbsm, primary_channel, secondary_channel_offset);
      mtlk_coex_20_40_inc_cnt(coex_sm, MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_20_TO_40);
      mtlk_osal_timer_cancel_sync(&coex_sm->transition_timer);
      ILOG2_V("Transition timer has been canceled, since moved to 40MHz spectrum");
    }
  }
}

void __MTLK_IFUNC mtlk_20_40_sta_process_obss_scan_results (mtlk_20_40_coexistence_sm_t *coex_sm, 
  mtlk_20_40_obss_scan_results_t *obss_scan_results)
{
  MTLK_ASSERT(0);
  // The function has yet to be implemented

  MTLK_ASSERT(coex_sm != NULL);
  MTLK_ASSERT(obss_scan_results != NULL);

  mtlk_osal_lock_acquire(&coex_sm->lock);
  mtlk_osal_lock_release(&coex_sm->lock);
}

void __MTLK_IFUNC mtlk_20_40_sta_notify_switch_to_20_mode ( mtlk_20_40_coexistence_sm_t *coex_sm,
  int channel)
{
  MTLK_ASSERT(0);
  // The function has yet to be implemented

  MTLK_ASSERT(coex_sm != NULL);

  mtlk_osal_lock_acquire(&coex_sm->lock);
  mtlk_osal_lock_release(&coex_sm->lock);
}

void __MTLK_IFUNC mtlk_20_40_sta_notify_switch_to_40_mode (mtlk_20_40_coexistence_sm_t *coex_sm,
  int primary_channel, int secondary_channel)
{
  MTLK_ASSERT(0);
  // The function has yet to be implemented

  MTLK_ASSERT(coex_sm != NULL);

  mtlk_osal_lock_acquire(&coex_sm->lock);
  mtlk_osal_lock_release(&coex_sm->lock);
}

void __MTLK_IFUNC mtlk_20_40_register_station (mtlk_20_40_coexistence_sm_t *coex_sm,
  const IEEE_ADDR *sta_addr, BOOL supports_coexistence, BOOL exempt, BOOL intolerant, BOOL legacy)
{
  MTLK_ASSERT(coex_sm != NULL);
  MTLK_ASSERT(sta_addr != NULL);

  ILOG1_YDDDD("Registering a new station, IEEE address = %Y, coex = %d, exempt = %d, intolerant = %d, legacy = %d", sta_addr, supports_coexistence, exempt, intolerant, legacy);
  mtlk_osal_lock_acquire(&coex_sm->lock);
  
  mtlk_sepm_register_station (&coex_sm->ap_scexmpt, sta_addr, supports_coexistence, exempt, intolerant, legacy);
  if ((supports_coexistence == TRUE) && (exempt == FALSE))
  {
    mtlk_20_40_set_intolerance_at_first_scan_flag(coex_sm, FALSE, TRUE /* don't lock, already locked */);
  }

  mtlk_osal_lock_release(&coex_sm->lock);
}

void __MTLK_IFUNC mtlk_20_40_unregister_station (mtlk_20_40_coexistence_sm_t *coex_sm, 
  const IEEE_ADDR *sta_addr)
{
  MTLK_ASSERT(coex_sm != NULL);
  MTLK_ASSERT(sta_addr != NULL);

  ILOG2_Y("Unregistering a station, IEEE address = %Y", sta_addr);
  mtlk_osal_lock_acquire(&coex_sm->lock);

  mtlk_sepm_unregister_station (&coex_sm->ap_scexmpt, sta_addr);

  mtlk_osal_lock_release(&coex_sm->lock);
}


/* Interface functions to be used by the child modules */
void __MTLK_IFUNC mtlk_20_40_perform_idb_update(mtlk_20_40_coexistence_sm_t *coex_sm)
{
  MTLK_ASSERT(NULL != coex_sm);

  (*coex_sm->xfaces.enumerate_external_intolerance_info)(
    (mtlk_handle_t)coex_sm, 
    coex_sm->xfaces.context,
    &_mtlk_coex_20_40_external_intolerance_enumerator_callback,
    mtlk_20_40_calc_transition_timer(coex_sm));
}

uint32 __MTLK_IFUNC mtlk_20_40_calc_transition_timer(mtlk_20_40_coexistence_sm_t *coex_sm)
{
  MTLK_ASSERT(coex_sm != NULL);

  return (MTLK_OSAL_MSEC_IN_SEC * mtlk_20_40_get_transition_delay_factor (coex_sm) * mtlk_20_40_get_scan_interval (coex_sm));
}

BOOL __MTLK_IFUNC mtlk_20_40_is_coex_el_intolerant_bit_detected(mtlk_20_40_coexistence_sm_t *coex_sm)
{
  BOOL res = FALSE;

  MTLK_ASSERT(coex_sm != NULL);

  if (coex_sm->intolerance_db.general_intolerance_data.intolerant_detected)
  {
    if (mtlk_20_40_calc_transition_timer(coex_sm) > 
              mtlk_osal_time_passed_ms(coex_sm->intolerance_db.general_intolerance_data.intolerant_detection_ts))
    {
      res = TRUE;
    }
    else
    {
      coex_sm->intolerance_db.general_intolerance_data.intolerant_detected = FALSE;
      coex_sm->intolerance_db.general_intolerance_data.intolerant_detection_ts = mtlk_osal_timestamp();
    }
  }

  return res;
}

/* Internal functions */

static BOOL
  _mtlk_20_40_find_available_channel_pair (mtlk_20_40_coexistence_sm_t *coex_sm, uint16 *primary_channel, int *secondary_channel_offset, int forced_secondary_channel_offset)
{
  BOOL res = FALSE;
  int primary_channel_candidate = 0;
  int secondary_channel_offset_first_candidate = 0;
  int secondary_channel_offset_second_candidate = 0;
  uint16 secondary_channel_candidate = 0;
  uint8 reg_domain;

  MTLK_ASSERT(NULL != coex_sm);
  MTLK_ASSERT(NULL != primary_channel);
  MTLK_ASSERT(NULL != secondary_channel_offset);

  *primary_channel = 0;
  *secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCN;
  primary_channel_candidate = mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CUR);
  reg_domain = (*coex_sm->xfaces.get_reg_domain)(coex_sm->xfaces.context);
  switch (forced_secondary_channel_offset)
  {
    case UMI_CHANNEL_SW_MODE_SCA:
      secondary_channel_offset_first_candidate = UMI_CHANNEL_SW_MODE_SCA;
      break;
    case UMI_CHANNEL_SW_MODE_SCB:
      secondary_channel_offset_first_candidate = UMI_CHANNEL_SW_MODE_SCB;
      break;
    case UMI_CHANNEL_SW_MODE_SCN:
    default:
      secondary_channel_offset_first_candidate = UMI_CHANNEL_SW_MODE_SCA;
      secondary_channel_offset_second_candidate = UMI_CHANNEL_SW_MODE_SCB;
      break;
  }
  if (mtlk_channels_find_secondary_channel_no(reg_domain, primary_channel_candidate, secondary_channel_offset_first_candidate, &secondary_channel_candidate))
  {
    if (mtlk_coex_lve_evaluate(&coex_sm->coexlve, LVT_20_40_OPERATION_PERMITTED, primary_channel_candidate, secondary_channel_candidate))
    {
      *primary_channel = primary_channel_candidate;
      *secondary_channel_offset = secondary_channel_offset_first_candidate;
      res = TRUE;
    }
  }
  if (!res && (forced_secondary_channel_offset == UMI_CHANNEL_SW_MODE_SCN) /* no forcing */)
  {
    if (mtlk_channels_find_secondary_channel_no(reg_domain, primary_channel_candidate, secondary_channel_offset_second_candidate, &secondary_channel_candidate))
    {
      if (mtlk_coex_lve_evaluate(&coex_sm->coexlve, LVT_20_40_OPERATION_PERMITTED, primary_channel_candidate, secondary_channel_candidate))
      {
        *primary_channel = primary_channel_candidate;
        *secondary_channel_offset = secondary_channel_offset_second_candidate;
        res = TRUE;
      }
    }
  }
  if (res)
  {
    ILOG2_DD("Found a channel pair to switch to: Primary channel = %d, Secondary channel offset = %d", primary_channel, secondary_channel_offset);
  } 
  else
  {
    ILOG2_V("No legal pair of channels is found to switch to");
  }
  
  return res;
}

static uint32 __MTLK_IFUNC
  _mtlk_transition_delay_timeout_callback (mtlk_osal_timer_t *timer, mtlk_handle_t clb_usr_data)
{
  mtlk_20_40_coexistence_sm_t *coex_sm = (mtlk_20_40_coexistence_sm_t*)clb_usr_data;
  uint16 primary_channel = 0;
  int secondary_channel_offset = 0;

  MTLK_ASSERT(NULL != coex_sm);
  MTLK_ASSERT(NULL != timer);

  ILOG2_V("Transition timer timeout, checking option to move to 40MHz spectrum");
  mtlk_osal_lock_acquire(&coex_sm->lock);

  if (mtlk_20_40_is_feature_enabled(coex_sm) &&
      (mtlk_osal_atomic_get(&coex_sm->current_csm_state) == CSM_STATE_20) && 
    (MTLK_HW_BAND_2_4_GHZ == channel_to_band(mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CUR))))
  {
    if (coex_sm->intolerance_detected_at_first_scan == FALSE)
    {
      if (_mtlk_20_40_find_available_channel_pair(coex_sm, &primary_channel, &secondary_channel_offset, UMI_CHANNEL_SW_MODE_SCN /* no forcing */))
      {
        mtlk_osal_atomic_set(&coex_sm->current_csm_state, CSM_STATE_20_40);
        mtlk_cbsm_switch_to_40_mode(&coex_sm->cbsm, primary_channel, secondary_channel_offset);
        mtlk_coex_20_40_inc_cnt(coex_sm, MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_20_TO_40);
      }
    }
  }

  mtlk_osal_lock_release(&coex_sm->lock);

  return 0; /* Don't reactivate the timer */
}

static BOOL _mtlk_coex_20_40_intolerant_db_update_data_from_frame( mtlk_20_40_coexistence_sm_t *coex_sm, UMI_INTOLERANT_CHANNEL_DESCRIPTOR *descriptor, mtlk_osal_timestamp_t rec_ts )
{
  int i;
  uint32 transition_timer_time;
  uint16 cur_primary_channel;
  BOOL cur_primary_channel_intolerant = FALSE;

  MTLK_ASSERT(NULL != coex_sm);
  MTLK_ASSERT(NULL != descriptor);

  transition_timer_time = mtlk_20_40_calc_transition_timer(coex_sm);
  cur_primary_channel = (uint16)mtlk_pdb_get_int(mtlk_vap_get_param_db(coex_sm->xfaces.vap_handle), PARAM_DB_CORE_CHANNEL_CUR);
  
  for (i = 0; i < descriptor->u8NumberOfIntolerantChannels; i ++)
  {
    if (descriptor->u8IntolerantChannels[i] == cur_primary_channel)
    {
      cur_primary_channel_intolerant = TRUE;
    }
    if (mtlk_osal_time_after(rec_ts, coex_sm->intolerance_db.channels_list[descriptor->u8IntolerantChannels[i]-1].intolerant_detection_ts))
    {
      coex_sm->intolerance_db.channels_list[descriptor->u8IntolerantChannels[i]-1].intolerant = TRUE;
      coex_sm->intolerance_db.channels_list[descriptor->u8IntolerantChannels[i]-1].intolerant_detection_ts = rec_ts;
    }
    if (mtlk_osal_time_after(rec_ts, coex_sm->intolerance_db.channels_list[descriptor->u8IntolerantChannels[i]-1].primary_detection_ts))
    {
      coex_sm->intolerance_db.channels_list[descriptor->u8IntolerantChannels[i]-1].primary = TRUE;
      coex_sm->intolerance_db.channels_list[descriptor->u8IntolerantChannels[i]-1].primary_detection_ts = rec_ts;
    }
    /* since intolerant, can not have a secondary channel, hence - no update is done */
  }

  return cur_primary_channel_intolerant;
}

static void _mtlk_coex_20_40_external_intolerance_enumerator_callback(mtlk_handle_t context,
  mtlk_20_40_external_intolerance_info_t *external_intolerance_info)
{
  mtlk_20_40_coexistence_sm_t *coex_sm = (mtlk_20_40_coexistence_sm_t*)context;

  MTLK_ASSERT(NULL != coex_sm);
  MTLK_ASSERT(NULL != external_intolerance_info);
  
  /* use channel-1 inside the array since array range is 0..13, while channels range is 1..14*/
  if ((mtlk_20_40_calc_transition_timer(coex_sm)) > mtlk_osal_time_passed_ms(external_intolerance_info->timestamp))
  {
    if (!external_intolerance_info->is_ht)
    {
      if (!coex_sm->intolerance_db.channels_list[external_intolerance_info->channel-1].intolerant ||
          mtlk_osal_time_after(external_intolerance_info->timestamp, coex_sm->intolerance_db.channels_list[external_intolerance_info->channel-1].intolerant_detection_ts))
      {
        coex_sm->intolerance_db.channels_list[external_intolerance_info->channel-1].intolerant_detection_ts = external_intolerance_info->timestamp;
        coex_sm->intolerance_db.channels_list[external_intolerance_info->channel-1].intolerant = TRUE;
      }
    }
    if (external_intolerance_info->forty_mhz_intolerant) /* && recognized at AP BSA */
    {
      if (!coex_sm->intolerance_db.general_intolerance_data.intolerant_detected ||
          mtlk_osal_time_after(external_intolerance_info->timestamp, coex_sm->intolerance_db.general_intolerance_data.intolerant_detection_ts))
      {
        coex_sm->intolerance_db.general_intolerance_data.intolerant_detection_ts = external_intolerance_info->timestamp;
        coex_sm->intolerance_db.general_intolerance_data.intolerant_detected = TRUE;
      }
    }
    /* update DB on primary and secondary channels detection */
    if (!coex_sm->intolerance_db.channels_list[external_intolerance_info->channel-1].primary ||
      mtlk_osal_time_after(external_intolerance_info->timestamp, coex_sm->intolerance_db.channels_list[external_intolerance_info->channel-1].primary_detection_ts))
    {
      coex_sm->intolerance_db.channels_list[external_intolerance_info->channel-1].primary_detection_ts = external_intolerance_info->timestamp;
      coex_sm->intolerance_db.channels_list[external_intolerance_info->channel-1].primary = TRUE;
    }
    if (external_intolerance_info->secondary_channel_offset != UMI_CHANNEL_SW_MODE_SCN)
    {
      uint8 reg_domain;
      uint16 secondary_channel;

      reg_domain = (*coex_sm->xfaces.get_reg_domain)(coex_sm->xfaces.context);
      if (mtlk_channels_find_secondary_channel_no(reg_domain, external_intolerance_info->channel, external_intolerance_info->secondary_channel_offset, &secondary_channel) &&
          (!coex_sm->intolerance_db.channels_list[external_intolerance_info->channel].secondary ||
          mtlk_osal_time_after(external_intolerance_info->timestamp, coex_sm->intolerance_db.channels_list[secondary_channel-1].secodnary_detection_ts)))
      {
        coex_sm->intolerance_db.channels_list[secondary_channel-1].secodnary_detection_ts = external_intolerance_info->timestamp;
        coex_sm->intolerance_db.channels_list[secondary_channel-1].secondary = TRUE;
      }
    }
    
  }
}

static int _mtlk_20_40_register_ability_set(mtlk_20_40_coexistence_sm_t *coex_sm, const mtlk_ability_id_t* ab_id_list, uint32 ab_id_num)
{
  MTLK_ASSERT(NULL != coex_sm);
  MTLK_ASSERT(NULL != ab_id_list);

  return (*coex_sm->xfaces.ability_control)(coex_sm->xfaces.context, eAO_REGISTER, ab_id_list, ab_id_num);
}

static int _mtlk_20_40_unregister_ability_set(mtlk_20_40_coexistence_sm_t *coex_sm, const mtlk_ability_id_t* ab_id_list, uint32 ab_id_num)
{
  MTLK_ASSERT(NULL != coex_sm);
  MTLK_ASSERT(NULL != ab_id_list);

  return (*coex_sm->xfaces.ability_control)(coex_sm->xfaces.context, eAO_UNREGISTER, ab_id_list, ab_id_num);
}

static int _mtlk_20_40_enable_ability_set(mtlk_20_40_coexistence_sm_t *coex_sm, const mtlk_ability_id_t* ab_id_list, uint32 ab_id_num)
{
  MTLK_ASSERT(NULL != coex_sm);
  MTLK_ASSERT(NULL != ab_id_list);

  return (*coex_sm->xfaces.ability_control)(coex_sm->xfaces.context, eAO_ENABLE, ab_id_list, ab_id_num);
}

static int _mtlk_20_40_disable_ability_set(mtlk_20_40_coexistence_sm_t *coex_sm, const mtlk_ability_id_t* ab_id_list, uint32 ab_id_num)
{
  MTLK_ASSERT(NULL != coex_sm);
  MTLK_ASSERT(NULL != ab_id_list);

  return (*coex_sm->xfaces.ability_control)(coex_sm->xfaces.context, eAO_DISABLE, ab_id_list, ab_id_num);
}

static void _mtlk_20_40_set_limit_to_20(mtlk_20_40_coexistence_sm_t *coex_sm, BOOL limit_flag)
{
  MTLK_ASSERT(NULL != coex_sm);

  ILOG2_D("Limit to 20 flag is set to %d", limit_flag);
  coex_sm->limited_to_20 = limit_flag;
}

/* statistics */
void __MTLK_IFUNC mtlk_coex_20_40_inc_cnt (mtlk_20_40_coexistence_sm_t *coex_sm, coex_20_40_info_cnt_id_e cnt_id)
{
  MTLK_ASSERT(NULL != coex_sm);
  MTLK_ASSERT(cnt_id >= 0 && cnt_id < MTLK_COEX_20_40_CNT_LAST);

  mtlk_wss_cntr_inc(coex_sm->wss_hcntrs[cnt_id]);
}
