#ifndef __UTILS_H__
#define __UTILS_H__

#include <sys/time.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)  (sizeof(a)/sizeof((a)[0]))
#endif

typedef enum _mtlk_osdep_level {
  MTLK_OSLOG_ERR,
  MTLK_OSLOG_WARN,
  MTLK_OSLOG_INFO
} mtlk_osdep_level_e;

int  __MTLK_IFUNC _mtlk_osdep_log_init(const char *app_name);
void __MTLK_IFUNC _mtlk_osdep_log_cleanup(void);
int  __MTLK_IFUNC _mtlk_osdep_log_enable_stderr(mtlk_osdep_level_e level,
                                                BOOL               value);
void __MTLK_IFUNC _mtlk_osdep_log(mtlk_osdep_level_e level,
                                  const char        *func,
                                  int                line,
                                  const char        *fmt,
                                  ...);

static inline unsigned long
timestamp(void)
{
  struct timeval ts;
  if (0 != gettimeofday(&ts, NULL))
    return 0;
  return ts.tv_usec + (ts.tv_sec * 1000000);
}

int __MTLK_IFUNC
mtlk_get_current_executable_name(char* buf, size_t size);

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#endif /* __UTILS_H__ */

