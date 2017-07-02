#ifndef __ASEL_H__
#define __ASEL_H__

#include "mtlkcontainer.h"
#include "mtlkirba.h"

extern const mtlk_component_api_t rf_mgmt_api;

struct mtlk_rf_mgmt_cfg
{
  uint32      rf_mgmt_type;
  uint32      refresh_time_ms;
  uint32      keep_alive_tmout_sec;
  uint32      averaging_alpha;
  uint32      margin_threshold;
  mtlk_irba_t *irba;
};

extern struct mtlk_rf_mgmt_cfg rf_mgmt_cfg;

#endif /* __ASEL_H__ */


