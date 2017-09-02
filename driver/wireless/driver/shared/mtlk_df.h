/*
 * $Id: mtlk_df.h 12816 2012-03-06 12:48:15Z hatinecs $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Driver framework debug API.
 *
 * Originally written by Andrii Tseglytskyi
 *
 */

#ifndef __DF_UI__
#define __DF_UI__

#include "mtlkdfdefs.h"
#include "mtlk_vap_manager.h"
#include "mtlknbufpriv.h"
#include "mtlkirbd.h"
#include "mtlk_wss.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/**********************************************************************
 * Data types declaration
***********************************************************************/
/***
 * DF PROC OS dependent definitions
 ***/
/* sequential print support */
#define _mtlk_seq_entry_t seq_file

/* sequential print support - context */
typedef struct _mtlk_seq_entry_t mtlk_seq_entry_t;

/**********************************************************************
 * Functions declaration
***********************************************************************/
/*! \fn      void mtlk_dfg_init(void)
    
    \brief   inits global df object.

    \param   dfg         df object

    \return  mtlk_err... code
*/
int mtlk_dfg_init(void);

/*! \fn      void mtlk_dfg_cleanup(void)
    
    \brief   cleanups global df object.

    \param   dfg         df object
*/
void mtlk_dfg_cleanup(void);

/*! \fn
    \brief   Returns pointer to driver-level IRB object
*/
mtlk_irbd_t * __MTLK_IFUNC
mtlk_dfg_get_driver_irbd(void);

/*! \fn
    \brief   Returns pointer to driver-level WSS object
*/
mtlk_wss_t * __MTLK_IFUNC
mtlk_dfg_get_driver_wss(void);

/*! \fn      mtlk_df_t* mtlk_df_create(mtlk_vap_handle_t vap_handle)
    
    \brief   Allocates and initiates DF object.

    \return  pointer to created DF
*/
mtlk_df_t* mtlk_df_create(mtlk_vap_handle_t vap_handle);

/*! \fn      void mtlk_df_delete(mtlk_df_t *df)
    
    \brief   Cleanups and deletes DF object.

    \param   df          DF object 
*/
void mtlk_df_delete(mtlk_df_t *df);

/*! \fn      void mtlk_df_start(mtlk_df_t *df)
    
    \brief   Starts the DF object.

    \param   df          DF object
    \param   reason      Stop reason (in case of error during start)
*/
int mtlk_df_start(mtlk_df_t *df, mtlk_vap_manager_interface_e intf);

/*! \fn      void mtlk_df_stop(mtlk_df_t *df)
    
    \brief   Stops the DF object.

    \param   df          DF object
    \param   reason      Stop reason
*/
void mtlk_df_stop(mtlk_df_t *df, mtlk_vap_manager_interface_e intf);

/*! \fn      mtlk_core_t* mtlk_df_get_core(mtlk_df_t *df);
    
    \brief   Returns pointer to Core object.

    \param   df          DF object 
*/
mtlk_core_t* mtlk_df_get_core(mtlk_df_t *df);

mtlk_vap_manager_t*
mtlk_df_get_vap_manager(const mtlk_df_t* df);

mtlk_vap_handle_t
mtlk_df_get_vap_handle(const mtlk_df_t* df);

const char* mtlk_df_get_name(mtlk_df_t *df);


#ifdef MTCFG_TSF_TIMER_ACCESS_ENABLED
/*! \fn      mtlk_df_t* mtlk_df_get_df_by_dfg_entry(mtlk_dlist_entry_t *df_entry)

    \brief   Returns pointer to DF object, expanded from node in DFG list

    \param   df_entry list entry for DF object
*/
mtlk_df_t* mtlk_df_get_df_by_dfg_entry(mtlk_dlist_entry_t *df_entry);
#endif /* MTCFG_TSF_TIMER_TIMESTAMPS_IN_DEBUG_PRINTOUTS */

#ifdef MTCFG_ENABLE_OBJPOOL
void __MTLK_IFUNC
mtlk_df_user_request_dump(mtlk_seq_entry_t *s, mtlk_user_request_t *user_req);
#endif /*MTCFG_ENABLE_OBJPOOL*/

/**********************************************************************
 * DF Network Buffer (former - Payload) Interface
 *  - The DF sub-interface which is provided OS depended functionality
 *    for Network Buffer manipulations
 **********************************************************************/

