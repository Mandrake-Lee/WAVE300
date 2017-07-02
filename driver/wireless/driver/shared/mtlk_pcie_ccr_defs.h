/* $Id: mtlk_pcie_ccr_defs.h 12137 2011-12-08 14:54:19Z nayshtut $ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DEFS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DEFS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DEFS

#define LOG_LOCAL_GID   GID_PCIE_CCR_DEFS
#define LOG_LOCAL_FID   0

MTLK_INIT_STEPS_LIST_BEGIN(pcie_ccr)
MTLK_INIT_INNER_STEPS_BEGIN(pcie_ccr)
MTLK_INIT_STEPS_LIST_END(pcie_ccr);

static __INLINE  void
_mtlk_pcie_ccr_cleanup(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);

  MTLK_CLEANUP_BEGIN(pcie_ccr, MTLK_OBJ_PTR(pcie_mem))
  MTLK_CLEANUP_END(pcie_ccr, MTLK_OBJ_PTR(pcie_mem));
}

static __INLINE int
_mtlk_pcie_ccr_init(_mtlk_pcie_ccr_t *pcie_mem, mtlk_pci_drv_t *pci_drv,
                    void* bar0, void* bar1) 
{
  MTLK_ASSERT(NULL != pcie_mem);

  if ((NULL == bar0) || (NULL == bar1))
    return MTLK_ERR_PARAMS;

  pcie_mem->pas = (struct g3_pas_map*) bar1;
  pcie_mem->pci_drv = pci_drv;

  /* This is a state of cpu on power-on */
  pcie_mem->current_ucpu_state = 
    pcie_mem->current_lcpu_state = 
      G3_CPU_RAB_Active | G3_CPU_RAB_DEBUG;

  pcie_mem->next_boot_mode = G3_CPU_RAB_DEBUG;

  pcie_mem->irqs_enabled = TRUE;
  pcie_mem->irq_pending = FALSE;

  MTLK_INIT_TRY(pcie_ccr, MTLK_OBJ_PTR(pcie_mem))
  MTLK_INIT_FINALLY(pcie_ccr, MTLK_OBJ_PTR(pcie_mem))
  MTLK_INIT_RETURN(pcie_ccr, MTLK_OBJ_PTR(pcie_mem), _mtlk_pcie_ccr_cleanup, (pcie_mem));
}

typedef struct
{
  _mtlk_pcie_ccr_t *pcie_mem;
  BOOL call_interrupt_handler;
} ___pcie_enable_interrupts_ctx;

static void __MTLK_IFUNC
___pcie_enable_interrupts(void* data)
{
  ___pcie_enable_interrupts_ctx* ctx = 
    (___pcie_enable_interrupts_ctx*) data;

  if(!ctx->pcie_mem->irqs_enabled)
  {
    ctx->pcie_mem->irqs_enabled = TRUE;
    ctx->call_interrupt_handler = ctx->pcie_mem->irq_pending;
  }
}

#ifdef MTCFG_TSF_TIMER_ACCESS_ENABLED
static __INLINE void
  _mtlk_pcie_read_hw_timestamp(_mtlk_pcie_ccr_t *pcie_mem, uint32 *low, uint32 *high)
{
  MTLK_ASSERT(NULL != pcie_mem);

  __ccr_readl(&pcie_mem->pas->tsf_timer_low, *low);
  __ccr_readl(&pcie_mem->pas->tsf_timer_high, *high);
}
#endif /* MTCFG_TSF_TIMER_ACCESS_ENABLED */

static __INLINE  void
_mtlk_pcie_enable_interrupts(_mtlk_pcie_ccr_t *pcie_mem)
{
  ___pcie_enable_interrupts_ctx ctx;
  MTLK_ASSERT(NULL != pcie_mem);

  ctx.pcie_mem = pcie_mem;
  ctx.call_interrupt_handler = FALSE;

  mtlk_mmb_sync_isr(pcie_mem->pci_drv->hw, ___pcie_enable_interrupts, &ctx);

  if(ctx.call_interrupt_handler)
    mtlk_bus_drv_handle_interrupt(pcie_mem->pci_drv);
}

static void __MTLK_IFUNC
___pcie_disable_interrupts(void* data)
{
  _mtlk_pcie_ccr_t *pcie_mem = 
    (_mtlk_pcie_ccr_t *) data;

  if(pcie_mem->irqs_enabled)
  {
    pcie_mem->irq_pending = FALSE;
    pcie_mem->irqs_enabled = FALSE;
  }
}

