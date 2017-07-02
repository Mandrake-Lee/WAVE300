/*
 * $Id: mtlkhal.h 12479 2012-01-19 16:27:53Z laptijev $
 *
 * Copyright (c) 2007 Metalink Broadband Ltd.
 *
 * HAL interface
 *
 */

#ifndef __MTLKHAL_H__
#define __MTLKHAL_H__

#include "mhi_umi.h"
#include "mtlkdfdefs.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/**********************************************************************
 * Common definitions. Used by HW and Core (logic module)
***********************************************************************/
typedef struct _mtlk_hw_msg_t                 mtlk_hw_msg_t;
typedef struct _mtlk_persistent_device_data_t mtlk_persistent_device_data_t;

typedef struct _mtlk_hal_file_t
{
  const char* buffer;
  uint32      size;
} __MTLK_IDATA mtlk_hal_file_t;
/**********************************************************************/

/**********************************************************************
 * HW => Core (logic module):
 *  - must be implemented by core (logic module)
 *  - called HW layer
***********************************************************************/
typedef struct _mtlk_core_release_tx_data_t
{
  mtlk_hw_msg_t       *msg;             /* HW Message passed to mtlk_hw_send_data() */
  mtlk_nbuf_t         *nbuf;            /* Network buffer                           */
  uint32               size;            /* Data size                                */
  uint8                access_category; /* Packet's access category                 */
  uint32               resources_free;  /* HW TX resources are available flag       */
  UMI_STATUS           status;          /* Is Tx failed?                            */
  uint16               nof_retries;     /* Number of retries made by FW             */
} __MTLK_IDATA mtlk_core_release_tx_data_t;

/* MTLK_CORE_PROP_TXM_STUCK_DETECTED additional info */
typedef enum
{
  MTLK_TXM_MAN,
  MTLK_TXM_DBG,
  MTLK_TXM_BCL,
  MTLK_TXM_LAST
} mtlk_txm_obj_id_e;

typedef enum _mtlk_core_prop_e
{/* prop_id */
  MTLK_CORE_PROP_FIRMWARE_BIN_BUFFER, /* SET: mtlk_core_fw_buffer_t*, GET: unsupported */
  MTLK_CORE_PROP_MAC_SW_RESET_ENABLED,/* SET: unsupported,            GET: uint32*     */
  MTLK_CORE_PROP_MAC_STUCK_DETECTED,  /* SET: uint32 *fw_cpu_no,      GET: unsupported */
  MTLK_CORE_PROP_LAST
} mtlk_core_prop_e;

#define MAX_FIRMWARE_FILENAME_LEN 64

typedef struct _mtlk_core_firmware_file_t
{
  char                   fname[MAX_FIRMWARE_FILENAME_LEN];
  mtlk_hal_file_t        content;
  mtlk_handle_t          context; /* for internal usage */
} __MTLK_IDATA mtlk_core_firmware_file_t;

#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
typedef struct _mtlk_core_fw_log_buffer_t
{
  uint32 addr;
  uint32 length;
} __MTLK_IDATA mtlk_core_fw_log_buffer_t;
#endif

typedef struct _mtlk_core_handle_rx_data_t
{
  mtlk_nbuf_t               *nbuf;     /* Network buffer        */
  uint32                     size;     /* Data size             */
  uint8                      offset;   /* Offset                */
  MAC_RX_ADDITIONAL_INFO_T  *info;     /* MAC additional info   */
} __MTLK_IDATA mtlk_core_handle_rx_data_t;

typedef int    (__MTLK_IFUNC *mtlk_hal_core_start_f)(mtlk_vap_handle_t vap_handle);
typedef int    (__MTLK_IFUNC *mtlk_hal_core_release_tx_data_f)(mtlk_vap_handle_t vap_handle, const mtlk_core_release_tx_data_t *data);
typedef int    (__MTLK_IFUNC *mtlk_hal_core_handle_rx_data_f)(mtlk_vap_handle_t vap_handle, mtlk_core_handle_rx_data_t *data);
typedef void   (__MTLK_IFUNC *mtlk_hal_core_handle_rx_ctrl_f)(mtlk_vap_handle_t vap_handle,
                                                              uint32            id,
                                                   void*                payload,
                                                   uint32               payload_buffer_size);