static __INLINE void*
mtlk_df_nbuf_get_virt_addr(mtlk_nbuf_t *nbuf);
static __INLINE uint32
mtlk_df_nbuf_get_tail_room_size(mtlk_nbuf_t *nbuf);
static __INLINE void
mtlk_df_nbuf_reserve(mtlk_nbuf_t *nbuf, uint32 len);
static __INLINE void*
mtlk_df_nbuf_put(mtlk_nbuf_t *nbuf, uint32 len);
static __INLINE void
mtlk_df_nbuf_trim(mtlk_nbuf_t *nbuf, uint32 len);
static __INLINE void*
mtlk_df_nbuf_pull(mtlk_nbuf_t *nbuf, uint32 len);
/*!
\brief   Read data from data buffer.

\param   nbuf           Pointer to data buffer
\param   offset         Offset from buffer beginning
\param   length         Number of bytes to read
\param   destination    Destination buffer

\return  MTLK_ERR... values, MTLK_ERR_OK if succeeded
*/
static __INLINE int
mtlk_df_nbuf_read(mtlk_nbuf_t *nbuf, uint32 offset, uint32 length, uint8 *destination);

/*! TODO: GS: unused - delete it
\brief   Write data to data buffer.

\param   nbuf           Pointer to data buffer
\param   offset         Offset from buffer beginning
\param   length         Number of bytes to write
\param   destination    Source buffer

\return  MTLK_ERR... values, MTLK_ERR_OK if succeeded
*/
static __INLINE int
mtlk_df_nbuf_write(mtlk_nbuf_t *nbuf, uint32 offset, uint32 length, uint8 *source);

/*! TODO: GS: unused - delete it
\brief   Query total data buffer length.

\param   nbuf        Pointer to data buffer

\return  Number of bytes
*/
static __INLINE uint32
mtlk_df_nbuf_get_data_length(mtlk_nbuf_t *nbuf);

/*!
\brief   Get packet priority supplied by OS.

\param   nbuf        Pointer to data buffer

\return  Priority value
*/
static __INLINE uint16
mtlk_df_nbuf_get_priority(mtlk_nbuf_t *nbuf);

/*!
\brief   Set packet's priority for OS.

\param   nbuf        Pointer to data buffer
\param   priority    Paket priority value
*/
static __INLINE void
mtlk_df_nbuf_set_priority(mtlk_nbuf_t *nbuf, uint16 priority);

static __INLINE mtlk_nbuf_priv_t*
mtlk_nbuf_priv(mtlk_nbuf_t *nbuf);

/**********************************************************************
 * DF Network Buffers lists (doubly linked)
 **********************************************************************/
/*!
  \brief   Function initializes list object
  \param   pbuflist List object
 */
static __INLINE void
mtlk_df_nbuf_list_init(mtlk_nbuf_list_t *pbuflist);

/*!
  \brief   Function cleanups list object, also asserts in case list is not empty.
  \param   pbuflist List object
 */
static __INLINE void
mtlk_df_nbuf_list_cleanup(mtlk_nbuf_list_t *pbuflist);

/*!
  \brief   Function adds entry to the beginning of a list
  \param   pbuflist List object
           pentry   List entry
 */
static __INLINE void
mtlk_df_nbuf_list_push_front(mtlk_nbuf_list_t *pbuflist, mtlk_nbuf_list_entry_t *pentry);

/*!
  \brief   Function removes entry from the beginning of a list
  \param   pbuflist List object
  \return  Removed entry or NULL if list is empty
 */
static __INLINE mtlk_nbuf_list_entry_t *
mtlk_df_nbuf_list_pop_front(mtlk_nbuf_list_t *pbuflist);

/*!
  \fn      mtlk_nbuf_list_remove_entry(mtlk_nbuf_list_t* pbuflist, mtlk_nbuf_list_entry_t* pentry)
  \brief   Function removes entry from the list

  \param   pbuflist List object
           pentry   List entry to be removed

  \return  Next entry after removed
 */
static __INLINE mtlk_nbuf_list_entry_t *
mtlk_df_nbuf_list_remove_entry(mtlk_nbuf_list_t       *pbuflist,
                               mtlk_nbuf_list_entry_t *pentry);

