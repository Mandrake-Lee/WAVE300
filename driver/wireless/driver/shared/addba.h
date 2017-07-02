#ifndef _MTLK_ADDBA_H_
#define _MTLK_ADDBA_H_

#include "txmm.h"
#include "mhi_ieee_address.h"
#include "mtlk_osal.h"
#include "mtlk_reflim.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/* Number of Traffic IDs (TID) */
#define MTLK_ADDBA_NOF_TIDs                8

/* ADDBA Re-sending: resend timeout  */
#define MTLK_ADDBA_RE_TX_REQ_TIMEOUT_MS    3000U

/* ADDBA Negative ADD BA responses: max attempts */ 
#define MTLK_ADDBA_MAX_REJECTS 5 

/* DELBA Timeout: addba_timeout parameter units  */
#define MTLK_ADDBA_BA_TIMEOUT_UNIT_US      1024U

/* Default maximal number of aggregations supported */
#define MTLK_ADDBA_DEF_MAX_AGGR_SUPPORTED  16
/* Default maximal number of reorderings supported */
#define MTLK_ADDBA_DEF_MAX_REORD_SUPPORTED 0xFFFFFFFF /* unlimited */

/* Maximal reordering window size supported */
#define MTLK_ADDBA_MAX_REORD_WIN_SIZE      64

/* ADDBA response result codes - according to Leonid's doc (SRD-051-225) */
#define MTLK_ADDBA_RES_CODE_SUCCESS      0
#define MTLK_ADDBA_RES_CODE_FAILURE      1
#define MTLK_ADDBA_RES_CODE_RESERVED     2
#define MTLK_ADDBA_RES_CODE_REFUSED_WFA  3
#define MTLK_ADDBA_RES_CODE_REFUSED_IEEE 37
#define MTLK_ADDBA_RES_CODE_INVAL_PARAM  38

/* Priority class aggregation negotiation states */
typedef enum _mtlk_addba_peer_tx_state_e
{
  MTLK_ADDBA_TX_NONE,
  MTLK_ADDBA_TX_ADDBA_REQ_SENT, 
  MTLK_ADDBA_TX_ADDBA_REQ_CFMD,
  MTLK_ADDBA_TX_ADD_AGGR_REQ_SENT,
  MTLK_ADDBA_TX_AGGR_OPENED,
  MTLK_ADDBA_TX_DEL_AGGR_REQ_SENT,
  MTLK_ADDBA_TX_LAST
} mtlk_addba_peer_tx_state_e;

/* Priority class reordering negotiation states */
typedef enum _mtlk_addba_peer_rx_state_e
{
  MTLK_ADDBA_RX_NONE,
  MTLK_ADDBA_RX_ADDBA_NEGATIVE_RES_SENT,
  MTLK_ADDBA_RX_ADDBA_POSITIVE_RES_SENT,
  MTLK_ADDBA_RX_REORD_IN_PROCESS,
  MTLK_ADDBA_RX_DELBA_REQ_SENT,
  MTLK_ADDBA_RX_LAST
} mtlk_addba_peer_rx_state_e;

typedef enum
{
  MTLK_ADDBAPI_CNT_AGGR_ACTIVE,
  MTLK_ADDBAPI_CNT_REORD_ACTIVE,
  MTLK_ADDBAPI_CNT_ADDBA_REQUEST_TX,
  MTLK_ADDBAPI_CNT_ADDBA_POSITIVE_RESPONSE_TX,
  MTLK_ADDBAPI_CNT_ADDBA_NEGATIVE_RESPONSE_TX,
  MTLK_ADDBAPI_CNT_ADDBA_REQUEST_RX,
  MTLK_ADDBAPI_CNT_ADDBA_POSITIVE_RESPONSE_RX,
  MTLK_ADDBAPI_CNT_ADDBA_NEGATIVE_RESPONSE_RX,
  MTLK_ADDBAPI_CNT_ADDBA_UNCONFIRMED_RESPONSE_RX,
  MTLK_ADDBAPI_CNT_DELBA_REQUEST_TX,
  MTLK_ADDBAPI_CNT_DELBA_SENT_REQUEST_RX,
  MTLK_ADDBAPI_CNT_DELBA_RECEIVED_REQUEST_RX,
  MTLK_ADDBAPI_CNT_AGGR_OPEN_REQUEST,
  MTLK_ADDBAPI_CNT_AGGR_OPEN_CONFIRM,
  MTLK_ADDBAPI_CNT_AGGR_CLOSE_REQUEST,
  MTLK_ADDBAPI_CNT_AGGR_CLOSE_CONFIRM,
  MTLK_ADDBAPI_CNT_LAST
} mtlk_addba_peer_cnt_id_e;

