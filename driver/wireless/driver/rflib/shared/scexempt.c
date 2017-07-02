/*
 * $Id: scexempt.c 11794 2011-10-23 12:54:42Z kashani $
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
#include "mtlkinc.h"

#define COEX_20_40_C
// This define is necessary for coex20_40priv.h file to compile successfully
#include "coex20_40priv.h"

#define LOG_LOCAL_GID   GID_COEX
#define LOG_LOCAL_FID   3

/**************************************************************************************/
/****************************************  AP  ****************************************/
/**************************************************************************************/

static mtlk_sta_exemption_descriptor_t *_mtlk_sepm_find_empty_slot(mtlk_scan_exemption_policy_manager_t *sepm_mgr);
static mtlk_sta_exemption_descriptor_t *_mtlk_sepm_find_first_non_exempted_station_slot(mtlk_scan_exemption_policy_manager_t *sepm_mgr);
static mtlk_sta_exemption_descriptor_t *_mtlk_sepm_find_slot_by_addr(mtlk_scan_exemption_policy_manager_t *sepm_mgr, const IEEE_ADDR *addr);
static uint32 _mtlk_sepm_count_non_exempted_stations(mtlk_scan_exemption_policy_manager_t *sepm_mgr, const IEEE_ADDR *exception);
static uint32 _mtlk_sepm_count_legacy_stations(mtlk_scan_exemption_policy_manager_t *sepm_mgr);
static uint32 _mtlk_sepm_count_intolerant_stations(mtlk_scan_exemption_policy_manager_t *sepm_mgr);

/* Initialization & cleanup */

int __MTLK_IFUNC mtlk_sepm_init (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  struct _mtlk_20_40_coexistence_sm *parent_csm, mtlk_20_40_csm_xfaces_t *xfaces, uint32 descriptor_array_size)
{
  int res = MTLK_ERR_NO_RESOURCES;

  MTLK_ASSERT(sepm_mgr != NULL);
  MTLK_ASSERT(parent_csm != NULL);
  MTLK_ASSERT(descriptor_array_size > 0);

  sepm_mgr->parent_csm = parent_csm;
  sepm_mgr->xfaces = xfaces;
  sepm_mgr->sta_exemption_descriptors = mtlk_osal_mem_alloc(descriptor_array_size * sizeof(mtlk_sta_exemption_descriptor_t), MTLK_MEM_TAG_COEX_20_40);
  if (sepm_mgr->sta_exemption_descriptors != NULL)
  {
    sepm_mgr->descriptor_array_size = descriptor_array_size;
    memset(sepm_mgr->sta_exemption_descriptors, 0, descriptor_array_size * sizeof(mtlk_sta_exemption_descriptor_t));
    res = MTLK_ERR_OK;
  }
  else
  {
    ELOG_V("ERROR: sta_exemption_descriptors memory allocation failed!");
  }

  return res;
}

void __MTLK_IFUNC mtlk_sepm_cleanup (mtlk_scan_exemption_policy_manager_t *sepm_mgr)
{
  MTLK_ASSERT(sepm_mgr != NULL);
  MTLK_ASSERT(sepm_mgr->parent_csm != NULL);

  if (sepm_mgr->sta_exemption_descriptors)
  {
    mtlk_osal_mem_free(sepm_mgr->sta_exemption_descriptors);
  }
}


/* External functional interfaces (meant only for the parent coexistence state machine) */

void __MTLK_IFUNC mtlk_sepm_register_station (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr, BOOL supports_coexistence, BOOL exempt, BOOL intolerant, BOOL legacy)
{
  mtlk_sta_exemption_descriptor_t *new_station_slot;

  MTLK_ASSERT(sepm_mgr != NULL);
  MTLK_ASSERT(sta_addr != NULL);

  if (!_mtlk_sepm_find_slot_by_addr(sepm_mgr, sta_addr))
  {
    new_station_slot = _mtlk_sepm_find_empty_slot(sepm_mgr);
    if (new_station_slot)
    {
      new_station_slot->supports_coexistence = supports_coexistence;
      new_station_slot->exempt = exempt;
      new_station_slot->intolerant = intolerant;
      new_station_slot->legacy = legacy;
      memcpy(new_station_slot->sta_addr.au8Addr, sta_addr, sizeof(new_station_slot->sta_addr.au8Addr));
      new_station_slot->slot_used = TRUE;
      if ((new_station_slot->legacy == TRUE) || (new_station_slot->intolerant))
      {
        mtlk_20_40_ap_notify_intolerant_or_legacy_station_connected(sepm_mgr->parent_csm, TRUE /* don't lock, the lock is already acquired by the caller */);
      }
    }
  }
}

void __MTLK_IFUNC mtlk_sepm_unregister_station (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr)
{
  mtlk_sta_exemption_descriptor_t *unregistered_station_slot;
  mtlk_sta_exemption_descriptor_t *exempted_station_slot;
  mtlk_20_40_coexistence_element coex_el;

  MTLK_ASSERT(sepm_mgr != NULL);
  MTLK_ASSERT(sta_addr != NULL);

  unregistered_station_slot = _mtlk_sepm_find_slot_by_addr(sepm_mgr, sta_addr);
  if (unregistered_station_slot)
  {
    unregistered_station_slot->slot_used = FALSE;
    if (unregistered_station_slot->supports_coexistence)
    {
      if (_mtlk_sepm_count_non_exempted_stations(sepm_mgr, NULL) == 0)
      { /* No scanning stations remain, we will look for an exempted station that can be requested to start
           periodic OBSS scanning */
        exempted_station_slot = _mtlk_sepm_find_first_non_exempted_station_slot(sepm_mgr);
        if (exempted_station_slot)
        {
          memset(&coex_el, 0, sizeof(coex_el));
          coex_el.u8OBSSScanningExemptionGrant = FALSE;
          if (mtlk_cefg_send_coexistence_frame(&sepm_mgr->parent_csm->frgen,
                                               &exempted_station_slot->sta_addr,
                                               &coex_el,
                                               NULL) == MTLK_ERR_OK)
         {
            exempted_station_slot->exempt = FALSE;
            mtlk_20_40_set_intolerance_at_first_scan_flag(sepm_mgr->parent_csm, FALSE, TRUE /* don't lock, already locked */);
            mtlk_coex_20_40_inc_cnt(sepm_mgr->parent_csm, MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED);
          }
        }
      }
    }
    if (unregistered_station_slot->legacy || unregistered_station_slot->intolerant)
    {
      if ((_mtlk_sepm_count_legacy_stations(sepm_mgr) == 0) && (_mtlk_sepm_count_intolerant_stations(sepm_mgr) == 0))
      {
        mtlk_20_40_ap_notify_last_40_incapable_station_disconnected(sepm_mgr->parent_csm, TRUE /* don't lock, the lock is already acquired by the caller */);
      }
    }
    memset(unregistered_station_slot->sta_addr.au8Addr, 0, sizeof(unregistered_station_slot->sta_addr.au8Addr));
    unregistered_station_slot->exempt = FALSE;
    unregistered_station_slot->intolerant = FALSE;
    unregistered_station_slot->legacy = FALSE;
  }
}

int __MTLK_IFUNC mtlk_sepm_process_exemption_request (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr)
{
  int ret_val = MTLK_ERR_OK;
  mtlk_sta_exemption_descriptor_t *station_slot;
  mtlk_20_40_coexistence_element coex_el;

  MTLK_ASSERT(sepm_mgr != NULL);
  MTLK_ASSERT(sta_addr != NULL);
  
  mtlk_coex_20_40_inc_cnt(sepm_mgr->parent_csm, MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_REQUESTED);

  station_slot = _mtlk_sepm_find_slot_by_addr(sepm_mgr, sta_addr);
  if (station_slot)
  {
    if (_mtlk_sepm_count_non_exempted_stations(sepm_mgr, sta_addr) > 0)
    { /* Other non-exempt stations exist, we can grant the station's request for exemption
         NOTE: we're not going to check whether the station is currently considered to be exempt
         or not; if it requests an exemption, we will grant one and send the respective coexistence
         frame whenever possible */
      memset(&coex_el, 0, sizeof(coex_el));
      coex_el.u8OBSSScanningExemptionGrant = TRUE;
      ret_val = mtlk_cefg_send_coexistence_frame (&sepm_mgr->parent_csm->frgen,
                                                  sta_addr,
                                                  &coex_el,
                                                  NULL);
      if (ret_val == MTLK_ERR_OK)
      {
        station_slot->exempt = TRUE;
        mtlk_coex_20_40_inc_cnt(sepm_mgr->parent_csm, MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_GRANTED);
      }
    }
  }

  return ret_val;
}

