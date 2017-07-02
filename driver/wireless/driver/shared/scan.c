/*
 * $Id: scan.c 11966 2011-11-20 16:30:24Z fleytman $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Roman Sikorskyy & Andriy Fidrya
 *
 */

#include "mtlkinc.h"
#include "scan.h"
#include "mhi_umi.h"
#include "channels.h"
#include "mtlk_coreui.h"
#include "mtlk_gpl_helper.h"
#include "mtlkhal.h"
#include "mtlkaux.h"
#include "mtlkmib.h"
#include "mtlk_core_iface.h"
#include "mtlk_param_db.h"

#define LOG_LOCAL_GID   GID_SCAN
#define LOG_LOCAL_FID   1

static const mtlk_ability_id_t _scan_abilities[] = {
  MTLK_CORE_REQ_START_SCANNING,
  MTLK_CORE_REQ_GET_SCANNING_RES,
  MTLK_CORE_REQ_GET_SCAN_CFG,
  MTLK_CORE_REQ_SET_SCAN_CFG
};
#define _scan_idle_abilities _scan_abilities

/* the set of AOCS abilities which are enabled in SCAN ACTIVE state
 * (when SCAN has started already)
 **/
static const mtlk_ability_id_t _scan_active_abilities[] = {
  /* Secondary SCAN request is needed for proper */
  /* interaction with wpa_supplicant.            */
  MTLK_CORE_REQ_START_SCANNING,
  MTLK_CORE_REQ_GET_SCANNING_RES
};

static int
_mtlk_scan_abilities_init(struct mtlk_scan *scan)
{
  int res = MTLK_ERR_OK;
  if (!mtlk_vap_is_ap(scan->vap_handle)) {
    res = mtlk_abmgr_register_ability_set(mtlk_vap_get_abmgr(scan->vap_handle),
                                               _scan_abilities, ARRAY_SIZE(_scan_abilities));

    if (MTLK_ERR_OK == res) {
      mtlk_abmgr_enable_ability_set(mtlk_vap_get_abmgr(scan->vap_handle),
                                         _scan_idle_abilities, ARRAY_SIZE(_scan_idle_abilities));
    }
  }

  return res;
}

static void
_mtlk_scan_abilities_cleanup(struct mtlk_scan *scan)
{
  if (!mtlk_vap_is_ap(scan->vap_handle)) {
    mtlk_abmgr_disable_ability_set(mtlk_vap_get_abmgr(scan->vap_handle),
                                    _scan_abilities, ARRAY_SIZE(_scan_abilities));
    mtlk_abmgr_unregister_ability_set(mtlk_vap_get_abmgr(scan->vap_handle),
                                      _scan_abilities, ARRAY_SIZE(_scan_abilities));
  }
}

static void
_mtlk_scan_abilities_set_idle(struct mtlk_scan *scan)
{
  if (!mtlk_vap_is_ap(scan->vap_handle)) {
    mtlk_abmgr_disable_ability_set(mtlk_vap_get_abmgr(scan->vap_handle),
                                    _scan_abilities, ARRAY_SIZE(_scan_abilities));
    mtlk_abmgr_enable_ability_set(mtlk_vap_get_abmgr(scan->vap_handle),
                                   _scan_idle_abilities, ARRAY_SIZE(_scan_idle_abilities));
  }
}

static void
_mtlk_scan_abilities_set_active(struct mtlk_scan *scan)
{
  if (!mtlk_vap_is_ap(scan->vap_handle)) {
    mtlk_abmgr_disable_ability_set(mtlk_vap_get_abmgr(scan->vap_handle),
                                   _scan_idle_abilities, ARRAY_SIZE(_scan_idle_abilities));
    mtlk_abmgr_enable_ability_set(mtlk_vap_get_abmgr(scan->vap_handle),
                                  _scan_active_abilities, ARRAY_SIZE(_scan_active_abilities));
  }
}

/*************************************************************************************************
 * SCAN implementation
 *************************************************************************************************/
static int __MTLK_IFUNC
_mtlk_scan_handle_evt_pause_elapsed(mtlk_handle_t scan_object, const void *payload, uint32 size);

static int complete_or_continue(struct mtlk_scan *scan);
static uint32 pause_timer_handler(mtlk_osal_timer_t *timer, mtlk_handle_t data);

static int
load_and_configure_progmodel(struct mtlk_scan *scan)
{
  int res;
  mtlk_aux_pm_related_params_t params;

  if(!mtlk_progmodel_is_loaded(scan->progmodels[scan->cur_band])) {
    res = mtlk_progmodel_load_to_hw(scan->progmodels[scan->cur_band]);
    if (res != MTLK_ERR_OK) {
      ELOG_D("Unable to load progmodel to HW, error code %d", res);
      return res;
    }
  }

  /* we should reconfigure MIBs anyway, because even if progmodel is already loaded,
   * it could be configured with different values (for example with the ones used for connection)
   */
  return mtlk_aux_pm_related_params_set_defaults(scan->config.txmm,
                                                 get_net_mode(scan->cur_band, 1),
                                                 mtlk_progmodel_get_spectrum(scan->progmodels[scan->cur_band]),
                                                 &params);
}

static int
switch_back_progmodel(struct mtlk_scan *scan)
{
  bss_data_t bss_found;
  int res;
  mtlk_aux_pm_related_params_t params;
  uint8 band = mtlk_pdb_get_int(mtlk_vap_get_param_db(scan->vap_handle), PARAM_DB_CORE_FREQ_BAND_CUR);
  unsigned char bssid[ETH_ALEN];
  
  MTLK_ASSERT(band < MTLK_HW_BAND_BOTH);
  MTLK_ASSERT(scan->params.is_background);

  if (!mtlk_progmodel_is_loaded(scan->progmodels[band])) {
    res = mtlk_progmodel_load_to_hw(scan->progmodels[band]);
    if (res != MTLK_ERR_OK) {
      ELOG_D("Unable to load progmodel to HW, error code %d", res);
      return res;
    }
  }

  mtlk_pdb_get_mac(
          mtlk_vap_get_param_db(scan->vap_handle), PARAM_DB_CORE_BSSID, bssid);

  if (mtlk_cache_find_bss_by_bssid(scan->config.bss_cache, bssid, &bss_found, NULL) == 0) {
    ELOG_V("Can't re-configure connection - unknown BSS");
    return MTLK_ERR_UNKNOWN;
  }

  return mtlk_aux_pm_related_params_set_bss_based(scan->config.txmm,
                                                  &bss_found,
                                                  mtlk_pdb_get_int(mtlk_vap_get_param_db(scan->vap_handle), PARAM_DB_CORE_NET_MODE_CUR),
                                                  mtlk_pdb_get_int(mtlk_vap_get_param_db(scan->vap_handle), PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE),
                                                  &params);
}

static int 
send_ps_req(struct mtlk_scan *scan, BOOL enabled)
{
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t* man_entry = NULL;
  UMI_PS *msg;
  int res;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg,
                                                 scan->config.txmm,
                                                 NULL);
  if (man_entry == NULL) {
    ELOG_V("No MM slot: failed to PS request");
    return MTLK_ERR_SCAN_FAILED;
  }

  man_entry->id           = MC_UM_PS_REQ;
  man_entry->payload_size = sizeof(UMI_PS);
  msg = (UMI_PS *)man_entry->payload;

  memset(msg, 0, sizeof(*msg));

  msg->PS_Mode = enabled ? PS_REQ_MODE_ON : PS_REQ_MODE_OFF;

  mtlk_dump(2, man_entry->payload, sizeof(UMI_PS), "dump of UMI_PS payload:");

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);

  if(res == MTLK_ERR_OK && msg->status != UMI_OK)
    res = MTLK_ERR_UNKNOWN;

  mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static void
init_default_params(struct mtlk_scan_params *params)
{
  memset(params, 0, sizeof(struct mtlk_scan_params));
  params->bss_type = UMI_BSS_INFRA; 

  params->min_scan_time = 10;
  params->max_scan_time = 100;
  params->probe_rate = 10;
  params->num_probes = 3;

  params->channels_per_chunk_limit = 2; // this is 200 ms max for passive scan with max_scan_time = 100
  params->pause_between_chunks = 50; //ms
  params->is_background_scan_enabled = FALSE;
}

int __MTLK_IFUNC
mtlk_scan_init (struct mtlk_scan *scan, struct mtlk_scan_config config, mtlk_vap_handle_t vap_handle)
{
  int res;

  MTLK_ASSERT(!scan->initialized);

  memset(scan, 0, sizeof(*scan));
  scan->vap_handle = vap_handle;
  scan->config = config;
  init_default_params(&scan->params);

  if (NULL == (scan->vector = mtlk_scan_create_vector())) {
    ELOG_V("Failed to create scan vector");
    res = MTLK_ERR_UNKNOWN;
    goto err_create_scan_vector;
  }

  mtlk_osal_atomic_set(&scan->is_running, FALSE);
  mtlk_osal_atomic_set(&scan->treminate_scan, FALSE);
  scan->is_synchronous = FALSE;

  res = mtlk_osal_event_init(&scan->completed);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Failed to initialize scan completed event with err code %i", res);
    goto err_completed_event;
  }

  res = mtlk_osal_timer_init(&scan->pause_timer, pause_timer_handler, HANDLE_T(scan));
  if (res != MTLK_ERR_OK) {
    ELOG_D("Failed to initialize timer with err code %i", res);
    goto err_timer;
  }

  res = mtlk_txmm_msg_init(&scan->async_scan_msg);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Failed to initialize scan async msg with err code %i", res);
    goto err_async_msg;
  }

  scan->flctrl_id = 0;
  res = mtlk_flctrl_register(scan->config.hw_tx_flctrl, &scan->flctrl_id);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Failed to register in flctrl with err code %i", res);
    goto err_flctrl;
  }

  res = mtlk_osal_event_init(&scan->chunk_scan_complete_event);
  if (res != MTLK_ERR_OK) {
    ELOG_V("Failed to intialize sync scan notification event");
    goto err_chunk_event;
  }

  scan->last_result = mtlk_osal_mem_alloc(mtlk_get_umi_scan_size(), MTLK_MEM_TAG_SCAN_RESULT);
  if (NULL == scan->last_result) {
    ELOG_V("Failed to allocate scan last result");
    goto err_alloc_last_result;
  }

  res = _mtlk_scan_abilities_init(scan);
  if (MTLK_ERR_OK != res) {
    goto err_alloc_last_result;
  }

  scan->initialized = TRUE;
  return MTLK_ERR_OK;

err_alloc_last_result:
  mtlk_osal_event_cleanup(&scan->chunk_scan_complete_event);
err_chunk_event:
  mtlk_flctrl_unregister(scan->config.hw_tx_flctrl, scan->flctrl_id);
err_flctrl:
  mtlk_txmm_msg_cleanup(&scan->async_scan_msg);
err_async_msg:
  mtlk_osal_timer_cleanup(&scan->pause_timer);
err_timer:
  mtlk_osal_event_cleanup(&scan->completed);
err_completed_event:
  mtlk_scan_delete_vector(scan->vector);
err_create_scan_vector:
  return res;
}

void __MTLK_IFUNC
mtlk_scan_cleanup (struct mtlk_scan *scan)
{
  MTLK_ASSERT(scan->initialized);

  _mtlk_scan_abilities_cleanup(scan);

  scan_terminate_and_wait_completion(scan);

  mtlk_txmm_msg_cleanup(&scan->async_scan_msg);

  mtlk_osal_timer_cleanup(&scan->pause_timer);

  mtlk_flctrl_unregister(scan->config.hw_tx_flctrl, scan->flctrl_id);

  mtlk_osal_mem_free(scan->last_result);

  mtlk_osal_event_cleanup(&scan->chunk_scan_complete_event);

  mtlk_osal_event_cleanup(&scan->completed);

  mtlk_free_scan_vector(scan->vector);

  mtlk_scan_delete_vector(scan->vector);

  scan->initialized = FALSE;
}

static int
scan_next_chunk(struct mtlk_scan *scan)
{
  int res = MTLK_ERR_OK;

  /* We could have traffic going through us right now (in case of BG scan).
   * Suspend it. Data packets are supposed to be accumulated in the
   * queues after this. Make suspend right before we enable power save mode 
   * in MAC to transmit as big amount of data packets as possible, because
   * we want BG scan to be as smooth as possible for the packet data. 
   * This is meaningless for regular scan (in disconnected state), because
   * no data passes through, but we want to have common control path
   * for both BG and regular scan.
   */
  mtlk_flctrl_stop_data(scan->config.hw_tx_flctrl, scan->flctrl_id);

  if (scan->params.is_background) {
    /* we have to enable power save mode
     * in order to go out from the current channel
     * and don't loose any data coming to us meanwhile
     */
    res = send_ps_req(scan, TRUE);
    if(res != MTLK_ERR_OK) {
      ELOG_V("Unable to enable PS mode");
      goto err_send_ps_req;
    }
  }

  /* - for BG scan this will definitely switch progmodel,
   * - for regular - this will switch progmodel only when going to second band.
   *   Otherwise this will just reconfigure MIBs
   */
  res = load_and_configure_progmodel(scan);
  if(res != MTLK_ERR_OK) {
    ELOG_V("Unable to load and configure progmodel");
    goto err_progmodel;
  }

  res = mtlk_scan_send_scan_req(scan);
  if (res != MTLK_ERR_OK) {
    ELOG_V("Unable to send scan request");
    goto err_scan;
  }

  return res;

err_scan:
err_progmodel:
  if (scan->params.is_background)
    send_ps_req(scan, FALSE);
err_send_ps_req:
  mtlk_flctrl_start_data(scan->config.hw_tx_flctrl, scan->flctrl_id);
  return res;
}

static int 
scan_current_band (struct mtlk_scan *scan)
{
  int res;

  if (mtlk_prepare_scan_vector(HANDLE_T(mtlk_vap_get_core(scan->vap_handle)),
                               scan,
                               scan->cur_band,
                               country_code_to_domain(mtlk_core_get_country_code(mtlk_vap_get_core(scan->vap_handle)))) != MTLK_ERR_OK ||
      mtlk_scan_vector_get_used(scan->vector) == 0)
  { 
    ILOG1_S("Scan failed on %s - can't prepare scan vector",
         mtlk_eeprom_band_to_string(scan->cur_band));
    goto error; /* we still might be able to scan next band */
  }

  scan->ch_offset = 0;
  scan->last_timestamp = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  res = scan_next_chunk(scan);
  if (res != MTLK_ERR_OK) 
    /* if we are unable to start scan of first chunk -
     * complete the whole scan, because this
     * is erroneous situation
     */ 
    scan_complete(scan);

  return res;

error:
  complete_or_continue(scan);
  return MTLK_ERR_SCAN_FAILED;
}

static int 
scan_next_band (struct mtlk_scan *scan)
{
  scan->cur_band = scan->next_band;
  scan->next_band = MTLK_HW_BAND_NONE;
  return scan_current_band(scan);
}

static void
delete_progmodels(struct mtlk_scan *scan)
{
  int i;
  for(i = 0; i < MTLK_HW_BAND_BOTH; i++) {
    if (scan->progmodels[i] != NULL) {
      mtlk_progmodel_delete(scan->progmodels[i]);
      scan->progmodels[i] = NULL;
    }
  }
}

