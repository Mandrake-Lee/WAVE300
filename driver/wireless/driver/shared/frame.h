/*
 * $Id: frame.h 12518 2012-01-29 16:41:44Z nayshtut $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 *  Originaly written by Artem Migaev
 *
 */

#ifndef _FRAME_H_
#define _FRAME_H_

// TODO: to use typedef we should include scan.h
// but this create cross reference between scan.h
// and frame.h, so we use plain forward declaration
// of struct _scan_data_t in header;

//#include "scan.h"
#include "rsn.h"
#include "addba.h"
#include "stadb.h"

#define  MTLK_IDEFS_ON
#define  MTLK_IDEFS_PACKING 1
#include "mtlkidefs.h"
#include "ieee80211defs.h"
#include "mhi_ieee_address.h"

struct mtlk_scan;
struct _mtlk_aocs_t;
struct _scan_cache_t;

#define BASIC_RATE_SET_11A_6MBPS                 0x00000001
#define BASIC_RATE_SET_11A_9MBPS                 0x00000002
#define BASIC_RATE_SET_11A_12MBPS                0x00000004
#define BASIC_RATE_SET_11A_18MBPS                0x00000008
#define BASIC_RATE_SET_11A_24MBPS                0x00000010
#define BASIC_RATE_SET_11A_36MBPS                0x00000020
#define BASIC_RATE_SET_11A_48MBPS                0x00000040
#define BASIC_RATE_SET_11A_54MBPS                0x00000080
#define BASIC_RATE_SET_11B_1MBPS_LONG            0x00000800
#define BASIC_RATE_SET_11B_2MBPS_SHORT_LONG      0x00001100
#define BASIC_RATE_SET_11B_5DOT5MBPS_SHORT_LONG  0x00002200
#define BASIC_RATE_SET_11B_11MBPS_SHORT_LONG     0x00004400

#define BASIC_RATE_SET_OFDM_MANDATORY_RATES    (BASIC_RATE_SET_11A_6MBPS  | \
                                               BASIC_RATE_SET_11A_12MBPS | \
                                               BASIC_RATE_SET_11A_24MBPS)

#define BASIC_RATE_SET_OFDM_MANDATORY_RATES_MASK    0x00000015
#define BASIC_RATE_SET_CCK_MANDATORY_RATES          0x00007800
#define BASIC_RATE_SET_CCK_MANDATORY_RATES_MASK     0x00007800
#define BASIC_RATE_SET_11N_RATE_MSK                 0xFFFF8000
#define BASIC_RATE_SET_2DOT4_OPERATIONAL_RATE       0x00007F00
#define BSS_DATA_2DOT4_CHANNEL_MIN                  1
#define BSS_DATA_2DOT4_CHANNEL_MAX                  14


/*
 * Frame packed headers
 */

#define FRAME_SUBTYPE_MASK                      0x00F0
#define FRAME_SUBTYPE_SHIFT                     4
#define MAX_INFO_ELEMENTS_LENGTH                512
#define COUNTRY_IE_LENGTH                       2
#define COEXISTENCE_IE_LENGTH                   1
#define MIN_INTOLERANT_CHANNELS_IE_LENGTH       2
#define INTOLERANT_CHANNELS_LIST_OFFSET         1

#define CE_INFORMATION_REQUEST_BIT_MASK     0x1
#define CE_INTOLERANT_BIT_MASK              0x2
#define CE_20MHZ_WIDTH_REQUEST_BIT_MASK     0x4
#define CE_SCAN_EXEMPTION_REQUEST_BIT_MASK  0x8
#define CE_SCAN_EXEMPTION_GRANT_BIT_MASK    0x10

typedef struct {
  uint16    frame_control;
  uint16    duration;
  uint8     dst_addr[IEEE_ADDR_LEN];
  uint8     src_addr[IEEE_ADDR_LEN];
  uint8     bssid[IEEE_ADDR_LEN];
  uint16    seq_control;
} __MTLK_IDATA frame_head_t;

#define CAPABILITY_IBSS_MASK    0x0002
#define CAPABILITY_NON_POLL     0x000C

typedef struct {
  uint64 beacon_timestamp;
  uint16 beacon_interval;
  uint16 capability;
} __MTLK_IDATA frame_beacon_head_t;

typedef struct {
  uint8 id; // actually this is ie_id type! but enum's size is int...
  uint8 length;
} __MTLK_IDATA ie_t;

#define GET_IE_DATA_PTR(ie_ptr) ((uint8*)ie_ptr + sizeof(*ie_ptr))

typedef struct {
  uint16 info;
  uint8 ampdu_params;
  uint8 supported_mcs[16];
  uint16 ht_ext_capabilities;
  uint32 txbf_capabilities;
  uint8 asel_capabilities;
} __MTLK_IDATA htcap_ie_t;

