#include "mtlkinc.h"

#include "mtlkmib.h"
#include "mtlkaux.h"
#include "eeprom.h"
#include "channels.h"

#define LOG_LOCAL_GID   GID_MIBS
#define LOG_LOCAL_FID   2

static uint16 _vap_specific_mibs[] = {
  MIB_ACL_MODE,
  MIB_AUTHENTICATION_PREFERENCE,
  MIB_WEP_DEFAULT_KEYID,
  MIB_ACL,
  MIB_ACL_MASKS,
  MIB_RSN_CONTROL,
  MIB_DTIM_PERIOD,
  MIB_WEP_DEFAULT_KEYS,
  MIB_WEP_KEY_MAPPINGS,
  MIB_WEP_KEY_MAPPING_LENGTH,
  MIB_ASSOCIATE_FAILURE_TIMEOUT,
  MIB_EXCLUDE_UNENCRYPTED,
  MIB_IEEE_ADDRESS,
  MIB_PRIVACY_INVOKED
};

int __MTLK_IFUNC
mtlk_set_mib_value_uint8 (mtlk_txmm_t *txmm, uint16 u16ObjectID, uint8 value)
{
  MIB_VALUE uValue;
  memset(&uValue, 0, sizeof(MIB_VALUE));
  uValue.u8Uint8 = value;
  return mtlk_set_mib_value_raw(txmm, u16ObjectID, &uValue);
}

int __MTLK_IFUNC
mtlk_get_mib_value_uint8 (mtlk_txmm_t *txmm, uint16 u16ObjectID, uint8 *value)
{
  MIB_VALUE uValue;
  int res;
  res = mtlk_get_mib_value_raw(txmm, u16ObjectID, &uValue);
  *value = uValue.u8Uint8;
  return res;
}

int __MTLK_IFUNC
mtlk_set_mib_value_uint16 (mtlk_txmm_t *txmm, uint16 u16ObjectID, uint16 value)
{
  MIB_VALUE uValue;
  memset(&uValue, 0, sizeof(MIB_VALUE));
  uValue.u16Uint16 = HOST_TO_MAC16(value);
  return mtlk_set_mib_value_raw(txmm, u16ObjectID, &uValue);
}

int __MTLK_IFUNC
mtlk_get_mib_value_uint16 (mtlk_txmm_t *txmm, uint16 u16ObjectID, uint16 *value)
{
  MIB_VALUE uValue;
  int res;
  res = mtlk_get_mib_value_raw(txmm, u16ObjectID, &uValue);
  *value = MAC_TO_HOST16(uValue.u16Uint16);
  return res;
}

int __MTLK_IFUNC
mtlk_set_mib_value_uint32 (mtlk_txmm_t *txmm, uint16 u16ObjectID, uint32 value)
{
  MIB_VALUE uValue;
  memset(&uValue, 0, sizeof(MIB_VALUE));
  uValue.u32Uint32 = HOST_TO_MAC32(value);
  return mtlk_set_mib_value_raw(txmm, u16ObjectID, &uValue);
}

int __MTLK_IFUNC
mtlk_get_mib_value_uint32 (mtlk_txmm_t *txmm, uint16 u16ObjectID, uint32 *value)
{
  MIB_VALUE uValue;
  int res;
  res = mtlk_get_mib_value_raw(txmm, u16ObjectID, &uValue);
  *value = MAC_TO_HOST32(uValue.u32Uint32);
  return res;
}

int __MTLK_IFUNC
mtlk_set_mib_rsn (mtlk_txmm_t *txmm, uint8 value)
{
  MIB_VALUE uValue;
  memset(&uValue, 0, sizeof(MIB_VALUE));
  uValue.sRsnControl.u8IsRsnEnabled = (uint8)!!value;
  uValue.sRsnControl.u8IsTsnEnabled = 0;
  ILOG1_D("RSN enabled is set to %u", value);
  return mtlk_set_mib_value_raw(txmm, MIB_RSN_CONTROL, &uValue);
}

