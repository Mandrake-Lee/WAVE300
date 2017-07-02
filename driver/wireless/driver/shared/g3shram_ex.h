#ifndef __G3SHRAM_EX_H__
#define __G3SHRAM_EX_H__

/* This structure describes PAS memory map as defined by:  */
/*   SRD-052-063 MAC Enhancements for Gen. 3               */
/*   SRD-052-065 PCIe System Requirements Document         */
/*   UTM-052-079 PCIe core integration - SW directives     */
/* *filler* fields are dummy fields to describe "holes"    */
/* i.e. PAS locations not significant for driver.          */
/* The pas memory map is the same for PCI and PCI express  */
/* Gen3 adapters.                                          */

#define  MTLK_IDEFS_ON
#define  MTLK_IDEFS_PACKING 1
#include "mtlkidefs.h"

struct g3_pas_map {
  unsigned char shram_access[0x100000];       /* 0x0 */
  unsigned char circular_buffers[0x100000];   /* 0x100,000 */
  struct {                                    
    unsigned char   pac_filler1[0x300];       /* 0x200,000 */
    volatile uint32 rx_control;               /* 0x200,300 */
  } PAC;

  unsigned char filler1[0x434];               /* 0x200,304 */
  volatile uint32 tsf_timer_low;              /* 0x200,738 */
  volatile uint32 tsf_timer_high;             /* 0x200,73C */
  unsigned char filler2[0x1F8C0];             /* 0x200,740 */

  struct {                                    
    unsigned char   rab_filler1[0x44];        /* 0x220,000 */
    volatile uint32 secure_write_register;    /* 0x220,044 */
    unsigned char   rab_filler2[0x10];        /* 0x220,048 */
    volatile uint32 cpu_control_register;     /* 0x220,058 */
  } RAB;
  unsigned char filler3[0x5FFA4];             /* 0x220,05C */
  struct {
    unsigned char   sys_if_filler1[0x68];     /* 0x280,000 */
    volatile uint32 secure_write_register;    /* 0x220,068 */
    unsigned char   sys_if_filler2[0x40];     /* 0x280,06C */
    volatile uint32 ucpu_32k_blocks;          /* 0x280,0AC */
    unsigned char   sys_if_filler3[0x24];     /* 0x280,0B0 */
    volatile uint32 m4k_rams_rm;              /* 0x280,0D4 */
    volatile uint32 iram_rm;                  /* 0x280,0D8 */
    volatile uint32 eram_rm;                  /* 0x280,0DC */
  } UPPER_SYS_IF;
  unsigned char filler4[0xFF20];              /* 0x280,0E0 */
  struct {
    unsigned char   sys_if_filler1[0xD4];     /* 0x290,000 */
    volatile uint32 m4k_rams_rm;              /* 0x290,0D4 */
    volatile uint32 iram_rm;                  /* 0x290,0D8 */
    volatile uint32 eram_rm;                  /* 0x290,0DC */
  } LOWER_SYS_IF;
  unsigned char filler5[0x5FF20];             /* 0x290,0E0 */
  struct {
    unsigned char   htext_filler1[0x14];      /* 0x2F0,000 */
    volatile uint32 bist_result;              /* 0x2F0,014 */
    volatile uint32 door_bell;                /* 0x2F0,018 */
    unsigned char   htext_filler2[0x0DC];     /* 0x2F0,01C */
    volatile uint32 htext_offset_f8;          /* 0x2F0,0F8 */
    unsigned char   htext_filler3[0x064];     /* 0x2F0,0FC */
    volatile uint32 start_bist;               /* 0x2F0,160 */
    unsigned char   htext_filler4[0x070];     /* 0x2F0,164 */

    /* Following two registers are PCIE PHY registers      */
    /* See WLS-2638                                        */
    volatile uint32 phy_ctl;                  /* 0x2F0,1D4 */
    volatile uint32 phy_data;                 /* 0x2F0,1D8 */

    unsigned char   htext_filler6[4];         /* 0x2F0,1DC */
    volatile uint32 host_irq;                 /* 0x2F0,1E0 */
  } HTEXT;
  unsigned char filler7[0x6FE1C];             /* 0x290,1E4 */
  struct {
    unsigned char   td_filler1[0x5D4];        /* 0x300,000 */
    volatile uint32 phy_rxtd_reg175;          /* 0x2F0,5D4 */
  } TD;
};

#define  MTLK_IDEFS_OFF
#include "mtlkidefs.h"

/* CPU control via RAB */

/* Pushbutton bit */
#define G3_CPU_RAB_Push_Button        (1)

/* Reset/Active bit */
#define G3_CPU_RAB_Reset              (0 << 1)
#define G3_CPU_RAB_Active             (1 << 1)

/* Boot mode bits */
#define G3_CPU_RAB_IRAM               (0 << 2)
#define G3_CPU_RAB_SHRAM              (2 << 2)
#define G3_CPU_RAB_DEBUG              (3 << 2)

/* Override bit */
#define G3_CPU_RAB_Override           (1 << 4)

/* Gen3 BIST check */

#define G3_VAL_START_BIST             (1)

#define G3_MASK_RX_ENABLED            (1)

#define G3_UCPU_32K_BLOCKS            (5)

#endif /* #ifndef __G3SHRAM_EX_H__ */
