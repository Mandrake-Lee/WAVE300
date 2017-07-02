#ifndef __LEDS_CTRL_H__
#define __LEDS_CTRL_H__

#include "mtlkcontainer.h"

extern const mtlk_component_api_t irb_leds_ctrl_api;


/* Private API.
 */
char *leds_ctrl_get_string(void);
char *leds_ctrl_get_bin_string(void);
void leds_ctrl_set_param(const char *key, const char *value);
void leds_ctrl_set_bin_param(const char *key, const int value);

void leds_ctrl_set_wps_stat(uint8 val);
void leds_ctrl_set_mask(uint32 val);
void leds_ctrl_set_leds_resolution(int val);
int  leds_ctrl_set_network_type(char* str);
int  leds_ctrl_set_link_path(char* str);
int  leds_ctrl_set_link_status_path(char* str);
void leds_ctrl_set_security_mode(int val);
void leds_ctrl_set_reconf_security(int val);
void leds_ctrl_set_conf_file(const char* str);

#endif /* __LEDS_CTRL_H__ */
