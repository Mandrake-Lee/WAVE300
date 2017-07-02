/*
 * $Id: cache.c 11862 2011-10-30 16:46:22Z kashani $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Roman Sikorskyy
 *
 */
#include "mtlkinc.h"

#include "cache.h"

#define LOG_LOCAL_GID   GID_CACHE
#define LOG_LOCAL_FID   1

static struct country_ie_t *
_cache_clone_country_ie(bss_data_t *bss)
{
  struct country_ie_t *country_ie = NULL;
  int ie_length;

  /* allocate and copy country IE */
  if (!bss->country_ie)
    goto FINISH;
  ie_length = bss->country_ie->length + sizeof(ie_t);
  country_ie = mtlk_osal_mem_alloc(ie_length, MTLK_MEM_TAG_BSS_COUNTRY_IE);
  if (!country_ie) {
    WLOG_V("Cannot allocate memory for Country IE");
    goto FINISH;
  }
  memcpy(country_ie, bss->country_ie, ie_length);
FINISH:
  return country_ie;
}

static void
_cache_free_entry(cache_entry_t *slot)
{
  if (slot->bss.country_ie != NULL)
    mtlk_osal_mem_free(slot->bss.country_ie);
  mtlk_osal_mem_free(slot);
}

void __MTLK_IFUNC
mtlk_cache_register_bss (scan_cache_t *cache, bss_data_t *bss)
{
  mtlk_slist_entry_t *prev, *next;
  cache_entry_t *slot = NULL;

  mtlk_osal_lock_acquire(&cache->lock);

  prev = mtlk_slist_head(&cache->bss_list);
  while ((next = mtlk_slist_next(prev)) != NULL) {
    cache_entry_t *place = MTLK_LIST_GET_CONTAINING_RECORD(next, cache_entry_t, link_entry);
    if (!memcmp(place->bss.bssid, bss->bssid, IEEE_ADDR_LEN)) {
      mtlk_slist_remove_next(&cache->bss_list, prev);
      slot = place;
      if (bss->essid[0] == '\0') {
        /* Preserve ESSID from active scan */
        memcpy(bss->essid, slot->bss.essid, sizeof(bss->essid));
      }
      break;
    }
    prev = next;
  }

  if (slot == NULL) {
    slot = mtlk_osal_mem_alloc(sizeof(cache_entry_t), MTLK_MEM_TAG_BSS_CACHE);
    if (slot == NULL) {
      ILOG2_Y("Failed to register BSS %Y", bss->bssid);
      goto out;
    }
    slot->bss.country_ie = NULL;
  }

  cache->modified = 1;
  /* release previously stored country IE if any */
  if (slot->bss.country_ie != NULL) {
    mtlk_osal_mem_free(slot->bss.country_ie);
    slot->bss.country_ie = NULL;
  }
  memset(slot, 0, sizeof(cache_entry_t));
  memcpy(&slot->bss, bss, sizeof(bss_data_t));
  slot->bss.country_ie =_cache_clone_country_ie(bss);
  slot->local_timestamp = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  slot->freq = bss->is_2_4;

  prev = mtlk_slist_head(&cache->bss_list);
  while ((next = mtlk_slist_next(prev)) != NULL) {
    cache_entry_t *place = MTLK_LIST_GET_CONTAINING_RECORD(next, cache_entry_t, link_entry);
    if (place->bss.channel >= slot->bss.channel)
      break;
    prev = next;
  }

  mtlk_slist_insert_next(&cache->bss_list, prev, &slot->link_entry);
  ILOG2_Y("BSS %Y registered/updated", slot->bss.bssid);

out:
  mtlk_osal_lock_release(&cache->lock);
}

/*************************************************************************************
**                                                                                  **
** NAME         mtlk_cache_init                                                     **
**                                                                                  **
** PARAMETERS                                                                       **
**                                                                                  **
**                                                                                  **
*************************************************************************************/

