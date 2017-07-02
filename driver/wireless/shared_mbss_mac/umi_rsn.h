/***************************************************************************
****************************************************************************
**
** COMPONENT:        ENET Upper MAC    SW
**
** MODULE:           $File: //bwp/enet/demo153_sw/develop/src/mac_upper/umi_rsn.h $
**
** VERSION:          $Revision: #1 $
**
** DATED:            $Date: 2007/03/04 $
**
** AUTHOR:           S Genia
**
** DESCRIPTION:      Upper MAC Public Header
**
*****************************************************************************
**
** Copyright (c) Metalink Ltd., 2006 - 2007
**
****************************************************************************/

#ifndef UMI_RSN_INCLUDED_H
#define UMI_RSN_INCLUDED_H

#define   MTLK_PACK_ON
#include "mtlkpack.h"

/* RSN key types */
#define UMI_RSN_PAIRWISE_KEY            0
#define UMI_RSN_GROUP_KEY               1

/* RSN key index options */
#define UMI_RSN_USE_DEFAULT_KEY_INDEX   4
#define UMI_RSN_USE_PER_ASSOCIATION_KEY 5

/* RSN modes */
#define UMI_RSN_MODE_SUPPORTED          1
#define UMI_RSN_MODE_UNSUPPORTED        0

/* RSN 802.1X station role */
#define UMI_RSN_AUTHENTICATOR           0
#define UMI_RSN_SUPPLICANT              1

/* RSN Cipher Suite selectors (as per 802.11i) */
#define UMI_RSN_CIPHER_SUITE_NONE       0
#define UMI_RSN_CIPHER_SUITE_WEP40      1
#define UMI_RSN_CIPHER_SUITE_TKIP       2
#define UMI_RSN_CIPHER_SUITE_WRAP       3
#define UMI_RSN_CIPHER_SUITE_CCMP       4
#define UMI_RSN_CIPHER_SUITE_WEP104     5

/* RSN field sizes */
#define UMI_RSN_IE_IEEE_MAX_LEN         40
#define UMI_RSN_IE_WECA_MAX_LEN         40
#define UMI_RSN_IE_MAX_LEN              (UMI_RSN_IE_IEEE_MAX_LEN + UMI_RSN_IE_WECA_MAX_LEN) /* In Mix Mode, driver sends both IE as one */
#define UMI_RSN_SEQ_NUM_LEN             6
#define UMI_RSN_TK1_LEN                 16
#define UMI_RSN_TK2_LEN                 16

/* RSN security event codes */
#define UMI_RSN_EVENT_TKIP_ICV_ERROR    0
#define UMI_RSN_EVENT_TKIP_MIC_FAILURE  1
#define UMI_RSN_EVENT_TKIP_REPLAY       2
#define UMI_RSN_EVENT_CCMP_MIC_FAILURE  3
#define UMI_RSN_EVENT_CCMP_REPLAY       4
#define UMI_RSN_EVENT_REKEY_PACKETS     5
#define UMI_RSN_EVENT_TEST_BIT(s,b)     ((s) & (1 << (b))
#define UMI_RSN_EVENT_MASK              (0x3F)

#define   MTLK_PACK_OFF
#include "mtlkpack.h"

#endif /* !UMI_RSN_INCLUDED_H */
