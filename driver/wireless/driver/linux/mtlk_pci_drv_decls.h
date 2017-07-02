#if !defined (SAFE_PLACE_TO_INCLUDE_PCI_DRV_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_HW_PCI_... */

#undef SAFE_PLACE_TO_INCLUDE_PCI_DRV_DECLS

#include "mtlk_card_types.h"

#define SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DECLS
#include "mtlk_ccr_decls.h"

#include "mtlk_vap_manager.h"

struct _mtlk_pci_drv_t
{
  struct pci_dev       *dev;
  /* in struct pci_dev member irq has unsigned int type,
   * we need it to be signed for -1 (was not requested)
   */
  unsigned char        *bar0;
  unsigned char        *bar1;

  mtlk_ccr_t            ccr;
  mtlk_card_type_t      card_type;

  mtlk_hw_t             *hw;

  mtlk_vap_manager_t    *vap_manager;

  struct tasklet_struct mmb_tasklet;
  MTLK_DECLARE_INIT_STATUS;
};

typedef struct _mtlk_pci_drv_t mtlk_pci_drv_t;

#define MTLK_HW_MAX_CARDS  10

#define MTLK_HW_PRESENT_CPUS         (MTLK_UPPER_CPU | MTLK_LOWER_CPU)

