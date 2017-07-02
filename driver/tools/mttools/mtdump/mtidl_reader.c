#include "mtlkinc.h"
#include "mtidl_reader.h"
#include "mtidl_ini_parser.h"
#include "mtlkwssa.h"
#include "mhi_statistics.h"
#include "mtidl_ini_parser.h"

#define LOG_LOCAL_GID   GID_MTDUMP
#define LOG_LOCAL_FID   1

typedef struct __mtlk_calculate_size_ctx
{
  uint32 size;
  const char* mtidl_dir;
  const char* binary_type;
} _mtlk_calculate_item_size_ctx;

static void
_mtlk_size_calculator(const mtlk_mtidl_field_t* mtidl_field, uint32 depth_in_tree, mtlk_handle_t ctx)
{
  uint32* size = HANDLE_T_PTR(uint32, ctx);
  *size += mtidl_field->element_size * mtidl_field->num_elements;
}

int __MTLK_IFUNC
mtlk_calculate_item_size(const char* mtidl_dir,
                         const char* root_binary_type,
                         uint32 *size)
{
  *size = 0;
  return mtlk_traverse_mtidl_subtree(mtidl_dir, root_binary_type, _mtlk_size_calculator, HANDLE_T(size));
}

typedef struct __mtlk_field_printer_ctx_t
{
  uint32 size;
  const char* buffer;
  mtlk_wss_data_source_t source;
  const char* mtidl_dir;
} _mtlk_field_printer_ctx_t;

static void
_mtlk_do_indentation(uint32 depth)
{
  while(depth--)
  {
    printf("  ");
  }
}

static uint64
_mtlk_get_mtidl_value(uint32 value_size, _mtlk_field_printer_ctx_t *caller_ctx)
{
  uint64 res = 0;

  if(caller_ctx->size < value_size)
  {
    ELOG_V("Failed to read value, buffer too small.");
    return 0;
  }

  switch(value_size)
  {
    case (sizeof(uint32)):
    {
      uint32 raw_value = *(uint32*) caller_ctx->buffer;
      res = mtlk_wssa_access_long_value(raw_value, caller_ctx->source);
      break;
    }
    case (sizeof(uint64)):
    {
      uint64 raw_value = *(uint64*) caller_ctx->buffer;
      res = mtlk_wssa_access_huge_value(raw_value, caller_ctx->source);
      break;
    }
    default:
      MTLK_ASSERT(FALSE);
  }

  caller_ctx->buffer += value_size;
  caller_ctx->size -= value_size;
  return res;
}

static void
_mtlk_get_mtidl_data(uint32 data_length, void* target_buffer, _mtlk_field_printer_ctx_t *caller_ctx)
{
  if(caller_ctx->size < data_length)
  {
    ELOG_V("Failed to read value, buffer too small.");
    memset(target_buffer, 0, data_length);
  }

  memcpy(target_buffer, caller_ctx->buffer, data_length);

  caller_ctx->buffer += data_length;
  caller_ctx->size -= data_length;
}

static void
_mtlk_skip_mtidl_data(uint32 data_length, _mtlk_field_printer_ctx_t *caller_ctx)
{
  if(caller_ctx->size < data_length)
  {
    ELOG_V("Failed to read value, buffer too small.");
  }

  caller_ctx->buffer += data_length;
  caller_ctx->size -= data_length;
}

typedef struct __enumeratable_printer_ctx_t
{
  uint32 enumeratable_value;
  uint32 depth_in_tree;
  int result;
} _enumeratable_printer_ctx_t;

static BOOL __MTLK_IFUNC
_mtlk_bitfield_printer(const mtlk_mtidl_enum_value_t* mtidl_enum_value, mtlk_handle_t ctx)
{
  _enumeratable_printer_ctx_t* caller_ctx = HANDLE_T_PTR(_enumeratable_printer_ctx_t, ctx);

  if(MTLK_BIT_GET(caller_ctx->enumeratable_value, mtidl_enum_value->value))
  {
    _mtlk_do_indentation(caller_ctx->depth_in_tree);
    printf("%20s :\n", mtidl_enum_value->name);
    MTLK_BIT_SET(caller_ctx->enumeratable_value, mtidl_enum_value->value, 0);
  }
  caller_ctx->result = MTLK_ERR_OK;

  return TRUE;
}

