/*
 * $Id: mtlk_eeprom.c 11419 2011-07-18 10:57:38Z fleytman $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * EEPROM data processing module
 *
 * Originally written by Andrii Tseglytskyi
 *
 */

#include "mtlkinc.h"
#include "mtlkerr.h"
#include "channels.h"
#include "mtlkaux.h"
#include "mtlkmib.h"

#include "eeprom.h"
#include "mtlk_eeprom.h"
#include "mtlk_tpcv4.h"
#include "mtlk_algorithms.h"
#include "mtlk_channels_propr.h"
#include "mtlk_coreui.h"

#define LOG_LOCAL_GID   GID_EEPROM
#define LOG_LOCAL_FID   1

/*****************************************************************************
* Local type definitions
******************************************************************************/

/* \cond DOXYGEN_IGNORE */
#define  MTLK_IDEFS_ON
#define  MTLK_IDEFS_PACKING 1
#include "mtlkidefs.h"
/* \endcond */



/* From TTPCom document "PCI/CardBus Host Reference Configuration
   Hardware Specification" p. 32 */
#define MTLK_EEPROM_EXEC_SIGNATURE              7164

/* number of TPC entries in the EEPROM */
#define MTLK_EEPROM_NUM_TPC_5_2_GHZ    5
#define MTLK_EEPROM_NUM_TPC_2_4_GHZ    1
#define MTLK_EEPROM_NUM_TPC            (MTLK_EEPROM_NUM_TPC_5_2_GHZ + MTLK_EEPROM_NUM_TPC_2_4_GHZ)

#define MTLK_EE_BLOCKED_SEND_TIMEOUT     (10000) /* ms */

#define MTLK_MAKE_EEPROM_VERSION(major, minor) (((major) << 8) | (minor))

struct mtlk_eeprom_tpc_data_t;

/* PCI configuration */
typedef struct _mtlk_eeprom_pci_cfg_t {           /* len  ofs */
  uint16 eeprom_executive_signature;              /*  2    2  */
  uint16 ee_control_configuration;                /*  2    4  */
  uint16 ee_executive_configuration;              /*  2    6  */
  uint8  revision_id;                             /*  1    7  */
  uint8  class_code[3];                           /*  3   10  */
  uint8  bist;                                    /*  1   11  */
#if defined(__LITTLE_ENDIAN_BITFIELD)
  uint8  misc : 4;
  uint8  status : 4;                              /*  1   12  */
#elif defined(__BIG_ENDIAN_BITFIELD)
  uint8  status : 4;                              /*  1   12  */
  uint8  misc : 4;
#else
  # error Endianess not defined!
#endif
  uint32 card_bus_cis_pointer;                    /*  4   16  */
  uint16 subsystem_vendor_id;                     /*  2   18  */
  uint16 subsystem_id;                            /*  2   20  */
  uint8  max_lat;                                 /*  1   21  */
  uint8  min_gnt;                                 /*  1   22  */
  uint16 power_management_capabilities;           /*  2   24  */
  uint32 hrc_runtime_base_address_register_range; /*  4   28  */
  uint32 local1_base_address_register_range;      /*  4   32  */
  uint16 hrc_target_configuration;                /*  2   34  */
  uint16 hrc_initiator_configuration;             /*  2   36  */
  uint16 vendor_id;                               /*  2   38  */
  uint16 device_id;                               /*  2   40  */
  uint32 reserved[6];                             /* 24   64  */
} __MTLK_IDATA mtlk_eeprom_pci_cfg_t;

/* version of the EEPROM */
typedef struct _mtlk_eeprom_version_t {
  uint8 version0;
  uint8 version1;
} __MTLK_IDATA mtlk_eeprom_version_t;

typedef struct _mtlk_eeprom_t {
  mtlk_eeprom_pci_cfg_t config_area;
  uint16                cis_size;
  uint32                cis_base_address;
  mtlk_eeprom_version_t version;
  uint8                 cis[1];
} __MTLK_IDATA mtlk_eeprom_t;

/* NOTE: all cards that are not updated will have 0x42 value by default */
typedef union _mtlk_dev_opt_mask_t {
  struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
    uint8 ap_disabled:1;
    uint8 disable_sm_channels:1;
    uint8 __reserved:6;
#elif defined(__BIG_ENDIAN_BITFIELD)
    uint8 __reserved:6;
    uint8 disable_sm_channels:1;
    uint8 ap_disabled:1;
#else
  # error Endianess not defined!
#endif
  } __MTLK_IDATA s;
  uint8 d;
} __MTLK_IDATA mtlk_eeprom_dev_opt_mask_t;

/* CIS: card ID */
typedef struct _mtlk_cis_cardid_t {
  uint8 type;
  uint8 revision;
  uint8 country_code;
  mtlk_eeprom_dev_opt_mask_t dev_opt_mask;
  uint8 rf_chip_number;
  uint8 mac_address[ETH_ALEN];
  uint8 sn[MTLK_EEPROM_SN_LEN];
  uint8 production_week;
  uint8 production_year;
} __MTLK_IDATA mtlk_eeprom_cis_cardid_t;

/* all data read from the EEPROM (except PCI configuration) as a structure */
struct _mtlk_eeprom_data_t {
  uint8             valid;
  uint16            eeprom_version;
  mtlk_eeprom_cis_cardid_t card_id;
  /* TPC calibration data */
  uint8             tpc_valid;
  //List_Of_Tpc_2_4_Ghz tpc_2_4_GHz[MTLK_TPC_ANT_LAST];
  //List_Of_Tpc_5_Ghz tpc_5_2_GHz[MTLK_TPC_ANT_LAST];
  uint16 vendor_id;
  uint16 device_id;
  uint16 sub_vendor_id;
  uint16 sub_device_id;
  /* EEPROM data */
  mtlk_eeprom_tpc_data_t *tpc_24;
  mtlk_eeprom_tpc_data_t *tpc_52;
} __MTLK_IDATA;

/* CardBus Information Structure header */
typedef struct _mtlk_cis_header_t
{
    uint8 tpl_code;
    uint8 link;
    uint8 data[1];
}__MTLK_IDATA mtlk_eeprom_cis_header_t;

/* CIS item: TPC for version1 == 1 */
typedef struct _mtlk_cis_tpc_item_v1_t
{
    uint8 channel;

#if defined(__LITTLE_ENDIAN_BITFIELD)
    uint8 ant0_tpc:5;
    uint8 ant0_band:1;
    uint8 pa0_a_hi:2;
#elif defined(__BIG_ENDIAN_BITFIELD)
    uint8 pa0_a_hi:2;
    uint8 ant0_band:1;
    uint8 ant0_tpc:5;
#else
# error Endianess not defined!
#endif

    uint8 ant0_max_power;
    uint8 pa0_a_lo;
    int8 pa0_b;

#if defined(__LITTLE_ENDIAN_BITFIELD)
    uint8 ant1_tpc:5;
    uint8 ant1_band:1;
    uint8 pa1_a_hi:2;
#elif defined(__BIG_ENDIAN_BITFIELD)
    uint8 pa1_a_hi:2;
    uint8 ant1_band:1;
    uint8 ant1_tpc:5;
#else
# error Endianess not defined!
#endif

    uint8 ant1_max_power;
    uint8 pa1_a_lo;
    int8 pa1_b;

}__MTLK_IDATA mtlk_eeprom_cis_tpc_item_v1_t;

/* CIS item: TPC for version1 == 2 */
typedef struct _mtlk_cis_tpc_item_v2_t
{
    uint8 channel;

#if defined(__LITTLE_ENDIAN_BITFIELD)
    uint8 ant0_tpc:5;
    uint8 ant0_band:1;
    uint8 padding_1:2;
#elif defined(__BIG_ENDIAN_BITFIELD)
    uint8 padding_1:2;
    uint8 ant0_band:1;
    uint8 ant0_tpc:5;
#else
# error Endianess not defined!
#endif

    uint8 ant0_max_power;
    uint8 pa0_a_lo;
    uint8 pa0_b_lo;

#if defined(__LITTLE_ENDIAN_BITFIELD)
    uint8 pa0_a_hi:4;
    uint8 pa0_b_hi:4;
    uint8 ant1_tpc:5;
    uint8 ant1_band:1;
    uint8 padding_2:2;
#elif defined(__BIG_ENDIAN_BITFIELD)
    uint8 pa0_b_hi:4;
    uint8 pa0_a_hi:4;
    uint8 padding_2:2;
    uint8 ant1_band:1;
    uint8 ant1_tpc:5;
#else
# error Endianess not defined!
#endif

    uint8 ant1_max_power;
    uint8 pa1_a_lo;
    uint8 pa1_b_lo;

#if defined(__LITTLE_ENDIAN_BITFIELD)
uint8 pa1_a_hi:4;
uint8 pa1_b_hi:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
uint8 pa1_b_hi:4;
uint8 pa1_a_hi:4;
#else
# error Endianess not defined!
#endif

}__MTLK_IDATA mtlk_eeprom_cis_tpc_item_v2_t;

/* ****** EEPROM v3 ****** */
#define TPC_CIS_HEADER_V3_SIZE 2 /* size of tpc cis header, excluding data field */

#define POINTS_4LN  5
#define POINTS_3LN  4
#define POINTS_2LN  3
#define POINTS_1LN  2

#define SECOND_TXPOWER_DELTA 24

typedef struct _mtlk_cis_tpc_header_v3
{
    uint8 size_24;
    uint8 size_52;
    uint8 data[1];
}__MTLK_IDATA mtlk_eeprom_cis_tpc_header_t;

typedef struct _mtlk_eeprom_tpc_v3_head
{
    uint8 channel;
#if defined(__LITTLE_ENDIAN_BITFIELD)
    uint8 ant0_tpc:5;
    uint8 band:1;
    uint8 mode:1;
    uint8 pa0_x1_hi:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
    uint8 pa0_x1_hi:1;
    uint8 mode:1;
    uint8 band:1;
    uint8 ant0_tpc:5;
#else
# error Endianess not defined!
#endif

#if defined(__LITTLE_ENDIAN_BITFIELD)
    uint8 ant1_tpc:5;
    //  uint8 reserved:2;
    uint8 backoff_a0:1;
    uint8 backoff_a1:1;
    uint8 pa1_x1_hi:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
    uint8 pa1_x1_hi:1;
    //  uint8 reserved:2;
    uint8 backoff_a1:1;
    uint8 backoff_a0:1;
    uint8 ant1_tpc:5;
#else
# error Endianess not defined!
#endif
    uint8 pa0_x1;
    uint8 pa0_y1; /* == max_power */
    uint8 pa0_x2;
    uint8 pa0_y2;
    uint8 pa1_x1;
    uint8 pa1_y1; /* == max_power */
    uint8 pa1_x2;
    uint8 pa1_y2;
#if defined(__LITTLE_ENDIAN_BITFIELD)
    uint8 pa0_x2_hi:1;
    uint8 pa0_x3_hi:1;
    uint8 pa0_x4_hi:1;
    uint8 pa0_x5_hi:1;
    uint8 pa1_x2_hi:1;
    uint8 pa1_x3_hi:1;
    uint8 pa1_x4_hi:1;
    uint8 pa1_x5_hi:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
    uint8 pa1_x5_hi:1;
    uint8 pa1_x4_hi:1;
    uint8 pa1_x3_hi:1;
    uint8 pa1_x2_hi:1;
    uint8 pa0_x5_hi:1;
    uint8 pa0_x4_hi:1;
    uint8 pa0_x3_hi:1;
    uint8 pa0_x2_hi:1;
#else
# error Endianess not defined!
#endif
    uint8 backoff0;
    uint8 backoff1;
}__MTLK_IDATA mtlk_eeprom_cis_tpc_item_v3_header;

