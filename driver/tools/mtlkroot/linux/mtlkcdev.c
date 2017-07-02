#include "mtlkinc.h"
#include "mtlkcdev.h"

#include <linux/module.h>

#define LOG_LOCAL_GID   GID_MTLKCDEV
#define LOG_LOCAL_FID   1

/*****************************************************************
 * MTLK cdev subsystem init/cleanup (called once in driver)
 *****************************************************************/
struct mtlk_cdev_global_data
{
  mtlk_osal_spinlock_t lock;
  mtlk_dlist_t         dev_list;
  MTLK_DECLARE_INIT_STATUS;
};

static struct mtlk_cdev_global_data cdev_global_data;

MTLK_INIT_STEPS_LIST_BEGIN(cdev_global)
  MTLK_INIT_STEPS_LIST_ENTRY(cdev_global, CDEV_GLOBAL_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(cdev_global, CDEV_GLOBAL_LIST)
MTLK_INIT_INNER_STEPS_BEGIN(cdev_global)
MTLK_INIT_STEPS_LIST_END(cdev_global);

int __MTLK_IFUNC
mtlk_cdev_drv_init (void)
{
  struct mtlk_cdev_global_data *data = &cdev_global_data;

  MTLK_INIT_TRY(cdev_global, MTLK_OBJ_PTR(data))
    MTLK_INIT_STEP(cdev_global, CDEV_GLOBAL_LOCK, MTLK_OBJ_PTR(data), 
                   mtlk_osal_lock_init, (&data->lock));
    MTLK_INIT_STEP_VOID(cdev_global, CDEV_GLOBAL_LIST, MTLK_OBJ_PTR(data), 
                        mtlk_dlist_init, (&data->dev_list));
  MTLK_INIT_FINALLY(cdev_global, MTLK_OBJ_PTR(data))
  MTLK_INIT_RETURN(cdev_global, MTLK_OBJ_PTR(data), mtlk_cdev_drv_cleanup, ());
}

void __MTLK_IFUNC
mtlk_cdev_drv_cleanup (void)
{
  struct mtlk_cdev_global_data *data = &cdev_global_data;

  MTLK_CLEANUP_BEGIN(cdev_global, MTLK_OBJ_PTR(data))
    MTLK_CLEANUP_STEP(cdev_global, CDEV_GLOBAL_LIST, MTLK_OBJ_PTR(data), 
                      mtlk_dlist_cleanup, (&data->dev_list));
    MTLK_CLEANUP_STEP(cdev_global, CDEV_GLOBAL_LOCK, MTLK_OBJ_PTR(data), 
                      mtlk_osal_lock_cleanup, (&data->lock));
  MTLK_CLEANUP_END(cdev_global, MTLK_OBJ_PTR(data));
}

static void
_mtlk_cdev_drv_add_to_db (mtlk_dlist_entry_t *lentry)
{
  struct mtlk_cdev_global_data *data = &cdev_global_data;

  mtlk_osal_lock_acquire(&data->lock);
  mtlk_dlist_push_back(&data->dev_list, lentry);
  mtlk_osal_lock_release(&data->lock);
}

static void
_mtlk_cdev_drv_del_from_db (mtlk_dlist_entry_t *lentry)
{
  struct mtlk_cdev_global_data *data = &cdev_global_data;

  mtlk_osal_lock_acquire(&data->lock);
  mtlk_dlist_remove(&data->dev_list, lentry);
  mtlk_osal_lock_release(&data->lock);
}
/*****************************************************************/

#define MTLK_CDEV_MAX_NODES_SUPPORTED MINOR((dev_t)-1)

struct mtlk_cdev_node_private
{
  mtlk_dlist_entry_t        lentry;
  int                       minor;
  mtlk_cdev_t              *cd;
  mtlk_cdev_ioctl_handler_f ioctl_handler;
  mtlk_rmlock_t             ioctl_handler_rmlock;
  mtlk_osal_spinlock_t      ioctl_handler_lock;
  mtlk_atomic_t             node_ref_count;
  mtlk_handle_t             ctx;
};

MTLK_INIT_STEPS_LIST_BEGIN(cdev)
  MTLK_INIT_STEPS_LIST_ENTRY(cdev, CDEV_LOCK)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  MTLK_INIT_STEPS_LIST_ENTRY(cdev, CDEV_REGION)
  MTLK_INIT_STEPS_LIST_ENTRY(cdev, CDEV_ADD)
#else
  MTLK_INIT_STEPS_LIST_ENTRY(cdev, CDEV_REGISTER)
#endif
  MTLK_INIT_STEPS_LIST_ENTRY(cdev, CDEV_NODES_LIST)
  MTLK_INIT_STEPS_LIST_ENTRY(cdev, CDEV_ADD_TO_DB)
MTLK_INIT_INNER_STEPS_BEGIN(cdev)
MTLK_INIT_STEPS_LIST_END(cdev);

static struct file_operations _mtlk_cdev_fops;

static void
_mtlk_cdev_node_addref(mtlk_cdev_node_t *node)
{
  mtlk_osal_atomic_inc(&node->node_ref_count);
}

static void
_mtlk_cdev_node_release(mtlk_cdev_node_t *node)
{
  if(0 == mtlk_osal_atomic_dec(&node->node_ref_count)) {
    mtlk_osal_lock_cleanup(&node->ioctl_handler_lock);
    mtlk_rmlock_cleanup(&node->ioctl_handler_rmlock);
    kfree_tag(node);
  }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
static int 
_mtlk_cdev_prepare_region (mtlk_cdev_t *cd)
{
  int   res = MTLK_ERR_UNKNOWN;
  dev_t dev = MKDEV(cd->cfg.major, 0);

  if (cd->cfg.major) {
    // Major number was specified on insmod
    res = register_chrdev_region(dev, cd->cfg.max_nodes, cd->cfg.name);
    if (res < 0) {
      ELOG_DD("Can't register chrdev region (major=%d, err=%d)", cd->cfg.major, res);
      res = MTLK_ERR_NO_RESOURCES;
      goto end;
    }
  } else {
    // Allocate dynamically
    res = alloc_chrdev_region(&dev, 0, cd->cfg.max_nodes, cd->cfg.name);
    if (res < 0) {
      ELOG_D("Can't allocate chrdev region (err=%d)", res);
      res = MTLK_ERR_NO_RESOURCES;
      goto end;
    }

    cd->cfg.major = MAJOR(dev);
  }

  res = MTLK_ERR_OK;

end:
  return res;
}

static int 
_mtlk_cdev_add (mtlk_cdev_t *cd)
{
  int res;

  cdev_init(&cd->cdev, &_mtlk_cdev_fops);
  cd->cdev.owner = THIS_MODULE;
  res = cdev_add(&cd->cdev, MKDEV(cd->cfg.major, 0), cd->cfg.max_nodes);
  if (res != 0) {
    ELOG_DD("Can't add cdev (major=%d, err=%d)", cd->cfg.major, res);
    return MTLK_ERR_NO_RESOURCES;
  }

  return MTLK_ERR_OK;
}

#else
static int 
_mtlk_cdev_register (mtlk_cdev_t *cd)
{
  int res = register_chrdev(cd->cfg.major, cd->cfg.name, &_mtlk_cdev_fops);
  if (res < 0) {
    ELOG_DD("Can't register chrdev (major=%d, err=%d)", cd->cfg.major, res);
    return MTLK_ERR_NO_RESOURCES;
  }

  cd->cfg.major = cd->cfg.major?cd->cfg.major:res;
  return MTLK_ERR_OK;
}
#endif

static int
_mtlk_cdev_open (struct inode *inode, struct file *filp)
{
  int major;
  int minor;
  mtlk_dlist_entry_t *head;
  mtlk_dlist_entry_t *entry;
  mtlk_cdev_t        *cd  = NULL;
  mtlk_cdev_node_t    *node = NULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  major = imajor(inode);
  minor = iminor(inode);
#else
  major = MAJOR(inode->i_rdev);
  minor = MINOR(inode->i_rdev);
#endif

  ILOG2_DD("Open %d:%d!", major, minor);
      
  mtlk_osal_lock_acquire(&cdev_global_data.lock);
  mtlk_dlist_foreach(&cdev_global_data.dev_list, entry, head) {
    cd = MTLK_CONTAINER_OF(entry, mtlk_cdev_t, lentry);
    if (major == cd->cfg.major)
      break;
    cd = NULL;
  }
  mtlk_osal_lock_release(&cdev_global_data.lock);

  MTLK_ASSERT(cd != NULL);

  mtlk_osal_lock_acquire(&cd->lock);
  mtlk_dlist_foreach(&cd->nodes_list, entry, head) {
    node = MTLK_CONTAINER_OF(entry, mtlk_cdev_node_t, lentry);
    if (minor == node->minor)
      break;
    node = NULL;
  }
  mtlk_osal_lock_release(&cd->lock);

  if (!node) {
    return -EINVAL;
  }

  _mtlk_cdev_node_addref(node);
  filp->private_data = node;
  return 0;
}

static int
_mtlk_cdev_release(struct inode *inode, struct file *filp)
{
  MTLK_ASSERT(NULL != filp->private_data);

  _mtlk_cdev_node_release((mtlk_cdev_node_t *) filp->private_data);
  return 0;
}

long _mtlk_cdev_invoke_safe(mtlk_cdev_node_t *node,
                            unsigned int     cmd, 
                            unsigned long    arg)
{
  long res = -ENODEV;
  mtlk_cdev_ioctl_handler_f handler = NULL;

  mtlk_osal_lock_acquire(&node->ioctl_handler_lock);

  if(NULL != node->ioctl_handler) {
    mtlk_rmlock_acquire(&node->ioctl_handler_rmlock);
    handler = node->ioctl_handler;
  }

  mtlk_osal_lock_release(&node->ioctl_handler_lock);

  if(NULL != handler) {
    res = handler(node->ctx, cmd, arg);
    mtlk_rmlock_release(&node->ioctl_handler_rmlock);
  }

  return res;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
static long 
_mtlk_cdev_unlocked_ioctl (struct file  *filp, 
                           unsigned int  cmd, 
                           unsigned long arg)
{
  mtlk_cdev_node_t *node = filp->private_data;

  MTLK_ASSERT(node != NULL);

  return _mtlk_cdev_invoke_safe(node, cmd, arg);
}
#else
static int 
_mtlk_cdev_ioctl (struct inode *inode, 
                  struct file  *filp, 
                  unsigned int  cmd, 
                  unsigned long arg)
{
  mtlk_cdev_node_t *node = filp->private_data;

  MTLK_ASSERT(node != NULL);

  return (int)_mtlk_cdev_invoke_safe(node, cmd, arg);
}
#endif

static struct file_operations _mtlk_cdev_fops =
{
  .owner          = THIS_MODULE,
  .llseek         = no_llseek,
  .open           = _mtlk_cdev_open,
  .release        = _mtlk_cdev_release,

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
  .unlocked_ioctl = _mtlk_cdev_unlocked_ioctl,
#else
  .ioctl          = _mtlk_cdev_ioctl,
#endif
};

int __MTLK_IFUNC
mtlk_cdev_init (mtlk_cdev_t *cd, const struct mtlk_cdev_cfg *cfg)
{
  MTLK_ASSERT(cd != NULL);
  MTLK_ASSERT(cfg != NULL);
  MTLK_ASSERT(cfg->name != NULL);

  cd->cfg = *cfg;
  if (!cd->cfg.max_nodes) {
    ILOG0_D("Max nodex set to %d", MTLK_CDEV_MAX_NODES_SUPPORTED);
    cd->cfg.max_nodes = MTLK_CDEV_MAX_NODES_SUPPORTED;
  }
  else if (cd->cfg.max_nodes > MTLK_CDEV_MAX_NODES_SUPPORTED) {
    MTLK_ASSERT(!"Too many nodes configured");
    cd->cfg.max_nodes = MTLK_CDEV_MAX_NODES_SUPPORTED;
  }

  MTLK_INIT_TRY(cdev, MTLK_OBJ_PTR(cd))
    MTLK_INIT_STEP(cdev, CDEV_LOCK, MTLK_OBJ_PTR(cd), 
                   mtlk_osal_lock_init, (&cd->lock));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    MTLK_INIT_STEP(cdev, CDEV_REGION, MTLK_OBJ_PTR(cd), 
                   _mtlk_cdev_prepare_region, (cd));
    MTLK_INIT_STEP(cdev, CDEV_ADD, MTLK_OBJ_PTR(cd), 
                   _mtlk_cdev_add, (cd));
#else
    MTLK_INIT_STEP(cdev, CDEV_REGISTER, MTLK_OBJ_PTR(cd), 
                   _mtlk_cdev_register, (cd));
#endif
    MTLK_INIT_STEP_VOID(cdev, CDEV_NODES_LIST, MTLK_OBJ_PTR(cd), 
                        mtlk_dlist_init, (&cd->nodes_list));
    MTLK_INIT_STEP_VOID(cdev, CDEV_ADD_TO_DB, MTLK_OBJ_PTR(cd),
                        _mtlk_cdev_drv_add_to_db, (&cd->lentry));
  MTLK_INIT_FINALLY(cdev, MTLK_OBJ_PTR(cd))
  MTLK_INIT_RETURN(cdev, MTLK_OBJ_PTR(cd), mtlk_cdev_cleanup, (cd))
}

int __MTLK_IFUNC
mtlk_cdev_get_major (mtlk_cdev_t *cd)
{
  MTLK_ASSERT(cd != NULL);

  return cd->cfg.major;
}

void __MTLK_IFUNC
mtlk_cdev_cleanup (mtlk_cdev_t *cd)
{
  MTLK_CLEANUP_BEGIN(cdev, MTLK_OBJ_PTR(cd))
    MTLK_CLEANUP_STEP(cdev, CDEV_ADD_TO_DB, MTLK_OBJ_PTR(cd),
                      _mtlk_cdev_drv_del_from_db, (&cd->lentry));
    MTLK_CLEANUP_STEP(cdev, CDEV_NODES_LIST, MTLK_OBJ_PTR(cd),
                      mtlk_dlist_cleanup, (&cd->nodes_list));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    MTLK_CLEANUP_STEP(cdev, CDEV_ADD, MTLK_OBJ_PTR(cd), 
                      cdev_del, (&cd->cdev));
    MTLK_CLEANUP_STEP(cdev, CDEV_REGION, MTLK_OBJ_PTR(cd), 
                      unregister_chrdev_region, (MKDEV(cd->cfg.major, 0), cd->cfg.max_nodes));
#else
    MTLK_CLEANUP_STEP(cdev, CDEV_REGISTER, MTLK_OBJ_PTR(cd), 
                      unregister_chrdev, (cd->cfg.major, cd->cfg.name));
#endif
    MTLK_CLEANUP_STEP(cdev, CDEV_LOCK, MTLK_OBJ_PTR(cd), 
                      mtlk_osal_lock_cleanup, (&cd->lock));
  MTLK_CLEANUP_END(cdev, MTLK_OBJ_PTR(cd))
}

mtlk_cdev_node_t *__MTLK_IFUNC
mtlk_cdev_node_create (mtlk_cdev_t              *cd, 
                       int                       minor, 
                       mtlk_cdev_ioctl_handler_f ioctl_handler, 
                       mtlk_handle_t             ctx)
{
  mtlk_cdev_node_t *node;
  int               max_nodes_reached = 0;

  MTLK_ASSERT(cd != NULL);
  MTLK_ASSERT(ioctl_handler != NULL);

  node = kmalloc_tag(sizeof(*node), GFP_KERNEL, MTLK_MEM_TAG_CDEV);
  if (!node) {
    ELOG_V("Can't allocate NODE");
    goto end;
  }

  node->cd            = cd;
  node->minor         = minor;
  node->ioctl_handler = ioctl_handler;
  node->ctx           = ctx;

  if(MTLK_ERR_OK != mtlk_osal_lock_init(&node->ioctl_handler_lock)) {
    ELOG_V("Can't initialize NODE lock");
    kfree_tag(node);
    node = NULL;
    goto end;
  }

  if(MTLK_ERR_OK != mtlk_rmlock_init(&node->ioctl_handler_rmlock)) {
    ELOG_V("Can't initialize NODE rmlock");
    mtlk_osal_lock_cleanup(&node->ioctl_handler_lock);
    kfree_tag(node);
    node = NULL;
    goto end;
  }

  mtlk_rmlock_acquire(&node->ioctl_handler_rmlock);

  mtlk_osal_lock_acquire(&cd->lock);
  if (mtlk_dlist_size(&cd->nodes_list) < cd->cfg.max_nodes) {
    mtlk_dlist_push_back(&cd->nodes_list, &node->lentry);
  }
  else {
    max_nodes_reached = 1;
  }
  mtlk_osal_lock_release(&cd->lock);

  if (max_nodes_reached) {
    WLOG_DD("Can't add node with minor#%d - MAX nodes limit reached (%d)",
            minor, cd->cfg.max_nodes);
    mtlk_osal_lock_cleanup(&node->ioctl_handler_lock);
    mtlk_rmlock_cleanup(&node->ioctl_handler_rmlock);
    kfree_tag(node);
    node = NULL;
    goto end;
  }

  /* Creator owns the initial reference to the node */
  mtlk_osal_atomic_set(&node->node_ref_count, 1);

end:
  return node;
}

int __MTLK_IFUNC
mtlk_cdev_node_get_minor (mtlk_cdev_node_t *node)
{
  MTLK_ASSERT(node != NULL);
  MTLK_ASSERT(node->cd != NULL);

  return node->minor;
}

void __MTLK_IFUNC
mtlk_cdev_node_delete (mtlk_cdev_node_t *node)
{
  MTLK_ASSERT(node != NULL);
  MTLK_ASSERT(node->cd != NULL);

  /* Make sure no new open calls will arrive */
  mtlk_osal_lock_acquire(&node->cd->lock);
  mtlk_dlist_remove(&node->cd->nodes_list, &node->lentry);
  mtlk_osal_lock_release(&node->cd->lock);

  /* Make sure ioctl handler will not be invoked anymore */
  mtlk_osal_lock_acquire(&node->ioctl_handler_lock);
  node->ioctl_handler = NULL;
  mtlk_osal_lock_release(&node->ioctl_handler_lock);

  /* Wait for current threads executing ioctl handler to leave it */
  mtlk_rmlock_release(&node->ioctl_handler_rmlock);
  mtlk_rmlock_wait(&node->ioctl_handler_rmlock);

  /* Dereference node memory. It will be freed when last reference gone */
  _mtlk_cdev_node_release(node);
}
