/****************************************************************************
 ****************************************************************************
 **
 ** COMPONENT:      Managed Object Definitions
 **
 ** MODULE:         $File: //bwp/enet/demo153_sw/develop/src/shared/mhi_mib_id.h $
 **
 ** VERSION:        $Revision: #8 $
 **
 ** DATED:          $Date: 2004/02/20 $
 **
 ** AUTHOR:         S Sondergaard
 **
 ** DESCRIPTION:    External interface for managed objects
 **
 ****************************************************************************
 *
 *  Copyright (c) TTPCom Limited., 2003
 *
 *  Copyright (c) Metalink Ltd., 2006 - 2007
 *
 ****************************************************************************/

#ifndef  __MHI_MIB_ID_INCLUDED
#define  __MHI_MIB_ID_INCLUDED

#define   MTLK_PACK_ON
#include "mtlkpack.h"

/***************************************************************************/
/***                   Include File Dependencies                         ***/
/***************************************************************************/

/***************************************************************************/
/***                  Global data                                        ***/
/***************************************************************************/

/*----Macro Definitions-----------------------------------------------------*/
/* Keep up to date with hi_motab.h */


/* MIB key length validations */
#define MIB_WEP_KEY_INVALID_LENGTH          0       /* WEP key is not set */
#define MIB_WEP_KEY_WEP1_LENGTH             5       /* WEP key type 1 is 40 bits (5 bytes) */
#define MIB_WEP_KEY_WEP2_LENGTH             13      /* WEP key type 2 is 104 bits (13 bytes) */
                                                    /* define other values for other keys here */

#define NO_RATE                             0xFFFF

#define FORCED_RATE_LEGACY_MASK             0x0001
#define FORCED_RATE_HT_MASK                 0x0002

/***************************************************************************/
/***                       RSN MIB Objects                               ***/
/***************************************************************************/

/* RSN Control */
#define RSNMIB_DEFAULT_REPLAY_WINDOW        (16)

typedef struct _RSNMIB_CONTROL
{
    uint8   u8IsRsnEnabled;
    uint8   u8IsTsnEnabled;
    uint8   reserved[2];
} __MTLK_PACKED RSNMIB_CONTROL;

/* Pairwise alert thresholds */
typedef struct _RSNMIB_PAIRWISE_ALERT_THRESHOLDS
{
    uint16  u16TkipIcvErrorThreshold;
    uint16  u16TkipMicFailureThreshold;
    uint16  u16TkipReplayThreshold;
    uint16  u16CcmpMicFailureThreshold;
    uint16  u16CcmpReplayThreshold;
    uint16  u16NumTxThreshold;
} __MTLK_PACKED RSNMIB_PAIRWISE_ALERT_THRESHOLDS;

/* Group alert thresholds */
typedef struct _RSNMIB_GROUP_ALERT_THRESHOLDS
{
    uint16  u16TkipIcvErrorThreshold;
    uint16  u16TkipMicFailureThreshold;
    uint16  u16CcmpMicFailureThreshold;
    uint16  u16NumTxThreshold;
} __MTLK_PACKED RSNMIB_GROUP_ALERT_THRESHOLDS;

/* RSN security parameters in RSN IE format */
#define MIB_RSN_PARAMETERS_LENGTH           (40)
/*--------------------------------------------------------------------------
 * Managed Object Type Definitions
 *
 * Description:     WME Parameters
 *--------------------------------------------------------------------------*/
/* Constant definitions for WME */

#define MAX_USER_PRIORITIES                 4


typedef struct _MIB_WME_TABLE
{
    uint8   au8CWmin[MTLK_PAD4(MAX_USER_PRIORITIES)];  /* Log CW min */
    uint16  au16Cwmax[MTLK_PAD4(MAX_USER_PRIORITIES)]; /* Log CW max */
    uint8   au8AIFS[MTLK_PAD4(MAX_USER_PRIORITIES)];
    uint16  au16TXOPlimit[MTLK_PAD4(MAX_USER_PRIORITIES)];
    uint16  auMSDULifeTime[MTLK_PAD4(MAX_USER_PRIORITIES)];
    uint8   auAdmissionControl[MTLK_PAD4(MAX_USER_PRIORITIES)];
} __MTLK_PACKED MIB_WME_TABLE;



typedef struct _FM_WME_OUI_AND_VER
{
    uint8  au8OUI[3];
    uint8  u8OUItype;
    uint8  u8OUIsubType;
    uint8  u8Version;
    uint8  u8QosInfoField;
    uint8  u8Reserved;
} __MTLK_PACKED FM_WME_OUI_AND_VER;

typedef struct _PRE_ACTIVATE_MIB_TYPE
{
    uint8  u8Reserved[3]                   ; // for 32 bit alignment
	uint8  u8NetworkMode                   ; // MIB_NETWORK_MODE uses NETWORK_MODES_E values
    uint8  u8UpperLowerChannel             ; // MIB_UPPER_LOWER_CHANNEL_BONDING
    uint8  u8SpectrumMode                  ; // MIB_SPECTRUM_MODE
    uint8  u8ShortSlotTimeOptionEnabled11g ; // MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G
    uint8  u8ShortPreambleOptionImplemented; // MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED
    uint32 u32OperationalRateSet           ; // MIB_OPERATIONAL_RATE_SET
    uint32 u32BSSbasicRateSet              ; // MIB_BSS_BASIC_RATE_SET
} __MTLK_PACKED PRE_ACTIVATE_MIB_TYPE;

typedef enum _NETWORK_MODES 
{
	NETWORK_11B_ONLY,
	NETWORK_11G_ONLY,
	NETWORK_11N_2_4_ONLY,
	NETWORK_11BG_MIXED,
	NETWORK_11GN_MIXED,
	NETWORK_11BGN_MIXED,
	NETWORK_11A_ONLY,
	NETWORK_11N_5_ONLY,
	NETWORK_11AN_MIXED,
	NUM_OF_NETWORK_MODES
} NETWORK_MODES_E;

/*--------------------------------------------------------------------------
 * Managed Object definitions
*
* Description:     The representation of unsigned 8-bits value
*
*--------------------------------------------------------------------------*/
typedef uint8  MIB_UINT8;

/*--------------------------------------------------------------------------
* Managed Object definitions
*
* Description:     The representation of unsigned 16-bits value
*
*--------------------------------------------------------------------------*/
typedef uint16 MIB_UINT16;

/*--------------------------------------------------------------------------
* Managed Object definitions
*
* Description:     The representation of unsigned 32-bits value
*
*--------------------------------------------------------------------------*/
typedef uint32 MIB_UINT32;

/*--------------------------------------------------------------------------
* Managed Object definitions
*
* Description:     The representation of unsigned 64-bits value
*
*--------------------------------------------------------------------------*/
#ifdef __ghs
typedef struct _MIB_UINT64
{
    uint32 ui32Low;
    uint32 ui32High;
} MIB_UINT64;
#else
typedef uint64 MIB_UINT64;
#endif


/*--------------------------------------------------------------------------
* Managed Object definitions
*
* Description:     TPC Object definitions
*
* Representation:
*
*--------------------------------------------------------------------------*/
#define TPC_FREQ_STR_NUM        (4)
#define TPC_FREQ_POINT_NUM      (5)

