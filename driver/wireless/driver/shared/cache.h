/*
 * $Id: cache.h 11862 2011-10-30 16:46:22Z kashani $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Roman Sikorskyy
 *
 */

#ifndef __cache_h__
#define __cache_h__

#include "mtlkerr.h"
#include "mtlk_osal.h"
#include "mtlklist.h"
#include "frame.h"

#define MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef struct _cache_entry_t
{
    mtlk_osal_msec_t        local_timestamp;
    BOOL                    is_persistent;  //entry is not removed even if expired
    int                     freq;
    bss_data_t              bss;
    mtlk_slist_entry_t      link_entry;
} __MTLK_IDATA cache_entry_t;

typedef struct _scan_cache_t
{
    unsigned long           cache_expire; // in seconds
    mtlk_osal_spinlock_t    lock;
    cache_entry_t           *cur_entry;
    mtlk_slist_t            bss_list;
    mtlk_osal_spinlock_t    temp_bss_lock;
    bss_data_t              temp_bss_data;
    uint8                   modified;
    MTLK_DECLARE_INIT_STATUS;
} __MTLK_IDATA scan_cache_t;

int __MTLK_IFUNC mtlk_cache_init (scan_cache_t *cache, unsigned long expire);
void __MTLK_IFUNC mtlk_cache_cleanup (scan_cache_t *cache);

void __MTLK_IFUNC mtlk_cache_clear (scan_cache_t *cache);
void __MTLK_IFUNC mtlk_cache_register_bss (scan_cache_t *cache, bss_data_t *bss);
uint32 __MTLK_IFUNC mtlk_cache_get_bss_count (scan_cache_t *cache, uint32 *storage_needed_for_ies);
void __MTLK_IFUNC mtlk_cache_rewind (scan_cache_t *cache);
uint8 __MTLK_IFUNC mtlk_cache_get_next_bss (scan_cache_t *cache, bss_data_t *bss, int *freq, unsigned long *timestamp);
uint8 __MTLK_IFUNC mtlk_cache_find_bss_by_bssid (scan_cache_t *cache, const uint8 *bssid, bss_data_t *bss_data, int *freq);
struct country_ie_t *__MTLK_IFUNC mtlk_cache_find_first_country_ie(scan_cache_t *cache, const uint8 country_code);
void __MTLK_IFUNC mtlk_cache_remove_bss_by_bssid (scan_cache_t *cache, const uint8 *bssid);
void __MTLK_IFUNC mtlk_cache_delete_current (scan_cache_t *cache);

uint8 __MTLK_IFUNC mtlk_cache_was_modified (scan_cache_t *cache);
void __MTLK_IFUNC mtlk_cache_clear_modified_flag(scan_cache_t *cache);

bss_data_t *__MTLK_IFUNC mtlk_cache_temp_bss_acquire(scan_cache_t *cache);
void __MTLK_IFUNC mtlk_cache_temp_bss_release(scan_cache_t *cache);

void __MTLK_IFUNC mtlk_cache_set_persistent(scan_cache_t *cache, const uint8 *bssid, BOOL is_persistent);

uint32 __MTLK_IFUNC mtlk_cache_get_expiration_time(scan_cache_t *cache);
void __MTLK_IFUNC mtlk_cache_set_expiration_time(scan_cache_t *cache, unsigned long expiration_time);

#define MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __cache_h__ */