/*!
  \brief   Function adds entry to the end of a list

  \param   pbuflist List object
           pentry   List entry
 */
static __INLINE void
mtlk_df_nbuf_list_push_back(mtlk_nbuf_list_t *pbuflist, mtlk_nbuf_list_entry_t *pentry);

/*!
  \brief   Function returns head of the list

  \param   pbuflist List object

 */
static __INLINE mtlk_nbuf_list_entry_t *
mtlk_df_nbuf_list_head (mtlk_nbuf_list_t *pbuflist);

/*!
  \brief   Function returns next entry of the list

  \param   pentry   List entry

 */
static __INLINE mtlk_nbuf_list_entry_t *
mtlk_df_nbuf_list_next (mtlk_nbuf_list_entry_t *pentry);

/*!
  \brief   Function checks if list is empty

  \param   pbuflist List object

  \return  1 if list is empty, 0 - otherwise
 */
static __INLINE int8
mtlk_df_nbuf_list_is_empty(mtlk_nbuf_list_t *pbuflist);

/*!
  \brief   Function returns count of elements which are in list

  \param   pbuflist List object

  \return  Count of elements which are in list
 */
static __INLINE uint32
mtlk_df_nbuf_list_size(mtlk_nbuf_list_t* pbuflist);

/*!
\brief   Returns pointer to packet's buffer list entry.

\param   nbuf        Pointer to data buffer

\return  Pointer to buffer list entry
*/
static __INLINE mtlk_nbuf_list_entry_t *
mtlk_df_nbuf_get_list_entry(mtlk_nbuf_t *nbuf);

/*!
\brief   Returns pointer to packet by its buffer list entry.

\return  Pointer to buffer list entry
*/

static __INLINE mtlk_nbuf_t *
mtlk_df_nbuf_get_by_list_entry(mtlk_nbuf_list_entry_t *pentry);

/**********************************************************************
 * DF FW utilities
 **********************************************************************/
typedef struct
{
  const uint8  *buffer;
  uint32        size;
  mtlk_handle_t context; /* for mtlk_hw_bus_t's usage. MMB does not use it. */
} __MTLK_IDATA mtlk_df_fw_file_buf_t;

/*
 * \brief   Load FW binaries from FS in memory.
 * */
int __MTLK_IFUNC
mtlk_df_fw_load_file (mtlk_df_t *df, const char *name,
                      mtlk_df_fw_file_buf_t *fb);

/*
 * \brief   Release FW related data in memory.
 * */
void __MTLK_IFUNC
mtlk_df_fw_unload_file(mtlk_df_t *df, mtlk_df_fw_file_buf_t *fb);

/**********************************************************************
 * DF USER utilities (CORE->DF->DF_USER->User space)
 **********************************************************************/
/**

*\brief Outer world interface subsidiary of driver framework

*\defgroup DFUSER DF user
*\{
*/

enum BCL_UNIT_TYPE {
  BCL_UNIT_INT_RAM = 0,
  BCL_UNIT_AFE_RAM,
  BCL_UNIT_RFIC_RAM,
  BCL_UNIT_EEPROM,
  BCL_UNIT_MAX = 10
};


/*! Notifies DF UI about user request processing completion

    \param   req     Request object.
    \param   result  Processing result.

    \warning
    Do not garble request processing result with request execution result.
    Request processing result indicates whether request was \b processed
    successfully. In case processing result is MTK_ERR_OK, caller may examine
    request execution result and collect request execution resulting data.

*/
void __MTLK_IFUNC
mtlk_df_ui_req_complete(mtlk_user_request_t *req, int result);

void __MTLK_IFUNC
mtlk_df_ui_set_mac_addr(mtlk_df_t *df, const uint8* mac_addr);

const uint8* __MTLK_IFUNC
mtlk_df_ui_get_mac_addr(mtlk_df_t* df);

BOOL __MTLK_IFUNC
mtlk_df_ui_is_promiscuous(mtlk_df_t *df);

/***
 * Requests from Core
 ***/
/* Data transfer functions */
int __MTLK_IFUNC
mtlk_df_ui_indicate_rx_data(mtlk_df_t *df_user, mtlk_nbuf_t *nbuf);

