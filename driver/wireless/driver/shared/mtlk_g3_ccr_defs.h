/* $Id: mtlk_g3_ccr_defs.h 12137 2011-12-08 14:54:19Z nayshtut $ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_G3_CCR_DEFS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_G3_CCR_DEFS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_G3_CCR_DEFS

static __INLINE  void
__mtlk_g3_release_ctl_from_reset_phase1(struct g3_pas_map *pas)
{
  MTLK_ASSERT(NULL != pas);

  /* See WLSG3-37 for detailed info */
  __ccr_writel(0x03333335,
               &pas->UPPER_SYS_IF.m4k_rams_rm);
  __ccr_writel(0x00000555,
               &pas->UPPER_SYS_IF.iram_rm);
  __ccr_writel(0x00555555,
               &pas->UPPER_SYS_IF.eram_rm);

  __ccr_writel(0x03333335,
               &pas->LOWER_SYS_IF.m4k_rams_rm);
  __ccr_writel(0x00000555,
               &pas->LOWER_SYS_IF.iram_rm);
  __ccr_writel(0x00555555,
               &pas->LOWER_SYS_IF.eram_rm);
}

static __INLINE  void
__mtlk_g3_release_ctl_from_reset_phase2(struct g3_pas_map *pas)
{
  MTLK_ASSERT(NULL != pas);

  /* See WLSG3-37 for detailed info */
  __ccr_writel(0x00005555,
               &pas->HTEXT.htext_offset_f8);

  __ccr_writel(0x33533353,
               &pas->TD.phy_rxtd_reg175);
}


static __INLINE  void
_mtlk_g3_put_ctl_to_reset(struct g3_pas_map *pas)
{
  MTLK_ASSERT(NULL != pas);

  /* Disable RX */
  __ccr_resetl(&pas->PAC.rx_control, G3_MASK_RX_ENABLED);
}

#define _mtlk_g3_open_secure_write_register(pas, block) \
  do { \
  MTLK_ASSERT(NULL != (pas)); \
  __ccr_writel(0xAAAA, &(pas)->block.secure_write_register); \
  __ccr_writel(0x5555, &(pas)->block.secure_write_register); \
  } while (0)

static __INLINE  void
__mtlk_g3_set_ucpu_32k_blocks (struct g3_pas_map *pas)
{
  MTLK_ASSERT(NULL != pas);
  _mtlk_g3_open_secure_write_register(pas, UPPER_SYS_IF);
  __ccr_writel(G3_UCPU_32K_BLOCKS, &pas->UPPER_SYS_IF.ucpu_32k_blocks);
}
