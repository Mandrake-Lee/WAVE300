/*
 * $Id: coexfrgen.c 11794 2011-10-23 12:54:42Z kashani $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * 20/40 coexistence feature
 * Provides transition between modes (20MHz->20/40MHz and vice versa)
 *
 * The coexistence frame generator will be used by the 20/40 coexistence state machine for generating 
 * and transmitting coexistence elements as well as entire coexistence frames. 
 * This sub-module will prepare the data and send it to the firmware that will build and transmit 
 * the actual frames.
 */
#include "mtlkinc.h"

#define COEX_20_40_C
// This define is necessary for coex20_40priv.h file to compile successfully
#include "coex20_40priv.h"

#define LOG_LOCAL_GID   GID_COEX
#define LOG_LOCAL_FID   4


/* Initialization & cleanup */

int __MTLK_IFUNC mtlk_cefg_init (mtlk_coex_frame_gen *coex_frame_gen,
  struct _mtlk_20_40_coexistence_sm *parent_csm, mtlk_20_40_csm_xfaces_t *xfaces)
{
  MTLK_ASSERT(coex_frame_gen != NULL);
  MTLK_ASSERT(parent_csm != NULL);

  coex_frame_gen->parent_csm = parent_csm;
  coex_frame_gen->xfaces = xfaces;

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC mtlk_cefg_cleanup (mtlk_coex_frame_gen *coex_frame_gen)
{
  MTLK_ASSERT(coex_frame_gen != NULL);
  MTLK_ASSERT(coex_frame_gen->parent_csm != NULL);
}


/* External functional interfaces (meant only for the parent coexistence state machine) */

int __MTLK_IFUNC mtlk_cefg_send_coexistence_element (mtlk_coex_frame_gen *coex_frame_gen,
  mtlk_20_40_coexistence_element *coex_el)
{
  MTLK_ASSERT(0);
  // The function has yet to be implemented

  MTLK_ASSERT(coex_frame_gen != NULL);
  MTLK_ASSERT(coex_el != NULL);
  return 1;
}

int __MTLK_IFUNC mtlk_cefg_send_coexistence_frame (mtlk_coex_frame_gen *coex_frame_gen,
  const IEEE_ADDR *sta_addr, const mtlk_20_40_coexistence_element *coex_el, const struct _mtlk_20_40_obss_scan_results *obss_scan_results)
{
  int ret_val = MTLK_ERR_OK;
  UMI_COEX_FRAME coex_frame;

  MTLK_ASSERT(coex_frame_gen != NULL);
  MTLK_ASSERT(coex_el != NULL);

  memset(&coex_frame, 0, sizeof(coex_frame));
  mtlk_osal_copy_eth_addresses(coex_frame.sDestAddr.au8Addr, sta_addr->au8Addr);
  memcpy(&coex_frame.sCoexistenceElement, coex_el, sizeof(coex_frame.sCoexistenceElement));
  if (obss_scan_results)
  {
    memcpy(&coex_frame.sIntolerantChannelsReport, &obss_scan_results->intolerant_channels_report, sizeof(coex_frame.sIntolerantChannelsReport));
  }

  (*coex_frame_gen->xfaces->send_cmf)(HANDLE_T(coex_frame_gen->parent_csm), sta_addr, &coex_frame);

  return ret_val;
}
