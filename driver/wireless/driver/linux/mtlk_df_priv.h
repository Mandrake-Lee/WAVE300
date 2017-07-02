/*
 * $Id: mtlk_df_priv.h 11559 2011-08-24 08:19:12Z fleytman $
 *
 * Copyright (c) 2006-2011 Metalink Broadband (Israel)
 *
 * Written by: Grygorii Strashko
 *
 * Private DF definitions
 *
 */


#ifndef __MTLK_DF_PRIV_H__
#define __MTLK_DF_PRIV_H__

mtlk_df_proc_fs_node_t* mtlk_dfg_get_drv_proc_node(void);

/*! \brief   Returns TRUE if the DF belongs to a Slave Core.

    \param   df          DF object
*/
BOOL
mtlk_df_is_slave(mtlk_df_t *df);


/*! \brief   Returns TRUE if the DF belongs to a AP.

    \param   df          DF object
*/
BOOL
mtlk_df_is_ap(mtlk_df_t *df);

/*! \fn      mtlk_df_user_t* mtlk_df_get_user(mtlk_df_t *df)

    \brief   Returns pointer to DF USER object.

    \param   df          DF object
*/
mtlk_df_user_t*
mtlk_df_get_user(mtlk_df_t *df);

int
_mtlk_df_user_process_core_retval_void(int processing_result,
                                       mtlk_clpb_t* execution_result,
                                       mtlk_core_tx_req_id_t core_req,
                                       BOOL delete_clipboard_on_success);

int
_mtlk_df_user_process_core_retval(int processing_result,
                                  mtlk_clpb_t* execution_result,
                                  mtlk_core_tx_req_id_t core_req,
                                  BOOL delete_clipboard_on_success);

int
_mtlk_df_user_invoke_core(mtlk_df_t* df,
                          mtlk_core_tx_req_id_t req_id,
                          mtlk_clpb_t **ppclpb,
                          const void* data,
                          size_t data_size);

typedef void __MTLK_IFUNC (*_invoke_async_clb_t)(mtlk_handle_t user_context,
                                                 int           processing_result,
                                                 mtlk_clpb_t  *pclpb);
void
_mtlk_df_user_invoke_core_async(mtlk_df_t* df,
                                mtlk_core_tx_req_id_t req_id,
                                const void* data,
                                size_t data_size,
                                _invoke_async_clb_t clb,
                                mtlk_handle_t ctx);

int
_mtlk_df_user_pull_core_data(mtlk_df_t* df,
                             mtlk_core_tx_req_id_t core_req,
                             BOOL is_void_request,
                             void **out_data,
                             uint32* out_data_length,
                             mtlk_handle_t *hdata);

void
_mtlk_df_user_free_core_data(mtlk_df_t* df,
                             mtlk_handle_t data_handle);

int
_mtlk_df_user_push_core_data(mtlk_df_t* df,
                             mtlk_core_tx_req_id_t core_req,
                             BOOL is_void_request,
                             void *in_data,
                             uint32 in_data_length);

void*
_mtlk_df_user_alloc_core_data(mtlk_df_t* df,
                              uint32 data_length);

#endif /* __MTLK_DF_PRIV_H__ */
