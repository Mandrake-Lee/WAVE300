/***************************************************************************
****************************************************************************
**
** COMPONENT:        Shared RAM Allocation
**
** MODULE:           $File: //bwp/enet/demo153_sw/develop/src/common/shram.h $
**
** VERSION:          $Revision: #7 $
**
** DATED:            $Date: 2004/03/10 $
**
** AUTHOR:           LDE
**
** DESCRIPTION:      Shared RAM Allocation Header
**
** CHANGE HISTORY:
**
**   $Log: shram.h $
**   Revision 1.3  2003/02/21 15:50:30Z  prh
**   Combine host and test_mc code to avoid duplication.
**   Revision 1.2  2003/02/18 15:01:23Z  wrh
**   Removed unused definition
**   Revision 1.1  2003/02/11 16:29:12Z  wrh
**   Initial revision
**   Revision 1.34  2002/07/31 11:41:14Z  jcm
**   CF poll list
**   Revision 1.33.1.1  2002/08/16 10:46:58Z  ifo
**   Reduced FSDU usage in FPGA to fit in new FPGA build
**   Revision 1.33  2002/04/29 13:54:22Z  ifo
**   Moved typedefs to umi.h to allow usage of this file in lower MAC
**   builds without requiring UMAC files.
**   Revision 1.32  2002/04/04 12:16:34Z  prh
**   Increase RX buffers size (except for COSIM).
**   Revision 1.31  2002/04/04 11:09:21Z  mrg
**   Modified number of TX FSDUs for cosim.
**   Revision 1.30  2002/03/08 16:52:28Z  lde
**   Suprising that HIM/Cardbus tests worked.
**   Revision 1.29  2002/03/04 15:07:58Z  ifo
**   Renames test loopback mac to use different macro.
**   Revision 1.28  2002/02/05 08:44:45Z  ifo
**   Made Rx buffer size same as LM (LM needs now to use this value).
**   Revision 1.27.1.1  2002/02/01 14:29:40Z  jag1
**   Revision 1.27  2002/01/08 08:07:43Z  ifo
**   Defined RX circular buffer size in powers of two so
**   the macro may be used by the Lower MAC.
**   Revision 1.26  2001/12/04 15:53:24Z  prh
**   Change for C100.
**   Revision 1.25  2001/12/04 15:18:31Z  prh
**   Compile errors.
**   Revision 1.24  2001/12/04 14:16:46Z  prh
**   Optimize memory usage.
**   Revision 1.23  2001/10/24 14:12:42Z  ifo
**   Chnaged macro to include defines for test umac.
**   Revision 1.22  2001/10/05 10:13:13Z  lde
**   Restored HIM message buffer allocation.
**   Revision 1.21  2001/09/04 07:31:23Z  ifo
**   Removed unused or revised parameters.
**   Revision 1.20  2001/08/24 17:22:10Z  ifo
**   Removed obsolete SRAM struct.
**   Revision 1.19  2001/08/24 16:19:16Z  ifo
**   Added some HIM definitions.
**   Removed SRAM defn as obsolete.
**   Revision 1.18  2001/08/10 11:33:10Z  prh
**   Allow module test UMI buffer allocation to be smaller.
**   Revision 1.17  2001/07/25 07:27:09Z  ifo
**   Added comment.
**   Revision 1.16  2001/07/18 07:44:24Z  ifo
**   Removed unused macros and eleements.
**   Still needs more tidy.
**   Revision 1.15  2001/07/09 12:58:02Z  prh
**   Changes for BSS module testing.
**   Revision 1.14  2001/07/06 14:12:30Z  prh
**   Sort out FSDU/SHRAM allocation.
**   Revision 1.13  2001/07/06 08:49:10Z  prh
**   Temp change for PROBE_FSDU.
**   Revision 1.12  2001/06/27 14:21:10Z  prh
**   Add variable payload refernce to some management packet types.
**   Revision 1.11  2001/06/22 14:30:03Z  ifo
**   bug fix
**   Revision 1.10  2001/06/22 10:07:46Z  prh
**   Add resource for Auth TX packet.
**   Revision 1.9  2001/06/18 07:21:21Z  ifo
**   Added definition of Tx FSDU/MSDU storage.
**   Revision 1.8  2001/06/14 16:51:12Z  ses
**   Add FSDUbeacon.
**   Revision 1.7  2001/06/12 13:26:26Z  ifo
**   Chnaged macro name.
**   Revision 1.6  2001/06/07 09:22:50Z  ses
**   Add probe allocation.
**   Revision 1.5  2001/06/04 15:44:12Z  ses
**   Move shared RAM structures.
**   Revision 1.4  2001/04/30 08:55:08Z  lde
**   In  PC land, the shared RAM is now just a block of memory.
**   Revision 1.3  2001/04/20 16:19:08Z  lde
**   Now includes um_sdu.h and database.h for constants.
**   Revision 1.2  2001/04/04 15:22:02Z  lde
**   Now compiles
**   Revision 1.1  2001/04/04 14:02:25Z  lde
**   Initial revision
**
** LAST MODIFIED BY:   $Author: prh $
**                     $Modtime:$
**
****************************************************************************
*
* Copyright (c) TTPCom Limited, 2001
*
* Copyright (c) Metalink Ltd., 2006 - 2007
*
****************************************************************************/

