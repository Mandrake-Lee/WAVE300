
#include "mtlkinc.h"
#include "mtlk_aocs_propr.h"
#include "mtlk_channels_propr.h"
#include "mtlkflctrl.h"
#include "aocs.h"
#include "channels.h"
#include "mtlk_core_iface.h"
#include "mtlk_coreui.h"
#include "mhi_umi.h"

/* TODO: include of core.h must be dropped after moving all core dereferences to Param DB */
#include "core.h"
#include "mib_osdep.h"
#include "mtlkparams.h"

#include "mtlk_dfs.h"

#define LOG_LOCAL_GID   GID_DFS
#define LOG_LOCAL_FID   1

static const mtlk_ability_id_t _dot11h_abilities_ap[] = {
  MTLK_CORE_REQ_SET_DOT11H_AP_CFG,
  MTLK_CORE_REQ_GET_DOT11H_AP_CFG,
  MTLK_CORE_REQ_SET_DOT11H_CFG,
  MTLK_CORE_REQ_GET_DOT11H_CFG
};

static const mtlk_ability_id_t _dot11h_abilities_sta[] = {
  MTLK_CORE_REQ_SET_DOT11H_CFG,
  MTLK_CORE_REQ_GET_DOT11H_CFG
};

static int
_mtlk_dot11h_abilities_init(mtlk_dot11h_t *dfs_data)
{
  int res = MTLK_ERR_OK;
  const mtlk_ability_id_t *ab_group = NULL;
  uint32  ab_group_size;
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(dfs_data->vap_handle));

  if (mtlk_vap_is_master_ap(dfs_data->vap_handle)) {
    ab_group = _dot11h_abilities_ap;
    ab_group_size = ARRAY_SIZE(_dot11h_abilities_ap);
  } else {
    ab_group = _dot11h_abilities_sta;
    ab_group_size = ARRAY_SIZE(_dot11h_abilities_sta);
  }

  res = mtlk_abmgr_register_ability_set(mtlk_vap_get_abmgr(dfs_data->vap_handle),
                                        ab_group, ab_group_size);
  if (MTLK_ERR_OK == res) {
    mtlk_abmgr_enable_ability_set(mtlk_vap_get_abmgr(dfs_data->vap_handle),
                                  ab_group, ab_group_size);
  }

  return res;
}

static void
_mtlk_dot11h_abilities_cleanup(mtlk_dot11h_t *dfs_data)
{
  const mtlk_ability_id_t *ab_group = NULL;
  uint32  ab_group_size;
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(dfs_data->vap_handle));

  if (mtlk_vap_is_master_ap(dfs_data->vap_handle)) {
    ab_group = _dot11h_abilities_ap;
    ab_group_size = ARRAY_SIZE(_dot11h_abilities_ap);
  } else {
    ab_group = _dot11h_abilities_sta;
    ab_group_size = ARRAY_SIZE(_dot11h_abilities_sta);
  }

  mtlk_abmgr_disable_ability_set(mtlk_vap_get_abmgr(dfs_data->vap_handle),
                                 ab_group, ab_group_size);

  mtlk_abmgr_unregister_ability_set(mtlk_vap_get_abmgr(dfs_data->vap_handle),
                                    ab_group, ab_group_size);
}


/**********************************************************************
 * Local definitions
 ************************************************************************/

#define DOT11H_TIMER_INTERVAL_CFM    (10000)

#define DOT11H_TIMER_INTERVAL        (10000)

#define  DFS_EMUL_CHNL_AUTO -1

/**********************************************************************
 * Local type definitions
***********************************************************************/
typedef enum _mtlk_dot11h_status_e{
  MTLK_DOT11H_IDLE,
  MTLK_DOT11H_IN_PROCESS,
  MTLK_DOT11H_IN_AVAIL_CHECK_TIME,
  MTLK_DOT11H_ERROR,
  MTLK_DOT11H_FINISHED_OK
} mtlk_dot11h_status_e;

/**********************************************************************
 * function declaration
***********************************************************************/
static void
mtlk_fill_default_dot11h_params(mtlk_dot11h_cfg_t *params);

static BOOL __MTLK_IFUNC
#if 1
mtlk_dot11h_cfm_clb(mtlk_dot11h_t *obj);
#else
mtlk_dot11h_cfm_clb(mtlk_handle_t clb_usr_data,
                    mtlk_txmm_data_t* data,
                    mtlk_txmm_clb_reason_e reason);
#endif

static void __MTLK_IFUNC mtlk_dot11h_on_channel_switch_announcement_ind(mtlk_dot11h_t *obj);
static void __MTLK_IFUNC mtlk_dot11h_on_channel_switch_done_ind(mtlk_dot11h_t *obj);
/**********************************************************************
 * code
***********************************************************************/
mtlk_dot11h_t* __MTLK_IFUNC
mtlk_dfs_create(void)
{
  mtlk_dot11h_t    *dfs_data;

  ILOG3_V("mtlk_dfs_create");
  dfs_data =  mtlk_osal_mem_alloc(sizeof(mtlk_dot11h_t), MTLK_MEM_TAG_DFS);

  if (NULL != dfs_data) {
      memset(dfs_data, 0, sizeof(mtlk_dot11h_t));
  }

  return dfs_data;
}

void __MTLK_IFUNC mtlk_dfs_delete(mtlk_dot11h_t *dfs_data)
{
  MTLK_ASSERT(NULL != dfs_data);

  ILOG3_V("mtlk_dfs_delete");

  mtlk_osal_mem_free(dfs_data);
}

