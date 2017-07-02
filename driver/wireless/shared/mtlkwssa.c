#include "mtlkinc.h"
#include "mtlkirba.h"
#include "mtlkwssa.h"
#include "mtlkwlanirbdefs.h"
#include "mtlkwssairb.h"
#include "dataex.h"

#define LOG_LOCAL_GID   GID_WSS_APP
#define LOG_LOCAL_FID   1

#define _MAX_NODE_NAME_LENGTH  (256)
#define _MAX_PEERS_NUMBER      (256)

const static mtlk_guid_t IRBR_GUID_WSSA_REQ_INFO   = MTLK_IRB_GUID_WSSA_REQ_INFO;
const static mtlk_guid_t IRBE_GUID_WSSA_SEND_EVENT = MTLK_IRB_GUID_WSSA_SEND_EVENT;

static void
_mtlk_node_name_from_sta_addr(IEEE_ADDR* sta_addr, char* name_buffer, uint32 name_buffer_size)
{
  if(0 > snprintf(name_buffer, name_buffer_size,
                  MTLK_IRB_STA_NAME MAC_PRINTF_FMT, MAC_PRINTF_ARG(sta_addr->au8Addr)))
  {
    name_buffer[0] = '\0';
  }
}

static void
_mtlk_sta_addr_from_node_name(const char* sta_name, IEEE_ADDR* sta_addr)
{
#define _MAC_ARG_SCAN(x) &(x)[0],&(x)[1],&(x)[2],&(x)[3],&(x)[4],&(x)[5]
  uint32 mac_addr_bytes[IEEE_ADDR_LEN];

  if(sizeof(IEEE_ADDR) > sscanf(sta_name, MTLK_IRB_STA_NAME MAC_PRINTF_FMT, _MAC_ARG_SCAN(mac_addr_bytes)))
  {
    memset(sta_addr, 0, sizeof(IEEE_ADDR));
  }
  else
  {
    int i;
    for(i = 0; i < IEEE_ADDR_LEN; i++)
      sta_addr->au8Addr[i] = mac_addr_bytes[i];
  }

#undef _MAC_ARG_SCAN
}

static int
_mtlk_wss_fill_stations_array(mtlk_node_t* wlan_node, IEEE_ADDR *station_array, uint32 array_size)
{
  uint32 num_stations = 0;

  mtlk_node_son_t *node_son = mtlk_node_son_get_first(wlan_node);
  while(NULL != node_son)
  {
    const char* son_name = mtlk_node_son_get_desc(wlan_node, node_son);

    if(0 == strncmp(son_name, MTLK_IRB_STA_NAME, strlen(MTLK_IRB_STA_NAME)))
    {
      if(num_stations < array_size)
      {
        _mtlk_sta_addr_from_node_name(son_name, &station_array[num_stations]);
      }
      num_stations++;
    }
    node_son = mtlk_node_son_get_next(wlan_node, node_son);
  }

  return num_stations;
}

static int
_mtlk_wss_get_hw_target_node_name (const char* interface_name,
                                   char* name_buffer,
                                   uint32 name_buffer_length)
{
  int         res = MTLK_ERR_NO_MEM;
  const char* parent_descr;

  mtlk_node_t *wlan_node = mtlk_node_alloc();
  if(NULL != wlan_node)
  {
    res = MTLK_ERR_NO_ENTRY;
    if(MTLK_ERR_OK == mtlk_node_attach(wlan_node, interface_name))
    {
      parent_descr = mtlk_node_parent_get_desc(wlan_node);
      if (NULL != parent_descr) 
      {
        strncpy(name_buffer, parent_descr, name_buffer_length);
        res = MTLK_ERR_OK;
      }
      mtlk_node_detach(wlan_node);
    }
    mtlk_node_free(wlan_node);
  }

  return res;
}

static int
_mtlk_wss_get_target_node_name(const char* interface_name,
                               mtlk_wss_provider_level_t level,
                               const void* provider_id,
                               char* name_buffer,
                               uint32 name_buffer_length)
{
  switch(level)
  {
  case MTIDL_PROVIDER_WLAN:
    {
      strncpy(name_buffer, interface_name, name_buffer_length);
      return MTLK_ERR_OK;
    }
  case MTIDL_PROVIDER_HW:
    {
      return _mtlk_wss_get_hw_target_node_name(interface_name, name_buffer, name_buffer_length);
   }
  case MTIDL_PROVIDER_PEER:
    {
      _mtlk_node_name_from_sta_addr((IEEE_ADDR*) provider_id,
                                    name_buffer,
                                    name_buffer_length);
      return MTLK_ERR_OK;
    }
  default:
    return MTLK_ERR_NO_ENTRY;
  }
};

struct _mtlk_wss_node_rm_clb_ctx
{
  mtlk_wss_rm_handler_f rm_func_ptr;
  mtlk_handle_t rm_ctx;
};

