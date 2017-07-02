/*
 * $Id: mtlk_df_user_tbl.c 12533 2012-02-02 11:24:00Z nayshtut $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Grygorii Strashko
 *
 */

#include "mtlkinc.h"
#include "mtlk_df_user_priv.h"
#include "mhi_umi.h"

#include <linux/wireless.h>

/********************************************************************
 *
 * DF UI tables definitions
 *
 ********************************************************************/

#define LOG_LOCAL_GID   GID_DFUSER
#define LOG_LOCAL_FID   2

/********************************************************************
 * Private definitions
 ********************************************************************/
#define INTVEC_SIZE 32
#define ADDRVEC_SIZE 64

#define TYPE_INT     (IW_PRIV_TYPE_INT|IW_PRIV_SIZE_FIXED|1)
#define TYPE_INTVEC  (IW_PRIV_TYPE_INT|INTVEC_SIZE)
#define TYPE_ADDR    (IW_PRIV_TYPE_ADDR|IW_PRIV_SIZE_FIXED|1)
#define TYPE_ADDRVEC (IW_PRIV_TYPE_ADDR|ADDRVEC_SIZE)
#define TYPE_TEXT    (IW_PRIV_TYPE_CHAR|TEXT_SIZE)

#define GET_PARAMETER( cmd , type , name ) { cmd , 0 , type , name },
#define SET_PARAMETER( cmd , type , name ) { cmd , type , 0 , name },

#define SET_INT( cmd , name )     SET_PARAMETER( cmd , TYPE_INT     , name )
#define GET_INT( cmd , name )     GET_PARAMETER( cmd , TYPE_INT     , name )
#define SET_INTVEC( cmd , name )  SET_PARAMETER( cmd , TYPE_INTVEC  , name )
#define GET_INTVEC( cmd , name )  GET_PARAMETER( cmd , TYPE_INTVEC  , name )
#define SET_ADDR( cmd , name )    SET_PARAMETER( cmd , TYPE_ADDR    , name )
#define GET_ADDR( cmd , name )    GET_PARAMETER( cmd , TYPE_ADDR    , name )
#define SET_ADDRVEC( cmd , name ) SET_PARAMETER( cmd , TYPE_ADDRVEC , name )
#define GET_ADDRVEC( cmd , name ) GET_PARAMETER( cmd , TYPE_ADDRVEC , name )
#define SET_TEXT( cmd , name )    SET_PARAMETER( cmd , TYPE_TEXT    , name )
#define GET_TEXT( cmd , name )    GET_PARAMETER( cmd , TYPE_TEXT    , name )

/********************************************************************
 * IW standard IOCTL handlers table
 ********************************************************************/
static int
iw_handler_nop (struct net_device *dev,
                struct iw_request_info *info,
                union iwreq_data *wrqu,
                char *extra)
{
  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);
  return 0;
}

/*
 * Structures to export the Wireless Handlers
 */
static const iw_handler mtlk_linux_handler[] = {
  iw_handler_nop,                               /* SIOCSIWCOMMIT */
  /* get name == wireless protocol */
  (iw_handler) mtlk_df_ui_linux_ioctl_getname,  /* SIOCGIWNAME */

/* Basic operations */
  /* set network id (pre-802.11) */
  (iw_handler) NULL,                            /* SIOCSIWNWID */
  /* get network id (the cell) */
  (iw_handler) NULL,                            /* SIOCGIWNWID */
  /* set channel/frequency (Hz) */
  (iw_handler) mtlk_df_ui_linux_ioctl_setfreq,  /* SIOCSIWFREQ */
  /* get channel/frequency (Hz) */
  (iw_handler) mtlk_df_ui_linux_ioctl_getfreq,  /* SIOCGIWFREQ */
  /* set operation mode */
  iw_handler_nop,                               /* SIOCSIWMODE */
  /* get operation mode */
  (iw_handler) mtlk_df_ui_linux_ioctl_getmode,  /* SIOCGIWMODE */
  /* set sensitivity (dBm) */
  (iw_handler) NULL,                            /* SIOCSIWSENS */
  /* get sensitivity (dBm) */
  (iw_handler) NULL,                            /* SIOCGIWSENS */

/* Informative stuff */
  /* Unused */
  (iw_handler) NULL,                            /* SIOCSIWRANGE */
  /* Get range of parameters */
  (iw_handler) mtlk_df_ui_linux_ioctl_getrange, /* SIOCGIWRANGE */
  /* Unused */
  (iw_handler) NULL,                      /* SIOCSIWPRIV */
  /* get private ioctl interface info */
  (iw_handler) NULL,                      /* SIOCGIWPRIV */
  /* Unused */
  (iw_handler) NULL,                      /* SIOCSIWSTATS */
  /* Get /proc/net/wireless stats */
  /* SIOCGIWSTATS is strictly used between user space and the kernel, and
   * is never passed to the driver (i.e. the driver will never see it). */
  (iw_handler) NULL,                      /* SIOCGIWSTATS */
  (iw_handler) NULL,                      /* SIOCSIWSPY */
  (iw_handler) NULL,                      /* SIOCGIWSPY */
  /* set spy threshold (spy event) */
  (iw_handler) NULL,                      /* SIOCSIWTHRSPY */
  /* get spy threshold */
  (iw_handler) NULL,                      /* SIOCGIWTHRSPY */

/* Access Point manipulation */
  /* set access point MAC addresses */
  (iw_handler) mtlk_df_ui_linux_ioctl_setap,    /* SIOCSIWAP */
  /* get access point MAC addresses */
  (iw_handler) mtlk_df_ui_linux_ioctl_getap,    /* SIOCGIWAP */
  /* request MLME operation; uses  struct iw_mlme */
  (iw_handler) mtlk_df_ui_linux_ioctl_setmlme,  /* SIOCSIWMLME */
  (iw_handler) mtlk_df_ui_linux_ioctl_getaplist,/* SIOCGIWAPLIST */

  mtlk_df_ui_linux_ioctl_set_scan,              /* SIOCSIWSCAN */
  mtlk_df_ui_linux_ioctl_get_scan_results,      /* SIOCGIWSCAN */

/* 802.11 specific support */
  /* set ESSID (network name) */
  (iw_handler) mtlk_df_ui_linux_ioctl_setessid, /* SIOCSIWESSID */
  /* get ESSID */
  (iw_handler) mtlk_df_ui_linux_ioctl_getessid, /* SIOCGIWESSID */
  /* set node name/nickname */
  (iw_handler) mtlk_df_ui_linux_ioctl_setnick,  /* SIOCSIWNICKN */
  /* get node name/nickname */
  (iw_handler) mtlk_df_ui_linux_ioctl_getnick,  /* SIOCGIWNICKN */

/* Other parameters useful in 802.11 and some other devices */
  (iw_handler) NULL,                      /* -- hole -- */
  (iw_handler) NULL,                      /* -- hole -- */
  (iw_handler) NULL,                      /* SIOCSIWRATE      */
  (iw_handler) NULL,                      /* SIOCGIWRATE      */
  (iw_handler) mtlk_df_ui_linux_ioctl_setrtsthr, /* SIOCSIWRTS */
  (iw_handler) mtlk_df_ui_linux_ioctl_getrtsthr, /* SIOCGIWRTS */
  /* set fragmentation thr (bytes) */
  (iw_handler) NULL,   // TBD?            /* SIOCSIWFRAG */
  /* get fragmentation thr (bytes) */
  (iw_handler) NULL,   // TBD?            /* SIOCGIWFRAG */
  /* set transmit power (dBm) */
  (iw_handler) mtlk_df_ui_linux_ioctl_settxpower, /* SIOCSIWTXPOW */
  /* get transmit power (dBm) */
  (iw_handler) mtlk_df_ui_linux_ioctl_gettxpower, /* SIOCGIWTXPOW */
  /* set retry limits and lifetime */
  (iw_handler) mtlk_df_ui_linux_ioctl_setretry, /* SIOCSIWRETRY */
  /* get retry limits and lifetime */
  (iw_handler) mtlk_df_ui_linux_ioctl_getretry, /* SIOCGIWRETRY */
  (iw_handler) mtlk_df_ui_linux_ioctl_setenc,   /* SIOCSIWENCODE */
  (iw_handler) mtlk_df_ui_linux_ioctl_getenc,   /* SIOCGIWENCODE */
/* Power saving stuff (power management, unicast and multicast) */
  /* set Power Management settings */
  (iw_handler) NULL,                      /* SIOCSIWPOWER */
  /* get Power Management settings */
  (iw_handler) NULL,                      /* SIOCGIWPOWER */
  /* Unused */
  (iw_handler) NULL,                      /* -- hole -- */
  /* Unused */
  (iw_handler) NULL,                      /* -- hole -- */

/* WPA : Generic IEEE 802.11 informatiom element (e.g., for WPA/RSN/WMM).
 * This ioctl uses struct iw_point and data buffer that includes IE id and len
 * fields. More than one IE may be included in the request. Setting the generic
 * IE to empty buffer (len=0) removes the generic IE from the driver. Drivers
 * are allowed to generate their own WPA/RSN IEs, but in these cases, drivers
 * are required to report the used IE as a wireless event, e.g., when
 * associating with an AP. */
  /* set generic IE */
  (iw_handler) mtlk_df_ui_linux_ioctl_setgenie, /* SIOCSIWGENIE */
  /* get generic IE */
  (iw_handler) NULL,                      /* SIOCGIWGENIE */

/* WPA : Authentication mode parameters */
  (iw_handler) mtlk_df_ui_linux_ioctl_setauth,  /* SIOCSIWAUTH */
  (iw_handler) mtlk_df_ui_linux_ioctl_getauth,  /* SIOCGIWAUTH */

/* WPA : Extended version of encoding configuration */
  /* set encoding token & mode */
  (iw_handler) mtlk_df_ui_linux_ioctl_setencext,/* SIOCSIWENCODEEXT */
   /* get encoding token & mode */
  (iw_handler) mtlk_df_ui_linux_ioctl_getencext, /* SIOCGIWENCODEEXT */

/* WPA2 : PMKSA cache management */
  (iw_handler) NULL,                      /* SIOCSIWPMKSA */
};

/********************************************************************
 * IW private get/set IOCTL table
 ********************************************************************/
static const struct iw_priv_args mtlk_linux_privtab[] = {
  SET_ADDR    (SIOCIWFIRSTPRIV + 16                        , "sMAC"            )
  GET_ADDR    (SIOCIWFIRSTPRIV + 17                        , "gMAC"            )

  /* Sub-ioctl handlers */
  SET_INT     (SIOCIWFIRSTPRIV + 0                         , ""                )
  GET_INT     (SIOCIWFIRSTPRIV + 1                         , ""                )
  /* 2, 3 are not used */
  SET_INTVEC  (SIOCIWFIRSTPRIV + 4                         , ""                )
  GET_INTVEC  (SIOCIWFIRSTPRIV + 5                         , ""                )
  SET_ADDR    (SIOCIWFIRSTPRIV + 6                         , ""                )
  GET_ADDR    (SIOCIWFIRSTPRIV + 7                         , ""                )
  SET_TEXT    (SIOCIWFIRSTPRIV + 8                         , ""                )
  GET_TEXT    (SIOCIWFIRSTPRIV + 9                         , ""                )
  SET_ADDRVEC (SIOCIWFIRSTPRIV + 10                        , ""                )
  GET_ADDRVEC (SIOCIWFIRSTPRIV + 11                        , ""                )
  SET_INTVEC  (SIOCIWFIRSTPRIV + 30                        , "sMacGpio"        )
  GET_TEXT    (SIOCIWFIRSTPRIV + 31                        , "gConInfo"        )

  /* Sub-ioctls */
  /* MIB int ioctles */
  SET_INT     (MIB_SHORT_RETRY_LIMIT                       , "sShortRetryLim"  )
  GET_INT     (MIB_SHORT_RETRY_LIMIT                       , "gShortRetryLim"  )
  SET_INT     (MIB_LONG_RETRY_LIMIT                        , "sLongRetryLimit" )
  GET_INT     (MIB_LONG_RETRY_LIMIT                        , "gLongRetryLimit" )
  SET_INT     (MIB_TX_MSDU_LIFETIME                        , "sMSDULifetime"   )
  GET_INT     (MIB_TX_MSDU_LIFETIME                        , "gMSDULifetime"   )
  SET_INT     (MIB_CURRENT_TX_ANTENNA                      , "sPrimaryAntenna" )
  GET_INT     (MIB_CURRENT_TX_ANTENNA                      , "gPrimaryAntenna" )
  SET_INT     (MIB_ADVANCED_CODING_SUPPORTED               , "sAdvancedCoding" )
  GET_INT     (MIB_ADVANCED_CODING_SUPPORTED               , "gAdvancedCoding" )
  SET_INT     (MIB_OVERLAPPING_PROTECTION_ENABLE           , "sBSSProtection"  )
  GET_INT     (MIB_OVERLAPPING_PROTECTION_ENABLE           , "gBSSProtection"  )
  SET_INT     (MIB_OFDM_PROTECTION_METHOD                  , "sERPProtection"  )
  GET_INT     (MIB_OFDM_PROTECTION_METHOD                  , "gERPProtection"  )
  SET_INT     (MIB_HT_PROTECTION_METHOD                    , "s11nProtection"  )
  GET_INT     (MIB_HT_PROTECTION_METHOD                    , "g11nProtection"  )
  SET_INT     (MIB_DTIM_PERIOD                             , "sDTIMPeriod"     )
  GET_INT     (MIB_DTIM_PERIOD                             , "gDTIMPeriod"     )
  SET_INT     (MIB_RECEIVE_AMPDU_MAX_LENGTH                , "sAMPDUMaxLength" )
  GET_INT     (MIB_RECEIVE_AMPDU_MAX_LENGTH                , "gAMPDUMaxLength" )
  SET_INT     (MIB_SM_ENABLE                               , "sChAnnounce"     )
  GET_INT     (MIB_SM_ENABLE                               , "gChAnnounce"     )
  SET_INT     (MIB_BEACON_PERIOD                           , "sBeaconPeriod"   )
  GET_INT     (MIB_BEACON_PERIOD                           , "gBeaconPeriod"   )
  SET_INT     (MIB_CB_DATABINS_PER_SYMBOL                  , "sDatabins"       )
  GET_INT     (MIB_CB_DATABINS_PER_SYMBOL                  , "gDatabins"       )
  SET_INT     (MIB_USE_LONG_PREAMBLE_FOR_MULTICAST         , "sLongPreambleMC" )
  GET_INT     (MIB_USE_LONG_PREAMBLE_FOR_MULTICAST         , "gLongPreambleMC" )
  SET_INT     (MIB_USE_SPACE_TIME_BLOCK_CODE               , "sSTBC"           )
  GET_INT     (MIB_USE_SPACE_TIME_BLOCK_CODE               , "gSTBC"           )
  SET_INT     (MIB_ONLINE_CALIBRATION_ALGO_MASK            , "sOnlineACM"      )
  GET_INT     (MIB_ONLINE_CALIBRATION_ALGO_MASK            , "gOnlineACM"      )
  SET_INT     (MIB_CALIBRATION_ALGO_MASK                   , "sAlgoCalibrMask" )
  GET_INT     (MIB_CALIBRATION_ALGO_MASK                   , "gAlgoCalibrMask" )
  SET_INT     (MIB_POWER_INCREASE_VS_DUTY_CYCLE            , "sPwrVsDutyCycle" )
  GET_INT     (MIB_POWER_INCREASE_VS_DUTY_CYCLE            , "gPwrVsDutyCycle" )
  SET_INT     (MIB_USE_SHORT_CYCLIC_PREFIX                 , "sShortCyclcPrfx" )
  GET_INT     (MIB_USE_SHORT_CYCLIC_PREFIX                 , "gShortCyclcPrfx" )
  SET_INT     (MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED       , "sShortPreamble"  )
  GET_INT     (MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED       , "gShortPreamble"  )
  SET_INT     (MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G      , "sShortSlotTime"  )
  GET_INT     (MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G      , "gShortSlotTime"  )
  SET_INT     (MIB_ACL_MODE                                , "sAclMode"        )
  GET_INT     (MIB_ACL_MODE                                , "gAclMode"        )
  /* backward compatibility - scheduled for removal in 2.3.15 */
  SET_INT     (MIB_DISCONNECT_ON_NACKS_ENABLE              , "sDisconnOnNACKs" ) /* alias */
  GET_INT     (MIB_DISCONNECT_ON_NACKS_ENABLE              , "gDisconnOnNACKs" ) /* alias */

  SET_INT     (MIB_DISCONNECT_ON_NACKS_ENABLE              , "sDiscNACKEnable" )
  GET_INT     (MIB_DISCONNECT_ON_NACKS_ENABLE              , "gDiscNACKEnable" )
  SET_INT     (MIB_DISCONNECT_ON_NACKS_WEIGHT              , "sDiscNACKWeight" )
  GET_INT     (MIB_DISCONNECT_ON_NACKS_WEIGHT              , "gDiscNACKWeight" )

  /* MIB text ioctles */
  SET_TEXT    (MIB_COUNTRY                                 , "sCountry"        )
  GET_TEXT    (MIB_COUNTRY                                 , "gCountry"        )
  SET_TEXT    (MIB_SUPPORTED_TX_ANTENNAS                   , "sTxAntennas"     )
  GET_TEXT    (MIB_SUPPORTED_TX_ANTENNAS                   , "gTxAntennas"     )
  SET_TEXT    (MIB_SUPPORTED_RX_ANTENNAS                   , "sRxAntennas"     )
  GET_TEXT    (MIB_SUPPORTED_RX_ANTENNAS                   , "gRxAntennas"     )

  /* AOCS int ioctles */
  SET_INT     (PRM_ID_AOCS_WEIGHT_CL                       , "sAocsWeightCl"   )
  GET_INT     (PRM_ID_AOCS_WEIGHT_CL                       , "gAocsWeightCl"   )
  SET_INT     (PRM_ID_AOCS_WEIGHT_TX                       , "sAocsWeightTx"   )
  GET_INT     (PRM_ID_AOCS_WEIGHT_TX                       , "gAocsWeightTx"   )
  SET_INT     (PRM_ID_AOCS_WEIGHT_BSS                      , "sAocsWeightBss"  )
  GET_INT     (PRM_ID_AOCS_WEIGHT_BSS                      , "gAocsWeightBss"  )
  SET_INT     (PRM_ID_AOCS_WEIGHT_SM                       , "sAocsWeightSm"   )
  GET_INT     (PRM_ID_AOCS_WEIGHT_SM                       , "gAocsWeightSm"   )
  SET_INT     (PRM_ID_AOCS_CFM_RANK_SW_THRESHOLD           , "sAocsCfmRnkThr"  )
  GET_INT     (PRM_ID_AOCS_CFM_RANK_SW_THRESHOLD           , "gAocsCfmRnkThr"  )
  SET_INT     (PRM_ID_AOCS_SCAN_AGING                      , "sAocsScanAging"  )
  GET_INT     (PRM_ID_AOCS_SCAN_AGING                      , "gAocsScanAging"  )
  SET_INT     (PRM_ID_AOCS_CONFIRM_RANK_AGING              , "sAocsCfmRAging"  )
  GET_INT     (PRM_ID_AOCS_CONFIRM_RANK_AGING              , "gAocsCfmRAging"  )
  SET_INT     (PRM_ID_AOCS_AFILTER                         , "sAocsAFilter"    )
  GET_INT     (PRM_ID_AOCS_AFILTER                         , "gAocsAFilter"    )
  SET_INT     (PRM_ID_AOCS_BONDING                         , "sAocsBonding"    )
  GET_INT     (PRM_ID_AOCS_BONDING                         , "gAocsBonding"    )
  SET_INT     (PRM_ID_AOCS_EN_PENALTIES                    , "sAocsEnPenalty"  )
  GET_INT     (PRM_ID_AOCS_EN_PENALTIES                    , "gAocsEnPenalty"  )
  SET_INT     (PRM_ID_AOCS_WIN_TIME                        , "sAocsWinTime"    )
  GET_INT     (PRM_ID_AOCS_WIN_TIME                        , "gAocsWinTime"    )
  SET_INT     (PRM_ID_AOCS_MSDU_DEBUG_ENABLED              , "sAocsEnMsduDbg"  )
  GET_INT     (PRM_ID_AOCS_MSDU_DEBUG_ENABLED              , "gAocsEnMsduDbg"  )
  SET_INT     (PRM_ID_AOCS_IS_ENABLED                      , "sAocsIsEnabled"  )
  GET_INT     (PRM_ID_AOCS_IS_ENABLED                      , "gAocsIsEnabled"  )
  SET_INT     (PRM_ID_AOCS_MSDU_THRESHOLD                  , "sAocsMsduThr"    )
  GET_INT     (PRM_ID_AOCS_MSDU_THRESHOLD                  , "gAocsMsduThr"    )
  SET_INT     (PRM_ID_AOCS_LOWER_THRESHOLD                 , "sAocsLwrThr"     )
  GET_INT     (PRM_ID_AOCS_LOWER_THRESHOLD                 , "gAocsLwrThr"     )
  SET_INT     (PRM_ID_AOCS_THRESHOLD_WINDOW                , "sAocsThrWindow"  )
  GET_INT     (PRM_ID_AOCS_THRESHOLD_WINDOW                , "gAocsThrWindow"  )
  SET_INT     (PRM_ID_AOCS_MSDU_PER_WIN_THRESHOLD          , "sAocsMsduWinThr" )
  GET_INT     (PRM_ID_AOCS_MSDU_PER_WIN_THRESHOLD          , "gAocsMsduWinThr" )
  SET_INT     (PRM_ID_AOCS_MEASUREMENT_WINDOW              , "sAocsMeasurWnd"  )
  GET_INT     (PRM_ID_AOCS_MEASUREMENT_WINDOW              , "gAocsMeasurWnd"  )
  SET_INT     (PRM_ID_AOCS_THROUGHPUT_THRESHOLD            , "sAocsThrThr"     )
  GET_INT     (PRM_ID_AOCS_THROUGHPUT_THRESHOLD            , "gAocsThrThr"     )
  SET_INT     (PRM_ID_AOCS_NON_OCCUPANCY_PERIOD            , "sNonOccupatePrd" )
  GET_INT     (PRM_ID_AOCS_NON_OCCUPANCY_PERIOD            , "gNonOccupatePrd" )

  /* AOCS text ioctles */
  SET_TEXT    (PRM_ID_AOCS_RESTRICTED_CHANNELS             , "sAocsRestrictCh" )
  GET_TEXT    (PRM_ID_AOCS_RESTRICTED_CHANNELS             , "gAocsRestrictCh" )
  SET_TEXT    (PRM_ID_AOCS_MSDU_TX_AC                      , "sAocsMsduTxAc"   )
  GET_TEXT    (PRM_ID_AOCS_MSDU_TX_AC                      , "gAocsMsduTxAc"   )
  SET_TEXT    (PRM_ID_AOCS_MSDU_RX_AC                      , "sAocsMsduRxAc"   )
  GET_TEXT    (PRM_ID_AOCS_MSDU_RX_AC                      , "gAocsMsduRxAc"   )

  /* AOCS intvec ioctles */
  SET_INTVEC  (PRM_ID_AOCS_PENALTIES                       , "sAocsPenalty"    )

  /* 11H int ioctles */
  SET_INT     (PRM_ID_11H_RADAR_DETECTION                  , "s11hRadarDetect" )
  GET_INT     (PRM_ID_11H_RADAR_DETECTION                  , "g11hRadarDetect" )
  SET_INT     (PRM_ID_11H_ENABLE_SM_CHANNELS               , "s11hEnSMChnls"   )
  GET_INT     (PRM_ID_11H_ENABLE_SM_CHANNELS               , "g11hEnSMChnls"   )
  SET_INT     (PRM_ID_11H_BEACON_COUNT                     , "s11hBeaconCount" )
  GET_INT     (PRM_ID_11H_BEACON_COUNT                     , "g11hBeaconCount" )
  SET_INT     (PRM_ID_11H_CHANNEL_AVAILABILITY_CHECK_TIME  , "s11hChCheckTime" )
  GET_INT     (PRM_ID_11H_CHANNEL_AVAILABILITY_CHECK_TIME  , "g11hChCheckTime" )
  SET_INT     (PRM_ID_11H_EMULATE_RADAR_DETECTION          , "s11hEmulatRadar" )
  SET_INT     (PRM_ID_11H_SWITCH_CHANNEL                   , "s11hSwitchChnnl" )
  SET_INT     (PRM_ID_11H_NEXT_CHANNEL                     , "s11hNextChannel" )
  GET_INT     (PRM_ID_11H_NEXT_CHANNEL                     , "g11hNextChannel" )

  /* 11H text ioctles */
  GET_TEXT    (PRM_ID_11H_STATUS                           , "g11hStatus"      )

  /* ADDBA configuration ioctles */
  /* BE */
  SET_INT     (PRM_ID_BE_BAUSE                             , "sBE.BAUse"       )
  GET_INT     (PRM_ID_BE_BAUSE                             , "gBE.BAUse"       )
  SET_INT     (PRM_ID_BE_BAACCEPT                          , "sBE.BAAccept"    )
  GET_INT     (PRM_ID_BE_BAACCEPT                          , "gBE.BAAccept"    )
  SET_INT     (PRM_ID_BE_BATIMEOUT                         , "sBE.BATimeout"   )
  GET_INT     (PRM_ID_BE_BATIMEOUT                         , "gBE.BATimeout"   )
  SET_INT     (PRM_ID_BE_BAWINSIZE                         , "sBE.BAWinSize"   )
  GET_INT     (PRM_ID_BE_BAWINSIZE                         , "gBE.BAWinSize"   )
  SET_INT     (PRM_ID_BE_AGGRMAXBTS                        , "sBE.AggrMaxBts"  )
  GET_INT     (PRM_ID_BE_AGGRMAXBTS                        , "gBE.AggrMaxBts"  )
  SET_INT     (PRM_ID_BE_AGGRMAXPKTS                       , "sBE.AggrMaxPkts" )
  GET_INT     (PRM_ID_BE_AGGRMAXPKTS                       , "gBE.AggrMaxPkts" )
  SET_INT     (PRM_ID_BE_AGGRMINPTSZ                       , "sBE.AggrMinPtSz" )
  GET_INT     (PRM_ID_BE_AGGRMINPTSZ                       , "gBE.AggrMinPtSz" )
  SET_INT     (PRM_ID_BE_AGGRTIMEOUT                       , "sBE.AggrTimeout" )
  GET_INT     (PRM_ID_BE_AGGRTIMEOUT                       , "gBE.AggrTimeout" )
  SET_INT     (PRM_ID_BE_AIFSN                             , "sBE.AIFSN"       )
  GET_INT     (PRM_ID_BE_AIFSN                             , "gBE.AIFSN"       )
  SET_INT     (PRM_ID_BE_AIFSNAP                           , "sBE.AIFSNAP"     )
  GET_INT     (PRM_ID_BE_AIFSNAP                           , "gBE.AIFSNAP"     )
  SET_INT     (PRM_ID_BE_CWMAX                             , "sBE.CWMax"       )
  GET_INT     (PRM_ID_BE_CWMAX                             , "gBE.CWMax"       )
  SET_INT     (PRM_ID_BE_CWMAXAP                           , "sBE.CWMaxAP"     )
  GET_INT     (PRM_ID_BE_CWMAXAP                           , "gBE.CWMaxAP"     )
  SET_INT     (PRM_ID_BE_CWMIN                             , "sBE.CWMin"       )
  GET_INT     (PRM_ID_BE_CWMIN                             , "gBE.CWMin"       )
  SET_INT     (PRM_ID_BE_CWMINAP                           , "sBE.CWMinAP"     )
  GET_INT     (PRM_ID_BE_CWMINAP                           , "gBE.CWMinAP"     )
  SET_INT     (PRM_ID_BE_TXOP                              , "sBE.TXOP"        )
  GET_INT     (PRM_ID_BE_TXOP                              , "gBE.TXOP"        )
  SET_INT     (PRM_ID_BE_TXOPAP                            , "sBE.TXOPAP"      )
  GET_INT     (PRM_ID_BE_TXOPAP                            , "gBE.TXOPAP"      )

  /* BK */
  SET_INT     (PRM_ID_BK_BAUSE                             , "sBK.BAUse"       )
  GET_INT     (PRM_ID_BK_BAUSE                             , "gBK.BAUse"       )
  SET_INT     (PRM_ID_BK_BAACCEPT                          , "sBK.BAAccept"    )
  GET_INT     (PRM_ID_BK_BAACCEPT                          , "gBK.BAAccept"    )
  SET_INT     (PRM_ID_BK_BATIMEOUT                         , "sBK.BATimeout"   )
  GET_INT     (PRM_ID_BK_BATIMEOUT                         , "gBK.BATimeout"   )
  SET_INT     (PRM_ID_BK_BAWINSIZE                         , "sBK.BAWinSize"   )
  GET_INT     (PRM_ID_BK_BAWINSIZE                         , "gBK.BAWinSize"   )
  SET_INT     (PRM_ID_BK_AGGRMAXBTS                        , "sBK.AggrMaxBts"  )
  GET_INT     (PRM_ID_BK_AGGRMAXBTS                        , "gBK.AggrMaxBts"  )
  SET_INT     (PRM_ID_BK_AGGRMAXPKTS                       , "sBK.AggrMaxPkts" )
  GET_INT     (PRM_ID_BK_AGGRMAXPKTS                       , "gBK.AggrMaxPkts" )
  SET_INT     (PRM_ID_BK_AGGRMINPTSZ                       , "sBK.AggrMinPtSz" )
  GET_INT     (PRM_ID_BK_AGGRMINPTSZ                       , "gBK.AggrMinPtSz" )
  SET_INT     (PRM_ID_BK_AGGRTIMEOUT                       , "sBK.AggrTimeout" )
  GET_INT     (PRM_ID_BK_AGGRTIMEOUT                       , "gBK.AggrTimeout" )
  SET_INT     (PRM_ID_BK_AIFSN                             , "sBK.AIFSN"       )
  GET_INT     (PRM_ID_BK_AIFSN                             , "gBK.AIFSN"       )
  SET_INT     (PRM_ID_BK_AIFSNAP                           , "sBK.AIFSNAP"     )
  GET_INT     (PRM_ID_BK_AIFSNAP                           , "gBK.AIFSNAP"     )
  SET_INT     (PRM_ID_BK_CWMAX                             , "sBK.CWMax"       )
  GET_INT     (PRM_ID_BK_CWMAX                             , "gBK.CWMax"       )
  SET_INT     (PRM_ID_BK_CWMAXAP                           , "sBK.CWMaxAP"     )
  GET_INT     (PRM_ID_BK_CWMAXAP                           , "gBK.CWMaxAP"     )
  SET_INT     (PRM_ID_BK_CWMIN                             , "sBK.CWMin"       )
  GET_INT     (PRM_ID_BK_CWMIN                             , "gBK.CWMin"       )
  SET_INT     (PRM_ID_BK_CWMINAP                           , "sBK.CWMinAP"     )
  GET_INT     (PRM_ID_BK_CWMINAP                           , "gBK.CWMinAP"     )
  SET_INT     (PRM_ID_BK_TXOP                              , "sBK.TXOP"        )
  GET_INT     (PRM_ID_BK_TXOP                              , "gBK.TXOP"        )
  SET_INT     (PRM_ID_BK_TXOPAP                            , "sBK.TXOPAP"      )
  GET_INT     (PRM_ID_BK_TXOPAP                            , "gBK.TXOPAP"      )

  /* VI */
  SET_INT     (PRM_ID_VI_BAUSE                             , "sVI.BAUse"       )
  GET_INT     (PRM_ID_VI_BAUSE                             , "gVI.BAUse"       )
  SET_INT     (PRM_ID_VI_BAACCEPT                          , "sVI.BAAccept"    )
  GET_INT     (PRM_ID_VI_BAACCEPT                          , "gVI.BAAccept"    )
  SET_INT     (PRM_ID_VI_BATIMEOUT                         , "sVI.BATimeout"   )
  GET_INT     (PRM_ID_VI_BATIMEOUT                         , "gVI.BATimeout"   )
  SET_INT     (PRM_ID_VI_BAWINSIZE                         , "sVI.BAWinSize"   )
  GET_INT     (PRM_ID_VI_BAWINSIZE                         , "gVI.BAWinSize"   )
  SET_INT     (PRM_ID_VI_AGGRMAXBTS                        , "sVI.AggrMaxBts"  )
  GET_INT     (PRM_ID_VI_AGGRMAXBTS                        , "gVI.AggrMaxBts"  )
  SET_INT     (PRM_ID_VI_AGGRMAXPKTS                       , "sVI.AggrMaxPkts" )
  GET_INT     (PRM_ID_VI_AGGRMAXPKTS                       , "gVI.AggrMaxPkts" )
  SET_INT     (PRM_ID_VI_AGGRMINPTSZ                       , "sVI.AggrMinPtSz" )
  GET_INT     (PRM_ID_VI_AGGRMINPTSZ                       , "gVI.AggrMinPtSz" )
  SET_INT     (PRM_ID_VI_AGGRTIMEOUT                       , "sVI.AggrTimeout" )
  GET_INT     (PRM_ID_VI_AGGRTIMEOUT                       , "gVI.AggrTimeout" )
  SET_INT     (PRM_ID_VI_AIFSN                             , "sVI.AIFSN"       )
  GET_INT     (PRM_ID_VI_AIFSN                             , "gVI.AIFSN"       )
  SET_INT     (PRM_ID_VI_AIFSNAP                           , "sVI.AIFSNAP"     )
  GET_INT     (PRM_ID_VI_AIFSNAP                           , "gVI.AIFSNAP"     )
  SET_INT     (PRM_ID_VI_CWMAX                             , "sVI.CWMax"       )
  GET_INT     (PRM_ID_VI_CWMAX                             , "gVI.CWMax"       )
  SET_INT     (PRM_ID_VI_CWMAXAP                           , "sVI.CWMaxAP"     )
  GET_INT     (PRM_ID_VI_CWMAXAP                           , "gVI.CWMaxAP"     )
  SET_INT     (PRM_ID_VI_CWMIN                             , "sVI.CWMin"       )
  GET_INT     (PRM_ID_VI_CWMIN                             , "gVI.CWMin"       )
  SET_INT     (PRM_ID_VI_CWMINAP                           , "sVI.CWMinAP"     )
  GET_INT     (PRM_ID_VI_CWMINAP                           , "gVI.CWMinAP"     )
  SET_INT     (PRM_ID_VI_TXOP                              , "sVI.TXOP"        )
  GET_INT     (PRM_ID_VI_TXOP                              , "gVI.TXOP"        )
  SET_INT     (PRM_ID_VI_TXOPAP                            , "sVI.TXOPAP"      )
  GET_INT     (PRM_ID_VI_TXOPAP                            , "gVI.TXOPAP"      )

  /* VO */
  SET_INT     (PRM_ID_VO_BAUSE                             , "sVO.BAUse"       )
  GET_INT     (PRM_ID_VO_BAUSE                             , "gVO.BAUse"       )
  SET_INT     (PRM_ID_VO_BAACCEPT                          , "sVO.BAAccept"    )
  GET_INT     (PRM_ID_VO_BAACCEPT                          , "gVO.BAAccept"    )
  SET_INT     (PRM_ID_VO_BATIMEOUT                         , "sVO.BATimeout"   )
  GET_INT     (PRM_ID_VO_BATIMEOUT                         , "gVO.BATimeout"   )
  SET_INT     (PRM_ID_VO_BAWINSIZE                         , "sVO.BAWinSize"   )
  GET_INT     (PRM_ID_VO_BAWINSIZE                         , "gVO.BAWinSize"   )
  SET_INT     (PRM_ID_VO_AGGRMAXBTS                        , "sVO.AggrMaxBts"  )
  GET_INT     (PRM_ID_VO_AGGRMAXBTS                        , "gVO.AggrMaxBts"  )
  SET_INT     (PRM_ID_VO_AGGRMAXPKTS                       , "sVO.AggrMaxPkts" )
  GET_INT     (PRM_ID_VO_AGGRMAXPKTS                       , "gVO.AggrMaxPkts" )
  SET_INT     (PRM_ID_VO_AGGRMINPTSZ                       , "sVO.AggrMinPtSz" )
  GET_INT     (PRM_ID_VO_AGGRMINPTSZ                       , "gVO.AggrMinPtSz" )
  SET_INT     (PRM_ID_VO_AGGRTIMEOUT                       , "sVO.AggrTimeout" )
  GET_INT     (PRM_ID_VO_AGGRTIMEOUT                       , "gVO.AggrTimeout" )
  SET_INT     (PRM_ID_VO_AIFSN                             , "sVO.AIFSN"       )
  GET_INT     (PRM_ID_VO_AIFSN                             , "gVO.AIFSN"       )
  SET_INT     (PRM_ID_VO_AIFSNAP                           , "sVO.AIFSNAP"     )
  GET_INT     (PRM_ID_VO_AIFSNAP                           , "gVO.AIFSNAP"     )
  SET_INT     (PRM_ID_VO_CWMAX                             , "sVO.CWMax"       )
  GET_INT     (PRM_ID_VO_CWMAX                             , "gVO.CWMax"       )
  SET_INT     (PRM_ID_VO_CWMAXAP                           , "sVO.CWMaxAP"     )
  GET_INT     (PRM_ID_VO_CWMAXAP                           , "gVO.CWMaxAP"     )
  SET_INT     (PRM_ID_VO_CWMIN                             , "sVO.CWMin"       )
  GET_INT     (PRM_ID_VO_CWMIN                             , "gVO.CWMin"       )
  SET_INT     (PRM_ID_VO_CWMINAP                           , "sVO.CWMinAP"     )
  GET_INT     (PRM_ID_VO_CWMINAP                           , "gVO.CWMinAP"     )
  SET_INT     (PRM_ID_VO_TXOP                              , "sVO.TXOP"        )
  GET_INT     (PRM_ID_VO_TXOP                              , "gVO.TXOP"        )
  SET_INT     (PRM_ID_VO_TXOPAP                            , "sVO.TXOPAP"      )
  GET_INT     (PRM_ID_VO_TXOPAP                            , "gVO.TXOPAP"      )

  /* L2NAT int ioctles */
  SET_INT     (PRM_ID_L2NAT_AGING_TIMEOUT                  , "sL2NATAgeTO"     )
  GET_INT     (PRM_ID_L2NAT_AGING_TIMEOUT                  , "gL2NATAgeTO"     )

  /* L2NAT addres ioctles */
  SET_ADDR    (PRM_ID_L2NAT_DEFAULT_HOST                   , "sL2NATDefHost"   )
  GET_ADDR    (PRM_ID_L2NAT_DEFAULT_HOST                   , "gL2NATDefHost"   )

  /* 11D int ioctles */
  SET_INT     (PRM_ID_11D                                  , "s11dActive"      )
  GET_INT     (PRM_ID_11D                                  , "g11dActive"      )
  SET_INT     (PRM_ID_11D_RESTORE_DEFAULTS                 , "s11dReset"       )

  /* MAC watchdog int ioctles */
  SET_INT     (PRM_ID_MAC_WATCHDOG_TIMEOUT_MS              , "sMACWdTimeoutMs" )
  GET_INT     (PRM_ID_MAC_WATCHDOG_TIMEOUT_MS              , "gMACWdTimeoutMs" )
  SET_INT     (PRM_ID_MAC_WATCHDOG_PERIOD_MS               , "sMACWdPeriodMs"  )
  GET_INT     (PRM_ID_MAC_WATCHDOG_PERIOD_MS               , "gMACWdPeriodMs"  )

  /* STADB watchdog int ioctles */
  SET_INT     (PRM_ID_STA_KEEPALIVE_TIMEOUT                , "sStaKeepaliveTO" )
  GET_INT     (PRM_ID_STA_KEEPALIVE_TIMEOUT                , "gStaKeepaliveTO" )
  SET_INT     (PRM_ID_STA_KEEPALIVE_INTERVAL               , "sStaKeepaliveIN" )
  GET_INT     (PRM_ID_STA_KEEPALIVE_INTERVAL               , "gStaKeepaliveIN" )
  SET_INT     (PRM_ID_AGGR_OPEN_THRESHOLD                  , "sAggrOpenThrsh"  )
  GET_INT     (PRM_ID_AGGR_OPEN_THRESHOLD                  , "gAggrOpenThrsh"  )

  /* SendQueue intvec ioctles */
  SET_INTVEC  (PRM_ID_SQ_LIMITS                            , "sSQLimits"       )
  GET_INTVEC  (PRM_ID_SQ_LIMITS                            , "gSQLimits"       )
  SET_INTVEC  (PRM_ID_SQ_PEER_LIMITS                       , "sSQPeerLimits"   )
  GET_INTVEC  (PRM_ID_SQ_PEER_LIMITS                       , "gSQPeerLimits"   )

  /* General Core int ioctles */
  SET_INT     (PRM_ID_BRIDGE_MODE                          , "sBridgeMode"     )
  GET_INT     (PRM_ID_BRIDGE_MODE                          , "gBridgeMode"     )
  SET_INT     (PRM_ID_RELIABLE_MULTICAST                   , "sReliableMcast"  )
  GET_INT     (PRM_ID_RELIABLE_MULTICAST                   , "gReliableMcast"  )
  SET_INT     (PRM_ID_AP_FORWARDING                        , "sAPforwarding"   )
  GET_INT     (PRM_ID_AP_FORWARDING                        , "gAPforwarding"   )
  SET_INT     (PRM_ID_SPECTRUM_MODE                        , "sFortyMHzOpMode" )
  GET_INT     (PRM_ID_SPECTRUM_MODE                        , "gFortyMHzOpMode" )
  SET_INT     (PRM_ID_POWER_SELECTION                      , "sPowerSelection" )
  GET_INT     (PRM_ID_POWER_SELECTION                      , "gPowerSelection" )
  SET_INT     (PRM_ID_NETWORK_MODE                         , "sNetworkMode"    )
  GET_INT     (PRM_ID_NETWORK_MODE                         , "gNetworkMode"    )
  SET_INT     (PRM_ID_IS_BACKGROUND_SCAN                   , "sIsBGScan"       )
  GET_INT     (PRM_ID_IS_BACKGROUND_SCAN                   , "gIsBGScan"       )
  SET_INT     (PRM_ID_BSS_BASIC_RATE_SET                   , "sBasicRateSet"   )
  GET_INT     (PRM_ID_BSS_BASIC_RATE_SET                   , "gBasicRateSet"   )
  SET_INT     (PRM_ID_HIDDEN_SSID                          , "sHiddenSSID"     )
  GET_INT     (PRM_ID_HIDDEN_SSID                          , "gHiddenSSID"     )
  SET_INT     (PRM_ID_UP_RESCAN_EXEMPTION_TIME             , "sUpRescanExmpTm" )
  GET_INT     (PRM_ID_UP_RESCAN_EXEMPTION_TIME             , "gUpRescanExmpTm" )

  /* General Core text ioctles */
  SET_TEXT    (PRM_ID_LEGACY_FORCE_RATE                    , "sFixedRate"      )
  GET_TEXT    (PRM_ID_LEGACY_FORCE_RATE                    , "gFixedRate"      )
  SET_TEXT    (PRM_ID_HT_FORCE_RATE                        , "sFixedHTRate"    )
  GET_TEXT    (PRM_ID_HT_FORCE_RATE                        , "gFixedHTRate"    )
  GET_TEXT    (PRM_ID_CORE_COUNTRIES_SUPPORTED             , "gCountries"      )

  /* General Core addrvec ioctles */
  SET_ADDRVEC (PRM_ID_ACL                                  , "sACL"            )
  GET_ADDRVEC (PRM_ID_ACL                                  , "gACL"            )
  SET_ADDRVEC (PRM_ID_ACL_DEL                              , "sDelACL"         )
  SET_ADDRVEC (PRM_ID_ACL_RANGE                            , "sACLRange"       )
  GET_ADDRVEC (PRM_ID_ACL_RANGE                            , "gACLRange"       )

  /* HSTDB int ioctles */
  SET_INT     (PRM_ID_WDS_HOST_TIMEOUT                     , "sWDSHostTO"      )
  GET_INT     (PRM_ID_WDS_HOST_TIMEOUT                     , "gWDSHostTO"      )

  /* HSTDB addr ioctles */
  SET_ADDR    (PRM_ID_HSTDB_LOCAL_MAC                      , "sL2NATLocMAC"    )
  GET_ADDR    (PRM_ID_HSTDB_LOCAL_MAC                      , "gL2NATLocMAC"    )

  /* Scan int ioctles */
  SET_INT     (PRM_ID_SCAN_CACHE_LIFETIME                  , "sScanCacheLifeT" )
  GET_INT     (PRM_ID_SCAN_CACHE_LIFETIME                  , "gScanCacheLifeT" )
  SET_INT     (PRM_ID_BG_SCAN_CH_LIMIT                     , "sBGScanChLimit"  )
  GET_INT     (PRM_ID_BG_SCAN_CH_LIMIT                     , "gBGScanChLimit"  )
  SET_INT     (PRM_ID_BG_SCAN_PAUSE                        , "sBGScanPause"    )
  GET_INT     (PRM_ID_BG_SCAN_PAUSE                        , "gBGScanPause"    )

  /* Scan text ioctles */
  SET_TEXT    (PRM_ID_ACTIVE_SCAN_SSID                     , "sActiveScanSSID" )
  GET_TEXT    (PRM_ID_ACTIVE_SCAN_SSID                     , "gActiveScanSSID" )

  /* EEPROM text ioctles */
  GET_TEXT    (PRM_ID_EEPROM                               , "gEEPROM"         )

  /* TX limits text ioctles */
  SET_TEXT    (PRM_ID_HW_LIMITS                            , "sHWLimit"        )
  SET_TEXT    (PRM_ID_ANT_GAIN                             , "sAntGain"        )

  /* Qos int ioctles */
  SET_INT     (PRM_ID_USE_8021Q                            , "sUse1QMap"       )
  GET_INT     (PRM_ID_USE_8021Q                            , "gUse1QMap"       )

#ifdef MTCFG_IRB_DEBUG
  /* IRB pinger int ioctles */
  SET_INT     (PRM_ID_IRB_PINGER_ENABLED                   , "sIRBPngEnabled"  )
  GET_INT     (PRM_ID_IRB_PINGER_ENABLED                   , "gIRBPngEnabled"  )
  SET_INT     (PRM_ID_IRB_PINGER_STATS                     , "sIRBPngStatsRst" )

  /* IRB pinger int ioctles */
  GET_TEXT    (PRM_ID_IRB_PINGER_STATS                     , "gIRBPngStats"    )
#endif
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  /* PPA directpath int ioctles */
  SET_INT     (PRM_ID_PPA_API_DIRECTPATH                   , "sIpxPpaEnabled"  )
  GET_INT     (PRM_ID_PPA_API_DIRECTPATH                   , "gIpxPpaEnabled"  )
#endif
  /* COC int ioctles */
  SET_INT     (PRM_ID_COC_LOW_POWER_MODE                   , "sCoCLowPower"    )
  GET_INT     (PRM_ID_COC_LOW_POWER_MODE                   , "gCoCLowPower"    )
  /* MBSS ioctls*/
  SET_INT     (PRM_ID_VAP_ADD                              , "sAddVap"         )
  SET_INT     (PRM_ID_VAP_DEL                              , "sDelVap"         )
  SET_INTVEC  (PRM_ID_VAP_STA_LIMS                         , "sVapSTALimits"   )
  GET_INTVEC  (PRM_ID_VAP_STA_LIMS                         , "gVapSTALimits"   )

  SET_INT     (PRM_ID_CHANGE_TX_POWER_LIMIT                , "sTxPowerLimOpt"  )

  /* 20/40 coexistence */
  SET_INT     (PRM_ID_COEX_MODE                           , "sCoexistenceMode" )
  GET_INT     (PRM_ID_COEX_MODE                           , "gCoexistenceMode" )

  SET_INT     (PRM_ID_INTOLERANCE_MODE                    , "sIntoleranceMode" )
  GET_INT     (PRM_ID_INTOLERANCE_MODE                    , "gIntoleranceMode" )

  SET_INT     (PRM_ID_EXEMPTION_REQ                        , "sExemptionReq"   )
  GET_INT     (PRM_ID_EXEMPTION_REQ                        , "gExemptionReq"   )

  SET_INT     (PRM_ID_DELAY_FACTOR                         , "sDelayFactor"    )
  GET_INT     (PRM_ID_DELAY_FACTOR                         , "gDelayFactor"    )

  SET_INT     (PRM_ID_OBSS_SCAN_INTERVAL                   , "sOBSSScnIntrvl"  )
  GET_INT     (PRM_ID_OBSS_SCAN_INTERVAL                   , "gOBSSScnIntrvl"  )

  /* Capabilities */
  GET_INT     (PRM_ID_AP_CAPABILITIES_MAX_STAs             , "gAPCapsMaxSTAs"  )
  GET_INT     (PRM_ID_AP_CAPABILITIES_MAX_VAPs             , "gAPCapsMaxVAPs"  )

  /* FW GPIO LED */
  SET_INTVEC  (PRM_ID_CFG_LED_GPIO                         , "sFWCfgLEDGPIO"   )
  GET_INTVEC  (PRM_ID_CFG_LED_GPIO                         , "gFWCfgLEDGPIO"   )

  SET_INTVEC  (PRM_ID_CFG_LED_STATE                        , "sFWCfgLEDState"  )
};

/********************************************************************
 * IW private IOCTL handlers table
 ********************************************************************/
static const iw_handler mtlk_linux_private_handler[] = {
  [ 0] = mtlk_df_ui_linux_ioctl_set_int,
  [ 1] = mtlk_df_ui_linux_ioctl_get_int,
  [ 4] = mtlk_df_ui_linux_ioctl_set_intvec,
  [ 5] = mtlk_df_ui_linux_ioctl_get_intvec,
  [ 6] = mtlk_df_ui_linux_ioctl_set_addr,
  [ 7] = mtlk_df_ui_linux_ioctl_get_addr,
  [ 8] = mtlk_df_ui_linux_ioctl_set_text,
  [ 9] = mtlk_df_ui_linux_ioctl_get_text,
  [10] = mtlk_df_ui_linux_ioctl_set_addrvec,
  [11] = mtlk_df_ui_linux_ioctl_get_addrvec,
  [16] = mtlk_df_ui_linux_ioctl_set_mac_addr,
  [17] = mtlk_df_ui_linux_ioctl_get_mac_addr,
  [20] = mtlk_df_ui_iw_bcl_mac_data_get,
  [21] = mtlk_df_ui_iw_bcl_mac_data_set,
  [22] = mtlk_df_ui_linux_ioctl_bcl_drv_data_exchange,
  [26] = mtlk_df_ui_linux_ioctl_stop_lower_mac,
  [27] = mtlk_df_ui_linux_ioctl_mac_calibrate,
  [28] = mtlk_df_ui_linux_ioctl_iw_generic,
  [30] = mtlk_df_ui_linux_ioctl_ctrl_mac_gpio,
  [31] = mtlk_df_ui_linux_ioctl_get_connection_info,
};

/********************************************************************
 * IW driver IOCTL descriptor table
 ********************************************************************/
const struct iw_handler_def mtlk_linux_handler_def = {
  .num_standard = ARRAY_SIZE(mtlk_linux_handler),
  .num_private = ARRAY_SIZE(mtlk_linux_private_handler),
  .num_private_args = ARRAY_SIZE(mtlk_linux_privtab),
  .standard = (iw_handler *) mtlk_linux_handler,
  .private = (iw_handler *) mtlk_linux_private_handler,
  .private_args = (struct iw_priv_args *) mtlk_linux_privtab,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  .get_wireless_stats = mtlk_df_ui_linux_ioctl_get_iw_stats,
#endif
};


/********************************************************************
 * Statistic fields mapping and description table
 * - defines Statistic fields mapping between MAC and DF UI
 ********************************************************************/
struct stat_print_info_t
{
  int idx;
  const char *name;
};

struct stat_print_info_t stat_info[] =
{
  { STAT_RX_UNICAST_DATA,    "Unicast data frames received" },
  { STAT_RX_DUPLICATE,       "Duplicate frames received" },
  { STAT_RX_MULTICAST_DATA,  "Multicast frames received" },
  { STAT_RX_DISCARD,         "Frames Discarded" },
  { STAT_RX_UNKNOWN,         "Unknown RX" },
  { STAT_RX_UNAUTH,          "Reception From Unauthenticated STA" },
  { STAT_RX_UNASSOC,         "AP: Frames RX from Unassociated STA" },
  { STAT_RX_INTS,            "RX Interrupts" },
  { STAT_RX_CONTROL,         "RX Control type frames" },

  { STAT_RX_TOTAL_MANAGMENT_PACKETS,  "Total management packets received" },
  { STAT_TX_TOTAL_MANAGMENT_PACKETS,  "Total management packets transmitted" },
  { STAT_RX_TOTAL_DATA_PACKETS,       "Total packets received" },

  { STAT_BEACON_TX,          "Beacons Sent" },
  { STAT_BEACON_RX,          "Beacons Received" },
  { STAT_AUTHENTICATE_TX,    "Authentication Requests Sent" },
  { STAT_AUTHENTICATE_RX,    "Authentication Requests Received" },

  { STAT_ASSOC_REQ_TX,       "Association Requests Sent" },
  { STAT_ASSOC_REQ_RX,       "Association Requests Received" },
  { STAT_ASSOC_RES_TX,       "Association Replies Sent" },
  { STAT_ASSOC_RES_RX,       "Association Replies Received" },

  { STAT_REASSOC_REQ_TX,     "ReAssociation Requests Sent" },
  { STAT_REASSOC_REQ_RX,     "ReAssociation Requests Received" },
  { STAT_REASSOC_RES_TX,     "ReAssociation Replies Sent" },
  { STAT_REASSOC_RES_RX,     "ReAssociation Replies Received" },

  { STAT_DEAUTH_TX,          "Deauthentication Notifications Sent" },
  { STAT_DEAUTH_RX,          "Deauthentication Notifications Received" },

  { STAT_DISASSOC_TX,        "Disassociation Notifications Sent" },
  { STAT_DISASSOC_RX,        "Disassociation Notifications Received" },

  { STAT_PROBE_REQ_TX,       "Probe Requests sent" },
  { STAT_PROBE_REQ_RX,       "Probe Requests received" },
  { STAT_PROBE_RES_TX,       "Probe Responses sent" },
  { STAT_PROBE_RES_RX,       "Probe Responses received" },

  { STAT_ATIM_TX,            "ATIMs Transmitted successfully" },
  { STAT_ATIM_RX,            "ATIMs Received" },
  { STAT_ATIM_TX_FAIL,       "ATIMs Failing transmission" },

  { STAT_TX_MSDU,            "TX MSDUs that have been sent" },

  { STAT_TX_FAIL,            "TX frames that have failed" },
  { STAT_TX_RETRY,           "TX retries to date" },
  { STAT_TX_DEFER_PS,        "Transmits deferred due to Power Mgmnt" },
  { STAT_TX_DEFER_UNAUTH,    "Transmit deferred pending authentication" },

  { STAT_BEACON_TIMEOUT,     "Beacon Timeouts" },
  { STAT_AUTH_TIMEOUT,       "Authentication Timeouts" },
  { STAT_ASSOC_TIMEOUT,      "Association Timeouts" },
  { STAT_ROAM_SCAN_TIMEOUT,  "Roam Scan timeout" },

  { STAT_WEP_TOTAL_PACKETS,  "Total number of packets passed through WEP" },
  { STAT_WEP_EXCLUDED,       "Unencrypted packets received when WEP is active" },
  { STAT_WEP_UNDECRYPTABLE,  "Packets with no valid keys for decryption " },
  { STAT_WEP_ICV_ERROR,      "Packets with incorrect WEP ICV" },
  { STAT_TX_PS_POLL,         "TX PS POLL" },
  { STAT_RX_PS_POLL,         "RX PS POLL" },

  { STAT_MAN_ACTION_TX,      "Management Actions sent" },
  { STAT_MAN_ACTION_RX,      "Management Actions received" },

  { STAT_OUT_OF_RX_MSDUS,    "Out of RX MSDUs" },

  { STAT_HOST_TX_REQ,        "Requests from PC to TX data - UM_DAT_TXDATA_REQ" },
  { STAT_HOST_TX_CFM,        "Confirm to PC by MAC of TX data - MC_DAT_TXDATA_CFM" },
  { STAT_BSS_DISCONNECT,     "Station remove from database due to beacon/data timeout" },

  { STAT_RX_DUPLICATE_WITH_RETRY_BIT_0, "Duplicate frames received with retry bit set to 0" },

  { STAT_RX_NULL_DATA,       "Total number of received NULL DATA packets" },
  { STAT_TX_NULL_DATA,       "Total number of sent NULL DATA packets" },

  { STAT_RX_BAR,             "BAR received" },
  { STAT_TX_BAR,             "BAR sent" },
  { STAT_TX_BAR_FAIL,        "BAR fail" },

  { STAT_RX_FAIL_NO_DECRYPTION_KEY, "RX Failures due to no key loaded" },
  { STAT_RX_DECRYPTION_SUCCESSFUL,  "RX decryption successful" },

  { STAT_NUM_UNI_PS_INACTIVE,       "Unicast packets in PS-Inactive queue" },
  { STAT_NUM_MULTI_PS_INACTIVE,     "Multicast packets in PS-Inactive queue" },
  { STAT_TOT_PKS_PS_INACTIVE,       "Total number of packets in PS-Inactive queue" },
  { STAT_NUM_MULTI_PS_ACTIVE,       "Multicast packets in PS-Active queue" },
  { STAT_NUM_TIME_IN_PS,            "Time in power-save" },

  { STAT_WDS_TX_UNICAST,            "Unicast WDS frames transmitted" },
  { STAT_WDS_TX_MULTICAST,          "Multicast WDS frames transmitted" },
  { STAT_WDS_RX_UNICAST,            "Unicast WDS frames received" },
  { STAT_WDS_RX_MULTICAST,          "Multicast WDS frames received" },

  { STAT_CTS2SELF_TX,       "CTS2SELF packets that have been sent" },
  { STAT_CTS2SELF_TX_FAIL,  "CTS2SELF packets that have failed" },
  { STAT_DECRYPTION_FAILED, "frames with decryption failed" },
  { STAT_FRAGMENT_FAILED,   "frames with wrong fragment number" },
  { STAT_TX_MAX_RETRY,      "TX dropped packets with retry limit exceeded" },

  { STAT_TX_RTS_SUCCESS,    "RTS succeeded" },
  { STAT_TX_RTS_FAIL,       "RTS failed" },
  { STAT_TX_MULTICAST_DATA, "transmitted multicast frames" },
  { STAT_FCS_ERROR,         "FCS errors" },

  { STAT_RX_ADDBA_REQ,      "Received ADDBA Request frames" },
  { STAT_RX_ADDBA_RES,      "Received ADDBA Response frames" },
  { STAT_RX_DELBA_PKT,      "Deceived DELBA frames" },

};

/* Statistic table functions */
uint32 mtlk_df_get_stat_info_len(void)
{
  return ARRAY_SIZE(stat_info);
}

int mtlk_df_get_stat_info_idx(uint32 num)
{
  MTLK_ASSERT(num < ARRAY_SIZE(stat_info));
  return stat_info[num].idx;
}

const char* mtlk_df_get_stat_info_name(uint32 num)
{
  MTLK_ASSERT(num < ARRAY_SIZE(stat_info));
  return stat_info[num].name;
}
