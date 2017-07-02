/*
 * $Id: dfs_pdb_def.h 11422 2011-07-19 12:36:42Z andrii $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Core's parameters DB definitions
 *
 * Written by: Andrii Tseglytskyi
 *
 */

#ifndef __MTLK_DFS_PDB_DEF_H_
#define __MTLK_DFS_PDB_DEF_H_

#include "mtlk_param_db.h"

#define MTLK_IDEFS_ON
#include "mtlkidefs.h"

static const uint32  mtlk_dfs_initial_radar_detection =  0;
static const uint32  mtlk_dfs_initial_sm_required =  0;

static const mtlk_pdb_initial_value mtlk_dfs_parameters[] =
{
  /* ID,                              TYPE,                 FLAGS,                        SIZE,                                       POINTER TO CONST */
  {PARAM_DB_DFS_RADAR_DETECTION,      PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_dfs_initial_radar_detection),   &mtlk_dfs_initial_radar_detection},
  {PARAM_DB_DFS_SM_REQUIRED,          PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_dfs_initial_sm_required),       &mtlk_dfs_initial_sm_required},

  {PARAM_DB_LAST_VALUE_ID,            0,                    0,                            0,                               NULL},
};

#define MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_DFS_PDB_DEF_H_ */
