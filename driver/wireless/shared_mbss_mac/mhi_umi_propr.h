#ifndef __MHI_UMI_PROPR_INCLUDED_H
#define __MHI_UMI_PROPR_INCLUDED_H

#include "mhi_umi.h"

#define   MTLK_PACK_ON
#include "mtlkpack.h"

/***************************************************************************
**
** NAME         UM_SET_CHAN_REQ
**              UM_SET_CHAN_CFM
**
** PARAMETERS   FREQUENCY_ELEMENT
**
** DESCRIPTION: Intiates a channel switch at run time
**
****************************************************************************/



/***************************************************************************/
/***                          MbssId definitions                         ***/
/***************************************************************************/
#define IND_REQ_INFO_BSS_IDX     MTLK_BFIELD_INFO(0, 3)  /* 3 bits starting BIT0 of u16Info field */
#define IND_REQ_WITHOUT_VAP_INX  MTLK_BFIELD_INFO(3, 13)  /* 3 bits starting BIT0 of u16Info field */
#define RX_IND_DATA_OFFSET       MTLK_BFIELD_INFO(3, 1)  /* 1 bit starting BIT3 of u16Info field */
#define RX_IND_DATA_SIZE         MTLK_BFIELD_INFO(4, 12) /* 12 bits starting BIT4 of u16Info field */
#define RX_RES_BUF_QUE_IDX       MTLK_BFIELD_INFO(3, 13)  /* 13 bit starting BIT3 of u16Info field */
#define LOG_IND_DATA_SIZE        MTLK_BFIELD_INFO(3, 13)  /* 13 bit starting BIT3 of u16Info field */

#define MTLK_MAX_VAPS               5
#define MTLK_VAP_STA_LIMIT_NONE     ((uint16)-1)  /* Ignore limit*/

#define VAP_OPERATION_QUERY             0x0 /* Returns UMI_OK if VAP exists */
#define VAP_OPERATION_ADD               0x1
#define VAP_OPERATION_DEL               0x2

/***************************************************************************
**
** NAME         FREQUENCY_ELEMENT
**              
**
** PARAMETERS   
**
** DESCRIPTION: 
**
****************************************************************************/
#define MAX_TX_POWER_ELEMENTS 3
typedef struct _UMI_FREQUENCY_ELEMENT
{
	uint16 u16Channel;                          // the one and only channel we're gonna use
	uint16 u16ChannelAvailabilityCheckTime;     // if sMIB_Objects.u8ChannelswitchRequired ==> use this to look for radars in the channel before sending beacons
	uint8  u8ScanType;                          // passive or active scan only !!!
	uint8  u8ChannelSwitchCount;                // tbtt countdown (relevant only for channel-switch and not for activate
	uint8  u8SwitchMode;                        // if sMIB_Objects.u8ChannelswitchEnabled ==>
	// [0:3] = SwitchMode from CHANNEL_SWITCH_ANNOUNCEMENT_ELEMENT
	// [4:7] value of SECONDARY_CHANNEL_OFFSET_ELEMENT
	uint8  u8SmRequired;                        // Should be added to this element
	int16  i16CbTransmitPowerLimit;             // This should be taken from FCC + Domain table.
	int16  i16nCbTransmitPowerLimit;            // This should be taken from FCC + Domain table.
	int16  i16AntennaGain;                      // This should be taken from Antenna gain table. Relevant from ver 2.3, at
	uint16 u16ChannelLoad;                      //  Channel Load in percent
	uint8  u8MaxTxPower[MTLK_PAD4(MAX_TX_POWER_ELEMENTS)];       // TPC algo will use it during SCAN process instead of regular TPC_FREQ strutures
	uint8  u8MaxTxPowerIndex[MTLK_PAD4(MAX_TX_POWER_ELEMENTS)];  // TPC algo will use it during SCAN process instead of regular TPC_FREQ strutures
	uint32 u32SwitchType;                       // 0 - Normal, 1 - OCS
} __MTLK_PACKED FREQUENCY_ELEMENT;

/***************************************************************************
**
** NAME         UM_MAN_SCAN_REQ
**
** PARAMETERS   
**
** DESCRIPTION: This message should be sent to request a scan of all
**              available BSSs. The BSS type parameter instructs the Upper
**              MAC only to report BSSs matching the specified type.
**
****************************************************************************/
typedef struct _UMI_SCAN
{
	UMI_SCAN_HDR      sScan;
	FREQUENCY_ELEMENT aChannelParams[UMI_MAX_CHANNELS_PER_SCAN_REQ];
} __MTLK_PACKED UMI_SCAN;

/***************************************************************************
**
** NAME         UM_MAN_MBSS_PRE_ACTIVATE_REQ
**
** PARAMETERS   
**
** DESCRIPTION  Pre-Activate Request. This request should be sent to the Upper
**              MAC to set internal operational parameters.
**
*****************************************************************************/
typedef struct _UMI_MBSS_PRE_ACTIVATE
{
  UMI_MBSS_PRE_ACTIVATE_HDR sHdr;
  FREQUENCY_ELEMENT         sFrequencyElement;
} __MTLK_PACKED UMI_MBSS_PRE_ACTIVATE;


/***************************************************************************
**
** NAME         UM_MAN_ACTIVATE_REQ
**
** PARAMETERS   
**
** DESCRIPTION  Activate Request. This request should be sent to the Upper
**              MAC to start or connect to a network.
**
*****************************************************************************/
typedef struct _UMI_ACTIVATE
{
	UMI_ACTIVATE_HDR          sActivate;
	FREQUENCY_ELEMENT         sFrequencyElement;
} __MTLK_PACKED UMI_ACTIVATE;

/*AOCS*/

/*************************************
* AOCS commands.
*/
#define MAC_AOCS_DISABLE        0
#define MAC_AOCS_ALG_ENABLE     1


/***************************************************************************
**
** NAME         UM_MAN_AOCS_REQ
**
** PARAMETERS   u16Command              MAC_AOCS_DISABLE
**                                      MAC_AOCS_ALG_V1_ENABLE
**              u16MeasurementWindow    Length ow AOCS measurement window in ms
**              u16ThroughputThreshold  Value of throughput threshold in Mbps
**              au32MaxPacketDelay      Array of maximal packet delays per 
**                                      access category in ms
** DESCRIPTION  AOCS configuration request.
**
****************************************************************************/
typedef struct _UMI_AOCS_CFG
{
	uint16 u16Command;
	uint16 u16MeasurementWindow;
	uint32 u32ThrouhhputTH;
} __MTLK_PACKED UMI_AOCS_CFG;

/***************************************************************************
**
** NAME         MC_MAN_AOCS_IND
**
** PARAMETERS   u16Status               MAC_AOCS_DISABLE
**                                      MAC_AOCS_ALG_V1_ENABLE
**              au32AvgPacketDelay      Array of average packet delays per 
**                                      access category in ms
**              u16Throughput           Array of current throughput values 
**                                      per access category in Mbps
** DESCRIPTION  Indication to the MAC Client that the MAC is reached
**              channel switch condition.
**
****************************************************************************/
typedef struct _UMI_AOCS_IND
{
	uint16 u16Status;
	uint16 u16Reserved;
	uint32 au32Throughput_TX[MAX_USER_PRIORITIES];
	uint32 au32Throughput_RX[MAX_USER_PRIORITIES];
} __MTLK_PACKED UMI_AOCS_IND;

/***************************************************************************
**
** NAME         MC_MAN_SET_SHARED_AP_PARAMS_REQ
**
** PARAMETERS   RestrictedChannel : 
**              FrequencyElement  : channel parameters
**
** DESCRIPTION:set BSS structure for set BSS requests from host
**              and confirms from MAC
**
****************************************************************************/

typedef struct _UMI_SET_SHARED_AP_PARAMS
{
    uint16			  RestrictedChannel;
    FREQUENCY_ELEMENT FrequencyElement;
    uint8			  WMMparams;
    uint32			  beaconInterval;
} __MTLK_PACKED UMI_SET_SHARED_AP_PARAMS;

