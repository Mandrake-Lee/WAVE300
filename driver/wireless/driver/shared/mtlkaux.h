/*
 * $Id:
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Shared Auxiliary routines
 *
 */

#ifndef _MTLK_AUX_H
#define _MTLK_AUX_H

#include "mhi_umi.h"
#include "mhi_mib_id.h"
/*
 * Driver-only network mode.
 * Should be used on STA for dual-band scan
 */
#define NETWORK_11ABG_MIXED  NUM_OF_NETWORK_MODES
#define NETWORK_11ABGN_MIXED (NUM_OF_NETWORK_MODES + 1)
#define NETWORK_NONE (NUM_OF_NETWORK_MODES + 2)

#define CFG_BASIC_RATE_SET_DEFAULT  0
#define CFG_BASIC_RATE_SET_EXTRA    1
#define CFG_BASIC_RATE_SET_LEGACY   2
#define NUM_OF_BASIC_RATE_SET_MODES 3

#define LM_PHY_11A_RATE_MSK             0x000000FF
#define LM_PHY_11B_RATE_MSK             0x00007F00
#define LM_PHY_11B_SHORT_RATE_MSK       0x00000700
#define LM_PHY_11B_LONG_RATE_MSK        (LM_PHY_11B_RATE_MSK & (~LM_PHY_11B_SHORT_RATE_MSK))
#define LM_PHY_11N_RATE_MSK             0xFFFF8000
#define LM_PHY_11N_MIMO_RATE_MSK        0x7F800000
#define LM_PHY_11N_NO_MIMO_RATE_MSK     0x007F8000
#define LM_PHY_11G_RATE_MSK             (LM_PHY_11A_RATE_MSK|LM_PHY_11B_RATE_MSK)

#define LM_PHY_11N_RATE_6_5      15// ,  11N    15     0x40
#define LM_PHY_11N_RATE_6_DUP    31//    11N    31     0x50

static __INLINE int
mtlk_aux_is_11n_rate (uint8 rate)
{
  return (rate >= LM_PHY_11N_RATE_6_5 &&
          rate <= LM_PHY_11N_RATE_6_DUP);
}

uint32 __MTLK_IFUNC get_operate_rate_set (uint8 net_mode);
uint32 __MTLK_IFUNC get_basic_rate_set (uint8 net_mode, uint8 mode);

uint8 __MTLK_IFUNC get_net_mode (uint8 band, uint8 is_ht);
uint8 __MTLK_IFUNC net_mode_to_band (uint8 net_mode);
BOOL __MTLK_IFUNC is_ht_net_mode (uint8 net_mode);
BOOL __MTLK_IFUNC is_mixed_net_mode (uint8 net_mode);
const char * __MTLK_IFUNC net_mode_to_string (uint8 net_mode);


#endif // _MTLK_AUX_H