typedef struct _mtlk_eeprom_tpc_v3_4ln
{
    mtlk_eeprom_cis_tpc_item_v3_header head;
    uint8 pa0_x3;
    uint8 pa0_y3;
    uint8 pa1_x3;
    uint8 pa1_y3;
    uint8 pa0_x4;
    uint8 pa0_y4;
    uint8 pa1_x4;
    uint8 pa1_y4;
    uint8 pa0_x5;
    uint8 pa0_y5;
    uint8 pa1_x5;
    uint8 pa1_y5;
}__MTLK_IDATA mtlk_eeprom_cis_tpc_item_v3_4ln_t;

typedef struct _mtlk_eeprom_tpc_v3_3ln
{
    mtlk_eeprom_cis_tpc_item_v3_header head;
    uint8 pa0_x3;
    uint8 pa0_y3;
    uint8 pa1_x3;
    uint8 pa1_y3;
    uint8 pa0_x4;
    uint8 pa0_y4;
    uint8 pa1_x4;
    uint8 pa1_y4;
}__MTLK_IDATA mtlk_eeprom_cis_tpc_item_v3_3ln_t;

typedef struct _mtlk_eeprom_tpc_v3_2ln
{
    mtlk_eeprom_cis_tpc_item_v3_header head;
    uint8 pa0_x3;
    uint8 pa0_y3;
    uint8 pa1_x3;
    uint8 pa1_y3;
}__MTLK_IDATA mtlk_eeprom_cis_tpc_item_v3_2ln_t;

typedef struct _mtlk_eeprom_tpc_v3_1ln
{
    mtlk_eeprom_cis_tpc_item_v3_header head;
}__MTLK_IDATA mtlk_eeprom_cis_tpc_item_v3_1ln_t;

/* ****** EEPROM v3 ****** */

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

/*****************************************************************************
* Local definitions
******************************************************************************/
#define TPC_DEBUG 0
/* CardBus setions TPL codes */
#define CIS_TPL_CODE_CID        0x80
#define CIS_TPL_CODE_TPC        0x81
#define CIS_TPL_CODE_LNA        0x82
#define CIS_TPL_CODE_TPCG3      0x83
#define CIS_TPL_CODE_NONE       0xFF

/*****************************************************************************
* Function implementation
******************************************************************************/

mtlk_eeprom_data_t* __MTLK_IFUNC
mtlk_eeprom_create(void)
{
    mtlk_eeprom_data_t    *eeprom_data =
            mtlk_osal_mem_alloc(sizeof(mtlk_eeprom_data_t), MTLK_MEM_TAG_EEPROM);

    if (NULL != eeprom_data)
    {
        memset(eeprom_data, 0, sizeof(mtlk_eeprom_data_t));
    }

    return eeprom_data;
}

void __MTLK_IFUNC mtlk_eeprom_delete(mtlk_eeprom_data_t *eeprom_data)
{
    if (NULL != eeprom_data)
    {
        mtlk_osal_mem_free(eeprom_data);
    }
}

/*****************************************************************************
**
** NAME         _mtlk_eeprom_cis_find
**
** PARAMETERS   id              id to search for
**              raw_eeprom      buffer holding eeprom to search in
**              buffer_len      buffer length
**              start_from_cis  this value is used by function to search for
**                              multiple sections of the same time. To search
**                              for the first section user have to provide
**                              pointer to zero, to search for the next section,
**                              user have to provide pointer to value that function
**                              placed to this variable on first invocation.
**
** RETURNS      pointer to the structure beginning. NULL if none
**              cis_size              size of the CIS data
**
** DESCRIPTION  This function tries to find a CIS (CardBus information
**              section CIS) in the buffer specified
**
******************************************************************************/
static void *
_mtlk_eeprom_cis_find (uint8 id, mtlk_eeprom_t *raw_eeprom, int buffer_len, 
                       int *cis_size, char** start_from_cis)
{
  /* Metalink's PCI eeprom address map:                                  */
  /* The header starts with two bytes defining the size of the           */
  /* CIS section in bytes (not including these bytes and the next four). */
  /* A value of 0h will be interpreted as "no CIS exist"                 */
  /* Next comes TPL_LINK byte, defining number of bytes (N) exists       */
  /*  at this CIS structure (not including the current byte).            */

  mtlk_eeprom_cis_header_t *curr_cis;

  MTLK_ASSERT(NULL != raw_eeprom);
  MTLK_ASSERT(NULL != start_from_cis);
  MTLK_ASSERT(NULL != cis_size);

  MTLK_ASSERT( (NULL == *start_from_cis) ||
               ( (*start_from_cis >= (char*)raw_eeprom) &&
                 (*start_from_cis <  (char*)raw_eeprom + buffer_len) ) );

  curr_cis = (mtlk_eeprom_cis_header_t *) 
    ((NULL != *start_from_cis) ? *start_from_cis : (char*) raw_eeprom->cis);

  while ( ( (char*)curr_cis < ((char*)raw_eeprom + buffer_len) ) &&
          ( curr_cis->tpl_code != CIS_TPL_CODE_NONE ) ) {

    if (curr_cis->tpl_code == id) {
      *cis_size = curr_cis->link;
      *start_from_cis = (char*)curr_cis + curr_cis->link + 2;
      return (void *)curr_cis->data;
    } else {
      curr_cis = (mtlk_eeprom_cis_header_t *) ( (char*)curr_cis + curr_cis->link + 2 );
    }

  }
  return NULL;
}

static int16
extend_sign_16(int16 a, int num_preserve_bits)
{
  if(a & (1 << (num_preserve_bits - 1))) 
    a |= ~((1 << num_preserve_bits) - 1);
  return a;
}

static mtlk_eeprom_tpc_data_t *
allocate_eeprom_tpc_data(uint8 num_points, uint8 num_antennas);

static void
convert_from_tpc_v1 (mtlk_eeprom_data_t *eeprom, mtlk_eeprom_cis_tpc_item_v1_t *tpc_v1)
{
  mtlk_eeprom_tpc_data_t *tpc_v3 = allocate_eeprom_tpc_data(POINTS_1LN, NUM_TX_ANTENNAS_GEN2);
  int a_param;
  const uint8 b_mult = 7;
  int sign;
  int y_b;
  
  if (tpc_v3) {
  
    tpc_v3->channel = tpc_v1->channel;
    tpc_v3->band = tpc_v1->ant0_band;
    tpc_v3->spectrum_mode = 1; // CB
    tpc_v3->freq = channel_to_frequency(tpc_v3->channel) + (tpc_v3->spectrum_mode ? 10: 0);
    tpc_v3->tpc_values[0] = tpc_v1->ant0_tpc;
    tpc_v3->tpc_values[1] = tpc_v1->ant1_tpc;

    ILOG5_DDDDD("TPC: ch=%d, band=%d, mode=%d, tpc_values[0]=%d, tpc_values[1]=%d",
        tpc_v3->channel, tpc_v3->band, tpc_v3->spectrum_mode, 
        tpc_v3->tpc_values[0], tpc_v3->tpc_values[1]);

    a_param = (tpc_v1->pa0_a_hi << 8) | tpc_v1->pa0_a_lo;
    a_param = extend_sign_16((uint16)a_param, 10);
    ILOG5_DD("TPC: antenna_0: a = %d, b = %d", a_param, tpc_v1->pa0_b);
    
    y_b = (tpc_v1->ant0_max_power - tpc_v1->pa0_b); 
    sign = (y_b * a_param) >= 0 ? 1: -1;
    ILOG5_DD("y_b = %d sign = %d", y_b, sign);
    tpc_v3->points[0][0].x =(uint16)(((y_b << b_mult) + sign * (a_param >> 1)) / a_param );
    tpc_v3->points[0][0].y = tpc_v1->ant0_max_power;
    y_b = (tpc_v1->ant0_max_power - SECOND_TXPOWER_DELTA - tpc_v1->pa0_b); 
    sign = (y_b * a_param) >= 0 ? 1: -1;
    ILOG5_DD("y_b = %d sign = %d", y_b, sign);
    tpc_v3->points[0][1].x =(uint16)(((y_b << b_mult) + sign * (a_param >> 1)) / a_param );
    tpc_v3->points[0][1].y = tpc_v1->ant0_max_power - SECOND_TXPOWER_DELTA;
    ILOG5_DDDD("TPC: antenna_0: (%d, %d) - (%d, %d)", 
        tpc_v3->points[0][0].x, tpc_v3->points[0][0].y,
        tpc_v3->points[0][1].x, tpc_v3->points[0][1].y);
    
    a_param = (tpc_v1->pa1_a_hi << 8) | tpc_v1->pa1_a_lo;
    a_param = extend_sign_16((uint16)a_param, 10);
    ILOG5_DD("TPC: antenna_1: a = %d, b = %d", a_param, tpc_v1->pa1_b);
    y_b = (tpc_v1->ant1_max_power - tpc_v1->pa1_b);
    sign = (y_b * a_param) >= 0 ? 1: -1;
    ILOG5_DD("y_b = %d sign = %d", y_b, sign);
    tpc_v3->points[1][0].x =(uint16)(((y_b << b_mult) + sign * (a_param >> 1)) / a_param);
    tpc_v3->points[1][0].y = tpc_v1->ant1_max_power;
    y_b = (tpc_v1->ant1_max_power - SECOND_TXPOWER_DELTA - tpc_v1->pa1_b);
    sign = (y_b * a_param) >= 0 ? 1: -1;
    ILOG5_DD("y_b = %d sign = %d", y_b, sign);
    tpc_v3->points[1][1].x =(uint16)(((y_b << b_mult) + sign * (a_param >> 1)) / a_param);
    tpc_v3->points[1][1].y = tpc_v1->ant1_max_power - SECOND_TXPOWER_DELTA;
    ILOG5_DDDD("TPC: antenna_1: (%d, %d) - (%d, %d)", 
        tpc_v3->points[1][0].x, tpc_v3->points[1][0].y,
        tpc_v3->points[1][1].x, tpc_v3->points[1][1].y);
  

    if (tpc_v3->band == MTLK_HW_BAND_2_4_GHZ) {
      tpc_v3->next = eeprom->tpc_24;
      eeprom->tpc_24 = tpc_v3;
    } else if (tpc_v3->band == MTLK_HW_BAND_5_2_GHZ) {
      tpc_v3->next = eeprom->tpc_52;
      eeprom->tpc_52 = tpc_v3;
    }
  }
}

