/***************************************************************************
****************************************************************************
**
** COMPONENT:        ENET Upper MAC    SW
**
** MODULE:           $File: //bwp/enet/demo153_sw/develop/src/shared/mhi_umi.h $
**
** VERSION:          $Revision: #4 $
**
** DATED:            $Date: 2007/03/04 $
**
** AUTHOR:           S Sondergaard
**
** DESCRIPTION:      Upper MAC Public Header
**
****************************************************************************
*
* Copyright (c) TTPCom Limited, 2003
*
* Copyright (c) Metalink Ltd., 2006 - 2007
*
****************************************************************************/

#ifndef __MHI_UMI_INCLUDED_H
#define __MHI_UMI_INCLUDED_H

#include "mhi_ieee_address.h"
#include "mhi_frame.h"
#include "mhi_rsn_caps.h"
#include "msgid.h"
#include "mhi_statistics.h"
#include "umi_rsn.h"
#include "MT_mac_host_adapt.h"
#include "mtlkbfield.h"
#include "mhi_mac_event.h"
#define   MTLK_PACK_ON
#include "mtlkpack.h"

#define  TU                             1024 /* 1TU (Time Unit) = 1024 microseconds - defined in 802.11 */

/***************************************************************************/
/***                       Types and Defines                             ***/
/***************************************************************************/

#define UMI_MAX_MSDU_LENGTH             (MSDU_MAX_LENGTH)

#define LOGGER_NUM_OF_GROUPS_BIT_MAP  4
typedef enum
{
    /* according to numbers defined in BB_UTILS_upper_lower_cpu */
    UMI_CPU_ID_LM,
    UMI_CPU_ID_UM,
    UMI_CPU_ID_ALL,
    UMI_CPU_ID_MAX
} UmiCpuId_e;

typedef uint8 UMI_STATUS;
#define UMI_OK                          0
#define UMI_NOT_INITIALISED             1
#define UMI_BAD_PARAMETER               2
#define UMI_BAD_VALUE                   3
#define UMI_BAD_LENGTH                  4
#define UMI_MC_BUSY                     5
#define UMI_ALREADY_ENABLED             6
#define UMI_HW_FAILURE                  7
#define UMI_BSS_ALREADY_ACTIVE          8
#define UMI_BSS_HAS_NO_CFP              9
#define UMI_BSS_UNKNOWN                 10
#define UMI_STATION_UNKNOWN             11
#define UMI_NOT_ENABLED                 12
#define UMI_OUT_OF_MEMORY               13
#define UMI_TIMEOUT                     14
#define UMI_NOT_CONNECTED               15
#define UMI_UNKNOWN_OBJECT              16
#define UMI_READ_ONLY                   17
#define UMI_WRITE_ONLY                  18
#define UMI_RATE_MISMATCH               19
#define UMI_TRANSFER_ALREADY_ACTIVE     20
#define UMI_TRANSFER_FAILED             21
#define UMI_NOT_SUPPORTED               22
#define UMI_RSN_IE_FORMAT_ERROR         23
#define UMI_RSN_BAD_CAPABILITIES        24
#define UMI_INTERNAL_MAC_ERROR          25
#define UMI_UM_BUSY						26
#define UMI_PS_NOT_ENABLED				27
#define UMI_ADD_BSS_FAIL				28
#define UMI_REMOVE_VAP_FAIL				29
#define UMI_MAX_VAP_WAS_ADDED			30
#define UMI_VAP_DB_FAIL                 31

/* Status codes for memory allocation */
#define UMI_ALLOC_OK                    UMI_OK
#define UMI_ALLOC_FAILED                UMI_OUT_OF_MEMORY
#define UMI_ALLOC_FWD_POOL_OK           26
#define UMI_STATUS_TOTAL_NUMBER         27

typedef uint8 UMI_NOTIFICATION;
#define UMI_NOTIFICATION_OK             0
#define UMI_NOTIFICATION_MIC_FAILURE    1

#define UMI_MAX_CHANNELS_PER_SCAN_REQ   16
typedef uint16 UMI_BSS_TYPE;
#define UMI_BSS_INFRA                   0
#define UMI_BSS_INFRA_PCF               1
#define UMI_BSS_ADHOC                   2
#define UMI_BSS_ANY                     3

typedef uint16 UMI_NETWORK_STATUS;
#define UMI_BSS_CREATED                 0   // We have created a network (BSS)
#define UMI_BSS_CONNECTING              1   // STA is trying to connect to AP
#define UMI_BSS_CONNECTED               2   // STA has connected to network (auth and assoc) or AP/STA resume connection after channel switch
#define UMI_BSS_FAILED                  4   // STA is unable to connect with any network
#define UMI_BSS_RADAR_NORM              5   // Regular radar was detected.
#define UMI_BSS_RADAR_HOP               6   // Frequency Hopping radar was detected.
#define UMI_BSS_CHANNEL_SWITCH_NORMAL   7   // STA received a channel announcement with non-silent mode.
#define UMI_BSS_CHANNEL_SWITCH_SILENT   8   // STA received a channel announcement with silent mode.
#define UMI_BSS_CHANNEL_SWITCH_DONE     9   // AP/STA have just switched channel (but traffic may be started only after UMI_BSS_CONNECTED event)
#define UMI_BSS_CHANNEL_PRE_SWITCH_DONE 10  //
#define UMI_BSS_OVER_CHANNEL_LOAD_THRESHOLD 11  // Channel load threshold over.


//PHY characteristics parameters
#define UMI_PHY_TYPE_802_11A          	0x00    /* 802.11a  */
#define UMI_PHY_TYPE_802_11B          	0x01    /* 802.11b  */
#define UMI_PHY_TYPE_802_11G          	0x02    /* 802.11g  */
#define UMI_PHY_TYPE_802_11B_L      	0x81    /* 802.11b with long preamble*/
#define UMI_PHY_TYPE_802_11N_5_2_BAND 	0x04    /* 802.11n_5.2G  */
#define UMI_PHY_TYPE_802_11N_2_4_BAND 	0x05    /* 802.11n_2.4G  */

#define UMI_PHY_TYPE_BAND_5_2_GHZ       0                  
#define UMI_PHY_TYPE_BAND_2_4_GHZ       1

#define UMI_PHY_TYPE_UPPER_CHANNEL      0       /* define UPPER secondary channel offset */
#define UMI_PHY_TYPE_LOWER_CHANNEL      1       /* define LOWER secondary channel offset */

#define UMI_PHY_11B_FIRST_RATE          8 // copied from LM_PHY_11B_RATE_2_SHORT
#define UMI_PHY_11N_FIRST_RATE          15 // copied from LM_PHY_11N_RATE_6_5
//Channel SwitchMode values
#define UMI_CHANNEL_SW_MODE_NORMAL		0x00
#define UMI_CHANNEL_SW_MODE_SILENT		0x01
#define UMI_CHANNEL_SW_MODE_MASK		0x0f
#define UMI_CHANNEL_SW_MODE_SCN			0x00 //SCN (NO SECONDARY)
#define UMI_CHANNEL_SW_MODE_SCA			0x10 //SCA (ABOVE)
#define UMI_CHANNEL_SW_MODE_SCB			0x30 //SCB (BELOW)
#define UMI_CHANNEL_SW_MODE_SC_MASK		0xf0
#define UMI_CHANNEL_SW_MODE_SC_SHIFT	(4)


/* Stop reasons */
#define BSS_STOP_REASON_JOINED	  		0	               
#define BSS_STOP_REASON_DISCONNECT		1
#define BSS_STOP_REASON_JOINED_FAILED	2
#define BSS_STOP_REASON_SCAN			3
#define BSS_STOP_REASON_MC_REQ			4
#define BSS_STOP_REASON_BGSCAN			5
#define BSS_STOP_REASON_UM_REQ			6
#define BSS_STOP_REASON_NONE	 		0xFF

typedef uint16 UMI_CONNECTION_STATUS;
#define UMI_CONNECTED                   0
#define UMI_DISCONNECTED                1
#define UMI_RECONNECTED                 2
#define UMI_AUTHENTICATION              3
#define UMI_ASSOCIATION                 4 // used to indicate failed association. successsful association is: UMI_CONNECTED

typedef uint16 UMI_PCF_CAPABILITY;
#define UMI_NO_PCF                      0
#define UMI_HAS_PCF                     1

typedef uint16 UMI_ACCESS_PROTOCOL;
#define UMI_USE_DCF                     0
#define UMI_USE_PCF                     1


#define UMI_DEBUG_BLOCK_SIZE            (256)
#define UMI_C100_DATA_SIZE   (UMI_DEBUG_BLOCK_SIZE - (sizeof(uint16) + sizeof(uint16) + sizeof(C100_MSG_HEADER)))
#define UMI_DEBUG_DATA_SIZE  (UMI_DEBUG_BLOCK_SIZE - (sizeof(uint16) + sizeof(uint16)))

#define PS_REQ_MODE_ON					1
#define PS_REQ_MODE_OFF					0

/*************************************
* AOCS commands.
*/
#define MAC_AOCS_DISABLE        0
#define MAC_AOCS_ALG_ENABLE     1


/*************************************
* For the use of the Generic message.
*/
#define MAC_VARIABLES_REQ       1
#define MAC_EEPROM_REQ          2
#define MAC_OCS_TIMER_START     3
#define MAC_OCS_TIMER_STOP      4
#define MAC_OCS_TIMER_TIMEOUT   5


#define NEW_CASE                6

/*************************************
* LBF defines.
*/

#define LBF_NUM_MAT_SETS 8
#define LBF_NUM_CDD_SETS 16
#define LBF_DISABLED_SET 0xff

/***************************************************************************/
/***                         memory array definition                     ***/
/***************************************************************************/
#define ARRAY_NULL              0
#define ARRAY_DAT_IND           1
#define ARRAY_DAT_REQ           2
#define ARRAY_MAN_IND           3
#define ARRAY_MAN_REQ           4
#define ARRAY_DBG_IND           5
#define ARRAY_DBG_REQ           6
// #define number 7 is not used. can be used if needed
#define ARRAY_DAT_IND_SND       8 /* Data Sounding packet */
#define ARRAY_DAT_LOGGER_IND    9 /* Logger buffer */


/***************************************************************************/
/***                          MbssId definitions                         ***/
/***************************************************************************/

#define IND_REQ_INFO_BSS_IDX			MTLK_BFIELD_INFO(0, 3)   /* 3 bits starting BIT0 of u16Info field */
#define IND_REQ_WITHOUT_VAP_INX			MTLK_BFIELD_INFO(3, 13)  /* 3 bits starting BIT0 of u16Info field */
#define RX_IND_DATA_OFFSET				MTLK_BFIELD_INFO(3, 1)   /* 1 bit starting BIT3 of u16Info field */
#define RX_IND_DATA_SIZE				MTLK_BFIELD_INFO(4, 12)  /* 12 bits starting BIT4 of u16Info field */
#define RX_RES_BUF_QUE_IDX				MTLK_BFIELD_INFO(3, 13)  /* 13 bit starting BIT3 of u16Info field */
#define LOG_IND_DATA_SIZE				MTLK_BFIELD_INFO(3, 13)  /* 13 bit starting BIT3 of u16Info field */
#define IND_REQ_TX_STATUS				MTLK_BFIELD_INFO(0, 8)   /* 8 bits starting BIT0 of u16Info field */
#define IND_REQ_NUM_RETRANSMISSIONS		MTLK_BFIELD_INFO(8, 8)   /* 8 bits starting BIT8 of u16Info field */

/***************************************************************************/
/***                         Message IDs                                 ***/
/***************************************************************************/

#define UMI_MSG_ID_INVALID              (MSG_COMP_MASK | MSG_NUM_MASK)

/* message NULL reserved K_NULL_MSG  */

/* Management messages */
#define UM_MAN_READY_REQ               (K_MSG_TYPE)0x0401 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 1)*/
#define MC_MAN_READY_CFM               (K_MSG_TYPE)0x1401 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 1)*/

#define UM_MAN_SET_MIB_REQ             (K_MSG_TYPE)0x0402 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 2)*/
#define MC_MAN_SET_MIB_CFM             (K_MSG_TYPE)0x1402 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 2)*/

#define UM_MAN_GET_MIB_REQ             (K_MSG_TYPE)0x0403 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 3)*/
#define MC_MAN_GET_MIB_CFM             (K_MSG_TYPE)0x1403 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 3)*/

#define UM_MAN_SCAN_REQ                (K_MSG_TYPE)0x0404 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 4)*/
#define MC_MAN_SCAN_CFM                (K_MSG_TYPE)0x1404 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 4)*/

#define UM_DOWNLOAD_PROG_MODEL_REQ     (K_MSG_TYPE)0x0405 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 5)*/
#define MC_DOWNLOAD_PROG_MODEL_CFM     (K_MSG_TYPE)0x1405 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 5)*/

#define UM_MAN_ACTIVATE_REQ            (K_MSG_TYPE)0x0406 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 6)*/
#define MC_MAN_ACTIVATE_CFM            (K_MSG_TYPE)0x1406 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 6)*/

#define UM_MAN_DISCONNECT_REQ          (K_MSG_TYPE)0x0409 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 9)*/
#define MC_MAN_DISCONNECT_CFM          (K_MSG_TYPE)0x1409 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 9)*/

