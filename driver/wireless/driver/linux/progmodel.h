/*
 * $Id: progmodel.h 11242 2011-06-08 10:00:06Z Strashko $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Progmodel
 *
 */
#ifndef __PROGMODEL_H__
#define __PROGMODEL_H__

#include "txmm.h"

struct nic;
typedef struct mtlk_progmodel mtlk_progmodel_t;

mtlk_progmodel_t * __MTLK_IFUNC mtlk_progmodel_create(mtlk_txmm_t *txmm, struct nic *nic, int freq, int spectrum);
int __MTLK_IFUNC mtlk_progmodel_load_from_os(mtlk_progmodel_t *fw);
int __MTLK_IFUNC mtlk_progmodel_load_to_hw(const mtlk_progmodel_t *fw);
void __MTLK_IFUNC mtlk_progmodel_delete(mtlk_progmodel_t *fw);
int mtlk_progmodel_load(mtlk_txmm_t *txmm, struct nic *nic, int freq, int spectrum);
uint8 __MTLK_IFUNC mtlk_progmodel_get_spectrum(mtlk_progmodel_t *fw);
BOOL __MTLK_IFUNC mtlk_progmodel_is_loaded(const mtlk_progmodel_t *fw);

#endif

