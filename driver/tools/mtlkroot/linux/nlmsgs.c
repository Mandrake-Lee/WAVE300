/*
 * $Id: nlmsgs.c 11887 2011-11-02 16:02:20Z nayshtut $
 *
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 * Subsystem providing communication with userspace over
 * NETLINK_USERSOCK netlink protocol.
 *
 */
#include "mtlkinc.h"

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/netlink.h>

#ifdef MTCFG_USE_GENL
#include <net/genetlink.h>
#endif

#include "nl.h"

#define LOG_LOCAL_GID   GID_NLMSGS
#define LOG_LOCAL_FID   1

#ifdef MTCFG_USE_GENL

/* family structure */
static struct genl_family mtlk_genl_family = {
        .id = GENL_ID_GENERATE,
        .name = MTLK_GENL_FAMILY_NAME,
        .version = MTLK_GENL_FAMILY_VERSION,
        .maxattr = MTLK_GENL_ATTR_MAX,
};

#else /* MTCFG_USE_GENL */

struct sock *nl_sock;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
DEFINE_MUTEX(nl_mutex);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
static void nl_input (struct sock *sk, int len)
{
  struct sk_buff *skb;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
  while ((skb = skb_dequeue(&sk->receive_queue)) != NULL) {
#else
  while ((skb = skb_dequeue(&sk->sk_receive_queue)) != NULL) {
#endif 
    kfree(skb);
  }

}
#else /* kernel version >=  2.6.24 */
static void nl_input (struct sk_buff *inskb) {}
#endif

#endif /* MTCFG_USE_GENL */

int mtlk_nl_send_brd_msg(void *data, int length, gfp_t flags, u32 dst_group, u8 cmd);

#ifndef MTCFG_USE_GENL

int mtlk_nl_send_brd_msg(void *data, int length, gfp_t flags, u32 dst_group, u8 cmd)
{
  struct sk_buff *skb = NULL;
  struct nlmsghdr *nlh;
  struct mtlk_nl_msghdr *mhdr;
  int full_len = length + sizeof(*mhdr);

  /* no socket - no messages */
  if (!nl_sock)
    return MTLK_ERR_UNKNOWN;

  skb = alloc_skb(NLMSG_SPACE(full_len), flags);
  if (skb == NULL)
    return MTLK_ERR_NO_MEM;

  nlh = (struct nlmsghdr*) skb->data;
  nlh->nlmsg_len = NLMSG_SPACE(full_len);
  nlh->nlmsg_pid = 0;
  nlh->nlmsg_flags = 0;

  /* fill the message header */

  skb_put(skb, NLMSG_SPACE(full_len));

  mhdr = (struct mtlk_nl_msghdr*) (NLMSG_DATA(nlh));
  memcpy(mhdr->fingerprint, "mtlk", 4);
  mhdr->proto_ver = 1;
  mhdr->cmd_id = cmd;
  mhdr->data_len = length;

  memcpy((char *)mhdr + sizeof(*mhdr), data, length);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
  NETLINK_CB(skb).dst_groups = dst_group;
#else
  NETLINK_CB(skb).dst_group = dst_group;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
  NETLINK_CB(skb).pid = 0;/* from kernel */
#else
  NETLINK_CB(skb).portid = 0;
#endif
  
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
  /* void return value in 2.4 kernels */
  netlink_broadcast(nl_sock, skb, 0, dst_group, flags);
#else
  if (netlink_broadcast(nl_sock, skb, 0, dst_group, flags))
    return MTLK_ERR_UNKNOWN;
#endif
  
  return MTLK_ERR_OK;
}


int mtlk_nl_init(void)
{
  /* netlink groups that are expected:
   * 1 - is joined by hostapd, wpa_supplicant and drvhlpr
   * 2 - is joined by wsccmd (Simple Config)
   * 3 - is joined by IRB media
   * so, total number of 3 groups must be told to kernel
   * when creating socket
   */
   
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
  nl_sock = netlink_kernel_create(NETLINK_USERSOCK, nl_input);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
  nl_sock = netlink_kernel_create(NETLINK_USERSOCK, 3, nl_input, THIS_MODULE);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
  nl_sock = netlink_kernel_create(NETLINK_USERSOCK, 3, nl_input, 
                                                       &nl_mutex, THIS_MODULE);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
  nl_sock = netlink_kernel_create(&init_net, NETLINK_USERSOCK, 3, nl_input,
                                                       &nl_mutex, THIS_MODULE);
#else
	struct netlink_kernel_cfg cfg = {
    	.input = nl_input,
	.cb_mutex = &nl_mutex,
	};
  nl_sock = netlink_kernel_create(&init_net, NETLINK_USERSOCK, &cfg);
#endif

  if (!nl_sock) {
    return MTLK_ERR_UNKNOWN;
  }
  return MTLK_ERR_OK;
}

void mtlk_nl_cleanup(void)
{
  if(nl_sock)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    sock_release(nl_sock->socket);  
#else
    sock_release(nl_sock->sk_socket);
#endif
}


#else /* MTCFG_USE_GENL */

int mtlk_nl_send_brd_msg(void *data, int length, gfp_t flags, u32 dst_group, u8 cmd)
{
  struct sk_buff *skb;
  struct nlattr *attr;
  void *msg_header;
  int size, genl_res, send_group;
  int res = MTLK_ERR_UNKNOWN;
  struct mtlk_nl_msghdr *mhdr;
  int full_len = length + sizeof(*mhdr);

  /* allocate memory */
  size = nla_total_size(full_len);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
  skb = nlmsg_new(NLMSG_ALIGN (GENL_HDRLEN + size));
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
  skb = nlmsg_new(genlmsg_total_size(size), flags);
#else
  skb = genlmsg_new(size, flags);
#endif
  if (!skb)
    return MTLK_ERR_NO_MEM;

  /* add the genetlink message header */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
  msg_header = genlmsg_put(skb, 0, 0, mtlk_genl_family.id, 0, 0, 
                           MTLK_GENL_CMD_EVENT, mtlk_genl_family.version);
#else
  msg_header = genlmsg_put(skb, 0, 0, &mtlk_genl_family, 0, MTLK_GENL_CMD_EVENT);
#endif
  if (!msg_header) 
    goto out_free_skb;

  /* fill the data */
  attr = nla_reserve(skb, MTLK_GENL_ATTR_EVENT, full_len);
  if (!attr)
    goto out_free_skb;
    
  mhdr = (struct mtlk_nl_msghdr*) (nla_data(attr));
  memcpy(mhdr->fingerprint, "mtlk", 4);
  mhdr->proto_ver = 1;
  mhdr->cmd_id = cmd;
  mhdr->data_len = length;

  memcpy((char *)mhdr + sizeof(*mhdr), data, length);

  /* send multicast genetlink message */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0)
  genl_res = genlmsg_end(skb, msg_header);
  if (genl_res < 0)
    goto out_free_skb;
#else
	genlmsg_end(skb, msg_header);
#endif
  /* the group to broadcast on is calculated on base of family id */
  send_group = mtlk_genl_family.id + dst_group - 1;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
  genl_res = genlmsg_multicast(skb, 0, send_group);
#else
	/*Might be needed just to overcome kernek 3.18*/
  genl_res = genlmsg_multicast(NULL,skb, 0, send_group, flags);
#endif
  if (genl_res)
    return MTLK_ERR_UNKNOWN;
  else 
    return MTLK_ERR_OK;

out_free_skb:
  nlmsg_free(skb);
  return res;
}

int mtlk_nl_init(void)
{
  int result;

  result = genl_register_family(&mtlk_genl_family);
  if (result) {
    return MTLK_ERR_UNKNOWN;
  }

  return MTLK_ERR_OK;
}

void mtlk_nl_cleanup(void)
{
  genl_unregister_family(&mtlk_genl_family);
}


#endif /* MTCFG_USE_GENL */

EXPORT_SYMBOL(mtlk_nl_send_brd_msg);