/*connection status*/
#define UM_MAN_GET_CHANNEL_STATUS_REQ   (K_MSG_TYPE)0x040A /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 10)*/
#define MC_MAN_GET_CHANNEL_STATUS_CFM   (K_MSG_TYPE)0x140A /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 10)*/

#define UM_MAN_RESET_REQ               (K_MSG_TYPE)0x040B /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 11)*/
#define MC_MAN_RESET_CFM               (K_MSG_TYPE)0x140B /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 11)*/

#define UM_MAN_POWER_MODE_REQ          (K_MSG_TYPE)0x040C /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 12)*/
#define MC_MAN_POWER_MODE_CFM          (K_MSG_TYPE)0x140C /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 12)*/


#define UM_MAN_SET_KEY_REQ             (K_MSG_TYPE)0x040E /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 14)*/
#define MC_MAN_SET_KEY_CFM             (K_MSG_TYPE)0x140E /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 14)*/

#define UM_MAN_CLEAR_KEY_REQ           (K_MSG_TYPE)0x040F /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 15)*/
#define MC_MAN_CLEAR_KEY_CFM           (K_MSG_TYPE)0x140F /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 15)*/

#define UM_START_DOWNLOAD_PROG_MODEL   (K_MSG_TYPE)0x0010 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 16)*/

#define UM_MAN_SET_BCL_VALUE           (K_MSG_TYPE)0x0411 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 17)*/
#define MC_MAN_SET_BCL_CFM             (K_MSG_TYPE)0x1411 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 17)*/

#define UM_MAN_QUERY_BCL_VALUE         (K_MSG_TYPE)0x0412 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 18)*/
#define MC_MAN_QUERY_BCL_CFM           (K_MSG_TYPE)0x1412 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 18)*/

#define UM_MAN_GET_MAC_VERSION_REQ     (K_MSG_TYPE)0x0413 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 19)*/
#define MC_MAN_GET_MAC_VERSION_CFM     (K_MSG_TYPE)0x1413 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 19)*/

#define UM_MAN_OPEN_AGGR_REQ           (K_MSG_TYPE)0x0414 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 20)*/
#define MC_MAN_OPEN_AGGR_CFM           (K_MSG_TYPE)0x1414 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 20)*/

#define UM_MAN_ADDBA_REQ_TX_REQ        (K_MSG_TYPE)0x0415 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 21)*/
#define MC_MAN_ADDBA_REQ_TX_CFM        (K_MSG_TYPE)0x1415 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 21)*/

#define UM_MAN_ADDBA_RES_TX_REQ        (K_MSG_TYPE)0x0417 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 23)*/
#define MC_MAN_ADDBA_RES_TX_CFM        (K_MSG_TYPE)0x1417 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 23)*/

#define UM_MAN_GENERIC_MAC_REQ         (K_MSG_TYPE)0x0418 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 24)*/
#define MC_MAN_GENERIC_MAC_CFM         (K_MSG_TYPE)0x1418 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 24)*/

#define UM_MAN_SW_RESET_MAC_REQ        (K_MSG_TYPE)0x0419 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 25)*/
#define MC_MAN_SW_RESET_MAC_CFM        (K_MSG_TYPE)0x1419 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 25)*/

#define UM_MAN_DELBA_REQ               (K_MSG_TYPE)0x041A /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 26)*/
#define MC_MAN_DELBA_CFM               (K_MSG_TYPE)0x141A /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 26)*/

#define UM_PER_CHANNEL_CALIBR_REQ      (K_MSG_TYPE)0x041B /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 27)*/
#define MC_PER_CHANNEL_CALIBR_CFM      (K_MSG_TYPE)0x141B /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 27)*/


#define UM_LM_STOP_REQ                 (K_MSG_TYPE)0x041C /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 22)*/
#define MC_LM_STOP_CFM                 (K_MSG_TYPE)0x141C /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 22)*/

#define UM_MAN_CLOSE_AGGR_REQ          (K_MSG_TYPE)0x041D /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 29)*/
#define MC_MAN_CLOSE_AGGR_CFM          (K_MSG_TYPE)0x141D /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 29)*/

#define UM_MAN_GET_PEERS_STATUS_REQ	   (K_MSG_TYPE)0x041E /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 30)*/
#define MC_MAN_GET_PEERS_STATUS_CFM	   (K_MSG_TYPE)0x141E /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 30)*/

#define UM_START_PER_CHANNEL_CALIBR    (K_MSG_TYPE)0x0020 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 32)*/


#define UM_MAN_CONFIG_GPIO_REQ         (K_MSG_TYPE)0x0429 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 41)*/
#define MC_MAN_CONFIG_GPIO_CFM         (K_MSG_TYPE)0x1429 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 41)*/

#define UM_MAN_GET_GROUP_PN_REQ        (K_MSG_TYPE)0x0430 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 48)*/
#define MC_MAN_GET_GROUP_PN_CFM        (K_MSG_TYPE)0x1430 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 48)*/

#define UM_MAN_SET_IE_REQ              (K_MSG_TYPE)0x0431 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 49)*/
#define MC_MAN_SET_IE_CFM              (K_MSG_TYPE)0x1431 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 49)*/

#define UM_SET_CHAN_REQ                (K_MSG_TYPE)0x0432 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 50)*/
#define UM_SET_CHAN_CFM                (K_MSG_TYPE)0x1432 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 50)*/

//channel load msg
#define UM_MAN_SET_CHANNEL_LOAD_VAR_REQ (K_MSG_TYPE)0x0433 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 51)*/
#define MC_MAN_SET_CHANNEL_LOAD_VAR_CFM (K_MSG_TYPE)0x1433 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 51)*/

#define UM_MAN_SET_LED_REQ             (K_MSG_TYPE)0x0434 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 52)*/
#define MC_MAN_SET_LED_CFM             (K_MSG_TYPE)0x1434 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 52)*/
#define UM_MAN_SET_DEF_RF_MGMT_DATA_REQ (K_MSG_TYPE)0x0435 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 53)*/
#define MC_MAN_SET_DEF_RF_MGMT_DATA_CFM (K_MSG_TYPE)0x1435 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 53)*/

#define UM_MAN_GET_DEF_RF_MGMT_DATA_REQ (K_MSG_TYPE)0x0436 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 54)*/
#define MC_MAN_GET_DEF_RF_MGMT_DATA_CFM (K_MSG_TYPE)0x1436 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 54)*/
#define UM_MAN_SEND_MTLK_VSAF_REQ      (K_MSG_TYPE)0x0437 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 55)*/
#define MC_MAN_SEND_MTLK_VSAF_CFM      (K_MSG_TYPE)0x1437 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 55)*/

#define MC_UM_PS_REQ				   (K_MSG_TYPE)0x0438 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 56)*/
#define UM_MC_PS_CFM				   (K_MSG_TYPE)0x1438 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 56)*/

#define MC_MAN_CHANGE_POWER_STATE_REQ  (K_MSG_TYPE)0x0439 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 57)*/
#define UM_MAN_CHANGE_POWER_STATE_CFM  (K_MSG_TYPE)0x1439 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 57)*/

#define UM_MAN_RF_MGMT_SET_TYPE_REQ    (K_MSG_TYPE)0x0440 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 58)*/
#define MC_MAN_RF_MGMT_SET_TYPE_CFM    (K_MSG_TYPE)0x1440 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 58)*/

#define UM_MAN_RF_MGMT_GET_TYPE_REQ    (K_MSG_TYPE)0x0441 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 59)*/
#define MC_MAN_RF_MGMT_GET_TYPE_CFM    (K_MSG_TYPE)0x1441 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 59)*/

#define UM_MAN_DOWNLOAD_PROG_MODEL_PERMISSION_REQ (K_MSG_TYPE)0x0442 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 60)*/
#define MC_MAN_DOWNLOAD_PROG_MODEL_PERMISSION_CFM (K_MSG_TYPE)0x1442 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 60)*/

#define UM_MAN_CHANGE_TX_POWER_LIMIT_REQ (K_MSG_TYPE)0x0444 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 61)*/
#define MC_MAN_CHANGE_TX_POWER_LIMIT_CFM (K_MSG_TYPE)0x1444 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 61)*/

#define UM_MAN_MBSS_PRE_ACTIVATE_REQ     (K_MSG_TYPE)0x0451 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 84)*/
#define MC_MAN_MBSS_PRE_ACTIVATE_CFM     (K_MSG_TYPE)0x1451 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 84)*/

#define UM_MAN_VAP_DB_REQ               (K_MSG_TYPE)0x0452  /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 85)*/
#define MC_MAN_VAP_DB_CFM               (K_MSG_TYPE)0x1452 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 85)*/

#define UM_MAN_ACTIVATE_VAP_REQ     (K_MSG_TYPE)0x0453 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 86)*/
#define MC_MAN_ACTIVATE_VAP_CFM     (K_MSG_TYPE)0x1453 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 86)*/

#define UM_MAN_DEACTIVATE_VAP_REQ   (K_MSG_TYPE)0x0454 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 87)*/
#define MC_MAN_DEACTIVATE_VAP_CFM   (K_MSG_TYPE)0x1454 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 87)*/

#define UM_MAN_VAP_LIMITS_REQ		(K_MSG_TYPE)0x0455 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 88)*/
#define MC_MAN_VAP_LIMITS_CFM		(K_MSG_TYPE)0x1455 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 88)*/

#define MC_MAN_NETWORK_EVENT_IND       (K_MSG_TYPE)0x3307 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 7)*/
#define UM_MAN_NETWORK_EVENT_RES       (K_MSG_TYPE)0x2307 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 7)*/

#define MC_MAN_CONNECTION_EVENT_IND    (K_MSG_TYPE)0x3308 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 8)*/
#define UM_MAN_CONNECTION_EVENT_RES    (K_MSG_TYPE)0x2308 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 8)*/

#define MC_MAN_SECURITY_ALERT_IND      (K_MSG_TYPE)0x3310 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 16)*/
#define UM_MAN_SECURITY_ALERT_RES      (K_MSG_TYPE)0x2310 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 16)*/

#define MC_MAN_BAR_IND                 (K_MSG_TYPE)0x3314 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 20)*/
#define UM_MAN_BAR_RES                 (K_MSG_TYPE)0x2314 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 20)*/

#define MC_MAN_ADDBA_REQ_RX_IND        (K_MSG_TYPE)0x3316 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 22)*/
#define UM_MAN_ADDBA_REQ_RX_RES        (K_MSG_TYPE)0x2316 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 22)*/

#define MC_MAN_ADDBA_RES_RX_IND        (K_MSG_TYPE)0x3319 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 25)*/
#define UM_MAN_ADDBA_RES_RX_RES        (K_MSG_TYPE)0x2319 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 25)*/

#define MC_MAN_DELBA_IND               (K_MSG_TYPE)0x332B /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 43)*/
#define UM_MAN_DELBA_RES               (K_MSG_TYPE)0x232B /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 43)*/

#define MC_MAN_DYNAMIC_PARAM_IND       (K_MSG_TYPE)0x3344 /*(MSG_IND + MSG_UMI_COMP + UMI_MEM_MSG + 4)*/
#define UM_MAN_DYNAMIC_PARAM_RES       (K_MSG_TYPE)0x2344 /*(MSG_RES + MSG_UMI_COMP + UMI_MEM_MSG + 4)*/

#define UM_MAN_AOCS_REQ                (K_MSG_TYPE)0x0421 /*(MSG_REQ + MSG_UMI_COMP + UMI_MAN_MSG + 33)*/
#define MC_MAN_AOCS_CFM                (K_MSG_TYPE)0x1421 /*(MSG_CFM + MSG_UMI_COMP + UMI_MAN_MSG + 33)*/

#define MC_MAN_AOCS_IND                (K_MSG_TYPE)0x3321 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 33)*/
#define UM_MAN_AOCS_RES                (K_MSG_TYPE)0x2321 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 33)*/

#define MC_MAN_PM_UPDATE_IND		   (K_MSG_TYPE)0x3332 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 50)*/
#define UM_MAN_PM_UPDATE_RES		   (K_MSG_TYPE)0x2332 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 50)*/

#define MC_MAN_VAP_WAS_REMOVED_IND	   (K_MSG_TYPE)0x3352 /*(MSG_IND + MSG_UMI_COMP + UMI_MAN_MSG + 82)*/
#define UM_MAN_VAP_WAS_REMOVED_RES	   (K_MSG_TYPE)0x2352 /*(MSG_RES + MSG_UMI_COMP + UMI_MAN_MSG + 82)*/

/* Data Messages */
#define UM_DAT_TXDATA_REQ              (K_MSG_TYPE)0x0240 /*(MSG_REQ + MSG_UMI_COMP + UMI_DAT_MSG + 0)*/
#define MC_DAT_TXDATA_CFM              (K_MSG_TYPE)0x1240 /*(MSG_CFM + MSG_UMI_COMP + UMI_DAT_MSG + 0)*/

#define MC_DAT_RXDATA_IND              (K_MSG_TYPE)0x3141 /*(MSG_IND + MSG_UMI_COMP + UMI_DAT_MSG + 1)*/
#define UM_DAT_RXDATA_RES              (K_MSG_TYPE)0x2141 /*(MSG_RES + MSG_UMI_COMP + UMI_DAT_MSG + 1)*/
#define MC_DAT_RXDATA_IND_SND          (K_MSG_TYPE)0x3841 /*(MSG_IND + MSG_UMI_COMP + UMI_DAT_MSG + 1)*/

