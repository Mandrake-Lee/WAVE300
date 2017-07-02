/*
 * $Id: compat.h 11320 2011-06-29 12:47:41Z fleytman $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * This file contains compartibility stuff
 * between different Linux kernels.
 *
 *  Written by Andriy Tkachuk
 *
 */

#ifndef _COMPAT_H_
#define _COMPAT_H_

#include <linux/pci.h>

#if defined(MTCFG_BUS_AHB)
#include <linux/platform_device.h>
#endif

#include <linux/wireless.h>
#include <linux/icmpv6.h>

#define LOG_LOCAL_GID   GID_COMPAT
#define LOG_LOCAL_FID   0

// ------------------------------------------------------------------------------------------
// kthread

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,4)
#include <linux/kthread.h>
#else

#include <asm/uaccess.h>
#include <linux/tqueue.h>

#ifndef CAVIUM
#define INIT_WORK INIT_TQUEUE
#define flush_scheduled_work flush_scheduled_tasks
#define schedule_work schedule_task
#endif

typedef task_queue workqueue;

#define work_struct tq_struct

#endif

// kthread
// ------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------
// ieee80211.h

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
#  include <linux/in6.h>
#  include <ieee80211defs.h>
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
#  include <net/ieee80211.h>
#else
#  include <linux/ieee80211.h>
#endif /* ieee80211.h */

// ieee80211.h
// ------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------
// wait stuff

#ifndef wait_event_interruptible_timeout
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#define __wait_event_interruptible_timeout(wq, condition, ret)     \
  do {                                                             \
    DEFINE_WAIT(__wait);                                           \
    for (;;) {                                                     \
      prepare_to_wait(&wq, &__wait, TASK_INTERRUPTIBLE);           \
      if (condition)                                               \
        break;                                                     \
      if (!signal_pending(current)) {                              \
        ret = schedule_timeout(ret);                               \
        if (!ret)                                                  \
          break;                                                   \
        continue;                                                  \
      }                                                            \
      ret = -ERESTARTSYS;                                          \
      break;                                                       \
    }                                                              \
    finish_wait(&wq, &__wait);                                     \
  } while (0)
#else
#define __wait_event_interruptible_timeout(wq, condition, ret)     \
  do {                                                             \
    wait_queue_t __wait;                                           \
    init_waitqueue_entry(&__wait, current);                        \
    add_wait_queue(&wq, &__wait);                                  \
    for (;;) {                                                     \
      set_current_state(TASK_INTERRUPTIBLE);                       \
      if (condition)                                               \
        break;                                                     \
      if (!signal_pending(current)) {                              \
        ret = schedule_timeout(ret);                               \
        if (!ret)                                                  \
          break;                                                   \
        continue;                                                  \
      }                                                            \
      ret = -ERESTARTSYS;                                          \
      break;                                                       \
    }                                                              \
    set_current_state(TASK_RUNNING);                               \
    remove_wait_queue(&wq, &__wait);                               \
  } while (0)
#endif /* 2.6.0 */

#define wait_event_interruptible_timeout(wq, condition, timeout)   \
  ({                                                               \
    long __ret = timeout;                                          \
    if (!(condition))                                              \
      __wait_event_interruptible_timeout(wq, condition, __ret);    \
    __ret;                                                         \
})
#endif

#ifndef wait_event_timeout
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#define __wait_event_timeout(wq, condition, ret)                   \
  do {                                                             \
    DEFINE_WAIT(__wait);                                           \
                                                                   \
    for (;;) {                                                     \
      prepare_to_wait(&wq, &__wait, TASK_UNINTERRUPTIBLE);         \
      if (condition)                                               \
        break;                                                     \
      ret = schedule_timeout(ret);                                 \
      if (!ret)                                                    \
        break;                                                     \
    }                                                              \
    finish_wait(&wq, &__wait);                                     \
} while (0)
#else
#define __wait_event_timeout(wq, condition, ret)                   \
  do {                                                             \
    wait_queue_t __wait;                                           \
    init_waitqueue_entry(&__wait, current);                        \
    add_wait_queue(&wq, &__wait);                                  \
                                                                   \
    for (;;) {                                                     \
      set_current_state(TASK_UNINTERRUPTIBLE);                     \
      if (condition)                                               \
        break;                                                     \
      ret = schedule_timeout(ret);                                 \
      if (!ret)                                                    \
        break;                                                     \
    }                                                              \
    set_current_state(TASK_RUNNING);                               \
    remove_wait_queue(&wq, &__wait);                               \
} while (0)
#endif /* 2.6.0 */

#define wait_event_timeout(wq, condition, timeout)                 \
  ({                                                               \
    long __ret = timeout;                                          \
    if (!(condition))                                              \
      __wait_event_timeout(wq, condition, __ret);                  \
     __ret;                                                        \
  })
#endif /* #ifndef wait_event_timeout */

// wait stuff
// -----------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------
// IGMP

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
#define IGMPV2_HOST_MEMBERSHIP_REPORT  0x16    /* V2 version of 0x11 */
#define IGMPV3_HOST_MEMBERSHIP_REPORT  0x22    /* V3 version of 0x11 */

#define IGMPV3_MODE_IS_INCLUDE         1
#define IGMPV3_MODE_IS_EXCLUDE         2
#define IGMPV3_CHANGE_TO_INCLUDE       3
#define IGMPV3_CHANGE_TO_EXCLUDE       4
#define IGMPV3_ALLOW_NEW_SOURCES       5
#define IGMPV3_BLOCK_OLD_SOURCES       6

struct igmpv3_grec {
  u8    grec_type;
  u8    grec_auxwords;
  u16   grec_nsrcs;
  u32   grec_mca;
  u32   grec_src[0];
};

struct igmpv3_report {
  u8 type;
  u8 resv1;
  u16 csum;
  u16 resv2;
  u16 ngrec;
  struct igmpv3_grec grec[0];
};
#endif

// IGMP
// ------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------
// netdev

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,23)
static inline void netif_tx_disable(struct net_device *dev)
{
	spin_lock_bh(&dev->xmit_lock);
	netif_stop_queue(dev);
	spin_unlock_bh(&dev->xmit_lock);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,27)
static inline void *netdev_priv(struct net_device *dev)
{
        return dev->priv;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,26)
static inline void free_netdev(struct net_device *dev)
{
        kfree(dev);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
static inline struct sk_buff *
netdev_alloc_skb (struct net_device *dev, unsigned int length)
{
  struct sk_buff *skb;
  skb = dev_alloc_skb(length);
  if (skb)
    skb->dev = dev;
  return skb;
}
#endif

// netdev
// ------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------
// time

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,29)
static inline unsigned int jiffies_to_msecs(const unsigned long j)
{
#if HZ <= 1000 && !(1000 % HZ)
       return (1000 / HZ) * j;
#elif HZ > 1000 && !(HZ % 1000)
       return (j + (HZ / 1000) - 1)/(HZ / 1000);
#else
       return (j * 1000) / HZ;
#endif
}

static inline unsigned long msecs_to_jiffies(const unsigned int m)
{
       if (m > jiffies_to_msecs(MAX_JIFFY_OFFSET))
               return MAX_JIFFY_OFFSET;
#if HZ <= 1000 && !(1000 % HZ)
       return (m + (1000 / HZ) - 1) / (1000 / HZ);
#elif HZ > 1000 && !(HZ % 1000)
       return m * (HZ / 1000);
#else
       return (m * HZ + 999) / 1000;
#endif
}
#else
#include <linux/delay.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,27)
static inline void msleep(unsigned long msecs)
{
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(msecs_to_jiffies(msecs) + 1);
}
#endif

// time
// ------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------
// Wireless extensions

#if WIRELESS_EXT < 18
/* WPA : Generic IEEE 802.11 informatiom element (e.g., for WPA/RSN/WMM).
 * This ioctl uses struct iw_point and data buffer that includes IE id and len
 * fields. More than one IE may be included in the request. Setting the generic
 * IE to empty buffer (len=0) removes the generic IE from the driver. Drivers
 * are allowed to generate their own WPA/RSN IEs, but in these cases, drivers
 * are required to report the used IE as a wireless event, e.g., when
 * associating with an AP. */
#define SIOCSIWGENIE    0x8B30          /* set generic IE */
#define SIOCGIWGENIE    0x8B31          /* get generic IE */

/* WPA : IEEE 802.11 MLME requests */
#define SIOCSIWMLME     0x8B16          /* request MLME operation; uses
                                         * struct iw_mlme */
/* WPA : Authentication mode parameters */
#define SIOCSIWAUTH     0x8B32          /* set authentication mode params */
#define SIOCGIWAUTH     0x8B33          /* get authentication mode params */

/* WPA : Extended version of encoding configuration */
#define SIOCSIWENCODEEXT 0x8B34         /* set encoding token & mode */
#define SIOCGIWENCODEEXT 0x8B35         /* get encoding token & mode */

/* WPA2 : PMKSA cache management */
#define SIOCSIWPMKSA    0x8B36          /* PMKSA cache operation */

#define IWEVGENIE       0x8C05          /* Generic IE (WPA, RSN, WMM, ..)
                                         * (scan results); This includes id and
                                         * length fields. One IWEVGENIE may
                                         * contain more than one IE. Scan
                                         * results may contain one or more
                                         * IWEVGENIE events. */
#define IWEVMICHAELMICFAILURE 0x8C06    /* Michael MIC failure
                                         * (struct iw_michaelmicfailure)
                                         */
#define IWEVASSOCREQIE  0x8C07          /* IEs used in (Re)Association Request.
                                         * The data includes id and length
                                         * fields and may contain more than one
                                         * IE. This event is required in
                                         * Managed mode if the driver
                                         * generates its own WPA/RSN IE. This
                                         * should be sent just before
                                         * IWEVREGISTERED event for the
                                         * association. */
#define IWEVASSOCRESPIE 0x8C08          /* IEs used in (Re)Association
                                         * Response. The data includes id and
                                         * length fields and may contain more
                                         * than one IE. This may be sent
                                         * between IWEVASSOCREQIE and
                                         * IWEVREGISTERED events for the
                                         * association. */
#define IWEVPMKIDCAND   0x8C09          /* PMKID candidate for RSN
                                         * pre-authentication
                                         * (struct iw_pmkid_cand) */

/* MLME requests (SIOCSIWMLME / struct iw_mlme) */
#define IW_MLME_DEAUTH          0
#define IW_MLME_DISASSOC        1

/* SIOCSIWAUTH/SIOCGIWAUTH struct iw_param flags */
#define IW_AUTH_INDEX           0x0FFF
#define IW_AUTH_FLAGS           0xF000
/* SIOCSIWAUTH/SIOCGIWAUTH parameters (0 .. 4095)
 * (IW_AUTH_INDEX mask in struct iw_param flags; this is the index of the
 * parameter that is being set/get to; value will be read/written to
 * struct iw_param value field) */
#define IW_AUTH_WPA_VERSION             0
#define IW_AUTH_CIPHER_PAIRWISE         1
#define IW_AUTH_CIPHER_GROUP            2
#define IW_AUTH_KEY_MGMT                3
#define IW_AUTH_TKIP_COUNTERMEASURES    4
#define IW_AUTH_DROP_UNENCRYPTED        5
#define IW_AUTH_80211_AUTH_ALG          6
#define IW_AUTH_WPA_ENABLED             7
#define IW_AUTH_RX_UNENCRYPTED_EAPOL    8
#define IW_AUTH_ROAMING_CONTROL         9
#define IW_AUTH_PRIVACY_INVOKED         10

/* IW_AUTH_WPA_VERSION values (bit field) */
#define IW_AUTH_WPA_VERSION_DISABLED    0x00000001
#define IW_AUTH_WPA_VERSION_WPA         0x00000002
#define IW_AUTH_WPA_VERSION_WPA2        0x00000004

