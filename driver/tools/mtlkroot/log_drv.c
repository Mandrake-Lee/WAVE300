/*
 *
 * Copyright (c) 2008 Metalink Broadband (Israel)
 *
 * Written by: Andriy Fidrya
 *
 */

/* Here we override user-defined rtlogger flags */
/* because log driver may print to console only */
#define RTLOG_FLAGS (RTLF_CONSOLE_ENABLED)

#include "mtlkinc.h"

#include "log_drv.h"

#include "drvver.h"

// -------------
// Configuration
// -------------

#define LOG_LOCAL_GID   GID_LOGDRV
#define LOG_LOCAL_FID   1

#define LOG_GET_DLVL_CON(dbg_lvl)      ((dbg_lvl) & 0x0f)
#define LOG_GET_DLVL_RT(dbg_lvl)       ((dbg_lvl) >> 4)

#define LOG_SET_DLVL_CON(dbg_lvl, val) { (dbg_lvl) &= 0xf0; (dbg_lvl) |= (val & 0x0f); }
#define LOG_SET_DLVL_RT(dbg_lvl, val)  { (dbg_lvl) &= 0x0f; (dbg_lvl) |= (val & 0x0f) << 4; }

// --------------------
// Forward declarations
// --------------------

static inline uint32 buf_space_left(mtlk_log_buf_entry_t *buf);
static void put_ready_buf_and_notify(mtlk_log_buf_entry_t *buf);

static uint32 __MTLK_IFUNC log_timer(
    mtlk_osal_timer_t *timer, mtlk_handle_t clb_usr_data);

// ----------------
// Global variables
// ----------------

mtlk_log_data_t log_data = { 0 };

static void
_mtlk_log_detach_active_buff(mtlk_log_data_t* plog_data);

static int
_mtlk_log_add_one_buffer(mtlk_log_data_t* plog_data)
{
  /* Warning this internal function is intended to be called on startup only. */
  /* It does not use locking, therefore.                                      */

  mtlk_log_buf_entry_t *pbuf_descr = 
    mtlk_osal_mem_alloc(sizeof(mtlk_log_buf_entry_t) + buf_size, MTLK_MEM_TAG_LOGGER);

  if (NULL == pbuf_descr) {
    return MTLK_ERR_NO_MEM;
  }

  pbuf_descr->refcount  = 0;
  pbuf_descr->data_size = 0;
  pbuf_descr->expired   = 0;

  mtlk_dlist_push_back(&plog_data->bufs_free, (mtlk_ldlist_entry_t *) pbuf_descr);
  return MTLK_ERR_OK;
}

static void
_mtlk_log_drop_one_buffer(mtlk_log_data_t* plog_data)
{
  /* Warning this internal function is intended to be called on cleanup only. */
  /* It does not use locking and assumes there are packets in the free list.  */

  mtlk_log_buf_entry_t *pbuf_descr = 
    MTLK_LIST_GET_CONTAINING_RECORD(mtlk_dlist_pop_front(&log_data.bufs_free), 
                                    mtlk_log_buf_entry_t,
                                    entry);
  MTLK_ASSERT(NULL != pbuf_descr);
  mtlk_osal_mem_free(pbuf_descr);
};

MTLK_INIT_STEPS_LIST_BEGIN(log_drv)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, FREE_BUFFS_LIST)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, READY_BUFFS_LIST)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, FREE_BUFFS_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, READY_BUFFS_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, FREE_BUFFS)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, DATA_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, BUFF_RM_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, BUFF_RM_LOCK_WAIT)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, BUFF_RM_LOCK_ACQ)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, LOG_OSDEP)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, DETACH_ACTIVE_BUFF)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, LOG_TIMER)
  MTLK_INIT_STEPS_LIST_ENTRY(log_drv, TIMER_SET)
MTLK_INIT_INNER_STEPS_BEGIN(log_drv)
MTLK_INIT_STEPS_LIST_END(log_drv);

void __MTLK_IFUNC
mtlk_log_cleanup(void)
{
  MTLK_CLEANUP_BEGIN(log_drv, MTLK_OBJ_PTR(&log_data))
    MTLK_CLEANUP_STEP(log_drv, TIMER_SET, MTLK_OBJ_PTR(&log_data),
                      mtlk_osal_timer_cancel_sync, (&log_data.timer));
    MTLK_CLEANUP_STEP(log_drv, LOG_TIMER, MTLK_OBJ_PTR(&log_data),
                      mtlk_osal_timer_cleanup, (&log_data.timer));
    MTLK_CLEANUP_STEP(log_drv, DETACH_ACTIVE_BUFF, MTLK_OBJ_PTR(&log_data),
                      _mtlk_log_detach_active_buff, (&log_data));

    MTLK_CLEANUP_STEP(log_drv, LOG_OSDEP, MTLK_OBJ_PTR(&log_data),
                      mtlk_log_on_cleanup, ());
    MTLK_CLEANUP_STEP(log_drv, BUFF_RM_LOCK_ACQ, MTLK_OBJ_PTR(&log_data),
                      mtlk_rmlock_release, (&log_data.buff_rm_lock));
    MTLK_CLEANUP_STEP(log_drv, BUFF_RM_LOCK_WAIT, MTLK_OBJ_PTR(&log_data),
                      mtlk_rmlock_wait, (&log_data.buff_rm_lock));
    MTLK_CLEANUP_STEP(log_drv, BUFF_RM_LOCK, MTLK_OBJ_PTR(&log_data),
                      mtlk_rmlock_cleanup, (&log_data.buff_rm_lock));
    MTLK_CLEANUP_STEP(log_drv, DATA_LOCK, MTLK_OBJ_PTR(&log_data),
                      mtlk_osal_lock_cleanup, (&log_data.data_lock));
    while (MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(&log_data), FREE_BUFFS) > 0) {
      MTLK_CLEANUP_STEP_LOOP(log_drv, FREE_BUFFS, MTLK_OBJ_PTR(&log_data),
                             _mtlk_log_drop_one_buffer, (&log_data));
    }
    MTLK_CLEANUP_STEP(log_drv, READY_BUFFS_LOCK, MTLK_OBJ_PTR(&log_data),
                      mtlk_osal_lock_cleanup, (&log_data.bufs_ready_lock));
    MTLK_CLEANUP_STEP(log_drv, FREE_BUFFS_LOCK, MTLK_OBJ_PTR(&log_data),
                      mtlk_osal_lock_cleanup, (&log_data.bufs_free_lock));
    MTLK_CLEANUP_STEP(log_drv, READY_BUFFS_LIST, MTLK_OBJ_PTR(&log_data),
                      mtlk_dlist_cleanup, (&log_data.bufs_ready));
    MTLK_CLEANUP_STEP(log_drv, FREE_BUFFS_LIST, MTLK_OBJ_PTR(&log_data),
                      mtlk_dlist_cleanup, (&log_data.bufs_free));
  MTLK_CLEANUP_END(log_drv, MTLK_OBJ_PTR(&log_data))
}

int __MTLK_IFUNC
mtlk_log_init(void)
{
  int i;

  MTLK_INIT_TRY(log_drv, MTLK_OBJ_PTR(&log_data))
    MTLK_INIT_STEP_VOID(log_drv, FREE_BUFFS_LIST, MTLK_OBJ_PTR(&log_data),
                        mtlk_dlist_init, (&log_data.bufs_free));
    MTLK_INIT_STEP_VOID(log_drv, READY_BUFFS_LIST, MTLK_OBJ_PTR(&log_data),
                        mtlk_dlist_init, (&log_data.bufs_ready));
    MTLK_INIT_STEP(log_drv, FREE_BUFFS_LOCK, MTLK_OBJ_PTR(&log_data),
                   mtlk_osal_lock_init, (&log_data.bufs_free_lock));
    MTLK_INIT_STEP(log_drv, READY_BUFFS_LOCK, MTLK_OBJ_PTR(&log_data),
                    mtlk_osal_lock_init, (&log_data.bufs_ready_lock));
    for (i = 0; i < bufs_no; ++i) {
      MTLK_INIT_STEP_LOOP(log_drv, FREE_BUFFS, MTLK_OBJ_PTR(&log_data),
                          _mtlk_log_add_one_buffer, (&log_data))
    }
    MTLK_INIT_STEP(log_drv, DATA_LOCK, MTLK_OBJ_PTR(&log_data),
                   mtlk_osal_lock_init, (&log_data.data_lock));
    MTLK_INIT_STEP(log_drv, BUFF_RM_LOCK, MTLK_OBJ_PTR(&log_data),
                   mtlk_rmlock_init, (&log_data.buff_rm_lock));
    MTLK_INIT_STEP_VOID(log_drv, BUFF_RM_LOCK_WAIT, MTLK_OBJ_PTR(&log_data),
                        MTLK_NOACTION, ());
    MTLK_INIT_STEP_VOID(log_drv, BUFF_RM_LOCK_ACQ, MTLK_OBJ_PTR(&log_data),
                        mtlk_rmlock_acquire, (&log_data.buff_rm_lock));
    MTLK_INIT_STEP(log_drv, LOG_OSDEP, MTLK_OBJ_PTR(&log_data),
                   mtlk_log_on_init, ());
    MTLK_INIT_STEP_VOID(log_drv, DETACH_ACTIVE_BUFF, MTLK_OBJ_PTR(&log_data),
                        MTLK_NOACTION, ());
    MTLK_INIT_STEP(log_drv, LOG_TIMER, MTLK_OBJ_PTR(&log_data),
                   mtlk_osal_timer_init, (&log_data.timer, log_timer, HANDLE_T(NULL)));

    mtlk_log_set_defconf(-1, cdebug, rdebug);

    // TODO: This step should become a part of start sequence
    MTLK_INIT_STEP(log_drv, TIMER_SET, MTLK_OBJ_PTR(&log_data),
                   mtlk_osal_timer_set, (&log_data.timer, buf_swap_tmout));
  MTLK_INIT_FINALLY(log_drv, MTLK_OBJ_PTR(&log_data))
  MTLK_INIT_RETURN(log_drv, MTLK_OBJ_PTR(&log_data), mtlk_log_cleanup, ());
}

int __MTLK_IFUNC 
mtlk_log_get_conf (char *buffer, int size)
{
  int i, oid, len, written = 0;
  uint8 clvl, rlvl;

  if (!buffer || !size)
    goto end;
  for (oid = 0; oid < MAX_OID; oid++) {
    len = snprintf(buffer, size, "GID  c  r  for OID %d\n", oid);
    buffer += len; size -= len; written += len;
    for (i = 0; i < MAX_GID; i++) {
      if (size <= 0)
        break;
      clvl = LOG_GET_DLVL_CON(log_data.dbg_level[oid][i]);
      rlvl = LOG_GET_DLVL_RT(log_data.dbg_level[oid][i]);
      if (!clvl && !rlvl)
        continue;
      len = snprintf(buffer, size, "%3d %2d %2d\n", i, clvl, rlvl);
      buffer += len; size -= len; written += len;
    }
  }
end:
  return written;
}

void __MTLK_IFUNC
mtlk_log_set_defconf(int oid, int cdebug, int rdebug)
{
  int i, j;

  if (oid < 0) {
    for (j = 0; j < MAX_OID; j++) {
      for (i = 0; i < MAX_GID; i++) {
        LOG_SET_DLVL_CON(log_data.dbg_level[j][i], cdebug);
        LOG_SET_DLVL_RT(log_data.dbg_level[j][i], rdebug);
      }
    }
  } else {
    for (i = 0; i < MAX_GID; i++) {
      LOG_SET_DLVL_CON(log_data.dbg_level[oid][i], cdebug);
      LOG_SET_DLVL_RT(log_data.dbg_level[oid][i], rdebug);
    }
  }
}

#define LOG_PARSE_ERROR "Log configuration parse error: "

static int
log_read_int (char **p)
{
  char *endp;
  int result;

  if (!isdigit(**p)) {
    ELOG_C(LOG_PARSE_ERROR "'%c' unexpected", **p);
    goto err_end;
  }
  endp = NULL;
  result = (uint8)simple_strtoul(*p, &endp, 10);
  if (*p == endp) {
    ELOG_V(LOG_PARSE_ERROR "value expected");
    goto err_end;
  }
  *p = endp;
  return result;
err_end:
  return -1;
}

static int
log_read_dlevel (char **p, char *target)
{
  char *endp;
  int result;

  if (**p != 'c' && **p != 'r') {
    ELOG_C(LOG_PARSE_ERROR "invalid log target: %c", **p);
    goto err_end;
  }
  *target = **p;
  (*p)++;
  if (!*p || !isdigit(**p)) {
    ELOG_V(LOG_PARSE_ERROR "invalid debug level");
    goto err_end;
  }
  endp = NULL;
  result = (uint8)simple_strtoul(*p, &endp, 10);
  if (*p == endp) {
    ELOG_V(LOG_PARSE_ERROR "value expected");
    goto err_end;
  }
  *p = endp;
  return result;
err_end:
  return -1;
}

void __MTLK_IFUNC 
mtlk_log_set_conf(char *conf)
{
  char *p, *str, target, *dbg_str;
  uint8 gid, enable;
  int val, i, c, r, oid;

  if (!conf || *conf == '\0')
    return;
  /* remove all blanks from the configuration string */
  if (!(p = str = mtlk_osal_mem_alloc(strlen(conf), MTLK_MEM_TAG_LOGGER)))
    return;
  while (*conf) {
    if ((*conf == ' ') || (*conf == '\t') || (*conf == '\r') || (*conf == '\n')) {
    } else {
      *p++ = *conf;
    }
    conf++;
  }
  *p = '\0';
  r = c = -1;
  /* read oid now */
  p = str;
  oid = log_read_int(&p);
  if (oid < 0 || oid >= MAX_OID) {
    ELOG_V(LOG_PARSE_ERROR "invalid OID");
    goto end;
  }
  if ((dbg_str = strstr(p, "rdebug="))) {
    if (sscanf(dbg_str, "rdebug=%d", &r) != 1) {
      ELOG_V("Wrong rdebug value");
      goto end;
    }
    ILOG0_DD("Using value of %d for all remote debug levels, OID %d", r, oid);
    rdebug = r;
    mtlk_log_set_defconf(oid, cdebug, r);
  }
  if ((dbg_str = strstr(p, "cdebug="))) {
    if (sscanf(dbg_str, "cdebug=%d", &c) != 1) {
      ELOG_V("Wrong cdebug value");
      goto end;
    }
    ILOG0_DD("Using value of %d for all console debug levels, OID %d", c, oid);
    cdebug = c;
    mtlk_log_set_defconf(oid, c, rdebug);
  }
  if (r >= 0 || c >= 0)
    goto end;
  /* read all settings */
  if (*p == '\0') {
    ILOG0_D("Logging disabled for OID %d", oid);
    mtlk_log_set_defconf(oid, 0, 0);
    goto end;
  }
  while (*p) {
    if (*p == '-')
      enable = 0;
    else if (*p == '+')
      enable = 1;
    else {
      ELOG_C(LOG_PARSE_ERROR "+ or - expected '%c'", *p);
      break;
    }
    if (*(++p) == '\0') {
      ELOG_V(LOG_PARSE_ERROR "GID is missing");
      goto end;
    }
    /* here comes GID */
    val = log_read_int(&p);
    if (val < 0 || val >= MAX_GID) {
      ELOG_V(LOG_PARSE_ERROR "invalid GID");
      goto end;
    }
    gid = val;
    if (!enable) {
      LOG_SET_DLVL_CON(log_data.dbg_level[oid][gid], 0)
      LOG_SET_DLVL_RT(log_data.dbg_level[oid][gid], 0);
      continue;
    }
    /* here come target specifier and debug level: c or r or both */
    for (i = 0; i < 2; i++) {
      if (*p == '\0' && !i) {
        ELOG_V(LOG_PARSE_ERROR "target missed");
        break;
      }
      if (*p == '\0' || *p == '+' || *p == '-' || isdigit(*p))
        break;
      val = log_read_dlevel(&p, &target);
      if (val < 0)
        continue;
      if (target == 'c')
        LOG_SET_DLVL_CON(log_data.dbg_level[oid][gid], val)
      else
        LOG_SET_DLVL_RT(log_data.dbg_level[oid][gid], val);
    }
  }
end:
  if (str)
    mtlk_osal_mem_free(str);
}

BOOL __MTLK_IFUNC
mtlk_log_check_version (uint16 major, uint16 minor)
{
  BOOL res = FALSE;

  if (major != RTLOGGER_VER_MAJOR) {
    CERROR(__FILE__, __LINE__, "Incompatible Logger version (%d != %d)", major, RTLOGGER_VER_MAJOR);
  }
  else if (minor != RTLOGGER_VER_MINOR) {
    CWARNING(__FILE__, __LINE__, "Different minor Logger version (%d != %d)", minor, RTLOGGER_VER_MINOR);
    res = TRUE;
  }
  else {
    CINFO(__FILE__, __LINE__, "Logger version %d.%d", major, minor);
    res = TRUE;
  }

  return res;
}

int __MTLK_IFUNC
mtlk_log_get_flags(int level, int oid, int gid)
{
  uint8 dbg_lvl;
  int flags;

  mtlk_osal_lock_acquire(&log_data.data_lock);
  dbg_lvl = log_data.dbg_level[oid][gid];
  flags = 0;
  if (level <= LOG_GET_DLVL_CON(dbg_lvl))
    flags |= LOG_TARGET_CONSOLE;
  if (level <= LOG_GET_DLVL_RT(dbg_lvl))
    flags |= LOG_TARGET_REMOTE;
  mtlk_osal_lock_release(&log_data.data_lock);
  return flags;
}

mtlk_log_buf_entry_t * __MTLK_IFUNC
mtlk_log_get_free_buf(void)
{
  mtlk_log_buf_entry_t *res;

  mtlk_osal_lock_acquire(&log_data.bufs_free_lock);
  res =  MTLK_LIST_GET_CONTAINING_RECORD(mtlk_dlist_pop_front(&log_data.bufs_free), 
                                         mtlk_log_buf_entry_t,
                                         entry);
  if(NULL != res)
  {
    mtlk_rmlock_acquire(&log_data.buff_rm_lock);
    log_data.log_buff_allocations_succeeded++;
  } else {
    log_data.log_buff_allocations_failed++;
  }

  mtlk_osal_lock_release(&log_data.bufs_free_lock);

  return res;
}

mtlk_log_buf_entry_t * __MTLK_IFUNC
mtlk_log_get_ready_buf(void)
{
  mtlk_log_buf_entry_t *res;

  mtlk_osal_lock_acquire(&log_data.bufs_ready_lock);
  res =  MTLK_LIST_GET_CONTAINING_RECORD(mtlk_dlist_pop_front(&log_data.bufs_ready), 
                                         mtlk_log_buf_entry_t,
                                         entry);
  mtlk_osal_lock_release(&log_data.bufs_ready_lock);

  return res;
}

int __MTLK_IFUNC
mtlk_log_put_free_buf(mtlk_log_buf_entry_t *buf)
{
  // Note: reset is done in put_free_buf and not in get_free_buf because
  // get is usually called in more time-critical situations
  buf->data_size = 0;
  buf->expired = 0;

  mtlk_osal_lock_acquire(&log_data.bufs_free_lock);
  mtlk_dlist_push_back(&log_data.bufs_free, (mtlk_ldlist_entry_t *) buf);
  mtlk_rmlock_release(&log_data.buff_rm_lock);
  mtlk_osal_lock_release(&log_data.bufs_free_lock);
  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
mtlk_log_put_ready_buf(mtlk_log_buf_entry_t *buf)
{
  mtlk_osal_lock_acquire(&log_data.bufs_ready_lock);
  mtlk_dlist_push_back(&log_data.bufs_ready, (mtlk_ldlist_entry_t *) buf);
  mtlk_osal_lock_release(&log_data.bufs_ready_lock);
}

static BOOL
_mtlk_log_unsafe_active_buf_alloc (void) 
{ 
  log_data.unsafe_active_buf = mtlk_log_get_free_buf(); 

  if (NULL != log_data.unsafe_active_buf) { 
    /* restart buffer expiration timer */ 
    mtlk_osal_timer_set(&log_data.timer, buf_swap_tmout);
    return TRUE;
  } 

  return FALSE;
} 

mtlk_log_buf_entry_t * __MTLK_IFUNC
mtlk_log_new_pkt_reserve(uint pkt_size, uint8 **ppdata)
{
  mtlk_log_buf_entry_t *buf = NULL;
  mtlk_log_buf_entry_t *ready_buf = NULL;

  MTLK_ASSERT(NULL != ppdata);
  MTLK_ASSERT(pkt_size <= buf_size);

  mtlk_osal_lock_acquire(&log_data.data_lock);

  if (!log_data.unsafe_active_buf && !_mtlk_log_unsafe_active_buf_alloc()) {
      goto cs_end;
  }

try_again:
  if (buf_space_left(log_data.unsafe_active_buf) >= pkt_size) {
    buf = log_data.unsafe_active_buf;
    ++buf->refcount;
    *ppdata = _mtlk_log_buf_get_data_buffer(buf) + buf->data_size;
    buf->data_size += pkt_size;
  } else {
    // Not enough space is available to store the packet
    if (log_data.unsafe_active_buf->refcount == 0) {
      ready_buf = log_data.unsafe_active_buf;
    }

    if (_mtlk_log_unsafe_active_buf_alloc()) {
      goto try_again;
    }
  }
cs_end:
  mtlk_osal_lock_release(&log_data.data_lock);

  if (ready_buf != NULL)
    put_ready_buf_and_notify(ready_buf);

  (NULL != buf) ? log_data.log_pkt_reservations_succeeded++
                : log_data.log_pkt_reservations_failed++;

  return buf;
}

void __MTLK_IFUNC
mtlk_log_new_pkt_release(mtlk_log_buf_entry_t *buf)
{
  int buf_is_ready = 0;

  mtlk_osal_lock_acquire(&log_data.data_lock);
  /* cs */ ASSERT(buf->refcount >= 1);
  /* cs */ --buf->refcount;
  /* cs */ if (buf->refcount == 0) {
  /* cs */   if (buf != log_data.unsafe_active_buf) {
  /* cs */     // No one can obtain references to this buffer anymore,
  /* cs */     // we were the only owner: buf can be moved to ready pool
  /* cs */     buf_is_ready = 1;
  /* cs */   } else if (buf->expired) {
  /* cs */     // Timer marked this buffer as expired and no one holds
  /* cs */     // references to it except us: move buf to ready pool
  /* cs */     log_data.unsafe_active_buf = NULL;
  /* cs */     buf_is_ready = 1;
  /* cs */   }
  /* cs */ }
  mtlk_osal_lock_release(&log_data.data_lock);

  if (buf_is_ready)
    put_ready_buf_and_notify(buf);
}

// ---------------
// Local functions
// ---------------

static inline uint32
buf_space_left(mtlk_log_buf_entry_t *buf)
{
  return buf_size - buf->data_size;
}

static void
put_ready_buf_and_notify(mtlk_log_buf_entry_t *buf)
{
  int rslt;

  mtlk_log_put_ready_buf(buf);
  rslt = mtlk_log_on_buf_ready();
  if (rslt != MTLK_ERR_OK) {
    mtlk_osal_emergency_print("ERROR: Failed to initiate ready logger buffer processing");
  }

  // TODO: reschedule the timer here or it can trigger too soon
}

static uint32 __MTLK_IFUNC
log_timer(mtlk_osal_timer_t *timer, mtlk_handle_t clb_usr_data)
{
  mtlk_log_buf_entry_t *ready_buf = NULL;

  mtlk_osal_lock_acquire(&log_data.data_lock);
  /* cs */ if (!log_data.unsafe_active_buf)
  /* cs */   goto cs_end; // Error already reported, just silently ignore
  /* cs */
  /* cs */ // ASSERT if expired buffer wasn't processed by it's last owner
  /* cs */ ASSERT( ! ( log_data.unsafe_active_buf->refcount == 0 &&
  /* cs */             log_data.unsafe_active_buf->expired) );
  /* cs */
  /* cs */ if (log_data.unsafe_active_buf->refcount == 0) {
  /* cs */   if (log_data.unsafe_active_buf->data_size == 0) {
  /* cs */     // The buffer is empty and no one is writing to it now,
  /* cs */     // don't touch it
  /* cs */     goto cs_end;
  /* cs */   }
  /* cs */   ready_buf = log_data.unsafe_active_buf;
  /* cs */   log_data.unsafe_active_buf = mtlk_log_get_free_buf();
  /* cs */   if (!log_data.unsafe_active_buf) {
  /* cs */     goto cs_end;
  /* cs */   }
  /* cs */ } else {
  /* cs */   // Someone is holding a lock on active buffer: schedule the buffer
  /* cs */   // for moving to ready pool after refcount reaches zero
  /* cs */   // (mtlk_log_new_pkt_release function will do this).
  /* cs */   log_data.unsafe_active_buf->expired = 1;
  /* cs */ }
cs_end:
  mtlk_osal_lock_release(&log_data.data_lock);

  if (ready_buf != NULL)
    put_ready_buf_and_notify(ready_buf);

  return (uint32)buf_swap_tmout;
}

static void
_mtlk_log_detach_active_buff(mtlk_log_data_t* plog_data)
{
  mtlk_log_buf_entry_t *detached_buf = NULL;

  mtlk_osal_lock_acquire(&plog_data->data_lock);
  /* cs */ if (!plog_data->unsafe_active_buf)
  /* cs */   goto cs_end;
  /* cs */
  /* cs */ // ASSERT if expired buffer wasn't processed by it's last owner
  /* cs */ ASSERT( ! ( plog_data->unsafe_active_buf->refcount == 0 &&
  /* cs */             plog_data->unsafe_active_buf->expired) );
  /* cs */
  /* cs */ if (plog_data->unsafe_active_buf->refcount == 0) {
  /* cs */   detached_buf = plog_data->unsafe_active_buf;
  /* cs */   plog_data->unsafe_active_buf = NULL;
  /* cs */ } else {
  /* cs */   // Someone is holding a lock on active buffer: schedule the buffer
  /* cs */   // for moving to ready pool after refcount reaches zero
  /* cs */   // (mtlk_log_new_pkt_release function will do this).
  /* cs */   plog_data->unsafe_active_buf->expired = 1;
  /* cs */ }
cs_end:
  mtlk_osal_lock_release(&plog_data->data_lock);

  if (NULL != detached_buf) {
    (detached_buf->data_size > 0) ? put_ready_buf_and_notify(detached_buf)
                                  : mtlk_log_put_free_buf(detached_buf);
  }
}

