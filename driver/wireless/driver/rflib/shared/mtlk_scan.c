
/*
* $Id: $
*
* Copyright (c) 2006-2007 Metalink Broadband (Israel)
*
* Proprietary scan implementation
*
*/

#include "mtlkinc.h"
#include "mtlk_scan_propr.h"
#include "mtlk_channels_propr.h"
#include "mtlk_gpl_helper.h"
#include "scan.h"
#include "mtlk_core_iface.h"

#define LOG_LOCAL_GID   GID_SCAN
#define LOG_LOCAL_FID   1

#define UNLIMITED 0

mtlk_scan_vector_t * __MTLK_IFUNC
mtlk_scan_create_vector(void)
{
  mtlk_scan_vector_t* scan_vector;

  scan_vector = mtlk_osal_mem_alloc(sizeof(mtlk_scan_vector_t), MTLK_MEM_TAG_SCAN_VECTOR);

  if (NULL != scan_vector)
  {
      memset(scan_vector, 0, sizeof(mtlk_scan_vector_t));
  }

  return scan_vector;
}

void __MTLK_IFUNC
mtlk_scan_delete_vector(mtlk_scan_vector_t *vector)
{
  MTLK_ASSERT(NULL != vector);
  mtlk_osal_mem_free(vector);
}

uint16 __MTLK_IFUNC
mtlk_scan_vector_get_used(mtlk_scan_vector_t *vector)
{
  return vector->used;
}

FREQUENCY_ELEMENT * __MTLK_IFUNC
mtlk_scan_vector_get_offset(mtlk_scan_vector_t *vector, uint8 offs)
{
  return &vector->params[offs];
}

/*****************************************************************************
**
** NAME         mtlk_handle_bss_found_ind
**
** PARAMETERS   bss                 Message object as received from MAC
**              data                scan context
**
** RETURNS      none
**
** DESCRIPTION  Called by msg_receive when UMI indicates that new BSS was
**              found.
**
******************************************************************************/
void __MTLK_IFUNC
mtlk_scan_handle_bss_found_ind (struct mtlk_scan *scan, uint16 channel)
{
    int i = -1;

    /*
    * If scan is in progress, and
    * scan request was issued for specifid SSID and
    * reg_domain is known, and the scan type was passive and
    * we detect a hidden SSID (SSID with value of 0),
    * we do an active scan on this channel:
    */

    if (mtlk_scan_is_running(scan) &&
        //      bss_data->essid[0] == 0 &&
        //      data->scan_params.essid[0] != 0 &&
        mtlk_core_get_country_code(mtlk_vap_get_core(scan->vap_handle))) {
            mtlk_scan_vector_t *vector = scan->vector;
            //ILOG3_D("Found hidden SSID on channel %u", bss_data->channel);

            /*
            * Start iterating from end because most recent channel scan entry is
            * located at the end of vector and we need specifically the last one
            * to determine if most recent scan was active or not.
            * In case it was active, nothing else left to do
            */
            for (i = vector->used - 1; i >= 0; --i) {
                if (vector->params[i].u16Channel != HOST_TO_MAC16(channel))
                    continue;

                // If scan type of this channel wasn't passive, do nothing
                if (vector->params[i].u8ScanType != SCAN_PASSIVE)
                    break;

                ILOG2_D("Performing active scan on channel %u", channel);
                // Do active scan on this channel
                if (vector->used == vector->count) {
#if 0
                    ILOG2_V("Increasing scan vector by 1 element");
                    scan_vector_grow(vector, 1);
#else
                    ELOG_V("Unable to schedule active scan on channel with hidden SSID: "
                        "no space left in scan vector");
                    break;
#endif
                }
                memcpy(&vector->params[vector->used], &vector->params[i],
                    sizeof(vector->params[i]));
                vector->params[vector->used].u8ScanType = SCAN_ACTIVE;

                mtlk_fill_channel_params_by_tpc(HANDLE_T(mtlk_vap_get_core(scan->vap_handle)), &vector->params[vector->used]);

                vector->used++;
                break;
            }
    }
}