BOOL __MTLK_IFUNC mtlk_sepm_register_station_intolerance (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr)
{
  BOOL changed = FALSE;
  mtlk_sta_exemption_descriptor_t *station_slot;

  MTLK_ASSERT(sepm_mgr != NULL);
  MTLK_ASSERT(sta_addr != NULL);

  station_slot = _mtlk_sepm_find_slot_by_addr(sepm_mgr, sta_addr);
  if (station_slot)
  {
    if (station_slot->intolerant == FALSE)
    {
      station_slot->intolerant = TRUE;
      changed = TRUE;
    }
  }

  return changed;
}

BOOL __MTLK_IFUNC mtlk_sepm_is_intolerant_or_legacy_station_connected(mtlk_scan_exemption_policy_manager_t *sepm_mgr)
{
  BOOL res = FALSE;
  uint32 i;

  MTLK_ASSERT(sepm_mgr != NULL);

  for (i = 0; i < sepm_mgr->descriptor_array_size; i ++)
  {
    if (sepm_mgr->sta_exemption_descriptors[i].slot_used == TRUE)
    {
      if ((sepm_mgr->sta_exemption_descriptors[i].intolerant == TRUE) || (sepm_mgr->sta_exemption_descriptors[i].legacy == TRUE))
      {
        res = TRUE;
        break;
      }
    }
  }

  return res;
}

/* Internal functions */
static mtlk_sta_exemption_descriptor_t *_mtlk_sepm_find_empty_slot(mtlk_scan_exemption_policy_manager_t *sepm_mgr)
{
  uint32 i;
  mtlk_sta_exemption_descriptor_t *ret_val = NULL;

  MTLK_ASSERT(sepm_mgr != NULL);

  for (i = 0; i < sepm_mgr->descriptor_array_size; i ++)
  {
    if (sepm_mgr->sta_exemption_descriptors[i].slot_used == FALSE)
    {
      ret_val = &sepm_mgr->sta_exemption_descriptors[i];
      break;
    }
  }

  return ret_val;
}

static mtlk_sta_exemption_descriptor_t *_mtlk_sepm_find_first_non_exempted_station_slot(mtlk_scan_exemption_policy_manager_t *sepm_mgr)
{
  uint32 i;
  mtlk_sta_exemption_descriptor_t *ret_val = NULL;

  MTLK_ASSERT(sepm_mgr != NULL);

  for (i = 0; i < sepm_mgr->descriptor_array_size; i ++)
  {
    if ((sepm_mgr->sta_exemption_descriptors[i].slot_used == TRUE) &&
        (sepm_mgr->sta_exemption_descriptors[i].supports_coexistence == TRUE) &&
        (sepm_mgr->sta_exemption_descriptors[i].exempt == FALSE))
    {
      ret_val = &sepm_mgr->sta_exemption_descriptors[i];
      break;
    }
  }

  return ret_val;
}

static mtlk_sta_exemption_descriptor_t *_mtlk_sepm_find_slot_by_addr(mtlk_scan_exemption_policy_manager_t *sepm_mgr, const IEEE_ADDR *addr)
{
  uint32 i;
  mtlk_sta_exemption_descriptor_t *ret_val = NULL;

  MTLK_ASSERT(sepm_mgr != NULL);
  MTLK_ASSERT(addr != NULL);

  for (i = 0; i < sepm_mgr->descriptor_array_size; i ++)
  {
    if (sepm_mgr->sta_exemption_descriptors[i].slot_used == TRUE)
    {
      if (!memcmp(sepm_mgr->sta_exemption_descriptors[i].sta_addr.au8Addr, addr->au8Addr, sizeof(sepm_mgr->sta_exemption_descriptors[i].sta_addr.au8Addr)))
      {
        ret_val = &sepm_mgr->sta_exemption_descriptors[i];
        break;
      }
    }
  }

  return ret_val;
}

static uint32 _mtlk_sepm_count_non_exempted_stations(mtlk_scan_exemption_policy_manager_t *sepm_mgr, const IEEE_ADDR *exception)
{
  uint32 i;
  uint32 ret_val = 0;

  MTLK_ASSERT(sepm_mgr != NULL);

  for (i = 0; i < sepm_mgr->descriptor_array_size; i ++)
  {
    if ((sepm_mgr->sta_exemption_descriptors[i].slot_used == TRUE) &&
        (sepm_mgr->sta_exemption_descriptors[i].supports_coexistence == TRUE))
    {
      if (sepm_mgr->sta_exemption_descriptors[i].exempt == FALSE)
      {
        if (exception)
        { /* We've been instructed to count all stations with one exception, so we first have to check whether the current
             station is the exception */
          if (memcmp(sepm_mgr->sta_exemption_descriptors[i].sta_addr.au8Addr, exception->au8Addr, sizeof(sepm_mgr->sta_exemption_descriptors[i].sta_addr.au8Addr)))
          {
            ret_val ++;
          }
        }
        else
        { /* We've been instructed to count all stations without exception */
          ret_val ++;
        }
      }
    }
  }

  return ret_val;
}

static uint32 _mtlk_sepm_count_legacy_stations(mtlk_scan_exemption_policy_manager_t *sepm_mgr)
{
  uint32 i;
  uint32 ret_val = 0;

  MTLK_ASSERT(sepm_mgr != NULL);

  for (i = 0; i < sepm_mgr->descriptor_array_size; i ++)
  {
    if ((sepm_mgr->sta_exemption_descriptors[i].slot_used == TRUE) &&
        (sepm_mgr->sta_exemption_descriptors[i].legacy == TRUE))
    {
        ret_val ++;
    }
  }

  return ret_val;
}

static uint32 _mtlk_sepm_count_intolerant_stations(mtlk_scan_exemption_policy_manager_t *sepm_mgr)
{
  uint32 i;
  uint32 ret_val = 0;

  MTLK_ASSERT(sepm_mgr != NULL);

  for (i = 0; i < sepm_mgr->descriptor_array_size; i ++)
  {
    if ((sepm_mgr->sta_exemption_descriptors[i].slot_used == TRUE) &&
        (sepm_mgr->sta_exemption_descriptors[i].intolerant == TRUE))
    {
      ret_val ++;
    }
  }

  return ret_val;
}

/**************************************************************************************/
/****************************************  STA  ***************************************/
/**************************************************************************************/


/* Initialization & cleanup */

int __MTLK_IFUNC mtlk_srrm_init (mtlk_scan_reqresp_manager_t *srrm_mgr,
  struct _mtlk_20_40_coexistence_sm *parent_csm, mtlk_20_40_csm_xfaces_t *xfaces)
{
  MTLK_ASSERT(srrm_mgr != NULL);
  MTLK_ASSERT(parent_csm != NULL);

  srrm_mgr->parent_csm = parent_csm;
  srrm_mgr->xfaces = xfaces;

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC mtlk_srrm_cleanup (mtlk_scan_reqresp_manager_t *srrm_mgr)
{
  MTLK_ASSERT(srrm_mgr != NULL);
  MTLK_ASSERT(srrm_mgr->parent_csm != NULL);
}


/* External functional interfaces (meant only for the parent coexistence state machine) */

int __MTLK_IFUNC mtlk_srrm_request_scan_exemption (mtlk_scan_reqresp_manager_t *srrm_mgr)
{
  MTLK_ASSERT(0);
  // The function has yet to be implemented

  MTLK_ASSERT(srrm_mgr != NULL);
  return 1;
}


int __MTLK_IFUNC mtlk_srrm_process_response (mtlk_scan_reqresp_manager_t *srrm_mgr, BOOL exemption_granted)
{
  MTLK_ASSERT(0);
  // The function has yet to be implemented

  MTLK_ASSERT(srrm_mgr != NULL);
  return 1;
}
