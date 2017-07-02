/*
 * Copyright (c) 2007 Metalink Broadband (Israel)
 *
 * Metalink Driver Helper tool.
 *
 * Originally written by Andrey Fidrya.
 *
 */
#include "mtlkinc.h"

#include "mhi_ieee_address.h"

#include "mtlk_osal.h"
#include "mtlknlink.h"
#include "mtlkirba.h"
#include "mtlk_cli_server.h"
#include "mtlk_cli_port.h"
#include "argv_parser.h"

#include "drvhlpr.h"
#include "driver_api.h"

#include "iniparseraux.h"
#include "mtlkcontainer.h"

#ifdef MTCFG_WPS_LEDS_ENABLE
#include "ledsctrl.h"
#include "wpsctrl.h"
#endif

#include "mhi_umi.h"

#ifdef MTCFG_IRB_DEBUG
#include "irbponger.h"
#endif
#include "arphelper.h"
#ifdef MTCFG_RF_MANAGEMENT_MTLK
#include "asel.h"
#endif

#ifdef MTCFG_ENABLE_OBJPOOL
#include "mtlk_objpool.h"
MTLK_DECLARE_OBJPOOL(g_objpool);
#endif

#include <signal.h>
#include <stdarg.h>

#define LOG_LOCAL_GID   GID_DRVHLPR
#define LOG_LOCAL_FID   1

#define APP_NAME "drvhlpr"

/************************************************************************************
 * CFG-file parameters handling
 ************************************************************************************/
#define PNAME_RF_MGMT_TYPE             "RFMgmtEnable"
#define PNAME_RF_MGMT_REFRESH_TIME     "RFMgmtRefreshTime"
#define PNAME_RF_MGMT_KEEPALIVE_TMOUT  "RFMgmtKeepAliveTimeout"
#define PNAME_RF_MGMT_AVERAGING_APLHA  "RFMgmtAveragingAlpha"
#define PNAME_RF_MGMT_MARGIN_THRESHOLD "RFMgmtMetMarginThreshold"

#define DEFAULT_LED_RESOLUTION      1 /* sec */
#define DEFAULT_SECURITY_MODE       0

mtlk_cli_srv_t          *app_cli_srv        = NULL;
mtlk_irba_t             *mtlk_irba_wlan     = NULL;

static uint32            cfg_mask           = 0;
static char              wlanitrf[IFNAMSIZ] = "";
static const char       *params_conf_file   = NULL;

static pthread_t         signal_thread;
static mtlk_osal_event_t close_evt;
static int               close_status       = 0;

#ifdef MTCFG_ENABLE_OBJPOOL

enum mem_alarm_type_e
{
  MAT_PRINT_INFO,
  MAT_PRINT_ALLOC_INFO_ONCE,
  MAT_PRINT_ALLOC_INFO_CONTINUOUSLY,
  MAT_PRINT_ALLOC_INFO_AND_ASSERT,
  MAT_LAST
};

static uint32            mem_alarm_limit    = 0;
static uint32            mem_alarm_type     = MAT_PRINT_ALLOC_INFO_ONCE;
static volatile BOOL     mem_alarm_fired    = FALSE;
static volatile uint32   mem_alarm_prints   = 0;

#endif

/* 
 * Enum with indexes of driver helper components array.
 * This enum is needed to provide direct reference to
 * components (during configuration loading).
 *
 * Note: after any change in drvhelper_components[] (addition,
 *       reordering etc.) this enum must be also updated to 
 *       provide synchronization
 */
typedef enum {
#ifdef MTCFG_IRB_DEBUG
  MTLK_IRB_COMPONENT_IDX,
#endif

#ifdef MTCFG_WPS_LEDS_ENABLE
  MTLK_LEDS_COMPONENT_IDX,
  MTLK_WPS_COMPONENT_IDX,
#endif

  MTLK_ARP_COMPONENT_IDX,

#ifdef MTCFG_RF_MANAGEMENT_MTLK
  MTLK_RF_MGMT_COMPONENT_IDX,
#endif
} mtlk_drvhelper_components_idx;

/*
 * Components of driver helper
 */
static mtlk_component_t drvhelper_components[] = {
#ifdef MTCFG_IRB_DEBUG
  { 
    .api  = &irb_ponger_component_api, 
    .name = "irb_pong"
  },
#endif

#ifdef MTCFG_WPS_LEDS_ENABLE
  { 
    .api  = NULL, 
    .name = "leds_ctrl"
  },
  { 
    .api  = NULL, 
    .name = "wps_ctrl"
  },
#endif

  { 
    .api  = &irb_arp_helper_api, 
    .name = "arp_hlpr"
  },

#ifdef MTCFG_RF_MANAGEMENT_MTLK
  { 
    .api  = &rf_mgmt_api, 
    .name = "rf mgmt"
  },
#endif
};

#ifdef MTCFG_ENABLE_OBJPOOL
static int __MTLK_IFUNC
_print_mem_alloc_dump (mtlk_handle_t printf_ctx,
                       const char   *format,
                       ...)
{
  int     res;
  va_list valst;
  char    buf[512];

  va_start(valst, format);
  res = vsnprintf(buf, sizeof(buf), format, valst);
  va_end(valst);

  ELOG_S("%s", buf);

  return res;
}

static void
print_mem_alloc_dump (void)
{
  mem_leak_dbg_print_allocators_info(_print_mem_alloc_dump, 
                                     HANDLE_T(0));
}
#endif

/* NOTE: The signal_thread is necessary for the following:
 *      - to make sure there is at least one thread that has 
 *      SIGTERM unmasked. 
 *       Thus, signal handler will run in its context even if 
 *       there are no other threads in the application.
 */
static void *
_signal_thread_proc (void* param)
{
  while (1) {
#ifdef MTCFG_ENABLE_OBJPOOL
    if (mem_alarm_fired) {
      switch (mem_alarm_type)
      {
      case MAT_PRINT_INFO:
        break;
      case MAT_PRINT_ALLOC_INFO_ONCE:
        if (!mem_alarm_prints) {
          ++mem_alarm_prints;
          print_mem_alloc_dump();
        }
        break;
      case MAT_PRINT_ALLOC_INFO_CONTINUOUSLY:
        print_mem_alloc_dump();
        break;
      case MAT_PRINT_ALLOC_INFO_AND_ASSERT:
        print_mem_alloc_dump();
        MTLK_ASSERT(0);
        break;
      case MAT_LAST:
      default:
        WLOG_D("Incorrect Memory Alarm Type: %d", mem_alarm_type);
        break;
      }
    }
    mtlk_osal_msleep(20);
#else
    sleep(50);
#endif
  }

  return NULL;
}

static void
_kill_signal_thread (void)
{
  pthread_kill(signal_thread, 0);
}

static int
_create_signal_thread (void)
{
  int            res = MTLK_ERR_UNKNOWN;
  pthread_attr_t attr;

  /* Initialize attribute */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  /* Run thread */
  if (pthread_create(&signal_thread, 
                     &attr,
                     _signal_thread_proc,
                     NULL) == 0) {
    res = MTLK_ERR_OK;
  }

  /* Free resources */
  pthread_attr_destroy(&attr);

  return res;
}

#ifdef MTCFG_ENABLE_OBJPOOL

static void __MTLK_IFUNC 
memory_alarm_clb (mtlk_handle_t usr_ctx,
                  uint32        allocated)
{
  MTLK_UNREFERENCED_PARAM(usr_ctx);
  MTLK_UNREFERENCED_PARAM(allocated);

  ELOG_DD("Memory alarm limit reached: %d >= %d", allocated, mem_alarm_limit);

  mem_alarm_fired = TRUE;
}

static void
install_memory_alarm (BOOL set)
{
  if (set) {
    mtlk_objpool_memory_alarm_t memory_alarm_info;

    memory_alarm_info.limit   = mem_alarm_limit;
    memory_alarm_info.clb     = memory_alarm_clb;
    memory_alarm_info.usr_ctx = HANDLE_T(0);

    mtlk_objpool_set_memory_alarm(&g_objpool, &memory_alarm_info);
  }
  else {
    mtlk_objpool_set_memory_alarm(&g_objpool, NULL);
  }
}
#endif

static void
initiate_app_shutdown (int status)
{
  close_status = status;
  mtlk_osal_event_set(&close_evt);
}

static int 
load_cfg (const char *cfg_fname)
{
  int         res  = MTLK_ERR_UNKNOWN;
  dictionary *dict = NULL;
  char       *tmp = NULL;
  char       *net_type = NULL;
  int        sec_mode = 0;

#ifdef MTCFG_WPS_LEDS_ENABLE
  char *ptr = NULL;
  char buf[128];
  int  tmp_val = 0;
#endif

  MTLK_UNREFERENCED_PARAM(net_type);
  MTLK_UNREFERENCED_PARAM(sec_mode);

  MTLK_ASSERT(cfg_fname != NULL);
  MTLK_ASSERT(strlen(cfg_fname) != 0);

  dict = iniparser_load(cfg_fname);
  if (!dict) {
    ELOG_S("Can't load CFG file (%s)", cfg_fname);
    res = MTLK_ERR_PARAMS;
    goto end;
  }

  res = iniparser_aux_fillstr(dict,
                              iniparser_aux_gkey_const("interface"),
                              wlanitrf,
                              sizeof(wlanitrf));
  if (res != MTLK_ERR_OK) {
    ELOG_D("'interface' parameter reading error: %d", res);
    goto end;
  }

  net_type = iniparser_getstr(dict, iniparser_aux_gkey_const("network_type"));
  if (!net_type) {
    ELOG_V("'network_type' parameter reading error");
    goto end;
  }

  res = iniparser_getint(dict, 
                         iniparser_aux_gkey_const("Debug_SoftwareWatchdogEnable"), 
                         1);
  if (res == 0) {
    cfg_mask |= DHFLAG_NO_DRV_HUNG_HANDLING;
  }

  sec_mode = iniparser_getint(dict, 
                         iniparser_aux_gkey_const("NonProcSecurityMode"), 
                         DEFAULT_SECURITY_MODE);

  tmp = iniparser_getstr(dict, iniparser_aux_gkey_const("arp_iface0"));
  if (!tmp) {
    ELOG_V("Can't find the 'arp_iface0' parameter");
    goto end;
  }

  res = arp_helper_register_iface(tmp);
  if (res != MTLK_ERR_OK) {
    ELOG_SD("Can't register ARP xFace#0 (%s), err=%d'", tmp, res);
    res = MTLK_ERR_PARAMS;
    goto end;
  }

  tmp = iniparser_getstr(dict, iniparser_aux_gkey_const("arp_iface1"));
  if (tmp) {
    res = arp_helper_register_iface(tmp);
    if (res != MTLK_ERR_OK) {
      ELOG_SD("Can't register ARP xFace#1 (%s), err=%d'", tmp, res);
      res = MTLK_ERR_PARAMS;
      goto end;
    }
  }

#ifdef MTCFG_RF_MANAGEMENT_MTLK
  rf_mgmt_cfg.rf_mgmt_type = 
    (uint32)iniparser_getint(dict, 
                             iniparser_aux_gkey_const(PNAME_RF_MGMT_TYPE), 
                             (int)rf_mgmt_cfg.rf_mgmt_type);

  rf_mgmt_cfg.refresh_time_ms = 
    (uint32)iniparser_getint(dict, 
                             iniparser_aux_gkey_const(PNAME_RF_MGMT_REFRESH_TIME),
                             (int)rf_mgmt_cfg.refresh_time_ms);

  rf_mgmt_cfg.keep_alive_tmout_sec = 
    (uint32)iniparser_getint(dict, 
                             iniparser_aux_gkey_const(PNAME_RF_MGMT_KEEPALIVE_TMOUT),
                             (int)rf_mgmt_cfg.keep_alive_tmout_sec);

  rf_mgmt_cfg.averaging_alpha = 
    (uint32)iniparser_getint(dict, 
                             iniparser_aux_gkey_const(PNAME_RF_MGMT_AVERAGING_APLHA),
                             (int)rf_mgmt_cfg.averaging_alpha);

  rf_mgmt_cfg.margin_threshold = 
    (uint32)iniparser_getint(dict, 
                             iniparser_aux_gkey_const(PNAME_RF_MGMT_MARGIN_THRESHOLD),
                             (int)rf_mgmt_cfg.margin_threshold);
#endif

#ifdef MTCFG_WPS_LEDS_ENABLE
  tmp = iniparser_getstr(dict, iniparser_aux_gkey_const("wps_script_path"));
  if (!tmp) {
    ILOG0_V("'wps_script_path' parameter isn't present");
    goto end;
  }

  /* initialize components for following addition to drvhlpr components list */
  drvhelper_components[MTLK_LEDS_COMPONENT_IDX].api = &irb_leds_ctrl_api;
  drvhelper_components[MTLK_WPS_COMPONENT_IDX].api  = &irb_wps_ctrl_api;

  res = wps_ctrl_set_wps_script_path(tmp);
  if (res != MTLK_ERR_OK) {
    ELOG_D("'wps_script_path' parameter set error #%d", res);
    goto end;
  }

  leds_ctrl_set_mask(cfg_mask);

  res = leds_ctrl_set_network_type(net_type);
  if (res != MTLK_ERR_OK) {
    ELOG_D("'network_type' parameter set error #%d", res);
    goto end;
  }

  tmp = iniparser_getstr(dict, iniparser_aux_gkey_const("wls_link_script_path"));
  if (!tmp) {
    ELOG_V("'wls_link_script_path' parameter reading error");
    goto end;
  }

  res = leds_ctrl_set_link_path(tmp);
  if (res != MTLK_ERR_OK) {
    ELOG_D("'wls_link_script_path' parameter set error #%d", res);
    goto end;
  }

  tmp_val = iniparser_getint(dict, 
                         iniparser_aux_gkey_const("led_resolution"), 
                         DEFAULT_LED_RESOLUTION);
  leds_ctrl_set_leds_resolution(tmp_val);
  wps_ctrl_set_leds_resolution(tmp_val);

  tmp = iniparser_getstr(dict, iniparser_aux_gkey_const("wls_link_status_script_path"));
  if (!tmp) {
    ELOG_V("'wls_link_status_script_path' parameter reading error");
    goto end;
  }

  res = leds_ctrl_set_link_status_path(tmp);
  if (res != MTLK_ERR_OK) {
    ELOG_D("'wls_link_script_path' parameter set (LEDS) error #%d", res);
    goto end;
  }

  res = wps_ctrl_set_link_status_path(tmp);
  if (res != MTLK_ERR_OK) {
    ELOG_D("'wls_link_script_path' parameter set (WPS) error #%d", res);
    goto end;
  }

  leds_ctrl_set_security_mode(sec_mode);

  leds_ctrl_set_conf_file(params_conf_file);

  /*---wlan leds------------------------------------------------------------*/
  /*iniparser_getstr(dict, iniparser_aux_gkey(ptr,strlen(ptr))*/
  memset(&buf,0,sizeof(buf));
  while ((ptr = leds_ctrl_get_string()) != NULL) {
    ILOG2_S("string = %s",ptr);
    iniparser_aux_gkey(ptr,buf,sizeof(buf));
    ILOG2_S("buf = %s",buf);
    tmp = iniparser_getstr(dict, buf);
    if (!tmp) {
      ELOG_S("Can't find the %s parameter",buf);
      goto end;
    }
    leds_ctrl_set_param(ptr, tmp);
  }

  while ((ptr = leds_ctrl_get_bin_string()) != NULL) {
    iniparser_aux_gkey(ptr,buf,sizeof(buf));
   ILOG2_S("buf = %s",buf);
    tmp_val = iniparser_getint(dict, 
                           buf, 
                           sec_mode);
    leds_ctrl_set_bin_param(ptr, tmp_val);
  }
#endif

end:
  if (dict) {
    iniparser_freedict(dict);
  }

  return res;
}
/*************************************************************************************/


typedef struct _network_interface
{
    char name[IFNAMSIZ];
} network_interface;


#define DRV_RETRY_CONNECTION_SEC 1

static const mtlk_guid_t IRBE_RMMOD = MTLK_IRB_GUID_RMMOD;
static const mtlk_guid_t IRBE_HANG  = MTLK_IRB_GUID_HANG;

static mtlk_guid_t MAIN_IRBE_LIST[] = 
{
  MTLK_IRB_GUID_RMMOD,
  MTLK_IRB_GUID_HANG
};


/************************************************/

static void
sigterm_handler (int sig)
{
  initiate_app_shutdown(EVENT_SIGTERM);
}

static void
_uninstall_sigactions (void)
{
  int              res;
  struct sigaction action;

  sigemptyset(&action.sa_mask);

  action.sa_handler = SIG_IGN;
  action.sa_flags   = 0;

  res = sigaction(SIGTERM, &action, NULL);
  if (res != 0) {
    ELOG_D("return error from sigaction: %d", errno);
    MTLK_ASSERT(0);
  }
}

static int
_install_sigactions (void)
{
  int              res;
  struct sigaction action;

  sigemptyset(&action.sa_mask);
  sigaddset(&action.sa_mask, SIGTERM); /* prevent handler re-entrance */
  
  action.sa_handler = sigterm_handler;
  action.sa_flags   = 0;

  /* Block the desired signals for main() thread which waits on close_evt
   * to allow signal handler to signal it with no deadlock.
   */
  res = pthread_sigmask(SIG_BLOCK, &action.sa_mask, NULL);
  if (res != 0) {
    ELOG_D("return error from pthread_sigmask: %d", errno);
    res = MTLK_ERR_PARAMS;
    goto end;
  }

  res = sigaction(SIGTERM, &action, NULL);
  if (res != 0) {
    ELOG_D("return error from sigaction: %d", errno);
    res = MTLK_ERR_NO_RESOURCES;
    goto end;
  }
   
  res = MTLK_ERR_OK;

end:
  /* NOTE: no rollback action is required for pthread_sigmask() error,
   *       the application is going to exit if an error is returned
   *       from this function anyway.
   */
  return res;
}

static const struct mtlk_argv_param_info_ex param_cfg_file = {
  {
    "p",
    NULL,
    MTLK_ARGV_PINFO_FLAG_HAS_STR_DATA
  },
  "configuration file",
  MTLK_ARGV_PTYPE_MANDATORY
};

static const struct mtlk_argv_param_info_ex param_dlevel = {
  {
    "d",
    "debug-level",
    MTLK_ARGV_PINFO_FLAG_HAS_INT_DATA
  },
  "debug level (DEBUG verion only)",
  MTLK_ARGV_PTYPE_OPTIONAL
};

static const struct mtlk_argv_param_info_ex param_step_to_fail = {
  {
    NULL,
    "step-to-fail",
    MTLK_ARGV_PINFO_FLAG_HAS_INT_DATA
  },
  "step to simulate failure (start up macros, DEBUG version only)",
  MTLK_ARGV_PTYPE_OPTIONAL
};

static const struct mtlk_argv_param_info_ex param_stderr_err = {
  {
    NULL,
    "stderr-err",
    MTLK_ARGV_PINFO_FLAG_HAS_NO_DATA
  },
  "duplicate ELOG printouts to stderr",
  MTLK_ARGV_PTYPE_OPTIONAL
};

static const struct mtlk_argv_param_info_ex param_stderr_warn = {
  {
    NULL,
    "stderr-warn",
    MTLK_ARGV_PINFO_FLAG_HAS_NO_DATA
  },
  "duplicate ELOG and WLOG printouts to stderr",
  MTLK_ARGV_PTYPE_OPTIONAL
};

static const struct mtlk_argv_param_info_ex param_stderr_all = {
  {
    "e",
    "stderr-all",
    MTLK_ARGV_PINFO_FLAG_HAS_NO_DATA
  },
  "duplicate all (ELOG, WLOG, LOG) printouts to stderr",
  MTLK_ARGV_PTYPE_OPTIONAL
};

static const struct mtlk_argv_param_info_ex param_mem_alarm_limit = {
  {
    NULL,
    "mem-alarm-limit",
    MTLK_ARGV_PINFO_FLAG_HAS_INT_DATA
  },
  "alarm when allocated memory size becomes more then this limit",
  MTLK_ARGV_PTYPE_OPTIONAL
};

static const struct mtlk_argv_param_info_ex param_mem_alarm_type = {
  {
    NULL,
    "mem-alarm-type",
    MTLK_ARGV_PINFO_FLAG_HAS_INT_DATA
  },
  "how to alarm when allocated memory size becomes more then this limit:\n"
  "           0 - print message\n"
  "           1 - print allocations once\n"
  "           2 - print allocations continuously\n"
  "           3 - print allocations once and assert",
  MTLK_ARGV_PTYPE_OPTIONAL
};

static const struct mtlk_argv_param_info_ex param_help =  {
  {
    "h",
    "help",
    MTLK_ARGV_PINFO_FLAG_HAS_NO_DATA
  },
  "print this help",
  MTLK_ARGV_PTYPE_OPTIONAL
};

static void
_print_help (const char *app_name)
{
  const struct mtlk_argv_param_info_ex *all_params[] = {
    &param_cfg_file,
    &param_dlevel,
    &param_stderr_err,
    &param_stderr_warn,
    &param_stderr_all,
#ifdef MTCFG_ENABLE_OBJPOOL
    &param_mem_alarm_limit,
    &param_mem_alarm_type,
#endif
    &param_help
  };
  const char *app_fname = strrchr(app_name, '/');

  if (!app_fname) {
    app_fname = app_name;
  }
  else {
    ++app_fname; /* skip '/' */
  }

  mtlk_argv_print_help(stdout,
                       app_fname,
                       "Metalink Driver Helper v." MTLK_SOURCE_VERSION,
                       all_params,
                       (uint32)ARRAY_SIZE(all_params));
}


static int
process_commandline (int argc, char *argv[])
{
  int                res    = MTLK_ERR_UNKNOWN;
  BOOL               inited = FALSE;
  mtlk_argv_parser_t argv_parser;
  mtlk_argv_param_t *param  = NULL;

  if (argc < 2) {
    res  = MTLK_ERR_PARAMS;
    goto end;
  }

  res = mtlk_argv_parser_init(&argv_parser, argc, argv);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Can't init argv parser (err=%d)", res);
    goto end;
  }
  inited = TRUE;

  param = mtlk_argv_parser_param_get(&argv_parser, &param_help.info);
  if (param) {
    mtlk_argv_parser_param_release(param);
    res = MTLK_ERR_UNKNOWN;
    goto end;
  }

  param = mtlk_argv_parser_param_get(&argv_parser, &param_cfg_file.info);
  if (param) {
    params_conf_file = mtlk_argv_parser_param_get_str_val(param); /* We can use this pointer after release
                                                                   * since it actually points to argv.
                                                                   */
    mtlk_argv_parser_param_release(param);
  }

  if (!params_conf_file) {
    ELOG_V("CFG file must be specified!");
    res = MTLK_ERR_PROHIB;
    goto end;  
  }

#ifdef MTLK_DEBUG
  param = mtlk_argv_parser_param_get(&argv_parser, &param_step_to_fail.info);
  if (param) {
    uint32 v = mtlk_argv_parser_param_get_uint_val(param, (uint32)-1);
    mtlk_argv_parser_param_release(param);
    if (v != (uint32)-1) {
      mtlk_startup_set_step_to_fail(v);
    }
    else {
      ELOG_V("Invalid step-to-fail!");
      res = MTLK_ERR_VALUE;
      goto end;
    }
  }
#endif

  param = mtlk_argv_parser_param_get(&argv_parser, &param_dlevel.info);
  if (param) {
    uint32 v = mtlk_argv_parser_param_get_uint_val(param, (uint32)-1);
    mtlk_argv_parser_param_release(param);
    if (v != (uint32)-1) {
      debug = v;
    }
    else {
      ELOG_V("Invalid debug-level!");
      res = MTLK_ERR_VALUE;
      goto end;
    }
  }

  param = mtlk_argv_parser_param_get(&argv_parser, &param_stderr_err.info);
  if (param) {
    mtlk_argv_parser_param_release(param);
    _mtlk_osdep_log_enable_stderr(MTLK_OSLOG_ERR, TRUE);
  }

  param = mtlk_argv_parser_param_get(&argv_parser, &param_stderr_warn.info);
  if (param) {
    mtlk_argv_parser_param_release(param);
    _mtlk_osdep_log_enable_stderr(MTLK_OSLOG_ERR, TRUE);
    _mtlk_osdep_log_enable_stderr(MTLK_OSLOG_WARN, TRUE);
  }

  param = mtlk_argv_parser_param_get(&argv_parser, &param_stderr_all.info);
  if (param) {
    mtlk_argv_parser_param_release(param);
    _mtlk_osdep_log_enable_stderr(MTLK_OSLOG_ERR, TRUE);
    _mtlk_osdep_log_enable_stderr(MTLK_OSLOG_WARN, TRUE);
    _mtlk_osdep_log_enable_stderr(MTLK_OSLOG_INFO, TRUE);
  }

#ifdef MTCFG_ENABLE_OBJPOOL
  param = mtlk_argv_parser_param_get(&argv_parser, &param_mem_alarm_limit.info);
  if (param) {
    mem_alarm_limit = mtlk_argv_parser_param_get_uint_val(param, mem_alarm_limit);
    mtlk_argv_parser_param_release(param);
  }

  param = mtlk_argv_parser_param_get(&argv_parser, &param_mem_alarm_type.info);
  if (param) {
    mem_alarm_type = mtlk_argv_parser_param_get_uint_val(param, mem_alarm_type);
    mtlk_argv_parser_param_release(param);
    if (mem_alarm_type >= MAT_LAST) {
      ELOG_V("Invalid mem_alarm_type!");
      res = MTLK_ERR_VALUE;
      goto end;
    }
  }
#endif

  res = MTLK_ERR_OK;

end:
  if (inited) {
    mtlk_argv_parser_cleanup(&argv_parser);
  }

  if (res != MTLK_ERR_OK) {
    _print_help(argv[0]);
  }

  return res;
}

#ifdef MTCFG_ENABLE_OBJPOOL
static int __MTLK_IFUNC
_cli_mem_printf (mtlk_handle_t printf_ctx,
                 const char   *format,
                 ...)
{
  int                 res;
  va_list             valst;
  char                buf[512];
  mtlk_cli_srv_rsp_t *rsp = 
    HANDLE_T_PTR(mtlk_cli_srv_rsp_t, printf_ctx);

  va_start(valst, format);
  res = vsnprintf(buf, sizeof(buf), format, valst);
  va_end(valst);

  mtlk_cli_srv_rsp_add_str(rsp, buf);

  return res;
}

static void __MTLK_IFUNC
main_cli_mem_alloc_dump_handler (mtlk_cli_srv_t           *srv,
                                 const mtlk_cli_srv_cmd_t *cmd,
                                 mtlk_cli_srv_rsp_t       *rsp,
                                 mtlk_handle_t             ctx)
{
  mem_leak_dbg_print_allocators_info(_cli_mem_printf, HANDLE_T(rsp));
}
#endif /* MTCFG_ENABLE_OBJPOOL */

static void __MTLK_IFUNC
main_cli_exit_handler (mtlk_cli_srv_t           *srv,
                       const mtlk_cli_srv_cmd_t *cmd,
                       mtlk_cli_srv_rsp_t       *rsp,
                       mtlk_handle_t             ctx)
{
  mtlk_cli_srv_rsp_add_str(rsp, "Exiting...");
  initiate_app_shutdown(EVENT_CLI_EXIT);
}

