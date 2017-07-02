#ifndef __MTLK_WSSA_H__
#define __MTLK_WSSA_H__

#include "mhi_statistics.h"

#define LOG_LOCAL_GID   GID_WSS_APP
#define LOG_LOCAL_FID   0

/*! \file  mtlkwssa.h
    \brief WSS user mode API for client applications
*/

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/* ! \brief   Data providers (MTIDL_PROVIDER_*)
*/
typedef uint32 mtlk_wss_provider_level_t;

/* ! \brief   Data sources (MTIDL_SOURCE_*)
*/
typedef uint32 mtlk_wss_data_source_t;

/* ! \brief   List of providers. Buffer with provider IDs immediately follows providers_number variable;
*/
typedef struct _mtlk_wss_provider_list_t
{
  uint32 providers_number;     /* < number of providers IDs in the array */
  char   provider_id_array[1]; /* < First byte of provider IDs array     */
} mtlk_wss_provider_list_t;


/*! \brief  Requests information/statistics element

    \param  interface_name - name of interface used to fetch the data
    \param  level - data provider level
    \param  info_source - data source
    \param  provider_id - name of entity that provides the data, i.e. peer MAC address
    \param  info_id - identifier of the item as defined by MTIDL
    \param  data_buff - pointer to output data buffer
    \param  buff_length - passed/returned length of data in buffer

    \return MTLK_ERR_... error code
*/
int __MTLK_IFUNC
mtlk_wss_request_info(const char* interface_name, mtlk_wss_provider_level_t level,
                      mtlk_wss_data_source_t info_source, const void* provider_id, uint32 info_id,
                      void* data_buff, uint32 buff_length);

/*! \brief  Callback function for WSS events

    \param  info_source - data source
    \param  info_id - identifier of the item as defined by MTIDL
    \param  data_buff - pointer to data buffer received
    \param  buff_length - length of data in buffer
    \param  ctx - caller context passed on callback registration

    \return MTLK_ERR_... error code
*/
typedef void (__MTLK_IFUNC* mtlk_wss_event_receiver_f)(mtlk_wss_data_source_t info_source,
                                                       uint32 info_id,
                                                       void* data_buff,
                                                       uint32 buff_length,
                                                       mtlk_handle_t ctx);

/*! \brief   "Provider disappears" event handler function prototype.

    This function is a \b mandatory parameter for the mtlk_wss_register_events_callback API
    The "Provider disappears" event handler will be called once the provider will disappear
    in the driver.

    \param  ctx - caller context passed on callback registration
*/
typedef void (__MTLK_IFUNC* mtlk_wss_rm_handler_f)(mtlk_handle_t ctx);

/*! \brief  Register callback for WSS events

    \param  interface_name - name of interface used to fetch the data
    \param  level - data provider level
    \param  provider_id - name of entity that provides the data, i.e. peer MAC address
    \param  receiver_func_ptr - pointer to callback function
    \param  ctx - caller context to be passed into callback function
    \param  unregister_handle - opaque handle to be passed into mtlk_wss_unregister_events_callback
    \param  rm_func_ptr - pointer to provider removal function
    \param  rm_ctx - caller context to be passed into provider removal function

    \return MTLK_ERR... status code
*/
int __MTLK_IFUNC
mtlk_wss_register_events_callback(const char* interface_name,
                                  mtlk_wss_provider_level_t level,
                                  const void* provider_id,
                                  mtlk_wss_event_receiver_f receiver_func_ptr,
                                  mtlk_handle_t ctx,
                                  mtlk_handle_t *unregister_handle,
                                  mtlk_wss_rm_handler_f rm_func_ptr,
                                  mtlk_handle_t rm_ctx);

/*! \brief  Unregister callback for WSS events

    \param  register_handle - handle received from mtlk_wss_register_events_callback
*/
void __MTLK_IFUNC
mtlk_wss_unregister_events_callback(mtlk_handle_t register_handle);

/*! \brief  Lists WSS data providers on given provider level

    \param  interface_name - name of interface used to fetch the data
    \param  level - data provider level
    \param  res - pointer to store error code

    \return NULL in case of error or list of providers found
*/
mtlk_wss_provider_list_t* __MTLK_IFUNC
mtlk_wss_get_providers_list(const char* interface_name, mtlk_wss_provider_level_t level, int *res);

/*! \brief  Free memory used by providers list returned by mtlk_wss_get_providers_list() API

    \param  providers_list - list to be freed
*/
void __MTLK_IFUNC
mtlk_wss_free_providers_list(mtlk_wss_provider_list_t* providers_list);

/*! \brief  Initialize WSS subsystem. Must be called before any other WSS API call

    \param  rm_func_ptr - pointer to driver removal callback
    \param  rm_ctx - caller context to be passed into driver removal callback
    \param  wss_cleanup_context - output parameter that holds context
            value required by subsequent mtlk_wss_cleanup() call

    \return MTLK_ERR_... error code or MTLK_ERR_OK
*/
int __MTLK_IFUNC
mtlk_wssa_api_init(mtlk_wss_rm_handler_f rm_func_ptr,
                   mtlk_handle_t rm_ctx,
                   mtlk_handle_t* wss_cleanup_context);

/*! \brief  Clean up WSS subsystem. Must be called before application shutdown

    \param  wss_cleanup_context - context value received from mtlk_wss_init()
*/
void __MTLK_IFUNC
mtlk_wssa_api_cleanup(mtlk_handle_t wss_cleanup_context);

static __INLINE uint32
mtlk_wssa_access_long_value(uint32 raw_value, mtlk_wss_data_source_t src)
{
  return (MTIDL_SRC_FW == src) ? MAC_TO_HOST32(raw_value) : raw_value;
}

static __INLINE uint64
mtlk_wssa_access_huge_value(uint64 raw_value, mtlk_wss_data_source_t src)
{
  return (MTIDL_SRC_FW == src) ? MAC_TO_HOST64(raw_value) : raw_value;
}

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif /* __MTLK_WSSA_H__ */
