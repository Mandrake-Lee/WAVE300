/*
 * $Id: core_priv.h 11751 2011-10-17 14:36:09Z moshe $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Private Core's definitions
 *
 * Written by: Grygorii Strashko
 *
 */

#ifndef __CORE_PRIV_H_
#define __CORE_PRIV_H_

#include "mtlkdfdefs.h"
#include "mtlk_param_db.h"

#define MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define LOG_LOCAL_GID   GID_CORE
#define LOG_LOCAL_FID   3

/*****************************************************************************************
 * Core definitions
 *****************************************************************************************/
/* TODO: Empiric values, must be replaced somehow */
#define MAC_WATCHDOG_DEFAULT_TIMEOUT_MS 10000
#define MAC_WATCHDOG_DEFAULT_PERIOD_MS 30000

/*****************************************************************************************
 *****************************************************************************************/

/*****************************************************************************************
 * Core abilities private definitions
 *****************************************************************************************/
int
mtlk_core_abilities_register(mtlk_core_t *core);

void
mtlk_core_abilities_unregister(mtlk_core_t *core);
/*****************************************************************************************
 *****************************************************************************************/

/*****************************************************************************************
 * Core ParameterDB private definitions
 *****************************************************************************************/
typedef enum {
  CORE_DB_CORE_BRIDGE_MODE,
  CORE_DB_CORE_AP_FORWARDING,
  CORE_DB_CORE_RELIABLE_MCAST,
  CORE_DB_CORE_MAC_ADDR,
  CORE_DB_CORE_BSSID,
  CORE_DB_LAST_VALUE_ID
} mtlk_core_pdb_handle_id_t;

typedef mtlk_pdb_handle_t mtlk_core_hot_path_param_handles_t[CORE_DB_LAST_VALUE_ID];

int
mtlk_core_pdb_fast_handles_open(mtlk_pdb_t* obj, mtlk_core_hot_path_param_handles_t handles);

void
mtlk_core_pdb_fast_handles_close(mtlk_core_hot_path_param_handles_t handles);


static mtlk_pdb_handle_t __INLINE
mtlk_core_pdb_fast_handle_get(mtlk_core_hot_path_param_handles_t handles, mtlk_core_pdb_handle_id_t core_pdb_id)
{
  MTLK_ASSERT(CORE_DB_LAST_VALUE_ID > core_pdb_id);
  return handles[core_pdb_id];
}

#define MTLK_CORE_HOT_PATH_PDB_GET_INT(core, id) \
		mtlk_pdb_fast_get_int(mtlk_core_pdb_fast_handle_get(core->pdb_hot_path_handles, id))

#define MTLK_CORE_HOT_PATH_PDB_GET_MAC(core, id, mac) \
    mtlk_pdb_fast_get_mac(mtlk_core_pdb_fast_handle_get(core->pdb_hot_path_handles, id), mac)

#define MTLK_CORE_HOT_PATH_PDB_CMP_MAC(core, id, mac) \
		mtlk_pdb_fast_cmp_mac(mtlk_core_pdb_fast_handle_get(core->pdb_hot_path_handles, id), mac)

#define MTLK_CORE_PDB_GET_INT(core, id) \
		mtlk_pdb_get_int(mtlk_vap_get_param_db(core->vap_handle), id)

#define MTLK_CORE_PDB_SET_INT(core, id, value) \
		mtlk_pdb_set_int(mtlk_vap_get_param_db(core->vap_handle), id, value)

#define MTLK_CORE_PDB_GET_BINARY(core, id, buf, size) \
  mtlk_pdb_get_binary(mtlk_vap_get_param_db(core->vap_handle), id, buf, size)
  
#define MTLK_CORE_PDB_SET_BINARY(core, id, value, size) \
  mtlk_pdb_set_binary(mtlk_vap_get_param_db(core->vap_handle), id, value, size)
/*****************************************************************************************
 *****************************************************************************************/

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#define MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __CORE_PRIV_H_ */