MTLK_INIT_STEPS_LIST_BEGIN(cache)
  MTLK_INIT_STEPS_LIST_ENTRY(cache, SLIST_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(cache, LOCK_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(cache, BSS_LOCK_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(cache, BSS_CACHE_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(cache)
MTLK_INIT_STEPS_LIST_END(cache);

int __MTLK_IFUNC
mtlk_cache_init (scan_cache_t *cache, unsigned long expire)
{
  MTLK_INIT_TRY(cache, MTLK_OBJ_PTR(cache))
    MTLK_INIT_STEP_VOID(cache, SLIST_INIT, MTLK_OBJ_PTR(cache), 
                        mtlk_slist_init, (&cache->bss_list));
    MTLK_INIT_STEP(cache, LOCK_INIT, MTLK_OBJ_PTR(cache), 
                   mtlk_osal_lock_init, (&cache->lock));
    MTLK_INIT_STEP(cache, BSS_LOCK_INIT, MTLK_OBJ_PTR(cache), 
                   mtlk_osal_lock_init, (&cache->temp_bss_lock));
    MTLK_INIT_STEP_VOID(cache, BSS_CACHE_INIT, MTLK_OBJ_PTR(cache),
                        MTLK_NOACTION, ());

    cache->cache_expire = expire;
    
  MTLK_INIT_FINALLY(cache, MTLK_OBJ_PTR(cache))
  MTLK_INIT_RETURN(cache, MTLK_OBJ_PTR(cache), mtlk_cache_cleanup, (cache));
}

void __MTLK_IFUNC
mtlk_cache_cleanup (scan_cache_t *cache)
{
  MTLK_CLEANUP_BEGIN(cache, MTLK_OBJ_PTR(cache))
  
    MTLK_CLEANUP_STEP(cache, BSS_CACHE_INIT, MTLK_OBJ_PTR(cache),
                      mtlk_cache_clear, (cache));
    MTLK_CLEANUP_STEP(cache, BSS_LOCK_INIT, MTLK_OBJ_PTR(cache),
                      mtlk_osal_lock_cleanup, (&cache->temp_bss_lock));
    MTLK_CLEANUP_STEP(cache, LOCK_INIT, MTLK_OBJ_PTR(cache),
                      mtlk_osal_lock_cleanup, (&cache->lock));
    MTLK_CLEANUP_STEP(cache, SLIST_INIT, MTLK_OBJ_PTR(cache),
                      mtlk_slist_cleanup, (&cache->bss_list));
  MTLK_CLEANUP_END(cache, MTLK_OBJ_PTR(cache));
}

void __MTLK_IFUNC
mtlk_cache_clear (scan_cache_t *cache)
{
    mtlk_slist_entry_t *list_entry_to_delete = NULL;
    cache_entry_t *cache_entry_to_delete = NULL;

    mtlk_osal_lock_acquire(&cache->lock);

    while ((list_entry_to_delete = mtlk_slist_pop(&cache->bss_list)) != NULL)
    {
        cache_entry_to_delete = MTLK_LIST_GET_CONTAINING_RECORD(list_entry_to_delete, cache_entry_t, link_entry);
        _cache_free_entry(cache_entry_to_delete);
    }
    
    cache->modified = 1;
    cache->cur_entry = NULL;

    mtlk_osal_lock_release(&cache->lock);
}

void __MTLK_IFUNC
mtlk_cache_rewind (scan_cache_t *cache)
{
    mtlk_slist_entry_t *current_list_entry;

    mtlk_osal_lock_acquire(&cache->lock);

    current_list_entry = mtlk_slist_begin(&cache->bss_list);
    if (current_list_entry)
    {
        cache->cur_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry, cache_entry_t, link_entry);
    }
    else
    {
        cache->cur_entry = NULL;
    }

    mtlk_osal_lock_release(&cache->lock);
}

uint8 __MTLK_IFUNC
mtlk_cache_get_next_bss (scan_cache_t *cache, bss_data_t *bss, int *freq, unsigned long *timestamp)
{
    uint8 res = 0;
    mtlk_slist_entry_t *prev_list_entry = NULL;
    mtlk_slist_entry_t *next_list_entry = NULL;
    mtlk_osal_msec_t cur_time = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());

    mtlk_osal_lock_acquire(&cache->lock);

    if (cache->cur_entry)
    {
        prev_list_entry = mtlk_slist_head(&cache->bss_list);
        while (prev_list_entry)
        {
            next_list_entry = mtlk_slist_next(prev_list_entry);
            if (next_list_entry == &cache->cur_entry->link_entry)
            {
                break;
            }
            prev_list_entry = next_list_entry;
        }
    }

    if (!prev_list_entry)
    {
        cache->cur_entry = NULL;    
        /* cache->cur_entry was not found in the list 
           it shouldn't normally happen, but we check
           for it just in case
        */
    }

    while (cache->cur_entry && !res)
    {
        mtlk_slist_entry_t *last_valid_entry = prev_list_entry;
        if (cache->cache_expire && 
            ((cur_time - cache->cur_entry->local_timestamp) > cache->cache_expire * 1000) && /* expire is in sec, timestamp is in ms */
            !cache->cur_entry->is_persistent)
        {
            ILOG3_YD("Expired cache entry removed: %Y (ch = %d)",
              cache->cur_entry->bss.bssid, cache->cur_entry->bss.channel);
            mtlk_slist_remove_next(&cache->bss_list, prev_list_entry);
            cache->modified = 1;
            /* prev_list_entry remains the same, but it now points to the next list element */

            _cache_free_entry(cache->cur_entry);
        }
        else
        {
            ILOG3_YD("Cache entry copied to user buffer: %Y (ch = %d)",
              cache->cur_entry->bss.bssid, cache->cur_entry->bss.channel);
            if (bss)
            {
              // It's possible that we are not interested in the actual data but have rather called
              // this function to remove the outdated entries from the ap database and to count the
              // remaining valid entries
                memcpy(bss, &cache->cur_entry->bss, sizeof(*bss));
            }
            if (freq)
            {
                *freq = cache->cur_entry->freq;
            }
            if (timestamp)
            {
                *timestamp = cache->cur_entry->local_timestamp;
            }

            last_valid_entry = &cache->cur_entry->link_entry;
            res = 1;
        }
        next_list_entry = mtlk_slist_next(last_valid_entry);
        if (next_list_entry)
        {
            cache->cur_entry = MTLK_LIST_GET_CONTAINING_RECORD(next_list_entry, cache_entry_t, link_entry);
        }
        else
        {
            cache->cur_entry = NULL;
        }
    }

    mtlk_osal_lock_release(&cache->lock);

    return res;
}

uint32 __MTLK_IFUNC
mtlk_cache_get_bss_count(scan_cache_t *cache, uint32 *storage_needed_for_ies)
{
    uint32 number_of_aps = 0;
    bss_data_t *bss_data;

    if (storage_needed_for_ies)
    {
        *storage_needed_for_ies = 0;
    }

    bss_data = mtlk_cache_temp_bss_acquire(cache);

    mtlk_cache_rewind(cache);
    while (mtlk_cache_get_next_bss(cache, bss_data, NULL, NULL))
    {
        number_of_aps ++;
        if (storage_needed_for_ies)
        {
            *storage_needed_for_ies += bss_data->ie_used_length;
        }
    }

    mtlk_cache_temp_bss_release(cache);

    return number_of_aps;
}

uint8 __MTLK_IFUNC
mtlk_cache_find_bss_by_bssid(scan_cache_t *cache, const uint8 *bssid, bss_data_t *bss_data, int *freq)
{
    uint8 found = 0;

    mtlk_cache_rewind(cache);
    while (mtlk_cache_get_next_bss(cache, bss_data, freq, NULL))
    {
        if (!memcmp(bssid, bss_data->bssid, IEEE_ADDR_LEN))
        {
            found = 1;
            break;
        }
    }

    return found;
}

struct country_ie_t *__MTLK_IFUNC
mtlk_cache_find_first_country_ie(scan_cache_t *cache, const uint8 country_code)
{   
  struct country_ie_t *country_ie = NULL;
  bss_data_t bss_data;
    
  mtlk_cache_rewind(cache);
  while (mtlk_cache_get_next_bss(cache, &bss_data, NULL, NULL)) {
    if (bss_data.country_code == country_code) {
      country_ie = _cache_clone_country_ie(&bss_data);
      break;
    }
  }
  return country_ie;
} 

/*****************************************************************************
**
** NAME         mtlk_cache_remove_bss_by_bssid
**
** PARAMETERS   data            Scan context
**              bssid           AP bssid to be removed from the scan list
**
** RETURNS      none
**
** DESCRIPTION  Remove bssid from the scan list
**
**
******************************************************************************/
void __MTLK_IFUNC
mtlk_cache_remove_bss_by_bssid (scan_cache_t *cache, const uint8 *bssid)
{
    mtlk_slist_entry_t *prev_list_entry;
    mtlk_slist_entry_t *current_list_entry;
    cache_entry_t *current_cache_entry;

    mtlk_osal_lock_acquire(&cache->lock);

    prev_list_entry = mtlk_slist_head(&cache->bss_list);
    while (prev_list_entry)
    {
        current_list_entry = mtlk_slist_next(prev_list_entry);
        if (!current_list_entry)
        {
            break;
        }
        current_cache_entry = MTLK_LIST_GET_CONTAINING_RECORD(current_list_entry, cache_entry_t, link_entry);
        if (!memcmp(bssid, current_cache_entry->bss.bssid, IEEE_ADDR_LEN))
        {
            mtlk_slist_remove_next(&cache->bss_list, prev_list_entry);
            cache->modified = 1;
            if (cache->cur_entry == current_cache_entry)
            {
                cache->cur_entry = NULL;
            }
            _cache_free_entry(current_cache_entry);
            break;
        }
        else
        {
            prev_list_entry = current_list_entry;
        }
    }

    mtlk_osal_lock_release(&cache->lock);
}

void __MTLK_IFUNC
mtlk_cache_delete_current (scan_cache_t *cache)
{
    if (cache->cur_entry)
    {
        mtlk_slist_entry_t *prev_list_entry = mtlk_slist_head(&cache->bss_list);
        while (prev_list_entry)
        {
            mtlk_slist_entry_t *next_list_entry = mtlk_slist_next(prev_list_entry);
            if (next_list_entry == &cache->cur_entry->link_entry)
            {
                break;
            }
            prev_list_entry = next_list_entry;
        }

        if (prev_list_entry)
        {
            ILOG4_S("BSS removed from cache: %s", cache->cur_entry->bss.essid);
            mtlk_slist_remove_next(&cache->bss_list, prev_list_entry);
            cache->modified = 1;
            _cache_free_entry(cache->cur_entry);
        }
        cache->cur_entry = NULL;
    }
}

/*****************************************************************************
**
** NAME         mtlk_cache_temp_bss_acquire
**
** PARAMETERS   cache           cache pointer
**              lock_value      pointer to location that receives the value
**                              to be passed later to mtlk_cache_temp_bss_release
**
** RETURNS      pointer to the temporary structure of bss_data_t type
**
** DESCRIPTION  Locks the spinlock that protects the temporary bss_data structure
**              and returns pointer to that structure
**
******************************************************************************/
bss_data_t *__MTLK_IFUNC
mtlk_cache_temp_bss_acquire(scan_cache_t *cache)
{
    mtlk_osal_lock_acquire(&cache->temp_bss_lock);
    return &cache->temp_bss_data;
}

/*****************************************************************************
**
** NAME         mtlk_cache_temp_bss_release
**
** PARAMETERS   cache           cache pointer
**              lock_value      The value returned in lock_value by the call to
**                              mtlk_scan_temp_bss_acquire
**
** RETURNS      none
**
** DESCRIPTION  Unlocks the spinlock that protects the temporary bss_data structure
**
******************************************************************************/
void __MTLK_IFUNC
mtlk_cache_temp_bss_release(scan_cache_t *cache)
{
    mtlk_osal_lock_release(&cache->temp_bss_lock);
}

/*****************************************************************************
**
** NAME         mtlk_cache_was_modified
**
** PARAMETERS   cache           cache pointer
**             
** RETURNS      uint32          if a new entry was entered to removed from cache
**
** DESCRIPTION  this function is an API to check if the content of cache was modified.
**
******************************************************************************/
uint8 __MTLK_IFUNC 
mtlk_cache_was_modified (scan_cache_t *cache)
{
    uint8 ret_val = 0;
    if(cache)
    {
        mtlk_osal_lock_acquire(&cache->lock);

        ret_val = cache->modified;

        mtlk_osal_lock_release(&cache->lock);
    }
    return ret_val;
}

/*****************************************************************************
**
** NAME         mtlk_cache_clear_modified_flag
**
** PARAMETERS   cache           cache pointer
**
** RETURNS      none
**
** DESCRIPTION  clears the modified flag; should be used once it was read.
**
******************************************************************************/
void __MTLK_IFUNC 
mtlk_cache_clear_modified_flag(scan_cache_t *cache)
{
    if(cache)
    {
        mtlk_osal_lock_acquire(&cache->lock);

        cache->modified = 0;

        mtlk_osal_lock_release(&cache->lock);
    }    
}

// Persistent BSS is not removed from cache on expiration.
// Usually STA sets BSS to which it has connected as persistent.
// This will allow to keep relevant information about this BSS without re-scan (we know BSS 
// is alive while we are connected to it).
void __MTLK_IFUNC
mtlk_cache_set_persistent(scan_cache_t *cache, const uint8 *bssid, BOOL is_persistent)
{
  mtlk_slist_entry_t *prev, *next;

  mtlk_osal_lock_acquire(&cache->lock);

  prev = mtlk_slist_head(&cache->bss_list);
  while ((next = mtlk_slist_next(prev)) != NULL) {
    cache_entry_t *entry = MTLK_LIST_GET_CONTAINING_RECORD(next, cache_entry_t, link_entry);

    if (!memcmp(&entry->bss.bssid, bssid, IEEE_ADDR_LEN)) {
      // We are sure BSS was alive just a second ago, thus we can update its timestamp
      entry->local_timestamp = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());      
      entry->is_persistent = is_persistent;
      ILOG2_YD("BSSID %Y, persistent=%i", bssid, is_persistent);
      break;
    }

    prev = next;
  }

  mtlk_osal_lock_release(&cache->lock);
}

uint32 __MTLK_IFUNC mtlk_cache_get_expiration_time(scan_cache_t *cache)
{
  uint32 ret_val;

  mtlk_osal_lock_acquire(&cache->lock);

  ret_val = cache->cache_expire;

  mtlk_osal_lock_release(&cache->lock);

  return ret_val;
}

void __MTLK_IFUNC mtlk_cache_set_expiration_time(scan_cache_t *cache, unsigned long expiration_time)
{
  mtlk_osal_lock_acquire(&cache->lock);

  cache->cache_expire = expiration_time;

  mtlk_osal_lock_release(&cache->lock);
}