/* IW_AUTH_PAIRWISE_CIPHER and IW_AUTH_GROUP_CIPHER values (bit field) */
#define IW_AUTH_CIPHER_NONE     0x00000001
#define IW_AUTH_CIPHER_WEP40    0x00000002
#define IW_AUTH_CIPHER_TKIP     0x00000004
#define IW_AUTH_CIPHER_CCMP     0x00000008
#define IW_AUTH_CIPHER_WEP104   0x00000010

/* IW_AUTH_KEY_MGMT values (bit field) */
#define IW_AUTH_KEY_MGMT_802_1X 1
#define IW_AUTH_KEY_MGMT_PSK    2

/* IW_AUTH_80211_AUTH_ALG values (bit field) */
#define IW_AUTH_ALG_OPEN_SYSTEM 0x00000001
#define IW_AUTH_ALG_SHARED_KEY  0x00000002
#define IW_AUTH_ALG_LEAP        0x00000004

/* IW_AUTH_ROAMING_CONTROL values */
#define IW_AUTH_ROAMING_ENABLE  0       /* driver/firmware based roaming */
#define IW_AUTH_ROAMING_DISABLE 1       /* user space program used for roaming
                                         * control */
/* SIOCSIWENCODEEXT definitions */
#define IW_ENCODE_SEQ_MAX_SIZE  8
/* struct iw_encode_ext ->alg */
#define IW_ENCODE_ALG_NONE      0
#define IW_ENCODE_ALG_WEP       1
#define IW_ENCODE_ALG_TKIP      2
#define IW_ENCODE_ALG_CCMP      3
/* struct iw_encode_ext ->ext_flags */
#define IW_ENCODE_EXT_TX_SEQ_VALID      0x00000001
#define IW_ENCODE_EXT_RX_SEQ_VALID      0x00000002
#define IW_ENCODE_EXT_GROUP_KEY         0x00000004
#define IW_ENCODE_EXT_SET_TX_KEY        0x00000008

/* IWEVMICHAELMICFAILURE : struct iw_michaelmicfailure ->flags */
#define IW_MICFAILURE_KEY_ID    0x00000003 /* Key ID 0..3 */
#define IW_MICFAILURE_GROUP     0x00000004
#define IW_MICFAILURE_PAIRWISE  0x00000008
#define IW_MICFAILURE_STAKEY    0x00000010
#define IW_MICFAILURE_COUNT     0x00000060 /* 1 or 2 (0 = count not supported)
                                            */

/* Bit field values for enc_capa in struct iw_range */
#define IW_ENC_CAPA_WPA         0x00000001
#define IW_ENC_CAPA_WPA2        0x00000002
#define IW_ENC_CAPA_CIPHER_TKIP 0x00000004
#define IW_ENC_CAPA_CIPHER_CCMP 0x00000008


/* ------------------------- WPA SUPPORT ------------------------- */

/*
 *      Extended data structure for get/set encoding (this is used with
 *      SIOCSIWENCODEEXT/SIOCGIWENCODEEXT. struct iw_point and IW_ENCODE_*
 *      flags are used in the same way as with SIOCSIWENCODE/SIOCGIWENCODE and
 *      only the data contents changes (key data -> this structure, including
 *      key data).
 *
 *      If the new key is the first group key, it will be set as the default
 *      TX key. Otherwise, default TX key index is only changed if
 *      IW_ENCODE_EXT_SET_TX_KEY flag is set.
 *
 *      Key will be changed with SIOCSIWENCODEEXT in all cases except for
 *      special "change TX key index" operation which is indicated by setting
 *      key_len = 0 and ext_flags |= IW_ENCODE_EXT_SET_TX_KEY.
 *
 *      tx_seq/rx_seq are only used when respective
 *      IW_ENCODE_EXT_{TX,RX}_SEQ_VALID flag is set in ext_flags. Normal
 *      TKIP/CCMP operation is to set RX seq with SIOCSIWENCODEEXT and start
 *      TX seq from zero whenever key is changed. SIOCGIWENCODEEXT is normally
 *      used only by an Authenticator (AP or an IBSS station) to get the
 *      current TX sequence number. Using TX_SEQ_VALID for SIOCSIWENCODEEXT and
 *      RX_SEQ_VALID for SIOCGIWENCODEEXT are optional, but can be useful for
 *      debugging/testing.
 */
