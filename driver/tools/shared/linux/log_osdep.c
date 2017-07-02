#include "mtlkinc.h"

/* TODO: This is temporary implementation that will be replaced by */
/*       full RTLOGGER integration with applications               */

int mtlk_log_get_flags(int level, int oid, int gid)
{
  MTLK_UNREFERENCED_PARAM(oid);
  MTLK_UNREFERENCED_PARAM(gid);
  return (level <= RTLOG_DLEVEL_VAR) ? LOG_TARGET_CONSOLE : 0;
}
