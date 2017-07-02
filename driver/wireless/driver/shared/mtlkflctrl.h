#ifndef __MTLK_FLOW_CTRL_H__
#define __MTLK_FLOW_CTRL_H__

#include "mtlkerr.h"
#include "mtlk_osal.h"
//#include "mtlkerr.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef struct _mtlk_flctrl_api_t
{
  mtlk_handle_t  ctx;
  void           (__MTLK_IFUNC *start_data)(mtlk_handle_t ctx);
  void           (__MTLK_IFUNC *stop_data)(mtlk_handle_t ctx);
} __MTLK_IDATA mtlk_flctrl_api_t;

typedef struct _mtlk_flctrl_t
{
  uint32               stop_requests_mask;
  uint32               available_bits;
  mtlk_flctrl_api_t    api;
  mtlk_osal_spinlock_t lock;
  MTLK_DECLARE_INIT_STATUS;
} __MTLK_IDATA mtlk_flctrl_t;

int  __MTLK_IFUNC mtlk_flctrl_init(mtlk_flctrl_t           *obj,
                                   const mtlk_flctrl_api_t *api);
void __MTLK_IFUNC mtlk_flctrl_cleanup(mtlk_flctrl_t *obj);
int  __MTLK_IFUNC mtlk_flctrl_register(mtlk_flctrl_t *obj, mtlk_handle_t *id);
void __MTLK_IFUNC mtlk_flctrl_unregister(mtlk_flctrl_t *obj, mtlk_handle_t id);
void __MTLK_IFUNC mtlk_flctrl_stop_data(mtlk_flctrl_t *obj, mtlk_handle_t id);
void __MTLK_IFUNC mtlk_flctrl_start_data(mtlk_flctrl_t *obj, mtlk_handle_t id);

static __INLINE BOOL
mtlk_flctrl_is_data_flowing (mtlk_flctrl_t *obj)
{
  return obj->stop_requests_mask?FALSE:TRUE;
}

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_FLOW_CTRL_H__*/