static __INLINE  void
_mtlk_pcie_disable_interrupts(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);

  mtlk_mmb_sync_isr(pcie_mem->pci_drv->hw, ___pcie_disable_interrupts, pcie_mem);
}

static __INLINE BOOL
_mtlk_pcie_clear_interrupts_if_pending(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);
  /* There is no "clear" operation for MSI interrupts  */
  /* and handler is called in case of interrupt only,  */ 
  /* i.e. there is no spurious interrupts or interrupt */
  /* line sharing.                                     */
  return TRUE;
}

static __INLINE BOOL
_mtlk_pcie_disable_interrupts_if_pending(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);
  /* For MSI interrupts handler is called in case  */
  /* of interrupt only, i.e. there is no spurious  */
  /* interrupts or interrupt line sharing.         */
  _mtlk_pcie_disable_interrupts(pcie_mem);
  return TRUE;
}

static __INLINE int
_mtlk_pcie_handle_interrupt(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);

  if( pcie_mem->irqs_enabled )
  {
    return mtlk_bus_drv_handle_interrupt(pcie_mem->pci_drv);
  }
  else
  {
    pcie_mem->irq_pending = TRUE;
    return MTLK_ERR_OK;
  }
}

static __INLINE  void
_mtlk_pcie_initiate_doorbell_inerrupt(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);
  __ccr_writel(1, &pcie_mem->pas->HTEXT.door_bell);
}

static __INLINE  void
_mtlk_pcie_boot_from_bus(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);

  pcie_mem->next_boot_mode = G3_CPU_RAB_SHRAM;
}

static __INLINE  void
_mtlk_pcie_switch_to_iram_boot(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);

  pcie_mem->next_boot_mode = G3_CPU_RAB_IRAM;
}

static __INLINE  void
_mtlk_pcie_exit_debug_mode(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);
  /* Not needed on PCIE */
  MTLK_UNREFERENCED_PARAM(pcie_mem);
}

static __INLINE  void
_mtlk_pcie_clear_boot_from_bus(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);

  pcie_mem->next_boot_mode = G3_CPU_RAB_DEBUG;
}

static __INLINE  void
_mtlk_pcie_put_cpus_to_reset(_mtlk_pcie_ccr_t *pcie_mem)
{
  uint8 new_cpu_boot_mode = G3_CPU_RAB_Override |
                            G3_CPU_RAB_IRAM;

  MTLK_ASSERT(NULL != pcie_mem);
  MTLK_ASSERT(NULL != pcie_mem->pas);

  pcie_mem->current_lcpu_state =
    pcie_mem->current_ucpu_state =
      G3_CPU_RAB_IRAM;

  _mtlk_g3_open_secure_write_register(pcie_mem->pas, RAB);
  __ccr_writel(new_cpu_boot_mode | (new_cpu_boot_mode << 8), 
               &pcie_mem->pas->RAB.cpu_control_register);

  /* CPU requires time to go to  reset, so we       */
  /* MUST wait here before writing something else   */
  /* to CPU control register. In other case this    */
  /* may lead to unpredictable results.             */
  mtlk_osal_msleep(20);
}

static __INLINE  void
_mtlk_pcie_power_on_cpus(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);
  /* Not needed on PCIE */
  MTLK_UNREFERENCED_PARAM(pcie_mem);
}

static __INLINE  void
_mtlk_pcie_release_cpus_reset(_mtlk_pcie_ccr_t *pcie_mem)
{
  uint32 new_cpus_state;

  //////////////////////////////////////////////////////////////////////////
  //This is a fix for bug WLS-2621 (G3 PCIE hardware).
  //Even so there is no visible reason, this code must stay.
  _mtlk_pcie_put_cpus_to_reset(pcie_mem);
  mtlk_osal_msleep(50);
  //////////////////////////////////////////////////////////////////////////

  __mtlk_g3_set_ucpu_32k_blocks(pcie_mem->pas);

  pcie_mem->current_ucpu_state = 
    pcie_mem->current_lcpu_state = 
      G3_CPU_RAB_Active | pcie_mem->next_boot_mode;

  new_cpus_state =   ( pcie_mem->current_ucpu_state | G3_CPU_RAB_Override ) |
                   ( ( pcie_mem->current_lcpu_state | G3_CPU_RAB_Override ) << 8 );

  _mtlk_g3_open_secure_write_register(pcie_mem->pas, RAB);

  __ccr_writel(new_cpus_state,
               &pcie_mem->pas->RAB.cpu_control_register);

  /* CPU requires time to change its state, so we   */
  /* MUST wait here before writing something else   */
  /* to CPU control register. In other case this    */
  /* may lead to unpredictable results.             */
  mtlk_osal_msleep(10);
}

static __INLINE BOOL
_mtlk_pcie_check_bist(_mtlk_pcie_ccr_t *pcie_mem, uint32 *bist_result)
{
  MTLK_ASSERT(NULL != pcie_mem);
  MTLK_ASSERT(NULL != bist_result);

   __ccr_writel(G3_VAL_START_BIST, &pcie_mem->pas->HTEXT.start_bist);
   mtlk_osal_msleep(20);

  __ccr_readl(&pcie_mem->pas->HTEXT.bist_result, *bist_result);

  return (*bist_result & G3PCIE_CPU_Control_BIST_Passed) == 
    G3PCIE_CPU_Control_BIST_Passed;
}

/* 
  PHY indirect control access, bit definitions
  ---------------------------------------------
  Control address, offset = 0x1D4:
  Bits[15:0] = write data
  Bit[16] = Capture address
  Bit[17] = capture data
  Bit[18] = CR read
  Bit[19] = CR write

  Status address, offset = 0x1D8
  Bits[15:0] = read data
  Bit[16] = ack.
*/

#define __MTLK_PCIE_ACK_WAIT_ITERATIONS (300)

static __INLINE void
__mtlk_pcie_wait_ack_assertion(_mtlk_pcie_ccr_t *pcie_mem, char* operation_id)
{
  uint32 phy_status, ct = 0;

#ifdef MTCFG_SILENT
  MTLK_UNREFERENCED_PARAM(operation_id);
#endif

  for(ct = 0; ct < __MTLK_PCIE_ACK_WAIT_ITERATIONS; ct++)
  {
    __ccr_readl(&pcie_mem->pas->HTEXT.phy_data, phy_status);
    if(phy_status & (1<<16)) return;
  }

  ILOG0_SD("Ack assertion %s wait failed (phy_status = 0x%08x)\n", operation_id, phy_status);
  MTLK_ASSERT(FALSE);
}

static __INLINE void
__mtlk_pcie_wait_ack_deassertion(_mtlk_pcie_ccr_t *pcie_mem, char* operation_id)
{
  uint32 phy_status, ct = 0;

#ifdef MTCFG_SILENT
  MTLK_UNREFERENCED_PARAM(operation_id);
#endif

  for(ct = 0; ct < __MTLK_PCIE_ACK_WAIT_ITERATIONS; ct++)
  {
    __ccr_readl(&pcie_mem->pas->HTEXT.phy_data, phy_status);
    if(!(phy_status & (1<<16))) return;
  }

  ILOG0_SD("Ack deassertion %s wait failed (phy_status = 0x%08x)\n", operation_id, phy_status);
  MTLK_ASSERT(FALSE);
}

static __INLINE void 
__mtlk_pcie_open_reg(_mtlk_pcie_ccr_t *pcie_mem, uint32 reg)
{
  /* 1.Set address bus */
  /* 2.Assert capture address bit */
  __ccr_writel((reg | (1<<16)), &pcie_mem->pas->HTEXT.phy_ctl);

  /* 3.Wait for CR_ack assertion */
  __mtlk_pcie_wait_ack_assertion(pcie_mem, "o1");

  /* 4.De-assert capture address bit */
  __ccr_writel(0x00000000, &pcie_mem->pas->HTEXT.phy_ctl);

  /* 5.Wait for CR_ack de-assertion */
  __mtlk_pcie_wait_ack_deassertion(pcie_mem, "o2");
}

