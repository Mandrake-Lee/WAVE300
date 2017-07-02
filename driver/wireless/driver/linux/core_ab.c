
/*
 * $Id: core_ab.c 12684 2012-02-21 09:45:55Z bejs $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Core's abilities management functionality
 *
 * Written by: Grygorii Strashko
 *
 */

#include "mtlkinc.h"
#include "core_priv.h"
#include "core.h"
#include "mtlk_coreui.h"

#define LOG_LOCAL_GID   GID_CORE
#define LOG_LOCAL_FID   1

/*************************************************************************************************
 * Core abilities
 *************************************************************************************************/
static const mtlk_ability_id_t _core_wme_abilities[] = {
  MTLK_CORE_REQ_GET_WME_BSS_CFG,
  MTLK_CORE_REQ_SET_WME_BSS_CFG
};

static const mtlk_ability_id_t _core_wme_ap_abilities[] = {
  MTLK_CORE_REQ_GET_WME_AP_CFG,
  MTLK_CORE_REQ_SET_WME_AP_CFG
};

static const  mtlk_ability_id_t _core_mbss_master_abilities[] = {
  MTLK_CORE_REQ_MBSS_ADD_VAP,
  MTLK_CORE_REQ_MBSS_DEL_VAP
};

static const  mtlk_ability_id_t _core_mbss_gen_abilities[] = {
  MTLK_CORE_REQ_MBSS_SET_VARS,
  MTLK_CORE_REQ_MBSS_GET_VARS
};

static const mtlk_ability_id_t _core_mac_watchdog_abilities[] = {
  MTLK_CORE_REQ_GET_MAC_WATCHDOG_CFG,
  MTLK_CORE_REQ_SET_MAC_WATCHDOG_CFG
};


static const mtlk_ability_id_t _core_dot11d_abilities[] = {
  MTLK_CORE_REQ_SET_DOT11D_CFG,
  MTLK_CORE_REQ_GET_DOT11D_CFG
};

static const mtlk_ability_id_t _core_general_abilities[] = {
  MTLK_CORE_REQ_ACTIVATE_OPEN,
  MTLK_CORE_REQ_DEACTIVATE,
  MTLK_CORE_REQ_GET_CORE_CFG,
  MTLK_CORE_REQ_SET_CORE_CFG,
  MTLK_CORE_REQ_SET_MAC_ADDR,
  MTLK_CORE_REQ_GET_MAC_ADDR,
  MTLK_CORE_REQ_GET_SERIALIZER_INFO
};

static const mtlk_ability_id_t _core_master_general_abilities[] = {
  /* TODO: merge into single core request? */
  MTLK_CORE_REQ_SET_COUNTRY_CFG,
  MTLK_CORE_REQ_GET_COUNTRY_CFG,
  MTLK_CORE_REQ_SET_MASTER_CFG,
  MTLK_CORE_REQ_GET_MASTER_CFG,
  MTLK_CORE_REQ_CTRL_MAC_GPIO,
  MTLK_CORE_REQ_SET_FW_LED_CFG,
  MTLK_CORE_REQ_GET_FW_LED_CFG
};

static const mtlk_ability_id_t _core_master_ap_general_abilities[] = {
  MTLK_CORE_REQ_GET_MASTER_AP_CFG,
  MTLK_CORE_REQ_SET_MASTER_AP_CFG,
  MTLK_CORE_REQ_GET_AP_CAPABILITIES
};

static const mtlk_ability_id_t _core_ap_general_abilities[] = {
  MTLK_CORE_REQ_AP_DISCONNECT_STA,
  MTLK_CORE_REQ_AP_DISCONNECT_ALL
};


static const mtlk_ability_id_t _core_hw_data_abilities[] = {
  MTLK_CORE_REQ_SET_HW_DATA_CFG,
  MTLK_CORE_REQ_GET_REG_LIMITS
};

static const mtlk_ability_id_t _core_bcl_abilities[] = {
  MTLK_CORE_REQ_SET_MAC_ASSERT,
  MTLK_CORE_REQ_GET_BCL_MAC_DATA,
  MTLK_CORE_REQ_SET_BCL_MAC_DATA
};

static const mtlk_ability_id_t _core_mibs_abilities[] = {
  MTLK_CORE_REQ_SET_MIBS_CFG,
  MTLK_CORE_REQ_GET_MIBS_CFG
};

static const mtlk_ability_id_t _core_status_abilities[] = {
  MTLK_CORE_REQ_GET_STATUS,
  MTLK_CORE_REQ_GEN_DATA_EXCHANGE,
  MTLK_CORE_REQ_GET_MC_IGMP_TBL,
  MTLK_CORE_REQ_RESET_STATS,
  MTLK_CORE_REQ_GET_RANGE_INFO
};

static const  mtlk_ability_id_t _core_security_abilities[] = {
  MTLK_CORE_REQ_SET_WEP_ENC_CFG,
  MTLK_CORE_REQ_GET_WEP_ENC_CFG,
  MTLK_CORE_REQ_SET_GENIE_CFG,
  MTLK_CORE_REQ_SET_AUTH_CFG,
  MTLK_CORE_REQ_GET_AUTH_CFG,
  MTLK_CORE_REQ_GET_ENCEXT_CFG,
  MTLK_CORE_REQ_SET_ENCEXT_CFG
};

static const mtlk_ability_id_t _core_sta_connect_abilities[] = {
  MTLK_CORE_REQ_CONNECT_STA,
  MTLK_CORE_REQ_DISCONNECT_STA
};


/*************************************************************************************************
 *************************************************************************************************/
typedef struct _mtlk_core_ap_tbl_item_t {
  const uint32            *abilities;
  uint32                   num_elems;
} mtlk_ab_tbl_item_t;

static const mtlk_ab_tbl_item_t mtlk_core_abilities_master_ap_init_tbl[]= {
    { _core_dot11d_abilities,             ARRAY_SIZE(_core_dot11d_abilities)},
    { _core_general_abilities,            ARRAY_SIZE(_core_general_abilities)},
    { _core_mibs_abilities,               ARRAY_SIZE(_core_mibs_abilities)},
    { _core_status_abilities,             ARRAY_SIZE(_core_status_abilities)},
    { _core_security_abilities,           ARRAY_SIZE(_core_security_abilities)},

    { _core_mac_watchdog_abilities,       ARRAY_SIZE(_core_mac_watchdog_abilities)},
    { _core_bcl_abilities,                ARRAY_SIZE(_core_bcl_abilities)},
    { _core_hw_data_abilities,            ARRAY_SIZE(_core_hw_data_abilities)},

    { _core_master_general_abilities,     ARRAY_SIZE(_core_master_general_abilities)},
    { _core_master_ap_general_abilities,  ARRAY_SIZE(_core_master_ap_general_abilities)},

    { _core_wme_abilities,                ARRAY_SIZE(_core_wme_abilities)},
    { _core_wme_ap_abilities,             ARRAY_SIZE(_core_wme_ap_abilities)},
    { _core_ap_general_abilities,         ARRAY_SIZE(_core_ap_general_abilities)},

    { _core_mbss_master_abilities,        ARRAY_SIZE(_core_mbss_master_abilities)},
    { _core_mbss_gen_abilities,           ARRAY_SIZE(_core_mbss_gen_abilities)},

    { NULL, 0},
};

static const mtlk_ab_tbl_item_t mtlk_core_abilities_master_sta_init_tbl[]= {
    { _core_dot11d_abilities,             ARRAY_SIZE(_core_dot11d_abilities)},
    { _core_general_abilities,            ARRAY_SIZE(_core_general_abilities)},
    { _core_mibs_abilities,               ARRAY_SIZE(_core_mibs_abilities)},
    { _core_status_abilities,             ARRAY_SIZE(_core_status_abilities)},
    { _core_security_abilities,           ARRAY_SIZE(_core_security_abilities)},

    { _core_mac_watchdog_abilities,       ARRAY_SIZE(_core_mac_watchdog_abilities)},
    { _core_bcl_abilities,                ARRAY_SIZE(_core_bcl_abilities)},
    { _core_hw_data_abilities,            ARRAY_SIZE(_core_hw_data_abilities)},

    { _core_master_general_abilities,     ARRAY_SIZE(_core_master_general_abilities)},
    { _core_sta_connect_abilities,        ARRAY_SIZE(_core_sta_connect_abilities)},
    { NULL, 0},
};

static const mtlk_ab_tbl_item_t mtlk_core_abilities_slave_ap_init_tbl[]= {
    { _core_general_abilities,            ARRAY_SIZE(_core_general_abilities)},
    { _core_mibs_abilities,               ARRAY_SIZE(_core_mibs_abilities)},
    { _core_status_abilities,             ARRAY_SIZE(_core_status_abilities)},
    { _core_security_abilities,           ARRAY_SIZE(_core_security_abilities)},
    { _core_ap_general_abilities,         ARRAY_SIZE(_core_ap_general_abilities)},
    { _core_mbss_gen_abilities,           ARRAY_SIZE(_core_mbss_gen_abilities)},
    { NULL, 0},
};
/*************************************************************************************************
 * Core's abilities management implementation
 *************************************************************************************************/
int
mtlk_core_abilities_register(mtlk_core_t *core)
{
  int res = MTLK_ERR_OK;
  const mtlk_ab_tbl_item_t* ab_tabl;
  mtlk_abmgr_t* abmgr = mtlk_vap_get_abmgr(core->vap_handle);
  const mtlk_ab_tbl_item_t* curr_item;

  if (mtlk_vap_is_master_ap(core->vap_handle)) {
    ab_tabl = mtlk_core_abilities_master_ap_init_tbl;
  } else if (mtlk_vap_is_slave_ap(core->vap_handle)) {
    ab_tabl = mtlk_core_abilities_slave_ap_init_tbl;
  } else {
    MTLK_ASSERT(!mtlk_vap_is_ap(core->vap_handle));
    ab_tabl = mtlk_core_abilities_master_sta_init_tbl;
  }

  for(curr_item = ab_tabl; NULL != curr_item->abilities; ++curr_item) {
    res = mtlk_abmgr_register_ability_set(abmgr, curr_item->abilities, curr_item->num_elems);
    if (MTLK_ERR_OK != res) {
      const mtlk_ab_tbl_item_t* rollback_item;
      for(rollback_item = ab_tabl; rollback_item != curr_item; ++rollback_item) {
        mtlk_abmgr_disable_ability_set(abmgr,  rollback_item->abilities, rollback_item->num_elems);
        mtlk_abmgr_unregister_ability_set(abmgr,  rollback_item->abilities, rollback_item->num_elems);
      }
      break;
    }
    mtlk_abmgr_enable_ability_set(abmgr, curr_item->abilities, curr_item->num_elems);
  }

  return res;
}

void
mtlk_core_abilities_unregister(mtlk_core_t *core)
{
  const mtlk_ab_tbl_item_t* ab_tabl;
  mtlk_abmgr_t* abmgr = mtlk_vap_get_abmgr(core->vap_handle);

  if (mtlk_vap_is_master_ap(core->vap_handle)) {
    ab_tabl = mtlk_core_abilities_master_ap_init_tbl;
  } else if (mtlk_vap_is_slave_ap(core->vap_handle)) {
    ab_tabl = mtlk_core_abilities_slave_ap_init_tbl;
  } else {
    MTLK_ASSERT(!mtlk_vap_is_ap(core->vap_handle));
    ab_tabl = mtlk_core_abilities_master_sta_init_tbl;
  }

  for (; NULL != ab_tabl->abilities; ++ab_tabl) {
    mtlk_abmgr_disable_ability_set(abmgr, ab_tabl->abilities, ab_tabl->num_elems);
    mtlk_abmgr_unregister_ability_set(abmgr, ab_tabl->abilities, ab_tabl->num_elems);
  }
}

void
mtlk_core_abilities_disable_vap_ops (mtlk_vap_handle_t vap_handle)
{
  mtlk_abmgr_disable_ability_set(mtlk_vap_get_abmgr(vap_handle), _core_mbss_master_abilities,
                                                            ARRAY_SIZE(_core_mbss_master_abilities));
}

void
mtlk_core_abilities_enable_vap_ops (mtlk_vap_handle_t vap_handle)
{
  mtlk_abmgr_enable_ability_set(mtlk_vap_get_abmgr(vap_handle), _core_mbss_master_abilities,
                                                            ARRAY_SIZE(_core_mbss_master_abilities));
}