static int access_mib_value (mtlk_txmm_t *txmm, uint16 u16ObjectID,
  uint16 direction, MIB_VALUE *uValue);

BOOL __MTLK_IFUNC
mtlk_mib_request_is_allowed (mtlk_vap_handle_t vap_handle, uint16 u16mib_id)
{
  signed char scIndex;
  BOOL bResult = TRUE;

  if (!mtlk_vap_is_master(vap_handle)) {
    scIndex = ARRAY_SIZE(_vap_specific_mibs) - 1;
    while ((scIndex >= 0) && (_vap_specific_mibs[scIndex] != u16mib_id)) {
      scIndex --;
    }

    if (scIndex == -1) {
      bResult = FALSE;
      ILOG5_DD("Query for non-Slave Mib=%d on slave ap %d", 
        u16mib_id, mtlk_vap_get_id(vap_handle));
    }
  }
  return bResult;
}

int __MTLK_IFUNC
mtlk_set_mib_value_raw (mtlk_txmm_t *txmm,
                        uint16 u16ObjectID, 
                        MIB_VALUE *uValue)
{
  if (!mtlk_mib_request_is_allowed (txmm->vap_handle, u16ObjectID)) {
    MTLK_ASSERT(0);
    return MTLK_ERR_NOT_SUPPORTED;
  }
  return access_mib_value(txmm, u16ObjectID, UM_MAN_SET_MIB_REQ, uValue);
}

int __MTLK_IFUNC
mtlk_get_mib_value_raw (mtlk_txmm_t *txmm,
                        uint16 u16ObjectID,
                        MIB_VALUE *uValue)
{
  if (!mtlk_mib_request_is_allowed (txmm->vap_handle, u16ObjectID)) {
     return MTLK_ERR_NOT_SUPPORTED;
  }
  return access_mib_value(txmm, u16ObjectID, UM_MAN_GET_MIB_REQ, uValue);
}

static int
access_mib_value (mtlk_txmm_t *txmm,
                  uint16 u16ObjectID,
                  uint16 direction,
                  MIB_VALUE *uValue)
{
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_MIB *psSetMib;
  int res = MTLK_ERR_OK; /* Equal to UMI_OK */

  ASSERT(txmm);
  ASSERT(uValue);
  ASSERT((direction == UM_MAN_SET_MIB_REQ) ||
         (direction == UM_MAN_GET_MIB_REQ));

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, txmm, NULL);
  if (!man_entry) {
    ELOG_V("Can't access MIB value due to lack of MAN_MSG");
    res = MTLK_ERR_NO_RESOURCES;
    goto end;
  }
  man_entry->id = direction;
  man_entry->payload_size = sizeof(UMI_MIB);
  psSetMib = (UMI_MIB*)man_entry->payload;
  psSetMib->u16ObjectID = HOST_TO_MAC16(u16ObjectID);

  if (direction == UM_MAN_SET_MIB_REQ)
    memcpy(&psSetMib->uValue, uValue, sizeof(MIB_VALUE));

  res = mtlk_txmm_msg_send_blocked(&man_msg, 5000);
  if (res != MTLK_ERR_OK) goto end;

  if (direction == UM_MAN_GET_MIB_REQ)
    memcpy(uValue, &psSetMib->uValue, sizeof(MIB_VALUE));

  res = MAC_TO_HOST16(psSetMib->u16Status);
#ifdef MTCFG_DEBUG
  {
    char *s;
    if (direction == UM_MAN_SET_MIB_REQ)
      s = "UM_MAN_SET_MIB_REQ";
    else
      s = "UM_MAN_GET_MIB_REQ";

    if (res == UMI_OK)
      ILOG2_SD("Successfull %s, u16ObjectID = 0x%04x", s, u16ObjectID);
    else
      ILOG2_SDD("%s failed, u16ObjectID = 0x%04x, status = 0x%04x",
           s, u16ObjectID, res);
  }
#endif

end:
  if (man_entry) mtlk_txmm_msg_cleanup(&man_msg);
  return res;
}

#define AUX_PREACTIVATION_SET_TIMEOUT 2000

static int 
aux_pm_related_set_to_mac_blocked (mtlk_txmm_t                  *txmm,
                                   uint8                         net_mode,
                                   uint32                        operational_rate_set,
                                   uint32                        basic_rate_set,
                                   uint8                         upper_lower,
                                   uint8                         spectrum,
                                   mtlk_aux_pm_related_params_t *result)
{
  int                    res       = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t        man_msg;
  mtlk_txmm_data_t      *man_entry = NULL;
  UMI_MIB               *psSetMib  = NULL;
  PRE_ACTIVATE_MIB_TYPE *data      = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, txmm, NULL);
  if (!man_entry) {
    ELOG_V("No MM slot: failed to set pre-activation MIB");
    res = MTLK_ERR_NO_RESOURCES;
    goto FINISH;
  }
  
  man_entry->id           = UM_MAN_SET_MIB_REQ;
  man_entry->payload_size = sizeof(*psSetMib);

  psSetMib = (UMI_MIB*)man_entry->payload;

  memset(psSetMib, 0, sizeof(*psSetMib));

  psSetMib->u16ObjectID = HOST_TO_MAC16(MIB_PRE_ACTIVATE);

  if (!result) {
    result = &psSetMib->uValue.sPreActivateType;
  } else {
    memset(result, 0, sizeof(*result));
  }

  data = &psSetMib->uValue.sPreActivateType;

  result->u32OperationalRateSet = operational_rate_set;
  result->u32BSSbasicRateSet    = basic_rate_set;
  result->u8NetworkMode         = net_mode;
  result->u8UpperLowerChannel   = upper_lower;
  result->u8SpectrumMode        = spectrum;

  /* force the STA to be legacy regardless of the AP type */
  if (!is_ht_net_mode(net_mode)) {
    result->u32OperationalRateSet &= LM_PHY_11A_RATE_MSK | LM_PHY_11B_RATE_MSK;
    result->u8UpperLowerChannel    = 0;
    result->u8SpectrumMode         = SPECTRUM_20MHZ;
  }

  /* Extract basic rate and preamble from 2.4 band */
  if (net_mode_to_band(net_mode) == MTLK_HW_BAND_2_4_GHZ) {
    if (result->u32OperationalRateSet & (~LM_PHY_11B_RATE_MSK)) {
    /* Not 802.11b, it's either g or n*/
        result->u8ShortSlotTimeOptionEnabled11g  = 1;
    }
    result->u8ShortPreambleOptionImplemented = 1;
  }

  /* XXX: Workaround for MAC bug */
  mtlk_set_mib_value_uint32(txmm, MIB_BSS_BASIC_RATE_SET, result->u32BSSbasicRateSet);
  mtlk_set_mib_value_uint32(txmm, MIB_OPERATIONAL_RATE_SET, result->u32OperationalRateSet);

  data->u8NetworkMode                    = result->u8NetworkMode;
  data->u8UpperLowerChannel              = result->u8UpperLowerChannel;
  data->u8SpectrumMode                   = result->u8SpectrumMode;
  data->u8ShortSlotTimeOptionEnabled11g  = result->u8ShortSlotTimeOptionEnabled11g;
  data->u8ShortPreambleOptionImplemented = result->u8ShortPreambleOptionImplemented;
  data->u32OperationalRateSet            = HOST_TO_MAC32(result->u32OperationalRateSet);
  data->u32BSSbasicRateSet               = HOST_TO_MAC32(result->u32BSSbasicRateSet);

  ILOG2_DDDDDDD("SENDING PREACTIVATION MIB:\n"
       "result->u8NetworkMode                       = %d\n"
       "result->u8UpperLowerChannel                 = %d\n"
       "result->u8SpectrumMode                      = %d\n"
       "result->u8ShortSlotTimeOptionEnabled11g     = %d\n"
       "result->u8ShortPreambleOptionImplemented    = %d\n"
       "result->u32OperationalRateSet               = %X\n"
       "result->u32BSSbasicRateSet                  = %X",
      result->u8NetworkMode,
      result->u8UpperLowerChannel,
      result->u8SpectrumMode,
      result->u8ShortSlotTimeOptionEnabled11g,
      result->u8ShortPreambleOptionImplemented,
      result->u32OperationalRateSet,
      result->u32BSSbasicRateSet);

  res = mtlk_txmm_msg_send_blocked(&man_msg, AUX_PREACTIVATION_SET_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Failed to set Pre-activation MIB (Err#%d)", res);
    goto FINISH;
  } else {  
    res = MTLK_ERR_UNKNOWN;
  }
          
  if (psSetMib->u16Status != HOST_TO_MAC16(UMI_OK)) {
    ELOG_V("Setting Pre-activation MIB status Failed");
    goto FINISH;
  }

  res = MTLK_ERR_OK;

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

int __MTLK_IFUNC 
mtlk_aux_pm_related_params_set_defaults (mtlk_txmm_t                  *txmm,
                                         uint8                        net_mode,
                                         uint8                        spectrum,
                                         mtlk_aux_pm_related_params_t *result)
{
  return aux_pm_related_set_to_mac_blocked(txmm,
           net_mode,
           get_operate_rate_set(net_mode),
           get_basic_rate_set(net_mode, CFG_BASIC_RATE_SET_DEFAULT),
           ALTERNATE_UPPER,
           spectrum,
           result);
}

int __MTLK_IFUNC 
mtlk_aux_pm_related_params_set_bss_based(mtlk_txmm_t                  *txmm, 
                                         bss_data_t                   *bss_data, 
                                         uint8                         net_mode,
                                         uint8                         spectrum,
                                         mtlk_aux_pm_related_params_t *result)
{
  return aux_pm_related_set_to_mac_blocked(txmm,
                                           net_mode,
                                           bss_data->operational_rate_set,
                                           bss_data->basic_rate_set,
                                           bss_data->upper_lower,
                                           spectrum,
                                           result);
}

int __MTLK_IFUNC 
mtlk_set_pm_related_params (mtlk_txmm_t *txmm, mtlk_aux_pm_related_params_t *params)
{
  int                    res       = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t        man_msg;
  mtlk_txmm_data_t      *man_entry = NULL;
  UMI_MIB               *psSetMib  = NULL;
  PRE_ACTIVATE_MIB_TYPE *data      = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, txmm, NULL);
  if (!man_entry) {
    ELOG_V("No MM slot: failed to set pre-activation MIB");
    res = MTLK_ERR_NO_RESOURCES;
    goto FINISH;
  }
  
  man_entry->id           = UM_MAN_SET_MIB_REQ;
  man_entry->payload_size = sizeof(*psSetMib);

  psSetMib = (UMI_MIB*)man_entry->payload;

  memset(psSetMib, 0, sizeof(*psSetMib));

  psSetMib->u16ObjectID = HOST_TO_MAC16(MIB_PRE_ACTIVATE);

  data = &psSetMib->uValue.sPreActivateType;

  /* XXX: Workaround for MAC bug */
  mtlk_set_mib_value_uint32(txmm, MIB_BSS_BASIC_RATE_SET, params->u32BSSbasicRateSet);
  mtlk_set_mib_value_uint32(txmm, MIB_OPERATIONAL_RATE_SET, params->u32OperationalRateSet);

  data->u8NetworkMode                    = params->u8NetworkMode;
  data->u8UpperLowerChannel              = params->u8UpperLowerChannel;
  data->u8SpectrumMode                   = params->u8SpectrumMode;
  data->u8ShortSlotTimeOptionEnabled11g  = params->u8ShortSlotTimeOptionEnabled11g;
  data->u8ShortPreambleOptionImplemented = params->u8ShortPreambleOptionImplemented;
  data->u32OperationalRateSet            = HOST_TO_MAC32(params->u32OperationalRateSet);
  data->u32BSSbasicRateSet               = HOST_TO_MAC32(params->u32BSSbasicRateSet);

  ILOG2_DDDDDDD("SENDING PREACTIVATION MIB:\n"
       "u8NetworkMode                       = %d\n"
       "u8UpperLowerChannel                 = %d\n"
       "u8SpectrumMode                      = %d\n"
       "u8ShortSlotTimeOptionEnabled11g     = %d\n"
       "u8ShortPreambleOptionImplemented    = %d\n"
       "u32OperationalRateSet               = %X\n"
       "u32BSSbasicRateSet                  = %X",
      data->u8NetworkMode,
      data->u8UpperLowerChannel,
      data->u8SpectrumMode,
      data->u8ShortSlotTimeOptionEnabled11g,
      data->u8ShortPreambleOptionImplemented,
      data->u32OperationalRateSet,
      data->u32BSSbasicRateSet);

  res = mtlk_txmm_msg_send_blocked(&man_msg, AUX_PREACTIVATION_SET_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Failed to set Pre-activation MIB (Err#%d)", res);
    goto FINISH;
  } else {  
    res = MTLK_ERR_UNKNOWN;
  }
          
  if (psSetMib->u16Status != HOST_TO_MAC16(UMI_OK)) {
    ELOG_V("Setting Pre-activation MIB status Failed");
    goto FINISH;
  }

  res = MTLK_ERR_OK;

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

int __MTLK_IFUNC
mtlk_set_mib_acl (mtlk_txmm_t *txmm, IEEE_ADDR *list, IEEE_ADDR *mask_list)
{
  int res = MTLK_ERR_OK;
  int i, j;
  MIB_VALUE mib;

  /*set MIB_ACL_TYPE_MASKS in MAC*/
  memset(&mib, 0, sizeof(mib));
  mib.sACLmasks.Network_Index = 0;
  for (i = 0, j = 0; i < MAX_ADDRESSES_IN_ACL; i++) {
    if (mtlk_osal_is_zero_address(list[i].au8Addr))
      continue;
    mib.sACLmasks.aACL[j] = mask_list[i];
    j++;
  }

  mib.sACLmasks.Num_Of_Entries = j;
  res = mtlk_set_mib_value_raw(txmm, MIB_ACL_MASKS, &mib);
  if (MTLK_ERR_OK != res) {
    goto finish;
  }

  /*set MIB_ACL_TYPE in MAC*/
  memset(&mib, 0, sizeof(mib));

  mib.sACL.Network_Index = 0;
  for (i = 0, j = 0; i < MAX_ADDRESSES_IN_ACL; i++) {
    if (mtlk_osal_is_zero_address(list[i].au8Addr))
      continue;
    mib.sACL.aACL[j] = list[i];
    j++;
  }

  mib.sACL.Num_Of_Entries = j;
  res = mtlk_set_mib_value_raw(txmm, MIB_ACL, &mib);

finish:
  return res;
}

int __MTLK_IFUNC
mtlk_set_mib_acl_mode (mtlk_txmm_t *txmm, uint16 u16ObjectID, uint32 value)
{
  MIB_VALUE m;
  int res;

  memset(&m, 0, sizeof(m));
  m.sAclMode.u8ACLMode = value;
  res = mtlk_set_mib_value_raw(txmm, MIB_ACL_MODE, &m);
  if (res != MTLK_ERR_OK) {
    ELOG_V("Updating MIB_ACL_MODE failed");
  }
  return res;
}

int __MTLK_IFUNC
mtlk_get_mib_acl_mode (mtlk_txmm_t *txmm, uint16 u16ObjectID, uint32 *value)
{
  MIB_VALUE uValue;
  int res;

  res = mtlk_get_mib_value_raw(txmm, u16ObjectID, &uValue);
  if (res != MTLK_ERR_OK) {
    ELOG_V("Getting MIB_ACL_MODE failed");
  } else {
    *value = uValue.sAclMode.u8ACLMode;
  }
  return res;
}
