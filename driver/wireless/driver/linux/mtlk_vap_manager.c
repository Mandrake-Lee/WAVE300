#include "mtlkinc.h"
#include "mtlk_vap_manager.h"
#include "mtlk_df.h"
#include "mtlkhal.h"
#include "mtlk_param_db.h"
#include "mtlk_ab_manager.h"

#define LOG_LOCAL_GID   GID_VAPM
#define LOG_LOCAL_FID   0

static int __MTLK_IFUNC _mtlk_vap_init(mtlk_vap_handle_t vap_handle, mtlk_vap_manager_t *obj, uint8 vap_index);

#define MAX_BSS_COUNT        5
#define MTLK_VAP_INVALID_IDX ((uint8)-1)

typedef struct _mtlk_vap_manager_vap_node_t
{
  struct _mtlk_vap_handle_t vap_info;
  mtlk_txmm_t               txmm;
  mtlk_txmm_t               txdm;
  BOOL                      active;
} mtlk_vap_manager_vap_node_t;

struct _mtlk_vap_manager_t
{
  mtlk_vap_manager_vap_node_t  guest_vap_array[MAX_BSS_COUNT];

  mtlk_osal_spinlock_t     guest_lock;

  mtlk_bus_drv_t          *bus_drv;

  mtlk_hw_api_t           *hw_api;
  mtlk_irbd_t             *hw_irbd;
  mtlk_wss_t              *hw_wss;

  mtlk_txmm_base_t        *txmm;
  mtlk_txmm_base_t        *txdm;

  mtlk_atomic_t           activated_vaps_num;
  BOOL                    is_ap;

  MTLK_DECLARE_INIT_STATUS;
};


MTLK_INIT_STEPS_LIST_BEGIN(vap_manager)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_manager, VAP_MANAGER_PREPARE_SYNCHRONIZATION)
MTLK_INIT_INNER_STEPS_BEGIN(vap_manager)
MTLK_INIT_STEPS_LIST_END(vap_manager);

MTLK_INIT_STEPS_LIST_BEGIN(vap_handler)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_PDB_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_DF_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_ABMGR_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_CORE_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_IRBD_ALLOC)
MTLK_INIT_INNER_STEPS_BEGIN(vap_handler)
MTLK_INIT_STEPS_LIST_END(vap_handler);

MTLK_START_STEPS_LIST_BEGIN(vap_handler)
  MTLK_START_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_IRBD_INIT)
  MTLK_START_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_CORE_START)
  MTLK_START_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_DF_START)
MTLK_START_INNER_STEPS_BEGIN(vap_handler)
MTLK_START_STEPS_LIST_END(vap_handler);

static __INLINE mtlk_vap_handle_t
__mtlk_vap_manager_vap_handle_by_id (mtlk_vap_manager_t* obj, uint8 vap_id)
{
  MTLK_ASSERT(obj != NULL);
  MTLK_ASSERT(vap_id < ARRAY_SIZE(obj->guest_vap_array));

  return &obj->guest_vap_array[vap_id].vap_info;
}

static __INLINE void
__mtlk_vap_manager_vap_set_active (mtlk_vap_manager_t *obj, uint8 vap_index)
{
  obj->guest_vap_array[vap_index].active = TRUE;
}

static __INLINE void
__mtlk_vap_manager_vap_unset_active (mtlk_vap_manager_t *obj, uint8 vap_index)
{
  obj->guest_vap_array[vap_index].active = FALSE;
}

static __INLINE void
__mtlk_vap_manager_vap_set_txmm (mtlk_vap_manager_t *obj, mtlk_vap_handle_t vap_handle, uint8 vap_index)
{
  obj->guest_vap_array[vap_index].txmm.base       = obj->txmm;
  obj->guest_vap_array[vap_index].txmm.vap_handle = vap_handle;
}

static __INLINE mtlk_txmm_t*
__mtlk_vap_manager_vap_get_txmm (mtlk_vap_manager_t *obj, uint8 vap_index)
{
  return &obj->guest_vap_array[vap_index].txmm;
}

static __INLINE void
__mtlk_vap_manager_vap_set_txdm (mtlk_vap_manager_t *obj, mtlk_vap_handle_t vap_handle, uint8 vap_index)
{
  obj->guest_vap_array[vap_index].txdm.base       = obj->txdm;
  obj->guest_vap_array[vap_index].txdm.vap_handle = vap_handle;
}

static __INLINE mtlk_txmm_t*
__mtlk_vap_manager_vap_get_txdm (mtlk_vap_manager_t *obj, uint8 vap_index)
{
  return &obj->guest_vap_array[vap_index].txdm;
}

static BOOL _mtlk_vap_manager_vap_exists (mtlk_vap_manager_t *obj, uint8 vap_index)
{
  MTLK_ASSERT(obj != NULL);
  MTLK_ASSERT(vap_index < ARRAY_SIZE(obj->guest_vap_array));

  return obj->guest_vap_array[vap_index].active;
}

static __INLINE mtlk_irbd_t*
__mtlk_vap_manager_get_hw_irbd (mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(obj);
  MTLK_ASSERT(obj->hw_irbd);

  return obj->hw_irbd;
}

static void __MTLK_IFUNC 
_mtlk_vap_manager_cleanup(mtlk_vap_manager_t* obj)
{
    MTLK_ASSERT(NULL != obj);

    MTLK_CLEANUP_BEGIN(vap_manager, MTLK_OBJ_PTR(obj))
      MTLK_CLEANUP_STEP(vap_manager, VAP_MANAGER_PREPARE_SYNCHRONIZATION, MTLK_OBJ_PTR(obj),
                        mtlk_osal_lock_cleanup, (&obj->guest_lock));
    MTLK_CLEANUP_END(vap_manager, MTLK_OBJ_PTR(obj));
}

static int __MTLK_IFUNC
_mtlk_vap_manager_init(mtlk_vap_manager_t* obj, mtlk_bus_drv_t *bus_drv, BOOL  is_ap)
{
    MTLK_ASSERT(NULL != obj);

    MTLK_INIT_TRY(vap_manager, MTLK_OBJ_PTR(obj))
      obj->bus_drv = bus_drv;
      obj->is_ap = is_ap;
      mtlk_osal_atomic_set(&obj->activated_vaps_num, 0);

      MTLK_INIT_STEP(vap_manager, VAP_MANAGER_PREPARE_SYNCHRONIZATION, MTLK_OBJ_PTR(obj),
                     mtlk_osal_lock_init, (&obj->guest_lock));
    MTLK_INIT_FINALLY(vap_manager, MTLK_OBJ_PTR(obj))
    MTLK_INIT_RETURN(vap_manager, MTLK_OBJ_PTR(obj), _mtlk_vap_manager_cleanup, (obj))
}

mtlk_vap_manager_t * __MTLK_IFUNC 
mtlk_vap_manager_create(mtlk_bus_drv_t* bus_drv, BOOL  is_ap)
{
    mtlk_vap_manager_t *vap_manager;

    MTLK_ASSERT(bus_drv != NULL);

    vap_manager = (mtlk_vap_manager_t *)mtlk_osal_mem_alloc(sizeof(mtlk_vap_manager_t), MTLK_MEM_TAG_VAP_MANAGER);
    if(NULL == vap_manager) {
      return NULL;
    }

    memset(vap_manager, 0, sizeof(mtlk_vap_manager_t));

    if (MTLK_ERR_OK != _mtlk_vap_manager_init(vap_manager, bus_drv, is_ap)) {
      mtlk_osal_mem_free(vap_manager);
      return NULL;
    }

    return vap_manager;
}

void __MTLK_IFUNC
mtlk_vap_manager_delete (mtlk_vap_manager_t *obj)
{
  _mtlk_vap_manager_cleanup(obj);
  mtlk_osal_mem_free(obj);
}

