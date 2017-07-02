/*
 * $Id: mtlk_serializer_osdep.h 9431 2010-08-09 12:04:37Z dmytrof $
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *  
 * Linux dependent serializer part
 *
 */

#ifndef __MTLK_SERIALIZER_OSDEP_H__
#define __MTLK_SERIALIZER_OSDEP_H__

#include "mtlk_osal.h"
#include "mtlkstartup.h"

#define  MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef void (*mtlk_szr_clb_t)(mtlk_handle_t ctx);

typedef struct _mtlk_serializer_osdep_t
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,4)
  mtlk_osal_event_t       new_event;
  struct task_struct     *thread;
  volatile BOOL           stop;
#else
  struct work_struct      work;
#endif
  mtlk_szr_clb_t          szr_clb;
  mtlk_handle_t           szr_clb_ctx;

  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
} __MTLK_IDATA mtlk_serializer_osdep_t;

int  __MTLK_IFUNC _mtlk_serializer_osdep_init(mtlk_serializer_osdep_t *osdep);
int  __MTLK_IFUNC _mtlk_serializer_osdep_start(mtlk_serializer_osdep_t* osdep,
                                               mtlk_szr_clb_t szr_clb,
                                               mtlk_handle_t szr_clb_ctx);
void __MTLK_IFUNC _mtlk_serializer_osdep_notify(mtlk_serializer_osdep_t *osdep);
void __MTLK_IFUNC _mtlk_serializer_osdep_stop(mtlk_serializer_osdep_t *osdep);
void __MTLK_IFUNC _mtlk_serializer_osdep_cleanup(mtlk_serializer_osdep_t *osdep);

#define  MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_SERIALIZER_OSDEP_H__ */
