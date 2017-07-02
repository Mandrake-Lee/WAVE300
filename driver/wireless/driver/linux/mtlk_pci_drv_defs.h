#if !defined (SAFE_PLACE_TO_INCLUDE_PCI_DRV_DEFS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_PCI_DRV_DEFS */

#undef SAFE_PLACE_TO_INCLUDE_PCI_DRV_DEFS

#include "mmb_ops.h" 

int __MTLK_IFUNC
mtlk_bus_drv_handle_interrupt(mtlk_pci_drv_t *pci_drv);

const char* __MTLK_IFUNC
mtlk_bus_drv_get_name(mtlk_pci_drv_t *pci_drv);

mtlk_card_type_t __MTLK_IFUNC
mtlk_bus_drv_get_card_type(mtlk_pci_drv_t *pci_drv);

struct device *
mtlk_bus_drv_get_device(mtlk_pci_drv_t *drv);

#define SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DEFS
#include "mtlk_ccr_defs.h"