static void
convert_from_tpc_v2 (mtlk_eeprom_data_t *eeprom, mtlk_eeprom_cis_tpc_item_v2_t *tpc_v2)
{
  mtlk_eeprom_tpc_data_t *tpc_v3 = allocate_eeprom_tpc_data(POINTS_1LN, NUM_TX_ANTENNAS_GEN2);
  int16 a_param, b_param;
  const uint8 b_mult = 8;
  int y_b;
  int sign;
  
  if (tpc_v3) {
  
    tpc_v3->channel = tpc_v2->channel;
    tpc_v3->band = tpc_v2->ant0_band;
    tpc_v3->spectrum_mode = 1; // CB
    tpc_v3->freq = channel_to_frequency(tpc_v3->channel) + (tpc_v3->spectrum_mode ? 10: 0);
    tpc_v3->tpc_values[0] = tpc_v2->ant0_tpc;
    tpc_v3->tpc_values[1] = tpc_v2->ant1_tpc;
    ILOG5_DDDDD("TPC: ch=%d, band=%d, mode=%d, tpc_values[0]=%d, tpc_values[1]=%d",
        tpc_v3->channel, tpc_v3->band, tpc_v3->spectrum_mode, 
        tpc_v3->tpc_values[0], tpc_v3->tpc_values[1]);

    a_param = (tpc_v2->pa0_a_hi << 8) | tpc_v2->pa0_a_lo;
    a_param = extend_sign_16(a_param, 12);
    b_param = (tpc_v2->pa0_b_hi << 8) | tpc_v2->pa0_b_lo;
    b_param = extend_sign_16(b_param, 12);
    ILOG5_DD("TPC: antenna_0: a = %d, b = %d", a_param, b_param);
    
    y_b = (tpc_v2->ant0_max_power << 1) - b_param;
    sign = (y_b * a_param) >= 0 ? 1: -1;
    tpc_v3->points[0][0].x = (uint16)( ((y_b << b_mult) + sign * (a_param >> 1)) / a_param );
    tpc_v3->points[0][0].y = tpc_v2->ant0_max_power;
    y_b = (((tpc_v2->ant0_max_power - SECOND_TXPOWER_DELTA) << 1) - b_param); 
    sign = (y_b * a_param) >= 0 ? 1: -1;
    tpc_v3->points[0][1].x =(uint16)(((y_b << b_mult) + sign * (a_param >> 1)) / a_param );
    tpc_v3->points[0][1].y = tpc_v2->ant0_max_power - SECOND_TXPOWER_DELTA;
    ILOG5_DDDD("TPC: antenna_0: (%d, %d) - (%d, %d)", 
        tpc_v3->points[0][0].x, tpc_v3->points[0][0].y,
        tpc_v3->points[0][1].x, tpc_v3->points[0][1].y);
    
    a_param = (tpc_v2->pa1_a_hi << 8) | tpc_v2->pa1_a_lo;
    a_param = extend_sign_16(a_param, 12);
    b_param = (tpc_v2->pa1_b_hi << 8) | tpc_v2->pa1_b_lo;
    b_param = extend_sign_16(b_param, 12);
    ILOG5_DD("TPC: antenna_1: a = %d, b = %d", a_param, b_param);
    
    y_b = ((tpc_v2->ant1_max_power << 1) - b_param); 
    sign = (y_b * a_param) >= 0 ? 1: -1;
    tpc_v3->points[1][0].x =(uint16)(((y_b << b_mult) + sign * (a_param >> 1)) / a_param );
    tpc_v3->points[1][0].y = tpc_v2->ant1_max_power;
    y_b = (((tpc_v2->ant1_max_power - SECOND_TXPOWER_DELTA) << 1) - b_param); 
    sign = (y_b * a_param) >= 0 ? 1: -1;
    tpc_v3->points[1][1].x = (uint16)(((y_b << b_mult) + sign * (a_param >> 1)) / a_param);
    tpc_v3->points[1][1].y = tpc_v2->ant1_max_power - SECOND_TXPOWER_DELTA;
    ILOG5_DDDD("TPC: antenna_1: (%d, %d) - (%d, %d)", 
        tpc_v3->points[1][0].x, tpc_v3->points[1][0].y,
        tpc_v3->points[1][1].x, tpc_v3->points[1][1].y);
  
    if (tpc_v3->band == MTLK_HW_BAND_2_4_GHZ) {
      tpc_v3->next = eeprom->tpc_24;
      eeprom->tpc_24 = tpc_v3;
    } else if (tpc_v3->band == MTLK_HW_BAND_5_2_GHZ) {
      tpc_v3->next = eeprom->tpc_52;
      eeprom->tpc_52 = tpc_v3;
    }
  }
}
/*****************************************************************************
**
** NAME         mtlk_eeprom_parse_tpc_v1
**
** PARAMETERS   eeprom_data           pointer to eeprom data struc to fill in
**              tpc                   pointer to the buffer with TPC items
**              cis_size              total length of the TPC CIS
**
** RETURNS      sets eeprom_data->tpc_valid on success
**
** DESCRIPTION  This function called to perform parsing of TPC EEPROM ver 1
**
******************************************************************************/
static void
mtlk_eeprom_parse_tpc_v1 (mtlk_eeprom_cis_tpc_item_v1_t *tpc, mtlk_eeprom_data_t *eeprom_data,
  int cis_size)
{
  int i, num_items;
  /* counters - at the end == number of TPCs found */
  int i_tpc_5_2, i_tpc_2_4;

  /* check overall length of the CIS section - must be a multiply
    of mtlk_eeprom_cis_tpc_item_t */ 
  if (cis_size % sizeof(mtlk_eeprom_cis_tpc_item_v1_t)) {
    ELOG_DD("EEPROM TPC section contains non-integer number of TPCs? Sizes: total CIS = %d, TPC item = %d",
      cis_size, (int)sizeof(mtlk_eeprom_cis_tpc_item_v1_t));
    return;
  }
  /* parse TPC entries - according to SRD-051-221 PCI eeprom address map
     5 entries of the TPC array contained in the EEPROM are for 5.2 GHz
     band, and the sixth one, is for 2.4 GHz. */
  i_tpc_5_2 = i_tpc_2_4 = 0;
  num_items = cis_size / sizeof(mtlk_eeprom_cis_tpc_item_v1_t);
  for (i = 0; i < num_items; i++) {
    /* check band */
    if (tpc->ant0_band != tpc->ant1_band) {
      ELOG_V("EEPROM - TPC band field for Ant0 is not equal to Ant1");
      return;
    }
    if (MTLK_HW_BAND_5_2_GHZ == tpc->ant0_band) {
      if (i_tpc_5_2 < MTLK_EEPROM_NUM_TPC_5_2_GHZ) {
        /* copy data */
        convert_from_tpc_v1(eeprom_data, tpc);
      }
      /* update counter */
      i_tpc_5_2++;
    } else {
      if (i_tpc_2_4 < MTLK_EEPROM_NUM_TPC_2_4_GHZ) {
        /* copy data */
        convert_from_tpc_v1(eeprom_data, tpc);
      }
      /* update counter */
      i_tpc_2_4++;
    }
    /* next item */
    tpc++;
  }
  /* all required data parsed out? */
  if ((i_tpc_5_2 != MTLK_EEPROM_NUM_TPC_5_2_GHZ) || (i_tpc_2_4 != MTLK_EEPROM_NUM_TPC_2_4_GHZ)) {
    ELOG_DD("Wrong number of TPCs defined: 5.2GHz - %d, 2.4GHz - %d",
      i_tpc_5_2, i_tpc_2_4);
    /* if all TPCs are for 2.4GHz or 5.2GHz then it could be that TPC
       has 0xFF or 0x00 in this section */
    if ((i_tpc_2_4 == num_items) || (i_tpc_5_2 == num_items))
      WLOG_V("EEPROM may have a blank or invalid TPC section");
    return;
  }
  eeprom_data->tpc_valid = 1;
}

/*****************************************************************************
**
** NAME         mtlk_eeprom_parse_tpc_v2
**
** PARAMETERS   eeprom_data           pointer to eeprom data struc to fill in
**              tpc                   pointer to the buffer with TPC items
**              cis_size              total length of the TPC CIS
**
** RETURNS      sets eeprom_data->tpc_valid on success
**
** DESCRIPTION  This function called to perform parsing of TPC EEPROM ver 2
**
******************************************************************************/
static void
mtlk_eeprom_parse_tpc_v2 (mtlk_eeprom_cis_tpc_item_v2_t *tpc, mtlk_eeprom_data_t *eeprom_data,
  int cis_size)
{
  int i, num_items;
  /* counters - at the end == number of TPCs found */
  int i_tpc_5_2, i_tpc_2_4;

  /* check overall length of the CIS section - must be a multiply
    of mtlk_eeprom_cis_tpc_item_t */ 
  if (cis_size % sizeof(mtlk_eeprom_cis_tpc_item_v2_t)) {
    ELOG_DD("EEPROM TPC section contains non-integer number of TPCs? Sizes: total CIS = %d, TPC item = %d",
      cis_size, (int)sizeof(mtlk_eeprom_cis_tpc_item_v2_t));
    return;
  }
  /* parse TPC entries - according to SRD-051-221 PCI eeprom address map
     5 entries of the TPC array contained in the EEPROM are for 5.2 GHz
     band, and the sixth one, is for 2.4 GHz. */
  i_tpc_5_2 = i_tpc_2_4 = 0;
  num_items = cis_size / sizeof(mtlk_eeprom_cis_tpc_item_v2_t);
  for (i = 0; i < num_items; i++) {
    /* check band */
    if (tpc->ant0_band != tpc->ant1_band) {
      ELOG_V("EEPROM - TPC band field for Ant0 is not equal to Ant1");
      return;
    }

    if (MTLK_HW_BAND_5_2_GHZ == tpc->ant0_band) {
      if (i_tpc_5_2 < MTLK_EEPROM_NUM_TPC_5_2_GHZ) {
        /* copy data */
        convert_from_tpc_v2(eeprom_data, tpc);
      }
      /* update counter */
      i_tpc_5_2++;
    } else {
      if (i_tpc_2_4 < MTLK_EEPROM_NUM_TPC_2_4_GHZ) {
        /* copy data */
        convert_from_tpc_v2(eeprom_data, tpc);
      }
      /* update counter */
      i_tpc_2_4++;
    }
    /* next item */
    tpc++;
  }
  /* all required data parsed out? */
  if ((i_tpc_5_2 != MTLK_EEPROM_NUM_TPC_5_2_GHZ) || (i_tpc_2_4 != MTLK_EEPROM_NUM_TPC_2_4_GHZ)) {
    ELOG_DD("Wrong number of TPCs defined: 5.2GHz - %d, 2.4GHz - %d",
      i_tpc_5_2, i_tpc_2_4);
    /* if all TPCs are for 2.4GHz or 5.2GHz then it could be that TPC
       has 0xFF or 0x00 in this section */
    if ((i_tpc_2_4 == num_items) || (i_tpc_5_2 == num_items))
      WLOG_V("EEPROM may have a blank or invalid TPC section");
    return;
  }
  eeprom_data->tpc_valid = 1;
}

/* helper functions to parse v3 and v4 EEPROM TPC section */

static void
free_eeprom_tpc_data(mtlk_eeprom_tpc_data_t * data)
{
  uint8 ant;
  for(ant = 0; ant < MAX_NUM_TX_ANTENNAS; ant++) {
    if(NULL != data->points[ant]) {
      mtlk_osal_mem_free(data->points[ant]);
    }
  }
  mtlk_osal_mem_free(data);
}

static mtlk_eeprom_tpc_data_t *
allocate_eeprom_tpc_data(uint8 num_points, uint8 num_antennas)
{
  uint8 ant;
  mtlk_eeprom_tpc_data_t *data = NULL;

  MTLK_ASSERT(num_antennas <= MAX_NUM_TX_ANTENNAS);

  data = mtlk_osal_mem_alloc(sizeof(mtlk_eeprom_tpc_data_t), MTLK_MEM_TAG_TPC_DATA);
  
  if (NULL == data)
    return NULL;
  
  memset(data, 0, sizeof(mtlk_eeprom_tpc_data_t));
  data->num_points = num_points;

  for(ant = 0; ant < num_antennas; ant++) {
    data->points[ant] = mtlk_osal_mem_alloc(sizeof(tpc_point_t) * num_points, 
      MTLK_MEM_TAG_TPC_POINT);
    if (NULL == data->points[ant])
      goto ERROR;
  }

  return data;
  
ERROR:
  free_eeprom_tpc_data(data);
  return NULL;
}

#define POINT(ant_num, head, out_tpc) \
  out_tpc->points[0][(ant_num) - 1].x = (head->pa0_x ## ant_num ## _hi << 8) | head->pa0_x ## ant_num;\
  out_tpc->points[0][(ant_num) - 1].y = head->pa0_y ## ant_num;\
  out_tpc->points[1][(ant_num) - 1].x = (head->pa1_x ## ant_num ##_hi << 8) | head->pa1_x ## ant_num;\
  out_tpc->points[1][(ant_num) - 1].y = head->pa1_y ## ant_num
  
#define POINT_I(ant_num, item, out_tpc) \
  out_tpc->points[0][(ant_num) - 1].x = (item->head.pa0_x ## ant_num ## _hi << 8) | item->pa0_x ## ant_num;\
  out_tpc->points[0][(ant_num) - 1].y = item->pa0_y ## ant_num;\
  out_tpc->points[1][(ant_num) - 1].x = (item->head.pa1_x ## ant_num ##_hi << 8) | item->pa1_x ## ant_num;\
  out_tpc->points[1][(ant_num) - 1].y = item->pa1_y ## ant_num

static void
parse_tpc_item_header (mtlk_eeprom_cis_tpc_item_v3_header *head, mtlk_eeprom_tpc_data_t *tpc)
{
  tpc->channel = head->channel;
  tpc->band = head->band;
  tpc->spectrum_mode = 1 - head->mode;
  tpc->freq = channel_to_frequency(tpc->channel) + (tpc->spectrum_mode ? 10: 0);
  tpc->tpc_values[0] = head->ant0_tpc;
  tpc->tpc_values[1] = head->ant1_tpc;
  tpc->backoff_values[0] = head->backoff0;
  tpc->backoff_values[1] = head->backoff1;
  tpc->backoff_mult_values[0] = head->backoff_a0;
  tpc->backoff_mult_values[1] = head->backoff_a1;

  ILOG5_DDD("TPC: ch=%d, band=%d, spectrum=%d", tpc->channel, tpc->band, tpc->spectrum_mode);

  POINT(1, head, tpc);
  POINT(2, head, tpc);
}

static void
parse_tpcv4_item_data (mtlk_tpcv4_t *item, mtlk_eeprom_tpc_data_t *tpc)
{
  uint8 ant_num, point_num;
  tpc->channel = mtlk_tpcv4_get_channel(item);
  tpc->band = (mtlk_tpcv4_get_band(item) == MTLK_TPC_BAND_2GHZ) ? 1 : 0;
  tpc->spectrum_mode = (mtlk_tpcv4_get_cb_flag(item) == MTLK_TPC_CB) ? 1 : 0;;
  tpc->freq = channel_to_frequency(tpc->channel) + (tpc->spectrum_mode ? 10: 0);

  for(ant_num = 0; ant_num < NUM_TX_ANTENNAS_GEN3; ant_num++) {
    tpc->tpc_values[ant_num] = mtlk_tpcv4_get_tpc_val(item, ant_num);
    tpc->backoff_values[ant_num] = mtlk_tpcv4_get_backoff_packed(item, ant_num);
    tpc->backoff_mult_values[0] = mtlk_tpcv4_get_backoff_mul(item, ant_num);

    for(point_num = 0; point_num < mtlk_tpcv4_get_num_points(item); point_num++) {
        mtlk_tpc_point_t point;
        mtlk_tpcv4_get_point(item, ant_num, point_num, &point);
        tpc->points[ant_num][point_num].x = point.x;
        tpc->points[ant_num][point_num].y = point.y;
    }
  }
  ILOG5_DDD("TPC: ch=%d, band=%d, spectrum=%d", tpc->channel, tpc->band, tpc->spectrum_mode);
}

static void
parse_tpc_item_4ln (mtlk_eeprom_cis_tpc_item_v3_4ln_t *item, mtlk_eeprom_data_t *tpc)
{
  mtlk_eeprom_tpc_data_t *data = allocate_eeprom_tpc_data(POINTS_4LN, NUM_TX_ANTENNAS_GEN2);
  ILOG5_D("%d points", POINTS_4LN);
  
  if (data) {
    parse_tpc_item_header((mtlk_eeprom_cis_tpc_item_v3_header*)item, data);
    
    POINT_I(3, item, data);
    POINT_I(4, item, data);
    POINT_I(5, item, data);

    if (data->band) {
      data->next = tpc->tpc_24;
      tpc->tpc_24 = data;
    } else {
      data->next = tpc->tpc_52;
      tpc->tpc_52 = data;
    }

#if TPC_DEBUG
  {
    int i;
    for (i = 0; i < data->num_points; i ++) {
      ILOG5_DDD("ant0: point_%d: (%d, %d)", i, data->points[0][i].x, data->points[0][i].y);
      ILOG5_DDD("ant1: point_%d: (%d, %d)", i, data->points[1][i].x, data->points[1][i].y);
    }
  }
#endif
  }
}

static void
parse_tpc_item_3ln (mtlk_eeprom_cis_tpc_item_v3_3ln_t *item, mtlk_eeprom_data_t *tpc)
{
  mtlk_eeprom_tpc_data_t *data = allocate_eeprom_tpc_data(POINTS_3LN, NUM_TX_ANTENNAS_GEN2);
  ILOG5_D("%d points", POINTS_3LN);
  
  if (data) {
    parse_tpc_item_header((mtlk_eeprom_cis_tpc_item_v3_header*)item, data);
    
    POINT_I(3, item, data);
    POINT_I(4, item, data);
  
    if (data->band) {
      data->next = tpc->tpc_24;
      tpc->tpc_24 = data;
    } else {
      data->next = tpc->tpc_52;
      tpc->tpc_52 = data;
    }

#if TPC_DEBUG
  {
    int i;
    for (i = 0; i < data->num_points; i ++) {
      ILOG5_DDD("ant0: point_%d: (%d, %d)", i, data->points[0][i].x, data->points[0][i].y);
      ILOG5_DDD("ant1: point_%d: (%d, %d)", i, data->points[1][i].x, data->points[1][i].y);
    }
  }
#endif
  }
}

static void
parse_tpc_item_2ln (mtlk_eeprom_cis_tpc_item_v3_2ln_t *item, mtlk_eeprom_data_t *tpc)
{
  mtlk_eeprom_tpc_data_t *data = allocate_eeprom_tpc_data(POINTS_2LN, NUM_TX_ANTENNAS_GEN2);
  ILOG5_D("%d points", POINTS_2LN);
  
  if (data) {
    parse_tpc_item_header((mtlk_eeprom_cis_tpc_item_v3_header*)item, data);
    
    POINT_I(3, item, data);

    if (data->band) {
      data->next = tpc->tpc_24;
      tpc->tpc_24 = data;
    } else {
      data->next = tpc->tpc_52;
      tpc->tpc_52 = data;
    }

#if TPC_DEBUG
  {
    int i;
    for (i = 0; i < data->num_points; i ++) {
      ILOG5_DDD("ant0: point_%d: (%d, %d)", i, data->points[0][i].x, data->points[0][i].y);
      ILOG5_DDD("ant1: point_%d: (%d, %d)", i, data->points[1][i].x, data->points[1][i].y);
    }
  }
#endif
  }
}

static void
parse_tpc_item_1ln (mtlk_eeprom_cis_tpc_item_v3_1ln_t *item, mtlk_eeprom_data_t *tpc)
{
  mtlk_eeprom_tpc_data_t *data = allocate_eeprom_tpc_data(POINTS_1LN, NUM_TX_ANTENNAS_GEN2);
  ILOG5_D("%d points", POINTS_1LN);
  
  if (data) {
    parse_tpc_item_header((mtlk_eeprom_cis_tpc_item_v3_header*)item, data);
    
    if (data->band) {
      data->next = tpc->tpc_24;
      tpc->tpc_24 = data;
    } else {
      data->next = tpc->tpc_52;
      tpc->tpc_52 = data;
    }

#if TPC_DEBUG
  {
    int i;
    for (i = 0; i < data->num_points; i ++) {
      ILOG5_DDD("ant0: point_%d: (%d, %d)", i, data->points[0][i].x, data->points[0][i].y);
      ILOG5_DDD("ant1: point_%d: (%d, %d)", i, data->points[1][i].x, data->points[1][i].y);
    }
  }
#endif
  }
}

#undef POINT
#undef POINT_I

static int
_mtlk_parse_tpc4_item (mtlk_tpcv4_t *item, mtlk_eeprom_data_t *out_tpc)
{
  mtlk_eeprom_tpc_data_t *data = 
    allocate_eeprom_tpc_data(mtlk_tpcv4_get_num_points(item), NUM_TX_ANTENNAS_GEN3);

  if(!data) {
    ELOG_V("Failed to allocate TPC data structure");
    return MTLK_ERR_NO_MEM;
  }

  ILOG5_D("Gen3 TPC with %d points", mtlk_tpcv4_get_num_points(item));
  
  parse_tpcv4_item_data(item, data);

  if (data->band) {
    data->next = out_tpc->tpc_24;
    out_tpc->tpc_24 = data;
  } else {
    data->next = out_tpc->tpc_52;
    out_tpc->tpc_52 = data;
  }

#if TPC_DEBUG
  {
    int i, j;
    for (i = 0; i < data->num_points; i++) {
      for(j = 0; j < NUM_TX_ANTENNAS_GEN3; j++) {
        ILOG5_DDDD("ant%d: point_%d: (%d, %d)", j, i, data->points[j][i].x, data->points[j][i].y);
      }
    }
  }
#endif
  return MTLK_ERR_OK;
}

static void* _mtlk_tpc_list_get_next(void* item)
{
  return (void*) ((mtlk_eeprom_tpc_data_t*)item)->next;
}

static void _mtlk_tpc_list_set_next(void* item, void* next)
{
  ((mtlk_eeprom_tpc_data_t*)item)->next = (mtlk_eeprom_tpc_data_t*) next;
}

static int _mtlk_tpc_list_is_less(void* item1, void* item2)
{
  return ((mtlk_eeprom_tpc_data_t*)item1)->freq 
    < ((mtlk_eeprom_tpc_data_t*)item2)->freq;
}

static void
_mtlk_sort_tpc(mtlk_eeprom_tpc_data_t** list)
{
  mtlk_sort_slist((void**)list, _mtlk_tpc_list_get_next,
                                _mtlk_tpc_list_set_next, 
                                _mtlk_tpc_list_is_less);

}
/*****************************************************************************
**
** NAME         mtlk_eeprom_parse_tpc_v3
**
** PARAMETERS   eeprom_data           pointer to eeprom data struct to fill in
**              tpc                   pointer to the buffer with TPC items
**              total_cis_size              total length of the TPC CIS
**
** RETURNS      sets eeprom_data->tpc_valid on success
**
** DESCRIPTION  This function called to perform parsing of TPC EEPROM ver 3
**
******************************************************************************/
static BOOL
mtlk_eeprom_parse_tpc_v3 (mtlk_eeprom_cis_tpc_header_t *tpc, mtlk_eeprom_data_t *eeprom_data,
  int total_cis_size)
{
  uint8 curr_tpc_section_size = 0;
  uint8 *curr_tpc_section = tpc->data;

  total_cis_size -= TPC_CIS_HEADER_V3_SIZE;

  ILOG5_DD("size_24=%d, size_52=%d", tpc->size_24, tpc->size_52);
  
  while (curr_tpc_section < (tpc->data + total_cis_size)) {
  
    uint8 band = ((mtlk_eeprom_cis_tpc_item_v3_1ln_t*)(curr_tpc_section))->head.band;

    MTLK_ASSERT( (MTLK_TPC_BAND_2GHZ == band) || 
                 (MTLK_TPC_BAND_5GHZ == band) );

    curr_tpc_section_size = (MTLK_TPC_BAND_2GHZ == band) ? tpc->size_24 
                                                         : tpc->size_52;

    if ((total_cis_size - (curr_tpc_section - tpc->data)) < curr_tpc_section_size) {
      ELOG_DD("EEPROM TPC CIS contains partial structure: structure size %d, left in CIS section: %d",
            curr_tpc_section_size, total_cis_size - (int)(curr_tpc_section - tpc->data));
      return FALSE;
    }

    ILOG5_DD("Current section start = %d, total CIS size = %d", 
         (int)(curr_tpc_section - tpc->data), total_cis_size);
    
    switch (curr_tpc_section_size) {
      case sizeof(mtlk_eeprom_cis_tpc_item_v3_4ln_t):
        parse_tpc_item_4ln((mtlk_eeprom_cis_tpc_item_v3_4ln_t*)curr_tpc_section, eeprom_data);
        break;
      case sizeof(mtlk_eeprom_cis_tpc_item_v3_3ln_t):
        parse_tpc_item_3ln((mtlk_eeprom_cis_tpc_item_v3_3ln_t*)curr_tpc_section, eeprom_data);
        break;
      case sizeof(mtlk_eeprom_cis_tpc_item_v3_2ln_t):
        parse_tpc_item_2ln((mtlk_eeprom_cis_tpc_item_v3_2ln_t*)curr_tpc_section, eeprom_data);
        break;
      case sizeof(mtlk_eeprom_cis_tpc_item_v3_1ln_t):
        parse_tpc_item_1ln((mtlk_eeprom_cis_tpc_item_v3_1ln_t*)curr_tpc_section, eeprom_data);
        break;
      default:
        WLOG_D("EEPROM TPC CIS contains structure of unknown size: %d", curr_tpc_section_size);
        return FALSE;
      }

    /* next item */
    curr_tpc_section += curr_tpc_section_size;
  } /* while */
  
  eeprom_data->tpc_valid = 1;
  return TRUE;
}

/*****************************************************************************
**
** NAME         mtlk_eeprom_parse_tpc_v4
**
** PARAMETERS   eeprom_data           pointer to eeprom data struct to fill in
**              tpc                   pointer to the buffer with TPC items
**              total_cis_size              total length of the TPC CIS
**
** RETURNS      sets eeprom_data->tpc_valid on success
**
** DESCRIPTION  This function called to perform parsing of TPC EEPROM ver 3
**
******************************************************************************/
static BOOL
mtlk_eeprom_parse_tpc_v4 (mtlk_eeprom_cis_tpc_header_t *tpc, mtlk_eeprom_data_t *eeprom_data,
  int total_cis_size)
{
  uint8 curr_tpc_section_size = 0;
  uint8 *curr_tpc_section = tpc->data;
  mtlk_tpcv4_t tpc4_parser;

  total_cis_size -= TPC_CIS_HEADER_V3_SIZE;

  ILOG5_DD("size_24=%d, size_52=%d", tpc->size_24, tpc->size_52);
  
  while (curr_tpc_section < (tpc->data + total_cis_size)) {
    MTLK_TPC_BAND band;
    int parse_result;

    band = mtlk_tpcv4_get_band_by_raw_buffer(curr_tpc_section);
    
    MTLK_ASSERT( (MTLK_TPC_BAND_2GHZ == band) || 
                 (MTLK_TPC_BAND_5GHZ == band) );

    curr_tpc_section_size = (MTLK_TPC_BAND_2GHZ == band) ? tpc->size_24 
                                                         : tpc->size_52;

    if ((total_cis_size - (curr_tpc_section - tpc->data)) < curr_tpc_section_size) {
      ELOG_DD("EEPROM TPC CIS contains partial structure: structure size %d, left in CIS section: %d",
            curr_tpc_section_size, total_cis_size - (int)(curr_tpc_section - tpc->data));
      return FALSE;
    }

    ILOG5_DD("Current section start = %d, total CIS size = %d", 
         (int)(curr_tpc_section - tpc->data), total_cis_size);

    if(MTLK_ERR_OK != mtlk_tpcv4_init(&tpc4_parser, curr_tpc_section, curr_tpc_section_size)) {
      ELOG_V("Failed to create TPC4 parser object");
      return FALSE;
    }

    parse_result = _mtlk_parse_tpc4_item(&tpc4_parser, eeprom_data);

    mtlk_tpcv4_cleanup(&tpc4_parser);

    if(MTLK_ERR_OK != parse_result) {
      ELOG_V("Failed to parse TPC4 item");
      return FALSE;
    }
    /* next item */
    curr_tpc_section += curr_tpc_section_size;
  } /* while */
  
  eeprom_data->tpc_valid = 1;
  return TRUE;
}

static void
fill_structure_from_tpc (mtlk_eeprom_data_t *eeprom, uint8 antenna, 
                         TPC_FREQ *tpc_list, mtlk_eeprom_tpc_data_t *tpc, 
                         int full_copy)
{
  MTLK_UNREFERENCED_PARAM(eeprom);
  MTLK_ASSERT(antenna < mtlk_eeprom_get_num_antennas(eeprom));

  memset(tpc_list, 0, sizeof(TPC_FREQ));

  tpc_list->MaxTxPowerIndex = tpc->tpc_values[antenna];

  if (full_copy) {
    uint8 i;
    tpc_list->chID = tpc->channel;
    tpc_list->Backoff = tpc->backoff_values[antenna];
    tpc_list->BackoffMultiplier = tpc->backoff_mult_values[antenna];
    ILOG5_DDDD("ch=%d, mpi=%d, backoff=%d, backoff_mult=%d", tpc_list->chID,
        tpc_list->MaxTxPowerIndex, tpc_list->Backoff, tpc_list->BackoffMultiplier);

    ILOG5_D("%d points", tpc->num_points);
    for (i = 0; i < tpc->num_points; i++) {
        tpc_list->X_axis[i] = HOST_TO_MAC16(tpc->points[antenna][i].x);
        tpc_list->Y_axis[i] = tpc->points[antenna][i].y;
        ILOG5_DDD("%d: (%d, %d)", i, tpc->points[antenna][i].x, 
                                tpc->points[antenna][i].y);
      }
  }
  else {
    tpc_list->Y_axis[0] = tpc->points[antenna][0].y;
  }
}

static mtlk_eeprom_tpc_data_t*
find_struct_for_spectrum (mtlk_eeprom_tpc_data_t *tpc_list, uint16 freq, uint8 spectrum)
{
  while (tpc_list) {
    if (tpc_list->freq == freq && tpc_list->spectrum_mode == spectrum)
      return tpc_list;

    tpc_list = tpc_list->next;
  }
  
  return NULL;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
mtlk_reload_tpc_cfm_clb(mtlk_handle_t clb_usr_data, mtlk_txmm_data_t* data, mtlk_txmm_clb_reason_e reason)
{
#ifdef MTCFG_DEBUG
  uint8 i = (uint8)clb_usr_data;
  UMI_MIB *pMib = (UMI_MIB *)data->payload;
  
  
  if ((pMib->u16Status == HOST_TO_MAC16(UMI_OK)) && (reason == MTLK_TXMM_CLBR_CONFIRMED)) {
    ILOG2_D("Successfully set MIB_TPC_ANT_%d", i);
  } else {
    ELOG_DD("Failed to set MIB_TPC_ANT_%d, error %d.", i, 
        MAC_TO_HOST16(pMib->u16Status));
  }
#else
  MTLK_UNREFERENCED_PARAM(clb_usr_data);
  MTLK_UNREFERENCED_PARAM(data);
  MTLK_UNREFERENCED_PARAM(reason);
#endif

  return MTLK_TXMM_CLBA_FREE;
}

static uint16
_mtlk_get_tpc_mib_id_for_antenna(uint8 ant_number)
{
  switch(ant_number) {
    case 0:
      return MIB_TPC_ANT_0;
    case 1:
      return MIB_TPC_ANT_1;
    case 2:
      return MIB_TPC_ANT_2;
    default:
      MTLK_ASSERT(!"Should never be here");
      return 0;
  }
}

static int
_mtlk_set_tpc_mib(mtlk_eeprom_data_t *eeprom, mtlk_txmm_t *txmm, 
                  mtlk_txmm_msg_t *man_msg,
                  uint8 ant_number, mtlk_eeprom_tpc_data_t* index_freq,
                  mtlk_eeprom_tpc_data_t *close_freq_index,
                  mtlk_eeprom_tpc_data_t *tpc)
{
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_MIB *pMib;
  mtlk_eeprom_tpc_data_t *tpc1;
  List_Of_Tpc *tpc_list;
  int res;

  MTLK_ASSERT(NULL != txmm);
  MTLK_ASSERT(NULL != man_msg);
  MTLK_ASSERT(ant_number < mtlk_eeprom_get_num_antennas(eeprom));

  man_entry = mtlk_txmm_msg_get_empty_data(man_msg, txmm);
  if (man_entry == NULL) {
    ELOG_V("No free man slot available to set MIB_TPC_ANT_x");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_MAN_SET_MIB_REQ;
  man_entry->payload_size = sizeof(UMI_MIB);
  pMib = (UMI_MIB*) man_entry->payload;
  
  memset(pMib, 0, sizeof(*pMib));
  pMib->u16ObjectID = HOST_TO_MAC16(_mtlk_get_tpc_mib_id_for_antenna(ant_number));
  
  tpc_list = &pMib->uValue.sList_Of_Tpc;
  
  /* fill mib value */
  if (index_freq) {
    if (index_freq->spectrum_mode) {
      fill_structure_from_tpc(eeprom, ant_number, &tpc_list->sTPCerFreq[0], index_freq, 1);
      tpc1 = find_struct_for_spectrum(tpc, index_freq->freq, 0);
      if (tpc1)
        fill_structure_from_tpc(eeprom, ant_number, &tpc_list->sTPCerFreq[2], tpc1, 1);
      else
        fill_structure_from_tpc(eeprom, ant_number, &tpc_list->sTPCerFreq[2], index_freq, 0);
    } else {
      fill_structure_from_tpc(eeprom, ant_number, &tpc_list->sTPCerFreq[2], index_freq, 1);
      tpc1 = find_struct_for_spectrum(tpc, index_freq->freq, 1);
      if (tpc1)
        fill_structure_from_tpc(eeprom, ant_number, &tpc_list->sTPCerFreq[0], tpc1, 1);
      else
        fill_structure_from_tpc(eeprom, ant_number, &tpc_list->sTPCerFreq[0], index_freq, 0);
    }
  }

  if (!close_freq_index)
    close_freq_index = index_freq;
  
  if (close_freq_index) {
    if (close_freq_index->spectrum_mode) {
      fill_structure_from_tpc(eeprom, ant_number, &tpc_list->sTPCerFreq[1], close_freq_index, 1);
      tpc1 = find_struct_for_spectrum(tpc, close_freq_index->freq, 0);
      if (tpc1)
        fill_structure_from_tpc(eeprom, ant_number, &tpc_list->sTPCerFreq[3], tpc1, 1);
      else
        fill_structure_from_tpc(eeprom, ant_number, &tpc_list->sTPCerFreq[3], close_freq_index, 0);
    } else {
      fill_structure_from_tpc(eeprom, ant_number, &tpc_list->sTPCerFreq[3], close_freq_index, 1);
      tpc1 = find_struct_for_spectrum(tpc, close_freq_index->freq, 1);
      if (tpc1)
        fill_structure_from_tpc(eeprom, ant_number, &tpc_list->sTPCerFreq[1], tpc1, 1);
      else
        fill_structure_from_tpc(eeprom, ant_number, &tpc_list->sTPCerFreq[1], close_freq_index, 0);
    }
  }
  /* send data to MAC */
  res = mtlk_txmm_msg_send(man_msg, mtlk_reload_tpc_cfm_clb, HANDLE_T(ant_number) ,5000);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Failed to set MIB_TPC_ANT_%d, timed-out", ant_number);
    return res;
  }

  return MTLK_ERR_OK;
};

int __MTLK_IFUNC
mtlk_reload_tpc (uint8 spectrum_mode, uint8 upper_lower, uint16 channel, mtlk_txmm_t *txmm, 
                 mtlk_txmm_msg_t *man_msgs, uint32 nof_man_msgs, mtlk_eeprom_data_t *eeprom)
{
  drv_params_t params;
  uint16 s_freq;
  uint8 ant;
  uint8 freq = channel_to_band(channel);
  uint16 minimum = 10000;
  uint16 second_minimum = 0;
  uint16 freq_delta, freq_delta1;
  int res = MTLK_ERR_UNKNOWN;
  mtlk_eeprom_tpc_data_t *tpc, *prev = NULL,
                         *close_freq_index = NULL, 
                         *index_freq = NULL;

  if (eeprom->tpc_valid == 0)
    return MTLK_ERR_EEPROM;

  params.band = freq;
  params.bandwidth = ((spectrum_mode == 1) ? 40 : 20);
  params.upper_lower = (uint8) ((spectrum_mode == 1) ? upper_lower : ALTERNATE_NONE);
  params.reg_domain = 0;
  params.spectrum_mode = spectrum_mode;

  s_freq = mtlk_calc_start_freq(&params, channel);

  if (freq == MTLK_HW_BAND_2_4_GHZ)
    tpc = eeprom->tpc_24;
  else
    tpc = eeprom->tpc_52;
  
  while (tpc) {
    freq_delta = ((s_freq > tpc->freq) ? (s_freq - tpc->freq) : (tpc->freq - s_freq)); // finds distance from center frequency.

    ILOG5_DDDDD("ch=%d, freq=%d, tpc_ch=%d, tpc_freq=%d, freq_delta=%d", channel, s_freq, tpc->channel, tpc->freq, freq_delta);
   
    if (prev && (prev->freq == tpc->freq)) {
      goto NEXT;
    }
     
    if(freq_delta < minimum) {
      second_minimum = minimum;
      close_freq_index  = index_freq;
      minimum = freq_delta;
      index_freq = tpc;
      ILOG5_DD("1: min=%d, sec_min=%d", minimum, second_minimum);
    } else if (freq_delta < second_minimum) {
      second_minimum = freq_delta;
      close_freq_index = tpc;
      ILOG5_DD("2: min=%d, sec_min=%d", minimum, second_minimum);
    }
    
NEXT:
    prev = tpc;
    tpc = tpc->next;
  }

  /* Sort structures on channel ID if distances to both are equal */
  if (index_freq && close_freq_index) {
    freq_delta = ((s_freq > index_freq->freq) ? (s_freq - index_freq->freq) : (index_freq->freq - s_freq));
    freq_delta1 = ((s_freq > close_freq_index->freq) ? (s_freq - close_freq_index->freq) : (close_freq_index->freq - s_freq));

    if (freq_delta == freq_delta1) {
      if (close_freq_index->channel < index_freq->channel) {
        prev = close_freq_index;
        close_freq_index = index_freq;
        index_freq = prev;
      }
    }
  }
 
  ILOG5_DDD("ch=%d: Closest_ch_1=%d, Closest_ch_2 ch=%d", channel, 
      index_freq ? index_freq->channel : -1, 
      close_freq_index ? close_freq_index->channel : -1);
  
  /* Fill list of closest frequencies */
  /* fill structures for CB and nCB modes.
   */
  if (freq == MTLK_HW_BAND_2_4_GHZ)
    tpc = eeprom->tpc_24;
  else
    tpc = eeprom->tpc_52;

  MTLK_ASSERT(man_msgs != NULL);
  MTLK_ASSERT(nof_man_msgs >= mtlk_eeprom_get_num_antennas(eeprom));

  for(ant = 0; ant < mtlk_eeprom_get_num_antennas(eeprom); ant++)
  {
    res = _mtlk_set_tpc_mib(eeprom, txmm, &man_msgs[ant], ant, index_freq, close_freq_index, tpc);
    if(MTLK_ERR_OK != res)
      return res;
  }
 
  return MTLK_ERR_OK;
}

/* Used only to fill scan vector FREQUENCY_ELEMENT structures with TPC data */
mtlk_eeprom_tpc_data_t* __MTLK_IFUNC
mtlk_find_closest_freq (uint8 channel, mtlk_eeprom_data_t *eeprom)
{
  uint8 band; 
  uint16 freq;  
  uint16 freq_diff = 0;
  uint16 minimum = 10000;
  
  mtlk_eeprom_tpc_data_t *tpc = NULL, *res = NULL, *list;
  
  if (eeprom->tpc_valid == 0)
    return NULL;

  band = channel_to_band(channel);
  freq = channel_to_frequency(channel);

  if (band == MTLK_HW_BAND_2_4_GHZ)
    list = tpc = eeprom->tpc_24;
  else
    list = tpc = eeprom->tpc_52;

  // Additional check.
  // Ideally, this function shouldn't be called for band with non-existant TPC
  if (!tpc) {
    ILOG5_S("No TPC for %s", mtlk_eeprom_band_to_string(band));
    return NULL;
  }

  while (tpc) {
    freq_diff = (freq > tpc->freq) ? (freq - tpc->freq) : (tpc->freq - freq);

    ILOG5_DDDDD("ch=%d freq=%d, tpc_ch=%d tpc_freq=%d, freq_delta=%d", channel, freq, tpc->channel, tpc->freq, freq_diff);
      
    if (freq_diff < minimum) {
      minimum = freq_diff;
      res = tpc;
    }

    tpc = tpc->next;
  }
  
  if (res) {
    tpc = find_struct_for_spectrum(list, res->freq, 0);
    if( tpc )
      res = tpc;

    ILOG5_DDDD("result: ch=%d freq=%d, tpc_ch=%d tpc_freq=%d", channel, freq, res->channel, res->freq);
  }

  return res;
}

uint16 __MTLK_IFUNC
mtlk_get_max_tx_power(mtlk_eeprom_data_t* eeprom, uint8 channel)
{
  uint16 tpc_tx_power = (uint16) -1;

  mtlk_eeprom_tpc_data_t *tpc =
    mtlk_find_closest_freq(channel, eeprom);

  if (NULL != tpc) {
    int ant;
    for(ant = 0; ant < mtlk_eeprom_get_num_antennas(eeprom); ant++)
      if (tpc_tx_power > tpc->points[ant][0].y)
        tpc_tx_power = tpc->points[ant][0].y;
  }

  return tpc_tx_power;
}

static void
_mtlk_clean_tpc_data_list(mtlk_eeprom_tpc_data_t *tpc)
{
  mtlk_eeprom_tpc_data_t *prev;

  while (tpc) {
    prev = tpc;
    tpc = tpc->next;
    free_eeprom_tpc_data(prev);
  }
}

static const mtlk_ability_id_t _eeprom_abilities[] = {
  MTLK_CORE_REQ_GET_EEPROM_CFG,
  MTLK_CORE_REQ_GET_EE_CAPS
};

void __MTLK_IFUNC
mtlk_clean_eeprom_data (mtlk_eeprom_data_t *eeprom_data, mtlk_abmgr_t *ab_man)
{
  int i;

  for(i = 0; i < ARRAY_SIZE(_eeprom_abilities); i++)
  {
    mtlk_abmgr_disable_ability(ab_man, _eeprom_abilities[i]);
    mtlk_abmgr_unregister_ability(ab_man, _eeprom_abilities[i]);
  }

  _mtlk_clean_tpc_data_list(eeprom_data->tpc_24);
  _mtlk_clean_tpc_data_list(eeprom_data->tpc_52);
}

/*****************************************************************************
**
** NAME         mtlk_eeprom_parse
**
** PARAMETERS   eeprom_data           pointer to eeprom data struc to fill in
**              buffer                pointer to the buffer to parse
**              buffer_len            buffer length
**
** RETURNS      MTLK_ERR...
**
** DESCRIPTION  This function called to perform parsing of EEPROM
**
******************************************************************************/
static int
mtlk_eeprom_parse (int buffer_len, char *buffer, mtlk_eeprom_data_t *parsed_eeprom)
{
  int res = MTLK_ERR_EEPROM;
  int cis_size;
  mtlk_eeprom_cis_cardid_t *card_id;
  mtlk_eeprom_t *raw_eeprom = (mtlk_eeprom_t *)buffer;
  void *tpc = NULL;
  char *cis_find_coockie = NULL;

  /* assume we have no valid EEPROM */
  parsed_eeprom->valid = 0;

  /* Verify EEPROM signature */
  if (raw_eeprom->config_area.eeprom_executive_signature != HOST_TO_MAC16(MTLK_EEPROM_EXEC_SIGNATURE)) {
    ELOG_D("Invalid EEPROM Executive Signature: 0x%X). Default parameters are used.",
      MAC_TO_HOST16(raw_eeprom->config_area.eeprom_executive_signature));
    goto FINISH;
  }

  /* Parse EEPROM */
  parsed_eeprom->vendor_id = raw_eeprom->config_area.vendor_id;
  parsed_eeprom->sub_vendor_id = raw_eeprom->config_area.subsystem_vendor_id;
  parsed_eeprom->device_id = raw_eeprom->config_area.device_id;
  parsed_eeprom->sub_device_id = raw_eeprom->config_area.subsystem_id;
  
  /* card ID section is common for all versions of the EEPROM */
  cis_find_coockie = NULL;
  card_id = (mtlk_eeprom_cis_cardid_t *)
    _mtlk_eeprom_cis_find(CIS_TPL_CODE_CID, raw_eeprom, 
                          buffer_len, &cis_size, &cis_find_coockie);
  if (!card_id) {
    ELOG_V("Can not find Card ID CIS");
    goto FINISH;
  }

  /* Current implementation supports EEPROM versions from 1 to 4 */
  if ((raw_eeprom->version.version1 < 1) || (raw_eeprom->version.version1 > 4)) {
    ELOG_DD("Unsupported version of the EEPROM: %d.%d",
      (int)raw_eeprom->version.version1, (int)raw_eeprom->version.version0);
    goto FINISH;
  }

  parsed_eeprom->tpc_valid = 0;

  /* if version is 1 or above then process TPC + LNA data */
  if (raw_eeprom->version.version1 <= 3) {
    cis_find_coockie = NULL;
    tpc = _mtlk_eeprom_cis_find(CIS_TPL_CODE_TPC, raw_eeprom, 
                                buffer_len, &cis_size, &cis_find_coockie);
    if(NULL == tpc) {
      ELOG_V("Cannot find Gen2 TPC CIS");
      goto FINISH;
    }

    /* pass it for further parsing - do not exit on error, */
    switch (raw_eeprom->version.version1) {
    case 1:
      mtlk_eeprom_parse_tpc_v1(tpc, parsed_eeprom, cis_size);
      break;
    case 2:
      mtlk_eeprom_parse_tpc_v2(tpc, parsed_eeprom, cis_size);
      break;
    case 3:
      if(!mtlk_eeprom_parse_tpc_v3(tpc, parsed_eeprom, cis_size))
        goto FINISH;
      break;
    default:
      MTLK_ASSERT(!"Should never be here");
    }
  }
  else
  {
    cis_find_coockie = NULL;
    MTLK_ASSERT(raw_eeprom->version.version1 == 4);

    /* EEPROM4 may contain multiple TPC sections, */
    /* we have to parse all of them.              */
    tpc = _mtlk_eeprom_cis_find(CIS_TPL_CODE_TPCG3, raw_eeprom, 
                                buffer_len, &cis_size, &cis_find_coockie);
    if(NULL == tpc) {
      ELOG_V("At least one Gen3 TPC CIS should be present in EEPROM");
      goto FINISH;
    }

    do{

      if( (char*)tpc + cis_size > (char*)raw_eeprom + buffer_len ) {
        ELOG_V("EEPROM Contains partial Gen3 TPC CIS");
        goto FINISH;
      }

      if(!mtlk_eeprom_parse_tpc_v4(tpc, parsed_eeprom, cis_size))
        goto FINISH;

      tpc = _mtlk_eeprom_cis_find(CIS_TPL_CODE_TPCG3, raw_eeprom, 
                                  buffer_len, &cis_size, &cis_find_coockie);
    } while(NULL != tpc);
  }
  _mtlk_sort_tpc(&parsed_eeprom->tpc_24);
  _mtlk_sort_tpc(&parsed_eeprom->tpc_52);
  
  /* check and copy */
  /* We report to MAC only version 3 or higher as data will be send in v3 format. */
  if (raw_eeprom->version.version1 < 3) {
    parsed_eeprom->eeprom_version = MTLK_MAKE_EEPROM_VERSION(3,0);
  } else
    parsed_eeprom->eeprom_version = 
      MTLK_MAKE_EEPROM_VERSION(raw_eeprom->version.version1, raw_eeprom->version.version0);
  /* check card_id values */
  if ((card_id->revision < 'A') || (card_id->revision > 'Z')) {
    ELOG_DC("Invalid Card Revision: %d (%c)",
          (int)card_id->revision,
          (char)card_id->revision);
    goto FINISH;
  }

  parsed_eeprom->card_id = *card_id;

  /* If country code not recognized - indicate *unknown* country */
  if (country_code_to_domain(parsed_eeprom->card_id.country_code) == 0)
    parsed_eeprom->card_id.country_code = 0;

  /* mark data as valid */
  parsed_eeprom->valid = 1;

  res = MTLK_ERR_OK;

FINISH:
  return res;
}

/*****************************************************************************
**
** NAME         _mtlk_eeprom_read_part
**
** PARAMETERS   buffer                Buffer to read to
**              size                  Number of bytes to read
**              txmm                  TXMM API
**
** RETURNS      MTLK_ERR... or number of bytes written to buffer
**
** DESCRIPTION  Read EEPROM to buffer
**
******************************************************************************/
#ifndef MTCFG_LINDRV_HW_AHBG35

#define _MTLK_MAX_EEPROM_PART_SIZE   (sizeof(((UMI_GENERIC_MAC_REQUEST *)NULL)->data))

static int
_mtlk_eeprom_read_part(mtlk_txmm_t *txmm, int offset, int size, char *buffer)
{
  UMI_GENERIC_MAC_REQUEST* psEepromReq;
  int result = MTLK_ERR_OK;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t* man_data;
  int bytes_read = 0;

  MTLK_ASSERT(NULL != txmm);
  MTLK_ASSERT(NULL != buffer);
  MTLK_ASSERT(_MTLK_MAX_EEPROM_PART_SIZE >= size);

  man_data = mtlk_txmm_msg_init_with_empty_data(&man_msg, txmm, NULL);
  if (NULL == man_data) {
    ELOG_V("Can't read EEPROM due to lack of MAN_MSG");
    result = MTLK_ERR_EEPROM;
    goto FINISH;
  }

  man_data->id           = UM_MAN_GENERIC_MAC_REQ;
  man_data->payload_size = sizeof(*psEepromReq);

  psEepromReq            = (UMI_GENERIC_MAC_REQUEST*)man_data->payload;
  psEepromReq->opcode    = HOST_TO_MAC32(MAC_EEPROM_REQ);
  psEepromReq->size      = HOST_TO_MAC32(size);
  psEepromReq->action    = HOST_TO_MAC32(MT_REQUEST_SET);
  psEepromReq->res0      = HOST_TO_MAC32(offset);
  psEepromReq->res1      = 0;
  psEepromReq->res2      = 0;
  psEepromReq->retStatus = 0;

  ILOG5_DD("Request EEPROM read: from %d, %d byte(s)", offset, size);

  if (mtlk_txmm_msg_send_blocked(&man_msg, MTLK_EE_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
    ELOG_V("Failed to read EEPROM, timed-out");
    result = MTLK_ERR_EEPROM;
    goto FINISH;
  }

  if (HOST_TO_MAC32(UMI_OK) == psEepromReq->retStatus) {
    bytes_read = size;
  } else if (HOST_TO_MAC32(EEPROM_ILLEGAL_ADDRESS) == psEepromReq->retStatus) {
    bytes_read = HOST_TO_MAC32(psEepromReq->res2) - offset;
  } else {
    ELOG_D("Failed to read EEPROM, MAC error %d", MAC_TO_HOST32(psEepromReq->retStatus));
    result = MTLK_ERR_EEPROM;
    goto FINISH;
  }
  
  ILOG5_DDD("Read %d bytes from EEPROM, starting at offset %d, read return status is %d", 
       bytes_read, offset, MAC_TO_HOST32(psEepromReq->retStatus));

  MTLK_ASSERT(bytes_read <= size);
  memcpy(buffer, psEepromReq->data, bytes_read);
  result = MTLK_ERR_OK;

FINISH:
  if (man_data)
    mtlk_txmm_msg_cleanup(&man_msg);

  return MTLK_SUCCESS(result) ? bytes_read : result;
}

#endif /* !defined MTCFG_LINDRV_HW_AHBG35 */

#ifdef MTCFG_LINDRV_HW_AHBG35
/* Static EEPROM snapshot for FPGA testing     */
/* To be removed after Gen35 eeprom management */
/* designed and implemented                    */
static const char _static_eeprom_for_tests[] =
{
  0xFC, 0x1B, 0xCD, 0x00, 0x00, 0x00, 0x01, 0x00, 0x80, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x30, 0x1A, 0x80, 0x06, 0x00, 0x00, 0x00, 0x00, 0x08, 0xF0, 0xFF, 0xFF, 0x08, 0x00, 0xE0, 0xFF,
  0x06, 0x08, 0x7E, 0x00, 0x30, 0x1A, 0x80, 0x06, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x80, 0x10, 0xC4, 0x45, 0x45, 0x00, 0x44, 0x00,
  0x09, 0x86, 0x02, 0x06, 0x25, 0x36, 0x5B, 0x00, 0x01, 0x07, 0x81, 0xA4, 0x1A, 0x16, 0x24, 0x0C,
  0x0C, 0xA0, 0x8F, 0x67, 0x5A, 0xA9, 0x8C, 0x74, 0x56, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x34, 0x09, 0x0A, 0xA4, 0x8F, 0x73, 0x5E, 0xAE, 0x92, 0x7D, 0x5F, 0x00,
  0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x64, 0x09, 0x07, 0xAE, 0x89, 0x7D,
  0x5B, 0xB7, 0x88, 0x8C, 0x5F, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x84, 0x0B, 0x0A, 0xB1, 0x89, 0x7F, 0x5A, 0xB5, 0x89, 0x82, 0x59, 0x00, 0x01, 0x01, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9D, 0x07, 0x06, 0xB0, 0x88, 0x81, 0x5E, 0xB4, 0x89, 0x87,
  0x60, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x31, 0x11, 0x96,
  0xAC, 0x40, 0x6B, 0x91, 0xAC, 0x3B, 0x6D, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x71, 0x11, 0xAB, 0xAB, 0x41, 0x6D, 0xA5, 0xAA, 0x39,
  0x6C, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
#endif

static char*
mtlk_eeprom_read_raw_data(mtlk_txmm_t *txmm, int* bytes_read)
{
  int read_res = 0;

  char* eeprom_raw_data = 
    mtlk_osal_mem_alloc(MTLK_MAX_EEPROM_SIZE, MTLK_MEM_TAG_EEPROM);

  MTLK_ASSERT(NULL != bytes_read);

  *bytes_read = 0;

  if(NULL == eeprom_raw_data) {
    ELOG_V("Failed to allocate memory for EEPROM");
    return NULL;
  }

#ifdef MTCFG_LINDRV_HW_AHBG35
  /* Static EEPROM for FPGA testing reading      */
  /* To be removed after Gen35 eeprom management */
  /* designed and implemented                    */
  memcpy(eeprom_raw_data, _static_eeprom_for_tests, sizeof(_static_eeprom_for_tests));
  *bytes_read = sizeof(_static_eeprom_for_tests);
  MTLK_UNREFERENCED_PARAM(read_res);
#else
  do {
    MTLK_ASSERT(*bytes_read <= MTLK_MAX_EEPROM_SIZE);

    read_res = _mtlk_eeprom_read_part(txmm, *bytes_read, 
                 _MTLK_MAX_EEPROM_PART_SIZE, eeprom_raw_data + *bytes_read);

    if(MTLK_FAILURE(read_res)) {
      mtlk_osal_mem_free(eeprom_raw_data);
      ELOG_V("Failed to read EEPROM");
      return NULL;
    }

    *bytes_read += read_res;
  } while(_MTLK_MAX_EEPROM_PART_SIZE == read_res);
#endif /* !defined MTCFG_LINDRV_HW_AHBG35 */

  ILOG5_D("EEPROM reading finished, %d bytes read.", *bytes_read);
  return eeprom_raw_data;
}
/*****************************************************************************
**
** NAME         _mtlk_eeprom_get_data_size
**
** PARAMETERS   txmm                  TXMM API
**
** RETURNS      size                  total size of the EEPROM
**
** DESCRIPTION  Parse EEPROM size
**
******************************************************************************/
static int
_mtlk_eeprom_get_data_size (mtlk_eeprom_t *header)
{
  int size;

  /* CIS size - Number of bytes from address until last CIS section */
  size = MAC_TO_HOST16(header->cis_size);
  /* add PCI data size */
  size += sizeof(mtlk_eeprom_pci_cfg_t);
  return size;
}

char* __MTLK_IFUNC
mtlk_eeprom_band_to_string(unsigned band)
{
  switch (band) {
  case MTLK_HW_BAND_5_2_GHZ:
    return "5.2";
  case MTLK_HW_BAND_2_4_GHZ:
    return "2.4";
  case MTLK_HW_BAND_BOTH:
    return "Dual";
  default:
    return "Unknown";
  }
}

int __MTLK_IFUNC
mtlk_eeprom_is_band_supported(const mtlk_eeprom_data_t *ee_data, unsigned band)
{
  if ((band == MTLK_HW_BAND_BOTH && ee_data->tpc_24 && ee_data->tpc_52) ||
      (band == MTLK_HW_BAND_2_4_GHZ && ee_data->tpc_24) ||
      (band == MTLK_HW_BAND_5_2_GHZ && ee_data->tpc_52))
    return MTLK_ERR_OK;

  return MTLK_ERR_UNKNOWN;    
}

int __MTLK_IFUNC
mtlk_eeprom_is_band_valid(const mtlk_eeprom_data_t *ee_data, unsigned band)
{
  if ((band == MTLK_HW_BAND_2_4_GHZ && ee_data->tpc_24) ||
      (band == MTLK_HW_BAND_5_2_GHZ && ee_data->tpc_52))
    return MTLK_ERR_OK;

  return MTLK_ERR_UNKNOWN;
}

int __MTLK_IFUNC
mtlk_eeprom_read_and_parse(mtlk_eeprom_data_t* ee_data, mtlk_txmm_t *txmm, mtlk_abmgr_t *ab_man)
{
  int eeprom_data_size, result, full_eeprom_size;

  char* raw_eeprom_data = mtlk_eeprom_read_raw_data(txmm, &full_eeprom_size);

  if(NULL == raw_eeprom_data) {
    ELOG_V("Failed to read data from EEPROM");
    return MTLK_ERR_EEPROM;
  }

  if(full_eeprom_size < sizeof(mtlk_eeprom_t)) {
    ELOG_D("Full EEPROM size is %d bytes, which is smaller than EEPROM header. Can not continue.",
          full_eeprom_size);
    result = MTLK_ERR_EEPROM;
    goto ERROR;
  }

  eeprom_data_size = _mtlk_eeprom_get_data_size((mtlk_eeprom_t *)raw_eeprom_data);

  if(eeprom_data_size > full_eeprom_size) {
    ELOG_V("EEPROM contains invalid data: data size is bigger than total size");
    result = MTLK_ERR_EEPROM;
    goto ERROR;
  } else {
    result = mtlk_eeprom_parse(eeprom_data_size, raw_eeprom_data, ee_data);
    if (MTLK_ERR_OK != result) {
      ELOG_D("EEPROM parsing failed with code %d", result);
      goto ERROR;
    }
  }

  result = mtlk_abmgr_register_ability_set(ab_man, _eeprom_abilities, ARRAY_SIZE(_eeprom_abilities));
  if(MTLK_ERR_OK != result) {
    goto ERROR;
  }

    mtlk_abmgr_enable_ability_set(ab_man, _eeprom_abilities, ARRAY_SIZE(_eeprom_abilities));

ERROR:
  mtlk_osal_mem_free(raw_eeprom_data);

  return result;
}

void __MTLK_IFUNC mtlk_eeprom_get_cfg(mtlk_eeprom_data_t *eeprom, mtlk_eeprom_data_cfg_t *cfg)
{
  MTLK_ASSERT(NULL != eeprom);
  MTLK_ASSERT(NULL != cfg);

  cfg->eeprom_version = eeprom->eeprom_version; 
  memcpy(cfg->mac_address, eeprom->card_id.mac_address, MTLK_EEPROM_SN_LEN);
  cfg->country_code = eeprom->card_id.country_code;
  cfg->type = eeprom->card_id.type;
  cfg->revision = eeprom->card_id.revision;
  cfg->vendor_id = MAC_TO_HOST16(eeprom->vendor_id);
  cfg->device_id = MAC_TO_HOST16(eeprom->device_id);
  cfg->sub_vendor_id = MAC_TO_HOST16(eeprom->sub_vendor_id);
  cfg->sub_device_id = MAC_TO_HOST16(eeprom->sub_device_id);
  memcpy(cfg->sn, eeprom->card_id.sn, MTLK_EEPROM_SN_LEN);
  cfg->production_week = eeprom->card_id.production_week;
  cfg->production_year = eeprom->card_id.production_year;
}

int __MTLK_IFUNC mtlk_eeprom_get_raw_cfg(mtlk_txmm_t *txmm, uint8 *buf, uint32 len)
{
  uint8 *data = NULL;
  int eeprom_total_size = 0;
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != buf);
  MTLK_ASSERT(NULL != txmm);
  MTLK_ASSERT(len >= MTLK_MAX_EEPROM_SIZE);

  data = mtlk_eeprom_read_raw_data(txmm, &eeprom_total_size);

  if (NULL == data) {
    res = MTLK_ERR_NO_MEM;
    goto err_no_mem;
  }

  if (len < eeprom_total_size){
    res = MTLK_ERR_PARAMS;
    goto err_params;
  }

  memcpy(buf, data, eeprom_total_size);

err_params:
  mtlk_osal_mem_free(data);
err_no_mem:
  return res;
}

uint32 __MTLK_IFUNC mtlk_eeprom_get_size(void)
{
  return sizeof(mtlk_eeprom_t);
}

int __MTLK_IFUNC
mtlk_eeprom_get_caps (const mtlk_eeprom_data_t *eeprom, mtlk_clpb_t *clpb)
{
  int res;

  mtlk_eeprom_data_stat_entry_t stats;

  stats.ap_disabled = eeprom->card_id.dev_opt_mask.s.ap_disabled;
  stats.disable_sm_channels = eeprom->card_id.dev_opt_mask.s.disable_sm_channels;

  res = mtlk_clpb_push(clpb, &stats, sizeof(stats));
  if (MTLK_ERR_OK != res) {
    mtlk_clpb_purge(clpb);
  }

  return res;
}

 uint8 __MTLK_IFUNC
mtlk_eeprom_get_nic_type(mtlk_eeprom_data_t *eeprom_data)
{
  return eeprom_data->card_id.type;
}

uint8 __MTLK_IFUNC
mtlk_eeprom_get_nic_revision(mtlk_eeprom_data_t *eeprom_data)
{
    return eeprom_data->card_id.revision;
}

const uint8* __MTLK_IFUNC
mtlk_eeprom_get_nic_mac_addr(mtlk_eeprom_data_t *eeprom_data)
{
    return eeprom_data->card_id.mac_address;
}

uint8 __MTLK_IFUNC
mtlk_eeprom_get_country_code(mtlk_eeprom_data_t *eeprom_data)
{
  if (eeprom_data->valid)
    return eeprom_data->card_id.country_code;
  else
    return 0;
}

uint8 __MTLK_IFUNC
mtlk_eeprom_get_num_antennas(mtlk_eeprom_data_t *eeprom)
{
  return ( eeprom->eeprom_version >= MTLK_MAKE_EEPROM_VERSION(4,0) ) ? NUM_TX_ANTENNAS_GEN3
                                                                     : NUM_TX_ANTENNAS_GEN2;
}

int __MTLK_IFUNC
mtlk_eeprom_is_valid(const mtlk_eeprom_data_t *ee_data)
{
    return (ee_data->valid) ? MTLK_ERR_OK : MTLK_ERR_UNKNOWN;
}

uint8 __MTLK_IFUNC
mtlk_eeprom_get_disable_sm_channels(mtlk_eeprom_data_t *eeprom)
{
    return eeprom->card_id.dev_opt_mask.s.disable_sm_channels;
}

uint16 __MTLK_IFUNC
mtlk_eeprom_get_vendor_id(mtlk_eeprom_data_t *eeprom)
{
    return eeprom->vendor_id;
}

uint16 __MTLK_IFUNC
mtlk_eeprom_get_device_id(mtlk_eeprom_data_t *eeprom)
{
    return eeprom->device_id;
}

int __MTLK_IFUNC
mtlk_eeprom_check_ee_data(mtlk_eeprom_data_t *eeprom, mtlk_txmm_t* txmm_p, BOOL is_ap)
{
  if (MTLK_ERR_OK == mtlk_eeprom_is_valid(eeprom))
  {
    MIB_VALUE uValue;

    /* Check EEPROM options mask */
    ILOG0_D("Options mask is 0x%02x", eeprom->card_id.dev_opt_mask.d);

    if (eeprom->card_id.dev_opt_mask.s.ap_disabled && is_ap) {
      ELOG_V("AP functionality is not available on this device");
      goto err_ap_func_is_not_available;
    }

    if (mtlk_eeprom_get_disable_sm_channels(eeprom)) {
      ILOG0_V("DFS (SM-required) channels will not be used");
    }

    /* Send ee_version to MAC */
    memset(&uValue, 0, sizeof(MIB_VALUE));
    uValue.sEepromInfo.u16EEPROMVersion = cpu_to_le16(eeprom->eeprom_version);

    if (eeprom->tpc_52) {
      uValue.sEepromInfo.u8NumberOfPoints5GHz = eeprom->tpc_52->num_points;
    }

    if (eeprom->tpc_24) {
      uValue.sEepromInfo.u8NumberOfPoints2GHz = eeprom->tpc_24->num_points;
    }

    if (MTLK_ERR_OK != mtlk_set_mib_value_raw(txmm_p, MIB_EEPROM_VERSION, &uValue)) {
      ELOG_V("set mib raw value failed");
      goto err_set_mib_value_raw;
    }
  }
  return MTLK_ERR_OK;

err_set_mib_value_raw:
err_ap_func_is_not_available:
  return MTLK_ERR_UNKNOWN;
}
