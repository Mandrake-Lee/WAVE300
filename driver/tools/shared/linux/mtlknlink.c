#include "mtlkinc.h"

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <linux/if.h>

#include "mtlkinc.h"
#include "nl.h"
#include "mtlknlink.h"

#define LOG_LOCAL_GID   GID_MTLKNLINK
#define LOG_LOCAL_FID   1

enum _RECEIVE_EVENT
{
  EVENT_DATA_ARRIVED = 0,
  EVENT_STOP_ARRIVED,
};

#define MAX_PAYLOAD 1024  /* maximum payload size*/

static int
nlink_wait_events(int socket_fd, int stop_fd)
{
  fd_set read_fds;
  int res = 0;

  FD_ZERO(&read_fds);
  FD_SET(stop_fd, &read_fds);
  FD_SET(socket_fd, &read_fds);

  for(;;)
  {
    res = select(MAX(socket_fd, stop_fd) + 1, &read_fds, NULL, NULL, NULL);
    if (res == EINTR) {
      continue; /* select returned because of signal */
    }
    else if (res <= 0) {
      ELOG_SD("select call failed with error: %s (%d)", strerror(errno), errno);
      return -errno; /* select failed */
    }
    else if (FD_ISSET(stop_fd, &read_fds)) {
      return EVENT_STOP_ARRIVED; /* stop signal received */
    }
    else if (FD_ISSET(socket_fd, &read_fds)) {
      return EVENT_DATA_ARRIVED; /* socket contains data */
    }
    WLOG_V("select returned without a reason");
  }
}

static int
nlink_parse_msg(mtlk_nlink_socket_t* nlink_socket, struct mtlk_nl_msghdr *phdr)
{
  static const char mtlk_fingerprint[] = { 'm', 't', 'l', 'k' };
  static const char supported_protocol_version = 1;

  /* Silently ignore packets from other applications */
  MTLK_ASSERT(sizeof(phdr->fingerprint) == sizeof(mtlk_fingerprint));
  if (memcmp(phdr->fingerprint, mtlk_fingerprint, sizeof(mtlk_fingerprint))) {
    return 0;
  }

  /* TODO: Temporary while not everything is implemented via IRB */
  /* Silently ignore non-IRBM packets */
  if (phdr->cmd_id != NL_DRV_IRBM_NOTIFY) {
    return 0;
  }

  if (phdr->proto_ver != supported_protocol_version) {
    ELOG_DD("Version mismatch: got %u, expected %u",
      phdr->proto_ver, supported_protocol_version);
    return 0;
  }

  nlink_socket->receive_callback(nlink_socket->receive_callback_ctx, (void *) (phdr + 1));
  return 0;
}

#ifdef MTCFG_USE_GENL

/* this function is registered as a callback for
* all "valid" messages received on the socket.
* it parses the generic netlink message and calls
* local parser.
*/
static int parse_nl_cb(struct nl_msg *msg, void *arg)
{
  static struct nla_policy mtlk_genl_policy[MTLK_GENL_ATTR_MAX + 1] = 
  {
    [MTLK_GENL_ATTR_EVENT] = { .type = NLA_UNSPEC, 
                               .minlen = sizeof(struct mtlk_nl_msghdr), 
                               .maxlen = 0 },
  };

  struct nlmsghdr *nlh = nlmsg_hdr(msg);
  struct nlattr *attrs[MTLK_GENL_ATTR_MAX+1];
  mtlk_nlink_socket_t* nlink_socket = (mtlk_nlink_socket_t*) arg;

  /* if not our family - do nothing */
  if (nlh->nlmsg_type != nlink_socket->family)
    return NL_SKIP;

  /* Validate message and parse attributes */
  if (genlmsg_parse(nlh, 0, attrs, MTLK_GENL_ATTR_MAX, mtlk_genl_policy) < 0)
    return NL_SKIP;

  /* Call the mtlk message parsing function */
  if (attrs[MTLK_GENL_ATTR_EVENT])
    nlink_parse_msg(nlink_socket,
      (struct mtlk_nl_msghdr *) nla_data(attrs[MTLK_GENL_ATTR_EVENT]));

  return NL_OK;
}

int __MTLK_IFUNC
mtlk_nlink_create(mtlk_nlink_socket_t* nlink_socket, 
                  mtlk_netlink_callback_t receive_callback, void* receive_callback_ctx)
{
  int res = 0;
  int irb_broadcast_group;

  MTLK_ASSERT(NULL != nlink_socket);

  /* Register callback */
  nlink_socket->receive_callback = receive_callback;
  nlink_socket->receive_callback_ctx = receive_callback_ctx;

  /* Allocate a new netlink socket */
  nlink_socket->sock = nl_socket_alloc();
  if (NULL == nlink_socket->sock) {
    ELOG_V("Generic netlink socket allocation failed");
    res = -1;
    goto end;
  }

  /* Connect to generic netlink socket on kernel side */
  if (genl_connect(nlink_socket->sock) < 0) {
    ELOG_V("Connect to generic netlink controller failed");
    res = -1;
    goto err_dealloc;
  }

  /* Ask kernel to resolve family name to family id */
  nlink_socket->family = genl_ctrl_resolve(nlink_socket->sock, MTLK_GENL_FAMILY_NAME);
  if (nlink_socket->family < 0) {
    ELOG_V("Cannot get Generic Netlink family identifier.");
    res = -1;
    goto err_dealloc;
  }

  /* use family id as the base for broadcast group */
  irb_broadcast_group = nlink_socket->family + NETLINK_IRBM_GROUP - 1;

  /* register to receive messsages from interested group */
  if (nl_socket_add_membership(nlink_socket->sock, irb_broadcast_group) < 0) {
    ELOG_D("Cannot add membership in %d group.", irb_broadcast_group);
    res = -1;
    goto err_dealloc;
  }

  /* This socket have to process all messages and not 
     only explicitly requested as it is should be in 
     event processing */
  nl_socket_disable_seq_check(nlink_socket->sock);

  /* set callback for all valid messages */
  nl_socket_modify_cb(nlink_socket->sock, NL_CB_VALID, 
    NL_CB_CUSTOM, parse_nl_cb, nlink_socket);

  goto end;

err_dealloc:
  nl_close(nlink_socket->sock);
  nl_socket_free(nlink_socket->sock);
end:
  return res;
}

int __MTLK_IFUNC
mtlk_nlink_receive_loop(mtlk_nlink_socket_t* nlink_socket, int stop_fd)
{
  int res = 0;

  MTLK_ASSERT(NULL != nlink_socket);
  
  /* Read message from kernel */
  for (;;) {
    int wait_results = nlink_wait_events(nl_socket_get_fd(nlink_socket->sock), stop_fd);
    if(EVENT_DATA_ARRIVED == wait_results) {
      res = nl_recvmsgs_default(nlink_socket->sock);
      if (res < 0)
        return res;
    }
    else if(EVENT_STOP_ARRIVED == wait_results) {
      return 0;
    }
    else {
      ELOG_D("Wait for data failed (%d)", wait_results);
      return wait_results;
    }
  }

  MTLK_ASSERT(!"Should never be here");
}

void __MTLK_IFUNC
mtlk_nlink_cleanup(mtlk_nlink_socket_t* nlink_socket)
{
  MTLK_ASSERT(NULL != nlink_socket);
  nl_close(nlink_socket->sock);
  nl_socket_free(nlink_socket->sock);
}

#else /* MTCFG_USE_GENL */

#define NETLINK_MTLK_DRV  NETLINK_USERSOCK 

int __MTLK_IFUNC
mtlk_nlink_create(mtlk_nlink_socket_t* nlink_socket, 
                  mtlk_netlink_callback_t receive_callback, void* callback_ctx)
{
  int res = 0;
  MTLK_ASSERT(NULL != nlink_socket);

  /* Register callback */
  nlink_socket->receive_callback = receive_callback;
  nlink_socket->receive_callback_ctx = callback_ctx;

  nlink_socket->sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_MTLK_DRV);
  if (-1 == nlink_socket->sock_fd) {
    res = -errno;
    ELOG_SD("Failed to open netlink socket: %s (%d)", strerror(errno), errno);
    goto end;
  }

  /* Fill netlink source address */
  memset(&nlink_socket->src_addr, 0, sizeof(nlink_socket->src_addr));
  nlink_socket->src_addr.nl_family = AF_NETLINK;
  nlink_socket->src_addr.nl_pid = getpid();
  nlink_socket->src_addr.nl_groups = 1 << (NETLINK_IRBM_GROUP - 1);

  /* Bind the new socket */
  if (-1 == bind(nlink_socket->sock_fd, (struct sockaddr *) &nlink_socket->src_addr, 
                 sizeof(nlink_socket->src_addr))) {
    res = -errno;
    ELOG_SD("Failed to bind netlink socket: %s (%d)", strerror(errno), errno);
    goto err_dealloc;
  }

  goto end;

err_dealloc:
  close(nlink_socket->sock_fd);
end:
  return res;
}

int __MTLK_IFUNC
mtlk_nlink_receive_loop(mtlk_nlink_socket_t* nlink_socket, int stop_fd)
{
  struct msghdr msg;
  struct iovec iov;
  struct sockaddr_nl dest_addr = {0};
  struct nlmsghdr *nlh = NULL;
  int res = 0;

  MTLK_ASSERT(NULL != nlink_socket);

  nlh = (struct nlmsghdr *) malloc(NLMSG_SPACE(MAX_PAYLOAD));
  if(NULL == nlh) {
    ELOG_V("Receive buffer allocation failed");
    return -ENOMEM;
  }

  memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));

  memset(&msg, 0, sizeof(msg));
  iov.iov_base = (void *)nlh;
  iov.iov_len = NLMSG_SPACE(MAX_PAYLOAD);
  msg.msg_name = (void *)&dest_addr;
  msg.msg_namelen = sizeof(dest_addr);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  /* Read message from kernel */
  for (;;) {
    int wait_results = nlink_wait_events(nlink_socket->sock_fd, stop_fd);

    if(EVENT_DATA_ARRIVED == wait_results) {
        int recv_res = recvmsg(nlink_socket->sock_fd, &msg, 0);
        switch(recv_res) {
        case 0:
          res = 0;
          ELOG_V("Netlink peer performed shutdown");
          goto end; /* Peer performed shutdown */
        case -1: {
            res = -errno;
            ELOG_SD("Receive message failed: %s (%d)", strerror(errno), errno);
            goto end;
          }
        default: {
            if(recv_res >= sizeof(struct mtlk_nl_msghdr))
              nlink_parse_msg(nlink_socket, (struct mtlk_nl_msghdr *) NLMSG_DATA(nlh));
            continue;
          }
        }
    }
    else if(EVENT_STOP_ARRIVED == wait_results) {
      res = 0;
      goto end;
    }
    else {
      ELOG_D("Wait for data failed (%d)", wait_results);
      res = wait_results;
      goto end;
    }
  }

  MTLK_ASSERT(!"Should never be here");

end:
  free(nlh);
  return res;
}

void __MTLK_IFUNC
mtlk_nlink_cleanup(mtlk_nlink_socket_t* nlink_socket)
{
  MTLK_ASSERT(NULL != nlink_socket);
  close(nlink_socket->sock_fd);
}

#endif /* MTCFG_USE_GENL */
