/*
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 * Written by: Andriy Fidrya
 *
 */
#ifndef __LOG_OSDEP_H__
#define __LOG_OSDEP_H__

#ifndef MTCFG_SILENT
uint8 __MTLK_IFUNC log_osdep_get_level (uint8 gid);
void __MTLK_IFUNC log_osdep_set_level (uint8 gid, uint8 level);
void __MTLK_IFUNC log_osdep_reset_levels (uint8 level);
void __MTLK_IFUNC log_osdep_init(void);
void __MTLK_IFUNC log_osdep_cleanup(void);
void __MTLK_IFUNC log_osdep_do(const char *fname, 
                               int         line_no, 
                               const char *level, 
                               const char *fmt, ...);
#else
static __INLINE uint8 log_osdep_get_level (uint8 gid) 
{ 
  MTLK_UNREFERENCED_PARAM(gid); 
  return 0;
}
static __INLINE void log_osdep_set_level (uint8 gid, uint8 level)
{
  MTLK_UNREFERENCED_PARAM(gid);
  MTLK_UNREFERENCED_PARAM(level);
}
static __INLINE void log_osdep_reset_levels (uint8 level)
{
  MTLK_UNREFERENCED_PARAM(level);
}
#define log_osdep_init()
#define log_osdep_cleanup()
#define log_osdep_do(...)
#endif

#define CLOG(fname, line_no, log_level, fmt, ...)     \
  log_osdep_do(fname, line_no, #log_level, fmt, ## __VA_ARGS__)
#define CINFO(fname, line_no, fmt, ...)                         \
  log_osdep_do("", 0, "", fmt, ## __VA_ARGS__)
#define CERROR(fname, line_no, fmt, ...)                        \
  log_osdep_do(fname, line_no, KERN_ERR, fmt, ## __VA_ARGS__)
#define CWARNING(fname, line_no, fmt, ...)                      \
  log_osdep_do(fname, line_no, KERN_WARNING, fmt, ## __VA_ARGS__)

#ifndef RTLOG_FLAGS
#define RTLOG_FLAGS (RTLF_REMOTE_ENABLED | RTLF_CONSOLE_ENABLED)
#endif

#ifndef RTLOG_MAX_DLEVEL
#if defined(MTCFG_SILENT)
#define RTLOG_MAX_DLEVEL (RTLOG_ERROR_DLEVEL - 1)
#elif defined(MTCFG_MAX_DLEVEL)
#define RTLOG_MAX_DLEVEL MTCFG_MAX_DLEVEL
#else
#error Wrong configuration settings for Log level!
#endif
#endif

#if defined (MTCFG_RT_LOGGER_INLINES)
/* Console & Remote - use inlines */
#define __MTLK_FLOG static __INLINE
#elif defined(MTCFG_RT_LOGGER_FUNCTIONS)
/* Console & Remote - use function calls */
#define __MTLK_FLOG
#else
#error Wrong RTLOGGER configuration!
#endif

#define mtlk_log_get_timestamp() mtlk_osal_timestamp()

#include "mtlklist.h"

#define MTLK_IDEFS_ON
#include "mtlkidefs.h"

struct _mtlk_log_buf_entry_t
{
  mtlk_ldlist_entry_t entry; // List entry data
  uint32 refcount;           // Reference counter
  uint32 data_size;          // Data size in buffer
  uint32 expired;            // Buffer swap timer expiration flag
} __MTLK_IDATA;

static __INLINE void*
_mtlk_log_buf_get_data_buffer(struct _mtlk_log_buf_entry_t* log_buffer)
{
  return (void*)(log_buffer + 1);
}

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#define LOG_CONSOLE_TEXT_INFO __FUNCTION__

#include "log.h"

#endif /* __LOG_OSDEP_H__ */
