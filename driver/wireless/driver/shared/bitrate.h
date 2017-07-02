#ifndef __BITRATE_H__
#define __BITRATE_H__

#include "mtlkidefs.h"

#define BITRATE_FIRST         0
#define BITRATE_LAST          31

#define BITRATES_MAX          32

int __MTLK_IFUNC mtlk_bitrate_get_value (int index, int sm, int scp);

int __MTLK_IFUNC
mtlk_bitrate_rates_to_idx(int int_rate,
                          int frac_rate,
                          int spectrum_mode,
                          int short_cyclic_prefix,
                          uint16 *index);

int __MTLK_IFUNC
mtlk_bitrate_idx_to_rates(uint16 index,
                          int spectrum_mode,
                          int short_cyclic_prefix,
                          int *int_rate,
                          int *frac_rate);

#define MTLK_HW_RATE_INVALID ((uint16)-1)

uint16 __MTLK_IFUNC
mtlk_hw_rate_params_to_rate(uint8 hw_param_tcr2, uint8 scp);


#endif

