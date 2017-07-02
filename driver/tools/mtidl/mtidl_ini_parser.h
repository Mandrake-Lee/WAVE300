#ifndef __MTLK_MTIDL_INI_PARSER_H__
#define __MTLK_MTIDL_INI_PARSER_H__

typedef struct _mtlk_mtidl_item_t mtlk_mtidl_item_t;
typedef BOOL __MTLK_IFUNC (*mtlk_enum_items_callback_f)(const mtlk_mtidl_item_t* mtidl_item,
                                                        mtlk_handle_t ctx);

struct _mtlk_mtidl_item_t
{
  const char* friendly_name;
  const char* description;
  const char* binary_type;
  int         type;
  int         provider_level;
  int         info_source;
  int         info_id;

  mtlk_handle_t fields_enum_ctx;
};

typedef struct _mtlk_mtidl_field_t mtlk_mtidl_field_t;
typedef int __MTLK_IFUNC (*mtlk_enum_fields_callback_f)(const mtlk_mtidl_field_t* mtidl_field, mtlk_handle_t ctx);

struct _mtlk_mtidl_field_t
{
  const char* binary_type;
  const char* binary_subtype;
  const char* description;
  int         element_size;
  int         fract_size;
  int         num_elements;
};

int __MTLK_IFUNC
mtlk_mtidl_enum_items(const char* mtidl_dir,
                      mtlk_enum_items_callback_f callback,
                      mtlk_handle_t context);

int __MTLK_IFUNC
mtlk_mtidl_item_enum_fields(mtlk_handle_t fields_enum_ctx,
                            mtlk_enum_fields_callback_f callback,
                            mtlk_handle_t context);

typedef void __MTLK_IFUNC (*mtlk_tree_traversal_clb_f)(const mtlk_mtidl_field_t* mtidl_field, uint32 depth_in_tree, mtlk_handle_t ctx);

int __MTLK_IFUNC
mtlk_traverse_mtidl_subtree(const char* mtidl_dir,
                            const char* root_binary_type,
                            mtlk_tree_traversal_clb_f per_field_callback,
                            mtlk_handle_t per_field_callback_ctx);

typedef struct _mtlk_mtidl_enum_value_t
{
  const char* name;
  uint32      value;
} mtlk_mtidl_enum_value_t;

typedef BOOL __MTLK_IFUNC (*mtlk_enum_value_clb_f)(const mtlk_mtidl_enum_value_t* mtidl_enum_value, mtlk_handle_t ctx);

int __MTLK_IFUNC
mtlk_mtidl_enum_bitfield_values(const char* mtidl_dir,
                                const char* bitfield_binary_type,
                                mtlk_enum_value_clb_f per_value_callback,
                                mtlk_handle_t per_value_callback_ctx);

int __MTLK_IFUNC
mtlk_mtidl_enum_enumeration_values(const char* mtidl_dir,
                                   const char* enum_binary_type,
                                   mtlk_enum_value_clb_f per_value_callback,
                                   mtlk_handle_t per_value_callback_ctx);

#endif //__MTLK_MTIDL_INI_PARSER_H__
