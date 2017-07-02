#ifndef _MTLKINC_H_
#define _MTLKINC_H_

#define __MTLK_IFUNC 
#define __INLINE     inline
#define __LIKELY
#define __UNLIKELY

#include <stdlib.h>

#ifdef MTCFG_DEBUG
#define MTLK_DEBUG
#endif

typedef unsigned char BOOL;
#define FALSE (0)
#define TRUE  (1)

#ifndef HANDLE_T_DEFINED
typedef unsigned long mtlk_handle_t;
#define HANDLE_T_DEFINED

#define MTLK_INVALID_HANDLE HANDLE_T(0)

#define HANDLE_T(x)       ((mtlk_handle_t)(x))
#define HANDLE_T_PTR(t,x) ((t*)(x))
#define HANDLE_T_INT(t,x) ((t)(x))
#endif

#include <arpa/inet.h> /* hton... */

# if __BYTE_ORDER == __BIG_ENDIAN

#include <byteswap.h>

/******************************************
 * MAC <=> Driver interface conversion
 *****************************************/
#define MAC_TO_HOST16(x)   bswap_16(x)
#define MAC_TO_HOST32(x)   bswap_32(x)
#define MAC_TO_HOST64(x)   bswap_64(x)

#define HOST_TO_MAC16(x)   bswap_16(x)
#define HOST_TO_MAC32(x)   bswap_32(x)
#define HOST_TO_MAC64(x)   bswap_64(x)
/******************************************/

/******************************************
 * 802.11 conversion
 *****************************************/
#define WLAN_TO_HOST16(x)  bswap_16(x)
#define WLAN_TO_HOST32(x)  bswap_32(x)
#define WLAN_TO_HOST64(x)  bswap_64(x)

#define HOST_TO_WLAN16(x)  bswap_16(x)
#define HOST_TO_WLAN32(x)  bswap_32(x)
#define HOST_TO_WLAN64(x)  bswap_64(x)
/******************************************/
#else /*__BYTE_ORDER == __BIG_ENDIAN */
/******************************************
 * MAC <=> Driver interface conversion
 *****************************************/
#define MAC_TO_HOST16(x)   (x)
#define MAC_TO_HOST32(x)   (x)
#define MAC_TO_HOST64(x)   (x)

#define HOST_TO_MAC16(x)   (x)
#define HOST_TO_MAC32(x)   (x)
#define HOST_TO_MAC64(x)   (x)
/******************************************/

/******************************************
 * 802.11 conversion
 *****************************************/
#define WLAN_TO_HOST16(x)  (x)
#define WLAN_TO_HOST32(x)  (x)
#define WLAN_TO_HOST64(x)  (x)

#define HOST_TO_WLAN16(x)  (x)
#define HOST_TO_WLAN32(x)  (x)
#define HOST_TO_WLAN64(x)  (x)
/******************************************/
#endif
/******************************************
 * Network conversion
 *****************************************/
#define NET_TO_HOST16(x)        ntohs(x)
#define NET_TO_HOST32(x)        ntohl(x)
#define NET_TO_HOST16_CONST(x)  (__constant_ntohs(x))
#define HOST_TO_NET16_CONST(x)  (__constant_htons(x))

#define HOST_TO_NET16(x)        htons(x)
#define HOST_TO_NET32(x)        htonl(x)
/******************************************/

#define MTLK_UNREFERENCED_PARAM(x)  ((x) = (x))
#define MTLK_OFFSET_OF(type, field) \
  (uint32)(&((type*)0)->field)
#define MTLK_CONTAINER_OF(address, type, field) \
  (type *)((uint8 *)(address) - MTLK_OFFSET_OF(type, field))

#ifndef NULL
#define NULL 0
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define MAX_UINT8 ((uint8)(-1))

#define MTLK_IPAD4(x)   (((4 - ((x) & 0x3)) & 0x3) + (x))

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))
#endif

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

#include "mtlk_slid.h"
#include "mtlk_assert.h"
#include "mtlk_osal.h"
#include "utils.h"
#include "log_osdep.h"
#include "mtlkstartup.h"

#endif /* !_MTLKINC_H_ */
