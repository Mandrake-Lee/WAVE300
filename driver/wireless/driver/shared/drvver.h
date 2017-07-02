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

#ifdef MTCFG_LINDRV_HW_PCIG3
# define MTLK_PCIG3 ".PciG3"
#else
# define MTLK_PCIG3
#endif

#ifdef MTCFG_LINDRV_HW_PCIE
# define MTLK_PCIEG3 ".PcieG3"
#else
# define MTLK_PCIEG3
#endif

#ifdef MTCFG_LINDRV_HW_AHBG35
# define MTLK_AHBG35 ".AhbG35"
#else
# define MTLK_AHBG35
#endif

#define MTLK_PLATFORMS  MTLK_PCIG3 MTLK_PCIEG3 MTLK_AHBG35

#define DRV_NAME        "mtlk"
#define DRV_VERSION     MTLK_SOURCE_VERSION \
                        MTLK_PLATFORMS DRV_COMPILATION_TYPE
#define DRV_DESCRIPTION "Metalink 802.11n WiFi Network Driver"

#endif /* !__DRV_VER_H__ */

