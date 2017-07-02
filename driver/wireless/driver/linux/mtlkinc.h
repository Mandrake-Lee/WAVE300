#ifndef _MTLKINC_H_
#define _MTLKINC_H_

#include <linux/types.h>


/* TODO: WAVE300_SW-100: remove this after FW fix */
#define MBSS_FORCE_VAP_ACTIVATION_SUCCEEDED

//#define MBSS_FORCE_NO_CHANNEL_SWITCH
//#define MBSS_FORCE_NO_AOCS_INITIAL_SELECTION

#if defined(MBSS_FORCE_NO_AOCS_INITIAL_SELECTION) && !defined(MBSS_FORCE_NO_CHANNEL_SWITCH)
#error No channel switch is available while AOCS initial selection is OFF
#endif

/* AirPeek specific compiling elimination */
#define WILD_PACKETS           0

#define __MTLK_IFUNC 
#define __INLINE     inline
#define __LIKELY     likely
#define __UNLIKELY   unlikely

#ifdef MTCFG_DEBUG
#define MTLK_DEBUG
#endif

#ifdef MTCFG_SILENT
#undef MTCFG_DEBUG
#endif

#define MTLK_TEXT(x)           x

#define ARGUMENT_PRESENT
#define MTLK_UNREFERENCED_PARAM(x) ((x) = (x))

#define MTLK_OFFSET_OF(type, field)             offsetof(type, field)
#define MTLK_CONTAINER_OF(address, type, field) container_of(address, type, field)
#define MTLK_FIELD_SIZEOF(type, field)          sizeof(((type *)0)->field)

#define SIZEOF(a) ((unsigned long)sizeof(a)/sizeof((a)[0]))
#define INC_WRAP_IDX(i,s) do { (i)++; if ((i) == (s)) (i) = 0; } while (0)

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

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

/******************************************
 * MAC <=> Driver interface conversion
 *****************************************/
#define MAC_TO_HOST16(x)   le16_to_cpu(x)
#define MAC_TO_HOST32(x)   le32_to_cpu(x)
#define MAC_TO_HOST64(x)   le64_to_cpu(x)

#define HOST_TO_MAC16(x)   cpu_to_le16(x)
#define HOST_TO_MAC32(x)   cpu_to_le32(x)
#define HOST_TO_MAC64(x)   cpu_to_le64(x)
/******************************************/

/******************************************
 * 802.11 conversion
 *****************************************/
#define WLAN_TO_HOST16(x)  le16_to_cpu(x)
#define WLAN_TO_HOST32(x)  le32_to_cpu(x)
#define WLAN_TO_HOST64(x)  le64_to_cpu(x)

#define HOST_TO_WLAN16(x)  cpu_to_le16(x)
#define HOST_TO_WLAN32(x)  cpu_to_le32(x)
#define HOST_TO_WLAN64(x)  cpu_to_le64(x)
/******************************************/

/******************************************
 * Network conversion
 *****************************************/
#define NET_TO_HOST16(x)        ntohs(x)
#define NET_TO_HOST32(x)        ntohl(x)
#define NET_TO_HOST16_CONST(x)  (__constant_ntohs(x))
#define HOST_TO_NET16_CONST(x)  (__constant_htons(x))

#define HOST_TO_NET16(x)   htons(x)
#define HOST_TO_NET32(x)   htonl(x)
/******************************************/

typedef s8  int8;
typedef s16 int16;
typedef s32 int32;
typedef s64 int64;

typedef u8  uint8;
typedef u16 uint16;
typedef u32 uint32;
typedef u64 uint64;

typedef uint16      K_MSG_TYPE;

#define MAX_UINT8  ((uint8)(-1))
#define MAX_UINT32 ((uint32)(-1))

#define MTLK_IPAD4(x)   (((4 - ((x) & 0x3)) & 0x3) + (x))

#include "formats.h"
#include "mtlk_slid.h"
#include "mtlk_assert.h"
#include "mtlk_osal.h"
#include "log_osdep.h"
#include "mtlkstartup.h"
#include "cpu_stat.h"
#include "utils.h"

#endif /* !_MTLKINC_H_ */