typedef struct {
  uint16    fc;                 /* Frame Control            */
  uint16    dur;                /* Duration                 */
  uint8     ra[IEEE_ADDR_LEN];  /* RA - Receiver Address    */
  uint8     ta[IEEE_ADDR_LEN];  /* TA - Transmitter Address */
  uint16    ctl;                /* BAR Control              */
  uint16    ssn;                /* Start Sequence Number    */
} __MTLK_IDATA frame_bar_t;

#define ACTION_FRAME_CATEGORY_BLOCK_ACK         3
#define ACTION_FRAME_CATEGORY_PUBLIC            4
#define ACTION_FRAME_CATEGORY_VENDOR_SPECIFIC   127

#define ACTION_FRAME_ACTION_ADDBA_REQUEST       0
#define ACTION_FRAME_ACTION_ADDBA_RESPONSE      1
#define ACTION_FRAME_ACTION_DELBA               2

#define ACTION_FRAME_ACTION_PUBLIC_COEXISTENCE  0

typedef struct {
  uint8      category;       /* CategoryCode         */
  uint8      action;         /* ActionCode           */
} __MTLK_IDATA frame_action_head_t; /* Action frame header */

#include "mtlkbfield.h"

#define ADDBA_PARAM_SET_POLICY    MTLK_BFIELD_INFO(1, 1)
#define ADDBA_PARAM_SET_TID       MTLK_BFIELD_INFO(2, 4)
#define ADDBA_PARAM_SET_SIZE      MTLK_BFIELD_INFO(6, 10)

#define ADDBA_SSN_SSN             MTLK_BFIELD_INFO(4, 12)

#define DELBA_PARAM_SET_INITIATOR MTLK_BFIELD_INFO(11, 1)
#define DELBA_PARAM_SET_TID       MTLK_BFIELD_INFO(12, 4)

typedef struct {
  uint8  dlgt;       /* DialogToken          */
  uint16 param_set;  /* BlockAckParameterSet */
  uint16 timeout;    /* BlockAckTimeOut      */
  uint16 ssn;        /* BlockAckStartSn      */
} __MTLK_IDATA frame_ba_addba_req_t;

typedef struct {
  uint8  dlgt;       /* DialogToken          */
  uint16 scode;      /* StatusCode           */
  uint16 param_set;  /* BlockAckParameterSet */
  uint16 timeout;    /* BlockAckTimeOut      */
} __MTLK_IDATA frame_ba_addba_res_t;

typedef struct {
  uint16 param_set;
  uint16 reason;
} __MTLK_IDATA frame_ba_delba_t;

/* HT capabilities info fields */
#define HTCAP_SUP_CHNL_WIDTH_SET 0x0002
#define HTCAP_40MHZ_INTOLERANT   0x4000

struct country_constr_t
{
  uint8 first_ch;
  uint8 num_ch;
  int8 max_power;
} __MTLK_IDATA;

struct country_ie_t
{
  uint8 id;
  uint8 length;
  uint8 country[3];
  // if first_ch < 201, channel info
  // else - reg class info, followed by channel info...
  struct country_constr_t constr[1];
} __MTLK_IDATA;

struct power_constr_ie_t
{
  uint8 id;
  uint8 length;
  uint8 power;
} __MTLK_IDATA;

typedef enum {
  MAN_TYPE_ASSOC_REQ,
  MAN_TYPE_ASSOC_RES,
  MAN_TYPE_REASSOC_REQ,
  MAN_TYPE_REASSOC_RES,
  MAN_TYPE_PROBE_REQ,
  MAN_TYPE_PROBE_RES,
  /* 6 - 7 reserved*/
  MAN_TYPE_BEACON = 8,
  MAN_TYPE_ATIM,
  MAN_TYPE_DISASSOC,
  MAN_TYPE_AUTH,
  MAN_TYPE_DEAUTH,
  MAN_TYPE_ACTION
  /* 13 - 15 reserved*/
} man_frame_t;

typedef enum {
  IE_SSID,
  IE_SUPPORTED_RATES,
  IE_FH_PARAM_SET,
  IE_DS_PARAM_SET,
  IE_CF_PARAM_SET,
  IE_TIM,
  IE_IBSS,
  IE_COUNTRY,
  IE_FH_PATTRN_PARAMS,
  IE_FH_PATTRM_TABLE,
  IE_REQUEST,
  IE_BSS_LOAD,
  IE_EDCA_PARAM_SET,
  IE_TSPEC,
  IE_TCLAS,
  IE_SCHEDULE,
  IE_CHALLENGE_TXT,
  /* 17...31 are not used */
  IE_PWR_CONSTRAINT = 32,
  IE_PWR_CAPABILITY,
  IE_TPC_REQUEST,
  IE_TPC_REPORT,
  IE_SUPP_CHANNELS,
  IE_CHANNEL_SW_ANN,
  IE_MEASURE_REQUEST,
  IE_MEASURE_REPOR,
  IE_QUIET,
  IE_IBSS_DFS,
  IE_ERP_INFO,
  IE_TS_DELAY,
  IE_TCLAS_PROCESSING,
  IE_HT_CAPABILITIES,
  IE_QOS_CAPABILITY = 46,
  /* 47 unused */
  IE_RSN = 48,
  /* 49 unused */
  IE_EXT_SUPP_RATES = 50,
  /* 51...60 are not used */
  IE_HT_INFORMATION = 61,
  IE_COEXISTENCE = 72,
  IE_INTOLERANT_CHANNELS_REPORT = 73,

  /* 74...126 are not used */
  IE_EXT_CAPABILITIES = 127,
  IE_VENDOR_SPECIFIC = 221
} ie_id_t;


