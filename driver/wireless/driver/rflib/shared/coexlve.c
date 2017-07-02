/*
 * $Id: coexlve.c 11794 2011-10-23 12:54:42Z kashani $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * 20/40 coexistence feature
 * Provides transition between modes (20MHz->20/40MHz and vice versa)
 *
 * The local variable evaluator 
 *
 */
#include "mtlkinc.h"

#define COEX_20_40_C

// This define is necessary for coex20_40priv.h file to compile successfully
#include "coex20_40priv.h"
#include "channels.h"

#define LOG_LOCAL_GID   GID_COEX
#define LOG_LOCAL_FID   1

static int _mtlk_coex_lve_calc_affected_freq_range_min (int f_primary, int f_secondary);
static int _mtlk_coex_lve_calc_affected_freq_range_max (int f_primary, int f_secondary);
static BOOL _mtlk_coex_lve_is_in_affected_freq_range (uint16 primary_channel, uint16 secondary_channel, uint16 tested_channel);
static BOOL _mtlk_coex_lve_get_required_field( struct _mtlk_coex_intolerant_channels_db *channel_data, int required_field);
static mtlk_osal_timestamp_t _mtlk_coex_lve_get_required_ts_field(struct _mtlk_coex_intolerant_channels_db *channel_data, int required_ts_field);
static BOOL _mtlk_coex_verify_channels_overlapping(uint16 primary_channel, uint16 secondary_channel, uint16 channel, _mtlk_channels_data_field_required required_field, struct _mtlk_coex_intolerant_db *intolerant_channels_data, uint32 transition_timer_time);
static int _mtlk_coex_lve_check_channel_bounds(uint16 channel);
static BOOL _mtlk_coex_lve_is_operation_permitted(mtlk_local_variable_evaluator *coex_lve, struct _mtlk_coex_intolerant_db *idb, uint16 primary_channel, uint16 secondary_channel);


/* Initialization & cleanup */

int __MTLK_IFUNC mtlk_coex_lve_init (mtlk_local_variable_evaluator *coex_lve,
  mtlk_20_40_coexistence_sm_t *parent_csm, mtlk_20_40_csm_xfaces_t *xfaces)
{
  MTLK_ASSERT(coex_lve != NULL);
  MTLK_ASSERT(parent_csm != NULL);

  coex_lve->parent_csm = parent_csm;
  coex_lve->xfaces = xfaces;
  coex_lve->op_perm_period_ms = 0;
  coex_lve->reg_cls_period_ms = 0;
  coex_lve->act_frc_period_ms = 0;

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC mtlk_coex_lve_cleanup (mtlk_local_variable_evaluator *coex_lve)
{
  MTLK_ASSERT(coex_lve != NULL);
  MTLK_ASSERT(coex_lve->parent_csm != NULL);

  coex_lve->op_perm_period_ms = 0;
  coex_lve->reg_cls_period_ms = 0;
  coex_lve->act_frc_period_ms = 0;
      
  coex_lve->parent_csm = NULL;
}


/* External functional interfaces (meant only for the parent coexistence state machine) */

void __MTLK_IFUNC mtlk_coex_lve_start_periodic_evaluation (mtlk_local_variable_evaluator *coex_lve,
  eLV_TYPES lv_type, int period_ms)
{
  MTLK_ASSERT(coex_lve != NULL);

  switch (lv_type)
  {
    case (LVT_20_40_OPERATION_PERMITTED):coex_lve->op_perm_period_ms = period_ms;
        break;
    case (LVT_40_MHZ_REGULATORY_CLASS):coex_lve->reg_cls_period_ms = period_ms;
        break;
    case (LVT_ACTIVITY_FRACTION):coex_lve->act_frc_period_ms = period_ms;
        break;
    default:
        ELOG_V("Bad ENUM value");
        break;
  }
}

void __MTLK_IFUNC mtlk_coex_lve_stop_periodic_evaluation (mtlk_local_variable_evaluator *coex_lve,
  eLV_TYPES lv_type)
{
  MTLK_ASSERT(coex_lve != NULL);

  switch (lv_type)
  {
    case (LVT_20_40_OPERATION_PERMITTED):coex_lve->op_perm_period_ms = 0;
      break;
    case (LVT_40_MHZ_REGULATORY_CLASS):coex_lve->reg_cls_period_ms = 0;
      break;
    case (LVT_ACTIVITY_FRACTION):coex_lve->act_frc_period_ms = 0;
      break;
    default:
      ELOG_V("Bad ENUM value");
      break;
  }
}

int __MTLK_IFUNC mtlk_coex_lve_evaluate( mtlk_local_variable_evaluator *coex_lve, eLV_TYPES lv_type, mtlk_handle_t param_1, mtlk_handle_t param_2 )
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(coex_lve != NULL);

  switch (lv_type)
  {
    case (LVT_20_40_OPERATION_PERMITTED):
      coex_lve->lvt_op_permitted = (int)_mtlk_coex_lve_is_operation_permitted(coex_lve, &coex_lve->parent_csm->intolerance_db, (uint16)param_1, (uint16)param_2);
      res = coex_lve->lvt_op_permitted;
  	  break;
    case (LVT_40_MHZ_REGULATORY_CLASS):
      res = coex_lve->lvt_reg_class;
      break;
    case (LVT_ACTIVITY_FRACTION):
      res = coex_lve->lvt_activity_frac;
      break;
    default:
      res = MTLK_ERR_VALUE;
      break;
  }

  return res;
}

int __MTLK_IFUNC mtlk_coex_lve_get_variable (mtlk_local_variable_evaluator *coex_lve, eLV_TYPES lv_type)
{
  int res = MTLK_ERR_VALUE;

  MTLK_ASSERT(coex_lve != NULL);

  switch (lv_type)
  {
    case (LVT_20_40_OPERATION_PERMITTED):
      res = coex_lve->lvt_op_permitted;
      break;
    case (LVT_40_MHZ_REGULATORY_CLASS):
      res = coex_lve->lvt_reg_class;
      break;
    case (LVT_ACTIVITY_FRACTION):
      res = coex_lve->lvt_activity_frac;
      break;
    default:
      res = MTLK_ERR_VALUE;
      break;
  }

  return res;
}


/* Internal functions */
static int _mtlk_coex_lve_calc_affected_freq_range_min (int f_primary, int f_secondary)
{
  /* the numbers of the formula are taken from the standard, how to calculate affected frequency range (10-2) */
  return (((f_primary + f_secondary) / 2) - CLVE_AFFECTED_FREQUENCY_RANGE_HALF);
}

static int _mtlk_coex_lve_calc_affected_freq_range_max (int f_primary, int f_secondary)
{
  /* the numbers of the formula are taken from the standard, how to calculate affected frequency range (10-2) */
  return (((f_primary + f_secondary) / 2) + CLVE_AFFECTED_FREQUENCY_RANGE_HALF);
}

static BOOL _mtlk_coex_lve_is_in_affected_freq_range (uint16 primary_channel, uint16 secondary_channel, 
  uint16 tested_channel)
{ 
  BOOL res = FALSE;
  int f_primary, f_secondary, f_tested;

  f_primary = channel_to_frequency(primary_channel);
  f_secondary = channel_to_frequency(secondary_channel);
  f_tested = channel_to_frequency(tested_channel);

  if (f_tested >= _mtlk_coex_lve_calc_affected_freq_range_min(f_primary, f_secondary) &&
      f_tested <= _mtlk_coex_lve_calc_affected_freq_range_max(f_primary, f_secondary))
  {
    res = TRUE;
  }

  return res;
}

static BOOL _mtlk_coex_lve_get_required_field( struct _mtlk_coex_intolerant_channels_db *channel_data, int required_field)
{
  BOOL res = MTLK_ERR_VALUE;

  MTLK_ASSERT(required_field >= PRIMARY_CHANNEL_DATA && required_field <= INTOLERANT_CHANNEL_DATA);

  switch (required_field)
  {
    case PRIMARY_CHANNEL_DATA:
      res = channel_data->primary;
      break;
    case SECONDARY_CHANNEL_DATA:
      res = channel_data->secondary;
      break;
    case INTOLERANT_CHANNEL_DATA:
      res = channel_data->intolerant;
      break;
    default:
      break;
  }
  return res;
}

static mtlk_osal_timestamp_t _mtlk_coex_lve_get_required_ts_field( struct _mtlk_coex_intolerant_channels_db *channel_data, int required_ts_field)
{
  mtlk_osal_timestamp_t res = 0;

  MTLK_ASSERT(required_ts_field >= PRIMARY_CHANNEL_DATA && required_ts_field <= INTOLERANT_CHANNEL_DATA);

  switch (required_ts_field)
  {
    case PRIMARY_CHANNEL_DATA:
      res = channel_data->primary_detection_ts;
      break;
    case SECONDARY_CHANNEL_DATA:
      res = channel_data->secodnary_detection_ts;
      break;
    case INTOLERANT_CHANNEL_DATA:
      res = channel_data->intolerant_detection_ts;
      break;
    default:
      break;
  }

  return res;
}

/* function assumes lock is acquired/cleaned-up outside the function */
static BOOL _mtlk_coex_verify_channels_overlapping(uint16 primary_channel, uint16 secondary_channel,  
  uint16 checked_channel, _mtlk_channels_data_field_required required_field, 
  struct _mtlk_coex_intolerant_db *intolerant_channels_data, uint32 transition_timer_time)
{
  BOOL res = TRUE;
  BOOL is_in_affected_range;
  BOOL checked_field;
  BOOL ts_updated;
  int i = 0;
  int current_channel_number = 1;

  /* check all channels in list or until contradiction is found */
  while((i < CE2040_NUMBER_OF_CHANNELS_IN_2G4_BAND) && (res))
  {
    /* i is from 0..13, while current_channel_number is from 1..14 */
    is_in_affected_range = _mtlk_coex_lve_is_in_affected_freq_range(primary_channel, secondary_channel, current_channel_number);
    checked_field = _mtlk_coex_lve_get_required_field(&intolerant_channels_data->channels_list[i], required_field);
    if(transition_timer_time > mtlk_osal_time_passed_ms(_mtlk_coex_lve_get_required_ts_field(&intolerant_channels_data->channels_list[i], required_field)))
      ts_updated = TRUE;
    else
      ts_updated = FALSE;
    if ((is_in_affected_range) && (checked_field) && (ts_updated))
    {
      res = FALSE;
    }
    ++i;
    ++current_channel_number;
  }

  return res;
}

static int _mtlk_coex_lve_check_channel_bounds( uint16 channel )
{
  int res = MTLK_ERR_OK;
  
  if ((channel < CE2040_FIRST_CHANNEL_NUMBER_IN_2G4_BAND) || (channel > CE2040_NUMBER_OF_CHANNELS_IN_2G4_BAND))
  {
    ELOG_D("Bad channel (%d) - out of bounds", channel);
    res = MTLK_ERR_VALUE;
  }
  return res;
}

static BOOL _mtlk_coex_lve_is_operation_permitted(mtlk_local_variable_evaluator *coex_lve, struct _mtlk_coex_intolerant_db *idb, uint16 primary_channel, uint16 secondary_channel )
{
  BOOL res = FALSE;
  BOOL cond_1 = FALSE;
  BOOL cond_2 = FALSE;
  BOOL cond_3 = FALSE;

  MTLK_ASSERT(coex_lve != NULL);
  MTLK_ASSERT(idb != NULL);

  if (mtlk_20_40_is_intolerance_declared(coex_lve->parent_csm) == FALSE)
  {
    if((mtlk_20_40_is_coex_el_intolerant_bit_detected(coex_lve->parent_csm) == FALSE) &&
       (mtlk_sepm_is_intolerant_or_legacy_station_connected(&coex_lve->parent_csm->ap_scexmpt) == FALSE) &&
       (MTLK_ERR_OK == _mtlk_coex_lve_check_channel_bounds(primary_channel)) &&
       (MTLK_ERR_OK == _mtlk_coex_lve_check_channel_bounds(secondary_channel)))
    {
      mtlk_20_40_perform_idb_update(coex_lve->parent_csm);
    
      /* for each condition, if one of the sides equals to zero or null, then the expression is defined 
         to have TRUE value. (standard, chapter 10.15.3.2 - scanning requirements for a 20/40MHz BSS) 
         primary/secondary channels can not be empty, as we aspire for 40MHz mode*/

      cond_1 = _mtlk_coex_verify_channels_overlapping(primary_channel, secondary_channel, primary_channel, 
        PRIMARY_CHANNEL_DATA, idb, mtlk_20_40_calc_transition_timer(coex_lve->parent_csm));

      cond_2 = _mtlk_coex_verify_channels_overlapping(primary_channel, secondary_channel, primary_channel, 
        INTOLERANT_CHANNEL_DATA, idb, mtlk_20_40_calc_transition_timer(coex_lve->parent_csm));

      cond_3 = _mtlk_coex_verify_channels_overlapping(primary_channel, secondary_channel, secondary_channel, 
        SECONDARY_CHANNEL_DATA, idb, mtlk_20_40_calc_transition_timer(coex_lve->parent_csm));

      res = (cond_1 && cond_2 && cond_3);
    }
  }
  if (res)
  {
    ILOG2_V("Operation of Moving to 40MHz is permitted");
  } 
  else
  {
    ILOG2_V("Operation of Moving to 40MHz is NOT permitted due to unsatisfied condition:");
    ILOG2_D("Condition 1 result = %d", cond_1);
    ILOG2_D("Condition 2 result = %d", cond_2);
    ILOG2_D("Condition 3 result = %d", cond_3);
    ILOG2_D("Intolerant declared = %d", mtlk_20_40_is_intolerance_declared(coex_lve->parent_csm));
  }
  
  return res;
}
