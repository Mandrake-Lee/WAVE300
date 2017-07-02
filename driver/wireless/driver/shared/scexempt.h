/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * 20/40 coexistence feature
 * Provides transition between modes (20MHz->20/40MHz and vice versa)
 *
 * AP:
 * The scan exemption policy manager will deal with the requests by the associated stations to be exempt 
 * from the periodic OBSS scanning. 
 * Its operation will depend on the current value of a number of MIBs.
 *
 * STA:
 * The scan exemption request / response manager will generate scan exemption requests 
 * and process the respective responses from the associated AP
 *
 * This file relates to both (AP and STA)
 */
#ifndef __SCEXEMPT_H__
#define __SCEXEMPT_H__

#ifndef COEX_20_40_C
#error This file can only be included from one of the 20/40 coexistence implementation (.c) files
#endif

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/**************************************************************************************/
/****************************************  AP  ****************************************/
/**************************************************************************************/

struct _mtlk_20_40_coexistence_sm ;

typedef struct _mtlk_sta_exemption_descriptor
{
  IEEE_ADDR                       sta_addr;
  uint16                          padding;
  BOOL                            slot_used;
  BOOL                            supports_coexistence;
  BOOL                            exempt;
  BOOL                            intolerant;
  BOOL                            legacy;
} mtlk_sta_exemption_descriptor_t;


typedef struct _mtlk_scan_exemption_policy_manager
{
  struct _mtlk_20_40_coexistence_sm *parent_csm;
  mtlk_20_40_csm_xfaces_t           *xfaces;
  mtlk_sta_exemption_descriptor_t   *sta_exemption_descriptors;
  uint32                            descriptor_array_size;
}mtlk_scan_exemption_policy_manager_t;

int __MTLK_IFUNC mtlk_sepm_init (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  struct _mtlk_20_40_coexistence_sm *parent_csm, mtlk_20_40_csm_xfaces_t *xfaces, uint32 descriptor_array_size);

void __MTLK_IFUNC mtlk_sepm_cleanup (mtlk_scan_exemption_policy_manager_t *sepm_mgr);

void __MTLK_IFUNC mtlk_sepm_register_station (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr, BOOL supports_coexistence, BOOL exempt, BOOL intolerant, BOOL legacy);

void __MTLK_IFUNC mtlk_sepm_unregister_station (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr);

int __MTLK_IFUNC mtlk_sepm_process_exemption_request (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr);

BOOL __MTLK_IFUNC mtlk_sepm_register_station_intolerance (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr);

BOOL __MTLK_IFUNC mtlk_sepm_is_intolerant_or_legacy_station_connected (mtlk_scan_exemption_policy_manager_t *sepm_mgr);

/**************************************************************************************/
/****************************************  STA  ***************************************/
/**************************************************************************************/

typedef struct _mtlk_scan_reqresp_manager
{
  struct _mtlk_20_40_coexistence_sm *parent_csm;
  mtlk_20_40_csm_xfaces_t           *xfaces;
} mtlk_scan_reqresp_manager_t;

int __MTLK_IFUNC mtlk_srrm_init (mtlk_scan_reqresp_manager_t *srrm_mgr,
  struct _mtlk_20_40_coexistence_sm *parent_csm, mtlk_20_40_csm_xfaces_t *xfaces);
  
void __MTLK_IFUNC mtlk_srrm_cleanup (mtlk_scan_reqresp_manager_t *srrm_mgr);

int __MTLK_IFUNC mtlk_srrm_request_scan_exemption (mtlk_scan_reqresp_manager_t *srrm_mgr);

int __MTLK_IFUNC mtlk_srrm_process_response (mtlk_scan_reqresp_manager_t *srrm_mgr, BOOL exemption_granted);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif
