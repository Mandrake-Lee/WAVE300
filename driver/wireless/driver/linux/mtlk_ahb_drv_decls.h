#if !defined (SAFE_PLACE_TO_INCLUDE_AHB_DRV_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_AHB_DRV_DECLS */

#undef SAFE_PLACE_TO_INCLUDE_AHB_DRV_DECLS

#include "mtlk_card_types.h"

#define SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DECLS
#include "mtlk_ccr_decls.h"

#include "mtlk_vap_manager.h"

struct _mtlk_ahb_drv_t
{
  struct platform_device *dev;
  /* in struct pci_dev member irq has unsigned int type,
   * we need it to be signed for -1 (was not requested)
   */
  struct resource      *bar0;
  void                 *bar0_va;
  struct resource      *bar1;
  void                 *bar1_va;

  mtlk_ccr_t            ccr;
  mtlk_card_type_t      card_type;

  mtlk_hw_t             *hw;
  mtlk_core_t           *core;

  mtlk_vap_manager_t *vap_manager;

  int irq;

  struct tasklet_struct mmb_tasklet;
  MTLK_DECLARE_INIT_STATUS;
};

typedef struct _mtlk_ahb_drv_t mtlk_ahb_drv_t;

#define MTLK_HW_MAX_CARDS  10

#define MTLK_HW_PRESENT_CPUS         (MTLK_UPPER_CPU)

