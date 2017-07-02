/*
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 * Written by: Andriy Fidrya
 *
 */
#ifndef __LOG_OSDEP_H__
#define __LOG_OSDEP_H__

#define LOG_CONSOLE_TEXT_INFO __FUNCTION__

uint8 __MTLK_IFUNC log_osdep_get_level (uint8 gid);
void __MTLK_IFUNC log_osdep_set_level (uint8 gid, uint8 level);
void __MTLK_IFUNC log_osdep_reset_levels (uint8 level);
void __MTLK_IFUNC log_osdep_init(void);
void __MTLK_IFUNC log_osdep_cleanup(void);
void __MTLK_IFUNC log_osdep_do(const char *fname, 
                               int         line_no, 
                               const char *level, 
                               const char *fmt, ...);

#define CLOG(fname, line_no, log_level, fmt, ...)                               \
  _mtlk_osdep_log(MTLK_OSLOG_INFO, fname, line_no, (fmt), ## __VA_ARGS__)
#define CINFO(fname, line_no, fmt, ...)                                         \
  _mtlk_osdep_log(MTLK_OSLOG_INFO, fname, line_no, (fmt), ## __VA_ARGS__)
#define CERROR(fname, line_no, fmt, ...)                                        \
  _mtlk_osdep_log(MTLK_OSLOG_ERR, fname, line_no, (fmt), ## __VA_ARGS__)
#define CWARNING(fname, line_no, fmt, ...)                                      \
  _mtlk_osdep_log(MTLK_OSLOG_WARN, fname, line_no, (fmt), ## __VA_ARGS__)

#define RTLOG_FLAGS (RTLF_CONSOLE_ENABLED)

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

#ifndef RTLOG_DLEVEL_VAR
#define RTLOG_DLEVEL_VAR debug
#endif
extern int RTLOG_DLEVEL_VAR;

#define mtlk_log_get_timestamp() mtlk_osal_timestamp()

#include "log.h"

#endif /* __LOG_OSDEP_H__ */