typedef int    (__MTLK_IFUNC *mtlk_hal_core_get_prop_f)(mtlk_vap_handle_t vap_handle, mtlk_core_prop_e prop_id, void* buffer, uint32 size);
typedef int    (__MTLK_IFUNC *mtlk_hal_core_set_prop_f)(mtlk_vap_handle_t vap_handle, mtlk_core_prop_e prop_id, void* buffer, uint32 size);
typedef void   (__MTLK_IFUNC *mtlk_hal_core_stop_f)(mtlk_vap_handle_t vap_handle);
typedef void   (__MTLK_IFUNC *mtlk_hal_core_prepare_stop_f)(mtlk_vap_handle_t vap_handle);

/* core specific function table
  NOTE: all functions should be initialized by core, no any NULL values
        accepted. In case of unsupported functionality the function with
        empty body required!
 */
typedef struct _mtlk_core_vft_t
{
  mtlk_hal_core_start_f           start;
  mtlk_hal_core_release_tx_data_f release_tx_data;
  mtlk_hal_core_handle_rx_data_f  handle_rx_data;
  mtlk_hal_core_handle_rx_ctrl_f  handle_rx_ctrl;   /* MTLK_ERR_PENDING for pending. In this be used to respond later.  */
  mtlk_hal_core_get_prop_f        get_prop;
  mtlk_hal_core_set_prop_f        set_prop;
  mtlk_hal_core_stop_f            stop;
  mtlk_hal_core_prepare_stop_f    prepare_stop;
} __MTLK_IDATA mtlk_core_vft_t;

typedef struct _mtlk_core_api_t
{
  mtlk_core_t           *obj;
  mtlk_core_vft_t const *vft;
} __MTLK_IDATA mtlk_core_api_t;

mtlk_core_api_t* __MTLK_IFUNC mtlk_core_api_create (mtlk_vap_handle_t vap_handle, mtlk_df_t* df);
void __MTLK_IFUNC mtlk_core_api_delete (mtlk_core_api_t *core_api);

/**********************************************************************/

/**********************************************************************
 * Core (logic module) => HW :
 *  - must be implemented by HW layer
 *  - called by core (logic module)
***********************************************************************/

typedef enum _mtlk_hw_prop_e
{/* prop_id */
  MTLK_HW_PROP_STATE,             /* buffer: GET: mtlk_hw_state_e*,              SET - mtlk_hw_state_e*                              */
  MTLK_HW_FREE_TX_MSGS,           /* buffer: GET: uint32*,                       SET - not supported                                 */
  MTLK_HW_TX_MSGS_USED_PEAK,      /* buffer: GET: uint32*,                       SET - not supported                                 */
  MTLK_HW_PROGMODEL,              /* buffer: GET: mtlk_core_firmware_file_t*,    SET - const mtlk_core_firmware_file_t*              */
  MTLK_HW_DUMP,                   /* buffer: GET: mtlk_hw_dump_t*,               SET - not supported                                 */
  MTLK_HW_BCL_ON_EXCEPTION,       /* buffer: GET: UMI_BCL_REQUEST*,              SET - UMI_BCL_REQUEST*                              */
  MTLK_HW_PRINT_BUS_INFO,         /* buffer: GET: char*,                         SET - not supported                                 */
  MTLK_HW_BIST,                   /* buffer: GET: uint32*,                       SET - not supported                                 */
  MTLK_HW_PROGMODEL_FREE,         /* buffer: GET: not supported,                 SET - mtlk_core_firmware_file_t*                    */
  MTLK_HW_RESET,                  /* buffer: GET: not supported,                 SET - void                                          */
  MTLK_HW_DBG_ASSERT_FW,          /* buffer: GET: not supported,                 SET - uint32* (LMIPS or UMIPS)                      */
#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
  MTLK_HW_FW_LOG_BUFFER,          /* buffer: GET: not supported,                 SET - const mtlk_core_fw_log_buffer_t*              */
  MTLK_HW_LOG,                    /* buffer: GET: not suppotted,                 SET - const mtlk_log_event_t*                       */
#endif
  MTLK_HW_FW_CAPS_MAX_STAs,       /* buffer: GET: uint32*,                       SET - not supported                                 */
  MTLK_HW_FW_CAPS_MAX_VAPs,       /* buffer: GET: uint32*,                       SET - not supported                                 */
  MTLK_HW_FW_BUFFERS_PROCESSED,   /* buffer: GET: uint32*,                       SET - not supported                                 */
  MTLK_HW_FW_BUFFERS_DROPPED,     /* buffer: GET: uint32*,                       SET - not supported                                 */
  MTLK_HW_IRBD,                   /* buffer: GET: mtlk_irbd_t**,                 SET - not supported                                 */
  MTLK_HW_WSS,                    /* buffer: GET: mtlk_wss_t**,                  SET - not supported                                 */
  MTLK_HW_PROP_LAST
} mtlk_hw_prop_e;

