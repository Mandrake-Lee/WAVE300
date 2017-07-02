#include "mtlkinc.h"
#include "bitrate.h"
#include "mtlk_coreui.h"
#include "mhi_ieee_address.h"
#include "mhi_mib_id.h"

#define LOG_LOCAL_GID   GID_BITRATE
#define LOG_LOCAL_FID   0

/*****************************************************************************
**
** NAME         mtlk_bitrate_get_value
**
** PARAMETERS   index  requested bitrate index
**              sm     SpectrumMode
**              scp    ShortCyclicPrefix
**
** RETURNS      Value of bitrate in bits/second
**
** DESCRIPTION  Function returns value of bitrate 
**              depend on current HW parameters and bitrate index
**
******************************************************************************/

/*
 * Table below contain all available bitrates.
 * Values representation is fixed point, i.e. 60 means 6.0 Mbit/s.
 *
 * Index in table composed of several parameters:
 * bits | param
 * ======================
 *    0 | ShortCyclicPrefix
 *    1 | SpectrumMode
 *  2-6 | bitrate index
 *
 *  see get_bitrate_value() for details
 */
static const short int bitrates[] = {
/*************** 802.11a rates */
    60,   60,   60,   60, /*  0 */
    90,   90,   90,   90, /*  1 */
   120,  120,  120,  120, /*  2 */
   180,  180,  180,  180, /*  3 */
   240,  240,  240,  240, /*  4 */
   360,  360,  360,  360, /*  5 */
   480,  480,  480,  480, /*  6 */
   540,  540,  540,  540, /*  7 */
/*************** 802.11bg rates */
    20,   20,   20,   20, /*  8 */
    55,   55,   55,   55, /*  9 */
   110,  110,  110,  110, /* 10 */
    10,   10,   10,   10, /* 11 */
    20,   20,   20,   20, /* 12 */
    55,   55,   55,   55, /* 13 */
   110,  110,  110,  110, /* 14 */
/*************** 802.11n rates */
    65,   72,  135,  150, /* 15 */
   130,  144,  270,  300, /* 16 */
   195,  217,  405,  450, /* 17 */
   260,  289,  540,  600, /* 18 */
   390,  433,  810,  900, /* 19 */
   520,  578, 1080, 1200, /* 20 */
   585,  650, 1215, 1350, /* 21 */
   650,  722, 1350, 1500, /* 22 */
   130,  144,  270,  300, /* 23 */
   260,  289,  540,  600, /* 24 */
   390,  433,  810,  900, /* 25 */
   520,  578, 1080, 1200, /* 26 */
   780,  867, 1620, 1800, /* 27 */
  1040, 1156, 2160, 2400, /* 28 */
  1170, 1300, 2430, 2700, /* 29 */
  1300, 1444, 2700, 3000, /* 30 */
  1300, 1444, 2700, 3000, /* 31 */
};

int __MTLK_IFUNC
mtlk_bitrate_get_value (int index, int sm, int scp)
{
  int i; /* index in table */
  int value;

  ASSERT((BITRATE_FIRST <= index) && (index <= BITRATE_LAST));
  ASSERT((0 == sm) || (1 == sm));
  ASSERT((0 == scp) || (1 == scp));

  i = index << 2;
  i |= sm   << 1;
  i |= scp  << 0;

  /* In table values in fixed-point representation: 60 -> 6.0 Mbit/s */
  value = 100*1000*bitrates[i];

  return value;
}

