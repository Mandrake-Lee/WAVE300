
#include "mtlkinc.h"
#include "addba.h"
#include "mtlkqos.h"
#include "mtlkaux.h"
#include "mtlk_core_iface.h"
#include "mtlk_coreui.h"

#define LOG_LOCAL_GID   GID_ADDBA
#define LOG_LOCAL_FID   1

/**********************************************************************
 * Local definitions
 ************************************************************************/

static const uint32 _mtlk_addba_peer_cntr_wss_id_map[] = 
{
    MTLK_WWSS_WLAN_STAT_ID_AGGR_ACTIVE,                   /* MTLK_ADDBAPI_CNT_AGGR_ACTIVE */
    MTLK_WWSS_WLAN_STAT_ID_REORD_ACTIVE,                  /* MTLK_ADDBAPI_CNT_REORD_ACTIVE */
    MTLK_WWSS_WLAN_STAT_ID_ADDBA_REQUEST_TX,              /* MTLK_ADDBAPI_CNT_ADDBA_REQUEST_TX */
    MTLK_WWSS_WLAN_STAT_ID_ADDBA_POSITIVE_RESPONSE_TX,    /* MTLK_ADDBAPI_CNT_ADDBA_POSITIVE_RESPONSE_TX */
    MTLK_WWSS_WLAN_STAT_ID_ADDBA_NEGATIVE_RESPONSE_TX,    /* MTLK_ADDBAPI_CNT_ADDBA_NEGATIVE_RESPONSE_TX */
    MTLK_WWSS_WLAN_STAT_ID_ADDBA_REQUEST_RX,              /* MTLK_ADDBAPI_CNT_ADDBA_REQUEST_RX */
    MTLK_WWSS_WLAN_STAT_ID_ADDBA_POSITIVE_RESPONSE_RX,    /* MTLK_ADDBAPI_CNT_ADDBA_POSITIVE_RESPONSE_RX */
    MTLK_WWSS_WLAN_STAT_ID_ADDBA_NEGATIVE_RESPONSE_RX,    /* MTLK_ADDBAPI_CNT_ADDBA_NEGATIVE_RESPONSE_RX */
    MTLK_WWSS_WLAN_STAT_ID_ADDBA_UNCONFIRMED_RESPONSE_RX, /* MTLK_ADDBAPI_CNT_ADDBA_UNCONFIRMED_RESPONSE_RX */
    MTLK_WWSS_WLAN_STAT_ID_DELBA_REQUEST_TX,              /* MTLK_ADDBAPI_CNT_DELBA_REQUEST_TX */
    MTLK_WWSS_WLAN_STAT_ID_DELBA_SENT_REQUEST_RX,         /* MTLK_ADDBAPI_CNT_DELBA_SENT_REQUEST_RX */
    MTLK_WWSS_WLAN_STAT_ID_DELBA_RECEIVED_REQUEST_RX,     /* MTLK_ADDBAPI_CNT_DELBA_RECEIVED_REQUEST_RX */
    MTLK_WWSS_WLAN_STAT_ID_AGGR_OPEN_REQUEST,             /* MTLK_ADDBAPI_CNT_AGGR_OPEN_REQUEST */
    MTLK_WWSS_WLAN_STAT_ID_AGGR_OPEN_CONFIRM,             /* MTLK_ADDBAPI_CNT_AGGR_OPEN_CONFIRM */
    MTLK_WWSS_WLAN_STAT_ID_AGGR_CLOSE_REQUEST,            /* MTLK_ADDBAPI_CNT_AGGR_CLOSE_REQUEST */
    MTLK_WWSS_WLAN_STAT_ID_AGGR_CLOSE_CONFIRM,            /* MTLK_ADDBAPI_CNT_AGGR_CLOSE_CONFIRM */
};

#define ADDBA_TU_TO_MS(nof_tu) ((nof_tu) * MTLK_ADDBA_BA_TIMEOUT_UNIT_US / 1000) /* 1TU = 1024 us */

#define INVALID_TAG_ID ((uint8)(-1))

static __INLINE int
_mtlk_addba_is_allowed_rate (uint16 rate) 
{
  int res = 1;
  if (rate != MTLK_ADDBA_RATE_ADAPTIVE &&
      !mtlk_aux_is_11n_rate((uint8)rate)) {
    res = 0;
  }

  return res;
}

static __INLINE void
_mtlk_addba_correct_res_win_size (uint8 *win_size)
{
  uint8 res = *win_size;

  if (!res)
  {
    res = MTLK_ADDBA_MAX_REORD_WIN_SIZE;
  }

  if (*win_size != res)
  {
    ILOG2_DD("WinSize correction: %d=>%d", (int)*win_size, (int)res);
    *win_size = res;
  }
}

#define _mtlk_addba_correct_res_timeout(t) /* VOID, We accept any timeout */

static __INLINE BOOL
_mtlk_addba_is_aggr_on (mtlk_addba_peer_tx_state_e state)
{
  return (state == MTLK_ADDBA_TX_AGGR_OPENED ||
          state == MTLK_ADDBA_TX_DEL_AGGR_REQ_SENT);
}

static __INLINE BOOL
_mtlk_addba_is_reord_on (mtlk_addba_peer_rx_state_e state)
{
  return (state == MTLK_ADDBA_RX_REORD_IN_PROCESS ||
          state == MTLK_ADDBA_RX_DELBA_REQ_SENT);
}

static __INLINE BOOL
_mtlk_addba_peer_reset_tx_state (mtlk_addba_peer_t *peer,
                                 uint16             tid_idx)
{
  BOOL res = FALSE;

  if (peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_NONE)
  {
    mtlk_reflim_unref(peer->addba->aggr_reflim);
    mtlk_wss_cntr_dec(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_AGGR_ACTIVE]);
    res = _mtlk_addba_is_aggr_on(peer->tid[tid_idx].tx.state);
    peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_NONE;
    peer->tid[tid_idx].tx.addba_req_dlgt = INVALID_TAG_ID;
  }

  return res;
}

static __INLINE BOOL
_mtlk_addba_peer_reset_rx_state (mtlk_addba_peer_t *peer,
                                 uint16             tid_idx)
{
  BOOL res = FALSE;

  if (peer->tid[tid_idx].rx.state != MTLK_ADDBA_RX_NONE)
  {
    if (peer->tid[tid_idx].rx.state != MTLK_ADDBA_RX_ADDBA_NEGATIVE_RES_SENT)
    {
      mtlk_reflim_unref(peer->addba->reord_reflim);
      mtlk_wss_cntr_dec(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_REORD_ACTIVE]);
    }
    res = _mtlk_addba_is_reord_on(peer->tid[tid_idx].rx.state);
    peer->tid[tid_idx].rx.state = MTLK_ADDBA_RX_NONE;
  }

  return res;
}



/******************************************************************************************************
 * CFM callbacks for TXMM
*******************************************************************************************************/
static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_peer_on_delba_req_tx_cfm_clb (mtlk_handle_t          clb_usr_data,
                                          mtlk_txmm_data_t      *data,
                                          mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_peer_t*  peer      = (mtlk_addba_peer_t *)clb_usr_data;
  UMI_DELBA_REQ_SEND* delba_req = (UMI_DELBA_REQ_SEND*)data->payload;
  uint16              tid_idx   = MAC_TO_HOST16(delba_req->u16AccessProtocol);
  uint16              status    = MAC_TO_HOST16(delba_req->u16Status);
  BOOL                stop      = FALSE;

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG_YD("RX %Y TID=%d DELBA isn't confirmed",
            delba_req->sDA.au8Addr, (int)tid_idx);
  }
  else if (status != UMI_OK)
  {
    ELOG_YDD("RX %Y TID=%d DELBA failed in FW (%u)",
      delba_req->sDA.au8Addr, (int)tid_idx, status);
  }
  else
  {
    ILOG2_YD("RX %Y TID=%d DELBA sent",
         delba_req->sDA.au8Addr, (int)tid_idx);
  }

  mtlk_osal_lock_acquire(&peer->addba->lock);
  stop = _mtlk_addba_peer_reset_rx_state(peer, tid_idx);
  mtlk_osal_lock_release(&peer->addba->lock);

  if (stop)
    peer->api.on_stop_reordering(peer->api.usr_data, tid_idx);

  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_peer_on_close_aggr_cfm_clb (mtlk_handle_t          clb_usr_data,
                                        mtlk_txmm_data_t      *data,
                                        mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_peer_t  *peer           = (mtlk_addba_peer_t*)clb_usr_data;
  UMI_CLOSE_AGGR_REQ *close_aggr_req = (UMI_CLOSE_AGGR_REQ*)data->payload;
  uint16              tid_idx        = MAC_TO_HOST16(close_aggr_req->u16AccessProtocol);
  uint16              status         = MAC_TO_HOST16(close_aggr_req->u16Status);
  BOOL                stop           = FALSE;
  int                 res            = MTLK_ERR_UNKNOWN;

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG_YD("TX %Y TID=%d aggregation closing isn't confirmed",
          close_aggr_req->sDA.au8Addr, tid_idx);
  }
  else if (status != UMI_OK)
  {
    ELOG_YDD("TX %Y TID=%d aggregation closing failed in FW (%u)",
      close_aggr_req->sDA.au8Addr, tid_idx, status);
  }
  else
  {
    ILOG2_YD("TX %Y TID=%d aggregation closed",
         close_aggr_req->sDA.au8Addr, tid_idx);
    res = MTLK_ERR_OK;
  }

  mtlk_osal_lock_acquire(&peer->addba->lock);
  stop = _mtlk_addba_peer_reset_tx_state(peer, tid_idx);
  if (MTLK_ERR_OK == res) {
    mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_AGGR_CLOSE_CONFIRM]);
  }
  mtlk_osal_lock_release(&peer->addba->lock);

  if (stop)
  {
    peer->api.on_stop_aggregation(peer->api.usr_data, tid_idx);
  }

  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_peer_on_open_aggr_cfm_clb (mtlk_handle_t          clb_usr_data,
                                       mtlk_txmm_data_t      *data,
                                       mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_peer_t *peer         = (mtlk_addba_peer_t*)clb_usr_data;
  UMI_OPEN_AGGR_REQ *add_aggr_req = (UMI_OPEN_AGGR_REQ*)data->payload;
  uint16             tid_idx      = MAC_TO_HOST16(add_aggr_req->u16AccessProtocol);
  uint16             status       = MAC_TO_HOST16(add_aggr_req->u16Status);
  int                res          = MTLK_ERR_UNKNOWN;

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG_YD("TX %Y TID=%d aggregation opening isn't confirmed",
          add_aggr_req->sDA.au8Addr, tid_idx);
  }
  else if (status != UMI_OK)
  {
    ELOG_YDD("TX %Y TID=%d aggregation opening failed in FW (%u)",
          add_aggr_req->sDA.au8Addr, tid_idx, status);
  }
  else
  {
    ILOG2_YD("TX %Y TID=%d aggregation opened", add_aggr_req->sDA.au8Addr, tid_idx);
    res = MTLK_ERR_OK;
  }

  mtlk_osal_lock_acquire(&peer->addba->lock);
  MTLK_ASSERT(peer->tid[tid_idx].tx.state == MTLK_ADDBA_TX_ADD_AGGR_REQ_SENT);
  peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_AGGR_OPENED;
  if (MTLK_ERR_OK == res) {
    mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_AGGR_OPEN_CONFIRM]);
  }
  mtlk_osal_lock_release(&peer->addba->lock);

  peer->api.on_start_aggregation(peer->api.usr_data, tid_idx);

  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_peer_on_addba_req_tx_cfm_clb (mtlk_handle_t          clb_usr_data,
                                          mtlk_txmm_data_t      *data,
                                          mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_peer_t *peer       = (mtlk_addba_peer_t*)clb_usr_data;
  UMI_ADDBA_REQ_SEND* addba_req = (UMI_ADDBA_REQ_SEND*)data->payload;
  uint16              tid_idx   = MAC_TO_HOST16(addba_req->u16AccessProtocol);
  uint16              status    = MAC_TO_HOST16(addba_req->u16Status);

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG_YDD("TX %Y TID=%d TAG=%d request isn't confirmed",
          addba_req->sDA.au8Addr, tid_idx, addba_req->u8DialogToken);
  }
  else if (status != UMI_OK)
  {
    ELOG_YDDD("TX %Y TID=%d TAG=%d request failed in FW (%u)",
      addba_req->sDA.au8Addr, tid_idx, addba_req->u8DialogToken, status);
  }
  else
  {
    ILOG2_YDD("TX %Y TID=%d TAG=%d request sent",
         addba_req->sDA.au8Addr, tid_idx, addba_req->u8DialogToken);
  }

  mtlk_osal_lock_acquire(&peer->addba->lock);
  peer->tid[tid_idx].tx.state               = MTLK_ADDBA_TX_ADDBA_REQ_CFMD;
  peer->tid[tid_idx].tx.addba_req_cfmd_time = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  mtlk_osal_lock_release(&peer->addba->lock);

  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_peer_on_addba_res_tx_cfm_clb (mtlk_handle_t          clb_usr_data,
                                          mtlk_txmm_data_t*      data,
                                          mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_peer_t  *peer      = (mtlk_addba_peer_t*)clb_usr_data;
  UMI_ADDBA_RES_SEND *addba_res = (UMI_ADDBA_RES_SEND*)data->payload;
  uint16              tid_idx   = MAC_TO_HOST16(addba_res->u16AccessProtocol);
  uint16              status    = MAC_TO_HOST16(addba_res->u16Status);
  int                 res       = MTLK_ERR_OK;

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG_YDD("RX %Y TID=%d TAG=%d response isn't confirmed",
          addba_res->sDA.au8Addr, tid_idx, addba_res->u8DialogToken);
    res = MTLK_ERR_UNKNOWN;
  }
  else if (status != UMI_OK)
  {
    ELOG_YDDD("RX %Y TID=%d TAG=%d response failed in FW (%u)",
      addba_res->sDA.au8Addr, tid_idx, addba_res->u8DialogToken, status);
    res = MTLK_ERR_UNKNOWN;
  }
  else
  {
    ILOG2_YDD("RX %Y TID=%d TAG=%d response sent",
         addba_res->sDA.au8Addr, tid_idx, addba_res->u8DialogToken);
  }

  mtlk_osal_lock_acquire(&peer->addba->lock);
  if (MTLK_ERR_OK != res) {
    mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_ADDBA_UNCONFIRMED_RESPONSE_RX]);
  }
  
  if (peer->tid[tid_idx].rx.state == MTLK_ADDBA_RX_ADDBA_POSITIVE_RES_SENT) {
    peer->tid[tid_idx].rx.state = MTLK_ADDBA_RX_REORD_IN_PROCESS;
  }
  else {
    MTLK_ASSERT(peer->tid[tid_idx].rx.state == MTLK_ADDBA_RX_ADDBA_NEGATIVE_RES_SENT);
    _mtlk_addba_peer_reset_rx_state(peer, tid_idx);
  }

  mtlk_osal_lock_release(&peer->addba->lock);

  return MTLK_TXMM_CLBA_FREE;
}
/******************************************************************************************************/

static void
_mtlk_addba_peer_on_addba_req_sent (mtlk_addba_peer_t *peer,
                                    uint16             tid_idx)
{
  peer->tid[tid_idx].tx.state          = MTLK_ADDBA_TX_ADDBA_REQ_SENT;
  peer->tid[tid_idx].tx.addba_req_dlgt = peer->addba->next_dlg_token;

  if (++peer->addba->next_dlg_token == INVALID_TAG_ID) {
  	++peer->addba->next_dlg_token;
  }

  mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_AGGR_ACTIVE]);
}

static void
_mtlk_addba_peer_fill_addba_req (mtlk_addba_peer_t  *peer,
                                 uint16              tid_idx,
                                 mtlk_txmm_data_t   *tx_data)
{
  UMI_ADDBA_REQ_SEND *addba_req = (UMI_ADDBA_REQ_SEND *)tx_data->payload;

  memset(addba_req, 0, sizeof(*addba_req));

  tx_data->id           = UM_MAN_ADDBA_REQ_TX_REQ;
  tx_data->payload_size = sizeof(*addba_req);

  addba_req->sDA               = peer->addr;
  addba_req->u8DialogToken     = peer->addba->next_dlg_token;
  addba_req->u16AccessProtocol = HOST_TO_MAC16(tid_idx);
  addba_req->u8BA_WinSize_O    = peer->addba->cfg.tid[tid_idx].aggr_win_size;
  addba_req->u16BATimeout      = HOST_TO_MAC16(peer->addba->cfg.tid[tid_idx].addba_timeout);
  addba_req->u16Status         = HOST_TO_MAC16(UMI_OK);

  ILOG2_YDDDD("TX %Y TID=%d TAG=%d request WSIZE=%d TM=%d",
       &peer->addr,
       (int)tid_idx,
       (int)peer->addba->next_dlg_token,
       (int)peer->addba->cfg.tid[tid_idx].aggr_win_size,
       (int)peer->addba->cfg.tid[tid_idx].addba_timeout);
}

static void
_mtlk_addba_peer_tx_addba_req (mtlk_addba_peer_t *peer,
                               uint16             tid_idx)
{
  if (peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_NONE)
  {
    ILOG2_YD("TX %Y TID=%d: duplicate ADDBA request? - ignored", &peer->addr, tid_idx);
    return;
  }

  if (mtlk_reflim_try_ref(peer->addba->aggr_reflim))
  { /* format and send ADDBA request */
    int               res     = MTLK_ERR_UNKNOWN;
    mtlk_txmm_data_t* tx_data = 
      mtlk_txmm_msg_get_empty_data(&peer->tid[tid_idx].tx.man_msg,
                                   peer->addba->txmm);
    if (tx_data)
    {
      _mtlk_addba_peer_fill_addba_req(peer, tid_idx, tx_data);

      res = mtlk_txmm_msg_send(&peer->tid[tid_idx].tx.man_msg, _mtlk_addba_peer_on_addba_req_tx_cfm_clb,
                                HANDLE_T(peer), 0);

      if (res == MTLK_ERR_OK)
      {
        _mtlk_addba_peer_on_addba_req_sent(peer, tid_idx);
        mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_ADDBA_REQUEST_TX]);
      }
      else
      {
        ELOG_D("Can't send ADDBA req due to TXMM err#%d", res);
      }
    }
    else
    {
      ELOG_V("Can't send ADDBA req due to lack of MAN_MSG");
    }

    if (res != MTLK_ERR_OK) {
      mtlk_reflim_unref(peer->addba->aggr_reflim);
    }
  }
  else
  {
    WLOG_DD("TX: ADDBA won't be sent (aggregations limit reached: %d >= %d)",
            mtlk_reflim_get_cur(peer->addba->aggr_reflim), mtlk_reflim_get_max(peer->addba->aggr_reflim));
  }
}

