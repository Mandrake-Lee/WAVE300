#ifndef __MTLK_HASH_IEEE_ADDR_H__
#define __MTLK_HASH_IEEE_ADDR_H__

#include "mtlkhash.h"
#include "mhi_ieee_address.h"

#define  MTLK_IDEFS_ON
#include "mtlkidefs.h"

/* Use MTLK_HASH_ENTRY_T(ieee_addr) for hash entry type */

MTLK_HASH_DECLARE_ENTRY_T(ieee_addr, IEEE_ADDR);

MTLK_HASH_DECLARE_EXTERN(ieee_addr, IEEE_ADDR);

#define  MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif
