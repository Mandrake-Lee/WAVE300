#ifndef __MTLK_RF_MGMT_IRB_H__
#define __MTLK_RF_MGMT_IRB_H__

#include "mtlkguid.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/*******************************************************************************
 * Enable/Disable RF MGMT (APP => DRV only)
 *******************************************************************************/
// {1A5FE49B-7917-4eff-A477-D8877F213A2B}
#define MTLK_IRB_GUID_RF_MGMT_SET_TYPE                                  \
    MTLK_DECLARE_GUID(0x1a5fe49b, 0x7917, 0x4eff, 0xa4, 0x77, 0xd8, 0x87, 0x7f, 0x21, 0x3a, 0x2b)
// {2A6E2E6B-E6B6-42f5-8EF2-AD7266FAD23A}
#define MTLK_IRB_GUID_RF_MGMT_GET_TYPE                                  \
    MTLK_DECLARE_GUID(0x2a6e2e6b, 0xe6b6, 0x42f5, 0x8e, 0xf2, 0xad, 0x72, 0x66, 0xfa, 0xd2, 0x3a)

struct mtlk_rf_mgmt_evt_type
{
  UMI_RF_MGMT_TYPE type;
  uint32           spr_queue_size; /* IN  (enable=1) max SPRs to store             */
  int32            result;         /* OUT MTLK_ERR_... code                        */
} __MTLK_IDATA;
/*******************************************************************************/

/*******************************************************************************
 * Get/Set default RF MGMT Data (APP => DRV only)
 *******************************************************************************/
// {621A9FE8-F334-4744-BE28-EA670D7600CF}
#define MTLK_IRB_GUID_RF_MGMT_SET_DEF_DATA                              \
    MTLK_DECLARE_GUID(0x621a9fe8, 0xf334, 0x4744, 0xbe, 0x28, 0xea, 0x67, 0xd, 0x76, 0x0, 0xcf)

// {07D5B2AB-8367-48b5-886E-7BE7339C083D}
#define MTLK_IRB_GUID_RF_MGMT_GET_DEF_DATA                              \
    MTLK_DECLARE_GUID(0x7d5b2ab, 0x8367, 0x48b5, 0x88, 0x6e, 0x7b, 0xe7, 0x33, 0x9c, 0x8, 0x3d)

struct mtlk_rf_mgmt_evt_def_data
{
  UMI_DEF_RF_MGMT_DATA data;
  int32                result; /* OUT MTLK_ERR_... code                 */
} __MTLK_IDATA;
/*******************************************************************************/

/*******************************************************************************
 * Get/Set per-peer RF MGMT Data (APP => DRV only)
 *******************************************************************************/
// {13D5587E-8E0D-4470-ADCF-76E63C44DB0F}
#define MTLK_IRB_GUID_RF_MGMT_GET_PEER_DATA                             \
    MTLK_DECLARE_GUID(0x13d5587e, 0x8e0d, 0x4470, 0xad, 0xcf, 0x76, 0xe6, 0x3c, 0x44, 0xdb, 0xf)

// {7FC100D5-FB81-4ed6-9A47-00798AA17D60}
#define MTLK_IRB_GUID_RF_MGMT_SET_PEER_DATA                             \
    MTLK_DECLARE_GUID(0x7fc100d5, 0xfb81, 0x4ed6, 0x9a, 0x47, 0x0, 0x79, 0x8a, 0xa1, 0x7d, 0x60)

struct mtlk_rf_mgmt_evt_peer_data
{
  uint8 mac[ETH_ALEN]; /* IN  desired peer MAC address       */
  uint8 rf_mgmt_data;  /* IN  (SET) desired RF MGMT Data     */
                       /* OUT (GET) current RF MGMT Data     */
  int32 result;        /* OUT MTLK_ERR_... code              */
} __MTLK_IDATA;
/*******************************************************************************/

/*******************************************************************************
 * Send Sounding Packet (APP => DRV only)
 *******************************************************************************/
// {F3E25F9E-D015-4a05-89C8-70E7AC720965}
#define MTLK_IRB_GUID_RF_MGMT_SEND_SP                                   \
    MTLK_DECLARE_GUID(0xf3e25f9e, 0xd015, 0x4a05, 0x89, 0xc8, 0x70, 0xe7, 0xac, 0x72, 0x9, 0x65)

struct mtlk_rf_mgmt_evt_send_sp
{
  uint8  rf_mgmt_data; /* IN  RF MGMT Data to use while sending this SP */
  uint8  rank;         /* IN  rank to use while sending this SP         */
  int32  result;       /* OUT MTLK_ERR_... code                         */
  uint16 data_size;    /* IN  size of data to send                      */
 } __MTLK_IDATA;

static __INLINE void *
mtlk_rf_mgmt_evt_send_sp_data (struct mtlk_rf_mgmt_evt_send_sp* evt)
{
  return &evt[1];
}
/*******************************************************************************/

/*******************************************************************************
 * Get Sounding Packet Response (APP => DRV only)
 *******************************************************************************/
// {E79DBE4D-5ED7-4184-B816-7EA681C4A955}
#define MTLK_IRB_GUID_RF_MGMT_GET_SPR                                   \
    MTLK_DECLARE_GUID(0xe79dbe4d, 0x5ed7, 0x4184, 0xb8, 0x16, 0x7e, 0xa6, 0x81, 0xc4, 0xa9, 0x55)

struct mtlk_rf_mgmt_evt_get_spr
{
  uint8  mac[ETH_ALEN]; /* OUT MAC address of SPR source peer            */
  int32  result;        /* OUT MTLK_ERR_... code                         */
  uint16 buffer_size;   /* IN  size of buffer to be filled with SPR data */
                        /* OUT actual amount of copied SPR data          */
} __MTLK_IDATA;

static __INLINE void *
mtlk_rf_mgmt_evt_get_spr_data (struct mtlk_rf_mgmt_evt_get_spr *evt)
{
  return &evt[1];
}
/*******************************************************************************/

/*******************************************************************************
 * Sounding Packet Response arrived notification (DRV => APP only)
 *******************************************************************************/
// {244DB8E6-EB11-44db-BD2F-8AD3CCA82DBC}
#define MTLK_IRB_GUID_RF_MGMT_SPR_ARRIVED                               \
    MTLK_DECLARE_GUID(0x244db8e6, 0xeb11, 0x44db, 0xbd, 0x2f, 0x8a, 0xd3, 0xcc, 0xa8, 0x2d, 0xbc)

struct mtlk_rf_mgmt_evt_spr_arrived
{
  uint32 required_buff_size;
} __MTLK_IDATA;
/*******************************************************************************/

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_RF_MGMT_IRB_H__ */
