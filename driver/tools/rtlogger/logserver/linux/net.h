/*
 * Copyright (c) 2007-2008 Metalink Broadband (Israel)
 *
 * Networking
 *
 * Written by: Andrey Fidrya
 *
 */

#ifndef __NET_H__
#define __NET_H__

#include "compat.h"

#define MAX_SND_BUF            8192
#define MAX_RCV_BUF            8192

#define SOCKET_SET_OPT(s, name, val) \
  socket_set_opt(s, name, #name, val)

int create_mother_socket(const char *addr, uint16 port, int *psocket);
int socket_set_opt(int s, int opt, const char *opt_name, int val);
int socket_set_linger(int s, int onoff, int linger);
int socket_set_nonblock(int s, int nonblock);

#endif // !__NET_H__

