#include "mtlkinc.h"
#include "mtlkerr.h"

#include <stdarg.h>
#include <syslog.h>

#define LOG_LOCAL_GID   GID_UTILS
#define LOG_LOCAL_FID   0

int debug = 0;

#define MAX_PRINT_BUFF_SIZE 512

#define MTLK_PRINT_USE_SYSLOG

int  __MTLK_IFUNC
_mtlk_osdep_log_init (const char *app_name)
{
#ifdef MTLK_PRINT_USE_SYSLOG
  openlog(app_name, LOG_PID, LOG_USER);
#endif
  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
_mtlk_osdep_log_cleanup (void)
{
#ifdef MTLK_PRINT_USE_SYSLOG
  closelog();
#endif
}

struct mtlk_osdep_level_info
{
  const char *prefix;
  int         syslog_priority;
  int         duplicate_to_stderr;
};

static struct mtlk_osdep_level_info
osdep_level_info[] = {
  {
    .prefix              = "ERROR: ",
    .syslog_priority     = LOG_ERR,
    .duplicate_to_stderr = 0
  },
  {
    .prefix              = "WARNING: ",
    .syslog_priority     = LOG_WARNING,
    .duplicate_to_stderr = 0
  },
  {
    .prefix              = "",
    .syslog_priority     = LOG_USER,
    .duplicate_to_stderr = 0
  },
};

int __MTLK_IFUNC 
_mtlk_osdep_log_enable_stderr (mtlk_osdep_level_e level,
                               BOOL               enable)
{
  if (level < ARRAY_SIZE(osdep_level_info)) {
    osdep_level_info[level].duplicate_to_stderr = (enable != FALSE);
    return MTLK_ERR_OK;
  }

  return MTLK_ERR_PARAMS;
}

void __MTLK_IFUNC 
_mtlk_osdep_log (mtlk_osdep_level_e level,
                 const char        *func,
                 int                line,
                 const char         *fmt,
                 ...)
{
  const struct mtlk_osdep_level_info *linfo;
  char    buff[MAX_PRINT_BUFF_SIZE] = {0};
  int     n = 0, offs = 0;
  va_list ap;

  MTLK_ASSERT(level < ARRAY_SIZE(osdep_level_info));

  linfo = &osdep_level_info[level];

  n = snprintf(buff + offs, sizeof(buff) - offs, 
               "[%010lu] [%s:%d]: %s", 
               timestamp(), func, line, linfo->prefix);
  if (n >= sizeof(buff) - offs) {
    goto end;
  }
  offs += n;

  va_start(ap, fmt);
  vsnprintf(buff + offs, sizeof(buff) - offs, 
            fmt,
            ap);
  va_end(ap);

end:
  buff[sizeof(buff) - 1] = 0;

#ifdef MTLK_PRINT_USE_SYSLOG
  syslog(linfo->syslog_priority, "%s", buff);
  if (linfo->duplicate_to_stderr) {
    fprintf(stderr, "%s\n", buff);
  }
#else
  fprintf(stderr, "%s\n", buff);
#endif
}

int __MTLK_IFUNC
mtlk_get_current_executable_name(char* buf, size_t size)
{
  char linkname[64]; /* /proc/<pid>/exe */
  pid_t pid;
  int ret;

  /* Get our PID and build the name of the link in /proc */
  pid = getpid();

  if (snprintf(linkname, sizeof(linkname), "/proc/%i/exe", pid) < 0)
  {
    /* This should only happen on large word systems. I'm not sure
       what the proper response is here.
       It really is an assert-like condition. */
    MTLK_ASSERT(FALSE);
  }

  /* Now read the symbolic link */
  ret = readlink(linkname, buf, size);

  /* In case of an error, leave the handling up to the caller */
  if (ret == -1)
    return MTLK_ERR_UNKNOWN;

  /* Report insufficient buffer size */
  if (ret >= size)
  {
    return MTLK_ERR_BUF_TOO_SMALL;
  }

  /* Ensure proper NUL termination */
  buf[ret] = 0;

  return MTLK_ERR_OK;
}
