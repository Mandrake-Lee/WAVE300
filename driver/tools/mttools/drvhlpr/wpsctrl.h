#ifndef __WPS_CTRL_H__
#define __WPS_CTRL_H__

#include "mtlkcontainer.h"


extern const mtlk_component_api_t irb_wps_ctrl_api;

/* Private API.
 */
void wps_ctrl_set_leds_resolution(int val);
int  wps_ctrl_set_link_status_path(char* str);
int  wps_ctrl_set_wps_script_path(char* str);

#endif /* __WPS_CTRL_H__ */
