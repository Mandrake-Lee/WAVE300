/*
 * $Id: mtlk_eeprom.h 9494 2010-09-02 12:38:41Z atseglytskyi $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * EEPROM data processing module
 *
 * Originally written by Grygorii Strashko
 *
 */

#ifndef MTLK_EEPROM_H_
#define MTLK_EEPROM_H_

#define  MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef struct _tpc_point_t
{
  uint16 x;
  uint8 y;
} __MTLK_IDATA tpc_point_t;

typedef struct _mtlk_eeprom_tpc_data_t
{
  uint8 channel;
  uint16 freq;
  uint8 band;
  uint8 spectrum_mode;
  uint8 tpc_values[MAX_NUM_TX_ANTENNAS];
  uint8 backoff_values[MAX_NUM_TX_ANTENNAS];
  uint8 backoff_mult_values[MAX_NUM_TX_ANTENNAS];
  uint8 num_points;
  tpc_point_t *points[MAX_NUM_TX_ANTENNAS];
  struct _mtlk_eeprom_tpc_data_t *next;
} __MTLK_IDATA mtlk_eeprom_tpc_data_t;

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

mtlk_eeprom_tpc_data_t* __MTLK_IFUNC mtlk_find_closest_freq (uint8 channel, mtlk_eeprom_data_t *eeprom);
uint16 __MTLK_IFUNC mtlk_get_max_tx_power(mtlk_eeprom_data_t* eeprom, uint8 channel);

#endif /* MTLK_EEPROM_H_ */
