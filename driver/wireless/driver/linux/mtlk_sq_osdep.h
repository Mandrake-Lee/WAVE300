/*
 * $Id: mtlk_sq_osdep.h 10052 2010-12-01 16:43:51Z dmytrof $
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *  
 * Linux dependant send queue parts.
 */

#ifndef __MTLK_SQ_OSDEP_H__
#define __MTLK_SQ_OSDEP_H__

/* functions called from shared SendQueue code */
int  _mtlk_sq_send_to_hw(mtlk_df_t *df, mtlk_nbuf_t *nbuf, uint16 prio);

#endif /* __MTLK_SQ_OSDEP_H__ */
