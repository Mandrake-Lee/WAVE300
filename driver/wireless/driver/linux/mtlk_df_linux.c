/*
 * $Id: mtlk_df_linux.c 11999 2011-11-23 14:54:33Z nayshtut $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Driver framework implementation for Linux
 *
 */

#include "mtlkinc.h"


#include "mtlk_fast_mem.h"
#include "mtlklist.h"
#include "mtlk_df.h"
#include "mtlk_df_user_priv.h"
#include "mtlk_coreui.h"


#define LOG_LOCAL_GID   GID_DFLINUX
#define LOG_LOCAL_FID   1

struct _mtlk_df_t {
  mtlk_dlist_entry_t   dfg_list_entry;
  mtlk_df_user_t      *df_user;
  mtlk_vap_handle_t    vap_handle;

  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
};

extern void _mtlk_dfg_register_df(mtlk_ldlist_entry_t *entry);

extern void _mtlk_dfg_unregister_df(mtlk_ldlist_entry_t *entry);

/*
 * Access functions
 */
mtlk_core_t*
mtlk_df_get_core(mtlk_df_t *df)
{
  MTLK_ASSERT(NULL != df);

  return mtlk_vap_get_core(df->vap_handle);
}

BOOL
mtlk_df_is_slave (mtlk_df_t *df)
{
  MTLK_ASSERT(NULL != df);

  return !mtlk_vap_is_master(df->vap_handle); 
}

BOOL
mtlk_df_is_ap(mtlk_df_t *df)
{
  MTLK_ASSERT(NULL != df);

  return mtlk_vap_is_ap(df->vap_handle);
}

mtlk_vap_manager_t*
mtlk_df_get_vap_manager(const mtlk_df_t *df)
{
  MTLK_ASSERT(NULL != df);
  return mtlk_vap_get_manager(df->vap_handle);
}

mtlk_vap_handle_t
mtlk_df_get_vap_handle(const mtlk_df_t *df)
{
  MTLK_ASSERT(NULL != df);
  return(df->vap_handle);
}

const char *
mtlk_df_get_name (mtlk_df_t *df)
{
  MTLK_ASSERT(NULL != df);
  MTLK_ASSERT(NULL != df->df_user);

  return mtlk_df_user_get_name(df->df_user);

}

/*****************************************************************************
 * Interface functions
 *****************************************************************************/
MTLK_START_STEPS_LIST_BEGIN(df)
  MTLK_START_STEPS_LIST_ENTRY(df, USER)
MTLK_START_INNER_STEPS_BEGIN(df)
MTLK_START_STEPS_LIST_END(df);

void mtlk_df_stop(mtlk_df_t *df, mtlk_vap_manager_interface_e reason)
{
  MTLK_STOP_BEGIN(df, MTLK_OBJ_PTR(df))
    MTLK_STOP_STEP(df, USER, MTLK_OBJ_PTR(df),
                   mtlk_df_user_stop, (df->df_user, reason));
  MTLK_STOP_END(df, MTLK_OBJ_PTR(df));
}

int mtlk_df_start(mtlk_df_t *df, mtlk_vap_manager_interface_e reason)
{
  MTLK_START_TRY(df, MTLK_OBJ_PTR(df))
    MTLK_START_STEP(df, USER, MTLK_OBJ_PTR(df),
                    mtlk_df_user_start, (df, df->df_user, reason));
  MTLK_START_FINALLY(df, MTLK_OBJ_PTR(df))
  MTLK_START_RETURN(df, MTLK_OBJ_PTR(df), mtlk_df_stop, (df, reason))
}

MTLK_INIT_STEPS_LIST_BEGIN(df)
  MTLK_INIT_STEPS_LIST_ENTRY(df, USER)
  MTLK_INIT_STEPS_LIST_ENTRY(df, REGISTER)
MTLK_INIT_INNER_STEPS_BEGIN(df)
MTLK_INIT_STEPS_LIST_END(df);

static void _mtlk_df_cleanup(mtlk_df_t *df)
{
  MTLK_CLEANUP_BEGIN(df, MTLK_OBJ_PTR(df))
    MTLK_CLEANUP_STEP(df, REGISTER, MTLK_OBJ_PTR(df),
                      _mtlk_dfg_unregister_df, (&df->dfg_list_entry));
    MTLK_CLEANUP_STEP(df, USER, MTLK_OBJ_PTR(df),
                      mtlk_df_user_delete, (df->df_user));
  MTLK_CLEANUP_END(df, MTLK_OBJ_PTR(df));
}

static int _mtlk_df_init(mtlk_df_t *df, mtlk_vap_handle_t vap_handle)
{
  MTLK_INIT_TRY(df, MTLK_OBJ_PTR(df))
    df->vap_handle  = vap_handle;
    MTLK_INIT_STEP_EX(df, USER, MTLK_OBJ_PTR(df),
                      mtlk_df_user_create, (df), df->df_user,
                      NULL != df->df_user, MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP_VOID(df, REGISTER, MTLK_OBJ_PTR(df),
                   _mtlk_dfg_register_df, (&df->dfg_list_entry));
  MTLK_INIT_FINALLY(df, MTLK_OBJ_PTR(df))
  MTLK_INIT_RETURN(df, MTLK_OBJ_PTR(df), _mtlk_df_cleanup, (df))
}

mtlk_df_t* mtlk_df_create(mtlk_vap_handle_t vap_handle)
{
  mtlk_df_t* df = mtlk_fast_mem_alloc(MTLK_FM_USER_DF, sizeof(mtlk_df_t));

  MTLK_ASSERT(sizeof(mtlk_nbuf_priv_t) <= MTLK_FIELD_SIZEOF(struct sk_buff, cb));

  if(NULL == df)
    return NULL;

  memset(df, 0, sizeof(mtlk_df_t));

  if(MTLK_ERR_OK != _mtlk_df_init(df, vap_handle))
  {
    mtlk_fast_mem_free(df);
    return NULL;
  }

  return df;
}

void mtlk_df_delete(mtlk_df_t *df)
{
  _mtlk_df_cleanup(df);
  mtlk_fast_mem_free(df);
}

mtlk_df_user_t* mtlk_df_get_user(mtlk_df_t *df)
{
  MTLK_ASSERT(NULL != df);
  return df->df_user;
}

#ifdef MTCFG_TSF_TIMER_ACCESS_ENABLED
mtlk_df_t*
mtlk_df_get_df_by_dfg_entry(mtlk_dlist_entry_t *df_entry)
{
  return (mtlk_df_t*)MTLK_CONTAINER_OF(df_entry, mtlk_df_t, dfg_list_entry);
}
#endif /* MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS */


/********************************************************************
 * DF to Core auxiliary interface implementation
 * - allows to pass requests to the Core synchronously and asynchronously
 *   through Core UI (mtlk_core_handle_tx_ctrl)
 ********************************************************************/
typedef void __MTLK_IFUNC (*_req_notify_clb_t)(mtlk_handle_t user_context);

struct _mtlk_user_request_t
{
  int result;
  BOOL is_waitable;

  union
  {
    mtlk_osal_event_t completion_event;
    struct
    {
      _req_notify_clb_t clb;
      mtlk_handle_t                ctx;
    } notification_clb;
  } u;
};

typedef void __MTLK_IFUNC (*_invoke_async_clb_t)(mtlk_handle_t user_context,
                                                 int           processing_result,
                                                 mtlk_clpb_t  *pclpb);

typedef struct __async_invoke_ctx_t
{
  mtlk_user_request_t   req;
  mtlk_clpb_t          *pclpb;
  _invoke_async_clb_t   orig_clb;
  mtlk_handle_t         orig_ctx;
} _async_invoke_ctx_t;

#ifdef MTCFG_ENABLE_OBJPOOL

void __MTLK_IFUNC
mtlk_df_user_request_dump(mtlk_seq_entry_t *s, mtlk_user_request_t *user_req)
{
  mtlk_aux_seq_printf(s, "\n*** mtlk_user_request_t DUMP ******\n");
  mtlk_aux_seq_printf(s, "is_waitable %u\n", user_req->is_waitable);
  mtlk_aux_seq_printf(s, "result %u\n", user_req->result);
}

#endif /*MTCFG_ENABLE_OBJPOOL*/

void __MTLK_IFUNC
mtlk_df_ui_req_complete(mtlk_user_request_t *req, int result)
{
  MTLK_ASSERT(NULL != req);

  req->result = result;

  if(req->is_waitable)
  {
    mtlk_osal_event_set(&req->u.completion_event);
  }
  else
  {
    req->u.notification_clb.clb(req->u.notification_clb.ctx);
  }
}

static __INLINE int
_mtlk_df_ui_waitable_req_init(mtlk_user_request_t *req)
{
  MTLK_ASSERT(NULL != req);

  req->result = MTLK_ERR_OK;
  req->is_waitable = TRUE;

  return mtlk_osal_event_init(&req->u.completion_event);
}

static __INLINE int
_mtlk_df_ui_notifying_req_init(mtlk_user_request_t *req,
                               _req_notify_clb_t clb,
                               mtlk_handle_t clb_ctx)
{
  MTLK_ASSERT(NULL != req);
  MTLK_ASSERT(NULL != clb);

  req->result = MTLK_ERR_OK;
  req->is_waitable = FALSE;

  req->u.notification_clb.clb = clb;
  req->u.notification_clb.ctx = clb_ctx;

  return MTLK_ERR_OK;
}

static __INLINE void
_mtlk_df_ui_req_cleanup(mtlk_user_request_t *req)
{
  MTLK_ASSERT(NULL != req);

  if(req->is_waitable)
  {
    mtlk_osal_event_cleanup(&req->u.completion_event);
  }
}

static __INLINE int
_mtlk_df_ui_req_get_result(mtlk_user_request_t *req)
{
  MTLK_ASSERT(NULL != req);

  return req->result;
}

static __INLINE int
_mtlk_df_ui_req_wait_completion(mtlk_user_request_t *req)
{
  MTLK_ASSERT(NULL != req);
  MTLK_ASSERT(req->is_waitable);

  return mtlk_osal_event_wait(&req->u.completion_event,
                               MTLK_OSAL_EVENT_INFINITE);
}

static void _core_async_invoke_clb(mtlk_handle_t hctx)
{
  _async_invoke_ctx_t* ctx = HANDLE_T_PTR(_async_invoke_ctx_t, hctx);

  ctx->orig_clb(ctx->orig_ctx,
                _mtlk_df_ui_req_get_result(&ctx->req),
                ctx->pclpb);

  mtlk_clpb_delete(ctx->pclpb);
  _mtlk_df_ui_req_cleanup(&ctx->req);
  mtlk_osal_mem_free(ctx);
}

void
_mtlk_df_user_invoke_core_async(mtlk_df_t* df,
                                mtlk_core_tx_req_id_t req_id,
                                const void* data,
                                size_t data_size,
                                _invoke_async_clb_t clb,
                                mtlk_handle_t ctx)
{
  _async_invoke_ctx_t* invocation_context;
  int res = MTLK_ERR_UNKNOWN;

  invocation_context = mtlk_osal_mem_alloc(sizeof(*invocation_context),
    MTLK_MEM_TAG_CORE_INVOKE);
  if(NULL == invocation_context)
  {
    ELOG_V("Cannot allocate invocation context");
    res = MTLK_ERR_NO_MEM;
    goto err_ctx_alloc;
  }

  invocation_context->orig_clb = clb;
  invocation_context->orig_ctx = ctx;

  invocation_context->pclpb = mtlk_clpb_create();
  if (NULL == invocation_context->pclpb) {
    ELOG_V("Cannot allocate clipboard object");
    res = MTLK_ERR_NO_MEM;
    goto err_clpb_create;
  }

  if(0 != data_size)
  {
    res = mtlk_clpb_push(invocation_context->pclpb, data, data_size);
    if(MTLK_ERR_OK != res) {
      ELOG_V("Cannot push data to the clipboard");
      goto err_push_data;
    }
  }

  res = _mtlk_df_ui_notifying_req_init(&invocation_context->req,
                                       _core_async_invoke_clb,
                                       HANDLE_T(invocation_context));
  if(MTLK_ERR_OK != res) {
    ELOG_V("Cannot initialize request object");
    goto err_init_req;
  }

  mtlk_core_handle_tx_ctrl(mtlk_df_get_core(df), &invocation_context->req,
                           req_id, invocation_context->pclpb);

  return;

err_init_req:
err_push_data:
  mtlk_clpb_delete(invocation_context->pclpb);
err_clpb_create:
  mtlk_osal_mem_free(invocation_context);
err_ctx_alloc:
  if(MTLK_ERR_OK != res)
  {
    clb(ctx, res, NULL);
  }
}

int
_mtlk_df_user_invoke_core(mtlk_df_t* df,
                          mtlk_core_tx_req_id_t req_id,
                          mtlk_clpb_t **ppclpb,
                          const void* data,
                          size_t data_size)
{
  mtlk_user_request_t req;
  int res = MTLK_ERR_UNKNOWN;

  *ppclpb = mtlk_clpb_create();
  if (NULL == *ppclpb) {
    ELOG_V("Cannot allocate clipboard object");
    res = MTLK_ERR_NO_MEM;
    goto err_clpb_create;
  }

  if(0 != data_size)
  {
    res = mtlk_clpb_push(*ppclpb, data, data_size);
    if(MTLK_ERR_OK != res) {
      ELOG_V("Cannot push data to the clipboard");
      goto err_push_data;
    }
  }

  res = _mtlk_df_ui_waitable_req_init(&req);
  if(MTLK_ERR_OK != res) {
    ELOG_V("Cannot initialize request object");
    goto err_init_req;
  }

  mtlk_core_handle_tx_ctrl(mtlk_df_get_core(df), &req, req_id, *ppclpb);

  res = _mtlk_df_ui_req_wait_completion(&req);
  if (MTLK_ERR_OK != res) {
    ELOG_V("Wait for request completion failed");
    goto err_wait;
  }

  res = _mtlk_df_ui_req_get_result(&req);
  if(MTLK_ERR_OK != res) {
    goto err_processing;
  }

  _mtlk_df_ui_req_cleanup(&req);

  return MTLK_ERR_OK;

err_processing:
err_wait:
  _mtlk_df_ui_req_cleanup(&req);
err_init_req:
err_push_data:
  mtlk_clpb_delete(*ppclpb);
  *ppclpb = NULL;
err_clpb_create:
  return res;
}

int
_mtlk_df_user_process_core_retval(int processing_result,
                                  mtlk_clpb_t* execution_result,
                                  mtlk_core_tx_req_id_t core_req,
                                  BOOL delete_clipboard_on_success)
{
  int res;

  if(MTLK_ERR_OK == processing_result)
  {
    uint32 size;
    void* data = mtlk_clpb_enum_get_next(execution_result, &size);
    MTLK_ASSERT(NULL != data);
    MTLK_ASSERT(sizeof(int) == size);
    res = *(int*) data;

    if((MTLK_ERR_OK != res) || delete_clipboard_on_success)
    {
      mtlk_clpb_delete(execution_result);
    }

    if(MTLK_ERR_OK != res)
      ILOG1_DD("Core request 0x%X failed with error #%d", core_req, processing_result);
  }
  else
  {
    ILOG1_DD("Core request 0x%X processing failed with error #%d", core_req, processing_result);
    res = processing_result;
  }

  return res;
}

int
_mtlk_df_user_process_core_retval_void(int processing_result,
                                       mtlk_clpb_t* execution_result,
                                       mtlk_core_tx_req_id_t core_req,
                                       BOOL delete_clipboard_on_success)
{
  if(MTLK_ERR_OK == processing_result)
  {
    if(delete_clipboard_on_success)
    {
      mtlk_clpb_delete(execution_result);
    }
  }
  else
  {
    ILOG1_DD("Core request 0x%X processing failed with error #%d", core_req, processing_result);
  }

  return processing_result;
}

int
_mtlk_df_user_pull_core_data(mtlk_df_t* df,
                             mtlk_core_tx_req_id_t core_req,
                             BOOL is_void_request,
                             void **out_data,
                             uint32* out_data_length,
                             mtlk_handle_t *hdata)
{
  mtlk_clpb_t* clpb;
  int res;

  MTLK_ASSERT(NULL != out_data);

  res = _mtlk_df_user_invoke_core(df, core_req, &clpb, NULL, 0);
  res = is_void_request ? _mtlk_df_user_process_core_retval_void(res, clpb, core_req, FALSE):
                          _mtlk_df_user_process_core_retval(res, clpb, core_req, FALSE);

  if(MTLK_ERR_OK == res) {
    *out_data = mtlk_clpb_enum_get_next(clpb, out_data_length);
    MTLK_ASSERT(NULL != *out_data);
    *hdata = HANDLE_T(clpb);
  }

  return res;
}

void _mtlk_df_user_free_core_data(mtlk_df_t* df,
                                  mtlk_handle_t data_handle)
{
  MTLK_ASSERT(MTLK_INVALID_HANDLE != data_handle);
  MTLK_UNREFERENCED_PARAM(df);
  mtlk_clpb_delete(HANDLE_T_PTR(mtlk_clpb_t, data_handle));
}

void*
_mtlk_df_user_alloc_core_data(mtlk_df_t* df,
                              uint32 data_length)
{
  void *data;
  MTLK_UNREFERENCED_PARAM(df);

  data = mtlk_osal_mem_alloc(data_length, MTLK_MEM_TAG_CORE_CFG);
  if(NULL != data)
    memset(data, 0, data_length);

  return data;
}

int
_mtlk_df_user_push_core_data(mtlk_df_t* df,
                             mtlk_core_tx_req_id_t core_req,
                             BOOL is_void_request,
                             void *in_data,
                             uint32 in_data_length)
{
  mtlk_clpb_t* clpb;
  int res;

  MTLK_ASSERT(NULL != in_data);

  res = _mtlk_df_user_invoke_core(df, core_req, &clpb, in_data, in_data_length);

  mtlk_osal_mem_free(in_data);

  res = is_void_request ? _mtlk_df_user_process_core_retval_void(res, clpb, core_req, TRUE):
                          _mtlk_df_user_process_core_retval(res, clpb, core_req, TRUE);

  return res;
}

