/*
 * $Id: frame.c 12715 2012-02-21 17:39:01Z hatinecs $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * 802.11n frame processing routines.
 *
 * Originaly written by Artem Migaev
 *
 */
#include "mtlkinc.h"
#include "scan.h"
#include "frame.h"
#include "mtlkaux.h"
#include "channels.h"
#include "mtlk_core_iface.h"
#include "aocs.h"
#include "core.h"
#include "coex20_40.h"

#define MTLK_STA_HTCAP_LDPC_SUPPORTED           MTLK_BFIELD_INFO(0, 1)
#define MTLK_STA_HTCAP_CB_SUPPORTED             MTLK_BFIELD_INFO(1, 1)
#define MTLK_STA_HTCAP_SGI20_SUPPORTED          MTLK_BFIELD_INFO(5, 1)
#define MTLK_STA_HTCAP_SGI40_SUPPORTED          MTLK_BFIELD_INFO(6, 1)
#define MTLK_STA_HTCAP_MIMO_CONFIG_TX           MTLK_BFIELD_INFO(7, 1)
#define MTLK_STA_HTCAP_MIMO_CONFIG_RX           MTLK_BFIELD_INFO(8, 2)

#define MTLK_STA_AMPDU_PARAMS_MAX_LENGTH_EXP    MTLK_BFIELD_INFO(0, 2)
#define MTLK_STA_AMPDU_PARAMS_MIN_START_SPACING MTLK_BFIELD_INFO(2, 3)

#define LOG_LOCAL_GID   GID_FRAME
#define LOG_LOCAL_FID   1

/*!
	\fn	ie_extract_phy_rates()
	\brief	Process Rates informational elements
	\param	ie_data Pointer to IE
	\param	length IE length
	\param	bss_data Pointer to BSS description structure.
  
	'Supported Rates' and 'Extended Supported Rates' IEs are parsed
	by this function and filled to the supplied bss_data structure.
	All known rates are extracted, unsopported silently ignored.
*/
static __INLINE int
ie_extract_phy_rates (uint8      *ie_data,
                      int32       length,
                      bss_data_t *bss_data)
{
  int res = MTLK_ERR_OK;

  while (length) {
    uint32 rate = 0;
    uint8  val  = *ie_data;
    switch (val & 0x7F) {
    case 6*2: /* 11a rate 0 6mbps */
      rate = BASIC_RATE_SET_11A_6MBPS;
      break;

    case 9*2: /* 11a rate 1 9mbps */
      rate = BASIC_RATE_SET_11A_9MBPS;
      break;

    case 12*2: /* 11a rate 2 12mbps */
      rate = BASIC_RATE_SET_11A_12MBPS;
      break;

    case 18*2: /* 11a rate 3 18mbps */
      rate = BASIC_RATE_SET_11A_18MBPS;
      break;

    case 24*2: /* 11a rate 4 24mbps */
      rate = BASIC_RATE_SET_11A_24MBPS;
      break;

    case 36*2: /* 11a rate 5 36mbps */
      rate = BASIC_RATE_SET_11A_36MBPS;
      break;

    case 48*2: /* 11a rate 6 48mbps */
      rate = BASIC_RATE_SET_11A_48MBPS;
      break;

    case 54*2: /* 11a rate 7 54mbps */
      rate = BASIC_RATE_SET_11A_54MBPS;
      break;

    case 1*2: /* 11b rate 11 1mbps-long-preamble */
      rate = BASIC_RATE_SET_11B_1MBPS_LONG;
      break;

    case 2*2: /* 11b rate 8+12 2mbps-short+long */
      rate = BASIC_RATE_SET_11B_2MBPS_SHORT_LONG;
      break;

    case 11: /* (5.5*2) 11b rate 9+13 5.5mbps-short+long */
      rate = BASIC_RATE_SET_11B_5DOT5MBPS_SHORT_LONG;
      break;

    case 11*2: /* 11b rate 10+14 11mbps-short+long */
      rate = BASIC_RATE_SET_11B_11MBPS_SHORT_LONG;
      break;

    default: /* no other rates are allowed */
      ILOG2_D("Unexpected PHY rate: 0x%02X", (int)val);
      res = MTLK_ERR_PARAMS;
      break;
    };

    bss_data->operational_rate_set |= rate;
    if (val & 0x80)
      bss_data->basic_rate_set |= rate;

    length--;
    ie_data++;
  }

  if (bss_data->operational_rate_set & BASIC_RATE_SET_2DOT4_OPERATIONAL_RATE)
    bss_data->is_2_4 = 1;

  return res;
}



/*!
	\fn	ie_extract_ht_info()
	\brief	Process HT informational element
	\param	ie_data Pointer to IE
	\param	lenfth IE length
	\param	bss_data Pointer to BSS description structure.

	'HT' IE parsed by this function and filled to the supplied 
	bss_data structure. CB and Spectrum information extracted.
*/
static __INLINE int
ie_extract_ht_info (uint8      *ie_data,
                    int32       length,
                    bss_data_t *bss_data)
{
  int res = MTLK_ERR_PARAMS;

  if (length < 2) {
    ELOG_D("Wrong HT info length: %d", (int)length);
    goto FINISH;
  }

  bss_data->channel = ie_data[0];
  ILOG4_D("HT info channel is %u", bss_data->channel);

  switch (ie_data[1] & 0x07) {
  case 7: /* 40 lower */
    bss_data->spectrum    = 1;
    bss_data->upper_lower = 1;
    bss_data->secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCB;
    break;

  case 5: /* 40 upper */
    bss_data->spectrum    = 1;
    bss_data->upper_lower = 0;
    bss_data->secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCA;
    break;

  default:
    bss_data->spectrum    = 0;
    bss_data->upper_lower = 0;
    bss_data->secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCN;
    break;
  };

  res = MTLK_ERR_OK;

FINISH:
  return res;
}



/*!
	\fn	ie_extract_htcap_info()
	\brief	Process HT capabilities informational element
	\param	ie_data Pointer to IE
	\param	lenfth IE length
	\param	bss_data Pointer to BSS description structure.
  
	'HT Capabilities' IE parsed by this function and filled to the
	supplied bss_data structure. Rates information extracted.
*/
static __INLINE int
ie_extract_htcap_info (uint8      *ie_data,
                       int32       length,
                       bss_data_t *bss_data)
{
  int res = MTLK_ERR_PARAMS;
  uint8 val = 0;
  htcap_ie_t *htcap = (htcap_ie_t *)ie_data;

  if (length < 7) {
    ELOG_D("Wrong HT Capabilities length: %d", (int)length);
    goto FINISH;
  }
  htcap->info = WLAN_TO_HOST16(htcap->info);
  bss_data->forty_mhz_intolerant = (BOOL)!!(htcap->info & HTCAP_40MHZ_INTOLERANT);

  /**********************************************************
   * Modulation and coding scheme parsing.
   *
   * This is a 'Supported MCS Set' field of HT Capabilities
   * Information Element. We're interested only in Rx MCS
   * bitmask (78 bits) subfield. See sections 7.3.2.52.1,
   * 7.3.2.52.4 and 20.6 of 802.11n Draft 3.02
   ***********************************************************/

  val = htcap->supported_mcs[0];
  /* MCS 0..7 are filled to OperationalRateSet bits 15..22
   * For 1 spartial stream with 1 BCC modulator
   * -------------------------
   * Idx          20 MHz                40 MHz
   *      Normal GI   Short GI  Normal GI   Short GI
   * 0    6.5         7.2       13.5        15
   * 1    13          14.4      27          30
   * 2    19.5        21.7      40.5        45
   * 3    26          28.9      54          60
   * 4    39          43.3      81          90
   * 5    52          57.8      108         120
   * 6    58.5        65.0      121.5       135
   * 7    65          72.2      135         150
   */
  bss_data->operational_rate_set |= ((uint32)val) << 15;

  val = htcap->supported_mcs[1];
  /* MCS 8..15 are filled to OperationalRateSet bits 23..30
   * For 2 spartial streams equal modulation with 1 BCC modulator
   * -------------------------
   * Idx          20 MHz                40 MHz
   *      Normal GI   Short GI  Normal GI   Short GI
   * 8    13          14.4      27          30
   * 9    26          28.9      54          60
   * 10   39          43.3      81          90
   * 11   52          57.8      108         120
   * 12   78          86.7      162         180
   * 13   104         115.6     216         240
   * 14   117         130       243         270
   * 15   130         144.4     270         300
   */
  bss_data->operational_rate_set |= ((uint32)val) << 23;

  val = htcap->supported_mcs[4];
  /* MCS 32 is filled to OperationalRateSet bit 31
   * For 1 spartial stream with 1 BCC modulator
   * -------------------------
   * Idx          20 MHz                40 MHz
   *      Normal GI   Short GI  Normal GI   Short GI
   * 32   x           x         6           6.7
   */
  bss_data->operational_rate_set |= ((uint32)val) << 31;

  res = MTLK_ERR_OK;

FINISH:
  return res;
}

static __INLINE int
ie_extract_coex_el_info(uint8 *ie_data,
                        int32 length,
                        mtlk_20_40_coexistence_element *coex_el)
{
  int res = MTLK_ERR_PARAMS;

  if (length != COEXISTENCE_IE_LENGTH) {
    ELOG_D("Wrong coexistence IE length: %d", (int)length);
    goto FINISH;
  }
  memset(coex_el, 0, sizeof(*coex_el));
  coex_el->u8InformationRequest = ((*ie_data) & CE_INFORMATION_REQUEST_BIT_MASK) ? TRUE : FALSE;
  coex_el->u8FortyMhzIntolerant = ((*ie_data) & CE_INTOLERANT_BIT_MASK) ? TRUE : FALSE;
  coex_el->u8TwentyMhzBSSWidthRequest = ((*ie_data) & CE_20MHZ_WIDTH_REQUEST_BIT_MASK) ? TRUE : FALSE;
  coex_el->u8OBSSScanningExemptionRequest = ((*ie_data) & CE_SCAN_EXEMPTION_REQUEST_BIT_MASK) ? TRUE : FALSE;
  coex_el->u8OBSSScanningExemptionGrant = ((*ie_data) & CE_SCAN_EXEMPTION_GRANT_BIT_MASK) ? TRUE : FALSE;

  res = MTLK_ERR_OK;

FINISH:

  return res;
}