BOOL __MTLK_IFUNC
mtlk_df_ui_check_is_mc_group_member(mtlk_df_t *df, const uint8* group_addr);

/***
 * Notifications from Core
 ***/
void __MTLK_IFUNC
mtlk_df_ui_notify_tx_start(mtlk_df_t *df);

/* Wireless subsystem access API*/
void __MTLK_IFUNC
mtlk_df_ui_notify_association(mtlk_df_t *df, const uint8 *mac);

void __MTLK_IFUNC
mtlk_df_ui_notify_disassociation(mtlk_df_t *df);

void __MTLK_IFUNC
mtlk_df_ui_notify_node_connect(mtlk_df_t *df, const uint8 *node_addr);

void __MTLK_IFUNC
mtlk_df_ui_notify_node_disconect(mtlk_df_t *df, const uint8 *node_addr);

void __MTLK_IFUNC
mtlk_df_ui_notify_secure_node_connect(mtlk_df_t *df_user, const uint8 *node_addr,
                                        const uint8 *rsnie, size_t rsnie_len);

void __MTLK_IFUNC
mtlk_df_ui_notify_scan_complete(mtlk_df_t *df);

typedef enum
{
  MIC_FAIL_PAIRWISE = 0,
  MIC_FAIL_GROUP
} mtlk_df_ui_mic_fail_type_t;

void __MTLK_IFUNC
mtlk_df_ui_notify_mic_failure(mtlk_df_t *df,
                                const uint8 *src_mac,
                                mtlk_df_ui_mic_fail_type_t fail_type);

void __MTLK_IFUNC
mtlk_df_ui_notify_notify_rmmod(uint32 rmmod_data);

void __MTLK_IFUNC
mtlk_df_ui_notify_notify_fw_hang(mtlk_df_t *df, uint32 fw_cpu, uint32 sw_watchdog_data);

/**********************************************************************
 * DF PROC FS utilities
 **********************************************************************/

#define mtlk_aux_seq_printf seq_printf

/* Proc_FS node type - Node contains entries */
typedef struct _mtlk_df_proc_fs_node_t mtlk_df_proc_fs_node_t;

/* read proc handler type */
typedef ssize_t (*mtlk_df_proc_entry_read_f)(struct file *file, char __user *buffer,
                                         size_t count, loff_t *off);

/* write proc handler type */
typedef ssize_t (*mtlk_df_proc_entry_write_f)(struct file *file, const char __user *buffer,
                                         size_t count, loff_t *off);

/* seq file show handler type */
typedef int (*mtlk_df_proc_seq_entry_show_f)(mtlk_seq_entry_t *seq_ctx, void *data);



mtlk_df_proc_fs_node_t* __MTLK_IFUNC
mtlk_df_proc_node_create(const uint8 *name, mtlk_df_proc_fs_node_t* parent);

void __MTLK_IFUNC
mtlk_df_proc_node_delete(mtlk_df_proc_fs_node_t* proc_node);

int __MTLK_IFUNC
mtlk_df_proc_node_add_ro_entry(char *name,
                               mtlk_df_proc_fs_node_t *parent_node,
                               void *df,
                               mtlk_df_proc_entry_read_f  read_func);

int __MTLK_IFUNC
mtlk_df_proc_node_add_wo_entry(char *name,
                               mtlk_df_proc_fs_node_t *parent_node,
                               void *df,
                               mtlk_df_proc_entry_write_f write_func);

int  __MTLK_IFUNC
mtlk_df_proc_node_add_rw_entry(char *name,
                               mtlk_df_proc_fs_node_t *parent_node,
                               void *df,
                               mtlk_df_proc_entry_read_f  read_func,
                               mtlk_df_proc_entry_write_f write_func);

int __MTLK_IFUNC
mtlk_df_proc_node_add_seq_entry(char *name,
                                 mtlk_df_proc_fs_node_t *parent_node,
                                 void *df,
                                 mtlk_df_proc_seq_entry_show_f show_func);

void __MTLK_IFUNC
mtlk_df_proc_node_remove_entry(char *name,
                               mtlk_df_proc_fs_node_t *parent_node);

void* __MTLK_IFUNC
mtlk_df_proc_seq_entry_get_df(mtlk_seq_entry_t *seq_ctx);

void mtlk_aux_seq_printf(mtlk_seq_entry_t *seq_ctx, const char *fmt, ...);



#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#define  SAFE_PLACE_TO_INCLUDE_DF_NBUF_IMPL
#include "mtlk_df_nbuf_impl.h"

#if !defined(MTCFG_ENABLE_OBJPOOL) && defined(MTCFG_ENABLE_NBUF_OBJPOOL)
#error ENABLE_OBJPOOL must be defined for ENABLE_NBUF_OBJPOOL
#endif

#ifdef MTCFG_ENABLE_NBUF_OBJPOOL
#define mtlk_objpool_add_nbuf(nbuf, caller_slid) \
  if(NULL != nbuf) {  \
    mtlk_objpool_add_object(&g_objpool, \
                            &( ((struct mtlk_nbuf_priv_internal*)mtlk_nbuf_priv(nbuf))->objpool_ctx ), \
                            MTLK_NET_BUF_OBJ, caller_slid); \
  }

#define mtlk_objpool_remove_nbuf(nbuf) \
  mtlk_objpool_remove_object(&g_objpool, \
                             &( ((struct mtlk_nbuf_priv_internal*)mtlk_nbuf_priv(nbuf))->objpool_ctx ), \
                             MTLK_NET_BUF_OBJ);
#else
#define mtlk_objpool_add_nbuf(nbuf, caller_slid) MTLK_UNREFERENCED_PARAM(caller_slid)
#define mtlk_objpool_remove_nbuf(nbuf)
#endif

static __INLINE mtlk_nbuf_t *
__mtlk_df_nbuf_alloc (mtlk_df_t *df, uint32 size, mtlk_slid_t caller_slid)
{
  mtlk_nbuf_t * nbuf = _mtlk_df_nbuf_alloc_osdep(df, size);
  mtlk_objpool_add_nbuf(nbuf, caller_slid);
  return nbuf;
}

#define mtlk_df_nbuf_alloc(df, size) __mtlk_df_nbuf_alloc((df), (size), MTLK_SLID)

static __INLINE void mtlk_df_nbuf_free(mtlk_df_t *df, mtlk_nbuf_t *nbuf)
{
  mtlk_objpool_remove_nbuf(nbuf);
  _mtlk_df_nbuf_free_osdep(df, nbuf);
}

static __INLINE mtlk_nbuf_t *
__mtlk_df_nbuf_clone_no_priv (mtlk_df_t *df, mtlk_nbuf_t *nbuf, mtlk_slid_t caller_slid)
{
  mtlk_nbuf_t *nbuf_new = _mtlk_df_nbuf_clone_no_priv_osdep(df, nbuf);
  mtlk_objpool_add_nbuf(nbuf_new, caller_slid);
  return nbuf_new;
}

#define mtlk_df_nbuf_clone_no_priv(df, nbuf) __mtlk_df_nbuf_clone_no_priv((df), (nbuf), MTLK_SLID)

static __INLINE mtlk_nbuf_t *
__mtlk_df_nbuf_clone_with_priv (mtlk_df_t *df, mtlk_nbuf_t *nbuf, mtlk_slid_t caller_slid)
{
  mtlk_nbuf_t *nbuf_new = _mtlk_df_nbuf_clone_with_priv_osdep(df, nbuf);
  mtlk_objpool_add_nbuf(nbuf_new, caller_slid);
  return nbuf_new;
}

#define mtlk_df_nbuf_clone_with_priv(df, nbuf) __mtlk_df_nbuf_clone_with_priv((df), (nbuf), MTLK_SLID)

static __INLINE void
__mtlk_nbuf_start_tracking (mtlk_nbuf_t *nbuf, mtlk_slid_t caller_slid)
{
#ifdef MTCFG_PER_PACKET_STATS
  mtlk_nbuf_priv_attach_stats(mtlk_nbuf_priv(nbuf));
#endif
  mtlk_objpool_add_nbuf(nbuf, caller_slid);
}

#define mtlk_nbuf_start_tracking(nbuf) __mtlk_nbuf_start_tracking((nbuf), MTLK_SLID)

static __INLINE void mtlk_nbuf_stop_tracking(mtlk_nbuf_t *nbuf)
{
  mtlk_objpool_remove_nbuf(nbuf);
}

#endif /* __DF_UI__ */