static BOOL __MTLK_IFUNC
_mtlk_enum_printer(const mtlk_mtidl_enum_value_t* mtidl_enum_value, mtlk_handle_t ctx)
{
  _enumeratable_printer_ctx_t* caller_ctx = HANDLE_T_PTR(_enumeratable_printer_ctx_t, ctx);

  if(caller_ctx->enumeratable_value == mtidl_enum_value->value)
  {
    printf("%20s", mtidl_enum_value->name);
    caller_ctx->result = MTLK_ERR_OK;
  }

  return TRUE;
}

static uint32
_mtlk_ipow(uint32 base, uint32 power)
{
  uint32 i;
  uint32 res = 1;

  for(i = 0; i < power; i++)
  {
    res *= base;
  }

  return res;
}

static void
_mtlk_array_element_printer(const char*                 description,
                            const char*                 binary_type,
                            const char*                 binary_subtype,
                            int                         element_size,
                            int                         fract_size,
                            uint32                      depth_in_tree,
                            _mtlk_field_printer_ctx_t * caller_ctx)
{
  _mtlk_do_indentation(depth_in_tree);

  if(0 == element_size)
  {
    printf("%s:\n", description);
  }
  else if(!strcmp(binary_type, "flag"))
  {
    printf("%20s : %s\n", _mtlk_get_mtidl_value(element_size, caller_ctx) ? "True" : "False",
                          description);
  }
  else if(!strcmp(binary_type, "time"))
  {
    printf("%11d msec ago : %s\n", (uint32 ) _mtlk_get_mtidl_value(element_size, caller_ctx),
                                    description);
  }
  else if(!strcmp(binary_type, "macaddr"))
  {
    mtidl_macaddr_t addr;
    _mtlk_get_mtidl_data(element_size, &addr, caller_ctx);
    printf("   " MAC_PRINTF_FMT " : %s\n", MAC_PRINTF_ARG(&addr), description);
  }
  else if(!strcmp(binary_type, "bitfield"))
  {
    _enumeratable_printer_ctx_t bf_printer_ctx;
    uint32 enumeratable_value = (uint32 ) _mtlk_get_mtidl_value(element_size, caller_ctx);

    if(0 == enumeratable_value)
    {
      printf("%20s : %s\n", "<empty>", description);
    }
    else
    {
      printf("%20s : %s\n", "", description);
      bf_printer_ctx.enumeratable_value = enumeratable_value;
      bf_printer_ctx.depth_in_tree = depth_in_tree;
      bf_printer_ctx.result = MTLK_ERR_NO_ENTRY;
      mtlk_mtidl_enum_bitfield_values(caller_ctx->mtidl_dir, binary_subtype, _mtlk_bitfield_printer, HANDLE_T(&bf_printer_ctx));
      if((MTLK_ERR_OK != bf_printer_ctx.result) || (0 != bf_printer_ctx.enumeratable_value))
      {
        _mtlk_do_indentation(depth_in_tree);
        printf("%20s :\n", "Unknown value");
      }
      _mtlk_do_indentation(depth_in_tree);
      printf("%20s :\n", "");
    }
  }
  else if(!strcmp(binary_type, "enum"))
  {
    _enumeratable_printer_ctx_t enum_printer_ctx;
    enum_printer_ctx.enumeratable_value = (uint32 ) _mtlk_get_mtidl_value(element_size, caller_ctx);
    enum_printer_ctx.depth_in_tree = depth_in_tree;
    enum_printer_ctx.result = MTLK_ERR_NO_ENTRY;
    mtlk_mtidl_enum_enumeration_values(caller_ctx->mtidl_dir, binary_subtype, _mtlk_enum_printer, HANDLE_T(&enum_printer_ctx));
    if(MTLK_ERR_OK != enum_printer_ctx.result)
    {
      printf("%20s", "Unknown value");
    }
    printf(" : %s\n", description);
  }
  else if(!strcmp(binary_type, "fract"))
  {
    uint32 value = (uint32)_mtlk_get_mtidl_value(element_size, caller_ctx);
    if (fract_size != 0)
    {
      uint32 base = _mtlk_ipow(10, fract_size);
      uint32 re_value =  value / base;
      uint32 fract_value = value % base;
      printf("%*u.%0*u : %s\n", 20 - fract_size - 1, re_value, fract_size, fract_value, description);
    }
    else
    {
      printf("%20u: %s\n", value, description);
    }
  }
  else if(!strcmp(binary_type, "sfract"))
  {
    int32 value = _mtlk_get_mtidl_value(element_size, caller_ctx);
    if (fract_size != 0)
    {
      uint32 base = _mtlk_ipow(10, fract_size);
      int32 re_value = value / base;
      int32 fract_value = value % base;
      printf("%*i.%0*i : %s\n", 20 - fract_size - 1, re_value, fract_size, fract_value , description);
    }
    else
    {
      printf("%20i: %s\n", value, description);
    }
  }
  else if(!strcmp(binary_type, "slong"))
  {
    printf("%20i : %s\n", (int32) _mtlk_get_mtidl_value(element_size, caller_ctx), description);
  }
  else if(!strcmp(binary_type, "shuge"))
  {
    printf("%20lli : %s\n", (int64) _mtlk_get_mtidl_value(element_size, caller_ctx), description);
  }
  else if(sizeof(uint32) == element_size)
  {
    printf("%20u : %s\n", (uint32) _mtlk_get_mtidl_value(element_size, caller_ctx), description);
  }
  else if(sizeof(uint64) == element_size)
  {
    printf("%20llu: %s\n", (uint64) _mtlk_get_mtidl_value(element_size, caller_ctx), description);
  }
  else
  {
    ELOG_S("Field of unknown type \"%s\" discovered", binary_type);
  }
}

static void
_mtlk_field_printer(const mtlk_mtidl_field_t* mtidl_field, uint32 depth_in_tree, mtlk_handle_t ctx)
{
  _mtlk_field_printer_ctx_t *caller_ctx = HANDLE_T_PTR(_mtlk_field_printer_ctx_t, ctx);

  if(!strcmp(mtidl_field->description, "===reserved==="))
  {
    _mtlk_skip_mtidl_data(mtidl_field->element_size * mtidl_field->num_elements, caller_ctx);
  }
  else if(1 == mtidl_field->num_elements)
  {
    _mtlk_array_element_printer(mtidl_field->description,
                                mtidl_field->binary_type,
                                mtidl_field->binary_subtype,
                                mtidl_field->element_size,
                                mtidl_field->fract_size,
                                depth_in_tree,
                                caller_ctx);
  }
  else
  {
    int i;
    char element_number_str[10];
    _mtlk_do_indentation(depth_in_tree);
    printf("%20s : %s\n", "", mtidl_field->description);

    for(i = 0; i < mtidl_field->num_elements; i++)
    {
      snprintf(element_number_str, ARRAY_SIZE(element_number_str), "[%d]", i);
      _mtlk_array_element_printer(element_number_str,
                                  mtidl_field->binary_type,
                                  mtidl_field->binary_subtype,
                                  mtidl_field->element_size,
                                  mtidl_field->fract_size,
                                  depth_in_tree+1,
                                  caller_ctx);
    }
    _mtlk_do_indentation(depth_in_tree + 1);
    printf("%20s :\n", "");
  }
}

int __MTLK_IFUNC
mtlk_print_mtidl_item(const char *mtidl_dir,
                      const char *binary_type,
                      mtlk_wss_data_source_t source,
                      const void *buffer,
                      uint32      size)
{
  _mtlk_field_printer_ctx_t ctx;
  ctx.buffer = buffer;
  ctx.size = size;
  ctx.source = source;
  ctx.mtidl_dir = mtidl_dir;

  return mtlk_traverse_mtidl_subtree(mtidl_dir, binary_type, _mtlk_field_printer, HANDLE_T(&ctx));
}

typedef struct __mtlk_print_by_id_ctx_t
{
  uint32 size;
  const char* buffer;
  mtlk_wss_data_source_t source;
  int info_id;
  const char* mtidl_dir;
  int result;
} _mtlk_print_by_id_ctx_t;

static BOOL
_mtlk_print_by_id_clb(const mtlk_mtidl_item_t* item, mtlk_handle_t ctx)
{
  _mtlk_print_by_id_ctx_t* caller_ctx = HANDLE_T_PTR(_mtlk_print_by_id_ctx_t, ctx);

  if((item->info_id == caller_ctx->info_id) && (item->info_source == caller_ctx->source))
  {
    caller_ctx->result = mtlk_print_mtidl_item(caller_ctx->mtidl_dir,
                                               item->binary_type,
                                               caller_ctx->source,
                                               caller_ctx->buffer,
                                               caller_ctx->size);
    return FALSE;
  }
  else return TRUE;
}

int __MTLK_IFUNC
mtlk_print_mtidl_item_by_id(const char *mtidl_dir,
                            mtlk_wss_data_source_t source,
                            int info_id,
                            const void *buffer,
                            uint32      size)
{
  int res;
  _mtlk_print_by_id_ctx_t ctx;
  ctx.buffer = buffer;
  ctx.size = size;
  ctx.source = source;
  ctx.mtidl_dir = mtidl_dir;
  ctx.info_id = info_id;
  ctx.result = MTLK_ERR_NO_ENTRY;

  res = mtlk_mtidl_enum_items(mtidl_dir, _mtlk_print_by_id_clb, HANDLE_T(&ctx));

  return (MTLK_ERR_OK == res) ? ctx.result : res;
}

#define _MTIDL_IS_REQUESTABLE(type)           (((type) == MTIDL_INFORMATION) || ((type) == MTIDL_STATISTICS))
#define _MTIDL_REQUIRE_PROVIDER_ID(provider)  ((provider) == MTIDL_PROVIDER_PEER)

static BOOL
_mtlk_print_requestable_mtidl_item_name(const mtlk_mtidl_item_t* item, mtlk_handle_t ctx)
{
  if(_MTIDL_IS_REQUESTABLE(item->type))
  {
    printf("        %-30s - %s\n", item->friendly_name, item->description);
  }
  return TRUE;
}

int __MTLK_IFUNC
mtlk_print_requestable_mtidl_items_list(const char* mtidl_dir)
{
  return mtlk_mtidl_enum_items(mtidl_dir, _mtlk_print_requestable_mtidl_item_name, HANDLE_T(NULL));
}

static BOOL
_mtlk_mtidl_items_counter(const mtlk_mtidl_item_t* item, mtlk_handle_t ctx)
{
  (*HANDLE_T_PTR(uint32, ctx))++;
  return TRUE;
}

int __MTLK_IFUNC
mtlk_count_mtidl_items(const char* mtidl_dir, uint32 *items_number)
{
  *items_number = 0;
  return mtlk_mtidl_enum_items(mtidl_dir, _mtlk_mtidl_items_counter, HANDLE_T(items_number));
}

typedef struct __mtlk_request_item_ctx
{
  const char* ifname;
  const char* friendly_name;
  const void* provider_id;
  const char* mtidl_dir;
  int result;
} _mtlk_request_item_ctx;

static BOOL
_mtlk_request_mtidl_item_clb(const mtlk_mtidl_item_t* item, mtlk_handle_t ctx)
{
  _mtlk_request_item_ctx* req_ctx = (_mtlk_request_item_ctx*) ctx;
  uint32 size;
  int res;

  void* item_buffer;

  if(_MTIDL_IS_REQUESTABLE(item->type) && !strcasecmp(req_ctx->friendly_name, item->friendly_name))
  {
    if(_MTIDL_REQUIRE_PROVIDER_ID(item->provider_level) && (NULL == req_ctx->provider_id))
    {
      req_ctx->result = MTLK_ERR_PARAMS;
      return FALSE;
    }

    res = mtlk_calculate_item_size(req_ctx->mtidl_dir, item->binary_type, &size);
    if(MTLK_ERR_OK != res)
    {
      ELOG_S("Failed to calculate size for binary type %s", item->binary_type);
      req_ctx->result = MTLK_ERR_NO_ENTRY;
      return FALSE;
    }

    item_buffer = mtlk_osal_mem_alloc(size, MTLK_MEM_TAG_MTDUMP_STATS);
    if(NULL == item_buffer)
    {
      ELOG_D("Failed to request buffer (%d bytes)", size);
      req_ctx->result = MTLK_ERR_NO_MEM;
      return FALSE;
    }

    req_ctx->result = mtlk_wss_request_info(req_ctx->ifname,
                                            item->provider_level,
                                            item->info_source,
                                            req_ctx->provider_id,
                                            item->info_id,
                                            item_buffer, size);

    if(MTLK_ERR_OK != req_ctx->result)
    {
      ELOG_D("Failed to retrieve information, error: %d", req_ctx->result);
    }
    else
    {
      mtlk_print_mtidl_item(req_ctx->mtidl_dir, item->binary_type, item->info_source, item_buffer, size);
    }

    mtlk_osal_mem_free(item_buffer);
    return FALSE;
  }
  return TRUE;
}

int __MTLK_IFUNC
mtlk_request_mtidl_item(const char* mtidl_dir, const char* ifname, const char* friendly_name, void* provider_id)
{
  int res;
  _mtlk_request_item_ctx ctx;

  ctx.ifname = ifname;
  ctx.friendly_name = friendly_name;
  ctx.provider_id = provider_id;
  ctx.result = MTLK_ERR_NO_ENTRY;
  ctx.mtidl_dir = mtidl_dir;

  res = mtlk_mtidl_enum_items(mtidl_dir, _mtlk_request_mtidl_item_clb, HANDLE_T(&ctx));

  return (MTLK_ERR_OK != res) ? res : ctx.result;
}
