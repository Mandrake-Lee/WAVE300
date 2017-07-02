/*
 * $Id: mtlk_serializer_osdep.c 11579 2011-08-29 14:31:05Z nayshtut $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Linux dependent serializer part
 *
 */

#include "mtlkinc.h"
#include "mtlk_serializer_osdep.h"

#define LOG_LOCAL_GID   GID_SERIALIZER
#define LOG_LOCAL_FID   0

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,4)

MTLK_INIT_STEPS_LIST_BEGIN(szr_osdep)
  MTLK_INIT_STEPS_LIST_ENTRY(szr_osdep, EVT_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(szr_osdep)
MTLK_INIT_STEPS_LIST_END(szr_osdep);

MTLK_START_STEPS_LIST_BEGIN(szr_osdep)
  MTLK_START_STEPS_LIST_ENTRY(szr_osdep, THREAD_START)
MTLK_START_INNER_STEPS_BEGIN(szr_osdep)
MTLK_START_STEPS_LIST_END(szr_osdep);

static int
thread(void *data)
{
  mtlk_serializer_osdep_t *osdep = (mtlk_serializer_osdep_t*)data;

  MTLK_ASSERT(NULL != osdep);
  MTLK_ASSERT(NULL != osdep->szr_clb);

  while (TRUE) {
    mtlk_osal_event_wait(&osdep->new_event, MTLK_OSAL_EVENT_INFINITE);
    if (__UNLIKELY(osdep->stop)) {
      break;
    }
    mtlk_osal_event_reset(&osdep->new_event);

    osdep->szr_clb(osdep->szr_clb_ctx);
  }

  while (!kthread_should_stop()) {
    schedule();
  }

  return 0;
}

int __MTLK_IFUNC
_mtlk_serializer_osdep_start(mtlk_serializer_osdep_t* osdep, mtlk_szr_clb_t szr_clb,
                             mtlk_handle_t szr_clb_ctx)
{
  MTLK_ASSERT(osdep != NULL);
  MTLK_ASSERT(szr_clb != NULL);

  osdep->stop = FALSE;
  mtlk_osal_event_reset(&osdep->new_event);
  osdep->szr_clb     = szr_clb;
  osdep->szr_clb_ctx = szr_clb_ctx;

  MTLK_START_TRY(szr_osdep, MTLK_OBJ_PTR(osdep))
    MTLK_START_STEP_EX(szr_osdep, THREAD_START, MTLK_OBJ_PTR(osdep),
                       kthread_run, (thread, (void*)osdep, "mtlk"),
                       osdep->thread, osdep->thread != NULL, MTLK_ERR_NO_RESOURCES)
  MTLK_START_FINALLY(szr_osdep, MTLK_OBJ_PTR(osdep))
  MTLK_START_RETURN(szr_osdep, MTLK_OBJ_PTR(osdep), _mtlk_serializer_osdep_stop, (osdep))
}

void __MTLK_IFUNC
_mtlk_serializer_osdep_stop(mtlk_serializer_osdep_t *osdep)
{
  MTLK_ASSERT(osdep != NULL);

  osdep->stop = TRUE;
  _mtlk_serializer_osdep_notify(osdep);

  MTLK_STOP_BEGIN(szr_osdep, MTLK_OBJ_PTR(osdep))
    MTLK_STOP_STEP(szr_osdep, THREAD_START, MTLK_OBJ_PTR(osdep),
                   kthread_stop, (osdep->thread))
  MTLK_STOP_END(szr_osdep, MTLK_OBJ_PTR(osdep))
}

void __MTLK_IFUNC
_mtlk_serializer_osdep_cleanup (mtlk_serializer_osdep_t *osdep)
{
  MTLK_ASSERT(osdep != NULL);

  MTLK_CLEANUP_BEGIN(szr_osdep, MTLK_OBJ_PTR(osdep))
    MTLK_CLEANUP_STEP(szr_osdep, EVT_INIT, MTLK_OBJ_PTR(osdep),
                      mtlk_osal_event_cleanup, (&osdep->new_event))
  MTLK_CLEANUP_END(szr_osdep, MTLK_OBJ_PTR(osdep))
}

int __MTLK_IFUNC
_mtlk_serializer_osdep_init (mtlk_serializer_osdep_t *osdep)
{
  MTLK_ASSERT(osdep != NULL);

  memset(osdep, 0, sizeof(*osdep));

  MTLK_INIT_TRY(szr_osdep, MTLK_OBJ_PTR(osdep))
    MTLK_INIT_STEP(szr_osdep, EVT_INIT, MTLK_OBJ_PTR(osdep),
                   mtlk_osal_event_init, (&osdep->new_event))
  MTLK_INIT_FINALLY(szr_osdep, MTLK_OBJ_PTR(osdep))
  MTLK_INIT_RETURN(szr_osdep, MTLK_OBJ_PTR(osdep), _mtlk_serializer_osdep_cleanup, (osdep))
}

void __MTLK_IFUNC
_mtlk_serializer_osdep_notify (mtlk_serializer_osdep_t *osdep)
{
  MTLK_ASSERT(osdep != NULL);

  mtlk_osal_event_set(&osdep->new_event);
}

#else

MTLK_INIT_STEPS_LIST_BEGIN(szr_osdep)
MTLK_INIT_INNER_STEPS_BEGIN(szr_osdep)
MTLK_INIT_STEPS_LIST_END(szr_osdep);

MTLK_START_STEPS_LIST_BEGIN(szr_osdep)
  MTLK_START_STEPS_LIST_ENTRY(szr_osdep, WORK_START)
MTLK_START_INNER_STEPS_BEGIN(szr_osdep)
MTLK_START_STEPS_LIST_END(szr_osdep);

static void 
work_handler(void *data)
{
  mtlk_serializer_osdep_t *osdep = (mtlk_serializer_osdep_t*)data;

  osdep->szr_clb(osdep->szr_clb_ctx);
}

int __MTLK_IFUNC
_mtlk_serializer_osdep_init (mtlk_serializer_osdep_t *osdep)
{
  MTLK_ASSERT(osdep != NULL);

  MTLK_INIT_TRY(szr_osdep, MTLK_OBJ_PTR(osdep))

  MTLK_INIT_FINALLY(szr_osdep, MTLK_OBJ_PTR(osdep))
  MTLK_INIT_RETURN(szr_osdep, MTLK_OBJ_PTR(osdep), _mtlk_serializer_osdep_cleanup, (osdep))
}

int __MTLK_IFUNC
_mtlk_serializer_osdep_start(mtlk_serializer_osdep_t* osdep, mtlk_szr_clb_t szr_clb,
                             mtlk_handle_t szr_clb_ctx)
{
  MTLK_ASSERT(osdep != NULL);
  MTLK_ASSERT(szr_clb != NULL);

  osdep->szr_clb     = szr_clb;
  osdep->szr_clb_ctx = szr_clb_ctx;

  MTLK_START_TRY(szr_osdep, MTLK_OBJ_PTR(osdep))
    MTLK_START_STEP_VOID(szr_osdep, WORK_START, MTLK_OBJ_PTR(osdep),
                         INIT_WORK, (&osdep->work, work_handler, osdep));
  MTLK_START_FINALLY(szr_osdep, MTLK_OBJ_PTR(osdep))
  MTLK_START_RETURN(szr_osdep, MTLK_OBJ_PTR(osdep), _mtlk_serializer_osdep_stop, (osdep))
}

void __MTLK_IFUNC
_mtlk_serializer_osdep_stop(mtlk_serializer_osdep_t *osdep)
{
  MTLK_ASSERT(osdep != NULL);

  MTLK_STOP_BEGIN(szr_osdep, MTLK_OBJ_PTR(osdep))
    MTLK_STOP_STEP(szr_osdep, WORK_START, MTLK_OBJ_PTR(osdep),
                   cancel_delayed_work, (&osdep->work))
  MTLK_STOP_END(szr_osdep, MTLK_OBJ_PTR(osdep))
}

void __MTLK_IFUNC
_mtlk_serializer_osdep_cleanup(mtlk_serializer_osdep_t *osdep)
{
  MTLK_ASSERT(osdep != NULL);

  MTLK_CLEANUP_BEGIN(szr_osdep, MTLK_OBJ_PTR(osdep))

  MTLK_CLEANUP_END(szr_osdep, MTLK_OBJ_PTR(osdep))
}

void __MTLK_IFUNC
_mtlk_serializer_osdep_notify (mtlk_serializer_osdep_t *osdep)
{
  MTLK_ASSERT(osdep != NULL);

  // work will not be scheduled by kernel if it's already scheduled, 
  // so no extra checks are required
  schedule_work(&osdep->work);
}

#endif

