#ifndef __MTLK_ASEL_H__
#define __MTLK_ASEL_H__

#include "txmm.h"
#include "stadb.h"
#include "mtlkirbd.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef struct
{
  sta_db       *stadb;
  mtlk_txmm_t  *txmm;
  mtlk_irbd_t  *irbd;
  mtlk_handle_t context;
  uint8    (__MTLK_IDATA *device_is_busy)(mtlk_handle_t context);
} __MTLK_IDATA mtlk_rf_mgmt_cfg_t;

typedef struct _mtlk_rf_mgmt_t mtlk_rf_mgmt_t;

mtlk_rf_mgmt_t * __MTLK_IFUNC mtlk_rf_mgmt_create(void);
int              __MTLK_IFUNC mtlk_rf_mgmt_handle_spr(mtlk_rf_mgmt_t  *rf_mgmt, 
                                                      const IEEE_ADDR *src_addr, 
                                                      uint8           *buffer, 
                                                      uint16           size);
void             __MTLK_IFUNC mtlk_rf_mgmt_delete(mtlk_rf_mgmt_t *rf_mgmt);

void             __MTLK_IFUNC mtlk_rf_mgmt_stop (mtlk_rf_mgmt_t *rf_mgmt);
int              __MTLK_IFUNC mtlk_rf_mgmt_start (mtlk_rf_mgmt_t *rf_mgmt,
                                                  const mtlk_rf_mgmt_cfg_t *cfg);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_ASEL_H__ */
