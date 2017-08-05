#include "mtlkinc.h"
#include "hw_mmb.h"
#include "drvver.h"
#include "mtlk_df.h"
#include "mtlk_fast_mem.h"
#include "mtlk_vap_manager.h"

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#ifdef MTCFG_BENCHMARK_TOOLS
#include "mtlk_dbg.h"
#endif

#define LOG_LOCAL_GID   GID_PCIDRV
#define LOG_LOCAL_FID   1

#define MTLK_VENDOR_ID               0x1a30
#define HYPERION_I_PCI_DEVICE_ID     0x0600
#define HYPERION_II_PCI_DEVICE_ID_A1 0x0680
#define HYPERION_II_PCI_DEVICE_ID_A2 0x0681
#define HYPERION_III_PCI_DEVICE      0x0700
#define HYPERION_III_PCIE_DEVICE     0x0710

/* we only support 32-bit addresses */
#define PCI_SUPPORTED_DMA_MASK       0xffffffff

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR(MTLK_COPYRIGHT);
MODULE_LICENSE("Proprietary");

static int ap[MTLK_HW_MAX_CARDS] = {0};
static int debug                 = 0;
#ifdef MTLK_DEBUG
extern int step_to_fail;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
MODULE_PARM(ap, "1-" __MODULE_STRING(MTLK_HW_MAX_CARDS) "i");
MODULE_PARM(debug, "i");
#ifdef MTLK_DEBUG
MODULE_PARM(step_to_fail, "i");
#endif
#else
module_param_array(ap, int, NULL, 0);
module_param(debug, int, 0);
#ifdef MTLK_DEBUG
module_param(step_to_fail, int, 0);
#endif
#endif

MODULE_PARM_DESC(ap, "Make an access point");
MODULE_PARM_DESC(debug, "Debug level");
#ifdef MTLK_DEBUG
MODULE_PARM_DESC(step_to_fail, "Init step to simulate fail");
#endif

/**************************************************************/

/* TODO: DEV_DF made external for init in DFG will be fixed */
mtlk_hw_mmb_t mtlk_mmb_obj;

static void
_mtlk_bus_drv_tasklet(unsigned long param) // bottom half of PCI irq handling
{
  mtlk_pci_drv_t *bus_drv = (mtlk_pci_drv_t*)param;
  mtlk_hw_mmb_deferred_handler(bus_drv->hw);
}

int __MTLK_IFUNC
mtlk_bus_drv_handle_interrupt(mtlk_pci_drv_t *bus_drv)
{
  int res;
  
  CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_ISR);
  res = mtlk_hw_mmb_interrupt_handler(bus_drv->hw);
  CPU_STAT_END_TRACK(CPU_STAT_ID_ISR);

  if (res == MTLK_ERR_OK)
    return MTLK_ERR_OK;
  else if (res == MTLK_ERR_PENDING) {
    tasklet_schedule(&bus_drv->mmb_tasklet);
    return MTLK_ERR_OK;
  }

  return MTLK_ERR_NOT_SUPPORTED;
}

const char* __MTLK_IFUNC
mtlk_bus_drv_get_name(mtlk_pci_drv_t *bus_drv)
{
  return pci_name(bus_drv->dev);
}

mtlk_card_type_t __MTLK_IFUNC
mtlk_bus_drv_get_card_type(mtlk_pci_drv_t *bus_drv)
{
  return bus_drv->card_type;
}

struct device * __MTLK_IFUNC
mtlk_bus_drv_get_device (mtlk_pci_drv_t *drv)
{
  return &drv->dev->dev;
}

static irqreturn_t
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
_mtlk_bus_drv_interrupt_handler(int irq, void *ptr, struct pt_regs *regs)
#else
_mtlk_bus_drv_interrupt_handler(int irq, void *ptr)
#endif
{
  /* Multiple returns are used to support void return */
  mtlk_pci_drv_t *obj = (mtlk_pci_drv_t *)ptr;
  MTLK_UNREFERENCED_PARAM(irq);

  if(MTLK_ERR_OK == mtlk_ccr_handle_interrupt(&obj->ccr))
    return IRQ_HANDLED;
  else
    return IRQ_NONE;
}

static void
_mtlk_bus_drv_clear_interrupts(mtlk_pci_drv_t *obj, struct pci_dev *dev)
{
#ifdef MTCFG_LINDRV_HW_PCIE
  if(MTLK_CARD_PCIE == obj->card_type)
  {
    pci_disable_msi(dev);
  }
#endif
}

MTLK_INIT_STEPS_LIST_BEGIN(bus_drv)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_ENABLE_DEVICE)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_SET_DMA_MASK)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_MAP_BAR0)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_MAP_BAR1)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_SETUP_INTERRUPTS)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_CREATE_VAP_MANAGER)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_MMB_ADD_CARD)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_CREATE_VAP)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_CREATE_CCR)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_MMB_INIT_CARD)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_REQUEST_IRQ)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_MMB_START_CARD)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_PREPARE_START_VAPS)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_START_MAC_EVENTS)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_START_VAPS)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_CORE_PREPARE_STOP)
MTLK_INIT_INNER_STEPS_BEGIN(bus_drv)
MTLK_INIT_STEPS_LIST_END(bus_drv);

static void
_mtlk_bus_drv_cleanup(mtlk_pci_drv_t *bus_obj, mtlk_vap_manager_interface_e intf)
{
  MTLK_CLEANUP_BEGIN(bus_drv, MTLK_OBJ_PTR(bus_obj))
    MTLK_CLEANUP_STEP(bus_drv, BUS_CORE_PREPARE_STOP, MTLK_OBJ_PTR(bus_obj),
                      mtlk_vap_manager_prepare_stop, (bus_obj->vap_manager));
    MTLK_CLEANUP_STEP(bus_drv, BUS_START_VAPS, MTLK_OBJ_PTR(bus_obj), 
                      mtlk_vap_manager_stop_all_vaps, (bus_obj->vap_manager, intf));
    MTLK_CLEANUP_STEP(bus_drv, BUS_START_MAC_EVENTS, MTLK_OBJ_PTR(bus_obj),
                      mtlk_hw_mmb_stop_mac_events, (bus_obj->hw));
    MTLK_CLEANUP_STEP(bus_drv, BUS_PREPARE_START_VAPS, MTLK_OBJ_PTR(bus_obj), 
                      MTLK_NOACTION, ());
    MTLK_CLEANUP_STEP(bus_drv, BUS_MMB_START_CARD, MTLK_OBJ_PTR(bus_obj), 
                      mtlk_hw_mmb_stop_card, (bus_obj->hw));
    MTLK_CLEANUP_STEP(bus_drv, BUS_REQUEST_IRQ, MTLK_OBJ_PTR(bus_obj),
                      free_irq, (bus_obj->dev->irq, bus_obj));
    MTLK_CLEANUP_STEP(bus_drv, BUS_MMB_INIT_CARD, MTLK_OBJ_PTR(bus_obj), 
                      mtlk_hw_mmb_cleanup_card, (bus_obj->hw));
    MTLK_CLEANUP_STEP(bus_drv, BUS_CREATE_CCR, MTLK_OBJ_PTR(bus_obj),
                      mtlk_ccr_cleanup, (&bus_obj->ccr));

    MTLK_CLEANUP_STEP(bus_drv, BUS_CREATE_VAP, MTLK_OBJ_PTR(bus_obj),
                      mtlk_vap_manager_delete_all_vaps, (bus_obj->vap_manager));

    MTLK_CLEANUP_STEP(bus_drv, BUS_MMB_ADD_CARD, MTLK_OBJ_PTR(bus_obj),
                      mtlk_hw_mmb_remove_card, (&mtlk_mmb_obj, mtlk_vap_manager_get_hw_api(bus_obj->vap_manager)));

    MTLK_CLEANUP_STEP(bus_drv, BUS_CREATE_VAP_MANAGER, MTLK_OBJ_PTR(bus_obj),
                      mtlk_vap_manager_delete, (bus_obj->vap_manager));

    MTLK_CLEANUP_STEP(bus_drv, BUS_SETUP_INTERRUPTS, MTLK_OBJ_PTR(bus_obj), 
                      _mtlk_bus_drv_clear_interrupts, (bus_obj, bus_obj->dev));
    MTLK_CLEANUP_STEP(bus_drv, BUS_MAP_BAR1, MTLK_OBJ_PTR(bus_obj), 
                      iounmap, (bus_obj->bar1));
    MTLK_CLEANUP_STEP(bus_drv, BUS_MAP_BAR0, MTLK_OBJ_PTR(bus_obj), 
                      iounmap, (bus_obj->bar0));
    MTLK_CLEANUP_STEP(bus_drv, BUS_SET_DMA_MASK, MTLK_OBJ_PTR(bus_obj), 
                      MTLK_NOACTION, ());
    MTLK_CLEANUP_STEP(bus_drv, BUS_ENABLE_DEVICE, MTLK_OBJ_PTR(bus_obj), 
                      MTLK_NOACTION, ());
  MTLK_CLEANUP_END(bus_drv, MTLK_OBJ_PTR(bus_obj));
}

static int __init
_mtlk_bus_drv_request_irq(mtlk_pci_drv_t *obj, struct pci_dev *dev)
{
  int retval;

  retval = request_irq(dev->irq, &_mtlk_bus_drv_interrupt_handler,
    IRQF_SHARED, DRV_NAME, obj);

  if(0 != retval)
    ELOG_DD("Failed to allocate PCI interrupt %d, error code: %d", dev->irq, retval);

  return retval;
}

static int __init
_mtlk_bus_drv_setup_interrupts(mtlk_pci_drv_t *obj, struct pci_dev *dev)
{
#ifdef MTCFG_LINDRV_HW_PCIE
  int retval;

  if(MTLK_CARD_PCIE == obj->card_type)
  {
    retval = pci_enable_msi(dev);
    if(0 != retval)
    {
      ELOG_D("Failed to enable MSI interrupts for the device, error code: %d", retval);
      return retval;
    }
  }
#endif

  return 0;
}

static void* __init
_mtlk_bus_drv_map_resource(struct pci_dev *dev, int res_id)
{

  void __iomem *ptr = pcim_iomap_table(dev)[res_id];

if (ptr == NULL){
	ELOG_D("Failed to enable pcim_iomap_table() for BAR=%i", res_id);
	}


  ILOG2_DHHP("BAR%d=0x%llX Len=0x%llX VA=0x%p", res_id, 
               (uint64) pci_resource_start(dev, res_id), 
               (uint64) pci_resource_len(dev, res_id),
               ptr);

  return ptr;
};

static int __init
_mtlk_bus_drv_init(mtlk_pci_drv_t *bus_obj, struct pci_dev *dev, const struct pci_device_id *ent)
{
  int result;
  mtlk_hw_mmb_card_cfg_t card_cfg;
  mtlk_vap_handle_t master_vap_handle;
  mtlk_hw_api_t *hw_api;

  memset(bus_obj, 0, sizeof(*bus_obj));
  bus_obj->dev = dev;
  bus_obj->card_type = (mtlk_card_type_t) ent->driver_data;

  MTLK_ASSERT(_known_card_type(bus_obj->card_type));

  MTLK_INIT_TRY(bus_drv, MTLK_OBJ_PTR(bus_obj))
    MTLK_INIT_STEP_EX(bus_drv, BUS_ENABLE_DEVICE, MTLK_OBJ_PTR(bus_obj),
                      pcim_enable_device, (dev), result, 0 == result, 
                      MTLK_ERR_UNKNOWN);

    pci_set_drvdata(dev, bus_obj);

    MTLK_INIT_STEP_EX(bus_drv, BUS_SET_DMA_MASK, MTLK_OBJ_PTR(bus_obj),
                      pci_set_dma_mask, (dev, PCI_SUPPORTED_DMA_MASK),
                      result, 0 == result,
                      MTLK_ERR_UNKNOWN);

    pci_set_master(dev);

/*Request iomem regions*/
    result = pcim_iomap_regions(dev,0x01 | 0x02,"mtlk");
    if (result) {
        dev_err(&dev->dev,"Failed to request and map BAR's.\n");
        return result;
    }

    MTLK_INIT_STEP_EX(bus_drv, BUS_MAP_BAR0, MTLK_OBJ_PTR(bus_obj),
                      _mtlk_bus_drv_map_resource, (dev, 0),
                      bus_obj->bar0, NULL != bus_obj->bar0,
                      MTLK_ERR_UNKNOWN);

    MTLK_INIT_STEP_EX(bus_drv, BUS_MAP_BAR1, MTLK_OBJ_PTR(bus_obj),
                      _mtlk_bus_drv_map_resource, (dev, 1),
                      bus_obj->bar1, NULL != bus_obj->bar1,
                      MTLK_ERR_UNKNOWN);

    MTLK_INIT_STEP_EX(bus_drv, BUS_SETUP_INTERRUPTS, MTLK_OBJ_PTR(bus_obj),
                      _mtlk_bus_drv_setup_interrupts, (bus_obj, dev),
                      result, 0 == result,
                      MTLK_ERR_UNKNOWN);

    tasklet_init(&bus_obj->mmb_tasklet, _mtlk_bus_drv_tasklet, (unsigned long)bus_obj);

    MTLK_INIT_STEP_EX(bus_drv, BUS_CREATE_VAP_MANAGER, MTLK_OBJ_PTR(bus_obj),
                      mtlk_vap_manager_create,
                      (bus_obj, ap[mtlk_hw_mmb_get_cards_no(&mtlk_mmb_obj)]),
                      bus_obj->vap_manager, NULL != bus_obj->vap_manager, MTLK_ERR_UNKNOWN);

    card_cfg.ccr               = NULL;
    card_cfg.pas               = bus_obj->bar1;
    card_cfg.parent_irbd       = mtlk_dfg_get_driver_irbd();
    card_cfg.parent_wss        = mtlk_dfg_get_driver_wss();

    MTLK_INIT_STEP_EX(bus_drv, BUS_MMB_ADD_CARD, MTLK_OBJ_PTR(bus_obj),
                      mtlk_hw_mmb_add_card, (&mtlk_mmb_obj, &card_cfg),
                      hw_api, NULL != hw_api, MTLK_ERR_UNKNOWN);

    mtlk_vap_manager_set_hw_api(bus_obj->vap_manager, hw_api);

    bus_obj->hw = mtlk_vap_manager_get_hw(bus_obj->vap_manager);

    MTLK_INIT_STEP(bus_drv, BUS_CREATE_VAP, MTLK_OBJ_PTR(bus_obj),
                      mtlk_vap_manager_create_vap,
                      (bus_obj->vap_manager, &master_vap_handle, MTLK_MASTER_VAP_ID));

    MTLK_INIT_STEP(bus_drv, BUS_CREATE_CCR, MTLK_OBJ_PTR(bus_obj), mtlk_ccr_init,
                   (&bus_obj->ccr, bus_obj->card_type, bus_obj->hw,
                   bus_obj, bus_obj->bar0, bus_obj->bar1) );

    MTLK_INIT_STEP(bus_drv, BUS_MMB_INIT_CARD, MTLK_OBJ_PTR(bus_obj),
                   mtlk_hw_mmb_init_card, (bus_obj->hw, bus_obj->vap_manager, &bus_obj->ccr));

    MTLK_INIT_STEP_EX(bus_drv, BUS_REQUEST_IRQ, MTLK_OBJ_PTR(bus_obj),
                      _mtlk_bus_drv_request_irq, (bus_obj, dev),
                      result, 0 == result, MTLK_ERR_UNKNOWN);
    ILOG2_SD("%s IRQ 0x%x", DRV_NAME, dev->irq);

    MTLK_INIT_STEP(bus_drv, BUS_MMB_START_CARD, MTLK_OBJ_PTR(bus_obj),
                   mtlk_hw_mmb_start_card, (bus_obj->hw));

    MTLK_INIT_STEP_VOID(bus_drv, BUS_PREPARE_START_VAPS, MTLK_OBJ_PTR(bus_obj),
                   mtlk_vap_manager_prepare_start, (bus_obj->vap_manager,
                                                    HANDLE_T(mtlk_hw_mmb_get_txmm(bus_obj->hw)),
                                                    HANDLE_T(mtlk_hw_mmb_get_txdm(bus_obj->hw))));

    MTLK_INIT_STEP_VOID(bus_drv, BUS_START_MAC_EVENTS, MTLK_OBJ_PTR(bus_obj),
                        MTLK_NOACTION, ());

    MTLK_INIT_STEP(bus_drv, BUS_START_VAPS, MTLK_OBJ_PTR(bus_obj),
                   mtlk_vap_start, (master_vap_handle, MTLK_VAP_MASTER_INTERFACE));

    MTLK_INIT_STEP_VOID(bus_drv, BUS_CORE_PREPARE_STOP, MTLK_OBJ_PTR(bus_obj),
                        MTLK_NOACTION, ());
  MTLK_INIT_FINALLY(bus_drv, MTLK_OBJ_PTR(bus_obj))    
  MTLK_INIT_RETURN(bus_drv, MTLK_OBJ_PTR(bus_obj), _mtlk_bus_drv_cleanup, (bus_obj, MTLK_VAP_MASTER_INTERFACE))
}

static int __init
_mtlk_bus_drv_probe(struct pci_dev *dev, const struct pci_device_id *ent)
{
  mtlk_pci_drv_t *obj = NULL;

  mtlk_fast_mem_print_info();

  if (NULL == (obj = mtlk_fast_mem_alloc(MTLK_FM_USER_PCIDRV,
                                         sizeof(mtlk_pci_drv_t)))) {
    goto err_bus_alloc;
  }

  if(MTLK_ERR_OK != _mtlk_bus_drv_init(obj, dev, ent)) {
    goto err_bus_init;
  }

  return 0;

err_bus_init:
  mtlk_fast_mem_free(obj);
err_bus_alloc:
  return -ENODEV;
}

static void 
_mtlk_bus_drv_remove(struct pci_dev *pdev)
{
  mtlk_pci_drv_t *bus_drv = pci_get_drvdata(pdev);

  ILOG2_S("%s CleanUp", pci_name(pdev));

  _mtlk_bus_drv_cleanup(bus_drv, MTLK_VAP_MASTER_INTERFACE);
  mtlk_fast_mem_free(bus_drv);

  ILOG2_S("%s CleanUp finished", pci_name(pdev));
}

static struct pci_device_id mtlk_dev_tbl[] = {
#ifdef MTCFG_LINDRV_HW_PCIE
  { MTLK_VENDOR_ID,     HYPERION_III_PCIE_DEVICE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MTLK_CARD_PCIE},
#endif

#ifdef MTCFG_LINDRV_HW_PCIG3
  { MTLK_VENDOR_ID,     HYPERION_III_PCI_DEVICE, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MTLK_CARD_PCIG3},
#endif
  { 0,}
};

MODULE_DEVICE_TABLE(pci, mtlk_dev_tbl);

static struct pci_driver mtlk_bus_driver = {
  .name     = "mtlk",
  .id_table = mtlk_dev_tbl,
  .probe    = _mtlk_bus_drv_probe,
  .remove   = _mtlk_bus_drv_remove,
};

struct mtlk_drv_state
{
  int os_res;
  int init_res;
  MTLK_DECLARE_INIT_STATUS;
};

static struct mtlk_drv_state drv_state = {0};

MTLK_INIT_STEPS_LIST_BEGIN(bus_drv_mod)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv_mod, DRV_DFG_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv_mod, DRV_MODULE_REGISTER)
MTLK_INIT_INNER_STEPS_BEGIN(bus_drv_mod)
MTLK_INIT_STEPS_LIST_END(bus_drv_mod);

static void
__mtlk_bus_drv_cleanup_module (void)
{
  MTLK_CLEANUP_BEGIN(bus_drv_mod, MTLK_OBJ_PTR(&drv_state))
    MTLK_CLEANUP_STEP(bus_drv_mod, DRV_MODULE_REGISTER, MTLK_OBJ_PTR(&drv_state),
                      pci_unregister_driver, (&mtlk_bus_driver));
    MTLK_CLEANUP_STEP(bus_drv_mod, DRV_DFG_INIT, MTLK_OBJ_PTR(&drv_state),
                      mtlk_dfg_cleanup, ());
  MTLK_CLEANUP_END(bus_drv_mod, MTLK_OBJ_PTR(&drv_state))
}

static int __init
__mtlk_bus_drv_init_module (void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22) 
#define pci_init_f pci_module_init
#else
#define pci_init_f pci_register_driver
#endif

  log_osdep_reset_levels(debug);

  MTLK_INIT_TRY(bus_drv_mod, MTLK_OBJ_PTR(&drv_state))
    MTLK_INIT_STEP(bus_drv_mod, DRV_DFG_INIT, MTLK_OBJ_PTR(&drv_state),
                   mtlk_dfg_init, ());
    MTLK_INIT_STEP_EX(bus_drv_mod, DRV_MODULE_REGISTER, MTLK_OBJ_PTR(&drv_state),
                      pci_init_f, (&mtlk_bus_driver),
                      drv_state.os_res, 0 == drv_state.os_res, MTLK_ERR_NO_RESOURCES);
  MTLK_INIT_FINALLY(bus_drv_mod, MTLK_OBJ_PTR(&drv_state))
  MTLK_INIT_RETURN(bus_drv_mod, MTLK_OBJ_PTR(&drv_state), __mtlk_bus_drv_cleanup_module, ())
}

static int __init
_mtlk_bus_drv_init_module (void)
{
  drv_state.init_res = __mtlk_bus_drv_init_module();
  return (drv_state.init_res == MTLK_ERR_OK)?0:drv_state.os_res;
}

static void __exit
_mtlk_bus_drv_cleanup_module (void)
{
  ILOG2_V("Cleanup");
  if (drv_state.init_res == MTLK_ERR_OK) {
    /* Call cleanup only if init succeeds, 
     * otherwise it will be called by macros on init itself 
     */
    __mtlk_bus_drv_cleanup_module();
  }
}

module_init(_mtlk_bus_drv_init_module);
module_exit(_mtlk_bus_drv_cleanup_module);

