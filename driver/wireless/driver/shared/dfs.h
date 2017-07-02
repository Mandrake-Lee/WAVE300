#ifndef __MTLK_11H_H__
#define __MTLK_11H_H__



#include "mtlk_osal.h"
#include "mtlkflctrl.h"


#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"


/**********************************************************************
 * definitions and database
***********************************************************************/

#define MTLK_DOT11H_STATUS_BUFSIZE 32

typedef enum _mtlk_dfs_event_e {
  MTLK_DFS_EVENT_RADAR_DETECTED,
  MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL,
  MTLK_DFS_EVENT_CHANGE_CHANNEL_SILENT,
  MTLK_DFS_CHANGE_CHANNEL_DONE,
  MTLK_DFS_CHANGE_CHANNEL_FAILED_TIMEOUT,
  MTLK_DFS_CHANGE_CHANNEL_FAILED_STOP,
  MTLK_DFS_EVENT_LAST
} mtlk_dfs_event_e;


typedef struct _mtlk_dot11h_wrap_api_t
{
  mtlk_txmm_t*      txmm;
  mtlk_aocs_t*      aocs;
  mtlk_flctrl_t*    hw_tx_flctrl;
} __MTLK_IDATA mtlk_dot11h_wrap_api_t;

typedef struct _mtlk_dot11h_t   mtlk_dot11h_t;
typedef struct _mtlk_dot11h_cfg_t mtlk_dot11h_cfg_t;

/**********************************************************************
 * function declaration
***********************************************************************/
mtlk_dot11h_t* __MTLK_IFUNC mtlk_dfs_create(void);
void __MTLK_IFUNC mtlk_dfs_delete(mtlk_dot11h_t *dfs_data);

int __MTLK_IFUNC mtlk_dot11h_init(mtlk_dot11h_t                *obj,
                                  const mtlk_dot11h_cfg_t      *cfg,
                                  const mtlk_dot11h_wrap_api_t *api,
                                  mtlk_vap_handle_t            vap_handle);

void __MTLK_IFUNC mtlk_dot11h_cleanup(mtlk_dot11h_t *obj);

int16 __MTLK_IFUNC
mtlk_dot11h_get_debug_next_channel(mtlk_dot11h_t *dot11h_obj);

void __MTLK_IFUNC
mtlk_dot11h_set_debug_next_channel(mtlk_dot11h_t *dot11h_obj, int16 channel);

int __MTLK_IFUNC
mtlk_dot11h_debug_event (mtlk_dot11h_t *dot11h_obj, uint8 event, uint16 channel);

void __MTLK_IFUNC
__MTLK_IFUNC mtlk_dot11h_status (mtlk_dot11h_t *dot11h_obj, char *buffer, uint32 size);

int __MTLK_IFUNC
mtlk_dot11h_handle_radar_ind(mtlk_handle_t dot11h, const void *payload, uint32 size);

int __MTLK_IFUNC
mtlk_dot11h_handle_channel_switch_ind(mtlk_handle_t dot11h, const void *payload, uint32 size);

int __MTLK_IFUNC
mtlk_dot11h_handle_channel_switch_done(mtlk_handle_t dot11h, const void *payload, uint32 size);

int __MTLK_IFUNC
mtlk_dot11h_handle_channel_pre_switch_done(mtlk_handle_t dot11h, const void *payload, uint32 size);

BOOL __MTLK_IFUNC mtlk_dot11h_is_data_stop(mtlk_dot11h_t *obj);
BOOL __MTLK_IFUNC mtlk_dot11h_can_switch_now(mtlk_dot11h_t *obj);

void __MTLK_IFUNC mtlk_dot11h_set_event(mtlk_dot11h_t *obj, mtlk_dfs_event_e event);
void __MTLK_IFUNC mtlk_dot11h_set_spectrum_mode(mtlk_dot11h_t *obj, uint8 spectrum_mode);
void __MTLK_IFUNC mtlk_dot11h_set_dbg_channel_availability_check_time(
    mtlk_dot11h_t *obj,
    uint8 channel_availability_check_time);
void __MTLK_IFUNC mtlk_dot11h_set_dbg_channel_switch_count(
    mtlk_dot11h_t *obj,
    uint8 channel_switch_count);

int16 __MTLK_IFUNC mtlk_dot11h_get_dbg_channel_availability_check_time(mtlk_dot11h_t *obj);
int8 __MTLK_IFUNC mtlk_dot11h_get_dbg_channel_switch_count(mtlk_dot11h_t *obj);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_11H_H__ */