static void __MTLK_IFUNC
_mtlk_irba_rm_handler (mtlk_irba_t   *irba,
                                    mtlk_handle_t  context)
{
  struct _mtlk_wss_node_rm_clb_ctx* ctx =
    HANDLE_T_PTR(struct _mtlk_wss_node_rm_clb_ctx, context);

  if(NULL != ctx->rm_func_ptr)
    ctx->rm_func_ptr(ctx->rm_ctx);
}

static int
_mtlk_wss_transact_irb(mtlk_irba_t *irba, mtlk_wss_data_source_t info_source,
                       uint32 info_id, void* data_buff, uint32 buff_length)
{
  int res;
  mtlk_wssa_info_hdr_t* request_buffer =
    mtlk_osal_mem_alloc(sizeof(mtlk_wssa_info_hdr_t) + buff_length, MTLK_MEM_TAG_WSS);

  if(NULL == request_buffer)
    return MTLK_ERR_NO_MEM;

  request_buffer->info_source = info_source;
  request_buffer->info_id = info_id;
  request_buffer->processing_result = MTLK_ERR_NO_ENTRY;

  memcpy(&request_buffer[1], data_buff, buff_length);
  res = mtlk_irba_call_drv(irba, &IRBR_GUID_WSSA_REQ_INFO,
                           request_buffer, sizeof(mtlk_wssa_info_hdr_t) + buff_length);

  if(MTLK_ERR_OK == res)
    res = request_buffer->processing_result;

  if(MTLK_ERR_OK == res)
    memcpy(data_buff, &request_buffer[1], buff_length);

  mtlk_osal_mem_free(request_buffer);
  return res;
}

static int
_mtlk_wss_alloc_irba_attached(mtlk_irba_t** irba,
                              struct _mtlk_wss_node_rm_clb_ctx** rm_clb_ctx,
                              const char* interface_name,
                              mtlk_wss_provider_level_t level,
                              const void* provider_id,
                              mtlk_wss_rm_handler_f rm_func_ptr,
                              mtlk_handle_t rm_ctx)
{
  char target_node_name[_MAX_NODE_NAME_LENGTH];

  /* Get target node name */
  int res = _mtlk_wss_get_target_node_name(interface_name, level, provider_id,
                                           target_node_name, sizeof(target_node_name));
  if(MTLK_ERR_OK != res)
  {
    return res;
  }

  /* Connect to target node by name */
  *irba = mtlk_irba_alloc();
  if(NULL == *irba)
    return MTLK_ERR_NO_MEM;

  *rm_clb_ctx = 
    mtlk_osal_mem_alloc(sizeof(struct _mtlk_wss_node_rm_clb_ctx),
                        MTLK_MEM_TAG_WSS);
  if(NULL == *rm_clb_ctx)
  {
    mtlk_irba_free(*irba);
    return MTLK_ERR_NO_MEM;
  }

  (*rm_clb_ctx)->rm_func_ptr = rm_func_ptr;
  (*rm_clb_ctx)->rm_ctx = rm_ctx;

  res = mtlk_irba_init(*irba, target_node_name,
                       _mtlk_irba_rm_handler, HANDLE_T(*rm_clb_ctx));
  if(MTLK_ERR_OK != res)
  {
    mtlk_osal_mem_free(*rm_clb_ctx);
    mtlk_irba_free(*irba);
  }

  return res;
}

static void
_mtlk_wss_free_irba_attached(mtlk_irba_t* irba, struct _mtlk_wss_node_rm_clb_ctx* rm_clb_ctx)
{
  mtlk_irba_cleanup(irba);
  mtlk_osal_mem_free(rm_clb_ctx);
  mtlk_irba_free(irba);
}

int __MTLK_IFUNC
mtlk_wss_request_info(const char* interface_name, mtlk_wss_provider_level_t level,
                      mtlk_wss_data_source_t info_source, const void* provider_id, uint32 info_id,
                      void* data_buff, uint32 buff_length)
{
  mtlk_irba_t *target_node_irba;
  struct _mtlk_wss_node_rm_clb_ctx* rm_clb_ctx;
  int res;

  /* Attach to the corresponding node */
  res = _mtlk_wss_alloc_irba_attached(&target_node_irba, &rm_clb_ctx, interface_name,
                                      level, provider_id, NULL, HANDLE_T(0));
  if(MTLK_ERR_OK != res)
    return res;

  /* Build and send IRB request */
  res = _mtlk_wss_transact_irb(target_node_irba, info_source, info_id,
                               data_buff, buff_length);

  /* Cleanup everything and exit with results */
  _mtlk_wss_free_irba_attached(target_node_irba, rm_clb_ctx);

  return res;
}

