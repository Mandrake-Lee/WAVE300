/*
 * $Id: channels.h 12315 2011-12-28 11:08:09Z nayshtut $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Written by: Roman Sikorskyy 
 *
 */

#ifndef __CHANNELS_H__
#define __CHANNELS_H__

#include "mhi_umi.h"
#include "mhi_mib_id.h"
#include "scan.h"
#include "aocs.h"
#include "mtlk_clipboard.h"

#define MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define SCAN_ACTIVE  MIB_ST_ACTIVE
#define SCAN_PASSIVE MIB_ST_PASSIVE

#define MAX_CHANNELS        50

/* Need to be synchronized with ARRAY_SIZE(country_reg_table) */
/* it can't be shared directly */
#define MAX_COUNTRIES       249

#define ALTERNATE_LOWER     1
#define ALTERNATE_UPPER     0
#define ALTERNATE_NONE      (-1)

#define SPECTRUM_20MHZ      MIB_SPECTRUM_20M
#define SPECTRUM_40MHZ      MIB_SPECTRUM_40M
#define SPECTRUM_AUTO       (SPECTRUM_40MHZ+1)

#define REG_LIMITS    0x01
#define HW_LIMITS     0x02
#define ANTENNA_GAIN  0x04
#define ALL_LIMITS    (REG_LIMITS | HW_LIMITS | ANTENNA_GAIN)

#define MTLK_CHNLS_DOT11H_CALLER    1
#define MTLK_CHNLS_SCAN_CALLER      2
#define MTLK_CHNLS_COUNTRY_CALLER   3

#define MTLK_CHNLS_COUNTRY_BUFSIZE   3

struct reg_tx_limit;
struct hw_reg_tx_limit;
struct antenna_gain;
struct _mtlk_hw_cfg_t;
struct _mtlk_ant_cfg_t;
struct _mtlk_tx_power_limit_cfg_t;

typedef struct _tx_limit_t
{
  struct reg_tx_limit *reg_lim;
  struct hw_reg_tx_limit *hw_lim;
  size_t num_gains;
  struct antenna_gain *gain;
  /* this is needed for HW limits - to load initial data */
  uint16 vendor_id;
  uint16 device_id;
  uint8 hw_type;
  uint8 hw_revision;
  uint8 num_tx_antennas;
  uint8 num_rx_antennas;
} __MTLK_IDATA tx_limit_t;

typedef struct _mtlk_get_channel_data_t {
  uint8 reg_domain;
  uint8 is_ht;
  BOOL ap;
  uint8 spectrum_mode;
  uint8 bonding;
  uint16 channel;
  uint8 frequency_band;
  BOOL disable_sm_channels;
} mtlk_get_channel_data_t;

typedef struct {
  int16              tx_lim;
  uint16             freq;
  uint8              spectrum;
  uint8              reg_domain;
} __MTLK_IDATA mtlk_hw_limits_stat_entry_t;

typedef struct {
  uint16 tx_lim;
  uint8  reg_domain;
  uint8  reg_class;
  uint8  spectrum;
  uint8  channel;
  uint8  mitigation;
} __MTLK_IDATA mtlk_reg_limits_stat_entry_t;

typedef struct {
  uint16 freq;
  int16  gain;
} __MTLK_IDATA mtlk_ant_gain_stat_entry_t;

typedef enum _select_channel_2ghz_ncb_e
{
  NCB_2GHZ_CH_1,
  NCB_2GHZ_CH_6,
  NCB_2GHZ_CH_11,
  NCB_2GHZ_CH_LAST
} select_channel_2ghz_ncb_e;

int16 __MTLK_IFUNC
mtlk_calc_tx_power_lim (tx_limit_t *lim, uint16 channel, uint8 reg_domain, 
    uint8 spectrum_mode, int8 upper_lower, uint8 num_antennas);
#if 0
int __MTLK_IFUNC mtlk_fill_freq_element (mtlk_handle_t context, FREQUENCY_ELEMENT *el, uint8 channel, uint8 band, uint8 bw, uint8 reg_domain);
#endif
int16 __MTLK_IFUNC mtlk_get_antenna_gain(tx_limit_t *lim, uint16 channel);

int __MTLK_IFUNC mtlk_init_tx_limit_tables (tx_limit_t *lim, uint16 vendor_id, uint16 device_id,
  uint8 hw_type, uint8 hw_revision);
int __MTLK_IFUNC mtlk_cleanup_tx_limit_tables (tx_limit_t *lim);
int __MTLK_IFUNC mtlk_reset_tx_limit_tables (tx_limit_t *lim);
int __MTLK_IFUNC mtlk_update_reg_limit_table (mtlk_handle_t handle, struct country_ie_t *ie, int8 power_constraint);

int __MTLK_IFUNC mtlk_set_hw_limit (tx_limit_t *lim, struct _mtlk_hw_cfg_t *cfg);
int __MTLK_IFUNC mtlk_set_ant_gain (tx_limit_t *lim, struct _mtlk_ant_cfg_t *cfg);
int __MTLK_IFUNC mtlk_set_power_limit (mtlk_core_t *core, struct _mtlk_tx_power_limit_cfg_t *cfg);

int __MTLK_IFUNC mtlk_channels_get_hw_limits(tx_limit_t *lim, mtlk_clpb_t *clpb);

int __MTLK_IFUNC mtlk_channels_get_reg_limits(tx_limit_t *lim, mtlk_clpb_t *clpb);

int __MTLK_IFUNC mtlk_channels_get_ant_gain(tx_limit_t *lim, mtlk_clpb_t *clpb);

int __MTLK_IFUNC mtlk_set_country_mib (mtlk_txmm_t *txmm, 
                                       uint8 reg_domain, 
                                       uint8 is_ht, 
                                       uint8 frequency_band, 
                                       BOOL  is_ap, 
                                       const char *country,
                                       BOOL is_dot11d_active);
uint8 __MTLK_IFUNC mtlk_get_channel_mitigation(uint8 reg_domain, uint8 is_ht, uint8 spectrum_mode, uint16 channel);
int __MTLK_IFUNC
mtlk_get_avail_channels(mtlk_get_channel_data_t *param, uint8 *channels);
int __MTLK_IFUNC
mtlk_check_channel(mtlk_get_channel_data_t *param, uint8 channel);

int __MTLK_IFUNC mtlk_prepare_scan_vector (mtlk_handle_t context, struct mtlk_scan *scan_data, int freq, uint8 reg_domain);
void __MTLK_IFUNC mtlk_free_scan_vector (mtlk_scan_vector_t *vector);

uint8 __MTLK_IFUNC mtlk_select_reg_domain (uint16 channel);

uint8 __MTLK_IFUNC mtlk_get_chnl_switch_mode (uint8 spectrum_mode, uint8 bonding, uint8 is_silent_sw);

static __INLINE uint8 __MTLK_IFUNC
channel_to_band (uint16 channel)
{
#define FIRST_5_2_CHANNEL 36
  return (channel < FIRST_5_2_CHANNEL)? MTLK_HW_BAND_2_4_GHZ : MTLK_HW_BAND_5_2_GHZ;
#undef FIRST_5_2_CHANNEL
}

static __INLINE uint16 __MTLK_IFUNC
channel_to_frequency (uint16 channel)
{
#define CHANNEL_THRESHOLD 180
  uint16 res;
  if (channel_to_band(channel) == MTLK_HW_BAND_2_4_GHZ)
  {
    res = 2407 + 5*channel;
    if(channel == 14) /* en.wikipedia.org/wiki/List_of_WLAN_channels defines from standard the currect freq. for channel 14 */
      res += 7;
  }
  else if (channel < CHANNEL_THRESHOLD)
    res = 5000 + 5*channel;
  else
    res = 4000 + 5*channel;
#undef CHANNEL_THRESHOLD
  return res;
}

uint8 __MTLK_IFUNC country_code_to_domain (uint8 country_code);
const char * __MTLK_IFUNC country_code_to_country (uint8 country_code);
uint8 __MTLK_IFUNC country_to_country_code (const char *country);

struct _mtlk_gen_core_country_name_t;
void __MTLK_IFUNC get_all_countries_for_domain(uint8 domain,
                                               struct _mtlk_gen_core_country_name_t *countries,
                                               uint32 countries_buffer_size);

static uint8 __INLINE __MTLK_IFUNC
country_to_domain (const char *country)
{
  return country_code_to_domain(country_to_country_code(country));
}

BOOL __MTLK_IFUNC mtlk_channels_does_domain_exist(uint8 reg_domain);

void __MTLK_IFUNC mtlk_channels_fill_activate_req_ext_data(mtlk_get_channel_data_t *params,
                                                           mtlk_core_t             *core,
                                                           uint8                   u8SwitchMode,
                                                           void                    *data);


void __MTLK_IFUNC mtlk_channels_fill_mbss_pre_activate_req_ext_data(mtlk_get_channel_data_t *params,
  mtlk_core_t             *core,
  uint8                    u8SwitchMode,
  void                    *data);

BOOL __MTLK_IFUNC mtlk_channels_find_secondary_channel_no(int reg_domain, uint16 primary_channel_no, int secondary_channel_offset, uint16 *secondary_channel_no);

BOOL __MTLK_IFUNC mtlk_channels_find_available_channel_pair(int reg_domain, uint16 primary_channel, int secondary_channel_offset);

uint16 __MTLK_IFUNC mtlk_channels_get_secondary_channel_no_by_offset(uint16 primary_channel_no, uint8 secondary_channel_offset);

#define MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __CHANNELS_H__ */
