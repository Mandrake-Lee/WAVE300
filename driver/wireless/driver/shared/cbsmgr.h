/*
 * $Id$
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
#ifndef __CBSMGR_H__
#define __CBSMGR_H__

#ifndef COEX_20_40_C
#error This file can only be included from one of the 20/40 coexistence implementation (.c) files
#endif

#define DEFAULT_CHANNEL_SWITCH_COUNT 6

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

struct _mtlk_20_40_coexistence_sm;

typedef struct _mtlk_cb_switch_manager
{
  struct _mtlk_20_40_coexistence_sm   *parent_csm;
  mtlk_20_40_csm_xfaces_t             *xfaces;
}mtlk_cb_switch_manager;


int __MTLK_IFUNC mtlk_cbsm_init (mtlk_cb_switch_manager *cbsm,
  struct _mtlk_20_40_coexistence_sm *parent_csm, mtlk_20_40_csm_xfaces_t *xfaces);

void __MTLK_IFUNC mtlk_cbsm_cleanup (mtlk_cb_switch_manager *cbsm);

int __MTLK_IFUNC mtlk_cbsm_switch_to_20_mode (mtlk_cb_switch_manager *cbsm, int channel);

int __MTLK_IFUNC mtlk_cbsm_switch_to_40_mode (mtlk_cb_switch_manager *cbsm, uint16 primary_channel,
  int secondary_channel_offset);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif
