/*
 * $Id: mtlkparams.h 11751 2011-10-17 14:36:09Z moshe $
 *
 * Copyright (c) 2006-2007 Metalink Broadband Ltd.
 *
 * Shared parameter names
 *
 * Author: Vasyl' Nikolaenko
 *
 */

#ifndef __MTLK_PARAMS_H
#define __MTLK_PARAMS_H

/*
 * Common parameters
 */
#define PRM_CHANNEL                             MTLK_TEXT("Channel")
#define PRM_ESSID                               MTLK_TEXT("ESSID")
#define PRM_REGULATORY_DOMAIN                   MTLK_TEXT("RegulatoryDomain")
#define PRM_COUNTRY                             MTLK_TEXT("Country")
#define PRM_AUTHENTIFICATION                    MTLK_TEXT("Authentication")
#define PRM_WEP_ENCRYPTION                      MTLK_TEXT("WepEncryption")
#define PRM_WEP_KEYS                            MTLK_TEXT("WepKeys")
#define PRM_DEFAULT_KEY0                        MTLK_TEXT("DefaultKey0")
#define PRM_DEFAULT_KEY1                        MTLK_TEXT("DefaultKey1")
#define PRM_DEFAULT_KEY2                        MTLK_TEXT("DefaultKey2")
#define PRM_DEFAULT_KEY3                        MTLK_TEXT("DefaultKey3")
#define PRM_FRAGMENT_THRESHOLD                  MTLK_TEXT("FragmentThreshold")
#define PRM_RTS_THRESHOLD                       MTLK_TEXT("RTSThreshold")
#define PRM_BEACON_PERIOD                       MTLK_TEXT("BeaconPeriod")
#define PRM_QOS                                 MTLK_TEXT("QOS")
#define PRM_DATABINS                            MTLK_TEXT("Databins")
#define PRM_ADVANCED_CODING                     MTLK_TEXT("AdvancedCoding")
#define PRM_PRIMARY_ANTENNA                     MTLK_TEXT("PrimaryAntenna")
#define PRM_SHORT_RETRY_LIMIT                   MTLK_TEXT("ShortRetryLimit")
#define PRM_LONG_RETRY_LIMIT                    MTLK_TEXT("LongRetryLimit")
#define PRM_TX_MSDU_LIFETIME                    MTLK_TEXT("TxMsduLifetime")
#define PRM_USE_LONG_PREAMBLE_FOR_MULTICAST     MTLK_TEXT("UseLongPreambleForMulticast")
#define PRM_STBC                                MTLK_TEXT("STBC")
#define PRM_OVERLAPPING_BSS_PROTECTION          MTLK_TEXT("OverlappingBSSProtection")
#define PRM_ERP_PROTECTION_TYPE                 MTLK_TEXT("ERPProtectionType")
#define PRM_11N_PROTECTION_TYPE                 MTLK_TEXT("11nProtectionType")
#define PRM_USE_AGGREGATION                     MTLK_TEXT("UseAggregation")
#define PRM_ACCEPT_AGGREGATION                  MTLK_TEXT("AcceptAggregation")
#define PRM_MAX_NUM_OF_PACKETS                  MTLK_TEXT("MaxNumOfPackets")
#define PRM_MAX_NUM_OF_BYTES                    MTLK_TEXT("MaxNumOfBytes")
#define PRM_TIMEOUT_INTERVAL                    MTLK_TEXT("TimeoutInterval")
#define PRM_MIN_SIZE_OF_PACKET_IN_AGGR          MTLK_TEXT("MinSizeOfPacketInAggr")
#define PRM_ADDBA_TIMEOUT                       MTLK_TEXT("ADDBATimeout")
#define PRM_AGGREGATION_WINDOW_SIZE             MTLK_TEXT("AggregationWindowSize")
#define PRM_CW_MIN                              MTLK_TEXT("CWmin")
#define PRM_CW_MAX                              MTLK_TEXT("CWmax")
#define PRM_AIFSN                               MTLK_TEXT("AIFSN")
#define PRM_TX_OP_LIMIT                         MTLK_TEXT("TXOP")
#define PRM_CW_MIN_AP                           MTLK_TEXT("CWmin_AP")
#define PRM_CW_MAX_AP                           MTLK_TEXT("CWmax_AP")
#define PRM_AIFSN_AP                            MTLK_TEXT("AIFSN_AP")
#define PRM_TX_OP_LIMIT_AP                      MTLK_TEXT("TXOP_AP")
#define PRM_DTIM_PERIOD                         MTLK_TEXT("DTIMPeriod")
#define PRM_AMPDU_MAX_LENGTH                    MTLK_TEXT("AMPDUMaxLength")
#define PRM_LOG_ON_INIT                         MTLK_TEXT("LogOnInit")     /* Logger support */
#define PRM_LOG_ENABLE                          MTLK_TEXT("LogEnable")     /* Logger support */
#define PRM_DEBUG_OUTPUT                        MTLK_TEXT("DebugOutput")   /* Logger support */
#define PRM_ALGO_ONLINE_CALIBR_MASK             MTLK_TEXT("AlgoOnLineCalibrMask")
#define PRM_BEACON_COUNT                        MTLK_TEXT("BeaconCount")
#define PRM_CHANNEL_AVAILABILITY_CHECK_TIME     MTLK_TEXT("ChannelAvailabilityCheckTime")
#define PRM_11_H_RADAR_DETECTION_ENABLED        MTLK_TEXT("11hRadarDetect")
#define PRM_POWER_CONSTRAINT                    MTLK_TEXT("PowerConstraint")
#define PRM_POWER_SELECTION                     MTLK_TEXT("PowerSelection")
#define PRM_DBG_SOFTWARE_RESET_ENEABLE          MTLK_TEXT("SoftwareWatchdogEnable")

/*
 * Linux driver only parameters
 */
#define PRM_HIDDEN_SSID                         MTLK_TEXT("HiddenSSID")
#define PRM_WDS_ENABLED	                        MTLK_TEXT("WDSEnabled")

/*
 * Windows driver only parameters
 */
#define PRM_BOOTLOADER                          MTLK_TEXT("Bootloader")
#define PRM_STA_UPPER                           MTLK_TEXT("STAupper")
#define PRM_STA_LOWER                           MTLK_TEXT("STAlower")
#define PRM_AP_UPPER                            MTLK_TEXT("APupper")
#define PRM_AP_LOWER                            MTLK_TEXT("APlower")
#define PRM_TEST_UPPER                          MTLK_TEXT("TestUpper")
#define PRM_TEST_LOWER                          MTLK_TEXT("TestLower")
#define PRM_NETWORK_TYPE                        MTLK_TEXT("NetworkType")
#define PRM_BIN_FOLDER                          MTLK_TEXT("BinFolder")
#define PRM_POWER_SAVING                        MTLK_TEXT("PowerSaving")
#define PRM_LONG_KEYS                           MTLK_TEXT("LongKeys")
#define PRM_MAX_AGGR_SUPPORTED                  MTLK_TEXT("MaxAggrSupported")
#define PRM_MAX_REORD_SUPPORTED                 MTLK_TEXT("MaxReordSupported")
#define PRM_TERMINATE_TIMEOUT                   MTLK_TEXT("TerminateTimeout") /* Reordering debugging (REORDER_DEBUG) */
#define PRM_ACTIVE_PRIORITY                     MTLK_TEXT("ActivePriority")   /* Reordering debugging (REORDER_DEBUG) */
#define PRM_AC_BE_USE_AGGREGATION               MTLK_TEXT("AC_BE_UseAggregation")
#define PRM_AC_BE_ACCEPT_AGGREGATION            MTLK_TEXT("AC_BE_AcceptAggregation")
#define PRM_AC_BK_USE_AGGREGATION               MTLK_TEXT("AC_BK_UseAggregation")
#define PRM_AC_BK_ACCEPT_AGGREGATION            MTLK_TEXT("AC_BK_AcceptAggregation")
#define PRM_AC_VI_USE_AGGREGATION               MTLK_TEXT("AC_VI_UseAggregation")
#define PRM_AC_VI_ACCEPT_AGGREGATION            MTLK_TEXT("AC_VI_AcceptAggregation")
#define PRM_AC_VO_USE_AGGREGATION               MTLK_TEXT("AC_VO_UseAggregation")
#define PRM_AC_VO_ACCEPT_AGGREGATION            MTLK_TEXT("AC_VO_AcceptAggregation")
#define PRM_HW_LIMIT_TABLE_DIR                  MTLK_TEXT("Configuration\\HW_LIMIT_TABLE")
#define PRM_ANTENNA_GAIN_TABLE_DIR              MTLK_TEXT("Configuration\\ANTENNA_GAIN")
#define PRM_ANTENNA_GAIN_TABLE_ENTRY            MTLK_TEXT("0")

#define PRM_ACL                               MTLK_TEXT("ACL")

#endif // __MTLK_PARAMS_H
