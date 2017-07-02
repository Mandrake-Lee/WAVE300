/**********************************************************************
 * MetaLink Driver OS Abstraction Layer implementation
 * 
 * This file:
 * [*] defines system-dependent implementation of OSAL data types 
 *     and interfaces.
 * [*] is included in mtlk_osal.h only. No other files must include it!
 *
 * NOTE (MUST READ!!!): 
 *  OSAL_... macros (if defined) are designed for OSAL internal 
 *  usage only (see mtlk_osal.h for more information). They can not 
 *  and must not be used anywhere else.
***********************************************************************/

#if !defined (SAFE_PLACE_TO_INCLUDE_OSAL_OSDEP_DEFS) /* definitions - functions etc. */
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_OSAL_OSDEP_... */

#undef SAFE_PLACE_TO_INCLUDE_OSAL_OSDEP_DEFS

#include <linux/in.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#include <linux/hardirq.h> /* in_atomic() is here for 2.6 */
#else
#include <asm/hardirq.h> /* in_interrupt() is here for 2.4 */
#define in_atomic() in_interrupt()
#endif

#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/ip.h>
#include <linux/ipv6.h>


#include "mem_leak.h"
#include "compat.h"

#define LOG_LOCAL_GID   GID_MTLK_OSAL
#define LOG_LOCAL_FID   0

#define mtlk_osal_emergency_print(fmt, ...) printk("[MTLKD] " fmt "\n", ##__VA_ARGS__)

MTLK_DECLARE_OBJPOOL(g_objpool);

static __INLINE void* 
mtlk_osal_mem_dma_alloc (uint32 size, uint32 tag)
{
  return kmalloc_tag(size, GFP_ATOMIC | __GFP_DMA, tag);
}

static __INLINE void* 
mtlk_osal_mem_alloc (uint32 size, uint32 tag)
{
  MTLK_ASSERT(0 != size);
  return kmalloc_tag(size, GFP_ATOMIC, tag);
}

static __INLINE void 
mtlk_osal_mem_free (void* buffer)
{
  MTLK_ASSERT(NULL != buffer);
  kfree_tag(buffer);
}

static __INLINE int
mtlk_osal_lock_init (mtlk_osal_spinlock_t* spinlock)
{
  spin_lock_init(&spinlock->lock);
#ifdef MTCFG_DEBUG
  atomic_set(&spinlock->acquired, 0);
#endif
  return MTLK_ERR_OK;
}

static __INLINE void
mtlk_osal_lock_acquire (mtlk_osal_spinlock_t* spinlock)
{
  spin_lock_bh(&spinlock->lock);
#ifdef MTCFG_DEBUG
  {
    uint32 acquired = 1;
    acquired = mtlk_osal_atomic_xchg(&spinlock->acquired, acquired);
    MTLK_ASSERT(0 == acquired);
  }
#endif
}

static __INLINE mtlk_handle_t
mtlk_osal_lock_acquire_irq (mtlk_osal_spinlock_t* spinlock)
{
  unsigned long res = 0;
  spin_lock_irqsave(&spinlock->lock, res);
#ifdef MTCFG_DEBUG
  {
    uint32 acquired = 1;
    acquired = mtlk_osal_atomic_xchg(&spinlock->acquired, acquired);
    MTLK_ASSERT(0 == acquired);
  }
#endif
  return HANDLE_T(res);
}

static __INLINE void
mtlk_osal_lock_release (mtlk_osal_spinlock_t* spinlock)
{
#ifdef MTCFG_DEBUG
  {
    uint32 acquired = 0;
    acquired = mtlk_osal_atomic_xchg(&spinlock->acquired, acquired);
    MTLK_ASSERT(1 == acquired);
  }
#endif
  spin_unlock_bh(&spinlock->lock);
}

static __INLINE void
mtlk_osal_lock_release_irq (mtlk_osal_spinlock_t* spinlock, 
                            mtlk_handle_t         acquire_val)
{
#ifdef MTCFG_DEBUG
  {
    uint32 acquired = 0;
    acquired = mtlk_osal_atomic_xchg(&spinlock->acquired, acquired);
    MTLK_ASSERT(1 == acquired);
  }
#endif
  spin_unlock_irqrestore(&spinlock->lock, (unsigned long)acquire_val);
}

static __INLINE void
mtlk_osal_lock_cleanup (mtlk_osal_spinlock_t* spinlock)
{
#ifdef MTCFG_DEBUG
  MTLK_ASSERT(0 == atomic_read(&spinlock->acquired));
#endif
}

static __INLINE int
mtlk_osal_event_init (mtlk_osal_event_t* event)
{
  event->wait_flag = 0;
  init_waitqueue_head(&event->wait_queue);
  return MTLK_ERR_OK;
}

static __INLINE int
mtlk_osal_event_wait (mtlk_osal_event_t* event, uint32 msec)
{
  int res = 
    msec?wait_event_timeout(event->wait_queue, 
                            event->wait_flag,
                            msecs_to_jiffies(msec)):event->wait_flag;

  if (event->wait_flag)
    res = MTLK_ERR_OK;
  else if (res == 0) 
    res = MTLK_ERR_TIMEOUT;
  else {
    // make sure we cover all cases
    printk(KERN_ALERT"wait_event_timeout returned %d", res);
    MTLK_ASSERT(FALSE);
  }

  return res;
}

static __INLINE void 
mtlk_osal_event_set (mtlk_osal_event_t* event)
{
  event->wait_flag = 1;
  wake_up(&event->wait_queue);
}

static __INLINE void
mtlk_osal_event_reset (mtlk_osal_event_t* event)
{
  event->wait_flag = 0;
}

static __INLINE void
mtlk_osal_event_cleanup (mtlk_osal_event_t* event)
{

}

static __INLINE int
mtlk_osal_mutex_init (mtlk_osal_mutex_t* mutex)
{
  sema_init(&mutex->sem, 1); 
  return MTLK_ERR_OK;
}

static __INLINE void
mtlk_osal_mutex_acquire (mtlk_osal_mutex_t* mutex)
{
  down(&mutex->sem); 
}

static __INLINE void
mtlk_osal_mutex_release (mtlk_osal_mutex_t* mutex)
{
  up(&mutex->sem);
}

static __INLINE void
mtlk_osal_mutex_cleanup (mtlk_osal_mutex_t* mutex)
{

}

static __INLINE void
mtlk_osal_msleep (uint32 msec)
{
  msleep(msec);
}

int  __MTLK_IFUNC _mtlk_osal_timer_init(mtlk_osal_timer_t *timer,
                                        mtlk_osal_timer_f  clb,
                                        mtlk_handle_t      clb_usr_data);
int  __MTLK_IFUNC _mtlk_osal_timer_set(mtlk_osal_timer_t *timer,
                                       uint32             msec);
int  __MTLK_IFUNC _mtlk_osal_timer_cancel(mtlk_osal_timer_t *timer);
int  __MTLK_IFUNC _mtlk_osal_timer_cancel_sync(mtlk_osal_timer_t *timer);
void __MTLK_IFUNC _mtlk_osal_timer_cleanup(mtlk_osal_timer_t *timer);

static __INLINE int
mtlk_osal_timer_init (mtlk_osal_timer_t *timer,
                     mtlk_osal_timer_f  clb,
                     mtlk_handle_t      clb_usr_data)
{
  return _mtlk_osal_timer_init(timer, clb, clb_usr_data);
}

static __INLINE int
mtlk_osal_timer_set (mtlk_osal_timer_t *timer,
                     uint32             msec)
{
  return _mtlk_osal_timer_set(timer, msec);
}

static __INLINE int
mtlk_osal_timer_cancel (mtlk_osal_timer_t *timer)
{
  return _mtlk_osal_timer_cancel(timer);
}

static __INLINE int
mtlk_osal_timer_cancel_sync (mtlk_osal_timer_t *timer)
{
  return _mtlk_osal_timer_cancel_sync(timer);
}

static __INLINE void
mtlk_osal_timer_cleanup (mtlk_osal_timer_t *timer)
{
  _mtlk_osal_timer_cleanup(timer);
}

static __INLINE mtlk_osal_timestamp_t
mtlk_osal_timestamp (void)
{
  return jiffies;
}

static __INLINE mtlk_osal_msec_t
mtlk_osal_timestamp_to_ms (mtlk_osal_timestamp_t timestamp)
{
  return jiffies_to_msecs(timestamp);
}

static __INLINE mtlk_osal_timestamp_t
mtlk_osal_ms_to_timestamp (mtlk_osal_msec_t msecs)
{
  return msecs_to_jiffies(msecs);
}

static __INLINE int
mtlk_osal_time_after (mtlk_osal_timestamp_t tm1, mtlk_osal_timestamp_t tm2)
{
  return time_after(tm1, tm2);
}

static __INLINE mtlk_osal_ms_diff_t
mtlk_osal_ms_time_diff (mtlk_osal_msec_t tm1, mtlk_osal_msec_t tm2)
{
  return ( (mtlk_osal_ms_diff_t) ( (uint32)(tm1) - (uint32)(tm2) ) );
}

static __INLINE mtlk_osal_timestamp_diff_t
mtlk_osal_timestamp_diff (mtlk_osal_timestamp_t tm1, mtlk_osal_timestamp_t tm2)
{
  return ( (mtlk_osal_timestamp_diff_t) ( (uint32)(tm1) - (uint32)(tm2) ) );
}

static __INLINE mtlk_osal_timestamp_diff_t
mtlk_osal_time_passed(mtlk_osal_timestamp_t tm)
{
  return mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), tm);
}

static __INLINE uint32
mtlk_osal_time_passed_ms(mtlk_osal_timestamp_t tm)
{
  return mtlk_osal_timestamp_to_ms(mtlk_osal_time_passed(tm));
}

static __INLINE int
mtlk_osal_time_get_mseconds_ago(mtlk_osal_timestamp_t tm)
{
  return mtlk_osal_time_passed_ms(tm) % MTLK_OSAL_MSEC_IN_SEC;
}

static __INLINE int
mtlk_osal_time_get_seconds_ago(mtlk_osal_timestamp_t timestamp)
{
  return (mtlk_osal_time_passed_ms(timestamp) / MTLK_OSAL_MSEC_IN_SEC)
    % MTLK_OSAL_SEC_IN_MIN;
}

static __INLINE int
mtlk_osal_time_get_minutes_ago(mtlk_osal_timestamp_t timestamp)
{
  return (mtlk_osal_time_passed_ms(timestamp) / MTLK_OSAL_MSEC_IN_MIN)
      % MTLK_OSAL_MIN_IN_HOUR;
}

static __INLINE int
mtlk_osal_time_get_hours_ago(mtlk_osal_timestamp_t timestamp)
{
  return (mtlk_osal_time_passed_ms(timestamp) /
    (MTLK_OSAL_MSEC_IN_MIN * MTLK_OSAL_MIN_IN_HOUR));
}

#include <linux/etherdevice.h>

static __INLINE void 
mtlk_osal_copy_eth_addresses (uint8* dst, 
                              const uint8* src)
{
  memcpy(dst, src, ETH_ALEN);
}

static __INLINE int 
mtlk_osal_compare_eth_addresses (const uint8* addr1, 
                                 const uint8* addr2)
{
  return compare_ether_addr(addr1, addr2);
}

static __INLINE int
mtlk_osal_is_zero_address (const uint8* addr)
{
  return is_zero_ether_addr(addr);
}

static __INLINE int 
mtlk_osal_eth_is_multicast (const uint8* addr)
{
  return is_multicast_ether_addr(addr);
}

static __INLINE int
mtlk_osal_eth_is_broadcast (const uint8* addr)
{
  return is_broadcast_ether_addr(addr);
}

static __INLINE int
mtlk_osal_is_valid_ether_addr(const uint8* addr)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,6)
  return is_valid_ether_addr((uint8*)addr);
#else
  return is_valid_ether_addr(addr);
#endif
}

static __INLINE int 
mtlk_osal_ipv4_is_multicast (uint32 addr)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
  return MULTICAST(addr);
#else
  return ipv4_is_multicast(addr);
#endif
}

static __INLINE int
mtlk_osal_ipv4_is_local_multicast (uint32 addr)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,25)
  return LOCAL_MCAST(addr);
#else
  return ipv4_is_local_multicast(addr);
#endif
}

static __INLINE void
mtlk_osal_eth_apply_mask(uint8* dst, uint8* src, const uint8* mask)
{
  dst[0] = src[0] & mask[0];
  dst[1] = src[1] & mask[1];
  dst[2] = src[2] & mask[2];
  dst[3] = src[3] & mask[3];
  dst[4] = src[4] & mask[4];
  dst[5] = src[5] & mask[5];
}

/* atomic counters */

/* 
   WORKAROUND for WAVE400_SW-3: Wireless driver is not compilable 
                                due to "branch is out of range" error

   On some MIPS kernels GCC fails to compile inline 
   wrappers of atomic kernel API, non-inline layer added 
   to OSAL API for these cases
*/
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21) ) && \
      ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28) ) && \
        ( defined (CONFIG_MIPS) )
#   define __MTLK_ATOMIC_API
#   include "osal_atomic_decls.h"
#else
#   define __MTLK_ATOMIC_API      static __INLINE
#   include "osal_atomic_defs.h"
#endif

static __INLINE uint32
mtlk_osal_atomic_add (mtlk_atomic_t* val, uint32 i)
{
  return __mtlk_osal_atomic_add(val, i);
}

static __INLINE uint32
mtlk_osal_atomic_sub (mtlk_atomic_t* val, uint32 i)
{
  return __mtlk_osal_atomic_sub(val, i);
}

static __INLINE uint32
mtlk_osal_atomic_inc (mtlk_atomic_t* val)
{
  return __mtlk_osal_atomic_inc(val);
}

static __INLINE uint32
mtlk_osal_atomic_dec (mtlk_atomic_t* val)
{
  return __mtlk_osal_atomic_dec(val);
}

static __INLINE void
mtlk_osal_atomic_set (mtlk_atomic_t* target, uint32 value)
{
  return __mtlk_osal_atomic_set(target, value);
}

static __INLINE uint32
mtlk_osal_atomic_get (const mtlk_atomic_t* val)
{
  return __mtlk_osal_atomic_get(val);
}

static __INLINE uint32
mtlk_osal_atomic_xchg (mtlk_atomic_t* target, uint32 value)
{
  return __mtlk_osal_atomic_xchg(target, value);
}

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21) ) && \
      ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28) ) && \
        ( defined (CONFIG_MIPS) )
#   define __MTLK_ATOMIC64_API
#   include "osal_atomic64_decls.h"
#else
#   define __MTLK_ATOMIC64_API      static __INLINE
#ifndef ATOMIC64_INIT
  extern mtlk_osal_spinlock_t  mtlk_osal_atomic64_lock;
#endif
#   include "osal_atomic64_defs.h"
#endif

static __INLINE uint64
mtlk_osal_atomic64_add (mtlk_atomic64_t* val, uint64 i)
{
  return __mtlk_osal_atomic64_add(val, i);
}

static __INLINE uint64
mtlk_osal_atomic64_sub (mtlk_atomic64_t* val, uint64 i)
{
  return __mtlk_osal_atomic64_sub(val, i);
}

static __INLINE uint64
mtlk_osal_atomic64_inc (mtlk_atomic64_t* val)
{
  return __mtlk_osal_atomic64_inc(val);
}

static __INLINE uint64
mtlk_osal_atomic64_dec (mtlk_atomic64_t* val)
{
  return __mtlk_osal_atomic64_dec(val);
}

static __INLINE void
mtlk_osal_atomic64_set (mtlk_atomic64_t* target, uint64 value)
{
  return __mtlk_osal_atomic64_set(target, value);
}

static __INLINE uint64
mtlk_osal_atomic64_get (mtlk_atomic64_t* val)
{
  return __mtlk_osal_atomic64_get(val);
}

static __INLINE uint64
mtlk_osal_atomic64_xchg (mtlk_atomic64_t* target, uint64 value)
{
  return __mtlk_osal_atomic64_xchg(target, value);
}


#include "mtlkrmlock.h"

struct _mtlk_osal_timer_t
{
  MTLK_DECLARE_OBJPOOL_CTX(objpool_ctx);

  struct timer_list os_timer;
  mtlk_osal_timer_f clb;
  mtlk_handle_t     clb_usr_data;
  mtlk_rmlock_t     rmlock;
  volatile BOOL     stop;
};



#define mtlk_osal_str_atol(s) simple_strtol(s, NULL, 0)

/* Prior to this version some architectures don't export memchr */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23)
#define mtlk_osal_str_memchr(s, c, n) memchr(s, c, n)
#endif

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID
