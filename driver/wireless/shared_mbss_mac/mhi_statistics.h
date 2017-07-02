/****************************************************************************
****************************************************************************
**
** COMPONENT:      ENET Upper MAC SW
**
** MODULE:         $File: //bwp/enet/demo153_sw/develop/src/shared/mhi_statistics.h $
**
** VERSION:        $Revision: #2 $
**
** DATED:          $Date: 2004/03/22 $
**
** AUTHOR:         S Sondergaard
**
** DESCRIPTION:    Statistics header
**
**
** LAST MODIFIED BY:   $Author: prh $
**
**
****************************************************************************
*
*   Copyright (c)TTPCom Limited, 2001
*
*   Copyright (c) Metalink Ltd., 2006 - 2007
*
****************************************************************************/

#ifndef __MHI_STATISTICS_INC
#define __MHI_STATISTICS_INC


#define   MTLK_PACK_ON
#include "mtlkpack.h"

#include "mtidl_c.h" // external from the driver trunk. defines the MTIDL.


/* Reason for network connection/disconnection */

/*
 * IMPORTANT NOTE: the following enum definitions will not work with c100 environment (since the
 * scripts recognize constant values by the prefix #define).
 * => For testing there is a need to replace all the enum's with defines.
 */

/* D.S  when using C100 on uart*/

MTIDL_ENUM_START
  MTIDL_ENUM_ENTRY(UMI_BSS_NEW_NETWORK,            0,  "BSS Created")
  MTIDL_ENUM_ENTRY(UMI_BSS_JOINED,                 1,  "BSS Joined")
  MTIDL_ENUM_ENTRY(UMI_BSS_DEAUTHENTICATED,        2,  "Peer deauthenticated")
  MTIDL_ENUM_ENTRY(UMI_BSS_DISASSOCIATED,          3,  "Peer disassociated")
  MTIDL_ENUM_ENTRY(UMI_BSS_JOIN_FAILED,            4,  "Join failed")
  MTIDL_ENUM_ENTRY(UMI_BSS_AUTH_FAILED,            5,  "Authentication failed")
  MTIDL_ENUM_ENTRY(UMI_BSS_ASSOC_FAILED,           6,  "Association failed")
MTIDL_ENUM_END(mtlk_wssa_peer_removal_reasons_t)

MTIDL_ENUM_START
  MTIDL_ENUM_ENTRY(UMI_BSS_AUTH_OPEN,       15, "Open")
  MTIDL_ENUM_ENTRY(UMI_BSS_AUTH_SHARED_KEY, 16, "Shared key")
MTIDL_ENUM_END(mtlk_wssa_authentication_type_t)

/* Status codes */
MTIDL_ENUM_START
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_SUCCESSFUL,                          0,  "Successful")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_FAILURE,                             1,  "Failure")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_RESERVED,                            4,  "Reserved")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_CANT_SUPPORT_CAPABILITIES,          10,  "Can't support capabilities")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_CANNOT_CONFIRM_ASSOCIATION,         11,  "Can't confirm association")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_NON_802_11_REASON,                  12,  "Non 802.11 reason")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_ALGORITHM_NOT_SUPPORTED,            13,  "Algorithm not supported")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_AUTH_SEQ_UNEXPECTED,                14,  "Authentication sequence unexpected")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_CHALLENGE_FAILURE,                  15,  "Challenge failure")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_AUTH_TIMEOUT,                       16,  "Authentication timeout")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_STATION_OVERLOAD,                   17,  "Station overloaded")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_BASIC_RATES_UNSUPPORTED,            18,  "Basic rates unsupported")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_ASSOC_DENIED_REQ_SHORT_PREAMBLE,    19,  "Denied due to short preamble")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_ASSOC_DENIED_REQ_PBCC,              20,  "Denied due to PBCC")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_ASSOC_DENIED_REQ_CHANNEL_AGILITY,   21,  "Denied due to channel agility")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_ASSOC_DENIED_HT_NOT_SUPPORTED,      27,  "Denied due to not supporting HT features")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_ASSOC_DENIED_QOS_RELATED_REASON,    32,  "Denied due to QoS")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_STA_LEAVING_BSS,                    36,  "STA is leaving")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_INVALID_RSN_IE_CAPABILITIES,        45,  "Invalid RSN IE capabilities")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_BEACON_TIMEOUT,                     101,  "STA has lost contact with network")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_ROAMING,                            102,  "STA is roaming to another BSS")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_MANUAL_DISCONNECT,                  103,  "UMI has forced a disconnect")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_NO_NETWORK,                         104,  "There is no network to join")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_IBSS_COALESCE,                      105,  "IBSS is coalescing with another one")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_11H,                                106,  "Radar detected on current channel")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_AGED_OUT,                           107,  "Station timed out")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_ASSOCIATED,                         108,  "Station associated successfully")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_INACTIVITY,                         109,  "Peer data timeout")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_USER_REQUEST,                       110,  "User request")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_PEER_PARAMS_CHANGED,                111,  "Peer reconfigured and new parameters not supported")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_MAC_ADDRESS_FILTER,                 112,  "STA's MAC address is not allowed in this BSS")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_W101_ASSOCIATED,                    113,  "W101 Station associated successfully")
  MTIDL_ENUM_ENTRY(FM_STATUSCODE_STA_ALREADY_AUTHENTICATED,          114,  "STA tried to authenticate while already authenticated")
  MTIDL_ENUM_END(mtlk_wssa_connection_status_t)


typedef uint16 FM_STATUS_CODE;


MTIDL_ENUM_START
	MTIDL_ENUM_ENTRY(VENDOR_UNKNOWN,		0,	 "Unknown")
	MTIDL_ENUM_ENTRY(VENDOR_LANTIQ,			1,	 "Lantiq")
	MTIDL_ENUM_ENTRY(VENDOR_W101,			2,	 "W101")
MTIDL_ENUM_END(mtlk_wssa_peer_vendor_t)


/* WARNING: This list must be synchronized with MTIDL_GeneralStats structure */
/* Preserve synchronisation while making changes                             */

