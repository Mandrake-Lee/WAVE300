#include "mtlkinc.h"
#include "mtlk_rfmgmt.h"
#include "mtlkmib.h"

#include "mtlkaselirb.h"
#include "mtlk_osal.h"

#define LOG_LOCAL_GID   GID_RFMGMT
#define LOG_LOCAL_FID   1

typedef struct
{
  IEEE_ADDR src_addr;
  uint32    data_size;
  uint8     data[1];
} mtlk_rf_mgmt_spr_t;

typedef struct
{
  mtlk_dlist_t         data;
  uint32               dlim; /* MAX data items to store */
  mtlk_osal_spinlock_t lock;
} mtlk_rf_mgmt_db_t;

typedef enum 
{
  MTLK_RF_MGMT_DEID_SET_TYPE,
  MTLK_RF_MGMT_DEID_GET_TYPE,
  MTLK_RF_MGMT_DEID_SET_DEF_ASET,
  MTLK_RF_MGMT_DEID_GET_DEF_ASET,
  MTLK_RF_MGMT_DEID_GET_PEER_ASET,
  MTLK_RF_MGMT_DEID_SET_PEER_ASET,
  MTLK_RF_MGMT_DEID_SEND_SP,
  MTLK_RF_MGMT_DEID_GET_SPR,
  MTLK_RF_MGMT_DEID_LAST
} mtlk_rf_mgmt_drv_evtid_e;

struct _mtlk_rf_mgmt_t
{
  mtlk_rf_mgmt_cfg_t cfg;
  uint8              type;
  uint8              def_rf_mgmt_data;
  mtlk_rf_mgmt_db_t  spr_db;
  mtlk_irbd_handle_t *irbh[MTLK_RF_MGMT_DEID_LAST];
  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
  MTLK_DECLARE_START_LOOP(IRB_REG);
};

const static mtlk_guid_t IRBE_RF_MGMT_SET_TYPE      = MTLK_IRB_GUID_RF_MGMT_SET_TYPE;
const static mtlk_guid_t IRBE_RF_MGMT_GET_TYPE      = MTLK_IRB_GUID_RF_MGMT_GET_TYPE;
const static mtlk_guid_t IRBE_RF_MGMT_SET_DEF_DATA  = MTLK_IRB_GUID_RF_MGMT_SET_DEF_DATA;
const static mtlk_guid_t IRBE_RF_MGMT_GET_DEF_DATA  = MTLK_IRB_GUID_RF_MGMT_GET_DEF_DATA;
const static mtlk_guid_t IRBE_RF_MGMT_GET_PEER_DATA = MTLK_IRB_GUID_RF_MGMT_GET_PEER_DATA;
const static mtlk_guid_t IRBE_RF_MGMT_SET_PEER_DATA = MTLK_IRB_GUID_RF_MGMT_SET_PEER_DATA;
const static mtlk_guid_t IRBE_RF_MGMT_SEND_SP       = MTLK_IRB_GUID_RF_MGMT_SEND_SP;
const static mtlk_guid_t IRBE_RF_MGMT_GET_SPR       = MTLK_IRB_GUID_RF_MGMT_GET_SPR;
const static mtlk_guid_t IRBE_RF_MGMT_SPR_ARRIVED   = MTLK_IRB_GUID_RF_MGMT_SPR_ARRIVED;

struct mtlk_rf_mgmt_drv_evt_handler
{
  const mtlk_guid_t     *evt;
  mtlk_irbd_evt_handler_f func;
};

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_set_type(mtlk_irbd_t       *irbd,
                            mtlk_handle_t      context,
                            const mtlk_guid_t *evt,
                            void              *buffer,
                            uint32            *size);

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_type(mtlk_irbd_t       *irbd,
                            mtlk_handle_t      context,
                            const mtlk_guid_t *evt,
                            void              *buffer,
                            uint32            *size);

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_set_def_data(mtlk_irbd_t       *irbd,
                                mtlk_handle_t      context,
                                const mtlk_guid_t *evt,
                                void              *buffer,
                                uint32            *size);
static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_def_data(mtlk_irbd_t       *irbd,
                                mtlk_handle_t      context,
                                const mtlk_guid_t *evt,
                                void              *buffer,
                                uint32            *size);
static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_set_peer_data(mtlk_irbd_t       *irbd,
                                 mtlk_handle_t      context,
                                 const mtlk_guid_t *evt,
                                 void              *buffer,
                                 uint32            *size);
static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_peer_data(mtlk_irbd_t       *irbd,
                                 mtlk_handle_t      context,
                                 const mtlk_guid_t *evt,
                                 void              *buffer,
                                 uint32            *size);
static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_send_sp(mtlk_irbd_t       *irbd,
                           mtlk_handle_t      context,
                           const mtlk_guid_t *evt,
                           void              *buffer,
                           uint32            *size);
static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_spr(mtlk_irbd_t       *irbd,
                           mtlk_handle_t      context,
                           const mtlk_guid_t *evt,
                           void              *buffer,
                           uint32            *size);

const static struct mtlk_rf_mgmt_drv_evt_handler rf_mgmt_drv_evts[] = {
  { &IRBE_RF_MGMT_SET_TYPE,      _mtlk_rf_mgmt_irbh_set_type      },
  { &IRBE_RF_MGMT_GET_TYPE,      _mtlk_rf_mgmt_irbh_get_type      },
  { &IRBE_RF_MGMT_SET_DEF_DATA,  _mtlk_rf_mgmt_irbh_set_def_data  },
  { &IRBE_RF_MGMT_GET_DEF_DATA,  _mtlk_rf_mgmt_irbh_get_def_data  },
  { &IRBE_RF_MGMT_SET_PEER_DATA, _mtlk_rf_mgmt_irbh_set_peer_data },
  { &IRBE_RF_MGMT_GET_PEER_DATA, _mtlk_rf_mgmt_irbh_get_peer_data },
  { &IRBE_RF_MGMT_SEND_SP,       _mtlk_rf_mgmt_irbh_send_sp       },
  { &IRBE_RF_MGMT_GET_SPR,       _mtlk_rf_mgmt_irbh_get_spr       }
};

static mtlk_rf_mgmt_spr_t *
_mtlk_rf_mgmt_db_spr_get(mtlk_rf_mgmt_t *rf_mgmt);
static void
_mtlk_rf_mgmt_db_spr_release(mtlk_rf_mgmt_t     *rf_mgmt,
                             mtlk_rf_mgmt_spr_t *spr);
static void
_mtlk_rf_mgmt_db_spr_return(mtlk_rf_mgmt_t     *rf_mgmt,
                            mtlk_rf_mgmt_spr_t *spr);
static void
_mtlk_rf_mgmt_db_spr_set_lim(mtlk_rf_mgmt_t *rf_mgmt,
                             uint32          lim);

static int
_mtlk_rf_mgmt_set_sta_data(mtlk_rf_mgmt_t *rf_mgmt, 
                           const uint8    *mac_addr,
                           uint8           rf_mgmt_data);
static int
_mtlk_rf_mgmt_get_sta_data(mtlk_rf_mgmt_t *rf_mgmt, 
                           const uint8    *mac_addr,
                           uint8          *rf_mgmt_data);
static int
_mtlk_rf_mgmt_send_sp_blocked(mtlk_rf_mgmt_t *rf_mgmt, 
                              uint8           rf_mgmt_data, 
                              uint8           rank,
                              const void     *data, 
                              uint32          data_size);

#define RF_MGMT_SET_TYPE_TIMEOUT     3000
#define RF_MGMT_SEND_DATA_TIMEOUT    2000
#define RF_MGMT_SEND_SP_VSAF_TIMEOUT 2000

typedef struct
{
  mtlk_ldlist_entry_t lentry;
  mtlk_rf_mgmt_spr_t  spr;
} mtlk_rf_mgmt_db_entry_spr_t;

#define MTLK_RF_MGMT_DB_ENSURE_DLIM(db, type, lim)                      \
  {                                                                     \
    while (mtlk_dlist_size(&(db)->data) > (lim)) {                      \
      mtlk_ldlist_entry_t *e__ = mtlk_dlist_pop_front(&(db)->data);     \
      type                *d__ = MTLK_CONTAINER_OF(e__, type, lentry);  \
      mtlk_osal_mem_free(d__);                                          \
    }                                                                   \
  }

#define MTLK_RF_MGMT_DB_ADD_ENSURE_DLIM(db, el, type)                   \
  {                                                                     \
    uint32 l__ = (db)->dlim?((db)->dlim - 1):0;                         \
    mtlk_osal_lock_acquire(&(db)->lock);                                \
    MTLK_RF_MGMT_DB_ENSURE_DLIM(db, type, l__);                         \
    if ((db)->dlim && (el)) {                                           \
      mtlk_dlist_push_back(&(db)->data, &((type *)(el))->lentry);       \
      (el) = NULL;                                                      \
    }                                                                   \
    mtlk_osal_lock_release(&(db)->lock);                                \
  }

