/*
 * $Id:
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Shared QoS
 *
 */

#ifndef _MTLK_QOS_H
#define _MTLK_QOS_H

#include "mtlk_osal.h"
#include "mtlkerr.h"
#include "mtlkdfdefs.h"
#include "mhi_umi.h"

#define LOG_LOCAL_GID   GID_QOS
#define LOG_LOCAL_FID   0

#define DPR_QOS_PFX "QOS: "

#define MTLK_WMM_ACI_DEFAULT_CLASS (0)

#define NTS_PRIORITIES          (MAX_USER_PRIORITIES) // defined in shared MAC headers
#define NTS_TIDS                (8)

#define AC_BK                   (1)
#define AC_BE                   (0)
#define AC_VI                   (2)
#define AC_VO                   (3)
#define AC_INVALID              (0xFF)

#define AC_LOWEST               (AC_BK)
#define AC_HIGHEST              (AC_VO)

#define USE_80211_MAP           (0)
#define USE_8021Q_MAP           (1)
#define USE_80211_MAP_INVERTED_VI_BE (100)
#define USE_8021Q_MAP_INVERTED_VI_BE (101)

struct mtlk_qos
{
  /* ACM substitution for minimal admission control support */
  uint8 acm_substitution[NTS_PRIORITIES];

  /* Lock for ACM substitution */
  mtlk_osal_spinlock_t acm_lock;
  MTLK_DECLARE_INIT_STATUS;  
};


typedef int (* mtlk_qos_do_classify_f)(mtlk_nbuf_t *skb);

/*
* This function returns access category by its serial 
* number in meaning of priority level starting from lowest
* Parameters validation is a responsibility of caller.
*/
static __INLINE uint16
mtlk_get_ac_by_number(uint16 number)
{
  static const uint16 sorted_qos_queues[] = {AC_BK, AC_BE, AC_VI, AC_VO};

  ASSERT(number < ARRAY_SIZE(sorted_qos_queues));

  return sorted_qos_queues[number & 0x03];
}

struct _mtlk_vap_handle_t;
int __MTLK_IFUNC mtlk_qos_init (struct mtlk_qos *qos, struct _mtlk_vap_handle_t * vap_handle);
void __MTLK_IFUNC mtlk_qos_cleanup (struct mtlk_qos *qos, struct _mtlk_vap_handle_t * vap_handle);

int __MTLK_IFUNC mtlk_qos_set_map (int map);
int __MTLK_IFUNC mtlk_qos_get_map (void);
BOOL __MTLK_IFUNC mtlk_qos_is_map_8021Q_based (void);

void __MTLK_IFUNC mtlk_qos_set_acm_bits (struct mtlk_qos *qos, const uint8 *acm_state_table);
void __MTLK_IFUNC mtlk_qos_reset_acm_bits (struct mtlk_qos *qos);

const char * __MTLK_IFUNC mtlk_qos_get_ac_name (int ac_idx);
uint16 __MTLK_IFUNC mtlk_qos_get_ac (struct mtlk_qos *qos, mtlk_nbuf_t *nbuf);
uint16 __MTLK_IFUNC mtlk_qos_get_ac_by_tid (uint16 tid);
uint16 __MTLK_IFUNC mtlk_qos_get_tid_by_ac (uint16 ac);

int __MTLK_IFUNC
mtlk_qos_classifier_register (mtlk_qos_do_classify_f classify_fn);
void __MTLK_IFUNC
mtlk_qos_classifier_unregister (void);

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif // _MTLK_QOS_H
