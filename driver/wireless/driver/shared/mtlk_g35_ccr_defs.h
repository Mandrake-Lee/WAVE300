/* $Id: mtlk_g35_ccr_defs.h 11899 2011-11-06 09:33:46Z vugenfir $ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DEFS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DEFS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DEFS

static __INLINE  void
_mtlk_g35_ccr_cleanup(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);
}

static __INLINE int
_mtlk_g35_ccr_init(_mtlk_g35_ccr_t *g35_mem, mtlk_ahb_drv_t *ahb_drv,
                    void* bar0, void* bar1) 
{
  MTLK_ASSERT(NULL != g35_mem);
  
  if (NULL == bar1)
    return MTLK_ERR_PARAMS;

  g35_mem->pas = (struct g35_pas_map*) bar1;
  g35_mem->ahb_drv = ahb_drv;

  /* This is a state of cpu on power-on */
  g35_mem->current_ucpu_state = 
    G3_CPU_RAB_Active | G3_CPU_RAB_DEBUG;

  g35_mem->next_boot_mode = G3_CPU_RAB_DEBUG;

  return MTLK_ERR_OK;
}


#ifdef MTCFG_TSF_TIMER_ACCESS_ENABLED
static __INLINE void
_mtlk_g35_read_hw_timestamp(_mtlk_g35_ccr_t *g35_mem, uint32 *low, uint32 *high)
{
  MTLK_ASSERT(NULL != g35_mem);

  __ccr_readl(&g35_mem->pas->tsf_timer_low, *low);
  __ccr_readl(&g35_mem->pas->tsf_timer_high, *high);
}
#endif /* MTCFG_TSF_TIMER_ACCESS_ENABLED */


static __INLINE  void
_mtlk_g35_enable_interrupts(_mtlk_g35_ccr_t *g35_mem)
{
#define MTLK_GEN35_ENABLE_INTERRUPT     (0x1)

#ifdef MTLK_G35_NPU
  MTLK_ASSERT(NULL != g35_mem);

  __ccr_writel(MTLK_GEN35_ENABLE_INTERRUPT, 
               &g35_mem->pas->SH_REG_BLOCK.interrupt_enable);
#else
  uint32 reg;

  MTLK_ASSERT(NULL != g35_mem);

  __ccr_writel(MTLK_GEN35_ENABLE_INTERRUPT,
               &g35_mem->pas->HTEXT.host_irq_mask);

  __ccr_readl(&g35_mem->pas->RAB.enable_phi_interrupt, reg);
  __ccr_writel(reg | MTLK_GEN35_ENABLE_INTERRUPT,
               &g35_mem->pas->RAB.enable_phi_interrupt);
#endif
}

static __INLINE  void
_mtlk_g35_disable_interrupts(_mtlk_g35_ccr_t *g35_mem)
{
#define MTLK_GEN35_DISABLE_INTERRUPT     (0x0)

  MTLK_ASSERT(NULL != g35_mem);

#ifdef MTLK_G35_NPU
  __ccr_writel(MTLK_GEN35_DISABLE_INTERRUPT, 
               &g35_mem->pas->SH_REG_BLOCK.interrupt_enable);
#else
  __ccr_writel(MTLK_GEN35_DISABLE_INTERRUPT,
               &g35_mem->pas->HTEXT.host_irq_mask);
#endif
}

/* WARNING                                                      */
/* Currently we do not utilize (and have no plans to utilize)   */ 
/* interrupt sharing on Gen 3.5 platform. However, Gen 3.5      */
/* hardware supports this ability.                              */
/* For now, in all *_if_pending functions we assume there is no */
/* interrupt sharing, so we may not check whether arrived       */
/* interrupt is our. This save us CPU cycles and code lines.    */
/* In case interrupt sharing will be used, additional checks    */
/* for interrupt appurtenance to be added into these functions. */

static __INLINE BOOL
_mtlk_g35_clear_interrupts_if_pending(_mtlk_g35_ccr_t *g35_mem)
{
#define MTLK_GEN35_CLEAR_INTERRUPT       (0x1)

  MTLK_ASSERT(NULL != g35_mem);

#ifdef MTLK_G35_NPU
  __ccr_writel(MTLK_GEN35_CLEAR_INTERRUPT, 
               &g35_mem->pas->SH_REG_BLOCK.interrupt_clear);
  return TRUE;
#else
  __ccr_writel(MTLK_GEN35_CLEAR_INTERRUPT, 
               &g35_mem->pas->RAB.phi_interrupt_clear);
  return TRUE;
#endif
}

static __INLINE BOOL
_mtlk_g35_disable_interrupts_if_pending(_mtlk_g35_ccr_t *g35_mem)
{
  _mtlk_g35_disable_interrupts(g35_mem);
  return TRUE;
}

static __INLINE int
_mtlk_g35_handle_interrupt(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);

  return mtlk_bus_drv_handle_interrupt(g35_mem->ahb_drv);
}

static __INLINE  void
_mtlk_g35_initiate_doorbell_inerrupt(_mtlk_g35_ccr_t *g35_mem)
{
#define MTLK_GEN35_GENERATE_DOOR_BELL    (0x1)

  MTLK_ASSERT(NULL != g35_mem);
  __ccr_writel(MTLK_GEN35_GENERATE_DOOR_BELL, 
               &g35_mem->pas->RAB.upi_interrupt);
}

static __INLINE  void
_mtlk_g35_boot_from_bus(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);
  /* No boot from bus on G35 */
  MTLK_UNREFERENCED_PARAM(g35_mem);
}