#define BSS_ESSID_MAX_SIZE 32
#define MTLK_NORMALIZED_RSSI(mac_rssi) ( ((int)(mac_rssi)) - 256 )

typedef struct {
  // Platform dependant stuff
  uint64    beacon_timestamp;
  mtlk_osal_timestamp_t received_timestamp;
  // Calculated
  uint8     bssid[IEEE_ADDR_LEN];
  uint16    bss_type;
  uint16    channel;
  uint8     secondary_channel_offset;

  uint8     max_rssi;
  uint8     all_rssi[3]; /* there are always 3 RX antennas */
  uint8     noise;
  // From IFs
  uint16    capability;
  uint16    beacon_interval;
  // From IEs
  uint8     essid[BSS_ESSID_MAX_SIZE+1];
  uint8     info_elements[MAX_INFO_ELEMENTS_LENGTH];
  uint16    ie_used_length;
  uint16    rsn_offs;
  uint16    wpa_offs;
  uint16    wps_offs;
  uint8     country_code;
  struct    country_ie_t *country_ie;
  uint8     is_2_4;
  uint8     is_ht;
  uint32    basic_rate_set;
  uint32    operational_rate_set;
  uint8     spectrum;
  uint8     upper_lower;
  int8      power;
  BOOL      forty_mhz_intolerant;
} bss_data_t;

#define RSN_IE_SIZE(bss) (((bss)->rsn_offs == 0xffff)? 0:(bss)->info_elements[(bss)->rsn_offs+1])
#define RSN_IE(bss) (&(bss)->info_elements[(bss)->rsn_offs])
#define WPA_IE_SIZE(bss) (((bss)->wpa_offs == 0xffff)? 0:(bss)->info_elements[(bss)->wpa_offs+1])
#define WPA_IE(bss) (&(bss)->info_elements[(bss)->wpa_offs])
#define WPS_IE_SIZE(bss) (((bss)->wps_offs == 0xffff)? 0:(bss)->info_elements[(bss)->wps_offs+1])
#define WPS_IE(bss) (&(bss)->info_elements[(bss)->wps_offs])
#define WPS_IE_FOUND(bss) ((bss)->wps_offs != 0xffff)

#define BSS_WEP_MASK ((uint16)0x0010)
#define BSS_IS_WEP_ENABLED(bss) (((bss)->capability & BSS_WEP_MASK) ? TRUE: FALSE)

/* BAR Control */
#define IEEE80211_BAR_TID       0xf000
#define IEEE80211_BAR_TID_S     12

typedef struct {
  uint8     rssi;      // Global RSSI in dBm
  uint8     noise;     // Global Noise in dBm
  uint16    reserved;
} __MTLK_IDATA rcv_status_t;

int mtlk_process_man_frame (mtlk_handle_t context,
                            sta_entry *sta,
                            struct mtlk_scan *scan_data,
                            struct _scan_cache_t* cache,
                            struct _mtlk_aocs_t *aocs,
                            uint8 *fbuf,
                            int32 flen,
                            const MAC_RX_ADDITIONAL_INFO_T *mac_rx_info);

int mtlk_process_ctl_frame (mtlk_handle_t context,
                            uint8 *fbuf,
                            int32 flen);

/* Move the following prototype to reconnect.h when it becomes
   a cross-platform file*/
void mtlk_find_and_update_ap(mtlk_handle_t context,
                             uint8 *addr,
                             bss_data_t *bss_data);

/* Move the following prototype to the corresponding cross-platform file
   when it available */
int mtlk_handle_bar(mtlk_handle_t context,
                    uint8 *ta,
                    uint8 tid,
                    uint16 ssn);

void mtlk_frame_process_sta_capabilities(
                    sta_capabilities *sta_capabilities,
                    uint8   lq_proprietry,
                    uint16  u16HTCapabilityInfo,
                    uint8   AMPDU_Parameters,
                    uint32  tx_bf_capabilities,
                    uint8   boWMEsupported,
                    uint32  u32SupportedRates,
                    uint8   band);


#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* _FRAME_H_ */

