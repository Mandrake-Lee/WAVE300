/*
 * $Id: mtlk_clipboard.h 10052 2010-12-01 16:43:51Z dmytrof $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Clipboard between Core and DF
 *
 * Originally written by Andrii Tseglytskyi
 *
 */

#ifndef _MTLK_CLIPBOARD_H_
#define _MTLK_CLIPBOARD_H_

/* "clipboard" between DF UI and Core */
typedef struct _mtlk_clpb_t mtlk_clpb_t;

mtlk_clpb_t* __MTLK_IFUNC
mtlk_clpb_create(void);

void __MTLK_IFUNC
mtlk_clpb_delete(mtlk_clpb_t *clpb);

void __MTLK_IFUNC
mtlk_clpb_purge(mtlk_clpb_t *clpb);

int __MTLK_IFUNC
mtlk_clpb_push(mtlk_clpb_t *clpb, const void *data, uint32 size);

void __MTLK_IFUNC
mtlk_clpb_enum_rewind(mtlk_clpb_t *clpb);

void* __MTLK_IFUNC
mtlk_clpb_enum_get_next(mtlk_clpb_t *clpb, uint32* size);

uint32 __MTLK_IFUNC
mtlk_clpb_get_num_of_elements(mtlk_clpb_t *clpb);

#endif /* _MTLK_CLIPBOARD_H_ */
