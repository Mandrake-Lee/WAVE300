/*
 * $Id: mib_osdep.c 12321 2011-12-29 12:41:19Z nayshtut $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Module responsible for configuration.
 *
 * Authors: originaly written by Joel Isaacson;
 *  further development and support by: Andriy Tkachuk, Artem Migaev,
 *  Oleksandr Andrushchenko.
 *
 */

#include "mtlkinc.h"

#include "mhi_mac_event.h"
#include "mib_osdep.h"
#include "mtlkparams.h"
#include "scan.h"
#include "eeprom.h"
#include "drvver.h"
#include "aocs.h" /*for dfs.h*/
#include "mtlkmib.h"
#include "mtlkhal.h"
#include "mtlkaux.h"
#include "mtlk_coreui.h"

#define LOG_LOCAL_GID   GID_MIBS
#define LOG_LOCAL_FID   1

/*
  We must adjust the minimal values of aggregation parameters according to the definitions imposed by the Firmware.
*/
#define ADDBA_BK_DEFAULT_AGGR_MAX_PACKETS  7
#define ADDBA_BE_DEFAULT_AGGR_MAX_PACKETS  8
#define ADDBA_VI_DEFAULT_AGGR_MAX_PACKETS  7
#define ADDBA_VO_DEFAULT_AGGR_MAX_PACKETS  2

#if (ADDBA_BK_DEFAULT_AGGR_MAX_PACKETS > NO_MAX_PACKETS_AGG_SUPPORTED_BK) 
#undef ADDBA_BK_DEFAULT_AGGR_MAX_PACKETS 
#define ADDBA_BK_DEFAULT_AGGR_MAX_PACKETS NO_MAX_PACKETS_AGG_SUPPORTED_BK
#endif
#if (ADDBA_BE_DEFAULT_AGGR_MAX_PACKETS > NO_MAX_PACKETS_AGG_SUPPORTED_BE) 
#undef ADDBA_BE_DEFAULT_AGGR_MAX_PACKETS 
#define ADDBA_BE_DEFAULT_AGGR_MAX_PACKETS NO_MAX_PACKETS_AGG_SUPPORTED_BE
#endif
#if (ADDBA_VI_DEFAULT_AGGR_MAX_PACKETS > NO_MAX_PACKETS_AGG_SUPPORTED_VI) 
#undef ADDBA_VI_DEFAULT_AGGR_MAX_PACKETS 
#define ADDBA_VI_DEFAULT_AGGR_MAX_PACKETS NO_MAX_PACKETS_AGG_SUPPORTED_VI
#endif
#if (ADDBA_VO_DEFAULT_AGGR_MAX_PACKETS > NO_MAX_PACKETS_AGG_SUPPORTED_VO) 
#undef ADDBA_VO_DEFAULT_AGGR_MAX_PACKETS 
#define ADDBA_VO_DEFAULT_AGGR_MAX_PACKETS NO_MAX_PACKETS_AGG_SUPPORTED_VO
#endif

/* UseAggregation
   AcceptAggregation
   MaxNumOfPackets
   MaxNumOfBytes
   TimeoutInterval
   MinSizeOfPacketInAggr
   ADDBATimeout
   AggregationWindowSize */
#define ADDBA_BK_DEFAULTS { 0, 1, ADDBA_BK_DEFAULT_AGGR_MAX_PACKETS,  12000, 3,  10, 0, MAX_REORD_WINDOW}
#define ADDBA_BE_DEFAULTS { 1, 1, ADDBA_BE_DEFAULT_AGGR_MAX_PACKETS, 16000, 3,  10, 0, MAX_REORD_WINDOW}
#define ADDBA_VI_DEFAULTS { 1, 1, ADDBA_VI_DEFAULT_AGGR_MAX_PACKETS,  12000, 3,  10, 0, MAX_REORD_WINDOW}
#define ADDBA_VO_DEFAULTS { 1, 1, ADDBA_VO_DEFAULT_AGGR_MAX_PACKETS,  10000, 10, 10, 0, MAX_REORD_WINDOW}