static uint16
_mtlk_rf_mgmt_fill_sp (mtlk_rf_mgmt_t *rf_mgmt, 
                       UMI_VSAF_INFO  *vsaf_info, 
                       const void     *data,
                       uint32          data_size,     
                       uint8           rf_mgmt_data,
                       uint8           rank)
{
  MTLK_VS_ACTION_FRAME_PAYLOAD_HEADER *vsaf_hdr = NULL;
  MTLK_VS_ACTION_FRAME_ITEM_HEADER    *item_hdr = NULL;
  void                                *sp_data  = NULL;

  /* Get pointers */
  vsaf_hdr  = (MTLK_VS_ACTION_FRAME_PAYLOAD_HEADER *)vsaf_info->au8Data;
  item_hdr  = (MTLK_VS_ACTION_FRAME_ITEM_HEADER *)(vsaf_info->au8Data + sizeof(*vsaf_hdr));
  sp_data   = (void *)(vsaf_info->au8Data + sizeof(*vsaf_hdr) + sizeof(*item_hdr));

  /* Format UMI message (sDA must be set ouside this function) */
  vsaf_info->u8Category   = ACTION_FRAME_CATEGORY_VENDOR_SPECIFIC;
  vsaf_info->au8OUI[0]    = MTLK_OUI_0;
  vsaf_info->au8OUI[1]    = MTLK_OUI_1;
  vsaf_info->au8OUI[2]    = MTLK_OUI_2;
  vsaf_info->u8RFMgmtData = rf_mgmt_data;
  vsaf_info->u8Rank       = rank;
  vsaf_info->u16Size      = HOST_TO_MAC16(sizeof(*vsaf_hdr) + 
                                          sizeof(*item_hdr) + 
                                          data_size);

  /* Format VSAF header */
  vsaf_hdr->u32Version    = HOST_TO_WLAN32(CURRENT_VSAF_FMT_VERSION);
  vsaf_hdr->u32DataSize   = HOST_TO_WLAN32(sizeof(*item_hdr) + data_size);
  vsaf_hdr->u32nofItems   = HOST_TO_WLAN32(1);

  /* Format VSAF SP Item header */
  item_hdr->u32DataSize   = HOST_TO_WLAN32(data_size);
  item_hdr->u32ID         = HOST_TO_WLAN32(MTLK_VSAF_ITEM_ID_SP);

  /* Format VSAF SP Item data */
  memcpy(sp_data, data, data_size);

  /* return VSAF payload size */
  return (uint16)(sizeof(*vsaf_hdr) + /* MTLK_VS_ACTION_FRAME_PAYLOAD_HEADER */
                  sizeof(*item_hdr) + /* MTLK_VS_ACTION_FRAME_ITEM_HEADER    */
                  data_size);         /* SP data                             */
}

static int
_mtlk_rf_mgmt_send_access_type_blocked (mtlk_rf_mgmt_t  *rf_mgmt, 
                                        UMI_RF_MGMT_TYPE *data,
                                        BOOL              set)
{
  int               res       = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg,
                                                 rf_mgmt->cfg.txmm,
                                                 &res);
  if (!man_entry) {
    ELOG_D("Can't set RF MGMT type due to lack of MM (err=%d)", res);
    goto end;
  }

  man_entry->id           = set?UM_MAN_RF_MGMT_SET_TYPE_REQ:
                                UM_MAN_RF_MGMT_GET_TYPE_REQ;
  man_entry->payload_size = sizeof(*data);

  memcpy(man_entry->payload, data, sizeof(*data));
  
  res = mtlk_txmm_msg_send_blocked(&man_msg, RF_MGMT_SET_TYPE_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Can't set RF MGMT type due to lTXMM err#%d", res);
    goto end;
  }

  memcpy(data, man_entry->payload, sizeof(*data));

  if (data->u16Status != UMI_OK) {
    res = MTLK_ERR_MAC;
    goto end;
  }

  if (set) {
    rf_mgmt->type = data->u8RFMType;
  }
  res = MTLK_ERR_OK;

end:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static int
_mtlk_rf_mgmt_send_def_data_blocked (mtlk_rf_mgmt_t       *rf_mgmt, 
                                     BOOL                  set,
                                     UMI_DEF_RF_MGMT_DATA *data)
{
  int               res       = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, rf_mgmt->cfg.txmm, NULL);
  if (!man_entry) {
    ELOG_S("Can't %s default RF MGMT data due to lack of MAN_MSG", set?"set":"get");
    res = MTLK_ERR_NO_RESOURCES;
    goto end;
  }

  man_entry->id           = set?
    UM_MAN_SET_DEF_RF_MGMT_DATA_REQ:UM_MAN_GET_DEF_RF_MGMT_DATA_REQ;
  man_entry->payload_size = sizeof(*data);

  memcpy(man_entry->payload, data, sizeof(*data));

  res = mtlk_txmm_msg_send_blocked(&man_msg, 
                                   RF_MGMT_SEND_DATA_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("default RF MGMT data sending(b) error#%d", res);
    goto end;
  }

  memcpy(data, man_entry->payload, sizeof(*data));

  if (data->u8Status != UMI_OK) {
    ELOG_SD("RF MGMT data %s MAC error#%d", set?"set":"get", data->u8Status);
    goto end;
  }

  rf_mgmt->def_rf_mgmt_data = data->u8Data;
  res                       = MTLK_ERR_OK;

end:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_set_type (mtlk_irbd_t       *irbd,
                             mtlk_handle_t      context,
                             const mtlk_guid_t *evt,
                             void              *buffer,
                             uint32            *size)
{
  mtlk_rf_mgmt_t               *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_type *data    =
    (struct mtlk_rf_mgmt_evt_type *)buffer;
  
  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_SET_TYPE) == 0);
  MTLK_ASSERT(*size == sizeof(*data));
  MTLK_ASSERT(data->type.u8RFMType == MTLK_RF_MGMT_TYPE_OFF || data->spr_queue_size != 0);

  data->result = _mtlk_rf_mgmt_send_access_type_blocked(rf_mgmt, &data->type, TRUE);
  if (data->result == MTLK_ERR_OK) {
    _mtlk_rf_mgmt_db_spr_set_lim(rf_mgmt, data->spr_queue_size);
  }
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_type (mtlk_irbd_t       *irbd,
                             mtlk_handle_t      context,
                             const mtlk_guid_t *evt,
                             void              *buffer,
                             uint32            *size)
{
  mtlk_rf_mgmt_t               *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_type *data    =
    (struct mtlk_rf_mgmt_evt_type *)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_GET_TYPE) == 0);
  MTLK_ASSERT(*size == sizeof(*data));

  data->result = _mtlk_rf_mgmt_send_access_type_blocked(rf_mgmt, &data->type, FALSE);
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_set_def_data (mtlk_irbd_t       *irbd,
                                 mtlk_handle_t      context,
                                 const mtlk_guid_t *evt,
                                 void              *buffer,
                                 uint32            *size)
{
  mtlk_rf_mgmt_t                   *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_def_data *data    = 
    (struct mtlk_rf_mgmt_evt_def_data*)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_SET_DEF_DATA) == 0);
  MTLK_ASSERT(*size == sizeof(*data));

  data->result = _mtlk_rf_mgmt_send_def_data_blocked(rf_mgmt, TRUE, &data->data);
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_def_data (mtlk_irbd_t       *irbd,
                                 mtlk_handle_t      context,
                                 const mtlk_guid_t *evt,
                                 void              *buffer,
                                 uint32            *size)
{
  mtlk_rf_mgmt_t                   *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_def_data *data    = 
    (struct mtlk_rf_mgmt_evt_def_data*)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_GET_DEF_DATA) == 0);
  MTLK_ASSERT(*size == sizeof(*data));

  data->result = _mtlk_rf_mgmt_send_def_data_blocked(rf_mgmt, FALSE, &data->data);
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_set_peer_data (mtlk_irbd_t       *irbd,
                                  mtlk_handle_t      context,
                                  const mtlk_guid_t *evt,
                                  void              *buffer,
                                  uint32            *size)
{
  mtlk_rf_mgmt_t                    *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_peer_data *data    = 
    (struct mtlk_rf_mgmt_evt_peer_data *)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_SET_PEER_DATA) == 0);
  MTLK_ASSERT(*size == sizeof(*data));

  data->result = _mtlk_rf_mgmt_set_sta_data(rf_mgmt, data->mac, data->rf_mgmt_data);
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_peer_data (mtlk_irbd_t       *irbd,
                                  mtlk_handle_t      context,
                                  const mtlk_guid_t *evt,
                                  void              *buffer,
                                  uint32            *size)
{
  mtlk_rf_mgmt_t                    *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_peer_data *data    = 
    (struct mtlk_rf_mgmt_evt_peer_data *)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_GET_PEER_DATA) == 0);
  MTLK_ASSERT(*size == sizeof(*data));

  data->result = _mtlk_rf_mgmt_get_sta_data(rf_mgmt, data->mac, &data->rf_mgmt_data);
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_send_sp (mtlk_irbd_t       *irbd,
                            mtlk_handle_t      context,
                            const mtlk_guid_t *evt,
                            void              *buffer,
                            uint32            *size)
{
  mtlk_rf_mgmt_t                  *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_send_sp *data    = 
    (struct mtlk_rf_mgmt_evt_send_sp *)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_SEND_SP) == 0);
  MTLK_ASSERT(*size == sizeof(*data) + data->data_size);

  if (rf_mgmt->cfg.device_is_busy(HANDLE_T(rf_mgmt->cfg.context)))
  {
    data->result = MTLK_ERR_BUSY;
    ILOG4_V("SP rejected");
  } else {
    data->result = _mtlk_rf_mgmt_send_sp_blocked(rf_mgmt,
                                                 data->rf_mgmt_data,
                                                 data->rank,
                                                 mtlk_rf_mgmt_evt_send_sp_data(data),
                                                 data->data_size);
    ILOG4_V("SP accepted");
  }
}

static void __MTLK_IFUNC
_mtlk_rf_mgmt_irbh_get_spr (mtlk_irbd_t       *irbd,
                            mtlk_handle_t      context,
                            const mtlk_guid_t *evt,
                            void              *buffer,
                            uint32            *size)
{
  mtlk_rf_mgmt_t                  *rf_mgmt = HANDLE_T_PTR(mtlk_rf_mgmt_t, context);
  struct mtlk_rf_mgmt_evt_get_spr *data    = 
    (struct mtlk_rf_mgmt_evt_get_spr *)buffer;
  mtlk_rf_mgmt_spr_t              *spr;

  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RF_MGMT_GET_SPR) == 0);

  /* Get next SPR from the DB */
  spr = _mtlk_rf_mgmt_db_spr_get(rf_mgmt);

  if (!spr) { /* There are no SPRs waiting */
    data->result = MTLK_ERR_NOT_READY;
    goto end;
  }
  
  if (data->buffer_size < spr->data_size) { /* Data buffer is too small */
    /* Put the SPR back to the DB */
    _mtlk_rf_mgmt_db_spr_return(rf_mgmt, spr);
    data->buffer_size = spr->data_size;
    data->result      = MTLK_ERR_BUF_TOO_SMALL;
    goto end;
  }

  /* Copy actual SPR Src Address and Data */
  memcpy(data->mac, spr->src_addr.au8Addr, ETH_ALEN);
  memcpy(mtlk_rf_mgmt_evt_get_spr_data(data), 
         spr->data,
         spr->data_size);

  /* Release the SPR since we don't need it anymore */
  _mtlk_rf_mgmt_db_spr_release(rf_mgmt, spr);

  data->buffer_size = spr->data_size;
  data->result      = MTLK_ERR_OK;

end:
  return;
}


MTLK_INIT_STEPS_LIST_BEGIN(mtlkasel)
  MTLK_INIT_STEPS_LIST_ENTRY(mtlkasel, DLIST_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(mtlkasel, LOCK_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(mtlkasel, SPR_SET_LIM)
MTLK_INIT_INNER_STEPS_BEGIN(mtlkasel)
MTLK_INIT_STEPS_LIST_END(mtlkasel);

MTLK_START_STEPS_LIST_BEGIN(mtlkasel)
  MTLK_START_STEPS_LIST_ENTRY(mtlkasel, IRB_REG)
MTLK_START_INNER_STEPS_BEGIN(mtlkasel)
MTLK_START_STEPS_LIST_END(mtlkasel);

void __MTLK_IFUNC
mtlk_rf_mgmt_stop (mtlk_rf_mgmt_t *rf_mgmt)
{
  int i = 0;

  MTLK_STOP_BEGIN(mtlkasel, MTLK_OBJ_PTR(rf_mgmt))
    for (i = 0; MTLK_STOP_ITERATIONS_LEFT(MTLK_OBJ_PTR(rf_mgmt), IRB_REG) > 0; i++) {
      MTLK_STOP_STEP_LOOP(mtlkasel, IRB_REG, MTLK_OBJ_PTR(rf_mgmt),
                          mtlk_irbd_unregister, (rf_mgmt->cfg.irbd, HANDLE_T_PTR(mtlk_irbd_handle_t, rf_mgmt->irbh[i])));
    }
  MTLK_STOP_END(mtlkasel, MTLK_OBJ_PTR(rf_mgmt));
}

int __MTLK_IFUNC
mtlk_rf_mgmt_start (mtlk_rf_mgmt_t *rf_mgmt, const mtlk_rf_mgmt_cfg_t *cfg)
{
  int i   = 0;

  MTLK_ASSERT(cfg        != NULL);
  MTLK_ASSERT(cfg->stadb != NULL);
  MTLK_ASSERT(cfg->txmm  != NULL);
  MTLK_ASSERT(cfg->irbd  != NULL);
  MTLK_ASSERT(ARRAY_SIZE(rf_mgmt->irbh) == ARRAY_SIZE(rf_mgmt_drv_evts));

  rf_mgmt->cfg = *cfg;

  MTLK_START_TRY(mtlkasel, MTLK_OBJ_PTR(rf_mgmt))
    for (i = 0; i < ARRAY_SIZE(rf_mgmt_drv_evts); ++i) {

      MTLK_START_STEP_LOOP_EX(mtlkasel, IRB_REG, MTLK_OBJ_PTR(rf_mgmt), mtlk_irbd_register, 
                              (rf_mgmt->cfg.irbd, rf_mgmt_drv_evts[i].evt, 1, rf_mgmt_drv_evts[i].func, HANDLE_T(rf_mgmt)),
                              rf_mgmt->irbh[i], rf_mgmt->irbh[i], MTLK_ERR_NO_RESOURCES);
    }
  MTLK_START_FINALLY(mtlkasel, MTLK_OBJ_PTR(rf_mgmt))
  MTLK_START_RETURN(mtlkasel, MTLK_OBJ_PTR(rf_mgmt), mtlk_rf_mgmt_stop, (rf_mgmt));
}

static void
_mtlk_rf_mgmt_cleanup (mtlk_rf_mgmt_t *rf_mgmt)
{
  MTLK_CLEANUP_BEGIN(mtlkasel, MTLK_OBJ_PTR(rf_mgmt))
    MTLK_CLEANUP_STEP(mtlkasel, SPR_SET_LIM, MTLK_OBJ_PTR(rf_mgmt),
                      _mtlk_rf_mgmt_db_spr_set_lim, (rf_mgmt, 0));
    MTLK_CLEANUP_STEP(mtlkasel, LOCK_INIT, MTLK_OBJ_PTR(rf_mgmt),
                      mtlk_osal_lock_cleanup, (&rf_mgmt->spr_db.lock));
    MTLK_CLEANUP_STEP(mtlkasel, DLIST_INIT, MTLK_OBJ_PTR(rf_mgmt),
                      mtlk_dlist_cleanup, (&rf_mgmt->spr_db.data));
  MTLK_CLEANUP_END(mtlkasel, MTLK_OBJ_PTR(rf_mgmt));
}

static int
_mtlk_rf_mgmt_init (mtlk_rf_mgmt_t *rf_mgmt)
{
  MTLK_INIT_TRY(mtlkasel, MTLK_OBJ_PTR(rf_mgmt))
    MTLK_INIT_STEP_VOID(mtlkasel, DLIST_INIT, MTLK_OBJ_PTR(rf_mgmt), 
                        mtlk_dlist_init, (&rf_mgmt->spr_db.data));  
    MTLK_INIT_STEP(mtlkasel, LOCK_INIT, MTLK_OBJ_PTR(rf_mgmt), 
                   mtlk_osal_lock_init, (&rf_mgmt->spr_db.lock));
    MTLK_INIT_STEP_VOID(mtlkasel, SPR_SET_LIM, MTLK_OBJ_PTR(rf_mgmt),
                        MTLK_NOACTION, ());
  MTLK_INIT_FINALLY(mtlkasel, MTLK_OBJ_PTR(rf_mgmt))
  MTLK_INIT_RETURN(mtlkasel, MTLK_OBJ_PTR(rf_mgmt), _mtlk_rf_mgmt_cleanup, (rf_mgmt));
}

static int
_mtlk_rf_mgmt_set_sta_data (mtlk_rf_mgmt_t *rf_mgmt, 
                            const uint8    *mac_addr,
                            uint8           rf_mgmt_data)
{
  int        res = MTLK_ERR_NOT_IN_USE;
  sta_entry* sta = mtlk_stadb_find_sta(rf_mgmt->cfg.stadb, mac_addr);

  if (sta != NULL) {
    mtlk_sta_set_rf_mgmt_data(sta, rf_mgmt_data);
    mtlk_sta_decref(sta); /* De-reference of find */

    res = MTLK_ERR_OK;
  }

  return res;
}

static int
_mtlk_rf_mgmt_get_sta_data (mtlk_rf_mgmt_t *rf_mgmt, 
                            const uint8    *mac_addr,
                            uint8          *rf_mgmt_data)
{
  int        res = MTLK_ERR_NOT_IN_USE;
  sta_entry *sta = mtlk_stadb_find_sta(rf_mgmt->cfg.stadb, mac_addr);

  MTLK_ASSERT(rf_mgmt_data != NULL);

  if (sta != NULL) {
    *rf_mgmt_data = mtlk_sta_get_rf_mgmt_data(sta);
    mtlk_sta_decref(sta); /* De-reference of find */

    res = MTLK_ERR_OK;
  }

  return res;
}

static int
_mtlk_rf_mgmt_send_sp_blocked (mtlk_rf_mgmt_t *rf_mgmt, 
                               uint8           rf_mgmt_data,
                               uint8           rank,
                               const void     *data, 
                               uint32          data_size)
{
  int                 res       = MTLK_ERR_OK;
  mtlk_txmm_msg_t     man_msg;
  mtlk_txmm_data_t   *man_entry = NULL;
  UMI_VSAF_INFO      *vsaf_info = NULL;
  const sta_entry    *sta;
  mtlk_stadb_iterator_t iter;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, rf_mgmt->cfg.txmm, NULL);
  if (!man_entry) {
    ELOG_V("Can't send Sounding Packet due to lack of MAN_MSG");
    res = MTLK_ERR_NO_RESOURCES;
    goto end;
  }

  vsaf_info = (UMI_VSAF_INFO *)man_entry->payload;

  man_entry->id           = UM_MAN_SEND_MTLK_VSAF_REQ;
  man_entry->payload_size = 
    /* header size before the data buffer + data size of VSAF with SP */
    sizeof(*vsaf_info) - sizeof(vsaf_info->au8Data) +
    _mtlk_rf_mgmt_fill_sp(rf_mgmt, vsaf_info, data, data_size, rf_mgmt_data, rank);

  sta = mtlk_stadb_iterate_first(rf_mgmt->cfg.stadb, &iter);
  if (sta) {
    do {
      int sres = MTLK_ERR_UNKNOWN;
      memcpy(vsaf_info->sDA.au8Addr, mtlk_sta_get_addr(sta), ETH_ALEN);

      ILOG3_Y("SP(b) sending to %Y", mtlk_sta_get_addr(sta));

      sres = mtlk_txmm_msg_send_blocked(&man_msg, 
                                        RF_MGMT_SEND_SP_VSAF_TIMEOUT);
      if (sres != MTLK_ERR_OK) {
        ELOG_D("SP sending(b) error#%d", sres);
        res = sres;
        break;
      }

      sta = mtlk_stadb_iterate_next(&iter);
    } while (sta);
    mtlk_stadb_iterate_done(&iter);
  }

end:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static mtlk_rf_mgmt_spr_t *
_mtlk_rf_mgmt_db_spr_get (mtlk_rf_mgmt_t *rf_mgmt)
{
  mtlk_rf_mgmt_spr_t  *spr = NULL;
  mtlk_ldlist_entry_t *e   = NULL;

  mtlk_osal_lock_acquire(&rf_mgmt->spr_db.lock);
  e = mtlk_dlist_pop_front(&rf_mgmt->spr_db.data);
  mtlk_osal_lock_release(&rf_mgmt->spr_db.lock);

  if (e != NULL) {
    mtlk_rf_mgmt_db_entry_spr_t *spr_entry = 
      MTLK_CONTAINER_OF(e, mtlk_rf_mgmt_db_entry_spr_t, lentry);
    spr = &spr_entry->spr;
  }

  return spr;
}

static void
_mtlk_rf_mgmt_db_spr_release (mtlk_rf_mgmt_t     *rf_mgmt,
                              mtlk_rf_mgmt_spr_t *spr)
{
  mtlk_rf_mgmt_db_entry_spr_t *spr_entry = 
    MTLK_CONTAINER_OF(spr, mtlk_rf_mgmt_db_entry_spr_t, spr);

  mtlk_osal_mem_free(spr_entry);
}

static void
_mtlk_rf_mgmt_db_spr_return (mtlk_rf_mgmt_t     *rf_mgmt,
                             mtlk_rf_mgmt_spr_t *spr)
{
  mtlk_rf_mgmt_db_entry_spr_t *spr_entry = 
    MTLK_CONTAINER_OF(spr, mtlk_rf_mgmt_db_entry_spr_t, spr);

  mtlk_osal_lock_acquire(&rf_mgmt->spr_db.lock);
  mtlk_dlist_push_front(&rf_mgmt->spr_db.data, &spr_entry->lentry);
  mtlk_osal_lock_release(&rf_mgmt->spr_db.lock);
}

static void
_mtlk_rf_mgmt_db_spr_set_lim (mtlk_rf_mgmt_t *rf_mgmt,
                              uint32          lim)
{
  MTLK_ASSERT(rf_mgmt != NULL);

  mtlk_osal_lock_acquire(&rf_mgmt->spr_db.lock);
  rf_mgmt->spr_db.dlim = lim;
  MTLK_RF_MGMT_DB_ENSURE_DLIM(&rf_mgmt->spr_db, mtlk_rf_mgmt_db_entry_spr_t, lim);
  mtlk_osal_lock_release(&rf_mgmt->spr_db.lock);
}

mtlk_rf_mgmt_t * __MTLK_IFUNC
mtlk_rf_mgmt_create (void)
{
  mtlk_rf_mgmt_t *rf_mgmt = NULL;

  rf_mgmt = (mtlk_rf_mgmt_t *)mtlk_osal_mem_alloc(sizeof(*rf_mgmt), 
                                                  MTLK_MEM_TAG_RFMGMT);
  if (rf_mgmt == NULL) {
    ELOG_V("Can't allocate RF MGMT object!");
  }
  else {
    int res;

    memset(rf_mgmt, 0, sizeof(*rf_mgmt));

    res = _mtlk_rf_mgmt_init(rf_mgmt);
    if (res != MTLK_ERR_OK) {
      ELOG_D("Can't initiate RF MGMT object (err=%d)!", res);
      mtlk_osal_mem_free(rf_mgmt);
      rf_mgmt = NULL;
    }
  }

  return rf_mgmt;
}

void __MTLK_IFUNC
mtlk_rf_mgmt_delete (mtlk_rf_mgmt_t *rf_mgmt)
{
  MTLK_ASSERT(rf_mgmt != NULL);
  _mtlk_rf_mgmt_cleanup(rf_mgmt);
  mtlk_osal_mem_free(rf_mgmt);
}

int  __MTLK_IFUNC
mtlk_rf_mgmt_handle_spr (mtlk_rf_mgmt_t  *rf_mgmt, 
                         const IEEE_ADDR *src_addr, 
                         uint8           *buffer, 
                         uint16           size)
{
  int                          res       = MTLK_ERR_UNKNOWN;
  mtlk_rf_mgmt_db_entry_spr_t *spr_entry = NULL;
  struct mtlk_rf_mgmt_evt_spr_arrived spr_evt;

  MTLK_ASSERT(buffer != NULL);
  MTLK_ASSERT(size != 0);

  ILOG3_YD("SPR received: src=%Y size=%d bytes", src_addr->au8Addr, size);

  if (!rf_mgmt->spr_db.dlim) {
    /* no SPR dbg records required */
    res = MTLK_ERR_OK;
    goto end;
  }

  spr_entry = 
    (mtlk_rf_mgmt_db_entry_spr_t *)mtlk_osal_mem_alloc(sizeof(*spr_entry) -
                                                       sizeof(spr_entry->spr.data) + 
                                                       size,
                                                       MTLK_MEM_TAG_RFMGMT);
  if (!spr_entry) {
    ELOG_D("Can't allocate SPR entry of %d bytes", 
          sizeof(*spr_entry) - sizeof(spr_entry->spr.data) + size);
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  spr_entry->spr.src_addr  = *src_addr;
  spr_entry->spr.data_size = size;
  memcpy(spr_entry->spr.data, buffer, size);

  MTLK_RF_MGMT_DB_ADD_ENSURE_DLIM(&rf_mgmt->spr_db, spr_entry, mtlk_rf_mgmt_db_entry_spr_t);

  if (spr_entry) { /* has not been added for some reason */
    mtlk_osal_mem_free(spr_entry);
    res = MTLK_ERR_UNKNOWN;
    goto end;
  }

  spr_evt.required_buff_size = size;

  res = mtlk_irbd_notify_app(rf_mgmt->cfg.irbd, &IRBE_RF_MGMT_SPR_ARRIVED, &spr_evt, sizeof(spr_evt));

  if (res != MTLK_ERR_OK) {
    WLOG_D("Can't notify IRB application (err = %d)", res); 
  }

  res = MTLK_ERR_OK;

end:
  return res;
}