int __MTLK_IFUNC
mtlk_bitrate_rates_to_idx(int int_rate,
                          int frac_rate,
                          int spectrum_mode,
                          int short_cyclic_prefix,
                          uint16 *index)
{
  /*
   * Check whether requested value is bare index or mpbs.
   * We can distinguish with sscanf() return value (number of tokens read)
   */
  if ((MTLK_CORE_BITRATE_AUTO == int_rate) &&
      (MTLK_CORE_BITRATE_AUTO == frac_rate)) {
    *index = NO_RATE;
    return MTLK_ERR_OK;
  }
  else {
    int i;
    /*
     * Try to convert mbps value into rate index.
     * Resulting index should fall into rate_set.
     * If failed - forced_rate should be NO_RATE.
     */
    for (i = BITRATE_FIRST; i <= BITRATE_LAST; i++)
      if (((1000000*int_rate + 100000*frac_rate) ==
          mtlk_bitrate_get_value(i, spectrum_mode, short_cyclic_prefix))) {
        *index = i;
        return MTLK_ERR_OK;
      }
  }

  return MTLK_ERR_PARAMS;
}

int __MTLK_IFUNC
mtlk_bitrate_idx_to_rates(uint16 index,
                          int spectrum_mode,
                          int short_cyclic_prefix,
                          int *int_rate,
                          int *frac_rate)
{
  if (index == NO_RATE) {
    *int_rate = MTLK_CORE_BITRATE_AUTO;
    *frac_rate = MTLK_CORE_BITRATE_AUTO;
  } 
  else if (index <= BITRATE_LAST) {
    uint32 bps = mtlk_bitrate_get_value(index, spectrum_mode, short_cyclic_prefix);

    /* bps should be converted into `xxx.x mbps' notation, i.e 13500000 -> 13.5 mbps */
    *int_rate = bps/(1000000);
    *frac_rate = (bps%(1000000))/100000;
  }
  else {
    return MTLK_ERR_PARAMS;
  }

  return MTLK_ERR_OK;
}

typedef struct  
{
  uint16 rate;
  uint8  tcr2;
  uint8  scp;
} mtlk_hw_rate_info_t;

#define HW_RATE_TCR2(mcs, b, ht, cb)                 \
  (MTLK_BFIELD_VALUE(HW_RX_RATE_CB,   (cb), uint8) | \
   MTLK_BFIELD_VALUE(HW_RX_RATE_HT,   (ht), uint8) | \
   MTLK_BFIELD_VALUE(HW_RX_RATE_B,     (b), uint8) | \
   MTLK_BFIELD_VALUE(HW_RX_RATE_MCS, (mcs), uint8))

#define HW_RATE_ENTRY(rate, mcs, b, ht, cb, scp)     \
 {                                                   \
    (rate),                                          \
    HW_RATE_TCR2((mcs), (b), (ht), (cb)),            \
    (scp)                                            \
 }

/*********************************************************************
 * The following table is taken as is from the RateTable.xls
 * attached to WAVE300_SW-1154.
 *
 * Columns map (_hw_brate_info <=> RateTable.xls):
 *  - 0 - B (Standard Params/Rate)
 *  - 1 - I (HW Params (TCR2)/MCS)
 *  - 2 - J (HW Params (TCR2)/11b-packets)
 *  - 3 - K (HW Params (TCR2)/HT-packets)
 *  - 4 - L (HW Params (TCR2)/Specturm Mode)
 *  - 5 - M (HW Params (TCR0)/SCP)
 *********************************************************************/
static const mtlk_hw_rate_info_t _hw_brate_info[] = 
{
  HW_RATE_ENTRY(   60,  0, 0, 0, 0, 0),
  HW_RATE_ENTRY(   90,  1, 0, 0, 0, 0),
  HW_RATE_ENTRY(  120,  2, 0, 0, 0, 0),
  HW_RATE_ENTRY(  180,  3, 0, 0, 0, 0),
  HW_RATE_ENTRY(  240,  4, 0, 0, 0, 0),
  HW_RATE_ENTRY(  360,  5, 0, 0, 0, 0),
  HW_RATE_ENTRY(  480,  6, 0, 0, 0, 0),
  HW_RATE_ENTRY(  540,  7, 0, 0, 0, 0),
  HW_RATE_ENTRY(   20,  1, 1, 0, 0, 0),
  HW_RATE_ENTRY(   55,  2, 1, 0, 0, 0),
  HW_RATE_ENTRY(  110,  3, 1, 0, 0, 0),
  HW_RATE_ENTRY(   10,  4, 1, 0, 0, 0),
  HW_RATE_ENTRY(   20,  5, 1, 0, 0, 0),
  HW_RATE_ENTRY(   55,  6, 1, 0, 0, 0),
  HW_RATE_ENTRY(  110,  7, 1, 0, 0, 0),
  HW_RATE_ENTRY(   65,  0, 0, 1, 0, 0),
  HW_RATE_ENTRY(  130,  1, 0, 1, 0, 0),
  HW_RATE_ENTRY(  195,  2, 0, 1, 0, 0),
  HW_RATE_ENTRY(  260,  3, 0, 1, 0, 0),
  HW_RATE_ENTRY(  390,  4, 0, 1, 0, 0),
  HW_RATE_ENTRY(  520,  5, 0, 1, 0, 0),
  HW_RATE_ENTRY(  585,  6, 0, 1, 0, 0),
  HW_RATE_ENTRY(  650,  7, 0, 1, 0, 0),
  HW_RATE_ENTRY(  130,  8, 0, 1, 0, 0),
  HW_RATE_ENTRY(  260,  9, 0, 1, 0, 0),
  HW_RATE_ENTRY(  390, 10, 0, 1, 0, 0),
  HW_RATE_ENTRY(  520, 11, 0, 1, 0, 0),
  HW_RATE_ENTRY(  780, 12, 0, 1, 0, 0),
  HW_RATE_ENTRY( 1040, 13, 0, 1, 0, 0),
  HW_RATE_ENTRY( 1170, 14, 0, 1, 0, 0),
  HW_RATE_ENTRY( 1300, 15, 0, 1, 0, 0),
  HW_RATE_ENTRY(   60,  0, 0, 0, 1, 0),
  HW_RATE_ENTRY(   90,  1, 0, 0, 1, 0),
  HW_RATE_ENTRY(  120,  2, 0, 0, 1, 0),
  HW_RATE_ENTRY(  180,  3, 0, 0, 1, 0),
  HW_RATE_ENTRY(  240,  4, 0, 0, 1, 0),
  HW_RATE_ENTRY(  360,  5, 0, 0, 1, 0),
  HW_RATE_ENTRY(  480,  6, 0, 0, 1, 0),
  HW_RATE_ENTRY(  540,  7, 0, 0, 1, 0),
  HW_RATE_ENTRY(   20,  1, 1, 0, 1, 0),
  HW_RATE_ENTRY(   55,  2, 1, 0, 1, 0),
  HW_RATE_ENTRY(  110,  3, 1, 0, 1, 0),
  HW_RATE_ENTRY(   10,  4, 1, 0, 1, 0),
  HW_RATE_ENTRY(   20,  5, 1, 0, 1, 0),
  HW_RATE_ENTRY(   55,  6, 1, 0, 1, 0),
  HW_RATE_ENTRY(  110,  7, 1, 0, 1, 0),
  HW_RATE_ENTRY(  135,  0, 0, 1, 1, 0),
  HW_RATE_ENTRY(  270,  1, 0, 1, 1, 0),
  HW_RATE_ENTRY(  405,  2, 0, 1, 1, 0),
  HW_RATE_ENTRY(  540,  3, 0, 1, 1, 0),
  HW_RATE_ENTRY(  810,  4, 0, 1, 1, 0),
  HW_RATE_ENTRY( 1080,  5, 0, 1, 1, 0),
  HW_RATE_ENTRY( 1215,  6, 0, 1, 1, 0),
  HW_RATE_ENTRY( 1350,  7, 0, 1, 1, 0),
  HW_RATE_ENTRY(  270,  8, 0, 1, 1, 0),
  HW_RATE_ENTRY(  540,  9, 0, 1, 1, 0),
  HW_RATE_ENTRY(  810, 10, 0, 1, 1, 0),
  HW_RATE_ENTRY( 1080, 11, 0, 1, 1, 0),
  HW_RATE_ENTRY( 1620, 12, 0, 1, 1, 0),
  HW_RATE_ENTRY( 2160, 13, 0, 1, 1, 0),
  HW_RATE_ENTRY( 2430, 14, 0, 1, 1, 0),
  HW_RATE_ENTRY( 2700, 15, 0, 1, 1, 0),
  HW_RATE_ENTRY(   60, 16, 0, 1, 1, 0),
  HW_RATE_ENTRY(   72,  0, 0, 1, 0, 1),
  HW_RATE_ENTRY(  144,  1, 0, 1, 0, 1),
  HW_RATE_ENTRY(  217,  2, 0, 1, 0, 1),
  HW_RATE_ENTRY(  289,  3, 0, 1, 0, 1),
  HW_RATE_ENTRY(  433,  4, 0, 1, 0, 1),
  HW_RATE_ENTRY(  578,  5, 0, 1, 0, 1),
  HW_RATE_ENTRY(  650,  6, 0, 1, 0, 1),
  HW_RATE_ENTRY(  722,  7, 0, 1, 0, 1),
  HW_RATE_ENTRY(  144,  8, 0, 1, 0, 1),
  HW_RATE_ENTRY(  289,  9, 0, 1, 0, 1),
  HW_RATE_ENTRY(  433, 10, 0, 1, 0, 1),
  HW_RATE_ENTRY(  578, 11, 0, 1, 0, 1),
  HW_RATE_ENTRY(  867, 12, 0, 1, 0, 1),
  HW_RATE_ENTRY( 1156, 13, 0, 1, 0, 1),
  HW_RATE_ENTRY( 1300, 14, 0, 1, 0, 1),
  HW_RATE_ENTRY( 1444, 15, 0, 1, 0, 1),
  HW_RATE_ENTRY(  150,  0, 0, 1, 1, 1),
  HW_RATE_ENTRY(  300,  1, 0, 1, 1, 1),
  HW_RATE_ENTRY(  450,  2, 0, 1, 1, 1),
  HW_RATE_ENTRY(  600,  3, 0, 1, 1, 1),
  HW_RATE_ENTRY(  900,  4, 0, 1, 1, 1),
  HW_RATE_ENTRY( 1200,  5, 0, 1, 1, 1),
  HW_RATE_ENTRY( 1350,  6, 0, 1, 1, 1),
  HW_RATE_ENTRY( 1500,  7, 0, 1, 1, 1),
  HW_RATE_ENTRY(  300,  8, 0, 1, 1, 1),
  HW_RATE_ENTRY(  600,  9, 0, 1, 1, 1),
  HW_RATE_ENTRY(  900, 10, 0, 1, 1, 1),
  HW_RATE_ENTRY( 1200, 11, 0, 1, 1, 1),
  HW_RATE_ENTRY( 1800, 12, 0, 1, 1, 1),
  HW_RATE_ENTRY( 2400, 13, 0, 1, 1, 1),
  HW_RATE_ENTRY( 2700, 14, 0, 1, 1, 1),
  HW_RATE_ENTRY( 3000, 15, 0, 1, 1, 1),
  HW_RATE_ENTRY(   67, 16, 0, 1, 1, 1)
};

uint16 __MTLK_IFUNC
mtlk_hw_rate_params_to_rate (uint8 hw_param_tcr2, uint8 scp)
{
  uint16 res = MTLK_HW_RATE_INVALID;
  int i;

  for (i = 0; i < ARRAY_SIZE(_hw_brate_info); i++) {
    if (_hw_brate_info[i].scp == scp && _hw_brate_info[i].tcr2 == hw_param_tcr2) {
      res = _hw_brate_info[i].rate;
      break;
    }
  }

  return res;
}