#define DEFAULT_TX_POWER  17

const mtlk_core_cfg_t def_card_cfg = 
{
  { /* addba */
    { /* per TIDs  */
      ADDBA_BE_DEFAULTS,
      ADDBA_BK_DEFAULTS,
      ADDBA_BK_DEFAULTS,
      ADDBA_BE_DEFAULTS,
      ADDBA_VI_DEFAULTS,
      ADDBA_VI_DEFAULTS,
      ADDBA_VO_DEFAULTS,
      ADDBA_VO_DEFAULTS
    }
  },
  { /* wme_bss */
    { /* class */
      /*   cwmin    cwmax   aifs    txop */
      {     4,      10,     3,      0   }, /* class[0] - BE */
      {     4,      10,     7,      0   }, /* class[1] - BK */
      {     3,       4,     2,   3008   }, /* class[2] - VI */
      {     2,       3,     2,   1504   }  /* class[3] - VO */
    }
  },
  { /* wme_ap */
    { /* class */
      /*   cwmin    cwmax   aifs    txop */
      {     4,       6,     3,      0   }, /* class[0] - BE */
      {     4,      10,     7,      0   }, /* class[1] - BK */
      {     3,       4,     1,   3008   }, /* class[2] - VI */
      {     2,       3,     1,   1504   }  /* class[3] - VO */
    }
  }
};

/*****************************************************************************
**
** NAME         mtlk_mib_set_nic_cfg
**
** PARAMETERS   nic            Card context
**
** RETURNS      none
**
** DESCRIPTION  Fills the card configuration structure with user defined
**              values (or default ones)
**
******************************************************************************/
void mtlk_mib_set_nic_cfg (struct nic *nic)
{
  nic->slow_ctx->cfg = def_card_cfg;
}

int
mtlk_mib_set_forced_rates (struct nic *nic)
{
  uint16 is_force_rate; /* FORCED_RATE_LEGACY_MASK & FORCED_RATE_HT_MASK */

  /*
   * Driver should first disable adaptive rate,
   * in order to avoid condition
   * where MIB_IS_FORCE_RATE configured to use forced rate
   * and MIB_{LEGACY,HT}_FORCE_RATE configured to use NO_RATE.
   */
  mtlk_set_mib_value_uint16(mtlk_vap_get_txmm(nic->vap_handle), MIB_IS_FORCE_RATE, 0);

  mtlk_set_mib_value_uint16(mtlk_vap_get_txmm(nic->vap_handle),
    MIB_LEGACY_FORCE_RATE, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_LEGACY_FORCED_RATE_SET));
  mtlk_set_mib_value_uint16(mtlk_vap_get_txmm(nic->vap_handle),
    MIB_HT_FORCE_RATE, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_HT_FORCED_RATE_SET));

  is_force_rate = 0;
  if (NO_RATE != MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_LEGACY_FORCED_RATE_SET)) {
    is_force_rate |= FORCED_RATE_LEGACY_MASK;
  }
  if (NO_RATE != MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_HT_FORCED_RATE_SET)) {
    is_force_rate |= FORCED_RATE_HT_MASK;
  }

  mtlk_set_mib_value_uint16(mtlk_vap_get_txmm(nic->vap_handle), MIB_IS_FORCE_RATE, is_force_rate);

  return MTLK_ERR_OK;
}

int
mtlk_mib_set_power_selection (struct nic *nic)
{
  mtlk_set_mib_value_uint8(mtlk_vap_get_txmm(nic->vap_handle),
                           MIB_USER_POWER_SELECTION,
                           MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_POWER_SELECTION));

  return MTLK_ERR_OK;
}

