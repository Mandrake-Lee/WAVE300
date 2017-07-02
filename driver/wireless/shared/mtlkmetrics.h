#ifndef __MTLK_METRICS_H__
#define __MTLK_METRICS_H__

#include "shram.h"

#define MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef ASL_SHRAM_METRIC_T mtlk_g2_rx_metrics_t;

uint32 __MTLK_IFUNC mtlk_metrics_calc_effective_snr(mtlk_g2_rx_metrics_t *raw_metrics,
                                                    uint8                *mcs_feedback,
                                                    uint32               *short_cp_metric);

uint32 __MTLK_IFUNC mtlk_calculate_effective_snr_margin(uint32 metric, uint8 mcs_feedback);

#define ESNR_DB_RESOLUTION 10 /* 0.1 db */
#define ESNR_DB_BASE       (45 * ESNR_DB_RESOLUTION)

uint32 __MTLK_IFUNC mtlk_get_esnr_db(uint32 metric, uint8 mcs_feedback);

#define MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_METRICS_H__ */
