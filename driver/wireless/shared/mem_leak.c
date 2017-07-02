#include "mtlkinc.h"
#include "mem_leak.h"
#include "mtlklist.h"

#define LOG_LOCAL_GID   GID_MEM_LEAK
#define LOG_LOCAL_FID   1

#ifdef MTCFG_ENABLE_OBJPOOL

MTLK_DECLARE_OBJPOOL(g_objpool);

/********************************************************************************
 * Private memory leaks debug releated stuff
 ********************************************************************************/
struct mem_obj
{
  MTLK_DECLARE_OBJPOOL_CTX(objpool_ctx);
  uint32 size;
  char   buffer[1];
};

#define MEM_OBJ_HDR_SIZE ((uint32)MTLK_OFFSET_OF(struct mem_obj, buffer))

#define FREED_MEM_FILL_CHAR (0xC)

static const uint32  FRONT_GUARD = 0xF0F0F0F0;
static const uint32  BACK_GUARD  = 0xBABABABA;

#define GET_GUARDED_BY_MEM(mem) (mem->buffer + sizeof(FRONT_GUARD))
#define GET_MEM_BY_GUARDED(buf) MTLK_CONTAINER_OF(((char*)buf) - sizeof(FRONT_GUARD), struct mem_obj, buffer[0])

#define GET_FGUARD_POS(mem)     (mem->buffer)
#define GET_BGUARD_POS(mem)     (mem->buffer + sizeof(FRONT_GUARD) + mem->size)

static void 
guards_set (struct mem_obj *mem)
{
  memcpy(GET_FGUARD_POS(mem), &FRONT_GUARD, sizeof(FRONT_GUARD));
  memcpy(GET_BGUARD_POS(mem), &BACK_GUARD, sizeof(BACK_GUARD));
}

static void 
guards_check (struct mem_obj *mem)
{
  mtlk_slid_t slid = mtlk_objpool_get_creator_slid(&g_objpool, &mem->objpool_ctx);

  if (memcmp(GET_FGUARD_POS(mem), &FRONT_GUARD, sizeof(FRONT_GUARD)))
  {
    ELOG_DDDPD("FGUARD corruption (G:%d F:%d L:%d, ptr: %p, size %u)",
              mtlk_slid_get_gid(slid), mtlk_slid_get_fid(slid), mtlk_slid_get_lid(slid),
              GET_GUARDED_BY_MEM(mem), mem->size);
    MTLK_ASSERT(FALSE);
  }

  if (memcmp(GET_BGUARD_POS(mem), &BACK_GUARD, sizeof(BACK_GUARD)))
  {
    ELOG_DDDPD("BGUARD corruption (G:%d F:%d L:%d, ptr: %p, size %u)",
              mtlk_slid_get_gid(slid), mtlk_slid_get_fid(slid), mtlk_slid_get_lid(slid),
              GET_GUARDED_BY_MEM(mem), mem->size);
    MTLK_ASSERT(FALSE);
  }
}

uint32 __MTLK_IFUNC
mem_leak_get_full_allocation_size (uint32 size)
{
  /* mem_dbg structure + requested size + guards */
  return (uint32)MEM_OBJ_HDR_SIZE + size + sizeof(FRONT_GUARD) + sizeof(BACK_GUARD);
}

void * __MTLK_IFUNC
mem_leak_handle_allocated_buffer (void *mem_dbg_buffer, uint32 size,
                                  mtlk_slid_t caller_slid)
{
  struct mem_obj *mem = (struct mem_obj *)mem_dbg_buffer;

  if (!mem) {
    return NULL;
  }

  mem->size = size;

  mtlk_objpool_add_object_ex(&g_objpool, &mem->objpool_ctx, MTLK_MEMORY_OBJ,
                             caller_slid, HANDLE_T(mem->size));
  guards_set(mem);

  ILOG5_PPD("%p (%p %d)", GET_GUARDED_BY_MEM(mem), mem, mem->size);

  return GET_GUARDED_BY_MEM(mem);
}

void * __MTLK_IFUNC
mem_leak_handle_buffer_to_free (void *buffer)
{
  struct mem_obj *mem = GET_MEM_BY_GUARDED(buffer);

  ILOG5_PPD("%p (%p %d)", buffer, mem, mem->size);

  mtlk_objpool_remove_object_ex(&g_objpool, &mem->objpool_ctx, MTLK_MEMORY_OBJ, HANDLE_T(mem->size));
  guards_check(mem);
  memset(mem, FREED_MEM_FILL_CHAR, MEM_OBJ_HDR_SIZE + mem->size + sizeof(FRONT_GUARD) + sizeof(BACK_GUARD));

  return mem;
}

uint32 __MTLK_IFUNC
mem_leak_dbg_get_allocated_size (void)
{
  return mtlk_objpool_get_memory_allocated(&g_objpool);
}

struct mem_leak_dbg_printf_ctx
{
  mem_leak_dbg_printf_f printf_func;
  mtlk_handle_t         printf_ctx;
  uint32                total_size;
  uint32                total_count;
};

static BOOL __MTLK_IFUNC
_mem_leak_dbg_printf_allocator_clb (mtlk_objpool_t* objpool,
                                    mtlk_slid_t     allocator_slid,
                                    uint32          objects_number,
                                    uint32          additional_allocations_size,
                                    mtlk_handle_t   context)
{
  struct mem_leak_dbg_printf_ctx *ctx = 
    HANDLE_T_PTR(struct mem_leak_dbg_printf_ctx, context);

  ctx->printf_func(ctx->printf_ctx, "| %7u | %3u | G:%3d F:%2d L:%5d",
                   additional_allocations_size, objects_number,
                   mtlk_slid_get_gid(allocator_slid),
                   mtlk_slid_get_fid(allocator_slid),
                   mtlk_slid_get_lid(allocator_slid));

  ctx->total_size  += additional_allocations_size;
  ctx->total_count += objects_number;

  return TRUE;
}

void __MTLK_IFUNC
mem_leak_dbg_print_allocators_info (mem_leak_dbg_printf_f printf_func,
                                    mtlk_handle_t         printf_ctx)
{
  struct mem_leak_dbg_printf_ctx ctx;

  MTLK_ASSERT(printf_func != NULL);

  ctx.printf_func = printf_func;
  ctx.printf_ctx  = printf_ctx;
  ctx.total_count = 0;
  ctx.total_size  = 0;

  printf_func(printf_ctx, "Allocations dump:");
  printf_func(printf_ctx, "---------------------------------------------");
  printf_func(printf_ctx, "|  size   | cnt |        SLID");
  printf_func(printf_ctx, "---------------------------------------------");

  mtlk_objpool_enum_by_type(&g_objpool, MTLK_MEMORY_OBJ, _mem_leak_dbg_printf_allocator_clb,
                            HANDLE_T(&ctx));

  printf_func(printf_ctx, "=============================================");
  printf_func(printf_ctx, " Total: %u allocations = %u bytes",
              ctx.total_count, ctx.total_size);
  printf_func(printf_ctx, "=============================================");
}

#endif /* MTCFG_ENABLE_OBJPOOL */
