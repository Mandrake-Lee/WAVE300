/*
 * $Id$
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
#ifndef __COEXFRGEN_H__
#define __COEXFRGEN_H__

#ifndef COEX_20_40_C
#error This file can only be included from one of the 20/40 coexistence implementation (.c) files
#endif

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

struct _mtlk_20_40_coexistence_sm;

typedef struct _mtlk_coex_frame_gen
{
  struct _mtlk_20_40_coexistence_sm *parent_csm;
  mtlk_20_40_csm_xfaces_t           *xfaces;
}mtlk_coex_frame_gen;

int __MTLK_IFUNC mtlk_cefg_init (mtlk_coex_frame_gen *coex_frame_gen,
  struct _mtlk_20_40_coexistence_sm *parent_csm, mtlk_20_40_csm_xfaces_t *xfaces);

void __MTLK_IFUNC mtlk_cefg_cleanup (mtlk_coex_frame_gen *coex_frame_gen);

int __MTLK_IFUNC mtlk_cefg_send_coexistence_element (mtlk_coex_frame_gen *coex_frame_gen, 
  mtlk_20_40_coexistence_element *coex_el);

int __MTLK_IFUNC mtlk_cefg_send_coexistence_frame (mtlk_coex_frame_gen *coex_frame_gen,
  const IEEE_ADDR *sta_addr, const mtlk_20_40_coexistence_element *coex_el, const struct _mtlk_20_40_obss_scan_results *obss_scan_results);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif
