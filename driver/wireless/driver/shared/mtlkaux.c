/*
 * $Id:
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Shared Auxiliary routines
 *
 */
#include "mtlkinc.h"

#include "mtlkaux.h"
#include "eeprom.h"

#define LOG_LOCAL_GID   GID_AUX
#define LOG_LOCAL_FID   1

static const uint32 OperateRateSet[] = {
  0x00007f00, /* 11b@2.4GHz */
  0x00007fff, /* 11g@2.4GHz */
  0xffffffff, /* 11n@2.4GHz */
  0x00007fff, /* 11bg@2.4GHz */
  0xffffffff, /* 11gn@2.4GHz */
  0xffffffff, /* 11bgn@2.4GHz */
  0x000000ff, /* 11a@5.2GHz */
  0xffff80ff, /* 11n@5.2GHz */
  0xffff80ff, /* 11an@5.2GHz */

  /* these ARE NOT real network modes. they are for
     unconfigured dual-band STA which may be NETWORK_11ABG_MIXED
     or NETWORK_11ABGN_MIXED */
  0x00007fff, /* 11abg */
  0xffffffff, /* 11abgn */
};

/*
 * There are three available BSSBasicRateSet modes:
 * CFG_BASIC_RATE_SET_DEFAULT:
 *   BSSBasicRateSet according to standard requirements:
 *   for 2.4GHz is 1, 2, 5.5 and 11 mbps,
 *   for 5.2GHz is 6, 12 and 24 mbps.
 * CFG_BASIC_RATE_SET_EXTRA:
 *   Make BSSBasicRateSet equal to OperateRateSet & ~11nRateSet.
 *   E.g. only STA which can work on all rates, which we support, can connect.
 * CFG_BASIC_RATE_SET_LEGACY:
 *   For 2.4GHz band only, includes 1 and 2 mbps,
 *   used for compatibility with old STAs, which supports only those two rates.
 */
static const uint32 BSSBasicRateSet[NUM_OF_NETWORK_MODES][NUM_OF_BASIC_RATE_SET_MODES] = {
  { 0x00007800, 0x00007f00, 0x00001800}, /* 11b@2.4GHz */
  { 0x00007800, 0x00007fff, 0x00001800}, /* 11g@2.4GHz */
  { 0x00007800, 0x00007fff, 0x00001800}, /* 11n@2.4GHz */
  { 0x00001800, 0x00007fff, 0x00001800}, /* 11bg@2.4GHz */
  { 0x00007800, 0x00007fff, 0x00001800}, /* 11gn@2.4GHz */
  { 0x00007800, 0x00007fff, 0x00001800}, /* 11bgn@2.4GHz */
  { 0x00000015, 0x000000ff, 0xfeedbeef}, /* 11a@5.2GHz */
  { 0x00000015, 0x000000ff, 0xfeedbeef}, /* 11n@5.2GHz */
  { 0x00000015, 0x000000ff, 0xfeedbeef}, /* 11an@5.2GHz */
};

uint32 __MTLK_IFUNC
get_operate_rate_set (uint8 net_mode)
{
  ASSERT(net_mode < NETWORK_NONE);
  return OperateRateSet[net_mode];
}

uint32 __MTLK_IFUNC
get_basic_rate_set (uint8 net_mode, uint8 mode)
{
  ASSERT(net_mode < NUM_OF_NETWORK_MODES);
  ASSERT(mode < NUM_OF_BASIC_RATE_SET_MODES);
  return BSSBasicRateSet[net_mode][mode];
}

/*
 * It should be noted, that
 * get_net_mode(net_mode_to_band(net_mode), is_ht_net_mode(net_mode)) != net_mode
 * because there are {ht, !ht}x{2.4GHz, 5.2GHz, both} == 6 combinations,
 * and there are 11 Network Modes.
 */