static void __MTLK_IFUNC
main_cli_set_dlevel_handler (mtlk_cli_srv_t           *srv,
                             const mtlk_cli_srv_cmd_t *cmd,
                             mtlk_cli_srv_rsp_t       *rsp,
                             mtlk_handle_t             ctx)
{
  int   res = MTLK_ERR_UNKNOWN;
  int32 tmp;

  if (mtlk_cli_srv_cmd_get_nof_params(cmd) != 1) {
    res = MTLK_ERR_PARAMS;
    goto end;
  }

  tmp = mtlk_cli_srv_cmd_get_param_int(cmd, 0, &res);
  if (res != MTLK_ERR_OK) {
    goto end;
  }

  debug = (int)tmp;
  res   = MTLK_ERR_OK;

end:
  if (res != MTLK_ERR_OK) {
    mtlk_cli_srv_rsp_add_str(rsp, "1 numeric parameter is required: <new_dlevel>");
    mtlk_cli_srv_rsp_set_error(rsp, res);
  }
}

static void __MTLK_IFUNC 
main_irb_handler (mtlk_irba_t       *irba,
                  mtlk_handle_t      context,
                  const mtlk_guid_t *evt,
                  void              *buffer,
                  uint32            size)
{
  if (mtlk_guid_compare(evt, &IRBE_HANG) == 0) {
    ILOG1_V("Got HANG event\n");
    if (!(cfg_mask & DHFLAG_NO_DRV_HUNG_HANDLING)){
      initiate_app_shutdown(EVENT_REQ_RESET);
    }
  }
  else if (mtlk_guid_compare(evt, &IRBE_RMMOD) == 0) {
    ILOG1_V("Got RMMOD event\n");
    if (!(cfg_mask & DHFLAG_NO_DRV_RMMOD_HANDLING)){
      initiate_app_shutdown(EVENT_REQ_RMMOD);
    }
  }
  else {
    char guid_str[MTLK_GUID_STR_SIZE];
    mtlk_guid_to_string(evt, guid_str, sizeof(guid_str));
    ELOG_S("Unexpected GUID: %s", guid_str);
  }
}

struct drvhlpr_cli_cmd
{
  const char            *name;
  mtlk_cli_srv_cmd_clb_f clb;
};

static const struct drvhlpr_cli_cmd drvhlpr_cli_cmds[] = {
  {
    .name = "sExit",
    .clb  = main_cli_exit_handler,
  },
  {
    .name = "sDLevel",
    .clb  = main_cli_set_dlevel_handler,
  },
#ifdef MTCFG_ENABLE_OBJPOOL
  {
    .name = "gMemAllocDump",
    .clb  = main_cli_mem_alloc_dump_handler,
  },
#endif
};

typedef struct _drvhlpr_main_t
{
  mtlk_container_t    container;
  mtlk_irba_handle_t *main_irb_client;
  mtlk_cli_srv_t     *cli_srv;
  mtlk_cli_srv_clb_t *clb_handle[ARRAY_SIZE(drvhlpr_cli_cmds)];
  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_INIT_LOOP(CLI_SRV_REG_CMDS);
} drvhlpr_main_t;

MTLK_INIT_STEPS_LIST_BEGIN(drvhlpr_main)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, OSDEP_LOG_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, OSAL_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, CLOSE_EVT_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, START_SIGNALS_THREAD)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, INSTALL_SIGACTIONS)
#ifdef MTCFG_ENABLE_OBJPOOL
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, INSTALL_MEMORY_ALARM)
#endif
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, CONNECT_TO_DRV)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, IRBA_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, IRBA_START)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, IRBA_WLAN_ALLOC)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, IRBA_WLAN_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, CLI_SRV_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, CLI_SRV_START)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, CLI_SRV_PORT_EXPORT)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, CLI_SRV_REG_CMDS)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, IRB_REG)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, CONTAINER_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, COMPONENTS_REG)
MTLK_INIT_INNER_STEPS_BEGIN(drvhlpr_main)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, PARSE_CMD_LINE)
  MTLK_INIT_STEPS_LIST_ENTRY(drvhlpr_main, PARSE_CFG_FILE)
MTLK_INIT_STEPS_LIST_END(drvhlpr_main);

static void
_disconnect_from_drv (void)
{
  if (driver_connected()) {
    driver_close_connection();
  }
}

static int
_connect_to_drv (void)
{
  int res = MTLK_ERR_OK;

  while (!driver_connected()) {
    if (0 != driver_setup_connection(wlanitrf)) {
      int wres = mtlk_osal_event_wait(&close_evt, DRV_RETRY_CONNECTION_SEC * 1000);
      if (wres == MTLK_ERR_OK) {
        res = MTLK_ERR_PROHIB;
        goto end;
      }
    }
  }

  ILOG2_S("Connected to driver, interface name = %s", wlanitrf);

end:
  return res;
}

static int
_drvhlpr_register_components (drvhlpr_main_t *_drvhlpr)
{
  int res = MTLK_ERR_OK;
  int i;

  for (i = 0; i < ARRAY_SIZE(drvhelper_components); ++i) {
    /* add only valid components */
    if (NULL == drvhelper_components[i].api) {
      continue;
    }
    res = mtlk_container_register(&_drvhlpr->container, &drvhelper_components[i]);
    if (res != MTLK_ERR_OK) {
      break;
    }
    ILOG0_S("Component added: %s", drvhelper_components[i].name);
  }

  return res;
}

static void __MTLK_IFUNC
irba_rm_handler (mtlk_irba_t   *irba,
                 mtlk_handle_t  context)
{
  WLOG_V("IRBA is going to disappear...");
}


static void
_drvhlpr_main_cleanup (drvhlpr_main_t *_drvhlpr)
{
  int tmp;

  MTLK_CLEANUP_BEGIN(drvhlpr_main, MTLK_OBJ_PTR(_drvhlpr))
    MTLK_CLEANUP_STEP(drvhlpr_main, COMPONENTS_REG, MTLK_OBJ_PTR(_drvhlpr),
                      MTLK_NOACTION, ());
    MTLK_CLEANUP_STEP(drvhlpr_main, CONTAINER_INIT, MTLK_OBJ_PTR(_drvhlpr),
                      mtlk_container_cleanup, (&_drvhlpr->container));
    MTLK_CLEANUP_STEP(drvhlpr_main, IRB_REG, MTLK_OBJ_PTR(_drvhlpr),
                      mtlk_irba_unregister, (MTLK_IRBA_ROOT, HANDLE_T_PTR(mtlk_irba_handle_t, _drvhlpr->main_irb_client)));
    for (tmp = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(_drvhlpr), CLI_SRV_REG_CMDS) > 0; tmp++) {
      MTLK_ASSERT(NULL != _drvhlpr->clb_handle[tmp]);
      MTLK_CLEANUP_STEP_LOOP(drvhlpr_main, CLI_SRV_REG_CMDS, MTLK_OBJ_PTR(_drvhlpr),
                             mtlk_cli_srv_unregister_cmd_clb, (_drvhlpr->clb_handle[tmp]));
    }
    app_cli_srv = NULL;
    MTLK_CLEANUP_STEP(drvhlpr_main, CLI_SRV_PORT_EXPORT, MTLK_OBJ_PTR(_drvhlpr),
                      mtlk_cli_port_export_cleanup, ());
    MTLK_CLEANUP_STEP(drvhlpr_main, CLI_SRV_START, MTLK_OBJ_PTR(_drvhlpr),
                      mtlk_cli_srv_stop, (_drvhlpr->cli_srv));
    MTLK_CLEANUP_STEP(drvhlpr_main, CLI_SRV_CREATE, MTLK_OBJ_PTR(_drvhlpr),
                      mtlk_cli_srv_delete, (_drvhlpr->cli_srv));

    rf_mgmt_cfg.irba = NULL;

    MTLK_CLEANUP_STEP(drvhlpr_main, IRBA_WLAN_INIT, MTLK_OBJ_PTR(_drvhlpr), 
                      mtlk_irba_cleanup, (mtlk_irba_wlan));
    MTLK_CLEANUP_STEP(drvhlpr_main, IRBA_WLAN_ALLOC, MTLK_OBJ_PTR(_drvhlpr), 
                      mtlk_irba_free, (mtlk_irba_wlan));
    MTLK_CLEANUP_STEP(drvhlpr_main, IRBA_START, MTLK_OBJ_PTR(_drvhlpr),
                      mtlk_irba_app_stop, ());
    MTLK_CLEANUP_STEP(drvhlpr_main, IRBA_INIT, MTLK_OBJ_PTR(_drvhlpr),
                      mtlk_irba_app_cleanup, ());
    MTLK_CLEANUP_STEP(drvhlpr_main, CONNECT_TO_DRV, MTLK_OBJ_PTR(_drvhlpr),
                      _disconnect_from_drv, ());
#ifdef MTCFG_ENABLE_OBJPOOL
    MTLK_CLEANUP_STEP(drvhlpr_main, INSTALL_MEMORY_ALARM, MTLK_OBJ_PTR(_drvhlpr),
                      install_memory_alarm, (FALSE));
#endif
    MTLK_CLEANUP_STEP(drvhlpr_main, INSTALL_SIGACTIONS, MTLK_OBJ_PTR(_drvhlpr),
                      _uninstall_sigactions, ());
    MTLK_CLEANUP_STEP(drvhlpr_main, START_SIGNALS_THREAD, MTLK_OBJ_PTR(_drvhlpr),
                      _kill_signal_thread, ());
    MTLK_CLEANUP_STEP(drvhlpr_main, CLOSE_EVT_INIT, MTLK_OBJ_PTR(_drvhlpr),
                      mtlk_osal_event_cleanup, (&close_evt));
    MTLK_CLEANUP_STEP(drvhlpr_main, OSAL_INIT, MTLK_OBJ_PTR(_drvhlpr),
                      mtlk_osal_cleanup, ());
    MTLK_CLEANUP_STEP(drvhlpr_main, OSDEP_LOG_INIT, MTLK_OBJ_PTR(_drvhlpr),
                      _mtlk_osdep_log_cleanup, ());
  MTLK_CLEANUP_END(drvhlpr_main, MTLK_OBJ_PTR(_drvhlpr))
}

static int
_drvhlpr_main_init (drvhlpr_main_t *_drvhlpr, int argc, char *argv[])
{
  int  tmp;
  char cli_srv_name[sizeof(APP_NAME) + 1 /*_*/ + IFNAMSIZ + 1 /* 0*/];

  MTLK_ASSERT(ARRAY_SIZE(_drvhlpr->clb_handle) == ARRAY_SIZE(drvhlpr_cli_cmds));

  MTLK_INIT_TRY(drvhlpr_main, MTLK_OBJ_PTR(_drvhlpr))
    MTLK_INIT_STEP(drvhlpr_main, OSDEP_LOG_INIT, MTLK_OBJ_PTR(_drvhlpr),
                   _mtlk_osdep_log_init, (APP_NAME));
    MTLK_INIT_STEP(drvhlpr_main, OSAL_INIT, MTLK_OBJ_PTR(_drvhlpr),
                   mtlk_osal_init, ());
    MTLK_INIT_STEP(drvhlpr_main, CLOSE_EVT_INIT, MTLK_OBJ_PTR(_drvhlpr),
                   mtlk_osal_event_init, (&close_evt));
    MTLK_INIT_STEP(drvhlpr_main, START_SIGNALS_THREAD, MTLK_OBJ_PTR(_drvhlpr),
                   _create_signal_thread, ());
    MTLK_INIT_STEP(drvhlpr_main, INSTALL_SIGACTIONS, MTLK_OBJ_PTR(_drvhlpr),
                   _install_sigactions, ());
    MTLK_INIT_STEP(drvhlpr_main, PARSE_CMD_LINE, MTLK_OBJ_NONE,
                   process_commandline, (argc, argv));
#ifdef MTCFG_ENABLE_OBJPOOL
    MTLK_INIT_STEP_VOID(drvhlpr_main, INSTALL_MEMORY_ALARM, MTLK_OBJ_PTR(_drvhlpr),
                        install_memory_alarm, (TRUE));
#endif
    MTLK_INIT_STEP(drvhlpr_main, PARSE_CFG_FILE, MTLK_OBJ_NONE,
                   load_cfg, (params_conf_file));
    MTLK_INIT_STEP(drvhlpr_main, CONNECT_TO_DRV, MTLK_OBJ_PTR(_drvhlpr),
                   _connect_to_drv, ());
    MTLK_INIT_STEP(drvhlpr_main, IRBA_INIT, MTLK_OBJ_PTR(_drvhlpr),
                   mtlk_irba_app_init, (irba_rm_handler, HANDLE_T(0)));
    MTLK_INIT_STEP(drvhlpr_main, IRBA_START, MTLK_OBJ_PTR(_drvhlpr),
                   mtlk_irba_app_start, ());
    MTLK_INIT_STEP_EX(drvhlpr_main, IRBA_WLAN_ALLOC,  MTLK_OBJ_PTR(_drvhlpr), 
                      mtlk_irba_alloc, (),
                      mtlk_irba_wlan,
                      mtlk_irba_wlan != NULL,
                      MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP(drvhlpr_main, IRBA_WLAN_INIT, MTLK_OBJ_PTR(_drvhlpr),
                   mtlk_irba_init, (mtlk_irba_wlan, wlanitrf, irba_rm_handler, HANDLE_T(0)));

    rf_mgmt_cfg.irba = mtlk_irba_wlan;

    MTLK_INIT_STEP_EX(drvhlpr_main, CLI_SRV_CREATE, MTLK_OBJ_PTR(_drvhlpr),
                      mtlk_cli_srv_create, (),
                      _drvhlpr->cli_srv,
                      _drvhlpr->cli_srv != NULL,
                      MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP(drvhlpr_main, CLI_SRV_START, MTLK_OBJ_PTR(_drvhlpr),
                   mtlk_cli_srv_start, (_drvhlpr->cli_srv, 0));

    tmp = (int)mtlk_cli_srv_get_bound_port(_drvhlpr->cli_srv);
    snprintf(cli_srv_name, sizeof(cli_srv_name), "%s_%s", APP_NAME, wlanitrf);
    ILOG0_SD("MTLK CLI Server (%s) bound on port %d", cli_srv_name, tmp);

    MTLK_INIT_STEP(drvhlpr_main, CLI_SRV_PORT_EXPORT, MTLK_OBJ_PTR(_drvhlpr),
                   mtlk_cli_port_export_init, ((uint16)tmp, cli_srv_name));

    app_cli_srv = _drvhlpr->cli_srv;

    for (tmp = 0; tmp < ARRAY_SIZE(_drvhlpr->clb_handle); tmp++) {
      MTLK_INIT_STEP_LOOP_EX(drvhlpr_main, CLI_SRV_REG_CMDS, MTLK_OBJ_PTR(_drvhlpr),
                             mtlk_cli_srv_register_cmd_clb, (_drvhlpr->cli_srv,
                                                             drvhlpr_cli_cmds[tmp].name,
                                                             drvhlpr_cli_cmds[tmp].clb,
                                                             HANDLE_T(0)),
                             _drvhlpr->clb_handle[tmp],
                             _drvhlpr->clb_handle[tmp] != NULL,
                             MTLK_ERR_NO_MEM);
    }
    MTLK_INIT_STEP_EX(drvhlpr_main, IRB_REG, MTLK_OBJ_PTR(_drvhlpr),
                      mtlk_irba_register, (MTLK_IRBA_ROOT,
                                           MAIN_IRBE_LIST, 
                                           ARRAY_SIZE(MAIN_IRBE_LIST), 
                                           main_irb_handler, 
                                           0),
                      _drvhlpr->main_irb_client,
                      _drvhlpr->main_irb_client != NULL,
                      MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP(drvhlpr_main, CONTAINER_INIT, MTLK_OBJ_PTR(_drvhlpr),
                   mtlk_container_init, (&_drvhlpr->container));
    
    MTLK_INIT_STEP(drvhlpr_main, COMPONENTS_REG, MTLK_OBJ_PTR(_drvhlpr),
                   _drvhlpr_register_components, (_drvhlpr));
  MTLK_INIT_FINALLY(drvhlpr_main, MTLK_OBJ_PTR(_drvhlpr))
    MTLK_CLEANUP_STEP(drvhlpr_main, PARSE_CFG_FILE, MTLK_OBJ_NONE,
                      MTLK_NOACTION, ());
    MTLK_CLEANUP_STEP(drvhlpr_main, PARSE_CMD_LINE, MTLK_OBJ_NONE,
                      MTLK_NOACTION, ());
  MTLK_INIT_RETURN(drvhlpr_main, MTLK_OBJ_PTR(_drvhlpr), _drvhlpr_main_cleanup, (_drvhlpr))
}

int
main (int argc, char *argv[])
{
  int            retval = 0;
  drvhlpr_main_t drvhlpr;

  memset(&drvhlpr, 0, sizeof(drvhlpr));

  retval = _drvhlpr_main_init(&drvhlpr, argc, argv);
  if (retval != MTLK_ERR_OK) {
    retval = EVENT_INT_ERR;
    goto end;
  }

  mtlk_container_run(&drvhlpr.container, &close_evt);
  retval = close_status;

  _drvhlpr_main_cleanup(&drvhlpr);

end:
  return retval;
}

