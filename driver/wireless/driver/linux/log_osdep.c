#include "mtlkinc.h"

#ifndef MTCFG_SILENT

#include "mtlk_snprintf.h"

#define LOG_LOCAL_GID   GID_LOG
#define LOG_LOCAL_FID   0

#define MAX_CLOG_LEN  512
#define MAX_CLOG_BUFS 3

static uint8 dbg_level[MAX_GID] = {0};

uint8 __MTLK_IFUNC
log_osdep_get_level (uint8 gid)
{
  ASSERT(gid < MAX_GID);
  return dbg_level[gid];
}

void __MTLK_IFUNC
log_osdep_set_level (uint8 gid, uint8 level)
{
  ASSERT(gid < MAX_GID);
  dbg_level[gid] = level;
}

void __MTLK_IFUNC
log_osdep_reset_levels (uint8 level)
{
  int gid;
  for (gid = 0; gid < MAX_GID; gid++) {
    dbg_level[gid] = level;
  }
}

struct clog_buf 
{
  struct list_head lentry;
  char             buf[MAX_CLOG_LEN];
};

struct clog_aux
{
  struct list_head list;
  spinlock_t       lock;
  struct clog_buf  bufs[MAX_CLOG_BUFS];
};

static struct clog_aux clog_aux;

void __MTLK_IFUNC
log_osdep_init (void)
{
  int i;

  INIT_LIST_HEAD(&clog_aux.list);
  spin_lock_init(&clog_aux.lock);

  for (i = 0; i < MAX_CLOG_BUFS; i++) {
    list_add(&clog_aux.bufs[i].lentry, &clog_aux.list);
  }
}

void __MTLK_IFUNC
log_osdep_cleanup (void)
{

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9)
inline void
_log_osdep_emergency_vprintk (const char *fname, int line_no, 
                              const char *fmt, va_list args)
{
  printk("[%010u] mtlk **EMERGENCY** (%s:%d): ",
         jiffies_to_msecs(jiffies), fname, line_no);
  vprintk(fmt, args);
  printk("\n");
}
#else
#define MAX_EMERGENCY_STR_SIZE 256

inline void
_log_osdep_emergency_vprintk (const char *fname, int line_no, 
                              const char *fmt, va_list args)
{
  char *ebuf = kmalloc(MAX_EMERGENCY_STR_SIZE, GFP_ATOMIC);

  if (unlikely(ebuf == NULL)) {
    printk("[%010u] mtlk **EMERGENCY** (%s:%d) [FAIL]",
           jiffies_to_msecs(jiffies), fname, line_no);
    return;
  }

  vsnprintf(ebuf, MAX_EMERGENCY_STR_SIZE, fmt, args);
  ebuf[MAX_EMERGENCY_STR_SIZE - 1] = 0;

  printk("[%010u] mtlk **EMERGENCY** (%s:%d): %s", 
         jiffies_to_msecs(jiffies), fname, line_no, ebuf);
  kfree(ebuf);
}
#endif /*LINUX_VERSION_CODE */

inline char *
_log_osdep_get_cbuf (void)
{
  char             *buf = NULL;
  struct list_head *e;

  spin_lock_bh(&clog_aux.lock);
  if (!list_empty(&clog_aux.list)) {
    e = clog_aux.list.next;
    list_del(e);

    buf = list_entry(e, struct clog_buf, lentry)->buf;
  }
  spin_unlock_bh(&clog_aux.lock);
  return buf;
}

inline void
_log_osdep_put_cbuf (char *buf)
{
  struct clog_buf *cb = container_of(buf, struct clog_buf, buf[0]);

  spin_lock_bh(&clog_aux.lock);
  list_add(&cb->lentry, &clog_aux.list);
  spin_unlock_bh(&clog_aux.lock);
}

#ifdef MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS
uint32 __MTLK_IFUNC get_hw_time_stamp(void);
#endif /* MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS */

void __MTLK_IFUNC
log_osdep_do (const char *fname, 
              int         line_no, 
              const char *level, 
              const char *fmt, 
              ...)
{
  char *cmsg_str = _log_osdep_get_cbuf();
  va_list args;

  va_start(args, fmt);
  if (__LIKELY(cmsg_str != NULL)) {
    int cmsg_ln__ = 
#ifdef MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS
      mtlk_snprintf(cmsg_str, MAX_CLOG_LEN,
      "[%010u|%010u] mtlk%s(%s:%d): ",
      jiffies_to_msecs(jiffies), get_hw_time_stamp(), level, fname, line_no);
#else
      mtlk_snprintf(cmsg_str, MAX_CLOG_LEN,
      "[%010u] mtlk%s(%s:%d): ",
      jiffies_to_msecs(jiffies), level, fname, line_no);
#endif /* MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS */
    mtlk_vsnprintf(cmsg_str + cmsg_ln__, MAX_CLOG_LEN - cmsg_ln__,
                  fmt, args);
    printk("%s\n", cmsg_str);
    _log_osdep_put_cbuf(cmsg_str);
  }
  else {
    _log_osdep_emergency_vprintk(fname, line_no, fmt, args);
  } 
  va_end(args);
}
#endif