#ifndef SHRAM_INCLUDED_H
#define SHRAM_INCLUDED_H

#define  MTLK_PACK_ON
#include "mtlkpack.h"

/***************************************************************************/
/***        Build Specific Defines that override the default             ***/
/***************************************************************************/

/***************************************************************************/
/***       Default memory sizes (overridable on a build by build basis   ***/
/***************************************************************************/

#define FSDU_SECTION_SIZE      (2304*7*4)

#if !defined(NO_TX_FSDUS)       // there are 7 if MSDU_MAX_LENGTH=2304 and 10 if MSDU_MAX_LENGTH=1600
    #define NO_TX_FSDUS         8//9    /* Used for allocating FSDU memory pool in shram_fsdu.c */
    #define NO_TX_FSDUS_AC_BK   2
    #define NO_TX_FSDUS_AC_BE   24//20//<y.s> 10
    #define NO_TX_FSDUS_AC_VI   4
    #define NO_TX_FSDUS_AC_VO   2// <y.s> 10
    #define TOTAL_TX_FSDUS      (NO_TX_FSDUS_AC_BK + NO_TX_FSDUS_AC_BE +NO_TX_FSDUS_AC_VI +NO_TX_FSDUS_AC_VO)

	#define NO_MAX_PACKETS_AGG_SUPPORTED_BK   1
	#define NO_MAX_PACKETS_AGG_SUPPORTED_BE	  12
	#define NO_MAX_PACKETS_AGG_SUPPORTED_VI	  4
	#define NO_MAX_PACKETS_AGG_SUPPORTED_VO	  2
#endif

#if !defined(NO_CF_POLLABLE) && defined (ENET_INC_PCF)
    #define NO_CF_POLLABLE      4
#endif

#if !defined(BSS_IND_COUNT)
    #define BSS_IND_COUNT       6
#endif

#if !defined(STA_IND_COUNT)
    #define STA_IND_COUNT       6
#endif

#if !defined(RSN_IND_COUNT)
    #define RSN_IND_COUNT       1
#endif

#if !defined(AGGR_IND_COUNT)
    #define AGGR_IND_COUNT      1
#endif

#if !defined(UMI_IND_COUNT)
    #define UMI_IND_COUNT       1
#endif

/* Note: module test t1616 and subsystem tests 2301, 2302, 2401 and 2731 depend on the following fixed values */
#if !defined (NUM_STATION_DBASE_ENTRIES)
    #if defined(ENET_CFG_AP)
        #define NUM_STATION_DBASE_ENTRIES       32 /* 100 reduced due to memory shortage */
        #define NUM_OF_TS                       20
    #else
        #define NUM_STATION_DBASE_ENTRIES        2
        #define NUM_OF_TS                       20
    #endif
#endif


#if !defined (DEBUG_BUFFER_SIZE)
#if defined(ENET_CFG_RSN_MODULE_TEST)
    #define DEBUG_BUFFER_SIZE   (MAX_DEBUG_OUTPUT*80)     /* Stop build overflowing  */
#else
    #define DEBUG_BUFFER_SIZE   (MAX_DEBUG_OUTPUT*100)    /* Large buffers prevent debug overflow */
#endif
#endif



/***************************************************************************/
/***       HIM buffers                                                   ***/
/***************************************************************************/
/*
 * These define the number of Management, Data and Debug message buffers
 * allocated by the HIM. Note that there are no Memory message buffers used
 * between the Host and the MAC.
 */

#if !defined(NUM_MAN_REQS)
#define NUM_MAN_REQS            2
#endif

#if !defined(NUM_DAT_REQS)
#if defined( ENET_CFG_TLM_SUBSYS )
#define NUM_DAT_REQS            16  /* formerly TX_MSDU_COUNT but C100 does not like it */
#else
#define NUM_DAT_REQS            200 /* formerly TX_MSDU_COUNT but C100 does not like it */
#endif
#endif

// Due to a bug in the driver - MAC created a workaround and reduced dbg req & ind to 1
// Because this workaround is bad for GTUM (uses C100 debug messages), this value
// is back to 2 for GTUM (uses old drivers)
#ifdef ENET_INC_LMAC_PRODUCTION
#define NUM_DBG_REQ_OR_IND_WORKAROUND 2
#else
#define NUM_DBG_REQ_OR_IND_WORKAROUND 1
#endif

#if !defined(NUM_DBG_REQS)
#define NUM_DBG_REQS            NUM_DBG_REQ_OR_IND_WORKAROUND   /* Buffers for HOST to send DBG messages to MAC with. Must cope with max number of consecutive SENDS from C100 without reply i.e. */
#endif

#if !defined(NUM_MEM_REQS)
#define NUM_MEM_REQS            0
#endif

#if !defined(NUM_DAT_INDS)
#define NUM_DAT_INDS            160  /* */
#endif

#if !defined(NUM_DBG_INDS)
#define NUM_DBG_INDS            NUM_DBG_REQ_OR_IND_WORKAROUND   /* 1 for each HIM 'serial' port */
#endif

#if !defined(NUM_MAN_INDS)
#define NUM_MAN_INDS            (BSS_IND_COUNT + STA_IND_COUNT + RSN_IND_COUNT + AGGR_IND_COUNT + UMI_IND_COUNT)
#endif

#if !defined(NUM_MEM_INDS)
#define NUM_MEM_INDS            0
#endif

#if !defined(MAX_RX_DATA_QUEUES)
#define MAX_RX_DATA_QUEUES              8
#define MAX_CALIBR_CACHE_DATA_SIZE      8
#endif


/* If we are using the forwarding pool it must be have at lease one buffer */
#if !defined (ENET_AP_FORWARD_POOL_SIZE) && defined (ENET_CFG_ALLOC_SEPARATE_FORWARD_POOL)
    #define ENET_AP_FORWARD_POOL_SIZE 3
#endif

 /*
 * The size of the Circular Buffer in the Lower MAC used for received MPDUs.
 * It must be a power of two bytes.
 * Note that the receive buffer size is in powers of 2 above 4K.
 * Thus a value of 12 => 4K, 13 => 8K, 14 => 16K, etc.
 */
#if !defined(ENET_CFG_SIZE_RX_CIRC_BUF_LOG_2)
 
    #define ENET_CFG_SIZE_RX_CIRC_BUF_LOG_2     14

   // #define ENET_CFG_SIZE_RX_CIRC_BUF_LOG_2     12
    #endif

#define ENET_CFG_SIZE_RX_CIRC_BUF  (1 << ENET_CFG_SIZE_RX_CIRC_BUF_LOG_2)
#define ENET_RX_CIRC_BUF_SHADOW    ENET_CFG_SIZE_RX_CIRC_BUF*2

/* Number of message that can be placed on message queues between host and HIM */
#define HIM_CHI_N               (NUM_MAN_REQS + NUM_DAT_REQS + NUM_DBG_REQS + NUM_MEM_REQS +\
                                 NUM_MAN_INDS + NUM_DAT_INDS + NUM_DBG_INDS + NUM_MEM_INDS)