struct _mtlk_wss_event_callback_ctx_t
{
  mtlk_wss_event_receiver_f original_callback;
  mtlk_handle_t             original_ctx;
};

struct _mtlk_wss_registered_callback_ctx_t
{
  mtlk_irba_t *irba;
  mtlk_irba_handle_t *register_handle;
  struct _mtlk_wss_event_callback_ctx_t *internal_context;
  struct _mtlk_wss_node_rm_clb_ctx* rm_clb_ctx;
};

static void __MTLK_IFUNC
_mtlk_wss_event_arrived (mtlk_irba_t       *irba,
                         mtlk_handle_t      context,
                         const mtlk_guid_t *evt,
                         void              *buffer,
                         uint32            size)
{
  struct _mtlk_wss_event_callback_ctx_t *internal_ctx =
    HANDLE_T_PTR(struct _mtlk_wss_event_callback_ctx_t, context);
  mtlk_wssa_info_hdr_t* info_hdr = (mtlk_wssa_info_hdr_t*) buffer;

  MTLK_ASSERT(mtlk_guid_compare(&IRBE_GUID_WSSA_SEND_EVENT, evt) == 0);
  MTLK_ASSERT(size >= sizeof(mtlk_wssa_info_hdr_t));

  internal_ctx->original_callback(info_hdr->info_source,
                                  info_hdr->info_id,
                                  &info_hdr[1],
                                  size - sizeof(mtlk_wssa_info_hdr_t),
                                  internal_ctx->original_ctx);
}

