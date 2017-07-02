/*
 * $Id: dataex.h 11983 2011-11-22 15:32:28Z fleytman $
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 * 
 * This file has common data for the driver, drvhlpr and mtdump utilities
 * to implement and support data exchange via ioctls
 *
 */

#ifndef __DATAEX_H_
#define __DATAEX_H_

#include <linux/sockios.h>
#include <linux/if.h>
#include <linux/wireless.h>

#include "mtlkguid.h"
#include "mhi_ieee_address.h"

#define WE_GEN_DATAEX_PROTO_VER           3 // Protocol version

#define WE_GEN_DATAEX_SUCCESS             0 // Command succeeded
#define WE_GEN_DATAEX_FAIL               -1 // Command failed
#define WE_GEN_DATAEX_PROTO_MISMATCH     -2 // Protocol version mismatch
#define WE_GEN_DATAEX_UNKNOWN_CMD        -3 // Unknown command
#define WE_GEN_DATAEX_DATABUF_TOO_SMALL  -4 // Results do not fit into the data buffer

#define WE_GEN_DATAEX_CMD_CONNECTION_STATS  1
#define WE_GEN_DATAEX_CMD_STATUS            2
#define WE_GEN_DATAEX_CMD_LEDS_MAC          3

typedef struct _WE_GEN_DATAEX_REQUEST {
  uint32 ver;
  uint32 cmd_id;
  uint32 datalen;
} __attribute__((aligned(1), packed)) WE_GEN_DATAEX_REQUEST;

typedef struct _WE_GEN_DATAEX_RESPONSE {
  uint32 ver;
  int32 status;
  uint32 datalen;
} __attribute__((aligned(1), packed)) WE_GEN_DATAEX_RESPONSE;

typedef struct _WE_GEN_DATAEX_DEVICE_STATUS {
  uint32    u32RxCount;
  uint32    u32TxCount;
} __attribute__((aligned(1), packed)) WE_GEN_DATAEX_DEVICE_STATUS;

typedef struct _WE_GEN_DATAEX_CONNECTION_STATUS {
  uint32 u32NumOfConnections;
  WE_GEN_DATAEX_DEVICE_STATUS sDeviceStatus[0];
} __attribute__((aligned(1), packed)) WE_GEN_DATAEX_CONNECTION_STATUS;

typedef struct _WE_GEN_DATAEX_STATUS {
  uint8 security_on;
  uint8 scan_started;
  uint8 frequency_band;
  uint8 link_up;
  uint8 wep_enabled;
} __attribute__((aligned(1), packed)) WE_GEN_DATAEX_STATUS;

typedef struct _WE_GEN_DATAEX_LED {
    uint8 u8BasebLed;
    uint8 u8LedStatus;
    uint8 reserved[2];
} __attribute__((aligned(1), packed)) WE_GEN_DATAEX_LED;

#define MTLK_IOCTL_FIRST       SIOCDEVPRIVATE
#define MTLK_IOCTL_LAST        (SIOCDEVPRIVATE + 15)

enum _MTLK_DEV_PRIVATE_IOCTLS {
  MTLK_IOCTL_DATAEX = MTLK_IOCTL_FIRST,
  MTLK_IOCTL_IRBM
};

#define MTLK_WE_IOCTL_GEN_GPIO (SIOCIWFIRSTPRIV + 30)

#define MTLK_WE_IOCTL_FIRST    SIOCIWFIRSTPRIV
#define MTLK_WE_IOCTL_LAST     SIOCIWLASTPRIV

typedef struct _IRBM_DRIVER_CALL_HDR {
  mtlk_guid_t    event;
  uint32         data_length;
} __attribute__((aligned(1), packed)) IRBM_DRIVER_CALL_HDR;

typedef struct _IRBM_APP_NOTIFY_HDR {
  mtlk_guid_t    event;
  uint32         sequence_number;
  uint32         data_length;
} __attribute__((aligned(1), packed)) IRBM_APP_NOTIFY_HDR;

// {3C528B55-9A70-4697-AD79-21A49F00F79E}
#define MTLK_IRB_GUID_PING                                              \
  MTLK_DECLARE_GUID(0x3c528b55, 0x9a70, 0x4697, 0xad, 0x79, 0x21, 0xa4, 0x9f, 0x00, 0xf7, 0x9e)
// {4FCF0612-53E0-4d1f-A748-78AB53686E8C}
#define MTLK_IRB_GUID_PONG                                              \
  MTLK_DECLARE_GUID(0x4fcf0612, 0x53e0, 0x4d1f, 0xa7, 0x48, 0x78, 0xab, 0x53, 0x68, 0x6e, 0x8c)

// {45EDC219-D0A4-4105-94F6-AE9F3AAE8D9F}
#define MTLK_IRB_GUID_ARP_REQ                                           \
  MTLK_DECLARE_GUID(0x45edc219, 0xd0a4, 0x4105, 0x94, 0xf6, 0xae, 0x9f, 0x3a, 0xae, 0x8d, 0x9f)
struct mtlk_arp_data {
  char   ifname[IFNAMSIZ];
  /* IP addresses are in network byte-order */
  uint32 daddr;
  uint32 saddr;
  uint8  smac[ETH_ALEN];
} __attribute__ ((aligned(1), packed));

// {90A27675-19B8-463b-A6AD-775D6925AF6B}
#define MTLK_IRB_GUID_HANG                                              \
  MTLK_DECLARE_GUID(0x90a27675, 0x19b8, 0x463b, 0xa6, 0xad, 0x77, 0x5d, 0x69, 0x25, 0xaf, 0x6b)

// {F5C4029B-B5D0-4c23-99A6-7937D3B9A4A6}
#define MTLK_IRB_GUID_RMMOD                                             \
  MTLK_DECLARE_GUID(0xf5c4029b, 0xb5d0, 0x4c23, 0x99, 0xa6, 0x79, 0x37, 0xd3, 0xb9, 0xa4, 0xa6)


#endif // !__DATAEX_H_

