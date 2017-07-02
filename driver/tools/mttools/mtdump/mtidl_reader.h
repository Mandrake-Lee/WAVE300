#ifndef __MTIDL_READER_H__
#define __MTIDL_READER_H__

#include "mtlkwssa.h"

int __MTLK_IFUNC mtlk_calculate_item_size(const char* mtidl_dir, 
                                          const char* root_binary_type,
                                          uint32 *size);

int __MTLK_IFUNC
mtlk_print_mtidl_item(const char *mtidl_dir,
                      const char *binary_type,
                      mtlk_wss_data_source_t source,
                      const void *buffer,
                      uint32 size);

int __MTLK_IFUNC
mtlk_print_mtidl_item_by_id(const char *mtidl_dir,
                            mtlk_wss_data_source_t source,
                            int info_id,
                            const void *buffer,
                            uint32      size);

int __MTLK_IFUNC
mtlk_print_requestable_mtidl_items_list(const char* mtidl_dir);

int __MTLK_IFUNC
mtlk_count_mtidl_items(const char* mtidl_dir, uint32 *items_number);

int __MTLK_IFUNC
mtlk_request_mtidl_item(const char* mtidl_dir,
                        const char* ifname,
                        const char* friendly_name,
                        void* provider_id);

#endif /* !__MTIDL_READER_H__ */