struct  iw_encode_ext
{
        __u32           ext_flags; /* IW_ENCODE_EXT_* */
        __u8            tx_seq[IW_ENCODE_SEQ_MAX_SIZE]; /* LSB first */
        __u8            rx_seq[IW_ENCODE_SEQ_MAX_SIZE]; /* LSB first */
        struct sockaddr addr; /* ff:ff:ff:ff:ff:ff for broadcast/multicast
                               * (group) keys or unicast address for
                               * individual keys */
        __u16           alg; /* IW_ENCODE_ALG_* */
        __u16           key_len;
        __u8            key[0];
};

/* SIOCSIWMLME data */
struct  iw_mlme
{
        __u16           cmd; /* IW_MLME_* */
        __u16           reason_code;
        struct sockaddr addr;
};

/* SIOCSIWPMKSA data */
#define IW_PMKSA_ADD            1
#define IW_PMKSA_REMOVE         2
#define IW_PMKSA_FLUSH          3

#define IW_PMKID_LEN    16

struct  iw_pmksa
{
        __u32           cmd; /* IW_PMKSA_* */
        struct sockaddr bssid;
        __u8            pmkid[IW_PMKID_LEN];
};

/* IWEVMICHAELMICFAILURE data */
struct  iw_michaelmicfailure
{
        __u32           flags;
        struct sockaddr src_addr;
        __u8            tsc[IW_ENCODE_SEQ_MAX_SIZE]; /* LSB first */
};
#endif    //  WIRELESS_EXT < 18

#if WIRELESS_EXT < 17
#define IW_QUAL_QUAL_UPDATED    0x01
#define IW_QUAL_LEVEL_UPDATED   0x02
#define IW_QUAL_NOISE_UPDATED   0x04
#define IW_QUAL_QUAL_INVALID    0x10
#define IW_QUAL_LEVEL_INVALID   0x20
#define IW_QUAL_NOISE_INVALID   0x40
#endif /* WIRELESS_EXT < 17 */

#if WIRELESS_EXT < 19
#define IW_QUAL_ALL_UPDATED     0x07
#define IW_QUAL_ALL_INVALID     0x70
#define IW_QUAL_DBM             0x08
#endif /* WIRELESS_EXT < 19 */


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27) && !defined(FEDORA_IWE_STREAM_ADD)
#define mtlk_iwe_stream_add_point(req, str, ends, piwe, extra) \
         iwe_stream_add_point((str), (ends), (piwe), (extra))
#define mtlk_iwe_stream_add_event(req, str, ends, piwe, extra) \
         iwe_stream_add_event((str), (ends), (piwe), (extra))
#define mtlk_iwe_stream_add_value(req, str, ends, piwe, extra) \
         iwe_stream_add_value((str), (ends), (piwe), (extra))
#else
#define mtlk_iwe_stream_add_point  iwe_stream_add_point
#define mtlk_iwe_stream_add_event  iwe_stream_add_event
#define mtlk_iwe_stream_add_value  iwe_stream_add_value
#endif

// Wireless extensions 
// ------------------------------------------------------------------------------------------

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)) || (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30))
#define WEP_KEYS 4
#endif

// ------------------------------------------------------------------------------------------
// Ethernet

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)

/**
 * is_multicast_ether_addr - Determine if the Ethernet address is a multicast.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is a multicast address.
 * By definition the broadcast address is also a multicast address.
 */
static inline int is_multicast_ether_addr(const u8 *addr)
{
        return (0x01 & addr[0]);
}

/**
 * is_broadcast_ether_addr - Determine if the Ethernet address is broadcast
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is the broadcast address.
 */
