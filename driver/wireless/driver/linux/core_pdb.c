
/*
 * $Id: core_pdb.c 11579 2011-08-29 14:31:05Z nayshtut $
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

#define LOG_LOCAL_GID   GID_CORE
#define LOG_LOCAL_FID   2

static const mtlk_pdb_id_t mtlk_core_hot_path_parameters[CORE_DB_LAST_VALUE_ID + 1] =
{
    PARAM_DB_CORE_BRIDGE_MODE,
    PARAM_DB_CORE_AP_FORWARDING,
    PARAM_DB_CORE_RELIABLE_MCAST,
    PARAM_DB_CORE_MAC_ADDR,
    PARAM_DB_CORE_BSSID,
    PARAM_DB_LAST_VALUE_ID
};

int
mtlk_core_pdb_fast_handles_open(mtlk_pdb_t* obj, mtlk_core_hot_path_param_handles_t handles)
{
  int i = 0;
  ILOG0_V("Open Hot-path parameters");

  while (PARAM_DB_LAST_VALUE_ID != mtlk_core_hot_path_parameters[i])
  {
    MTLK_ASSERT(CORE_DB_LAST_VALUE_ID > i);
    handles[i] = mtlk_pdb_open(obj, mtlk_core_hot_path_parameters[i]);
    i++;
  }

  return MTLK_ERR_OK;
}

void
mtlk_core_pdb_fast_handles_close(mtlk_core_hot_path_param_handles_t handles)
{
  int i = 0;
  ILOG0_V("Close Hot-path parameters");

  while (CORE_DB_LAST_VALUE_ID > i)
  {
     mtlk_pdb_close(handles[i]);
     i++;
  }
}

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID
