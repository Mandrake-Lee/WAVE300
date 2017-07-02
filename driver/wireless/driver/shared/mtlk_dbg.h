#ifndef __MTLK_DBG_H__
#define __MTLK_DBG_H__

/************************************************************************
 * WARNING: this file contains CP debug related definitions.
 * WARNING: this file is not a part of release driver.
 * WARNING: These definitions may or may not exist on each 
 *          particular platform - so DO NOT rely on them.
 ************************************************************************/

#ifndef MTCFG_BENCHMARK_TOOLS
#error This file could be only used for benchmarks!
#endif

#include "mtlk_dbg_osdep.h"

static __INLINE int
mtlk_dbg_hres_init (void)
{
  MTLK_DBG_HRES_INIT();
}

static __INLINE void
mtlk_dbg_hres_cleanup (void)
{
  MTLK_DBG_HRES_CLEANUP();
}

static __INLINE void
mtlk_dbg_hres_ts (mtlk_dbg_hres_ts_t *ts)
{
  MTLK_DBG_HRES_TS(ts);
}

static __INLINE uint64
mtlk_dbg_hres_diff_uint64 (const mtlk_dbg_hres_ts_t *ts_later,
                           const mtlk_dbg_hres_ts_t *ts_earlier)
{
  MTLK_DBG_HRES_DIFF_UINT64(ts_later, ts_earlier);
}

static __INLINE uint64
mtlk_dbg_hres_us_to_uint64 (uint32 us)
{
  MTLK_DBG_HRES_US_TO_UINT64(us);
}

static __INLINE const char *
mtlk_dbg_hres_get_unit_name (void)
{
  MTLK_DBG_HRES_GET_UNIT_NAME();
}

#endif /* __MTLK_DBG_H__ */