static mtlk_txmm_clb_action_e
scan_cfm_clb(mtlk_handle_t clb_usr_data, mtlk_txmm_data_t* man_entry, mtlk_txmm_clb_reason_e reason)
{
    int err = MTLK_ERR_OK;
    struct mtlk_scan *scan = (struct mtlk_scan *)clb_usr_data;

    MTLK_UNREFERENCED_PARAM(reason);

    scan->is_cfm_timeout = FALSE;

    if (MTLK_TXMM_CLBR_CONFIRMED != reason) {
      ELOG_D("Unconfirmed SCAN_REQ (reason=%d)", reason);
      /* Indicate timeout */
      scan->is_cfm_timeout = TRUE;
    }

    if(!scan->is_synchronous) {
      err = mtlk_core_schedule_internal_task(mtlk_vap_get_core(scan->vap_handle), HANDLE_T(scan),
                                             mtlk_scan_handle_evt_scan_confirmed,
                                             man_entry->payload, sizeof(UMI_SCAN));

      if (err != MTLK_ERR_OK) {
          ELOG_D("Can't notify SCAN CONFIRM event (err=%d)", err);
      }
    } else {
      memcpy(scan->last_result, man_entry->payload, man_entry->payload_size);
      mtlk_osal_event_set(&scan->chunk_scan_complete_event);
    }

    return MTLK_TXMM_CLBA_FREE;
}

/*****************************************************************************
**
** NAME         scan_send_req
**
** PARAMETERS   scan_data           Scan context
**
** RETURNS      none
**
** DESCRIPTION  Fill MAC structure with scan parameters according to scan flags
**              and send scan request to MAC.
**
******************************************************************************/
int __MTLK_IFUNC
mtlk_scan_send_scan_req (struct mtlk_scan *scan)
{
    mtlk_txmm_data_t* man_entry = NULL;
    UMI_SCAN *psScan;
    UMI_SCAN_HDR *psScanHdr;
    const struct mtlk_scan_params *params = &scan->params;
    int sres;

    ILOG2_V("Start scan request");
    man_entry = mtlk_txmm_msg_get_empty_data(&scan->async_scan_msg,
        scan->config.txmm);
    if (man_entry == NULL) {
        ELOG_V("No MM slot: failed to send scan request");
        return MTLK_ERR_SCAN_FAILED;
    }

    man_entry->id           = UM_MAN_SCAN_REQ;
    man_entry->payload_size = sizeof(UMI_SCAN);
    psScan = (UMI_SCAN *)man_entry->payload;

    memset(psScan, 0, sizeof(*psScan));

    psScanHdr = mtlk_get_umi_scan_hdr(psScan);
    psScanHdr->u16MinScanTime = HOST_TO_MAC16(params->min_scan_time); /* used for determination CCA in active scan */
    psScanHdr->u16MaxScanTime = HOST_TO_MAC16(params->max_scan_time); /* determine how long each channel should be scanned */
    psScanHdr->u8BSStype = (uint8)scan->params.bss_type; /* HOST_TO_MAC16(data->scan_params.bss_type);   enum ? 8 bit */
    /* ESSID to find */
    if (*(params->essid)) {
        mtlk_scan_get_essid(scan, psScanHdr->sSSID.acESSID);
        psScanHdr->sSSID.u8Length = strlen(psScanHdr->sSSID.acESSID);
    }
    psScanHdr->u8ProbeRequestRate = params->probe_rate; /* the rate on which the probe requests should be sent */
    psScanHdr->u8NumProbeRequests = params->num_probes;

    if (mtlk_core_get_country_code(mtlk_vap_get_core(scan->vap_handle)))
        psScanHdr->u8NumChannels = UMI_MAX_CHANNELS_PER_SCAN_REQ;
    else
        psScanHdr->u8NumChannels = 4;

    /* apply channels per chunk limit for background scan*/
    if (scan->params.is_background &&
        scan->params.channels_per_chunk_limit != UNLIMITED &&
        psScanHdr->u8NumChannels > scan->params.channels_per_chunk_limit)
        psScanHdr->u8NumChannels = scan->params.channels_per_chunk_limit;

    /* fill channels info */
    scan->ch_offset = mtlk_get_channels_for_reg_domain(scan,
        psScan->aChannelParams, &psScanHdr->u8NumChannels);
    ILOG2_DD("ch_offset = %d, u8NumChannels = %d", scan->ch_offset, psScanHdr->u8NumChannels);
    /* Allow active scan on last channel STA was connected to, even if SmRequired on that channel */
    if (scan->last_channel) {
        uint8 i;
        for (i = 0; i < psScanHdr->u8NumChannels; i++) {
            if (scan->last_channel == MAC_TO_HOST16(psScan->aChannelParams[i].u16Channel)) {
                psScan->aChannelParams[i].u8ScanType = SCAN_ACTIVE;
                scan->last_channel = 0;
                break;
            }
        }
    }

    mtlk_dump(4, man_entry->payload, sizeof(UMI_SCAN), "dump of UMI_SCAN payload:");

    sres = mtlk_txmm_msg_send(&scan->async_scan_msg, scan_cfm_clb, HANDLE_T(scan), SCAN_TIMEOUT);
    if (sres != MTLK_ERR_OK) {
        ELOG_D("Can't send SCAN req due to TXMM err#%d", sres);
    }

    return sres;
}

