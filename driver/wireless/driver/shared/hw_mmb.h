#ifndef __HW_MMB_H__
#define __HW_MMB_H__

#define MTLK_UPPER_CPU       (1 << 0)
#define MTLK_LOWER_CPU       (1 << 1)

#include "mtlkdfdefs.h"
#include "mtlkirbd.h"
#include "mtlk_wss.h"

#if defined (MTCFG_BUS_PCI_PCIE)
#define SAFE_PLACE_TO_INCLUDE_PCI_DRV_DECLS
#include "mtlk_pci_drv_decls.h"
#elif defined (MTCFG_BUS_AHB)
#define SAFE_PLACE_TO_INCLUDE_AHB_DRV_DECLS
#include "mtlk_ahb_drv_decls.h"
#else
#error Wrong platform!
#endif 

#define MTLK_HAVE_CPU(id) (MTLK_HW_PRESENT_CPUS & (id))

#include "shram_ex.h"

#include "mtlk_osal.h"
#include "txmm.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define MTLK_FRMW_UPPER_AP_NAME   "ap_upper.bin"
#define MTLK_FRMW_UPPER_STA_NAME  "sta_upper.bin"

#if MTLK_HAVE_CPU(MTLK_LOWER_CPU)
#define MTLK_FRMW_LOWER_NAME      "contr_lm.bin"
#endif

/**************************************************************
 * Auxiliary OS/bus abstractions required for MMB
 * NOTE: must be defined within hw_<platfrorm>.h
 **************************************************************/
typedef void (__MTLK_IFUNC *mtlk_hw_bus_sync_f)(void *context);

void mtlk_mmb_sync_isr(mtlk_hw_t         *hw, 
                       mtlk_hw_bus_sync_f func,
                       void              *context);
/**************************************************************/

/**************************************************************
 * MMB Inreface
 **************************************************************/

typedef struct
{
  mtlk_ccr_t            *ccr;
  unsigned char         *pas;
#if defined(MTCFG_LINDRV_HW_AHBG35)
  unsigned char         *cpu_ddr;
#endif
  mtlk_irbd_t           *parent_irbd;
  mtlk_wss_t            *parent_wss;
} __MTLK_IDATA mtlk_hw_mmb_card_cfg_t;

typedef struct
{
  uint8  bist_check_permitted;
  uint32 no_pll_write_delay_us;
  uint32 man_msg_size;
  uint32 dbg_msg_size;
} __MTLK_IDATA mtlk_hw_mmb_cfg_t;

typedef struct
{
  mtlk_hw_mmb_cfg_t    cfg;
  mtlk_hw_t           *cards[MTLK_HW_MAX_CARDS];
  uint32               nof_cards;
  mtlk_osal_spinlock_t lock;
  uint32               bist_passed;

  MTLK_DECLARE_INIT_STATUS;
} __MTLK_IDATA mtlk_hw_mmb_t;

/**************************************************************
 * Init/cleanup functions - must be called on driver's
 * loading/unloading
 **************************************************************/
int __MTLK_IFUNC 
mtlk_hw_mmb_init(mtlk_hw_mmb_t *mmb, const mtlk_hw_mmb_cfg_t *cfg);
void __MTLK_IFUNC
mtlk_hw_mmb_cleanup(mtlk_hw_mmb_t *mmb);
/**************************************************************/

/**************************************************************
 * Auxilliary MMB interface - for BUS module usage
 **************************************************************/
uint32 __MTLK_IFUNC
mtlk_hw_mmb_get_cards_no(mtlk_hw_mmb_t *mmb);
mtlk_txmm_base_t *__MTLK_IFUNC
mtlk_hw_mmb_get_txmm(mtlk_hw_t *card);
mtlk_txmm_base_t *__MTLK_IFUNC
mtlk_hw_mmb_get_txdm(mtlk_hw_t *card);
uint8 __MTLK_IFUNC
mtlk_hw_mmb_get_card_idx(mtlk_hw_t *card);

/* Stops all the MAC-initiated events (INDs), sending to MAC still working */
void __MTLK_IFUNC
mtlk_hw_mmb_stop_mac_events(mtlk_hw_t *card);
/**************************************************************/

/**************************************************************
 * Add/remove card - must be called on device addition/removal
 **************************************************************/
mtlk_hw_api_t * __MTLK_IFUNC 
mtlk_hw_mmb_add_card(mtlk_hw_mmb_t                *mmb,
                     const mtlk_hw_mmb_card_cfg_t *card_cfg);
void __MTLK_IFUNC 
mtlk_hw_mmb_remove_card(mtlk_hw_mmb_t *mmb,
                        mtlk_hw_api_t *hw_api);

/**************************************************************
 * Init/cleanup card - must be called on device init/cleanup
 **************************************************************/
int __MTLK_IFUNC 
mtlk_hw_mmb_init_card(mtlk_hw_t   *card,
                      mtlk_vap_manager_t *vap_manager,
                      mtlk_ccr_t *ccr);

void __MTLK_IFUNC 
mtlk_hw_mmb_cleanup_card(mtlk_hw_t *card);

int __MTLK_IFUNC 
mtlk_hw_mmb_start_card(mtlk_hw_t   *hw);

void __MTLK_IFUNC 
mtlk_hw_mmb_stop_card(mtlk_hw_t *card);


/**************************************************************/

/**************************************************************
 * Card's ISR - must be called on interrupt handler
 * Return values:
 *   MTLK_ERR_OK      - do nothing
 *   MTLK_ERR_UNKNOWN - not an our interrupt
 *   MTLK_ERR_PENDING - order bottom half routine (DPC, tasklet etc.)
 **************************************************************/
int __MTLK_IFUNC 
mtlk_hw_mmb_interrupt_handler(mtlk_hw_t *card);
/**************************************************************/

/**************************************************************
 * Card's bottom half of irq handling (DPC, tasklet etc.)
 **************************************************************/
void __MTLK_IFUNC 
mtlk_hw_mmb_deferred_handler(mtlk_hw_t *card);
/**************************************************************/
/**************************************************************/

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#if defined (MTCFG_BUS_PCI_PCIE)
#define SAFE_PLACE_TO_INCLUDE_PCI_DRV_DEFS
#include "mtlk_pci_drv_defs.h"
#elif defined (MTCFG_BUS_AHB)
#define SAFE_PLACE_TO_INCLUDE_AHB_DRV_DEFS
#include "mtlk_ahb_drv_defs.h"
#else
#error Wrong platform!
#endif 

#endif /* __HW_MMB_H__ */
