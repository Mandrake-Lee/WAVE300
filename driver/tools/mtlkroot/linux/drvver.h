#ifndef __DRV_VER_H__
#define __DRV_VER_H__

#ifdef MTCFG_DEBUG
#define DRV_COMPILATION_TYPE ".Debug"
#else
#  ifdef MTCFG_SILENT
#    define DRV_COMPILATION_TYPE ".Silent"
#  else
#    define DRV_COMPILATION_TYPE ".Release"
#  endif
#endif

#define DRV_NAME        "mtlklog"
#define DRV_VERSION     MTLK_SOURCE_VERSION DRV_COMPILATION_TYPE
#define DRV_DESCRIPTION "Metalink Logger Driver"

#endif /* !__DRV_VER_H__ */