/* Buffer Sending Messages */
#define MC_DAT_SEND_BUF_TO_HOST_IND    (K_MSG_TYPE)0x3942 /*(MSG_REQ + MSG_UMI_COMP + UMI_DAT_MSG + 2)*/
#define UM_DAT_SEND_BUF_TO_HOST_RES    (K_MSG_TYPE)0x2942 /*(MSG_CFM + MSG_UMI_COMP + UMI_DAT_MSG + 2)*/

/* Memory Messages */
#define MC_MEM_COPY_FROM_MAC_IND       (K_MSG_TYPE)0x3080 /*(MSG_IND + MSG_UMI_COMP + UMI_MEM_MSG + 0)*/
#define UM_MEM_COPY_FROM_MAC_RES       (K_MSG_TYPE)0x2080 /*(MSG_RES + MSG_UMI_COMP + UMI_MEM_MSG + 0)*/

#define MC_MEM_COPY_TO_MAC_IND         (K_MSG_TYPE)0x3081 /*(MSG_IND + MSG_UMI_COMP + UMI_MEM_MSG + 1)*/
#define UM_MEM_COPY_TO_MAC_RES         (K_MSG_TYPE)0x2081 /*(MSG_RES + MSG_UMI_COMP + UMI_MEM_MSG + 1)*/

/* Debug Messages */
#define UM_DBG_RESET_STATISTICS_REQ    (K_MSG_TYPE)0x06C0 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 0)*/
#define MC_DBG_RESET_STATISTICS_CFM    (K_MSG_TYPE)0x16C0 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 0)*/

#define UM_DBG_GET_STATISTICS_REQ      (K_MSG_TYPE)0x06C1 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 1)*/
#define MC_DBG_GET_STATISTICS_CFM      (K_MSG_TYPE)0x16C1 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 1)*/

#define UM_DBG_INPUT_REQ               (K_MSG_TYPE)0x06C2 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 2)*/
#define MC_DBG_INPUT_CFM               (K_MSG_TYPE)0x16C2 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 2)*/

#define MC_DBG_OUTPUT_IND              (K_MSG_TYPE)0x35C3 /*(MSG_IND + MSG_UMI_COMP + UMI_DBG_MSG + 3)*/
#define UM_DBG_OUTPUT_RES              (K_MSG_TYPE)0x25C3 /*(MSG_RES + MSG_UMI_COMP + UMI_DBG_MSG + 3)*/

#define UM_DBG_C100_IN_REQ             (K_MSG_TYPE)0x06C4 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 4)*/
#define MC_DBG_C100_IN_CFM             (K_MSG_TYPE)0x16C4 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 4)*/

#define MC_DBG_C100_OUT_IND            (K_MSG_TYPE)0x35C5 /*(MSG_IND + MSG_UMI_COMP + UMI_DBG_MSG + 5)*/
#define UM_DBG_C100_OUT_RES            (K_MSG_TYPE)0x25C5 /*(MSG_RES + MSG_UMI_COMP + UMI_DBG_MSG + 5)*/

#define UM_DBG_GET_FW_STATS_REQ		   (K_MSG_TYPE)0x06C5 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 5)*/
#define MC_DBG_GET_FW_STATS_CFM		   (K_MSG_TYPE)0x16C5 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 5)*/

#define UM_DBG_MAC_WATCHDOG_REQ        (K_MSG_TYPE)0x06C6 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 57)*/
#define MC_DBG_MAC_WATCHDOG_CFM        (K_MSG_TYPE)0x16C6 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 57)*/

#define UM_MAN_SET_COEX_EL_TEMPLATE_REQ			(K_MSG_TYPE)0x06C8 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 8)*/
#define MC_MAN_SET_COEX_EL_TEMPLATE_CFM			(K_MSG_TYPE)0x16C8 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 8)*/

#define UM_MAN_SET_SCAN_EXEMPTION_POLICY_REQ	(K_MSG_TYPE)0x06CD /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + D)*/
#define MC_MAN_SET_SCAN_EXEMPTION_POLICY_CFM	(K_MSG_TYPE)0x16CD /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + D)*/

#define UM_MAN_SEND_COEX_FRAME_REQ		(K_MSG_TYPE)0x06CC /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + C)*/
#define MC_MAN_SEND_COEX_FRAME_CFM		(K_MSG_TYPE)0x16CC /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + C)*/

/* Logger Messages */
#define UM_DBG_LOGGER_FLUSH_BUF_REQ    (K_MSG_TYPE)0x06C7 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 7)*/
#define MC_DBG_LOGGER_FLUSH_BUF_CFM    (K_MSG_TYPE)0x16C7 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 7)*/

#define UM_DBG_LOGGER_SET_MODE_REQ     (K_MSG_TYPE)0x06C9 /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 9)*/
#define MC_DBG_LOGGER_SET_MODE_CFM     (K_MSG_TYPE)0x16C9 /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 9)*/

#define UM_DBG_LOGGER_SET_SEVERITY_REQ (K_MSG_TYPE)0x06CA /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 10)*/
#define MC_DBG_LOGGER_SET_SEVERITY_CFM (K_MSG_TYPE)0x16CA /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 10)*/

#define UM_DBG_LOGGER_SET_FILTER_REQ   (K_MSG_TYPE)0x06CB /*(MSG_REQ + MSG_UMI_COMP + UMI_DBG_MSG + 11)*/
#define MC_DBG_LOGGER_SET_FILTER_CFM   (K_MSG_TYPE)0x16CB /*(MSG_CFM + MSG_UMI_COMP + UMI_DBG_MSG + 11)*/

#define MC_DBG_LOGGER_INIT_FAILD_IND   (K_MSG_TYPE)0x35C6 /*(MSG_IND + MSG_UMI_COMP + UMI_DBG_MSG + 6)*/
#define UM_DBG_LOGGER_INIT_FAILD_RES   (K_MSG_TYPE)0x25C6 /*(MSG_RES + MSG_UMI_COMP + UMI_DBG_MSG + 6)*/


/***************************************************************************/
/***                          Management Messages                        ***/
/***************************************************************************/

/***************************************************************************
**
** NAME         UMI_VAP_DB_OP
**
**
** DESCRIPTION:  The UM_MAN_MBSS_PRE_ACTIVATE_REQ message is issued to the firmware 
                 prior to first VAP DB ADD operation. It contains the FREQUENCY_ELEMENT structure, 
                 which contains common physical operational parameters that are used by the MAC. 
                 The FREQUENCY_ELEMENT structure has not been changed.
                 VAP DB state is set to "pre-initialized".There are no known VAPs at this point.

****************************************************************************/

#define VAP_OPERATION_QUERY        0x0 /* Returns UMI_OK if VAP exists */
#define VAP_OPERATION_ADD          0x1
#define VAP_OPERATION_DEL          0x2
typedef struct _UMI_VAP_DB_OP
{
  uint8  u8OperationCode; 
  uint8  u8VAPIdx;  /* Driver supplies the index here*/
  uint16 u16Status; /* FW returns operation result here */
} __MTLK_PACKED UMI_VAP_DB_OP;

typedef struct _UMI_MAC_VERSION
{
    uint8 u8Length;
    uint8 reserved[3];
    char  acVer[MTLK_PAD4(32 + 1)]; /* +1 allows zero termination for debug output */
} __MTLK_PACKED UMI_MAC_VERSION;


/***************************************************************************
**
** NAME         UM_MAN_RESET_REQ
**
** PARAMETERS   none
**
** DESCRIPTION  This message should be sent to disable the MAC. After sending
**              the message, ownership of the message buffer passes to the MAC
**              which will respond in due course to indicate the result of the
**              operation. This message is will only be successful when the
**              MAC is in the UMI_MAC_ENABLED state. After receiving the
**              message, the MAC will abort all transmit, receive or scan
**              operations in progress and return any associated buffers.
**
****************************************************************************/


/***************************************************************************
**
** NAME         MC_MAN_RESET_CFM
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_NOT_ENABLED
**
** DESCRIPTION  Response to request to reset the MAC.
**
****************************************************************************/
typedef struct _UMI_RESET
{
    uint16 u16Status;
    uint8  reserved[2];
} __MTLK_PACKED UMI_RESET;

/***************************************************************************
**
** NAME         UMI_POWER_MODE_REQ
**
** PARAMETERS   u16PowerMode        0 - Always powered on
**                                  1 - Power saving
**                                  2 - Dynamic selection
**
** DESCRIPTION  Station Only, change the Power saving mode request.
**              REVISIT - REPLACE MAGIC NUMBERS WITH DEFINES!
**
****************************************************************************/
typedef struct _UMI_POWER_MODE
{
    uint16 u16Status;
    uint16 u16PowerMode;
} __MTLK_PACKED UMI_POWER_MODE;


/***************************************************************************
**
** NAME         UMI_POWER_MODE_CFM
**
** PARAMETERS   u16PowerMode        0 - Always powered on
**                                  1 - Power saving
**                                  2 - Dynamic selection
**              u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_NOT_ENABLED
**
**
** DESCRIPTION  Station Only, change the Power saving mode confirmation.
**
****************************************************************************/

/* UMI_POWER_MODE (above) */


/***************************************************************************
**
** NAME         MC_MAN_READY_IND
**
** PARAMETERS   u32Magic        if UMI_READY_MAGIC, indicates following parameters
**              (little endian) are valid else it's legacy and they're not
**
**              u32MibSoftwareConfig   The MIB variable MIB_SOFTWARE_CONFIG
**              (little endian)
**
**              u32BoardStateAddressOffset    The address offset in shared RAM where the
**              (little endian)               status of the board can be read
**
** DESCRIPTION  Indication to the MAC Client that the MAC has completed its
**              initialisation phase and is ready to start or join a network.
**
**              The parameters to this message are used by the internal TTPCom
**              regression testing system.
**
****************************************************************************/
#define UMI_READY_MAGIC     0x98765432

typedef struct _UMI_READY
{
    uint32 u32Magic;
    uint32 u32MibSoftwareConfig;
#if defined(PLATFORM_FPGA_TTPCOM_SINGULLAR)
    uint32 u32BoardStateAddressOffset;
#endif
} __MTLK_PACKED UMI_READY;




/***************************************************************************
**
** NAME         MC_MAN_READY_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Response to indication
**
****************************************************************************/


/***************************************************************************
**
** NAME         UM_MAN_SET_MIB_REQ
**
** PARAMETERS   u16ObjectID         ID of the MIB Object to be set
**              uValue              Value to which the MID object should be
**                                  set.
**
** DESCRIPTION  A request to the Upper MAC to set the value of a Managed
**              Object in the MIB.
**
****************************************************************************/
typedef struct _UMI_MIB
{
    uint16    u16ObjectID;        /* ID of the MIB Object to be set */
    uint16    u16Status;          /* Status of request - confirms only */
    MIB_VALUE uValue;             /* New value for object */
} __MTLK_PACKED UMI_MIB;


/***************************************************************************
**
** NAME         MC_MAN_SET_MIB_CFM
**
** PARAMETERS   u16ObjectID         ID of the MIB Object that was to be set
**              u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_UNKNOWN_OBJECT
**                                  UMI_BAD_VALUE
**                                  UMI_NOT_NOW
**                                  UMI_READ_ONLY
**                                  UMI_RATE_MISMATCH
**
** DESCRIPTION  Response to set MIB request.
**
****************************************************************************/

/* UMI_MIB (above) */


/***************************************************************************
**
** NAME         UM_MAN_GET_MIB_REQ
**
** PARAMETERS   u16ObjectID         ID of the MIB Object to be retrieved.
**
** DESCRIPTION: This message should be sent to retrieve the value of a
**              Managed Object in the MIB.
**
****************************************************************************/

/* UMI_MIB (above) */



/***************************************************************************
**
** NAME         UM_MAN_SCAN_REQ
**
** PARAMETERS   u16BSStype          UMI_BSS_INFRA
**                                  UMI_BSS_INFRA_PCF
**                                  UMI_BSS_ADHOC
**                                  UMI_BSS_ANY
**
** DESCRIPTION: This message should be sent to request a scan of all
**              available BSSs. The BSS type parameter instructs the Upper
**              MAC only to report BSSs matching the specified type.
**
****************************************************************************/
typedef struct _UMI_SCAN_HDR
{
    IEEE_ADDR   sBSSID;
    uint16      u16MinScanTime;
    uint16      u16MaxScanTime;
    uint8       padding[2];
    MIB_ESS_ID  sSSID;
    uint8       u8NumChannels;
    uint8       u8NumProbeRequests;
    uint16      u16Status;
	uint16      u16OBSSScan;
	uint16      u16PassiveDwell;
	uint16      u16ActiveDwell;
    uint8       u8BSStype;
    uint8       u8ProbeRequestRate;
} __MTLK_PACKED UMI_SCAN_HDR;

/***************************************************************************
**
** NAME         UMI_PS
**
** PARAMETERS   PS_Mode          PS_REQ_MODE_ON
**                               PS_REQ_MODE_OFF
**
** DESCRIPTION: This message should be sent to request a chnage in 
**              power management mode where PS_Mode specifies On or Off request
**
****************************************************************************/

typedef struct _UMI_PS
{
	uint8  PS_Mode;
	uint8  status;
	uint16 reserved;
} __MTLK_PACKED UMI_PS;

/***************************************************************************
**
** NAME         UMI_PM_UPDATE
**
** PARAMETERS   sStationID   - IEEE address
**				newPowerMode - new power mode of station
**				reserved	 - FFU
**
** DESCRIPTION: This message should be sent to request a chnage in 
**              power management mode where PS_Mode specifies On or Off request
**
****************************************************************************/

typedef struct _UMI_PM_UPDATE
{
	IEEE_ADDR sStationID;
	uint8	  newPowerMode;
	uint8	  reserved;
} __MTLK_PACKED UMI_PM_UPDATE;


#define UMI_STATION_ACTIVE 0
#define UMI_STATION_IN_PS  1

/***************************************************************************
**
** NAME         UMI_CHANGE_POWER_STATE
**
** PARAMETERS   powerStateType - power state type to switch to
**				status         - return status
**				reserved	   - FFU
**
** DESCRIPTION: This message should be sent to request a change in RF
**              power state
**
****************************************************************************/

typedef struct
{
	uint8 TxNum;
	uint8 RxNum;
	uint8 status;
	uint8 reserved;
} __MTLK_PACKED UMI_CHANGE_POWER_STATE;

/***************************************************************************
**
** NAME         MC_MAN_SCAN_CFM
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_BAD_PARAMETER
**                                  UMI_BSS_ALREADY_ACTIVE
**                                  UMI_NOT_ENABLED
**                                  UMI_OUT_OF_MEMORY
**
** DESCRIPTION  Confirms the completion of a scan.
**
****************************************************************************/

/* UMI_SCAN */



/***************************************************************************
**
** NAME         UM_MAN_ACTIVATE_REQ
**
** PARAMETERS   sBSSID              The ID which identifies the Network to
**                                  be created or connected to. If the node
**                                  is a Infrastructure Station and a null
**                                  MAC Address is specified then the
**                                  request is interpreted to mean join any
**                                  suitable network.
**              sSSID               The Service Set Identifier of the ESS
**              sRSNie              RSN Information Element
**
** DESCRIPTION  Activate Request. This request should be sent to the Upper
**              MAC to start or connect to a network.
**
*****************************************************************************/
#define UMI_SC_BAND_MAX_LEN 32

/* RSN Information Element */
typedef struct _UMI_RSN_IE
{
    uint8   au8RsnIe[MTLK_PAD4(UMI_RSN_IE_MAX_LEN)];
} __MTLK_PACKED UMI_RSN_IE;

typedef struct _UMI_SUPPORTED_CHANNELS_IE
{
    uint8 asSBand[MTLK_PAD4(UMI_SC_BAND_MAX_LEN*2)]; // even bytes = u8FirstChannelNumber (0,2,4,...)
                                                     // odd bytes  = u8NumberOfChannels   (1,3,5,...)
}__MTLK_PACKED UMI_SUPPORTED_CHANNELS_IE;

typedef struct _UMI_PEER_CAPABILITIES
{
	uint16 u16HTCapabilityInfo;			// This is a copy of sHT_CAPABILITIES_INFO. The driver will parse the fields of this HT Capabilities IE.
    uint8 AMPDU_Parameters;				// 7.3.2.49.3	b0-b1   Maximum Rx A-MPDU Factor
                                        //				b2-b4   Minimum MPDU start spacing
                                        //				b5-b7   Reserved

	uint8  u8LantiqProprietry;			// B0-B2 - Is this station Proprietry (Lantiq / W101) or not 
                                        // B3 - Lantiq Proprietry LDPC enabled

    uint32 tx_bf_capabilities;          // 7.3.2.49.6  Transmit Beamforming Capability
										//          B0      TxBF Rx capable
										//          B1      Receive staggered sounding capable
										//          B2      Transmit staggered sounding capable
										//          B3      Receive NDP capable
										//          B4      Transmit NDP capable
										//          B5      Implicit TxBF capable
										//          B6-B7   Calibration
										//          B8      Explicit CSI TxBF capable
										//          B9      Explicit uncompressed BF Feedback matrix capable
										//          B10-B12 Explicit TxBF CSI feedback
										//          B13-B14 Explicit uncompressed BF Feedback Matrix
										//          B15-B16 Explicit compressed BF Feedback Matrix
										//          B17-B18 Minimal Grouping
										//			B19-B20 CSI Number of BF Antennas Supported.
										//          B21-B22 Uncompressed BF feedback matrix, number of BF antennas supported
										//          B23-B24 Compressed BF feedback matrix, number of BF antennas supported
										//          B25-B26 CSI Max number of Rows BeamFormer Supported
										//			B27-B28 Channel Estimation Capability
										//			B29-B31 Reserved
	
}__MTLK_PACKED UMI_PEER_CAPABILITIES;




typedef struct _UMI_ACTIVATE_HDR
{
    IEEE_ADDR  sBSSID;
    uint16     u16Status;
    uint16     u16RestrictedChannel;
    uint16     u16BSStype;
    MIB_ESS_ID sSSID;
    UMI_RSN_IE sRSNie;              /* RSN Specific Parameter */
	uint32      isHiddenBssID;
} __MTLK_PACKED UMI_ACTIVATE_HDR;

typedef struct _UMI_MBSS_PRE_ACTIVATE_HDR
{
	uint16     u16Status; /* FW returns operation result here */
	uint8       u8_CoexistenceEnabled;
	uint8       u8_40mhzIntolerant;
} __MTLK_PACKED UMI_MBSS_PRE_ACTIVATE_HDR;

/*********************************************************************************************
**
** NAME         _UMI_COEX_EL
**
**
** DESCRIPTION: used for passing information describing a coexistence element from SW to FW, 
**              that MUST be sent as part of the nearest appropriate management frame
**
**********************************************************************************************/
typedef struct _UMI_COEX_EL
{
	uint8 u8InformationRequest;
	uint8 u8FortyMhzIntolerant;
	uint8 u8TwentyMhzBSSWidthRequest;
	uint8 u8OBSSScanningExemptionRequest;
	uint8 u8OBSSScanningExemptionGrant;
	uint8 u8Reserved[3];
}
__MTLK_PACKED UMI_COEX_EL;

typedef struct _UMI_SCAN_EXEMPTION_POLICY
{
	uint8 u8GrantScanExemptions;
	uint8 u8Reserved[3];
} __MTLK_PACKED UMI_SCAN_EXEMPTION_POLICY;


/*********************************************************************************************
**
** NAME         UMI_VAP_LIMITS
**
**
** DESCRIPTION: defines limited number of station for each vap, updated within every addVap req
**
**********************************************************************************************/
#define MTLK_MAX_VAPS           5
#define MTLK_VAP_STA_LIMIT_NONE ((uint16)-1) /* Ignore limit*/

#define UMI_VAP_LIMITS_DEFAULT     0xFF
#define VAP_OPERATION_LIMITS_QUERY 0x0
#define VAP_OPERATIONS_LIMITS_SET  0x1

typedef struct _UMI_LIMITS_VAP_OPERATE
{
	uint8 u8MinLimit; /* Should be treat as 0 if u8MinLimit=UMI_VAP_LIMIT_DEFAULT */
	uint8 u8MaxLimit; /* Should be treat as Max. Supported STAs if u8MaxLimit=UMI_VAP_LIMIT_DEFAULT */
	uint8 u8OperationCode;
	uint8 u8Status;
} __MTLK_PACKED UMI_LIMITS_VAP_OPERATE;

/* Return values for UMI_VAP_LIMIT_DEFAULT's u8Status:
 * UMI_OK                  - Operation completed successfully
 * UMI_NOT_SUPPORTED       - Provided limits exceeds system limitations
 * UMI_BAD_PARAMETER       - (Max != -1 ) < Min
 * UMI_BSS_ALREADY_ACTIVE  - Provided limits cannot be set for active VAP
 *
 * NOTE: other values can be used if needed - FW team to define and put here
 */

/***************************************************************************
**
** NAME         UMI_ACTIVATE_VAP
**
** PARAMETERS   BSSID		: add VAP BSS ID
**				status		: confirmation status
**
** DESCRIPTION: add a VAP structure  from host
**              and confirms from MAC
**
****************************************************************************/
typedef struct _UMI_ACTIVATE_VAP
{
  IEEE_ADDR      sBSSID;
  uint16         u16Status;
  uint16         u16BSStype;
  uint8 	     isHiddenBssID;
  uint8 	     u8Reserved;
  MIB_ESS_ID     sSSID;
  UMI_RSN_IE     sRSNie;              /* RSN Specific Parameter */
} __MTLK_PACKED UMI_ACTIVATE_VAP;


/***************************************************************************
**
** NAME         UMI_DEACTIVATE_VAP
**
** PARAMETERS   BSSID		: removed VAP BSS ID
**				status		: confirmation status
**
** DESCRIPTION: remove VAP structure for remove VAP requests from host
**              and confirms from MAC
**
****************************************************************************/
typedef struct _UMI_DEACTIVATE_VAP
{
  uint16         u16Status; /* FW returns operation result here */
  uint16         u16Reserved;
} __MTLK_PACKED UMI_DEACTIVATE_VAP;


/***************************************************************************
**
** NAME         MC_MAN_ACTIVATE_CFM
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_BAD_PARAMETER
**                                  UMI_BSS_ALREADY_ACTIVE
**                                  UMI_NOT_ENABLED
**                                  UMI_BSS_UNKNOWN
**
** DESCRIPTION: Confirmation of an activate request.
**
****************************************************************************/

/* UMI_ACTIVATE */


/***************************************************************************
**
** NAME         MC_MAN_NETWORK_EVENT_IND
**
** PARAMETERS   sBSSID              The ID which identifies this Network
**              u16BSSstatus        UMI_BSS_CREATED
**                                  UMI_BSS_CONNECTED
**                                  UMI_BSS_DISCONNECTED
**                                  UMI_BSS_ID_CHANGED
**                                  UMI_BSS_LOST
**                                  UMI_BSS_FAILED
**              u16CFflag           Infrastructure Connection only -
**                                  indicates whether the Access Point has a
**                                  Point Coordinator function:
**                                  UMI_HAS_PCF
**                                  UMI_NO_PCF
**              u16Reason           Disconnection only - indicates the reason
**                                  for disassociation as defined in the
**                                  802.11a specification.
**
** DESCRIPTION  Network Event Indication. This message will be sent by the
**              Upper MAC to indicate changes in condition of the Network.
**
****************************************************************************/
typedef struct _UMI_NETWORK_EVENT
{
    IEEE_ADDR sBSSID;
    uint16    u16BSSstatus;
    uint16    u16CFflag;
	uint16    u16PrimaryChannel;
	uint8     u8SecondaryChannelOffset;
	uint8     u8Reason;
	uint8     reserved[2];
} __MTLK_PACKED UMI_NETWORK_EVENT;


/***************************************************************************
**
** NAME         UM_MAN_NETWORK_EVENT_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Return message buffer
**
****************************************************************************/


/***************************************************************************
**
** NAME         MC_MAN_CONNECTION_EVENT_IND
**
** PARAMETERS   u16Event            UMI_CONNECTED
**                                  UMI_RECONNECTED
**                                  UMI_DISCONNECTED
**              sStationID          The MAC Address of the station to which
**                                  the event applies.
**              sPrevBSSID          Reconnection only - indicates the
**                                  previous BSS that the station was
**                                  associated with.
**              u16Reason           Disconnection only - indicates the reason
**                                  for disassociation as defined in the
**                                  802.11a specification.
**              u16RSNmode          Specified if connecting station supports RSN
**              sRSNie              RSN Information Element
**
** DESCRIPTION  Connection Event Indication. This message is sent to indicate
**              when Stations connect, disconnect and reconnect to the
**              network.
**
****************************************************************************/
typedef struct _UMI_CONNECTION_EVENT
{
    uint16		u16Event;
    IEEE_ADDR	sStationID;
    IEEE_ADDR	sPrevBSSID;
    uint16		u16Reason;
    uint16		u16RSNmode;
    uint8		u8HTmode;
    uint8		boWMEsupported;
    UMI_RSN_IE	sRSNie;
    UMI_SUPPORTED_CHANNELS_IE sSupportedChannelsIE;
	UMI_PEER_CAPABILITIES sPeersCapabilities;
	uint32		u32SupportedRates;
    uint16		u16FailReason;
	uint8		twentyFortyBssCoexistenceManagementSupport;
	uint8		obssScanningExemptionGrant;
	uint8		fortyMHzIntolerant;
	uint8		twentyMHzBssWidthRequest;
	uint8		padding[2];
} __MTLK_PACKED UMI_CONNECTION_EVENT;


// bitfield for sPeersCapabilities.u8LantiqProprietry
#define LANTIQ_PROPRIETRY_VENDOR		    MTLK_BFIELD_INFO(0, 3)  /* 3 bits starting BIT0 of u8LantiqProprietry field */
#define LANTIQ_PROPRIETRY_LDPC              MTLK_BFIELD_INFO(3, 1)  /* 1 bits starting BIT3 of u8LantiqProprietry field */


/***************************************************************************
**
** NAME         UM_MAN_CONNECTION_EVENT_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Returns message buffer
**
****************************************************************************/



/***************************************************************************
**
** NAME         MC_MAN_MAC_EVENT_IND
**
** PARAMETERS
**
** DESCRIPTION  MAC Event Indication. This message will be sent by the
**              MAC ( upper or lower ) to indicate the exception occurrence.
**
****************************************************************************/
typedef struct _UMI_MAC_EVENT
{
    uint32  u32CPU;                 /* Upper or Lower MAC */
    uint32  u32CauseReg;            /* Cause register */
    uint32  u32EPCReg;              /* EPC register */
    uint32  u32StatusReg;           /* Status register */
} __MTLK_PACKED UMI_MAC_EVENT;

/***************************************************************************
**
** NAME         MC_MAN_MAC_EVENT_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Returns message buffer
**
****************************************************************************/

/* Kalish */

/***************************************************************************
**
** NAME         MC_MAN_DYNAMIC_PARAM_IND
**
** PARAMETERS   ACM_StateTable
**
** DESCRIPTION
**
****************************************************************************/
typedef struct _UMI_DYNAMIC_PARAM_TABLE
{
    uint8 ACM_StateTable[MTLK_PAD4(MAX_USER_PRIORITIES)];
    /* This table is implemented in a STA */
    /* it refers to the ACM bit which arrives from the AP*/
    /* The structure of the Array is [ AC ACM state, ...] this repeats itself four.*/
} __MTLK_PACKED UMI_DYNAMIC_PARAM_TABLE;


/***************************************************************************
**
** NAME         UM_MAN_DISCONNECT_REQ
**
** PARAMETERS   sStationID          Address of station (AP only).
**
** DESCRIPTION  Disconnect Request. This message is only sent within Access
**              Points to request disassociation of a Station from the BSS.
**
****************************************************************************/
typedef struct _UMI_DISCONNECT
{
    IEEE_ADDR sStationID;
#if defined (ENET_CFG_AP)
    uint32    vapIndex;
#else
    uint32 reserved;
#endif
    uint16    u16Status;
} __MTLK_PACKED UMI_DISCONNECT;


/***************************************************************************
**
** NAME         MC_MAN_DISCONNECT_CFM
**
** PARAMETERS   sStationID          Address of station (AP only).
**              u16Status           UMI_OK
**                                  UMI_STATION_NOT_FOUND
**
** DESCRIPTION  Disconnect Confirm
**
****************************************************************************/

/* UMI_DISCONNECT */

/***************************************************************************
**
** NAME         UMI_GET_CONNECTION_STATUS
**
** PARAMETERS   u8DeviceIndex        Index of first device to report in sDeviceStatus[].
**
** DESCRIPTION  Query various connection information.
**              Query global noise, channel load and info about associated devices.
**              For STA there is only one connected device - sDeviceStatus[0] - is AP.
**              As far as number of connected devices can exceed DEVICE_STATUS_ARRAY_SIZE
**              remaining info can be accessed with consequent requests.
**              On first request u8DeviceIndex should be set to zero.
**              On response MAC sets u8DeviceIndex to index in it's STA db,
**              or to zero if all information already reported.
**              If u8DeviceIndex reported by MAC isn't zero, MAC can be 
**              queried again for remaining info with reported u8DeviceIndex.
**              In u8NumOfDeviceStatus MAC reports number of actually written
**              entries in sDeviceStatus[].
**
****************************************************************************/

#define NUM_OF_RX_ANT 3
#define DEVICE_STATUS_ARRAY_SIZE 16

typedef struct _DEVICE_STATUS
{
     IEEE_ADDR      sMacAdd;
     uint16         u16TxRate;
     uint8          u8NetworkMode;
     uint8          au8RSSI[NUM_OF_RX_ANT];
} __MTLK_PACKED DEVICE_STATUS;

typedef struct _UMI_GET_CHANNEL_STATUS
{
	uint8          u8GlobalNoise;
	uint8          u8ChannelLoad;
	uint8		   u8Reserved[2];
} __MTLK_PACKED UMI_GET_CHANNEL_STATUS;

typedef struct _UMI_GET_PEERS_STATUS
{
	uint8          u8DeviceIndex;
	uint8          u8NumOfDeviceStatus;
	uint8		   u8Reserved[2];
	DEVICE_STATUS  sDeviceStatus[DEVICE_STATUS_ARRAY_SIZE];
} __MTLK_PACKED UMI_GET_PEERS_STATUS;

/***************************************************************************
**
** NAME         UM_MAN_GET_RSSI_REQ
**
** PARAMETERS   sStationID          The MAC Address for which an RSSI value
**                                  is desired.
**              u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_STATION_UNKNOWN
**
** DESCRIPTION  Get Receive Signal Strength Request
**
****************************************************************************/
typedef struct _UMI_GET_RSSI
{
    IEEE_ADDR sStationID;
    uint16    u16Status;
    uint16    u16RSSIvalue;
    uint8     reserved[2];
} __MTLK_PACKED UMI_GET_RSSI;


/***************************************************************************
**
** NAME         MC_MAN_GET_RSSI_CFM
**
** PARAMETERS   sStationID          The MAC Address for which an RSSI value
**                                  is required.
**              u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_STATION_UNKNOWN
**              u8RSSIvalue         The reported RSSI value.
**
** DESCRIPTION  Get Receive Signal Strength Confirm
**
****************************************************************************/

/* UMI_GET_RSSI */

/***************************************************************************
**
** NAME         UM_MAN_SET_CHANNEL_LOAD_REQ
**
** 
** DESCRIPTION  Set channel load request
**
****************************************************************************/
typedef struct _UMI_SET_CHANNEL_LOAD_VAR
{
    /* alpha-filter coefficient */
    uint8     uAlphaFilterCoefficient;
    /* channel load threshold, % */
    uint8     uChannelLoadThreshold;
    uint8     uReserved[2];
} __MTLK_PACKED UMI_SET_CHANNEL_LOAD_VAR;

/***************************************************************************
**                     SECURITY MESSAGES BEGIN                            **
***************************************************************************/

/***************************************************************************
**
** NAME         UM_MAN_SET_KEY_REQ
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_STATION_UNKNOWN
**              u16KeyType          Pairwise or group key
**              sStationID          MAC address of station
**              u16StationRole      Authenticator or supplicant
**              u16CipherSuite      Cipher suite selector
**              u16DefaultKeyIndex  For legacy WEP modes
**              au8RxSeqNum         Initial RX sequence number (little endian)
**              au8TxSeqNum         Initial TX sequence number (little endian)
**              au8Tk1              Temporal key 1
**              au8Tk2              Temporal key 2
**
** DESCRIPTION  Sets the temporal encryption key for the specified station
**
****************************************************************************/
typedef struct _UMI_SET_KEY
{
    uint16      u16Status;
    uint16      u16KeyType;
    IEEE_ADDR   sStationID;
    uint16      u16StationRole;
    uint16      u16CipherSuite;
    uint16      u16DefaultKeyIndex;
    uint8       au8RxSeqNum[MTLK_PAD4(UMI_RSN_SEQ_NUM_LEN)];
    uint8       au8TxSeqNum[MTLK_PAD4(UMI_RSN_SEQ_NUM_LEN)];
    uint8       au8Tk1[MTLK_PAD4(UMI_RSN_TK1_LEN)];
    uint8       au8Tk2[MTLK_PAD4(UMI_RSN_TK2_LEN)];
} __MTLK_PACKED UMI_SET_KEY;

/*  used for generating and sending an entire coexistence management frame based on 
	the information supplied by the caller. */

#define UMI_MAX_NUMBER_OF_INTOLERANT_CHANNELS				(30)
#define UMI_MAX_NUMBER_OF_INTOLERANT_CHANNEL_DESCRIPTORS	(4)

typedef struct _UMI_INTOLERANT_CHANNEL_DESCRIPTOR
{
	uint8 u8NumberOfIntolerantChannels;
	uint8 u8OperatingClass;
    uint8 padding[2]; 
	uint8 u8IntolerantChannels[MTLK_PAD4(UMI_MAX_NUMBER_OF_INTOLERANT_CHANNELS)];
} __MTLK_PACKED UMI_INTOLERANT_CHANNEL_DESCRIPTOR;

typedef struct _UMI_INTOLERANT_CHANNELS_REPORT
{
	uint8								u8NumberOfIntolerantChannelDescriptors;
	uint8								u8Reserved[3];
	UMI_INTOLERANT_CHANNEL_DESCRIPTOR	sIntolerantChannelDescriptors[MTLK_PAD4(UMI_MAX_NUMBER_OF_INTOLERANT_CHANNEL_DESCRIPTORS)];
} __MTLK_PACKED UMI_INTOLERANT_CHANNELS_REPORT;

typedef struct _UMI_COEX_FRAME
{
	IEEE_ADDR                       sDestAddr;
	uint16                          u16Padding;
	UMI_COEX_EL						sCoexistenceElement;		/* provide information for the coexistence element to be included in the coexistence frame*/
	UMI_INTOLERANT_CHANNELS_REPORT	sIntolerantChannelsReport;	/* provide information for the intolerant channels report. */
} __MTLK_PACKED UMI_COEX_FRAME;

/*The frame will be built by the firmware and the transmitted just once.*/

/***************************************************************************
**
** NAME         UM_MAN_CLEAR_KEY_REQ
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_STATION_UNKNOWN
**              u16KeyType          Pairwise or group key
**              sStationID          MAC address of station
**
** DESCRIPTION  Clears the temporal encryption key for the specified station
**
****************************************************************************/
typedef struct _UMI_CLEAR_KEY
{
    uint16      u16Status;
    uint16      u16KeyType;
    IEEE_ADDR   sStationID;
    uint8       reserved[2];
} __MTLK_PACKED UMI_CLEAR_KEY;

/***************************************************************************
**
** NAME         UM_MAN_GET_GROUP_PN_REQ
**
** PARAMETERS   UMI_GROUP_PN: empty structure to be filled on CFM
**
** DESCRIPTION  Requests the group transmit security sequence number
**
****************************************************************************/

/***************************************************************************
**
** NAME         UM_MAN_GET_GROUP_PN_CFM
**
** PARAMETERS   u16Status            UMI_OK
**                                   UMI_NOT_INITIALISED
**              au8TxSeqNum[6]       Group transmit security sequence count
**
** DESCRIPTION  Sends the group transmit security sequence number to higher
**              layer
**
****************************************************************************/
typedef struct _UMI_GROUP_PN
{
    uint16      u16Status;
#if defined (ENET_CFG_AP)
    uint8       vapIndex;
    uint8       reserved[1];
#else
    uint8 reserved[2];
#endif
    uint8       au8TxSeqNum[MTLK_PAD4(UMI_RSN_SEQ_NUM_LEN)];
} __MTLK_PACKED UMI_GROUP_PN;

/***************************************************************************
**
** NAME         UM_MAN_SECURITY_ALERT_IND
**
** PARAMETERS   u16Status           Event code - see rsn.h
**              u16KeyType          Pairwise or group key
**              sStationID          MAC address of station
**
** DESCRIPTION  Alerts higher layers that a security related event has
**              occurred that may need acting on by re-keying or
**              disconnecting
**
****************************************************************************/
typedef struct _UMI_SECURITY_ALERT
{
    uint16      u16EventCode;
    uint16      u16KeyType;
    IEEE_ADDR   sStationID;
    uint8       reserved[2];
} __MTLK_PACKED UMI_SECURITY_ALERT;

/***************************************************************************
**                        SECURITY MESSAGES END                           **
***************************************************************************/


/***************************************************************************
**
** NAME         UM_MAN_SET_BCL_VALUE/UM_MAN_QUERY_BCL_VALUE
**
** PARAMETERS   Unit
**              Address
**              Size
**              Data
**
** DESCRIPTION  Sets/queries BCL data from MAC
**
****************************************************************************/
#define MAX_GENERIC_REQ_DATA                    64

typedef struct _UMI_BCL_REQUEST
{
    uint32         Unit;
    uint32         Address;
    uint32         Size;
    uint32         Data[MAX_GENERIC_REQ_DATA];
} __MTLK_PACKED UMI_BCL_REQUEST;


/* Kalish */

/***************************************************************************
**
** NAME         UM_MAN_OPEN_AGGR_REQ
**
** PARAMETERS   sDA
**              u16AccessProtocol   - former u16Priority
**              pad
**              u16MaxNumOfPackets
**              u32MaxNumOfBytes
**              u32TimeoutInterval
**              u32MinSizeOfPacketInAggr
**
**              u16Index - removed
**
** DESCRIPTION  Directive to set the aggregation parameters
**
****************************************************************************/
typedef struct _UMI_OPEN_AGGR_REQ
{
    IEEE_ADDR   sDA;
    uint16      u16AccessProtocol;
    uint16      u16MaxNumOfPackets;
	uint16      u16Status;
    uint32      u32MaxNumOfBytes;
    uint32      u32TimeoutInterval;
    uint32      u32MinSizeOfPacketInAggr;
} __MTLK_PACKED UMI_OPEN_AGGR_REQ;

/***************************************************************************
**
** NAME         UM_MAN_CLOSE_AGGR_REQ
**
** PARAMETERS   sDA
**              u16AccessProtocol
**
** DESCRIPTION  Directive to set the aggregation parameters
**
****************************************************************************/
typedef struct _UMI_CLOSE_AGGR_REQ
{
    IEEE_ADDR   sDA;
    uint16      u16AccessProtocol;
	uint16      u16Status;
	uint16      reserved;
} __MTLK_PACKED UMI_CLOSE_AGGR_REQ;

typedef struct _UMI_ADDBA_REQ_SEND
{
    IEEE_ADDR   sDA;
    uint8       u8DialogToken;
    uint8       u8BA_WinSize_O;
    uint16      u16AccessProtocol;
    uint16      u16BATimeout;
	uint16      u16Status;
	uint16      reserved;
} __MTLK_PACKED UMI_ADDBA_REQ_SEND;