static uint32 __MTLK_IFUNC dot11h_timeout(mtlk_osal_timer_t *timer, mtlk_handle_t clb_usr_data)
{
  mtlk_dot11h_t *dot11h = (mtlk_dot11h_t*)clb_usr_data;

  MTLK_UNREFERENCED_PARAM(timer);

  /*enable data only in station*/
  if ((dot11h->data_stop) && (!mtlk_vap_is_ap(dot11h->vap_handle))) {
    ILOG4_V("dot11h_timeout - enable data \n");
    dot11h->data_stop = 0;
    mtlk_flctrl_start_data(dot11h->api.hw_tx_flctrl, dot11h->flctrl_id);
  }

  if (mtlk_vap_is_master_ap(dot11h->vap_handle)) {
    int i;
    mtlk_aocs_evt_switch_t aocs_data;

    /*set status for proc result*/
    dot11h->status = MTLK_DOT11H_ERROR;
    ILOG4_V("MTLK_DOT11H_ERROR, got timeout on 11h activation\n");
    ILOG4_D("events = %d\n",dot11h->event);
    /*update aocs module*/
    aocs_data.status = MTLK_ERR_TIMEOUT;
    for (i = 0; i < NTS_PRIORITIES; i++)
        aocs_data.sq_used[i] = mtlk_core_get_sq_size(mtlk_vap_get_core(dot11h->vap_handle), i);
    mtlk_aocs_indicate_event(dot11h->api.aocs, MTLK_AOCS_EVENT_SWITCH_DONE, (void*)&aocs_data, sizeof(aocs_data));
  }
  
  /*TODO: GS: send notification event to CF*/
  
  ILOG2_DD("channel switch with res = %d dot11h.cfg.u16Channel = %d",
      MTLK_ERR_TIMEOUT, dot11h->cfg.u16Channel);

  //indication to procfs debug param?
  //TODO

  return 0; /* do not start the timer */
}

static void dot11h_start_timer(mtlk_dot11h_t *obj)
{
  uint32 msec = DOT11H_TIMER_INTERVAL + (1000 * obj->cfg.u16ChannelAvailabilityCheckTime);
  ILOG2_D("starting timer for %d ms", msec);
  mtlk_osal_timer_set(&obj->timer, msec);
}

//stop timer
static void dot11h_stop_timer(mtlk_dot11h_t *obj)
{
  mtlk_osal_timer_cancel(&obj->timer);
}

void
mtlk_fill_default_dot11h_params(mtlk_dot11h_cfg_t *params)
{
  memset(params, 0, sizeof(*params));
  params->u8ScanType                        = SCAN_ACTIVE;
  params->u8ChannelSwitchCount              = 6; /*number of beacons wait before execute switch channel*/
  params->u8SwitchMode                      = 0x30; /*[4:7] according to MIBs, [0:3] from proc (scan type)*/
  params->u16Channel                        = 36;
  params->u16ChannelAvailabilityCheckTime   = 60;
  params->u8IsHT                            = 1;
  params->u8FrequencyBand                   = 0;

  params->debug_params.debugChannelSwitchCount           = -1;
  params->debug_params.debugChannelAvailabilityCheckTime = -1;
  params->debug_params.debugNewChannel                   = -1;
}

MTLK_INIT_STEPS_LIST_BEGIN(dfs)
  MTLK_INIT_STEPS_LIST_ENTRY(dfs, LOCK_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(dfs, FLCTRL_REG)
  MTLK_INIT_STEPS_LIST_ENTRY(dfs, TXMM_MSG_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(dfs, TIMER_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(dfs, ABILITIES_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(dfs)
MTLK_INIT_STEPS_LIST_END(dfs);

int __MTLK_IFUNC mtlk_dot11h_init(mtlk_dot11h_t *obj,
                                  const mtlk_dot11h_cfg_t      *cfg, 
                                  const mtlk_dot11h_wrap_api_t *api,
                                  mtlk_vap_handle_t            vap_handle)
{
 
  ILOG3_V("mtlk_dot11h_init");

  obj->vap_handle = vap_handle;

  MTLK_ASSERT(api         != NULL);
  MTLK_ASSERT(api->txmm   != NULL);
  MTLK_ASSERT(api->hw_tx_flctrl != NULL);

  if (mtlk_vap_is_master_ap(obj->vap_handle)) {
    MTLK_ASSERT(api->aocs   != NULL);
  }

  if (cfg) {
    obj->cfg = *cfg;
  } else {
    mtlk_fill_default_dot11h_params(&obj->cfg);
  }
  
  MTLK_INIT_TRY(dfs, MTLK_OBJ_PTR(obj))
    MTLK_INIT_STEP(dfs, LOCK_INIT, MTLK_OBJ_PTR(obj), 
                   mtlk_osal_lock_init, (&obj->lock));

    obj->api = *api;
    obj->status = MTLK_DOT11H_IDLE;
    obj->data_stop = 0;  

    MTLK_INIT_STEP(dfs, FLCTRL_REG, MTLK_OBJ_PTR(obj),
	                 mtlk_flctrl_register, (obj->api.hw_tx_flctrl, &(obj->flctrl_id)));
    ILOG4_D("mtlk_flctrl_register, flctrl_id = %lu", obj->flctrl_id);
  
    MTLK_INIT_STEP(dfs, TXMM_MSG_INIT, MTLK_OBJ_PTR(obj),
                   mtlk_txmm_msg_init, (&obj->man_msg));  
    MTLK_INIT_STEP(dfs, TIMER_INIT, MTLK_OBJ_PTR(obj),
                   mtlk_osal_timer_init, (&obj->timer, dot11h_timeout, HANDLE_T(obj)));

    MTLK_INIT_STEP(dfs, ABILITIES_INIT, MTLK_OBJ_PTR(obj),
                  _mtlk_dot11h_abilities_init, (obj));

  MTLK_INIT_FINALLY(dfs, MTLK_OBJ_PTR(obj))
  MTLK_INIT_RETURN(dfs, MTLK_OBJ_PTR(obj), mtlk_dot11h_cleanup, (obj));
  
}

#define MT_CFM_FROM_TXMM 0

#if MT_CFM_FROM_TXMM
/*confirmation from txmm*/
static BOOL __MTLK_IFUNC
mtlk_dot11h_cfm_clb(mtlk_handle_t clb_usr_data, mtlk_txmm_data_t* data, mtlk_txmm_clb_reason_e reason)
{
  mtlk_dot11h_t* obj = (mtlk_dot11h_t*)clb_usr_data;
  BOOL ret_val = TRUE;

  ILOG4_D("switch done clb, reason from txmm call: %d", reason);
#else
/*Indication instead of confirmation*/
BOOL __MTLK_IFUNC
mtlk_dot11h_cfm_clb(mtlk_dot11h_t *obj)
{
  BOOL ret_val = TRUE;
  uint32 sm_required;
#endif
  ILOG4_V("in switch done cfm clb");
  
#if MT_CFM_FROM_TXMM
   (reason != MTLK_TXMM_CLBR_CONFIRMED) {
    ILOG4_V("Error on the switch request, no cfm");
    obj->status = MTLK_DOT11H_ERROR;
    ret_val =  TRUE; /*TRUE free the message*/
    goto END;
  }
  else
    ILOG4_V("Got switch confirmation");
#endif
  
  obj->status = MTLK_DOT11H_IN_AVAIL_CHECK_TIME;
  ILOG4_V("change status to MTLK_DOT11H_IN_AVAIL_CHECK_TIME");

  sm_required = mtlk_pdb_get_int(mtlk_vap_get_param_db(obj->vap_handle), PARAM_DB_DFS_SM_REQUIRED);
  /*have to stop/start data according to SmRequirer*/
  ILOG4_DD("obj->data_stop=%d, obj->cfg.u8SmRequired=%d",
       obj->data_stop, sm_required);
  
  if ((obj->data_stop == 0) && sm_required) {
    ILOG4_V("have to stop data");
    obj->data_stop = 1;
    mtlk_flctrl_stop_data(obj->api.hw_tx_flctrl, obj->flctrl_id);
  }
  else if ((obj->data_stop == 1) && (sm_required == 0)) {
    ILOG4_V("call mtlk_flctrl_wake");
    obj->data_stop = 0;
    mtlk_flctrl_start_data(obj->api.hw_tx_flctrl, obj->flctrl_id);
  }

#if MT_CFM_FROM_TXMM
END:
#endif
  /*in case of error, disable the timer*/
  if (ret_val == FALSE) {
    ILOG4_V("stop timer");
    dot11h_stop_timer(obj);
  }

  ILOG4_V("end switch done cfm clb");
  return ret_val;
}   

static mtlk_txmm_clb_action_e __MTLK_IFUNC
channel_switch_clb(mtlk_handle_t clb_usr_data, mtlk_txmm_data_t* data, mtlk_txmm_clb_reason_e reason)
{
  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
    ELOG_D("Reason for TXMM callback is %d", reason);
    
  return MTLK_TXMM_CLBA_FREE;
} 

static void 
mtlk_dot11h_channel_switch_req(mtlk_dot11h_t *obj, FREQUENCY_ELEMENT* cs_cfg_s)
{
  mtlk_txmm_data_t* tx_data = 
    mtlk_txmm_msg_get_empty_data(&obj->man_msg, obj->api.txmm);

#ifdef MBSS_FORCE_NO_CHANNEL_SWITCH
  if (mtlk_vap_is_ap(obj->vap_handle))
    MTLK_ASSERT (FALSE);
#endif


  if (tx_data)
  {
    FREQUENCY_ELEMENT* dot11h_req = (FREQUENCY_ELEMENT*)tx_data->payload;
    int                sres;

    memset(dot11h_req, 0, sizeof(*dot11h_req));

    tx_data->id           = UM_SET_CHAN_REQ; /*UM_SET_CHAN_CFM as confirmation UMI_BSS_CHANNEL_SWITCH_DONE as indication*/
    tx_data->payload_size = sizeof(*dot11h_req);

    dot11h_req->u16Channel                       = HOST_TO_MAC16(cs_cfg_s->u16Channel);
    dot11h_req->u8SmRequired                     = cs_cfg_s->u8SmRequired;
    dot11h_req->u8ScanType                       = cs_cfg_s->u8ScanType;
    dot11h_req->u8ChannelSwitchCount             = cs_cfg_s->u8ChannelSwitchCount;
    dot11h_req->u8SwitchMode                     = cs_cfg_s->u8SwitchMode;
    dot11h_req->u16ChannelAvailabilityCheckTime  = HOST_TO_MAC16(cs_cfg_s->u16ChannelAvailabilityCheckTime);
    dot11h_req->i16CbTransmitPowerLimit          = HOST_TO_MAC16(cs_cfg_s->i16CbTransmitPowerLimit);
    dot11h_req->i16nCbTransmitPowerLimit         = HOST_TO_MAC16(cs_cfg_s->i16nCbTransmitPowerLimit);
    dot11h_req->i16AntennaGain                   = HOST_TO_MAC16(cs_cfg_s->i16AntennaGain);
    dot11h_req->u32SwitchType                    = HOST_TO_MAC32(cs_cfg_s->u32SwitchType);
/*TODO- from 11d    dot11h_req->u8SmRequired                     = HOST_TO_MAC16(cs_cfg_s->u8SmRequired);*/

    ILOG2_DDDDDD("FREQUENCY_ELEMENT sending:  channel=%d Type=%d SmRequired=%d, Count=%d Mode=%d Time=%d", 
         (int)MAC_TO_HOST16(dot11h_req->u16Channel),
         (int)dot11h_req->u8ScanType,
         (int)dot11h_req->u8SmRequired,
         (int)dot11h_req->u8ChannelSwitchCount,
         (int)dot11h_req->u8SwitchMode,
         (int)MAC_TO_HOST16(dot11h_req->u16ChannelAvailabilityCheckTime));
    ILOG2_DDDS("CBLimit=%d, nCBLimit=%d, AntennaGain=%d, switch type: %s", 
         (int)MAC_TO_HOST16(dot11h_req->i16CbTransmitPowerLimit),
         (int)MAC_TO_HOST16(dot11h_req->i16nCbTransmitPowerLimit),
         (int)MAC_TO_HOST16(dot11h_req->i16AntennaGain),
         cs_cfg_s->u32SwitchType ? "AOCS" : "non-AOCS");

    mtlk_reload_tpc_wrapper((uint8)cs_cfg_s->u16Channel, HANDLE_T(mtlk_vap_get_core(obj->vap_handle)));
#if MT_CFM_FROM_TXMM
    /*clb only on cfm. for ind use timer*/
    sres = mtlk_txmm_msg_send(&obj->man_msg, mtlk_dot11h_cfm_clb, HANDLE_T(obj), DOT11H_TIMER_INTERVAL_CFM);
#else
    sres = mtlk_txmm_msg_send(&obj->man_msg, channel_switch_clb, HANDLE_T(NULL), TXMM_DEFAULT_TIMEOUT);
#endif
    if (sres != MTLK_ERR_OK) {
      ELOG_D("Can't send switch req due to TXMM err#%d", sres);
    } else {
      mtlk_core_abilities_disable_vap_ops(obj->vap_handle);
    }
  }
  else {
    ELOG_V("Can't send switch req due to lack of MAN_MSG");
  }
}

void mtlk_dot11h_initiate_channel_switch(mtlk_dot11h_t *obj, mtlk_aocs_evt_select_t *switch_data, 
                                         BOOL is_aocs_switch)
{
  FREQUENCY_ELEMENT cs_cfg_s;
  
  ILOG2_D("mtlk_dot11h_initiate_channel_switch, status = %d",obj->status);
  mtlk_osal_lock_acquire(&obj->lock);
  
  //eliminate reactivate
  if(obj->status == MTLK_DOT11H_IN_PROCESS) {
    ELOG_V("Previous channel switch not finished yet");
    mtlk_osal_lock_release(&obj->lock);
    return;
  }
  if (obj->status == MTLK_DOT11H_IN_AVAIL_CHECK_TIME) {
    /*new radar detected while in validity time.
      disable the timer and prepare to new session.
      data handling issue.
    */
    ILOG4_V("new radar detected while in validity time, stop timer");
    dot11h_stop_timer(obj);
  }

  if (mtlk_vap_is_master_ap(obj->vap_handle)) {

    if (obj->event == MTLK_DFS_EVENT_RADAR_DETECTED) {
      /* tell AOCS that we detected a radar on current channel */
      if (MTLK_ERR_OK != mtlk_aocs_indicate_event(obj->api.aocs, MTLK_AOCS_EVENT_RADAR_DETECTED, NULL, 0))
      {
        mtlk_osal_lock_release(&obj->lock);
        return;
      }
    }

    obj->status = MTLK_DOT11H_IN_PROCESS;
    ILOG4_D("set status to %d",obj->status);

    /*if needed, stop data transmit. obj->data_stop = 1 only if recursive call:
      (new radar detected while in validity time)*/
    if ((obj->data_stop == 0) &&
       ((obj->event == MTLK_DFS_EVENT_RADAR_DETECTED) ||
        (obj->event == MTLK_DFS_EVENT_CHANGE_CHANNEL_SILENT))){
      obj->data_stop = 1;
      mtlk_flctrl_stop_data(obj->api.hw_tx_flctrl, obj->flctrl_id);
    }

    /******************* Call AOCS ***********************
     * call to select new channel or use forced channel.
     * If forced channel by the user, validate the channel
     * else, it uses the scan (11d table currently) table for new channe select.
     * If channel is not available (for illegal forced, try to select new) return error.
     */
    if (mtlk_aocs_indicate_event (obj->api.aocs, MTLK_AOCS_EVENT_SELECT_CHANNEL,
        switch_data, sizeof(*switch_data)) != MTLK_ERR_OK ) {
      obj->status = MTLK_DOT11H_ERROR;
      ELOG_V("MTLK_DOT11H_ERROR, AOCS did not find available channel");
      /* if radar or silent mode, do not start data when no channel selected */
      if ((obj->data_stop) && (obj->event == MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL)){
        obj->data_stop = 0;
        mtlk_flctrl_start_data(obj->api.hw_tx_flctrl, obj->flctrl_id);
      }
      mtlk_osal_lock_release(&obj->lock);
      return;
    } 

    /******************************************************/
    /* channel was selected by AOCS */
    cs_cfg_s.u16Channel = switch_data->channel;
    obj->cfg.u8Bonding = switch_data->bonding;

    /******************************************************/

    ILOG4_D("got channel from aocs = %d",cs_cfg_s.u16Channel);

    {
      mtlk_get_channel_data_t chnl_data;

      memset(&chnl_data, 0, sizeof(chnl_data));
      chnl_data.spectrum_mode = obj->cfg.u8SpectrumMode;
      chnl_data.bonding = obj->cfg.u8Bonding;
      chnl_data.reg_domain = country_code_to_domain(mtlk_core_get_country_code(mtlk_vap_get_core(obj->vap_handle)));
      chnl_data.is_ht = obj->cfg.u8IsHT;
  
      chnl_data.ap = mtlk_vap_is_ap(obj->vap_handle);
      chnl_data.channel = cs_cfg_s.u16Channel;

      mtlk_get_channel_data (&chnl_data, &cs_cfg_s, NULL, NULL);
    }
    
    /* TODO:
      What about 0 TxPower from table (from mtlk_get_channel_data)? at the moment ignore these channels !!
    */

    cs_cfg_s.u8SwitchMode = mtlk_get_chnl_switch_mode(obj->cfg.u8SpectrumMode,
      obj->cfg.u8Bonding, (obj->event == MTLK_DFS_EVENT_RADAR_DETECTED) ||
        (obj->event == MTLK_DFS_EVENT_CHANGE_CHANNEL_SILENT));
    
    cs_cfg_s.u8ChannelSwitchCount = 6;

    //TPower related params:
    cs_cfg_s.i16CbTransmitPowerLimit  = mtlk_calc_tx_power_lim_wrapper(HANDLE_T(mtlk_vap_get_core(obj->vap_handle)),1, (uint8)cs_cfg_s.u16Channel);
    cs_cfg_s.i16nCbTransmitPowerLimit = mtlk_calc_tx_power_lim_wrapper(HANDLE_T(mtlk_vap_get_core(obj->vap_handle)), 0, (uint8)cs_cfg_s.u16Channel);
    cs_cfg_s.i16AntennaGain           = mtlk_get_antenna_gain_wrapper(HANDLE_T(mtlk_vap_get_core(obj->vap_handle)), (uint8)cs_cfg_s.u16Channel);

    ILOG4_DDDDD("send_switch_req:  u8SmRequired=%d Type=%d Channel=%d Mode=%d CheckTime=%d",
      (int)cs_cfg_s.u8SmRequired,
      (int)cs_cfg_s.u8ScanType,
      (int)cs_cfg_s.u16Channel,
      (int)cs_cfg_s.u8SwitchMode,
      (int)cs_cfg_s.u16ChannelAvailabilityCheckTime);
    
    obj->cfg.u16Channel                        = cs_cfg_s.u16Channel;
    obj->cfg.u8ScanType                        = cs_cfg_s.u8ScanType;
    obj->cfg.u8SwitchMode                      = cs_cfg_s.u8SwitchMode;
    obj->cfg.u8ChannelSwitchCount              = cs_cfg_s.u8ChannelSwitchCount;
    obj->cfg.u16ChannelAvailabilityCheckTime   = cs_cfg_s.u16ChannelAvailabilityCheckTime;

    mtlk_pdb_set_int(mtlk_vap_get_param_db(obj->vap_handle), PARAM_DB_DFS_SM_REQUIRED, cs_cfg_s.u8SmRequired);

    /*for debug purpose - from 11h debug procs*/
    /*if obj->api.aocs->u16ChannelAvailabilityCheckTime != 0,
      no channel is ready to work yet, all have radar detected < last_clear_check_time.
      set the u16ChannelAvailabilityCheckTime to that T.O. (overun if user requested)*/
    if (mtlk_aocs_get_channel_availability_check_time(obj->api.aocs) != 0) {
      cs_cfg_s.u16ChannelAvailabilityCheckTime = mtlk_aocs_get_channel_availability_check_time(obj->api.aocs);
      obj->cfg.u16ChannelAvailabilityCheckTime = cs_cfg_s.u16ChannelAvailabilityCheckTime;
      ILOG2_D("set u16ChannelAvailabilityCheckTime from aocs, %d",obj->cfg.u16ChannelAvailabilityCheckTime);
    }
    if (obj->cfg.debug_params.debugChannelAvailabilityCheckTime != -1) {
      cs_cfg_s.u16ChannelAvailabilityCheckTime = obj->cfg.debug_params.debugChannelAvailabilityCheckTime;
      obj->cfg.u16ChannelAvailabilityCheckTime = cs_cfg_s.u16ChannelAvailabilityCheckTime;
      ILOG2_D("set debug u16ChannelAvailabilityCheckTime, %d",obj->cfg.u16ChannelAvailabilityCheckTime);
    }
    if (obj->cfg.debug_params.debugChannelSwitchCount != -1)
      cs_cfg_s.u8ChannelSwitchCount = obj->cfg.debug_params.debugChannelSwitchCount;
    if (0 == mtlk_pdb_get_int(mtlk_vap_get_param_db(obj->vap_handle), PARAM_DB_DFS_RADAR_DETECTION))
      cs_cfg_s.u8SmRequired = 0;
    if (is_aocs_switch)
      cs_cfg_s.u32SwitchType = 1;
    else
      cs_cfg_s.u32SwitchType = 0;
    /*call to send a request. Endians is treated at the called function*/
    mtlk_dot11h_channel_switch_req(obj, &cs_cfg_s);

    //start timeout (add timer) with clb mtlk_dot11h_timeout_clb():
    dot11h_start_timer(obj);
    /* call AOCS to indicate channel switch start */
    mtlk_aocs_indicate_event(obj->api.aocs, MTLK_AOCS_EVENT_SWITCH_STARTED, switch_data, sizeof(*switch_data));
 } else {
    ELOG_V("Not supported in STA");
  }
  
  mtlk_osal_lock_release(&obj->lock);
}

void __MTLK_IFUNC mtlk_dot11h_on_channel_switch_done_ind(mtlk_dot11h_t *obj)
{
  int i;
  mtlk_aocs_evt_switch_t aocs_data;

  //TODO - if got IND after the timeout ??!!
  ILOG4_D("status = %d",obj->status);
  
  if ((obj->status == MTLK_DOT11H_ERROR) && mtlk_vap_is_ap(obj->vap_handle)) {
    /*got timeout before indication?*/;
  }

  if (obj->data_stop) {
    obj->data_stop = 0;
    mtlk_flctrl_start_data(obj->api.hw_tx_flctrl, obj->flctrl_id);
  }
  else
    ILOG4_D("no need to start data, data_stop = %d",obj->data_stop);
  
  /*stop timer because switch successuly finished*/
  ILOG4_V("stop timer");
  dot11h_stop_timer(obj);
  
  if (mtlk_vap_is_master_ap(obj->vap_handle))
  {
    obj->status = MTLK_DOT11H_FINISHED_OK;

    /*TODO: GS: send notification event to CF*/
    ILOG2_DD("finished channel switch with res = %d dot11h.cfg.u16Channel = %d",
        MTLK_ERR_OK, obj->cfg.u16Channel);

    /* update AOCS on channel switch complete */
    aocs_data.status = MTLK_ERR_OK;
    for (i = 0; i < NTS_PRIORITIES; i++)
      aocs_data.sq_used[i] = mtlk_core_get_sq_size(mtlk_vap_get_core(obj->vap_handle), i);
    mtlk_aocs_indicate_event(obj->api.aocs, MTLK_AOCS_EVENT_SWITCH_DONE,
      (void*)&aocs_data, sizeof(aocs_data));
  }/*if ap*/
}

void __MTLK_IFUNC mtlk_dot11h_on_channel_switch_announcement_ind(mtlk_dot11h_t *obj)
{
  FREQUENCY_ELEMENT cs_cfg_s;
  
  //TODO - if got IND after the timeout ??!!
  ILOG4_D("status = %d",obj->status);
  
  if (!mtlk_vap_is_ap(obj->vap_handle)) {
    /*STA only*/
    if (obj->status == MTLK_DOT11H_IN_AVAIL_CHECK_TIME) {
      /*new radar detected while in validity time.
        disable the timer and prepare to new session.
        data handling issue.
      */
      ILOG2_V("new radar detected while in validity time, stop timer");
      dot11h_stop_timer(obj);
    }
    /*if needed, stop data transmit. obj->data_stop = 1 only if recursive call:
      (new radar detected while in validity time)*/
    if ((obj->data_stop == 0) &&
        (obj->event == MTLK_DFS_EVENT_CHANGE_CHANNEL_SILENT)) {
      obj->data_stop = 1;
      mtlk_flctrl_stop_data(obj->api.hw_tx_flctrl, obj->flctrl_id);
    }
    
    /*TODO !!! get the channel to switch to...
      Update params from the 11d table.
      Send channel switch request to the MAC.
      Note- only the power related params have to be update for the STA.
      (rest of params are taken from the AP packets) 
      The driver has to update u16ChannelAvailabilityCheckTime for the timer.
    */
    cs_cfg_s.u16Channel   = obj->set_channel;
    ILOG2_DDDD("channel = %d, SpectrumMode = %d, UpperLowerChannelBonding = %d, domain = 0x%x",
      cs_cfg_s.u16Channel, obj->cfg.u8SpectrumMode, obj->cfg.u8Bonding,
      country_code_to_domain(mtlk_core_get_country_code(mtlk_vap_get_core(obj->vap_handle))));

    {
      mtlk_get_channel_data_t chnl_data;

      memset(&chnl_data, 0, sizeof(chnl_data));
      chnl_data.spectrum_mode = obj->cfg.u8SpectrumMode;
      chnl_data.bonding = obj->cfg.u8Bonding;
      chnl_data.reg_domain = country_code_to_domain(mtlk_core_get_country_code(mtlk_vap_get_core(obj->vap_handle)));
      chnl_data.is_ht = obj->cfg.u8IsHT;
  
      chnl_data.ap = mtlk_vap_is_ap(obj->vap_handle);
      chnl_data.channel = obj->set_channel;

      mtlk_get_channel_data (&chnl_data, &cs_cfg_s, NULL, NULL);
    }

    cs_cfg_s.u8ChannelSwitchCount = 6;

    //TPower related params:
    cs_cfg_s.i16CbTransmitPowerLimit  = mtlk_calc_tx_power_lim_wrapper(HANDLE_T(mtlk_vap_get_core(obj->vap_handle)),1, (uint8)cs_cfg_s.u16Channel);
    cs_cfg_s.i16nCbTransmitPowerLimit = mtlk_calc_tx_power_lim_wrapper(HANDLE_T(mtlk_vap_get_core(obj->vap_handle)), 0, (uint8)cs_cfg_s.u16Channel);
    cs_cfg_s.i16AntennaGain           = mtlk_get_antenna_gain_wrapper(HANDLE_T(mtlk_vap_get_core(obj->vap_handle)), (uint8)cs_cfg_s.u16Channel);

    /*update [0:3] from proc*/
    cs_cfg_s.u8SwitchMode = 0;
    switch ((int8)(obj->cfg.u8Bonding))
    {
      case ALTERNATE_LOWER:
        cs_cfg_s.u8SwitchMode |= UMI_CHANNEL_SW_MODE_SCB;
        break;
      case ALTERNATE_UPPER:
        cs_cfg_s.u8SwitchMode |= UMI_CHANNEL_SW_MODE_SCA;
        break;
      case ALTERNATE_NONE:
      default:
        cs_cfg_s.u8SwitchMode |= UMI_CHANNEL_SW_MODE_SCN;
        break;
    }

    if (obj->event == MTLK_DFS_EVENT_CHANGE_CHANNEL_SILENT)
      cs_cfg_s.u8SwitchMode |= 1;
    else
      cs_cfg_s.u8SwitchMode |= 0;

    /*update module cfg struct*/
    obj->cfg.u16Channel                        = cs_cfg_s.u16Channel;
    obj->cfg.u8ScanType                        = cs_cfg_s.u8ScanType;
    obj->cfg.u8SwitchMode                      = cs_cfg_s.u8SwitchMode;
    obj->cfg.u8ChannelSwitchCount              = cs_cfg_s.u8ChannelSwitchCount;
    obj->cfg.u16ChannelAvailabilityCheckTime   = cs_cfg_s.u16ChannelAvailabilityCheckTime;
    if (0 == mtlk_pdb_get_int(mtlk_vap_get_param_db(obj->vap_handle), PARAM_DB_DFS_RADAR_DETECTION)) {
      mtlk_pdb_set_int(mtlk_vap_get_param_db(obj->vap_handle), PARAM_DB_DFS_SM_REQUIRED, 0);
    } else {
      mtlk_pdb_set_int(mtlk_vap_get_param_db(obj->vap_handle), PARAM_DB_DFS_SM_REQUIRED, cs_cfg_s.u8SmRequired);
    }
    /* this switch is caused by radar, not AOCS */
    cs_cfg_s.u32SwitchType = 0;
    /*call to send a request. Endians is treated at the called function*/
    mtlk_dot11h_channel_switch_req(obj, &cs_cfg_s);

    if (obj->status != MTLK_DOT11H_ERROR) {
      /*start timeout (add timer) with clb mtlk_dot11h_timeout_clb():
        If no beacon/done_ind after the T.O., can disconnect station*/
      dot11h_start_timer(obj);
    }
  }/*if !ap*/
}

void __MTLK_IFUNC mtlk_dot11h_cleanup(mtlk_dot11h_t *obj)
{
  ILOG3_V("mtlk_dot11h_cleanup");

  MTLK_CLEANUP_BEGIN(dfs, MTLK_OBJ_PTR(obj))
    MTLK_CLEANUP_STEP(dfs, ABILITIES_INIT, MTLK_OBJ_PTR(obj),
                      _mtlk_dot11h_abilities_cleanup, (obj));
    MTLK_CLEANUP_STEP(dfs, TIMER_INIT, MTLK_OBJ_PTR(obj),
                      mtlk_osal_timer_cleanup, (&obj->timer));
    MTLK_CLEANUP_STEP(dfs, TXMM_MSG_INIT, MTLK_OBJ_PTR(obj),
                      mtlk_txmm_msg_cleanup, (&obj->man_msg));
    MTLK_CLEANUP_STEP(dfs, FLCTRL_REG, MTLK_OBJ_PTR(obj),
                      mtlk_flctrl_unregister, (obj->api.hw_tx_flctrl, obj->flctrl_id));
    MTLK_CLEANUP_STEP(dfs, LOCK_INIT, MTLK_OBJ_PTR(obj),
                      mtlk_osal_lock_cleanup, (&(obj->lock)));
  MTLK_CLEANUP_END(dfs, MTLK_OBJ_PTR(obj));


}

/* Getting the channel that will be switched to in case of radar detection*/
int16 mtlk_dot11h_get_debug_next_channel(mtlk_dot11h_t *dot11h_obj)
{
  MTLK_ASSERT(NULL != dot11h_obj);

  return dot11h_obj->cfg.debug_params.debugNewChannel;
}

/* Setting the channel that will be switched to in case of radar detection*/
void mtlk_dot11h_set_debug_next_channel(mtlk_dot11h_t *dot11h_obj, int16 channel)
{
  MTLK_ASSERT(NULL != dot11h_obj);
  MTLK_ASSERT(mtlk_vap_is_master_ap(dot11h_obj->vap_handle));

  dot11h_obj->cfg.debug_params.debugNewChannel = channel;
}

int
mtlk_dot11h_debug_event (mtlk_dot11h_t *dot11h_obj, uint8 event, uint16 channel)
{
   mtlk_aocs_evt_select_t switch_data;

  if (!mtlk_pdb_get_int(mtlk_vap_get_param_db(dot11h_obj->vap_handle), PARAM_DB_DFS_RADAR_DETECTION)) {
    WLOG_V("Radar detection is disabled by user, iwpriv g11hRadarDetect = 0");
    return MTLK_ERR_NOT_IN_USE;
  }

  if (MTLK_DOT11H_IN_PROCESS == dot11h_obj->status) {
    WLOG_V("Can't switch channel - already in progress");
    return MTLK_ERR_BUSY;
  }

  ILOG0_V("Radar detected on current channel, switching...");

  dot11h_obj->cfg.u16Channel         = channel? channel : mtlk_pdb_get_int(mtlk_vap_get_param_db(dot11h_obj->vap_handle), PARAM_DB_CORE_CHANNEL_CUR);
  dot11h_obj->cfg.u8IsHT             = mtlk_pdb_get_int(mtlk_vap_get_param_db(dot11h_obj->vap_handle), PARAM_DB_CORE_IS_HT_CUR);
  dot11h_obj->cfg.u8FrequencyBand    = mtlk_pdb_get_int(mtlk_vap_get_param_db(dot11h_obj->vap_handle), PARAM_DB_CORE_FREQ_BAND_CUR);
  /* thoughout */
  dot11h_obj->cfg.u8SpectrumMode     = mtlk_pdb_get_int(mtlk_vap_get_param_db(dot11h_obj->vap_handle), PARAM_DB_CORE_SELECTED_SPECTRUM_MODE);
  dot11h_obj->cfg.u8Bonding          = mtlk_vap_get_core(dot11h_obj->vap_handle)->slow_ctx->bonding;
  dot11h_obj->event                  = event;

  /* TODO: remove check after DFS moving to Master Core */
  if (mtlk_vap_is_master_ap(dot11h_obj->vap_handle)) {
    mtlk_aocs_set_spectrum_mode(dot11h_obj->api.aocs, dot11h_obj->cfg.u8SpectrumMode);
  }

  if (channel == (uint16)DFS_EMUL_CHNL_AUTO)
    switch_data.channel = 0;
  else
    switch_data.channel = channel;
  switch_data.reason = SWR_RADAR_DETECTED;
  switch_data.criteria = CHC_USERDEF;

  mtlk_dot11h_initiate_channel_switch(dot11h_obj, &switch_data, FALSE );

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
mtlk_dot11h_status (mtlk_dot11h_t *dot11h_obj, char *buffer, uint32 size)
{
  static const char not_started[] = "Not started";
  static const char in_process[] = "In Process";
  static const char in_avail_check_time[] = "In Availability check time";
  static const char stop_with_error[] = "Stop with error";
  static const char finished_ok[] = "Finished OK";
  const char *ret_str = NULL;

  MTLK_ASSERT(NULL != buffer);
  MTLK_ASSERT(0 != size);
  MTLK_ASSERT(size >= sizeof(not_started));
  MTLK_ASSERT(size >= sizeof(in_process));
  MTLK_ASSERT(size >= sizeof(in_avail_check_time));
  MTLK_ASSERT(size >= sizeof(stop_with_error));
  MTLK_ASSERT(size >= sizeof(finished_ok));
  MTLK_UNREFERENCED_PARAM(size);

  buffer[0] = 0;
  switch (dot11h_obj->status) {
    case MTLK_DOT11H_IDLE:
      ret_str =  not_started;
      break;
    case MTLK_DOT11H_IN_PROCESS:
      ret_str =  in_process;
      break;
    case MTLK_DOT11H_IN_AVAIL_CHECK_TIME:
      ret_str =  in_avail_check_time;
      break;
    case MTLK_DOT11H_ERROR:
      ret_str =  stop_with_error;
      break;
    case MTLK_DOT11H_FINISHED_OK:
      ret_str =  finished_ok;
      break;
    default:
      MTLK_ASSERT(0);
  }
  if (ret_str) {
    strcpy(buffer, ret_str);
  }
}

BOOL __MTLK_IFUNC mtlk_dot11h_is_data_stop(mtlk_dot11h_t *obj)
{
  return obj->data_stop;
}

int
mtlk_dot11h_handle_radar_ind(mtlk_handle_t dot11h, const void *payload, uint32 size)
{
  mtlk_aocs_evt_select_t switch_data;
  uint16 bss_status;
  mtlk_dot11h_t * dot11h_obj = HANDLE_T_PTR(mtlk_dot11h_t, dot11h);
  const UMI_NETWORK_EVENT *network_event = (const UMI_NETWORK_EVENT *)payload;

  bss_status = MAC_TO_HOST16(network_event->u16BSSstatus);
  ILOG0_D("got UMI_BSS_RADAR_%x indication", bss_status);

  if (!mtlk_pdb_get_int(mtlk_vap_get_param_db(dot11h_obj->vap_handle), PARAM_DB_DFS_RADAR_DETECTION)) {
    ELOG_V("Radar detection is disabled by user, iwpriv g11hRadarDetect = 0");
    return MTLK_ERR_NOT_IN_USE;
  }

  if (MTLK_DOT11H_IN_PROCESS == dot11h_obj->status) {
    ELOG_V("Previous channel switch not finished yet");
    return MTLK_ERR_BUSY;
  }

  dot11h_obj->cfg.u16Channel         =  mtlk_pdb_get_int(mtlk_vap_get_param_db(dot11h_obj->vap_handle), PARAM_DB_CORE_CHANNEL_CUR);

  dot11h_obj->cfg.u8IsHT = mtlk_pdb_get_int(mtlk_vap_get_param_db(dot11h_obj->vap_handle), PARAM_DB_CORE_IS_HT_CUR);
  dot11h_obj->cfg.u8FrequencyBand = mtlk_pdb_get_int(mtlk_vap_get_param_db(dot11h_obj->vap_handle), PARAM_DB_CORE_FREQ_BAND_CUR);
  dot11h_obj->cfg.u8SpectrumMode = mtlk_pdb_get_int(mtlk_vap_get_param_db(dot11h_obj->vap_handle), PARAM_DB_CORE_SELECTED_SPECTRUM_MODE);
  dot11h_obj->cfg.u8Bonding = mtlk_vap_get_core(dot11h_obj->vap_handle)->slow_ctx->bonding;
  dot11h_obj->event = MTLK_DFS_EVENT_RADAR_DETECTED;

  switch_data.channel = 0;
  switch_data.reason = SWR_RADAR_DETECTED;
  switch_data.criteria = SWR_UNKNOWN;

  if (dot11h_obj->cfg.debug_params.debugNewChannel != -1) {
    switch_data.channel = dot11h_obj->cfg.debug_params.debugNewChannel;
    switch_data.criteria = CHC_USERDEF;
    ILOG0_D("The next channel explicitely set to %d by user", switch_data.channel);
  }

  mtlk_dot11h_initiate_channel_switch(dot11h_obj, &switch_data, FALSE);

  return MTLK_ERR_OK;
}

int
mtlk_dot11h_handle_channel_switch_ind(mtlk_handle_t dot11h, const void *payload, uint32 size)
{
  uint16 bss_status, new_channel;
  mtlk_dot11h_t * dot11h_obj = HANDLE_T_PTR(mtlk_dot11h_t, dot11h);
  const UMI_NETWORK_EVENT *network_event = (const UMI_NETWORK_EVENT *)payload;


  bss_status = MAC_TO_HOST16(network_event->u16BSSstatus);

  MTLK_ASSERT(network_event->u8Reason == FM_STATUSCODE_11H);
  new_channel = MAC_TO_HOST16(network_event->u16PrimaryChannel);
  ILOG1_DD("got UMI_BSS_CHANNEL_SWITCH_%x IND (announce), channel = %d",
        bss_status, new_channel);

  switch (network_event->u8SecondaryChannelOffset)
  {
    case UMI_CHANNEL_SW_MODE_SCB:
      dot11h_obj->cfg.u8Bonding = ALTERNATE_LOWER;
      dot11h_obj->cfg.u8SpectrumMode = SPECTRUM_40MHZ;
      break;
    case UMI_CHANNEL_SW_MODE_SCA:
      dot11h_obj->cfg.u8Bonding = ALTERNATE_UPPER;
      dot11h_obj->cfg.u8SpectrumMode = SPECTRUM_40MHZ;
      break;
    case UMI_CHANNEL_SW_MODE_SCN:
    default:
      dot11h_obj->cfg.u8Bonding = ALTERNATE_NONE;
      dot11h_obj->cfg.u8SpectrumMode = SPECTRUM_20MHZ;
      break;
  }

  if(UMI_BSS_CHANNEL_SWITCH_NORMAL == bss_status)
    dot11h_obj->event = MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL;
  else
    dot11h_obj->event = MTLK_DFS_EVENT_CHANGE_CHANNEL_SILENT;

  dot11h_obj->set_channel = new_channel;

  mtlk_dot11h_on_channel_switch_announcement_ind(dot11h_obj);

  return MTLK_ERR_OK;
};


int
mtlk_dot11h_handle_channel_switch_done(mtlk_handle_t dot11h, const void *payload, uint32 size)
{
  mtlk_dot11h_t * dot11h_obj = HANDLE_T_PTR(mtlk_dot11h_t, dot11h);
  const UMI_NETWORK_EVENT *network_event = (const UMI_NETWORK_EVENT *)payload;
  uint16 primary_channel;
  uint16 switch_reason;

  MTLK_ASSERT(!mtlk_vap_is_slave_ap(dot11h_obj->vap_handle));
  MTLK_ASSERT(network_event->u8Reason == FM_STATUSCODE_11H);

  primary_channel   = MAC_TO_HOST16(network_event->u16PrimaryChannel);

  mtlk_pdb_set_int(mtlk_vap_get_param_db(dot11h_obj->vap_handle), PARAM_DB_CORE_CHANNEL_CUR, primary_channel);

  ILOG1_DD("Switched to channel %d [offs=%d] done", primary_channel, network_event->u8SecondaryChannelOffset);
  mtlk_dot11h_on_channel_switch_done_ind(dot11h_obj);

  if(mtlk_vap_is_master_ap(dot11h_obj->vap_handle))
  {
    switch_reason = mtlk_aocs_get_last_switch_reason(dot11h_obj->api.aocs);
  }
  else
  {/* STA only*/
    switch_reason = SWR_AP_SWITCHED;
  }

  mtlk_core_on_channel_switch_done(dot11h_obj->vap_handle, primary_channel, network_event->u8SecondaryChannelOffset, switch_reason);
  return MTLK_ERR_OK;
}

int
mtlk_dot11h_handle_channel_pre_switch_done(mtlk_handle_t dot11h, const void *payload, uint32 size)
{
  mtlk_dot11h_t * dot11h_obj = HANDLE_T_PTR(mtlk_dot11h_t, dot11h);

  MTLK_UNREFERENCED_PARAM(payload);
  MTLK_UNREFERENCED_PARAM(size);

  ILOG1_V("Channel pre-switch done indication received");
  mtlk_dot11h_cfm_clb(dot11h_obj);

  return MTLK_ERR_OK;
}

void mtlk_dot11h_set_event(mtlk_dot11h_t *obj, mtlk_dfs_event_e event)
{
  obj->event = event;
}


void mtlk_dot11h_set_spectrum_mode(mtlk_dot11h_t *obj, uint8 spectrum_mode)
{
  obj->cfg.u8SpectrumMode = spectrum_mode;
}

void mtlk_dot11h_set_dbg_channel_availability_check_time(
    mtlk_dot11h_t *obj,
    uint8 channel_availability_check_time)
{
  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(mtlk_vap_is_master_ap(obj->vap_handle));

  obj->cfg.debug_params.debugChannelAvailabilityCheckTime = channel_availability_check_time;
}

void mtlk_dot11h_set_dbg_channel_switch_count(
    mtlk_dot11h_t *obj,
    uint8 channel_switch_count)
{
  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(mtlk_vap_is_master_ap(obj->vap_handle));

  obj->cfg.debug_params.debugChannelSwitchCount = channel_switch_count;
}

int16  mtlk_dot11h_get_dbg_channel_availability_check_time(mtlk_dot11h_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  return obj->cfg.debug_params.debugChannelAvailabilityCheckTime;
}

int8 mtlk_dot11h_get_dbg_channel_switch_count(mtlk_dot11h_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  return obj->cfg.debug_params.debugChannelSwitchCount;
}

BOOL __MTLK_IFUNC mtlk_dot11h_can_switch_now(mtlk_dot11h_t *obj)
{
  if ((obj->status == MTLK_DOT11H_IDLE) || (obj->status == MTLK_DOT11H_FINISHED_OK))
    return TRUE;
  ILOG4_D("status %u", obj->status);
  return FALSE;
}
