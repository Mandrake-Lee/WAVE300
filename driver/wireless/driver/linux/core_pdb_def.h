/*
 * $Id: core_pdb_def.h 12533 2012-02-02 11:24:00Z nayshtut $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Core's parameters DB definitions
 *
 * Written by: Grygorii Strashko
 *
 */

#ifndef __CORE_PDB_DEF_H_
#define __CORE_PDB_DEF_H_

#include "mtlk_param_db.h"
#include "channels.h"
#include "mtlkaux.h"
#include "core_priv.h"
#include "mtlk_coreui.h"

#define MTLK_IDEFS_ON
#include "mtlkidefs.h"

static const uint32 mtlk_core_initial_zero_int = 0;

static const uint32  mtlk_core_initial_bridge_mode = 0;
static const uint32  mtlk_core_initial_ap_forwarding = 1;
static const uint32  mtlk_core_initial_reliable_mcast = 1;

static const uint8   mtlk_core_initial_mac_addr[ETH_ALEN] = {0};
static const uint8   mtlk_core_initial_bssid[ETH_ALEN] = {0};

static const uint32  mtlk_core_initial_sta_force_spectrum_mode = SPECTRUM_AUTO;
static const uint32  mtlk_core_initial_spectrum_mode = MIB_SPECTRUM_40M;
static const uint32  mtlk_core_initial_ui_rescan_exemption_time = 60000; /* ms */
static const uint32  mtlk_core_initial_channel = 0;
static const uint32  mtlk_core_initial_power_selection = 0;
static const uint32  mtlk_core_initial_mac_soft_reset_enable = FALSE;
static const uint32  mtlk_core_initial_country_code = 0;
static const uint32  mtlk_core_initial_dot11d_enabled = TRUE;
static const uint32  mtlk_core_initial_basic_rate_set = CFG_BASIC_RATE_SET_DEFAULT;
static const uint32  mtlk_core_initial_legacy_forced_rate = NO_RATE;
static const uint32  mtlk_core_initial_ht_forced_rate = NO_RATE;
static const uint32  mtlk_core_initial_mac_watchdog_timeout_ms = MAC_WATCHDOG_DEFAULT_TIMEOUT_MS;
static const uint32  mtlk_core_initial_mac_watchdog_period_ms = MAC_WATCHDOG_DEFAULT_PERIOD_MS;
static const char    mtlk_core_initial_nick_name[] = "";
static const char    mtlk_core_initial_essid[] = "";
static const uint32  mtlk_core_initial_net_mode_cfg = NETWORK_11A_ONLY;
static const uint32  mtlk_core_initial_net_mode_cur = NETWORK_11A_ONLY;
static const uint32  mtlk_core_initial_frequency_band_cfg = MTLK_HW_BAND_2_4_GHZ;
static const uint32  mtlk_core_initial_frequency_band_cur = MTLK_HW_BAND_2_4_GHZ;
static const uint32  mtlk_core_initial_is_ht_cfg = TRUE;
static const uint32  mtlk_core_initial_is_ht_cur = TRUE;
static const uint32  mtlk_core_initial_l2nat_aging_timeout = 600;
/* MIBS */
static const uint32  mtlk_core_initial_short_preamble = TRUE;
static const uint32  mtlk_core_initial_tx_power = 0;
static const uint32  mtlk_core_initial_short_cyclic_prefix = 0;
static const uint32  mtlk_core_initial_calibration_algo_mask = 255;
static const uint32  mtlk_core_initial_short_slot_time = TRUE;
static const uint32  mtlk_core_initial_power_increase = FALSE;
static const uint32  mtlk_core_initial_sm_enable = TRUE;
static const uint8   mtlk_core_initial_tx_antennas[MTLK_NUM_ANTENNAS_BUFSIZE] = {1, 2, 0, 0};
static const uint8   mtlk_core_initial_rx_antennas[MTLK_NUM_ANTENNAS_BUFSIZE] = {1, 2, 3, 0};


static const mtlk_pdb_initial_value mtlk_core_parameters[] =
{
  /* ID,                          TYPE,                 FLAGS,                        SIZE,                            POINTER TO CONST */
  {PARAM_DB_CORE_MAC_ADDR,        PARAM_DB_TYPE_MAC,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_mac_addr),        mtlk_core_initial_mac_addr},
  {PARAM_DB_CORE_BRIDGE_MODE,     PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_bridge_mode),     &mtlk_core_initial_bridge_mode},
  {PARAM_DB_CORE_AP_FORWARDING,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_ap_forwarding),   &mtlk_core_initial_ap_forwarding},
  {PARAM_DB_CORE_RELIABLE_MCAST,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_reliable_mcast),   &mtlk_core_initial_reliable_mcast},
  {PARAM_DB_CORE_BSSID,           PARAM_DB_TYPE_MAC,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_bssid),        mtlk_core_initial_bssid},

  {PARAM_DB_CORE_STA_FORCE_SPECTRUM_MODE,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_sta_force_spectrum_mode),   &mtlk_core_initial_sta_force_spectrum_mode},
  {PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_spectrum_mode),   &mtlk_core_initial_spectrum_mode},
  {PARAM_DB_CORE_SELECTED_SPECTRUM_MODE,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_spectrum_mode),   &mtlk_core_initial_spectrum_mode},
  {PARAM_DB_CORE_UP_RESCAN_EXEMPTION_TIME,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_ui_rescan_exemption_time),   &mtlk_core_initial_ui_rescan_exemption_time},
  {PARAM_DB_CORE_CHANNEL_CFG,     PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_channel),   &mtlk_core_initial_channel},
  {PARAM_DB_CORE_CHANNEL_CUR,     PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_channel),   &mtlk_core_initial_channel},
  {PARAM_DB_CORE_POWER_SELECTION, PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_power_selection),   &mtlk_core_initial_power_selection},
  {PARAM_DB_CORE_MAC_SOFT_RESET_ENABLE, PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_mac_soft_reset_enable),   &mtlk_core_initial_mac_soft_reset_enable},
  {PARAM_DB_CORE_COUNTRY_CODE,    PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_country_code),   &mtlk_core_initial_country_code},
  {PARAM_DB_CORE_DOT11D_ENABLED,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_dot11d_enabled),   &mtlk_core_initial_dot11d_enabled},
  {PARAM_DB_CORE_BASIC_RATE_SET,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_basic_rate_set),   &mtlk_core_initial_basic_rate_set},
  {PARAM_DB_CORE_LEGACY_FORCED_RATE_SET,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_legacy_forced_rate),   &mtlk_core_initial_legacy_forced_rate},
  {PARAM_DB_CORE_HT_FORCED_RATE_SET,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_ht_forced_rate),   &mtlk_core_initial_ht_forced_rate},
  {PARAM_DB_CORE_MAC_WATCHDOG_TIMER_TIMEOUT_MS,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_mac_watchdog_timeout_ms),   &mtlk_core_initial_mac_watchdog_timeout_ms},
  {PARAM_DB_CORE_MAC_WATCHDOG_TIMER_PERIOD_MS,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_mac_watchdog_period_ms),   &mtlk_core_initial_mac_watchdog_period_ms},
  {PARAM_DB_CORE_NICK_NAME,       PARAM_DB_TYPE_STRING, PARAM_DB_VALUE_FLAG_NO_FLAG,  MTLK_ESSID_MAX_SIZE,   mtlk_core_initial_nick_name},
  {PARAM_DB_CORE_ESSID,           PARAM_DB_TYPE_STRING, PARAM_DB_VALUE_FLAG_NO_FLAG,  MTLK_ESSID_MAX_SIZE,   mtlk_core_initial_essid},
  {PARAM_DB_CORE_NET_MODE_CFG,    PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_net_mode_cfg),   &mtlk_core_initial_net_mode_cfg},
  {PARAM_DB_CORE_NET_MODE_CUR,    PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_net_mode_cur),   &mtlk_core_initial_net_mode_cur},
  {PARAM_DB_CORE_FREQ_BAND_CFG,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_frequency_band_cfg),   &mtlk_core_initial_frequency_band_cfg},
  {PARAM_DB_CORE_FREQ_BAND_CUR,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_frequency_band_cur),   &mtlk_core_initial_frequency_band_cur},
  {PARAM_DB_CORE_IS_HT_CFG,       PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_is_ht_cfg),   &mtlk_core_initial_is_ht_cfg},
  {PARAM_DB_CORE_IS_HT_CUR,       PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_is_ht_cur),   &mtlk_core_initial_is_ht_cur},
  {PARAM_DB_CORE_L2NAT_AGING_TIMEOUT,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_l2nat_aging_timeout),   &mtlk_core_initial_l2nat_aging_timeout},
  /* MIBS */
  {PARAM_DB_CORE_SHORT_PREAMBLE,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_short_preamble),   &mtlk_core_initial_short_preamble},
  {PARAM_DB_CORE_TX_POWER,        PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_tx_power),   &mtlk_core_initial_tx_power},
  {PARAM_DB_CORE_SHORT_CYCLIC_PREFIX,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_short_cyclic_prefix),   &mtlk_core_initial_short_cyclic_prefix},
  {PARAM_DB_CORE_CALIBRATION_ALGO_MASK,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_calibration_algo_mask),   &mtlk_core_initial_calibration_algo_mask},
  {PARAM_DB_CORE_SHORT_SLOT_TIME, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_short_slot_time),   &mtlk_core_initial_short_slot_time},
  {PARAM_DB_CORE_POWER_INCREASE,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_power_increase),   &mtlk_core_initial_power_increase},
  {PARAM_DB_CORE_SM_ENABLE,       PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_sm_enable),   &mtlk_core_initial_sm_enable},
  {PARAM_DB_CORE_TX_ANTENNAS,     PARAM_DB_TYPE_BINARY, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_tx_antennas),   &mtlk_core_initial_tx_antennas},
  {PARAM_DB_CORE_RX_ANTENNAS,     PARAM_DB_TYPE_BINARY, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_rx_antennas),   &mtlk_core_initial_rx_antennas},

  {PARAM_DB_FW_LED_GPIO_DISABLE_TESTBUS, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},
  {PARAM_DB_FW_LED_GPIO_ACTIVE_GPIOs,    PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},
  {PARAM_DB_FW_LED_GPIO_LED_POLARITY,    PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},
  
  {PARAM_DB_LAST_VALUE_ID,        0,                    0,                            0,                               NULL},
};

#define MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __CORE_PDB_DEF_H_ */
