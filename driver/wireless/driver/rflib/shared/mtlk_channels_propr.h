/*
* $Id: $
*
* Copyright (c) 2006-2007 Metalink Broadband (Israel)
*
* Proprietary includes for library only!
*
*/

#ifndef _MTLK_CHANNELS_PROPR_H_
#define _MTLK_CHANNELS_PROPR_H_

#include "channels.h"
#include "scan.h"
#include "mhi_umi.h"
#include "mhi_umi_propr.h"

#define  MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef struct _drv_params_t
{
    uint8 band;
    uint8 bandwidth;
    uint8 upper_lower;
    uint8 reg_domain;
    uint8 spectrum_mode;
} __MTLK_IDATA drv_params_t;

struct reg_class_t
{
    uint8 reg_class;
    uint16 start_freq;
    uint8 spacing;
    int8 max_power;
    uint8 scan_type; /* 0 - active, 1 - passive */
    uint8 sm_required;
    int8 mitigation;
    uint8 num_channels;
    const uint8 *channels;
    uint16 channelAvailabilityCheckTime;
    uint8 radar_detection_validity_time;
    uint8 non_occupied_period;
} __MTLK_IDATA;

struct reg_domain_t
{
    uint8 num_classes;
    const struct reg_class_t *classes;
} __MTLK_IDATA;

#define MAKE_PROTOCOL_INDEX(is_ht, frequency_band) ((((is_ht) & 0x1) << 1) + ((frequency_band) & 0x1))
#define IS_HT_PROTOCOL_INDEX(index) (((index) & 0x2) >> 1)
#define IS_2_4_PROTOCOL_INDEX(index) ((index) & 0x1)

uint16 __MTLK_IFUNC
mtlk_calc_start_freq (drv_params_t *param, uint16 channel);

uint8 __MTLK_IFUNC mtlk_get_channels_for_reg_domain (struct mtlk_scan *scan_data,
                                                     FREQUENCY_ELEMENT *channels,
                                                     uint8 *num_channels);

const struct reg_domain_t * __MTLK_IFUNC mtlk_get_domain(uint8 reg_domain
                                                         ,int *result
                                                         ,uint16 *num_of_protocols
                                                         ,uint8 u8Upper
                                                         ,uint16 caller);

int __MTLK_IFUNC mtlk_get_channel_data(mtlk_get_channel_data_t *params,
                                       FREQUENCY_ELEMENT *freq_element, uint8 *non_occupied_period,
                                       uint8 *radar_detection_validity_time);

void __MTLK_IFUNC mtlk_fill_channel_params_by_tpc(mtlk_handle_t context, FREQUENCY_ELEMENT *params);

void __MTLK_IFUNC mtlk_fill_channel_params_by_tpc_by_vap(mtlk_vap_handle_t vap_handle, FREQUENCY_ELEMENT *params);
                    
#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* _MTLK_CHANNELS_PROPR_H_ */