#define STAT_RX_UNICAST_DATA                0   /* Unicats data frames received */
#define STAT_RX_DUPLICATE                   1   /* Duplicate frames received */
#define STAT_RX_MULTICAST_DATA              2   /* Number of multicast frames received */
#define STAT_RX_DISCARD                     3   /* Frames Discarded */
#define STAT_RX_UNKNOWN                     4   /* Unknown Rx */
#define STAT_RX_UNAUTH                      5   /* Reception From Unauthenticated STA */
#define STAT_RX_UNASSOC                     6   /* AP: Frames Rx from Unassociated STA */
#define STAT_RX_INTS                        7   /* Rx Interrupts */
#define STAT_RX_CONTROL                     8   /* RX Control type frames */

#define STAT_BEACON_TX                      9   /* Beacons Sent */
#define STAT_BEACON_RX                      10  /* Beacons Received */
#define STAT_AUTHENTICATE_TX                11  /* Authentication Requests Sent */
#define STAT_AUTHENTICATE_RX                12  /* Authentication Requests Received */

#define STAT_RESERVED                       13  /* Reserved to avoid holes in numeration */

#define STAT_ASSOC_REQ_TX                   14  /* Association Requests Sent */
#define STAT_ASSOC_REQ_RX                   15  /* Association Requests Received */
#define STAT_ASSOC_RES_TX                   16  /* Association Replies Sent */
#define STAT_ASSOC_RES_RX                   17  /* Association Replies Received */

#define STAT_REASSOC_REQ_TX                 18  /* ReAssociation Requests Sent */
#define STAT_REASSOC_REQ_RX                 19  /* ReAssociation Requests Received */
#define STAT_REASSOC_RES_TX                 20  /* ReAssociation Replies Sent */
#define STAT_REASSOC_RES_RX                 21  /* ReAssociation Replies Received */

#define STAT_DEAUTH_TX                      22  /* Deauthentication Notifications Sent */
#define STAT_DEAUTH_RX                      23  /* Deauthentication Notifications Received */

#define STAT_DISASSOC_TX                    24  /* Disassociation Notifications Sent */
#define STAT_DISASSOC_RX                    25  /* Disassociation Notifications Received */

#define STAT_PROBE_REQ_TX                   26  /* Probe Requests sent */
#define STAT_PROBE_REQ_RX                   27  /* Probe Requests received */
#define STAT_PROBE_RES_TX                   28  /* Probe Responses sent */
#define STAT_PROBE_RES_RX                   29  /* Probe Responses received */

#define STAT_ATIM_TX                        30  /* ATIMs Transmitted successfully */
#define STAT_ATIM_RX                        31  /* ATIMs received */
#define STAT_ATIM_TX_FAIL                   32  /* ATIMs Failing transmission */

#define STAT_TX_MSDU                        33  /* TX msdus that have been sent */

#define STAT_TX_FAIL                        34  /* TX frames that have failed */
#define STAT_TX_RETRY                       35  /* TX retries to date */
#define STAT_TX_DEFER_PS                    36  /* Transmits deferred due to Power Mgmnt */
#define STAT_TX_DEFER_UNAUTH                37  /* Transmit deferred pending authentication */

#define STAT_BEACON_TIMEOUT                 38  /* Authentication Timeouts */
#define STAT_AUTH_TIMEOUT                   39  /* Authentication Timeouts */
#define STAT_ASSOC_TIMEOUT                  40  /* Association Timeouts */
#define STAT_ROAM_SCAN_TIMEOUT              41  /* Roam Scan timeout */

#define STAT_WEP_TOTAL_PACKETS              42  /* total number of packets passed through WEP */
#define STAT_WEP_EXCLUDED                   43  /* unencrypted packets received when WEP is active */
#define STAT_WEP_UNDECRYPTABLE              44  /* packets with no valid keys for decryption */
#define STAT_WEP_ICV_ERROR                  45  /* packets with incorrect WEP ICV */
#define STAT_TX_PS_POLL                     46  /* TX PS POLL */
#define STAT_RX_PS_POLL                     47  /* RX PS POLL */

#define STAT_MAN_ACTION_TX                  48  /* Management Actions sent */
#define STAT_MAN_ACTION_RX                  49  /* Management Actions received */

#define STAT_OUT_OF_RX_MSDUS                50  /* Management Actions received */

#define STAT_HOST_TX_REQ                    51  /* Requests from PC to Tx data - UM_DAT_TXDATA_REQ */
#define STAT_HOST_TX_CFM                    52  /* Confirm to PC by MAC of Tx data - MC_DAT_TXDATA_CFM */
#define STAT_BSS_DISCONNECT                 53  /* Station remove from database due to beacon/data timeout */

#define STAT_RX_DUPLICATE_WITH_RETRY_BIT_0  54  /* Duplicate frames received with retry bit set to 0 */
#define STAT_RX_NULL_DATA                   55  /* total number of received NULL DATA packets */
#define STAT_TX_NULL_DATA                   56  /* total number of sent NULL DATA packets */
#define STAT_TX_BAR                         57  /* <E.Z> - BAR Request sent */
#define STAT_RX_BAR                         58  /* <E.Z> */
#define STAT_TX_TOTAL_MANAGMENT_PACKETS     59  /*Total managment packet transmitted)*/
#define STAT_RX_TOTAL_MANAGMENT_PACKETS     60  /*Total Total managment packet recieved*/
#define STAT_RX_TOTAL_DATA_PACKETS          61
#define STAT_RX_FAIL_NO_DECRYPTION_KEY      62  /* RX Failures due to no key loaded (needed by Windows) */
#define STAT_RX_DECRYPTION_SUCCESSFUL       63  /* RX decryption successful (needed by Windows) */
#define STAT_TX_BAR_FAIL                    64
//PS_STATS
#define STAT_NUM_UNI_PS_INACTIVE            65  /* Number of unicast packets in PS-Inactive queue */
#define STAT_NUM_MULTI_PS_INACTIVE          66  /* Number of multicast packets in PS-Inactive queue */
#define STAT_TOT_PKS_PS_INACTIVE            67  /* total number of packets in PS-Inactive queue */
#define STAT_NUM_MULTI_PS_ACTIVE            68  /* Number of multicast packets in PS-Active queue */
#define STAT_NUM_TIME_IN_PS                 69  /* Number of STAs in power-save */
//WDS_STATS
#define STAT_WDS_TX_UNICAST                 70  /* Number of unicast WDS frames transmitted */
#define STAT_WDS_TX_MULTICAST               71  /* Number of multicast WDS frames transmitted */
#define STAT_WDS_RX_UNICAST                 72  /* Number of unicast WDS frames received */
#define STAT_WDS_RX_MULTICAST               73  /* Number of multicast WDS frames received */

#define STAT_CTS2SELF_TX                    74  /* CTS2SELF packets that have been sent */
#define STAT_CTS2SELF_TX_FAIL               75  /* CTS2SELF packets that have failed */

#define STAT_DECRYPTION_FAILED              76  /* Number of frames with decryption failed */
#define STAT_FRAGMENT_FAILED                77  /* Number of frames with wrong fragment number */
#define STAT_TX_MAX_RETRY                   78  /* Number of TX dropped packets with retry limit exceeded */

#define STAT_TX_RTS_SUCCESS					79
#define STAT_TX_RTS_FAIL					80

#define STAT_TX_MULTICAST_DATA				81

#define STAT_FCS_ERROR						82

#define STAT_RX_ADDBA_REQ					83
#define STAT_RX_ADDBA_RES					84
#define STAT_RX_DELBA_PKT					85

#define STAT_RX_MULTICAST_SOURCE			86

#define STAT_RX_MIC_FAILURE      			87 /* Number of MIC failures */

#define STAT_RX_CYCBUF_OVERFLOW				88 /* Number of Cyclic Buffer overflows */

#define STAT_AGGR_POSTPONE_PS				89 /* AGGR Postponed due to PS */

#define STAT_FSDU_CANCELLED					90 /* Number of cancelled FSDUs*/

#define STAT_CANT_ADD_ADDRESS_Q			    91

#define STAT_DUPLICATE_MNGT					92
#define STAT_DUPLICATE_MCAST				93

#define STAT_TX_RTS_MAX_RETRY				94
#define STAT_TX_LBF_PROBE					95

#define STAT_TOTAL_NUMBER                   96  /* Size of stats array  */

#define GET_ALL_STATS						0xFFFF  /* used by host to request the entire statistics array  */

#define Managment_Min_Index                 9
#define Managment_Max_Index                 31

/*************************************************************************************************/
/************************ Aggregation Counters ***************************************************/
/*************************************************************************************************/

#define STAT_TX_AGG_PKT                      0
#define STAT_RETRY_PKT                       1
#define STAT_DISCARD_PKT                     2
#define STAT_TX_AGG                          3
#define STAT_CLOSE_COND_SHORT_PKT            4  /* Agg closed due to minimum sized packet */
#define STAT_CLOSE_COND_MAX_PKTS             5  /* Agg closed due to max amount of pkts */
#define STAT_CLOSE_COND_MAX_BYTES            6  /* Agg closed due to max amount of bytes */
#define STAT_CLOSE_COND_TIMEOUT              7  /* Agg closed due to timeout interrupt */
#define STAT_CLOSE_COND_OUT_OF_WINDOW        8  /* Agg closed due to packet out of window  */
#define STAT_CLOSE_COND_MAX_MEM_USAGE        9  /* Agg closed due to max usage of agrregation memory (no more space for any additional packets) */
#define STAT_RECIEVED_BA                    10
#define STAT_NACK_EVENT                     11
#define STAT_SUB_FRAME_ATTACHED             12  /* Number of sub frames within the aggregate that we still atatched to  fsdu's (txaggrcounter) */
#define STAT_SUB_FRAME_CFM                  13  /* Number of sub frames within the aggregate already confirmed (R5). */
#define STAT_TX_CLOSED_PKT                  14
#define STAT_AGGR_STATE                     15
#define STAT_AGGR_LAST_CLOSED_REASON        16
#define STAT_BA_PROCESSED                   17  /* <O.H> - number of processed block acks */
#define STAT_BA_CORRUPTED                   18  /* Number of corrupted block ack frames */
#define STAT_BAR_TRANSMIT                   19  /* <E.Z> */
#define STAT_NEW_AGG_NOT_ALLOWED			20
#define STAT_CLOSE_COND_SOUNDING_PACKET		21
#define STAT_AGGR_TOTAL_NUMBER              22  /* Size of stats array - this define is always last  */

/*********************************************************************************************/
/******************** Debug Counters *********************************************************/
/*********************************************************************************************/

#define STAT_TX_OUT_OF_TX_MSDUS             0   /* No free Tx MSDUs for Host Tx request */
#define STAT_TX_LM_Q                        1   /* In vTXST_PutTxFsdu - Place data on LM Tx Q */
#define STAT_TX_FREE_FSDU_0                 2   /* vTXST_ReturnFsdu - UM frees data FSDU on priority 0 */
#define STAT_TX_FREE_FSDU_1                 3   /* vTXST_ReturnFsdu - UM frees data FSDU on priority 1 */
#define STAT_TX_FREE_FSDU_2                 4   /* vTXST_ReturnFsdu - UM frees data FSDU on priority 2 */
#define STAT_TX_FREE_FSDU_3                 5   /* vTXST_ReturnFsdu - UM frees data FSDU on priority 3 */
#define STAT_TX_PROTOCOL_CFM                6   /* vTXST_FsduCallback - Packet returned from LM confirm to Protocol task */
#define STAT_TX_DATA_REQ                    7   /* Call vTXP_SendDataPkt from vTx_dataReq - each req from Host */
#define STAT_TX_RX_SEND_PACKET              8   /* Call vTXP_SendDataPkt for Rx data - forward data */
#define STAT_TX_SEND_PACKET_AA              9   /* Confirm Tx on requests from Host */
#define STAT_TX_FSDU_UNKNOWN_STATE          10  /* FSDU is received in an unknown state - not confirmed to Host */
#define STAT_TX_FSDU_CALLBACK               11  /* Free FSDU from Host Tx Req */
#define STAT_TX_R5                          12  /* R5 indication from LM */
#define STAT_LM_TX_FSDU_CFM                 13  /* vLMIfsduConfirmViaKnl - Transmit cfm from LM (not aggregated) */
#define STAT_TX_LEGACY_PACKETS              14  /* vTXP_SendDataPkt - Total leagacy packets tx */
#define STAT_TX_AGGR_PACKETS                15  /* vTXP_SendDataPkt - Total aggr packets tx */
#define STAT_TX_UNICAST_PACKETS_FROM_HOST   16  /* vTx_dataReq - Totla unicast packets recieved from host */
#define STAT_TX_UNICAST_PACKETS_CFM         17  /* vTXQU_Callback - Total transmitted unicast packets */
#define STAT_TX_MULTICAST_PACKETS_FROM_HOST 18  /* vTx_dataReq - Totla multicast packets recieved from host */
#define STAT_TX_MULTICAST_PACKETS_CFM       19  /* vTXQU_Callback - Total transmitted multicast packets */
#define STAT_AGGR_TX_PKT_TO_LM_Q            20
#define STAT_BA_ERR                         21

#define DEBUG_STAT_TOTAL_NUMBER             22  /* Size of stats array  */



/*********************************************************************************************/
/******************** MTIDL - Metalink Information Definition Language ***********************/
/********************       Debug statistics, delivered to driver      ***********************/
/*********************************************************************************************/

/* Definitions required for the MTIDL*/ 

	/* IDs list */
	MTIDL_ID_LIST_START
		MTIDL_ID(DBG_STATS_RX_DISCARD_REASONS,	0)
		MTIDL_ID(DBG_STATS_TX_DISCARD_REASONS,	1)
		MTIDL_ID(DBG_STATS_STATUS,				2)
		MTIDL_ID(DBG_STATS_SECURITY,			3)
		MTIDL_ID(DBG_STATS_AGGREGATIONS,		4)
		MTIDL_ID(DBG_STATS_NUM_OF_STATS,		5)
	MTIDL_ID_LIST_END

MTIDL_ID_LIST_START
		MTIDL_ID(DBG_STATS_FWSTATUS,		1000)
MTIDL_ID_LIST_END

	
	/* Types list */
	MTIDL_TYPE_LIST_START
		MTIDL_TYPE(MTIDL_INFORMATION, 1)
		MTIDL_TYPE(MTIDL_STATISTICS,  2)
		MTIDL_TYPE(MTIDL_EVENT,       3)
	MTIDL_TYPE_LIST_END

	/* Levels list */
	MTIDL_LEVEL_LIST_START 
		MTIDL_LEVEL(MTIDL_PROVIDER_HW,    1)
		MTIDL_LEVEL(MTIDL_PROVIDER_WLAN,  2)
		MTIDL_LEVEL(MTIDL_PROVIDER_PEER,  3)
	MTIDL_LEVEL_LIST_END   

	/* Sources list */
	MTIDL_SOURCE_LIST_START
		MTIDL_SOURCE(MTIDL_SRC_FW,  1)
		MTIDL_SOURCE(MTIDL_SRC_DRV, 2)
	MTIDL_SOURCE_LIST_END


/* Statistics structs using MTIDL*/		

	MTIDL_ITEM_START(RxDiscardedFw, MTIDL_STATISTICS, MTIDL_PROVIDER_HW, MTIDL_SRC_FW, DBG_STATS_RX_DISCARD_REASONS, "Firmware RX packets discard statistics")
		MTIDL_LONGVAL( MT_STAT_MPDU_BSS_NOT_ACTIVE, "BSS is not active")
		MTIDL_LONGVAL( MT_STAT_MPDU_SELF, "Packets from self")
		MTIDL_LONGVAL( MT_STAT_MPDU_BSSID, "Mulitcast frames from other BSS's")
		MTIDL_LONGVAL( MT_STAT_MPDU_UNAUTH, "Packet from not authenticated sender")
		MTIDL_LONGVAL( MT_STAT_MPDU_UNASSOC, "Packet from not associated sender")
		MTIDL_LONGVAL( MT_STAT_MPDU_NO_DATA, "Packet with no data")
		MTIDL_LONGVAL( MT_STAT_MPDU_NO_TO_DS, "Data frames received without 'ToDS'")
		MTIDL_LONGVAL( MT_STAT_MPDU_NO_FROM_DS, "Data frames received without 'FromDS'")
		MTIDL_LONGVAL( MT_STAT_MPDU_ENC_MISMATCH, "Packets with wrong encryption type")
		MTIDL_LONGVAL( MT_STAT_MPDU_UNENCRYPTED, "Packets without encryption when its required ")
		MTIDL_LONGVAL( MT_STAT_MPDU_8021X_FILTER, "Packets didnt pass 802.1X filtering")
		MTIDL_LONGVAL( MT_STAT_MPDU_INVALID_TYPE, "Management packets with unknown or unwanted type")
		MTIDL_LONGVAL( MT_STAT_MPDU_IGNORED_TYPE, "Packets with ignored type")
		MTIDL_LONGVAL( MT_STAT_MPDU_DUPLICATE, "Duplicated packets")
		MTIDL_LONGVAL( MT_STAT_MPDU_DUP_NO_RETRY, "Duplicated packets with retry bit not set")
		MTIDL_LONGVAL( MT_STAT_MPDU_SID_ERROR, "Packets from station which couldn't be found in database")
		MTIDL_LONGVAL( MT_STAT_MPDU_WRONG_FRAG_NUM, "Packets with wrong fragement number")
		MTIDL_LONGVAL( MT_STAT_MPDU_DECRYPION_FAILED, "Packets which failed decryption")
		MTIDL_LONGVAL( MT_STAT_MPDU_UN_DECRYPTED_PACKET, "Packets which were not supposed to be encrypted")
		MTIDL_LONGVAL( MT_STAT_RX_IDLE_STATE, "Packets received while RX was idle")
		MTIDL_LONGVAL( MT_STAT_RX_MULTICAST_SOURCE, "Packets with a multicast transmitter address")
	MTIDL_ITEM_END(MTIDL_RxDiscardReason)


	MTIDL_ITEM_START(TxDiscardedFw, MTIDL_STATISTICS, MTIDL_PROVIDER_HW, MTIDL_SRC_FW, DBG_STATS_TX_DISCARD_REASONS, "Firmware TX packets discard statistics")
		MTIDL_LONGVAL(MT_STAT_MSDU_BAD_LENGTH, "MSDUs too large to fragment and transmit")
		MTIDL_LONGVAL(MT_STAT_OUT_OF_MSDU, "Out of MSDUs")
		MTIDL_LONGVAL(MT_STAT_STA_NOT_CONNECTED, "Station is not connected")
		MTIDL_LONGVAL(MT_STAT_MSDU_TIME_OUT, "Aged packets due to timeout")
		MTIDL_LONGVAL(MT_STAT_MSDU_RETRY_LIMIT, "Reached maximum retry limit")
		MTIDL_LONGVAL(MT_STAT_AGGR_Q_FULL, "Aggregation queue was full")
		MTIDL_LONGVAL(MT_STAT_LM_TX_FAILURE, "Transmission failed in lower MAC")
		MTIDL_LONGVAL(MT_STAT_MNG_OUT_OF_MSDU, "Out of MNG MSDU")
		MTIDL_LONGVAL(MT_STAT_MNG_UNKNOWN_VAP, "MNG dropped due to unknown VAP")
		MTIDL_LONGVAL(MT_STAT_DISCARD_LEGACY, "How many times legacy packet is discarded at data request")
		MTIDL_LONGVAL(MT_STAT_LM_UNKNOWN_FAILURE, "Unknown Tx failure received from LM")
	MTIDL_ITEM_END(MTIDL_TxDiscardReason)

	MTIDL_ITEM_START(FWDebugStats, MTIDL_STATISTICS, MTIDL_PROVIDER_HW, MTIDL_SRC_FW, DBG_STATS_STATUS, "Firmware debug statistics")
		MTIDL_LONGVAL(MT_STAT_STA_CONNECTED, "Total number of stations connected")
		MTIDL_LONGVAL(MT_STAT_STA_DISCONNECT_AGED_OUT, "STA was disconnected since it did not responded more than reasonable time")
		MTIDL_LONGVAL(MT_STAT_USED_TX_MSDUS_PRI_0, "Number of TX MSDUs used on priority 0 (BE)")
		MTIDL_LONGVAL(MT_STAT_USED_TX_MSDUS_PRI_1, "Number of TX MSDUs used on priority 1 (BK)")
		MTIDL_LONGVAL(MT_STAT_USED_TX_MSDUS_PRI_2, "Number of TX MSDUs used on priority 2 (VI)")
		MTIDL_LONGVAL(MT_STAT_USED_TX_MSDUS_PRI_3, "Number of TX MSDUs used on priority 3 (VO)")
		MTIDL_LONGVAL(MT_STAT_MAX_TX_MSDUS_PRI_0, "Max Number of TX MSDUs used on priority 0 (BE)")
		MTIDL_LONGVAL(MT_STAT_MAX_TX_MSDUS_PRI_1, "Max Number of TX MSDUs used on priority 1 (BK)")
		MTIDL_LONGVAL(MT_STAT_MAX_TX_MSDUS_PRI_2, "Max Number of TX MSDUs used on priority 2 (VI)")
		MTIDL_LONGVAL(MT_STAT_MAX_TX_MSDUS_PRI_3, "Max Number of TX MSDUs used on priority 3 (VO)")
		MTIDL_LONGVAL(MT_STAT_LIMIT_TX_MSDUS, "Number of times maximum TX MSDUs usage was reached")
		MTIDL_LONGVAL(MT_STAT_USED_RX_MSDUS, "Number of times maximum RX MSDUs usage was reached") // seems like its not used.
		MTIDL_LONGVAL(MT_STAT_USED_FSDU_PRI_0, "Number of TX FSDUs used on priority 0 (BE)")
		MTIDL_LONGVAL(MT_STAT_USED_FSDU_PRI_1, "Number of TX FSDUs used on priority 1 (BK)")
		MTIDL_LONGVAL(MT_STAT_USED_FSDU_PRI_2, "Number of TX FSDUs used on priority 2 (VI)")
		MTIDL_LONGVAL(MT_STAT_USED_FSDU_PRI_3, "Number of TX FSDUs used on priority 3 (VO)")
		MTIDL_LONGVAL(MT_STAT_LAST_TX_MPDU_TYPE, "Last TX packet type")
		MTIDL_LONGVAL(MT_STAT_LAST_RX_MPDU_TYPE, "Last RX packet type")
		MTIDL_LONGVAL(MT_STAT_NUM_AGGR_OBJECTS, "Number of currently allocated aggregation objects")
		MTIDL_LONGVAL(MT_STAT_LAST_AGGR_OBJECT_TX_RATE, "TX rate of last aggregation object")
		MTIDL_LONGVAL(MT_STAT_USER_DEFINED_UM_STAT1, "User defined stat1")
		MTIDL_LONGVAL(MT_STAT_USER_DEFINED_UM_STAT2, "User defined stat2")
		MTIDL_LONGVAL(MT_STAT_USER_DEFINED_UM_STAT3, "User defined stat3")
		MTIDL_LONGVAL(MT_STAT_MAX_CONNECTION_OVERFLOW, "Number of times station couldn't connect since VAP is full")
		MTIDL_LONGVAL(MT_STAT_STA_FILTERED, "Number of stations which didn't pass ACL filtering during authentication")
		MTIDL_LONGVAL(MT_STAT_RX_INVALID_BA, "Number of invalid BlockAcks received")
		MTIDL_LONGVAL(MT_STAT_TX_OUT_OF_FSDU_PRI_0, "Number of Out of TX FSDus on priority 0")
		MTIDL_LONGVAL(MT_STAT_TX_OUT_OF_FSDU_PRI_1, "Number of Out of TX FSDus on priority 1")
		MTIDL_LONGVAL(MT_STAT_TX_OUT_OF_FSDU_PRI_2, "Number of Out of TX FSDus on priority 2")
		MTIDL_LONGVAL(MT_STAT_TX_OUT_OF_FSDU_PRI_3, "Number of Out of TX FSDus on priority 3")
		MTIDL_LONGVAL(MT_STAT_TX_HOLD_DUE_TO_RX_PENDING, "Number of times TX is delayed due to RX pending")
	MTIDL_ITEM_END(MTIDL_StatusStats)

	MTIDL_ITEM_START(HWEncrypterStats, MTIDL_STATISTICS, MTIDL_PROVIDER_HW, MTIDL_SRC_FW, DBG_STATS_SECURITY, "HW encrypter statistics")
		MTIDL_LONGVAL(MT_STAT_SECURITY_GROUP_CIPHER_TYPE, "Last used cipher suite type")
		MTIDL_LONGVAL(MT_STAT_SECURITY_ENCRYPTION, "Number of packets sent to encryption")
		MTIDL_LONGVAL(MT_STAT_SECURITY_DECRYPTION, "Number of packets sent to decryption")
		MTIDL_LONGVAL(MT_STAT_SECURITY_TX_PENDING, "Number of packets which were put on encryption pending queue")
		MTIDL_LONGVAL(MT_STAT_SECURITY_RX_PENDING, "Number of packets which were put on decryption pending queue")
		MTIDL_LONGVAL(MT_STAT_SECURITY_RX_1XPACKETS, "Number of unencrypted 802.1x packets received in WPA/2")
		MTIDL_LONGVAL(MT_STAT_SECURITY_ENCRYPT_SUCCESS, "Number of packets encrypted successfully")
		MTIDL_LONGVAL(MT_STAT_SECURITY_DECRYPT_SUCCESS, "Number of packets decrypted successfully")
		MTIDL_LONGVAL(MT_STAT_SECURITY_TX_FAIL_NO_KEY, "Number of encryption failures due to no key loaded")
		MTIDL_LONGVAL(MT_STAT_SECURITY_RX_FAIL_NO_KEY, "Number of decryption failures due to no key loaded")
	MTIDL_ITEM_END(MTIDL_SecurityStats)

	MTIDL_ITEM_START(AggregationsStats, MTIDL_STATISTICS, MTIDL_PROVIDER_HW, MTIDL_SRC_FW, DBG_STATS_AGGREGATIONS, "Aggregations statistics")
		MTIDL_LONGVAL(MT_STAT_TX_AGG_PKT, "Number of packets added to aggregation queue")
		MTIDL_LONGVAL(MT_STAT_RETRY_PKT, "Number of packets added to retry queue")
		MTIDL_LONGVAL(MT_STAT_DISCARD_PKT, "Number of discarded packets")
		MTIDL_LONGVAL(MT_STAT_TX_AGG, "Number of closed aggregations")
		MTIDL_LONGVAL(MT_STAT_CLOSE_COND_SHORT_PKT, "Aggregation closed due to minimum sized packet")
		MTIDL_LONGVAL(MT_STAT_CLOSE_COND_MAX_PKTS, "Aggregation closed due to max amount of packets")
		MTIDL_LONGVAL(MT_STAT_CLOSE_COND_MAX_BYTES, "Aggregation closed due to max amount of bytes")
		MTIDL_LONGVAL(MT_STAT_CLOSE_COND_TIMEOUT, "Aggregation closed due to timeout interrupt")
		MTIDL_LONGVAL(MT_STAT_CLOSE_COND_OUT_OF_WINDOW, "Aggregation closed due to packet out of window")
		MTIDL_LONGVAL(MT_STAT_CLOSE_COND_MAX_MEM_USAGE, "Aggregation closed due to max usage of agrregation memory")
		MTIDL_LONGVAL(MT_STAT_RECIEVED_BA, "Aggregation closed due to max usage of aggregation memory")
		MTIDL_LONGVAL(MT_STAT_NACK_EVENT, "Number of NACK events")
		MTIDL_LONGVAL(MT_STAT_SUB_FRAME_ATTACHED, "Number of subframes within aggregation that were attached to fsdus")
		MTIDL_LONGVAL(MT_STAT_SUB_FRAME_CFM, "Number of subframes within aggregation already confirmed (R5)")
		MTIDL_LONGVAL(MT_STAT_TX_CLOSED_PKT, "Number of packets closed")
		MTIDL_LONGVAL(MT_STAT_AGGR_STATE, "Current Aggregation State")
		MTIDL_LONGVAL(MT_STAT_AGGR_LAST_CLOSED_REASON, "Last Aggregation closing reason")
		MTIDL_LONGVAL(MT_STAT_BA_PROCESSED, "Number of processed block ACKs")
		MTIDL_LONGVAL(MT_STAT_BA_CORRUPTED, "Number of corrupted block ACK frames")
		MTIDL_LONGVAL(MT_STAT_BAR_TRANSMIT, "Number of transmitted BARs")
		MTIDL_LONGVAL(MT_STAT_NEW_AGG_NOT_ALLOWED, "Number of times new aggregations were not allowed")
		MTIDL_LONGVAL(MT_STAT_CLOSE_COND_SOUNDING_PACKET, "Aggregation closed due to sounding packet")
	MTIDL_ITEM_END(MTIDL_AggregationsStats)

  MTIDL_ITEM_START(FWFlowStatus, MTIDL_STATISTICS, MTIDL_PROVIDER_HW, MTIDL_SRC_FW, DBG_STATS_FWSTATUS, "Firmware packets flow statistics")
		MTIDL_LONGVAL(FW_STAT_RX_UNICAST_DATA,               "Unicast data frames received")
		MTIDL_LONGVAL(FW_STAT_RX_DUPLICATE,                  "Duplicate frames received")
		MTIDL_LONGVAL(FW_STAT_RX_MULTICAST_DATA,             "Number of multicast frames received")
		MTIDL_LONGVAL(FW_STAT_RX_DISCARD,                    "Frames Discarded")
		MTIDL_LONGVAL(FW_STAT_RX_UNKNOWN,                    "Unknown Rx")
		MTIDL_LONGVAL(FW_STAT_RX_UNAUTH,                     "Reception From Unauthenticated STA")
		MTIDL_LONGVAL(FW_STAT_RX_UNASSOC,                    "AP: Frames Rx from Unassociated STA")
		MTIDL_LONGVAL(FW_STAT_RX_INTS,                       "Rx Interrupts")
		MTIDL_LONGVAL(FW_STAT_RX_CONTROL,                    "RX Control type frames")
		MTIDL_LONGVAL(FW_STAT_BEACON_TX,                     "Beacons Sent")
		MTIDL_LONGVAL(FW_STAT_BEACON_RX,                     "Beacons Received")
		MTIDL_LONGVAL(FW_STAT_AUTHENTICATE_TX,               "Authentication Requests Sent")
		MTIDL_LONGVAL(FW_STAT_AUTHENTICATE_RX,               "Authentication Requests Received")
		MTIDL_LONGVAL(FW_STAT_RESERVED,                      "===reserved===")
		MTIDL_LONGVAL(FW_STAT_ASSOC_REQ_TX,                  "Association Requests Sent")
		MTIDL_LONGVAL(FW_STAT_ASSOC_REQ_RX,                  "Association Requests Received")
		MTIDL_LONGVAL(FW_STAT_ASSOC_RES_TX,                  "Association Replies Sent")
		MTIDL_LONGVAL(FW_STAT_ASSOC_RES_RX,                  "Association Replies Received")
		MTIDL_LONGVAL(FW_STAT_REASSOC_REQ_TX,                "ReAssociation Requests Sent")
		MTIDL_LONGVAL(FW_STAT_REASSOC_REQ_RX,                "ReAssociation Requests Received")
		MTIDL_LONGVAL(FW_STAT_REASSOC_RES_TX,                "ReAssociation Replies Sent")
		MTIDL_LONGVAL(FW_STAT_REASSOC_RES_RX,                "ReAssociation Replies Received")
		MTIDL_LONGVAL(FW_STAT_DEAUTH_TX,                     "Deauthentication Notifications Sent")
		MTIDL_LONGVAL(FW_STAT_DEAUTH_RX,                     "Deauthentication Notifications Received")
		MTIDL_LONGVAL(FW_STAT_DISASSOC_TX,                   "Disassociation Notifications Sent")
		MTIDL_LONGVAL(FW_STAT_DISASSOC_RX,                   "Disassociation Notifications Received")
		MTIDL_LONGVAL(FW_STAT_PROBE_REQ_TX,                  "Probe Requests sent")
		MTIDL_LONGVAL(FW_STAT_PROBE_REQ_RX,                  "Probe Requests received")
		MTIDL_LONGVAL(FW_STAT_PROBE_RES_TX,                  "Probe Responses sent")
		MTIDL_LONGVAL(FW_STAT_PROBE_RES_RX,                  "Probe Responses received")
		MTIDL_LONGVAL(FW_STAT_ATIM_TX,                       "ATIMs Transmitted successfully")
		MTIDL_LONGVAL(FW_STAT_ATIM_RX,                       "ATIMs received")
		MTIDL_LONGVAL(FW_STAT_ATIM_TX_FAIL,                  "ATIMs Failing transmission")
		MTIDL_LONGVAL(FW_STAT_TX_MSDU,                       "TX MSDU sent")
		MTIDL_LONGVAL(FW_STAT_TX_FAIL,                       "TX frames that have failed")
		MTIDL_LONGVAL(FW_STAT_TX_RETRY,                      "TX retries to date")
		MTIDL_LONGVAL(FW_STAT_TX_DEFER_PS,                   "Transmits deferred due to power management")
		MTIDL_LONGVAL(FW_STAT_TX_DEFER_UNAUTH,               "Transmit deferred pending authentication")
		MTIDL_LONGVAL(FW_STAT_BEACON_TIMEOUT,                "Authentication Timeouts")
		MTIDL_LONGVAL(FW_STAT_AUTH_TIMEOUT,                  "Authentication Timeouts")
		MTIDL_LONGVAL(FW_STAT_ASSOC_TIMEOUT,                 "Association Timeouts")
		MTIDL_LONGVAL(FW_STAT_ROAM_SCAN_TIMEOUT,             "Roam Scan timeout")
		MTIDL_LONGVAL(FW_STAT_WEP_TOTAL_PACKETS,             "Total number of packets passed through WEP")
		MTIDL_LONGVAL(FW_STAT_WEP_EXCLUDED,                  "Unencrypted packets received when WEP is active")
		MTIDL_LONGVAL(FW_STAT_WEP_UNDECRYPTABLE,             "Packets with no valid keys for decryption")
		MTIDL_LONGVAL(FW_STAT_WEP_ICV_ERROR,                 "Packets with incorrect WEP ICV")
		MTIDL_LONGVAL(FW_STAT_TX_PS_POLL,                    "Transmitted PS POLL")
		MTIDL_LONGVAL(FW_STAT_RX_PS_POLL,                    "Received PS POLL")
		MTIDL_LONGVAL(FW_STAT_MAN_ACTION_TX,                 "Management Actions sent")
		MTIDL_LONGVAL(FW_STAT_MAN_ACTION_RX,                 "Management Actions received")
		MTIDL_LONGVAL(FW_STAT_OUT_OF_RX_MSDUS,               "Management Actions received")
		MTIDL_LONGVAL(FW_STAT_HOST_TX_REQ,                   "Transmit data requests received")
		MTIDL_LONGVAL(FW_STAT_HOST_TX_CFM,                   "Transmit data confirmations sent")
		MTIDL_LONGVAL(FW_STAT_BSS_DISCONNECT,                "Station remove from database due to beacon/data timeout")
		MTIDL_LONGVAL(FW_STAT_RX_DUPLICATE_WITH_RETRY_BIT_0, "Duplicate frames received with retry bit set to 0")
		MTIDL_LONGVAL(FW_STAT_RX_NULL_DATA,                  "Total number of received NULL DATA packets")
		MTIDL_LONGVAL(FW_STAT_TX_NULL_DATA,                  "Total number of sent NULL DATA packets")
		MTIDL_LONGVAL(FW_STAT_TX_BAR,                        "BAR Request sent")
		MTIDL_LONGVAL(FW_STAT_RX_BAR,                        "BAR Request received")
		MTIDL_LONGVAL(FW_STAT_TX_TOTAL_MANAGMENT_PACKETS,    "Total management packet transmitted")
		MTIDL_LONGVAL(FW_STAT_RX_TOTAL_MANAGMENT_PACKETS,    "Total management packet received")
		MTIDL_LONGVAL(FW_STAT_RX_TOTAL_DATA_PACKETS,         "Total data packets received")
		MTIDL_LONGVAL(FW_STAT_RX_FAIL_NO_DECRYPTION_KEY,     "RX Failures due to no key loaded")
		MTIDL_LONGVAL(FW_STAT_RX_DECRYPTION_SUCCESSFUL,      "RX decryption successful")
		MTIDL_LONGVAL(FW_STAT_TX_BAR_FAIL,                   "TX BAR packets failures")
		MTIDL_LONGVAL(FW_STAT_NUM_UNI_PS_INACTIVE,           "Number of unicast packets in PS-Inactive queue")
		MTIDL_LONGVAL(FW_STAT_NUM_MULTI_PS_INACTIVE,         "Number of multicast packets in PS-Inactive queue")
		MTIDL_LONGVAL(FW_STAT_TOT_PKS_PS_INACTIVE,           "Total number of packets in PS-Inactive queue")
		MTIDL_LONGVAL(FW_STAT_NUM_MULTI_PS_ACTIVE,           "Number of multicast packets in PS-Active queue")
		MTIDL_LONGVAL(FW_STAT_NUM_TIME_IN_PS,                "Number of STAs in power-save")
		MTIDL_LONGVAL(FW_STAT_WDS_TX_UNICAST,                "Number of unicast WDS frames transmitted")
		MTIDL_LONGVAL(FW_STAT_WDS_TX_MULTICAST,              "Number of multicast WDS frames transmitted")
		MTIDL_LONGVAL(FW_STAT_WDS_RX_UNICAST,                "Number of unicast WDS frames received")
		MTIDL_LONGVAL(FW_STAT_WDS_RX_MULTICAST,              "Number of multicast WDS frames received")
		MTIDL_LONGVAL(FW_STAT_CTS2SELF_TX,                   "CTS2SELF packets that have been sent")
		MTIDL_LONGVAL(FW_STAT_CTS2SELF_TX_FAIL,              "CTS2SELF packets that have failed")
		MTIDL_LONGVAL(FW_STAT_DECRYPTION_FAILED,             "Number of frames with decryption failed")
		MTIDL_LONGVAL(FW_STAT_FRAGMENT_FAILED,               "Number of frames with wrong fragment number")
		MTIDL_LONGVAL(FW_STAT_TX_MAX_RETRY,                  "Number of TX dropped packets with retry limit exceeded")
		MTIDL_LONGVAL(FW_STAT_TX_RTS_SUCCESS,                "Number of RTS succeeded")
		MTIDL_LONGVAL(FW_STAT_TX_RTS_FAIL,                   "Number of RTS failed")
		MTIDL_LONGVAL(FW_STAT_TX_MULTICAST_DATA,             "Number of transmitted multicast frames")
		MTIDL_LONGVAL(FW_STAT_FCS_ERROR,                     "FCS errors")
		MTIDL_LONGVAL(FW_STAT_RX_ADDBA_REQ,                  "Number of ADDBA requests received")
		MTIDL_LONGVAL(FW_STAT_RX_ADDBA_RES,                  "Number of ADDBA responces received")
		MTIDL_LONGVAL(FW_STAT_RX_DELBA_PKT,                  "Number of DELBA requests received")
		MTIDL_LONGVAL(FW_STAT_RX_MULTICAST_SOURCE,           "Packets with multicast transmitter address received")
		MTIDL_LONGVAL(FW_STAT_RX_MIC_FAILURE,                "Number of MIC failures")
		MTIDL_LONGVAL(FW_STAT_RX_CYCBUF_OVERFLOW,            "Number of Cyclic Buffer Overflows")
		MTIDL_LONGVAL(FW_STAT_AGGR_POSTPONE_PS,				 "Number of AGGR postponed due to PS")
		MTIDL_LONGVAL(FW_STAT_FSDU_CANCELLED,				 "Number of Cancelled FSDUs")
		MTIDL_LONGVAL(FW_STAT_CANT_ADD_ADDRESS_Q,			 "Number of times out of DB address queue is full")
		MTIDL_LONGVAL(FW_STAT_DUPLICATE_MNGT,				 "Number of times duplicate management packet has been received")
		MTIDL_LONGVAL(FW_STAT_DUPLICATE_MCAST,				 "Number of times duplicate MC packet has been received")
		MTIDL_LONGVAL(FW_STAT_TX_RTS_MAX_RETRIES,			 "Number of times retries of RTS reached limit")
		MTIDL_LONGVAL(FW_STAT_TX_LBF_PROBE,					 "Number of LBF probing packets transmitted")
MTIDL_ITEM_END(MTIDL_GeneralStats)

	// if the union DBG_FW_Statistics_u is updated, make sure that /* IDs list */ is updated accordingly.
	typedef union DBG_FW_Statistics_u
	{
		MTIDL_RxDiscardReason	sRxDiscardReason;
		MTIDL_TxDiscardReason	sTxDiscardReason;
		MTIDL_StatusStats		sStatusStats;
		MTIDL_SecurityStats		sSecurityStats;
		MTIDL_AggregationsStats sAggregationsStats;
	} __MTLK_PACKED DBG_FW_Statistics;


#define   MTLK_PACK_OFF
#include "mtlkpack.h"


#endif /* !__MHI_STATISTICS_INC */