int __MTLK_IFUNC
mtlk_vap_manager_create_vap (mtlk_vap_manager_t *obj, 
                             mtlk_vap_handle_t  *_vap_handle,
                             uint8 vap_index)
{
  mtlk_vap_handle_t vap_handle = MTLK_INVALID_VAP_HANDLE;
  int               res_value;

  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(NULL != _vap_handle);

  if (vap_index >= MAX_BSS_COUNT) {
    ELOG_D("Invalid VAP ID %d", vap_index);
    return MTLK_VAP_INVALID_IDX;
  }

  if (_mtlk_vap_manager_vap_exists(obj, vap_index)) {
    ELOG_D("VAP %d already exists", vap_index);
    return MTLK_VAP_INVALID_IDX;
  }

  vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, vap_index);
  MTLK_ASSERT(NULL != vap_handle);

  res_value = _mtlk_vap_init(vap_handle, obj, vap_index);
  if (MTLK_ERR_OK != res_value) {
    return res_value;
  }

  __mtlk_vap_manager_vap_set_active(obj, vap_index);
  *_vap_handle = vap_handle;

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC 
mtlk_vap_manager_delete_all_vaps(mtlk_vap_manager_t *obj)
{
  int vap_index = 0;

  MTLK_ASSERT(NULL != obj);

  for(vap_index = MAX_BSS_COUNT - 1; vap_index >= 0; vap_index--) {
    if (_mtlk_vap_manager_vap_exists(obj, vap_index)) {
      mtlk_vap_handle_t vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, vap_index);
      mtlk_vap_delete(vap_handle);
    }
  }
}

void __MTLK_IFUNC
mtlk_vap_manager_prepare_start(mtlk_vap_manager_t *obj, mtlk_handle_t txmm_handle, mtlk_handle_t txdm_handle)
{
  mtlk_vap_handle_t master_vap_handle = NULL;

  MTLK_ASSERT(NULL != obj);

  obj->hw_irbd = NULL;
  obj->hw_wss  = NULL;
  if (mtlk_vap_manager_get_master_vap(obj, &master_vap_handle) == MTLK_ERR_OK) {
    if (mtlk_vap_get_hw_vft(master_vap_handle)->get_prop(master_vap_handle,
                                                         MTLK_HW_IRBD,
                                                         &obj->hw_irbd,
                                                         sizeof(&obj->hw_irbd)) != MTLK_ERR_OK) {
      obj->hw_irbd = NULL;
    }
    if (mtlk_vap_get_hw_vft(master_vap_handle)->get_prop(master_vap_handle,
                                                         MTLK_HW_WSS,
                                                         &obj->hw_wss,
                                                         sizeof(&obj->hw_wss)) != MTLK_ERR_OK) {
      obj->hw_wss = NULL;
    }
  }

  MTLK_ASSERT(NULL != obj->hw_irbd);
  MTLK_ASSERT(NULL != obj->hw_wss);

  obj->txmm = HANDLE_T_PTR(mtlk_txmm_base_t, txmm_handle);
  obj->txdm = HANDLE_T_PTR(mtlk_txmm_base_t, txdm_handle);

  MTLK_ASSERT(NULL != obj->txmm);
  MTLK_ASSERT(NULL != obj->txdm);
}

void __MTLK_IFUNC 
mtlk_vap_manager_prepare_stop(mtlk_vap_manager_t *obj) 
{
  int vap_index =0;
  MTLK_ASSERT(NULL != obj);

  for(vap_index = MAX_BSS_COUNT - 1; vap_index >= 0; vap_index--) {
    if (_mtlk_vap_manager_vap_exists(obj, vap_index)) {
      mtlk_vap_handle_t vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, vap_index);

      mtlk_vap_get_core_vft(vap_handle)->prepare_stop(vap_handle);
    }
  }

  obj->hw_irbd = NULL;
  obj->hw_wss  = NULL;
}

void __MTLK_IFUNC 
mtlk_vap_manager_stop_all_vaps(mtlk_vap_manager_t *obj, mtlk_vap_manager_interface_e intf)
{
  int vap_index =0;
  MTLK_ASSERT(NULL != obj);

  for(vap_index = MAX_BSS_COUNT - 1; vap_index >= 0; vap_index--) {
    if (_mtlk_vap_manager_vap_exists(obj, vap_index)) {
      mtlk_vap_handle_t vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, vap_index);
      mtlk_vap_stop(vap_handle, intf);
    }
  }
}

int __MTLK_IFUNC
mtlk_vap_manager_get_master_vap (mtlk_vap_manager_t *obj,
                                 mtlk_vap_handle_t  *vap_handle)
{
  int res = MTLK_ERR_NOT_IN_USE;

  MTLK_ASSERT(obj != NULL);
  MTLK_ASSERT(vap_handle != NULL);

  if (_mtlk_vap_manager_vap_exists(obj, MTLK_MASTER_VAP_ID)) {
    *vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, MTLK_MASTER_VAP_ID);
    res = MTLK_ERR_OK;
  }

  return res;
}

int __MTLK_IFUNC
mtlk_vap_manager_get_vap_handle (mtlk_vap_manager_t *obj,
                                 uint8               vap_id,
                                 mtlk_vap_handle_t  *vap_handle)
{
  int res = MTLK_ERR_UNKNOWN;

  MTLK_ASSERT(obj != NULL);
  MTLK_ASSERT(vap_handle != NULL);

  if (!_mtlk_vap_manager_vap_exists(obj, vap_id)) {
    res = MTLK_ERR_NOT_IN_USE;
    goto FINISH;
  }

  *vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, vap_id);
  res         = MTLK_ERR_OK;

FINISH:
  return res;
}

mtlk_bus_drv_t* __MTLK_IFUNC
mtlk_vap_manager_get_bus_drv(mtlk_vap_manager_t *obj)
{
  return obj->bus_drv;
}

mtlk_hw_t* __MTLK_IFUNC
mtlk_vap_manager_get_hw(mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(NULL != obj->hw_api->hw);

  return obj->hw_api->hw;
}

void __MTLK_IFUNC
mtlk_vap_manager_set_hw_api(mtlk_vap_manager_t *obj, mtlk_hw_api_t *hw_api)
{
  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(NULL != hw_api);

  obj->hw_api = hw_api;
}

mtlk_hw_api_t * __MTLK_IFUNC
mtlk_vap_manager_get_hw_api(mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(NULL != obj->hw_api);

  return obj->hw_api;
}

mtlk_wss_t * __MTLK_IFUNC
mtlk_vap_manager_get_hw_wss (mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(NULL != obj->hw_wss);

  return obj->hw_wss;
}

void __MTLK_IFUNC
mtlk_vap_manager_notify_vap_activated(mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  mtlk_osal_atomic_inc(&obj->activated_vaps_num);
}

void __MTLK_IFUNC
mtlk_vap_manager_notify_vap_deactivated(mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  mtlk_osal_atomic_dec(&obj->activated_vaps_num);
}

uint32 __MTLK_IFUNC
mtlk_vap_manager_get_active_vaps_number(mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  return mtlk_osal_atomic_get(&obj->activated_vaps_num);
}

mtlk_core_t * __MTLK_IFUNC
mtlk_vap_get_core (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->core_api);

  return _info->core_api->obj;
}

mtlk_core_vft_t const * __MTLK_IFUNC
mtlk_vap_get_core_vft (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->core_api);
  MTLK_ASSERT(NULL != _info->core_api->vft);

  return _info->core_api->vft;
}

BOOL __MTLK_IFUNC
mtlk_vap_manager_is_ap(mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  return obj->is_ap;
}

mtlk_hw_vft_t const * __MTLK_IFUNC
mtlk_vap_get_hw_vft (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->manager->hw_api);
  MTLK_ASSERT(NULL != _info->manager->hw_api->vft);

  return _info->manager->hw_api->vft;
}

static void __MTLK_IFUNC
_mtlk_vap_cleanup (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *vap_info;

  MTLK_ASSERT(NULL != vap_handle);
  vap_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_CLEANUP_BEGIN(vap_handler, MTLK_OBJ_PTR(vap_info))
    MTLK_CLEANUP_STEP(vap_handler, VAP_HANDLER_IRBD_ALLOC, MTLK_OBJ_PTR(vap_info),
                      mtlk_irbd_free, (vap_info->irbd));
    MTLK_CLEANUP_STEP(vap_handler, VAP_HANDLER_CORE_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_core_api_delete, (vap_info->core_api));
    MTLK_CLEANUP_STEP(vap_handler, VAP_HANDLER_ABMGR_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_abmgr_delete, (vap_info->abmgr));
    MTLK_CLEANUP_STEP(vap_handler, VAP_HANDLER_DF_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_df_delete, (vap_info->df));
    MTLK_CLEANUP_STEP(vap_handler, VAP_HANDLER_PDB_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_pdb_delete, (vap_info->param_db));
  MTLK_CLEANUP_END(vap_handler, MTLK_OBJ_PTR(vap_info))
}

