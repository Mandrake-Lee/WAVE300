/*
 * $Id: mtlk_dfs.h 11422 2011-07-19 12:36:42Z andrii $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * EEPROM data processing module
 *
 * Originally written by Grygorii Strashko
 *
 */

#ifndef __MTLK_DFS_H__
#define __MTLK_DFS_H__

#include "aocs.h"
#include "txmm.h"
#include "dfs.h"

/**********************************************************************
 * Type definitions
***********************************************************************/
#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef struct _mtlk_dot11h_debug_params_t
{
  int16    debugNewChannel; /*for driver use*/
  int16    debugChannelAvailabilityCheckTime;
  int8     debugChannelSwitchCount;
  int8     debugSmRequired;
} __MTLK_IDATA mtlk_dot11h_debug_params_t;

struct _mtlk_dot11h_cfg_t
{
  uint16   u16Channel;
  uint16   u16ChannelAvailabilityCheckTime;
  uint8    u8ScanType;
  uint8    u8ChannelSwitchCount;
  uint8    u8SwitchMode;
  uint8    u8Bonding;
  uint8    u8IsHT;
  uint8    u8FrequencyBand;
  uint8    u8SpectrumMode;
  mtlk_dot11h_debug_params_t    debug_params;
} __MTLK_IDATA;

struct _mtlk_dot11h_t
{
  uint32                    init_mask;
  mtlk_dot11h_cfg_t         cfg;
  mtlk_dot11h_wrap_api_t    api;
  mtlk_vap_handle_t         vap_handle;
  uint16                    set_channel;
  uint8                     status;
  uint8                     data_stop; /*STA only, for NULL Packet stop*/
  mtlk_dfs_event_e          event; /*aocs.h to be included before dfs.h*/
  mtlk_osal_spinlock_t      lock;
  mtlk_osal_timer_t         timer;
  mtlk_txmm_msg_t           man_msg;
  mtlk_handle_t             flctrl_id;
  MTLK_DECLARE_INIT_STATUS;
} __MTLK_IDATA;

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

/**********************************************************************
 * function declaration
***********************************************************************/
void __MTLK_IFUNC mtlk_dot11h_initiate_channel_switch(mtlk_dot11h_t *obj,
  mtlk_aocs_evt_select_t *switch_data, BOOL is_aocs_switch);


#endif /* __MTLK_DFS_H__ */