static void 
_mtlk_addba_peer_close_aggr_req (mtlk_addba_peer_t *peer, 
                                 uint16             tid_idx)
{
  mtlk_txmm_data_t* tx_data;
  UMI_CLOSE_AGGR_REQ* close_aggr_req;
  int state = peer->tid[tid_idx].tx.state;
  int sres;

  if (state != MTLK_ADDBA_TX_ADD_AGGR_REQ_SENT &&
      state != MTLK_ADDBA_TX_AGGR_OPENED)
  {
    WLOG_YD("TX %Y TID=%d: trying to close not opened aggregation", &peer->addr, tid_idx);
    return;
  }

  tx_data = mtlk_txmm_msg_get_empty_data(&peer->tid[tid_idx].tx.man_msg,
                                         peer->addba->txmm);

  if (!tx_data)
  {
    ELOG_V("Can't close Aggr due to lack of MAN_MSG");
    return;
  }

  tx_data->id           = UM_MAN_CLOSE_AGGR_REQ;
  tx_data->payload_size = sizeof(*close_aggr_req);

  close_aggr_req = (UMI_CLOSE_AGGR_REQ*)tx_data->payload;
  close_aggr_req->sDA               = peer->addr;
  close_aggr_req->u16AccessProtocol = HOST_TO_MAC16(tid_idx);
  close_aggr_req->u16Status         = HOST_TO_MAC16(UMI_OK);

  ILOG2_YD("TX %Y TID=%d closing aggregation", &peer->addr, (int)tid_idx);

  sres = mtlk_txmm_msg_send(&peer->tid[tid_idx].tx.man_msg, _mtlk_addba_peer_on_close_aggr_cfm_clb,
                            HANDLE_T(peer), 0);
  if (sres == MTLK_ERR_OK) {
    peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_DEL_AGGR_REQ_SENT;
    mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_AGGR_CLOSE_REQUEST]);
  }
  else
  {
    ELOG_D("Can't close Aggr due to TXMM err#%d", sres);
  }
}

static void
_mtlk_addba_peer_send_rx_delba_req (mtlk_addba_peer_t *peer,
                                    uint16             tid_idx)
{
  mtlk_txmm_msg_t  *tx_msg = &peer->tid[tid_idx].rx.man_msg;
  mtlk_txmm_data_t *tx_data;

  tx_data = mtlk_txmm_msg_get_empty_data(tx_msg, peer->addba->txmm);
  if (tx_data)
  {
    UMI_DELBA_REQ_SEND *delba_req = (UMI_DELBA_REQ_SEND*)tx_data->payload;
    int                 sres;

    tx_data->id           = UM_MAN_DELBA_REQ;
    tx_data->payload_size = sizeof(*delba_req);

    delba_req->sDA               = peer->addr;
    delba_req->u16AccessProtocol = HOST_TO_MAC16(tid_idx);
    delba_req->u16ResonCode      = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_SUCCESS);
    delba_req->u16Intiator       = HOST_TO_MAC16(0);
    delba_req->u16Status         = HOST_TO_MAC16(UMI_OK);

    ILOG2_YD("RX %Y TID=%d send DELBA", &peer->addr, tid_idx);

    sres = mtlk_txmm_msg_send(tx_msg, _mtlk_addba_peer_on_delba_req_tx_cfm_clb,
                              HANDLE_T(peer), 0);
    if (sres != MTLK_ERR_OK)
    {
      ELOG_D("Can't send DELBA req due to TXMM err#%d", sres);
    }
    else
    {
      peer->tid[tid_idx].rx.state = MTLK_ADDBA_RX_DELBA_REQ_SENT;
      mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_DELBA_SENT_REQUEST_RX]);
    }
  }
  else
  {
    ELOG_V("no msg available");
  }
}

static void
_mtlk_addba_peer_stop (mtlk_addba_peer_t *peer)
{
  uint16 tid_idx = 0;
  BOOL   stop    = FALSE;

  /* Close opened aggregations & reorderings */
  for (tid_idx = 0; tid_idx < SIZEOF(peer->tid); tid_idx++)
  {
    mtlk_osal_lock_acquire(&peer->addba->lock);
    stop = _mtlk_addba_peer_reset_tx_state(peer, tid_idx);
    mtlk_osal_lock_release(&peer->addba->lock);

    /* After _mtlk_addba_peer_reset_tx_state invocation state machine */
    /* is in state MTLK_ADDBA_TX_NONE so no new retries will be sent  */
    /* by timer. There still may be outstanding TXMM message with     */
    /* pending callback. It is not a problem if it is called now,     */
    /* but since we don't want it to be called later, we have to      */
    /* cancel the message.                                            */
    mtlk_txmm_msg_cancel(&peer->tid[tid_idx].tx.man_msg);

    if (stop)
    {
      peer->api.on_stop_aggregation(peer->api.usr_data, tid_idx);
      ILOG2_YD("TX %Y TID=%d aggregation closed", &peer->addr, tid_idx);
    }

    mtlk_osal_lock_acquire(&peer->addba->lock);
    stop = _mtlk_addba_peer_reset_rx_state(peer, tid_idx);
    mtlk_osal_lock_release(&peer->addba->lock);

    /* After _mtlk_addba_peer_reset_rx_state invocation state machine */
    /* is in state MTLK_ADDBA_RX_NONE so no new retries will be sent  */
    /* by timer. There still may be outstanding TXMM message with     */
    /* pending callback. It is not a problem if it is called now,     */
    /* but since we don't want it to be called later, we have to      */
    /* cancel the message.                                            */
    mtlk_txmm_msg_cancel(&peer->tid[tid_idx].rx.man_msg);

    if (stop)
    {
      peer->api.on_stop_reordering(peer->api.usr_data, tid_idx);
      ILOG2_YD("RX %Y TID=%d reordering closed", &peer->addr, tid_idx);
    }

    /* Reset excluding the man_msg's */
    memset(&peer->tid[tid_idx].tx, 0, MTLK_OFFSET_OF(mtlk_addba_peer_tx_t, man_msg));
    memset(&peer->tid[tid_idx].rx, 0, MTLK_OFFSET_OF(mtlk_addba_peer_rx_t, man_msg));
  }
}

static void
_mtlk_addba_peer_start_negotiation (mtlk_addba_peer_t *peer)
{
  uint16 tid, i = 0;

  for (i = 0; i < NTS_PRIORITIES; i++)
  {
    tid = mtlk_qos_get_tid_by_ac(i);
    ILOG2_DDD("use_aggr[%d (ac=%d)]=%d", (int)tid, (int)i, peer->addba->cfg.tid[tid].use_aggr);
    if (peer->addba->cfg.tid[tid].use_aggr)
      _mtlk_addba_peer_tx_addba_req(peer, tid);
  }
}

