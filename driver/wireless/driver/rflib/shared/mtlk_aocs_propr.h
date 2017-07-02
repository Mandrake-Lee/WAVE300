/*
* $Id: $
*
* Copyright (c) 2006-2007 Metalink Broadband (Israel)
*
* Proprietary includes for library only!
*
*/

#ifndef _MTLK_AOCS_PROPR_H_
#define _MTLK_AOCS_PROPR_H_

#include "mtlkqos.h"

#define  MTLK_IDEFS_ON
#include "mtlkidefs.h"

/* AOCS related data */
/* on channel switch status */
typedef struct _mtlk_aocs_evt_switch_t {
    int status;
    uint16 sq_used[NTS_PRIORITIES];
} __MTLK_IDATA mtlk_aocs_evt_switch_t;

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* _MTLK_AOCS_PROPR_H_ */