typedef struct _TPC_FREQ
{
    uint8   chID;
    uint8   BackoffMultiplier;
    uint8   MaxTxPowerIndex;
    uint8   Backoff;
// These are occupy 4 bytes
    uint16  X_axis[ MTLK_PAD2( TPC_FREQ_POINT_NUM ) ];
// Array is assigned to 4-byte boundary
    uint8   Y_axis[ MTLK_PAD4( TPC_FREQ_POINT_NUM ) ];
} __MTLK_PACKED TPC_FREQ;


typedef struct _List_Of_Tpc
{
    TPC_FREQ  sTPCerFreq [TPC_FREQ_STR_NUM];
} __MTLK_PACKED List_Of_Tpc;
/*--------------------------------------------------------------------------
 * Managed Object Type Definitions
 *
 * Description:     EEPROM Version ID, Number of points in TPC_FREQ structure
 *
 * Representation:  MIB_MANUFACTURER_ID
 *
 *--------------------------------------------------------------------------*/
typedef struct __EEPROM_VERSION_TYPE
{
 	uint16 u16EEPROMVersion;
 	uint8 u8NumberOfPoints5GHz;
 	uint8 u8NumberOfPoints2GHz;
} __MTLK_PACKED EEPROM_VERSION_TYPE;


/*--------------------------------------------------------------------------
 * Managed Object definitions
 *
 * Description:     The name of the manufacturer
 *
 * Representation:  MIB_MANUFACTURER_ID
 *
 *--------------------------------------------------------------------------*/
/* Max length of manufacturer id (including terminating null) */
#define     MIB_MANUFACTURER_ID_LENGTH      16

typedef struct _MIB_MAN_ID
{
    char acName[MTLK_PAD4(MIB_MANUFACTURER_ID_LENGTH)];
} __MTLK_PACKED MIB_MAN_ID;



/*--------------------------------------------------------------------------
 * Managed Object Type Definitions
 *
 * Description:     The name/id of the product
 *
 * Representation:  MIB_MANUFACTURER_ID
 *
 *--------------------------------------------------------------------------*/
/* Max length of manufacturer id (including terminating null) */
#define     MIB_PRODUCT_ID_LENGTH           16

typedef struct _MIB_PROD_ID
{
    char acName[MTLK_PAD4(MIB_PRODUCT_ID_LENGTH)];
} __MTLK_PACKED MIB_PROD_ID;

/*--------------------------------------------------------------------------
 * Managed Object Type Definition
 *
 * Description:     ESS ID
 *
 * Representation:  MIB_ESSID
 *
 *--------------------------------------------------------------------------*/
/* Maximum length of ESSID */
#define     MIB_ESSID_LENGTH                32

typedef struct _MIB_ESS_ID
{
    /* ESSID is a counted string.
    * A zero length implies broadcast address for scanning */
    uint8 u8Length;
    uint8 reserved[3];
    char  acESSID[MTLK_PAD4(MIB_ESSID_LENGTH + 1)]; /* + 1 allows zero termination for debug output */
} __MTLK_PACKED MIB_ESS_ID;

/*--------------------------------------------------------------------------
 * Managed Object Type Definition
 *
 * Description:     Set of uint8 integers.
 *
 * Used to represent sets (e.g. authentication algorithms) of small  integers.
 *--------------------------------------------------------------------------*/
#define MIB_SET_LENGTH                      32

typedef struct _MIB_SET_OF_U8
{
    uint8 u8Nelements;         /* Number of elements in set */
    uint8 reserved[3];
    uint8 au8Elements[MTLK_PAD4(MIB_SET_LENGTH)];
} __MTLK_PACKED MIB_SET_OF_U8;


/*--------------------------------------------------------------------------
 * Managed Object Type Definition
 *
 * Description:     List of uint8 integers.
 *
 * Used to represent lists of small integers.
 *--------------------------------------------------------------------------*/
#define MIB_LIST_LENGTH                     32

typedef struct _MIB_LIST_OF_U8
{
    uint8  au8Elements[MTLK_PAD4(MIB_LIST_LENGTH)];
} __MTLK_PACKED MIB_LIST_OF_U8;

/*--------------------------------------------------------------------------
 * Managed Object Type Definition
 *
 * Description:     Default WEP Keys
 *
 * Contains the default WEP keys
 *--------------------------------------------------------------------------*/
/* The WEP secret key is either 40bits or 104bits */
/* In each case, a 24bit IV is appended to create 64 or 128bit RC4 key */
/* but that is stored in a separate structure */
#define MIB_WEP_KEY_MAX_LENGTH              13

typedef struct _MIB_WEP_KEY
{
    uint8 u8KeyLength;  /* Note: Length is used to infer type */
    uint8 reserved[3];
    uint8 au8KeyData[MTLK_PAD4(MIB_WEP_KEY_MAX_LENGTH)];
} __MTLK_PACKED MIB_WEP_KEY;


#define MIB_WEP_N_DEFAULT_KEYS              4

typedef struct _MIB_WEP_DEF_KEYS
{
    MIB_WEP_KEY  sKey[MTLK_PAD4(MIB_WEP_N_DEFAULT_KEYS)];
} __MTLK_PACKED MIB_WEP_DEF_KEYS;

#define BSS_BASIC_RATE_FLAG                 0x80;

/*
 * MAC related managed objects as specified in IEEE 802.11
 *   For detailed descriptions see IEEE 802.11,
 *   For other information (type etc) see hi_motab.h
 */

#define MAC_MO_BASE                         0x0000

#define MIB_KALISH_THE_INVALID_ID           0x0000

#define MIB_STATION_ID                      0x0001
/* Reserved:                                0x0002 */
#define MIB_DESIRED_ESSID                   0x0003
//#define MIB_PROTOCOL_TYPE                 0x0004 <-- removed 13/8/2007.

#define MIB_PRIVACY_OPTION                  0x0005

#define MIB_AUTHENTICATION_TYPE             0x0006
#define MIB_AUTHENTICATION_PREFERENCE       0x0007
#define MIB_AUTHENTICATION_OPEN_SYSTEM      0
#define MIB_AUTHENTICATION_SHARED_KEY       1
#define MIB_AUTHENTICATION_AUTO				2		// STA connects to AP according to AP's algorithm

#define MIB_PRIVACY_INVOKED                 0x0008
#define MIB_EXCLUDE_UNENCRYPTED             0x0009

/* Reserved:                                0x0010 */
/* Reserved:                                0x0011 */

#define MIB_LISTEN_INTERVAL                 0x0012
/* Reserved:                                0x0013 */
/* Reserved:                                0x0014 */
/* Reserved:                                0x0015 */
#define MIB_DTIM_PERIOD                     0x0016
#define MIB_ATIM_WINDOW                     0x0017
#define MIB_MEDIUM_OCC_LIMIT                0x0018
#define MIB_MEDIUM_OCC_LIMIT_MAX            1000

#define MIB_SHORT_RETRY_LIMIT               0x0019
#define MIB_LONG_RETRY_LIMIT                0x0020