static __INLINE  void
_mtlk_g35_clear_boot_from_bus(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);
  /* No boot from bus on G35 */
  MTLK_UNREFERENCED_PARAM(g35_mem);
}

static __INLINE  void
_mtlk_g35_switch_to_iram_boot(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);

  g35_mem->next_boot_mode = G3_CPU_RAB_IRAM;
}

static __INLINE  void
_mtlk_g35_exit_debug_mode(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);
  /* Not needed on G35 */
  MTLK_UNREFERENCED_PARAM(g35_mem);
}

static __INLINE void
__g35_open_secure_write_register(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);

  __ccr_writel(0xAAAA,
               &g35_mem->pas->RAB.secure_write_register);
  __ccr_writel(0x5555,
               &g35_mem->pas->RAB.secure_write_register);
}

static __INLINE  void
_mtlk_g35_put_cpus_to_reset(_mtlk_g35_ccr_t *g35_mem)
{
  uint8 new_cpu_boot_mode = G3_CPU_RAB_Override |
                            G3_CPU_RAB_IRAM;

  MTLK_ASSERT(NULL != g35_mem);

  g35_mem->current_ucpu_state =
    G3_CPU_RAB_IRAM;

  __g35_open_secure_write_register(g35_mem);
  __ccr_writel(new_cpu_boot_mode,
               &g35_mem->pas->RAB.cpu_control_register);

  /* CPU requires time to go to  reset, so we       */
  /* MUST wait here before writing something else   */
  /* to CPU control register. In other case this    */
  /* may lead to unpredictable results.             */
  mtlk_osal_msleep(20);
}

static __INLINE  void
_mtlk_g35_put_ctl_to_reset(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);

  /* Disable RX */
  __ccr_resetl(&g35_mem->pas->PAC.rx_control, 
               G3_MASK_RX_ENABLED);
}

static __INLINE  void
_mtlk_g35_power_on_cpus(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);
  /* Not needed on G35 */
  MTLK_UNREFERENCED_PARAM(g35_mem);
}

static __INLINE  void
_mtlk_g35_release_cpus_reset(_mtlk_g35_ccr_t *g35_mem)
{
  uint32 new_cpu_state;

  MTLK_ASSERT(NULL != g35_mem);
  MTLK_ASSERT(G3_CPU_RAB_IRAM == g35_mem->next_boot_mode);

  g35_mem->current_ucpu_state = 
    G3_CPU_RAB_Active | g35_mem->next_boot_mode;

  new_cpu_state = ( g35_mem->current_ucpu_state | G3_CPU_RAB_Override );

  __g35_open_secure_write_register(g35_mem);

  __ccr_writel(new_cpu_state,
               &g35_mem->pas->RAB.cpu_control_register);

  /* CPU requires time to change its state, so we   */
  /* MUST wait here before writing something else   */
  /* to CPU control register. In other case this    */
  /* may lead to unpredictable results.             */
  mtlk_osal_msleep(10);
}

static __INLINE BOOL
_mtlk_g35_check_bist(_mtlk_g35_ccr_t *g35_mem, uint32 *bist_result)
{
  MTLK_ASSERT(NULL != g35_mem);
  /* Not needed on G35 */
  MTLK_UNREFERENCED_PARAM(g35_mem);

  *bist_result = 0;
  return TRUE;
}

#ifndef MTLK_G35_NPU
#include <asm/ifx/ifx_regs.h>
#endif

static __INLINE  void
_mtlk_g35_release_ctl_from_reset(_mtlk_g35_ccr_t *g35_mem)
{
  extern int bb_cpu_ddr_mb_number;
#ifdef MTLK_G35_NPU
  MTLK_ASSERT(NULL != g35_mem);
  __ccr_writel(bb_cpu_ddr_mb_number,
               &g35_mem->pas->SH_REG_BLOCK.bb_ddr_offset_mb);
#else
#define BBCPU_PAGE_REG_PAD  MTLK_BFIELD_INFO(0, 4)
#define BBCPU_PAGE_REG_VAL  MTLK_BFIELD_INFO(4, 12)
#define BBCPU_MASK_REG_PAD  MTLK_BFIELD_INFO(16, 4)
#define BBCPU_MASK_REG_VAL  MTLK_BFIELD_INFO(20, 12)

#define BBCPU_MASK -1

  uint32 bbcpu_reg_val = 
    MTLK_BFIELD_VALUE(BBCPU_PAGE_REG_VAL, bb_cpu_ddr_mb_number, uint32) |
    MTLK_BFIELD_VALUE(BBCPU_MASK_REG_VAL, BBCPU_MASK, uint32);

  mtlk_osal_emergency_print("BBCPU Reg: offs=0x%04x (%p) val=0x%08x (offset_mb=%d)",
      MTLK_OFFSET_OF(struct g35_pas_map, HTEXT.ahb_arb_bbcpu_page_reg) - MTLK_OFFSET_OF(struct g35_pas_map, HTEXT),
      &g35_mem->pas->HTEXT.ahb_arb_bbcpu_page_reg,
      bbcpu_reg_val, bb_cpu_ddr_mb_number);

  __ccr_writel(bbcpu_reg_val,
               &g35_mem->pas->HTEXT.ahb_arb_bbcpu_page_reg);

  mtlk_osal_emergency_print("Enabling interrupt (%p)", IFX_ICU_IM0_IER);
  (*IFX_ICU_IM0_IER) |= 0x00040000;
  mtlk_osal_emergency_print("Interrupt enabled");
#endif

}
