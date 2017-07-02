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

/* NLD MTLK OSAL */
#if !defined(SAFE_PLACE_TO_INCLUDE_OSAL_OSDEP_DECLS) /* forward declarations - types etc. */
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_OSAL_OSDEP_... */

#undef SAFE_PLACE_TO_INCLUDE_OSAL_OSDEP_DECLS

#include <linux/version.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26))
#include <asm/semaphore.h>
#else
#include <linux/semaphore.h>
#endif

typedef struct 
{
  MTLK_DECLARE_OBJPOOL_CTX(objpool_ctx);

#ifdef MTCFG_DEBUG
  atomic_t    acquired;
#endif
  spinlock_t  lock;
} mtlk_osal_spinlock_t;

typedef struct
{
  MTLK_DECLARE_OBJPOOL_CTX(objpool_ctx);

  wait_queue_head_t wait_queue;
  volatile int      wait_flag;
} mtlk_osal_event_t;

#define MTLK_OSAL_EVENT_INFINITE ((uint32)-1)

typedef struct 
{
  MTLK_DECLARE_OBJPOOL_CTX(objpool_ctx);

  struct semaphore sem;
} mtlk_osal_mutex_t;

//Type for timestamps
typedef unsigned long mtlk_osal_timestamp_t;
//Type for numbers of milliseconds
typedef uint32 mtlk_osal_msec_t;
//Type for difference of numbers of milliseconds
typedef uint32 mtlk_osal_ms_diff_t;
//Type for difference of timestamps
typedef uint32 mtlk_osal_timestamp_diff_t;

typedef atomic_t mtlk_atomic_t;

#ifdef ATOMIC64_INIT
  typedef atomic64_t mtlk_atomic64_t;
#else
  typedef struct {
    uint64   counter;
  } mtlk_atomic64_t;
#endif

typedef struct _mtlk_osal_timer_t mtlk_osal_timer_t;

