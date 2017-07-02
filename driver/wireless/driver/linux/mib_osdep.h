/*
 * $Id: mib_osdep.h 12028 2011-11-27 16:17:04Z hatinecs $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Authors: originaly written by Joel Isaacson;
 *  further development and support by: Andriy Tkachuk, Artem Migaev,
 *  Oleksandr Andrushchenko.
 *
 */

#ifndef _MIB_OSDEP_H_
#define _MIB_OSDEP_H_

#include "mhi_umi.h"
#include "mhi_mib_id.h"

#include "core.h"

#define LOG_LOCAL_GID   GID_MIBS
#define LOG_LOCAL_FID   0

void  mtlk_mib_set_nic_cfg (struct nic *nic);

int mtlk_set_mib_values(struct nic *nic);
int mtlk_mib_set_forced_rates (struct nic *nic);
int mtlk_mib_set_power_selection (struct nic *nic);

void mtlk_mib_update_pm_related_mibs (struct nic *nic, mtlk_aux_pm_related_params_t *data);

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif // _MIB_OSDEP_H_