static __INLINE int
ie_extract_intolerant_channels_report(uint8 *ie_data,
                                      int32 length,
                                      UMI_INTOLERANT_CHANNEL_DESCRIPTOR *intolerant_channels_descriptor)
{
  int res = MTLK_ERR_PARAMS;

  memset(intolerant_channels_descriptor, 0, sizeof(*intolerant_channels_descriptor));
  if ((length >= MIN_INTOLERANT_CHANNELS_IE_LENGTH) && (length <= UMI_MAX_NUMBER_OF_INTOLERANT_CHANNELS + INTOLERANT_CHANNELS_LIST_OFFSET))
  {
    intolerant_channels_descriptor->u8OperatingClass = *ie_data;
    ie_data += INTOLERANT_CHANNELS_LIST_OFFSET;
    intolerant_channels_descriptor->u8NumberOfIntolerantChannels = length - INTOLERANT_CHANNELS_LIST_OFFSET;
    memcpy(intolerant_channels_descriptor->u8IntolerantChannels, ie_data, intolerant_channels_descriptor->u8NumberOfIntolerantChannels);
    res = MTLK_ERR_OK;
  }

  return res;
}

/*!
	\fn	ie_process()
	\brief	Process management frame IEs (Beacon or Probe Response)
	\param	fbuf Pointer to IE
	\param	flen IE length
	\param	bss_data Pointer to BSS description structure.

	All supported IEs parsed by this function and filled to the
	supplied bss_data structure. 
*/
static void
ie_process (mtlk_handle_t context, uint8 *fbuf, int32 flen, bss_data_t *bss_data)
{
  static const uint8 wpa_oui_type[] = {0x00, 0x50, 0xF2, 0x01};
  static const uint8 wps_oui_type[] = {0x00, 0x50, 0xF2, 0x04};

  ie_t *ie;
  int32 copy_length;
  mtlk_core_t *core = ((mtlk_core_t*)context);
  struct _mtlk_20_40_coexistence_sm *coex_sm;

  ILOG4_D("Frame length: %d", flen);
  bss_data->basic_rate_set       = 0;
  bss_data->operational_rate_set = 0;
  bss_data->ie_used_length = 0;
  bss_data->rsn_offs = 0xFFFF;
  bss_data->wpa_offs = 0xFFFF;
  bss_data->wps_offs = 0xFFFF;

  while (flen > sizeof(ie_t))
  {
    ie = (ie_t *)fbuf;  /* WARNING! check on different platforms */
    if ((int32)(ie->length + sizeof(ie_t)) > flen) {
        flen = 0;
        break;
    }

    if (bss_data->ie_used_length + ie->length + sizeof(ie_t) > sizeof(bss_data->info_elements))
    {
        ILOG4_V("Not enough room to store all information elements!!!");
        flen = 0;
        break;
    }

    if (ie->length) {
        memcpy(&bss_data->info_elements[bss_data->ie_used_length], ie, ie->length + sizeof(ie_t));
        switch (ie->id)
        {
        case IE_EXT_SUPP_RATES:
          bss_data->is_2_4 = 1;
          /* FALLTHROUGH */
        case IE_SUPPORTED_RATES:
          ie_extract_phy_rates(GET_IE_DATA_PTR(ie),
                               ie->length,
                               bss_data);
          break;
        case IE_HT_INFORMATION:
          ie_extract_ht_info(GET_IE_DATA_PTR(ie),
                             ie->length,
                             bss_data);
          break;
        case IE_SSID:
          copy_length = ie->length;
          if (copy_length > BSS_ESSID_MAX_SIZE) {
            ILOG1_DD("SSID IE truncated (%u > %d)!!!",
                 copy_length, (int)BSS_ESSID_MAX_SIZE);
            copy_length = BSS_ESSID_MAX_SIZE;
          }
          memcpy(bss_data->essid, GET_IE_DATA_PTR(ie), copy_length);
          bss_data->essid[copy_length] = '\0';  // ESSID always zero-terminated
          ILOG4_S("ESSID : %s", bss_data->essid);
          break;
        case IE_RSN:
          bss_data->rsn_offs = bss_data->ie_used_length;
          mtlk_dump(4, ie, ie->length + sizeof(ie_t),
                "RSN information element (WPA2)");
          break;
        case IE_VENDOR_SPECIFIC:
          if (!memcmp(GET_IE_DATA_PTR(ie), wpa_oui_type, sizeof(wpa_oui_type))) {
            bss_data->wpa_offs = bss_data->ie_used_length;
            mtlk_dump(4, ie, ie->length + sizeof(ie_t),
                  "Vendor Specific information element (WPA)");
          }
          else if (!memcmp(GET_IE_DATA_PTR(ie), wps_oui_type, sizeof(wps_oui_type))) {
            bss_data->wps_offs = bss_data->ie_used_length;
            mtlk_dump(4, ie, ie->length + sizeof(ie_t),
                  "Vendor Specific information element (WPS)");
          }
          break;
        case IE_DS_PARAM_SET:
          bss_data->channel = GET_IE_DATA_PTR(ie)[0];
          ILOG4_D("DS parameter set channel: %u", bss_data->channel);
          break;
        case IE_HT_CAPABILITIES:
          bss_data->is_ht = 1;
          mtlk_dump(4, ie, ie->length + sizeof(ie_t),
                "HTCap information element");
          ie_extract_htcap_info(GET_IE_DATA_PTR(ie),
                                ie->length,
                                bss_data);
          break;
        case IE_PWR_CONSTRAINT:
          bss_data->power = GET_IE_DATA_PTR(ie)[0];
          ILOG4_D("Power constraint: %d", bss_data->power);
          break;
        case IE_COUNTRY:
          bss_data->country_code = country_to_country_code((const char*)GET_IE_DATA_PTR(ie));
          bss_data->country_ie = (struct country_ie_t *)ie;
          break;
        /* TODO: Add required IE processing here */
        case IE_COEXISTENCE:
          coex_sm = mtlk_core_get_coex_sm(core);
          if (mtlk_20_40_is_feature_enabled(coex_sm) &&
              (mtlk_core_get_freq_band_cfg(core) == MTLK_HW_BAND_2_4_GHZ))
          {
            mtlk_20_40_coexistence_element coex_el;
            if (ie_extract_coex_el_info(GET_IE_DATA_PTR(ie),
                                ie->length,
                                &coex_el) == MTLK_ERR_OK)
            {
              IEEE_ADDR bss_addr;
              mtlk_osal_copy_eth_addresses(bss_addr.au8Addr, bss_data->bssid);
              if (mtlk_vap_is_ap(core->vap_handle))
              {
                  mtlk_20_40_ap_process_coexistence_element(coex_sm, &coex_el, &bss_addr);
              }
            }
          }
          break;
        case IE_INTOLERANT_CHANNELS_REPORT:
          coex_sm = mtlk_core_get_coex_sm(core);
          if (mtlk_20_40_is_feature_enabled(coex_sm) &&
              (mtlk_core_get_freq_band_cfg(core) == MTLK_HW_BAND_2_4_GHZ))
          {
            if (mtlk_vap_is_ap(core->vap_handle))
            {
                UMI_INTOLERANT_CHANNEL_DESCRIPTOR intolerant_channels_descriptor;
                if (ie_extract_intolerant_channels_report(GET_IE_DATA_PTR(ie),
                                                          ie->length,
                                                          &intolerant_channels_descriptor) == MTLK_ERR_OK)
                {
                    mtlk_20_40_ap_process_obss_scan_results(coex_sm, &intolerant_channels_descriptor);
                }
            }
          }
          break;
        default:
          break;
        } /*switch*/
        bss_data->ie_used_length += ie->length + sizeof(ie_t);
    }
    ILOG4_D("IE length: %d", ie->length);
    fbuf += ie->length + sizeof(ie_t);
    flen -= ie->length + sizeof(ie_t);
  } /*while*/

  /* Discard IEs that cause MAC failures afterward. */
  if ( ((!bss_data->is_2_4) && ((bss_data->basic_rate_set & BASIC_RATE_SET_OFDM_MANDATORY_RATES)
        != BASIC_RATE_SET_OFDM_MANDATORY_RATES_MASK)) ||
       (bss_data->is_2_4 && (bss_data->channel > BSS_DATA_2DOT4_CHANNEL_MAX)) ||
        !bss_data->channel ||
        !bss_data->operational_rate_set ||
       ((bss_data->spectrum &&
       !(bss_data->operational_rate_set & BASIC_RATE_SET_11N_RATE_MSK))))
  {
    memset(&bss_data->essid, 0, sizeof(bss_data->essid));
  }
}

static void
mtlk_process_action_frame_block_ack (mtlk_handle_t    context,
                                     sta_entry       *sta,
                                     uint8           *fbuf, 
                                     int32            flen,
                                     const IEEE_ADDR *src_addr)
{
  uint8 action = *(uint8 *)fbuf;

  fbuf += sizeof(action);
  flen -= sizeof(action);

  ILOG2_D("ACTION BA: action=%d", (int)action);

  /* NOTE: no WLAN_TO_HOST required for bitfields */
  switch (action) {
  case ACTION_FRAME_ACTION_ADDBA_REQUEST:
    if (sta)
    {
      frame_ba_addba_req_t *req       = (frame_ba_addba_req_t *)fbuf;
      uint16                param_set = WLAN_TO_HOST16(req->param_set);
      uint16                ssn       = WLAN_TO_HOST16(req->ssn);
 
      mtlk_addba_peer_on_addba_req_rx(mtlk_sta_get_addb_peer(sta),
                                      MTLK_BFIELD_GET(ssn, ADDBA_SSN_SSN),
                                      MTLK_BFIELD_GET(param_set, ADDBA_PARAM_SET_TID),
                                      (uint8)MTLK_BFIELD_GET(param_set, ADDBA_PARAM_SET_SIZE),
                                      (uint8)req->dlgt,
                                      WLAN_TO_HOST16((uint16)req->timeout),
                                      MTLK_BFIELD_GET(param_set, ADDBA_PARAM_SET_POLICY),
                                      MTLK_ADDBA_RATE_ADAPTIVE);
    }
    break;
  case ACTION_FRAME_ACTION_ADDBA_RESPONSE:
    if (sta)
    {
      frame_ba_addba_res_t *res       = (frame_ba_addba_res_t *)fbuf;
      uint16                param_set = WLAN_TO_HOST16(res->param_set);

      mtlk_addba_peer_on_addba_res_rx(mtlk_sta_get_addb_peer(sta),
                                      WLAN_TO_HOST16((uint16)res->scode),
                                      MTLK_BFIELD_GET(param_set, ADDBA_PARAM_SET_TID),
                                      (uint8)res->dlgt);
    }
    break;
  case ACTION_FRAME_ACTION_DELBA:
    if (sta)
    {
      frame_ba_delba_t* delba     = (frame_ba_delba_t *)fbuf;
      uint16            param_set = WLAN_TO_HOST16(delba->param_set);

      mtlk_addba_peer_on_delba_req_rx(mtlk_sta_get_addb_peer(sta),
                                      MTLK_BFIELD_GET(param_set, DELBA_PARAM_SET_TID),
                                      WLAN_TO_HOST16((uint16)delba->reason),
                                      MTLK_BFIELD_GET(param_set, DELBA_PARAM_SET_INITIATOR));
    }
    break;
  default:
    break;
  }
}

static void
_mtlk_process_action_frame_public(mtlk_handle_t    context,
                                 uint8           *fbuf, 
                                 int32            flen,
                                 scan_cache_t    *cache,
                                 IEEE_ADDR       *src_addr)
{
  uint8 action = *(uint8 *)fbuf;
  bss_data_t *bss_data;

  fbuf += sizeof(action);
  flen -= sizeof(action);

  ILOG2_D("ACTION PUBLIC: action=%d", (int)action);

  switch (action)
  {
    case ACTION_FRAME_ACTION_PUBLIC_COEXISTENCE:
        bss_data = mtlk_cache_temp_bss_acquire(cache);
        memset(bss_data, 0, sizeof(*bss_data));
        /* ie_process() expects to find the sender's MAC address in the bssid field of bss_data structure. */
        /* We're going to put it there */
        mtlk_osal_copy_eth_addresses(bss_data->bssid, src_addr->au8Addr);
        ie_process(context, fbuf, (int32)flen, bss_data);
        mtlk_cache_temp_bss_release(cache);
        break;
    default:
        break;
  }
}

static void
_mtlk_process_coex_info_from_man_frame(mtlk_handle_t    context,
                                       uint16           subtype,
                                       uint8           *fbuf, 
                                       int32            flen,
                                       scan_cache_t    *cache,
                                       IEEE_ADDR       *src_addr)
{
  bss_data_t *bss_data;

  bss_data = mtlk_cache_temp_bss_acquire(cache);
  memset(bss_data, 0, sizeof(*bss_data));
  /* ie_process() expects to find the sender's MAC address in the bssid field of bss_data structure. */
  /* We're going to put it there */
  mtlk_osal_copy_eth_addresses(bss_data->bssid, src_addr->au8Addr);
  ie_process(context, fbuf, (int32)flen, bss_data);
  mtlk_cache_temp_bss_release(cache);
}


static void
mtlk_process_action_frame_vendor_specific (mtlk_handle_t    context,
                                           uint8           *fbuf, 
                                           int32            flen,
                                           const IEEE_ADDR *src_addr)
{
  MTLK_VS_ACTION_FRAME_PAYLOAD_HEADER *vsaf_hdr = NULL;

  if (fbuf[0] != MTLK_OUI_0 ||
      fbuf[1] != MTLK_OUI_1 ||
      fbuf[2] != MTLK_OUI_2) {
    ILOG0_DDD("ACTION Vendor Specific: %02x-%02x-%02x",
         (int)fbuf[0],
         (int)fbuf[1],
         (int)fbuf[2]);
    goto end;
  }
  
  /* Skip OUI */
  fbuf += 3;
  flen -= 3;

  ILOG3_D("ACTION Vendor Specific: MTLK\n"
          "VSAF of %u bytes", flen);

  mtlk_dump(3, fbuf, 16, "");

  vsaf_hdr = (MTLK_VS_ACTION_FRAME_PAYLOAD_HEADER *)fbuf;

  /* Skip VSAF payload header */
  fbuf += sizeof(*vsaf_hdr);
  flen -= sizeof(*vsaf_hdr);

  /* Check VSAF payload data size */
  vsaf_hdr->u32DataSize = WLAN_TO_HOST32(vsaf_hdr->u32DataSize);
  if (vsaf_hdr->u32DataSize != flen) {
    WLOG_DD("Incorrect VSAF length (%u != %u), skipped",
            vsaf_hdr->u32DataSize,
            flen);
    goto end;
  }

  /* Check VSAF format version */
  vsaf_hdr->u32Version = WLAN_TO_HOST32(vsaf_hdr->u32Version);
  if (vsaf_hdr->u32Version != CURRENT_VSAF_FMT_VERSION) {
    WLOG_DD("Incorrect VSAF FMT version (%u != %u), skipped",
            vsaf_hdr->u32Version,
            CURRENT_VSAF_FMT_VERSION);
    goto end;
  }

  /* Parse VSAF items */
  vsaf_hdr->u32nofItems = WLAN_TO_HOST32(vsaf_hdr->u32nofItems);
  while (vsaf_hdr->u32nofItems) {
    MTLK_VS_ACTION_FRAME_ITEM_HEADER *item_hdr = 
      (MTLK_VS_ACTION_FRAME_ITEM_HEADER *)fbuf;

    fbuf += sizeof(*item_hdr);
    flen -= sizeof(*item_hdr);

    item_hdr->u32DataSize = WLAN_TO_HOST32(item_hdr->u32DataSize);
    item_hdr->u32ID       = WLAN_TO_HOST32(item_hdr->u32ID);

    switch (item_hdr->u32ID) {
#ifdef MTCFG_RF_MANAGEMENT_MTLK
    case MTLK_VSAF_ITEM_ID_SPR:
      {
        mtlk_rf_mgmt_t *rf_mgmt = mtlk_get_rf_mgmt(context);
      
        ILOG3_D("SPR received of %u bytes",
             item_hdr->u32DataSize);

        mtlk_rf_mgmt_handle_spr(rf_mgmt, src_addr, fbuf, flen);
      }
      break;
#endif
    default:
      WLOG_DD("Unsupported VSAF item (id=0x%08x, size = %u)",
              item_hdr->u32ID,
              item_hdr->u32DataSize);
      break;
    }

    fbuf += item_hdr->u32DataSize;
    flen -= item_hdr->u32DataSize;

    vsaf_hdr->u32nofItems--;
  }

end:
  return;
}

int
mtlk_process_man_frame (mtlk_handle_t context, sta_entry *sta, struct mtlk_scan *scan_data, scan_cache_t* cache, 
    mtlk_aocs_t *aocs, uint8 *fbuf, int32 flen, const MAC_RX_ADDITIONAL_INFO_T *mac_rx_info)
{
  mtlk_core_t *core = ((mtlk_core_t*)context);
  uint16 subtype;
  frame_head_t *head;
  frame_beacon_head_t *beacon_head;
  bss_data_t *bss_data;
  struct _mtlk_20_40_coexistence_sm *coex_sm;

  head = (frame_head_t *)fbuf;
  fbuf += sizeof(frame_head_t);
  flen -= sizeof(frame_head_t);

  mtlk_dump(4, head, sizeof(frame_head_t), "802.11n head:");

  subtype = head->frame_control;
  subtype = (MAC_TO_HOST16(subtype) & FRAME_SUBTYPE_MASK) >> FRAME_SUBTYPE_SHIFT;

  ILOG4_D("Subtype is %d", subtype);

  switch (subtype)
  {
  case MAN_TYPE_ASSOC_REQ:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_MGMT_OTHER);
    ILOG3_V("MAN_TYPE_ASSOC_REQ arrived. Parsing...");
    _mtlk_process_coex_info_from_man_frame(context, subtype, fbuf, flen, cache, (IEEE_ADDR *)head->src_addr);
    break;
  case MAN_TYPE_ASSOC_RES:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_MGMT_OTHER);
    ILOG3_V("MAN_TYPE_ASSOC_RES arrived. Parsing...");
    _mtlk_process_coex_info_from_man_frame(context, subtype, fbuf, flen, cache, (IEEE_ADDR *)head->src_addr);
    break;
  case MAN_TYPE_REASSOC_REQ:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_MGMT_OTHER);
    ILOG3_V("MAN_TYPE_REASSOC_REQ arrived. Parsing...");
    _mtlk_process_coex_info_from_man_frame(context, subtype, fbuf, flen, cache, (IEEE_ADDR *)head->src_addr);
    break;
  case MAN_TYPE_REASSOC_RES:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_MGMT_OTHER);
    ILOG3_V("MAN_TYPE_REASSOC_RES arrived. Parsing...");
    _mtlk_process_coex_info_from_man_frame(context, subtype, fbuf, flen, cache, (IEEE_ADDR *)head->src_addr);
    break;
  case MAN_TYPE_PROBE_REQ:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_MGMT_OTHER);
    ILOG3_V("MAN_TYPE_PROBE_REQ arrived. Parsing...");
    _mtlk_process_coex_info_from_man_frame(context, subtype, fbuf, flen, cache, (IEEE_ADDR *)head->src_addr);
    break;
  case MAN_TYPE_BEACON:
  case MAN_TYPE_PROBE_RES:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_MGMT_BEACON);
    beacon_head = (frame_beacon_head_t *)fbuf;
    fbuf += sizeof(frame_beacon_head_t);
    flen -= sizeof(frame_beacon_head_t);

    ILOG4_DDDDDDDDDD("RX_INFO: en=%u mn=%u c=%u rsn=%u prl=%u r=%u r=%u:%u:%u mr=%u",
         (unsigned)mac_rx_info->u8EstimatedNoise,
         (unsigned)mac_rx_info->u8MinNoise,
         (unsigned)mac_rx_info->u8Channel,
         (unsigned)mac_rx_info->u8RSN,
         (unsigned)MAC_TO_HOST16(mac_rx_info->u16PhyRxLen),
         (unsigned)mac_rx_info->u8RxRate,
         (unsigned)mac_rx_info->aRssi[0],
         (unsigned)mac_rx_info->aRssi[1],
         (unsigned)mac_rx_info->aRssi[2],
         (unsigned)mac_rx_info->MaxRssi);
    
    bss_data = mtlk_cache_temp_bss_acquire(cache);

    memset(bss_data, 0, sizeof(*bss_data));
    bss_data->channel = mac_rx_info->u8Channel;
    mtlk_osal_copy_eth_addresses(bss_data->bssid, head->bssid);
    ILOG4_YD("BSS %Y found on channel %d", bss_data->bssid,
          bss_data->channel);
    bss_data->capability = MAC_TO_HOST16(beacon_head->capability);
    bss_data->beacon_interval = MAC_TO_HOST16(beacon_head->beacon_interval);
    bss_data->beacon_timestamp = WLAN_TO_HOST64(beacon_head->beacon_timestamp);
    bss_data->received_timestamp = mtlk_osal_timestamp();
    ILOG4_D("Advertised capabilities : 0x%04X", bss_data->capability);
    if (bss_data->capability & CAPABILITY_IBSS_MASK) {
      bss_data->bss_type = UMI_BSS_ADHOC;
      ILOG4_V("BSS type is Ad-Hoc");
    } else {
      bss_data->bss_type = UMI_BSS_INFRA;
      ILOG4_V("BSS type is Infra");
      if (bss_data->capability & CAPABILITY_NON_POLL) {
        bss_data->bss_type = UMI_BSS_INFRA_PCF;
        ILOG4_V("PCF supported");
      }
    }

    // RSSI
    ASSERT(sizeof(mac_rx_info->aRssi) == sizeof(bss_data->all_rssi));
    memcpy(bss_data->all_rssi, mac_rx_info->aRssi, sizeof(mac_rx_info->aRssi));
    bss_data->max_rssi = mac_rx_info->MaxRssi;
    bss_data->noise    = mac_rx_info->u8EstimatedNoise;

    ILOG3_YDDD("BSS %Y, channel %u, max_rssi is %d, noise is %d",
         bss_data->bssid, bss_data->channel, 
         MTLK_NORMALIZED_RSSI(bss_data->max_rssi),
         MTLK_NORMALIZED_RSSI(bss_data->noise));

    // Process information elements
    ie_process(context, fbuf, (int32)flen, bss_data);

    if (!mtlk_vap_is_ap(core->vap_handle)) {
      mtlk_core_sta_country_code_update_from_bss(core, bss_data->country_code);
    }

    mtlk_find_and_update_ap(context, head->src_addr, bss_data);
    // APs are updated on beacons

    if (!mtlk_vap_is_slave_ap(core->vap_handle)) {
      mtlk_scan_handle_bss_found_ind (scan_data, bss_data->channel);
    }

    coex_sm = mtlk_core_get_coex_sm(core);
    if (mtlk_core_scan_is_running(core) || !mtlk_core_get_country_code(core) || mtlk_20_40_is_feature_enabled(coex_sm)) {
      mtlk_cache_register_bss(cache, bss_data);
      if (!bss_data->is_ht) {
        if (mtlk_core_get_freq_band_cfg(core) == MTLK_HW_BAND_2_4_GHZ) {
          mtlk_20_40_ap_notify_non_ht_beacon_received(coex_sm, bss_data->channel);
        }
      }
    }
#ifndef MBSS_FORCE_NO_AOCS_INITIAL_SELECTION
    if (mtlk_vap_is_ap(core->vap_handle) && mtlk_core_scan_is_running(core)) {
      MTLK_ASSERT(mtlk_vap_is_master_ap(core->vap_handle));
      mtlk_aocs_on_bss_data_update(core->slow_ctx->aocs, bss_data);
    }
#endif

    mtlk_cache_temp_bss_release(cache);

    break;
  case MAN_TYPE_ACTION:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_MGMT_ACTION);
    {
      uint8 category = *(uint8 *)fbuf;

      fbuf += sizeof(category);
      flen -= sizeof(category);

      ILOG3_D("ACTION: category=%d", (int)category);

      switch (category) {
      case ACTION_FRAME_CATEGORY_BLOCK_ACK:
        mtlk_process_action_frame_block_ack(context,
                                            sta,
                                            fbuf,
                                            flen,
                                            (IEEE_ADDR *)head->src_addr);
        break;
      case ACTION_FRAME_CATEGORY_PUBLIC:
        _mtlk_process_action_frame_public(context,
                                          fbuf,
                                          flen,
                                          cache,
                                          (IEEE_ADDR *)head->src_addr);
        break;
      case ACTION_FRAME_CATEGORY_VENDOR_SPECIFIC:
        mtlk_process_action_frame_vendor_specific(context,
                                                  fbuf,
                                                  flen,
                                                  (IEEE_ADDR *)head->src_addr);
        break;
      default:
        break;
      }
    }
    break;
  default:
    break;
  }
  return 0;
}



int
mtlk_process_ctl_frame (mtlk_handle_t context, uint8 *fbuf, int32 flen)
{
  uint16 fc, subtype;

  MTLK_UNREFERENCED_PARAM(flen);

  fc = MAC_TO_HOST16(*(uint16 *)fbuf);
  subtype = WLAN_FC_GET_STYPE(fc);

  ILOG4_D("Subtype is 0x%04X", subtype);

  switch (subtype)
  {
  case IEEE80211_STYPE_BAR:
    {
      frame_bar_t  *bar = (frame_bar_t*)fbuf;

      uint16 bar_ctl = MAC_TO_HOST16(bar->ctl);
      uint16 ssn = MAC_TO_HOST16(bar->ssn);
      uint8  tid = (uint8)((bar_ctl & IEEE80211_BAR_TID) >> IEEE80211_BAR_TID_S);

      ssn = (ssn & IEEE80211_SCTL_SEQ) >> 4; /* 0x000F - frag#, 0xFFF0 - seq */

      mtlk_handle_bar(context, bar->ta, tid, ssn);
    }
    break;
  default:
    break;
  }
  return 0;
}

void mtlk_frame_process_sta_capabilities(
                    sta_capabilities *capabilities,
                    uint8   lq_proprietry,
                    uint16  u16HTCapabilityInfo,
                    uint8   AMPDU_Parameters,
                    uint32  tx_bf_capabilities,
                    uint8   boWMEsupported,
                    uint32  u32SupportedRates,
                    uint8   band)
{
  MTLK_ASSERT(capabilities != NULL);

  /* Store supported bit rates capability */
  capabilities->NetModesSupported = 0;
  if (band == MTLK_HW_BAND_2_4_GHZ) {
    MTLK_BIT_SET(capabilities->NetModesSupported, MTLK_WSSA_11B_SUPPORTED, !!(u32SupportedRates & LM_PHY_11B_RATE_MSK));
    MTLK_BIT_SET(capabilities->NetModesSupported, MTLK_WSSA_11G_SUPPORTED, !!(u32SupportedRates & LM_PHY_11G_RATE_MSK));
  }
  else {
    MTLK_BIT_SET(capabilities->NetModesSupported, MTLK_WSSA_11A_SUPPORTED, !!(u32SupportedRates & LM_PHY_11A_RATE_MSK));
  }
  MTLK_BIT_SET(capabilities->NetModesSupported, MTLK_WSSA_11N_SUPPORTED, !!(u32SupportedRates & LM_PHY_11N_RATE_MSK));
  /* Store WME supported capability */
  capabilities->WMMSupported = boWMEsupported;

  /* Parse u16HTCapabilityInfo according to
   * IEEE Draft P802.11-REVmb™/D9.0, May 2011 -- 8.4.2.58.2 HT Capabilities Info field*/
  capabilities->LDPCSupported =
      (uint8)MTLK_BFIELD_GET(u16HTCapabilityInfo, MTLK_STA_HTCAP_LDPC_SUPPORTED);
  capabilities->CBSupported =
      (uint8)MTLK_BFIELD_GET(u16HTCapabilityInfo, MTLK_STA_HTCAP_CB_SUPPORTED);
  capabilities->SGI20Supported =
      (uint8)MTLK_BFIELD_GET(u16HTCapabilityInfo, MTLK_STA_HTCAP_SGI20_SUPPORTED);
  capabilities->SGI40Supported =
      (uint8)MTLK_BFIELD_GET(u16HTCapabilityInfo, MTLK_STA_HTCAP_SGI40_SUPPORTED);
  capabilities->MIMOConfigTX =
      (uint8)MTLK_BFIELD_GET(u16HTCapabilityInfo, MTLK_STA_HTCAP_MIMO_CONFIG_TX);
  capabilities->MIMOConfigRX =
      (uint8)MTLK_BFIELD_GET(u16HTCapabilityInfo, MTLK_STA_HTCAP_MIMO_CONFIG_RX);
  capabilities->STBCSupported =
      (capabilities->MIMOConfigTX || capabilities->MIMOConfigRX);

  /* Parse AMPDU_Parameters according to
   * IEEE Draft P802.11-REVmb™/D9.0, May 2011 -- 8.4.2.58.3 A-MPDU Parameters field*/
  capabilities->AMPDUMaxLengthExp =
      (uint8)MTLK_BFIELD_GET(AMPDU_Parameters, MTLK_STA_AMPDU_PARAMS_MAX_LENGTH_EXP);
  capabilities->AMPDUMinStartSpacing =
      (uint8)MTLK_BFIELD_GET(AMPDU_Parameters, MTLK_STA_AMPDU_PARAMS_MIN_START_SPACING);

  /* Parse tx_bf_capabilities according to
   * IEEE Draft P802.11-REVmb™/D9.0, May 2011 -- 8.4.2.58.6 Transmit Beamforming Capabilities*/
  /* We assume that explicit beamforming is supported in case at least one BF capability is supported */
  capabilities->BFSupported = !!tx_bf_capabilities;

  capabilities->Vendor =
        (uint8)MTLK_BFIELD_GET(lq_proprietry, LANTIQ_PROPRIETRY_VENDOR);
  capabilities->LQLDPCEnabled =
        (uint8)MTLK_BFIELD_GET(lq_proprietry, LANTIQ_PROPRIETRY_LDPC);
}
