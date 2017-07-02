/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * 20/40 coexistence feature
 * Provides transition between modes (20MHz->20/40MHz and vice versa)
 *
 * The local variable evaluator 
 *
 */
#ifndef __COEXLVE_H__
#define __COEXLVE_H__

#ifndef COEX_20_40_C
#error This file can only be included from one of the 20/40 coexistence implementation (.c) files
#endif

#define CLVE_AFFECTED_FREQUENCY_RANGE_HALF      25

/*! 
  \file coexlve.h 
  \brief define the main structure representing the evaluator
*/

struct _mtlk_20_40_coexistence_sm;

typedef struct _mtlk_local_variable_evaluator
{
  struct _mtlk_20_40_coexistence_sm *parent_csm;
  mtlk_20_40_csm_xfaces_t           *xfaces;
  uint32                            op_perm_period_ms;
  uint32                            reg_cls_period_ms;
  uint32                            act_frc_period_ms;
  int                               lvt_op_permitted;
  int                               lvt_reg_class;
  int                               lvt_activity_frac;
}mtlk_local_variable_evaluator;

/* an enum describing various types of local variables */
typedef enum 
{
  LVT_20_40_OPERATION_PERMITTED,
  LVT_40_MHZ_REGULATORY_CLASS,
  LVT_ACTIVITY_FRACTION
}eLV_TYPES;

typedef enum 
{
  PRIMARY_CHANNEL_DATA,
  SECONDARY_CHANNEL_DATA,
  INTOLERANT_CHANNEL_DATA
}_mtlk_channels_data_field_required;

/* The variable evaluator will export the following interfaces: */

int __MTLK_IFUNC mtlk_coex_lve_init (mtlk_local_variable_evaluator *coex_lve,
  struct _mtlk_20_40_coexistence_sm *parent_csm, mtlk_20_40_csm_xfaces_t *xfaces);
/* This function will initialize the variable evaluator object. */

void __MTLK_IFUNC mtlk_coex_lve_cleanup (mtlk_local_variable_evaluator *coex_lve);
/* This function will destroy (cleanup) the variable evaluator object. */

/* instruct the local variable evaluator to periodically evaluate a certain variable. */
void __MTLK_IFUNC mtlk_coex_lve_start_periodic_evaluation (mtlk_local_variable_evaluator *coex_lve,
  eLV_TYPES lv_type, int period_ms);

/* stop the periodic evaluation.*/
void __MTLK_IFUNC mtlk_coex_lve_stop_periodic_evaluation (mtlk_local_variable_evaluator *coex_lve,
  eLV_TYPES lv_type);

/* This function will evaluate the specified variable and return its value. */
int __MTLK_IFUNC mtlk_coex_lve_evaluate (mtlk_local_variable_evaluator *coex_lve, eLV_TYPES lv_type, mtlk_handle_t param_1, mtlk_handle_t param_2);

/* This function will return the current value of the specified variable. */
int __MTLK_IFUNC mtlk_coex_lve_get_variable (mtlk_local_variable_evaluator *coex_lve, eLV_TYPES lv_type);

#endif
