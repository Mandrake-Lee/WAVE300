/*
 * $Id: scan.h 11413 2011-07-15 12:03:32Z andrii $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Roman Sikorskyy & Andriy Fidrya
 *
 */

#ifndef __SCAN_H__
#define __SCAN_H__

#include "mhi_ieee_address.h"
#include "mtlkerr.h"
#include "mtlk_osal.h"
#include "eeprom.h"
#include "frame.h"
#include "rsn.h"
#include "cache.h"
#include "aocs.h"
#include "mtlkflctrl.h"
#include "progmodel.h"

#define  MTLK_IDEFS_ON
//#define  MTLK_IDEFS_PACKING 1
#include "mtlkidefs.h"

#ifndef MTCFG_LINDRV_HW_AHBG35
#define SCAN_TIMEOUT (10000)
#else
#define SCAN_TIMEOUT (30000)
#endif

/**
 * scan_params
 **/
struct mtlk_scan_params
{
  char essid[MIB_ESSID_LENGTH + 1]; /* ssid to connect to */
  int bss_type; /* bss type: infra = 0, ad-hoc = 2, all = 3*/
  uint16 min_scan_time; /* in milliseconds */
  uint16 max_scan_time; /* in milliseconds */
  uint8 probe_rate;
  uint8 num_probes;
  uint8 channels_per_chunk_limit;
  uint16 pause_between_chunks;
  BOOL is_background;
  BOOL  is_background_scan_enabled;
} __MTLK_IDATA;

struct mtlk_scan_config {
  mtlk_txmm_t *txmm;
  mtlk_aocs_t *aocs;
  mtlk_flctrl_t *hw_tx_flctrl;
  scan_cache_t *bss_cache; 
} __MTLK_IDATA;

typedef struct _mtlk_scan_vector_t mtlk_scan_vector_t;

struct mtlk_scan
{
  struct mtlk_scan_config config;
  mtlk_vap_handle_t       vap_handle;
  BOOL initialized;
  BOOL rescan;
  struct mtlk_scan_params params;
  uint8 ch_offset;
  uint16 last_channel;
  uint8 spectrum;
  mtlk_scan_vector_t *vector;
  mtlk_osal_msec_t last_timestamp;
  uint8 orig_band;
  uint8 cur_band;
  uint8 next_band;  // for dual-band scan, indicates next band that should be scanned
  mtlk_osal_event_t completed; // for sync scan
  mtlk_atomic_t is_running;
  mtlk_atomic_t treminate_scan;

  BOOL is_synchronous;  // Whether synchronous scan is running
  BOOL is_cfm_timeout;  /* SCAN_SFM timeout */
  uint8 *last_result; // Result of the last chunk scanning during synchronous scan
  mtlk_osal_event_t chunk_scan_complete_event; //Event indicating chunk scan complete for sync scan

  mtlk_osal_timer_t pause_timer;
  mtlk_handle_t flctrl_id;
  mtlk_progmodel_t *progmodels[MTLK_HW_BAND_BOTH];
  mtlk_txmm_msg_t async_scan_msg;
} __MTLK_IDATA;

int __MTLK_IFUNC mtlk_scan_init (struct mtlk_scan *scan, struct mtlk_scan_config config, mtlk_vap_handle_t vap_handle);
void __MTLK_IFUNC mtlk_scan_cleanup (struct mtlk_scan *scan);

mtlk_scan_vector_t * __MTLK_IFUNC mtlk_scan_create_vector(void);
void __MTLK_IFUNC mtlk_scan_delete_vector(mtlk_scan_vector_t *vector);
uint16 __MTLK_IFUNC mtlk_scan_vector_get_used(mtlk_scan_vector_t *vector);

void __MTLK_IFUNC mtlk_scan_handle_bss_found_ind (struct mtlk_scan *scan, uint16 channel);
int __MTLK_IFUNC mtlk_scan_sync (struct mtlk_scan *scan, uint8 band, uint8 is_cb_scan);
int __MTLK_IFUNC mtlk_scan_async (struct mtlk_scan *scan, uint8 band, const char* essid);
void __MTLK_IFUNC mtlk_scan_set_background(struct mtlk_scan *scan, BOOL is_background);

void __MTLK_IFUNC mtlk_scan_schedule_rescan (struct mtlk_scan *scan);

void __MTLK_IFUNC mtlk_scan_set_essid (struct mtlk_scan *scan, const char *buf);
size_t __MTLK_IFUNC mtlk_scan_get_essid (struct mtlk_scan *scan, char *buf);

static __INLINE BOOL mtlk_scan_is_initialized(struct mtlk_scan *scan)
{
  return (scan->initialized == TRUE);
}

static __INLINE BOOL mtlk_scan_is_running(struct mtlk_scan *scan)
{
  return mtlk_osal_atomic_get(&scan->is_running);
}

int __MTLK_IFUNC mtlk_scan_send_scan_req (struct mtlk_scan *scan);

int __MTLK_IFUNC
mtlk_scan_handle_evt_scan_confirmed (mtlk_handle_t scan_object, const void *payload, uint32 size);

void __MTLK_IFUNC
scan_terminate_and_wait_completion(struct mtlk_scan *scan);

void __MTLK_IFUNC
scan_terminate(struct mtlk_scan *scan);

void __MTLK_IFUNC
scan_complete (struct mtlk_scan *scan);

void __MTLK_IFUNC
mtlk_scan_set_per_chunk_limit(struct mtlk_scan *scan, uint8 channels_per_chunk_limit);

uint8 __MTLK_IFUNC
mtlk_scan_get_per_chunk_limit(struct mtlk_scan *scan);

void __MTLK_IFUNC
mtlk_scan_set_pause_between_chunks(struct mtlk_scan *scan, uint16 pause_between_chunks);

uint16 __MTLK_IFUNC
mtlk_scan_get_pause_between_chunks(struct mtlk_scan *scan);

void __MTLK_IFUNC
mtlk_scan_set_is_background_scan_enabled(struct mtlk_scan *scan, BOOL is_background_scan_enabled);

BOOL __MTLK_IFUNC
mtlk_scan_is_background_scan_enabled(struct mtlk_scan *scan);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __SCAN_H__ */
