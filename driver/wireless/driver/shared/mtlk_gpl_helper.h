
#ifndef __MTLK_GPL_HELPER_H__
#define __MTLK_GPL_HELPER_H__

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

uint32 __MTLK_IFUNC mtlk_get_umi_man_size(void);
uint32 __MTLK_IFUNC mtlk_get_umi_dbg_size(void);
uint32 __MTLK_IFUNC mtlk_get_umi_activate_size(void);
uint32 __MTLK_IFUNC mtlk_get_umi_mbss_pre_activate_size(void);

uint32 __MTLK_IFUNC mtlk_get_umi_scan_size(void);

UMI_ACTIVATE_HDR* __MTLK_IFUNC mtlk_get_umi_activate_hdr(void *data);

UMI_SCAN_HDR* __MTLK_IFUNC mtlk_get_umi_scan_hdr(void *data);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_GPL_HELPER_H__ */