static inline int is_broadcast_ether_addr(const u8 *addr)
{
        return (addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5]) == 0xff;
}
#endif // LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)

/**
 * compare_ether_addr - Compare two Ethernet addresses
 * @addr1: Pointer to a six-byte array containing the Ethernet address
 * @addr2: Pointer other six-byte array containing the Ethernet address
 *
 * Compare two ethernet addresses, returns 0 if equal
 */
static inline unsigned compare_ether_addr(const u8 *addr1, const u8 *addr2)
{
        const u16 *a = (const u16 *) addr1;
        const u16 *b = (const u16 *) addr2;

        return ((a[0] ^ b[0]) | (a[1] ^ b[1]) | (a[2] ^ b[2])) != 0;
}

#endif // LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,12)
/**
 * is_zero_ether_addr - Determine if give Ethernet address is all zeros.
 * @addr: Pointer to a six-byte array containing the Ethernet address
 *
 * Return true if the address is all zeroes.
 */
static inline int is_zero_ether_addr(const u8 *addr)
{
        return !(addr[0] | addr[1] | addr[2] | addr[3] | addr[4] | addr[5]);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13)
#define ETH_P_PAE 0x888E /* Port Access Entity (IEEE 802.1X) */
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
static inline int ipv6_addr_equal(const struct in6_addr *a1,
                                  const struct in6_addr *a2)
{
        return (a1->s6_addr32[0] == a2->s6_addr32[0] &&
                a1->s6_addr32[1] == a2->s6_addr32[1] &&
                a1->s6_addr32[2] == a2->s6_addr32[2] &&
                a1->s6_addr32[3] == a2->s6_addr32[3]);
}
#endif

// Ethernet
// ------------------------------------------------------------------------------------------

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define mtlk_try_module_get try_module_get
#define mtlk_module_put     module_put
#else
#define mtlk_try_module_get __MOD_INC_USE_COUNT
#define mtlk_module_put     __MOD_DEC_USE_COUNT
#endif

// ------------------------------------------------------------------------------------------
// ICMPv6

/* Definition of ICMPv6 MLD2 Report is wrong in kernels prior to 2.6.6 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,6)
#ifdef ICMPV6_MLD2_REPORT
#undef ICMPV6_MLD2_REPORT
#endif
#define ICMPV6_MLD2_REPORT      143
/* Definitions of MLD2 are absent in kernels prior to 2.4.22 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,22)
#define MLD2_MODE_IS_INCLUDE    1
#define MLD2_MODE_IS_EXCLUDE    2
#define MLD2_CHANGE_TO_INCLUDE  3
#define MLD2_CHANGE_TO_EXCLUDE  4
#define MLD2_ALLOW_NEW_SOURCES  5
#define MLD2_BLOCK_OLD_SOURCES  6
#endif /* KERNEL_VERSION(2,4,22) */
#endif /* KERNEL_VERSION(2,6,6) */

// ICMPv6
// ------------------------------------------------------------------------------------------

/* container_of macro is absent in 2.4 kernels
 * (with exception of uClinux 2.4 kernels). list_entry macro
 * provides same functionality.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) && \
                                        !defined (LINUX_2_4_SNAPGEAR)
#define container_of(ptr, type, member) list_entry((ptr), type, member)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)
#define __iomem
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#define        IRQF_SHARED     SA_SHIRQ
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
typedef unsigned gfp_t;
#endif

// ------------------------------------------------------------------------------------------
// hash32

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,25)
/* hash_32 is needed for l2nat. find details on it in
 * include/linux/hash.h
 */
#include <linux/hash.h>
#else
/* 2^31 + 2^29 - 2^25 + 2^22 - 2^19 - 2^16 + 1 */
#define GOLDEN_RATIO_PRIME_32 0x9e370001UL

static inline u32 hash_32(u32 val, unsigned int bits)
{
        /* On some cpus multiply is faster, on others gcc will do shifts */
        u32 hash = val * GOLDEN_RATIO_PRIME_32;

        /* High bits are more random, so use them. */
        return hash >> (32 - bits);
}
#endif