uint8 __MTLK_IFUNC get_net_mode (uint8 band, uint8 is_ht)
{
  switch (band) {
  case MTLK_HW_BAND_2_4_GHZ:
    if (is_ht)
      return NETWORK_11BGN_MIXED;
    else
      return NETWORK_11BG_MIXED;
  case MTLK_HW_BAND_5_2_GHZ:
    if (is_ht)
      return NETWORK_11AN_MIXED;
    else
      return NETWORK_11A_ONLY;
  case MTLK_HW_BAND_BOTH:
    if (is_ht)
      return NETWORK_11ABGN_MIXED;
    else
      return NETWORK_11ABG_MIXED;
  default:
    break;
  }

  ASSERT(0);

  return 0; /* just fake cc */
}

uint8 __MTLK_IFUNC net_mode_to_band (uint8 net_mode)
{
  switch (net_mode) {
  case NETWORK_11BG_MIXED: /* walk through */
  case NETWORK_11BGN_MIXED: /* walk through */
  case NETWORK_11B_ONLY: /* walk through */
  case NETWORK_11GN_MIXED: /* walk through */
  case NETWORK_11G_ONLY: /* walk through */
  case NETWORK_11N_2_4_ONLY:
    return MTLK_HW_BAND_2_4_GHZ;
  case NETWORK_11AN_MIXED: /* walk through */
  case NETWORK_11A_ONLY: /* walk through */
  case NETWORK_11N_5_ONLY:
    return MTLK_HW_BAND_5_2_GHZ;
  case NETWORK_11ABG_MIXED: /* walk through */
  case NETWORK_11ABGN_MIXED:
    return MTLK_HW_BAND_BOTH;
  default:
    break;
  }

  ASSERT(0);

  return 0; /* just fake cc */
}

BOOL __MTLK_IFUNC is_ht_net_mode (uint8 net_mode)
{
  switch (net_mode) {
  case NETWORK_11ABG_MIXED: /* walk through */
  case NETWORK_11A_ONLY: /* walk through */
  case NETWORK_11BG_MIXED: /* walk through */
  case NETWORK_11B_ONLY: /* walk through */
  case NETWORK_11G_ONLY:
    return FALSE;
  case NETWORK_11ABGN_MIXED: /* walk through */
  case NETWORK_11AN_MIXED: /* walk through */
  case NETWORK_11BGN_MIXED: /* walk through */
  case NETWORK_11GN_MIXED: /* walk through */
  case NETWORK_11N_2_4_ONLY: /* walk through */
  case NETWORK_11N_5_ONLY:
    return TRUE;
  default:
    break;
  }

  ASSERT(0);

  return FALSE; /* just fake cc */
}

BOOL __MTLK_IFUNC is_mixed_net_mode (uint8 net_mode)
{
  switch (net_mode) {
  case NETWORK_11ABG_MIXED:
  case NETWORK_11BG_MIXED:
  case NETWORK_11ABGN_MIXED:
  case NETWORK_11AN_MIXED:
  case NETWORK_11BGN_MIXED:
  case NETWORK_11GN_MIXED:
    return TRUE;
  case NETWORK_11B_ONLY:
  case NETWORK_11G_ONLY:
  case NETWORK_11A_ONLY:
  case NETWORK_11N_2_4_ONLY:
  case NETWORK_11N_5_ONLY:
    return FALSE;
  default:
    break;
  }

  ASSERT(0);

  return FALSE; /* just fake cc */
}

const char * __MTLK_IFUNC
net_mode_to_string (uint8 net_mode)
{ 
  switch (net_mode) {
  case NETWORK_11B_ONLY:
    return "802.11b";
  case NETWORK_11G_ONLY:
    return "802.11g";
  case NETWORK_11N_2_4_ONLY:
    return "802.11n(2.4)";
  case NETWORK_11BG_MIXED:
    return "802.11bg";
  case NETWORK_11GN_MIXED:
    return "802.11gn";
  case NETWORK_11BGN_MIXED:
    return "802.11bgn"; 
  case NETWORK_11A_ONLY:
    return "802.11a";
  case NETWORK_11N_5_ONLY:
    return "802.11n(5.2)";
  case NETWORK_11AN_MIXED:
    return "802.11an";
  case NETWORK_11ABG_MIXED:
    return "802.11abg";
  case NETWORK_11ABGN_MIXED:
    return "802.11abgn";
  }

  return "invalid mode";
}

