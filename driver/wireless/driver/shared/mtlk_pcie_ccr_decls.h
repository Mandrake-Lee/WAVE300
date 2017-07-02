/* $Id: mtlk_pcie_ccr_decls.h 10358 2011-01-15 09:59:33Z dmytrof $ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DECLS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DECLS

#include "g3shram_ex.h"

typedef struct
{
  struct g3_pas_map         *pas;
  struct _mtlk_pci_drv_t    *pci_drv;
  uint8                      current_ucpu_state;
  uint8                      current_lcpu_state;
  uint8                      next_boot_mode;

  volatile BOOL              irqs_enabled;
  volatile BOOL              irq_pending;

  MTLK_DECLARE_INIT_STATUS;
} _mtlk_pcie_ccr_t;

#define G3PCIE_CPU_Control_BIST_Passed    ((1 << 31) | (1 << 15)) 