static __INLINE void 
__mtlk_pcie_write_reg(_mtlk_pcie_ccr_t *pcie_mem, uint32 reg, uint16 val)
{
  /* 1-5. Open the register */
  __mtlk_pcie_open_reg(pcie_mem, reg);

  /* 6.Set write data */
  /* 7.Assert capture data bit (while keeping the write data) */
  __ccr_writel(((1<<17)|val), &pcie_mem->pas->HTEXT.phy_ctl);
  
  /* 8.Wait for CR_ack assertion */
  __mtlk_pcie_wait_ack_assertion(pcie_mem, "w3");

  /* 9.De-assert capture data bit */
  __ccr_writel(0x00000000, &pcie_mem->pas->HTEXT.phy_ctl);

  /* 10.Wait for CR_ack de-assertion */
  __mtlk_pcie_wait_ack_deassertion(pcie_mem, "w4");

  /* 11.Set CR write bit */
  __ccr_writel((1<<19), &pcie_mem->pas->HTEXT.phy_ctl);

  /* 12.Wait for CR ack assertion */
  __mtlk_pcie_wait_ack_assertion(pcie_mem, "w5");

  /* 13.De-assert CR write bit */
  __ccr_writel(0x00000000, &pcie_mem->pas->HTEXT.phy_ctl);

  /* 14.Wait for CR ack de-assertion */
  __mtlk_pcie_wait_ack_deassertion(pcie_mem, "w6");
}

static __INLINE uint32 
__mtlk_pcie_read_reg(_mtlk_pcie_ccr_t *pcie_mem, uint32 reg) 
{
    uint32 val;

    /* 1-5. Open the register */
    __mtlk_pcie_open_reg(pcie_mem, reg);

    /* 6.Set CR-read data bit */
    __ccr_writel((1<<18), &pcie_mem->pas->HTEXT.phy_ctl);

    /* 7.Wait for CR ack assertion */ 
    __mtlk_pcie_wait_ack_assertion(pcie_mem, "r3");

    /* 8.Read the <read data> from status bus */
    __ccr_readl(&pcie_mem->pas->HTEXT.phy_data, val);

    /* 9.De-assert CR-read bit */
    __ccr_writel(0x00000000, &pcie_mem->pas->HTEXT.phy_ctl);

    /* 10.Wait for CR_ack de-assertion */
    __mtlk_pcie_wait_ack_deassertion(pcie_mem, "r4");

    return val & 0x0000ffff;
}

extern int tx_ovr_and_mask;
extern int tx_ovr_or_mask;
extern int lvl_ovr_and_mask;
extern int lvl_ovr_or_mask;

static __INLINE void
_mtlk_pcie_release_ctl_from_reset(_mtlk_pcie_ccr_t *pcie_mem)
{
  uint32 tx_ovr_before, lvl_ovr_before;

#if (defined MTCFG_PCIE_TUNING) && (RTLOG_MAX_DLEVEL >= 1)
  uint32 tx_ovr_after, lvl_ovr_after;
#endif

  uint16 dataval;

  MTLK_ASSERT(NULL != pcie_mem);

  __mtlk_g3_release_ctl_from_reset_phase1(pcie_mem->pas);

  tx_ovr_before = __mtlk_pcie_read_reg(pcie_mem ,0x2004);

  /*                                            EN         Boost        Atten        Edge   */
  dataval = (uint16)((tx_ovr_before & tx_ovr_and_mask) | tx_ovr_or_mask);
  __mtlk_pcie_write_reg(pcie_mem, 0x2004, dataval);

#if (defined MTCFG_PCIE_TUNING) && (RTLOG_MAX_DLEVEL >= 1)
  tx_ovr_after = __mtlk_pcie_read_reg(pcie_mem ,0x2004);
#endif

  lvl_ovr_before = __mtlk_pcie_read_reg(pcie_mem ,0x14);
  /*								                          Transmit Lvl      EN  */
  dataval = (uint16)((lvl_ovr_before & lvl_ovr_and_mask) | lvl_ovr_or_mask);
  __mtlk_pcie_write_reg(pcie_mem, 0x14, dataval);

#if (defined MTCFG_PCIE_TUNING) && (RTLOG_MAX_DLEVEL >= 1)
  lvl_ovr_after = __mtlk_pcie_read_reg(pcie_mem ,0x14);
#endif

#ifdef MTCFG_PCIE_TUNING
  ILOG1_DD("de-emphasis: TX input override modification: 0x%08x --> 0x%08x", tx_ovr_before, tx_ovr_after);
  ILOG1_DD("de-emphasis: level override modification: 0x%08x --> 0x%08x", lvl_ovr_before, lvl_ovr_after);
#endif

  __mtlk_g3_release_ctl_from_reset_phase2(pcie_mem->pas);
}

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

