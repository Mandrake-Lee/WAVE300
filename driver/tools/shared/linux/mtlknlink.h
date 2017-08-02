#ifndef __MTLK_NLINK_H__
#define __MTLK_NLINK_H__

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef void (*mtlk_netlink_callback_t)(void* ctx, void* data);

#ifdef MTCFG_USE_GENL

#include <linux/socket.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/socket.h>

typedef struct _mtlk_nlink_socket_t
{
  int family;
  struct nl_sock *sock;
  mtlk_netlink_callback_t receive_callback;
  void* receive_callback_ctx;
} mtlk_nlink_socket_t;

//TODO: Move this to .c file

#define MTLK_GENL_FAMILY_NAME    "MTLK_WLS"

#else /* MTCFG_USE_GENL */

#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>

typedef struct _mtlk_nlink_socket_t
{
  int sock_fd;
  struct sockaddr_nl src_addr;
  mtlk_netlink_callback_t receive_callback;
  void* receive_callback_ctx;
} mtlk_nlink_socket_t;

#endif /* MTCFG_USE_GENL */

int __MTLK_IFUNC
mtlk_nlink_create(mtlk_nlink_socket_t* nlink_socket, 
                  mtlk_netlink_callback_t receive_callback, void* callback_ctx);

int __MTLK_IFUNC
mtlk_nlink_receive_loop(mtlk_nlink_socket_t* nlink_socket, int stop_fd);

void __MTLK_IFUNC
mtlk_nlink_cleanup(mtlk_nlink_socket_t* nlink_socket);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_NLINK_H__ */