static int
mtlk_set_mib_values_ex (struct nic *nic, mtlk_txmm_msg_t* man_msg)
{
  mtlk_txmm_data_t* man_entry;
  UMI_MIB *psSetMib;
  mtlk_l2nat_t *l2nat = &nic->l2nat;
  mtlk_txmm_t *txmm = mtlk_vap_get_txmm(nic->vap_handle);
  mtlk_pdb_size_t size = 0;

  man_entry = mtlk_txmm_msg_get_empty_data(man_msg, txmm);
  if (!man_entry) {
    ELOG_V("Can't get MM data");
    return -ENOMEM;
  }

  ILOG2_V("Must do MIB's");
  // Check if TPC close loop is ON and we have calibrations in EEPROM for
  // selected frequency band.
  if (!mtlk_vap_is_slave_ap(nic->vap_handle)) {
    uint8 calibration = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CALIBRATION_ALGO_MASK);

    if (calibration & 0x80)
    {
        uint8 freq_band_cur = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CUR);
        if (mtlk_eeprom_is_band_supported(
                mtlk_core_get_eeprom(nic),
                freq_band_cur) != MTLK_ERR_OK)
        {
            mtlk_set_hw_state(nic, MTLK_HW_STATE_ERROR);
            ELOG_S("TPC close loop is ON and no calibrations for current frequency "
                 "(%s GHz) in EEPROM",
                 MTLK_HW_BAND_2_4_GHZ == freq_band_cur ? "2.4" :
                 MTLK_HW_BAND_5_2_GHZ == freq_band_cur ? "5" : "both");
        }
    }
    else
    { // TPC close loop is OFF. Check if TxPower is not zero.
        uint16 val = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_TX_POWER);
        ILOG1_D("TxPower = %u", val);
        if (!val)
        {
            MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_TX_POWER, DEFAULT_TX_POWER);
        }
    }
  }

  if (!mtlk_vap_is_slave_ap(nic->vap_handle)) {
    mtlk_mib_set_forced_rates(nic);
   
    if (MTLK_ERR_OK != 
        mtlk_set_mib_value_uint8 (txmm, MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED, 
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_PREAMBLE))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED);
      return -ENODEV;
    }
    
    /* Set supported antennas for transmission */
    psSetMib = (UMI_MIB*)man_entry->payload;
    memset(psSetMib, 0, sizeof(*psSetMib));
    size = MTLK_NUM_ANTENNAS_BUFSIZE;

    if (MTLK_ERR_OK != 
        MTLK_CORE_PDB_GET_BINARY(nic, PARAM_DB_CORE_TX_ANTENNAS, 
                                 psSetMib->uValue.au8ListOfu8.au8Elements, &size))
    {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_SUPPORTED_TX_ANTENNAS);
      return -ENODEV;
    }

    if (MTLK_ERR_OK != mtlk_set_mib_value_raw(txmm, MIB_SUPPORTED_TX_ANTENNAS, &psSetMib->uValue))
    {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_SUPPORTED_TX_ANTENNAS);
      return -ENODEV;
    }

    /* Set supported antennas for reception */
    psSetMib = (UMI_MIB*)man_entry->payload;
    memset(psSetMib, 0, sizeof(*psSetMib));
    size = MTLK_NUM_ANTENNAS_BUFSIZE;
    
    if (MTLK_ERR_OK != 
        MTLK_CORE_PDB_GET_BINARY(nic, PARAM_DB_CORE_RX_ANTENNAS, 
                                 psSetMib->uValue.au8ListOfu8.au8Elements, &size))
    {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_SUPPORTED_RX_ANTENNAS);
      return -ENODEV;
    }
    
    if (MTLK_ERR_OK != mtlk_set_mib_value_raw(txmm, MIB_SUPPORTED_RX_ANTENNAS, &psSetMib->uValue))
    {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_SUPPORTED_RX_ANTENNAS);
      return -ENODEV;
    }  

    /* set transmission power */
    if (MTLK_ERR_OK != 
        mtlk_set_mib_value_uint8 (txmm, MIB_TX_POWER, 
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_TX_POWER))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_TX_POWER);
      return -ENODEV;
    }

    if (MTLK_ERR_OK != 
        mtlk_set_mib_value_uint8 (txmm, MIB_USE_SHORT_CYCLIC_PREFIX, 
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_USE_SHORT_CYCLIC_PREFIX);
      return -ENODEV;
    }

    if (MTLK_ERR_OK != 
        mtlk_set_mib_value_uint8 (txmm, MIB_CALIBRATION_ALGO_MASK, 
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CALIBRATION_ALGO_MASK))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_CALIBRATION_ALGO_MASK);
      return -ENODEV;
    }

    if (MTLK_ERR_OK != 
        mtlk_set_mib_value_uint8 (txmm, MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G, 
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_SLOT_TIME))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G);
      return -ENODEV;
    }

    if (MTLK_ERR_OK != 
        mtlk_set_mib_value_uint8 (txmm, MIB_POWER_INCREASE_VS_DUTY_CYCLE, 
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_POWER_INCREASE))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_POWER_INCREASE_VS_DUTY_CYCLE);
      return -ENODEV;
    }

    if (MTLK_ERR_OK != 
        mtlk_set_mib_value_uint8 (txmm, MIB_SM_ENABLE, 
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SM_ENABLE))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_SM_ENABLE);
      return -ENODEV;
    }
  }
    
  if (MTLK_ERR_OK != mtlk_set_mib_acl(txmm, nic->slow_ctx->acl, nic->slow_ctx->acl_mask)) {
    ELOG_V("Failed to set MIB ACL");
    return -ENODEV;
  }

  if (mtlk_vap_is_master_ap(nic->vap_handle))
  {
    int i = 0;
    
    // set WME BSS parameters (relevant only on AP)
    man_entry->id           = UM_MAN_SET_MIB_REQ;
    man_entry->payload_size = sizeof(*psSetMib);

    psSetMib = (UMI_MIB*)man_entry->payload;

    memset(psSetMib, 0, sizeof(*psSetMib));

    psSetMib->u16ObjectID = cpu_to_le16(MIB_WME_PARAMETERS);

    for (i = 0; i < NTS_PRIORITIES; i++) {
      psSetMib->uValue.sWMEParameters.au8CWmin[i] = nic->slow_ctx->cfg.wme_bss.wme_class[i].cwmin;
      psSetMib->uValue.sWMEParameters.au16Cwmax[i] = cpu_to_le16(nic->slow_ctx->cfg.wme_bss.wme_class[i].cwmax);
      psSetMib->uValue.sWMEParameters.au8AIFS[i] = nic->slow_ctx->cfg.wme_bss.wme_class[i].aifsn;
      psSetMib->uValue.sWMEParameters.au16TXOPlimit[i] = cpu_to_le16(nic->slow_ctx->cfg.wme_bss.wme_class[i].txop / 32);
    }

    if (mtlk_txmm_msg_send_blocked(man_msg,
                                   MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out",
          le16_to_cpu(psSetMib->u16ObjectID));
      return -ENODEV;
    }

    if (psSetMib->u16Status == cpu_to_le16(UMI_OK)) {
      ILOG2_D("Successfully set MIB item 0x%04x.",
        le16_to_cpu(psSetMib->u16ObjectID));
    } else {
      ELOG_DD("Failed to set MIB item 0x%x, error %d.",
        le16_to_cpu(psSetMib->u16ObjectID), le16_to_cpu(psSetMib->u16Status));
    }

    // set WME AP parameters (relevant only on AP)
    man_entry->id           = UM_MAN_SET_MIB_REQ;
    man_entry->payload_size = sizeof(*psSetMib);

    psSetMib = (UMI_MIB*)man_entry->payload;

    memset(psSetMib, 0, sizeof(*psSetMib));

    psSetMib->u16ObjectID = cpu_to_le16(MIB_WME_QAP_PARAMETERS);

    for (i = 0; i < NTS_PRIORITIES; i++) {
      psSetMib->uValue.sWMEParameters.au8CWmin[i] = nic->slow_ctx->cfg.wme_ap.wme_class[i].cwmin;
      psSetMib->uValue.sWMEParameters.au16Cwmax[i] = cpu_to_le16(nic->slow_ctx->cfg.wme_ap.wme_class[i].cwmax);
      psSetMib->uValue.sWMEParameters.au8AIFS[i] = nic->slow_ctx->cfg.wme_ap.wme_class[i].aifsn;
      psSetMib->uValue.sWMEParameters.au16TXOPlimit[i] = cpu_to_le16(nic->slow_ctx->cfg.wme_ap.wme_class[i].txop / 32);
    }

    if (mtlk_txmm_msg_send_blocked(man_msg,
                                   MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out",
          le16_to_cpu(psSetMib->u16ObjectID));
      return -ENODEV;
    }

    if (psSetMib->u16Status == cpu_to_le16(UMI_OK)) {
      ILOG2_D("Successfully set MIB item 0x%04x.",
        le16_to_cpu(psSetMib->u16ObjectID));
    } else {
      ELOG_DD("Failed to set MIB item 0x%x, error %d",
        le16_to_cpu(psSetMib->u16ObjectID), le16_to_cpu(psSetMib->u16Status));
    }
  }

  // set driver features
  if (mtlk_vap_is_master_ap(nic->vap_handle)) {
    mtlk_set_mib_value_uint8(txmm, MIB_SPECTRUM_MODE, 
                             MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE));
  }

  l2nat->l2nat_aging_timeout = HZ * MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_L2NAT_AGING_TIMEOUT);
  ILOG1_D("l2nat_aging_timeout set to %u", l2nat->l2nat_aging_timeout);

  if (mtlk_vap_is_ap(nic->vap_handle)) {
    nic->slow_ctx->pm_params.u8NetworkMode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_NET_MODE_CUR);
    nic->slow_ctx->pm_params.u8SpectrumMode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE);
    nic->slow_ctx->pm_params.u32BSSbasicRateSet =
        get_basic_rate_set(
              MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_NET_MODE_CUR),
              MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_BASIC_RATE_SET));
    nic->slow_ctx->pm_params.u32OperationalRateSet =
        get_operate_rate_set(MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_NET_MODE_CFG));
    nic->slow_ctx->pm_params.u8ShortSlotTimeOptionEnabled11g = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_SLOT_TIME);
    nic->slow_ctx->pm_params.u8ShortPreambleOptionImplemented = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_PREAMBLE);
    nic->slow_ctx->pm_params.u8UpperLowerChannel = nic->slow_ctx->bonding;
  }

  /* this is requirement from MAC */
  MTLK_ASSERT(ASSOCIATE_FAILURE_TIMEOUT < CONNECT_TIMEOUT);
  mtlk_set_mib_value_uint32(txmm, MIB_ASSOCIATE_FAILURE_TIMEOUT, ASSOCIATE_FAILURE_TIMEOUT);

  ILOG2_V("End Mibs");
  return 0;
}

int
mtlk_set_mib_values(struct nic *nic)
{
  int             res       = -ENOMEM;
  mtlk_txmm_msg_t man_msg;

  if (mtlk_txmm_msg_init(&man_msg) == MTLK_ERR_OK) {
    res = mtlk_set_mib_values_ex(nic, &man_msg);
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  else {
    ELOG_V("Can't init TXMM msg");
  }

  return res;
}

void
mtlk_mib_update_pm_related_mibs (struct nic *nic,
                        mtlk_aux_pm_related_params_t *data)
{
  /* Update MIB DB */
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE,
                        (int)data->u8SpectrumMode);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SHORT_SLOT_TIME,
                        (int)data->u8ShortSlotTimeOptionEnabled11g);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SHORT_PREAMBLE, 
                        (int)data->u8ShortPreambleOptionImplemented);
  nic->slow_ctx->bonding = (int)data->u8UpperLowerChannel;
}

