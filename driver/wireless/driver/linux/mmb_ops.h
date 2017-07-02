#ifndef __MMB_OPS_H__
#define __MMB_OPS_H__

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
/* For 2.4.x kernels the readl/writel functions defined in asm/io.h
 * receive address as unsigned long that causes compilation warnings.
 */
#define _MMB_OPS_ADDR(a) ((uint32)(a))
#elif defined (CONFIG_MIPS)
/* For MIPSes all the readx/writex functions are defined
 * in asm/io.h via __BUILD_MEMORY_SINGLE with no const qualifier that causes 
 * compilation warnings.
 */
#define _MMB_OPS_ADDR(a) (volatile void __iomem *)(a) 
#else 
#define _MMB_OPS_ADDR(a) (a)
#endif

#if (defined (CONFIG_AR9) || \
     defined (CONFIG_VR9) || \
     defined (CONFIG_DANUBE) || \
     defined (CONFIG_AMAZON_S) || \
     defined (CONFIG_HN1))

#if (defined (CONFIG_IFX_PCI_HW_SWAP) || \
     defined (CONFIG_IFX_PCIE_HW_SWAP) || \
     defined (CONFIG_DANUBE_PCI_HW_SWAP) || \
     defined (CONFIG_AMAZON_S_PCI_HW_SWAP))

#if !defined (CONFIG_SWAP_IO_SPACE)
#define MMB_USE_SWAP_IO_OPS
#endif /* CONFIG_SWAP_IO_SPACE */

#else /* CONFIG_..._HW_SWAP */

#error Unsupported HW SWAP configuration!

#endif /* CONFIG_..._HW_SWAP */

#endif

#if defined (MMB_USE_KERNEL_RAW_OPS)

#include <asm/io.h>

static inline void
mtlk_raw_writel (uint32 val, const volatile void __iomem *addr)
{
  __raw_writel(val, _MMB_OPS_ADDR(addr));
#ifdef iobarrier_rw
  iobarrier_rw();
#endif
}

static inline uint32
mtlk_raw_readl (const volatile void __iomem *addr)
{
  uint32 val = __raw_readl(_MMB_OPS_ADDR(addr));
#ifdef iobarrier_rw
  iobarrier_rw();
#endif
  return val;
}

#elif defined(PCI_USE_CUSTOM_OPS)

static inline void
mtlk_raw_writel (uint32 val, const volatile void __iomem *addr)
{
  PCI_CUSTOM_WRITEL((val), (addr));
}

static inline uint32
mtlk_raw_readl (const volatile void __iomem *addr)
{
  PCI_CUSTOM_READL(addr);
}

#elif defined(MMB_USE_SWAP_IO_OPS)

static inline void
mtlk_raw_writel (uint32 val, const volatile void __iomem *addr)
{
  writel(val, _MMB_OPS_ADDR(addr));
}

static inline uint32
mtlk_raw_readl (const volatile void __iomem *addr)
{
  return readl(_MMB_OPS_ADDR(addr));
}

#else 

static inline void
mtlk_raw_writel (uint32 val, volatile void __iomem *addr)
{
  writel(le32_to_cpu(val), _MMB_OPS_ADDR(addr));
}

static inline uint32
mtlk_raw_readl (const volatile void __iomem *addr)
{
  uint32 val = readl(_MMB_OPS_ADDR(addr));
  return cpu_to_le32(val);
}
#endif

static inline void
mtlk_udelay (uint32 us)
{
  udelay(us);
}

#if !defined(MMB_USE_SWAP_IO_OPS)
#define mtlk_writel(v, a) writel((v), _MMB_OPS_ADDR(a))
#define mtlk_readl(a)     readl(_MMB_OPS_ADDR(a))
#else
#define mtlk_writel(v, a) writel(le32_to_cpu(v), _MMB_OPS_ADDR(a))
#define mtlk_readl(a)     cpu_to_le32(readl(_MMB_OPS_ADDR(a)))
#endif

#endif /* __MMB_OPS_H__ */
