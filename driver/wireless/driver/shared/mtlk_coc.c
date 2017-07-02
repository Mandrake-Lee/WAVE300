/*
 * $Id: mtlk_coc.c 11419 2011-07-18 10:57:38Z fleytman $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Grygorii Strashko
 *
 * Power management functionality implementation in compliance with
 * Code of Conduct on Energy Consumption of Broadband Equipment (a.k.a CoC)
 *
 */

#include "mtlkinc.h"
#include "mhi_umi.h"
#include "txmm.h"
#include "core.h"
#include "mtlk_coreui.h"
#include "mtlk_coc.h"

#define LOG_LOCAL_GID   GID_COC
#define LOG_LOCAL_FID   1

/**********************************************************************
 * Local definitions
***********************************************************************/
#define MTLK_COC_LOW_POWER_MODE_TXNUM  (1)
#define MTLK_COC_LOW_POWER_MODE_RXNUM  (1)

struct _mtlk_coc_t
{
  mtlk_core_t *core;
  mtlk_txmm_t *txmm;
  mtlk_coc_antenna_cfg_t hw_antenna_cfg;
  mtlk_coc_antenna_cfg_t current_antenna_cfg;

  MTLK_DECLARE_INIT_STATUS;
};

/**********************************************************************
 * Local function declaration
***********************************************************************/
static int __MTLK_IFUNC
_mtlk_coc_switch_mode(mtlk_coc_t *coc_obj, uint8 num_tx_antennas, uint8 num_rx_antennas);

static int __MTLK_IFUNC
_mtlk_coc_init(mtlk_coc_t *coc_obj, const mtlk_coc_cfg_t *cfg);

static void __MTLK_IFUNC
_mtlk_coc_cleanup(mtlk_coc_t *coc_obj);

/**
 * ---------------------------------
 * \brief CoC functionality description
 *
 * UI based changes:
 * - Driver provides gCoCLowPower iwpriv to get Low Power State
 * - Driver provides sCoCLowPower iwpriv to set Low Power State
 * - By default, Driver configures FW to High Power State (i.e. gCoCLowPower returns 0 by default)
 * - To switch to Low Power State (iwpriv sCoCLowPower 1),
 *   Driver sends TxNum=1, RxNum=1
 * - To switch to High Power State (insmod or iwpriv sCoCLowPower 0),
 *   Driver sends TxNum and RxNum according to HW type.
 *
 * Traffic based changes:
 * - Driver supports UI based changes as well
 * - Driver supports CoCIdleTmout parameter via gCoCIdleTmout /sCoCIdleTmout iwpriv's. By default,
 *   CoCIdleTmout == 0 that means Traffic based CoC mechanism is OFF.
 * - Driver automatically switches FW to Low Power State
 *   if there were no TX packets within CoCIdleTmout seconds
 * - Driver automatically switches FW to High Power State
 *   if a the FW is in Low Power State (i.e. no Switch to High Power State message has been sent)
 *   and a packet arrives for TX
 */

/*****************************************************************************
* Function implementation
******************************************************************************/
mtlk_coc_t* __MTLK_IFUNC
mtlk_coc_create(const mtlk_coc_cfg_t *cfg)
{
  mtlk_coc_t    *coc_obj =
            mtlk_osal_mem_alloc(sizeof(mtlk_coc_t), MTLK_MEM_TAG_DFS);

    if (NULL != coc_obj)
    {
      memset(coc_obj, 0, sizeof(mtlk_coc_t));
      if(MTLK_ERR_OK != _mtlk_coc_init(coc_obj, cfg))
      {
        mtlk_osal_mem_free(coc_obj);
        coc_obj = NULL;
      }
    }

    return coc_obj;
}

void __MTLK_IFUNC
mtlk_coc_delete(mtlk_coc_t *coc_obj)
{
  MTLK_ASSERT(NULL != coc_obj);

  _mtlk_coc_cleanup(coc_obj);

  mtlk_osal_mem_free(coc_obj);
}

static const mtlk_ability_id_t _coc_abilities[] = {
  MTLK_CORE_REQ_GET_COC_CFG,
  MTLK_CORE_REQ_SET_COC_CFG
};

MTLK_INIT_STEPS_LIST_BEGIN(coc)
  MTLK_INIT_STEPS_LIST_ENTRY(coc, REG_ABILITIES)
  MTLK_INIT_STEPS_LIST_ENTRY(coc, EN_ABILITIES)
MTLK_INIT_INNER_STEPS_BEGIN(coc)
MTLK_INIT_STEPS_LIST_END(coc);

int __MTLK_IFUNC
_mtlk_coc_init(mtlk_coc_t *coc_obj, const mtlk_coc_cfg_t *cfg)
{
  MTLK_ASSERT(NULL != coc_obj);
  MTLK_ASSERT(NULL != cfg->txmm);

  /* Initial state is High power mode */
  coc_obj->hw_antenna_cfg = cfg->hw_antenna_cfg;
  coc_obj->current_antenna_cfg = cfg->hw_antenna_cfg;
  coc_obj->txmm = cfg->txmm;
  coc_obj->core = cfg->core;

  MTLK_INIT_TRY(coc, MTLK_OBJ_PTR(coc_obj))
    MTLK_INIT_STEP(coc, REG_ABILITIES, MTLK_OBJ_PTR(coc_obj),
                   mtlk_abmgr_register_ability_set,
                   (mtlk_vap_get_abmgr(coc_obj->core->vap_handle),
                    _coc_abilities, ARRAY_SIZE(_coc_abilities)));
    MTLK_INIT_STEP_VOID(coc, EN_ABILITIES, MTLK_OBJ_PTR(coc_obj),
                        mtlk_abmgr_enable_ability_set,
                        (mtlk_vap_get_abmgr(coc_obj->core->vap_handle),
                         _coc_abilities, ARRAY_SIZE(_coc_abilities)));
  MTLK_INIT_FINALLY(coc, MTLK_OBJ_PTR(coc_obj))
  MTLK_INIT_RETURN(coc, MTLK_OBJ_PTR(coc_obj), _mtlk_coc_cleanup, (coc_obj));
}

void __MTLK_IFUNC
_mtlk_coc_cleanup(mtlk_coc_t *coc_obj)
{
  MTLK_ASSERT(NULL != coc_obj);

  MTLK_CLEANUP_BEGIN(coc, MTLK_OBJ_PTR(coc_obj))
    MTLK_CLEANUP_STEP(coc, EN_ABILITIES, MTLK_OBJ_PTR(coc_obj),
                      mtlk_abmgr_disable_ability_set,
                      (mtlk_vap_get_abmgr(coc_obj->core->vap_handle),
                      _coc_abilities, ARRAY_SIZE(_coc_abilities)));
    MTLK_CLEANUP_STEP(coc, REG_ABILITIES, MTLK_OBJ_PTR(coc_obj),
                      mtlk_abmgr_unregister_ability_set,
                      (mtlk_vap_get_abmgr(coc_obj->core->vap_handle),
                      _coc_abilities, ARRAY_SIZE(_coc_abilities)));
  MTLK_CLEANUP_END(coc, MTLK_OBJ_PTR(coc_obj));
}

int __MTLK_IFUNC
mtlk_coc_low_power_mode_enable(mtlk_coc_t *coc_obj)
{
  int  res;

  MTLK_ASSERT(NULL != coc_obj);

  res = _mtlk_coc_switch_mode(
            coc_obj,
            MTLK_COC_LOW_POWER_MODE_TXNUM,
            MTLK_COC_LOW_POWER_MODE_RXNUM);

  if (MTLK_ERR_OK == res)
  {
    coc_obj->current_antenna_cfg.num_tx_antennas = MTLK_COC_LOW_POWER_MODE_TXNUM;
    coc_obj->current_antenna_cfg.num_rx_antennas = MTLK_COC_LOW_POWER_MODE_RXNUM;
  }

  return res;
}

int __MTLK_IFUNC
mtlk_coc_high_power_mode_enable(mtlk_coc_t *coc_obj)
{
  int  res;

  MTLK_ASSERT(NULL != coc_obj);

  res = _mtlk_coc_switch_mode(
      coc_obj,
      coc_obj->hw_antenna_cfg.num_tx_antennas,
      coc_obj->hw_antenna_cfg.num_rx_antennas);

  if (MTLK_ERR_OK == res)
  {
    coc_obj->current_antenna_cfg = coc_obj->hw_antenna_cfg;
  }

  return res;
}

int __MTLK_IFUNC
_mtlk_coc_switch_mode(mtlk_coc_t *coc_obj, uint8 num_tx_antennas, uint8 num_rx_antennas)
{
  mtlk_txmm_msg_t         man_msg;
  mtlk_txmm_data_t*       man_entry = NULL;
  UMI_CHANGE_POWER_STATE  *mac_msg;
  int                     res = MTLK_ERR_OK;

  ILOG4_DD("UMI_CHANGE_POWER_STATE low_power_state TX%dxRX%d", num_tx_antennas, num_rx_antennas);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, coc_obj->txmm, NULL);

  if (NULL == man_entry)
  {
    res = MTLK_ERR_NO_RESOURCES;
  }
  else
  {
    man_entry->id           = MC_MAN_CHANGE_POWER_STATE_REQ;
    man_entry->payload_size = sizeof(UMI_CHANGE_POWER_STATE);
    mac_msg = (UMI_CHANGE_POWER_STATE *)man_entry->payload;

    memset(mac_msg, 0, sizeof(*mac_msg));

    mac_msg->TxNum = num_tx_antennas;
    mac_msg->RxNum = num_rx_antennas;

    mtlk_dump(1, man_entry->payload, sizeof(UMI_CHANGE_POWER_STATE), "UMI_CHANGE_POWER_STATE dump:");

    res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);

    if ((MTLK_ERR_OK == res) && (UMI_OK != mac_msg->status))
    {
      res = MTLK_ERR_UNKNOWN;
    }

    mtlk_txmm_msg_cleanup(&man_msg);
  }

  if (MTLK_ERR_OK != res)
  {
    ELOG_DDD("UMI_CHANGE_POWER_STATE (low_power_state TX%dxRX%d failed (res=%d)",
        num_tx_antennas, num_rx_antennas, res);
  }

  return res;
}

BOOL __MTLK_IFUNC
mtlk_coc_low_power_mode_get(mtlk_coc_t *coc_obj)
{
  MTLK_ASSERT(NULL != coc_obj);

  if (   (MTLK_COC_LOW_POWER_MODE_TXNUM == coc_obj->current_antenna_cfg.num_tx_antennas)
      && (MTLK_COC_LOW_POWER_MODE_RXNUM == coc_obj->current_antenna_cfg.num_rx_antennas))
  {
    return TRUE;
  }
  return FALSE;
}

int __MTLK_IFUNC
mtlk_coc_set_mode(mtlk_coc_t *coc_obj, const mtlk_coc_antenna_cfg_t *antenna_cfg)
{
  int  res;

  MTLK_ASSERT(NULL != coc_obj);

  if (   (antenna_cfg->num_tx_antennas > coc_obj->hw_antenna_cfg.num_tx_antennas)
      || (antenna_cfg->num_rx_antennas > coc_obj->hw_antenna_cfg.num_rx_antennas))
  {
    return MTLK_ERR_PARAMS;
  }

  res = _mtlk_coc_switch_mode(
      coc_obj,
      antenna_cfg->num_tx_antennas,
      antenna_cfg->num_rx_antennas);

  if (MTLK_ERR_OK == res)
  {
    coc_obj->current_antenna_cfg = *antenna_cfg;
  }

  return res;
}

int __MTLK_IFUNC
mtlk_coc_get_mode(mtlk_coc_t *coc_obj, mtlk_coc_antenna_cfg_t *antenna_cfg)
{
  MTLK_ASSERT(NULL != coc_obj);

  *antenna_cfg = coc_obj->current_antenna_cfg;

  return MTLK_ERR_OK;
}