typedef struct _mtlk_hw_dump_t
{
  uint32 addr;
  uint32 size;
  void*  buffer;
} __MTLK_IDATA mtlk_hw_dump_t;

typedef struct _mtlk_hw_send_data_t
{
  mtlk_hw_msg_t       *msg;             /* HW Message returned by mtlk_hw_get_msg_to_send() */
  mtlk_nbuf_t         *nbuf;            /* Network buffer                                   */
  uint32               size;            /* Data size                                        */
  const IEEE_ADDR     *rcv_addr;        /* WDS MAC address                                  */
  uint8                access_category; /* Packet's access category                         */
  uint8                wds;             /* WDS packet flag                                  */
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  uint8                rf_mgmt_data;    /* RF Management related data, 
                                         * MTLK_RF_MGMT_DATA_DEFAULT if none    
                                         */
#endif
  uint8                encap_type;      /* Encapsulation type, one of defined by UMI        */
} __MTLK_IDATA mtlk_hw_send_data_t;

typedef enum _mtlk_hw_state_e
{
  MTLK_HW_STATE_HALTED,
  MTLK_HW_STATE_INITIATING,
  MTLK_HW_STATE_WAITING_READY,
  MTLK_HW_STATE_READY,
  MTLK_HW_STATE_ERROR,
  MTLK_HW_STATE_EXCEPTION,
  MTLK_HW_STATE_APPFATAL,
  MTLK_HW_STATE_UNLOADING,
  MTLK_HW_STATE_LAST
} mtlk_hw_state_e;

typedef mtlk_hw_msg_t* (__MTLK_IFUNC *mtlk_hw_get_msg_to_send_f)(mtlk_vap_handle_t vap_handle, uint32 *nof_free_tx_msgs);
typedef int            (__MTLK_IFUNC *mtlk_hw_send_data_f)(mtlk_vap_handle_t vap_handle, const mtlk_hw_send_data_t *data);
typedef int            (__MTLK_IFUNC *mtlk_hw_release_msg_to_send_f)(mtlk_vap_handle_t vap_handle, mtlk_hw_msg_t *msg);
typedef int            (__MTLK_IFUNC *mtlk_hw_set_prop_f)(mtlk_vap_handle_t vap_handle,
                                                          mtlk_hw_prop_e    prop_id,
                                                          void              *buffer,
                                                          uint32            size);
typedef int            (__MTLK_IFUNC *mtlk_hw_get_prop_f)(mtlk_vap_handle_t vap_handle,
                                                          mtlk_hw_prop_e    prop_id,
                                                          void              *buffer,
                                                          uint32            size);

typedef struct _mtlk_hw_vft_t
{
  mtlk_hw_get_msg_to_send_f     get_msg_to_send;
  mtlk_hw_send_data_f           send_data;
  mtlk_hw_release_msg_to_send_f release_msg_to_send;
  mtlk_hw_set_prop_f            set_prop;
  mtlk_hw_get_prop_f            get_prop;
} __MTLK_IDATA mtlk_hw_vft_t;
  
struct _mtlk_hw_api_t
{
  const mtlk_hw_vft_t *vft;
  mtlk_hw_t           *hw;
};

#ifdef MTCFG_TSF_TIMER_ACCESS_ENABLED
uint32          __MTLK_IFUNC mtlk_hw_get_timestamp(mtlk_vap_handle_t vap_handle);
#endif /* MTCFG_TSF_TIMER_ACCESS_ENABLED */
/**********************************************************************/

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* !__MTLKHAL_H__ */
