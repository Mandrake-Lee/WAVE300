#ifndef __MTLK_CDEV_H__
#define __MTLK_CDEV_H__

#include "mtlk_osal.h"
#include "mtlklist.h"

/*! \file  mtlkcdev.h
    \brief Linux character device wrapper

    Kernel version independent Char Device API.
*/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <linux/cdev.h>
#endif

/*****************************************************************
 * MTLK cdev subsystem init/cleanup (called once in driver)
 *****************************************************************/
/*! \fn      int mtlk_cdev_drv_init(void)
    \brief   Initiates driver-wide MTLK CDEV data.

    \return  MTLK_ERR... code

    \warning This function should be called once a driver, 
             prior to any other mtlk_cdev_...() call.
*/
int __MTLK_IFUNC
mtlk_cdev_drv_init(void);

/*! \fn     void mtlk_cdev_drv_cleanup(void)
    \brief  Cleans up the driver-wide MTLK CDEV data.

    \warning This function should be called once a driver, 
             no mtlk_cdev_...() calls are allowed after this.
*/
void __MTLK_IFUNC
mtlk_cdev_drv_cleanup(void);
/*****************************************************************/

/*****************************************************************
 * MTLK cdev API
 *****************************************************************/

/*! \def   MTLK_CDEV_MAX_NAME_LEN
    \brief Char Device name length limitation.
*/
#define MTLK_CDEV_MAX_NAME_LEN 32

/*! \typedef struct mtlk_cdev_private mtlk_cdev_t
    \brief   "Black box" Char Device object. 
*/
typedef struct mtlk_cdev_private mtlk_cdev_t;

/*! \typedef struct mtlk_cdev_node_private mtlk_cdev_node_t
    \brief   "Black box" Char Device node object. 
*/
typedef struct mtlk_cdev_node_private mtlk_cdev_node_t;

/*! \struct mtlk_cdev_cfg
    \brief  Single Char Device configuration.
*/
struct mtlk_cdev_cfg
{
  int                             major;                        /*!< Desired \c major. 0 for dynamic assignment. */
  uint32                          max_nodes;                    /*!< Maximal nodes supported for this Char Device,
                                                                     0 - maximum supported by kernel. */
  char                            name[MTLK_CDEV_MAX_NAME_LEN]; /*!< Device name as it will appear in /proc/devices */
};

/*! \struct  mtlk_cdev_private
    \brief   "Black box" Char Device object. 

    \warning Must be accessed using the mtlk_cdev_t type and MTLK CDEV API only!
*/
struct mtlk_cdev_private
{
  mtlk_dlist_entry_t   lentry;     /*!< \private For global majors list (mtlk_cdev_global_data->dev_list) */
  struct mtlk_cdev_cfg cfg;        /*!< \private */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  struct cdev          cdev;       /*!< \private */
#endif
  mtlk_osal_spinlock_t lock;       /*!< \private */
  mtlk_dlist_t         nodes_list; /*!< \private List of minors */
  MTLK_DECLARE_INIT_STATUS;        /*!< \private */
};

/*! \typedef long (*mtlk_cdev_ioctl_handler_f)(mtlk_handle_t ctx, 
                                               unsigned int  cmd, 
                                               unsigned long arg)
    \brief   Char Device node's IOCTL handler function prototype.

    This function acts as regular kernel's <tt>struct file_operations::ioctl</tt>.

    \param   ctx user-defined context previously passed to the mtlk_cdev_node_create().
    \param   cmd The same as <tt>struct file_operations::ioctl</tt> \e cmd parameter.
    \param   arg The same as <tt>struct file_operations::ioctl</tt> \e arg parameter.

    \return  the same as struct file_operations::ioctl returns.
*/
typedef long (__MTLK_IFUNC *mtlk_cdev_ioctl_handler_f)(mtlk_handle_t ctx, 
                                                       unsigned int  cmd, 
                                                       unsigned long arg);

/*! \fn      int mtlk_cdev_init(mtlk_cdev_t *cd, const struct mtlk_cdev_cfg *cfg)
    \brief   Initiates Char Device object.

    This function initializes internal device-specific data and registers 
    Char Device driver with the kernel and internal DB.

    \param   cd  Char Device object.
    \param   cfg Char Device configuration.

    \return  MTLK_ERR... code
*/
int __MTLK_IFUNC
mtlk_cdev_init(mtlk_cdev_t *cd, const struct mtlk_cdev_cfg *cfg);

/*! \fn      int mtlk_cdev_get_major(mtlk_cdev_t *cd)
    \brief   Returnes Char Device's \c major.

    This function returnes actual Char Device \c major 
    (specified at struct mtlk_cdev_cfg::major or assigned dinamically).

    \param   cd Char Device object.

    \return  actual \c major value.
*/
int __MTLK_IFUNC
mtlk_cdev_get_major(mtlk_cdev_t *cd);

/*! \fn      void mtlk_cdev_cleanup(mtlk_cdev_t *cd)
    \brief   Cleans up the Char Device object.

    This function unreggisters Char Device driver with the kernel and 
    and internal DB and cleans up the internal device-specific data.

    \param   cd Char Device object.
*/
void __MTLK_IFUNC
mtlk_cdev_cleanup(mtlk_cdev_t *cd);

/*! \fn      mtlk_cdev_node_t *mtlk_cdev_node_create(mtlk_cdev_t             *cd, 
                                                     int                       minor, 
                                                     mtlk_cdev_ioctl_handler_f ioctl_handler, 
                                                     mtlk_handle_t             ctx)
    \brief   Creates Char Device node object.

    This function creates Char Device node object for specific \c minor and 
    registers it with internal DB.

    \param   cd            Char Device object.
    \param   minor         Desired \c minor.
    \param   ioctl_handler Node's IOCTL handler.
    \param   ctx           User context to be passed to node's IOCTL handler.

    \return  Char Device node object. NULL in case of an error.
*/
mtlk_cdev_node_t *__MTLK_IFUNC
mtlk_cdev_node_create (mtlk_cdev_t              *cd, 
                       int                       minor, 
                       mtlk_cdev_ioctl_handler_f ioctl_handler, 
                       mtlk_handle_t             ctx);

/*! \fn      int mtlk_cdev_node_get_minor(mtlk_cdev_node_t *node)
    \brief   Returnes Char Device node's \c minor.

    This function returnes Char Device node's \c minor.

    \param   node Char Device node object.

    \return  \c minor value.
*/
int __MTLK_IFUNC
mtlk_cdev_node_get_minor(mtlk_cdev_node_t *node);

/*! \fn      void mtlk_cdev_node_delete(mtlk_cdev_node_t *node)
    \brief   Deletes Char Device node object.

    This function deletes Char Device node object and 
    unregisters it with internal DB.

    \param   node Char Device node object.
*/
void __MTLK_IFUNC
mtlk_cdev_node_delete(mtlk_cdev_node_t *node);
/*****************************************************************/

#endif /* __MTLK_CDEV_H__ */
