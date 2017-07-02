#ifndef __MEM_LEAK_H__
#define __MEM_LEAK_H__

/***************************************************************************
 * Memory leak/overwrite control API. To add your allocation to the memory 
 * leak/overwrite control system you must create your allocator/de-allocator
 * routines like:
 * 
 * void *my_malloc (int size, ...)
 * {
 *   void *buf = <system_allocator>(mem_leak_get_full_allocation_size(size), ...);
 *   return mem_leak_handle_allocated_buffer(buf, size, slid);
 * }
 *
 * void my_free (void *buffer)
 * {
 *   void *buf = mem_leak_handle_buffer_to_free(buffer);
 *   <system_deallocator>(buf);
 * }
 *
 ***************************************************************************/
void * __MTLK_IFUNC
mem_leak_handle_allocated_buffer(void       *mem_leak_buffer, 
                                 uint32      size, 
                                 mtlk_slid_t caller_slid);
void * __MTLK_IFUNC
mem_leak_handle_buffer_to_free(void *buffer);
uint32 __MTLK_IFUNC
mem_leak_get_full_allocation_size(uint32 size);


/* DEBUG abilities */
uint32 __MTLK_IFUNC
mem_leak_dbg_get_allocated_size(void);

struct mem_leak_dbg_ainfo /* allocator info */
{
  mtlk_slid_t allocator_slid;
  uint32 count;
  uint32 size;
};

typedef void (__MTLK_IFUNC * mem_leak_dbg_enum_f)(mtlk_handle_t                    ctx,
                                                  const struct mem_leak_dbg_ainfo *ainfo);

typedef int (__MTLK_IFUNC *mem_leak_dbg_printf_f)(mtlk_handle_t printf_ctx,
                                                  const char   *format,
                                                  ...);

void __MTLK_IFUNC
mem_leak_dbg_print_allocators_info(mem_leak_dbg_printf_f printf_func,
                                   mtlk_handle_t         printf_ctx);

#endif

