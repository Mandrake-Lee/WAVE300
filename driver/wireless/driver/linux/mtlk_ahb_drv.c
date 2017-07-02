#include "mtlkinc.h"
#include "hw_mmb.h"
#include "drvver.h"
#include "mtlk_df.h"
#include "mtlk_fast_mem.h"
#include "mtlk_vap_manager.h"

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/device.h>
#ifdef MTCFG_PLATFORM_GEN35FPGA
#include <mtlk_interrupt.h>
#endif

#ifdef MTCFG_BENCHMARK_TOOLS
#include "mtlk_dbg.h"
#endif

#define LOG_LOCAL_GID   GID_AHBDRV
#define LOG_LOCAL_FID   1

#define MTLK_MEM_BAR0_INDEX  0
#define MTLK_MEM_BAR1_INDEX  1

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR(MTLK_COPYRIGHT);
MODULE_LICENSE("GPL");

static int ap[MTLK_HW_MAX_CARDS] = {0};
static int debug                 = 0;

#define MTLK_G35_DEFAULT_CPU_MB_NUMBER (31) /* Default number of megabyte of  */
                                            /* DDR that is mapped into BB CPU */
                                            /* internal memory region         */
int bb_cpu_ddr_mb_number         = MTLK_G35_DEFAULT_CPU_MB_NUMBER;

#ifdef MTLK_DEBUG
extern int step_to_fail;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
MODULE_PARM(ap, "1-" __MODULE_STRING(MTLK_HW_MAX_CARDS) "i");
MODULE_PARM(debug, "i");
MODULE_PARM(bb_cpu_ddr_mb_number, "i");
#ifdef MTLK_DEBUG
MODULE_PARM(step_to_fail, "i");
#endif
#else
module_param_array(ap, int, NULL, 0);
module_param(debug, int, 0);
module_param(bb_cpu_ddr_mb_number, int, 0);
#ifdef MTLK_DEBUG
module_param(step_to_fail, int, 0);
#endif
#endif

MODULE_PARM_DESC(ap, "Make an access point");
MODULE_PARM_DESC(debug, "Debug level");
MODULE_PARM_DESC(bb_cpu_ddr_mb_number, "Offset of DDR memory region allocated for wireless card CPU (in megabytes)");
#ifdef MTLK_DEBUG
MODULE_PARM_DESC(step_to_fail, "Init step to simulate fail");
#endif

/**************************************************************/

/* TODO: DEV_DF made external for init in DFG will be fixed */
mtlk_hw_mmb_t mtlk_mmb_obj;

static void
_mtlk_bus_drv_tasklet(unsigned long param) // bottom half of PCI irq handling
{
  mtlk_ahb_drv_t *bus_drv = (mtlk_ahb_drv_t*)param;
  mtlk_hw_mmb_deferred_handler(bus_drv->hw);
}

int __MTLK_IFUNC
mtlk_bus_drv_handle_interrupt(mtlk_ahb_drv_t *bus_drv)
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
mtlk_bus_drv_get_name(mtlk_ahb_drv_t *bus_drv)
{
  return dev_name(&bus_drv->dev->dev);
}

mtlk_card_type_t __MTLK_IFUNC
mtlk_bus_drv_get_card_type(mtlk_ahb_drv_t *bus_drv)
{
  return bus_drv->card_type;
}

struct device * __MTLK_IFUNC
mtlk_bus_drv_get_device (mtlk_ahb_drv_t *bus_drv)
{
  return &bus_drv->dev->dev;
}


static irqreturn_t
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
_mtlk_bus_drv_interrupt_handler(int irq, void *ptr, struct pt_regs *regs)
#else
_mtlk_bus_drv_interrupt_handler(int irq, void *ptr)
#endif
{
  /* Multiple returns are used to support void return */
  mtlk_ahb_drv_t *obj;

  obj = (mtlk_ahb_drv_t *)ptr;
  MTLK_UNREFERENCED_PARAM(irq);

  if(MTLK_ERR_OK == mtlk_ccr_handle_interrupt(&obj->ccr)) {
    return IRQ_HANDLED;
  }
  else {
    return IRQ_NONE;
  }
}

MTLK_INIT_STEPS_LIST_BEGIN(bus_drv)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_REQ_BAR0_REGION)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_MAP_BAR0)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_REQ_BAR1_REGION)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv, BUS_MAP_BAR1)
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
_mtlk_bus_drv_free_mem_region(struct resource* mem_resource)
{
  MTLK_ASSERT(NULL != mem_resource);
  
  release_mem_region(mem_resource->start, 
                     mem_resource->end - mem_resource->start + 1);
}

static struct resource* __init
_mtlk_bus_setup_mem_region(struct platform_device *pdev, 
                           uint32 resource_index)
{
  size_t mem_resource_size;
  struct resource* mem_resource; 

  MTLK_ASSERT(NULL != pdev);

  mem_resource = platform_get_resource(pdev, IORESOURCE_MEM, resource_index);

  if(NULL == mem_resource)
      return NULL;

  mem_resource_size = mem_resource->end - mem_resource->start + 1;

  if(NULL == request_mem_region(mem_resource->start, 
                                mem_resource_size, pdev->name))
    return NULL;
  else return mem_resource;
}

static void* __init
_mtlk_bus_drv_map_mem_region(struct resource* res)
{
  void* virt_addr = ioremap(res->start, res->end - res->start + 1);

  ILOG2_PPD("Memory block: PA: 0x%p , VA: 0x%p, Len=0x%x",
            (void*)  res->start, virt_addr,
            (uint32) res->end - res->start + 1);

  return virt_addr;
}

static void
_mtlk_bus_drv_unmap_mem_region(void* virt_addr)
{
  iounmap(virt_addr);
}

static void
_mtlk_bus_drv_free_irq(mtlk_ahb_drv_t *obj)
{
#ifdef MTLK_G35_NPU
  mtlk_unregister_irq(MTLK_WIRELESS_IRQ_IN_INDEX, MTLK_WIRELESS_IRQ_OUT_INDEX,
                      obj->dev);
#else
  free_irq(obj->irq, obj);
#endif
}

static void
_mtlk_bus_drv_cleanup(mtlk_ahb_drv_t *bus_obj, mtlk_vap_manager_interface_e intf)
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
                      _mtlk_bus_drv_free_irq, (bus_obj));
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

    MTLK_CLEANUP_STEP(bus_drv, BUS_MAP_BAR1, MTLK_OBJ_PTR(bus_obj), 
                      _mtlk_bus_drv_unmap_mem_region, (bus_obj->bar1_va));
    MTLK_CLEANUP_STEP(bus_drv, BUS_REQ_BAR1_REGION, MTLK_OBJ_PTR(bus_obj), 
                      _mtlk_bus_drv_free_mem_region, (bus_obj->bar1));
    MTLK_CLEANUP_STEP(bus_drv, BUS_MAP_BAR0, MTLK_OBJ_PTR(bus_obj), 
                      _mtlk_bus_drv_unmap_mem_region, (bus_obj->bar0_va));
    MTLK_CLEANUP_STEP(bus_drv, BUS_REQ_BAR0_REGION, MTLK_OBJ_PTR(bus_obj), 
                      _mtlk_bus_drv_free_mem_region, (bus_obj->bar0));
  MTLK_CLEANUP_END(bus_drv, MTLK_OBJ_PTR(bus_obj));
}

static int __init
_mtlk_bus_drv_request_irq(mtlk_ahb_drv_t *obj, struct platform_device *pdev)
{

#ifdef MTLK_G35_NPU
  mtlk_register_irq(MTLK_WIRELESS_IRQ_IN_INDEX, MTLK_WIRELESS_IRQ_OUT_INDEX, 
                    _mtlk_bus_drv_interrupt_handler, IRQF_DISABLED,
                    "mtlk_wls", obj, metalink_wls_irq);

  return MTLK_ERR_OK;
#else
  int retval = MTLK_ERR_UNKNOWN;
  int i;

  /* find IRQ from resource list */
  for(i = 0; i< pdev->num_resources; i++) {
    if(pdev->resource[i].flags & IORESOURCE_IRQ) {
      obj->irq = pdev->resource[i].start;
      ELOG_D("Found device IRQ: %d", obj->irq);
      retval = MTLK_ERR_OK;
    }
  }

  if(MTLK_ERR_OK != retval) {
    ELOG_D("Failed to find interrupt for the device, error code: %d", retval);
    goto end;
  }
  /* requrst IRQ */

  retval = request_irq(obj->irq, _mtlk_bus_drv_interrupt_handler,
    IRQF_SHARED, DRV_NAME, obj);

  if (retval < 0) {
    ELOG_DD("Failed to allocate interrupt %d, error code: %d", obj->irq, retval);
    retval = MTLK_ERR_UNKNOWN;
  }

end:
  return retval;
#endif
}

static int __init
_mtlk_bus_drv_init(mtlk_ahb_drv_t* bus_obj, struct platform_device *dev)
{
  mtlk_hw_mmb_card_cfg_t card_cfg;
  mtlk_vap_handle_t master_vap_handle;
  mtlk_hw_api_t *hw_api;

  memset(bus_obj, 0, sizeof(*bus_obj));
  bus_obj->dev = dev;
  bus_obj->card_type = MTLK_CARD_AHBG35;

  MTLK_ASSERT(_known_card_type(bus_obj->card_type));

  MTLK_INIT_TRY(bus_drv, MTLK_OBJ_PTR(bus_obj))
    platform_set_drvdata(dev, bus_obj);
    MTLK_INIT_STEP_EX(bus_drv, BUS_REQ_BAR0_REGION, MTLK_OBJ_PTR(bus_obj),
                      _mtlk_bus_setup_mem_region, (dev, MTLK_MEM_BAR0_INDEX),
                      bus_obj->bar0, NULL != bus_obj->bar0,
                      MTLK_ERR_UNKNOWN);

    MTLK_INIT_STEP_EX(bus_drv, BUS_MAP_BAR0, MTLK_OBJ_PTR(bus_obj),
                      _mtlk_bus_drv_map_mem_region, (bus_obj->bar0),
                      bus_obj->bar0_va, NULL != bus_obj->bar0_va,
                      MTLK_ERR_UNKNOWN);

    MTLK_INIT_STEP_EX(bus_drv, BUS_REQ_BAR1_REGION, MTLK_OBJ_PTR(bus_obj),
                      _mtlk_bus_setup_mem_region, (dev, MTLK_MEM_BAR1_INDEX),
                      bus_obj->bar1, NULL != bus_obj->bar1,
                      MTLK_ERR_UNKNOWN);

    MTLK_INIT_STEP_EX(bus_drv, BUS_MAP_BAR1, MTLK_OBJ_PTR(bus_obj),
                      _mtlk_bus_drv_map_mem_region, (bus_obj->bar1),
                      bus_obj->bar1_va, NULL != bus_obj->bar1_va,
                      MTLK_ERR_UNKNOWN);

    tasklet_init(&bus_obj->mmb_tasklet, _mtlk_bus_drv_tasklet, (unsigned long)bus_obj);

    MTLK_INIT_STEP_EX(bus_drv, BUS_CREATE_VAP_MANAGER, MTLK_OBJ_PTR(bus_obj),
                      mtlk_vap_manager_create,
                      (bus_obj, ap[mtlk_hw_mmb_get_cards_no(&mtlk_mmb_obj)]),
                      bus_obj->vap_manager, NULL != bus_obj->vap_manager, MTLK_ERR_UNKNOWN);

    card_cfg.ccr               = NULL;
    card_cfg.pas               = (unsigned char*) bus_obj->bar1_va;
    card_cfg.cpu_ddr           = (unsigned char*) bus_obj->bar0_va;
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
                   bus_obj, (void*) bus_obj->bar0_va, (void*) bus_obj->bar1_va) );

    MTLK_INIT_STEP(bus_drv, BUS_MMB_INIT_CARD, MTLK_OBJ_PTR(bus_obj),
                   mtlk_hw_mmb_init_card, (bus_obj->hw, bus_obj->vap_manager, &bus_obj->ccr));

    MTLK_INIT_STEP(bus_drv, BUS_REQUEST_IRQ, MTLK_OBJ_PTR(bus_obj),
                   _mtlk_bus_drv_request_irq, (bus_obj, dev));

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
_mtlk_bus_drv_probe(struct platform_device *dev)
{
  mtlk_ahb_drv_t *obj = NULL;

  mtlk_fast_mem_print_info();

  if (NULL == (obj = mtlk_fast_mem_alloc(MTLK_FM_USER_PCIDRV,
                                         sizeof(mtlk_ahb_drv_t)))) {
    goto err_bus_alloc;
  }

  if(MTLK_ERR_OK != _mtlk_bus_drv_init(obj, dev)) {
    goto err_bus_init;
  }

  return 0;

err_bus_init:
  mtlk_fast_mem_free(obj);
err_bus_alloc:
  return -ENODEV;
}

static int __devexit
_mtlk_bus_drv_remove(struct platform_device *dev)
{
  mtlk_ahb_drv_t *bus_drv = platform_get_drvdata(dev);

  ILOG2_S("%s CleanUp", dev_name(&dev->dev));

  _mtlk_bus_drv_cleanup(bus_drv, MTLK_VAP_MASTER_INTERFACE);
  mtlk_fast_mem_free(bus_drv);

  ILOG2_S("%s CleanUp finished", dev_name(&dev->dev));

  return 0;
}

static struct platform_driver mtlk_bus_driver = {
  .probe    = _mtlk_bus_drv_probe,
  .remove   = __devexit_p(_mtlk_bus_drv_remove),
  .driver   = {
    .name   = "mtlk",
  },
};

/* TODO: DEV_DF made external for init in DFG will be fixed */
mtlk_hw_mmb_cfg_t mtlk_pci_mmb_cfg =   {
  .bist_check_permitted  = 1,
  .no_pll_write_delay_us = 0,
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
                      platform_driver_unregister, (&mtlk_bus_driver));
    MTLK_CLEANUP_STEP(bus_drv_mod, DRV_DFG_INIT, MTLK_OBJ_PTR(&drv_state),
                      mtlk_dfg_cleanup, ());
  MTLK_CLEANUP_END(bus_drv_mod, MTLK_OBJ_PTR(&drv_state))
}

static int __init
__mtlk_bus_drv_init_module (void)
{
  log_osdep_reset_levels(debug);

  MTLK_INIT_TRY(bus_drv_mod, MTLK_OBJ_PTR(&drv_state))
    MTLK_INIT_STEP(bus_drv_mod, DRV_DFG_INIT, MTLK_OBJ_PTR(&drv_state),
                   mtlk_dfg_init, ());
    MTLK_INIT_STEP_EX(bus_drv_mod, DRV_MODULE_REGISTER, MTLK_OBJ_PTR(&drv_state), 
                      platform_driver_register, (&mtlk_bus_driver),
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

