/*
 * $Id: nlmsgs.h 11167 2011-05-25 11:10:20Z Strashko $
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 * Subsystem providing communication with userspace over
 * NETLINK_USERSOCK netlink protocol.
 *
 */

#ifndef __NLMSGS_H__
#define __NLMSGS_H__

#include "nl.h"

#define LOG_LOCAL_GID   GID_NLMSGS
#define LOG_LOCAL_FID   0

int mtlk_nl_init(void);
void mtlk_nl_cleanup(void);
int mtlk_nl_send_brd_msg(void *data, int length, gfp_t flags, u32 dst_group, u8 cmd); 

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif /* __NLMSGS_H__ */