#define MIB_GROUP_0_ADDRESS                 0x0021
#define MIB_GROUP_1_ADDRESS                 0x0022
#define MIB_GROUP_2_ADDRESS                 0x0023
/* Reserved:                                0x0024 */

#define MIB_FRAGMENT_THRESHOLD              0x0025
#define MIB_RTS_THRESHOLD                   0x0026
#define MIB_MANUFACTURER_ID                 0x0027
#define MIB_PRODUCT_ID                      0x0028
#define MIB_RTS_ENABLED                     0x0029
/* Reserved:                                0x0030 */
/* Reserved:                                0x0031 */
/* Reserved:                                0x0032 */
/* Reserved                                 0x0033 */

/* Reserved:                                0x0034 */

/* Reserved:                                0x0035 */

#define MIB_TX_MSDU_LIFETIME                0x0036
#define MIB_RX_MSDU_LIFETIME                0x0037
#define MIB_BEACON_PERIOD                   0x0038
#define     MIB_DEFAULT_BEACON_PERIOD       100 /* TU */

/* Reserved:                                0x0039 */

#define MIB_NETWORK_MODE                    0x003A

#define MIB_IEEE_ADDRESS                    0x0040
#define MIB_ESSID                           0x0041

#define MIB_BSSID                           0x0042

#define MIB_ACTING_AS_AP                    0x0043
/* Relevant for STA only */
/* Reserved:                                0x0044 */

/* Make default channel scan time, beacon time * 1.33 */
#define     MIB_DEFAULT_SCAN_MAX_CHANNEL_TIME (MIB_DEFAULT_BEACON_PERIOD + (MIB_DEFAULT_BEACON_PERIOD / 3))

#define		MIB_SCAN_TYPE                       0x0049
#define     MIB_ST_ACTIVE  0    /* Active - sends probes */
#define     MIB_ST_PASSIVE 1    /* Passive - scan for beacons */

#define     MIB_REG_DOMAIN_APAC   0x50

#define MIB_POWER_SAVING_CONTROL_MODE       0x0052
#define     MIB_PSCM_CAM      0     /* Constant awake mode - ACTIVE     */
#define     MIB_PSCM_PS       1     /* Power-saving                     */
#define     MIB_PSCM_INVALID  2     /* Initial setting for code         */

/* Reserved:                                0x0053 */
/* Reserved:                                0x0054 */


#define MIB_STATION_AID                     0x0055
#define MIB_HIBERNATION_CONTROL_MODE        0x0056
#define     MIB_HCM_DISABLED  0     /* No Hibernation     */
#define     MIB_HCM_STANDARD  1     /* Standard operation */
#define     MIB_HCM_SMART     2     /* Smart    operation */

#define MIB_REQUIRE_IBSS_AUTHENTICATION     0x0057

#define MIB_WEP_DEFAULT_KEYS                0x0060
#define MIB_WEP_DEFAULT_KEYID               0x0061

#define MIB_WEP_KEY_MAPPINGS                0x0062
#define MIB_WEP_KEY_MAPPING_LENGTH          0x0063
#define MIB_WEP_ICV_ERROR_COUNT             0x0064
#define MIB_WEP_EXCLUDED_COUNT              0x0065
#define MIB_WEP_UNDECRYPTABLE_COUNT         0x0066

#define MIB_CFP_POLLABLE                    0x0070
#define MIB_NOT_CF_POLLABLE          0 /* STA is not CF pollable                                    */
#define MIB_CF_POLLABLE_WITH_TX_ONLY 1 /* STA is CF pollable, do no poll (unless TXing)             */
#define MIB_FULLY_CF_POLLABLE        2 /* STA is fully CF pollable                                  */
#define MIB_CF_POLLABLE_DO_NOT_POLL  3 /* STA may RX dtat during CFP but does not wich to be polled */

#define MIB_CFP_PERIOD                      0x0071
#define MIB_CFP_MAX_DURATION                0x0072

#define MIB_AUTH_RESP_TIMEOUT               0x0078
#define MIB_BSS_BASIC_RATE_SET              0x007A
#define MIB_OPERATIONAL_RATE_SET            0x007B
/* #define MIB_MANDATORY_RATE_SET              0x007C */
#define MIB_JOIN_FAILURE_TIMEOUT            0x0080
#define MIB_ASSOCIATE_FAILURE_TIMEOUT       0x0081
#define MIB_REASSOCIATE_FAILURE_TIMEOUT     0x0082


/* Statistics */
#define STAT_MO_BASE 0x0100

#define MIB_TX_FRAGMENT_COUNT               0x0101
/* Reserved:                                0x0102 */
/* Reserved:                                0x0103 */
#define MIB_MULTICAST_TX_COUNT              0x0104
/* Reserved:                                0x0105 */
#define MIB_FAILED_TX_COUNT                 0x0106
#define MIB_RETRY_COUNT                     0x0107
#define MIB_MULTIPLE_RETRY_COUNT            0x0108
#define MIB_PRE_ACTIVATE                    0x0109
#define MIB_RTS_FAILURE                     0x0110
#define MIB_ACK_FAILURE                     0x0111
#define MIB_RX_FRAGMENT_COUNT               0x0112
/* Reserved:                                0x0113 */
#define MIB_MULTICAST_RX_COUNT              0x0114
/* Reserved:                                0x0115 */
/* Reserved:                                0x0116 */
#define MIB_FCS_ERROR_RX_COUNT              0x0117
/* Reserved:                                0x0118 */
/* Reserved:                                0x0119 */
/* Reserved:                                0x0120 */
#define MIB_FRAME_DUPLICATE_COUNT           0x0121

/* PHY Managed Objects */
#define PHY_MO_BASE 0x200

#define MIB_PHY_TYPE                        0x0200
#define     MIB_PHY_TYPE_FHSS     1     /* hopping radio */
#define     MIB_PHY_TYPE_DSS      2     /* direct sequence radio */
#define     MIB_PHY_TYPE_IR       3     /* Infrared */
#define     MIB_PHY_TYPE_OFDM     4     /* OFDM for 80211.b  */

#define MIB_CURRENT_REG_DOMAIN              0x0201
#define MIB_REG_DOMAINS_SUPPORTED           0x0202
#define     MIB_REG_DOMAIN_FCC    0x10
#define     MIB_REG_DOMAIN_DOC    0x20
#define     MIB_REG_DOMAIN_ETSI   0x30
#define     MIB_REG_DOMAIN_SPAIN  0x31
#define     MIB_REG_DOMAIN_FRANCE 0x32
#define		MIB_REG_DOMAIN_UAE	  0x33
#define     MIB_REG_DOMAIN_GERMANY   0x34
#define     MIB_REG_DOMAIN_MKK    0x40

#define     MIB_REG_DOMAIN_TEST_ONE     0x50
#define     MIB_REG_DOMAIN_TEST_FOUR    0x60


#define MIB_CHANNEL_SET                     0x0203

/* #define MIB_CCA_JAM                      0x0203 */


/* Reserved:                                0x0207 */
/* Reserved:                                0x0208 */

#define MIB_SUPPORTED_TX_ANTENNAS           0x0209
#define MIB_CURRENT_TX_ANTENNA              0x0210
/* This value selects Tx Antenna diversity.  This is an enhancement
 * to 802.11.  Other values take range 1..N_ANTENNAS. */
#define     MIB_TX_ANT_DIVERSITY 0

/* Reserved:                                0x0211 */

#define MIB_SUPPORTED_RX_ANTENNAS           0x0212
#define MIB_DIVERSITY_SUPPORT               0x0213
#define     MIB_DIVERSITY_FIXED         1
#define     MIB_DIVERSITY_UNSUPPORTED   2
#define     MIB_DIVERSITY_CONTROLLABLE  3

#define MIB_DIVERSITY_SELECTION_RX          0x0214
/* This value selects Rx Antenna diversity.  This is an enhancement
 * to 802.11.  Other values take range 1..N_ANTENNAS. */
#define     MIB_RX_ANT_DIVERSITY        0
#define     MIB_RX_ANT_1                1
#define     MIB_RX_ANT_2                2

#define MIB_CURRENT_CHANNEL_NUMBER          0x0215
#define MIB_RESTRICTED_CHANNEL_NUMBER       0x0216
#define MIB_ANY_CHANNEL                     0

#define MIB_CURRENT_SET                     0x0217
#define MIB_CURRENT_PATTERN                 0x0218
#define MIB_CURRENT_INDEX                   0x0219
/* Reserved:                                0x0220 */
#define MIB_HOP_DWELL_PERIOD                0x0221
#define MIB_CURRENT_TX_POWER_LEVEL          0x0222
#define MIB_NUMBER_SUPPORTED_POWER_LEVELS   0x0223
#define MIB_TX_POWER                        0x0224
#define MIB_TEMP_TYPE                       0x0228
#define     MIB_TEMP_COMMERCIAL         1 /*   0..40 deg. C */
#define     MIB_TEMP_MILITARY1          2 /* -20..55 deg. C */
#define     MIB_TEMP_MILITARY2          3 /* -30..70 deg. C */


#define MIB_MPDU_MAX_LENGTH                 0x0230
#define MIB_MAX_DWELL_TIME                  0x0231
/* Reserved:                                0x0232 */
/* Reserved:                                0x0233 */
/* Added by eyal.be for phy metrics   */

#define MIB_RSSI_ALPHA_FILTER               0x0234
#define MIB_NOISE_ALPHA_FILTER              0x0235
#define MIB_RF_NOISE_ALPHA_FILTER           0x0236
#define MIB_RF_FREQ_LONG_ALPHA_FILTER       0x0237
#define MIB_RF_FREQ_SHORT_ALPHA_FILTER      0x0238

/* 802.11h - new MIB */

#define MIB_SM_ENABLE                             0x0240
#define MIB_SM_MITIGATION_FACTOR                  0x0241  // This should be added to MIB. Because could be changed during normal BSS operation.
#define MIB_SM_POWER_CAPABILITY_MAX               0x0242
#define MIB_SM_POWER_CAPABILITY_MIN               0x0243
#define MIB_COUNTRY                               0x0246  // These two elements should be added to the MIB. In the AP they are used to constructe the
#define MIB_USER_POWER_SELECTION                  0x0247

/* Direct Multicast*/
#define MIB_ENABLE_DMCAST						  0x0261  // Direct mutlicast enabled (uint16, TRUE/FALSE)
#define MIB_DMCATS_RATE                           0x0262  // Direct mutlicast rate (uin16, rate index)
#define MIB_MCAST_CB_DECISION					  0x0263  // Multicast CB/nCB mode

/* other MIB */
#define MIB_SWRESET_ON_ASSERT                     0x0250  // indicates that MAC should perform SW reset on assert
#define MIB_DISCONNECT_ON_NACKS_ENABLE            0x0251  // indicates that MAC Consecutive NACK model for disconnecting station that left BSS should be enabled
#define MIB_DISCONNECT_ON_NACKS_WEIGHT            0x0252  // indicates the weight level on which the MAC Consecutive NACK model should disconnect a station

#define MIB_CHANNELS_MAX_GROUPS                   32

typedef struct _MIB_CHANNELS_TYPE_ENTRY
{
    uint8  u8FirstChannelNumber;
    uint8  u8NumberOfChannels;
    uint8  u8MaximumTransmitPowerLevel;
    uint8  u8Reserved;
} __MTLK_PACKED MIB_CHANNELS_TYPE_ENTRY;

typedef struct _MIB_DMCAST_TYPE
{
	uint16		bEnable;
	IEEE_ADDR	sMcastAddress;
} __MTLK_PACKED MIB_DMCAST_TYPE;


#define MAX_CHANNEL_TYPE_COUNTRIES 3

typedef struct _MIB_CHANNELS_TYPE
{
    uint8 Country[MTLK_PAD4(MAX_CHANNEL_TYPE_COUNTRIES)];
    MIB_CHANNELS_TYPE_ENTRY Table[MIB_CHANNELS_MAX_GROUPS];
} __MTLK_PACKED MIB_CHANNELS_TYPE;


/*----PRIVATE MANAGED OBJECTS-----------------------------------------------
 * The following managed objects are not part of the 802.11 specification,
 * but are required for the operation of the MAC software.
 *--------------------------------------------------------------------------*/
#define     MIB_PRIVATE_BASE                0x1000

/*--------------------------------------------------------------------------
 * Managed Object:      MIB_MAC_ENABLED
 *
 * Description:     Enable state of mac.   False (zero) prior to a
 *                  MIB_R_ENABLE_MAC request.   True (non-zero) after.
 *
 * Representation:  uint16 u16Nbytes
 *
 * Access:          r   at any time
 *--------------------------------------------------------------------------*/
#define     MIB_MAC_ENABLED                 0x1003

/* Reserved: 0x1004 */
/* Reserved: 0x1005 */
/* Reserved: 0x1006 */
/* Reserved: 0x1007 */
/* Reserved: 0x1008 */
/* Reserved: 0x1008 */
/* Reserved: 0x1009 */
/* Reserved: 0x100A */
/* Reserved: 0x100B */

/*--------------------------------------------------------------------------
 * Managed Object:      MIB_EEPROM_VERSION
 *
 * Description:     EEPROM version number
 * Representation:  uint16
 *
 * Access:          w
  *--------------------------------------------------------------------------*/
#define     MIB_EEPROM_VERSION              0x100C

/*--------------------------------------------------------------------------
 * Managed Object:      MIB_SW_VERSION
 *
 * Description:     Software version number
 * Representation:  uint16.   Decimal fixed-point 2-places
 *                  e.g. 123 represents V1.23.
 *
 * Access:          r   - At any time
 *--------------------------------------------------------------------------*/
#define     MIB_SW_VERSION                  0x100D

/*--------------------------------------------------------------------------
 * Managed Object:      MIB_ASIC_VER
 *
 * Description:     ASIC ID and version number
 * Representation:  uint16
 *
 * Access:          r   - At any time
 *--------------------------------------------------------------------------*/
#define     MIB_ASIC_VER                    0x100E
/* Reserved: 0x100F */
/* Reserved: 0x1010 */
/* Reserved: 0x1011 */
/* Reserved: 0x1012 */


/*--------------------------------------------------------------------------
 * Managed Object:      MIB_INFRASTRUCTURE
 *
 * Description:     ESS infrastructure indicator
 *                  TRUE => infrastructure network
 *                  FALSE => ad-hoc network.
 * Representation:
 *
 * Access:          rw  - Prior to Adopt Configuration
 *                  r   - Thereafter
 *--------------------------------------------------------------------------*/

#define     MIB_INFRASTRUCTURE              0x1013

/*--------------------------------------------------------------------------
 * Managed Object:      MIB_RX_MODE
 *
 * Description:     Receive packet address filter mode.
 *
 * Representation:  uint16 (see below).        One of the MIB_RXM_* values.
 *
 * Access:          rw  - Prior to Adopt Configuration
 *                  r   - Thereafter
 *--------------------------------------------------------------------------*/
#define     MIB_RXM_ADDRESSED     0 /* Packets addressed to me + broadcast */
#define     MIB_RXM_MULTICAST     1 /* Packets addressed to me + multicast */
#define     MIB_RXM_ALL           2 /* All Rx packets (promiscuous) */
#define     MIB_RX_MODE                     0x1014

/* Reserved: 0x1015 */
/* Reserved: 0x1016 */

/*--------------------------------------------------------------------------
 * Managed Object:      MIB_START_NET_TIMEOUT
 *
 * Description:     Time for which an station attempting to start
 *                  its own BSS will look for a suitable channel.
 *
 * Representation:  uint16 S
 *
 * Access:          rw  - Prior to Adopt Configuration
 *                  r   - Thereafter
 *--------------------------------------------------------------------------*/
#define     MIB_START_NET_TIMEOUT           0x1017


/*--------------------------------------------------------------------------
 * Managed Object:      MIB_BEACON_TIMEOUT
 *
 * Description:     Time after the last beacon RXed for the current BSS which
 *                  a station will look for another BSS.
 *
 * Representation:  uint16 S
 *
 * Access:          rw  - Prior to Adopt Configuration
 *                  r   - Thereafter
 *--------------------------------------------------------------------------*/
#define     MIB_BEACON_TIMEOUT              0x1018


/* Reserved: 0x1019 */

/*--------------------------------------------------------------------------
 * Managed Object:      MIB_PLATFORM
 *
 * Description:     Specifies the platform (including radio type) on which
 *                  the MAC is operating. Used to index a platform configuration
 *                  table which contains settings for radio registers and other
 *                  useful information
 *
 * Representation:  uint16
 *
 * Access:          rw  - Prior to Adopt Configuration
 *                  r   - Thereafter
 *--------------------------------------------------------------------------*/
#define     MIB_PLATFORM                    0x101A


/* Reserved: 0x101B */
/* Reserved: 0x101C */
/* Reserved: 0x101D */
/* Reserved: 0x101E */
/* Reserved: 0x101F */
/* Reserved: 0x1020 */

/*--------------------------------------------------------------------------
 * Managed Object:      MIB_BSS_STATE
 *
 * Description:     Represents the current state of membership of a BSS.
 *
 * Representation:  uint16
 *
 * Access:          r   - read only
 *--------------------------------------------------------------------------*/

#define     MIB_BSS_STATE                   0x1021

/* Mac is not enabled         */
#define       MIB_BSS_IDLE            0

/* Scanning to join or start a network */
#define       MIB_BSS_SCANNING        1

/* Scanning to rejoin an existing network */
#define       MIB_BSS_REJOINING       2

/* Are in sync with a BSS and have adopted BSS-wide parameters. */
#define       MIB_BSS_ADOPTED         3

/* Have started my own network - but have not yet seen a beacon from
 * another station.
 */
#define       MIB_BSS_STARTED         4

/*
 * The BSS actions are complete and successful.  The station is in sync with the BSS.
 * In an IBSS,  the station has seen at least one beacon from another station
 * in the same BSS.   For an infrastructure station,  it is authenticated and
 * associated.
 */
#define       MIB_BSS_SUCCESSFUL      5

/* Failed to join or start a network.   The station must be reset to leave this state. */
#define       MIB_BSS_FAILED          6

#define       MIB_BSS_AUTHENTICATING  7  /* Am authenticating with an AP */
#define       MIB_BSS_ASSOCIATING     8  /* Am associating with an AP    */

/* Reserved: 0x1022 */

/*--------------------------------------------------------------------------
 * Managed Object:      MIB_DB_TIMEOUT
 *
 * Description:     The timeout applied to station entries in the database.
 *
 * Representation:  uint16
 *
 * Access:          RW adopt
 *--------------------------------------------------------------------------*/
#define     MIB_DB_TIMEOUT                  0x1023
/* Reserved: 0x1024 */
/* Reserved: 0x1025 */

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_RECEIVE_DTIMS
 *
 * Description:     Controls whether the station should wake out of power-
 *                  saving in order to receive DTIMS.
 *
 * Representation:  uint8 boolean
 *
 * Access:          RW
 *--------------------------------------------------------------------------*/
#define     MIB_RECEIVE_DTIMS               0x1026

/* Reserved: 0x1027 */
/* Reserved: 0x1028 */

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_DESIRED_BSSID
 *
 * Description:     Specifies a specific BSSID which a station is required
 *                  to join,  or the broadcast BSSID (any BSSID).
 *
 * Representation:  IEEE_ADDDR
 *
 * Access:          RW_ADOPT
 *-------------------------------------------------------------------------*/
#define     MIB_DESIRED_BSSID               0x1029

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_REQUIRE_PCF
 *
 * Description:     Specifies require PCF if an infra BSS SCAN
 *
 *
 * Representation:  bool
 *
 * Access:          rw adopt
 *--------------------------------------------------------------------------*/
#define  MIB_REQUIRE_PCF                    0x102B


/*--------------------------------------------------------------------------
 * Managed Object:  MIB_DWELL_TIME
 *
 * Description:     The duration of the dwell.
 *
 * Representation:  uint16 Kus
 *
 * Access:          rw adopt
 *--------------------------------------------------------------------------*/
#define     MIB_DWELL_TIME                  0x102C


/*--------------------------------------------------------------------------
 * Managed Object:  MIB_MSDU_MAX_LENGTH
 *
 * Description:     This is the longest MSDU length which will be accepted
 *                  for transmission.
 *
 * Representation:  uint16 bytes payload length
 *
 * Access:          r
 *--------------------------------------------------------------------------*/
#define     MIB_MSDU_MAX_LENGTH             0x102D


/*--------------------------------------------------------------------------
 * Managed Object:  MIB_PASSIVE_SCAN_DUR
 *
 * Description:     Duration a station will listen for beacons on each channel
 *                  during passive scanning.
 *
 * Representation:  uint32 Kus
 *
 * Access:          rw adopt
 *--------------------------------------------------------------------------*/
#define     MIB_PASSIVE_SCAN_DUR            0x102E
#define     MIB_DEFAULT_PASSIVE_SCAN_DUR    (MIB_DEFAULT_BEACON_PERIOD + (MIB_DEFAULT_BEACON_PERIOD / 3))

/* Reserved: 0x1031 */
#ifdef      PLATFORM_SETUP
/*--------------------------------------------------------------------------
 * Managed Object:  MIB_MODCTL0
 *                  MIB_MODCTL1
 *                  MIB_DEMCTL0
 *                  MIB_DEMCTL1
 * Description:     These contain override settings to the platform
 *                  definition values.  The entire field in the platdefs.h
 *                  file is replaced by this MO value if it is non-zero.
 *
 * Representation:  uint16
 *
 * Access:          rw adopt
 *--------------------------------------------------------------------------*/
#define     MIB_MODCTL0                     0x1032
#define     MIB_MODCTL1                     0x1033
#define     MIB_DEMCTL0                     0x1034
#define     MIB_DEMCTL1                     0x1035
#endif

/* Reserved: 0x1036 */
/* Reserved: 0x1037 */
/* Reserved: 0x1038 */
/* Reserved: 0x1039 */
/* Reserved: 0x103A */
/* Reserved: 0x103B */

/* Transmit rate fallback management parameters */
/*--------------------------------------------------------------------------
 * Managed Object:  MIB_FALLBACK_TXFAIL_THRESHOLD
 *
 * Description:    Percent tx failures before rate fallback occurs
 *
 *
 * Representation: uint16 percent
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_FALLBACK_TXFAIL_THRESHOLD   0x103C

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_FALLBACK_POLL
 *
 * Description:     Time between testing the txfail threshold
 *
 *
 * Representation:  uint16 seconds
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_FALLBACK_POLL               0x103D

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_FALLBACK_RECOVERY
 *
 * Description:     Time between trying to recover from a fallback state
 *
 *
 * Representation:  uint16 seconds
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_FALLBACK_RECOVERY           0x103E

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_FALLBACK_FAILURE_RATIO_SMOOTHING_CONSTANT
 *
 * Description:     constant for smoothing the failure ratio for deciding when to
 *                  fall back to a lower data rate.
 *
 *
 * Representation:  percentage, 0--100
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_FALLBACK_FAILURE_RATIO_SMOOTHING_CONSTANT    0x103F

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_NON_ERP_PROTECTION_CONTROL_11G
 *
 * Description:     802.11g. In APs and Ad-hoc Stations, controls whether
 *                  RTS/CTS protection should be used to improve
 *                  interoperability between 802.11g and legacy 802.11b
 *                  systems.
 *
 * Representation:  uint8 - boolean
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_PROTECTION_MODE                   0x1040

/* Bits 0:2 in several globals, including MIB_PROTECTION_MODE, are used to select
   the type of protection applied - OFDM (11G/ERP) and/or HT and/or HT_CB */
#define PROTECT_NOTHING              0x00
#define PROTECT_OFDM_PACKETS         0x01
#define PROTECT_HT_PACKETS           0x02
#define PROTECT_HT_CB_PACKETS        0x04
#define PROTECT_MASK                 (PROTECT_OFDM_PACKETS|PROTECT_HT_PACKETS|PROTECT_HT_CB_PACKETS)

/* Bits 4:6 in several globals are used to select the type of protection
   applied in case of overlapping BSS condition (OFDM and/or HT and/or HT_CB */
#define PROTECT_OVRLP_SHIFT          4
#define PROTECT_OFDM_PACKETS_OVRLP   (PROTECT_OFDM_PACKETS <<PROTECT_OVRLP_SHIFT)
#define PROTECT_HT_PACKETS_OVRLP     (PROTECT_HT_PACKETS   <<PROTECT_OVRLP_SHIFT)
#define PROTECT_HT_CB_PACKETS_OVRLP  (PROTECT_HT_CB_PACKETS<<PROTECT_OVRLP_SHIFT)
#define PROTECT_MASK_OVRLP           (PROTECT_OFDM_PACKETS_OVRLP|PROTECT_HT_PACKETS_OVRLP|PROTECT_HT_CB_PACKETS_OVRLP)

/* Bits 8:10 in several globals are used to store a copy of bits 4:6 in case of repeated
   detection of overlapping events in a single 10 sec observation interval */
#define PROTECT_OVRLP_EXTRA_SHIFT    4
#define PROTECT_HOVRLP_SHIFT         (PROTECT_OVRLP_SHIFT+PROTECT_OVRLP_EXTRA_SHIFT)
#define PROTECT_OFDM_PACKETS_HOVRLP  (PROTECT_OFDM_PACKETS  <<PROTECT_HOVRLP_SHIFT)
#define PROTECT_HT_PACKETS_HOVRLP    (PROTECT_HT_PACKETS    <<PROTECT_HOVRLP_SHIFT)
#define PROTECT_HT_CB_PACKETS_HOVRLP (PROTECT_HT_CB_PACKETS <<PROTECT_HOVRLP_SHIFT)
#define PROTECT_MASK_HOVRLP          (PROTECT_OFDM_PACKETS_HOVRLP|PROTECT_HT_PACKETS_HOVRLP|PROTECT_HT_CB_PACKETS_HOVRLP)

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED
 *
 * Description:     802.11b/g. In APs (and STAs in a IBSS), indicates that
 *                  it has PHY supporting short preamble.
 *
 * Representation:  uint8 - boolean
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED        0x1041

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_PBCC_OPTION_IMPLEMENTED
 *
 * Description:     802.11b/g. In APs (and STAs in a IBSS), indicates that
 *                  it has a PHY supporting PBCC coding.
 *
 * Representation:  uint8 - boolean
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_PBCC_OPTION_IMPLEMENTED     0x1042

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_REQ_CCK_OFDM_CODING_CAPS
 *
 * Description:     802.11g. In APs (and STAs in a IBSS), indicates that
 *                  it has a PHY supporting CCK-OFDM coding.
 *
 * Representation:  uint8 - boolean
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_REQ_CCK_OFDM_CODING_CAP     0x1043


/*--------------------------------------------------------------------------
 * Managed Object:  MIB_BSS_PHY_CAPS
 *
 * Description:     802.11b/g. In STAs(and STAs in a IBSS), stores the BSS PHY capabilities as
 *                  indicated by the AP.
 *
 * Representation:  uint16 - bit mask
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_BSS_PHY_CAPS                0x1044

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_CFP_CAPABILITY_STA
 *
 * Description:     802.11b/g. In STAs, stores the BSS PHY capabilities as
 *                  indicated by the AP.
 *
 * Representation:  uint16 - bit mask
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define MIB_CFP_CAPABILITY_STA              0x1045

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_CFP_CAPABILITY_AP
 *
 * Description:     STAs Contention Free Capability
 *
 * Representation:  uint8 - bit mask
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define MIB_CFP_CAPABILITY_AP               0x1046

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_PCF_MORE_DATA_LIMIT
 *
 * Description:
 *
 * Representation:  uint32
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define MIB_PCF_MORE_DATA_LIMIT             0x1047

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_PCF_MORE_DATA_LIMIT
 *
 * Description:
 *
 * Representation:  uint32
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define MIB_PCF_NO_MORE_DATA_LIMIT          0x1048

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_PCF_ERROR_LIMIT
 *
 * Description:
 *
 * Representation:  uint32
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define MIB_PCF_ERROR_LIMIT                 0x1049
/*--------------------------------------------------------------------------
 * Managed Object:  MIB_USE_LONG_PREAMBLE_FOR_MULTICAST
 *
 * Description:     802.11b/g. In APs (and STAs in a IBSS), when TRUE
 *                  forces all multicast packets to be transmitted
 *                  using long  preamble.  Allow connectivity with all
 *                  legacy 'b' devices that do not support short preamble.
 *
 * Representation:  uint8 - boolean
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_USE_LONG_PREAMBLE_FOR_MULTICAST      0x104A

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G
 *
 * Description:     When TRUE the STA Short Slot option has been implemented.
 *
 *
 * Representation:  uint8 - boolean
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G      0x104B

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_NON_ERP_ADJACENT_NETWORK_TIMEOUT_11G
 *
 * Description:     When TRUE the STA Short Slot option has been implemented.
 *
 *
 * Representation:  uint8 - boolean
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_NON_ERP_ADJACENT_NETWORK_TIMEOUT_11G      0x104C

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_RTS_DATA_RATE
 *
 * Description:     Data rate used for sending RTS.
 *
 * Representation:  uint8 - boolean
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_RTS_DATA_RATE               0x104D

/*----WME MANAGED OBJECTS---------------------------------------------------
 * Managed objects for 802.11 WME extensions
 *--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
 * Managed Objects:  MIB_WME_XXX
 *
 * Description:     Configuration of WME parameters.
 *
 *
 * Representation:  various
 *
 * Access:          rw at an AP r at a STA
 *--------------------------------------------------------------------------*/
#define     MIB_WME_PARAMETERS              0x1050
#define     MIB_WME_DAMPING_FACTORS         0x1051
#define     MIB_WME_OUI_AND_VERSION         0x1052
#define     MIB_WME_ENABLED                 0x1053
#define     MIB_WME_APSD_ENABLED            0x1054
#define     MIB_WME_TXOP_BUDGET             0x1055
#define     MIB_WME_QAP_PARAMETERS          0x1056

/*----RSN MANAGED OBJECTS---------------------------------------------------
 * Managed objects for 802.11i RSN management
 *--------------------------------------------------------------------------*/
#define     MIB_RSN_BASE                    0x1100

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_RSN_CONTROL
 *
 * Description:     RSN control object
 *
 * Representation:  RSNMIB_CONTROL structure
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_RSN_CONTROL                 0x1101

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_RSN_REPLAY_WINDOW
 *
 * Description:     RSN control object
 *
 * Representation:  uint8
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_RSN_REPLAY_WINDOW           0x1102

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_RSN_PAIRWISE_ALERT_THRESHOLDS
 *
 * Description:     RSN alert thresholds for pairwise associations
 *
 * Representation:  RSNMIB_PAIRWISE_ALERT_THRESHOLDS structure
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_RSN_PAIRWISE_ALERT_THRESHOLDS   0x1103

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_RSN_GROUP_ALERT_THRESHOLDS
 *
 * Description:     RSN alert thresholds for the group association
 *
 * Representation:  RSNMIB_GROUP_ALERT_THRESHOLDS structure
 *
 * Access:          rw
 *--------------------------------------------------------------------------*/
#define     MIB_RSN_GROUP_ALERT_THRESHOLDS  0x1104


/*--------------------------------------------------------------------------
 * Managed Object:  MIB_RSN_SECURITY_PARAMETERS
 *
 * Description:     RSN security parameters
 *
 * Representation:  Loaded into RSN Manager
 *
 * Access:          w
 *--------------------------------------------------------------------------*/
#define     MIB_RSN_SECURITY_PARAMETERS     0x1105



/*--------------------------------------------------------------------------
 * Managed Object:  MIB_SOFTWARE_CONFIG
 *
 * Description:     Enables test scripts to determine which conditionally
 *                  compiled modules have been included
 *
 * Representation:  uint32 (bitmask)
 *
 * Access:          r
 *--------------------------------------------------------------------------*/
#define     MIB_SOFTWARE_CONFIG             0x1150

/* Bitmasks indicate the presence of conditionally compiled features */
#define MIB_SW_CFG_ENET_CFG_AP                      0x00000001
#define MIB_SW_CFG_ENET_CFG_STA                     0x00000002
#define MIB_SW_CFG_ENET_INC_PCF                     0x00000004
#define MIB_SW_CFG_ENET_CFG_RATES_11G               0x00000008
#define MIB_SW_CFG_ENET_INC_CIPHER_SUITE_NONE       0x00000010
#define MIB_SW_CFG_ENET_INC_CIPHER_SUITE_WEP        0x00000020
#define MIB_SW_CFG_ENET_INC_CIPHER_SUITE_TKIP       0x00000040
#define MIB_SW_CFG_ENET_INC_CIPHER_SUITE_CCMP       0x00000080
#define MIB_SW_CFG_ENET_INC_WME                     0x00000100
#define MIB_SW_CFG_PLATFORM_WIN32X86                0x00000200
#define MIB_SW_CFG_PLATFORM_FPGA                    0x00000400
#define MIB_SW_CFG_PLATFORM_FPGA_TTPCOM_ARM7        0x00000800
#define MIB_SW_CFG_PLATFORM_FPGA_TTPCOM_SINGULLAR   0x00001000
#define MIB_SW_CFG_PLATFORM_COSIM                   0x00002000
#define MIB_SW_CFG_ENET_INC_TEST_MC                 0x00004000
#define MIB_SW_CFG_ALLOC_SEPARATE_FORWARD_POOL      0x00008000
#define MIB_SW_CFG_CIPHER_SUITE_SHRAM_DMA           0x00010000

/*--------------------------------------------------------------------------
 * Managed Object:  MIB_MT_PHY_CFG
 *
 * Description:     Advanced PHY parameters
 *--------------------------------------------------------------------------*/
#define     MIB_MT_PHY_CFG                  0x1200

#define     MIB_SHORT2_REQUIRED                 (MIB_MT_PHY_CFG + 0x01)
#define     MIB_SPECTRUM_MODE                   (MIB_MT_PHY_CFG + 0x02)
#define     MIB_CB_DATABINS_PER_SYMBOL          (MIB_MT_PHY_CFG + 0x03)
#define     MIB_SERVICE_REQUIRED                (MIB_MT_PHY_CFG + 0x04)
#define     MIB_ADVANCED_CODING_SUPPORTED       (MIB_MT_PHY_CFG + 0x05)
#define     MIB_USE_SHORT_CYCLIC_PREFIX         (MIB_MT_PHY_CFG + 0x06)
#define     MIB_SUPPORTED_RX_CHANNELS           (MIB_MT_PHY_CFG + 0x07)
#define     MIB_NUM_OF_RFICs                    (MIB_MT_PHY_CFG + 0x08)
#define     MIB_RFIC_OSCIL_FREQ                 (MIB_MT_PHY_CFG + 0x09)
#define     MIB_AFE_CLOCK                       (MIB_MT_PHY_CFG + 0x0A)
#define     MIB_CALIBRATION_ALGO_MASK           (MIB_MT_PHY_CFG + 0x0B)
#define     MIB_USE_SPACE_TIME_BLOCK_CODE       (MIB_MT_PHY_CFG + 0x0C)
/* #define     MIB_USE_CHANNEL_BONDING             (MIB_MT_PHY_CFG + 0x0D) */
#define     MIB_STOP_CAPTURE_AFTER_CRC          (MIB_MT_PHY_CFG + 0x0E)
#define     MIB_UPPER_LOWER_CHANNEL_BONDING     (MIB_MT_PHY_CFG + 0x0F)
#define     MIB_FORCE_TPC_0                     (MIB_MT_PHY_CFG + 0x10)
#define     MIB_FORCE_TPC_1                     (MIB_MT_PHY_CFG + 0x11)
#define     MIB_FORCE_TPC_2                     (MIB_MT_PHY_CFG + 0x15)
#define     MIB_TPC_ANT_0                       (MIB_MT_PHY_CFG + 0x12)   
#define     MIB_TPC_ANT_1                       (MIB_MT_PHY_CFG + 0x13)    
#define     MIB_TPC_ANT_2                       (MIB_MT_PHY_CFG + 0x14)    
/* Reserved - (MIB_MT_PHY_CFG + 0x15) */
#define     MIB_USE_11B_DUPLICATE               (MIB_MT_PHY_CFG + 0x16)
#define     MIB_OVERLAPPING_PROTECTION_ENABLE   (MIB_MT_PHY_CFG + 0x17) /* [1] Enable protection also in case of Ovelapping BSS condition (OLBC) in 2.4 GHz */
#define     MIB_OFDM_PROTECTION_METHOD          (MIB_MT_PHY_CFG + 0x18) /* How to protect OFDM: PROTECTION_NONE / PROTECTION_RTS /PROTECTION_CTS  */
#define     MIB_HT_PROTECTION_METHOD            (MIB_MT_PHY_CFG + 0x19) /* How to protect HT: PROTECTION_NONE / PROTECTION_RTS /PROTECTION_CTS */
#define     MIB_ONLINE_CALIBRATION_ALGO_MASK    (MIB_MT_PHY_CFG + 0x20)

#define     MIB_SPECTRUM_20M    0
#define     MIB_SPECTRUM_40M    1
/*--------------------------------------------------------------------------
 * Managed Object:  MIB_MT_SW_CFG
 *
 * Description:     Advanced SW parameters
 *--------------------------------------------------------------------------*/
#define     MIB_MT_SW_CFG                   0x1300
#define     MIB_ENABLE_AUTO_RATE            (MIB_MT_SW_CFG + 0x01)
#if defined (ENET_CFG_STA)
#define     MIB_HIDDEN_SSID                 (MIB_MT_SW_CFG + 0x03)
#endif
#define     MIB_BEACON_KEEPALIVE_TIMEOUT    (MIB_MT_SW_CFG + 0x04)

#define     MIB_IS_FORCE_RATE               (MIB_MT_SW_CFG + 0x06) // 1=use rate defined in MIB_FORCE_RATE / 0=rate-adapt-enable (was MIB_MIN_RATE)
#define     MIB_HT_FORCE_RATE               (MIB_MT_SW_CFG + 0x07) // rate to use if MIB_IS_FORCE_RATE & FORCED_RATE_HT_MASK is 1 (15.31) (was MIB_MAX_RATE)
#define     MIB_LEGACY_FORCE_RATE           (MIB_MT_SW_CFG + 0x09) // rate to use if MIB_IS_FORCE_RATE & FORCED_RATE_LEGACY_MASK is 1 (0.14) (was MIB_MAX_RATE)

#define     MIB_AUTO_AGGREGATE              (MIB_MT_SW_CFG + 0x08)

#define     MIB_RECEIVE_AMPDU_MAX_LENGTH    (MIB_MT_SW_CFG + 0x0A)

#define     MIB_ACL_MAX_CONNECTIONS         (MIB_MT_SW_CFG + 0x20)
#define     MIB_ACL_MODE                    (MIB_MT_SW_CFG + 0x21)
#define     MIB_ACL							(MIB_MT_SW_CFG + 0x22)
#define     MIB_ACL_MASKS					(MIB_MT_SW_CFG + 0x24)

#define     MIB_POWER_INCREASE_VS_DUTY_CYCLE (MIB_MT_SW_CFG + 0x23)

/* ACL Definitions */
#define     MAX_ADDRESSES_IN_ACL 32

typedef struct _MIB_ACL_MAX_CONNECTIONS_TYPE
{
  uint8 Network_Index;
  uint8 u8MaxAllowedConnections;
  uint8 reserved[2];
} __MTLK_PACKED MIB_ACL_MAX_CONNECTIONS_TYPE;

typedef struct _MIB_ACL_MODE_TYPE
{
  uint8 Network_Index;
  uint8 u8ACLMode;
  uint8 reserved[2];
} __MTLK_PACKED MIB_ACL_MODE_TYPE;

typedef struct _MIB_ACL_TYPE
{
  uint8 Network_Index;
  uint8 Num_Of_Entries;
  IEEE_ADDR aACL[MAX_ADDRESSES_IN_ACL];
#if ((MAX_ADDRESSES_IN_ACL % 2) == 0)
  uint8 reserved[2];
#endif
} __MTLK_PACKED MIB_ACL_TYPE;

typedef struct _MIB_ACL_TYPE_MASKS
{
	uint8 Network_Index;
	uint8 Num_Of_Entries;
	IEEE_ADDR aACL[MAX_ADDRESSES_IN_ACL];
#if ((MAX_ADDRESSES_IN_ACL % 2) == 0)
	uint8 reserved[2];
#endif
} __MTLK_PACKED MIB_ACL_TYPE_MASKS;

/*-- Type definitions -----------------------------------------------------*/

typedef uint16 MIB_ID;

typedef union _MIB_VALUE
{
    MIB_UINT8                   u8Uint8;                    /* Generic tags for test harness */
    MIB_UINT16                  u16Uint16;
    MIB_UINT32                  u32Uint32;
    MIB_UINT64                  u64Uint64;
    MIB_LIST_OF_U8              au8ListOfu8;
    MIB_SET_OF_U8               sSetOfu8;
    MIB_WEP_DEF_KEYS            sDefaultWEPKeys;
    RSNMIB_CONTROL              sRsnControl;
    MIB_WME_TABLE               sWMEParameters;
    FM_WME_OUI_AND_VER          sWMEOuiandVer;
    MIB_ESS_ID                  sDesiredESSID;
    PRE_ACTIVATE_MIB_TYPE       sPreActivateType;
    MIB_CHANNELS_TYPE           sCountry;
	MIB_ACL_MAX_CONNECTIONS_TYPE  sMaxConnections;
    MIB_ACL_MODE_TYPE             sAclMode;
    MIB_ACL_TYPE                  sACL;
	MIB_ACL_TYPE_MASKS            sACLmasks;
    List_Of_Tpc                 sList_Of_Tpc;   
    EEPROM_VERSION_TYPE         sEepromInfo;
} __MTLK_PACKED MIB_VALUE;

typedef struct _MIB_OBJECT
{
    MIB_ID                  u16ID;
    MIB_VALUE               uValue;
} __MTLK_PACKED MIB_OBJECT;

#define   MTLK_PACK_OFF
#include "mtlkpack.h"

#endif /* !__MHI_MIB_ID_INCLUDED */
