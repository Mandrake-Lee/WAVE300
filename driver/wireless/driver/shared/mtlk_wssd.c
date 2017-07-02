#include "mtlkinc.h"
#include "mhi_statistics.h"
#include "mtlkirbd.h"
#include "mtlk_wssd.h"

#define LOG_LOCAL_GID   GID_MTLKWSSD
#define LOG_LOCAL_FID   1

mtlk_irbd_handle_t* __MTLK_IFUNC
mtlk_wssd_register_request_handler(mtlk_irbd_t* irbd,
                                   mtlk_irbd_evt_handler_f handler_func,
                                   mtlk_handle_t handler_ctx)
{
  static const mtlk_guid_t IRBE_GUID_WSSA_REQ_INFO = MTLK_IRB_GUID_WSSA_REQ_INFO;

  return mtlk_irbd_register(irbd, &IRBE_GUID_WSSA_REQ_INFO, 1,
                            handler_func, handler_ctx);
}

void __MTLK_IFUNC
mtlk_wssd_unregister_request_handler(mtlk_irbd_t* irbd,
                                     mtlk_irbd_handle_t* reg_handle)
{
  mtlk_irbd_unregister(irbd, reg_handle);
}

int __MTLK_IFUNC
mtlk_wssd_send_event(mtlk_irbd_t *     irbd,
                     uint32            info_id,
                     void*             data_buff,
                     uint32            buff_length)
{
  int res;
  mtlk_wssa_info_hdr_t* request_buffer;
  static const mtlk_guid_t IRBE_GUID_WSSA_SEND_EVENT = MTLK_IRB_GUID_WSSA_SEND_EVENT;

  request_buffer = mtlk_osal_mem_alloc(sizeof(mtlk_wssa_info_hdr_t) + buff_length, MTLK_MEM_TAG_WSS);

  if(NULL == request_buffer) {
    return MTLK_ERR_NO_MEM;
  }

  request_buffer->info_source = MTIDL_SRC_DRV;
  request_buffer->info_id = info_id;
  request_buffer->processing_result = MTLK_ERR_OK;

  memcpy(&request_buffer[1], data_buff, buff_length);

  res = mtlk_irbd_notify_app(irbd,
                             &IRBE_GUID_WSSA_SEND_EVENT,
                             request_buffer,
                             sizeof(mtlk_wssa_info_hdr_t) + buff_length);

  mtlk_osal_mem_free(request_buffer);

  return res;
}


