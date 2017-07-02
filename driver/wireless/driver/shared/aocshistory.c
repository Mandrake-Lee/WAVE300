/*
 * $Id: aocshistory.c 12355 2012-01-03 14:56:37Z laptijev $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Dmitry Fleytman
 *
 */
#include "mtlkinc.h"

#include "aocshistory.h"
#include "mtlkaux.h"

#define LOG_LOCAL_GID   GID_AOCS
#define LOG_LOCAL_FID   3

#define MAX_HISTORY_LENGTH 20

void __MTLK_IFUNC 
mtlk_aocs_history_init(mtlk_aocs_history_t *history)
{
    mtlk_dlist_init(history);
}

void __MTLK_IFUNC 
mtlk_aocs_history_clean(mtlk_aocs_history_t *history)
{
    mtlk_dlist_entry_t* entry;
    mtlk_aocs_history_entry_t* history_entry;
    
    while( NULL != (entry = mtlk_dlist_pop_back(history)) )
    {
        history_entry = MTLK_LIST_GET_CONTAINING_RECORD(entry,
            mtlk_aocs_history_entry_t, list_entry);
        mtlk_osal_mem_free(history_entry);
    }

    mtlk_dlist_cleanup(history);
}

#ifdef MTCFG_DEBUG

static BOOL
aocs_history_validate_sw_info(mtlk_sw_info_t* info)
{
    return (info->reason   > SWR_LOWER_BOUND)  &&
           (info->reason   < SWR_HIGHER_BOUND) &&
           (info->criteria > CHC_LOWER_BOUND)  &&
           (info->criteria < CHC_HIGHER_BOUND);
}

#endif

BOOL __MTLK_IFUNC 
mtlk_aocs_history_add(mtlk_aocs_history_t *history, 
                    mtlk_sw_info_t* info)
{
    mtlk_aocs_history_entry_t* history_entry;

    MTLK_ASSERT(aocs_history_validate_sw_info(info));

    if(mtlk_dlist_size(history) < MAX_HISTORY_LENGTH)
    {
        history_entry = mtlk_osal_mem_alloc(sizeof(*history_entry), 
            MTLK_MEM_TAG_ANTENNA_GAIN);

        if(NULL == history_entry)
        {
            ELOG_V("Failed to allocate channel switch history entry.");
            return FALSE;
        }
    }
    else
    {
        mtlk_dlist_entry_t* entry;
        entry = mtlk_dlist_pop_front(history);

        history_entry = MTLK_LIST_GET_CONTAINING_RECORD(entry,
            mtlk_aocs_history_entry_t, list_entry);
    }

    history_entry->info = *info;
    history_entry->timestamp = mtlk_osal_timestamp();
    mtlk_dlist_push_back(history, &history_entry->list_entry);

    return TRUE;
}

void __MTLK_IFUNC
mtlk_cl_sw_criteria_text(channel_criteria_t criteria,
                         channel_criteria_details_t* criteria_details,
                         char* buff)
{
    switch(criteria)
    {
    case CHC_SCAN_RANK:
        sprintf(buff, "Scan rank (%u)", criteria_details->scan.rank);
        return;
    case CHC_CONFIRM_RANK:
        sprintf(buff, "Confirm rank (%u --> %u)", 
          criteria_details->confirm.old_rank, criteria_details->confirm.new_rank);
        return;
    case CHC_RANDOM:
        strcpy(buff, "Random");
        return;
    case CHC_USERDEF:
        strcpy(buff, "Defined by user");
        return;
    case CHC_2GHZ_BSS_MAJORITY:
        strcpy(buff, "BSS Majority");
        return;
    case CHC_LOWEST_TIMEOUT:
        strcpy(buff, "Minimal timeout");
        return;
    default:
        strcpy(buff, "UNKNOWN");
        MTLK_ASSERT(FALSE);
        return;
    }
};

static char*
_mtlk_wssa_get_switch_reason_text(mtlk_aocs_channel_switch_reasons_t reason)
{
    switch(reason)
    {
    case SWR_LOW_THROUGHPUT:
        return "Low TX rate";
    case SWR_HIGH_SQ_LOAD:
        return "High SQ load";
    case SWR_RADAR_DETECTED:
        return "Radar";
    case SWR_CHANNEL_LOAD_CHANGED:
        return "Channel load";
    case SWR_INITIAL_SELECTION:
        return "Initial selection";
    case SWR_MAC_PRESSURE_TEST:
        return "Pressure test";
    case SWR_AP_SWITCHED:
        return "AP is switched";
    default:
        MTLK_ASSERT(FALSE);
        return "UNKNOWN";
    }
};

int  __MTLK_IFUNC
mtlk_aocshistory_get_history(mtlk_aocs_history_t *history, mtlk_clpb_t *clpb)
{
  mtlk_dlist_entry_t *head;
  mtlk_dlist_entry_t *entry;
  mtlk_aocs_history_entry_t *history_entry;
  mtlk_aocs_history_stat_entry_t stat_entry;
  int res = MTLK_ERR_UNKNOWN;

  MTLK_ASSERT(NULL != history);
  MTLK_ASSERT(NULL != clpb);

  mtlk_dlist_foreach(history,entry,head) {
    history_entry = MTLK_LIST_GET_CONTAINING_RECORD(entry,
        mtlk_aocs_history_entry_t, list_entry);

    mtlk_cl_sw_criteria_text(history_entry->info.criteria, 
        &history_entry->info.criteria_details, stat_entry.criteria_text);

    stat_entry.hour_ago = mtlk_osal_time_get_hours_ago(history_entry->timestamp);
    stat_entry.min_ago = mtlk_osal_time_get_minutes_ago(history_entry->timestamp);
    stat_entry.sec_ago = mtlk_osal_time_get_seconds_ago(history_entry->timestamp);
    stat_entry.msec_ago = mtlk_osal_time_get_mseconds_ago(history_entry->timestamp);
    stat_entry.primary_channel = history_entry->info.primary_channel;
    stat_entry.secondary_channel = history_entry->info.secondary_channel;

    strcpy(stat_entry.reason_text, _mtlk_wssa_get_switch_reason_text(history_entry->info.reason));

    if (MTLK_ERR_OK != (res = mtlk_clpb_push(clpb, &stat_entry, sizeof(stat_entry)))) {
      goto err_push;
    }
  }
  return MTLK_ERR_OK;

err_push:
  mtlk_clpb_purge(clpb);
  return res;
}
