#ifndef __MEM_DEFS_H__
#define __MEM_DEFS_H__

#include "memtags.h"

#ifdef MTCFG_ENABLE_OBJPOOL

#include "mem_leak.h"

/* Special versions for objpool */
static __INLINE void *
malloc_objpool (size_t size, unsigned int tag)
{
  return kmalloc(size, GFP_ATOMIC);
}

static __INLINE void
free_objpool (void *buffer)
{
  kfree(buffer);
}

/* Normal allocator that must be monitored by objpool */
static __INLINE void *
_kmalloc_tag (size_t size, gfp_t gfp, unsigned int tag, mtlk_slid_t caller_slid)
{
  void *buf = kmalloc(mem_leak_get_full_allocation_size(size), gfp);
  return mem_leak_handle_allocated_buffer(buf, size, caller_slid);
}

static __INLINE void 
_kfree_tag (void *buffer)
{
  void *buf = mem_leak_handle_buffer_to_free(buffer);
  kfree(buf);
}

#define kmalloc_tag(size, gfp, tag) _kmalloc_tag((size), (gfp), (tag), MTLK_SLID)
#define kfree_tag(buffer)           _kfree_tag(buffer)

static __INLINE void *
_vmalloc_tag (size_t size, unsigned int tag, mtlk_slid_t caller_slid)
{
  void *buf = kmalloc(mem_leak_get_full_allocation_size(size), GFP_KERNEL);
  return mem_leak_handle_allocated_buffer(buf, size, caller_slid);
}

static __INLINE void
_vfree_tag (void *buffer)
{
  void *buf = mem_leak_handle_buffer_to_free(buffer);
  kfree(buf);
}

#define vmalloc_tag(size, tag)      _vmalloc_tag((size), (tag), MTLK_SLID)
#define vfree_tag(buffer)           _vfree_tag(buffer)

#else /* MTCFG_ENABLE_OBJPOOL */

void *__mtlk_kmalloc(size_t size, int flags);
void  __mtlk_kfree(void *p);

#define kmalloc_tag(a, b, c) __mtlk_kmalloc((a), (b))
#define vmalloc_tag(a, b)    vmalloc((a))
#define kfree_tag            __mtlk_kfree
#define vfree_tag            vfree

#endif /* MTCFG_ENABLE_OBJPOOL */

#endif /* __MEM_DEFS_H__ */
