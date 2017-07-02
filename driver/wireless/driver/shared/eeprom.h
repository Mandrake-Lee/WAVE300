#ifndef _MTLK_EEPROM_H_
#define _MTLK_EEPROM_H_

#include "mtlk_ab_manager.h"
#include "mhi_mib_id.h"
#include "txmm.h"
#include "mtlk_clipboard.h"

#define  MTLK_IDEFS_ON
#include "mtlkidefs.h"

/* TODO: GS: Move it in some common configuration file */
#define NUM_TX_ANTENNAS_GEN2 (2)
#define NUM_TX_ANTENNAS_GEN3 (3)
#define MAX_NUM_TX_ANTENNAS  NUM_TX_ANTENNAS_GEN3

/* hardware codes as read from the EEPROM.
   These constants are used for Windows debug messages */
#define MTLK_EEPROM_HW_CODE_EVM          0xC5
#define MTLK_EEPROM_HW_CODE_CARDBUS      0xC6
#define MTLK_EEPROM_HW_CODE_LONGPCI      0xC4
#define MTLK_EEPROM_HW_CODE_SHORTPCI     0xC9

#define MTLK_EEPROM_SN_LEN     3
#define MTLK_MAX_EEPROM_SIZE   (1024)

typedef struct _mtlk_eeprom_data_t mtlk_eeprom_data_t;

struct _mtlk_eeprom_data_cfg_t;

/* band TODO: GS: Move it in some common configuration file */
typedef enum _mtlk_hw_band {
  MTLK_HW_BAND_5_2_GHZ,
  MTLK_HW_BAND_2_4_GHZ,
  MTLK_HW_BAND_BOTH,
  MTLK_HW_BAND_NONE
} mtlk_hw_band;

/* EEPROM to DF UI interface structure*/
typedef struct {
  BOOL   ap_disabled;
  BOOL   disable_sm_channels;
} __MTLK_IDATA mtlk_eeprom_data_stat_entry_t;


mtlk_eeprom_data_t* __MTLK_IFUNC mtlk_eeprom_create(void);
void __MTLK_IFUNC mtlk_eeprom_delete(mtlk_eeprom_data_t *eeprom_data);

int __MTLK_IFUNC mtlk_eeprom_read_and_parse(mtlk_eeprom_data_t* ee_data, mtlk_txmm_t *txmm, mtlk_abmgr_t *ab_man);

int __MTLK_IFUNC
mtlk_eeprom_check_ee_data(mtlk_eeprom_data_t *eeprom, mtlk_txmm_t* txmm_p, BOOL is_ap);

int __MTLK_IFUNC
mtlk_reload_tpc (uint8 spectrum_mode, uint8 upper_lower, uint16 channel, mtlk_txmm_t *txmm,
        mtlk_txmm_msg_t *msgs, uint32 nof_msgs, mtlk_eeprom_data_t *eeprom);

void __MTLK_IFUNC mtlk_clean_eeprom_data(mtlk_eeprom_data_t *eeprom_data, mtlk_abmgr_t *ab_man);

char* __MTLK_IFUNC mtlk_eeprom_band_to_string(unsigned band);

/* EEPROM data accessors */

int __MTLK_IFUNC mtlk_eeprom_is_band_supported(const mtlk_eeprom_data_t *ee_data, unsigned band);

int __MTLK_IFUNC mtlk_eeprom_is_band_valid(const mtlk_eeprom_data_t *ee_data, unsigned band);

int __MTLK_IFUNC mtlk_eeprom_is_valid(const mtlk_eeprom_data_t *ee_data);

uint8 __MTLK_IFUNC mtlk_eeprom_get_nic_type(mtlk_eeprom_data_t *eeprom_data);

uint8 __MTLK_IFUNC mtlk_eeprom_get_nic_revision(mtlk_eeprom_data_t *eeprom_data);

const uint8* __MTLK_IFUNC mtlk_eeprom_get_nic_mac_addr(mtlk_eeprom_data_t *eeprom_data);

/*
 * If eeprom has no valid country (i.e country code has no associated domain)
 * or eeprom is not valid at all - return zero, thus indicate *unknown* country.
 * Country validation performed on initial eeprom read.
 */
uint8 __MTLK_IFUNC mtlk_eeprom_get_country_code(mtlk_eeprom_data_t *eeprom_data);

void mtlk_eeprom_get_cfg(mtlk_eeprom_data_t *eeprom, struct _mtlk_eeprom_data_cfg_t *cfg);

int mtlk_eeprom_get_raw_cfg(mtlk_txmm_t *txmm, uint8 *buf, uint32 len);

int __MTLK_IFUNC mtlk_eeprom_get_caps (const mtlk_eeprom_data_t *eeprom, mtlk_clpb_t *clpb);

uint8 __MTLK_IFUNC mtlk_eeprom_get_num_antennas(mtlk_eeprom_data_t *eeprom);

uint8 __MTLK_IFUNC mtlk_eeprom_get_disable_sm_channels(mtlk_eeprom_data_t *eeprom);

uint16 __MTLK_IFUNC mtlk_eeprom_get_vendor_id(mtlk_eeprom_data_t *eeprom);

uint16 __MTLK_IFUNC mtlk_eeprom_get_device_id(mtlk_eeprom_data_t *eeprom);

uint32 __MTLK_IFUNC mtlk_eeprom_get_size(void);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif
