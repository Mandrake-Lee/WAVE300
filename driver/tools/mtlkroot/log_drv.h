/*
 *
 * Copyright (c) 2008 Metalink Broadband (Israel)
 *
 * Written by: Andriy Fidrya
 *
 */

#ifndef __LOG_DRV_H__
#define __LOG_DRV_H__

#include "mtlklist.h"
#include "mtlk_osal.h"

#ifdef FMT_NO_ATTR_CHK
#define __attribute__(x)
#endif

#define LOG_TARGET_CONSOLE (1 << 0)
#define LOG_TARGET_REMOTE  (1 << 1)

#define LOG_CONSOLE_ON(flags) (((flags) & LOG_TARGET_CONSOLE) != 0)
#define LOG_REMOTE_ON(flags)  (((flags) & LOG_TARGET_REMOTE) != 0)

// ---------------
// Data structures
// ---------------

#define MTLK_IDEFS_ON
#include "mtlkidefs.h"

extern int rdebug;
extern int cdebug;
extern int bufs_no;
extern int buf_size;
extern int buf_swap_tmout;

// --------------------
// Internal definitions
// --------------------

typedef struct _mtlk_log_data_t
{
  // Never changes until shutdown, so requires no locking
  int total_buf_count;       // Total buffers count
  mtlk_osal_timer_t timer;   // Buffer swap timer

  // Implementation should use data_lock when accessing these fields
  mtlk_osal_spinlock_t   data_lock;
  uint8 dbg_level[MAX_OID][MAX_GID];
  mtlk_log_buf_entry_t *unsafe_active_buf;      // Driver's active buffer

  mtlk_dlist_t           bufs_free;       // Free buffer pool
  mtlk_osal_spinlock_t   bufs_free_lock;
  mtlk_dlist_t           bufs_ready;      // Ready buffer pool
  mtlk_osal_spinlock_t   bufs_ready_lock;

  mtlk_rmlock_t          buff_rm_lock;    // Remove lock that tracks used buffers

  /* Logging statistics */
  uint32 log_buff_allocations_succeeded;
  uint32 log_buff_allocations_failed;
  uint32 log_pkt_reservations_succeeded;
  uint32 log_pkt_reservations_failed;

  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_INIT_LOOP(FREE_BUFFS);
} __MTLK_IDATA mtlk_log_data_t;

// --------------------
// Functions and macros
// --------------------

/*!
 * \fn      int mtlk_log_init(void)
 * \brief   Initialize logger
 *
 * Call this function on startup before any logging is done.
 * Errors must be reported to user via CERROR. Errors aren't fatal.
 *
 * Implementation details:
 *  - Initializes log_data global variable.
 *  - Allocates buffers for Lower MAC, Upper MAC and Driver and
 *    adds them to free buffers pool.
 *  - Gets a free buffer from pool and makes it active.
 *  - Initializes buffer swap timer and schedules it for execution.
 *
 *  \return MTLK_ERR_OK     all buffers were succesfully allocated.
 *  \return MTLK_ERR_NO_MEM failed to allocate one or more buffers or assign
 *                          an active buffer.
 */
int __MTLK_IFUNC mtlk_log_init(void);

/*!
 * \fn      void mtlk_log_cleanup(void)
 * \brief   Cleanup logger.
 *
 * Call this function on cleanup when logging is no longer required.
 *
 * Implementation details:
 *  - Waits until all allocated buffers are returned into free buffers pool
 *    then frees them.
 *  - Releases all logger resources.
 */
void __MTLK_IFUNC mtlk_log_cleanup(void);

/*!
 * \fn      void mtlk_log_set_defconf(int cdebug, int rdebug)
 * \brief   Set default debug levels for console and remote logging
 *
 * \param   cdebug   default debug level for console
 * \param   rdebug   default debug level for remote
 *
 * Set default debug levels for console and remote logging
 *
 */
void __MTLK_IFUNC mtlk_log_set_defconf(int oid, int cdebug, int rdebug);

/*!
 * \fn      mtlk_log_buf_entry_t *mtlk_log_get_free_buf(void)
 * \brief   Get buffer from free buffers pool.
 *
 * Caller is a new owner of the buffer returned and must return it to one
 * of the pools.
 *
 * If no buffers are available, caller must report this to user via CERROR.
 *
 * \return buffer from free buffers pool if available, NULL otherwise
 */
mtlk_log_buf_entry_t * __MTLK_IFUNC mtlk_log_get_free_buf(void);

/*!
 * \fn      mtlk_log_buf_entry_t *mtlk_log_get_ready_buf(void)
 * \brief   Get buffer from ready buffers pool.
 *
 * Caller is a new owner of the buffer returned and must return it to one
 * of the pools.
 *
 * \return buffer from ready buffers pool if available, NULL otherwise
 */
mtlk_log_buf_entry_t * __MTLK_IFUNC mtlk_log_get_ready_buf(void);

/*!
 * \fn      int mtlk_log_put_free_buf(mtlk_log_buf_entry_t *buf)
 * \brief   Puts buffer to free buffers pool.
 *
 * Caller's buffer ownership will be relinquished on success.
 *
 * Errors must be reported to user via normal logging methods.
 *
 * \return MTLK_ERR_OK      success.
 * \return MTLK_ERR_NO_MEM  error has occured while adding buffer to pool.
 */
int __MTLK_IFUNC mtlk_log_put_free_buf(mtlk_log_buf_entry_t *buf);

/*!
 * \fn      void mtlk_log_put_ready_buf(mtlk_log_buf_entry_t *buf)
 * \brief   Puts buffer to ready buffers pool.
 *
 * Caller's buffer ownership will be relinquished on success.
 *
 * Errors must be reported to user via normal logging methods.
 */
void __MTLK_IFUNC mtlk_log_put_ready_buf(mtlk_log_buf_entry_t *buf);

/*!
 * \fn      void mtlk_log_set_conf(char *conf)
 * \brief   Configure log filters.
 *
 * \param   conf   Configuration string.
 *
 * Parses configuration string and configures log filters.
 *
 * Configuration string contains one or more tokens of form:
 * "<+/-><gid>[r<rd>][c<cd>]"
 * NB! blank configuration line switches all debug off
 * Space separators between tokens are optional.
 * '+' enables logging of messages with gid <gid>
 * '-' disables logging of messages with gid <gid>
 * gid is numeric value (GID).
 * 'r' specified remote logging.
 * 'rd' debug level for the remote target
 * 'c' specified console logging.
 * 'cd' debug level for the console logging
 * If neither 'r' nor 'c' is specified, both ("rc") are assumed.
 * For example:
 *   "-114 +115r3c1 +112c1"
 * Disables remote and console logging for GID 114.
 * Enables remote and console logging for GID 115: remote
 *   w/ debug level 5 and console w/ 1
 * Enables console logging for gid 112 w/ debug level 1
 *
 * Syntax errors are reported using normal logging methods so when
 * both console and remote logging is disabled user won't see anything
 * on a console in response to bad configuration string.
 *
 * By default both remote and console logging is enabled for all GIDs.
 */
void __MTLK_IFUNC mtlk_log_set_conf(char *conf);

/*!
 * \fn      int mtlk_log_get_conf(char *buffer, int size)
 * \brief   Query all logger settings.
 *
 * \param   buffer   buffer to place configuration
 * \param   size     size of the buffer
 *
 * Queries all logger settings
 *
 * \return number of bytes written
 */
int __MTLK_IFUNC mtlk_log_get_conf(char *buffer, int size);

/*!
 * \fn      int mtlk_log_on_init(void)
 * \brief   Platform-specific logger initialization.
 *
 * Implementation is platform-specific.
 * Called on logger initialization to setup communication with userspace if
 * needed (for example, open netlink sockets in Linux etc).
 *
 * \return MTLK_ERR_OK  success.
 * \return < 0          failure.
 */
int __MTLK_IFUNC mtlk_log_on_init(void);

/*!
 * \fn      void mtlk_log_on_cleanup(void)
 * \brief   Platform-specific logger cleanup.
 *
 * Implementation is platform-specific.
 * Called to abort any pending userspace operations and return buffers to
 * free buffers pool.
 *
 * \return MTLK_ERR_OK  success.
 * \return < 0          failure.
 */
void __MTLK_IFUNC mtlk_log_on_cleanup(void);

/*!
 * \fn      int mtlk_log_on_buf_ready(void)
 * \brief   Notification of new buffer in ready pool.
 *
 * Implementation is platform-specific.
 * Called when new buffer appears in ready pool.
 *
 * Implementation details:
 *  - take buffer from ready pool using mtlk_log_get_ready_buf.
 *  - after sending buffer contents, return buffer to free buffers pool.
 *
 * Log module will log all errors reported by this function using normal
 * logging methods.
 *
 * \return MTLK_ERR_OK      success.
 * \return MTLK_ERR_NO_MEM  error has occured during processing of buffer
 *                          data.
 */
int __MTLK_IFUNC mtlk_log_on_buf_ready(void);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __LOG_DRV_H__ */
