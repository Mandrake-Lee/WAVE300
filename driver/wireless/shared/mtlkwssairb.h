#ifndef __MTLK_WSSA_IRB_H__
#define __MTLK_WSSA_IRB_H__

#include "mtlkguid.h"
#include "mtlkwssa.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/*******************************************************************************
 * Get WSSA data (APP => DRV only)
 *******************************************************************************/
// {8D1CA40E-0169-48BF-8E21-90C8475C330B}
#define MTLK_IRB_GUID_WSSA_REQ_INFO \
    MTLK_DECLARE_GUID(0x8d1ca40e, 0x169, 0x48bf, 0x8e, 0x21, 0x90, 0xc8, 0x47, 0x5c, 0x33, 0xb);

/*******************************************************************************
 * Send WSSA event (DRV => APP only)
 *******************************************************************************/
// {43553F9C-07D8-4892-8A0B-ED170DC68339}
#define MTLK_IRB_GUID_WSSA_SEND_EVENT \
    MTLK_DECLARE_GUID(0x43553f9c, 0x7d8, 0x4892, 0x8a, 0xb, 0xed, 0x17, 0xd, 0xc6, 0x83, 0x39);

typedef struct _mtlk_wssa_info_hdr_t
{
  mtlk_wss_data_source_t  info_source;
  uint32                  info_id;
  uint32                  processing_result;
} __MTLK_IDATA mtlk_wssa_info_hdr_t;

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_WSSA_IRB_H__ */
