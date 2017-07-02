/*
* $Id: $
*
* Copyright (c) 2006-2007 Metalink Broadband (Israel)
*
* Helper functions for GPL code implementation
*
*/

#include "mtlkinc.h"
#include "mhi_umi_propr.h"

uint32 __MTLK_IFUNC mtlk_get_umi_man_size(void)
{
  return sizeof(UMI_MAN);
}

uint32 __MTLK_IFUNC mtlk_get_umi_dbg_size(void)
{
  return sizeof(UMI_DBG);
}

uint32 __MTLK_IFUNC mtlk_get_umi_activate_size(void)
{
  return sizeof(UMI_ACTIVATE);
}

uint32 __MTLK_IFUNC mtlk_get_umi_mbss_pre_activate_size(void)
{
  return sizeof(UMI_MBSS_PRE_ACTIVATE);
}

uint32 __MTLK_IFUNC mtlk_get_umi_scan_size(void)
{
  return sizeof(UMI_SCAN);
}

UMI_ACTIVATE_HDR* __MTLK_IFUNC mtlk_get_umi_activate_hdr(void *data)
{
  return &((UMI_ACTIVATE*)data)->sActivate;
}

UMI_SCAN_HDR* __MTLK_IFUNC mtlk_get_umi_scan_hdr(void *data)
{
  return &((UMI_SCAN*)data)->sScan;
}