/***************************************************************************
**
** NAME         UMI_MAN
**
** DESCRIPTION  A union of all Management Messages.
**
****************************************************************************/
typedef union _UMI_MAN
{
    UMI_READY                       sReady;
    UMI_RESET                       sReset;
    UMI_POWER_MODE                  sPowerMode;
    UMI_MIB                         sMIB;
    UMI_SCAN                        sScan;
    UMI_ACTIVATE                    sActivate;
    UMI_DISCONNECT                  sDisconnect;
    UMI_NETWORK_EVENT               sNetworkEvent;
    UMI_CONNECTION_EVENT            sConnectionEvent;
    UMI_MAC_EVENT                   sMACEvent;
	UMI_GET_CHANNEL_STATUS			sGetChannelStatus;
	UMI_GET_PEERS_STATUS		    sGetPeersStatus;
    UMI_SET_KEY                     sSetKey;
    UMI_CLEAR_KEY                   sClearKey;
    UMI_SECURITY_ALERT              sSecurityAlert;
    UMI_BCL_REQUEST                 sBclRequest;
    UMI_MAC_VERSION                 sMacVersion;
    UMI_BAR_IND                     sBarRcv;
    UMI_GENERIC_MAC_REQUEST         sGenericMacRequest;
    UMI_OPEN_AGGR_REQ               sOpenAggr;
    UMI_CLOSE_AGGR_REQ              sCloseAggr;
    UMI_ADDBA_REQ_SEND              sAddBaRequestSend;
    UMI_ADDBA_REQ_RCV               sAddBaRequestRcv;
    UMI_ADDBA_RES_SEND              sAddBaRespondSend;
    UMI_ADDBA_RES_RCV               sAddBaRespondRcv;
    UMI_DELBA_REQ_SEND              sDelBaRespondSend;
    UMI_DELBA_REQ_RCV               sDelBaRespondRcv;
    UMI_DYNAMIC_PARAM_TABLE         sACMTable;
	UMI_SET_LED						sLedMode;
    UMI_CONFIG_GPIO                 sSetgpio;
	UMI_DEF_RF_MGMT_DATA            sRFMdata;
	UMI_RF_MGMT_TYPE                sRFMtype;
	UMI_VSAF_INFO                   sVSAFInfo;
    UMI_AOCS_CFG                    sAOCSCfg;
    UMI_AOCS_IND                    sAOCSInd;
	UMI_PS							sPS;
	UMI_PM_UPDATE					sPMUpdate;
	UMI_MAC_WATCHDOG				sMacWatchdog;
	UMI_CHANGE_POWER_STATE			sChangePowerState;
    UMI_TX_POWER_LIMIT		        sChangeTxPowerLimit;
    UMI_ACTIVATE_VAP				sAddVap;
    UMI_DEACTIVATE_VAP				sRemoveVap;
    UMI_SET_SHARED_AP_PARAMS		sSetBss;
} __MTLK_PACKED UMI_MAN;






/***************************************************************************/
/***                      Top Level UMI Message                          ***/
/***************************************************************************/

/***************************************************************************
**
** NAME         UMI_MESSAGE
**
** DESCRIPTION     Top level message structure for all UMI messages.
**
****************************************************************************/
typedef union _UMI_MESSAGE
{
    UMI_MAN uMan;
    UMI_DBG uDbg;
    UMI_MEM uMem;
    UMI_DAT uDat;
} __MTLK_PACKED UMI_MESSAGE;

/* Management messages between MAC and host */
typedef struct _SHRAM_MAN_MSG
{
    UMI_MSG_HEADER sHdr;                 /* Kernel Message Header */
    UMI_MAN        sMsg;                 /* UMI Management Message */
} __MTLK_PACKED SHRAM_MAN_MSG;

/* Debug messages between MAC and host */
typedef struct _SHRAM_DBG_MSG
{
    UMI_MSG_HEADER sHdr;                 /* Kernel Message Header */
    UMI_DBG        sMsg;                 /* UMI Debug Message */
} __MTLK_PACKED SHRAM_DBG_MSG;

#define   MTLK_PACK_OFF
#include "mtlkpack.h"

#endif /* __MHI_UMI_PROPR_INCLUDED_H */