typedef struct _UMI_ADDBA_REQ_RCV
{
    IEEE_ADDR   sSA;                /* Transmitter Address RA*/
    uint8       u8DialogToken;
    uint8       u8WinSize;          /* Extracted from the ADDBA request */
    uint16      u16AccessProtocol;  /* TID */
    uint16      u16StartSN;         /* set to SSN value extracted from the ADDBA request */
    uint16      u16AckPolicy;       /* Ack Policy*/
    uint16      u16BATimeout;       /* Timeout */
} __MTLK_PACKED UMI_ADDBA_REQ_RCV;

typedef struct _UMI_ADDBA_RES_SEND
{
    IEEE_ADDR   sDA;                /* Receiver Address TA*/
    uint8       u8DialogToken;
    uint8       u8WinSize;          /* Actual buffer size */
    uint16      u16AccessProtocol;  /* TID */
    uint16      u16ResultCode;      /* Response Status */
    uint16      u16BATimeout;
	uint16      u16Status;
} __MTLK_PACKED UMI_ADDBA_RES_SEND;

typedef struct _UMI_ADDBA_RES_RCV
{
    IEEE_ADDR   sSA;                /* Transmitter Address RA*/
    uint8       u8DialogToken;
    uint8       reserved[1];
    uint16      u16AccessProtocol;  /* TID */
    uint16      u16ResultCode;      /* Response status */
} __MTLK_PACKED UMI_ADDBA_RES_RCV;

typedef struct _UMI_DELBA_REQ_SEND
{
    IEEE_ADDR   sDA;                /* Receiver Address TA*/
    uint16      u16Intiator;        /* ADDBA agreement side 1- Stop aggregation that we initiate
                                    0 -Stop aggregation from connected side */
    uint16      u16AccessProtocol;  /* TID */
    uint16      u16ResonCode;       /* Response Status */
	uint16      u16Status;
	uint16      reserved;
} __MTLK_PACKED UMI_DELBA_REQ_SEND;

typedef struct _UMI_DELBA_REQ_RCV
{
    IEEE_ADDR   sSA;                /* Transmitter Address RA*/
    uint16      u16Intiator;        /* ADDBA agreement side 0- Stop aggregation that we initiate
                                       1 -Stop aggregation from connected side */
    uint16      u16AccessProtocol;  /* TID */
    uint16      u16ResultCode;      /* Response Status */
} __MTLK_PACKED UMI_DELBA_REQ_RCV;

typedef struct _UMI_ACTION_FRAME_GENERAL_SEND
{
    IEEE_ADDR   sDA;                /* Receiver Address TA*/
    uint8       u8ActionCode;
    uint8       u8DialogToken;
} __MTLK_PACKED UMI_ACTION_FRAME_GENERAL_SEND;


/***************************************************************************
**
** NAME         MC_MAN_BAR_IND
**
** PARAMETERS   u16AccessProtocol   This field is used to transfer
**                                  TID (Traffic ID) of the traffic stream (TS)
**              sSA                 Transmitter Source Ethernet Address,
**                                  used as Transmitter Address (TA)
**              u16SSN              Set to SSN value extracted from the BAR MSDU
**
** DESCRIPTION  Indication to the upper MAC including info on the BAR
**
****************************************************************************/
typedef struct _UMI_BAR_IND
{
    uint16      u16AccessProtocol;
    IEEE_ADDR   sSA;
    uint16      u16SSN;
    uint8       reserved[2];
} __MTLK_PACKED UMI_BAR_IND;

/***************************************************************************
**
** NAME         UM_DAT_BAR_RES
**
** PARAMETERS   None
**
** DESCRIPTION  MC response upon receiving BAR indication
**
****************************************************************************/



/***************************************************************************
**
** NAME         UMI_GENERIC_MAC_REQUEST
**
** PARAMETERS   none
**
** DESCRIPTION  TODO
**
****************************************************************************/
typedef struct _UMI_GENERIC_MAC_REQUEST
{
    uint32 opcode;
    uint32 size;
    uint32 action;
    uint32 res0;
    uint32 res1;
    uint32 res2;
    uint32 retStatus;
    uint32 data[MAX_GENERIC_REQ_DATA];
} __MTLK_PACKED UMI_GENERIC_MAC_REQUEST;

#define MT_REQUEST_GET                  0
#define MT_REQUEST_SET                  1

#define EEPROM_ILLEGAL_ADDRESS          0x3
/***************************************************************************
**
** NAME         UMI_GENERIC_IE
**
** PARAMETERS
**              u8Type            IE type. Predefined types:
**                                  0(UMI_WPS_IE_BEACON) - WPS IE in beacon ;
**                                  1(UMI_WPS_IE_PROBEREQUEST) - WPS IE in probe request;
**                                  2(UMI_WPS_IE_PROBERESPONSE) - WPS IE in probe response;
**                                  3(UMI_WPS_IE_ASSOCIATIONREQUEST) - WPS IE in association request (optional);
**                                  4(UMI_WPS_IE_ASSOCIATIONRESPONSE) - WPS IE in association response (optional).
**              u8reserved[1]     Added for aligning.
**              u16Length         Size of WPS IE. If u16Length == 0 then WPS IE deleted.
**              au8IE             Whole IE.
**
** DESCRIPTION  Used in request from the driver to add WPS IE to beacons,
**              probe requests and responses
**
****************************************************************************/
#define UMI_MAX_GENERIC_IE_SIZE         257
#define UMI_WPS_IE_BEACON               0
#define UMI_WPS_IE_PROBEREQUEST         1
#define UMI_WPS_IE_PROBERESPONSE        2
#define UMI_WPS_IE_ASSOCIATIONREQUEST   3
#define UMI_WPS_IE_ASSOCIATIONRESPONSE  4

typedef struct _UMI_GENERIC_IE
{
    uint8  u8Type;
    uint8  u8reserved[1];
    uint16 u16Length;
    uint8  au8IE[UMI_MAX_GENERIC_IE_SIZE];
} __MTLK_PACKED UMI_GENERIC_IE;

/***************************************************************************
**
** NAME         UM_MAN_SET_LED_REQ
**
** PARAMETERS   u8BasebLed - 
**              u8LedStatus - 
**              reserved
**
** DESCRIPTION  TODO
**
****************************************************************************/
typedef struct _UMI_SET_LED
{
    uint8 u8BasebLed;
	uint8 u8LedStatus;
	uint8 status;
	uint8 reserved[1];
} __MTLK_PACKED UMI_SET_LED;


/****************************************************************************
**
** NAME:           vUM_MAN_INIT_GPIO_REQ
**
** PARAMETERS:
**
** RETURN VALUES:  none
**
** DESCRIPTION:    Handle vUM_MAC_EVENT_IND message
**                 the GPIO driver support multi GPIO activation.
**                 uActiveGpios - is bitmask of all active GPIOs.
**                 toggle_pgio[] - struct array, used for multi-leds blink operation
**
****************************************************************************/

typedef struct _UMI_CONFIG_GPIO
{
	uint8 uDisableTestbus; // used to disable testbus (LVDS) in order to use LEDs on the same GPIOs.
	uint8 uActiveGpios;  // Bitmap of active GPIOs on this specific platform
	uint8 bLedPolarity;
	uint8 reserved[1];
} __MTLK_PACKED UMI_CONFIG_GPIO;


/*****************************/



typedef struct _UMI_DEF_RF_MGMT_DATA
{
	uint8     u8Data;        /* set: IN - a RF Management data (depend on RF MGMT type), OUT - ignored 
						      * get: IN - ignored, OUT - a RF Management data (depend on RF MGMT type) */
	uint8     u8Status;      /* set & get: IN - ignored, OUT - a UMI_STATUS error code */
	uint8     u8Reserved[2]; /* set & get: IN - ignored, OUT - ignored */
} __MTLK_PACKED UMI_DEF_RF_MGMT_DATA;

typedef struct _UMI_RF_MGMT_TYPE
{
	uint8  u8RFMType;  /* set: IN - a MTLK_RF_MGMT_TYPE_... value, OUT - ignored
						* get: IN - ignored, OUT - a MTLK_RF_MGMT_TYPE_... value
						*/
	uint8  u8HWType;  /* set & get: IN - ignored, OUT - a MTLK_HW_TYPE_... value */
	uint16 u16Status; /* set & get: IN - ignored, OUT - a UMI_STATUS error code */
} __MTLK_PACKED UMI_RF_MGMT_TYPE;

/***************************************************************************
**
** NAME         UMI_CHANGE_TX_POWER_LIMIT
**
** PARAMETERS   PowerLimitOption - 
**
** DESCRIPTION: This message should be sent to request a change to the transmit
**              power limit table.
**
****************************************************************************/
typedef struct UMI_TX_POWER_LIMIT
{
	uint8 TxPowerLimitOption;
    uint8 Status;
	uint8 Reserved[2];
} __MTLK_PACKED UMI_TX_POWER_LIMIT;

/* MTLK Vendor Specific Action Frame UM_MAN_SEND_MTLK_VSAF_REQ 
 */
#define MAX_VSAF_DATA_SIZE (MAX_GENERIC_REQ_DATA * sizeof(uint32))

typedef struct _UMI_VSAF_INFO
{
  IEEE_ADDR sDA;
  uint16    u16Size;
  uint8     u8RFMgmtData;
  uint8		u8Status;
  uint8		u8Rank;//Num of spatial streams 
  uint8     u8Reserved[1];
  /**********************************************************************
   * NOTE: u8Category and au8OUI are added for the MAC convenience.
   *       They are constants and should be always set by the driver to:
   *        - u8Category = ACTION_FRAME_CATEGORY_VENDOR_SPECIFIC
   *        - au8Data    = { MTLK_OUI_0, MTLK_OUI_1, MTLK_OUI_2 }
   **********************************************************************/
  uint8     u8Category;
  uint8     au8OUI[3]; /* */
  /**********************************************************************/
  uint8     au8Data[MTLK_PAD4(MAX_VSAF_DATA_SIZE)];
} __MTLK_PACKED UMI_VSAF_INFO;

/***************************************************************************
**
** NAME         UMI_MAC_WATCHDOG
**
** PARAMETERS   none
**
** DESCRIPTION  MAC Soft Watchdog
**
****************************************************************************/

typedef struct _UMI_MAC_WATCHDOG
{
	uint8  u8Status;  /* WD Status */
	uint8  u8Reserved[1];
	uint16 u16Timeout; /* Timeout for waiting answer from LM in milliseconds*/
} __MTLK_PACKED UMI_MAC_WATCHDOG;

/***************************************************************************/
/***                           Debug Messages                            ***/
/***************************************************************************/

/***************************************************************************
**
** NAME         UM_DBG_RESET_STATISTICS_REQ
**
** PARAMETERS   none
**
** DESCRIPTION  This message should be sent to reset the statistics
**              maintained by the MAC software to their default values. The
**              MAC will confirm that the request has been processed with a
**              MC_MAN_RESET_STATISTICS_CFM.
**
****************************************************************************/


/***************************************************************************
**
** NAME         MC_DBG_RESET_STATISTICS_CFM
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**
** DESCRIPTION  Response to reset statistics request.
**
****************************************************************************/
typedef struct _UMI_RESET_STATISTICS
{
    uint16 u16Status;
    uint8  reserved[2];
} __MTLK_PACKED UMI_RESET_STATISTICS;


/***************************************************************************
**
** NAME         UM_DBG_GET_STATISTICS_REQ
**
** PARAMETERS   u16Status           Not used
**              u16Ident            REVISIT - what is this?
**              sStats              Structure containing statistics.
**
** DESCRIPTION  This message should be sent to request a copy of the current
**              set of statistics maintained by the MAC software.
**
****************************************************************************/
/* MAC statistics counters */
typedef struct _UMI_STATISTICS
{
    uint32 au32Statistics[STAT_TOTAL_NUMBER];
} __MTLK_PACKED UMI_STATISTICS;

typedef struct _UMI_GET_STATISTICS
{
    uint16         u16Status;
    uint16         u16Ident;
    UMI_STATISTICS sStats;
} __MTLK_PACKED UMI_GET_STATISTICS;


/***************************************************************************
**
** NAME         UM_DBG_GET_FW_STATS_REQ
**
** PARAMETERS   u16StatId           ID of the desired statistics
**              u16StatIndex        index within the array if it is two dimenstional
**				u16Status			UMI_OK / UMI_BAD_VALUE
**              sStats              Structure containing statistics.
**
** DESCRIPTION  This message should be sent to request a copy of the current
**              set of statistics maintained by the MAC software.
**
****************************************************************************/

typedef struct _UMI_GET_FW_STATS
{
	uint32				u32StatId; // use /* IDs list */ from mhi_statistics.h
	uint16				u16StatIndex;
    uint16				u16Status;
	DBG_FW_Statistics	uStatistics;
} __MTLK_PACKED UMI_GET_FW_STATS;


/***************************************************************************
**
** NAME         MC_DBG_GET_STATISTICS_CFM
**
** PARAMETERS   u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**
** DESCRIPTION  Response to get statistics request to return message buffer.
**
****************************************************************************/

/* UMI_GET_STATISTICS */


/***************************************************************************
**
** NAME         UM_DBG_INPUT_REQ
**
** PARAMETERS   u16Length           The number of bytes of input stream
**                                  contained in this message.
**              au8Data             An array of characters containing a
**                                  section of debug input stream.
**
** DESCRIPTION  Debug Input Request
**
****************************************************************************/
typedef struct _UMI_DEBUG
{
    uint16 u16Length;
    uint16 u16stream;
    uint8  au8Data[MTLK_PAD4(UMI_DEBUG_DATA_SIZE)];
} __MTLK_PACKED UMI_DEBUG;


/***************************************************************************
**
** NAME         MC_DBG_INPUT_CFM
**
** PARAMETERS   none
**
** DESCRIPTION  Return message buffer.
**
****************************************************************************/


/***************************************************************************
**
** NAME         MC_DBG_OUTPUT_IND
**
** PARAMETERS   pau8Addr            The address in shared memory of a section
**                                  of output stream.
**              u16Length           The number of bytes of output stream to
**                                  be read.
**
** DESCRIPTION  Debug Output Indication
**
****************************************************************************/

/* UMI_DEBUG */


/***************************************************************************
**
** NAME         UM_DBG_OUTPUT_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Returns message buffer.
**
****************************************************************************/




/***************************************************************************
**
** NAME         UMI_DBG
**
** DESCRIPTION  A union of all Debug Messages.
**
****************************************************************************/
typedef union _UMI_DBG
{
    UMI_RESET_STATISTICS sResetStatistics;
    UMI_GET_STATISTICS   sGetStatistics;
	UMI_GET_FW_STATS	 sGetFwStats;
    UMI_DEBUG            sDebug;
} __MTLK_PACKED UMI_DBG;



/***************************************************************************/
/***                            Data Messages                            ***/
/***************************************************************************/

/***************************************************************************
**
** NAME         UM_DAT_TXDATA_REQ
**
** PARAMETERS   u32MSDUtag          Reference to the buffer containing the
**                                  payload of the MSDU in external memory.
**              u16MSDUlength       Length of the MSDU payload in the range
**                                  0..UMI_MAX_MSDU_LENGTH.
**              u16AccessProtocol   UMI_USE_DCF
**                                  UMI_USE_PCF
**              sSA                 Source MAC Address (AP only).
**              sDA                 Destination MAC Address.
**              sWDSA               Wireless Distribution System Address
**                                  (reserved).
**              u16Status           Not used.
**              pvMyMsdu            Reserved for use by the MAC.
**
** DESCRIPTION  Transmit Data Request
**
****************************************************************************/
/* <O.H> - Data Request Message Descriptor (TX) */

#define TX_DATA_INFO_WDS    MTLK_BFIELD_INFO(0, 1)  /*  1 bit  starting bit0 */
#define TX_DATA_INFO_TID    MTLK_BFIELD_INFO(1, 3)  /*  3 bits starting bit1 */
#define TX_DATA_INFO_LENGTH MTLK_BFIELD_INFO(4, 12) /* 12 bits starting bit4 */

#define TX_EXTRA_ENCAP_TYPE				MTLK_BFIELD_INFO(0, 7)  /* 7 LS bits */
#define TX_EXTRA_NUM_OF_RETRANSMISSIONS	MTLK_BFIELD_INFO(0, 8)  /* overlaying ENCAP_TYPE and IS_SOUNDING since driver is not using it when we send the UMI_DATA_TX to him */
#define TX_EXTRA_IS_SOUNDING			MTLK_BFIELD_INFO(7, 1)  /* 1 MS bit  */

#define MTLK_RF_MGMT_DATA_DEFAULT 0x00

/* Values for u8PacketType in UMI_DATA_TX struct */
#define ENCAP_TYPE_RFC1042           0
#define ENCAP_TYPE_STT               1
#define ENCAP_TYPE_8022              2
#define ENCAP_TYPE_ILLEGAL           MTLK_BFIELD_VALUE(TX_EXTRA_ENCAP_TYPE, -1, uint8)

typedef struct _UMI_DATA_TX
{
    IEEE_ADDR sRA;
    uint16    u16FrameInfo; /* use FRAME_INFO_... macros for access */
    uint32    u32HostPayloadAddr;
    uint8     u8RFMgmtData;
    uint8     u8Status;
#if defined (ENET_CFG_AP)
    uint8     vapId;
#else
    uint8     u8Reserved;
#endif
    uint8     u8ExtraData; /* see TX_EXTRA_... for available values */
} __MTLK_PACKED UMI_DATA_TX;

typedef UMI_DATA_TX TXDAT_REQ_MSG_DESC;

/* This was the old Data Request Message Descriptor */
typedef struct _UMI_DATA_RX
{
    uint32    u32MSDUtag;
    uint16    u16MSDUlength;
	uint8     u8Notification;
	uint8     u8Offset;
    uint16    u16AccessProtocol;
    IEEE_ADDR sSA;
    IEEE_ADDR sDA;
    IEEE_ADDR sWDSA;
    mtlk_void_ptr psMyMsdu;
} __MTLK_PACKED UMI_DATA_RX;


/***************************************************************************
**
** NAME         MC_DAT_TXDATA_CFM
**
** PARAMETERS   u32MSDUtag          Reference to the buffer containing the
**                                  payload of the MSDU that was transmitted.
**              u16MSDUlength       As request.
**              u16AccessProtocol   As request.
**              sSA                 As request.
**              sDA                 As request.
**              sWDSA               As request.
**              u16Status           UMI_OK
**                                  UMI_NOT_INITIALISED
**                                  UMI_BAD_LENGTH
**                                  UMI_TX_TIMEOUT
**                                  UMI_BSS_HAS_NO_PCF
**                                  UMI_NOT_CONNECTED
**                                  UMI_NOT_INITIALISED
**
** DESCRIPTION  Transmit Data Confirm
**
****************************************************************************/

/* UMI_DATA */


/***************************************************************************
**
** NAME         RXDAT_IND_MSG_DESC (used for MC_DAT_RXDATA_IND)
**
** PARAMETERS   u32HostPayloadAddr       Reference to the payload address in the host memory
**
** DESCRIPTION  Receive Data Indication
**
****************************************************************************/
/* <O.H> - new Data Indication Message Descriptor (RX) */
typedef struct
{
    uint32 u32HostPayloadAddr;
} __MTLK_PACKED RXDAT_IND_MSG_DESC;


/***************************************************************************
**
** NAME         RX_ADDITIONAL_INFO
**
** PARAMETERS   u8EstimatedNoise
**				u8MinNoise      
**              u8Channel
**              u8RSN
**				u16Length
**				u8RxRate
**				au8Rssi - all 3 rssi
**				u8MaxRssi
**
** DESCRIPTION  Additional Receive Data Information
**
****************************************************************************/
typedef struct MAC_RX_ADDITIONAL_INFO
{
    uint8	u8EstimatedNoise; /* the estimated noise in RF (noise in BB + rx path noise gain)*/     
	uint8	u8MinNoise;   
    uint8	u8Channel;
    uint8	u8RSN; /* MAC will send here 0 or 1 */
	uint16  u16PhyRxLen;
	uint8  	u8RxRate;          
	uint8   u8HwInfo; // Bitfield. Used to provide the driver extra data about the packet
	int8   aRssi[3];
	int8   MaxRssi;/* Can be calculated in driver in case we need this memory for another variable */
	
}__MTLK_PACKED MAC_RX_ADDITIONAL_INFO_T;

#define HW_INFO_SCP       MTLK_BFIELD_INFO(7, 1)  /*  1 bits starting bit7 [0=Long CP, 1=short CP ]*/
#define HW_INFO_AGGR_LAST MTLK_BFIELD_INFO(0, 1)  /*  1 bits starting bit1 [0=Not a last, 1=Last packet in aggregation]*/
#define HW_INFO_AGGR      MTLK_BFIELD_INFO(1, 1)  /*  1 bits starting bit0 [0=Legacy, 1=Aggregated]*/

#define HW_RX_RATE_CB     MTLK_BFIELD_INFO(7, 1)  /*  1 bits starting bit7 [0=nCB, 1=CB ] */
#define HW_RX_RATE_HT     MTLK_BFIELD_INFO(6, 1)  /*  1 bits starting bit6 [0=non-HT, 1=HT ] */
#define HW_RX_RATE_B      MTLK_BFIELD_INFO(5, 1)  /*  1 bits starting bit5 [0=Not a B rate, 1=B rate]*/
#define HW_RX_RATE_MCS    MTLK_BFIELD_INFO(0, 5)  /*  5 bits starting bit0 [for N: 0-16, for A/B - 0-7] */

/***************************************************************************
**
** NAME         UM_DAT_RXDATA_RES
**
** PARAMETERS   u32MSDUtag          Reference to the RX buffer that was
**                                  received.
**              u16MSDUlength       As request.
**              u16AccessProtocol   As request.
**              sSA                 As request.
**              sDA                 As request.
**              sWDSA               As request.
**              u16Status           UMI_OK
**
** DESCRIPTION  Sent by the MAC Client in response to a MC_DAT_RXDATA_IND
**              indication. Returns the message buffer to the Upper MAC.
**
****************************************************************************/

/* UMI_DATA */


/***************************************************************************
**
** NAME         MC_DAT_ALLOC_BUFFER_IND
**
** PARAMETERS   u32MSDUtag          Not used.
**              u16MSDUlength       The size of the required buffer in bytes.
**              u16AccessProtocol   Not used.
**              sSA                 Not used.
**              sDA                 Not used.
**              sWDSA               Not used.
**              u16Status           Not used.
**
** DESCRIPTION  Request for MAC Client to allocate a buffer in external
**              memory.
**
****************************************************************************/

/* UMI_DATA */


/***************************************************************************
**
** NAME         UM_DAT_ALLOC_BUFFER_RES
**
** PARAMETERS   u32MSDUtag          Reference to the buffer in external
**                                  memory which has been allocated.
**              u16MSDUlength       Length of the buffer that was allocated.
**              u16AccessProtocol   Not used.
**              sSA                 Not used.
**              sDA                 Not used.
**              sWDSA               Not used.
**              u16Status           UMI_OK
**                                  UMI_OUT_OF_MEMORY
**
** DESCRIPTION  Response to Upper MAC request for the MAC Client to allocate
**              a buffer in external memory.
**
****************************************************************************/

/* UMI_DATA */


/***************************************************************************
**
** NAME         MC_DAT_FREE_BUFFER_IND
**
** PARAMETERS   u32MSDUtag          Reference to the buffer in external
**                                  memory to be deallocated.
**              u16MSDUlength       Not used.
**              u16AccessProtocol   Not used.
**              sSA                 Not used.
**              sDA                 Not used.
**              sWDSA               Not used.
**              u16Status           Not used.
**
** DESCRIPTION  Returns a previously allocated buffer to the MAC Client.
**
****************************************************************************/

/* UMI_DATA */


/***************************************************************************
**
** NAME         UM_DAT_FREE_BUFFER_RES
**
** PARAMETERS   none
**
** DESCRIPTION  Return message buffer
**
****************************************************************************/


/***************************************************************************
**
** NAME         UMI_MEM
**
** DESCRIPTION  A union of all Data Messages.
**
****************************************************************************/
typedef union _UMI_DAT
{
    UMI_DATA_TX sDataTx;
    UMI_DATA_RX sDataRx;
} __MTLK_PACKED UMI_DAT;



/***************************************************************************/
/***                         Memory Messages                             ***/
/***************************************************************************/

/***************************************************************************
**
** NAME         MC_MEM_COPY_FROM_MAC_IND & UM_MEM_COPY_FROM_MAC_RES
**
** PARAMETERS   pau8SourceAddr      Source address for the transfer.
**              u32DestinationTag   The destination tag for the transfer
**                                  (i.e. address only understood by the MAC
**                                  Client).
**              u16TransferOffset   Offset from Destination Address in bytes
**                                  at which the transfer should start.
**              u16TransferLength   Number of bytes to transfer.
**              u16Status           Only used in response:
**                                  UMI_OK
**                                  UMI_TRANSFER_ALREADY_ACTIVE
**                                  UMI_TRANSFER_FAILED
**
** DESCRIPTION  Request from Upper MAC to copy a block of data from the
**              specified location in the MAC memory to external memory. The
**              same message structure is used in the reply from the MAC
**              Client.
**
****************************************************************************/
typedef struct _UMI_COPY_FROM_MAC
{
    mtlk_uint8_ptr pau8SourceAddr;
    mtlk_umidata_ptr psDestinationUmiData;
    uint16 u16TransferOffset;
    uint16 u16TransferLength;
    uint16 u16Status;
    uint8  reseved[2];
} __MTLK_PACKED UMI_COPY_FROM_MAC;


/***************************************************************************
**
** NAME         MC_MEM_COPY_TO_MAC_IND & UM_MEM_COPY_TO_MAC_RES
**
** PARAMETERS   u32SourceTag        Source tag for the transfer (i.e. address
**                                  only understood by the MAC Client).
**              pau8DestinationAddr Destination address for the transfer.
**              u16TransferOffset   Offset from Destination Address in bytes
**                                  at which the transfer should start.
**              u16TransferLength   Number of bytes to transfer.
**              u16Status           Only used in response:
**                                  UMI_OK
**                                  UMI_TRANSFER_ALREADY_ACTIVE
**                                  UMI_TRANSFER_FAILED
**
** DESCRIPTION  Request from Upper MAC to copy a block of data from external
**              memory to the specified location in MAC memory. The same
**              message structure is used in the reply from the MAC Client.
**
****************************************************************************/
typedef struct _UMI_COPY_TO_MAC
{
    mtlk_umidata_ptr psSourceUmiData;
    mtlk_uint8_ptr pau8DestinationAddr;
    uint16 u16TransferOffset;
    uint16 u16TransferLength;
    uint16 u16Status;
    uint8  reseved[2];
} __MTLK_PACKED UMI_COPY_TO_MAC;


