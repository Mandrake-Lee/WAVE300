#ifndef __DRVMGR_H__
#define __DRVMGR_H__

#include "compat.h"
#include "dataex.h"

/* Application return codes */
#define EVENT_DO_NOTHING 0
#define EVENT_REQ_RESET  1
#define EVENT_REQ_RMMOD  2
#define EVENT_CLI_EXIT   3
#define EVENT_INT_ERR    4
#define EVENT_SIGTERM    5

/* Application bitmask fields */
#define DHFLAG_NO_DRV_HUNG_HANDLING  0x00000001
#define DHFLAG_NO_DRV_RMMOD_HANDLING 0x00000002
#define DHFLAG_NO_WRITE_LED          0x00000004

#define MTLK_LED_STRING_BUFFER_SIZE 50
#define MTLK_DELAY_READ_SECURITY_CONF 5

/* band */
typedef enum _mtlk_hw_band {
  MTLK_HW_BAND_5_2_GHZ,
  MTLK_HW_BAND_2_4_GHZ,
  MTLK_HW_BAND_BOTH,
  MTLK_HW_BAND_NONE
} mtlk_hw_band;


#endif // !__DRVMGR_H__

