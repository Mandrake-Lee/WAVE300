/* $Id: mtlk_g35_ccr_decls.h 11899 2011-11-06 09:33:46Z vugenfir $ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DECLS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DECLS

#if defined(MTCFG_PLATFORM_UGW_WAVE400) || defined (MTCFG_PLATFORM_GEN35FPGA)
#define MTLK_G35_NPU
#endif

#include "g3shram_ex.h"
#ifndef MTLK_G35_NPU
#include "pcishram_ex.h"
#endif

struct g35_pas_map {
  unsigned char filler1[0x00200000];          /* 0x0 */
  struct {                                    
    unsigned char   pac_filler1[0x300];       /* 0x0,200,000 */
    volatile uint32 rx_control;               /* 0x0,200,300 */
  } PAC;

  unsigned char filler2[0x434];               /* 0x200,304 */
  volatile uint32 tsf_timer_low;              /* 0x200,738 */
  volatile uint32 tsf_timer_high;             /* 0x200,73C */
  unsigned char filler3[0x1F8C0];             /* 0x200,740 */

  struct {                                    
    unsigned char   rab_filler1[0x08];        /* 0x0,220,000 */
    volatile uint32 upi_interrupt;            /* 0x0,220,008 */
    volatile uint32 upi_interrupt_clear;      /* 0x0,220,00C */
    volatile uint32 lpi_interrupt;            /* 0x0,220,010 */
    volatile uint32 lpi_interrupt_clear;      /* 0x0,220,014 */
    volatile uint32 phi_interrupt;            /* 0x0,220,018 */
    volatile uint32 phi_interrupt_clear;      /* 0x0,220,01C */
    unsigned char   rab_filler2[0x24];        /* 0x0,220,020 */
    volatile uint32 secure_write_register;    /* 0x0,220,044 */
    unsigned char   rab_filler3[0xC];         /* 0x0,220,048 */
    volatile uint32 enable_phi_interrupt;     /* 0x0,220,054 */
    volatile uint32 cpu_control_register;     /* 0x0,220,058 */
  } RAB;
  unsigned char filler4[0x5FFA4];             /* 0x0,220,05C */
  struct {
    unsigned char   sys_if_filler1[0xD4];     /* 0x0,280,000 */
    volatile uint32 m4k_rams_rm;              /* 0x0,280,0D4 */
    volatile uint32 iram_rm;                  /* 0x0,280,0D8 */
    volatile uint32 eram_rm;                  /* 0x0,280,0DC */
  } CPU_SYS_IF;
  unsigned char filler5[0x6FF20];             /* 0x0,280,0E0 */
  struct {
    unsigned char   htext_filler1[0x14];      /* 0x0,2F0,000 */
    volatile uint32 bist_result;              /* 0x0,2F0,014 */
    volatile uint32 door_bell;                /* 0x0,2F0,018 */
    unsigned char   htext_filler2[0x0DC];     /* 0x0,2F0,01C */
    volatile uint32 htext_offset_f8;          /* 0x0,2F0,0F8 */
    unsigned char   htext_filler3[0x064];     /* 0x0,2F0,0FC */
    volatile uint32 start_bist;               /* 0x0,2F0,160 */
    unsigned char   htext_filler4[0x05C];     /* 0x0,2F0,164 */
    volatile uint32 ahb_arb_bbcpu_page_reg;   /* 0x0,2F0,1C0 */
    unsigned char   htext_filler5[0x00C];     /* 0x0,2F0,1C4 */
    volatile uint32 host_irq_status;          /* 0x0,2F0,1D0 */
    volatile uint32 host_irq_mask;            /* 0x0,2F0,1D4 */
    unsigned char   htext_filler6[0x08];      /* 0x0,2F0,1D8 */
    volatile uint32 host_irq;                 /* 0x0,2F0,1E0 */
  } HTEXT;
#ifdef MTLK_G35_NPU
  unsigned char filler6[0xECFE1C];            /* 0x0,2F0,1E4 */
  struct {
    unsigned char   shb_filler1[0x10];        /* 0x1,1C0,000 */
    volatile uint32 door_bell;                /* 0x1,1C0,010 */
    unsigned char   shb_filler2[0x04];        /* 0x1,1C0,014 */
    volatile uint32 interrupt_clear;          /* 0x1,1C0,018 */
    volatile uint32 interrupt_enable;         /* 0x1,1C0,01C */
    unsigned char   shb_filler3[0x54];        /* 0x1,1C0,020 */
    volatile uint32 bb_ddr_offset_mb;         /* 0x1,1C0,074 */
  } SH_REG_BLOCK;
#endif
};

typedef struct
{
  struct g35_pas_map        *pas;
  struct _mtlk_ahb_drv_t    *ahb_drv;
  uint8                      current_ucpu_state;
  uint8                      next_boot_mode;
} _mtlk_g35_ccr_t;