/* Logger <-> HIM Messages */

typedef struct
{
    uint32 logAgentLoggerGroupsBitMap[LOGGER_NUM_OF_GROUPS_BIT_MAP];
}LogAgentLoggerGroupsBitMap_t;

typedef enum
{
	LOGGER_STATE_READY,
	LOGGER_STATE_INACTIVE,
	LOGGER_STATE_ACTIVE,
	LOGGER_STATE_INIT_FAILED,
	LOGGER_STATE_CYCLIC_MODE,
	LOGGER_STATE_MAX = MAX_UINT8
} LogAgentState_e;

/*****************************************************************
**	LogAgentSeverityLevel_t and LOGGER_SEVERITY definitions -
**	used for setting logger severity level
******************************************************************/
#define LOGGER_SEVERITY_ERROR			(-2)
#define LOGGER_SEVERITY_WARNING			(-1)
#define LOGGER_SEVERITY_INFORMATION0	(0)
#define LOGGER_SEVERITY_INFORMATION1	(1)
#define LOGGER_SEVERITY_INFORMATION2	(2)
#define LOGGER_SEVERITY_INFORMATION3	(3)
#define LOGGER_SEVERITY_INFORMATION4	(4)
#define LOGGER_SEVERITY_INFORMATION5	(5)
#define LOGGER_SEVERITY_INFORMATION6	(6)
#define LOGGER_SEVERITY_INFORMATION7	(7)
#define LOGGER_SEVERITY_INFORMATION8	(8)
#define LOGGER_SEVERITY_INFORMATION9	(9)		//highest debug level - all logs will be output

#define LOGGER_SEVERITY_DEFAULT_LEVEL   LOGGER_SEVERITY_INFORMATION9   //default severityLevel level

typedef int16 LogAgentSeverityLevel_t;

/***************************************************************************
**
** NAME         BUFFER_DAT_IND_MSG_DESC
**              used for MC_DAT_LOGGERDATA_IND
**
** PARAMETERS   u32HostPayloadAddr - Reference to the payload address in
**                                   the host memory
**
** DESCRIPTION  Receive Data Indication
**
****************************************************************************/
typedef struct
{
    uint32 u32HostPayloadAddr;
} __MTLK_PACKED BUFFER_DAT_IND_MSG_DESC;

/***************************************************************************
**
** NAME         MC_DAT_SEND_BUF_TO_HOST_IND
**
** PARAMETERS   length - Length (in bytes) of actual buffer payload
**              buffer - Pointer to the buffer being sent
**
** DESCRIPTION  This message is sent by the FW MAC, over the Host/IF module,
**              to deliver a logAgent buffer from the MAC to the Host
**              (and eventually out to the LogViewer through the LogServer).
**
****************************************************************************/
typedef struct
{
    uint32  length;
    char*   buffer;
} __MTLK_PACKED UmiLoggerMsgSendBuffer_t;

/***************************************************************************
**
** NAME         UM_DAT_SEND_BUF_TO_HOST_RES
**
** PARAMETERS   None
**
** DESCRIPTION  This message is sent by the Host when the buffer sent to it 
**              was handled by the Host/IF and is given back to the sender.
**              Upon receiving this message the buffer will be considered free.
**
****************************************************************************/

/* UM_DAT_SEND_BUF_TO_HOST_RES */
/* UmiLoggerMsgSendBuffer_t from IND  */


/***************************************************************************
**
** NAME         MC_DBG_LOGGER_INIT_FAILD_IND
**
** PARAMETERS   u8Type  - Type of message (ARRAY_DAT_LOGGER_IND).
**              u8Index - Index of the message in the array.
**              u16Info - Indication to the host that logAgent's initialization failed.
**
** DESCRIPTION  This message is sent by the MAC FW, to notify the Host that
**              the logAgent failed to initialize. The Host can use this to
**              alert the user and make changes (such as de-allocate the logger's
**              RAM for example).
**
****************************************************************************/
/* TBD */

/***************************************************************************
**
** NAME         UM_DBG_LOGGER_INIT_FAILD_RES
**
** PARAMETERS   None
**
** DESCRIPTION  This message is sent by the Host to confirm receiving of
**              the IND message.
**
****************************************************************************/

/* UM_DBG_LOGGER_INIT_FAILD_RES */
/* No implementation needed yet */

/***************************************************************************
**
** NAME         UM_DBG_LOGGER_FLUSH_BUF_REQ
**
** PARAMETERS   targetCPU  - The CPU (LM, UM, single CPU, etc.) whose logAgent's
**                           buffer should be flushed
**
** DESCRIPTION  This message is sent from the Host to the MAC when a flush request
**              was made in the code. Upon receiving this message, the logAgent
**              will send its current buffer "outside" (unless buffer is empty).
**
****************************************************************************/
typedef struct _UmiLoggerMsgFlushBuffer_t
{ 
    uint32 /*UmiCpuId_e*/ targetCPU;
} __MTLK_PACKED UmiLoggerMsgFlushBuffer_t;

/***************************************************************************
**
** NAME         MC_DBG_LOGGER_FLUSH_BUF_CFM
**
** PARAMETERS   None
**
** DESCRIPTION  This message is sent from the MAC back to the Host, to confirm
**              that the logAgent has received the flush message and is 
**              starting to handle the event.
**
****************************************************************************/

/* MC_DBG_LOGGER_FLUSH_BUF_CFM */
/* No implementation needed yet*/

/***************************************************************************
**
** NAME         UM_DBG_LOGGER_SET_MODE_REQ
**
** PARAMETERS   modeReq   - either of these logAgent states (LogAgentState_e):
**							LOGGER_MODE_ACTIVE, LOGGER_MODE_INACTIVE, LOGGER_MODE_CYCLIC
**              targetCPU  - The target CPU (LM, UM, single CPU, etc.)
**                           whose logAgent's state should be changed
**
** DESCRIPTION  This message is sent from the Host to the MAC's logAgent to
**              switch between logAgent states. The user can only switch between
**              LOGGER_MODE_ACTIVE, LOGGER_STATE_INACTIVE and LOGGER_STATE_CYCLIC_MODE. If the
**              logAgent is already in the state being set in the message,
**              nothing happens.
**
****************************************************************************/
typedef struct _UmiLoggerMsgSetState_t
{
    uint32 /*LogAgentState_e*/ modeReq;
    uint32 /*UmiCpuId_e*/     targetCPU;
} __MTLK_PACKED UmiLoggerMsgSetMode_t;

/***************************************************************************
**
** NAME         MC_DBG_LOGGER_SET_MODE_CFM
**
** PARAMETERS   None
**
** DESCRIPTION  This message is sent from the MAC back to the Host, to confirm
**              that the logAgent state has been updated according to the new
**              state given in the request message.
**
****************************************************************************/

/* MC_DBG_LOGGER_SET_MODE_CFM */
/* No implementation needed yet*/

/***************************************************************************
**
** NAME         UM_DBG_LOGGER_SET_SEVERITY_REQ
**
** PARAMETERS   newLevel   - Value of the new severity level
**              targetCPU  - The target CPU (LM, UM, single CPU, etc.) whose
**                           logAgent's severity level is to be set
**
** DESCRIPTION  This message is sent from the Host to the MAC's logAgent to
**              set the value of severity level filter.
**
****************************************************************************/
typedef struct _UmiLoggerMsgSetSeverity_t
{
    uint32 /*LogAgentSeverityLevel_t*/	newLevel;
    uint32 /*UmiCpuId_e*/              targetCPU;
} __MTLK_PACKED UmiLoggerMsgSetSeverity_t;

/***************************************************************************
**
** NAME         MC_DBG_LOGGER_SET_SEVERITY_CFM
**
** PARAMETERS   None
**
** DESCRIPTION  This message is sent from the MAC back to the Host, to confirm
**              that the severity level has been updated according to the new
**              level given in the request message.
**
****************************************************************************/

/* MC_DBG_LOGGER_SET_SEVERITY_CFM */
/*  No implementation needed yet  */

/***************************************************************************
**
** NAME         UM_DBG_LOGGER_SET_FILTER_REQ
**
** PARAMETERS   gidFilterMask   - Value of the new severity level
**              targetCPU       - The target CPU (LM, UM, single CPU, etc.)
**                                whose logAgent's filter should be set
**
** DESCRIPTION  This message is sent from the Host to the MAC's logAgent to set
**              the GID filters map / mask. The user can turn on (GID bit = 1),
**              or off (GID bit = 0) any GID, either from the web UI, or from a
**              command line interface.
**
****************************************************************************/
typedef struct _UmiLoggerMsgSetFilter_t
{
    LogAgentLoggerGroupsBitMap_t gidFilterMask;
    uint32 /*UmiCpuId_e*/                   targetCPU;
} __MTLK_PACKED UmiLoggerMsgSetFilter_t;

/***************************************************************************
**
** NAME         MC_DBG_LOGGER_SET_FILTER_CFM
**
** PARAMETERS   None
**
** DESCRIPTION  This message is sent from the MAC back to the Host, to confirm
**              that the filter map has been updated according to the filter
**              mask given in the request message.
**
****************************************************************************/

/* MC_DBG_LOGGER_SET_FILTER_CFM */
/* No implementation needed yet */

/***************************************************************************
**
** NAME         UMI_MEM
**
** DESCRIPTION  A union of all Memory Messages.
**
****************************************************************************/
typedef union _UMI_MEM
{
    UMI_COPY_FROM_MAC sCopyFromMAC;
    UMI_COPY_TO_MAC   sCopyToMAC;
} __MTLK_PACKED UMI_MEM;

/***                      Public Function Prototypes                     ***/
/***************************************************************************/

/*
 * Message between the MC and UM have a header.  The MC only needs the position
 * of the type field within the message and the length of the header.  All other
 * elements of the header are unused in the LM
*/

typedef struct _UMI_MSG
{
    mtlk_umimsg_ptr psNext;       /* Used to link list structures */
    uint8  u8Pad1;
    uint8  u8Persistent;
    uint16 u16MsgId;
    uint32 u32Pad2;                 /* For MIPS 8 bytes alignment */
    uint32 u32MessageRef;           /* Address in Host for Message body copy by DMA */
    uint8  abData[1];
} __MTLK_PACKED UMI_MSG;

typedef struct _UMI_MSG_HEADER
{
    mtlk_umimsg_ptr psNext;       /* Used to link list structures */
    uint8  u8Pad1;
    uint8  u8Persistent;
    uint16 u16MsgId;
    uint32 u32Pad2;                 /* For MIPS 8 bytes alignment */
    uint32 u32MessageRef;           /* Address in Host for Message body copy by DMA */
} __MTLK_PACKED UMI_MSG_HEADER;

/* REVISIT - was in shram.h - maybe should be in a him .h file but here is better for now */
/* linked UMI_DATA, MSDU, Host memory */
typedef struct _UMI_DATA_RX_STORAGE_ELEMENT
{
    UMI_MSG_HEADER    sMsgHeader;
    UMI_DATA_RX       sDATA;

} __MTLK_PACKED UMI_DATA_RX_STORAGE_ELEMENT;


typedef struct _UMI_DATA_TX_STORAGE_ELEMENT
{
    UMI_MSG_HEADER    sMsgHeader;
    UMI_DATA_TX       sDATA;

} __MTLK_PACKED UMI_DATA_TX_STORAGE_ELEMENT;

/***************************************************************************/

/* Memory messages between MAC and host - REVISIT - are none so why does this struct exist! */
typedef struct _SHRAM_MEM_MSG
{
    UMI_MSG_HEADER sHdr;                 /* Kernel Message Header */
    UMI_MEM        sMsg;                 /* UMI Memory Message */
} __MTLK_PACKED SHRAM_MEM_MSG;

/* Data transfer messages between MAC and Host */
typedef struct _SHRAM_DAT_REQ_MSG
{
    UMI_MSG_HEADER sHdr;                 /* Kernel Message Header */
    UMI_DATA_TX    sMsg;                 /* UMI Data Message */
} __MTLK_PACKED SHRAM_DAT_REQ_MSG;

/*Channel number for Fast Reboot - fast calibration  E.B */
typedef struct _UMI_CHNUM_FR
{
    uint16    u16CHNumber;
    uint8     calibrationAlgoMask;
    uint8     u8NumOfRxChains;
    uint8     u8NumOfTxChains;
    uint8     Reserved[3];
} __MTLK_PACKED UMI_CHNUM_FR;

/***************************************************************************
**
** NAME
**
** DESCRIPTION     Trace buffer Protocol Struct
**
****************************************************************************/


/*
*  HwTraceBuffer: The Trace Buffer Object
*/
#define   MTLK_PACK_OFF
#include "mtlkpack.h"

//LBF structures & defines

//Rank1/Rank2 rates mask
#define RANK_TWO_NUMBER_OF_RATES	9
#define RANK_TWO_SHIFT				23
#define RANK_TWO_RATES_MASK			MASK(RANK_TWO_NUMBER_OF_RATES, RANK_TWO_SHIFT, uint32)
#define RANK_ONE_RATES_MASK			~RANK_TWO_RATES_MASK

#endif /* !__MHI_UMI_INCLUDED_H */