typedef struct
{
    uint32 u32IndStartOffset;
    uint32 u32IndNumOfElements;
    uint32 u32ReqStartOffset;
    uint32 u32ReqNumOfElements;
} __MTLK_PACKED CHI_MEM_AREA_DESC;

typedef struct
{
    CHI_MEM_AREA_DESC sFifoQ;
    CHI_MEM_AREA_DESC sDAT;
    CHI_MEM_AREA_DESC sMAN;
    CHI_MEM_AREA_DESC sDBG;
    uint32            u32Magic; /* must be written last!!! */
} __MTLK_PACKED VECTOR_AREA_BASIC;

/* <O.H> -  Indication/Request Buffer Descriptor (new Fifo element) */
typedef struct 
{
    uint8  u8Type;
    uint8  u8Index;
    uint16 u16Info;
} __MTLK_PACKED IND_REQ_BUF_DESC_ELEM;

typedef struct 
{
    uint16 u16QueueSize;
	uint16 u16BufferSize;	    
} __MTLK_PACKED BD_QUEUE_PARAMS;

typedef struct 
{    
	BD_QUEUE_PARAMS asQueueParams[MAX_RX_DATA_QUEUES];
	uint32	FWinterface;
} __MTLK_PACKED READY_REQ;


#define HOST_EXTENSION_MAGIC         		        0xBEADFEED

#define VECTOR_AREA_CALIBR_EXTENSION_ID 	        1
#define VECTOR_AREA_MIPS_CONTROL_EXTENSION_ID 	    2
#define VECTOR_AREA_LOGGER_EXTENSION_ID             3
#define VECTOR_AREA_FW_CAPABILITIES_NUM_OF_STATIONS_EXTENSION_ID	4
#define VECTOR_AREA_FW_CAPABILITIES_NUM_OF_VAPS_EXTENSION_ID	5


typedef struct _VECTOR_AREA_EXTENSION_HEADER
{ 
    uint32 u32ExtensionMagic;
    uint32 u32ExtensionID;
    uint32 u32ExtensionDataSize;
} __MTLK_PACKED VECTOR_AREA_EXTENSION_HEADER;


typedef struct _VECTOR_AREA_CALIBR_EXTENSION_DATA
{ 
    uint32 u32DescriptorLocation;  /* PAS offset to set Calibration Cache Descriptor */
    uint32 u32BufferRequestedSize; /* Requested Calibration Cache Buffer Size        */

}__MTLK_PACKED VECTOR_AREA_CALIBR_EXTENSION_DATA;


typedef struct
{ 
    VECTOR_AREA_EXTENSION_HEADER      sHeader;
    VECTOR_AREA_CALIBR_EXTENSION_DATA sData;
}__MTLK_PACKED VECTOR_AREA_CALIBR_EXTENSION;

typedef struct _VECTOR_AREA_MIPS_CONTROL_EXTENSION_DATA
{
    uint32 u32DescriptorLocation;  /* PAS offset to set mips control Descriptor (MIPS_CONTROL_DESCRIPTOR) */
}__MTLK_PACKED VECTOR_AREA_MIPS_CONTROL_EXTENSION_DATA;


typedef struct
{ 
    VECTOR_AREA_EXTENSION_HEADER            sHeader;
    VECTOR_AREA_MIPS_CONTROL_EXTENSION_DATA sData;
}__MTLK_PACKED VECTOR_AREA_MIPS_CONTROL;

#define LMIPS       0
#define UMIPS       1
#define NUM_OF_MIPS 2

typedef struct
{ 
    uint32 u32MIPSctrl[NUM_OF_MIPS];
} __MTLK_PACKED MIPS_CONTROL_DESCRIPTOR;

#define MIPS_CTRL_DO_ASSERT MTLK_BFIELD_INFO(0, 1) /*  1 bit  starting bit0 */

typedef struct 
{
    uint32 u32BufferDescriptorsLocation; /* PAS offset to set Logger buffers Descriptor */
    uint32 u32NumOfBufferDescriptors;    /* number of BD in Logger BD table */
} __MTLK_PACKED VECTOR_AREA_LOGGER_EXTENSION_DATA;

typedef struct
{
    VECTOR_AREA_EXTENSION_HEADER      sHeader;
    VECTOR_AREA_LOGGER_EXTENSION_DATA sData;
} __MTLK_PACKED VECTOR_AREA_LOGGER_EXTENSION;

typedef struct 
{
uint32 u32NumOfStations;	/* number of stas supported */
} __MTLK_PACKED VECTOR_AREA_FW_CAPABILITIES_NUM_OF_STATIONS_EXTENSION_DATA;

typedef struct
{
	VECTOR_AREA_EXTENSION_HEADER								sHeader;
	VECTOR_AREA_FW_CAPABILITIES_NUM_OF_STATIONS_EXTENSION_DATA	sData;
} __MTLK_PACKED VECTOR_AREA_FW_CAPABILITIES_NUM_OF_STATIONS_EXTENSION;

typedef struct 
{
	uint32 u32NumOfVaps;		/* number of vaps supported */
} __MTLK_PACKED VECTOR_AREA_FW_CAPABILITIES_NUM_OF_VAPS_EXTENSION_DATA;

typedef struct
{
	VECTOR_AREA_EXTENSION_HEADER							sHeader;
	VECTOR_AREA_FW_CAPABILITIES_NUM_OF_VAPS_EXTENSION_DATA	sData;
} __MTLK_PACKED VECTOR_AREA_FW_CAPABILITIES_NUM_OF_VAPS_EXTENSION;

typedef struct 
{
    VECTOR_AREA_BASIC   									sBasic;
    VECTOR_AREA_CALIBR_EXTENSION							sCalibr;
    VECTOR_AREA_MIPS_CONTROL								sMipsControl;
    VECTOR_AREA_LOGGER_EXTENSION							sLoggerExt;
	VECTOR_AREA_FW_CAPABILITIES_NUM_OF_STATIONS_EXTENSION	sFwCapabilitiesNumOfStationsExt;
	VECTOR_AREA_FW_CAPABILITIES_NUM_OF_VAPS_EXTENSION		sFwCapabilitiesNumOfVapsExt;
}__MTLK_PACKED VECTOR_AREA;

/***************************************************************************
**
** NAME         RXMETRICS_HEADER
**
** PARAMETERS       
**              
**              
**
** DESCRIPTION  Sounding packet metric header
**
****************************************************************************/
typedef struct _MTLK_RXMETRICS_HEADER
{
	uint32 rank; /* The number of spatial streams of received sounding packet */
	uint32 isCB;        
	/* put here all the metrics-related stuff you need - padded to 32bit */
} __MTLK_PACKED MTLK_RXMETRICS_HEADER;

/*********************************************************************************************
* G2 Metrics
*********************************************************************************************/

/***************************************************************************
**
** NAME         ASL_SHRAM_METRIC_T

** DESCRIPTION  Sounding packet metric struct
**
****************************************************************************/
#define MAX_SCALE_FACTOR_LENGTH			27
#define MAX_MEASURED_SPATIAL_STREAMS	2
#define MAX_NUM_OF_MEASURED_BINS		108


typedef struct ASL_SHRAM_METRIC_T 
{

	MTLK_RXMETRICS_HEADER     sMtlkHeader;
	
	uint32			 au32metric[MAX_MEASURED_SPATIAL_STREAMS][MAX_NUM_OF_MEASURED_BINS];
	uint32			 au32scaleFactor[MAX_SCALE_FACTOR_LENGTH];
	uint32			 u32NoiseFunction; //represents different values for different Antenna Selection methods

}__MTLK_PACKED ASL_SHRAM_METRIC_T;
/*********************************************************************************************/

/*********************************************************************************************
* G3 Metrics
*********************************************************************************************/
#define PHY_ESNR_VAL MTLK_BFIELD_INFO(0, 13) /* 13 bits starting BIT0  in effectiveSNR field */
#define PHY_MCSF_VAL MTLK_BFIELD_INFO(13, 4) /*  4 bits starting BIT13 in effectiveSNR field */

typedef struct RFM_SHRAM_METRIC_T
{
	uint32			 effectiveSNR;
}__MTLK_PACKED RFM_SHRAM_METRIC_T;
/*********************************************************************************************/

#define  MTLK_PACK_OFF
#include "mtlkpack.h"
#endif /* !SHRAM_INCLUDED_H */

