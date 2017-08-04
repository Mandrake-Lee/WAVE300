/*
 * $Id: mtlk_df_user.c 12952 2012-04-10 09:08:07Z bejs $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Driver framework implementation for Linux
 *
 */

#include "mtlkinc.h"
#include "mtlk_clipboard.h"
#include "mtlk_coreui.h"
#include "mtlk_df_user_priv.h"
#include "mtlk_df_priv.h"
#include "core.h"
#include "mtlk_snprintf.h"
#include "wpa.h"

#include <linux/wireless.h>
#include <linux/if_arp.h>
#include <net/iw_handler.h>
#include <asm/unaligned.h>
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
#include <net/ifx_ppa_api.h>
#include <net/ifx_ppa_hook.h>
#endif /* CONFIG_IFX_PPA_API_DIRECTPATH */

#include "dataex.h"
#ifdef MTCFG_IRB_DEBUG
#include "mtlk_irb_pinger.h"
#endif

#include "mtlkaux.h"
#include "mtlkwlanirbdefs.h"

#define LOG_LOCAL_GID   GID_DFUSER
#define LOG_LOCAL_FID   1

#define MTLK_NDEV_NAME  MTLK_IRB_VAP_NAME

#define _DF_STAT_POLL_PERIOD   (1000)
#define _DF_RTS_THRESHOLD_MIN 100
#define _DF_WAIT_FW_ASSERT    2000

#define DF_USER_DEFAULT_IWPRIV_LIM_VALUE (-1)

#define MTLK_NIP6(addr) \
        ntohs((addr).s6_addr16[0]), \
        ntohs((addr).s6_addr16[1]), \
        ntohs((addr).s6_addr16[2]), \
        ntohs((addr).s6_addr16[3]), \
        ntohs((addr).s6_addr16[4]), \
        ntohs((addr).s6_addr16[5]), \
        ntohs((addr).s6_addr16[6]), \
        ntohs((addr).s6_addr16[7])

#define MTLK_NIP6_FMT "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x"

#define MTLK_NIPQUAD(addr) \
        ((unsigned char *)&addr)[0], \
        ((unsigned char *)&addr)[1], \
        ((unsigned char *)&addr)[2], \
        ((unsigned char *)&addr)[3]

#define MTLK_NIPQUAD_FMT "%u.%u.%u.%u"

static const IEEE_ADDR EMPTY_MAC_MASK = { {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} };


/********************************************************************
 * IW driver IOCTL descriptor table
 ********************************************************************/
extern const struct iw_handler_def mtlk_linux_handler_def;


/*
 * These magic numbers we got from WEB team.
 * Should be replaced by meaningful protocol names.
 */
enum {
  NETWORK_MODE_11AN = 14,
  NETWORK_MODE_11A = 10,
  NETWORK_MODE_11N5 = 12,
  NETWORK_MODE_11BGN = 23,
  NETWORK_MODE_11BG = 19,
  NETWORK_MODE_11B = 17,
  NETWORK_MODE_11G = 18,
  NETWORK_MODE_11N2 = 20,
  NETWORK_MODE_11GN = 22,
  NETWORK_MODE_11ABGN = 30,
  NETWORK_MODE_11ABG = 0,
};

#define MAX_DF_UI_STAT_NAME_LENGTH 256
#define UM_DATA_BASE    0x80000000
#define UM_DATA_SIZE    0x0003fd00
#define LM_DATA_BASE    0x80080000
#define LM_DATA_SIZE    0x00017d00
#define SHRAM_DATA_BASE 0xa6000000
#define SHRAM_DATA_SIZE 0x00020000

/* slow context of DF user */
struct _mtlk_df_ui_slow_ctx_t
{
  /* Core status & statistic */
  mtlk_core_general_stats_t core_general_stats;

  struct iw_statistics    iw_stats;
  /* Network device statistics */
  struct net_device_stats linux_stats;

  /**** BCL data *****/
  uint32      *dbg_general_pkt_cnts;
  uint32      dbg_general_pkt_cnts_num;
  IEEE_ADDR   *dbg_general_pkt_addr;
  uint32      dbg_general_pkt_addr_num;
  uint32      *dbg_rr_cnts;
  uint32      dbg_rr_cnts_num;
  IEEE_ADDR   *dbg_rr_addr;
  uint32      dbg_rr_addr_num;
  /*******************/

  mtlk_osal_timer_t       stat_timer;

  mtlk_df_proc_fs_node_t  *proc_df_node;
  mtlk_df_proc_fs_node_t  *proc_df_debug_node;

#ifdef MTCFG_IRB_DEBUG
  mtlk_irb_pinger_t pinger;
#endif
};

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
typedef struct _mtlk_df_user_ppa_stats_t
{
  uint32 tx_processed;
  uint32 rx_accepted;
  uint32 rx_rejected;
} mtlk_df_user_ppa_stats_t;

typedef struct _mtlk_df_user_ppa_t
{
  PPA_DIRECTPATH_CB        clb;
  uint32                   if_id;
  mtlk_df_user_ppa_stats_t stats;
} mtlk_df_user_ppa_t;
#endif /* CONFIG_IFX_PPA_API_DIRECTPATH */

struct _mtlk_df_user_t
{
  struct _mtlk_df_t *df;
  struct net_device *dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
  struct net_device_ops dev_ops;
#endif

  struct _mtlk_df_ui_slow_ctx_t *slow_ctx;

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  mtlk_df_user_ppa_t ppa;
#endif
  mtlk_osal_event_t    fw_hang_evts[NUM_OF_MIPS];


  MTLK_DECLARE_INIT_LOOP(FW_HANG_EVTs);
  MTLK_DECLARE_INIT_LOOP(PROC_INIT);
  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
};

/********************************************************************
 * Tools API definitions
 ********************************************************************/
static uint8 __MTLK_IFUNC
_net_mode_ingress_filter (uint8 ingress_net_mode);
static uint8 __MTLK_IFUNC
_net_mode_egress_filter (uint8 egress_net_mode);

static void
mtlk_df_debug_bcl_cleanup(mtlk_df_user_t *df_user);
static int
mtlk_df_debug_bcl_init(mtlk_df_user_t *df_user);


/*****************************************************************************
**
** NAME         mtlk_df_traffic_classifier_register / mtlk_df_traffic_classifier_unregister
**
** DESCRIPTION  This functions are used for registration of External Metalink's
**              traffic classifier module
**
******************************************************************************/
int mtlk_df_traffic_classifier_register(void * classify_fn);
void mtlk_df_traffic_classifier_unregister(void);
EXPORT_SYMBOL(mtlk_df_traffic_classifier_register);
EXPORT_SYMBOL(mtlk_df_traffic_classifier_unregister);

int mtlk_df_traffic_classifier_register (void * classify_fn)
{
  mtlk_qos_classifier_register((mtlk_qos_do_classify_f)classify_fn);
  return 0;  /*mtlk_err_to_linux_err(res)*/
}


void mtlk_df_traffic_classifier_unregister (void)
{
  mtlk_qos_classifier_unregister();
}

/*****************************************************************************
 *****************************************************************************/
static __INLINE uint32 
_mtlk_df_user_get_core_slave_vap_index_by_iwpriv_param (uint32 iwpriv_slave_vap_index)
{
  /* DF iwpriv commands use 0-based VAP index for Slave VAPs, while Core 
   * uses 0-based numeration for *all* the VAPs, including the Master.
   * This function translates the DFs Slave VAP Index to a Cores one.
   */
  return (iwpriv_slave_vap_index + 1);
}

static __INLINE int 
_mtlk_df_mtlk_to_linux_error_code(int mtlk_res)
{
  int linux_res;

  switch (mtlk_res) {

    case MTLK_ERR_OK:
      linux_res = 0;
      break;

    case MTLK_ERR_PARAMS:
    case MTLK_ERR_VALUE:
      linux_res = -EINVAL;
      break;

    case MTLK_ERR_NO_MEM:
      linux_res = -ENOMEM;
      break;

    case MTLK_ERR_PENDING:
      linux_res = -EINPROGRESS;
      break;

    case MTLK_ERR_BUSY:
      linux_res = -EBUSY;
      break;

    case MTLK_ERR_EEPROM:
    case MTLK_ERR_HW:
    case MTLK_ERR_FW:
    case MTLK_ERR_UMI:
    case MTLK_ERR_MAC:
      linux_res = -EFAULT;
      break;

    case MTLK_ERR_TIMEOUT:
      linux_res = -ETIME;
      break;

    case MTLK_ERR_NOT_READY:
      linux_res = -EAGAIN;
      break;

    case MTLK_ERR_NOT_SUPPORTED:
      linux_res = -EOPNOTSUPP;
      break;

    case MTLK_ERR_NOT_IN_USE:
    case MTLK_ERR_NO_RESOURCES:
    case MTLK_ERR_WRONG_CONTEXT:
    case MTLK_ERR_SCAN_FAILED:
    case MTLK_ERR_AOCS_FAILED:
    case MTLK_ERR_PROHIB:
    case MTLK_ERR_BUF_TOO_SMALL:
    case MTLK_ERR_PKT_DROPPED:
    case MTLK_ERR_FILEOP:
    case MTLK_ERR_CANCELED:
    case MTLK_ERR_NOT_HANDLED:
    case MTLK_ERR_UNKNOWN:
      linux_res = -EFAULT;
      break;

    case MTLK_ERR_DATA_TOO_BIG:
      linux_res = -E2BIG;
      break;

    default :
      linux_res = -EFAULT;
      break;
  }

  return linux_res;
}

/* User-friendly interface/device name */
const char*
mtlk_df_user_get_name(mtlk_df_user_t *df_user)
{
  MTLK_ASSERT(NULL != df_user);
  return df_user->dev->name;
}

/* Layer-2 subsystem access */
void __MTLK_IFUNC
mtlk_df_ui_set_mac_addr(mtlk_df_t *df, const uint8* mac_addr)
{
  MTLK_ASSERT(NULL != df);
  MTLK_ASSERT(NULL != mac_addr);

  mtlk_osal_copy_eth_addresses(mtlk_df_get_user(df)->dev->dev_addr, mac_addr);
}

const uint8* __MTLK_IFUNC
mtlk_df_ui_get_mac_addr(mtlk_df_t* df)
{
  MTLK_ASSERT(NULL != df);

  return mtlk_df_get_user(df)->dev->dev_addr;
}

BOOL __MTLK_IFUNC
mtlk_df_ui_is_promiscuous(mtlk_df_t *df)
{
  MTLK_ASSERT(NULL != df);
  return mtlk_df_get_user(df)->dev->promiscuity ? TRUE : FALSE;
}

void __MTLK_IFUNC
mtlk_df_ui_notify_tx_start(mtlk_df_t *df)
{
  MTLK_ASSERT(NULL != df);
  mtlk_df_get_user(df)->dev->trans_start = jiffies;
}

MTLK_INIT_STEPS_LIST_BEGIN(df_user)
  MTLK_INIT_STEPS_LIST_ENTRY(df_user, ALLOC_SLOW_CTX)
  MTLK_INIT_STEPS_LIST_ENTRY(df_user, DEBUG_BCL)
  MTLK_INIT_STEPS_LIST_ENTRY(df_user, ALLOC_NAME)
  MTLK_INIT_STEPS_LIST_ENTRY(df_user, CREATE_CARD_DIR)
  MTLK_INIT_STEPS_LIST_ENTRY(df_user, CREATE_DEBUG_DIR)
  MTLK_INIT_STEPS_LIST_ENTRY(df_user, STAT_TIMER)
#ifdef MTCFG_IRB_DEBUG
  MTLK_INIT_STEPS_LIST_ENTRY(df_user, IRB_PINGER_INIT)
#endif
  MTLK_INIT_STEPS_LIST_ENTRY(df_user, PROC_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(df_user, FW_HANG_EVTs)
MTLK_INIT_INNER_STEPS_BEGIN(df_user)
MTLK_INIT_STEPS_LIST_END(df_user);

static int 
_mtlk_df_user_start_tx(struct sk_buff *skb, struct net_device *dev)
{
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(skb->dev);

  MTLK_UNREFERENCED_PARAM(dev);
  MTLK_ASSERT(NULL != skb);
  MTLK_ASSERT(NULL != df_user);

  mtlk_nbuf_priv_init(mtlk_nbuf_priv(skb));

  mtlk_nbuf_start_tracking(skb);
  mtlk_core_handle_tx_data(mtlk_df_get_core(df_user->df), skb);
  return NETDEV_TX_OK;
}

/********************************************************************
 * PPA supprting functionality BEGIN
 ********************************************************************/
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH

uint32 _mtlk_df_user_ppa_tx_sent_up = 0;
uint32 _mtlk_df_user_ppa_tx_dropped = 0;

static int
_mtlk_df_user_ppa_start_xmit (struct net_device *rx_dev,
                              struct net_device *tx_dev,
                              struct sk_buff *skb,
                              int len)

{
  if (tx_dev != NULL) {
    mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(skb->dev);

    ++df_user->ppa.stats.tx_processed;

    skb->dev = tx_dev;
    _mtlk_df_user_start_tx(skb, tx_dev);
  }
  else if (rx_dev != NULL)
  {
    MTLK_ASSERT(NULL != skb->dev);
    /* as usual shift the eth header with skb->data */
    skb->protocol = eth_type_trans(skb, skb->dev);
    /* push up to protocol stacks */
    netif_rx(skb);
    ++_mtlk_df_user_ppa_tx_sent_up;
  }
  else {
    dev_kfree_skb_any(skb);
    ++_mtlk_df_user_ppa_tx_dropped;
  }

  return 0;
}

BOOL __MTLK_IFUNC
_mtlk_df_user_ppa_is_available (void)
{
  return (ppa_hook_directpath_register_dev_fn != NULL);
}

BOOL __MTLK_IFUNC 
_mtlk_df_user_ppa_is_registered (mtlk_df_user_t* df_user)
{
  MTLK_ASSERT(df_user != NULL);

  return (df_user->ppa.clb.rx_fn != NULL);
}

int __MTLK_IFUNC 
_mtlk_df_user_ppa_register (mtlk_df_user_t* df_user)
{
  int    res = MTLK_ERR_UNKNOWN;
  uint32 ppa_res;

  MTLK_ASSERT(df_user != NULL);
  MTLK_ASSERT(_mtlk_df_user_ppa_is_registered(df_user) == FALSE);

  if (_mtlk_df_user_ppa_is_available() == FALSE) {
    res = MTLK_ERR_NOT_SUPPORTED;
    goto end;
  }

  memset(&df_user->ppa.clb, 0, sizeof(df_user->ppa.clb));

  df_user->ppa.clb.rx_fn = _mtlk_df_user_ppa_start_xmit;

  ppa_res = 
    ppa_hook_directpath_register_dev_fn(&df_user->ppa.if_id, 
                                        df_user->dev, &df_user->ppa.clb, 
                                        PPA_F_DIRECTPATH_REGISTER | PPA_F_DIRECTPATH_ETH_IF);

  if (ppa_res != IFX_SUCCESS)
  {
    df_user->ppa.clb.rx_fn = NULL;
    ELOG_D("Can't register PPA device function (err=%d)", ppa_res);
    res = MTLK_ERR_UNKNOWN;
    goto end;
  }

  ILOG0_D("PPA device function is registered (id=%d)", df_user->ppa.if_id);
  res = MTLK_ERR_OK;

end:
  return res;
}

void __MTLK_IFUNC
_mtlk_df_user_ppa_unregister (mtlk_df_user_t* df_user)
{
  uint32 ppa_res;

  MTLK_ASSERT(df_user != NULL);
  MTLK_ASSERT(_mtlk_df_user_ppa_is_registered(df_user) == TRUE);

  ppa_res = 
    ppa_hook_directpath_register_dev_fn(&df_user->ppa.if_id, 
                                        df_user->dev, NULL, 
                                        0/*PPA_F_DIRECTPATH_DEREGISTER*/);


  if (ppa_res == IFX_SUCCESS) {
    ILOG0_D("PPA device function is unregistered (id=%d)", df_user->ppa.if_id);
    df_user->ppa.clb.rx_fn = NULL;
    df_user->ppa.if_id     = 0;
  }
  else {
    ELOG_D("Can't unregister PPA device function (err=%d)", ppa_res);
  }
}

void __MTLK_IFUNC
_mtlk_df_user_ppa_get_stats (mtlk_df_user_t*           df_user,
                             mtlk_df_user_ppa_stats_t *stats)
{
  MTLK_ASSERT(df_user != NULL);
  MTLK_ASSERT(stats != NULL);

  memcpy(stats, &df_user->ppa.stats, sizeof(df_user->ppa.stats));
}

void __MTLK_IFUNC
_mtlk_df_user_ppa_zero_stats (mtlk_df_user_t* df_user)
{
  MTLK_ASSERT(df_user != NULL);
  memset(&df_user->ppa.stats, 0, sizeof(df_user->ppa.stats));
}
#endif /* CONFIG_IFX_PPA_API_DIRECTPATH */

static void
_mtlk_df_priv_xface_stop (mtlk_df_user_t* df_user)
{
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  if (_mtlk_df_user_ppa_is_registered(df_user)) {
    _mtlk_df_user_ppa_unregister(df_user);
  }
#endif /* CONFIG_IFX_PPA_API_DIRECTPATH */
}

/********************************************************************
 * PPA supprting functionality END
 ********************************************************************/

static int
_mtlk_df_user_iface_open(struct net_device *dev)
{
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);
  mtlk_clpb_t *clpb = NULL;
  int res;

  MTLK_ASSERT(NULL != df_user);

  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_ACTIVATE_OPEN, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval(res, clpb, MTLK_CORE_REQ_ACTIVATE_OPEN, TRUE);

  return (MTLK_ERR_OK == res) ? 0 : -EAGAIN;
}

static int
_mtlk_df_user_iface_stop(struct net_device *dev)
{
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);
  mtlk_clpb_t *clpb = NULL;
  int res;

  MTLK_ASSERT(NULL != df_user);

  ILOG1_S("%s: stop the interface", mtlk_df_user_get_name(df_user));

  /* 
    The following loop was implemented since the OS might not 
    interpret an -EAGAIN error code correctly and hence will not 
    repeat calling _mtlk_df_user_iface_stop - in case retry is 
    needed.
  */

  do {
    res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_DEACTIVATE, &clpb, NULL, 0);
    res = _mtlk_df_user_process_core_retval(res, clpb, MTLK_CORE_REQ_DEACTIVATE, TRUE);
    mtlk_osal_msleep(100);
  } while ((MTLK_ERR_OK != res) && (MTLK_ERR_FW != res));

  return 0;
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setap (struct net_device *dev,
                              struct iw_request_info *info,
                              struct sockaddr *sa,
                              char *extra)
{ 
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);
  mtlk_clpb_t *clpb = NULL;
  int res = MTLK_ERR_NOT_SUPPORTED;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (!netif_running(dev)) {
    ILOG0_V("You should bring interface up first");
    goto finish;
  }

  if (is_broadcast_ether_addr(sa->sa_data)) goto finish;

  if (is_zero_ether_addr(sa->sa_data)) {
    res = MTLK_ERR_OK;
    goto finish;
  }

  ILOG1_SY("%s: Handle request: connect to %Y", dev->name, sa->sa_data);

  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_CONNECT_STA,
                                  &clpb, &sa->sa_data, sizeof(sa->sa_data));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_CONNECT_STA, TRUE);

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setmlme (struct net_device *dev,
                                struct iw_request_info *info,
                                struct iw_point *data,
                                char *extra)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);
  mtlk_clpb_t *clpb = NULL;
  struct iw_mlme *mlme = (struct iw_mlme *) extra;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (!netif_running(dev)) {
    ILOG0_V("You should bring interface up first");
    goto finish;
  }

  if (NULL == mlme) {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }
  ILOG2_D("MLME cmd %i", mlme->cmd);

  switch (mlme->cmd) {
  case IW_MLME_DEAUTH:
  case IW_MLME_DISASSOC:
    ILOG1_SY("%s: Got MLME Disconnect/Disassociate ioctl (%Y)", dev->name, mlme->addr.sa_data);
    if (!mtlk_df_is_ap(df_user->df)) {
      res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_DISCONNECT_STA,
                                      &clpb, NULL, 0);
      res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_DISCONNECT_STA, TRUE);
    }
    else if(mtlk_osal_eth_is_broadcast(mlme->addr.sa_data)) {
      res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_AP_DISCONNECT_ALL,
                                      &clpb, NULL, 0);
      res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_AP_DISCONNECT_ALL, TRUE);
    }
    else if(!mtlk_osal_is_valid_ether_addr(mlme->addr.sa_data)) {
      ILOG1_SY("%s: Invalid MAC address (%Y)!", dev->name, mlme->addr.sa_data);
      res = MTLK_ERR_PARAMS;
      goto finish;
    }
    else {
      res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_AP_DISCONNECT_STA,
                                      &clpb, mlme->addr.sa_data, sizeof(mlme->addr.sa_data));
      res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_AP_DISCONNECT_STA, TRUE);
    }
    break;
  default:
    goto finish;
  }


finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

/*!
        \fn     mtlk_df_ui_linux_ioctl_set_scan()
        \param  net_device Network device for scan to be performed on
        \param  extra Scan options (if any) starting from WE18
        \return Zero on success, negative error code on failure
        \brief  Handle 'start scan' request

        Handler for SIOCSIWSCAN - request for scan schedule. Process scan
        options (if any), and schedule scan. If scan already running - return
        zero, to avoid 'setscan-getscan' infinite loops in user applications.
        If scheduling succeed - return zero. If scan can't be started - return
        -EAGAIN
*/
int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_scan (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);
  mtlk_clpb_t *clpb = NULL;
  char *essid = NULL;
  size_t essid_size = 0;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

#if WIRELESS_EXT >= 18
  /* iwlist wlan0 scan <ESSID> */
  if (extra && wrqu->data.pointer) {
    struct iw_scan_req *scanopt = (struct iw_scan_req*)extra;
    if (wrqu->data.flags & IW_SCAN_THIS_ESSID) {
      ILOG2_SS("%s: Set ESSID pattern to (%s)", dev->name, scanopt->essid);
      essid = scanopt->essid;
      essid_size = sizeof(scanopt->essid);
    }
  }
#endif

  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_START_SCANNING,
                                  &clpb, essid, essid_size);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_START_SCANNING, TRUE);

  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static uint8
_mtlk_df_ui_calc_signal_strength(int8 RSSI)
{
  uint8 sig_strength = 1;

  if (RSSI > -65)
    sig_strength = 5;
  else if (RSSI > -71)
    sig_strength = 4;
  else if (RSSI > -77)
    sig_strength = 3;
  else if (RSSI > -83)
    sig_strength = 2;

  return sig_strength;
}

/*!
        \fn     mtlk_df_ui_linux_ioctl_get_scan_results()
        \return Zero and scan results on success, negative error code on failure
        \brief  Handle 'get scan results' request

        Handler for SIOCGIWSCAN - request for scan results. If scan running -
        return -EAGAIN and required buffer size. If not enough memory to store
        all scan results - return -E2BIG. On success return zero and cached
        scan results
*/
int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_scan_results (struct net_device *dev,
                                         struct iw_request_info *info,
                                         union iwreq_data *wrqu,
                                         char *extra)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_df_user_t  *df_user = (mtlk_df_user_t*) netdev_priv(dev);
  mtlk_clpb_t     *clpb = NULL;

  bss_data_t      *bss_found;
  struct iw_event iwe;
  char            *stream;
  char            *border;
  size_t          size;
  char            *work_str;
  char            buf[32]; /* used for 3 RSSI string: "-xxx:-xxx:-xxx dBm" */

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_GET_SCANNING_RES,
                                  &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_SCANNING_RES, FALSE);
  if (MTLK_ERR_OK != res) {
    goto finish;
  }

  memset(extra, 0, wrqu->data.length);
  stream = extra;
  border = extra + wrqu->data.length;
  size = 0;

  /* Process scanning results */
  while(NULL != (bss_found = mtlk_clpb_enum_get_next(clpb, NULL))) {

    ILOG1_SYD("\"%-32s\" %Y %3i", bss_found->essid, bss_found->bssid, bss_found->channel);

    iwe.cmd = SIOCGIWAP;
    memcpy(iwe.u.ap_addr.sa_data, bss_found->bssid, ETH_ALEN);
    iwe.u.ap_addr.sa_family = ARPHRD_IEEE80211;
    size += IW_EV_ADDR_LEN;
    stream = mtlk_iwe_stream_add_event(info, stream, border, &iwe, IW_EV_ADDR_LEN);

    iwe.cmd = SIOCGIWESSID;
    iwe.u.data.length = strnlen(bss_found->essid, IW_ESSID_MAX_SIZE);
    iwe.u.data.flags = 1;
    size += IW_EV_POINT_LEN + iwe.u.data.length;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, bss_found->essid);

    iwe.cmd = SIOCGIWFREQ;
    iwe.u.freq.m = bss_found->channel;
    iwe.u.freq.e = 0;
    size += IW_EV_FREQ_LEN;
    stream = mtlk_iwe_stream_add_event(info, stream, border, &iwe, IW_EV_FREQ_LEN);

    if (RSN_IE_SIZE(bss_found)) {
      iwe.cmd = IWEVGENIE;
      iwe.u.data.length = RSN_IE_SIZE(bss_found) + sizeof(ie_t);
      size += IW_EV_POINT_LEN + iwe.u.data.length;
      stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, RSN_IE(bss_found));
    }

    if (WPA_IE_SIZE(bss_found)) {
      iwe.cmd = IWEVGENIE;
      iwe.u.data.length = WPA_IE_SIZE(bss_found) + sizeof(ie_t);
      size += IW_EV_POINT_LEN + iwe.u.data.length;
      stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, WPA_IE(bss_found));
    }

    if (WPS_IE_SIZE(bss_found)) {
      iwe.cmd = IWEVGENIE;
      iwe.u.data.length = WPS_IE_SIZE(bss_found) + sizeof(ie_t);
      size += IW_EV_POINT_LEN + iwe.u.data.length;
      stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, WPS_IE(bss_found));
    }

    iwe.cmd = SIOCGIWENCODE;
    iwe.u.data.flags = BSS_IS_WEP_ENABLED(bss_found)? IW_ENCODE_ENABLED:IW_ENCODE_DISABLED;
    iwe.u.data.length = 0;
    size += IW_EV_POINT_LEN;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, NULL);

    iwe.cmd = IWEVQUAL;
    iwe.u.qual.updated = IW_QUAL_ALL_UPDATED | IW_QUAL_DBM;
    iwe.u.qual.qual = _mtlk_df_ui_calc_signal_strength(MTLK_NORMALIZED_RSSI(bss_found->max_rssi));
    iwe.u.qual.level = bss_found->max_rssi;
    iwe.u.qual.noise = bss_found->noise;
    size += IW_EV_QUAL_LEN;
    stream = mtlk_iwe_stream_add_event(info, stream, border, &iwe, IW_EV_QUAL_LEN);

    iwe.cmd = IWEVCUSTOM;
    work_str = (bss_found->is_2_4)? "2.4 band":"5.2 band";
    iwe.u.data.length = strlen(work_str);
    size += IW_EV_POINT_LEN + iwe.u.data.length;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, work_str);

    iwe.cmd = IWEVCUSTOM;
    work_str = (bss_found->is_ht)? "HT":"not HT";
    iwe.u.data.length = strlen(work_str);
    size += IW_EV_POINT_LEN + iwe.u.data.length;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, work_str);

    iwe.cmd = IWEVCUSTOM;
    work_str = (bss_found->spectrum == SPECTRUM_40MHZ)? "40 MHz":"20 MHz";
    iwe.u.data.length = strlen(work_str);
    size += IW_EV_POINT_LEN + iwe.u.data.length;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, work_str);

    iwe.cmd = IWEVCUSTOM;
    work_str = WPS_IE_FOUND(bss_found)? "WPS":"not WPS";
    iwe.u.data.length = strlen(work_str);
    size += IW_EV_POINT_LEN + iwe.u.data.length;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, work_str);

    iwe.cmd = IWEVCUSTOM;
    sprintf(buf, "%d:%d:%d dBm",
            MTLK_NORMALIZED_RSSI(bss_found->all_rssi[0]),
            MTLK_NORMALIZED_RSSI(bss_found->all_rssi[1]),
            MTLK_NORMALIZED_RSSI(bss_found->all_rssi[2]));
    work_str = buf;
    iwe.u.data.length = strlen(work_str);
    size += IW_EV_POINT_LEN + iwe.u.data.length;
    stream = mtlk_iwe_stream_add_point(info, stream, border, &iwe, work_str);
  }

  wrqu->data.length = size;
  if (stream - extra < size) {
    ILOG1_S("%s: Can't get scan results - buffer is not big enough", dev->name);
    res = MTLK_ERR_DATA_TOO_BIG;
  }

  mtlk_clpb_delete(clpb);
finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}


static int
_mtlk_df_user_set_mac_addr_internal (struct net_device *dev, 
                                     struct sockaddr *addr)
{
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);
  mtlk_clpb_t *clpb = NULL;
  int res = MTLK_ERR_BUSY;

  MTLK_ASSERT(NULL != df_user);

  /* Allow to set MAC address only if !IFF_UP */
  if (dev->flags & IFF_UP) {
    WLOG_S("%s: Can't set MAC address with IFF_UP set", dev->name);
    goto finish;
  }

  /* Validate address family */
  if ((addr->sa_family != ARPHRD_IEEE80211) && (addr->sa_family != ARPHRD_ETHER)) {
    WLOG_S("%s: Can't set MAC address - invalid sa_family", dev->name);
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_SET_MAC_ADDR,
                                  &clpb, addr->sa_data, sizeof(addr->sa_data));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_SET_MAC_ADDR, TRUE);

  if(MTLK_ERR_OK == res)
  {
    mtlk_osal_copy_eth_addresses(dev->dev_addr, addr->sa_data);
  }

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int
_mtlk_df_user_set_mac_addr (struct net_device *dev, void* p)
{
  return _mtlk_df_user_set_mac_addr_internal(dev, (struct sockaddr *)p);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_mac_addr (struct net_device *dev,
                                     struct iw_request_info *info,
                                     union  iwreq_data *wrqu,
                                     char   *extra)
{
  return _mtlk_df_user_set_mac_addr_internal(dev, &wrqu->addr);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_mac_addr (struct net_device *dev,
                                     struct iw_request_info *info,
                                     union  iwreq_data *wrqu,
                                     char   *extra)
{
  mtlk_df_user_t      *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  mtlk_clpb_t         *clpb;
  int                 res = MTLK_ERR_PARAMS;
  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_GET_MAC_ADDR,
                                  &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_MAC_ADDR, FALSE);
  if(MTLK_ERR_OK == res)
  {
    int mac_addr_size;
    void* mac_addr = mtlk_clpb_enum_get_next(clpb, &mac_addr_size);
    MTLK_ASSERT(ETH_ALEN == mac_addr_size);

    mtlk_osal_copy_eth_addresses(wrqu->addr.sa_data, mac_addr);
    wrqu->addr.sa_family = ARPHRD_IEEE80211;

    mtlk_clpb_delete(clpb);
  }

  return _mtlk_df_mtlk_to_linux_error_code(res);
}

#define _DF_USER_GET_PARAM_MAP_START(df_user, param_id_var, int_res) \
  { \
    uint32 __get_param_macro_mix_guard__ = 0; \
    uint32 __req_data_length_in_map; \
    mtlk_df_user_t* __df_user_in_map = df_user; \
    int* __result_in_map = &int_res; \
    mtlk_handle_t __hreq_data_in_map = MTLK_INVALID_HANDLE; \
    *__result_in_map = MTLK_ERR_UNKNOWN; \
    switch(param_id_var) { \
      default: { { \
          MTLK_ASSERT(!"Unknown parameter id in get request"); \
          {

#define _DF_USER_GET_PARAM_MAP_END() \
          } \
        } \
        if(MTLK_INVALID_HANDLE != __hreq_data_in_map) \
        { \
          _mtlk_df_user_free_core_data(__df_user_in_map->df, __hreq_data_in_map); \
        } \
      } \
    } \
    MTLK_UNREFERENCED_PARAM(__get_param_macro_mix_guard__); \
  }

#define _DF_USER_GET_ON_PARAM(param_id, core_req_id, is_void_request, cfg_struct_type, cfg_struct_name) \
        } \
      } \
      if(MTLK_INVALID_HANDLE != __hreq_data_in_map) \
      { \
        _mtlk_df_user_free_core_data(__df_user_in_map->df, __hreq_data_in_map); \
      } \
      MTLK_UNREFERENCED_PARAM(__get_param_macro_mix_guard__); \
      break; \
    } \
    case (param_id): { \
      cfg_struct_type *cfg_struct_name; \
      *__result_in_map = _mtlk_df_user_pull_core_data(__df_user_in_map->df, (core_req_id), (is_void_request), (void**) &cfg_struct_name, &__req_data_length_in_map, &__hreq_data_in_map); \
      if(MTLK_ERR_OK == *__result_in_map) \
      { \
        MTLK_ASSERT(sizeof(cfg_struct_type) == __req_data_length_in_map); \
        {

#define _DF_USER_SET_PARAM_MAP_START(df_user, param_id_var, int_res) \
  { \
    uint32 __set_param_macro_mix_guard__ = 0; \
    mtlk_df_user_t* __df_user_in_map = df_user; \
    int* __result_in_map = &int_res; \
    void* __core_data_in_map = NULL; \
    uint32 __core_data_size_in_map = 0; \
    uint32 __core_req_id_in_map = 0; \
    uint32 __is_void_request_in_map = FALSE; \
    *__result_in_map = MTLK_ERR_OK; \
    switch(param_id_var) { \
      default: { \
        { \
            MTLK_ASSERT(!"Unknown parameter id in set request");

#define _DF_USER_SET_PARAM_MAP_END() \
        } \
          if((NULL != __core_data_in_map))\
          { \
             if(MTLK_ERR_OK == *__result_in_map) { \
               *__result_in_map = _mtlk_df_user_push_core_data(__df_user_in_map->df, __core_req_id_in_map, __is_void_request_in_map, __core_data_in_map, __core_data_size_in_map); \
             } else { \
               mtlk_osal_mem_free(__core_data_in_map); \
             } \
          } \
      } \
    } \
    MTLK_UNREFERENCED_PARAM(__set_param_macro_mix_guard__); \
  }

#define _DF_USER_SET_ON_PARAM(param_id, core_req_id, is_void_request, cfg_struct_type, cfg_struct_name) \
      } \
      if((NULL != __core_data_in_map))\
      { \
         if(MTLK_ERR_OK == *__result_in_map) { \
           *__result_in_map = _mtlk_df_user_push_core_data(__df_user_in_map->df, __core_req_id_in_map, __is_void_request_in_map, __core_data_in_map, __core_data_size_in_map); \
         } else { \
           mtlk_osal_mem_free(__core_data_in_map); \
         } \
      } \
    } \
    MTLK_UNREFERENCED_PARAM(__set_param_macro_mix_guard__); \
    break; \
    case (param_id): { \
      cfg_struct_type *cfg_struct_name = _mtlk_df_user_alloc_core_data(__df_user_in_map->df, sizeof(cfg_struct_type)); \
      __core_data_size_in_map = sizeof(cfg_struct_type); \
      __core_data_in_map = cfg_struct_name; \
      __core_req_id_in_map = core_req_id; \
      __is_void_request_in_map = is_void_request; \
      if(NULL == __core_data_in_map) { \
        *__result_in_map = MTLK_ERR_NO_MEM; \
      } else {

static int
_mtlk_df_user_print_msdu_ac(char *buffer, size_t len, mtlk_aocs_ac_t *ac)
{
  return mtlk_snprintf(buffer, len,
    "\nBE %d BK %d VI %d VO %d",
    ac->ac[AC_BE], ac->ac[AC_BK], 
    ac->ac[AC_VI], ac->ac[AC_VO]);
}

static int 
_mtlk_df_user_print_restricted_ch(char *buffer, size_t len, uint8 *restricted_channels)
{
  uint32 i, length=0;
  
  length += mtlk_snprintf(buffer+length, len-length, "\n");
  for (i=0; i<MAX_CHANNELS; i++) {
    if (MTLK_CHANNEL_NOT_USED == restricted_channels[i]) {
      break;
    }
    length += mtlk_snprintf(buffer+length, len-length, "%d ", restricted_channels[i]);
  }

  if (length ==0) {
    /* no retricted channels */
    length = mtlk_snprintf(buffer, len, "\nAll channels allowed");
  }

  return (int) length;
}

static BOOL
_mtlk_df_user_get_ac_by_name (char *buffer, uint8 *ac)
{
  int i;
  BOOL found = FALSE;
  static const char *aocs_msdu_ac_name[NTS_PRIORITIES] = {"BE", "BK", "VI", "VO"};

  for (i = 0; i < NTS_PRIORITIES; i++)
    if (!strcmp(buffer, aocs_msdu_ac_name[i])) {
       found = TRUE;
       *ac = i;
       break;
    }
  return found;
}

static int
_mtlk_df_user_fill_ac_values(char *buffer, mtlk_aocs_ac_t *ac)
{
  char buf[16];
  char *next_token;
  uint8 ac_num;
  int v, result = MTLK_ERR_OK;

  if (buffer[0] == '\0') {
    goto FINISH;
  }

  for (v = 0; v < NTS_PRIORITIES; v++) {
    ac->ac[v] = AC_NOT_USED;
  }

  next_token = mtlk_get_token(buffer, buf, ARRAY_SIZE(buf), ' ');

  if (!(next_token || (buf[0] != '\0'))) {
    result = MTLK_ERR_UNKNOWN;
    goto FINISH;
  }

  while (next_token || (buf[0] != '\0')) {
    
    /* here comes AC */
    ILOG4_S("ac_num = %s", buf);
    if (!_mtlk_df_user_get_ac_by_name(buf, &ac_num)) {
      ELOG_S("Wrong access category %s", buf);
      result = MTLK_ERR_UNKNOWN;
      goto FINISH;
    }
    
    /* here comes the value */
    next_token = mtlk_get_token(next_token, buf, ARRAY_SIZE(buf), ' ');
    if (buf[0] == '\0') {
      ELOG_V("Value is missed");
      result = MTLK_ERR_UNKNOWN;
      goto FINISH;
    }

    ILOG4_S("value %s", buf);
    v = mtlk_osal_str_atol(buf);
    if (0 == v) {
      ac->ac[ac_num] = AC_DISABLED; 
    }
    else if (1 == v) {
      ac->ac[ac_num] = AC_ENABLED; 
    }
    else {
      ELOG_V("Wrong value for ac");
      result = MTLK_ERR_UNKNOWN;
      goto FINISH;
    }
    next_token = mtlk_get_token(next_token, buf, ARRAY_SIZE(buf), ' ');
  }

FINISH:
  return result;
}

static int
_mtlk_df_user_fill_restricted_ch(char *buffer, uint8 *out_chnl)
{
  uint16 channel;
  const uint8 max_channel_num = (uint8)-1;
  char buf[8];
  char *next_token;
  uint32 i=0;

  memset(out_chnl,(uint8)MTLK_CHANNEL_NOT_USED,MAX_CHANNELS);
  if (buffer[0] == '\0') {
    /* reset restricted channels */
    ILOG4_V("Reset restricted channels - allow all");
  }
  else {
    next_token = mtlk_get_token(buffer, buf, ARRAY_SIZE(buf), ' ');
    while (next_token || (buf[0] != '\0')) {
      channel = mtlk_osal_str_atol(buf);
      if (channel > max_channel_num) {
        return MTLK_ERR_UNKNOWN;
      }

      out_chnl[i] = channel;

      /* find next channel */
      next_token = mtlk_get_token(next_token, buf, ARRAY_SIZE(buf), ' ');
      if (MAX_CHANNELS == ++i) {
        break;
      }
    }
  }
  return MTLK_ERR_OK;
}

static int
_mtlk_df_user_fill_ether_address(IEEE_ADDR *mac_addr, struct sockaddr *sa)
{
  if (sa->sa_family != ARPHRD_ETHER) {
    return MTLK_ERR_PARAMS;
  }

  MTLK_ASSERT(ETH_ALEN == IEEE_ADDR_LEN);
  memcpy(mac_addr->au8Addr, sa->sa_data, ETH_ALEN);

  return MTLK_ERR_OK;
}

static int
_mtlk_df_user_translate_network_mode(uint8 df_network_more, uint8 *core_network_mode)
{
  *core_network_mode = _net_mode_ingress_filter(df_network_more);
  return (*core_network_mode != NETWORK_NONE) ? MTLK_ERR_OK : MTLK_ERR_PARAMS;
}

static int
_mtlk_df_user_parse_bitrate_str(const char *rate,
                                mtlk_core_rate_cfg_t *rate_cfg)
{
  int tmp_int;
  int tmp_fractional;
  
  /*
   * Check whether requested value is bare index or mpbs.
   * We can distinguish with sscanf() return value (number of tokens read)
   */
  if (2 == sscanf(rate, "%i.%i", &tmp_int, &tmp_fractional)) {
    MTLK_CFG_SET_ITEM(rate_cfg, int_rate, tmp_int)
    MTLK_CFG_SET_ITEM(rate_cfg, frac_rate, tmp_fractional)
  }
  else if ((1 == sscanf(rate, "%i", &tmp_int))) {
    MTLK_CFG_SET_ITEM(rate_cfg, array_idx, (uint16)tmp_int)
  }
  else if (!strcmp(rate, "auto")) {
    MTLK_CFG_SET_ITEM(rate_cfg, int_rate, MTLK_CORE_BITRATE_AUTO)
    MTLK_CFG_SET_ITEM(rate_cfg, frac_rate, MTLK_CORE_BITRATE_AUTO)
  }
  else {
    return MTLK_ERR_PARAMS;
  }
  return MTLK_ERR_OK;
}

static int 
_mtlk_df_user_bitrate_to_str(mtlk_core_rate_cfg_t *rate_cfg,
                             char                 *res,
                             uint32               length)
{
  if ((MTLK_CORE_BITRATE_AUTO == rate_cfg->int_rate) &&
      (MTLK_CORE_BITRATE_AUTO == rate_cfg->frac_rate)) {
    snprintf(res, length, "auto");
  }
  else {
    snprintf(res, length, "%i.%i mbps", rate_cfg->int_rate, rate_cfg->frac_rate);
  }
  return MTLK_ERR_OK;
}

static int
_mtlk_df_user_countries_supported_to_str(mtlk_gen_core_country_name_t *country,
                                         char                         *res,
                                         int32                        length)
{
  uint32 i;
  int32 printed_len = 0;

  for(i=0; i<MAX_COUNTRIES; i++) {
    /* stop if no more countries left */
    /* we got a zero-filled memory here */
    if(0 == *country[i].name) {
      break;
    }

    /* no more iwpriv buffer left */
    if((length-printed_len) < 0) {
      break;
    }

    /* newline each 8 elements */
    if(0 == i%8) {
      printed_len += snprintf(res+printed_len, length-printed_len, "\n");
    }

    printed_len += snprintf(res+printed_len, length-printed_len, "%s  ", country[i].name);
  }
  return MTLK_ERR_OK;
}

static int 
_mtlk_df_user_fill_hw_cfg(mtlk_hw_cfg_t *cfg, char *str)
{
  char buf[30];
  char *next_token = str;
  int res = MTLK_ERR_PARAMS;

  memset(cfg, 0, sizeof(cfg));

  next_token = mtlk_get_token(next_token, buf, sizeof(buf), ',');
  if (next_token) {
    strncpy(cfg->buf, buf, sizeof(cfg->buf)-1);
  } else goto end;

  next_token = mtlk_get_token(next_token, buf, sizeof(buf), ',');
  if (next_token) {
    cfg->field_01 = (uint16)mtlk_osal_str_atol(buf);
  } else goto end;

  next_token = mtlk_get_token(next_token, buf, sizeof(buf), ',');
  if (next_token) {
    cfg->field_02 = mtlk_osal_str_atol(buf);
  } else goto end;

  mtlk_get_token(next_token, buf, sizeof(buf), ',');
  cfg->field_03 = (int16)mtlk_osal_str_atol(buf);
  
  res = MTLK_ERR_OK;

end:
  return res;
}

static int 
_mtlk_df_user_fill_ant_cfg(mtlk_ant_cfg_t *cfg, char *str)
{
  char buf[30];
  char *next_token = str;
  int res = MTLK_ERR_PARAMS;

  next_token = mtlk_get_token(next_token, buf, sizeof(buf), ',');
  if (next_token) {
    cfg->field_01 = mtlk_osal_str_atol(buf);
  } else goto end;

  mtlk_get_token(next_token, buf, sizeof(buf), ',');
  cfg->field_02 = mtlk_osal_str_atol(buf);

  res = MTLK_ERR_OK;

end:
  return res;
}

static int 
_mtlk_df_user_fill_power_limit_cfg(mtlk_tx_power_limit_cfg_t *cfg, uint32 data)
{

  if (data > (uint8)-1)
	  return MTLK_ERR_VALUE;

  cfg->field_01 = (uint8)data;

  return MTLK_ERR_OK;
}

static uint32
_mtlk_df_user_print_eeprom(mtlk_eeprom_data_cfg_t *eeprom, char *buffer, uint32 buf_len)
{
  uint8 *buf;
  uint32 used_len = 0;
  uint32 max_len = MTLK_MAX_EEPROM_SIZE;

  buf = mtlk_osal_mem_alloc(max_len, MTLK_MEM_TAG_EEPROM);

  if (NULL == buf) {
    goto nomem;
  }

  memset(buf, 0, max_len);

  used_len += mtlk_snprintf(buf+used_len, max_len-used_len,
                            "\nEEPROM version: %i.%i\n", eeprom->eeprom_version/0x100, eeprom->eeprom_version%0x100);
  MTLK_ASSERT(used_len < max_len);

  used_len += mtlk_snprintf(buf+used_len, max_len-used_len,
                           "EEPROM MAC    : " MAC_PRINTF_FMT "\n", MAC_PRINTF_ARG(eeprom->mac_address));
  MTLK_ASSERT(used_len < max_len);

  used_len += mtlk_snprintf(buf+used_len, max_len-used_len,
                            "EEPROM country: %s\n", country_code_to_country(eeprom->country_code));
  MTLK_ASSERT(used_len < max_len);

  used_len += mtlk_snprintf(buf+used_len, max_len-used_len,
                           "HW type       : 0x%02X\n", eeprom->type);
  MTLK_ASSERT(used_len < max_len);

  used_len += mtlk_snprintf(buf+used_len, max_len-used_len,
                            "HW revision   : 0x%02X (%c)\n", eeprom->revision, eeprom->revision);
  MTLK_ASSERT(used_len < max_len);

  used_len += mtlk_snprintf(buf+used_len, max_len-used_len, "HW ID         : 0x%x,0x%x,0x%x,0x%x\n",
    eeprom->vendor_id, eeprom->device_id,
    eeprom->sub_vendor_id, eeprom->sub_device_id);
  MTLK_ASSERT(used_len < max_len);

  used_len += mtlk_snprintf(buf+used_len, max_len-used_len, "Serial number : %02x%02x%02x\n",
    eeprom->sn[2], eeprom->sn[1], eeprom->sn[0]);
  MTLK_ASSERT(used_len < max_len);

  used_len += mtlk_snprintf(buf+used_len, max_len-used_len, "Week/Year     : %02d/%02d\n",
    eeprom->production_week, eeprom->production_year);
  MTLK_ASSERT(used_len < max_len);
  MTLK_ASSERT(strlen(buf) == used_len);

  if (used_len <= buf_len) {
    strncpy(buffer, buf, used_len);
  }
  else {
    used_len = 0;
  }

  mtlk_osal_mem_free(buf);
nomem:
  return used_len;
}

static uint32
_mtlk_df_user_print_raw_eeprom(uint8 *raw_eeprom, uint32 size_raw_eeprom, char *buffer, uint32 buf_len)
{
  uint8 *buf;
  uint32 used_len = 0;
  uint32 max_len = MTLK_MAX_EEPROM_SIZE;
 
  MTLK_ASSERT(max_len <= buf_len);
  MTLK_UNREFERENCED_PARAM(size_raw_eeprom);

  buf = mtlk_osal_mem_alloc(max_len, MTLK_MEM_TAG_EEPROM);
  if (NULL == buf) {
    goto nomem;
  }

  memset(buf, 0, max_len);

  used_len += mtlk_snprintf(buf+used_len, max_len-used_len, "\nEEPROM header binary dump:\n");
  MTLK_ASSERT(used_len < max_len);

  MTLK_ASSERT(size_raw_eeprom >= mtlk_eeprom_get_size());
  MTLK_ASSERT((max_len-used_len) >= mtlk_eeprom_get_size());
  used_len += mtlk_shexdump(buf+used_len, raw_eeprom, mtlk_eeprom_get_size());

  if (used_len <= buf_len) {
    strncpy(buffer, buf, used_len);
  }
  else {
    used_len = 0;
  }

  mtlk_osal_mem_free(buf);
nomem:
  return used_len;
}

#ifdef MTCFG_IRB_DEBUG

static uint32
_mtlk_df_user_print_irb_pinger_stats(char *buf, uint32 len, struct mtlk_irb_pinger_stats *stats)
{
  uint64 avg_delay = 0;

  if (stats->nof_recvd_pongs) {
    /* NOTE: 64-bit division is not supported by default in linux kernel space =>
     *       we should use the do_div() ASM macro here.
     */
     avg_delay = stats->all_delay;
     do_div(avg_delay, stats->nof_recvd_pongs); /* the result is stored in avg_delay */
  }
      
  return mtlk_snprintf(buf, len, "NofPongs=%u NofMissed=%u NofOOO=%u AllDly=%llu AvgDly=%llu PeakDly=%llu\n",
                        stats->nof_recvd_pongs,
                        stats->nof_missed_pongs,
                        stats->nof_ooo_pongs,
                        stats->all_delay,
                        avg_delay,
                        stats->peak_delay);
}

static int __MTLK_IFUNC
_mtlk_df_user_irb_pinger_int_get_cfg(mtlk_df_user_t* df_user, uint32 subcmd, char* data, uint16* length)
{
  struct mtlk_irb_pinger_stats stats;

  switch (subcmd) {
    case PRM_ID_IRB_PINGER_ENABLED:
      *(uint32*)data = mtlk_irb_pinger_get_ping_period_ms(&df_user->slow_ctx->pinger);
      return MTLK_ERR_OK;
    case PRM_ID_IRB_PINGER_STATS:
      mtlk_irb_pinger_get_stats(&df_user->slow_ctx->pinger, &stats);
      *length = _mtlk_df_user_print_irb_pinger_stats(data, TEXT_SIZE, &stats);
      return MTLK_ERR_OK;
    default:
      return MTLK_ERR_NOT_HANDLED;
  }
}

static int __MTLK_IFUNC
_mtlk_df_user_irb_pinger_int_set_cfg(mtlk_df_user_t* df_user, uint32 subcmd, char* data, uint16* length)
{
  switch (subcmd) {
    case PRM_ID_IRB_PINGER_ENABLED:
      return mtlk_irb_pinger_restart(&df_user->slow_ctx->pinger, *(uint32*)data);
    case PRM_ID_IRB_PINGER_STATS:
      mtlk_irb_pinger_zero_stats(&df_user->slow_ctx->pinger);
      return MTLK_ERR_OK;
    default:
      return MTLK_ERR_NOT_HANDLED;
  }
}

#endif /* MTCFG_IRB_DEBUG */

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
static int __MTLK_IFUNC
_mtlk_df_user_ppa_directpath_int_get_cfg(mtlk_df_user_t* df_user, uint32 subcmd, char* data, uint16* length)
{
  if(PRM_ID_PPA_API_DIRECTPATH == subcmd) {
      if (!_mtlk_df_user_ppa_is_available()) {
        return MTLK_ERR_NOT_SUPPORTED;
      } else {
        *(uint32*)data = _mtlk_df_user_ppa_is_registered(df_user);
        return MTLK_ERR_OK;
      }
  } else {
    return MTLK_ERR_NOT_HANDLED;
  }
}

static int __MTLK_IFUNC
_mtlk_df_user_ppa_directpath_int_set_cfg(mtlk_df_user_t* df_user, uint32 subcmd, char* data, uint16* length)
{ 
  uint32 value = *(uint32*)data;

  if (PRM_ID_PPA_API_DIRECTPATH == subcmd) {
      if (!_mtlk_df_user_ppa_is_available()) {
        return MTLK_ERR_NOT_SUPPORTED;
      } else if (!value && _mtlk_df_user_ppa_is_registered(df_user)) {
        _mtlk_df_user_ppa_unregister(df_user);
        return MTLK_ERR_OK;
      } else if (value && !_mtlk_df_user_ppa_is_registered(df_user)) {
        return _mtlk_df_user_ppa_register(df_user);
      } else {
        return MTLK_ERR_OK;
      }
  } else {
    return MTLK_ERR_NOT_HANDLED;
  }
}
#endif

static void
_mtlk_df_set_vap_limits_cfg (mtlk_mbss_cfg_t *mbss_cfg, uint32 low, uint32 up)
{
  mbss_cfg->vap_limits.lower_limit = low;
  mbss_cfg->vap_limits.upper_limit = up;
}

static void
_mtlk_df_get_vap_limits_cfg (mtlk_mbss_cfg_t *mbss_cfg, uint32 *low, uint32 *up)
{
  *low = mbss_cfg->vap_limits.lower_limit;
  *up = mbss_cfg->vap_limits.upper_limit;
}

static void
_mtlk_df_user_get_intvec_by_fw_gpio_cfg (uint32 *intvec, uint16 *intvec_lenth, const mtlk_fw_led_gpio_cfg_t *gpio_cfg)
{
  intvec[0] = gpio_cfg->disable_testbus;
  intvec[1] = gpio_cfg->active_gpios;
  intvec[2] = gpio_cfg->led_polatity;

  *intvec_lenth = 3;
}

static int
_mtlk_df_user_get_fw_gpio_cfg_by_intvec (const uint32 *intvec, uint16 intvec_lenth, mtlk_fw_led_gpio_cfg_t *gpio_cfg)
{
  int res = MTLK_ERR_PARAMS;

  if (3 != intvec_lenth) {
    ELOG_D("Incorrect vector length. length(%u)", intvec_lenth);
  }
  else if (intvec[0] >= MAX_UINT8 || intvec[1] >= MAX_UINT8 || intvec[2] >= MAX_UINT8) {
    ELOG_DDD("Incorrect value (%u %u %u)", intvec[0], intvec[1], intvec[2]);
  }
  else {
    gpio_cfg->disable_testbus = (uint8)intvec[0];
    gpio_cfg->active_gpios    = (uint8)intvec[1];
    gpio_cfg->led_polatity    = (uint8)intvec[2];
    res                       = MTLK_ERR_OK;
  }

  return res;
}

static int
_mtlk_df_user_get_fw_led_state_by_intvec (const uint32 *intvec, uint16 intvec_lenth, mtlk_fw_led_state_t *led_state)
{
  int res = MTLK_ERR_PARAMS;

  if (2 != intvec_lenth) {
    ELOG_D("Incorrect vector length. length(%u)", intvec_lenth);
  }
  else if (intvec[0] >= MAX_UINT8 || intvec[1] >= MAX_UINT8) {
    ELOG_DD("Incorrect value (%u %u)", intvec[0], intvec[1]);
  }
  else {
    led_state->baseb_led = (uint8)intvec[0];
    led_state->led_state = (uint8)intvec[1];
    res                  = MTLK_ERR_OK;
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_df_user_iwpriv_get_core_param(mtlk_df_user_t* df_user, uint32 param_id, char* data, uint16* length)
{
  int res;

  _DF_USER_GET_PARAM_MAP_START(df_user, param_id, res)

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_BAUSE, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[0], use_aggr, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_BAUSE, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[1], use_aggr, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_BAUSE, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[5], use_aggr, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_BAUSE, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[6], use_aggr, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_BAACCEPT, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[3], accept_aggr, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_BAACCEPT, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[2], accept_aggr, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_BAACCEPT, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[5], accept_aggr, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_BAACCEPT, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[7], accept_aggr, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_BATIMEOUT, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[0], addba_timeout, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_BATIMEOUT, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[1], addba_timeout, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_BATIMEOUT, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[5], addba_timeout, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_BATIMEOUT, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[6], addba_timeout, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_BAWINSIZE, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[0], aggr_win_size, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_BAWINSIZE, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[1], aggr_win_size, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_BAWINSIZE, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[5], aggr_win_size, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_BAWINSIZE, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[6], aggr_win_size, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_AGGRMAXBTS, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[0], max_nof_bytes, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_AGGRMAXBTS, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[1], max_nof_bytes, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_AGGRMAXBTS, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[5], max_nof_bytes, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_AGGRMAXBTS, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[6], max_nof_bytes, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_AGGRMAXPKTS, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[0], max_nof_packets, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_AGGRMAXPKTS, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[1], max_nof_packets, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_AGGRMAXPKTS, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[5], max_nof_packets, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_AGGRMAXPKTS, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[6], max_nof_packets, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_AGGRMINPTSZ, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[0], min_packet_size_in_aggr, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_AGGRMINPTSZ, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[1], min_packet_size_in_aggr, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_AGGRMINPTSZ, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[5], min_packet_size_in_aggr, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_AGGRMINPTSZ, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[6], min_packet_size_in_aggr, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_AGGRTIMEOUT, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[0], timeout_interval, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_AGGRTIMEOUT, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[1], timeout_interval, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_AGGRTIMEOUT, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[5], timeout_interval, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_AGGRTIMEOUT, MTLK_CORE_REQ_GET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_GET_ITEM(&addba_cfg->tid[6], timeout_interval, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_AIFSN, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[0], aifsn, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_AIFSN, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[1], aifsn, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_AIFSN, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[2], aifsn, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_AIFSN, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[3], aifsn, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_CWMAX, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[0], cwmax, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_CWMAX, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[1], cwmax, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_CWMAX, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[2], cwmax, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_CWMAX, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[3], cwmax, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_CWMIN, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[0], cwmin, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_CWMIN, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[1], cwmin, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_CWMIN, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[2], cwmin, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_CWMIN, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[3], cwmin, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_TXOP, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[0], txop, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_TXOP, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[1], txop, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_TXOP, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[2], txop, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_TXOP, MTLK_CORE_REQ_GET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_GET_ITEM(&wme_bss_cfg->wme_class[3], txop, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_AIFSNAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[0], aifsn, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_AIFSNAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[1], aifsn, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_AIFSNAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[2], aifsn, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_AIFSNAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[3], aifsn, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_CWMAXAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[0], cwmax, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_CWMAXAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[1], cwmax, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_CWMAXAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[2], cwmax, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_CWMAXAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[3], cwmax, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_CWMINAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[0], cwmin, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_CWMINAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[1], cwmin, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_CWMINAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[2], cwmin, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_CWMINAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[3], cwmin, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BE_TXOPAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[0], txop, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BK_TXOPAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[1], txop, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VI_TXOPAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[2], txop, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VO_TXOPAP, MTLK_CORE_REQ_GET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_GET_ITEM(&wme_ap_cfg->wme_class[3], txop, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_WEIGHT_CL, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, weight_ch_load, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_WEIGHT_TX, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, weight_tx_power, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_WEIGHT_BSS, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, weight_nof_bss, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_WEIGHT_SM, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, weight_sm_required, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_SCAN_AGING, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, scan_aging_ms, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_CONFIRM_RANK_AGING, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, confirm_rank_aging_ms, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_CFM_RANK_SW_THRESHOLD, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, cfm_rank_sw_threshold, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_AFILTER, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, alpha_filter_coefficient, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_BONDING, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, bonding, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_EN_PENALTIES, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, use_tx_penalties, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_WIN_TIME, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, udp_aocs_window_time_ms, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_LOWER_THRESHOLD, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, udp_lower_threshold, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_THRESHOLD_WINDOW, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, udp_threshold_window, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_MSDU_DEBUG_ENABLED, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, udp_msdu_debug_enabled, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_IS_ENABLED, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, type, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_MSDU_PER_WIN_THRESHOLD, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, udp_msdu_per_window_threshold, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_MSDU_THRESHOLD, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, udp_msdu_threshold_aocs, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_MEASUREMENT_WINDOW, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, tcp_measurement_window, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_THROUGHPUT_THRESHOLD, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, tcp_throughput_threshold, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_NON_OCCUPANCY_PERIOD, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM(aocs_cfg, dbg_non_occupied_period, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_RESTRICTED_CHANNELS, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC(aocs_cfg, restricted_channels, _mtlk_df_user_print_restricted_ch, 
        (data, TEXT_SIZE, aocs_cfg->restricted_channels), *length);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_MSDU_TX_AC, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC(aocs_cfg, msdu_tx_ac, _mtlk_df_user_print_msdu_ac,
        (data, TEXT_SIZE, &aocs_cfg->msdu_tx_ac), *length);

    _DF_USER_GET_ON_PARAM(PRM_ID_AOCS_MSDU_RX_AC, MTLK_CORE_REQ_GET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC(aocs_cfg, msdu_rx_ac, _mtlk_df_user_print_msdu_ac,
        (data, TEXT_SIZE, &aocs_cfg->msdu_rx_ac), *length);

    _DF_USER_GET_ON_PARAM(PRM_ID_11H_RADAR_DETECTION, MTLK_CORE_REQ_GET_DOT11H_CFG, FALSE, mtlk_11h_cfg_t, dot11h_cfg)
      MTLK_CFG_GET_ITEM(dot11h_cfg, radar_detect, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_11H_STATUS, MTLK_CORE_REQ_GET_DOT11H_CFG, FALSE, mtlk_11h_cfg_t, dot11h_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(dot11h_cfg, status, strcpy, (data, dot11h_cfg->status));
      *length = strlen(dot11h_cfg->status);

    _DF_USER_GET_ON_PARAM(PRM_ID_11H_BEACON_COUNT, MTLK_CORE_REQ_GET_DOT11H_AP_CFG, FALSE, mtlk_11h_ap_cfg_t, dot11h_cfg)
      MTLK_CFG_GET_ITEM(dot11h_cfg, debugChannelSwitchCount, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_11H_CHANNEL_AVAILABILITY_CHECK_TIME, MTLK_CORE_REQ_GET_DOT11H_AP_CFG, FALSE, mtlk_11h_ap_cfg_t, dot11h_cfg)
      MTLK_CFG_GET_ITEM(dot11h_cfg, debugChannelAvailabilityCheckTime, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_11H_ENABLE_SM_CHANNELS, MTLK_CORE_REQ_GET_DOT11H_AP_CFG, FALSE, mtlk_11h_ap_cfg_t, dot11h_cfg)
      MTLK_CFG_GET_ITEM(dot11h_cfg, enable_sm_required, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_11H_NEXT_CHANNEL, MTLK_CORE_REQ_GET_DOT11H_AP_CFG, FALSE, mtlk_11h_ap_cfg_t, dot11h_cfg)
      MTLK_CFG_GET_ITEM(dot11h_cfg, next_channel, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_ACL_MODE, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, acl_mode, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_VAP_STA_LIMS, MTLK_CORE_REQ_MBSS_GET_VARS, FALSE, mtlk_mbss_cfg_t, mbss_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(mbss_cfg, vap_limits, _mtlk_df_get_vap_limits_cfg,
                                     (mbss_cfg, &((uint32*)data)[0], &((uint32*)data)[1]));
      *length = VAP_LIMIT_SET_SIZE;

    _DF_USER_GET_ON_PARAM(MIB_CALIBRATION_ALGO_MASK, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, calibr_algo_mask, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_POWER_INCREASE_VS_DUTY_CYCLE, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, power_increase, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_USE_SHORT_CYCLIC_PREFIX, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, short_cyclic_prefix, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, short_preamble_option, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, short_slot_time_option, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_SHORT_RETRY_LIMIT, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, short_retry_limit, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_LONG_RETRY_LIMIT, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, long_retry_limit, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_TX_MSDU_LIFETIME, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, tx_msdu_lifetime, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_CURRENT_TX_ANTENNA, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, current_tx_antenna, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_BEACON_PERIOD, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, beacon_period, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_DISCONNECT_ON_NACKS_WEIGHT, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, disconnect_on_nacks_weight, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_SM_ENABLE, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, sm_enable, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_ADVANCED_CODING_SUPPORTED, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, advanced_coding_supported, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_OVERLAPPING_PROTECTION_ENABLE, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, overlapping_protect_enabled, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_OFDM_PROTECTION_METHOD, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, ofdm_protect_method, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_HT_PROTECTION_METHOD, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, ht_method, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_DTIM_PERIOD, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, dtim_period, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_RECEIVE_AMPDU_MAX_LENGTH, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, receive_ampdu_max_len, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_CB_DATABINS_PER_SYMBOL, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, cb_databins_per_symbol, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_USE_LONG_PREAMBLE_FOR_MULTICAST, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, use_long_preamble_for_multicast, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_USE_SPACE_TIME_BLOCK_CODE, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, use_space_time_block_code, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_ONLINE_CALIBRATION_ALGO_MASK, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, online_calibr_algo_mask, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_DISCONNECT_ON_NACKS_ENABLE, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)  
      MTLK_CFG_GET_ITEM(mibs_cfg, disconnect_on_nacks_enable, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_RTS_THRESHOLD, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, rts_threshold, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_TX_POWER, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM(mibs_cfg, tx_power, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_SUPPORTED_TX_ANTENNAS, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(mibs_cfg, tx_antennas, strncpy, 
                                     (data, mibs_cfg->tx_antennas, MTLK_NUM_ANTENNAS_BUFSIZE));
      *length = MTLK_NUM_ANTENNAS_BUFSIZE;

    _DF_USER_GET_ON_PARAM(MIB_SUPPORTED_RX_ANTENNAS, MTLK_CORE_REQ_GET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(mibs_cfg, rx_antennas, strncpy, 
                                     (data, mibs_cfg->rx_antennas, MTLK_NUM_ANTENNAS_BUFSIZE));
      *length = MTLK_NUM_ANTENNAS_BUFSIZE;

    _DF_USER_GET_ON_PARAM(PRM_ID_HIDDEN_SSID, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM(core_cfg, is_hidden_ssid, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(MIB_COUNTRY, MTLK_CORE_REQ_GET_COUNTRY_CFG, FALSE, mtlk_country_cfg_t, country_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(country_cfg, country, strncpy, 
                                      (data, country_cfg->country, MTLK_CHNLS_COUNTRY_BUFSIZE));
      *length = MTLK_CHNLS_COUNTRY_BUFSIZE;

    _DF_USER_GET_ON_PARAM(PRM_ID_L2NAT_AGING_TIMEOUT, MTLK_CORE_REQ_GET_L2NAT_CFG, FALSE, mtlk_l2nat_cfg_t, l2nat_cfg)
      MTLK_CFG_GET_ITEM(l2nat_cfg, aging_timeout, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_L2NAT_DEFAULT_HOST, MTLK_CORE_REQ_GET_L2NAT_CFG, FALSE, mtlk_l2nat_cfg_t, l2nat_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(l2nat_cfg, address, memcpy, ((*(struct sockaddr*)data).sa_data, l2nat_cfg->address.au8Addr, ETH_ALEN));

    _DF_USER_GET_ON_PARAM(PRM_ID_11D, MTLK_CORE_REQ_GET_DOT11D_CFG, FALSE, mtlk_dot11d_cfg_t, dot11d_cfg)
      MTLK_CFG_GET_ITEM(dot11d_cfg, is_dot11d, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_MAC_WATCHDOG_TIMEOUT_MS, MTLK_CORE_REQ_GET_MAC_WATCHDOG_CFG, FALSE, mtlk_mac_wdog_cfg_t, mac_wdog_cfg)
      MTLK_CFG_GET_ITEM(mac_wdog_cfg, mac_watchdog_timeout_ms, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_MAC_WATCHDOG_PERIOD_MS, MTLK_CORE_REQ_GET_MAC_WATCHDOG_CFG, FALSE, mtlk_mac_wdog_cfg_t, mac_wdog_cfg)
      MTLK_CFG_GET_ITEM(mac_wdog_cfg, mac_watchdog_period_ms, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_STA_KEEPALIVE_TIMEOUT, MTLK_CORE_REQ_GET_STADB_CFG, FALSE, mtlk_stadb_cfg_t, stadb_cfg)
      MTLK_CFG_GET_ITEM(stadb_cfg, sta_keepalive_timeout, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_STA_KEEPALIVE_INTERVAL, MTLK_CORE_REQ_GET_STADB_CFG, FALSE, mtlk_stadb_cfg_t, stadb_cfg)
      MTLK_CFG_GET_ITEM(stadb_cfg, keepalive_interval, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AGGR_OPEN_THRESHOLD, MTLK_CORE_REQ_GET_STADB_CFG, FALSE, mtlk_stadb_cfg_t, stadb_cfg)
      MTLK_CFG_GET_ITEM(stadb_cfg, aggr_open_threshold, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BRIDGE_MODE, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM(core_cfg, bridge_mode, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_DBG_SW_WD_ENABLE, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM(core_cfg, dbg_sw_wd_enable, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_RELIABLE_MULTICAST, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM(core_cfg, reliable_multicast, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_UP_RESCAN_EXEMPTION_TIME, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM(core_cfg, up_rescan_exemption_time, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_AP_FORWARDING, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM(core_cfg, ap_forwarding, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_SPECTRUM_MODE, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM(core_cfg, spectrum_mode, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_NETWORK_MODE, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM(core_cfg, net_mode, *(uint32*)data);
      *(uint32*)data = _net_mode_egress_filter(*(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_CHANNEL, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM(core_cfg, channel, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_POWER_SELECTION, MTLK_CORE_REQ_GET_MASTER_CFG, FALSE, mtlk_master_core_cfg_t, master_cfg)
      MTLK_CFG_GET_ITEM(master_cfg, power_selection, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BSS_BASIC_RATE_SET, MTLK_CORE_REQ_GET_MASTER_AP_CFG, FALSE, mtlk_master_ap_core_cfg_t, master_ap_cfg)
      MTLK_CFG_GET_ITEM(master_ap_cfg, bss_rate, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_CORE_COUNTRIES_SUPPORTED, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(core_cfg, countries_supported,
          _mtlk_df_user_countries_supported_to_str, (core_cfg->countries_supported, data, TEXT_SIZE));
      *length = TEXT_SIZE;

    _DF_USER_GET_ON_PARAM(PRM_ID_NICK_NAME, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(core_cfg, nickname,
          memcpy, (data, core_cfg->nickname, IW_ESSID_MAX_SIZE));
      *length = strnlen(data, IW_ESSID_MAX_SIZE);

    _DF_USER_GET_ON_PARAM(PRM_ID_ESSID, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(core_cfg, essid,
          memcpy, (data, core_cfg->essid, IW_ESSID_MAX_SIZE));
      *length = strnlen(data, IW_ESSID_MAX_SIZE);

    _DF_USER_GET_ON_PARAM(PRM_ID_BSSID, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(core_cfg, bssid, memcpy,
          (data, core_cfg->bssid, ETH_ALEN));
      *length = ETH_ALEN;

    _DF_USER_GET_ON_PARAM(PRM_ID_LEGACY_FORCE_RATE, MTLK_CORE_REQ_GET_MASTER_CFG, FALSE, mtlk_master_core_cfg_t, master_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(master_cfg, legacy_force_rate, _mtlk_df_user_bitrate_to_str,
                                     (&master_cfg->legacy_force_rate, data, TEXT_SIZE));
      *length = TEXT_SIZE;

    _DF_USER_GET_ON_PARAM(PRM_ID_HT_FORCE_RATE, MTLK_CORE_REQ_GET_MASTER_CFG, FALSE, mtlk_master_core_cfg_t, master_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(master_cfg, ht_force_rate, _mtlk_df_user_bitrate_to_str,
                                     (&master_cfg->ht_force_rate, data, TEXT_SIZE));
      *length = TEXT_SIZE;

    _DF_USER_GET_ON_PARAM(PRM_ID_ACL, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      int i;
      MTLK_CFG_GET_ITEM(core_cfg, num_macs_to_set, *length);
      MTLK_ASSERT(*length <= MAX_ADDRESSES_IN_ACL);
      for (i=0; i<*length; i++) {
        MTLK_CFG_GET_ITEM_BY_FUNC_VOID(core_cfg, macs_to_set, memcpy,
          (((struct sockaddr*)data)[i].sa_data, core_cfg->macs_to_set[i].au8Addr,sizeof(core_cfg->macs_to_set[i].au8Addr)))
      }

    _DF_USER_GET_ON_PARAM(PRM_ID_ACL_RANGE, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      int i, num_ranges;
      MTLK_CFG_GET_ITEM(core_cfg, num_macs_to_set, *length);
      MTLK_ASSERT(*length <= MAX_ADDRESSES_IN_ACL);
      for (i=0, num_ranges=0; i<*length; i++) {
        if (mtlk_osal_compare_eth_addresses(EMPTY_MAC_MASK.au8Addr, core_cfg->mac_mask[i].au8Addr)) {
          MTLK_CFG_GET_ITEM_BY_FUNC_VOID(core_cfg, macs_to_set, memcpy,
            (((struct sockaddr*)data)[num_ranges << 1].sa_data, core_cfg->macs_to_set[i].au8Addr,sizeof(core_cfg->macs_to_set[i].au8Addr)))
          MTLK_CFG_GET_ITEM_BY_FUNC_VOID(core_cfg, macs_to_set, memcpy,
            (((struct sockaddr*)data)[(num_ranges << 1) + 1].sa_data, core_cfg->mac_mask[i].au8Addr,sizeof(core_cfg->mac_mask[i].au8Addr)))
          num_ranges++;
        }
      }
      *length = num_ranges << 1;

    _DF_USER_GET_ON_PARAM(PRM_ID_ACTIVE_SCAN_SSID, MTLK_CORE_REQ_GET_SCAN_CFG, FALSE, mtlk_scan_cfg_t, scan_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(scan_cfg, essid, strncpy,
                                     (data, scan_cfg->essid, MIB_ESSID_LENGTH + 1));
      *length = MIB_ESSID_LENGTH + 1;

    _DF_USER_GET_ON_PARAM(PRM_ID_EEPROM, MTLK_CORE_REQ_GET_EEPROM_CFG, FALSE, mtlk_eeprom_cfg_t, eeprom_cfg)
      /* TODO: incorrect implementation, is_if_stopped flag must be dropped in favor of abilities or internal core logic */
      BOOL is_if_stopped = FALSE;
      MTLK_CFG_GET_ITEM(eeprom_cfg, is_if_stopped, is_if_stopped);

      if (is_if_stopped) {
        int used_len = 0;
        *length = 0;
        MTLK_CFG_GET_ITEM_BY_FUNC(eeprom_cfg, eeprom_data, _mtlk_df_user_print_eeprom,
                                  (&eeprom_cfg->eeprom_data, data, TEXT_SIZE), used_len);
        *length += used_len;
        MTLK_ASSERT(*length <= TEXT_SIZE);

        MTLK_CFG_GET_ITEM_BY_FUNC(eeprom_cfg, eeprom_raw_data, _mtlk_df_user_print_raw_eeprom,
                                  (eeprom_cfg->eeprom_raw_data, sizeof(eeprom_cfg->eeprom_raw_data), data+*length, TEXT_SIZE-*length), used_len);
        *length += used_len;
        MTLK_ASSERT(*length <= TEXT_SIZE);
      }
      else {
         *length = mtlk_snprintf(data, TEXT_SIZE, "EE data is not available since IF is up\n");
      }

    _DF_USER_GET_ON_PARAM(PRM_ID_WDS_HOST_TIMEOUT, MTLK_CORE_REQ_GET_HSTDB_CFG, FALSE, mtlk_hstdb_cfg_t, hstdb_cfg)
      MTLK_CFG_GET_ITEM(hstdb_cfg, wds_host_timeout, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_HSTDB_LOCAL_MAC, MTLK_CORE_REQ_GET_HSTDB_CFG, FALSE, mtlk_hstdb_cfg_t, hstdb_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(hstdb_cfg, address, memcpy,
                                    (((struct sockaddr*)data)->sa_data, hstdb_cfg->address.au8Addr, ETH_ALEN));

    _DF_USER_GET_ON_PARAM(PRM_ID_SCAN_CACHE_LIFETIME, MTLK_CORE_REQ_GET_SCAN_CFG, FALSE, mtlk_scan_cfg_t, scan_cfg)
      MTLK_CFG_GET_ITEM(scan_cfg, cache_expire, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BG_SCAN_CH_LIMIT, MTLK_CORE_REQ_GET_SCAN_CFG, FALSE, mtlk_scan_cfg_t, scan_cfg)
      MTLK_CFG_GET_ITEM(scan_cfg, channels_per_chunk_limit, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_BG_SCAN_PAUSE, MTLK_CORE_REQ_GET_SCAN_CFG, FALSE, mtlk_scan_cfg_t, scan_cfg)
      MTLK_CFG_GET_ITEM(scan_cfg, pause_between_chunks, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_IS_BACKGROUND_SCAN, MTLK_CORE_REQ_GET_SCAN_CFG, FALSE, mtlk_scan_cfg_t, scan_cfg)
      MTLK_CFG_GET_ITEM(scan_cfg, is_background_scan, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_USE_8021Q, MTLK_CORE_REQ_GET_QOS_CFG, FALSE, mtlk_qos_cfg_t, qos_cfg)
      MTLK_CFG_GET_ITEM(qos_cfg, map, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_COC_LOW_POWER_MODE, MTLK_CORE_REQ_GET_COC_CFG, FALSE, mtlk_coc_mode_cfg_t, coc_cfg)
      MTLK_CFG_GET_ITEM(coc_cfg, is_enabled, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_SQ_LIMITS, MTLK_CORE_REQ_GET_SQ_CFG, FALSE, mtlk_sq_cfg_t, sq_cfg)
      MTLK_CFG_GET_ARRAY_ITEM(sq_cfg, sq_limit, (int32*)data, NTS_PRIORITIES);
      *length = NTS_PRIORITIES;

    _DF_USER_GET_ON_PARAM(PRM_ID_SQ_PEER_LIMITS, MTLK_CORE_REQ_GET_SQ_CFG, FALSE, mtlk_sq_cfg_t, sq_cfg)
      MTLK_CFG_GET_ARRAY_ITEM(sq_cfg, peer_queue_limit, (int32*)data, NTS_PRIORITIES);
      *length = NTS_PRIORITIES;

    /* 20/40 coexistence */

    _DF_USER_GET_ON_PARAM(PRM_ID_COEX_MODE, MTLK_CORE_REQ_GET_COEX_20_40_MODE_CFG, FALSE, mtlk_coex_20_40_mode_cfg_t, coex20_40_cfg)
      MTLK_CFG_GET_ITEM(coex20_40_cfg, coexistence_mode, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_INTOLERANCE_MODE, MTLK_CORE_REQ_GET_COEX_20_40_MODE_CFG, FALSE, mtlk_coex_20_40_mode_cfg_t, coex20_40_cfg)
      MTLK_CFG_GET_ITEM(coex20_40_cfg, intolerance_mode, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_EXEMPTION_REQ, MTLK_CORE_REQ_GET_COEX_20_40_EXM_REQ_CFG, FALSE, mtlk_coex_20_40_exm_req_cfg_t, coex20_40_cfg)
      MTLK_CFG_GET_ITEM(coex20_40_cfg, exemption_req, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_DELAY_FACTOR, MTLK_CORE_REQ_GET_COEX_20_40_TIMES_CFG, FALSE, mtlk_coex_20_40_times_cfg_t, coex20_40_cfg)
      MTLK_CFG_GET_ITEM(coex20_40_cfg, delay_factor, *(uint32*)data);

    _DF_USER_GET_ON_PARAM(PRM_ID_OBSS_SCAN_INTERVAL, MTLK_CORE_REQ_GET_COEX_20_40_TIMES_CFG, FALSE, mtlk_coex_20_40_times_cfg_t, coex20_40_cfg)
      MTLK_CFG_GET_ITEM(coex20_40_cfg, obss_scan_interval, *(uint32*)data);

    /* Card Capabilities */
    _DF_USER_GET_ON_PARAM(PRM_ID_AP_CAPABILITIES_MAX_STAs, MTLK_CORE_REQ_GET_AP_CAPABILITIES, TRUE, mtlk_card_capabilities_t, card_capabilities)
      MTLK_CFG_GET_ITEM(card_capabilities, max_stas_supported, *(uint32*)data);
    _DF_USER_GET_ON_PARAM(PRM_ID_AP_CAPABILITIES_MAX_VAPs, MTLK_CORE_REQ_GET_AP_CAPABILITIES, TRUE, mtlk_card_capabilities_t, card_capabilities)
      MTLK_CFG_GET_ITEM(card_capabilities, max_vaps_supported, *(uint32*)data);

    /* FW GPIO LED */
    _DF_USER_GET_ON_PARAM(PRM_ID_CFG_LED_GPIO, MTLK_CORE_REQ_GET_FW_LED_CFG, FALSE, mtlk_fw_led_cfg_t, led_gpio_cfg)
      MTLK_CFG_GET_ITEM_BY_FUNC_VOID(led_gpio_cfg, gpio_cfg,
                                     _mtlk_df_user_get_intvec_by_fw_gpio_cfg, ((uint32*)data, length, &led_gpio_cfg->gpio_cfg));

  _DF_USER_GET_PARAM_MAP_END()

  return res;
}

static int __MTLK_IFUNC
_mtlk_df_user_iwpriv_set_core_param(mtlk_df_user_t* df_user, uint32 param_id, char* data, uint16* length)
{
  int res;

  _DF_USER_SET_PARAM_MAP_START(df_user, param_id, res)
    _DF_USER_SET_ON_PARAM(PRM_ID_BE_BAUSE, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[0], use_aggr, !!*(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_BAUSE, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[1], use_aggr, !!*(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_BAUSE, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[5], use_aggr, !!*(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_BAUSE, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[6], use_aggr, !!*(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_BAACCEPT, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[0], accept_aggr, !!*(uint32*)data);
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[3], accept_aggr, !!*(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_BAACCEPT, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[1], accept_aggr, !!*(uint32*)data);
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[2], accept_aggr, !!*(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_BAACCEPT, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[5], accept_aggr, !!*(uint32*)data);
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[4], accept_aggr, !!*(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_BAACCEPT, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[6], accept_aggr, !!*(uint32*)data);
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[7], accept_aggr, !!*(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_BATIMEOUT, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[0], addba_timeout, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_BATIMEOUT, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[1], addba_timeout, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_BATIMEOUT, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[5], addba_timeout, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_BATIMEOUT, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[6], addba_timeout, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_BAWINSIZE, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[0], aggr_win_size, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_BAWINSIZE, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[1], aggr_win_size, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_BAWINSIZE, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[5], aggr_win_size, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_BAWINSIZE, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[6], aggr_win_size, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_AGGRMAXBTS, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[0], max_nof_bytes, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_AGGRMAXBTS, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[1], max_nof_bytes, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_AGGRMAXBTS, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[5], max_nof_bytes, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_AGGRMAXBTS, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[6], max_nof_bytes, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_AGGRMAXPKTS, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[0], max_nof_packets, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_AGGRMAXPKTS, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[1], max_nof_packets, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_AGGRMAXPKTS, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[5], max_nof_packets, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_AGGRMAXPKTS, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[6], max_nof_packets, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_AGGRMINPTSZ, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[0], min_packet_size_in_aggr, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_AGGRMINPTSZ, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[1], min_packet_size_in_aggr, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_AGGRMINPTSZ, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[5], min_packet_size_in_aggr, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_AGGRMINPTSZ, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[6], min_packet_size_in_aggr, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_AGGRTIMEOUT, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[0], timeout_interval, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_AGGRTIMEOUT, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[1], timeout_interval, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_AGGRTIMEOUT, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[5], timeout_interval, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_AGGRTIMEOUT, MTLK_CORE_REQ_SET_ADDBA_CFG, FALSE, mtlk_addba_cfg_entity_t, addba_cfg)
      MTLK_CFG_SET_ITEM(&addba_cfg->tid[6], timeout_interval, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_AIFSN, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[0], aifsn, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_AIFSN, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[1], aifsn, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_AIFSN, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[2], aifsn, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_AIFSN, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[3], aifsn, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_CWMAX, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[0], cwmax, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_CWMAX, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[1], cwmax, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_CWMAX, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[2], cwmax, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_CWMAX, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[3], cwmax, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_CWMIN, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[0], cwmin, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_CWMIN, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[1], cwmin, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_CWMIN, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[2], cwmin, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_CWMIN, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[3], cwmin, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_TXOP, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[0], txop, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_TXOP, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[1], txop, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_TXOP, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[2], txop, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_TXOP, MTLK_CORE_REQ_SET_WME_BSS_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_bss_cfg)
      MTLK_CFG_SET_ITEM(&wme_bss_cfg->wme_class[3], txop, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_AIFSNAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[0], aifsn, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_AIFSNAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[1], aifsn, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_AIFSNAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[2], aifsn, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_AIFSNAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[3], aifsn, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_CWMAXAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[0], cwmax, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_CWMAXAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[1], cwmax, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_CWMAXAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[2], cwmax, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_CWMAXAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[3], cwmax, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_CWMINAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[0], cwmin, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_CWMINAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[1], cwmin, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_CWMINAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[2], cwmin, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_CWMINAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[3], cwmin, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BE_TXOPAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[0], txop, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BK_TXOPAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[1], txop, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VI_TXOPAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[2], txop, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_VO_TXOPAP, MTLK_CORE_REQ_SET_WME_AP_CFG, FALSE, mtlk_wme_cfg_entity_t, wme_ap_cfg)
      MTLK_CFG_SET_ITEM(&wme_ap_cfg->wme_class[3], txop, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_WEIGHT_CL, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, weight_ch_load, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_WEIGHT_TX, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, weight_tx_power, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_WEIGHT_BSS, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, weight_nof_bss, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_WEIGHT_SM, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, weight_sm_required, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_CFM_RANK_SW_THRESHOLD, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, cfm_rank_sw_threshold, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_SCAN_AGING, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, scan_aging_ms, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_CONFIRM_RANK_AGING, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, confirm_rank_aging_ms, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_AFILTER, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, alpha_filter_coefficient, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_BONDING, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, bonding, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_EN_PENALTIES, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, use_tx_penalties, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_WIN_TIME, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, udp_aocs_window_time_ms, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_LOWER_THRESHOLD, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, udp_lower_threshold, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_THRESHOLD_WINDOW, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, udp_threshold_window, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_MSDU_DEBUG_ENABLED, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, udp_msdu_debug_enabled, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_IS_ENABLED, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, type, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_MSDU_PER_WIN_THRESHOLD, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, udp_msdu_per_window_threshold, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_MSDU_THRESHOLD, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, udp_msdu_threshold_aocs, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_MEASUREMENT_WINDOW, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, tcp_measurement_window, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_THROUGHPUT_THRESHOLD, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, tcp_throughput_threshold, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_NON_OCCUPANCY_PERIOD, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ITEM(aocs_cfg, dbg_non_occupied_period, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_11H_RADAR_DETECTION, MTLK_CORE_REQ_SET_DOT11H_CFG, FALSE, mtlk_11h_cfg_t, dot11h_cfg)
      MTLK_CFG_SET_ITEM(dot11h_cfg, radar_detect, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_11H_EMULATE_RADAR_DETECTION, MTLK_CORE_REQ_SET_DOT11H_AP_CFG, FALSE, mtlk_11h_ap_cfg_t, dot11h_cfg)
      MTLK_CFG_SET_ITEM(dot11h_cfg, channel_emu, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_11H_SWITCH_CHANNEL, MTLK_CORE_REQ_SET_DOT11H_AP_CFG, FALSE, mtlk_11h_ap_cfg_t, dot11h_cfg)
      MTLK_CFG_SET_ITEM(dot11h_cfg, channel_switch, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_11H_ENABLE_SM_CHANNELS, MTLK_CORE_REQ_SET_DOT11H_AP_CFG, FALSE, mtlk_11h_ap_cfg_t, dot11h_cfg)
      MTLK_CFG_SET_ITEM(dot11h_cfg, enable_sm_required, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_11H_BEACON_COUNT, MTLK_CORE_REQ_SET_DOT11H_AP_CFG, FALSE, mtlk_11h_ap_cfg_t, dot11h_cfg)
      MTLK_CFG_SET_ITEM(dot11h_cfg, debugChannelSwitchCount, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_11H_CHANNEL_AVAILABILITY_CHECK_TIME, MTLK_CORE_REQ_SET_DOT11H_AP_CFG, FALSE, mtlk_11h_ap_cfg_t, dot11h_cfg)
      MTLK_CFG_SET_ITEM(dot11h_cfg, debugChannelAvailabilityCheckTime, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_11H_NEXT_CHANNEL, MTLK_CORE_REQ_SET_DOT11H_AP_CFG, FALSE, mtlk_11h_ap_cfg_t, dot11h_cfg)
      MTLK_CFG_SET_ITEM(dot11h_cfg, next_channel, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_ACL_MODE, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      if (*(uint32*)data > 2) {
        ELOG_V("Usage: 0 - OFF, 1 - white list, 2 - black list");
        res = MTLK_ERR_PARAMS;
      } else if (!mtlk_df_is_ap(df_user->df)) {
        ELOG_V("The command is supported only in AP mode");
        res = MTLK_ERR_PARAMS;
      } else {
        MTLK_CFG_SET_ITEM(mibs_cfg, acl_mode, *(uint32*)data);
      }
	
    _DF_USER_SET_ON_PARAM(PRM_ID_VAP_STA_LIMS, MTLK_CORE_REQ_MBSS_SET_VARS, FALSE, mtlk_mbss_cfg_t, mbss_cfg)
      int i;
      uint32 *value = (uint32*)data;
      if (*length != VAP_LIMIT_SET_SIZE) {
        ELOG_V("Needs a set of two parameters");
        res = MTLK_ERR_PARAMS;
      } else {
        for (i = 0; i < VAP_LIMIT_SET_SIZE; i ++) {
          if (value[i] == (uint32)DF_USER_DEFAULT_IWPRIV_LIM_VALUE) {
            value[i] = MTLK_MBSS_VAP_LIMIT_DEFAULT;
          }
          else if (((int32)value[i]) < 0) {
            ELOG_V("invalid parameter");
            res = MTLK_ERR_PARAMS;
          }
        }
        if(MTLK_ERR_OK == res) {
          MTLK_CFG_SET_ITEM_BY_FUNC_VOID(mbss_cfg, vap_limits, _mtlk_df_set_vap_limits_cfg, (mbss_cfg, value[0], value[1]));
        }
      }

    _DF_USER_SET_ON_PARAM(PRM_ID_VAP_ADD, MTLK_CORE_REQ_MBSS_ADD_VAP, FALSE, mtlk_mbss_cfg_t, mbss_cfg)
      MTLK_CFG_SET_ITEM(mbss_cfg, added_vap_index, _mtlk_df_user_get_core_slave_vap_index_by_iwpriv_param (*(uint32*)data));

    _DF_USER_SET_ON_PARAM(PRM_ID_VAP_DEL, MTLK_CORE_REQ_MBSS_DEL_VAP, FALSE, mtlk_mbss_cfg_t, mbss_cfg)
      MTLK_CFG_SET_ITEM(mbss_cfg, deleted_vap_index, _mtlk_df_user_get_core_slave_vap_index_by_iwpriv_param (*(uint32*)data));

    _DF_USER_SET_ON_PARAM(MIB_CALIBRATION_ALGO_MASK, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, calibr_algo_mask, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_POWER_INCREASE_VS_DUTY_CYCLE, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, power_increase, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_USE_SHORT_CYCLIC_PREFIX, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, short_cyclic_prefix, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_SHORT_PREAMBLE_OPTION_IMPLEMENTED, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, short_preamble_option, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_SHORT_SLOT_TIME_OPTION_ENABLED_11G, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, short_slot_time_option, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_SHORT_RETRY_LIMIT, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, short_retry_limit, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_LONG_RETRY_LIMIT, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, long_retry_limit, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_TX_MSDU_LIFETIME, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, tx_msdu_lifetime, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_CURRENT_TX_ANTENNA, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, current_tx_antenna, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_BEACON_PERIOD, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, beacon_period, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_DISCONNECT_ON_NACKS_WEIGHT, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, disconnect_on_nacks_weight, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_SM_ENABLE, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, sm_enable, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_ADVANCED_CODING_SUPPORTED, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, advanced_coding_supported, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_OVERLAPPING_PROTECTION_ENABLE, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, overlapping_protect_enabled, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_OFDM_PROTECTION_METHOD, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, ofdm_protect_method, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_HT_PROTECTION_METHOD, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, ht_method, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_DTIM_PERIOD, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, dtim_period, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_RECEIVE_AMPDU_MAX_LENGTH, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, receive_ampdu_max_len, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_CB_DATABINS_PER_SYMBOL, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, cb_databins_per_symbol, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_USE_LONG_PREAMBLE_FOR_MULTICAST, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, use_long_preamble_for_multicast, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_USE_SPACE_TIME_BLOCK_CODE, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, use_space_time_block_code, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_ONLINE_CALIBRATION_ALGO_MASK, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, online_calibr_algo_mask, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_DISCONNECT_ON_NACKS_ENABLE, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, disconnect_on_nacks_enable, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_RTS_THRESHOLD, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, rts_threshold, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_TX_POWER, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ITEM(mibs_cfg, tx_power, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(MIB_SUPPORTED_TX_ANTENNAS, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(mibs_cfg, tx_antennas, strncpy,
                                            (mibs_cfg->tx_antennas, data, MTLK_NUM_ANTENNAS_BUFSIZE));

    _DF_USER_SET_ON_PARAM(MIB_SUPPORTED_RX_ANTENNAS, MTLK_CORE_REQ_SET_MIBS_CFG, FALSE, mtlk_mibs_cfg_t, mibs_cfg)
      MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(mibs_cfg, rx_antennas, strncpy,
                                            (mibs_cfg->rx_antennas, data, MTLK_NUM_ANTENNAS_BUFSIZE));

    _DF_USER_SET_ON_PARAM(PRM_ID_HIDDEN_SSID, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, core_cfg)
      MTLK_CFG_SET_ITEM(core_cfg, is_hidden_ssid, (*(uint32*)data)?TRUE:FALSE);

    _DF_USER_SET_ON_PARAM(MIB_COUNTRY, MTLK_CORE_REQ_SET_COUNTRY_CFG, FALSE, mtlk_country_cfg_t, country_cfg)
      MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(country_cfg, country, strncpy,
                                           (country_cfg->country, data, MTLK_CHNLS_COUNTRY_BUFSIZE));

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_RESTRICTED_CHANNELS, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC(aocs_cfg, restricted_channels, _mtlk_df_user_fill_restricted_ch,
                                       (data, aocs_cfg->restricted_channels), res);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_MSDU_TX_AC, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC(aocs_cfg, msdu_tx_ac, _mtlk_df_user_fill_ac_values,
                                      (data, &aocs_cfg->msdu_tx_ac), res);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_MSDU_RX_AC, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC(aocs_cfg, msdu_rx_ac, _mtlk_df_user_fill_ac_values,
                                      (data, &aocs_cfg->msdu_rx_ac), res);

    _DF_USER_SET_ON_PARAM(PRM_ID_AOCS_PENALTIES, MTLK_CORE_REQ_SET_AOCS_CFG, FALSE, mtlk_aocs_cfg_t, aocs_cfg)
      MTLK_CFG_SET_ARRAY_ITEM(aocs_cfg, penalties, (int32*)data, *length, res);

    _DF_USER_SET_ON_PARAM(PRM_ID_L2NAT_AGING_TIMEOUT, MTLK_CORE_REQ_SET_L2NAT_CFG, FALSE, mtlk_l2nat_cfg_t, l2nat_cfg)
      MTLK_CFG_SET_ITEM(l2nat_cfg, aging_timeout, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_L2NAT_DEFAULT_HOST, MTLK_CORE_REQ_SET_L2NAT_CFG, FALSE, mtlk_l2nat_cfg_t, l2nat_cfg)
      MTLK_CFG_SET_ITEM_BY_FUNC(l2nat_cfg, address, _mtlk_df_user_fill_ether_address,
                                (&l2nat_cfg->address, (struct sockaddr*)data), res);

    _DF_USER_SET_ON_PARAM(PRM_ID_11D, MTLK_CORE_REQ_SET_DOT11D_CFG, FALSE, mtlk_dot11d_cfg_t, dot11d_cfg)
      MTLK_CFG_SET_ITEM(dot11d_cfg, is_dot11d, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_11D_RESTORE_DEFAULTS, MTLK_CORE_REQ_SET_DOT11D_CFG, FALSE, mtlk_dot11d_cfg_t, dot11d_cfg)
      MTLK_CFG_SET_ITEM(dot11d_cfg, should_reset_tx_limits, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_MAC_WATCHDOG_TIMEOUT_MS, MTLK_CORE_REQ_SET_MAC_WATCHDOG_CFG, FALSE, mtlk_mac_wdog_cfg_t, mac_wdog_cfg)
      MTLK_CFG_SET_ITEM(mac_wdog_cfg, mac_watchdog_timeout_ms, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_MAC_WATCHDOG_PERIOD_MS, MTLK_CORE_REQ_SET_MAC_WATCHDOG_CFG, FALSE, mtlk_mac_wdog_cfg_t, mac_wdog_cfg)
      MTLK_CFG_SET_ITEM(mac_wdog_cfg, mac_watchdog_period_ms, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_STA_KEEPALIVE_TIMEOUT, MTLK_CORE_REQ_SET_STADB_CFG, FALSE, mtlk_stadb_cfg_t, stadb_cfg)
      MTLK_CFG_SET_ITEM(stadb_cfg, sta_keepalive_timeout, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_STA_KEEPALIVE_INTERVAL, MTLK_CORE_REQ_SET_STADB_CFG, FALSE, mtlk_stadb_cfg_t, stadb_cfg)
      MTLK_CFG_SET_ITEM(stadb_cfg, keepalive_interval, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AGGR_OPEN_THRESHOLD, MTLK_CORE_REQ_SET_STADB_CFG, FALSE, mtlk_stadb_cfg_t, stadb_cfg)
      MTLK_CFG_SET_ITEM(stadb_cfg, aggr_open_threshold, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BRIDGE_MODE, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      MTLK_CFG_SET_ITEM(pcore_cfg, bridge_mode, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_DBG_SW_WD_ENABLE, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      MTLK_CFG_SET_ITEM(pcore_cfg, dbg_sw_wd_enable, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_RELIABLE_MULTICAST, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      MTLK_CFG_SET_ITEM(pcore_cfg, reliable_multicast, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_UP_RESCAN_EXEMPTION_TIME, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      MTLK_CFG_SET_ITEM(pcore_cfg, up_rescan_exemption_time, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_AP_FORWARDING, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      MTLK_CFG_SET_ITEM(pcore_cfg, ap_forwarding, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_SPECTRUM_MODE, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      MTLK_CFG_SET_ITEM(pcore_cfg, spectrum_mode, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_NETWORK_MODE, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      MTLK_CFG_SET_ITEM_BY_FUNC(pcore_cfg, net_mode, _mtlk_df_user_translate_network_mode,
                                (*(uint32*)data, &pcore_cfg->net_mode), res)

    _DF_USER_SET_ON_PARAM(PRM_ID_CHANNEL, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      MTLK_CFG_SET_ITEM(pcore_cfg, channel, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_POWER_SELECTION, MTLK_CORE_REQ_SET_MASTER_CFG, FALSE, mtlk_master_core_cfg_t, master_core_cfg)
      MTLK_CFG_SET_ITEM(master_core_cfg, power_selection, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BSS_BASIC_RATE_SET, MTLK_CORE_REQ_SET_MASTER_AP_CFG, FALSE, mtlk_master_ap_core_cfg_t, master_ap_core_cfg)
      MTLK_CFG_SET_ITEM(master_ap_core_cfg, bss_rate, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_NICK_NAME, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      MTLK_ASSERT(sizeof(pcore_cfg->nickname) >= *length);
      MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(pcore_cfg, nickname, memcpy,
                                            (pcore_cfg->nickname, data, *length));

    _DF_USER_SET_ON_PARAM(PRM_ID_ESSID, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      MTLK_ASSERT(sizeof(pcore_cfg->essid) >= *length);
      MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(pcore_cfg, essid, memcpy,
                                            (pcore_cfg->essid, data, *length));

    _DF_USER_SET_ON_PARAM(PRM_ID_LEGACY_FORCE_RATE, MTLK_CORE_REQ_SET_MASTER_CFG, FALSE, mtlk_master_core_cfg_t, pmaster_cfg)
      data[*length - 1] = '\0'; /* force string null-termination */
      MTLK_CFG_SET_ITEM_BY_FUNC(pmaster_cfg, legacy_force_rate, _mtlk_df_user_parse_bitrate_str,
                                            (data, &pmaster_cfg->legacy_force_rate), res);

    _DF_USER_SET_ON_PARAM(PRM_ID_HT_FORCE_RATE, MTLK_CORE_REQ_SET_MASTER_CFG, FALSE, mtlk_master_core_cfg_t, pmaster_cfg)
      data[*length - 1] = '\0'; /* force string null-termination */
      MTLK_CFG_SET_ITEM_BY_FUNC(pmaster_cfg, ht_force_rate, _mtlk_df_user_parse_bitrate_str,
                                            (data, &pmaster_cfg->ht_force_rate), res);

    _DF_USER_SET_ON_PARAM(PRM_ID_ACL, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      int i;
      MTLK_CFG_SET_ITEM(pcore_cfg, num_macs_to_set, *length);
      for (i = 0; (i < pcore_cfg->num_macs_to_set) && (i < MAX_ADDRESSES_IN_ACL); i++) {
        MTLK_CFG_SET_ITEM_BY_FUNC_VOID(pcore_cfg, macs_to_set, memcpy,
          (pcore_cfg->macs_to_set[i].au8Addr, ((struct sockaddr*)data)[i].sa_data, sizeof(pcore_cfg->macs_to_set[i].au8Addr)));
      }

    _DF_USER_SET_ON_PARAM(PRM_ID_ACL_RANGE, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      int i;

      if (0 != (*length % 2)) {
        ELOG_D("Address vector length should be even. length(%u)", *length);
        res = MTLK_ERR_PARAMS;
      } else if ((*length >> 1) > MAX_ADDRESSES_IN_ACL) {
        ELOG_D("Address vector length should be not more than %u address/mask pairs", MAX_ADDRESSES_IN_ACL);
        res = MTLK_ERR_PARAMS;
      } else {
        MTLK_CFG_SET_ITEM(pcore_cfg, num_macs_to_set, (*length) >> 1);
        for (i = 0; i < pcore_cfg->num_macs_to_set; i++) {
          MTLK_CFG_SET_ITEM_BY_FUNC_VOID(pcore_cfg, macs_to_set, memcpy,
            (pcore_cfg->macs_to_set[i].au8Addr, ((struct sockaddr*)data)[i << 1].sa_data, sizeof(pcore_cfg->macs_to_set[i].au8Addr)));
          MTLK_CFG_SET_ITEM_BY_FUNC_VOID(pcore_cfg, mac_mask, memcpy,
            (pcore_cfg->mac_mask[i].au8Addr, ((struct sockaddr*)data)[(i << 1) + 1].sa_data, sizeof(pcore_cfg->mac_mask[i].au8Addr)));
        }
      }

    _DF_USER_SET_ON_PARAM(PRM_ID_ACL_DEL, MTLK_CORE_REQ_SET_CORE_CFG, FALSE, mtlk_gen_core_cfg_t, pcore_cfg)
      int i;
      MTLK_CFG_SET_ITEM(pcore_cfg, num_macs_to_del, *length);
      for (i = 0; (i < pcore_cfg->num_macs_to_del) && (i < MAX_ADDRESSES_IN_ACL); i++) {
        MTLK_CFG_SET_ITEM_BY_FUNC_VOID(pcore_cfg, macs_to_del, memcpy,
          (pcore_cfg->macs_to_del[i].au8Addr, ((struct sockaddr*)data)[i].sa_data, sizeof(pcore_cfg->macs_to_del[i].au8Addr)));
      }

    _DF_USER_SET_ON_PARAM(PRM_ID_ACTIVE_SCAN_SSID, MTLK_CORE_REQ_SET_SCAN_CFG, FALSE, mtlk_scan_cfg_t, scan_cfg)
      data[*length - 1] = '\0'; /* force string null-termination */
      MTLK_CFG_SET_ITEM_BY_FUNC_VOID(scan_cfg, essid, strncpy,
                                     (scan_cfg->essid, data, MIB_ESSID_LENGTH + 1));
      scan_cfg->essid[MIB_ESSID_LENGTH] = '\0';

    _DF_USER_SET_ON_PARAM(PRM_ID_HW_LIMITS, MTLK_CORE_REQ_SET_HW_DATA_CFG, FALSE, mtlk_hw_data_cfg_t, hw_data_cfg)
      data[*length - 1] = '\0';
      MTLK_CFG_SET_ITEM_BY_FUNC(hw_data_cfg, hw_cfg, _mtlk_df_user_fill_hw_cfg,
                                (&hw_data_cfg->hw_cfg, data), res);

    _DF_USER_SET_ON_PARAM(PRM_ID_ANT_GAIN, MTLK_CORE_REQ_SET_HW_DATA_CFG, FALSE, mtlk_hw_data_cfg_t, hw_data_cfg)
      data[*length - 1] = '\0';
      MTLK_CFG_SET_ITEM_BY_FUNC(hw_data_cfg, ant, _mtlk_df_user_fill_ant_cfg,
                                (&hw_data_cfg->ant, data), res);

    _DF_USER_SET_ON_PARAM(PRM_ID_CHANGE_TX_POWER_LIMIT, MTLK_CORE_REQ_SET_HW_DATA_CFG, FALSE, mtlk_hw_data_cfg_t, hw_data_cfg)
      MTLK_CFG_SET_ITEM_BY_FUNC(hw_data_cfg, power_limit, _mtlk_df_user_fill_power_limit_cfg,
                                (&hw_data_cfg->power_limit, *(uint32*)data), res);

    _DF_USER_SET_ON_PARAM(PRM_ID_WDS_HOST_TIMEOUT, MTLK_CORE_REQ_SET_HSTDB_CFG, FALSE, mtlk_hstdb_cfg_t, hstdb_cfg)
      MTLK_CFG_SET_ITEM(hstdb_cfg, wds_host_timeout, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_HSTDB_LOCAL_MAC, MTLK_CORE_REQ_SET_HSTDB_CFG, FALSE, mtlk_hstdb_cfg_t, hstdb_cfg)
      MTLK_CFG_SET_ITEM_BY_FUNC(hstdb_cfg, address, _mtlk_df_user_fill_ether_address, (&hstdb_cfg->address, (struct sockaddr*)data), res);

    _DF_USER_SET_ON_PARAM(PRM_ID_SCAN_CACHE_LIFETIME, MTLK_CORE_REQ_SET_SCAN_CFG, FALSE, mtlk_scan_cfg_t, scan_cfg)
      MTLK_CFG_SET_ITEM(scan_cfg, cache_expire, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BG_SCAN_CH_LIMIT, MTLK_CORE_REQ_SET_SCAN_CFG, FALSE, mtlk_scan_cfg_t, scan_cfg)
      MTLK_CFG_SET_ITEM(scan_cfg, channels_per_chunk_limit, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_BG_SCAN_PAUSE, MTLK_CORE_REQ_SET_SCAN_CFG, FALSE, mtlk_scan_cfg_t, scan_cfg)
      MTLK_CFG_SET_ITEM(scan_cfg, pause_between_chunks, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_IS_BACKGROUND_SCAN, MTLK_CORE_REQ_SET_SCAN_CFG, FALSE, mtlk_scan_cfg_t, scan_cfg)
      MTLK_CFG_SET_ITEM(scan_cfg, is_background_scan, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_USE_8021Q, MTLK_CORE_REQ_SET_QOS_CFG, FALSE, mtlk_qos_cfg_t, qos_cfg)
      MTLK_CFG_SET_ITEM(qos_cfg, map, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_COC_LOW_POWER_MODE, MTLK_CORE_REQ_SET_COC_CFG, FALSE, mtlk_coc_mode_cfg_t, coc_cfg)
      MTLK_CFG_SET_ITEM(coc_cfg, is_enabled, *(uint32*)data);

    _DF_USER_SET_ON_PARAM(PRM_ID_SQ_LIMITS, MTLK_CORE_REQ_SET_SQ_CFG, FALSE, mtlk_sq_cfg_t, sq_cfg)
      MTLK_CFG_SET_ARRAY_ITEM(sq_cfg, sq_limit, (int32*)data, *length, res);

    _DF_USER_SET_ON_PARAM(PRM_ID_SQ_PEER_LIMITS, MTLK_CORE_REQ_SET_SQ_CFG, FALSE, mtlk_sq_cfg_t, sq_cfg)
      MTLK_CFG_SET_ARRAY_ITEM(sq_cfg, peer_queue_limit, (int32*)data, *length, res);
    
    /* 20/40 coexistence */

    _DF_USER_SET_ON_PARAM(PRM_ID_COEX_MODE, MTLK_CORE_REQ_SET_COEX_20_40_MODE_CFG, FALSE, mtlk_coex_20_40_mode_cfg_t, coex20_40_cfg)
      MTLK_CFG_SET_ITEM(coex20_40_cfg, coexistence_mode, *(uint32*)data);
     /*ILOG0_D("FUNCTION Value in core = %d\n", *(uint32*)data);*/

    _DF_USER_SET_ON_PARAM(PRM_ID_INTOLERANCE_MODE, MTLK_CORE_REQ_SET_COEX_20_40_MODE_CFG, FALSE, mtlk_coex_20_40_mode_cfg_t, coex20_40_cfg)
      MTLK_CFG_SET_ITEM(coex20_40_cfg, intolerance_mode, *(uint32*)data);
     /*ILOG0_D("FUNCTION Value in core = %d\n", *(uint32*)data);*/

    _DF_USER_SET_ON_PARAM(PRM_ID_EXEMPTION_REQ, MTLK_CORE_REQ_SET_COEX_20_40_EXM_REQ_CFG, FALSE, mtlk_coex_20_40_exm_req_cfg_t, coex20_40_cfg)
      MTLK_CFG_SET_ITEM(coex20_40_cfg, exemption_req, *(uint32*)data);
      /*ILOG0_D("FUNCTION Value in core = %d\n", *(uint32*)data);*/

    _DF_USER_SET_ON_PARAM(PRM_ID_DELAY_FACTOR, MTLK_CORE_REQ_SET_COEX_20_40_TIMES_CFG, FALSE, mtlk_coex_20_40_times_cfg_t, coex20_40_cfg)
      MTLK_CFG_SET_ITEM(coex20_40_cfg, delay_factor, *(uint32*)data);
      /*ILOG0_D("FUNCTION Value in core = %d\n", *(uint32*)data);*/

    _DF_USER_SET_ON_PARAM(PRM_ID_OBSS_SCAN_INTERVAL, MTLK_CORE_REQ_SET_COEX_20_40_TIMES_CFG, FALSE, mtlk_coex_20_40_times_cfg_t, coex20_40_cfg)
      MTLK_CFG_SET_ITEM(coex20_40_cfg, obss_scan_interval, *(uint32*)data);
      /*ILOG0_D("FUNCTION Value in core = %d\n", *(uint32*)data);*/

    _DF_USER_SET_ON_PARAM(PRM_ID_CFG_LED_GPIO, MTLK_CORE_REQ_SET_FW_LED_CFG, FALSE, mtlk_fw_led_cfg_t, fw_led_cfg)
      MTLK_CFG_SET_ITEM_BY_FUNC(fw_led_cfg, gpio_cfg,
                                _mtlk_df_user_get_fw_gpio_cfg_by_intvec, ((uint32*)data, *length, &fw_led_cfg->gpio_cfg), res);

    _DF_USER_SET_ON_PARAM(PRM_ID_CFG_LED_STATE, MTLK_CORE_REQ_SET_FW_LED_CFG, FALSE, mtlk_fw_led_cfg_t, fw_led_cfg)
      MTLK_CFG_SET_ITEM_BY_FUNC(fw_led_cfg, led_state,
                                _mtlk_df_user_get_fw_led_state_by_intvec,  ((uint32*)data, *length, &fw_led_cfg->led_state), res);

  _DF_USER_SET_PARAM_MAP_END()

  return res;
}

static int
_mtlk_ioctl_drv_gen_data_exchange (mtlk_df_user_t* df_user, struct ifreq *ifr)
{
  int res = MTLK_ERR_OK;
  mtlk_core_ui_gen_data_t req;
  WE_GEN_DATAEX_RESPONSE resp;
  mtlk_clpb_t *clpb = NULL;

  WE_GEN_DATAEX_DEVICE_STATUS *dev_status = NULL;
  WE_GEN_DATAEX_CONNECTION_STATUS *con_status = NULL;
  WE_GEN_DATAEX_STATUS *status = NULL;
  WE_GEN_DATAEX_RESPONSE *core_resp = NULL;
  uint32 core_resp_size = 0;

  /* response, should be placed first to user buffer */
  void *out = ifr->ifr_data + sizeof(WE_GEN_DATAEX_RESPONSE);
  const char *reason = "system error";

  /* Make sure no fields will be left uninitialized by command handler */
  memset(&req, 0, sizeof(req));
  memset(&resp, 0, sizeof(resp));

  if (0 != copy_from_user(&req.request, ifr->ifr_data, sizeof(req.request))) {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  /* error case - copy response with error status and needed protocol version to user space */
  if (WE_GEN_DATAEX_PROTO_VER != req.request.ver) {
    uint32 proto_ver = WE_GEN_DATAEX_PROTO_VER;

    WLOG_DD("Data exchange protocol version mismatch (version is %u, expected %u)",
            req.request.ver,
            WE_GEN_DATAEX_PROTO_VER);

    resp.status = WE_GEN_DATAEX_PROTO_MISMATCH;
    resp.datalen = sizeof(uint32);
    
    if (0 != copy_to_user(out, &proto_ver, sizeof(uint32))) {
      res = MTLK_ERR_PARAMS;
    }
    goto finish;
  }

  /* in case of MAC leds additional parameters are needed from userspace */
  if (WE_GEN_DATAEX_CMD_LEDS_MAC == req.request.cmd_id) {
    if (0 != copy_from_user(&req.leds_status,
                            ifr->ifr_data + sizeof(WE_GEN_DATAEX_REQUEST),
                            sizeof(req.leds_status))) {
      res = MTLK_ERR_PARAMS;
      goto finish;
    }
  }

  res = _mtlk_df_user_invoke_core(df_user->df,
                                 MTLK_CORE_REQ_GEN_DATA_EXCHANGE,
                                 &clpb, &req, sizeof(req));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GEN_DATA_EXCHANGE, FALSE);

  if (MTLK_ERR_OK != res) {
    goto finish;
  }

  switch (req.request.cmd_id) {
    case WE_GEN_DATAEX_CMD_CONNECTION_STATS:
    {
      /* step - WE_GEN_DATAEX_CONNECTION_STATUS should be copied first 
       * to userspace */
      void *tmp_out = out;
      out += sizeof(WE_GEN_DATAEX_CONNECTION_STATUS);

      MTLK_ASSERT(sizeof(WE_GEN_DATAEX_DEVICE_STATUS) !=
                  sizeof(WE_GEN_DATAEX_CONNECTION_STATUS));

      MTLK_ASSERT(sizeof(WE_GEN_DATAEX_DEVICE_STATUS) !=
                  sizeof(WE_GEN_DATAEX_RESPONSE));

      MTLK_ASSERT(sizeof(WE_GEN_DATAEX_CONNECTION_STATUS) !=
                  sizeof(WE_GEN_DATAEX_RESPONSE));

      /* retrieve several WE_GEN_DATAEX_DEVICE_STATUS from clipboard 
       * and copy them to userspace, these structures may be not present
       * in clipboard in case of no STA connected to AP */
      dev_status = mtlk_clpb_enum_get_next(clpb, &core_resp_size);

      while (sizeof(WE_GEN_DATAEX_DEVICE_STATUS) == core_resp_size) {
        if (0 != copy_to_user(out, dev_status, core_resp_size)) {
          res = MTLK_ERR_VALUE;
          goto process_res;
        }
        out += sizeof(WE_GEN_DATAEX_DEVICE_STATUS); 
        dev_status = mtlk_clpb_enum_get_next(clpb, &core_resp_size);
      }
    
      MTLK_ASSERT(core_resp_size != sizeof(WE_GEN_DATAEX_RESPONSE));
      MTLK_ASSERT(core_resp_size == sizeof(WE_GEN_DATAEX_CONNECTION_STATUS));

      /* WE_GEN_DATAEX_CONNECTION_STATUS must be placed after WE_GEN_DATAEX_DEVICE_STATUS
       * in clipboard, copy it to userspace using previously stored pointer tmp_out! */
      con_status = (WE_GEN_DATAEX_CONNECTION_STATUS*)dev_status;

      MTLK_ASSERT(NULL != con_status);

      if (0 != copy_to_user(tmp_out, con_status, core_resp_size)) {
        res = MTLK_ERR_VALUE;
        goto process_res;
      }

      /* retrieve WE_GEN_DATAEX_RESPONSE from clipboard 
       * it will be copied to userspace at the end of function */
      core_resp = mtlk_clpb_enum_get_next(clpb, &core_resp_size);

      MTLK_ASSERT(NULL != core_resp);
      MTLK_ASSERT(sizeof(*core_resp) == core_resp_size);
      
      resp = *core_resp;
    }
    break;
    case WE_GEN_DATAEX_CMD_STATUS:
    {
      /* retrieve several WE_GEN_DATAEX_STATUS from clipboard 
       * and copy them to userspace */
      status = mtlk_clpb_enum_get_next(clpb, &core_resp_size);

      MTLK_ASSERT(NULL != status);
      MTLK_ASSERT(sizeof(*status) == core_resp_size);
      
      if (0 != copy_to_user(out, status, core_resp_size)) {
          res = MTLK_ERR_VALUE;
          goto process_res;
        }

      /* retrieve WE_GEN_DATAEX_RESPONSE from clipboard 
       * it will be copied to userspace at the end of function */
      core_resp = mtlk_clpb_enum_get_next(clpb, &core_resp_size);

      MTLK_ASSERT(NULL != core_resp);
      MTLK_ASSERT(sizeof(*core_resp) == core_resp_size);
      
      resp = *core_resp;
    }
    break;
    case WE_GEN_DATAEX_CMD_LEDS_MAC:
    {
      /* retrieve WE_GEN_DATAEX_RESPONSE from clipboard 
       * it will be copied to userspace at the end of function */
      core_resp = mtlk_clpb_enum_get_next(clpb, &core_resp_size);

      MTLK_ASSERT(NULL != core_resp);
      MTLK_ASSERT(sizeof(*core_resp) == core_resp_size);
      
      resp = *core_resp;
    }
    break;
    default:
      WLOG_D("Data exchange protocol: unknown command %u", req.request.cmd_id);
      resp.status = WE_GEN_DATAEX_UNKNOWN_CMD;
      resp.datalen = 0;
      break;
   }

  if (resp.status == WE_GEN_DATAEX_FAIL) {
    /* Return failure reason */
    size_t reason_sz = strlen(reason) + 1;
    if (req.request.datalen >= reason_sz) {
      if (0 != copy_to_user(ifr->ifr_data + sizeof(WE_GEN_DATAEX_RESPONSE), reason, reason_sz)) {
        res = MTLK_ERR_VALUE;
        goto process_res;
      }
      resp.datalen = reason_sz;
    }
    else {
      resp.datalen = 0; /* error string does not fit */
    }
  }

  /* copy previously filled WE_GEN_DATAEX_RESPONSE */
  if (copy_to_user(ifr->ifr_data, &resp, sizeof(resp)) != 0) {
    res = MTLK_ERR_VALUE;
  }

process_res:
  mtlk_clpb_delete(clpb);

finish:
  if (MTLK_ERR_OK != res) {
    ELOG_D("Error#%d during data exchange request", res);
  }

  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int
mtlk_df_do_command (struct net_device *dev, struct ifreq *ifr, int cmd)
{
  mtlk_df_user_t* df_user = (mtlk_df_user_t*)netdev_priv(dev);
  int res = -EOPNOTSUPP;

  ILOG3_SSDD("%s: Invoked from %s (%i), cmd is 0x%04x", dev->name, current->comm, current->pid, cmd);

  /* we only support private ioctls */
  if ((cmd < MTLK_IOCTL_FIRST) || (cmd > MTLK_IOCTL_LAST)) {
    goto FINISH;
  }

  switch (cmd) {
  case MTLK_IOCTL_DATAEX:
    res = _mtlk_ioctl_drv_gen_data_exchange(df_user, ifr);
    break;
  default:
    ELOG_D("ioctl not supported: 0x%04x", cmd);
    break;
  }

FINISH:
  return res;
}

static void
_df_user_copy_linux_stats(struct net_device_stats* linux_stats,
                          mtlk_core_general_stats_t* core_status)
{
  linux_stats->rx_packets     = core_status->rx_packets;
  linux_stats->tx_packets     = core_status->tx_packets - core_status->core_priv_stats.fwd_tx_packets;
  linux_stats->rx_bytes       = core_status->rx_bytes;
  linux_stats->tx_bytes       = core_status->tx_bytes - core_status->core_priv_stats.fwd_tx_bytes;
  linux_stats->tx_dropped     = core_status->mac_stat.stat[STAT_TX_FAIL]; /* total tx packets dropped */
  linux_stats->collisions     = core_status->mac_stat.stat[STAT_TX_RETRY]; /* total tx retries */
  linux_stats->rx_fifo_errors = core_status->mac_stat.stat[STAT_OUT_OF_RX_MSDUS]; /* total rx queue overruns */
  linux_stats->tx_fifo_errors = core_status->core_priv_stats.tx_overruns;
}

static struct net_device_stats *
_mtlk_df_user_get_stats(struct net_device *dev)
{
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);
  return &df_user->slow_ctx->linux_stats;
}

static void __MTLK_IFUNC
_mtlk_df_poll_stats_clb(mtlk_handle_t user_context,
                        int           processing_result,
                        mtlk_clpb_t  *pclpb)
{
  int res;
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) user_context;

  res = _mtlk_df_user_process_core_retval_void(processing_result, pclpb, MTLK_CORE_REQ_GET_STATUS, FALSE);

  if(MTLK_ERR_OK == res)
  {
    uint32 size;

    mtlk_core_general_stats_t* core_status =
      (mtlk_core_general_stats_t*) mtlk_clpb_enum_get_next(pclpb, &size);

    MTLK_ASSERT(NULL != core_status);
    MTLK_ASSERT(sizeof(*core_status) == size);

    df_user->slow_ctx->core_general_stats = *core_status;

    _df_user_copy_linux_stats(&df_user->slow_ctx->linux_stats, core_status);
  }
  else
  {
    memset(&df_user->slow_ctx->linux_stats, 0, 
           sizeof(df_user->slow_ctx->linux_stats));
  }

  mtlk_osal_timer_set(&df_user->slow_ctx->stat_timer, _DF_STAT_POLL_PERIOD);
}

static uint32
_mtlk_df_poll_stats (mtlk_osal_timer_t *timer, mtlk_handle_t data)
{
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) data;

  _mtlk_df_user_invoke_core_async(df_user->df, MTLK_CORE_REQ_GET_STATUS,
                                  NULL, 0, _mtlk_df_poll_stats_clb, HANDLE_T(df_user));

  return 0;
}

static int
_mtlk_df_user_prevent_change_mtu(struct net_device *dev, int new_mtu)
{
  return -EFAULT;
}

static void _mtlk_df_user_fill_callbacks(mtlk_df_user_t *df_user)
{
  df_user->dev->wireless_handlers = (struct iw_handler_def *)&mtlk_linux_handler_def;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
  df_user->dev->hard_start_xmit = _mtlk_df_user_start_tx;
  df_user->dev->open            = _mtlk_df_user_iface_open;
  df_user->dev->stop            = _mtlk_df_user_iface_stop;
  df_user->dev->set_mac_address = _mtlk_df_user_set_mac_addr;
  df_user->dev->get_stats       = _mtlk_df_user_get_stats;

  df_user->dev->tx_timeout      = NULL;
  df_user->dev->change_mtu      = _mtlk_df_user_prevent_change_mtu;

  df_user->dev->do_ioctl        = mtlk_df_do_command;
#else
  df_user->dev_ops.ndo_start_xmit      = _mtlk_df_user_start_tx;
  df_user->dev_ops.ndo_open            = _mtlk_df_user_iface_open;
  df_user->dev_ops.ndo_stop            = _mtlk_df_user_iface_stop;
  df_user->dev_ops.ndo_set_mac_address = _mtlk_df_user_set_mac_addr;
  df_user->dev_ops.ndo_get_stats       = _mtlk_df_user_get_stats;

  df_user->dev_ops.ndo_tx_timeout      = NULL;
  df_user->dev_ops.ndo_change_mtu      = _mtlk_df_user_prevent_change_mtu;

  df_user->dev_ops.ndo_do_ioctl        = mtlk_df_do_command;

  df_user->dev->netdev_ops             = &df_user->dev_ops;
#endif

  df_user->dev->destructor             = free_netdev;
}

static int _mtlk_df_ui_create_card_dir(mtlk_df_user_t* df_user)
{
  MTLK_ASSERT(NULL != df_user);

  df_user->slow_ctx->proc_df_node =
      mtlk_df_proc_node_create(mtlk_df_user_get_name(df_user), mtlk_dfg_get_drv_proc_node());

  if (NULL == df_user->slow_ctx->proc_df_node) {
    return MTLK_ERR_NO_MEM;
  }
  return MTLK_ERR_OK;
}

static int _mtlk_df_ui_create_debug_dir(mtlk_df_user_t* df_user)
{
  MTLK_ASSERT(NULL != df_user);

  df_user->slow_ctx->proc_df_debug_node =
      mtlk_df_proc_node_create("Debug", df_user->slow_ctx->proc_df_node);

  if (NULL == df_user->slow_ctx->proc_df_debug_node) {
    return MTLK_ERR_NO_MEM;
  }
  return MTLK_ERR_OK;
}

static void _mtlk_df_ui_delete_card_dir(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_delete(df_user->slow_ctx->proc_df_node);
  df_user->slow_ctx->proc_df_node = NULL;
}

static void _mtlk_df_ui_delete_debug_dir(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_delete(df_user->slow_ctx->proc_df_debug_node);
  df_user->slow_ctx->proc_df_debug_node = NULL;
}

static void
_mtlk_df_user_unregister_ndev(mtlk_df_user_t *df_user, mtlk_vap_manager_interface_e intf)
{
  switch(intf) {
    case MTLK_VAP_MASTER_INTERFACE:
      /* master interface can be removed only in case of driver remove (rmmod),
       * so no need to check is slave or not here
       * this call holds rtnl_lock() */
      unregister_netdev(df_user->dev);
      break;

    case MTLK_VAP_SLAVE_INTERFACE:
      /* doesn't hold rtnl_lock(), it's already held, because we are in ioctl now */
      unregister_netdevice(df_user->dev);
      break;

    default:
      MTLK_ASSERT(0);
  }
}

static int
_mtlk_df_user_register_ndev(mtlk_df_user_t *df_user, mtlk_vap_manager_interface_e intf)
{
  int res = -EINVAL;
  switch(intf) {
    case MTLK_VAP_MASTER_INTERFACE:
      /* master interface can be removed only in case of driver remove (rmmod),
       * so no need to check is slave or not here
       * this call holds rtnl_lock() */
      res = register_netdev(df_user->dev);
      break;

    case MTLK_VAP_SLAVE_INTERFACE:
      /* doesn't hold rtnl_lock(), it's already held, because we are in ioctl now */
      res = register_netdevice(df_user->dev);

      /* we don't need to play rtnl_lock/rtnl_unlock here -
       * there is no "todo's" scheduled in kernel on Network Device registration
       */
      break;
    default:
      MTLK_ASSERT(0);
  }

  return (0 == res) ? MTLK_ERR_OK : MTLK_ERR_UNKNOWN;
}

MTLK_START_STEPS_LIST_BEGIN(df_user)
  MTLK_START_STEPS_LIST_ENTRY(df_user, REGISTER_DEVICE)
  MTLK_START_STEPS_LIST_ENTRY(df_user, STAT_POLL)
  MTLK_START_STEPS_LIST_ENTRY(df_user, PPA_XFACE)
MTLK_START_INNER_STEPS_BEGIN(df_user)
MTLK_START_STEPS_LIST_END(df_user);

void
mtlk_df_user_stop(mtlk_df_user_t *df_user, mtlk_vap_manager_interface_e intf)
{
#ifdef MTCFG_IRB_DEBUG
  if (mtlk_irb_pinger_is_started(&df_user->slow_ctx->pinger)) {
    mtlk_irb_pinger_stop(&df_user->slow_ctx->pinger);
  }
#endif /* MTCFG_IRB_DEBUG */

  MTLK_STOP_BEGIN(df_user, MTLK_OBJ_PTR(df_user))
    MTLK_STOP_STEP(df_user, PPA_XFACE, MTLK_OBJ_PTR(df_user),
                   _mtlk_df_priv_xface_stop, (df_user));
    MTLK_STOP_STEP(df_user, STAT_POLL, MTLK_OBJ_PTR(df_user),
                   mtlk_osal_timer_cancel_sync, (&df_user->slow_ctx->stat_timer));
    MTLK_STOP_STEP(df_user, REGISTER_DEVICE, MTLK_OBJ_PTR(df_user),
                   _mtlk_df_user_unregister_ndev, (df_user, intf));
  MTLK_STOP_END(df_user, MTLK_OBJ_PTR(df_user));
}

int
mtlk_df_user_start(mtlk_df_t *df, mtlk_df_user_t *df_user, mtlk_vap_manager_interface_e intf)
{
  /* From time we've allocated device name till time we register netdevice
   * we should hold rtnl lock to prohibit someone else from registering same netdevice name.
   * We can't use register_netdev, because we've splitted netdevice registration into 2 phases:
   * 1) allocate name
   * ... driver internal initialization
   * 2) register.
   * We need this because:
   * 1) initialization (registration of proc entries) requires knowledge of netdevice name
   * 2) we can't register netdevice before we have initialized the driver (we might crash on
   * request from the OS)
   *
   * NEW APPROACH: Now the Net Device name is allocated on DF UI initialization in assumption
   * that no one else will register the same device name.
   *  - DF infrastructure has been created already on DF UI initialization.
   *  - register_netdev() API can used here from now
   *  - It is not needed to take rtnl_lock manually
   *    This approach allows Core to register abilities correctly before Net Device registration.
   */

  MTLK_START_TRY(df_user, MTLK_OBJ_PTR(df_user))
    MTLK_START_STEP(df_user, REGISTER_DEVICE, MTLK_OBJ_PTR(df_user),
                       _mtlk_df_user_register_ndev, (df_user, intf));

    MTLK_START_STEP(df_user, STAT_POLL, MTLK_OBJ_PTR(df_user),
                       mtlk_osal_timer_set, 
                       (&df_user->slow_ctx->stat_timer, _DF_STAT_POLL_PERIOD));

    MTLK_START_STEP_VOID(df_user, PPA_XFACE, MTLK_OBJ_PTR(df_user),
                         MTLK_NOACTION, ());

  MTLK_START_FINALLY(df_user, MTLK_OBJ_PTR(df_user))
  MTLK_START_RETURN(df_user, MTLK_OBJ_PTR(df_user), mtlk_df_user_stop, (df_user, intf))
}

int __MTLK_IFUNC
mtlk_df_ui_indicate_rx_data(mtlk_df_t *df, mtlk_nbuf_t *nbuf)
{
  int res;
  mtlk_df_user_t *df_user;

  MTLK_ASSERT(NULL != df);
  MTLK_ASSERT(NULL != nbuf);

  df_user = mtlk_df_get_user(df);

  mtlk_nbuf_stop_tracking(nbuf);
  mtlk_nbuf_priv_cleanup(mtlk_nbuf_priv(nbuf));

  /* set pointer to dev, nbuf->protocol for PPA case will be set within PPA RX call back */
  nbuf->dev = df_user->dev;

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH 
  if (ppa_hook_directpath_send_fn && df_user->ppa.clb.rx_fn) {
    /* set raw pointer for proper work if directpath is disabled */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
    skb_reset_mac_header(nbuf);
#else 
    nbuf->mac.raw = nbuf->data;
#endif

    /* send packets to the ppa */
    if (ppa_hook_directpath_send_fn(df_user->ppa.if_id,
                                    nbuf, nbuf->len, 0) == IFX_SUCCESS) {
      ++df_user->ppa.stats.rx_accepted;
      return MTLK_ERR_OK;
    } 

    ++df_user->ppa.stats.rx_rejected;
    return MTLK_ERR_UNKNOWN;
  }
#endif /* CONFIG_IFX_PPA_API_DIRECTPATH */ 

  nbuf->protocol        = eth_type_trans(nbuf, nbuf->dev);
  res                   = netif_rx(nbuf);
  df_user->dev->last_rx = jiffies;

  if(NET_RX_SUCCESS != res)
  {
    ILOG2_D("netif_rx failed: %d", res);
    return MTLK_ERR_UNKNOWN;
  }

  return MTLK_ERR_OK;
}

BOOL __MTLK_IFUNC mtlk_df_ui_check_is_mc_group_member(mtlk_df_t *df,
                                                  const uint8* group_addr)
{
  mtlk_df_user_t *df_user;

  MTLK_ASSERT(NULL != df);
  MTLK_ASSERT(NULL != group_addr);

  df_user = mtlk_df_get_user(df);

  //check if we subscribed to all multicast
  if (df_user->dev->allmulti)
    return TRUE;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
  {
    struct dev_mc_list* mc_list_entry;
    //check if we subscribed to that group
    for (mc_list_entry = df_user->dev->mc_list;
         mc_list_entry != NULL;
         mc_list_entry = mc_list_entry->next) {
        if (0 == mtlk_osal_compare_eth_addresses(mc_list_entry->dmi_addr, group_addr))
          return TRUE;
    }
  }
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35) */
  {
    struct netdev_hw_addr *ha;
      netdev_for_each_mc_addr(ha, df_user->dev) {
        if (!mtlk_osal_compare_eth_addresses(ha->addr, group_addr))
          return TRUE;
    }
  }
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35) */

  return FALSE;
}

static void
_mtlk_df_user_notify_mac_addr_event( mtlk_df_user_t *df_user, 
                                     int event_id,
                                     const uint8 *mac )
{
  union iwreq_data req_data;

  MTLK_ASSERT(NULL != df_user);
  MTLK_ASSERT(NULL != mac);

  req_data.ap_addr.sa_family = ARPHRD_ETHER;
  mtlk_osal_copy_eth_addresses(req_data.ap_addr.sa_data, mac);

  wireless_send_event(df_user->dev, event_id, &req_data, NULL);
}

void __MTLK_IFUNC 
mtlk_df_ui_notify_association( mtlk_df_t *df,
                                 const uint8 *mac )
{
  _mtlk_df_user_notify_mac_addr_event(mtlk_df_get_user(df), SIOCGIWAP, mac);
}

void __MTLK_IFUNC
mtlk_df_ui_notify_disassociation(mtlk_df_t *df)
{
  static const uint8 mac[ETH_ALEN] = {0};
  _mtlk_df_user_notify_mac_addr_event(mtlk_df_get_user(df), SIOCGIWAP, mac);
}

void __MTLK_IFUNC
mtlk_df_ui_notify_node_connect(mtlk_df_t *df, const uint8 *node_addr)
{
  _mtlk_df_user_notify_mac_addr_event(mtlk_df_get_user(df), IWEVREGISTERED, node_addr);
}

void __MTLK_IFUNC
mtlk_df_ui_notify_node_disconect(mtlk_df_t *df, const uint8 *node_addr)
{
  _mtlk_df_user_notify_mac_addr_event(mtlk_df_get_user(df), IWEVEXPIRED, node_addr);
}

void __MTLK_IFUNC
mtlk_df_ui_notify_secure_node_connect(mtlk_df_t *df,
                                        const uint8 *node_addr,
                                        const uint8 *rsnie,
                                        size_t rsnie_len)
{
  union iwreq_data wrqu;
  uint8 buf[IW_CUSTOM_MAX] = {0};
  uint8 *p=buf;
  size_t i;

  MTLK_ASSERT(NULL != df);

  p += sprintf(p, "NEWSTA " MAC_PRINTF_FMT ", RSNIE_LEN %i : ",
               MAC_PRINTF_ARG(node_addr), rsnie_len);

  MTLK_ASSERT(buf - p + rsnie_len*2 < IW_CUSTOM_MAX);

  for (i = 0; i < rsnie_len; i++)
    p += sprintf(p, "%02x", rsnie[i]);

  wrqu.data.length = p - buf +1;

  wireless_send_event(mtlk_df_get_user(df)->dev, IWEVCUSTOM, &wrqu, buf);
}

void __MTLK_IFUNC
mtlk_df_ui_notify_mic_failure(mtlk_df_t *df,
                                const uint8 *src_mac, 
                                mtlk_df_ui_mic_fail_type_t fail_type)
{
  union iwreq_data wrqu;
  struct iw_michaelmicfailure mic = {0};

  MTLK_ASSERT(NULL != df);
  MTLK_ASSERT(NULL != src_mac);

  wrqu.data.length = sizeof(struct iw_michaelmicfailure);

  mic.src_addr.sa_family = ARPHRD_ETHER;
  mtlk_osal_copy_eth_addresses(mic.src_addr.sa_data, src_mac);

  switch(fail_type)
  {
  case MIC_FAIL_PAIRWISE:
    mic.flags |= IW_MICFAILURE_PAIRWISE;
    break;
  case MIC_FAIL_GROUP:
    mic.flags |= IW_MICFAILURE_GROUP;
    break;
  default:
    MTLK_ASSERT(!"Unknown MIC failure type");
  }

  wireless_send_event(mtlk_df_get_user(df)->dev, IWEVMICHAELMICFAILURE, &wrqu, (char*)&mic);
}

void __MTLK_IFUNC
mtlk_df_ui_notify_scan_complete(mtlk_df_t *df)
{
  union iwreq_data wrqu;

  MTLK_ASSERT(NULL != df);

  wrqu.data.length = 0;
  wrqu.data.flags = 0;

  wireless_send_event(mtlk_df_get_user(df)->dev, SIOCGIWSCAN, &wrqu, NULL);
}

static const mtlk_guid_t IRBE_RMMOD = MTLK_IRB_GUID_RMMOD;
static const mtlk_guid_t IRBE_HANG  = MTLK_IRB_GUID_HANG;

void __MTLK_IFUNC
mtlk_df_ui_notify_notify_rmmod(uint32 rmmod_data)
{
  mtlk_irbd_notify_app(mtlk_irbd_get_root(), &IRBE_RMMOD, &rmmod_data, sizeof(rmmod_data));
}

void __MTLK_IFUNC
mtlk_df_ui_notify_notify_fw_hang(mtlk_df_t *df, uint32 fw_cpu, uint32 sw_watchdog_data)
{
  MTLK_ASSERT(df != NULL);
  MTLK_ASSERT(fw_cpu < ARRAY_SIZE(mtlk_df_get_user(df)->fw_hang_evts));

  mtlk_irbd_notify_app(mtlk_irbd_get_root(), &IRBE_HANG, &sw_watchdog_data, sizeof(sw_watchdog_data));
  mtlk_osal_event_set(&mtlk_df_get_user(df)->fw_hang_evts[fw_cpu]);
  ILOG0_DD("CID-%04x: FW CPU#%d asserted", mtlk_vap_get_oid(mtlk_df_get_vap_handle(df)), fw_cpu);
}
/**************************************************************
 * Transactions with Core
 **************************************************************/
static int mtlk_df_ui_aocs_history(mtlk_seq_entry_t *s, void *data)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_clpb_t *clpb = NULL;
  mtlk_aocs_history_stat_entry_t *stat_entry;
  mtlk_df_t *df = mtlk_df_proc_seq_entry_get_df(s);

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_AOCS_HISTORY,
                                  &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_AOCS_HISTORY, FALSE);

  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  mtlk_aux_seq_printf(s,"Channel switch history:\n"
                        "Time (ago)            Ch (2nd)"
                        "      Switch reason      Selection criteria\n");

  while(NULL != (stat_entry = mtlk_clpb_enum_get_next(clpb, NULL))) {
    mtlk_aux_seq_printf(s,"%04dh %02dm %02d.%03ds    %3d (%3d)  %17s %26s\n"
      , stat_entry->hour_ago
      , stat_entry->min_ago
      , stat_entry->sec_ago
      , stat_entry->msec_ago
      , stat_entry->primary_channel
      , stat_entry->secondary_channel
      , stat_entry->reason_text
      , stat_entry->criteria_text);
  }

  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int mtlk_df_ui_aocs_table(mtlk_seq_entry_t *s, void *data)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_clpb_t *clpb = NULL;
  mtlk_aocs_table_stat_entry_t *stat_entry;
  mtlk_df_t *df = mtlk_df_proc_seq_entry_get_df(s);

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_AOCS_TBL, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_AOCS_TBL, FALSE);

  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  mtlk_aux_seq_printf(s,"AOCS table:\n"
                        " Ch (2nd) SRnk CRnk BSS  CL      Tx SM Rad NOc"
                        "     ClrChk !use Excl Noisy Rdr TxPenalty  Tx11d\n");

  while(NULL != (stat_entry = mtlk_clpb_enum_get_next(clpb, NULL))) {
    mtlk_aux_seq_printf(s,"%3d (%3d) %4d %4d %3d %3d %3d.%03d %2d %3d %3d %10u %4d %4d %5d %3d  %3d.%03d %d.%03d\n",
      stat_entry->channel_primary,
      stat_entry->channel_secondary,
      stat_entry->scan_rank,
      stat_entry->confirm_rank,
      stat_entry->nof_bss,
      stat_entry->channel_load,
      stat_entry->tx_power / 8,
      (stat_entry->tx_power % 8) * 125,
      stat_entry->sm,
      stat_entry->radar_detected,
      stat_entry->time_ms_non_occupied_period / 60000,
      stat_entry->time_ms_last_clear_check,
      stat_entry->dont_use,
      stat_entry->exclude,
      stat_entry->is_noisy,
      stat_entry->is_in_radar_timeout,
      stat_entry->tx_power_penalty / 8,
      (stat_entry->tx_power_penalty % 8) * 125,
      stat_entry->max_tx_power / 8,
      (stat_entry->max_tx_power % 8) * 125);
  }

  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int mtlk_df_ui_aocs_channels(mtlk_seq_entry_t *s, void *data)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_clpb_t *clpb = NULL;
  mtlk_aocs_channels_stat_t *stat;
  mtlk_df_t *df = mtlk_df_proc_seq_entry_get_df(s);

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_AOCS_CHANNELS_TBL,
                                  &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_AOCS_CHANNELS_TBL, FALSE);

  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  mtlk_aux_seq_printf(s,"AOCS channels:\n Ch BSS  SM  CL n20 n40 Intol Affected\n");

  while(NULL != (stat = mtlk_clpb_enum_get_next(clpb, NULL))) {
    mtlk_aux_seq_printf(s,"%3d %3d %3d %3d %3d %3d %5d %8d\n",
                        stat->channel,
                        stat->nof_bss,
                        stat->sm_required,
                        stat->channel_load,
                        stat->num_20mhz_bss,
                        stat->num_40mhz_bss,
                        stat->forty_mhz_intolerant,
                        stat->forty_mhz_int_affected);
  }

  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int mtlk_df_ui_aocs_penalties(mtlk_seq_entry_t *s, void *data)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_clpb_t *clpb = NULL;
  mtlk_aocs_penalties_stat_entry_t *stat_entry;
  mtlk_df_t *df = mtlk_df_proc_seq_entry_get_df(s);

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_AOCS_PENALTIES,
                                  &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_AOCS_PENALTIES, FALSE);

  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  mtlk_aux_seq_printf(s,"AOCS TxPowerPenalties:\nFreq Penatly\n");

  while(NULL != (stat_entry = mtlk_clpb_enum_get_next(clpb, NULL))) {
    mtlk_aux_seq_printf(s,"%4d %d.%03d\n",
                        stat_entry->freq,
                        stat_entry->penalty / 8,
                        (stat_entry->penalty % 8) * 125);
  }
  
  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int mtlk_df_ui_hw_limits(mtlk_seq_entry_t *s, void *data)
{
  int res = MTLK_ERR_OK;
  mtlk_clpb_t *clpb = NULL;
  mtlk_hw_limits_stat_entry_t *stat_entry;
  mtlk_df_t *df = mtlk_df_proc_seq_entry_get_df(s);

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_HW_LIMITS, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_HW_LIMITS, FALSE);

  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  mtlk_aux_seq_printf(s,"HW specific limits:\nFreq Spectrum Limit Domain\n");

  while(NULL != (stat_entry = mtlk_clpb_enum_get_next(clpb, NULL))) {
    mtlk_aux_seq_printf(s,"%4d %8d %5d 0x%x\n",
                        stat_entry->freq,
                        stat_entry->spectrum,
                        stat_entry->tx_lim,
                        stat_entry->reg_domain);
  }

  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int mtlk_df_ui_reg_limits(mtlk_seq_entry_t *s, void *data)
{
  int res = MTLK_ERR_OK;
  mtlk_clpb_t *clpb = NULL;
  mtlk_reg_limits_stat_entry_t *stat_entry;
  mtlk_df_t *df = mtlk_df_proc_seq_entry_get_df(s);
  
  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_REG_LIMITS, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_REG_LIMITS, FALSE);

  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  mtlk_aux_seq_printf(s,"Regulatory domain limits:\n");

  while(NULL != (stat_entry = mtlk_clpb_enum_get_next(clpb, NULL))) {
    mtlk_aux_seq_printf(s,"domain: 0x%x, class: %d, spacing: %d, channel: %d,%d,%d\n",
                        stat_entry->reg_domain,
                        stat_entry->reg_class,
                        stat_entry->spectrum,
                        stat_entry->channel,
                        stat_entry->tx_lim,
                        stat_entry->mitigation);
  }

  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int mtlk_df_ui_ant_gain(mtlk_seq_entry_t *s, void *data)
{
  int res = MTLK_ERR_OK;
  mtlk_clpb_t *clpb = NULL;
  mtlk_ant_gain_stat_entry_t *stat_entry;
  mtlk_df_t *df = mtlk_df_proc_seq_entry_get_df(s);

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_ANTENNA_GAIN, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_ANTENNA_GAIN, FALSE);

  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  mtlk_aux_seq_printf(s,"Antenna gain table:\nFreq Gain\n");

  while(NULL != (stat_entry = mtlk_clpb_enum_get_next(clpb, NULL))) {
    mtlk_aux_seq_printf(s,"%4d %4d\n",
                        stat_entry->freq,
                        stat_entry->gain);
  }

  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int mtlk_df_ui_serializer_dump(mtlk_seq_entry_t *s, void *data)
{
  int res = MTLK_ERR_OK;
  mtlk_clpb_t *clpb = NULL;
  mtlk_serializer_command_info_t *cmd_info;
  mtlk_df_t *df = mtlk_df_proc_seq_entry_get_df(s);

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_SERIALIZER_INFO, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_SERIALIZER_INFO, FALSE);

  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  mtlk_aux_seq_printf(s,"Serialized Commands:\nCurr Prio GID FID LID\n");

  while(NULL != (cmd_info = mtlk_clpb_enum_get_next(clpb, NULL))) {
    mtlk_aux_seq_printf(s,"%4u %4u %3u %3u %3u\n",
      cmd_info->is_current,
      cmd_info->priority,
      mtlk_slid_get_gid(cmd_info->issuer_slid),
      mtlk_slid_get_fid(cmd_info->issuer_slid),
      mtlk_slid_get_lid(cmd_info->issuer_slid));
  }

  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static void
_mtlk_df_ui_print_driver_stats(mtlk_seq_entry_t *s, mtlk_core_general_stats_t *general_stats)
{
  struct priv_stats *pstats;
  mtlk_mac_stats_t *mac_stat;

  char buf[MAX_DF_UI_STAT_NAME_LENGTH];
  uint32 i;
  const char *uint_fmt = "%10u %s\n";

  mac_stat = &general_stats->mac_stat;

  pstats = &general_stats->core_priv_stats;

  mtlk_aux_seq_printf(s, uint_fmt, general_stats->rx_dat_frames, "data frames received");
  mtlk_aux_seq_printf(s, uint_fmt, general_stats->rx_ctl_frames, "control frames received");
  mtlk_aux_seq_printf(s, uint_fmt, general_stats->rx_man_frames, "management frames received");
  mtlk_aux_seq_printf(s, uint_fmt, mac_stat->stat[STAT_TX_FAIL], "TX packets dropped");
  mtlk_aux_seq_printf(s, uint_fmt, pstats->tx_max_cons_drop, "TX maximum consecutive dropped packets");

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sprintf(buf, "MSDUs received, QoS priority %d", i);
    mtlk_aux_seq_printf(s, uint_fmt, pstats->ac_rx_counter[i], buf);
  }

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sprintf(buf, "MSDUs transmitted, QoS priority %d", i);
    mtlk_aux_seq_printf(s, uint_fmt, pstats->ac_tx_counter[i], buf);
  }

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sprintf(buf, "MSDUs dropped, QoS priority %d", i);
    mtlk_aux_seq_printf(s, uint_fmt, pstats->ac_dropped_counter[i], buf);
  }

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sprintf(buf, "MSDUs used, QoS priority %d", i);
    mtlk_aux_seq_printf(s, uint_fmt, pstats->ac_used_counter[i], buf);
  }

  mtlk_aux_seq_printf(s, uint_fmt, general_stats->tx_msdus_free, "TX MSDUs free");
  mtlk_aux_seq_printf(s, uint_fmt, general_stats->tx_msdus_usage_peak, "TX MSDUs usage peak");

  mtlk_aux_seq_printf(s, uint_fmt, general_stats->fwd_rx_packets, "packets received that should be forwarded to one or more STAs");
  mtlk_aux_seq_printf(s, uint_fmt, general_stats->fwd_rx_bytes, "bytes received that should be forwarded to one or more STAs");
  mtlk_aux_seq_printf(s, uint_fmt, pstats->fwd_tx_packets, "packets transmitted for forwarded data");
  mtlk_aux_seq_printf(s, uint_fmt, pstats->fwd_tx_bytes, "bytes transmitted for forwarded data");
  mtlk_aux_seq_printf(s, uint_fmt, pstats->fwd_dropped, "forwarding (transmission) failures");
  mtlk_aux_seq_printf(s, uint_fmt, pstats->rmcast_dropped, "reliable multicast (transmission) failures");
  mtlk_aux_seq_printf(s, uint_fmt, general_stats->unicast_replayed_packets + general_stats->multicast_replayed_packets, "packets replayed");
  mtlk_aux_seq_printf(s, uint_fmt, pstats->bars_cnt, "BAR frames received");

  mtlk_aux_seq_printf(s, uint_fmt, general_stats->bist_check_passed, "BIST check passed");

  mtlk_aux_seq_printf(s, uint_fmt, general_stats->txmm_sent, "MAN Messages sent");
  mtlk_aux_seq_printf(s, uint_fmt, general_stats->txmm_cfmd, "MAN Messages confirmed");
  mtlk_aux_seq_printf(s, uint_fmt, general_stats->txmm_peak, "MAN Messages in peak");
  mtlk_aux_seq_printf(s, uint_fmt, general_stats->txdm_sent, "DBG Messages sent");
  mtlk_aux_seq_printf(s, uint_fmt, general_stats->txdm_cfmd, "DBG Messages confirmed");
  mtlk_aux_seq_printf(s, uint_fmt, general_stats->txdm_peak, "DBG Messages in peak");
  mtlk_aux_seq_printf(s, uint_fmt, general_stats->fw_logger_packets_processed, "FW logger packets processed");
  mtlk_aux_seq_printf(s, uint_fmt, general_stats->fw_logger_packets_dropped, "FW logger packets dropped");
}

static int mtlk_df_ui_debug_general(mtlk_seq_entry_t *s, void *data)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_clpb_t *clpb = NULL;
  mtlk_stadb_stat_t *stadb_stat;
  uint32 size;
  unsigned long total_rx_packets;
  unsigned long total_tx_packets;
  unsigned long total_rx_dropped;
  mtlk_df_t *df = mtlk_df_proc_seq_entry_get_df(s);
  mtlk_df_user_t *df_user = mtlk_df_get_user(df);
  mtlk_core_ui_get_stadb_status_req_t get_stadb_status_req;
  
  /* Get Core general information from DF buffer and don't call Core */

  get_stadb_status_req.get_hostdb = TRUE;
  get_stadb_status_req.use_cipher = FALSE;
  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_STADB_STATUS, &clpb,
                                  &get_stadb_status_req, sizeof(get_stadb_status_req));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_STADB_STATUS, FALSE);
  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  mtlk_aux_seq_printf(s, "\n"
      "Driver Statistics\n"
      "\n"
      "------------------+------------------+--------------------------------------\n"
      "MAC               | Packets received | Packets sent     | Rx packets dropped\n"
      "------------------+------------------+--------------------------------------\n");

  total_rx_packets = 0;
  total_tx_packets = 0;
  total_rx_dropped = 0;

  /* enumerate sta entries */
  while(NULL != (stadb_stat = mtlk_clpb_enum_get_next(clpb, &size))) {
    if (sizeof(*stadb_stat) != size) {
      res = MTLK_ERR_UNKNOWN;
      goto delete_clpb;
    }

    if (STAT_ID_STADB == stadb_stat->type) {

      total_rx_packets += stadb_stat->u.general_stat.sta_rx_packets;
      total_tx_packets += stadb_stat->u.general_stat.sta_tx_packets;
      total_rx_dropped += stadb_stat->u.general_stat.sta_rx_dropped;

      mtlk_aux_seq_printf(s, MAC_PRINTF_FMT " | %-16u | %-16u | %-16u\n",
                          MAC_PRINTF_ARG(stadb_stat->u.general_stat.addr.au8Addr),
                          stadb_stat->u.general_stat.sta_rx_packets,
                          stadb_stat->u.general_stat.sta_tx_packets,
                          stadb_stat->u.general_stat.sta_rx_dropped);
    }
    else if (STAT_ID_HSTDB == stadb_stat->type) {
      if (mtlk_df_is_ap(df)) {
        mtlk_aux_seq_printf(s, "   STA's WDS hosts:\n");
      }
      else {
        mtlk_aux_seq_printf(s, "   All WDS hosts connected to this STA\n");
      }

      mtlk_aux_seq_printf(s, MAC_PRINTF_FMT "\n", MAC_PRINTF_ARG(stadb_stat->u.hstdb_stat.addr.au8Addr));
    } else {
      res = MTLK_ERR_UNKNOWN;
      goto delete_clpb;
    }
  }

  mtlk_aux_seq_printf(s,
      "------------------+------------------+--------------------------------------\n"
      "Total             | %-16lu | %-16lu | %lu\n"
      "------------------+------------------+--------------------------------------\n"
      "Broadcast/non-reliable multicast     | %-16lu |\n"
      "------------------+------------------+--------------------------------------\n"
      "\n",
      total_rx_packets, total_tx_packets, total_rx_dropped,
      (unsigned long)df_user->slow_ctx->core_general_stats.core_priv_stats.tx_bcast_nrmcast);

  _mtlk_df_ui_print_driver_stats(s, &df_user->slow_ctx->core_general_stats);

delete_clpb:
  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int mtlk_df_ui_debug_mac_stats(mtlk_seq_entry_t *s, void *data)
{
  int                 idx;
  int                 res = MTLK_ERR_OK;
  mtlk_df_t           *df = mtlk_df_proc_seq_entry_get_df(s);
  mtlk_df_user_t      *df_user = mtlk_df_get_user(df);

  mtlk_aux_seq_printf(s, "\nMAC Statistics\n\n");

  for (idx = 0; idx < mtlk_df_get_stat_info_len(); idx++) {
    mtlk_aux_seq_printf(s, "%10u %s\n",
        df_user->slow_ctx->core_general_stats.mac_stat.stat[mtlk_df_get_stat_info_idx(idx)],
        mtlk_df_get_stat_info_name(idx));
  }
  
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int mtlk_df_ui_reordering_stats(mtlk_seq_entry_t *s, void *data)
{
  int                     res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_clpb_t            *clpb = NULL;
  uint32                  size;
  mtlk_stadb_stat_t      *stadb_stat;
  uint8                   tid;
  mtlk_df_t               *df = mtlk_df_proc_seq_entry_get_df(s);
  mtlk_core_ui_get_stadb_status_req_t get_stadb_status_req;

  get_stadb_status_req.get_hostdb = FALSE;
  get_stadb_status_req.use_cipher = FALSE;
  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_STADB_STATUS, &clpb,
                                  &get_stadb_status_req, sizeof(get_stadb_status_req));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_STADB_STATUS, FALSE);
  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  mtlk_aux_seq_printf(s, "\n"
      "\n"
      "Reordering Statistics\n"
      "\n"
      "------------------+----+------------+------------+------------+------------+------------\n"
      "MAC               | ID | Too old    | Duplicate  | Queued     | Overflows  | Lost       \n"
      "------------------+----+------------+------------+------------+------------+------------\n");

  while(NULL != (stadb_stat = mtlk_clpb_enum_get_next(clpb, &size))) {
    if ((sizeof(*stadb_stat) != size) || (STAT_ID_STADB != stadb_stat->type)) {
      res = MTLK_ERR_UNKNOWN;
      goto delete_clpb;
    }

    for (tid = 0; tid < ARRAY_SIZE(stadb_stat->u.general_stat.reordering_stats); tid++ )
    {
      if (stadb_stat->u.general_stat.reordering_stats[tid].used) {
        mtlk_aux_seq_printf(s, MAC_PRINTF_FMT " | %-2d | %-10u | %-10u | %-10u | %-10u | %-10u\n",
                        MAC_PRINTF_ARG(stadb_stat->u.general_stat.addr.au8Addr),
                        tid,
                        stadb_stat->u.general_stat.reordering_stats[tid].reord_stat.too_old,
                        stadb_stat->u.general_stat.reordering_stats[tid].reord_stat.duplicate,
                        stadb_stat->u.general_stat.reordering_stats[tid].reord_stat.queued,
                        stadb_stat->u.general_stat.reordering_stats[tid].reord_stat.overflows,
                        stadb_stat->u.general_stat.reordering_stats[tid].reord_stat.lost);
      } else {
        mtlk_aux_seq_printf(s, MAC_PRINTF_FMT " | %-2d | Not Used\n",
                        MAC_PRINTF_ARG(stadb_stat->u.general_stat.addr.au8Addr),
                        tid);
      }
    }
  }

  mtlk_aux_seq_printf(s,
      "------------------+----+------------+------------+------------+------------+------------\n");

delete_clpb:
  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int mtlk_df_ui_debug_l2nat(mtlk_seq_entry_t *s, void *data)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_clpb_t *clpb = NULL;
  mtlk_df_t *df = mtlk_df_proc_seq_entry_get_df(s);
  struct l2nat_hash_entry_stats  *hash_entry_stats;
  struct l2nat_buckets_stats     *buckets_stats;
  int idx;

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_L2NAT_STATS, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_L2NAT_STATS, FALSE);

  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  buckets_stats = mtlk_clpb_enum_get_next(clpb, NULL);
  if (NULL == buckets_stats) {
    res = MTLK_ERR_UNKNOWN;
    goto err_get_stat;
  }

  mtlk_aux_seq_printf(s, "\nL2NAT Statistics\n\n"
                  "IP               MAC                Inactive (ms)       Age(secs)  Pkts from\n"
                  "----------------------------------------------------------------------------\n");

  while(NULL != (hash_entry_stats = mtlk_clpb_enum_get_next(clpb, NULL))) {
    mtlk_aux_seq_printf(s, MTLK_NIPQUAD_FMT "\t" MAC_PRINTF_FMT "%15lu %15lu %10u\n",
                        MTLK_NIPQUAD(hash_entry_stats->ip),
                        MAC_PRINTF_ARG(hash_entry_stats->mac),
                        hash_entry_stats->last_pkt_timestamp,
                        hash_entry_stats->first_pkt_timestamp,
                        hash_entry_stats->pkts_from);
  }

  mtlk_aux_seq_printf(s,
      "----------------------------------------------------------------------------\n");

  mtlk_aux_seq_printf(s, "\n\nHash table statistics\n\n"
                         "Bucket length     :");

  for (idx = 0; idx < ARRAY_SIZE(buckets_stats->blens) - 1; idx++) {
    mtlk_aux_seq_printf(s, "   %3d", idx);
  }

  mtlk_aux_seq_printf(s, "   >%2d\n", (int)(ARRAY_SIZE(buckets_stats->blens) - 2));
  mtlk_aux_seq_printf(s, "Number of buckets :");

  for (idx = 0; idx < ARRAY_SIZE(buckets_stats->blens) ; idx++) {
    mtlk_aux_seq_printf(s, "   %3d", buckets_stats->blens[idx]);
  }

  mtlk_aux_seq_printf(s, "\n\n");

err_get_stat:
  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int mtlk_df_ui_debug_send_queue(mtlk_seq_entry_t *s, void *data)
{
  int                   res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_clpb_t           *clpb = NULL;
  mtlk_df_t             *df = mtlk_df_proc_seq_entry_get_df(s);
  mtlk_sq_status_t      *status;
  mtlk_sq_peer_status_t *peer_status;
  int                   idx;

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_SQ_STATUS, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_SQ_STATUS, FALSE);

  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  status = mtlk_clpb_enum_get_next(clpb, NULL);
  if (NULL == status) {
    res = MTLK_ERR_UNKNOWN;
    goto err_get_stat;
  }

/* iterate with format string */
#define ITF(head_str, ...)                  \
	mtlk_aux_seq_printf(s, head_str);         \
  for (idx = 0; idx < NTS_PRIORITIES; idx++)      \
    mtlk_aux_seq_printf(s,  __VA_ARGS__ );  \
  mtlk_aux_seq_printf(s,"\n");

  ITF("--------------------------", "%s", "-------------");
  ITF("Name                     |", "%11s |", mtlk_qos_get_ac_name(idx));
  ITF("--------------------------", "%s", "-------------");

  ITF("packets pushed           |", "%11d |", status->stats.pkts_pushed[idx]);
  ITF("packets sent to UM       |", "%11d |", status->stats.pkts_sent_to_um[idx]);
  ITF("global queue limits      |", "%11d |", status->limits.global_queue_limit[idx]);
  ITF("peer queue limits        |", "%11d |", status->limits.peer_queue_limit[idx]);
  ITF("current sizes            |", "%11d |", status->qsizes.qsize[idx]);
  ITF("packets dropped (limit)  |", "%11d |", status->stats.pkts_limit_dropped[idx]);

  ITF("--------------------------", "%s", "-------------");

  while(NULL != (peer_status = mtlk_clpb_enum_get_next(clpb, NULL))) {

    mtlk_aux_seq_printf(s, "\nMAC:" MAC_PRINTF_FMT "\nLimit: %10u\nUsed: %11u\n", MAC_PRINTF_ARG(peer_status->mac_addr.au8Addr), peer_status->limit, peer_status->used);

    ITF("    --------------------------", "%s", "-------------");
    ITF("    Name                     |", "    %s   |", mtlk_qos_get_ac_name(idx));
    ITF("    --------------------------", "%s", "-------------");
    ITF("    packets pushed           |", "%11d |", peer_status->stats.pkts_pushed[idx]);
    ITF("    packets sent to UM       |", "%11d |", peer_status->stats.pkts_sent_to_um[idx]);
    ITF("    current sizes            |", "%11d |", peer_status->current_size[idx]);
    ITF("    packets dropped (limit)  |", "%11d |", peer_status->stats.pkts_limit_dropped[idx]);
    ITF("    --------------------------", "%s", "-------------");
  }

err_get_stat:
  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int
mtlk_df_do_debug_assert_write (struct file *file, const char *buf,
                               unsigned long count, void *data)
{
  int          res = MTLK_ERR_PARAMS;
  char         str[MAX_PROC_STR_LEN];
  int          tmp;
  uint32       assert_type;
  mtlk_df_t   *df = (mtlk_df_t *)data;
  mtlk_clpb_t *clpb = NULL;

  if (count > MAX_PROC_STR_LEN) {
    goto end;
  }

  memset(str, 0, sizeof(str));
  if (copy_from_user(str, buf, count)) {
    ELOG0_D("CID-%04x: copy_from_user error", mtlk_vap_get_oid(mtlk_df_get_vap_handle(df)));
    goto end;
  }

  sscanf(str, "%d", &tmp);

  if (tmp < 0 || tmp >= MTLK_CORE_UI_ASSERT_TYPE_LAST) {
    ELOG_DD("CID-%04x: Unsupported assert type: %d", mtlk_vap_get_oid(mtlk_df_get_vap_handle(df)), tmp);
    goto end;
  }

  assert_type = (uint32)tmp;

  switch (assert_type)
  {
  case MTLK_CORE_UI_ASSERT_TYPE_FW_LMIPS:
    /* Check whether the LMIPS has already been asserted. If so - doesn't call the Core */
    res = mtlk_osal_event_wait(&mtlk_df_get_user(df)->fw_hang_evts[LMIPS], 0);
    if (res == MTLK_ERR_OK) {
      ILOG0_D("CID-%04x: LMIPS already asserted", mtlk_vap_get_oid(mtlk_df_get_vap_handle(df)));
      goto end;
    }
    break;
  case MTLK_CORE_UI_ASSERT_TYPE_FW_UMIPS:
    /* Check whether the UMIPS has already been asserted. If so - doesn't call the Core */
    res = mtlk_osal_event_wait(&mtlk_df_get_user(df)->fw_hang_evts[UMIPS], 0);
    if (res == MTLK_ERR_OK) {
      ILOG0_D("CID-%04x: UMIPS already asserted", mtlk_vap_get_oid(mtlk_df_get_vap_handle(df)));
      goto end;
    }
  default:
    break;
  }

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_SET_MAC_ASSERT, &clpb,
                                  &assert_type, sizeof(assert_type));
  _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_SET_MAC_ASSERT, TRUE);

  switch (assert_type)
  {
  case MTLK_CORE_UI_ASSERT_TYPE_FW_LMIPS:
    /* Wait for LMIPS to detect ASSERTION */
    res = mtlk_osal_event_wait(&mtlk_df_get_user(df)->fw_hang_evts[LMIPS], _DF_WAIT_FW_ASSERT);
    if (res != MTLK_ERR_OK) {
      WLOG_DD("CID-%04x: LMIPS assertion failed (res=%d)", mtlk_vap_get_oid(mtlk_df_get_vap_handle(df)), res);
    }
    else {
      ILOG0_D("CID-%04x: LMIPS asserted", mtlk_vap_get_oid(mtlk_df_get_vap_handle(df)));
    }
    break;
  case MTLK_CORE_UI_ASSERT_TYPE_FW_UMIPS:
    /* Wait for LMIPS to detect ASSERTION */
    res = mtlk_osal_event_wait(&mtlk_df_get_user(df)->fw_hang_evts[UMIPS], _DF_WAIT_FW_ASSERT);
    if (res != MTLK_ERR_OK) {
      WLOG_DD("CID-%04x: UMIPS assertion failed (res=%d)", mtlk_vap_get_oid(mtlk_df_get_vap_handle(df)), res);
    }
    else {
      ILOG0_D("CID-%04x: UMIPS asserted", mtlk_vap_get_oid(mtlk_df_get_vap_handle(df)));
    }
  case MTLK_CORE_UI_ASSERT_TYPE_LAST:
  default:
    break;
  }

end:
  /* need to return counter, not result in this type of proc */
  return count;
}

/*Interface functions to deal with kernel >3.1 proc API*/
static int
mtlk_df_do_debug_assert_write_proc_fop (struct file *file, const char *buf,
                               size_t count, loff_t *off){
	mtlk_df_t   *df = (mtlk_df_t *)PDE_DATA(file_inode(file));

	return mtlk_df_do_debug_assert_write (file, buf, count, df);
}

static int
_mtlk_df_ui_proc_bcl_read (char *page, char **start, off_t off, int count, int *eof,
  void *data, int io_base, int io_size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_df_t *df = (mtlk_df_t *)data;
  mtlk_clpb_t *clpb = NULL;
  UMI_BCL_REQUEST req;
  UMI_BCL_REQUEST *req_result;

  /* Calculate io offset */
  if (off >= io_size) {
    *eof = 1;
    return 0;
  }

  if ((off & (sizeof(req.Data) - 1)) || (count < sizeof(req.Data)))
    return -EIO;

  count = 0;

  /* Fill get BCL request */
  /* We mast inform core in some way that it should not convert result data words in host format
   * when BCL read operation has been requested from Proc (memory dump). So, add BCL_UNIT_MAX to
   * Unit field and check&subtract it in core  */
  req.Unit = BCL_UNIT_INT_RAM + BCL_UNIT_MAX;
  req.Size = sizeof(req.Data);
  req.Address = io_base + off;
  memset(req.Data, 0x5c, sizeof(req.Data)); /* poison */

  /* Send request to core */
  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_BCL_MAC_DATA, &clpb, &req, sizeof(req));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_BCL_MAC_DATA, FALSE);
  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  /* Handle result */
  req_result = mtlk_clpb_enum_get_next(clpb, NULL);

  if (NULL != req_result) {
    count = sizeof(req_result->Data);
/*Needs copy_to_user() below?*/
    memcpy(page, req_result->Data, count); 
    *start = (char*)sizeof(req_result->Data);
  }

  mtlk_clpb_delete(clpb);
err_ret:
  return count;
}

/*Interface functions to meet new kernel 3.10 API*/

/*These variables are global*/
	char **start;
	int *eof;

static int mtlk_df_ui_lm_proc_fop(struct file *file, char __user *buffer, size_t count, loff_t *off)
{
	void *data = PDE_DATA(file_inode(file));
	return _mtlk_df_ui_proc_bcl_read(buffer, start, *off, count, eof, data, LM_DATA_BASE, LM_DATA_SIZE);
}

static int mtlk_df_ui_um_proc_fop(struct file *file,char __user *buffer, size_t count, loff_t *off)
{
	void *data = PDE_DATA(file_inode(file));
  return _mtlk_df_ui_proc_bcl_read(buffer, start, *off, count, eof, data, UM_DATA_BASE, UM_DATA_SIZE);
}

static int mtlk_df_ui_shram_proc_fop(struct file *file,char __user *buffer, size_t count, loff_t *off)
{
	void *data = PDE_DATA(file_inode(file));
  return _mtlk_df_ui_proc_bcl_read(buffer, start, *off, count, eof, data, SHRAM_DATA_BASE, SHRAM_DATA_SIZE);
}
/*End of interface functions*/

static int
_mtlk_df_ui_ee_caps(mtlk_seq_entry_t *s, void *data)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_df_t *df = mtlk_df_proc_seq_entry_get_df(s);
  mtlk_eeprom_data_stat_entry_t *pstat;
  mtlk_handle_t hdata;

  res = _mtlk_df_user_pull_core_data(df, MTLK_CORE_REQ_GET_EE_CAPS, TRUE, (void**) &pstat, NULL, &hdata);
  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  mtlk_aux_seq_printf(s, "AP disabled: %i\nDFS channels disabled: %i\n",
                      pstat->ap_disabled, pstat->disable_sm_channels);

  _mtlk_df_user_free_core_data(df, hdata);

err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int
_mtlk_df_ui_debug_igmp_read(mtlk_seq_entry_t *s, void *data)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_df_t *df = mtlk_df_proc_seq_entry_get_df(s);
  mtlk_clpb_t *clpb = NULL;
  mtlk_mc_igmp_tbl_item_t   *igmp_tbl_item;

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_GET_MC_IGMP_TBL, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_MC_IGMP_TBL, FALSE);
  if (MTLK_ERR_OK != res) {
    goto err_ret;
  }

  while (NULL != (igmp_tbl_item = mtlk_clpb_enum_get_next(clpb, NULL))) {

    switch (igmp_tbl_item->type) {
    case MTLK_MC_IPV4_ADDR:
      mtlk_aux_seq_printf(s, "IPv4 mcast group " MTLK_NIPQUAD_FMT "\n",
              MTLK_NIPQUAD( ((mtlk_mc_igmp_tbl_ipv4_item_t*)igmp_tbl_item)->addr.s_addr ));
      break;
    case MTLK_MC_IPV6_ADDR:
      mtlk_aux_seq_printf(s, "IPv6 mcast group " MTLK_NIP6_FMT "\n",
              MTLK_NIP6( ((mtlk_mc_igmp_tbl_ipv6_item_t*)igmp_tbl_item)->addr ));
      break;
    case MTLK_MC_MAC_ADDR:
      mtlk_aux_seq_printf(s, "    " MAC_PRINTF_FMT "\n",
              MAC_PRINTF_ARG( ((mtlk_mc_igmp_tbl_mac_item_t*)igmp_tbl_item)->addr.au8Addr ));
      break;
    default:
      MTLK_ASSERT(FALSE);
    }
  }

  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int
mtlk_df_ui_reset_stats(mtlk_df_t* df)
{
  mtlk_clpb_t *clpb = NULL;
  int res;

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_RESET_STATS, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval(res, clpb, MTLK_CORE_REQ_RESET_STATS, TRUE);

  return res;
}

int _mtlk_df_ui_reset_stats_proc(struct file *file, const char __user *buffer,
                                 unsigned long count, void *data)
{
  mtlk_df_ui_reset_stats((mtlk_df_t*)data);
  return count;
}

/*Interface functions to deal with kernel >3.1 proc API*/
int _mtlk_df_ui_reset_stats_proc_fop(struct file *file, const char __user *buffer,
                                 size_t count, loff_t *off){
	mtlk_df_t   *df = (mtlk_df_t *)PDE_DATA(file_inode(file));
	return _mtlk_df_ui_reset_stats_proc(file, buffer, count, df);
}

static int _mtlk_df_ui_l2nat_clear_table(struct file *file, const char __user *buffer,
                                 unsigned long count, void *data)
{
  mtlk_clpb_t *clpb = NULL;
  mtlk_df_t *df = (mtlk_df_t*) data;

  int res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_L2NAT_CLEAR_TABLE, &clpb, NULL, 0);
  _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_L2NAT_CLEAR_TABLE, TRUE);

  return count;
}

/*Interface functions to deal with kernel >3.1 proc API*/
static int _mtlk_df_ui_l2nat_clear_table_proc_fop(struct file *file, const char __user *buffer,
                                 size_t count, loff_t *off){
	mtlk_df_t   *df = (mtlk_df_t *)PDE_DATA(file_inode(file));
	return _mtlk_df_ui_l2nat_clear_table(file, buffer, count, df);
}


#ifdef AOCS_DEBUG
static int mtlk_df_ui_aocs_proc_cl(struct file *file, const char __user *buffer,
                                   unsigned long count, void *data)
{
  int res = MTLK_ERR_OK;
  mtlk_clpb_t *clpb = NULL;
  uint32 cl;
  mtlk_df_t *df = (mtlk_df_t*) data;

  if (1 != sscanf(buffer, "%u", &cl)) {
    return count;
  }

  res = _mtlk_df_user_invoke_core(df, MTLK_CORE_REQ_SET_AOCS_CL, &clpb, &cl, sizeof(cl));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_SET_AOCS_CL, TRUE);

  return count;
}

/*Interface functions to deal with kernel >3.1 proc API*/
static int mtlk_df_ui_aocs_proc_cl_fop(struct file *file, const char __user *buffer,
                                   unsigned long count, void *data){
	mtlk_df_t   *df = (mtlk_df_t *)PDE_DATA(file_inode(file));
	return mtlk_df_ui_aocs_proc_cl(file, buffer, count, df);
}

#endif
/**************************************************************
 * Register handlers
 **************************************************************/
/*Due to changes in kernel >3.10, create_proc() needs to be updated*/
/*First creates all struct file_operations for the different procs*/
/*Functions _mtlk_df_proc_seq_entry_start_ops,_mtlk_df_proc_seq_entry_next_ops &*/
/*_mtlk_df_proc_seq_entry_stop_ops are defines as static in mtlk_df_proc_impl.c*/

/*Start of declarations struct file_operations*/

static struct seq_operations mtlk_df_ui_aocs_history_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_aocs_history
};

static struct seq_operations mtlk_df_ui_aocs_table_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_aocs_table
};

static struct seq_operations mtlk_df_ui_aocs_channels_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_aocs_channels
};

static struct seq_operations mtlk_df_ui_aocs_penalties_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_aocs_penalties
};

static struct seq_operations mtlk_df_ui_hw_limits_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_hw_limits
};

static struct seq_operations mtlk_df_ui_reg_limits_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_reg_limits
};

static struct seq_operations mtlk_df_ui_ant_gain_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_ant_gain
};

static struct seq_operations mtlk_df_ui_serializer_dump_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_serializer_dump
};

static struct seq_operations mtlk_df_ui_debug_general_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_debug_general
};

static struct seq_operations mtlk_df_ui_reordering_stats_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_reordering_stats
};

static struct seq_operations mtlk_df_ui_debug_l2nat_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_debug_l2nat
};

static struct seq_operations mtlk_df_ui_debug_send_queue_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_debug_send_queue
};

static struct seq_operations mtlk_df_ui_debug_mac_stats_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	mtlk_df_ui_debug_mac_stats
};

static struct seq_operations _mtlk_df_ui_ee_caps_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	_mtlk_df_ui_ee_caps
};

static struct seq_operations _mtlk_df_ui_debug_igmp_read_seq_ops = {
	.start = _mtlk_df_proc_seq_entry_start_ops,
	.next  = _mtlk_df_proc_seq_entry_next_ops,
	.stop  = _mtlk_df_proc_seq_entry_stop_ops,
	.show =	_mtlk_df_ui_debug_igmp_read
};

/*End of declarations struct file_operations*/

static int mtlk_df_ui_reg_aocs_history(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "aocs_history", df_user->slow_ctx->proc_df_node, df_user->df, &mtlk_df_ui_aocs_history_seq_ops);
}

static int mtlk_df_ui_reg_aocs_table(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "aocs_table", df_user->slow_ctx->proc_df_node, df_user->df, &mtlk_df_ui_aocs_table_seq_ops);
}

static int mtlk_df_ui_reg_aocs_channels(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "aocs_channels", df_user->slow_ctx->proc_df_node, df_user->df, &mtlk_df_ui_aocs_channels_seq_ops);
}

static int mtlk_df_ui_reg_aocs_penalties(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "aocs_penalties", df_user->slow_ctx->proc_df_node, df_user->df, &mtlk_df_ui_aocs_penalties_seq_ops);
}

static int mtlk_df_ui_reg_hw_limits(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "hw_limits", df_user->slow_ctx->proc_df_node, df_user->df, &mtlk_df_ui_hw_limits_seq_ops);
}

static int mtlk_df_ui_reg_reg_limits(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "reg_limits", df_user->slow_ctx->proc_df_node, df_user->df, &mtlk_df_ui_reg_limits_seq_ops);
}

static int mtlk_df_ui_reg_ant_gain(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "antenna_gain", df_user->slow_ctx->proc_df_node, df_user->df, &mtlk_df_ui_ant_gain_seq_ops);
}

static void mtlk_df_ui_unreg_serializer_dump(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("serializer_dump", df_user->slow_ctx->proc_df_debug_node);
}

static int mtlk_df_ui_reg_serializer_dump(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "serializer_dump", df_user->slow_ctx->proc_df_debug_node, df_user->df, &mtlk_df_ui_serializer_dump_seq_ops);
}

static void mtlk_df_ui_unreg_core_status(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("General", df_user->slow_ctx->proc_df_debug_node);
  mtlk_df_proc_node_remove_entry("MACStats", df_user->slow_ctx->proc_df_debug_node);
}

static int mtlk_df_ui_reg_core_status(mtlk_df_user_t* df_user)
{
  int res;
  res = mtlk_df_proc_node_add_seq_entry(
           "General", df_user->slow_ctx->proc_df_debug_node, df_user->df, &mtlk_df_ui_debug_general_seq_ops);

  if (MTLK_ERR_OK == res) {
    res = mtlk_df_proc_node_add_seq_entry(
              "MACStats",
              df_user->slow_ctx->proc_df_debug_node,
              df_user->df,
              &mtlk_df_ui_debug_mac_stats_seq_ops);
  }

  if (MTLK_ERR_OK != res) {
    mtlk_df_ui_unreg_core_status(df_user);
  }

  return res;
}

static int mtlk_df_ui_reg_debug_reordering_stats(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "ReorderingStats",
            df_user->slow_ctx->proc_df_debug_node,
            df_user->df,
            &mtlk_df_ui_reordering_stats_seq_ops);
}

static int mtlk_df_ui_reg_debug_l2nat(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "L2NAT", df_user->slow_ctx->proc_df_debug_node, df_user->df, &mtlk_df_ui_debug_l2nat_seq_ops);
}

static int mtlk_df_ui_reg_debug_send_queue(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "SendQueue", df_user->slow_ctx->proc_df_debug_node, df_user->df, &mtlk_df_ui_debug_send_queue_seq_ops);
}

static int mtlk_df_ui_reg_debug_mac_assert(mtlk_df_user_t* df_user)
{

mtlk_df_proc_node_add_wo_entry(
            "do_debug_assert", df_user->slow_ctx->proc_df_node, df_user->df, mtlk_df_do_debug_assert_write_proc_fop);
return 0;

}

static int mtlk_df_ui_reg_bcl_read_lm(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_ro_entry(
            "lm", df_user->slow_ctx->proc_df_node, df_user->df, mtlk_df_ui_lm_proc_fop);
}

static int mtlk_df_ui_reg_bcl_read_um(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_ro_entry(
            "um", df_user->slow_ctx->proc_df_node, df_user->df, mtlk_df_ui_um_proc_fop);
}

static int mtlk_df_ui_reg_bcl_read_shram(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_ro_entry(
            "shram", df_user->slow_ctx->proc_df_node, df_user->df, mtlk_df_ui_shram_proc_fop);
}

static int mtlk_df_ui_reg_ee_caps(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "EECaps", df_user->slow_ctx->proc_df_node, df_user->df, &_mtlk_df_ui_ee_caps_seq_ops);
}

static int mtlk_df_ui_reg_debug_igmp_read(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_seq_entry(
            "igmp", df_user->slow_ctx->proc_df_node, df_user->df, &_mtlk_df_ui_debug_igmp_read_seq_ops);
}

static int mtlk_df_ui_reg_reset_stats(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_wo_entry(
            "ResetStats",
            df_user->slow_ctx->proc_df_debug_node,
            df_user->df,
            _mtlk_df_ui_reset_stats_proc_fop);
}

static int mtlk_df_ui_reg_l2nat_clear_table(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_wo_entry(
            "L2NAT_ClearTable",
            df_user->slow_ctx->proc_df_debug_node,
            df_user->df,
            _mtlk_df_ui_l2nat_clear_table_proc_fop);
}

#ifdef AOCS_DEBUG
static int mtlk_df_ui_reg_aocs_proc_cl(mtlk_df_user_t* df_user)
{
  return mtlk_df_proc_node_add_wo_entry(
            "aocs_cl", df_user->slow_ctx->proc_df_debug_node, df_user->df, mtlk_df_ui_aocs_proc_cl_fop);
}
#endif

/**************************************************************
 * Unregister handlers
 **************************************************************/
static void mtlk_df_ui_unreg_aocs_history(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("aocs_history", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_aocs_table(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("aocs_table", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_aocs_channels(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("aocs_channels", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_aocs_penalties(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("aocs_penalties", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_hw_limits(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("hw_limits", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_reg_limits(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("reg_limits", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_ant_gain(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("antenna_gain", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_debug_reordering_stats(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("ReorderingStats", df_user->slow_ctx->proc_df_debug_node);
}

static void mtlk_df_ui_unreg_debug_l2nat(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("L2NAT", df_user->slow_ctx->proc_df_debug_node);
}

static void mtlk_df_ui_unreg_debug_send_queue(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("SendQueue", df_user->slow_ctx->proc_df_debug_node);
}

static void mtlk_df_ui_unreg_debug_mac_assert(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("do_debug_assert", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_bcl_read_lm(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("lm", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_bcl_read_um(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("um", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_bcl_read_shram(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("shram", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_ee_caps(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("EECaps", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_debug_igmp_read(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("igmp", df_user->slow_ctx->proc_df_node);
}

static void mtlk_df_ui_unreg_reset_stats(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("ResetStats", df_user->slow_ctx->proc_df_debug_node);
}

static void mtlk_df_ui_unreg_l2nat_clear_table(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("L2NAT_ClearTable", df_user->slow_ctx->proc_df_debug_node);
}

#ifdef AOCS_DEBUG
static void mtlk_df_ui_unreg_aocs_proc_cl(mtlk_df_user_t* df_user)
{
  mtlk_df_proc_node_remove_entry("aocs_cl", df_user->slow_ctx->proc_df_debug_node);
}
#endif

static const struct 
{
  int  (*on_register)  (mtlk_df_user_t* df_user);
  void (*on_unregister)(mtlk_df_user_t* df_user);
}
_proc_management_handlers[] =
{
  { mtlk_df_ui_reg_hw_limits,                 mtlk_df_ui_unreg_hw_limits },
  { mtlk_df_ui_reg_reg_limits,                mtlk_df_ui_unreg_reg_limits },
  { mtlk_df_ui_reg_ant_gain,                  mtlk_df_ui_unreg_ant_gain },
  { mtlk_df_ui_reg_debug_mac_assert,          mtlk_df_ui_unreg_debug_mac_assert },
  { mtlk_df_ui_reg_bcl_read_lm,               mtlk_df_ui_unreg_bcl_read_lm },
  { mtlk_df_ui_reg_bcl_read_um,               mtlk_df_ui_unreg_bcl_read_um },
  { mtlk_df_ui_reg_bcl_read_shram,            mtlk_df_ui_unreg_bcl_read_shram },
  { mtlk_df_ui_reg_core_status,               mtlk_df_ui_unreg_core_status },
  { mtlk_df_ui_reg_debug_igmp_read,           mtlk_df_ui_unreg_debug_igmp_read },
  { mtlk_df_ui_reg_reset_stats,               mtlk_df_ui_unreg_reset_stats },
  { mtlk_df_ui_reg_aocs_history,              mtlk_df_ui_unreg_aocs_history },
  { mtlk_df_ui_reg_aocs_table,                mtlk_df_ui_unreg_aocs_table },
  { mtlk_df_ui_reg_aocs_channels,             mtlk_df_ui_unreg_aocs_channels },
  { mtlk_df_ui_reg_aocs_penalties,            mtlk_df_ui_unreg_aocs_penalties },
#ifdef AOCS_DEBUG
  { mtlk_df_ui_reg_aocs_proc_cl,              mtlk_df_ui_unreg_aocs_proc_cl },
#endif
  { mtlk_df_ui_reg_debug_l2nat,               mtlk_df_ui_unreg_debug_l2nat },
  { mtlk_df_ui_reg_l2nat_clear_table,         mtlk_df_ui_unreg_l2nat_clear_table },
  { mtlk_df_ui_reg_debug_reordering_stats,    mtlk_df_ui_unreg_debug_reordering_stats },
  { mtlk_df_ui_reg_ee_caps,                   mtlk_df_ui_unreg_ee_caps },
  { mtlk_df_ui_reg_debug_send_queue,          mtlk_df_ui_unreg_debug_send_queue },
  { mtlk_df_ui_reg_serializer_dump,           mtlk_df_ui_unreg_serializer_dump },
};

int __MTLK_IFUNC
mtlk_df_ui_iw_bcl_mac_data_get (struct net_device *dev,
            struct iw_request_info *info,
            union iwreq_data *wrqu, char *extra)
{
  int res = 0;
  UMI_BCL_REQUEST req;
  UMI_BCL_REQUEST *req_result;
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);
  mtlk_clpb_t *clpb = NULL;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (copy_from_user(&req, wrqu->data.pointer, sizeof(req)) != 0) {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  /* Send request to core */
  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_GET_BCL_MAC_DATA,
                                  &clpb, &req, sizeof(req));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_BCL_MAC_DATA, FALSE);
  if (MTLK_ERR_OK != res) {
    goto finish;
  }

  /* Handle result */
  req_result = mtlk_clpb_enum_get_next(clpb, NULL);

  if (NULL == req_result) {
    res = MTLK_ERR_UNKNOWN;
  } else if (copy_to_user(wrqu->data.pointer, req_result, sizeof(*req_result)) != 0) {
    res = MTLK_ERR_UNKNOWN;
  }

  mtlk_clpb_delete(clpb);
finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_iw_bcl_mac_data_set (struct net_device *dev,
            struct iw_request_info *info,
            union iwreq_data *wrqu, char *extra)
{
  int res = 0;
  UMI_BCL_REQUEST req;
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);
  mtlk_clpb_t *clpb = NULL;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (copy_from_user(&req, wrqu->data.pointer, sizeof(req)) != 0) {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  /* Send request to core */
  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_SET_BCL_MAC_DATA,
                                  &clpb, &req, sizeof(req));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_SET_BCL_MAC_DATA, TRUE);

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_stop_lower_mac (struct net_device *dev,
                                       struct iw_request_info *info,
                                       union iwreq_data *wrqu,
                                       char *extra)
{
  int res = 0;
  mtlk_clpb_t *clpb = NULL;
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_STOP_LM, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_STOP_LM, TRUE);

  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_mac_calibrate (struct net_device *dev,
                                      struct iw_request_info *info,
                                      union iwreq_data *wrqu,
                                      char *extra)
{
  int res = 0;
  mtlk_clpb_t *clpb = NULL;
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_MAC_CALIBRATE, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_MAC_CALIBRATE, TRUE);

  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_iw_generic (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu, char *extra)
{
  int res = MTLK_ERR_OK;
  UMI_GENERIC_MAC_REQUEST req;
  UMI_GENERIC_MAC_REQUEST *preq = NULL;
  uint32 preq_size = 0;
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);
  mtlk_clpb_t *clpb = NULL;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (0 != copy_from_user(&req, wrqu->data.pointer, sizeof(req))) {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_GET_IW_GENERIC,
                                  &clpb, &req, sizeof(req));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_IW_GENERIC, FALSE);

  if (MTLK_ERR_OK != res) {
    goto finish;
  }

  preq = mtlk_clpb_enum_get_next(clpb, &preq_size);

  MTLK_ASSERT(NULL != preq);
  MTLK_ASSERT(sizeof(*preq) == preq_size);

  ILOG2_DDDDDD("Generic opcode %ux, %ud dwords, action 0x%ux results 0x%ux 0x%ux 0x%ux\n",
         (unsigned int)preq->opcode,
         (unsigned int)preq->size,
         (unsigned int)preq->action,
         (unsigned int)preq->res0,
         (unsigned int)preq->res1,
         (unsigned int)preq->res2);

  if (0 != copy_to_user(wrqu->data.pointer, preq, sizeof(*preq))) {
    ELOG_V("Unable to copy iw generic data");
    res = MTLK_ERR_PARAMS;
  }

  mtlk_clpb_delete(clpb);
finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_ctrl_mac_gpio (struct net_device *dev,
                                      struct iw_request_info *info,
                                      union iwreq_data *wrqu,
                                      char *extra)
{
  int res = 0;
  mtlk_df_user_t* df_user = (mtlk_df_user_t*) netdev_priv(dev);
  mtlk_clpb_t *clpb = NULL;
  int *ptr_extra = (int *)extra;
  UMI_SET_LED leds_status;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);
  ILOG3_DD("ptr_extra = 0x%x, ptr_extra1 = 0x%x", ptr_extra[0], ptr_extra[1]);

  memset(&leds_status, 0, sizeof(leds_status));

  leds_status.u8BasebLed = (uint8)(ptr_extra[0] & 0xFF);
  leds_status.u8LedStatus = (uint8)(ptr_extra[1] & 0xFF);
 
  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_CTRL_MAC_GPIO,
                                 &clpb, &leds_status, sizeof(leds_status));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_CTRL_MAC_GPIO, TRUE);

  return _mtlk_df_mtlk_to_linux_error_code(res);
}

/*
 * Due to lack of synchronization between fw-to-driver and driver-to-user
 * Network Mode should be converted on driver-to-user border.
 */
uint8 __MTLK_IFUNC
_net_mode_ingress_filter (uint8 ingress_net_mode)
{
  switch (ingress_net_mode) {
  case NETWORK_MODE_11A:
    return NETWORK_11A_ONLY;
  case NETWORK_MODE_11ABG:
    return NETWORK_11ABG_MIXED;
  case NETWORK_MODE_11ABGN:
    return NETWORK_11ABGN_MIXED;
  case NETWORK_MODE_11AN:
    return NETWORK_11AN_MIXED;
  case NETWORK_MODE_11B:
    return NETWORK_11B_ONLY;
  case NETWORK_MODE_11BG:
    return NETWORK_11BG_MIXED;
  case NETWORK_MODE_11BGN:
    return NETWORK_11BGN_MIXED;
  case NETWORK_MODE_11GN:
    return NETWORK_11GN_MIXED;
  case NETWORK_MODE_11G:
    return NETWORK_11G_ONLY;
  case NETWORK_MODE_11N2:
    return NETWORK_11N_2_4_ONLY;
  case NETWORK_MODE_11N5:
    return NETWORK_11N_5_ONLY;
  default:
    break;
  }

  return NETWORK_NONE;
}

uint8 __MTLK_IFUNC
_net_mode_egress_filter (uint8 egress_net_mode)
{
  switch (egress_net_mode) {
  case NETWORK_11ABG_MIXED:
    return NETWORK_MODE_11ABG;
  case NETWORK_11ABGN_MIXED:
    return NETWORK_MODE_11ABGN;
  case NETWORK_11B_ONLY:
    return NETWORK_MODE_11B;
  case NETWORK_11G_ONLY:
    return NETWORK_MODE_11G;
  case NETWORK_11N_2_4_ONLY:
    return NETWORK_MODE_11N2;
  case NETWORK_11BG_MIXED:
    return NETWORK_MODE_11BG;
  case NETWORK_11GN_MIXED:
    return NETWORK_MODE_11GN;
  case NETWORK_11BGN_MIXED:
    return NETWORK_MODE_11BGN;
  case NETWORK_11A_ONLY:
    return NETWORK_MODE_11A;
  case NETWORK_11N_5_ONLY:
    return NETWORK_MODE_11N5;
  case NETWORK_11AN_MIXED:
    return NETWORK_MODE_11AN;
  default:
    break;
  }

  ASSERT(0);

  return 0; /* just fake cc */
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getname (struct net_device *dev,
                          struct iw_request_info *info,
                          char *name,
                          char *extra)
{
  uint32  net_mode = NUM_OF_NETWORK_MODES;
  int res = MTLK_ERR_NOT_HANDLED;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  res = _mtlk_df_user_iwpriv_get_core_param(df_user, PRM_ID_NETWORK_MODE, (char*)&net_mode, NULL);
  if (MTLK_ERR_OK == res) {
    strcpy(name, net_mode_to_string(_net_mode_ingress_filter(net_mode)));
  }

  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getmode (struct net_device *dev,
                                struct iw_request_info *info,
                                u32 *mode,
                                char *extra)
{
  mtlk_df_user_t* df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (mtlk_df_is_ap(df_user->df))
    *mode = IW_MODE_MASTER;
  else
    *mode = IW_MODE_INFRA;

  return 0;
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setnick (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra)
{
  int res = MTLK_ERR_OK;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (0 == wrqu->essid.length) {
    goto finish;
  }

  if (MTLK_ESSID_MAX_SIZE < wrqu->essid.length) {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  /* WARNING: Take care about kernel and WE version compatibility:
   * the NickName can be passed without null terminated character*/
  res = _mtlk_df_user_iwpriv_set_core_param(df_user, PRM_ID_NICK_NAME, extra, &wrqu->essid.length);

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getnick (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra)
{
  int res = MTLK_ERR_PARAMS;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if ((NULL == extra) || (0 == wrqu->essid.length)) {
    goto finish;
  }

  res = _mtlk_df_user_iwpriv_get_core_param(df_user, PRM_ID_NICK_NAME, extra, &wrqu->essid.length);

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setessid (struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu,
                           char *extra)
{
  int res = MTLK_ERR_OK;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (0 == wrqu->essid.length) {
    goto finish;
  }

  if (MTLK_ESSID_MAX_SIZE < wrqu->essid.length) {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  /* WARNING: Take care about kernel and WE version compatibility:
  * the ESSID can be passed without null terminated character*/
  res = _mtlk_df_user_iwpriv_set_core_param(df_user, PRM_ID_ESSID, extra, &wrqu->essid.length);

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getessid (struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu,
                           char *extra)
{
  int res = MTLK_ERR_PARAMS;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if ((NULL == extra) || (0 == wrqu->essid.length)) {
    goto finish;
  }

  res = _mtlk_df_user_iwpriv_get_core_param(df_user, PRM_ID_ESSID, extra, &wrqu->essid.length);

  if (MTLK_ERR_OK == res) {
    wrqu->data.flags = 1;
  }

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getap (struct net_device *dev,
                              struct iw_request_info *info,
                              union iwreq_data *wrqu,
                              char *extra)
{
  int                 res = MTLK_ERR_PARAMS;
  mtlk_df_user_t      *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (NULL == wrqu) {
    goto finish;
  }

  if (mtlk_df_is_ap(df_user->df)) {
    res = mtlk_df_ui_linux_ioctl_get_mac_addr(dev, info, wrqu, extra);
  } else {
    /* STA: Return connected BSSID */
    uint16              len;
    res = _mtlk_df_user_iwpriv_get_core_param(df_user, PRM_ID_BSSID, wrqu->addr.sa_data, &len);
    MTLK_ASSERT(ETH_ALEN == len);
  }

  if (MTLK_ERR_OK == res) {
    wrqu->addr.sa_family = ARPHRD_ETHER;
  }

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getfreq (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra)
{
  int res = MTLK_ERR_NOT_HANDLED;
  uint32 channel = 0;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  res = _mtlk_df_user_iwpriv_get_core_param(df_user, PRM_ID_CHANNEL, (char*)&channel, NULL);
  if (MTLK_ERR_OK != res) {
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  if (0 == channel) {
    ILOG2_S("%s: Channel autoselection enabled", dev->name);
    wrqu->freq.e = 0;
    wrqu->freq.m = 0;
    wrqu->freq.i = 0;
#if WIRELESS_EXT >= 17
    wrqu->freq.flags = IW_FREQ_AUTO;
#endif
  } else {
    ILOG2_SD("%s: Value of channel is %i", dev->name, channel);
    wrqu->freq.e = 0;
    wrqu->freq.m = channel;
    wrqu->freq.i = 0;
#if WIRELESS_EXT >= 17
    wrqu->freq.flags = IW_FREQ_FIXED;
#endif
  }

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setfreq (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra)
{
  int res = MTLK_ERR_PARAMS;
  uint32 channel = 0;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (!mtlk_df_is_ap(df_user->df)) {
    ILOG2_S("%s: Channel selection unsupported on STA.", dev->name);
    res = MTLK_ERR_NOT_SUPPORTED;
    goto finish;
  }

  /*
   * To enable AOCS
   * with WE < 17 use 'iwconfig <IF> channel 0'
   * with WE >= 17 also can use 'iwconfig <IF> channel auto'
   */

#if WIRELESS_EXT < 17
  if ((wrqu->freq.e == 0) && (wrqu->freq.m == 0)) {
#else
  if ( ((wrqu->freq.e == 0) && (wrqu->freq.m == 0)) ||
       ((IW_FREQ_FIXED & wrqu->freq.flags) == 0) ) {
#endif /* WIRELESS_EXT < 17 */
    channel = 0;

  } else if (wrqu->freq.e == 0) {
    channel = wrqu->freq.m;

  } else {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  res = _mtlk_df_user_iwpriv_set_core_param(df_user, PRM_ID_CHANNEL, (char*)&channel, NULL);

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setrtsthr (struct net_device *dev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu,
                                  char *extra)
{
  int res = MTLK_ERR_PARAMS;
  int32 value;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (wrqu->rts.value < 0) {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  value = (wrqu->rts.value < _DF_RTS_THRESHOLD_MIN ? _DF_RTS_THRESHOLD_MIN : wrqu->rts.value);
  ILOG2_D("Set RTS/CTS threshold to value %i", value);

  res = _mtlk_df_user_iwpriv_set_core_param(df_user, MIB_RTS_THRESHOLD, (char*)&value, NULL);

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getrtsthr (struct net_device *dev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu,
                                  char *extra)
{
  int res = MTLK_ERR_PARAMS;
  int32 value;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  res = _mtlk_df_user_iwpriv_get_core_param(df_user, MIB_RTS_THRESHOLD, (char*)&value, NULL);
  if (MTLK_ERR_OK == res) {
    wrqu->rts.value = value;
    wrqu->rts.fixed = 1;
    wrqu->rts.disabled = 0;
    wrqu->rts.flags = 0;
  }

  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_settxpower (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra)
{
  int res = MTLK_ERR_PARAMS;
  int32 value;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (wrqu->txpower.value > 0) {
    ILOG2_D("Set TxPower to specified value %i mW", wrqu->txpower.value);
    value = wrqu->txpower.value;
  } else {
    ILOG2_V("Enable TxPower auto-calculation");
    value = 0; /* Auto */
  }

  res = _mtlk_df_user_iwpriv_set_core_param(df_user, MIB_TX_POWER, (char*)&value, NULL);

  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_gettxpower (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra)
{

  int res = MTLK_ERR_PARAMS;
  int32 value;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  res = _mtlk_df_user_iwpriv_get_core_param(df_user, MIB_TX_POWER, (char*)&value, NULL);
  if (MTLK_ERR_OK == res) {
    wrqu->txpower.value = value;
    wrqu->txpower.fixed = 1;
    wrqu->txpower.flags = IW_TXPOW_DBM;
    wrqu->txpower.disabled = 0;
  }

  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setretry (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra)
{
  int res = MTLK_ERR_PARAMS;
  int32 value;
  uint16 obj_id;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  value = wrqu->retry.value;

  /* Determine requested parameter */
  switch (wrqu->retry.flags) {
  case IW_RETRY_LIMIT:
  case IW_RETRY_LIMIT | IW_RETRY_MIN:
    obj_id = MIB_SHORT_RETRY_LIMIT;
    break;
  case IW_RETRY_LIMIT | IW_RETRY_MAX:
    obj_id = MIB_LONG_RETRY_LIMIT;
    break;
  case IW_RETRY_LIFETIME:
  case IW_RETRY_LIFETIME | IW_RETRY_MIN:
  case IW_RETRY_LIFETIME | IW_RETRY_MAX:
    /* WT send to us value in microseconds, but MAC accepts miliseconds */
    if (value < 1000) {
      goto finish;
    }
    obj_id = MIB_TX_MSDU_LIFETIME;
    value = value/1000;
    break;
  default:
    WLOG_D("Unknown parameter type: 0x%04x", wrqu->retry.flags);
    goto finish;
  }

  if (value >= 0) {
    ILOG2_DD("Set Retry limits (lifetime) to value %i %i ", value, wrqu->retry.value);
    res = _mtlk_df_user_iwpriv_set_core_param(df_user, obj_id, (char*)&value, NULL);
  }

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getretry (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra)
{
  int res = MTLK_ERR_PARAMS;
  uint16 obj_id;
  int    scale = 1;
  int32 value;
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  /* Determine requested parameter */
  switch (wrqu->retry.flags) {
  case 0:
  case IW_RETRY_LIFETIME:
  case IW_RETRY_LIFETIME | IW_RETRY_MIN:
  case IW_RETRY_LIFETIME | IW_RETRY_MAX:
    obj_id = MIB_TX_MSDU_LIFETIME;
    /* WT expects value in microseconds, but MAC work with miliseconds */
    scale = 1000;
    wrqu->retry.flags = IW_RETRY_LIFETIME;
    break;
  case IW_RETRY_LIMIT:
  case IW_RETRY_LIMIT | IW_RETRY_MIN:
    obj_id = MIB_SHORT_RETRY_LIMIT;
    wrqu->retry.flags = IW_RETRY_LIMIT | IW_RETRY_MIN;
    break;
  case IW_RETRY_LIMIT | IW_RETRY_MAX:
    obj_id = MIB_LONG_RETRY_LIMIT;
    wrqu->retry.flags = IW_RETRY_LIMIT | IW_RETRY_MAX;
    break;
  default:
    WLOG_D("Unknown parameter type: 0x%04x", wrqu->retry.flags);
    goto finish;
  }

  res = _mtlk_df_user_iwpriv_get_core_param(df_user, obj_id, (char*)&value, NULL);
  if (MTLK_ERR_OK == res) {
    wrqu->retry.value = scale*value;
    wrqu->retry.fixed = 1;
    wrqu->retry.disabled = 0;
  }

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getrange (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra)
{
  int res = MTLK_ERR_NOT_HANDLED;
  mtlk_df_user_t* df_user = (mtlk_df_user_t*)netdev_priv(dev);
  mtlk_core_ui_range_info_t *range_info = NULL;
  mtlk_handle_t hcore_data;
  struct iw_range *range = (struct iw_range *) extra;
  int idx;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  res= _mtlk_df_user_pull_core_data(df_user->df, MTLK_CORE_REQ_GET_RANGE_INFO, TRUE, (void**) &range_info, NULL, &hcore_data);

  if(MTLK_ERR_OK != res) {
    goto finish;
  }

  /** Fill output data **/
  wrqu->data.length = sizeof(struct iw_range);
  /* In kernes < 2.6.22 range is not zeroed */
  memset(range, 0, sizeof(struct iw_range));

  range->we_version_compiled = WIRELESS_EXT;

  /* kernel MUST be patched up to this version if security is needed */
  if (WIRELESS_EXT < 18)
    range->we_version_compiled = 18;

  range->we_version_source = 18; /* what version we support */

  /* WEP stuff */
  range->num_encoding_sizes  = 2;
  range->encoding_size[0]    = 5;  /* 40 bit */
  range->encoding_size[1]    = 13; /* 104 bit */
  range->max_encoding_tokens = 4;

  /* TxPower stuff */
  range->txpower_capa = IW_TXPOW_DBM;

  /* Maximum quality */
  range->max_qual.qual = 5;

  /** Fill output bitrates data **/
  range->num_bitrates = 0;
  if (IW_MAX_BITRATES < range_info->num_bitrates) {
    res = MTLK_ERR_UNKNOWN;
    goto error_delete_data;
  } else if (0 < range_info->num_bitrates) {
    range->num_bitrates = range_info->num_bitrates;
    /* Array of bitrates is sorted and consist of only unique elements */
    for (idx = 0; idx <= range_info->num_bitrates; idx++) {
        range->bitrate[idx] = range_info->bitrates[idx];
    }
  }

  /* Fill output channels and frequencies data */
  range->num_frequency = 0;
  range->num_channels = 0;

  if (0 < range_info->num_channels) {
    if (IW_MAX_FREQUENCIES < range_info->num_channels) {
      range_info->num_channels = IW_MAX_FREQUENCIES;
    }

    for(idx = 0; idx < range_info->num_channels; idx++) {
      range->freq[idx].i = range_info->channels[idx];
      range->freq[idx].m = channel_to_frequency(range_info->channels[idx]);
      range->freq[idx].e = 6;
      //range->freq[i].flags = 0; /* fixed/auto (not used by iwlist currently)*/
      range->num_frequency++;
    }
  }

  range->num_channels = range->num_frequency;

error_delete_data:
  _mtlk_df_user_free_core_data(df_user->df, hcore_data);
finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getaplist (struct net_device *dev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu,
                                  char *extra)
{
  int                               res = MTLK_ERR_NOT_HANDLED;
  mtlk_df_user_t                    *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  mtlk_clpb_t                       *clpb = NULL;
  struct sockaddr                   *list = (struct sockaddr*)extra;
  int                               idx = 0;
  uint32                            size;
  mtlk_stadb_stat_t                *stadb_stat;
  mtlk_core_ui_get_stadb_status_req_t get_stadb_status_req;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (!mtlk_df_is_ap(df_user->df)) {
    /* This is STA - do not print anything */
    res = MTLK_ERR_NOT_SUPPORTED;
    wrqu->data.length = 0;
    goto err_ret;
  }

  get_stadb_status_req.get_hostdb = FALSE;
  get_stadb_status_req.use_cipher = TRUE;
  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_GET_STADB_STATUS,
                                  &clpb, &get_stadb_status_req, sizeof(get_stadb_status_req));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_STADB_STATUS, FALSE);
  if(MTLK_ERR_OK != res) {
    goto err_ret;
  }

  /* Print list of connected STAs */
  while (NULL != (stadb_stat = mtlk_clpb_enum_get_next(clpb, &size))) {
    if ((sizeof(*stadb_stat) != size) || (STAT_ID_STADB != stadb_stat->type)) {
      res = MTLK_ERR_UNKNOWN;
      goto delete_clpb;
    }

    if (idx == wrqu->data.length) break;

    list[idx].sa_family = ARPHRD_ETHER;
    memcpy(list[idx].sa_data, stadb_stat->u.general_stat.addr.au8Addr, ETH_ALEN);
    idx++;
  }

  wrqu->data.length = idx;

delete_clpb:
  mtlk_clpb_delete(clpb);
err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_connection_info (struct net_device *dev,
                                            struct iw_request_info *info,
                                            union iwreq_data *wrqu,
                                            char *extra)
{
  int                               res = MTLK_ERR_NOT_HANDLED;
  mtlk_df_user_t                    *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  mtlk_stadb_stat_t                 *stadb_stat = NULL;
  mtlk_gen_core_cfg_t               *core_cfg = NULL;
  uint32                            size;
  char                              *str = extra;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  wrqu->data.length = 0;
  /* in case we don't add anything to the string we relay on string length */
  *str = '\0';

  if (!mtlk_df_is_ap(df_user->df)) {
    /* This is STA */
    uint16  channel = 0;
    uint8   bonding = 0;
    uint8   frequency_band_cur = 0;
    uint8   spectrum_mode = 0;
    mtlk_handle_t hcore_data;

    res = _mtlk_df_user_pull_core_data(df_user->df, MTLK_CORE_REQ_GET_CORE_CFG, FALSE, (void**) &core_cfg, NULL, &hcore_data);

    MTLK_CFG_GET_ITEM(core_cfg, channel, channel);
    MTLK_CFG_GET_ITEM(core_cfg, bonding, bonding);
    MTLK_CFG_GET_ITEM(core_cfg, frequency_band_cur, frequency_band_cur);
    MTLK_CFG_GET_ITEM(core_cfg, spectrum_mode, spectrum_mode);

    str += sprintf(str, "%sGHz,%d,%dMHz",
        MTLK_HW_BAND_5_2_GHZ == frequency_band_cur ? "5.2" : "2.4",
        channel,
        spectrum_mode == SPECTRUM_20MHZ ? 20 : 40);

    if (spectrum_mode == SPECTRUM_40MHZ)
      str += sprintf(str, ALTERNATE_LOWER == bonding ? ",Lower" : ",Upper");

    _mtlk_df_user_free_core_data(df_user->df, hcore_data);
  } else {     /* This is AP */
    mtlk_clpb_t                       *clpb = NULL;
    mtlk_core_ui_get_stadb_status_req_t get_stadb_status_req;

    get_stadb_status_req.get_hostdb = FALSE;
    get_stadb_status_req.use_cipher = TRUE;
    res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_GET_STADB_STATUS, &clpb,
                                    &get_stadb_status_req, sizeof(get_stadb_status_req));
    res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_STADB_STATUS, FALSE);
    if(MTLK_ERR_OK != res) {
      goto err_ret;
    }

    while (NULL != (stadb_stat = mtlk_clpb_enum_get_next(clpb, &size))) {
      if ((sizeof(*stadb_stat) != size) || (STAT_ID_STADB != stadb_stat->type)) {
        res = MTLK_ERR_UNKNOWN;
        mtlk_clpb_delete(clpb);
        goto err_ret;
      } else {
        str += sprintf(str, "\n" MAC_PRINTF_FMT, MAC_PRINTF_ARG(stadb_stat->u.general_stat.addr.au8Addr));
        str += sprintf(str, " %10d", stadb_stat->u.general_stat.sta_rx_packets);
        str += sprintf(str, " %10d", stadb_stat->u.general_stat.sta_tx_packets);
      }
    }

    mtlk_clpb_delete(clpb);
  }

  wrqu->data.length = strlen(extra);

err_ret:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

/* Driver support only 40 bit and 104 bit length WEP keys.
 * Also, according to IEEE standard packet transmission
 * with zero-filled WEP key is not supported.
 */
static int
mtlk_df_ui_validate_wep_key (uint8 *key, size_t length) {
  int i;
  ASSERT(key);

  /* Check key length */
  if ((length != MIB_WEP_KEY_WEP1_LENGTH) && /* 40 bit */
      (length != MIB_WEP_KEY_WEP2_LENGTH)) { /* 104 bit */
    return MTLK_ERR_PARAMS;
  }

  /* Validate key's value */
  for (i = 0; i < length; i++)
    if (key[i] == '\0')
      return MTLK_ERR_PARAMS;

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setenc (struct net_device *dev,
                               struct iw_request_info *info,
                               union  iwreq_data *wrqu,
                               char   *extra)
{
  int                      res = MTLK_ERR_NOT_HANDLED;
  mtlk_df_user_t           *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  mtlk_clpb_t              *clpb = NULL;
  mtlk_core_ui_enc_cfg_t   enc_cfg;
  mtlk_core_ui_enc_cfg_t   *enc_cfg_res;
  uint32                   size;


  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  memset(&enc_cfg, 0, sizeof(enc_cfg));
  enc_cfg.authentication = -1;

  if (wrqu->data.flags & IW_ENCODE_DISABLED) {
    /* Disable WEP encryption */
    enc_cfg.wep_enabled = FALSE;
    enc_cfg.authentication = 0;
  } else {
    /* Configure WEP encryption */
    enc_cfg.wep_enabled = TRUE;

    if ((wrqu->data.flags & IW_ENCODE_OPEN) &&
        (wrqu->data.flags & IW_ENCODE_RESTRICTED)) { /* auto mode */
      enc_cfg.authentication = MIB_AUTHENTICATION_AUTO;
    } else if (wrqu->data.flags & IW_ENCODE_OPEN) { /* Open mode */
      enc_cfg.authentication = MIB_AUTHENTICATION_OPEN_SYSTEM;
    } else if (wrqu->data.flags & IW_ENCODE_RESTRICTED) {/* Restricted mode */
      enc_cfg.authentication = MIB_AUTHENTICATION_SHARED_KEY;
    }

    /* Validate and adjust key index
     *
     * Up to 4 WEP keys supported.
     * WE enumerate WEP keys from 1 to N, and 0 - is current TX key.
     */
    enc_cfg.key_id = (uint8)(wrqu->data.flags & IW_ENCODE_INDEX);
    if (MIB_WEP_N_DEFAULT_KEYS < enc_cfg.key_id ) {
      WLOG_S("%s: Unsupported WEP key index", dev->name);
      res = MTLK_ERR_PARAMS;
      goto finish;
    }

    if (0 != enc_cfg.key_id) {
      enc_cfg.key_id--;
      enc_cfg.update_current_key = FALSE;
    } else {
      enc_cfg.update_current_key = TRUE;
    }

    /* Set WEP key */
    /* Validate key first */
    if (0 != wrqu->data.length){
      if (mtlk_df_ui_validate_wep_key(extra, wrqu->data.length) != MTLK_ERR_OK) {
        WLOG_S("%s: Bad WEP key", dev->name);
        res = MTLK_ERR_PARAMS;
        goto finish; /* res = -EINVAL */
      }
      enc_cfg.wep_keys.sKey[enc_cfg.key_id].u8KeyLength = wrqu->data.length;
      memcpy(enc_cfg.wep_keys.sKey[enc_cfg.key_id].au8KeyData, extra, wrqu->data.length);
    }
  }
  
  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_SET_WEP_ENC_CFG,
                                  &clpb, (char*)&enc_cfg, sizeof(enc_cfg));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_SET_WEP_ENC_CFG, FALSE);
  if(MTLK_ERR_OK != res) {
    goto finish;
  }

  enc_cfg_res = mtlk_clpb_enum_get_next(clpb, &size);
  if (NULL != enc_cfg_res) {
    if (sizeof(*enc_cfg_res) != size) {
      res = MTLK_ERR_UNKNOWN;
      goto error_delete_clpb;
    } else {
      /* Update key index */
      wrqu->data.flags |= ((uint16)(enc_cfg_res->key_id + 1) & IW_ENCODE_INDEX);
    }
  }

error_delete_clpb:
  mtlk_clpb_delete(clpb);
finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getenc (struct net_device *dev,
                               struct iw_request_info *info,
                               union  iwreq_data *wrqu,
                               char   *extra)
{
  int                      res = MTLK_ERR_NOT_HANDLED;
  mtlk_df_user_t           *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  mtlk_clpb_t              *clpb = NULL;
  mtlk_core_ui_enc_cfg_t   *enc_cfg_res;
  uint32                   size;
  uint8                    key_index;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_GET_WEP_ENC_CFG, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_WEP_ENC_CFG, FALSE);
  if(MTLK_ERR_OK != res) {
    goto finish;
  }

  enc_cfg_res = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == enc_cfg_res) || (sizeof(*enc_cfg_res) != size) ) {
      res = MTLK_ERR_UNKNOWN;
      goto delete_clpb;
  }

  if (!enc_cfg_res->wep_enabled) {
    /* WEP is disabled - fill iw response and finish processing*/
    wrqu->data.length = 0;
    wrqu->data.flags  = IW_ENCODE_DISABLED;
    goto delete_clpb;
  }

  /* Validate, adjust and report key index
   *
   * Up to 4 WEP keys supported.
   * WE enumerate WEP keys from 1 to N, and 0 - is current TX key.
   */
  key_index = (uint8)(wrqu->data.flags & IW_ENCODE_INDEX);
  if (MIB_WEP_N_DEFAULT_KEYS < key_index ) {
    WLOG_S("%s: Unsupported WEP key index", dev->name);
    res = MTLK_ERR_PARAMS;
    goto delete_clpb;
  }

  if (key_index == 0) {
    wrqu->data.flags |= ((uint16)(enc_cfg_res->key_id + 1) & IW_ENCODE_INDEX);
    key_index = enc_cfg_res->key_id;
  }
  else {
    key_index--;
  }

  /* Report access policy */
  if (MIB_AUTHENTICATION_SHARED_KEY == enc_cfg_res->authentication) {
    wrqu->data.flags |= IW_ENCODE_RESTRICTED;
  } else if (MIB_AUTHENTICATION_OPEN_SYSTEM == enc_cfg_res->authentication) {
    wrqu->data.flags |= IW_ENCODE_OPEN;
  } else if (MIB_AUTHENTICATION_AUTO == enc_cfg_res->authentication) {
    wrqu->data.flags |= IW_ENCODE_OPEN|IW_ENCODE_RESTRICTED;
  }

  /* Get requested key */
  wrqu->data.length = enc_cfg_res->wep_keys.sKey[key_index].u8KeyLength;
  memcpy(extra, enc_cfg_res->wep_keys.sKey[key_index].au8KeyData, wrqu->data.length);

delete_clpb:
  mtlk_clpb_delete(clpb);
finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setauth (struct net_device *dev,
                                struct iw_request_info *info,
                                union  iwreq_data *wrqu,
                                char   *extra)
{
  int                      res = MTLK_ERR_OK;
  mtlk_df_user_t           *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  mtlk_clpb_t              *clpb = NULL;
  mtlk_core_ui_auth_cfg_t   auth_cfg;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  memset(&auth_cfg, 0, sizeof(auth_cfg));
  auth_cfg.wep_enabled = -1;
  auth_cfg.rsn_enabled = -1;
  auth_cfg.authentication = -1;

  ILOG2_SDD("%s: flags %i, value %i", dev->name, wrqu->param.flags, wrqu->param.value);

  switch (wrqu->param.flags & IW_AUTH_INDEX) {
  case IW_AUTH_WPA_VERSION:
    if ((wrqu->param.value & IW_AUTH_WPA_VERSION_WPA) ||
        (wrqu->param.value & IW_AUTH_WPA_VERSION_WPA2)) {
      auth_cfg.rsn_enabled = 1;
    }
    break;

  case IW_AUTH_CIPHER_PAIRWISE:
  case IW_AUTH_CIPHER_GROUP:
    if (wrqu->param.value & (IW_AUTH_CIPHER_WEP40|IW_AUTH_CIPHER_WEP104)) {
      auth_cfg.wep_enabled = 1;
    } else {
      auth_cfg.wep_enabled = 0;
    }
    break;


  case IW_AUTH_80211_AUTH_ALG:
    if ((wrqu->param.value & IW_AUTH_ALG_OPEN_SYSTEM) &&
        (wrqu->param.value & IW_AUTH_ALG_SHARED_KEY)) {   /* automatic mode */
      auth_cfg.authentication = MIB_AUTHENTICATION_AUTO;
    } else if (wrqu->param.value & IW_AUTH_ALG_SHARED_KEY) {
      auth_cfg.authentication = MIB_AUTHENTICATION_SHARED_KEY;
    } else if (wrqu->param.value & IW_AUTH_ALG_OPEN_SYSTEM) {
      auth_cfg.authentication = MIB_AUTHENTICATION_OPEN_SYSTEM;
    } else {
      res = MTLK_ERR_PARAMS;
    }
    break;

  case IW_AUTH_KEY_MGMT:
  case IW_AUTH_PRIVACY_INVOKED:
  case IW_AUTH_RX_UNENCRYPTED_EAPOL:
  case IW_AUTH_DROP_UNENCRYPTED:
  case IW_AUTH_TKIP_COUNTERMEASURES:
  case IW_AUTH_WPA_ENABLED:
    res = MTLK_ERR_NOT_HANDLED;
    break;

  default:
    res = MTLK_ERR_NOT_SUPPORTED;
  }

  if (MTLK_ERR_NOT_SUPPORTED == res) {
    res = MTLK_ERR_OK;
    goto finish;
  } else if (MTLK_ERR_OK == res) {
    res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_SET_AUTH_CFG,
                                    &clpb, (char*)&auth_cfg, sizeof(auth_cfg));
    res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_SET_AUTH_CFG, TRUE);
  }

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static int
mtlk_df_ui_iw_cipher_encode2auth(int enc_cipher)
{
  switch (enc_cipher) {
  case IW_ENCODE_ALG_NONE:
    return IW_AUTH_CIPHER_NONE;
  case IW_ENCODE_ALG_WEP:
    return IW_AUTH_CIPHER_WEP40;
  case IW_ENCODE_ALG_TKIP:
    return IW_AUTH_CIPHER_TKIP;
  case IW_ENCODE_ALG_CCMP:
    return IW_AUTH_CIPHER_CCMP;
  default:
    return 0;
  }
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getauth (struct net_device *dev,
                                struct iw_request_info *info,
                                struct iw_param *param,
                                char *extra)
{
  int                       res = MTLK_ERR_NOT_HANDLED;
  mtlk_df_user_t            *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  mtlk_clpb_t               *clpb = NULL;
  mtlk_core_ui_auth_state_t *auth_state;
  uint32                    size;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  ILOG2_SDD("%s: flags %i, value %i", dev->name, param->flags, param->value);

  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_GET_AUTH_CFG, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_AUTH_CFG, FALSE);
  if (MTLK_ERR_OK != res) {
    goto finish;
  }

  auth_state = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == auth_state) || (sizeof(*auth_state) != size) ) {
      res = MTLK_ERR_UNKNOWN;
      goto delete_clpb;
  }

  /* Fill result */

  param->value = 0;
  switch (param->flags & IW_AUTH_INDEX) {
  case IW_AUTH_WPA_VERSION:
    if (!auth_state->rsnie.au8RsnIe[0]) {
      res = MTLK_ERR_PARAMS;
    } else if (auth_state->rsnie.au8RsnIe[0] == RSN_INFO_ELEM) {
      param->value = IW_AUTH_WPA_VERSION_WPA2;
    } else if (auth_state->rsnie.au8RsnIe[0] == GENERIC_INFO_ELEM) {
      param->value = IW_AUTH_WPA_VERSION_WPA;
    }
    break;

  case IW_AUTH_CIPHER_PAIRWISE:
    if (0 > auth_state->cipher_pairwise) {
      res = MTLK_ERR_NOT_SUPPORTED;
    } else {
      param->value = mtlk_df_ui_iw_cipher_encode2auth(auth_state->cipher_pairwise);
    }
    break;

  case IW_AUTH_CIPHER_GROUP:
    if (!auth_state->group_cipher) {
      res = MTLK_ERR_PARAMS;
    } else {
      param->value = mtlk_df_ui_iw_cipher_encode2auth(auth_state->group_cipher);
    }
    break;

  default:
    res = MTLK_ERR_NOT_SUPPORTED;
  }

delete_clpb:
  mtlk_clpb_delete(clpb);
finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setgenie (struct net_device *dev,
                                 struct iw_request_info *info,
                                 struct iw_point *data,
                                 char *extra)
{
  int                       res = MTLK_ERR_PARAMS;
  mtlk_df_user_t           *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  mtlk_clpb_t              *clpb = NULL;
  mtlk_core_ui_genie_cfg_t genie_cfg;
  u8   *ie = (u8 *)extra;
  u8   *oui = ie +2;
  u8    wpa_oui[] = {0x00, 0x50, 0xf2, 0x01};
  u8    wps_oui[] = {0x00, 0x50, 0xf2, 0x04};
  int   ie_len = data->length;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  memset(&genie_cfg, 0, sizeof(genie_cfg));
  ILOG2_SDD("%s: IE %i, length %i", dev->name, ie ? ie[0] : 0, ie_len);

  if ((0 != ie_len) && (NULL == ie)) {
    goto finish;
  }

  if ( (NULL != ie) && (0 != ie_len) && (GENERIC_INFO_ELEM == ie[0]) &&
       (0 != memcmp(oui, wpa_oui, sizeof(wpa_oui)) ) ) {
    ILOG2_D("WPS IE, type %i", data->flags);
    mtlk_dump(3, ie, ie_len, "dump of WPS IE:");
    // Set wps_in_progress flag. In AP mode parse WPS IE
    // and check Selected Registrar attribute. In STA mode
    // it is enough to check that WPS IE is not zero.
    if (mtlk_df_is_ap(df_user->df)) { /* AP */
      /* Parse WPS IE and */
      u8 *p = ie;
      /* Go to WPS OUI */
      while (memcmp(oui, wps_oui, sizeof(wps_oui)) != 0) {
        p += p[1] + 2;
        if (p >= ie+ie_len) {
          WLOG_V("WPS OUI was not found");
          goto finish;
        }
        oui = p + 2;
      }
      p = oui + sizeof(wps_oui);
      MTLK_CFG_SET_ITEM(&genie_cfg, wps_in_progress, 0);

      while (p < ie+ie_len) {
        ILOG2_D("WPS IE, attr %04x", ntohs(*(uint16*)p));
        if (ntohs(get_unaligned((uint16*)p)) == 0x1041) { // Selected Registrar
          if (p[4] == 1) {
            MTLK_CFG_SET_ITEM(&genie_cfg, wps_in_progress, 1);
          }
          break;
        }
        p += 4 + ntohs(get_unaligned((uint16*)(p+2)));  // Go to next attribute
      }
    } else { /* STA */
      MTLK_CFG_SET_ITEM(&genie_cfg, rsnie_reset, 1);
      MTLK_CFG_SET_ITEM(&genie_cfg, wps_in_progress, 1);
    }

    if (sizeof(genie_cfg.gen_ie) < ie_len) {
      WLOG_V("WPS gen ie invalid length");
      goto finish;
    }

    MTLK_CFG_SET_ITEM(&genie_cfg, gen_ie_set, 1);
    MTLK_CFG_SET_ITEM(&genie_cfg, gen_ie_type, data->flags);
    MTLK_CFG_SET_ITEM(&genie_cfg, gen_ie_len, ie_len);
    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&genie_cfg, gen_ie, memcpy,
                                  (genie_cfg.gen_ie, ie, ie_len));

    goto configure;
  }

  if (sizeof(UMI_RSN_IE) < ie_len ) {
    WLOG_DD("invalid RSN IE length (%i > %i)", ie_len, (int)sizeof(UMI_RSN_IE));
    goto finish;
  }

  MTLK_CFG_SET_ITEM(&genie_cfg, rsn_enabled, 0);

  if (ie_len) {
    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&genie_cfg, rsnie, memcpy, (&genie_cfg.rsnie, ie, ie_len));
    mtlk_dump(2, ie, ie_len, "dump of rsnie:");

    if (mtlk_df_is_ap(df_user->df) &&
        (ie[0] == GENERIC_INFO_ELEM || ie[0] == RSN_INFO_ELEM)) {
      MTLK_CFG_SET_ITEM(&genie_cfg, rsn_enabled, 1);
    }

  } else {  /* reset WPS IE case */

    // Note: in WPS mode data->flags represents the type of
    // WPS IE (for beacons, probe responces or probe reqs).
    // In STA mode flags == 1 (probe reqs type). So we
    // check it to avoid collisions with WPA IE reset.

    MTLK_CFG_SET_ITEM(&genie_cfg, rsnie_reset, 1);
#ifdef MTCFG_HOSTAP_06
    if (!mtlk_df_is_ap(df_user->df))
#else
    if (!mtlk_df_is_ap(df_user->df) && data->flags)
#endif
    {
      MTLK_CFG_SET_ITEM(&genie_cfg, wps_in_progress, 0);
    }

    MTLK_CFG_SET_ITEM(&genie_cfg, gen_ie_set, 1);
    MTLK_CFG_SET_ITEM(&genie_cfg, gen_ie_type, data->flags);
    MTLK_CFG_SET_ITEM(&genie_cfg, gen_ie_len, ie_len);
    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&genie_cfg, gen_ie, memcpy,
                                  (genie_cfg.gen_ie, ie, ie_len));
  }

configure:
  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_SET_GENIE_CFG,
                                  &clpb, (char*)&genie_cfg, sizeof(genie_cfg));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_SET_GENIE_CFG, TRUE);

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getencext (struct net_device *dev,
                                  struct iw_request_info *info,
                                  struct iw_point *encoding,
                                  char *extra)
{

  int                       res = MTLK_ERR_PARAMS;
  mtlk_df_user_t            *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  mtlk_clpb_t               *clpb = NULL;
  uint32                    size;
  UMI_GROUP_PN              *umi_gpn;
  struct iw_encode_ext      *ext = NULL;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  ext = (struct iw_encode_ext *)extra;
  if (ext == NULL)
    goto finish;

  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_GET_ENCEXT_CFG, &clpb, NULL, 0);
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_ENCEXT_CFG, FALSE);
  if (MTLK_ERR_OK != res) {
    goto finish;
  }

  umi_gpn = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == umi_gpn) || (sizeof(*umi_gpn) != size) ) {
      res = MTLK_ERR_UNKNOWN;
      goto delete_clpb;
  }

  memcpy(ext->rx_seq, umi_gpn->au8TxSeqNum, 6);
  ILOG2_DDDDDD("RSC:  %02x%02x%02x%02x%02x%02x",
      ext->rx_seq[0], ext->rx_seq[1], ext->rx_seq[2],
      ext->rx_seq[3], ext->rx_seq[4], ext->rx_seq[5]);

delete_clpb:
  mtlk_clpb_delete(clpb);
finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setencext (struct net_device *dev,
                                  struct iw_request_info *info,
                                  struct iw_point *encoding,
                                  char *extra)
{
  int                       res = MTLK_ERR_PARAMS;
  mtlk_df_user_t            *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  mtlk_clpb_t               *clpb = NULL;
  struct iw_encode_ext      *ext = NULL;
  mtlk_core_ui_encext_cfg_t encext_cfg;
  uint16                    tmp;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  ext = (struct iw_encode_ext *)extra;
  if (ext == NULL) {
    WLOG_S("%s: extra parameter expected", dev->name);
    goto finish;
  }

  memset(&encext_cfg, 0, sizeof(encext_cfg));
  ILOG2_DDD("alg %04x, enc flags %04x, ext flags %08x", ext->alg, encoding->flags, ext->ext_flags);

  /* Determine and validate the encoding algorithm */
  if (encoding->flags & IW_ENCODE_DISABLED)
    ext->alg = IW_ENCODE_ALG_NONE;
  switch (ext->alg) {
  case IW_ENCODE_ALG_NONE:
    /* IW_ENCODE_ALG_NONE - reset keys */
    MTLK_CFG_SET_ITEM(&encext_cfg, wep_enabled, 0);
    break;
  case IW_ENCODE_ALG_WEP:
    MTLK_CFG_SET_ITEM(&encext_cfg, wep_enabled, 1);
    break;
  case IW_ENCODE_ALG_TKIP:
    break;
  case IW_ENCODE_ALG_CCMP:
    break;
  default:
    WLOG_SD("%s: Unsupported encoding algorithm type (%u)", dev->name, ext->alg);
    res = MTLK_ERR_NOT_SUPPORTED;
    goto finish;
  }
  ILOG2_D("alg: type (%u)", ext->alg);
  MTLK_CFG_SET_ITEM(&encext_cfg, alg_type, ext->alg);

  /* Determine and validate the key index */
  tmp = (encoding->flags & IW_ENCODE_INDEX);
  if (MIB_WEP_N_DEFAULT_KEYS < tmp ) {
    WLOG_SD("%s: Invalid WEP key index (%u)", dev->name, tmp);
    goto finish;
  }
  if (0 != tmp) {
    tmp--;
  }
  ILOG2_D("key: key_idx (%u)", tmp);
  MTLK_CFG_SET_ITEM(&encext_cfg, key_idx, tmp);

  if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY) { /* Set default key index */
    MTLK_CFG_SET_ITEM(&encext_cfg, default_key_idx, tmp);
    ILOG2_D("key: default_key_idx (%u)", tmp);
  }

  /* Determine and validate the STA ether address */
  MTLK_CFG_SET_ITEM_BY_FUNC(&encext_cfg, sta_addr,
                            _mtlk_df_user_fill_ether_address, (&encext_cfg.sta_addr, &ext->addr), res);
  if (MTLK_ERR_OK != res) {
    WLOG_SY("%s: Invalid address (%Y)", dev->name, ext->addr.sa_data);
    goto finish;
  }
  ILOG2_Y("%Y", ext->addr.sa_data);

  /* Determine and validate the key length */
  tmp = 0;
  if (ext->ext_flags & (IW_ENCODE_EXT_SET_TX_KEY|IW_ENCODE_EXT_GROUP_KEY)) {
    tmp = ext->key_len;

    if ( (IW_ENCODE_ALG_WEP == ext->alg) || (IW_ENCODE_ALG_NONE == ext->alg) )
    {
      if ((0 < tmp) && (tmp != MIB_WEP_KEY_WEP1_LENGTH) && (tmp != MIB_WEP_KEY_WEP2_LENGTH)) {
        WLOG_SD("%s: Invalid WEP key length (%u)", dev->name, tmp);
        res = MTLK_ERR_PARAMS;
        goto finish;
      }
    }

    if ((UMI_RSN_TK1_LEN + UMI_RSN_TK2_LEN) < tmp) {
      WLOG_SD("%s: Invalid key length (%u)", dev->name, tmp);
      tmp = UMI_RSN_TK1_LEN + UMI_RSN_TK2_LEN;
    }
  }
  MTLK_CFG_SET_ITEM(&encext_cfg, key_len, tmp);
  ILOG2_D("key: key_len (%u)", tmp);

  /* Determine and validate the key */
  if (tmp) {
    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&encext_cfg, key, memcpy,
                                         (encext_cfg.key, ext->key, tmp));
    mtlk_dump(2, ext->key, tmp, "KEY:");
  }

  /* Determine and validate the RX seq */
  if (ext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID) {
     MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&encext_cfg, rx_seq, memcpy,
                                          (encext_cfg.rx_seq, ext->rx_seq, 6));
     mtlk_dump(2, ext->rx_seq, 6, "group RSC");
  }

  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_SET_ENCEXT_CFG,
                                  &clpb, (char*)&encext_cfg, sizeof(encext_cfg));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_SET_ENCEXT_CFG, TRUE);

finish:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

struct iw_statistics* __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_iw_stats (struct net_device *dev)
{
  mtlk_df_user_t            *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  mtlk_core_general_stats_t *general_stats;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  /* Get Core general information from DF buffer and don't call Core */
  general_stats = &df_user->slow_ctx->core_general_stats;

  df_user->slow_ctx->iw_stats.status = general_stats->net_state;
  df_user->slow_ctx->iw_stats.discard.nwid = general_stats->core_priv_stats.discard_nwi;
  df_user->slow_ctx->iw_stats.miss.beacon = general_stats->core_priv_stats.missed_beacon;

  if (!mtlk_df_is_ap(df_user->df) && (mtlk_core_net_state_is_connected(general_stats->net_state))) {

    df_user->slow_ctx->iw_stats.qual.updated = IW_QUAL_ALL_UPDATED | IW_QUAL_DBM;
    df_user->slow_ctx->iw_stats.qual.noise = general_stats->noise;
    df_user->slow_ctx->iw_stats.qual.level = general_stats->max_rssi;
    df_user->slow_ctx->iw_stats.qual.qual =
        _mtlk_df_ui_calc_signal_strength(MTLK_NORMALIZED_RSSI(general_stats->max_rssi));
  }

  df_user->slow_ctx->iw_stats.discard.fragment = general_stats->mac_stat.stat[STAT_FRAGMENT_FAILED];
  df_user->slow_ctx->iw_stats.discard.retries = general_stats->mac_stat.stat[STAT_TX_MAX_RETRY];
  df_user->slow_ctx->iw_stats.discard.code =
      + general_stats->mac_stat.stat[STAT_WEP_UNDECRYPTABLE]
      + general_stats->mac_stat.stat[STAT_WEP_ICV_ERROR]
      + general_stats->mac_stat.stat[STAT_WEP_EXCLUDED]
      + general_stats->mac_stat.stat[STAT_DECRYPTION_FAILED];
  df_user->slow_ctx->iw_stats.discard.misc =
      + general_stats->mac_stat.stat[STAT_RX_DISCARD]
      + general_stats->mac_stat.stat[STAT_TX_FAIL]
      - df_user->slow_ctx->iw_stats.discard.code
      - df_user->slow_ctx->iw_stats.discard.retries;

  return &df_user->slow_ctx->iw_stats;

}

typedef int (*mtlk_df_ui_cfg_handler_f)(mtlk_df_user_t* df_user, uint32 subcmd, char* data, uint16* length);

static int
_mtlk_df_ui_execute_cfg_handler(mtlk_df_ui_cfg_handler_f *cfg_handlers_tbl,
                                mtlk_df_user_t* df_user,
                                uint32 subcmd_id,
                                char* value,
                                uint16* length)
{
  int result = MTLK_ERR_NOT_HANDLED;
  ILOG2_D("IWPRIV #0x%04x being processed", subcmd_id);

  while ((NULL != *cfg_handlers_tbl) && (MTLK_ERR_NOT_HANDLED == result)) {
    result = (*cfg_handlers_tbl++)(df_user, subcmd_id, value, length);
  }

  MTLK_ASSERT(MTLK_ERR_NOT_HANDLED != result);
  ILOG2_D("IWPRIV #0x%04x processing done", subcmd_id);

  return result;
}

static mtlk_df_ui_cfg_handler_f _mtlk_df_ui_cfg_getters_tbl[] =
{
#ifdef MTCFG_IRB_DEBUG
  _mtlk_df_user_irb_pinger_int_get_cfg,
#endif
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  _mtlk_df_user_ppa_directpath_int_get_cfg,
#endif
  _mtlk_df_user_iwpriv_get_core_param,
  NULL
};

static mtlk_df_ui_cfg_handler_f _mtlk_df_ui_cfg_setters_tbl[] =
{
#ifdef MTCFG_IRB_DEBUG
  _mtlk_df_user_irb_pinger_int_set_cfg,
#endif
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  _mtlk_df_user_ppa_directpath_int_set_cfg,
#endif
  _mtlk_df_user_iwpriv_set_core_param,
  NULL,
};


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_int (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra)
{
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  int res = _mtlk_df_ui_execute_cfg_handler(_mtlk_df_ui_cfg_getters_tbl,
                                            df_user, wrqu->mode, extra, NULL);
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_int (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra)
{
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  int res = _mtlk_df_ui_execute_cfg_handler(_mtlk_df_ui_cfg_setters_tbl,
                                            df_user, wrqu->mode, extra + sizeof(uint32), NULL);
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_text (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra)
{
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  int res = _mtlk_df_ui_execute_cfg_handler(_mtlk_df_ui_cfg_getters_tbl,
                                            df_user, wrqu->data.flags, extra, &wrqu->data.length);
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_text (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra)
{
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  int res = _mtlk_df_ui_execute_cfg_handler(_mtlk_df_ui_cfg_setters_tbl,
                                            df_user, wrqu->data.flags, extra, &wrqu->data.length);
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_intvec (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra)
{
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  int res = _mtlk_df_ui_execute_cfg_handler(_mtlk_df_ui_cfg_getters_tbl,
                                            df_user, wrqu->data.flags, extra, &wrqu->data.length);
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_intvec (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra)
{
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  int res = _mtlk_df_ui_execute_cfg_handler(_mtlk_df_ui_cfg_setters_tbl,
                                            df_user, wrqu->data.flags, extra, &wrqu->data.length);
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_addr (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra)
{
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  int res = _mtlk_df_ui_execute_cfg_handler(_mtlk_df_ui_cfg_getters_tbl,
                                            df_user, wrqu->mode, extra, NULL);
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_addr (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra)
{
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  int res = _mtlk_df_ui_execute_cfg_handler(_mtlk_df_ui_cfg_setters_tbl,
                                            df_user, wrqu->data.flags, extra, NULL);
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_addrvec (struct net_device *dev,
                                    struct iw_request_info *info,
                                    union iwreq_data *wrqu,
                                    char *extra)
{
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  int res = _mtlk_df_ui_execute_cfg_handler(_mtlk_df_ui_cfg_getters_tbl,
                                            df_user, wrqu->data.flags, extra, &wrqu->data.length);
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_addrvec (struct net_device *dev,
                                    struct iw_request_info *info,
                                    union iwreq_data *wrqu,
                                    char *extra)
{
  mtlk_df_user_t *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  int res = _mtlk_df_ui_execute_cfg_handler(_mtlk_df_ui_cfg_setters_tbl,
                                            df_user, wrqu->data.flags, extra, &wrqu->data.length);
  return _mtlk_df_mtlk_to_linux_error_code(res);
}

static void
_mtlk_df_user_cleanup(mtlk_df_user_t *df_user)
{
  int i;
  MTLK_CLEANUP_BEGIN(df_user, MTLK_OBJ_PTR(df_user))
      for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(df_user), FW_HANG_EVTs) > 0; i++) {
        MTLK_CLEANUP_STEP_LOOP(df_user, FW_HANG_EVTs, MTLK_OBJ_PTR(df_user),
          mtlk_osal_event_cleanup, (&df_user->fw_hang_evts[i]));
      }

      for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(df_user), PROC_INIT) > 0; i++) 
      {
        MTLK_CLEANUP_STEP_LOOP(df_user, PROC_INIT, MTLK_OBJ_PTR(df_user),
                               _proc_management_handlers[i].on_unregister, (df_user));
      }
#ifdef MTCFG_IRB_DEBUG
    MTLK_CLEANUP_STEP(df_user, IRB_PINGER_INIT, MTLK_OBJ_PTR(df_user),
                      mtlk_irb_pinger_cleanup,
                      (&df_user->slow_ctx->pinger));
#endif /* MTCFG_IRB_DEBUG */

    MTLK_CLEANUP_STEP(df_user, STAT_TIMER, MTLK_OBJ_PTR(df_user),
                      mtlk_osal_timer_cleanup, (&df_user->slow_ctx->stat_timer));
    MTLK_CLEANUP_STEP(df_user, CREATE_DEBUG_DIR, MTLK_OBJ_PTR(df_user),
                      _mtlk_df_ui_delete_debug_dir, (df_user));
    MTLK_CLEANUP_STEP(df_user, CREATE_CARD_DIR, MTLK_OBJ_PTR(df_user),
                      _mtlk_df_ui_delete_card_dir, (df_user));
    MTLK_CLEANUP_STEP(df_user, ALLOC_NAME, MTLK_OBJ_PTR(df_user),
                      MTLK_NOACTION, ());
    MTLK_CLEANUP_STEP(df_user, DEBUG_BCL, MTLK_OBJ_PTR(df_user),
                      mtlk_df_debug_bcl_cleanup, (df_user));
    MTLK_CLEANUP_STEP(df_user, ALLOC_SLOW_CTX, MTLK_OBJ_PTR(df_user),
                      mtlk_osal_mem_free, (df_user->slow_ctx));
  MTLK_CLEANUP_END(df_user, MTLK_OBJ_PTR(df_user));
}

static int
_mtlk_df_user_alloc_devname(mtlk_df_user_t *df_user)
{
  int res;
  char ndev_name_pattern[IFNAMSIZ];

  if (mtlk_df_is_slave(df_user->df)) {
    mtlk_df_t *master_df = mtlk_vap_manager_get_master_df(
                                mtlk_df_get_vap_manager(df_user->df));

    MTLK_ASSERT(master_df != NULL);
    sprintf(ndev_name_pattern, "%s.%d", mtlk_df_user_get_name(mtlk_df_get_user(master_df)), mtlk_vap_get_id(mtlk_df_get_vap_handle(df_user->df))-1);
  }
  else {
    strcpy(ndev_name_pattern, MTLK_NDEV_NAME "%d");
  }

  ILOG0_S("NDEV Name pattern: %s", ndev_name_pattern);

  if (mtlk_df_is_slave(df_user->df)) {
    res = dev_alloc_name(df_user->dev, ndev_name_pattern);
  } else {
    rtnl_lock();
    res = dev_alloc_name(df_user->dev, ndev_name_pattern);
    rtnl_unlock();
  }
  if (res < 0) {
    res = MTLK_ERR_NO_RESOURCES;
  }
  else {
    res = MTLK_ERR_OK;
  }

  return res;
}

static int
_mtlk_df_user_init(mtlk_df_user_t *df_user, 
                   mtlk_df_t *df,
                   struct net_device *dev)
{
  /* NEW APPROACH: Now the Net Device name is allocated on DF UI initialization in assumption
  * that no one else will register the same device name.
  *  - DF infrastructure (proc_fs dirrectories) has been created here.
  *    This approach allows Core to register abilities correctly before Net Device registration.
  */
  int i;

  MTLK_INIT_TRY(df_user, MTLK_OBJ_PTR(df_user))
    df_user->df = df;
    df_user->dev = dev;
    MTLK_INIT_STEP_EX(df_user, ALLOC_SLOW_CTX, MTLK_OBJ_PTR(df_user),
                      mtlk_osal_mem_alloc, (sizeof(*df_user->slow_ctx), MTLK_MEM_TAG_DF_SLOW),
                      df_user->slow_ctx, NULL != df_user->slow_ctx, MTLK_ERR_NO_MEM);

    memset(&df_user->slow_ctx->linux_stats, 0,
           sizeof(df_user->slow_ctx->linux_stats));
    memset(&df_user->slow_ctx->iw_stats, 0,
             sizeof(df_user->slow_ctx->iw_stats));
    memset(&df_user->slow_ctx->core_general_stats, 0,
             sizeof(df_user->slow_ctx->core_general_stats));

    MTLK_INIT_STEP(df_user, DEBUG_BCL, MTLK_OBJ_PTR(df_user), mtlk_df_debug_bcl_init, (df_user));

    MTLK_INIT_STEP(df_user, ALLOC_NAME, MTLK_OBJ_PTR(df_user), _mtlk_df_user_alloc_devname, (df_user));

    MTLK_INIT_STEP(df_user, CREATE_CARD_DIR, MTLK_OBJ_PTR(df_user),
                    _mtlk_df_ui_create_card_dir, (df_user));

    MTLK_INIT_STEP(df_user, CREATE_DEBUG_DIR, MTLK_OBJ_PTR(df_user),
                    _mtlk_df_ui_create_debug_dir, (df_user));

    MTLK_INIT_STEP(df_user, STAT_TIMER, MTLK_OBJ_PTR(df_user),
                    mtlk_osal_timer_init,
                    (&df_user->slow_ctx->stat_timer, _mtlk_df_poll_stats, HANDLE_T(df_user)));

    _mtlk_df_user_fill_callbacks(df_user);


#ifdef MTCFG_IRB_DEBUG
    /* TODO: GS: It's used ROOT IRB ??? Allow it only for MASTER??*/
    MTLK_INIT_STEP(df_user, IRB_PINGER_INIT, MTLK_OBJ_PTR(df_user),
                   mtlk_irb_pinger_init,
                   (&df_user->slow_ctx->pinger, mtlk_irbd_get_root()));
#endif /*MTCFG_IRB_DEBUG*/

    for(i = 0; i < ARRAY_SIZE(_proc_management_handlers); i++) {
   
	MTLK_INIT_STEP_LOOP(df_user, PROC_INIT, MTLK_OBJ_PTR(df_user),
                          _proc_management_handlers[i].on_register, (df_user));
    }

    for (i = 0; i < ARRAY_SIZE(df_user->fw_hang_evts); i++) {
      MTLK_INIT_STEP_LOOP(df_user, FW_HANG_EVTs, MTLK_OBJ_PTR(df_user),
        mtlk_osal_event_init, (&df_user->fw_hang_evts[i]));
    }

  MTLK_INIT_FINALLY(df_user, MTLK_OBJ_PTR(df_user))
  MTLK_INIT_RETURN(df_user, MTLK_OBJ_PTR(df_user), _mtlk_df_user_cleanup, (df_user))
}

mtlk_df_user_t*
mtlk_df_user_create(mtlk_df_t *df)
{
  struct net_device *dev = alloc_etherdev(sizeof(mtlk_df_user_t));
  mtlk_df_user_t* df_user;

  if(NULL == dev)
  {
    ELOG_V("Failed to allocate network device");
    return NULL;
  }

  df_user = netdev_priv(dev);

  if(MTLK_ERR_OK != _mtlk_df_user_init(df_user, df, dev))
  {
    free_netdev(dev);
    return NULL;
  }

  return df_user;
}

void
mtlk_df_user_delete(mtlk_df_user_t* df_user)
{
  _mtlk_df_user_cleanup(df_user);
}

/******************************************************************************************
 * BCL debugging interface implementation
 ******************************************************************************************/
/* TODO: add packing
 * Do not change this structure (synchronized with BCLSockServer) */
typedef struct _BCL_DRV_DATA_EX_REQUEST
{
    uint32         mode;
    uint32         category;
    uint32         index;
    uint32         datalen;
} BCL_DRV_DATA_EX_REQUEST;

/* BCL Driver message modes
 * Do not change these numbers (synchronized with BCLSockServer) */
typedef enum
{
    BclDrvModeCatInit  = 1,
    BclDrvModeCatFree  = 2,
    BclDrvModeNameGet  = 3,
    BclDrvModeValGet   = 4,
    BclDrvModeValSet   = 5
} BCL_DRV_DATA_EX_MODE;

/* BCL Driver message categories for DRV_* commands */
/* Do not change these values (synchronized with BBStudio) */
#define DRVCAT_DBG_API_RESET         1
#define DRVCAT_DBG_API_GENERAL_PKT   2
#define DRVCAT_DBG_API_GENERAL       3
#define DRVCAT_DBG_API_MAC_STATS     4
#define DRVCAT_DBG_API_RR_STATS      5

/* Subcommand indices for DRVCAT_DBG_API_RESET: */
/* Do not change these values (synchronized with BBStudio) */
#define IDX_DBG_API_RESET_ALL  1

/***** DRVCAT_DBG_API_RR_STATS categories definitions */
/**
 * The header string should be returned on get_text with 0 index value.
 **/
const char bcl_rr_counters_hdr[] = "MAC|ID|Too old|Duplicate|Queued|Overflows|Lost";
const int bcl_rr_counters_num_cnts = 5;

/***** DRVCAT_DBG_API_GENERAL_PKT categories definitions */
/**
 * The header string should be returned on get_text with 0 index value.
 * The footer string should be returned on get_text with last index value.
 **/
const char bcl_pkt_counters_hdr[] = "MAC|Packets received|Packets sent";
const int bcl_pkt_counters_num_cnts = 2;
const char bcl_pkt_counters_ftr[] = "Total";

/***** DRVCAT_DBG_API_GENERAL categories definitions */
#define MAX_STAT_NAME_LENGTH 256

#define FETCH_NAME 1
#define FETCH_VAL 2

/* General statistic data processing structures */
struct bcl_general_count_stat_params_t
{
  int num_stats;
};

struct bcl_general_fetch_stat_params_t
{
  int index_search;
  int index_cur;
  int what;
  char name[MAX_STAT_NAME_LENGTH];
  unsigned long val;
};

/***********************************************************
 * BCL function implementation
 ***********************************************************/
static int
mtlk_df_debug_bcl_category_free(mtlk_df_user_t *df_user, uint32 category);

void
mtlk_df_debug_bcl_cleanup(mtlk_df_user_t *df_user)
{
  mtlk_df_debug_bcl_category_free(df_user, DRVCAT_DBG_API_GENERAL);
  mtlk_df_debug_bcl_category_free(df_user, DRVCAT_DBG_API_GENERAL_PKT);
  mtlk_df_debug_bcl_category_free(df_user, DRVCAT_DBG_API_RR_STATS);
  mtlk_df_debug_bcl_category_free(df_user, DRVCAT_DBG_API_MAC_STATS);
}

int
mtlk_df_debug_bcl_init(mtlk_df_user_t *df_user)
{
  df_user->slow_ctx->dbg_rr_addr = NULL;
  df_user->slow_ctx->dbg_rr_addr_num = 0;
  df_user->slow_ctx->dbg_rr_cnts = NULL;
  df_user->slow_ctx->dbg_rr_cnts_num = 0;

  df_user->slow_ctx->dbg_general_pkt_addr = NULL;
  df_user->slow_ctx->dbg_general_pkt_addr_num = 0;
  df_user->slow_ctx->dbg_general_pkt_cnts = NULL;
  df_user->slow_ctx->dbg_general_pkt_cnts_num = 0;
  return MTLK_ERR_OK;
}

static int
mtlk_df_debug_bcl_debug_mac_stats_init(mtlk_df_user_t *df_user, uint32 *pcnt)
{
  int                 res = MTLK_ERR_OK;

  ILOG2_S("%s: Create mac_stats_init", mtlk_df_user_get_name(df_user));

  /* Get MAC statistic from DF buffer and don't call Core */

  *pcnt = mtlk_df_get_stat_info_len();

  return res;
}

static int
mtlk_df_debug_bcl_debug_rr_counters_init(mtlk_df_user_t *df_user, uint32 *pcnt)
{
  int                    res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_clpb_t            *clpb = NULL;
  uint32                 size;
  mtlk_stadb_stat_t      *stadb_stat;
  mtlk_core_ui_get_stadb_status_req_t get_stadb_status_req;
  uint8                  tid;

  uint32                 num_sta_connected = 0;
  uint32                 addr_num_entries_max = 0;
  uint32                 cnt_num_entries_max = 0;
  uint32                 *dbg_rr_cnts = NULL;
  IEEE_ADDR              *dbg_rr_addr = NULL;

  uint32                 addr_cur_entry = 0;
  uint32                 cnts_cur_entry = 0;

  ILOG2_S("%s: Create rr_counters", mtlk_df_user_get_name(df_user));

  ASSERT(df_user->slow_ctx->dbg_rr_addr == NULL);
  ASSERT(df_user->slow_ctx->dbg_rr_cnts == NULL);

  *pcnt = 0;

  get_stadb_status_req.get_hostdb = FALSE;
  get_stadb_status_req.use_cipher = FALSE;
  res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_GET_STADB_STATUS,
                                  &clpb, &get_stadb_status_req, sizeof(get_stadb_status_req));
  res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_STADB_STATUS, FALSE);
  if (MTLK_ERR_OK != res) {
    goto finish;
  }

  num_sta_connected = mtlk_clpb_get_num_of_elements(clpb);
  if (0 == num_sta_connected) {
    goto delete_clpb;
  }

  addr_num_entries_max = num_sta_connected * NTS_TIDS;
  cnt_num_entries_max = num_sta_connected * NTS_TIDS * (bcl_rr_counters_num_cnts + 1/*+TID*/);

  dbg_rr_addr = mtlk_osal_mem_alloc(addr_num_entries_max * sizeof(IEEE_ADDR), MTLK_MEM_TAG_DEBUG_DATA);
  if (NULL == dbg_rr_addr) {
    ELOG_V("Out of memory");
    res = MTLK_ERR_NO_MEM;
    goto delete_clpb;
  }
  memset(dbg_rr_addr, 0, addr_num_entries_max * sizeof(IEEE_ADDR));

  dbg_rr_cnts = mtlk_osal_mem_alloc(cnt_num_entries_max * sizeof(uint32), MTLK_MEM_TAG_DEBUG_DATA);
  if (NULL == dbg_rr_cnts) {
    ELOG_V("Out of memory");
    res = MTLK_ERR_NO_MEM;
    goto delete_dbg_rr_addr;
  }
  memset(dbg_rr_cnts, 0, cnt_num_entries_max * sizeof(uint32));

  addr_cur_entry = 0;
  cnts_cur_entry = 0;

  while(NULL != (stadb_stat = mtlk_clpb_enum_get_next(clpb, &size))) {
    if ((sizeof(*stadb_stat) != size) || (STAT_ID_STADB != stadb_stat->type)) {
      res = MTLK_ERR_UNKNOWN;
      goto delete_dbg_rr_cnts;
    }

    for (tid = 0; tid < ARRAY_SIZE(stadb_stat->u.general_stat.reordering_stats); tid++ )
    {
      if (stadb_stat->u.general_stat.reordering_stats[tid].used) {
        ASSERT(addr_cur_entry < addr_num_entries_max);
        dbg_rr_addr[addr_cur_entry++] = stadb_stat->u.general_stat.addr;

        ASSERT(cnts_cur_entry < cnt_num_entries_max);
        dbg_rr_cnts[cnts_cur_entry++] = tid;

        ASSERT(cnts_cur_entry < cnt_num_entries_max);
        dbg_rr_cnts[cnts_cur_entry++] =
            stadb_stat->u.general_stat.reordering_stats[tid].reord_stat.too_old;

        ASSERT(cnts_cur_entry < cnt_num_entries_max);
        dbg_rr_cnts[cnts_cur_entry++] =
            stadb_stat->u.general_stat.reordering_stats[tid].reord_stat.duplicate;

        ASSERT(cnts_cur_entry < cnt_num_entries_max);
        dbg_rr_cnts[cnts_cur_entry++] =
            stadb_stat->u.general_stat.reordering_stats[tid].reord_stat.queued;

        ASSERT(cnts_cur_entry < cnt_num_entries_max);
        dbg_rr_cnts[cnts_cur_entry++] =
            stadb_stat->u.general_stat.reordering_stats[tid].reord_stat.overflows;

        ASSERT(cnts_cur_entry < cnt_num_entries_max);
        dbg_rr_cnts[cnts_cur_entry++] =
            stadb_stat->u.general_stat.reordering_stats[tid].reord_stat.lost;
      }
    }
  }

  df_user->slow_ctx->dbg_rr_addr = dbg_rr_addr;
  df_user->slow_ctx->dbg_rr_addr_num = addr_cur_entry;
  df_user->slow_ctx->dbg_rr_cnts = dbg_rr_cnts;
  df_user->slow_ctx->dbg_rr_cnts_num = cnts_cur_entry;

  ILOG2_DDDDD("Created "
      "num_sta_connected (%d), "
      "addr_num (%d), cnts_num (%d), "
      "addr_num_entries_max (%d), cnt_num_entries_max (%d)",
      num_sta_connected,
      addr_cur_entry, cnts_cur_entry,
      addr_num_entries_max, cnt_num_entries_max);

  *pcnt = addr_cur_entry;

  if (0 == addr_cur_entry) {
    /* Not TIDs used */
    goto delete_dbg_rr_cnts;
  }

  goto delete_clpb;

delete_dbg_rr_cnts:
  mtlk_osal_mem_free(dbg_rr_cnts);
delete_dbg_rr_addr:
  mtlk_osal_mem_free(dbg_rr_addr);
delete_clpb:
  mtlk_clpb_delete(clpb);
finish:
  return res;
}

static int
mtlk_df_debug_bcl_debug_pkt_counters_init(mtlk_df_user_t *df_user, uint32 *pcnt)
{
  int                    res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_clpb_t            *clpb = NULL;
  uint32                 size;

  uint32                 num_sta_connected;
  uint32                 addr_num_entries_max;
  uint32                 cnt_num_entries_max;
  uint32                 *dbg_general_pkt_cnts = NULL;
  IEEE_ADDR              *dbg_general_pkt_addr = NULL;
  uint32                 addr_cur_entry = 0;
  uint32                 cnts_cur_entry = 0;
  uint32                 total_rx_packets = 0;
  uint32                 total_tx_packets = 0;

  ILOG2_S("%s: Create pkt_counters", mtlk_df_user_get_name(df_user));

  ASSERT(df_user->slow_ctx->dbg_general_pkt_cnts == NULL);
  ASSERT(df_user->slow_ctx->dbg_general_pkt_addr == NULL);

  *pcnt = 0;

  if (mtlk_df_is_ap(df_user->df)) { /*AP*/
    mtlk_stadb_stat_t      *stadb_stat;
    mtlk_core_ui_get_stadb_status_req_t get_stadb_status_req;

    get_stadb_status_req.get_hostdb = FALSE;
    get_stadb_status_req.use_cipher = FALSE;
    res = _mtlk_df_user_invoke_core(df_user->df, MTLK_CORE_REQ_GET_STADB_STATUS,
                                    &clpb, &get_stadb_status_req, sizeof(get_stadb_status_req));
    res = _mtlk_df_user_process_core_retval_void(res, clpb, MTLK_CORE_REQ_GET_STADB_STATUS, FALSE);
    if (MTLK_ERR_OK != res) {
      goto finish;
    }

    num_sta_connected = mtlk_clpb_get_num_of_elements(clpb);
    if (0 == num_sta_connected) {
      goto delete_clpb;
    }

    addr_num_entries_max = num_sta_connected;
    cnt_num_entries_max = (num_sta_connected + 1 /*Total*/) * bcl_pkt_counters_num_cnts;

    dbg_general_pkt_addr = mtlk_osal_mem_alloc(addr_num_entries_max * sizeof(IEEE_ADDR), MTLK_MEM_TAG_DEBUG_DATA);
    if (NULL == dbg_general_pkt_addr) {
      ELOG_V("Out of memory");
      res = MTLK_ERR_NO_MEM;
      goto delete_clpb;
    }
    memset(dbg_general_pkt_addr, 0, addr_num_entries_max * sizeof(IEEE_ADDR));

    dbg_general_pkt_cnts = mtlk_osal_mem_alloc(cnt_num_entries_max * sizeof(uint32), MTLK_MEM_TAG_DEBUG_DATA);
    if (NULL == dbg_general_pkt_cnts) {
      ELOG_V("Out of memory");
      res = MTLK_ERR_NO_MEM;
      goto delete_dbg_general_pkt_addr;
    }
    memset(dbg_general_pkt_cnts, 0, cnt_num_entries_max * sizeof(uint32));

    while(NULL != (stadb_stat = mtlk_clpb_enum_get_next(clpb, &size))) {
      if ((sizeof(*stadb_stat) != size) || (STAT_ID_STADB != stadb_stat->type)) {
        res = MTLK_ERR_UNKNOWN;
        goto delete_dbg_general_pkt_cnts;
      }

      ASSERT(addr_cur_entry < addr_num_entries_max);
      dbg_general_pkt_addr[addr_cur_entry++] = stadb_stat->u.general_stat.addr;

      total_rx_packets += stadb_stat->u.general_stat.sta_rx_packets;
      total_tx_packets += stadb_stat->u.general_stat.sta_tx_packets;

      ASSERT(cnts_cur_entry < cnt_num_entries_max);
      dbg_general_pkt_cnts[cnts_cur_entry++] = stadb_stat->u.general_stat.sta_rx_packets;

      ASSERT(cnts_cur_entry < cnt_num_entries_max);
      dbg_general_pkt_cnts[cnts_cur_entry++] = stadb_stat->u.general_stat.sta_tx_packets;
    }
  } else { /*STA*/
    mtlk_core_general_stats_t *general_stats;

    /* Get Core general information from DF buffer and don't call Core */
    general_stats = &df_user->slow_ctx->core_general_stats;

    if (!mtlk_core_net_state_is_connected(general_stats->net_state)) {
      res = MTLK_ERR_NOT_READY;
      goto finish;
    }

    num_sta_connected = 1;
    addr_num_entries_max = num_sta_connected;
    cnt_num_entries_max = (num_sta_connected + 1 /*Total*/) * bcl_pkt_counters_num_cnts;

    dbg_general_pkt_addr =
        mtlk_osal_mem_alloc(addr_num_entries_max * sizeof(IEEE_ADDR), MTLK_MEM_TAG_DEBUG_DATA);
    if (NULL == dbg_general_pkt_addr) {
      ELOG_V("Out of memory");
      res = MTLK_ERR_NO_MEM;
      goto finish;
    }
    memset(dbg_general_pkt_addr, 0, addr_num_entries_max * sizeof(IEEE_ADDR));

    dbg_general_pkt_cnts =
        mtlk_osal_mem_alloc(cnt_num_entries_max * sizeof(uint32), MTLK_MEM_TAG_DEBUG_DATA);
    if (NULL == dbg_general_pkt_cnts) {
      ELOG_V("Out of memory");
      res = MTLK_ERR_NO_MEM;
      mtlk_osal_mem_free(dbg_general_pkt_addr);
      goto finish;
    }
    memset(dbg_general_pkt_cnts, 0, cnt_num_entries_max * sizeof(uint32));

    memcpy(&dbg_general_pkt_addr[addr_cur_entry++], general_stats->bssid, sizeof(IEEE_ADDR));

    total_rx_packets = general_stats->core_priv_stats.sta_session_rx_packets;
    total_tx_packets = general_stats->core_priv_stats.sta_session_tx_packets;

    dbg_general_pkt_cnts[cnts_cur_entry++] = total_rx_packets;
    dbg_general_pkt_cnts[cnts_cur_entry++] = total_tx_packets;
  }

  ASSERT(cnts_cur_entry < cnt_num_entries_max);
  dbg_general_pkt_cnts[cnts_cur_entry++] = total_rx_packets;

  ASSERT(cnts_cur_entry < cnt_num_entries_max);
  dbg_general_pkt_cnts[cnts_cur_entry++] = total_tx_packets;

  df_user->slow_ctx->dbg_general_pkt_addr = dbg_general_pkt_addr;
  df_user->slow_ctx->dbg_general_pkt_addr_num = addr_cur_entry;
  df_user->slow_ctx->dbg_general_pkt_cnts = dbg_general_pkt_cnts;
  df_user->slow_ctx->dbg_general_pkt_cnts_num = cnts_cur_entry;

  ILOG2_DDDDD("Created "
      "num_sta_connected (%d), "
      "addr_num (%d), cnts_num (%d), "
      "addr_num_entries_max (%d), cnt_num_entries_max (%d)",
      num_sta_connected,
      addr_cur_entry, cnts_cur_entry,
      addr_num_entries_max, cnt_num_entries_max);

  *pcnt = addr_cur_entry + 1 /*Total*/;

  if (0 == addr_cur_entry) {
    /* Not TIDs used */
    goto delete_dbg_general_pkt_cnts;
  }

  goto delete_clpb;

delete_dbg_general_pkt_cnts:
  mtlk_osal_mem_free(dbg_general_pkt_cnts);
delete_dbg_general_pkt_addr:
  mtlk_osal_mem_free(dbg_general_pkt_addr);
delete_clpb:
  mtlk_clpb_delete(clpb);
finish:
  return res;
}

static int mtlk_df_debug_bcl_debug_general_iterate(mtlk_df_user_t *df_user,
    int (* fn)(void *params, unsigned long val, const char* str), void *params)
{
  int i;
  char buf[MAX_STAT_NAME_LENGTH];

  if (MTLK_ERR_OK != fn(params, df_user->slow_ctx->core_general_stats.rx_dat_frames,
        "data frames received"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK != fn(params, df_user->slow_ctx->core_general_stats.rx_ctl_frames,
        "control frames received"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK != fn(params, df_user->slow_ctx->core_general_stats.rx_man_frames,
        "management frames received"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK != fn(params, df_user->slow_ctx->core_general_stats.mac_stat.stat[STAT_TX_FAIL],
        "TX packets dropped"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK != fn(params, df_user->slow_ctx->core_general_stats.core_priv_stats.tx_max_cons_drop,
        "TX maximum consecutive dropped packets"))
    return MTLK_ERR_PARAMS;

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sprintf(buf, "MSDUs received, QoS priority %d", i);
    if (MTLK_ERR_OK != fn(params, df_user->slow_ctx->core_general_stats.core_priv_stats.ac_rx_counter[i], buf))
      return MTLK_ERR_PARAMS;
  }

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sprintf(buf, "MSDUs transmitted, QoS priority %d", i);
    if (MTLK_ERR_OK != fn(params, df_user->slow_ctx->core_general_stats.core_priv_stats.ac_tx_counter[i], buf))
      return MTLK_ERR_PARAMS;
  }

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sprintf(buf, "MSDUs dropped, QoS priority %d", i);
    if (MTLK_ERR_OK != fn(params, df_user->slow_ctx->core_general_stats.core_priv_stats.ac_dropped_counter[i], buf))
      return MTLK_ERR_PARAMS;
  }

  for (i = 0; i < NTS_PRIORITIES; i++) {
    sprintf(buf, "MSDUs used, QoS priority %d", i);
    if (MTLK_ERR_OK != fn(params, df_user->slow_ctx->core_general_stats.core_priv_stats.ac_used_counter[i], buf))
      return MTLK_ERR_PARAMS;
  }

  if (MTLK_ERR_OK != fn(params, df_user->slow_ctx->core_general_stats.tx_msdus_free, "TX MSDUs free"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK != fn(params, df_user->slow_ctx->core_general_stats.tx_msdus_usage_peak, "TX MSDUs usage peak"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK != fn(params, df_user->slow_ctx->core_general_stats.fwd_rx_packets,
        "packets received that should be forwarded to one or more STAs"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK !=fn(params, df_user->slow_ctx->core_general_stats.fwd_rx_bytes,
        "bytes received that should be forwarded to one or more STAs"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK !=fn(params, df_user->slow_ctx->core_general_stats.core_priv_stats.fwd_tx_packets,
        "packets transmitted for forwarded data"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK !=fn(params, df_user->slow_ctx->core_general_stats.core_priv_stats.fwd_tx_bytes,
        "bytes transmitted for forwarded data"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK !=fn(params, df_user->slow_ctx->core_general_stats.core_priv_stats.fwd_dropped,
        "forwarding (transmission) failures"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK !=fn(params, df_user->slow_ctx->core_general_stats.core_priv_stats.rmcast_dropped,
        "reliable multicast (transmission) failures"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK !=fn(params, df_user->slow_ctx->core_general_stats.unicast_replayed_packets+df_user->slow_ctx->core_general_stats.multicast_replayed_packets,
        "packets replayed"))
    return MTLK_ERR_PARAMS;
  if (MTLK_ERR_OK !=fn(params, df_user->slow_ctx->core_general_stats.core_priv_stats.bars_cnt,
        "BAR frames received"))
    return MTLK_ERR_PARAMS;

#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
  {
    mtlk_df_user_ppa_stats_t ppa_stats;

    _mtlk_df_user_ppa_get_stats(df_user, &ppa_stats);

    if (MTLK_ERR_OK !=fn(params, ppa_stats.tx_processed, "TX Frames processed from PPA"))
      return MTLK_ERR_PARAMS;
    if (MTLK_ERR_OK !=fn(params, _mtlk_df_user_ppa_tx_sent_up, "TX Frames sent up from PPA"))
      return MTLK_ERR_PARAMS;
    if (MTLK_ERR_OK !=fn(params, _mtlk_df_user_ppa_tx_dropped, "TX Frames dropped from PPA"))
      return MTLK_ERR_PARAMS;
    if (MTLK_ERR_OK !=fn(params, ppa_stats.rx_accepted, "RX Frames accepted by PPA"))
      return MTLK_ERR_PARAMS;
    if (MTLK_ERR_OK !=fn(params, ppa_stats.rx_rejected, "RX Frames rejected by PPA"))
      return MTLK_ERR_PARAMS;
  }
#endif

  if (MTLK_ERR_OK !=fn(params, df_user->slow_ctx->core_general_stats.bist_check_passed, "BIST check passed"))
    return MTLK_ERR_PARAMS;

  return MTLK_ERR_OK;
}

static int mtlk_df_debug_bcl_debug_general_count_stat(void *params, unsigned long val, const char* str)
{
  struct bcl_general_count_stat_params_t *pcsp = (struct bcl_general_count_stat_params_t *) params;
  ++pcsp->num_stats;
  return MTLK_ERR_OK;
}

static int mtlk_df_debug_bcl_debug_general_fetch_stat(void *params, unsigned long val, const char *str)
{
  struct bcl_general_fetch_stat_params_t *pfsp = (struct bcl_general_fetch_stat_params_t*) params;
  int res = MTLK_ERR_OK;

  if (pfsp->index_cur == pfsp->index_search) {
    if (pfsp->what == FETCH_VAL)
      pfsp->val = val;

    else if (pfsp->what == FETCH_NAME) {
      int rslt = snprintf(pfsp->name, MAX_STAT_NAME_LENGTH, "%s", str);
      if (rslt < 0 || rslt >= MAX_STAT_NAME_LENGTH)
        res = MTLK_ERR_PARAMS;

    } else {
      res = MTLK_ERR_PARAMS;
    }
  }
  ++pfsp->index_cur;
  return res;
}

static int
mtlk_df_debug_bcl_debug_general_init(mtlk_df_user_t *df_user, uint32 *pcnt)
{
  int res = MTLK_ERR_OK;
  struct bcl_general_count_stat_params_t  csp;

  ILOG2_S("%s: Create general", mtlk_df_user_get_name(df_user));

  /* Get Core general information from DF buffer and don't call Core */
  *pcnt = 0;

  csp.num_stats = 0;
  res = mtlk_df_debug_bcl_debug_general_iterate(
          df_user, &mtlk_df_debug_bcl_debug_general_count_stat, &csp);
  if (MTLK_ERR_OK != res) {
    ELOG_V("Error while iterating driver statistics");
    goto finish;
  }

  *pcnt = csp.num_stats;

finish:
  return res;
}

static int
mtlk_df_debug_bcl_category_init(mtlk_df_user_t *df_user, uint32 category, uint32 *cnt)
{
  int res = MTLK_ERR_PARAMS;

  mtlk_df_debug_bcl_category_free(df_user, category);

  switch (category)
  {
  case DRVCAT_DBG_API_GENERAL:
    res = mtlk_df_debug_bcl_debug_general_init(df_user, cnt);
    break;
  case DRVCAT_DBG_API_GENERAL_PKT:
    res = mtlk_df_debug_bcl_debug_pkt_counters_init(df_user, cnt);
    break;
  case DRVCAT_DBG_API_RR_STATS:
    res = mtlk_df_debug_bcl_debug_rr_counters_init(df_user, cnt);
    break;
  case DRVCAT_DBG_API_MAC_STATS:
    res = mtlk_df_debug_bcl_debug_mac_stats_init(df_user, cnt);
    break;
  default:
    ELOG_D("Unsupported data category (%u) requested", category);
    break;
  }
  return res;
}

static int
mtlk_df_debug_bcl_debug_mac_stats_category_free(mtlk_df_user_t *df_user)
{
  ILOG2_S("%s: Free mac_stats_category_free", mtlk_df_user_get_name(df_user));

  return MTLK_ERR_OK;
}

static int
mtlk_df_debug_bcl_debug_rr_counters_category_free(mtlk_df_user_t *df_user)
{
  ILOG2_S("%s: Free rr_counters", mtlk_df_user_get_name(df_user));

  if (NULL != df_user->slow_ctx->dbg_rr_addr) {
    mtlk_osal_mem_free(df_user->slow_ctx->dbg_rr_addr);
    df_user->slow_ctx->dbg_rr_addr = NULL;
  }
  df_user->slow_ctx->dbg_rr_addr_num = 0;

  if (NULL != df_user->slow_ctx->dbg_rr_cnts) {
    mtlk_osal_mem_free(df_user->slow_ctx->dbg_rr_cnts);
    df_user->slow_ctx->dbg_rr_cnts = NULL;
  }
  df_user->slow_ctx->dbg_rr_cnts_num = 0;
  return MTLK_ERR_OK;
}

static int
mtlk_df_debug_bcl_debug_pkt_counters_category_free(mtlk_df_user_t *df_user)
{
  ILOG2_S("%s: Free pkt_counters", mtlk_df_user_get_name(df_user));

  if (NULL != df_user->slow_ctx->dbg_general_pkt_addr) {
    mtlk_osal_mem_free(df_user->slow_ctx->dbg_general_pkt_addr);
    df_user->slow_ctx->dbg_general_pkt_addr = NULL;
  }
  df_user->slow_ctx->dbg_general_pkt_addr_num = 0;

  if (NULL != df_user->slow_ctx->dbg_general_pkt_cnts) {
    mtlk_osal_mem_free(df_user->slow_ctx->dbg_general_pkt_cnts);
    df_user->slow_ctx->dbg_general_pkt_cnts = NULL;
  }
  df_user->slow_ctx->dbg_general_pkt_cnts_num = 0;
  return MTLK_ERR_OK;
}

static int
mtlk_df_debug_bcl_debug_general_category_free(mtlk_df_user_t *df_user)
{
  ILOG2_S("%s: Free core_general_stats", mtlk_df_user_get_name(df_user));

  return MTLK_ERR_OK;
}

int
mtlk_df_debug_bcl_category_free(mtlk_df_user_t *df_user, uint32 category)
{
  int res = MTLK_ERR_PARAMS;

  switch (category)
  {
  case DRVCAT_DBG_API_GENERAL:
    res = mtlk_df_debug_bcl_debug_general_category_free(df_user);
    break;
  case DRVCAT_DBG_API_GENERAL_PKT:
    res = mtlk_df_debug_bcl_debug_pkt_counters_category_free(df_user);
    break;
  case DRVCAT_DBG_API_RR_STATS:
    res = mtlk_df_debug_bcl_debug_rr_counters_category_free(df_user);
    break;
  case DRVCAT_DBG_API_MAC_STATS:
    res = mtlk_df_debug_bcl_debug_mac_stats_category_free(df_user);
    break;
  default:
    ELOG_D("Unsupported data category (%u) requested", category);
    break;
  }
  return res;
}

static int
mtlk_df_debug_bcl_debug_mac_stats_name_get(uint32 index, char *pdata, uint32 datalen)
{
  int rslt;
  if (index >= mtlk_df_get_stat_info_len()) {
    ELOG_D("Index out of bounds (index %u)", index);
    return MTLK_ERR_PARAMS;
  }
  rslt = snprintf(pdata, datalen, "%s", mtlk_df_get_stat_info_name(index));
  if (rslt < 0 || rslt >= datalen) {
    WLOG_DD("Buffer size (%u) too small: string truncated (index %u)", datalen, index);
  }

  return MTLK_ERR_OK;
}

static int
mtlk_df_debug_bcl_debug_rr_counters_name_get(
    mtlk_df_user_t *df_user, uint32 index, char *pdata, uint32 datalen)
{
  int res = MTLK_ERR_OK;
  int rslt = 0;

  if (index >= (df_user->slow_ctx->dbg_rr_addr_num + 1)) { /* +1 for header */
    ELOG_D("Index out of bounds (index %u)", index);
    res = MTLK_ERR_PARAMS;

  } else if (0 == index) {
    rslt = snprintf(pdata, datalen, "%s", bcl_rr_counters_hdr);

  } else if (NULL == df_user->slow_ctx->dbg_rr_addr) {
    res = MTLK_ERR_NOT_READY;

  } else {
    rslt = snprintf(pdata, datalen, MAC_PRINTF_FMT,
        MAC_PRINTF_ARG(df_user->slow_ctx->dbg_rr_addr[index - 1].au8Addr));
  }

  if (rslt < 0 || rslt >= datalen) {
    WLOG_DD("Buffer size (%u) too small: string truncated (index %u)", datalen, index);
  }

  return res;
}

static int
mtlk_df_debug_bcl_debug_pkt_counters_name_get(
    mtlk_df_user_t *df_user, uint32 index, char *pdata, uint32 datalen)
{
  int res = MTLK_ERR_OK;
  int rslt = 0;

  if (index >= (df_user->slow_ctx->dbg_general_pkt_addr_num + 2)) { /* +1 for header +1 footer*/
    ELOG_D("Index out of bounds (index %u)", index);
    res = MTLK_ERR_PARAMS;

  } else if (0 == index) {
    rslt = snprintf(pdata, datalen, "%s", bcl_pkt_counters_hdr);

  } else if ((df_user->slow_ctx->dbg_general_pkt_addr_num + 1) == index) {
    rslt = snprintf(pdata, datalen, "%s", bcl_pkt_counters_ftr);

  } else if (NULL == df_user->slow_ctx->dbg_general_pkt_addr) {
    res = MTLK_ERR_NOT_READY;

  } else {
    rslt = snprintf(pdata, datalen, MAC_PRINTF_FMT,
        MAC_PRINTF_ARG(df_user->slow_ctx->dbg_general_pkt_addr[index - 1].au8Addr));
  }

  if (rslt < 0 || rslt >= datalen) {
    WLOG_DD("Buffer size (%u) too small: string truncated (index %u)", datalen, index);
  }

  return res;
}

static int
mtlk_df_debug_bcl_debug_general_name_get(
    mtlk_df_user_t *df_user, uint32 index, char *pdata, uint32 datalen)
{
  int res = MTLK_ERR_OK;
  int rslt = 0;
  struct bcl_general_fetch_stat_params_t fsp;

  fsp.index_cur = 0;
  fsp.index_search = index;
  fsp.what = FETCH_NAME;

  res = mtlk_df_debug_bcl_debug_general_iterate(df_user, &mtlk_df_debug_bcl_debug_general_fetch_stat, &fsp);
  if (MTLK_ERR_OK != res) {
    ELOG_V("Error while iterating driver statistics");
  } else {
    rslt = snprintf(pdata, datalen, "%s", fsp.name);
    if (rslt < 0 || rslt >= datalen) {
      WLOG_DD("Buffer size (%u) too small: string truncated (index %u)", datalen, index);
    }
  }

  return res;
}

static int
mtlk_df_debug_bcl_name_get(mtlk_df_user_t *df_user, uint32 category,
    uint32 index, char *pdata, uint32 datalen)
{
  int res = MTLK_ERR_PARAMS;

  switch (category)
  {
  case DRVCAT_DBG_API_GENERAL:
    res = mtlk_df_debug_bcl_debug_general_name_get(df_user, index, pdata, datalen);
    break;
  case DRVCAT_DBG_API_GENERAL_PKT:
    res = mtlk_df_debug_bcl_debug_pkt_counters_name_get(df_user, index, pdata, datalen);
    break;
  case DRVCAT_DBG_API_RR_STATS:
    res = mtlk_df_debug_bcl_debug_rr_counters_name_get(df_user, index, pdata, datalen);
    break;
  case DRVCAT_DBG_API_MAC_STATS:
    res = mtlk_df_debug_bcl_debug_mac_stats_name_get(index, pdata, datalen);
    break;
  default:
    ELOG_D("Unsupported data category (%u) requested", category);
    break;
  }
  return res;
}

static int
mtlk_df_debug_bcl_debug_mac_stats_val_get(mtlk_df_user_t *df_user, uint32 index, uint32 *pval)
{
  int res = MTLK_ERR_OK;

  if (index >= mtlk_df_get_stat_info_len()) {
    ELOG_D("Index out of bounds (index %u)", index);
    res = MTLK_ERR_PARAMS;
  } else {
    *pval = df_user->slow_ctx->core_general_stats.mac_stat.stat[mtlk_df_get_stat_info_idx(index)];
  }

  return res;
}

static int
mtlk_df_debug_bcl_debug_rr_counters_val_get(mtlk_df_user_t *df_user, uint32 index, uint32 *pval)
{
  int res = MTLK_ERR_OK;

  if (NULL == df_user->slow_ctx->dbg_rr_cnts) {
    res = MTLK_ERR_NOT_READY;
  } else if (index >= df_user->slow_ctx->dbg_rr_cnts_num) {
    ELOG_D("Index out of bounds (index %u)", index);
    res = MTLK_ERR_PARAMS;
  } else {
    *pval = df_user->slow_ctx->dbg_rr_cnts[index];
  }

  return res;
}

static int
mtlk_df_debug_bcl_debug_pkt_counters_val_get(mtlk_df_user_t *df_user, uint32 index, uint32 *pval)
{
  int res = MTLK_ERR_OK;

  if (NULL == df_user->slow_ctx->dbg_general_pkt_cnts) {
    res = MTLK_ERR_NOT_READY;
  } else if (index >= df_user->slow_ctx->dbg_general_pkt_cnts_num) {
    ELOG_D("Index out of bounds (index %u)", index);
    res = MTLK_ERR_PARAMS;
  } else {
    *pval = df_user->slow_ctx->dbg_general_pkt_cnts[index];
  }

  return res;
}

static int
mtlk_df_debug_bcl_debug_general_val_get(mtlk_df_user_t *df_user, uint32 index, uint32 *pval)
{
  int res = MTLK_ERR_OK;
  struct bcl_general_fetch_stat_params_t fsp;

  fsp.index_cur = 0;
  fsp.index_search = index;
  fsp.what = FETCH_VAL;

  res = mtlk_df_debug_bcl_debug_general_iterate(df_user, &mtlk_df_debug_bcl_debug_general_fetch_stat, &fsp);
  if (MTLK_ERR_OK != res) {
    ELOG_V("Error while iterating driver statistics");
  } else {
    *pval = fsp.val;
  }

  return res;
}

static int
mtlk_df_debug_bcl_val_get(mtlk_df_user_t *df_user, uint32 category, uint32 index, uint32 *pval)
{
  int res = MTLK_ERR_PARAMS;

  switch (category)
  {
  case DRVCAT_DBG_API_GENERAL:
    res = mtlk_df_debug_bcl_debug_general_val_get(df_user, index, pval);
    break;
  case DRVCAT_DBG_API_GENERAL_PKT:
    res = mtlk_df_debug_bcl_debug_pkt_counters_val_get(df_user, index, pval);
    break;
  case DRVCAT_DBG_API_RR_STATS:
    res = mtlk_df_debug_bcl_debug_rr_counters_val_get(df_user, index, pval);
    break;
  case DRVCAT_DBG_API_MAC_STATS:
    res = mtlk_df_debug_bcl_debug_mac_stats_val_get(df_user, index, pval);
    break;
  default:
    ELOG_D("Unsupported data category (%u) requested", category);
    break;
  }
  return res;
}

static int
mtlk_df_debug_bcl_reset(mtlk_df_user_t *df_user, uint32 index, uint32 val)
{
  int res = MTLK_ERR_PARAMS;

  switch (index) {
  case IDX_DBG_API_RESET_ALL:
    res = mtlk_df_ui_reset_stats(df_user->df);
#ifdef CONFIG_IFX_PPA_API_DIRECTPATH
    _mtlk_df_user_ppa_zero_stats(df_user);
#endif
    break;
  default:
    ELOG_D("Index out of bounds (index %u)", index);
  }

  return res;
}

static int
mtlk_df_debug_bcl_val_put(mtlk_df_user_t *df_user, uint32 category, uint32 index, uint32 val)
{
  int res = MTLK_ERR_PARAMS;

  switch (category)
  {
  case DRVCAT_DBG_API_RESET:
    res = mtlk_df_debug_bcl_reset(df_user, index, val);
    break;
  default:
    ELOG_D("Unsupported data category (%u) requested", category);
    break;
  }

  return res;
}

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_bcl_drv_data_exchange (struct net_device *dev,
            struct iw_request_info *info,
            union iwreq_data *wrqu, char *extra)
{
  mtlk_df_user_t            *df_user = (mtlk_df_user_t*)netdev_priv(dev);
  BCL_DRV_DATA_EX_REQUEST   preq;
  char                      *pdata = NULL;
  int                       res = MTLK_ERR_OK;
  uint32                    value;

  ILOG3_SSD("%s: Invoked from %s (%i)", dev->name, current->comm, current->pid);

  if (mtlk_df_is_slave(df_user->df)) { 
    res = MTLK_ERR_NOT_SUPPORTED; 
    goto cleanup; 
  } 

  if (0 != copy_from_user(&preq, wrqu->data.pointer, sizeof(preq))) {
    res = MTLK_ERR_VALUE;
    goto cleanup;
  }

  switch (preq.mode) {
  case BclDrvModeCatInit:
    /* Make sure there's enough space to fit the counter: */
    if (sizeof(uint32) != preq.datalen) {
      res = MTLK_ERR_PARAMS;
    } else {
      /* Return category items counter to BCLSockServer: */
      res = mtlk_df_debug_bcl_category_init(df_user, preq.category, /* out */ &value);
      if (MTLK_ERR_OK == res) {
        if (0 != copy_to_user(wrqu->data.pointer + sizeof(preq), &value, sizeof(uint32))) {
          res = MTLK_ERR_VALUE;
        }
      }
    }
    break;
  case BclDrvModeCatFree:
    res = mtlk_df_debug_bcl_category_free(df_user, preq.category);
    break;
  case BclDrvModeNameGet:
    pdata = mtlk_osal_mem_alloc(preq.datalen * sizeof(char), MTLK_MEM_TAG_IOCTL);
    if (NULL == pdata) {
      res = MTLK_ERR_NO_MEM;
    } else {
      res = mtlk_df_debug_bcl_name_get(df_user, preq.category, preq.index, pdata, preq.datalen);
      if (MTLK_ERR_OK == res) {
        if (0 != copy_to_user(wrqu->data.pointer + sizeof(preq), pdata, strlen(pdata) + 1)) {
          res = MTLK_ERR_VALUE;
        }
      }
      mtlk_osal_mem_free(pdata);
    }
    break;
  case BclDrvModeValGet:
    /* Make sure there's enough space to store the value: */
    if (sizeof(uint32) != preq.datalen) {
      res = MTLK_ERR_PARAMS;
    } else {
      /* Return the value to BCLSockServer: */
      res = mtlk_df_debug_bcl_val_get(df_user, preq.category, preq.index, /* out */ &value);
      if (MTLK_ERR_OK == res) {
        if (0 != copy_to_user(wrqu->data.pointer + sizeof(preq), &value, sizeof(uint32))) {
          res = MTLK_ERR_VALUE;
        }
      }
    }
    break;
  case BclDrvModeValSet:
    /* Make sure the value is present: */
    if (sizeof(uint32) != preq.datalen) {
      res = MTLK_ERR_PARAMS;
    } else {
    /* Process the value: */
      if (0 != copy_from_user(&value, wrqu->data.pointer + sizeof(preq), sizeof(uint32))) {
        res = MTLK_ERR_VALUE;
      } else {
        res = mtlk_df_debug_bcl_val_put(df_user, preq.category, preq.index, value);
      }
    }
    break;
  default:
    ELOG_D("Unknown data exchange mode (%u)", preq.mode);
    res = MTLK_ERR_PARAMS;
  }

cleanup:
  return _mtlk_df_mtlk_to_linux_error_code(res);
}
