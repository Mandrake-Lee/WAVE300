
#include "mtlkinc.h"
#include "mtlkflctrl.h"

#define LOG_LOCAL_GID   GID_MTLKFLCTRL
#define LOG_LOCAL_FID   1

#define MAX_NOF_AVAILABLE_BITS 32

MTLK_INIT_STEPS_LIST_BEGIN(mtlkflctrl)
  MTLK_INIT_STEPS_LIST_ENTRY(mtlkflctrl, LOCK_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(mtlkflctrl)
MTLK_INIT_STEPS_LIST_END(mtlkflctrl);

int __MTLK_IFUNC 
mtlk_flctrl_init (mtlk_flctrl_t           *obj,
                  const mtlk_flctrl_api_t *api) 
{
  ILOG4_V("mtlk_flctrl_init");
  
  MTLK_ASSERT(api        != NULL);
  MTLK_ASSERT(api->start_data != NULL);
  MTLK_ASSERT(api->stop_data  != NULL);

  MTLK_INIT_TRY(mtlkflctrl, MTLK_OBJ_PTR(obj))
    MTLK_INIT_STEP(mtlkflctrl, LOCK_INIT, MTLK_OBJ_PTR(obj), 
                   mtlk_osal_lock_init, (&obj->lock));
  
    obj->api                = *api;
    obj->stop_requests_mask = 0x00000000; /* no stop done */
    obj->available_bits     = 0xFFFFFFFF; /* all free     */

    MTLK_INIT_FINALLY(mtlkflctrl, MTLK_OBJ_PTR(obj))
  MTLK_INIT_RETURN(mtlkflctrl, MTLK_OBJ_PTR(obj), mtlk_flctrl_cleanup, (obj));  
}

/*****************************
************************************
* mtlk_flctrl_register
* 
* description - get id for the stop request
*              (use it also for the following start)
*
* return -  OK if can serve ID request
*          Error if can not allocate ID
*/
int __MTLK_IFUNC 
mtlk_flctrl_register (mtlk_flctrl_t *obj, mtlk_handle_t *id) 
{
  int    res  = MTLK_ERR_UNKNOWN;
  int    i    = 0;
  uint32 mask = 0x00000001;

  *id = HANDLE_T(0);

  ILOG4_V("mtlk_flctrl_register");
  mtlk_osal_lock_acquire(&obj->lock);
  for (; i < MAX_NOF_AVAILABLE_BITS; i++) {
    if (obj->available_bits & mask) {
      obj->available_bits &= ~mask;
      *id = HANDLE_T(mask);
      res = MTLK_ERR_OK;
      break;
    }
    mask = mask << 1;
  }
  ILOG1_D("mtlk_flctrl_register id = 0x%lx",*id);
  mtlk_osal_lock_release(&obj->lock);

  return res;
}

/*****************************************************************
* mtlk_flctrl_stop
* 
* description - request stop. Use the ID as an identification.
*              The use of the ID ensures proper data start.
*
* return -  OK if stop succeeded
*          Error if stop failed
*/
void __MTLK_IFUNC 
mtlk_flctrl_stop_data (mtlk_flctrl_t *obj, mtlk_handle_t id)
{
  ILOG4_D("mtlk_flctrl_stop with id = 0x%lx",id);
  mtlk_osal_lock_acquire(&obj->lock);
  if (!obj->stop_requests_mask) {
    obj->api.stop_data(obj->api.ctx);
    ILOG1_D("mtlk_flctrl_stop, data stopped with id = 0x%lx",id);
  }
  obj->stop_requests_mask |= (uint32)id;
  ILOG1_D("mask is 0x%x", obj->stop_requests_mask);
  mtlk_osal_lock_release(&obj->lock);
}

/*****************************************************************
* mtlk_flctrl_start
* 
* description - request start. Use the ID as an identification.
*              It clear the stop bit by the use of the ID.
*              If no other stop requests, can start data
*
* return -  OK if start data
*          Error if not
* Note - it is not real error, but indicate that no data !!?? to change?
*/
void __MTLK_IFUNC 
mtlk_flctrl_start_data (mtlk_flctrl_t *obj, mtlk_handle_t id)
{
  ILOG4_D("mtlk_flctrl_start with id = 0x%lx",id);
  mtlk_osal_lock_acquire(&obj->lock);
  obj->stop_requests_mask &= ~((uint32)id);
  if (!obj->stop_requests_mask) {
    obj->api.start_data(obj->api.ctx);
    ILOG1_D("mtlk_flctrl_start, data started with id = 0x%lx", id);
  }
  mtlk_osal_lock_release(&obj->lock);
}

/*****************************************************************
* mtlk_flctrl_unregister
* 
* description - return the ID to the resource, can't use it any more
*
* return -  OK
*/
void __MTLK_IFUNC 
mtlk_flctrl_unregister(mtlk_flctrl_t *obj, mtlk_handle_t id) 
{
  ILOG1_D("mtlk_flctrl_unregister with id = 0x%lx",id);
  mtlk_osal_lock_acquire(&obj->lock);
  obj->available_bits |= (uint32)id;
  mtlk_osal_lock_release(&obj->lock);
}

void __MTLK_IFUNC 
mtlk_flctrl_cleanup(mtlk_flctrl_t *obj)
{
  ILOG4_V("mtlk_flctrl_cleanup");
  
  MTLK_CLEANUP_BEGIN(mtlkflctrl, MTLK_OBJ_PTR(obj))
    MTLK_CLEANUP_STEP(mtlkflctrl, LOCK_INIT, MTLK_OBJ_PTR(obj),
                      mtlk_osal_lock_cleanup, (&(obj->lock)));
  MTLK_CLEANUP_END(mtlkflctrl, MTLK_OBJ_PTR(obj));  
}

