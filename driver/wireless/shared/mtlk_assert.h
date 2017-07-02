#ifndef __MTLK_ASSERT_H__
#define __MTLK_ASSERT_H__

#ifdef MTCFG_DEBUG

void __MTLK_IFUNC
__mtlk_assert(mtlk_slid_t slid);

#define __MTLK_ASSERT(expr, slid)  \
  do {                             \
    if (__UNLIKELY(!(expr))) {     \
      __mtlk_assert(slid);         \
    }                              \
  } while (0)

#else
#define __MTLK_ASSERT(expr, slid)
#endif

#define ASSERT(expr)      __MTLK_ASSERT(expr, MTLK_SLID)
#define MTLK_ASSERT(expr) __MTLK_ASSERT(expr, MTLK_SLID)

#endif /* __MTLK_ASSERT_H__ */