static int __MTLK_IFUNC
_mtlk_vap_init (mtlk_vap_handle_t vap_handle, mtlk_vap_manager_t *obj, uint8 vap_index)
{
  mtlk_vap_info_internal_t *vap_info;

  MTLK_ASSERT(vap_handle != NULL);
  vap_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_INIT_TRY(vap_handler, MTLK_OBJ_PTR(vap_info))
    vap_info->manager = obj;
    vap_info->id = vap_index;
    vap_info->oid = ((mtlk_hw_mmb_get_card_idx(obj->hw_api->hw) << 8) | vap_index);

    MTLK_INIT_STEP_EX(vap_handler, VAP_HANDLER_PDB_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_pdb_create, (),
                      vap_info->param_db,
                      vap_info->param_db != NULL,
                      MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP_EX(vap_handler, VAP_HANDLER_DF_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_df_create, (vap_handle),
                      vap_info->df,
                      vap_info->df != NULL,
                      MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP_EX(vap_handler, VAP_HANDLER_ABMGR_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_abmgr_create, (),
                      vap_info->abmgr,
                      vap_info->abmgr != NULL,
                      MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP_EX(vap_handler, VAP_HANDLER_CORE_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_core_api_create, (vap_handle, vap_info->df),
                      vap_info->core_api,
                      vap_info->core_api != NULL,
                      MTLK_ERR_NO_MEM);
    /* validate core VFT
      NOTE: all functions should be initialized by core, no any NULL values
        accepted. In case of unsupported functionality the function with
        empty body required!
        Validation must be in sync with mtlk_core_vft_t declaration. */
    MTLK_ASSERT(NULL != vap_info->core_api->vft->start);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->release_tx_data);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->handle_rx_data);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->handle_rx_ctrl);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->get_prop);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->set_prop);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->stop);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->prepare_stop);

    MTLK_INIT_STEP_EX(vap_handler, VAP_HANDLER_IRBD_ALLOC, MTLK_OBJ_PTR(vap_info),
                      mtlk_irbd_alloc, (),
                      vap_info->irbd,
                      vap_info->irbd != NULL,
                      MTLK_ERR_NO_MEM);
  MTLK_INIT_FINALLY(vap_handler, MTLK_OBJ_PTR(vap_info))
  MTLK_INIT_RETURN(vap_handler, MTLK_OBJ_PTR(vap_info), _mtlk_vap_cleanup, (vap_handle))
}

void __MTLK_IFUNC
mtlk_vap_delete (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_manager_t *obj = NULL;
  uint8              vap_index;

  MTLK_ASSERT(NULL != vap_handle);

  obj = mtlk_vap_get_manager(vap_handle);
  MTLK_ASSERT(NULL != obj);

  vap_index = mtlk_vap_get_id(vap_handle);
  MTLK_ASSERT(vap_index < ARRAY_SIZE(obj->guest_vap_array));

  MTLK_ASSERT(obj->guest_vap_array[vap_index].active == TRUE);

  _mtlk_vap_cleanup(vap_handle);
  __mtlk_vap_manager_vap_unset_active(obj, vap_index);
}

int __MTLK_IFUNC
mtlk_vap_start (mtlk_vap_handle_t vap_handle, mtlk_vap_manager_interface_e intf)
{
  mtlk_vap_info_internal_t *vap_info;
  mtlk_vap_manager_t       *obj       = NULL;
  uint8                     vap_index = MTLK_VAP_INVALID_IDX;

  MTLK_ASSERT(NULL != vap_handle);
  vap_info = (mtlk_vap_info_internal_t *)vap_handle;

  obj = mtlk_vap_get_manager(vap_handle);
  MTLK_ASSERT(NULL != obj);

  vap_index = mtlk_vap_get_id(vap_handle);
  MTLK_ASSERT(vap_index < ARRAY_SIZE(obj->guest_vap_array));

  MTLK_ASSERT(obj->guest_vap_array[vap_index].active == TRUE);

  __mtlk_vap_manager_vap_set_txmm(obj, vap_handle, vap_index);
  __mtlk_vap_manager_vap_set_txdm(obj, vap_handle, vap_index);

  vap_info->txmm = __mtlk_vap_manager_vap_get_txmm(obj, vap_index);
  vap_info->txdm = __mtlk_vap_manager_vap_get_txdm(obj, vap_index);

  MTLK_START_TRY(vap_handler, MTLK_OBJ_PTR(vap_info))
    MTLK_START_STEP(vap_handler, VAP_HANDLER_IRBD_INIT, MTLK_OBJ_PTR(vap_info),
                    mtlk_irbd_init, (vap_info->irbd, __mtlk_vap_manager_get_hw_irbd(vap_info->manager), mtlk_df_get_name(vap_info->df)));
    MTLK_START_STEP(vap_handler, VAP_HANDLER_CORE_START, MTLK_OBJ_PTR(vap_info),
                    mtlk_vap_get_core_vft(vap_handle)->start, (vap_handle));
    MTLK_START_STEP(vap_handler, VAP_HANDLER_DF_START, MTLK_OBJ_PTR(vap_info),
                    mtlk_df_start, (mtlk_vap_get_df(vap_handle), intf));
  MTLK_START_FINALLY(vap_handler, MTLK_OBJ_PTR(vap_info))
  MTLK_START_RETURN(vap_handler, MTLK_OBJ_PTR(vap_info), mtlk_vap_stop, (vap_handle, intf))
}

void __MTLK_IFUNC
mtlk_vap_stop (mtlk_vap_handle_t vap_handle, mtlk_vap_manager_interface_e intf)
{
  mtlk_vap_info_internal_t *vap_info;

  MTLK_ASSERT(NULL != vap_handle);
  vap_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_STOP_BEGIN(vap_handler, MTLK_OBJ_PTR(vap_info))
    MTLK_STOP_STEP(vap_handler, VAP_HANDLER_DF_START, MTLK_OBJ_PTR(vap_info),
                   mtlk_df_stop, (mtlk_vap_get_df(vap_handle), intf));
    MTLK_STOP_STEP(vap_handler, VAP_HANDLER_CORE_START, MTLK_OBJ_PTR(vap_info),
                   mtlk_vap_get_core_vft (vap_handle)->stop, (vap_handle));
    MTLK_STOP_STEP(vap_handler, VAP_HANDLER_IRBD_INIT, MTLK_OBJ_PTR(vap_info),
                   mtlk_irbd_cleanup, (vap_info->irbd));
  MTLK_STOP_END(vap_handler, MTLK_OBJ_PTR(vap_info))
}

BOOL __MTLK_IFUNC
mtlk_vap_is_master (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(_info != NULL);

  /* NOTE: don't use the mtlk_vap_get_id API here since this function
   *       can be called in context of DF/Core creation, i.e.
   *       prior to manager data member assignment.
   */
  return (_info->id  == MTLK_MASTER_VAP_ID);
}

BOOL __MTLK_IFUNC
mtlk_vap_is_ap (mtlk_vap_handle_t vap_handle)
{
  return mtlk_vap_manager_is_ap(mtlk_vap_get_manager(vap_handle));
}

BOOL __MTLK_IFUNC
mtlk_vap_is_master_ap (mtlk_vap_handle_t vap_handle)
{
  return (BOOL)(mtlk_vap_is_ap(vap_handle) && mtlk_vap_is_master(vap_handle));
}

BOOL __MTLK_IFUNC
mtlk_vap_is_slave_ap (mtlk_vap_handle_t vap_handle)
{
  return (BOOL)(mtlk_vap_is_ap(vap_handle) && !mtlk_vap_is_master(vap_handle));
}