static int
_mtlk_wss_register_for_driver_events(mtlk_irba_t *irba,
                                     struct _mtlk_wss_node_rm_clb_ctx* rm_clb_ctx,
                                     mtlk_wss_event_receiver_f receiver_func_ptr,
                                     mtlk_handle_t ctx,
                                     mtlk_handle_t* unregister_handle)
{
  struct _mtlk_wss_event_callback_ctx_t *internal_ctx =
    mtlk_osal_mem_alloc(sizeof(struct _mtlk_wss_event_callback_ctx_t),
                        MTLK_MEM_TAG_WSS);
  mtlk_irba_handle_t *irba_unregister_handle;

  if(NULL == internal_ctx)
    return MTLK_ERR_NO_MEM;

  internal_ctx->original_callback = receiver_func_ptr;
  internal_ctx->original_ctx = ctx;

  irba_unregister_handle = mtlk_irba_register(irba, &IRBE_GUID_WSSA_SEND_EVENT, 1,
                                              _mtlk_wss_event_arrived, HANDLE_T(internal_ctx));

  if(NULL == irba_unregister_handle)
  {
    mtlk_osal_mem_free(internal_ctx);
    return MTLK_ERR_NO_RESOURCES;
  }

  struct _mtlk_wss_registered_callback_ctx_t *registered_callback_ctx =
    mtlk_osal_mem_alloc(sizeof(struct _mtlk_wss_registered_callback_ctx_t),
                        MTLK_MEM_TAG_WSS);

  if(NULL == registered_callback_ctx)
  {
    mtlk_irba_unregister(irba, irba_unregister_handle);
    mtlk_osal_mem_free(internal_ctx);
    return MTLK_ERR_NO_MEM;
  }

  registered_callback_ctx->irba = irba;
  registered_callback_ctx->register_handle = irba_unregister_handle;
  registered_callback_ctx->internal_context = internal_ctx;
  registered_callback_ctx->rm_clb_ctx = rm_clb_ctx;

  *unregister_handle = HANDLE_T(registered_callback_ctx);
  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_wss_register_events_callback(const char* interface_name,
                                  mtlk_wss_provider_level_t level,
                                  const void* provider_id,
                                  mtlk_wss_event_receiver_f receiver_func_ptr,
                                  mtlk_handle_t ctx,
                                  mtlk_handle_t *unregister_handle,
                                  mtlk_wss_rm_handler_f rm_func_ptr,
                                  mtlk_handle_t rm_ctx)
{
  mtlk_irba_t *target_node_irba;
  struct _mtlk_wss_node_rm_clb_ctx* rm_clb_ctx;
  int res;

  /* Attach to the corresponding node */
  res = _mtlk_wss_alloc_irba_attached(&target_node_irba, &rm_clb_ctx, interface_name,
                                      level, provider_id, rm_func_ptr, rm_ctx);
  if(MTLK_ERR_OK != res)
    return res;

  /* Build and send IRB request */
  res = _mtlk_wss_register_for_driver_events(target_node_irba, rm_clb_ctx, receiver_func_ptr,
                                             ctx, unregister_handle);

  if(MTLK_ERR_OK != res)
    _mtlk_wss_free_irba_attached(target_node_irba, rm_clb_ctx);

  return res;
}

void __MTLK_IFUNC
mtlk_wss_unregister_events_callback(mtlk_handle_t register_handle)
{
  struct _mtlk_wss_registered_callback_ctx_t* registered_callback_ctx =
    HANDLE_T_PTR(struct _mtlk_wss_registered_callback_ctx_t, register_handle);

  mtlk_irba_unregister(registered_callback_ctx->irba,
                       registered_callback_ctx->register_handle);

  _mtlk_wss_free_irba_attached(registered_callback_ctx->irba,
                               registered_callback_ctx->rm_clb_ctx);

  mtlk_osal_mem_free(registered_callback_ctx->internal_context);
  mtlk_osal_mem_free(registered_callback_ctx);
}

int __MTLK_IFUNC
mtlk_wssa_api_init(mtlk_wss_rm_handler_f rm_func_ptr,
                   mtlk_handle_t rm_ctx,
                   mtlk_handle_t* wss_cleanup_context)
{
  int res;
  struct _mtlk_wss_node_rm_clb_ctx *removal_clb_ctx;

  removal_clb_ctx = 
    mtlk_osal_mem_alloc(sizeof(struct _mtlk_wss_node_rm_clb_ctx),
                        MTLK_MEM_TAG_WSS);
  if(NULL == removal_clb_ctx)
    return MTLK_ERR_NO_MEM;

  removal_clb_ctx->rm_func_ptr = rm_func_ptr;
  removal_clb_ctx->rm_ctx = rm_ctx;

  res = mtlk_irba_app_init(_mtlk_irba_rm_handler, HANDLE_T(removal_clb_ctx));

  if(MTLK_ERR_OK == res)
    res = mtlk_irba_app_start();

  if(MTLK_ERR_OK != res)
    mtlk_osal_mem_free(removal_clb_ctx);
  else *wss_cleanup_context = HANDLE_T(removal_clb_ctx);

  return res;
}

void __MTLK_IFUNC
mtlk_wssa_api_cleanup(mtlk_handle_t wss_cleanup_context)
{
  struct _mtlk_wss_node_rm_clb_ctx *removal_clb_ctx =
    HANDLE_T_PTR(struct _mtlk_wss_node_rm_clb_ctx, wss_cleanup_context);

  mtlk_irba_app_stop();
  mtlk_irba_app_cleanup();
  mtlk_osal_mem_free(removal_clb_ctx);
}

mtlk_wss_provider_list_t* __MTLK_IFUNC
mtlk_wss_get_providers_list(const char* interface_name, mtlk_wss_provider_level_t level, int *res)
{
  mtlk_wss_provider_list_t *res_list = NULL;

  MTLK_ASSERT(res != NULL);

  switch(level)
  {
  case MTIDL_PROVIDER_WLAN:
  case MTIDL_PROVIDER_HW:
    {
      res_list = mtlk_osal_mem_alloc(sizeof(res_list->providers_number) + _MAX_NODE_NAME_LENGTH, MTLK_MEM_TAG_WSS);
      if(NULL != res_list)
      {
        res_list->providers_number = 1;
        *res = _mtlk_wss_get_target_node_name(interface_name, level, NULL,
                                              (char*) &res_list->provider_id_array, _MAX_NODE_NAME_LENGTH);
        if(MTLK_ERR_OK != *res)
        {
          mtlk_osal_mem_free(res_list);
          res_list = NULL;
        }
      }
      else
      {
        *res = MTLK_ERR_NO_MEM;
      }
    }
    break;
  case MTIDL_PROVIDER_PEER:
    {
      mtlk_node_t *wlan_node = mtlk_node_alloc();
      if(NULL != wlan_node)
      {
        if (MTLK_ERR_OK != mtlk_node_attach(wlan_node, interface_name))
        {
          *res = MTLK_ERR_NO_MEM;
        }
        else if (mtlk_node_parent_get_desc(wlan_node) == NULL)
        {
          *res = MTLK_ERR_NO_ENTRY;
        }
        else
        {
          res_list = mtlk_osal_mem_alloc(sizeof(res_list->providers_number) + 
                                         sizeof(IEEE_ADDR) * _MAX_PEERS_NUMBER, MTLK_MEM_TAG_WSS);
          if(NULL != res_list)
          {
            res_list->providers_number =
              _mtlk_wss_fill_stations_array(wlan_node, (IEEE_ADDR*)res_list->provider_id_array, _MAX_PEERS_NUMBER);
            *res = MTLK_ERR_OK;
          }
          else
          {
            *res = MTLK_ERR_NO_MEM;
          }
        }
        mtlk_node_free(wlan_node);
      }
    }
    break;
  default:
    MTLK_ASSERT(0); /* Unknown level */
    break;
  }

  return res_list;
}

void __MTLK_IFUNC
mtlk_wss_free_providers_list(mtlk_wss_provider_list_t* providers_list)
{
  mtlk_osal_mem_free(providers_list);
}
