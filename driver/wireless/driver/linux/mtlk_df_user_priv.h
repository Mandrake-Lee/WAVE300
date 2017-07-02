/*
 * $Id: mtlk_df_user_priv.h 12533 2012-02-02 11:24:00Z nayshtut $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Driver framework implementation for Linux
 *
 */

#ifndef __DF_USER_PRIVATE_H__
#define __DF_USER_PRIVATE_H__

#include <net/iw_handler.h>

#include "mtlk_df.h"

#define MAX_PROC_STR_LEN     32
#define TEXT_SIZE IW_PRIV_SIZE_MASK /* 2047 */

#ifdef MTCFG_DEBUG
#define AOCS_DEBUG
#endif

typedef struct _mtlk_df_user_t mtlk_df_user_t;

enum {
  PRM_ID_FIRST = 0x7fff, /* Range 0x0000 - 0x7fff reserved for MIBs */

  /* AP Capabilities */
  PRM_ID_AP_CAPABILITIES_MAX_STAs,
  PRM_ID_AP_CAPABILITIES_MAX_VAPs,

  /* FW GPIO LED */
  PRM_ID_CFG_LED_GPIO,
  PRM_ID_CFG_LED_STATE,

  /* AOCS configuration ioctles */
  PRM_ID_AOCS_WEIGHT_CL,
  PRM_ID_AOCS_WEIGHT_TX,
  PRM_ID_AOCS_WEIGHT_BSS,
  PRM_ID_AOCS_WEIGHT_SM,
  PRM_ID_AOCS_CFM_RANK_SW_THRESHOLD,
  PRM_ID_AOCS_SCAN_AGING,
  PRM_ID_AOCS_CONFIRM_RANK_AGING,
  PRM_ID_AOCS_EN_PENALTIES,
  PRM_ID_AOCS_AFILTER,
  PRM_ID_AOCS_BONDING,
  PRM_ID_AOCS_MSDU_THRESHOLD,
  PRM_ID_AOCS_WIN_TIME,
  PRM_ID_AOCS_MSDU_PER_WIN_THRESHOLD,
  PRM_ID_AOCS_LOWER_THRESHOLD,
  PRM_ID_AOCS_THRESHOLD_WINDOW,
  PRM_ID_AOCS_MSDU_DEBUG_ENABLED,
  PRM_ID_AOCS_IS_ENABLED,
  PRM_ID_AOCS_MEASUREMENT_WINDOW,
  PRM_ID_AOCS_THROUGHPUT_THRESHOLD,
  PRM_ID_AOCS_NON_OCCUPANCY_PERIOD,
  PRM_ID_AOCS_RESTRICTED_CHANNELS,
  PRM_ID_AOCS_MSDU_TX_AC,
  PRM_ID_AOCS_MSDU_RX_AC,
  PRM_ID_AOCS_PENALTIES,

  /* ADDBA configuration ioctles */
  PRM_ID_BE_BAUSE,
  PRM_ID_BK_BAUSE,
  PRM_ID_VI_BAUSE,
  PRM_ID_VO_BAUSE,
  PRM_ID_BE_BAACCEPT,
  PRM_ID_BK_BAACCEPT,
  PRM_ID_VI_BAACCEPT,
  PRM_ID_VO_BAACCEPT,
  PRM_ID_BE_BATIMEOUT,
  PRM_ID_BK_BATIMEOUT,
  PRM_ID_VI_BATIMEOUT,
  PRM_ID_VO_BATIMEOUT,
  PRM_ID_BE_BAWINSIZE,
  PRM_ID_BK_BAWINSIZE,
  PRM_ID_VI_BAWINSIZE,
  PRM_ID_VO_BAWINSIZE,
  PRM_ID_BE_AGGRMAXBTS,
  PRM_ID_BK_AGGRMAXBTS,
  PRM_ID_VI_AGGRMAXBTS,
  PRM_ID_VO_AGGRMAXBTS,
  PRM_ID_BE_AGGRMAXPKTS,
  PRM_ID_BK_AGGRMAXPKTS,
  PRM_ID_VI_AGGRMAXPKTS,
  PRM_ID_VO_AGGRMAXPKTS,
  PRM_ID_BE_AGGRMINPTSZ,
  PRM_ID_BK_AGGRMINPTSZ,
  PRM_ID_VI_AGGRMINPTSZ,
  PRM_ID_VO_AGGRMINPTSZ,
  PRM_ID_BE_AGGRTIMEOUT,
  PRM_ID_BK_AGGRTIMEOUT,
  PRM_ID_VI_AGGRTIMEOUT,
  PRM_ID_VO_AGGRTIMEOUT,

  /* WME BSS configuration */
  PRM_ID_BE_AIFSN,
  PRM_ID_BK_AIFSN,
  PRM_ID_VI_AIFSN,
  PRM_ID_VO_AIFSN,
  PRM_ID_BE_CWMAX,
  PRM_ID_BK_CWMAX,
  PRM_ID_VI_CWMAX,
  PRM_ID_VO_CWMAX,
  PRM_ID_BE_CWMIN,
  PRM_ID_BK_CWMIN,
  PRM_ID_VI_CWMIN,
  PRM_ID_VO_CWMIN,
  PRM_ID_BE_TXOP,
  PRM_ID_BK_TXOP,
  PRM_ID_VI_TXOP,
  PRM_ID_VO_TXOP,
  /* WME BSS configuration end */

  /* WME AP configuration */
  PRM_ID_BE_AIFSNAP,
  PRM_ID_BK_AIFSNAP,
  PRM_ID_VI_AIFSNAP,
  PRM_ID_VO_AIFSNAP,
  PRM_ID_BE_CWMAXAP,
  PRM_ID_BK_CWMAXAP,
  PRM_ID_VI_CWMAXAP,
  PRM_ID_VO_CWMAXAP,
  PRM_ID_BE_CWMINAP,
  PRM_ID_BK_CWMINAP,
  PRM_ID_VI_CWMINAP,
  PRM_ID_VO_CWMINAP,
  PRM_ID_BE_TXOPAP,
  PRM_ID_BK_TXOPAP,
  PRM_ID_VI_TXOPAP,
  PRM_ID_VO_TXOPAP,

  /* 11H configuration */
  PRM_ID_11H_ENABLE_SM_CHANNELS,
  PRM_ID_11H_BEACON_COUNT,
  PRM_ID_11H_CHANNEL_AVAILABILITY_CHECK_TIME,
  PRM_ID_11H_EMULATE_RADAR_DETECTION,
  PRM_ID_11H_SWITCH_CHANNEL,
  PRM_ID_11H_NEXT_CHANNEL,
  PRM_ID_11H_RADAR_DETECTION,
  PRM_ID_11H_STATUS,

  /* L2NAT configuration */
  PRM_ID_L2NAT_AGING_TIMEOUT,
  PRM_ID_L2NAT_ADDR_FIRST,
  PRM_ID_L2NAT_DEFAULT_HOST,

  /* DOT11D configuration */
  PRM_ID_11D,
  PRM_ID_11D_RESTORE_DEFAULTS,

  /* MAC watchdog configuration */
  PRM_ID_MAC_WATCHDOG_TIMEOUT_MS,
  PRM_ID_MAC_WATCHDOG_PERIOD_MS,

  /* STADB configuration */
  PRM_ID_STA_KEEPALIVE_TIMEOUT,
  PRM_ID_STA_KEEPALIVE_INTERVAL,
  PRM_ID_AGGR_OPEN_THRESHOLD,

  /* SendQueue configuration */
  PRM_ID_SQ_LIMITS,
  PRM_ID_SQ_PEER_LIMITS,

  /* General Core configuration */
  PRM_ID_BRIDGE_MODE,
  PRM_ID_DBG_SW_WD_ENABLE,
  PRM_ID_RELIABLE_MULTICAST,
  PRM_ID_AP_FORWARDING,
  PRM_ID_SPECTRUM_MODE,
  PRM_ID_NETWORK_MODE,
  PRM_ID_CHANNEL,
  PRM_ID_HIDDEN_SSID,

  /* Master Core configuration */
  PRM_ID_POWER_SELECTION,
  PRM_ID_BSS_BASIC_RATE_SET,
  PRM_ID_CORE_COUNTRIES_SUPPORTED,
  PRM_ID_NICK_NAME,
  PRM_ID_ESSID,
  PRM_ID_BSSID,
  PRM_ID_LEGACY_FORCE_RATE,
  PRM_ID_HT_FORCE_RATE,
  PRM_ID_ACL,
  PRM_ID_ACL_DEL,
  PRM_ID_ACL_RANGE,
  PRM_ID_UP_RESCAN_EXEMPTION_TIME,

  /* HSTDB configuration */
  PRM_ID_WDS_HOST_TIMEOUT,
  PRM_ID_HSTDB_LOCAL_MAC,

  /* Scan configuration */
  PRM_ID_SCAN_CACHE_LIFETIME,
  PRM_ID_BG_SCAN_CH_LIMIT,
  PRM_ID_BG_SCAN_PAUSE,
  PRM_ID_IS_BACKGROUND_SCAN,
  PRM_ID_ACTIVE_SCAN_SSID,

  /* EEPROM configuration */
  PRM_ID_EEPROM,

  /* TX limits configuration */
  PRM_ID_HW_LIMITS,
  PRM_ID_ANT_GAIN,
  PRM_ID_CHANGE_TX_POWER_LIMIT,

  /* QoS configuration */
  PRM_ID_USE_8021Q,

#ifdef MTCFG_IRB_DEBUG
  /* IRB pinger configuration */
  PRM_ID_IRB_PINGER_ENABLED,
  PRM_ID_IRB_PINGER_STATS,
#endif
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  /* PPA directpath configuration */
  PRM_ID_PPA_API_DIRECTPATH,
#endif
  /* COC configuration */
  PRM_ID_COC_LOW_POWER_MODE,

  /* MBSS configuration */
  PRM_ID_VAP_ADD,
  PRM_ID_VAP_DEL,
  PRM_ID_VAP_STA_LIMS,

  /* user requests */
  PRM_ID_AOCS_HISTORY,
  PRM_ID_AOCS_TABLE,
  PRM_ID_AOCS_CHANNELS,

  PRM_ID_REG_LIMITS,
  PRM_ID_CORE_STATUS,
  PRM_ID_DEBUG_L2NAT,
  PRM_ID_DEBUG_MAC_ASSERT,
  PRM_ID_BCL_READ_LM,
  PRM_ID_BCL_READ_UM,
  PRM_ID_BCL_READ_SHRAM,
  PRM_ID_DEBUG_IGMP_READ,
  PRM_ID_RESET_STATS,
  PRM_ID_L2NAT_CLEAR_TABLE,
#ifdef AOCS_DEBUG
  PRM_ID_AOCS_PROC_CL,
#endif
  /* Standart ioctles */
  PRM_ID_CORE_GET_STATS,
  PRM_ID_CORE_RANGE,
  PRM_ID_CORE_MLME,
  PRM_ID_STADB_GET_STATUS,
  PRM_ID_CORE_SCAN,
  PRM_ID_CORE_SCAN_GET_RESULTS,
  PRM_ID_CORE_ENC,
  PRM_ID_CORE_GENIE,
  PRM_ID_CORE_AUTH,
  PRM_ID_CORE_ENCEXT,
  PRM_ID_CORE_MAC_ADDR,

  /* 20/40 coexistence parameters */
  PRM_ID_COEX_MODE,
  PRM_ID_INTOLERANCE_MODE,
  PRM_ID_EXEMPTION_REQ,
  PRM_ID_DELAY_FACTOR,
  PRM_ID_OBSS_SCAN_INTERVAL
};


/*********************************************************************
 *               DF User management interface
 *********************************************************************/
mtlk_df_user_t*
mtlk_df_user_create(mtlk_df_t *df);

void
mtlk_df_user_delete(mtlk_df_user_t* df_user);

int
mtlk_df_user_start(mtlk_df_t *df, mtlk_df_user_t *df_user, mtlk_vap_manager_interface_e intf);

void
mtlk_df_user_stop(mtlk_df_user_t *df_user, mtlk_vap_manager_interface_e intf);

/* User-friendly interface/device name */
const char*
mtlk_df_user_get_name(mtlk_df_user_t *df_user);

/* statistic utility functions */
uint32 mtlk_df_get_stat_info_len(void);
int mtlk_df_get_stat_info_idx(uint32 num);
const char* mtlk_df_get_stat_info_name(uint32 num);

/*********************************************************************
 * IOCTL handlers
 *********************************************************************/
int __MTLK_IFUNC
mtlk_df_ui_iw_bcl_mac_data_get (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_iw_bcl_mac_data_set (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu, char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_stop_lower_mac (struct net_device *dev,
                                       struct iw_request_info *info,
                                       union iwreq_data *wrqu,
                                       char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_mac_calibrate (struct net_device *dev,
                                      struct iw_request_info *info,
                                      union iwreq_data *wrqu,
                                      char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_iw_generic (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_ctrl_mac_gpio (struct net_device *dev,
                                      struct iw_request_info *info,
                                      union iwreq_data *wrqu,
                                      char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getname (struct net_device *dev,
                          struct iw_request_info *info,
                          char *name,
                          char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getmode (struct net_device *dev,
                                struct iw_request_info *info,
                                u32 *mode,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setnick (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getnick (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setessid (struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu,
                           char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getessid (struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu,
                           char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getap (struct net_device *dev,
                              struct iw_request_info *info,
                              union iwreq_data *wrqu,
                              char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getfreq (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setfreq (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setrtsthr (struct net_device *dev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu,
                                  char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getrtsthr (struct net_device *dev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu,
                                  char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_gettxpower (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_settxpower (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setretry (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getretry (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_int (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_int (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_text (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_text (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_intvec (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_intvec (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_addr (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_addr (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_addrvec (struct net_device *dev,
                                    struct iw_request_info *info,
                                    union iwreq_data *wrqu,
                                    char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_addrvec (struct net_device *dev,
                                    struct iw_request_info *info,
                                    union iwreq_data *wrqu,
                                    char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_mac_addr (struct net_device *dev,
                                     struct iw_request_info *info,
                                     union  iwreq_data *wrqu,
                                     char   *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_mac_addr (struct net_device *dev,
                                     struct iw_request_info *info,
                                     union  iwreq_data *wrqu,
                                     char   *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setap (struct net_device *dev,
                              struct iw_request_info *info,
                              struct sockaddr *sa,
                              char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getrange (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setmlme (struct net_device *dev,
                                struct iw_request_info *info,
                                struct iw_point *data,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_scan (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_scan_results (struct net_device *dev,
                                         struct iw_request_info *info,
                                         union iwreq_data *wrqu,
                                         char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getaplist (struct net_device *dev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu,
                                  char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_connection_info (struct net_device *dev,
                                            struct iw_request_info *info,
                                            union iwreq_data *wrqu,
                                            char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setenc (struct net_device *dev,
                               struct iw_request_info *info,
                               union  iwreq_data *wrqu,
                               char   *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getenc (struct net_device *dev,
                               struct iw_request_info *info,
                               union  iwreq_data *wrqu,
                               char   *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setauth (struct net_device *dev,
                                struct iw_request_info *info,
                                union  iwreq_data *wrqu,
                                char   *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getauth (struct net_device *dev,
                                struct iw_request_info *info,
                                struct iw_param *param,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setgenie (struct net_device *dev,
                                 struct iw_request_info *info,
                                 struct iw_point *data,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getencext (struct net_device *dev,
                                  struct iw_request_info *info,
                                  struct iw_point *encoding,
                                  char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setencext (struct net_device *dev,
                                  struct iw_request_info *info,
                                  struct iw_point *encoding,
                                  char *extra);

struct iw_statistics* __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_iw_stats (struct net_device *dev);

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_bcl_drv_data_exchange (struct net_device *dev,
            struct iw_request_info *info,
            union iwreq_data *wrqu, char *extra);

#endif /*__DF_USER_PRIVATE_H__*/
