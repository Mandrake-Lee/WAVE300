#ifndef __MTLK_VAP_MANAGER_H__
#define __MTLK_VAP_MANAGER_H__

#include "mtlkdfdefs.h"
#include "mtlkirbd.h"
#include "mtlk_wss.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define LOG_LOCAL_GID   GID_VAPM
#define LOG_LOCAL_FID   1

#define MTLK_MASTER_VAP_ID  0

struct _mtlk_hw_vft_t;
typedef struct _mtlk_hw_api_t mtlk_hw_api_t;

typedef struct _mtlk_bus_drv_t mtlk_bus_drv_t;

typedef struct _mtlk_vap_handle_t *mtlk_vap_handle_t;
typedef struct _mtlk_vap_manager_t mtlk_vap_manager_t;

#define MTLK_INVALID_VAP_HANDLE ((mtlk_vap_handle_t)0)

typedef enum {
  MTLK_VAP_MASTER_INTERFACE,
  MTLK_VAP_SLAVE_INTERFACE
} mtlk_vap_manager_interface_e;

/* VAP Manager API */
mtlk_vap_manager_t * __MTLK_IFUNC mtlk_vap_manager_create(mtlk_bus_drv_t *bus_drv, BOOL  is_ap);
void                 __MTLK_IFUNC mtlk_vap_manager_delete(mtlk_vap_manager_t* obj);

int                  __MTLK_IFUNC mtlk_vap_manager_create_vap(mtlk_vap_manager_t *obj,
                                                              mtlk_vap_handle_t  *vap_handle,
                                                              uint8 vap_index);
void                 __MTLK_IFUNC mtlk_vap_manager_delete_all_vaps(mtlk_vap_manager_t *obj);

void                 __MTLK_IFUNC mtlk_vap_manager_prepare_start(mtlk_vap_manager_t *obj,
                                                                 mtlk_handle_t      txmm_handle,
                                                                 mtlk_handle_t      txdm_handle);
void                 __MTLK_IFUNC mtlk_vap_manager_prepare_stop(mtlk_vap_manager_t *obj);
void                 __MTLK_IFUNC mtlk_vap_manager_stop_all_vaps(mtlk_vap_manager_t          *obj, 
                                                                 mtlk_vap_manager_interface_e intf);
int                  __MTLK_IFUNC mtlk_vap_manager_get_master_vap(mtlk_vap_manager_t *obj,
                                                                  mtlk_vap_handle_t  *vap_handle);
int                  __MTLK_IFUNC mtlk_vap_manager_get_vap_handle(mtlk_vap_manager_t *obj,
                                                                  uint8               vap_id,
                                                                  mtlk_vap_handle_t  *vap_handle);
mtlk_bus_drv_t*      __MTLK_IFUNC mtlk_vap_manager_get_bus_drv(mtlk_vap_manager_t *obj);
mtlk_hw_t*           __MTLK_IFUNC mtlk_vap_manager_get_hw(mtlk_vap_manager_t *obj);
mtlk_wss_t *         __MTLK_IFUNC mtlk_vap_manager_get_hw_wss(mtlk_vap_manager_t *obj);
void                 __MTLK_IFUNC mtlk_vap_manager_set_hw_api(mtlk_vap_manager_t *obj, mtlk_hw_api_t *hw_api);
mtlk_hw_api_t*       __MTLK_IFUNC mtlk_vap_manager_get_hw_api(mtlk_vap_manager_t *obj);
BOOL                 __MTLK_IFUNC mtlk_vap_manager_is_ap(mtlk_vap_manager_t *obj);
void __MTLK_IFUNC
mtlk_vap_manager_notify_vap_activated(mtlk_vap_manager_t *obj);
void __MTLK_IFUNC
mtlk_vap_manager_notify_vap_deactivated(mtlk_vap_manager_t *obj);
uint32 __MTLK_IFUNC
mtlk_vap_manager_get_active_vaps_number(mtlk_vap_manager_t *obj);

struct _mtlk_abmgr_t;
struct _mtlk_core_api_t;
struct _mtlk_core_vft_t;

/* VAP API */
typedef struct _mtlk_vap_info_internal_t
{
  uint8                   id;
  uint16                  oid;
  mtlk_vap_manager_t      *manager;
  struct _mtlk_core_api_t *core_api;
  mtlk_df_t               *df;
  mtlk_pdb_t              *param_db;
  mtlk_txmm_t             *txmm;
  mtlk_txmm_t             *txdm;
  struct _mtlk_abmgr_t    *abmgr;
  mtlk_irbd_t             *irbd;

  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
} __MTLK_IDATA mtlk_vap_info_internal_t;

struct _mtlk_vap_handle_t __MTLK_IDATA
{
  uint8 internal[sizeof(mtlk_vap_info_internal_t)];
};

static __INLINE uint16
mtlk_vap_get_oid(mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);

  return _info->oid;
}

static __INLINE mtlk_vap_manager_t *
mtlk_vap_get_manager (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);

  return _info->manager;
}

static __INLINE mtlk_df_t *
mtlk_vap_get_df (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->df);

  return _info->df;
}

mtlk_core_t * __MTLK_IFUNC
mtlk_vap_get_core (mtlk_vap_handle_t vap_handle);

struct _mtlk_core_vft_t const * __MTLK_IFUNC
mtlk_vap_get_core_vft (mtlk_vap_handle_t vap_handle);

static __INLINE struct _mtlk_abmgr_t*
mtlk_vap_get_abmgr(mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->abmgr);

  return _info->abmgr;
}

static __INLINE mtlk_pdb_t *
mtlk_vap_get_param_db (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->param_db);

  return _info->param_db;
}

static __INLINE mtlk_txmm_t *
mtlk_vap_get_txmm (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->txmm);

  return _info->txmm;
}

static __INLINE mtlk_txmm_t *
mtlk_vap_get_txdm (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->txdm);

  return _info->txdm;
}

static __INLINE uint8
mtlk_vap_get_id (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);

  return _info->id;
}

static __INLINE mtlk_irbd_t *
mtlk_vap_get_irbd (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->irbd);

  return _info->irbd;
}

struct _mtlk_hw_vft_t const * __MTLK_IFUNC
mtlk_vap_get_hw_vft(mtlk_vap_handle_t vap_handle);

void                 __MTLK_IFUNC mtlk_vap_delete(mtlk_vap_handle_t vap_handle);


int                  __MTLK_IFUNC mtlk_vap_start(mtlk_vap_handle_t            vap_handle, 
                                                 mtlk_vap_manager_interface_e intf);
void                 __MTLK_IFUNC mtlk_vap_stop(mtlk_vap_handle_t            vap_handle, 
                                                mtlk_vap_manager_interface_e intf);

BOOL                 __MTLK_IFUNC mtlk_vap_is_master(mtlk_vap_handle_t vap_handle);

BOOL                 __MTLK_IFUNC mtlk_vap_is_ap(mtlk_vap_handle_t vap_handle);

BOOL                 __MTLK_IFUNC mtlk_vap_is_master_ap(mtlk_vap_handle_t vap_handle);

BOOL                 __MTLK_IFUNC mtlk_vap_is_slave_ap(mtlk_vap_handle_t vap_handle);

/* Auxiliary VAP Manager API wrappers */
static __INLINE mtlk_df_t*
mtlk_vap_manager_get_master_df(mtlk_vap_manager_t *obj)
{
  mtlk_vap_handle_t master_vap_handle;
  int               ires;

  ires = mtlk_vap_manager_get_master_vap(obj, &master_vap_handle);
  MTLK_ASSERT(MTLK_ERR_OK == ires);
  MTLK_UNREFERENCED_PARAM(ires);

  return mtlk_vap_get_df(master_vap_handle);
}

static __INLINE mtlk_core_t*
mtlk_vap_manager_get_master_core(mtlk_vap_manager_t *obj)
{
  mtlk_vap_handle_t master_vap_handle;
  mtlk_core_t*      core;
  int               ires;

  ires = mtlk_vap_manager_get_master_vap(obj, &master_vap_handle);
  MTLK_ASSERT(MTLK_ERR_OK == ires);
  MTLK_UNREFERENCED_PARAM(ires);

  core = mtlk_vap_get_core(master_vap_handle);
  MTLK_ASSERT(NULL != core);
  return core;
}

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_VAP_MANAGER_H__ */