/* Priority class aggregation negotiation info */
typedef struct _mtlk_addba_peer_tx_t
{
  mtlk_addba_peer_tx_state_e state;
  mtlk_osal_msec_t           addba_req_cfmd_time;
  uint8                      addba_req_dlgt;
  uint8                      addba_res_rejects;
  mtlk_txmm_msg_t            man_msg;
}  __MTLK_IDATA mtlk_addba_peer_tx_t;

/* Priority class reordering negotiation info */
typedef struct _mtlk_addba_peer_rx_t
{
  mtlk_addba_peer_rx_state_e state;
  uint16                     delba_timeout;
  uint32                     req_tstamp; 
  mtlk_txmm_msg_t            man_msg;
}  __MTLK_IDATA mtlk_addba_peer_rx_t;

/* Priority class negotiation info */
typedef struct _mtlk_addba_peer_tid_t
{
  mtlk_addba_peer_tx_t tx;
  mtlk_addba_peer_rx_t rx;
}  __MTLK_IDATA mtlk_addba_peer_tid_t;

struct _mtlk_addba_t;

/* ADDBA Peer Outward API 
 *
 * ADDBA module owner must fill this structure and pass it to the ADDBA Peer object
 * in order to allow ADDBA Peer -> Owner communication.
 *
 * mtlk_addba_wrap_api_t's usr_data field is the Owner-dependent context to be passed
 * to the Outward API calls as is.
 */
typedef struct _mtlk_addba_wrap_api_t
{
  mtlk_handle_t      usr_data;

  uint32             (__MTLK_IFUNC *get_last_rx_timestamp)(mtlk_handle_t usr_data,
                                                           uint16        tid);
  void               (__MTLK_IFUNC *on_start_aggregation)(mtlk_handle_t usr_data, 
                                                          uint16        tid);
  void               (__MTLK_IFUNC *on_stop_aggregation)(mtlk_handle_t usr_data, 
                                                         uint16        tid);
  void               (__MTLK_IFUNC *on_start_reordering)(mtlk_handle_t usr_data, 
                                                         uint16        tid,
                                                         uint16        ssn,
                                                         uint8         win_size);
  void               (__MTLK_IFUNC *on_stop_reordering)(mtlk_handle_t usr_data, 
                                                        uint16        tid);
  void               (__MTLK_IFUNC *on_ba_req_rejected)(mtlk_handle_t usr_data, 
                                                        uint16 tid);
  void               (__MTLK_IFUNC *on_ba_req_unconfirmed)(mtlk_handle_t usr_data, 
                                                           uint16        tid);
} __MTLK_IDATA mtlk_addba_wrap_api_t;

/* Peer negotiation info */
typedef struct _mtlk_addba_peer_t
{
  IEEE_ADDR               addr;
  mtlk_addba_peer_tid_t   tid[MTLK_ADDBA_NOF_TIDs];
  volatile BOOL           is_active; /* peer should be served by ADDBA module */
  mtlk_addba_wrap_api_t   api;
  struct _mtlk_addba_t   *addba;
  mtlk_wss_t             *wss;
  mtlk_wss_cntr_handle_t *wss_hcntrs[MTLK_ADDBAPI_CNT_LAST];
  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_INIT_LOOP(TXMM_MSG_INIT_TX);
  MTLK_DECLARE_INIT_LOOP(TXMM_MSG_INIT_RX);  
}  __MTLK_IDATA mtlk_addba_peer_t;