static __INLINE void
_mtlk_addba_peer_check_addba_res_rx_timeouts (mtlk_addba_peer_t *peer)
{
  uint8 tid_idx = 0;

  for (tid_idx = 0; tid_idx < SIZEOF(peer->tid); tid_idx++)
  {
    BOOL notify = FALSE;
    mtlk_osal_lock_acquire(&peer->addba->lock);
    if (peer->tid[tid_idx].tx.state == MTLK_ADDBA_TX_ADDBA_REQ_CFMD)
    {
      mtlk_osal_msec_t now_time  = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
      mtlk_osal_ms_diff_t diff_time = mtlk_osal_ms_time_diff(now_time, peer->tid[tid_idx].tx.addba_req_cfmd_time);
      if (diff_time >= MTLK_ADDBA_RE_TX_REQ_TIMEOUT_MS)
      {
        BOOL stop = _mtlk_addba_peer_reset_tx_state(peer, tid_idx);

        MTLK_ASSERT(stop == FALSE);    /* Aggregations cannot be started at this phase */
        MTLK_UNREFERENCED_PARAM(stop); /* For release compilation */

        ILOG2_YDDD("TX %Y TID=%d request timeout expired (%u >= %u)",
            &peer->addr, tid_idx, diff_time,
            MTLK_ADDBA_RE_TX_REQ_TIMEOUT_MS);

        notify = TRUE;
      }
    }
    mtlk_osal_lock_release(&peer->addba->lock);

    if (notify)
    {
      peer->api.on_ba_req_unconfirmed(peer->api.usr_data, tid_idx);
    }
  }
}

static __INLINE void
_mtlk_addba_peer_check_delba_timeouts (mtlk_addba_peer_t *peer)
{
  uint16 tid_idx = 0;

  for (tid_idx = 0; tid_idx < SIZEOF(peer->tid); tid_idx++)
  {
    mtlk_osal_lock_acquire(&peer->addba->lock);
    /* check DELBA send timeout */
    if (peer->tid[tid_idx].rx.state == MTLK_ADDBA_RX_REORD_IN_PROCESS)
    {
      if (peer->tid[tid_idx].rx.delba_timeout)
      {
        uint32 last_rx, now, diff;

        last_rx = peer->api.get_last_rx_timestamp(peer->api.usr_data, tid_idx);

        if (last_rx < peer->tid[tid_idx].rx.req_tstamp)
          last_rx = peer->tid[tid_idx].rx.req_tstamp;

        now  = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
        diff = mtlk_osal_ms_time_diff(now, last_rx);

        if (diff >= peer->tid[tid_idx].rx.delba_timeout)
        {
          ILOG2_YDDD("RX %Y TID=%d DELBA timeout expired (%u >= %u)",
              &peer->addr, tid_idx,
              diff, peer->tid[tid_idx].rx.delba_timeout);
          _mtlk_addba_peer_send_rx_delba_req(peer, tid_idx);
        }
      }
    }
    mtlk_osal_lock_release(&peer->addba->lock);
  }
}

MTLK_INIT_STEPS_LIST_BEGIN(addba)
  MTLK_INIT_STEPS_LIST_ENTRY(addba, INIT_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(addba, REG_ABILITIES)
  MTLK_INIT_STEPS_LIST_ENTRY(addba, EN_ABILITIES)
MTLK_INIT_INNER_STEPS_BEGIN(addba)
MTLK_INIT_STEPS_LIST_END(addba);

static const mtlk_ability_id_t _addba_abilities[] = {
  MTLK_CORE_REQ_GET_ADDBA_CFG,
  MTLK_CORE_REQ_SET_ADDBA_CFG
};

int __MTLK_IFUNC
mtlk_addba_init (mtlk_addba_t           *obj,
                 mtlk_txmm_t            *txmm,
                 mtlk_reflim_t          *aggr_reflim,
                 mtlk_reflim_t          *reord_reflim,
                 const mtlk_addba_cfg_t *cfg,
                 mtlk_vap_handle_t      vap_handle)
{
  MTLK_ASSERT(obj != NULL);
  MTLK_ASSERT(cfg != NULL);
  MTLK_ASSERT(txmm != NULL);
  MTLK_ASSERT(aggr_reflim != NULL);
  MTLK_ASSERT(reord_reflim != NULL);

  MTLK_INIT_TRY(addba, MTLK_OBJ_PTR(obj))
    MTLK_INIT_STEP(addba, INIT_LOCK, MTLK_OBJ_PTR(obj), 
                   mtlk_osal_lock_init,  (&obj->lock));
    obj->vap_handle     = vap_handle;
    obj->cfg            = *cfg;
    obj->txmm           = txmm;
    obj->aggr_reflim    = aggr_reflim;
    obj->reord_reflim   = reord_reflim;
    obj->next_dlg_token = 0;

    MTLK_INIT_STEP(addba, REG_ABILITIES, MTLK_OBJ_PTR(obj),
                   mtlk_abmgr_register_ability_set,
                   (mtlk_vap_get_abmgr(obj->vap_handle), _addba_abilities, ARRAY_SIZE(_addba_abilities)));
    MTLK_INIT_STEP_VOID(addba, EN_ABILITIES, MTLK_OBJ_PTR(obj),
                        mtlk_abmgr_enable_ability_set,
                        (mtlk_vap_get_abmgr(obj->vap_handle), _addba_abilities, ARRAY_SIZE(_addba_abilities)));
  MTLK_INIT_FINALLY(addba, MTLK_OBJ_PTR(obj))
  MTLK_INIT_RETURN(addba, MTLK_OBJ_PTR(obj), mtlk_addba_cleanup, (obj))
}

void __MTLK_IFUNC
mtlk_addba_cleanup (mtlk_addba_t *obj)
{
  MTLK_CLEANUP_BEGIN(addba, MTLK_OBJ_PTR(obj))
    MTLK_CLEANUP_STEP(addba, EN_ABILITIES, MTLK_OBJ_PTR(obj),
                      mtlk_abmgr_disable_ability_set,
                      (mtlk_vap_get_abmgr(obj->vap_handle), _addba_abilities, ARRAY_SIZE(_addba_abilities)));
    MTLK_CLEANUP_STEP(addba, REG_ABILITIES, MTLK_OBJ_PTR(obj),
                      mtlk_abmgr_unregister_ability_set,
                      (mtlk_vap_get_abmgr(obj->vap_handle), _addba_abilities, ARRAY_SIZE(_addba_abilities)));
    MTLK_CLEANUP_STEP(addba, INIT_LOCK, MTLK_OBJ_PTR(obj),
                      mtlk_osal_lock_cleanup, (&obj->lock));
  MTLK_CLEANUP_END(addba, MTLK_OBJ_PTR(obj));
}

int  __MTLK_IFUNC 
mtlk_addba_reconfigure (mtlk_addba_t           *obj, 
                        const mtlk_addba_cfg_t *cfg)
{
  mtlk_osal_lock_acquire(&obj->lock);
  obj->cfg = *cfg;
  mtlk_osal_lock_release(&obj->lock);
  
  return MTLK_ERR_OK;
}

MTLK_INIT_STEPS_LIST_BEGIN(addba_peer)
  MTLK_INIT_STEPS_LIST_ENTRY(addba_peer, TXMM_MSG_INIT_TX)
  MTLK_INIT_STEPS_LIST_ENTRY(addba_peer, TXMM_MSG_INIT_RX)
  MTLK_INIT_STEPS_LIST_ENTRY(addba_peer, WSS_OBJ)
  MTLK_INIT_STEPS_LIST_ENTRY(addba_peer, WSS_CNTRs)
MTLK_INIT_INNER_STEPS_BEGIN(addba_peer)
MTLK_INIT_STEPS_LIST_END(addba_peer);

int  __MTLK_IFUNC
mtlk_addba_peer_init (mtlk_addba_peer_t           *peer,
                      const mtlk_addba_wrap_api_t *api,
                      mtlk_addba_t                *addba,
                      mtlk_wss_t                  *parent_wss)
{
  uint8 i;

  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(addba != NULL);
  MTLK_ASSERT(api != NULL);
  MTLK_ASSERT(parent_wss != NULL);
  MTLK_ASSERT(api->get_last_rx_timestamp != NULL);
  MTLK_ASSERT(api->on_start_aggregation != NULL);
  MTLK_ASSERT(api->on_stop_aggregation != NULL);
  MTLK_ASSERT(api->on_start_reordering != NULL);
  MTLK_ASSERT(api->on_stop_reordering != NULL);
  MTLK_ASSERT(api->on_ba_req_rejected != NULL);
  MTLK_ASSERT(api->on_ba_req_unconfirmed != NULL);

  MTLK_ASSERT(ARRAY_SIZE(_mtlk_addba_peer_cntr_wss_id_map) == MTLK_ADDBAPI_CNT_LAST);

  peer->addba     = addba;
  peer->is_active = FALSE;
  peer->api       = *api;

  MTLK_INIT_TRY(addba_peer, MTLK_OBJ_PTR(peer))
    for (i = 0; i < ARRAY_SIZE(peer->tid); i++) {
      MTLK_INIT_STEP_LOOP(addba_peer, TXMM_MSG_INIT_TX, MTLK_OBJ_PTR(peer),
                          mtlk_txmm_msg_init, (&peer->tid[i].tx.man_msg));
    }
    for (i = 0; i < ARRAY_SIZE(peer->tid); i++) {
      MTLK_INIT_STEP_LOOP(addba_peer, TXMM_MSG_INIT_RX, MTLK_OBJ_PTR(peer),
                           mtlk_txmm_msg_init, (&peer->tid[i].rx.man_msg));
    }
    MTLK_INIT_STEP_EX(addba_peer, WSS_OBJ, MTLK_OBJ_PTR(peer),
                      mtlk_wss_create, (parent_wss, NULL, 0),
                      peer->wss, peer->wss != NULL, MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP(addba_peer, WSS_CNTRs, MTLK_OBJ_PTR(peer),
                   mtlk_wss_cntrs_open, (peer->wss, _mtlk_addba_peer_cntr_wss_id_map, peer->wss_hcntrs, MTLK_ADDBAPI_CNT_LAST));
  MTLK_INIT_FINALLY(addba_peer, MTLK_OBJ_PTR(peer))
  MTLK_INIT_RETURN(addba_peer, MTLK_OBJ_PTR(peer), mtlk_addba_peer_cleanup, (peer))
}

void __MTLK_IFUNC
mtlk_addba_peer_cleanup (mtlk_addba_peer_t *peer)
{
  uint8 i;

  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(peer->is_active == FALSE);

  MTLK_CLEANUP_BEGIN(addba_peer, MTLK_OBJ_PTR(peer))
    MTLK_CLEANUP_STEP(addba_peer, WSS_CNTRs, MTLK_OBJ_PTR(peer),
                      mtlk_wss_cntrs_close, (peer->wss, peer->wss_hcntrs, MTLK_ADDBAPI_CNT_LAST));
    MTLK_CLEANUP_STEP(addba_peer, WSS_OBJ, MTLK_OBJ_PTR(peer),
                      mtlk_wss_delete, (peer->wss));
    for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(peer), TXMM_MSG_INIT_RX) > 0; i++) {
      MTLK_CLEANUP_STEP_LOOP(addba_peer, TXMM_MSG_INIT_RX, MTLK_OBJ_PTR(peer),
                             mtlk_txmm_msg_cleanup, (&peer->tid[i].rx.man_msg));
    }
                               
    for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(peer), TXMM_MSG_INIT_TX) > 0; i++) {
      MTLK_CLEANUP_STEP_LOOP(addba_peer, TXMM_MSG_INIT_TX, MTLK_OBJ_PTR(peer),
                             mtlk_txmm_msg_cleanup, (&peer->tid[i].tx.man_msg));
    }
  peer->addba = NULL;
  MTLK_CLEANUP_END(addba_peer, MTLK_OBJ_PTR(peer))
}

void __MTLK_IFUNC 
mtlk_addba_peer_on_delba_req_rx (mtlk_addba_peer_t* peer,
                                 uint16             tid_idx,
                                 uint16             res_code,
                                 uint16             initiator)
{
  MTLK_ASSERT(peer != NULL);

  ILOG2_YDDD("TX %Y TID=%d DELBA recvd, RES=%d IN=%d", &peer->addr,
       (int)tid_idx, (int)res_code, (int)initiator);

  if (peer->is_active)
  {
    BOOL stop = FALSE;
    mtlk_osal_lock_acquire(&peer->addba->lock);

    if (initiator) {
      stop = _mtlk_addba_peer_reset_rx_state(peer, tid_idx);
      mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_DELBA_RECEIVED_REQUEST_RX]);
    }
    else {
      /* 
         Sent by recipient of the data => our TX-related =>
         we should stop the aggregations transmission.
      */
      _mtlk_addba_peer_close_aggr_req(peer, tid_idx);
      mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_DELBA_REQUEST_TX]);
    }

    mtlk_osal_lock_release(&peer->addba->lock);

    if (stop)
    {
      peer->api.on_stop_reordering(peer->api.usr_data, tid_idx);
    }
  }
}

void __MTLK_IFUNC 
mtlk_addba_peer_on_addba_res_rx (mtlk_addba_peer_t *peer,
                                 uint16             res_code,
                                 uint16             tid_idx,
                                 uint8              dlgt)
{
  int notify_reject = 0;

  MTLK_ASSERT(peer != NULL);

  ILOG2_YDDD("TX %Y TID=%d TAG=%d response recvd RES=%d",
       &peer->addr, (int)tid_idx, (int)dlgt, (int)res_code);

  if (peer->is_active)
  {
    mtlk_osal_lock_acquire(&peer->addba->lock);

    /* Check the TID and drop message if it isn't belongs to the
     * current ADDBA session */
    if (peer->tid[tid_idx].tx.addba_req_dlgt != dlgt) {
      WLOG_YDDD("TX: %Y TID=%u TAG=%u invalid TAG: currentTAG=%u - ignoring response",
              &peer->addr, (int)tid_idx, (int)dlgt,
              (int)peer->tid[tid_idx].tx.addba_req_dlgt);

      goto out;
    };

    if (res_code == MTLK_ADDBA_RES_CODE_SUCCESS)
    {
      mtlk_txmm_data_t  *tx_data;
      UMI_OPEN_AGGR_REQ *add_aggr_req;
      int                sres;

      ILOG2_YDD("TX %Y TID=%d TAG=%d accepted by peer",
          &peer->addr, (int)tid_idx, (int)dlgt);

      peer->tid[tid_idx].tx.addba_res_rejects = 0;

      if (peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_ADDBA_REQ_SENT &&
          peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_ADDBA_REQ_CFMD) {
        WLOG_YDDD("TX: %Y TID=%d TAG=%d invalid state: %d - ignoring response",
                &peer->addr, (int)tid_idx, (int)dlgt,
                (int)peer->tid[tid_idx].tx.state);
        goto out;
      }

      tx_data = mtlk_txmm_msg_get_empty_data(&peer->tid[tid_idx].tx.man_msg,
                                             peer->addba->txmm);
      if (!tx_data)
      {
        ELOG_V("no msg available");
        goto out;
      }

      add_aggr_req = (UMI_OPEN_AGGR_REQ*)tx_data->payload;

      tx_data->id           = UM_MAN_OPEN_AGGR_REQ;
      tx_data->payload_size = sizeof(*add_aggr_req);
    
      add_aggr_req->sDA                      = peer->addr;
      add_aggr_req->u16AccessProtocol        = HOST_TO_MAC16(tid_idx);
      add_aggr_req->u16MaxNumOfPackets       = HOST_TO_MAC16(peer->addba->cfg.tid[tid_idx].max_nof_packets);
      add_aggr_req->u32MaxNumOfBytes         = HOST_TO_MAC32(peer->addba->cfg.tid[tid_idx].max_nof_bytes);
      add_aggr_req->u32TimeoutInterval       = HOST_TO_MAC32(peer->addba->cfg.tid[tid_idx].timeout_interval);
      add_aggr_req->u32MinSizeOfPacketInAggr = HOST_TO_MAC32(peer->addba->cfg.tid[tid_idx].min_packet_size_in_aggr);
      add_aggr_req->u16Status                = HOST_TO_MAC16(UMI_OK);

      ILOG2_YD("TX %Y TID=%d opening aggregation",
           add_aggr_req->sDA.au8Addr, (int)tid_idx);

      sres = mtlk_txmm_msg_send(&peer->tid[tid_idx].tx.man_msg, _mtlk_addba_peer_on_open_aggr_cfm_clb,
                                HANDLE_T(peer), 0);
      if (sres == MTLK_ERR_OK)
      {
        peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_ADD_AGGR_REQ_SENT;
        mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_AGGR_OPEN_REQUEST]);
      }
      else
      {
        ELOG_D("Can't open AGGR due to TXMM err#%d", sres);
      }
      mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_ADDBA_POSITIVE_RESPONSE_TX]);
    }
    else
    {
      BOOL stop = _mtlk_addba_peer_reset_tx_state(peer, tid_idx);

      MTLK_ASSERT(stop == FALSE);    /* Aggregations cannot be started at this phase */
      MTLK_UNREFERENCED_PARAM(stop); /* For Release compilation */

      ILOG2_YDD("TX %Y TID=%d TAG=%d rejected by peer",
          &peer->addr, (int)tid_idx, (int)dlgt);

      ++peer->tid[tid_idx].tx.addba_res_rejects; 
      if (peer->tid[tid_idx].tx.addba_res_rejects < MTLK_ADDBA_MAX_REJECTS) 
      { 
        notify_reject = 1; 
      } 
      else 
      { 
        ILOG1_YD("ADDBA rejects limit reached for peer %Y, TID=%d. "
                "No more notifications will be sent to upper layer", 
                 &peer->addr, tid_idx); 
        peer->tid[tid_idx].tx.addba_res_rejects = 0; 
      }
      mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_ADDBA_NEGATIVE_RESPONSE_TX]);
    }
out:
    mtlk_osal_lock_release(&peer->addba->lock);
    if (notify_reject) 
    { 
      peer->api.on_ba_req_rejected(peer->api.usr_data, tid_idx); 
    } 
  }
}

void __MTLK_IFUNC 
mtlk_addba_peer_on_addba_req_rx (mtlk_addba_peer_t *peer, 
                                 uint16             ssn,
                                 uint16             tid_idx,
                                 uint8              win_size,
                                 uint8              dlgt,
                                 uint16             tmout,
                                 uint16             ack_policy,
                                 uint16             rate)
{
  MTLK_ASSERT(peer != NULL);

  ILOG2_YDDDDDD("RX %Y TID=%d TAG=%d req recvd WSIZE=%d SSN=%d TMBA=%d ACP=%d",
       &peer->addr,
       (int)tid_idx,
       (int)dlgt,
       (int)win_size,
       (int)ssn,
       (int)tmout,
       (int)ack_policy);

  if (peer->is_active)
  {
    int stop_reord  = 0;
    int start_reord = 0;

    mtlk_osal_lock_acquire(&peer->addba->lock);
    mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_ADDBA_REQUEST_RX]);

    if (tid_idx >= SIZEOF(peer->tid))
    {
      ELOG_YDDDD("RX %Y TID=%d TAG=%d: wrong priority (%d >= %ld)",
          &peer->addr, (int)tid_idx, (int)dlgt,
          tid_idx, SIZEOF(peer->tid));
    }
    else
    {
      mtlk_txmm_data_t* tx_data = NULL;

      if (peer->tid[tid_idx].rx.state != MTLK_ADDBA_RX_NONE)
      {
        ILOG2_YDDD("RX %Y TID=%d TAG=%d: invalid state %d",
            &peer->addr, (int)tid_idx, (int)dlgt,
            peer->tid[tid_idx].rx.state);
        stop_reord = _mtlk_addba_peer_reset_rx_state(peer, tid_idx);
      }

      tx_data = mtlk_txmm_msg_get_empty_data(&peer->tid[tid_idx].rx.man_msg,
                                             peer->addba->txmm);
      if (tx_data)
      {
        UMI_ADDBA_RES_SEND* addba_res = (UMI_ADDBA_RES_SEND*)tx_data->payload;
        int                 sres;

        tx_data->id           = UM_MAN_ADDBA_RES_TX_REQ;
        tx_data->payload_size = sizeof(*addba_res);

        addba_res->sDA               = peer->addr;
        addba_res->u8DialogToken     = dlgt;
        addba_res->u16AccessProtocol = HOST_TO_MAC16(tid_idx);
        addba_res->u16Status         = HOST_TO_MAC16(UMI_OK);

        if (!_mtlk_addba_is_allowed_rate(rate))
        {
          ILOG2_YDDD("RX %Y TID=%d TAG=%d (DECLINED: RATE == %d)",
               &peer->addr, (int)tid_idx, (int)dlgt, (int)rate);
          addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_FAILURE);
        }
        else if (!peer->addba->cfg.tid[tid_idx].accept_aggr)
        {
          ILOG2_YDD("RX %Y TID=%d TAG=%d (DECLINED: CFG OFF)",
              &peer->addr, (int)tid_idx, (int)dlgt);
          addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_FAILURE);
        }
        else if (!ack_policy)
        {
          ILOG2_YDD("RX %Y TID=%d TAG=%d (DECLINED: ACP == 0)",
              &peer->addr, (int)tid_idx, (int)dlgt);
          addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_FAILURE);
        }
        else if (!mtlk_reflim_try_ref(peer->addba->reord_reflim))
        {
          ILOG2_YDDDD("RX %Y TID=%d TAG=%d (DECLINED: LIM %d >= %d)",
                      &peer->addr, (int)tid_idx, (int)dlgt,
                      mtlk_reflim_get_cur(peer->addba->reord_reflim), mtlk_reflim_get_max(peer->addba->reord_reflim));
          addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_FAILURE);
        }
        else
        {
          _mtlk_addba_correct_res_win_size(&win_size);
          _mtlk_addba_correct_res_timeout(&tmout);

          addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_SUCCESS);
          addba_res->u16BATimeout  = HOST_TO_MAC16(tmout);
          addba_res->u8WinSize     = win_size;

          ILOG2_YDDDD("RX %Y TID=%d TAG=%d (ACCEPTED: TMBA=%d WSIZE=%d)",
              &peer->addr, (int)tid_idx, (int)dlgt,
              (int)tmout, (int)win_size);
          
          start_reord = 1;
        }

        sres = mtlk_txmm_msg_send(&peer->tid[tid_idx].rx.man_msg, _mtlk_addba_peer_on_addba_res_tx_cfm_clb,
                                  HANDLE_T(peer), 0);
        if (sres != MTLK_ERR_OK)
        {
          ELOG_D("Can't send ADDBA response due to TXMM err#%d", sres);
          if (start_reord) {
            mtlk_reflim_unref(peer->addba->reord_reflim);
          }
        }
        else if (start_reord)
        {
          peer->tid[tid_idx].rx.state         = MTLK_ADDBA_RX_ADDBA_POSITIVE_RES_SENT;
          peer->tid[tid_idx].rx.delba_timeout = (uint16)ADDBA_TU_TO_MS(tmout);
          peer->tid[tid_idx].rx.req_tstamp    = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());

          mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_REORD_ACTIVE]);
          mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_ADDBA_POSITIVE_RESPONSE_RX]);
        }
        else
        {
          peer->tid[tid_idx].rx.state         = MTLK_ADDBA_RX_ADDBA_NEGATIVE_RES_SENT;
          mtlk_wss_cntr_inc(peer->wss_hcntrs[MTLK_ADDBAPI_CNT_ADDBA_NEGATIVE_RESPONSE_RX]);
        }
      }
      else
      {
        ELOG_V("Can't send ADDBA resp due to lack of MAN_MSG");
      }
    }
    mtlk_osal_lock_release(&peer->addba->lock);

    if (stop_reord)
    {
      peer->api.on_stop_reordering(peer->api.usr_data, tid_idx);
    }
    
    if (start_reord)
    {
      peer->api.on_start_reordering(peer->api.usr_data, tid_idx, ssn, win_size);
    }
  }
}

void __MTLK_IFUNC
mtlk_addba_peer_iterate (mtlk_addba_peer_t *peer)
{
  MTLK_ASSERT(peer != NULL);

  if (peer->is_active)
  {
    _mtlk_addba_peer_check_delba_timeouts(peer);
    _mtlk_addba_peer_check_addba_res_rx_timeouts(peer);
  }
}

int __MTLK_IFUNC
mtlk_addba_peer_start (mtlk_addba_peer_t *peer,
                       const IEEE_ADDR   *addr)
{
  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(addr != NULL);
  MTLK_ASSERT(peer->is_active == FALSE);

  _mtlk_addba_peer_stop(peer);
   peer->addr      = *addr;
   peer->is_active = TRUE;

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
mtlk_addba_peer_start_negotiation (mtlk_addba_peer_t *peer,
                                   uint16             rate)
{
  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(peer->is_active == TRUE);

  if (_mtlk_addba_is_allowed_rate(rate)) {
    mtlk_osal_lock_acquire(&peer->addba->lock);
    _mtlk_addba_peer_start_negotiation(peer);
    mtlk_osal_lock_release(&peer->addba->lock);
  }
  else {
     /* Aggregations is not allowed for this rate */
    ILOG2_D("Aggregations are not allowed (rate=%d)", (int)rate);
  }
}

void __MTLK_IFUNC
mtlk_addba_peer_stop (mtlk_addba_peer_t *peer)
{
  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(peer->is_active == TRUE);

  peer->is_active = FALSE;
  _mtlk_addba_peer_stop(peer);
  memset(&peer->addr, 0, sizeof(peer->addr));
}

void __MTLK_IFUNC
mtlk_addba_peer_start_aggr_negotiation (mtlk_addba_peer_t *peer, 
                                        uint16             tid)
{
  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(peer->is_active == TRUE);
  MTLK_ASSERT(tid < ARRAY_SIZE(peer->addba->cfg.tid));

  mtlk_osal_lock_acquire(&peer->addba->lock);
  if (peer->addba->cfg.tid[tid].use_aggr)
  {
    _mtlk_addba_peer_tx_addba_req(peer, tid);
  }
  mtlk_osal_lock_release(&peer->addba->lock);
}