// hash32
// ------------------------------------------------------------------------------------------

// ------------------------------------------------------------------------------------------
// Architecture

/* Linux ARM architectrure has no atomic_{inc,dec}_return
 * before 2.6.10 kernel
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10) && \
    defined(CONFIG_ARM)

/* 2.6.7 and newer has atomic_{add,sub}_return*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)

#if __LINUX_ARM_ARCH__ < 6

static inline int atomic_add_return(int i, atomic_t *v)
{
	unsigned long flags;
	int val;

	local_irq_save(flags);
	val = v->counter;
	v->counter = val += i;
	local_irq_restore(flags);

	return val;
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
	unsigned long flags;
	int val;

	local_irq_save(flags);
	val = v->counter;
	v->counter = val -= i;
	local_irq_restore(flags);

	return val;
}

#else /* __LINUX_ARM_ARCH__ */
  /* here should be implementation in assembler since archs >= 6
   * may be SMP, but hope there won't be any with such an old kernel
   */
#endif /* __LINUX_ARM_ARCH__ */

#endif /* kernel version < 2.6.7 */

#define atomic_inc_return(v)    (atomic_add_return(1, v))
#define atomic_dec_return(v)    (atomic_sub_return(1, v))

#endif /* kernel version older than 2.6.10 and arm */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,26)
/* a2k - Sigma Design */
#ifndef SIGMA_DESIGN
static inline char *pci_name(struct pci_dev *pdev)
{
        return pdev->slot_name;
}
#endif
#endif

/* a2k - Sigma Design - ARM has no such inline */
#if (defined SIGMA_DESIGN) || (defined SMDK2800)
static inline int atomic_inc_and_test(volatile atomic_t *v)
{
        unsigned long flags;
        int val;

        local_irq_save(flags);
        val = v->counter;
        v->counter = val += 1;
        local_irq_restore(flags);

        return val == 0;
}
#endif

#ifdef SMDK2800
static inline void __list_splice(struct list_head *list,
                                 struct list_head *head)
{
        struct list_head *first = list->next;
        struct list_head *last = list->prev;
        struct list_head *at = head->next;

        first->prev = head;
        head->next = first;

        last->next = at;
        at->prev = last;
}

/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 *
 * The list at @list is reinitialised
 */
static inline void list_splice_init(struct list_head *list,
                                    struct list_head *head)
{
        if (!list_empty(list)) {
                __list_splice(list, head);
                INIT_LIST_HEAD(list);
        }
}

#endif

// Architecture
// ------------------------------------------------------------------------------------------

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16) && !defined(atomic_xchg)
/* atomic_xchg() was introduced in 2.6.16. Here is implementation for
 * older kernels. It's a bit dirty, it fiddles with atomic_t inner
 * structure, which can be different on per-architecture basis, but
 * still can't see how to implement mtlk_osal_atomic_set() without it.
 */
#define atomic_xchg(v, val) (xchg(&((v)->counter), (val)))
#endif

// ------------------------------------------------------------------------------------------
// firmware

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <linux/firmware.h>
#else
struct firmware {
        size_t size;
        u8 *data;
};

int request_firmware (const struct firmware **fw, const char *name,
                      const void *device);

void release_firmware (const struct firmware *fw);
#endif

// firmware
// ------------------------------------------------------------------------------------------

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) 
# define FUNCTION routine
/* No timeout in 2.4 */
#else
# define FUNCTION func
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
#define skb_tail_pointer(skb) (skb)->tail
#define skb_end_pointer(skb) (skb)->end
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
#ifdef CONFIG_RESOURCES_64BIT
typedef u64 resource_size_t;
#else
typedef u32 resource_size_t;
#endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,23)
#ifndef SIGMA_DESIGN
typedef void irqreturn_t;
#define IRQ_NONE
#define IRQ_HANDLED
#endif /* SIGMA_DESIGN */
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)
static inline const char *dev_name(const struct device *dev)
{
  return dev->bus_id;
}
#endif

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif /* _COMPAT_H_ */
