/*
* $Id: mtlk_core_iface.h 11821 2011-10-25 17:05:25Z nayshtut $
*
* Copyright (c) 2006-2008 Metalink Broadband (Israel)
*
* Cross - platfrom core module code
*/

#ifndef _MTLK_CORE_IFACE_H_
#define _MTLK_CORE_IFACE_H_

struct nic;
struct _mtlk_eeprom_data_t;
struct _mtlk_dot11h_t;
struct _mtlk_vap_handle_t;

#ifdef MTCFG_RF_MANAGEMENT_MTLK
struct _mtlk_rf_mgmt_t;
#endif

/* Core "getters" */
struct _mtlk_eeprom_data_t* __MTLK_IFUNC mtlk_core_get_eeprom(struct nic* core);
struct _mtlk_dot11h_t*      __MTLK_IFUNC mtlk_core_get_dfs(struct nic* core);
uint8                       __MTLK_IFUNC mtlk_core_get_country_code (struct nic *core);
BOOL                        __MTLK_IFUNC mtlk_core_get_dot11d (struct nic *core);
uint16                      __MTLK_IFUNC mtlk_core_get_sq_size(struct nic *nic, uint16 access_category);
uint8                       __MTLK_IFUNC mtlk_core_get_bonding(struct nic *core);

uint8  __MTLK_IFUNC mtlk_core_get_network_mode_cur(mtlk_core_t *core);
uint8  __MTLK_IFUNC mtlk_core_get_network_mode_cfg(mtlk_core_t *core);
uint8  __MTLK_IFUNC mtlk_core_get_freq_band_cur(struct nic *core);
uint8  __MTLK_IFUNC mtlk_core_get_freq_band_cfg(struct nic *core);
uint8  __MTLK_IFUNC mtlk_core_get_is_ht_cur(mtlk_core_t *core);
uint8  __MTLK_IFUNC mtlk_core_get_is_ht_cfg(mtlk_core_t *core);

/* Move the following prototype to frame master interface
   when frame module design is introduced */
#ifdef MTCFG_RF_MANAGEMENT_MTLK
struct _mtlk_rf_mgmt_t*     __MTLK_IFUNC mtlk_get_rf_mgmt(mtlk_handle_t context);
#endif

void    __MTLK_IFUNC mtlk_core_sta_country_code_update_from_bss(struct nic* core, uint8 country_code);
int16   __MTLK_IFUNC mtlk_calc_tx_power_lim_wrapper(mtlk_handle_t usr_data, int8 spectrum_mode, uint8 channel);
int16   __MTLK_IFUNC mtlk_scan_calc_tx_power_lim_wrapper(mtlk_handle_t usr_data, int8 spectrum_mode, uint8 reg_domain, uint8 channel);
int16   __MTLK_IFUNC mtlk_get_antenna_gain_wrapper(mtlk_handle_t usr_data, uint8 channel);
int     __MTLK_IFUNC mtlk_reload_tpc_wrapper (uint8 channel, mtlk_handle_t usr_data);
uint8   __MTLK_IFUNC mtlk_core_is_device_busy(mtlk_handle_t context);
void    __MTLK_IFUNC mtlk_core_notify_scan_complete(struct _mtlk_vap_handle_t *vap_handle);
BOOL    __MTLK_IFUNC mtlk_core_is_stopping(struct nic *core);

/**
*\defgroup CORE_SERIALIZED_TASKS Core serialization facility
*\brief Core interface for scheduling tasks on demand of outer world

*\{

*/

/*! Core serialized task callback prototype

    \param   object           Handle of receiver object
    \param   data             Pointer to the data buffer provided by caller
    \param   data_size        Size of data buffer provided by caller
    \return  MTLK_ERR_... code indicating whether function invocation succeeded

    \warning
    Do not garble function invocation result with execution result.
    Execution result indicates whether request was \b processed
    successfully. In case invocation result is MTK_ERR_OK, caller may examine
    Execution result and collect resulting data.
*/
typedef int __MTLK_IFUNC (*mtlk_core_task_func_t)(mtlk_handle_t object,
                                                  const void *data,
                                                  uint32 data_size);

/*! Function for scheduling serialized task on demand of internal core activities

    \param   nic              Pointer to the core object
    \param   object           Handle of receiver object
    \param   func             Task callback
    \param   data             Pointer to the data buffer provided by caller
    \param   data_size        Size of data buffer provided by caller

*/
int __MTLK_IFUNC mtlk_core_schedule_internal_task_ex(struct nic* core,
                                                     mtlk_handle_t object,
                                                     mtlk_core_task_func_t func,
                                                     const void *data, size_t size,
                                                     mtlk_slid_t issuer_slid);

#define mtlk_core_schedule_internal_task(core, object, func, data, size) \
  mtlk_core_schedule_internal_task_ex((core), (object), (func), (data), (size), MTLK_SLID)

/*\}*/
#endif //_MTLK_CORE_IFACE_H_
