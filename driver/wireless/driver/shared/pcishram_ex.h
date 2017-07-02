#ifndef __PCISHRAM_EX_H__
#define __PCISHRAM_EX_H__

#define HWI_SIZE_HRC_MEMORY_SPACE               (0x1000)
#define HWI_SIZE_PAS_MEMORY_SPACE               (0x00200000)

#define  MTLK_IDEFS_ON
#define  MTLK_IDEFS_PACKING 1
#include "mtlkidefs.h"

struct pci_hrc_regs {
  /* Host Runtime Registers */
  volatile uint32 HWI_ADDR_hrc_gpio_in; //                        (0x000)
  volatile uint32 HWI_ADDR_hrc_gpio_out; //                       (0x004)
  volatile uint32 HWI_ADDR_hrc_gpio_output_enable; //             (0x008)
  volatile uint32 HWI_ADDR_host_semaphore; //                     (0x00C)
  volatile uint32 HWI_ADDR_paging_register; //                    (0x010)
  volatile uint32 HWI_ADDR_force_local_interrupt; //              (0x014)
  volatile uint32 filler1[2]; //                                  (0x018)
  volatile uint32 HWI_ADDR_dma_host_base_address; //              (0x020)
  volatile uint32 HWI_ADDR_dma_host_cursor; //                    (0x024)
  volatile uint32 HWI_ADDR_dma_local_address; //                  (0x028)
  volatile uint32 HWI_ADDR_dma_length; //                         (0x02C)
  volatile uint32 HWI_ADDR_dma_configuration; //                  (0x030)
  volatile uint32 HWI_ADDR_dma_command; //                        (0x034)
  volatile uint32 HWI_ADDR_dma_status; //                         (0x038)
  volatile uint32 filler2[5]; //                                  (0x03C)
  volatile uint32 HWI_ADDR_rev_number; //                         (0x050)
  volatile uint32 filler3[3]; //                                  (0x054)
  volatile uint32 HWI_ADDR_local_interrupt_command; //            (0x060)
  volatile uint32 HWI_ADDR_local_semaphore; //                    (0x064)
  volatile uint32 HWI_ADDR_hem_sram0_wait_states; //              (0x068)
  volatile uint32 HWI_ADDR_hem_sram1_wait_states; //              (0x06C)
  volatile uint32 filler4[16]; //                                 (0x070)
  volatile uint32 HWI_ADDR_target_configuration; //               (0x0b0)
  volatile uint32 HWI_ADDR_initiator_configuration; //            (0x0b4)
  volatile uint32 filler5[18]; //                                 (0x0b8)
  volatile uint32 HWI_ADDR_host_interrupt_status; //              (0x100)
  volatile uint32 HWI_ADDR_host_interrupt_enable; //              (0x104)
  volatile uint32 HWI_ADDR_host_interrupt_active; //              (0x108)
  volatile uint32 HWI_ADDR_host_to_local_doorbell_interrupt; //   (0x10C)
  volatile uint32 HWI_ADDR_general_purpose_control; //            (0x110)
  volatile uint32 HWI_ADDR_general_purpose_status ; //            (0x114)
  volatile uint32 HWI_ADDR_cpu_control; //                        (0x118)
};

#define  MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* #ifndef __PCISHRAM_EX_H__ */