static int
complete_or_continue(struct mtlk_scan *scan)
{
  if (scan->next_band == MTLK_HW_BAND_NONE) {
    BOOL old_val;
    delete_progmodels(scan);

    old_val = mtlk_osal_atomic_xchg(&scan->is_running, FALSE);
    MTLK_ASSERT(old_val == TRUE); /* bug is somewhere */
    MTLK_UNREFERENCED_PARAM(old_val);

    if (!mtlk_vap_is_ap(scan->vap_handle)) {
      mtlk_scan_set_essid(scan, "");
    }

    mtlk_osal_event_set(&scan->completed);
    mtlk_core_notify_scan_complete(scan->vap_handle);

    _mtlk_scan_abilities_set_idle(scan);

    ILOG2_V("Scan completed");
    return MTLK_ERR_OK;
  } else
    return scan_next_band(scan);
} 

void __MTLK_IFUNC mtlk_scan_schedule_rescan (struct mtlk_scan *scan)
{
  ILOG1_V("Schedule re-scan");
  scan->rescan = TRUE;
}

static void
setup_scan_sequence(struct mtlk_scan *scan)
{
  if (scan->orig_band == MTLK_HW_BAND_BOTH) {
    scan->cur_band = MTLK_HW_BAND_5_2_GHZ;
    scan->next_band = MTLK_HW_BAND_2_4_GHZ;
  } else {
    scan->cur_band = scan->orig_band;
    scan->next_band = MTLK_HW_BAND_NONE;
  }
}

static int 
preload_progmodels(struct mtlk_scan *scan)
{
  int i;
  uint8 cur_spectrum;
  uint8 next_spectrum;

  cur_spectrum = next_spectrum = scan->spectrum;

  if (scan->params.is_background) {
    /* For working band we scan using the same progmodel (CB/nCB) that we are connected on.
     * This is requirment from MAC team.
     * We benefit from this in a way that we don't have to preload 3 progmodels 
     * for case when we're connected in CB mode and do dual-band nCB scan.
     */
    uint8 band = mtlk_pdb_get_int(mtlk_vap_get_param_db(scan->vap_handle), PARAM_DB_CORE_FREQ_BAND_CUR);
    uint8 spectrum = mtlk_pdb_get_int(mtlk_vap_get_param_db(scan->vap_handle), PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE);
    MTLK_ASSERT(band < MTLK_HW_BAND_BOTH);
 
    cur_spectrum = (scan->cur_band == band ? spectrum : scan->spectrum);   
    next_spectrum = (scan->next_band == band ? spectrum : scan->spectrum);
  }
  
  scan->progmodels[scan->cur_band] = mtlk_progmodel_create(scan->config.txmm, mtlk_vap_get_core(scan->vap_handle), scan->cur_band, cur_spectrum);
  if (!scan->progmodels[scan->cur_band])
    goto ERROR;

  if(scan->next_band != MTLK_HW_BAND_NONE) {
    scan->progmodels[scan->next_band] = mtlk_progmodel_create(scan->config.txmm, mtlk_vap_get_core(scan->vap_handle), scan->next_band, next_spectrum);
    if (!scan->progmodels[scan->next_band])
      goto ERROR;
  } 

  for (i = 0; i < MTLK_HW_BAND_BOTH; i++) {
    if (scan->progmodels[i] != NULL)
      if (mtlk_progmodel_load_from_os(scan->progmodels[i]) != MTLK_ERR_OK)
        goto ERROR;
  }

  return MTLK_ERR_OK;

ERROR:
  delete_progmodels(scan);

  return MTLK_ERR_SCAN_FAILED;
}

static int
do_scan(struct mtlk_scan *scan, uint8 band, uint8 spectrum, const char* essid, BOOL is_sync)
{
  int res = MTLK_ERR_OK;

  if (mtlk_core_is_stopping(mtlk_vap_get_core(scan->vap_handle)))
    return MTLK_ERR_SCAN_FAILED; /* prohibited */
  if (mtlk_osal_atomic_xchg(&scan->is_running, TRUE) == TRUE) 
    return MTLK_ERR_SCAN_FAILED; /* already running */

  if(scan->params.is_background) {
    ILOG0_V("Starting background scan...");
  } else {
    ILOG0_V("Starting normal scan...");
  }

  /* Clear cached scan results */
  mtlk_cache_clear(scan->config.bss_cache);

  mtlk_osal_atomic_set(&scan->treminate_scan, FALSE);
  scan->is_synchronous = is_sync;
  mtlk_osal_event_reset(&scan->completed);

  _mtlk_scan_abilities_set_active(scan);
  /*
   * Attention!!! Scan context is not thread safe.
   * Change scan context only after this watermark!
   */

  /*
   * If new pattern differs from the previous one - purge cache.
   * Otherwise we'll get union of old-pattern-matched BSS's and new ones.
   * Set active scan essid irrespective of essid pattern.
   */
  if (essid != NULL)
    mtlk_scan_set_essid(scan, essid);

  scan->spectrum = spectrum;
  scan->orig_band = band;

  MTLK_ASSERT(band < MTLK_HW_BAND_NONE);

  setup_scan_sequence(scan);

  res = preload_progmodels(scan);
  if (res != MTLK_ERR_OK) {
    ELOG_V("Unable to load progmodels");
    scan_complete(scan);
    return res;
  }

  res = scan_current_band(scan);
  if (res != MTLK_ERR_OK) {
    ELOG_V("Failed to scan current band");
    return res;
  }

  return res;
}

int __MTLK_IFUNC
mtlk_scan_async (struct mtlk_scan *scan, uint8 band, const char* essid)
{
  MTLK_ASSERT(band < MTLK_HW_BAND_NONE);

  return do_scan(scan, band, SPECTRUM_20MHZ, essid, FALSE);
}

int __MTLK_IFUNC
mtlk_scan_sync (struct mtlk_scan *scan, uint8 band, uint8 spectrum)
{
  int res;

  MTLK_ASSERT(band < MTLK_HW_BAND_BOTH);

  mtlk_osal_event_reset(&scan->chunk_scan_complete_event);

  res = do_scan(scan, band, spectrum, NULL, TRUE);
  if (res != MTLK_ERR_OK)
    goto finish;

#define _MTLK_SYNC_SCAN_WAIT_PORTION (20)
  while(MTLK_ERR_TIMEOUT == mtlk_osal_event_wait(&scan->completed,
                                                 _MTLK_SYNC_SCAN_WAIT_PORTION))
  {
    if(MTLK_ERR_OK == mtlk_osal_event_wait(&scan->chunk_scan_complete_event, 
                                           _MTLK_SYNC_SCAN_WAIT_PORTION))
    {
      mtlk_osal_event_reset(&scan->chunk_scan_complete_event);

      res = mtlk_scan_handle_evt_scan_confirmed(HANDLE_T(scan),
                                                scan->last_result,
                                                mtlk_get_umi_scan_size());
    }
  }

finish:
  return res;
}

static void
scan_remaining_channels(struct mtlk_scan *scan)
{
  ILOG2_V("Send request to scan remaining channels");
  if (scan_next_chunk(scan) != MTLK_ERR_OK) 
    scan_complete(scan);
}

static uint32
pause_timer_handler(mtlk_osal_timer_t *timer, mtlk_handle_t data)
{
  int err;
  struct mtlk_scan *scan = (struct mtlk_scan*)data;


  err = mtlk_core_schedule_internal_task(mtlk_vap_get_core(scan->vap_handle), HANDLE_T(scan),
                                         _mtlk_scan_handle_evt_pause_elapsed,
                                         NULL, 0);

  if (err != MTLK_ERR_OK) {
    ELOG_D("Can't schedule SCAN CONTINUE task (err=%d)", err);
  }

  return 0;
}

