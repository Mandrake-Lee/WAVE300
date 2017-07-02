/* $Id */
#include "mtlkinc.h"

int tx_ovr_and_mask  = 0x000080ff;
int tx_ovr_or_mask   = (1<<15) | (0x0 << 6) | (0x2 << 10) | (0x3<<13);
int lvl_ovr_and_mask = 0x000083ff;
int lvl_ovr_or_mask  = (0x1f << 10) | (1<<15);

#ifdef MTCFG_LINDRV_HW_PCIE
#ifdef MTCFG_PCIE_TUNING

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
MODULE_PARM(tx_ovr_and_mask, "i");
MODULE_PARM(tx_ovr_or_mask, "i");
MODULE_PARM(lvl_ovr_and_mask, "i");
MODULE_PARM(lvl_ovr_or_mask, "i");
#else
module_param(tx_ovr_and_mask, int, 0);
module_param(tx_ovr_or_mask, int, 0);
module_param(lvl_ovr_and_mask, int, 0);
module_param(lvl_ovr_or_mask, int, 0);
#endif

MODULE_PARM_DESC(tx_ovr_and_mask, "de-emphasis: AND mask for TX input override register modification");
MODULE_PARM_DESC(tx_ovr_or_mask,  "de-emphasis: OR mask for TX input override register modification");
MODULE_PARM_DESC(lvl_ovr_and_mask, "de-emphasis: AND mask for level override register modification");
MODULE_PARM_DESC(lvl_ovr_or_mask,  "de-emphasis: OR mask for level override register modification");

#endif //MTCFG_PCIE_TUNING
#endif //MTCFG_LINDRV_HW_PCIE
