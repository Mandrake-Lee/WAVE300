/*
 * $Id: cbsmgr.c 11794 2011-10-23 12:54:42Z kashani $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * 20/40 coexistence feature
 * Provides transition between modes (20MHz->20/40MHz and visa versa)
 *
 * The CB switch manager will be used by the coexistence state machine for initiating 
 * and controlling the actual switch between the channel bonding modes: 20 -> 20/40 and 20/40 -> 20. 
 * The implementation will be based on building the appropriate requests and sending them down to the 
 * firmware.
 *
 */
#include "mtlkinc.h"

#define COEX_20_40_C
// This define is necessary for coex20_40priv.h file to compile successfully
#include "coex20_40priv.h"
#include "mtlk_param_db.h"
#include "mtlk_channels_propr.h"

#define LOG_LOCAL_GID   GID_COEX
#define LOG_LOCAL_FID   2


/* Initialization & cleanup */

int __MTLK_IFUNC mtlk_cbsm_init (mtlk_cb_switch_manager *cbsm,
  struct _mtlk_20_40_coexistence_sm *parent_csm_param, mtlk_20_40_csm_xfaces_t *xfaces)
{
  MTLK_ASSERT(cbsm != NULL);
  MTLK_ASSERT(parent_csm_param != NULL);

  cbsm->parent_csm = parent_csm_param;
  cbsm->xfaces = xfaces;

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC mtlk_cbsm_cleanup (mtlk_cb_switch_manager *cbsm)
{
  MTLK_ASSERT(cbsm != NULL);
  MTLK_ASSERT(cbsm->parent_csm != NULL);
}


/* External functional interfaces (meant only for the parent coexistence state machine) */

int __MTLK_IFUNC mtlk_cbsm_switch_to_20_mode (mtlk_cb_switch_manager *cbsm, int channel)
{
  int res = FALSE;
  FREQUENCY_ELEMENT freq_element;
  mtlk_get_channel_data_t channel_data;

  ILOG2_D("Requested channel = %d\n", channel);

  memset(&freq_element, 0, sizeof(freq_element));
  memset(&channel_data, 0, sizeof(channel_data));
  freq_element.u16Channel = (uint16)channel;
  freq_element.u8SwitchMode = UMI_CHANNEL_SW_MODE_SCN;
  (*cbsm->xfaces->switch_cb_mode_stage1)(cbsm->xfaces->context, &channel_data, &freq_element);
  mtlk_get_channel_data(&channel_data, &freq_element, NULL, NULL);
  freq_element.u8ChannelSwitchCount =  (uint8)DEFAULT_CHANNEL_SWITCH_COUNT;
  freq_element.u16Channel = HOST_TO_MAC16(freq_element.u16Channel);
  freq_element.u16ChannelAvailabilityCheckTime = HOST_TO_MAC16(freq_element.u16ChannelAvailabilityCheckTime);
  mtlk_fill_channel_params_by_tpc_by_vap(cbsm->xfaces->vap_handle, &freq_element);
  /* The function fills u8MaxTxPower and u8MaxTxPowerIndex */

  (*cbsm->xfaces->switch_cb_mode_stage2)(cbsm->xfaces->context, &freq_element);

  res = TRUE;

  return res;
}

int __MTLK_IFUNC mtlk_cbsm_switch_to_40_mode (mtlk_cb_switch_manager *cbsm, uint16 primary_channel,
  int secondary_channel_offset)
{
  int res = FALSE;
  FREQUENCY_ELEMENT freq_element;
  mtlk_get_channel_data_t channel_data;

  ILOG2_DD("Requested primary channel = %d, secondary channel offset = %d\n", primary_channel, secondary_channel_offset);

  memset(&freq_element, 0, sizeof(freq_element));
  memset(&channel_data, 0, sizeof(channel_data));
  freq_element.u16Channel = primary_channel;
  freq_element.u8SwitchMode = secondary_channel_offset;
  (*cbsm->xfaces->switch_cb_mode_stage1)(cbsm->xfaces->context, &channel_data, &freq_element);
  mtlk_get_channel_data(&channel_data, &freq_element, NULL, NULL);
  freq_element.u8ChannelSwitchCount =  (uint8)DEFAULT_CHANNEL_SWITCH_COUNT;
  freq_element.u16Channel = HOST_TO_MAC16(freq_element.u16Channel);
  freq_element.u16ChannelAvailabilityCheckTime = HOST_TO_MAC16(freq_element.u16ChannelAvailabilityCheckTime);
  mtlk_fill_channel_params_by_tpc_by_vap(cbsm->xfaces->vap_handle, &freq_element);
  /* The function fills u8MaxTxPower and u8MaxTxPowerIndex */
  (*cbsm->xfaces->switch_cb_mode_stage2)(cbsm->xfaces->context, &freq_element);
  res = TRUE;

  return res;
}