static int __MTLK_IFUNC
_mtlk_scan_handle_evt_pause_elapsed (mtlk_handle_t scan_object, const void *payload, uint32 size)
{
  struct mtlk_scan *scan = HANDLE_T_PTR(struct mtlk_scan, scan_object);

  MTLK_ASSERT(0 == size);

  MTLK_UNREFERENCED_PARAM(payload);
  MTLK_UNREFERENCED_PARAM(size);

  ILOG2_V("Handling scan pause elapsed event");

  if (TRUE == mtlk_osal_atomic_get(&scan->treminate_scan)) {
    ILOG2_V("Scan interrupted");
    goto ERR; /* terminate scan */
  }

  if (scan->rescan) {
    scan->rescan = FALSE;

   /* We need to clear BSS cache,
    * because it might contain APs that don't match our new regulatory domain constraints
    * (for ex. we have DFS channels disabled in EEPROM and BSS cache contains APs on channels with SM required).
    * Otherwise user might have faulty impression that she can safely work on those channels.
    */
    if (!scan->params.is_background)
      /* Clearing cache during background scan
       * will purge current BSS information.
       * And we might be not able to reconnect.
       */
      mtlk_cache_clear(scan->config.bss_cache);

    /* scan from scratch */
    setup_scan_sequence(scan);
    scan_current_band(scan);
  } else if (scan->ch_offset < mtlk_scan_vector_get_used(scan->vector))
    scan_remaining_channels(scan);
  else
    complete_or_continue(scan);

  return MTLK_ERR_OK;

ERR:
  scan_complete(scan);
  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
scan_complete (struct mtlk_scan *scan)
{
  scan->next_band = MTLK_HW_BAND_NONE;
  complete_or_continue(scan);
}

int __MTLK_IFUNC
mtlk_scan_handle_evt_scan_confirmed (mtlk_handle_t scan_object, const void *payload, uint32 size)
{
  int res = MTLK_ERR_OK;
  struct mtlk_scan *scan = HANDLE_T_PTR(struct mtlk_scan, scan_object);
  const UMI_SCAN_HDR *scan_data = NULL;
  BOOL error = FALSE;
  uint16 status;

  MTLK_ASSERT(size == mtlk_get_umi_scan_size());

  ILOG2_V("Handling scan confirm");

  if (TRUE == scan->is_cfm_timeout) {
    res = MTLK_ERR_TIMEOUT;
    goto ERR;
  }

  scan_data = mtlk_get_umi_scan_hdr((void*)payload);

  status = MAC_TO_HOST16(scan_data->u16Status);
  ILOG2_D("scan status from MAC: %d", status);

  if (scan->params.is_background) {
    res = switch_back_progmodel(scan);
    if(MTLK_ERR_OK != res) {
      ELOG_V("Unable to switch progmodel back");
      error = TRUE;
    }

    res = send_ps_req(scan, FALSE);
    if (MTLK_ERR_OK != res) {
      ELOG_V("Unable to disable PS");
      error = TRUE;
    }
  }

  /* We shouldn't check return value, because someone else might still
   * hold hw_tx_flctrl stopped, and data flow is not actually started after this.
   * This case definitely happens when we trigger regular scan and flow is stopped.
   * We should wake flow even in case of error, because someone might want to
   * reconnect without scan after this.
   */
  mtlk_flctrl_start_data(scan->config.hw_tx_flctrl, scan->flctrl_id);

  if (TRUE == mtlk_osal_atomic_get(&scan->treminate_scan)) {
    ILOG2_V("Scan interrupted");
    goto ERR; /* terminate scan */
  }

  if(error)
    goto ERR;

  if (status != UMI_OK) {// MAC error - stop scanning
    res = MTLK_ERR_MAC;
    goto ERR;
  }

  /* TODO: remove check after Scan moving to Master Core */
  if (mtlk_vap_is_master_ap(scan->vap_handle)) {
    mtlk_aocs_update_cl_on_scan_cfm(scan->config.aocs, (void*)payload);
  }

  if (scan->params.is_background && scan->params.pause_between_chunks)
    /* Defer scanning to user-configured time.
     * This is needed to not to affect traffic, that is currently passing throught.
     */
    mtlk_osal_timer_set(&scan->pause_timer, scan->params.pause_between_chunks);
  else
    /* call directly */
    _mtlk_scan_handle_evt_pause_elapsed(HANDLE_T(scan), NULL, 0);

  return res;

ERR:
  scan_complete(scan);
  return res;
}

void __MTLK_IFUNC
mtlk_scan_set_essid (struct mtlk_scan *scan, const char *buf)
{
  MTLK_ASSERT(NULL != scan);
  MTLK_ASSERT(!mtlk_vap_is_ap(scan->vap_handle));

  strncpy(scan->params.essid, buf, MIB_ESSID_LENGTH + 1);
}

size_t __MTLK_IFUNC
mtlk_scan_get_essid (struct mtlk_scan *scan, char *buf)
{
  MTLK_ASSERT(NULL != scan);
  MTLK_ASSERT(!mtlk_vap_is_ap(scan->vap_handle));

  strncpy(buf, scan->params.essid, MIB_ESSID_LENGTH + 1);

  return MIB_ESSID_LENGTH + 1;
}

void __MTLK_IFUNC mtlk_scan_set_per_chunk_limit(struct mtlk_scan *scan, uint8 channels_per_chunk_limit)
{
  MTLK_ASSERT(NULL != scan);
  MTLK_ASSERT(!mtlk_vap_is_ap(scan->vap_handle));

  scan->params.channels_per_chunk_limit = channels_per_chunk_limit;
}

uint8 __MTLK_IFUNC mtlk_scan_get_per_chunk_limit(struct mtlk_scan *scan)
{ 
  MTLK_ASSERT(NULL != scan);

  return scan->params.channels_per_chunk_limit;
}

void __MTLK_IFUNC mtlk_scan_set_pause_between_chunks(struct mtlk_scan *scan, uint16 pause_between_chunks)
{
  MTLK_ASSERT(NULL != scan);
  MTLK_ASSERT(!mtlk_vap_is_ap(scan->vap_handle));

  scan->params.pause_between_chunks = pause_between_chunks;
}

uint16 __MTLK_IFUNC mtlk_scan_get_pause_between_chunks(struct mtlk_scan *scan)
{
  MTLK_ASSERT(NULL != scan);

  return scan->params.pause_between_chunks;
}

void __MTLK_IFUNC
mtlk_scan_set_is_background_scan_enabled(struct mtlk_scan *scan, BOOL is_background_scan_enabled)
{
  MTLK_ASSERT(NULL != scan);
  MTLK_ASSERT(!mtlk_vap_is_ap(scan->vap_handle));

  scan->params.is_background_scan_enabled = is_background_scan_enabled;
}

BOOL __MTLK_IFUNC
mtlk_scan_is_background_scan_enabled(struct mtlk_scan *scan)
{
  MTLK_ASSERT(NULL != scan);

  return scan->params.is_background_scan_enabled;
}

void __MTLK_IFUNC
mtlk_scan_set_background(struct mtlk_scan *scan, BOOL is_background)
{
  scan->params.is_background = is_background;
}

void __MTLK_IFUNC
scan_terminate_and_wait_completion(struct mtlk_scan *scan)
{
  if(!mtlk_scan_is_running(scan))
    return;

  ILOG2_V("Terminate and Wait for Scan completion....");

  /* Terminate Scanning and wait for Scan completion.
   * The maximum waiting timeout depends on pause_between_chunks parameter*/
  mtlk_osal_atomic_set(&scan->treminate_scan, TRUE);

  mtlk_osal_event_wait(&scan->completed, MTLK_OSAL_EVENT_INFINITE);

  /* Clearing cache */
  mtlk_cache_clear(scan->config.bss_cache);

  ILOG2_V("Scan terminated");
}

void __MTLK_IFUNC
scan_terminate(struct mtlk_scan *scan)
{
  if(!mtlk_scan_is_running(scan))
    return;

  ILOG2_V("Terminate Scan....");

  /* Terminate Scanning and wait for Scan completion.
   * The maximum waiting timeout depends on pause_between_chunks parameter*/
  mtlk_osal_atomic_set(&scan->treminate_scan, TRUE);
}