/* ADDBA module TID parameters */
typedef struct _mtlk_addba_tid_cfg_t
{
  int    use_aggr;                 /* UseAggrgation            */
  int    accept_aggr;              /* AcceptAggregation        */
  uint16 max_nof_packets;          /* MaxNumOfPackets          */
  uint32 max_nof_bytes;            /* MaxNumOfBytes            */
  uint32 timeout_interval;         /* TimeoutInterval          */
  uint32 min_packet_size_in_aggr;  /* MinSizeOfPacketInAggr    */
  uint16 addba_timeout;            /* ADDBA timeout            */
  uint8  aggr_win_size;            /* Aggregation Window Size  */
}  __MTLK_IDATA mtlk_addba_tid_cfg_t;

#define MTLK_ADDBA_RATE_ADAPTIVE 0xFFFF

/* ADDBA module parameters */
typedef struct _mtlk_addba_cfg_t
{
  /* Data */
  mtlk_addba_tid_cfg_t tid[MTLK_ADDBA_NOF_TIDs];
}  __MTLK_IDATA mtlk_addba_cfg_t;

/* ADDBA module object */
typedef struct _mtlk_addba_t
{
  mtlk_addba_cfg_t      cfg;
  mtlk_txmm_t          *txmm;
  mtlk_reflim_t        *aggr_reflim;
  mtlk_reflim_t        *reord_reflim;
  mtlk_vap_handle_t     vap_handle;
  uint8                 next_dlg_token;
  mtlk_osal_spinlock_t  lock;

  MTLK_DECLARE_INIT_STATUS;
}  __MTLK_IDATA mtlk_addba_t;

/* Init/CleanUp */
int  __MTLK_IFUNC mtlk_addba_init(mtlk_addba_t                *obj,
                                  mtlk_txmm_t                 *txmm,
                                  mtlk_reflim_t               *aggr_reflim,
                                  mtlk_reflim_t               *reord_reflim,
                                  const mtlk_addba_cfg_t      *cfg,
                                  mtlk_vap_handle_t           vap_handle);
void __MTLK_IFUNC mtlk_addba_cleanup(mtlk_addba_t* obj);

/* Live re-configuration */
int  __MTLK_IFUNC mtlk_addba_reconfigure(mtlk_addba_t           *obj, 
                                         const mtlk_addba_cfg_t *cfg);

/* Peer Init/CleanUp */
int  __MTLK_IFUNC mtlk_addba_peer_init(mtlk_addba_peer_t           *peer,
                                       const mtlk_addba_wrap_api_t *api,
                                       mtlk_addba_t                *addba,
                                       mtlk_wss_t                  *parent_wss);
void __MTLK_IFUNC mtlk_addba_peer_cleanup(mtlk_addba_peer_t *peer);

/* Peer Start/Stop */
int  __MTLK_IFUNC mtlk_addba_peer_start(mtlk_addba_peer_t *peer, 
                                        const IEEE_ADDR   *addr);
void __MTLK_IFUNC mtlk_addba_peer_stop(mtlk_addba_peer_t *peer);


/* Peer IND Handlers - common */
void __MTLK_IFUNC 
mtlk_addba_peer_on_delba_req_rx(mtlk_addba_peer_t *peer, 
                                uint16             tid_idx,
                                uint16             res_code,
                                uint16             initiator);

/* IND Handlers - TX PATH */
void __MTLK_IFUNC
mtlk_addba_peer_on_addba_res_rx(mtlk_addba_peer_t *peer, 
                                uint16             res_code,
                                uint16             tid_idx,
                                uint8              dlgt);

/* IND Handlers - RX PATH */
void __MTLK_IFUNC
mtlk_addba_peer_on_addba_req_rx(mtlk_addba_peer_t *peer,
                                uint16             ssn,
                                uint16             tid_idx,
                                uint8              win_size,
                                uint8              dlgt,
                                uint16             tmout,
                                uint16             ack_policy,
                                uint16             rate);

/* Auxiliary */
void __MTLK_IFUNC
mtlk_addba_peer_iterate(mtlk_addba_peer_t *peer);

/* Control - high level API */
void __MTLK_IFUNC
mtlk_addba_peer_start_negotiation(mtlk_addba_peer_t *peer, 
                                  uint16             rate);

/* Control - TID level API (Dynamic BA preparations) */
void __MTLK_IFUNC
mtlk_addba_peer_start_aggr_negotiation(mtlk_addba_peer_t *peer,
                                       uint16             tid);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* !_MTLK_ADDBA_H_ */

