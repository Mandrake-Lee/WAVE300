#ifndef __ARP_HELPER_H__
#define __ARP_HELPER_H__

#include "mtlkcontainer.h"

extern const mtlk_component_api_t irb_arp_helper_api;

/* Private API.
 * TODO: Must be replaced by ParamDB
 */
int arp_helper_register_iface(const char *iface);

#endif /* __ARP_HELPER_H__ */


