/*
 * Copyright (c) 2006-2008 Metalink Broadband (Israel)
 *
 * Written by: Andriy Fidrya
 *
 */
#include "mtlkinc.h"

#include <linux/module.h>
#include <linux/poll.h>
#include <linux/version.h>
#include <linux/init.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#include <linux/fs.h>
#define mtlk_char_dev_t struct char_device
#else
#include <linux/cdev.h>
#define mtlk_char_dev_t struct cdev
#endif
#include <linux/proc_fs.h>

#include "mtlkerr.h"

#include "log_drv.h"
#include "compat.h"


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#define PROC_NET init_net.proc_net
#else
#define PROC_NET proc_net
#endif

#define LOG_LOCAL_GID   GID_LOGDRV
#define LOG_LOCAL_FID   2

int debug = 0;

extern mtlk_log_data_t log_data;

typedef struct _mtlk_cdev_t {
  int initialized;
  int chrdev_region_allocated;
  wait_queue_head_t in_queue;
  wait_queue_head_t out_queue;
  struct fasync_struct *async_queue;
  mtlk_char_dev_t cdev;
} mtlk_cdev_t;

// FIXME: delete this line
//extern mtlk_log_data_t log_data;
static int mtlk_major = 0;

// Buffer size in bytes
#define LOG_BUF_SIZE    1400

// Number of buffers to allocate for Driver, Upper and Lower MACs
#define LOG_BUF_COUNT   64

// Log buffer swap timeout in msec
#define LOG_BUF_SWAP_TIMEOUT 1000

int rdebug         = 0;
int cdebug         = 0;
int bufs_no        = LOG_BUF_COUNT;
int buf_size       = LOG_BUF_SIZE;
int buf_swap_tmout = LOG_BUF_SWAP_TIMEOUT;

// -------------
// Configuration
// -------------

#define MAX_LOG_STRING_LENGTH 1024

#ifndef MTCFG_SILENT
#define TIMESTAMP_FMT "[%010u] "
#define COMMA_TIMESTAMP_ARG ,jiffies_to_msecs(jiffies)
#else
#define TIMESTAMP_FMT
#define COMMA_TIMESTAMP_ARG
#endif

// ----------------
// Type definitions
// ----------------

#define USP_ENTRY_CDATA     1
#define USP_ENTRY_READY_BUF 2

typedef struct _mtlk_usp_entry_t
{
  mtlk_ldlist_entry_t entry; // List entry data
  int type;
  int len; // data length
  int pos; // current read position
  union {
    uint8 *pdata;
    mtlk_log_buf_entry_t *pbuf;
  } u;
} mtlk_usp_entry_t;

typedef struct _mtlk_log_osdep_data_t
{
  // Uses locking list implementation, so requires no additional locking
  mtlk_ldlist_t usp_queue; // Userspace data queue

  wait_queue_head_t in_wait_queue;
  struct proc_dir_entry *pnet_procfs_dir;
  struct proc_dir_entry *pdebug_entry;
  struct proc_dir_entry *pstat_entry;

  MTLK_DECLARE_INIT_STATUS;
} mtlk_log_osdep_data_t;

// --------------------
// Forward declarations
// --------------------

static int cdev_log_open(struct inode *inode, struct file *filp);
static int cdev_log_release(struct inode *inode, struct file *filp);
static ssize_t cdev_log_read(struct file *filp, char __user *buf,
    size_t count, loff_t *f_pos);
static ssize_t cdev_log_write(struct file *filp, const char __user *buf,
    size_t count, loff_t *f_pos);
static unsigned int cdev_log_poll(struct file *filp, poll_table *wait);
static int cdev_log_fasync(int fd, struct file *filp, int mode);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
static long cdev_unlocked_ioctl(struct file  *filp, 
                                unsigned int  cmd, 
                                unsigned long arg);
#else
static int cdev_ioctl(struct inode *inode, 
                      struct file  *filp, 
                      unsigned int  cmd, 
                      unsigned long arg);
#endif

static mtlk_usp_entry_t *alloc_usp_entry(void);
static void release_usp_entry(mtlk_usp_entry_t *usp_entry);
static void usp_queue_release_entries(void);
static int setup_cdev(void);

static int logger_debug_write(struct file *file, const char *buffer,
  size_t count, loff_t *data);
static int logger_debug_read(struct file *filp,char *page,size_t count,loff_t *offp );
static int logger_stat_read (struct file *filp,char *page,size_t count,loff_t *offp );
// ----------------
// Global variables
// ----------------

mtlk_log_osdep_data_t log_data_osdep;

// Log device can be opened by only one process at a time
static atomic_t log_dev_available = ATOMIC_INIT(1);

mtlk_cdev_t mtlk_log_dev; 

struct file_operations mtlk_log_fops =
{
  .owner =     THIS_MODULE,
  .llseek =    no_llseek,
  .open =      cdev_log_open,
  .release =   cdev_log_release,
  .read =      cdev_log_read,
  .write =     cdev_log_write,
  .poll =      cdev_log_poll,
  .fasync =    cdev_log_fasync,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
  .unlocked_ioctl = cdev_unlocked_ioctl,
#else
  .ioctl = cdev_ioctl,
#endif
};

struct file_operations logger_debug_proc_fops =
{
  .read =	logger_debug_read,
  .write =	logger_debug_write,
};

struct file_operations logger_stat_proc_fops =
{
  .read =	logger_stat_read,
  .write =	NULL,
};


// -------------------
// Interface functions
// -------------------

static int
log_cdev_init(void)
{
  int res;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  dev_t dev = MKDEV(mtlk_major, 0);

  memset(&mtlk_log_dev, 0, sizeof(mtlk_log_dev));
  if (mtlk_major) {
    // Major number was specified on insmod
    res = register_chrdev_region(dev, 1, "mtlk_log");
    if (res < 0)
      return MTLK_ERR_NO_RESOURCES;
  } else {
    // Allocate dynamically
    res = alloc_chrdev_region(&dev, 0, 1, "mtlk_log");
    if (res < 0)
      return MTLK_ERR_NO_RESOURCES;
    mtlk_major = MAJOR(dev);
  }
#else
  res = register_chrdev(mtlk_major, "mtlk_log", &mtlk_log_fops);
  if (res < 0)
    return MTLK_ERR_NO_RESOURCES;
  else if (mtlk_major == 0)
    mtlk_major = res;
#endif
  mtlk_log_dev.chrdev_region_allocated = 1;

  if (setup_cdev() < 0)
    return MTLK_ERR_NO_RESOURCES;
  return MTLK_ERR_OK;
}

static void
log_cdev_cleanup(void)
{
  if (!mtlk_log_dev.chrdev_region_allocated)
    return;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  if (mtlk_log_dev.initialized)
    cdev_del(&mtlk_log_dev.cdev);
  unregister_chrdev_region(MKDEV(mtlk_major, 0), 1);
#else
  unregister_chrdev(mtlk_major, "mtlk");
#endif

  mtlk_major = 0;
}

MTLK_INIT_STEPS_LIST_BEGIN(logdrv_osdep)
  MTLK_INIT_STEPS_LIST_ENTRY(logdrv_osdep, LOGDRV_USP_QUEUE)
  MTLK_INIT_STEPS_LIST_ENTRY(logdrv_osdep, LOGDRV_USP_QUEUE_ENTRIES)
  MTLK_INIT_STEPS_LIST_ENTRY(logdrv_osdep, LOGDRV_WAIT_QUEUE)
  MTLK_INIT_STEPS_LIST_ENTRY(logdrv_osdep, LOGDRV_PROC_DIR)
  MTLK_INIT_STEPS_LIST_ENTRY(logdrv_osdep, LOGDRV_PROC_DEBUG)
  MTLK_INIT_STEPS_LIST_ENTRY(logdrv_osdep, LOGDRV_PROC_STAT)
  MTLK_INIT_STEPS_LIST_ENTRY(logdrv_osdep, LOGDRV_CDEV_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(logdrv_osdep)
MTLK_INIT_STEPS_LIST_END(logdrv_osdep);

void __MTLK_IFUNC
mtlk_log_on_cleanup(void)
{
  MTLK_CLEANUP_BEGIN(logdrv_osdep, MTLK_OBJ_PTR(&log_data_osdep))
    MTLK_CLEANUP_STEP(logdrv_osdep, LOGDRV_CDEV_INIT, MTLK_OBJ_PTR(&log_data_osdep),
                      log_cdev_cleanup, ());
    MTLK_CLEANUP_STEP(logdrv_osdep, LOGDRV_PROC_STAT, MTLK_OBJ_PTR(&log_data_osdep),
                      proc_remove, (log_data_osdep.pstat_entry));
    MTLK_CLEANUP_STEP(logdrv_osdep, LOGDRV_PROC_DEBUG, MTLK_OBJ_PTR(&log_data_osdep),
                      proc_remove, (log_data_osdep.pdebug_entry));
    MTLK_CLEANUP_STEP(logdrv_osdep, LOGDRV_PROC_DIR, MTLK_OBJ_PTR(&log_data_osdep),
                      proc_remove, (log_data_osdep.pnet_procfs_dir));
    MTLK_CLEANUP_STEP(logdrv_osdep, LOGDRV_WAIT_QUEUE, MTLK_OBJ_PTR(&log_data_osdep),
                      MTLK_NOACTION, ());
    MTLK_CLEANUP_STEP(logdrv_osdep, LOGDRV_USP_QUEUE_ENTRIES, MTLK_OBJ_PTR(&log_data_osdep),
                      usp_queue_release_entries, ());
    MTLK_CLEANUP_STEP(logdrv_osdep, LOGDRV_USP_QUEUE, MTLK_OBJ_PTR(&log_data_osdep),
                      mtlk_ldlist_cleanup, (&log_data_osdep.usp_queue));
  MTLK_CLEANUP_END(logdrv_osdep, MTLK_OBJ_PTR(&log_data_osdep));
}

int __MTLK_IFUNC
mtlk_log_on_init(void)
{
  memset(&log_data_osdep, 0, sizeof(log_data_osdep));

  MTLK_INIT_TRY(logdrv_osdep, MTLK_OBJ_PTR(&log_data_osdep))
    MTLK_INIT_STEP_VOID(logdrv_osdep, LOGDRV_USP_QUEUE, MTLK_OBJ_PTR(&log_data_osdep),
                        mtlk_ldlist_init, (&log_data_osdep.usp_queue));

    MTLK_INIT_STEP_VOID(logdrv_osdep, LOGDRV_USP_QUEUE_ENTRIES, MTLK_OBJ_PTR(&log_data_osdep),
                        MTLK_NOACTION, ());

    MTLK_INIT_STEP_VOID(logdrv_osdep, LOGDRV_WAIT_QUEUE, MTLK_OBJ_PTR(&log_data_osdep),
                        init_waitqueue_head, (&log_data_osdep.in_wait_queue));

    MTLK_INIT_STEP_EX(logdrv_osdep, LOGDRV_PROC_DIR, MTLK_OBJ_PTR(&log_data_osdep),
                      proc_mkdir, ("mtlk_log", PROC_NET), log_data_osdep.pnet_procfs_dir,
                      NULL != log_data_osdep.pnet_procfs_dir, MTLK_ERR_NO_RESOURCES);
   
    proc_set_user(log_data_osdep.pnet_procfs_dir,KUIDT_INIT(0),KGIDT_INIT(0));

    MTLK_INIT_STEP_EX(logdrv_osdep, LOGDRV_PROC_DEBUG, MTLK_OBJ_PTR(&log_data_osdep),
                      proc_create_data, ("debug", S_IFREG|S_IRUSR|S_IRGRP, log_data_osdep.pnet_procfs_dir,&logger_debug_proc_fops,NULL),
                      log_data_osdep.pdebug_entry, NULL != log_data_osdep.pdebug_entry,
                      MTLK_ERR_NO_RESOURCES);

    proc_set_user(log_data_osdep.pdebug_entry,KUIDT_INIT(0),KGIDT_INIT(0));

    MTLK_INIT_STEP_EX(logdrv_osdep, LOGDRV_PROC_STAT, MTLK_OBJ_PTR(&log_data_osdep),
                      proc_create_data, ("statistics", S_IFREG|S_IRUSR|S_IRGRP, log_data_osdep.pnet_procfs_dir,&logger_stat_proc_fops,NULL),
                      log_data_osdep.pstat_entry, NULL != log_data_osdep.pstat_entry,
                      MTLK_ERR_NO_RESOURCES);

    proc_set_user(log_data_osdep.pstat_entry,KUIDT_INIT(0),KGIDT_INIT(0));    


    MTLK_INIT_STEP_VOID(logdrv_osdep, LOGDRV_CDEV_INIT, MTLK_OBJ_PTR(&log_data_osdep),
                        log_cdev_init, ());
  MTLK_INIT_FINALLY(logdrv_osdep, MTLK_OBJ_PTR(&log_data_osdep))
  MTLK_INIT_RETURN(logdrv_osdep, MTLK_OBJ_PTR(&log_data_osdep), mtlk_log_on_cleanup, ());
}

int __MTLK_IFUNC
mtlk_log_on_buf_ready(void)
{
  mtlk_log_buf_entry_t *ready_buf;
  mtlk_usp_entry_t *usp_entry;
  int num_bufs_processed;

  num_bufs_processed = 0;
  while ((ready_buf = mtlk_log_get_ready_buf()) != NULL) {
    usp_entry = alloc_usp_entry();
    if (!usp_entry) {
      mtlk_log_put_free_buf(ready_buf);
      ELOG_V("Out of memory: log data lost");
      return MTLK_ERR_NO_MEM;
    }
    usp_entry->type = USP_ENTRY_READY_BUF;
    usp_entry->len = ready_buf->data_size;
    usp_entry->pos = 0;
    usp_entry->u.pbuf = ready_buf;
    mtlk_ldlist_push_back(&log_data_osdep.usp_queue,
        (mtlk_ldlist_entry_t *) usp_entry);
    ++num_bufs_processed;
  }

  // This check is neccessary because other thread could already process
  // the buffer this notification arrived for
  if (num_bufs_processed > 0)
    wake_up_interruptible(&log_data_osdep.in_wait_queue);
  return MTLK_ERR_OK;
}

// ---------------
// Local functions
// ---------------

static int
setup_cdev(void)
{
  init_waitqueue_head(&mtlk_log_dev.in_queue);
  init_waitqueue_head(&mtlk_log_dev.out_queue);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
  {
    int res;
    int devno = MKDEV(mtlk_major, 0);

    cdev_init(&mtlk_log_dev.cdev, &mtlk_log_fops);
    mtlk_log_dev.cdev.owner = THIS_MODULE;
    res = cdev_add(&mtlk_log_dev.cdev, devno, 1);
    if (res != 0) {
      ELOG_D("Can't initialize mtlk character device: error %d", res);
      return -1;
    }
  }
#endif
  mtlk_log_dev.initialized = 1;
  return 0;
}

static int
cdev_log_open(struct inode *inode, struct file *filp)
{
  int ret;
  unsigned int minor = MINOR(inode->i_rdev);

  if (minor)
    return -ENOTSUPP;

  // Disallow multiple open
  if (!atomic_dec_and_test(&log_dev_available)) {
    atomic_inc(&log_dev_available);
    return -EBUSY;
  }

  // FIXME: this is a workaround: see cdev_log_release for problem description
  usp_queue_release_entries();

  // If there are events accumulated in ready queue, send them to user
  // TODO: implement old ready buffers dropping in get_free_buf (overflow)
  ret = mtlk_log_on_buf_ready(); // check ready queue
  if (ret != MTLK_ERR_OK)
    return (ret == MTLK_ERR_NO_MEM) ? -ENOMEM : -EFAULT;

  filp->private_data = &mtlk_log_dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
  ret = nonseekable_open(inode, filp);
  if (ret < 0)
    return ret;
#endif
  return 0;
}

static int
cdev_log_release(struct inode *inode, struct file *filp)
{
  //mtlk_cdev_t *dev = filp->private_data;

  // Remove filp from asynchronous notifications list
  cdev_log_fasync(-1, filp, 0);

  // FIXME: on_ready could potentially insert a new buffer entry to usp_queue
  // before device is released: change ldlist to dlist and add manual locking.
  // Currently as workaround we're calling usp_queue_release_entries
  // on cdev_log_open to eliminate this buffer entry.
  usp_queue_release_entries();

  // Allow other processes to open the device
  atomic_inc(&log_dev_available);

  return 0;
}

static ssize_t
cdev_log_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
  mtlk_usp_entry_t *usp_entry;
  long to_send;
  long not_copied;
  ssize_t written = 0;
  uint8 *copy_src;

next_entry:
  while (!(usp_entry = (mtlk_usp_entry_t *)
        mtlk_ldlist_pop_front(&log_data_osdep.usp_queue))) {
    if (written)
      return written;
    if (filp->f_flags & O_NONBLOCK)
      return -EAGAIN;

    if (wait_event_interruptible(log_data_osdep.in_wait_queue,
          !mtlk_ldlist_is_empty(&log_data_osdep.usp_queue)))
      return -ERESTARTSYS; // signal
  }

  // Now there is some data to send
  to_send = usp_entry->len - usp_entry->pos;
  if (!to_send) {
    release_usp_entry(usp_entry);
    goto next_entry;
  }

  if (to_send > count)
    to_send = count;
  
  switch (usp_entry->type) {
  case USP_ENTRY_CDATA:
    copy_src = usp_entry->u.pdata;
    break;
  case USP_ENTRY_READY_BUF:
    copy_src = _mtlk_log_buf_get_data_buffer(usp_entry->u.pbuf);
    break;
  default:
    ASSERT(0);
    // FIXME: group similar messages
    ELOG_V("Unknown data queue item type");
    return -EFAULT;
  }

  not_copied = copy_to_user(buf + written, copy_src + usp_entry->pos,
      to_send);
  if (not_copied != 0) {
    mtlk_ldlist_push_front(&log_data_osdep.usp_queue,
        (mtlk_ldlist_entry_t *) usp_entry);
    if (written)
      return written;
    else
      return -EFAULT;
  }
  written += to_send;
  count -= to_send;
  usp_entry->pos += to_send;

  if (usp_entry->pos == usp_entry->len) {
    // No more data to write in this entry, try the next one
    release_usp_entry(usp_entry);
    goto next_entry;
  } else {
    // No more space in userspace buffer
    ASSERT(count == 0);
    mtlk_ldlist_push_front(&log_data_osdep.usp_queue,
          (mtlk_ldlist_entry_t *) usp_entry);
  }
  return written;
}

static ssize_t
cdev_log_write(struct file *filp, const char __user *buf, size_t count,
                    loff_t *f_pos)
{
  return -EFAULT;
}

static unsigned int
cdev_log_poll(struct file *filp, poll_table *wait)
{
  unsigned int mask = 0;

  poll_wait(filp, &log_data_osdep.in_wait_queue, wait);
  if (!mtlk_ldlist_is_empty(&log_data_osdep.usp_queue)) {
    mask |= POLLIN | POLLRDNORM; // readable
  }
  return mask;
}

static int
cdev_log_fasync(int fd, struct file *filp, int mode)
{
  mtlk_cdev_t *dev = filp->private_data;

  return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static long 
cdev_on_logserv_ctrl (char __user *buf)
{
  long                res = -EINVAL;
  mtlk_log_ctrl_hdr_t hdr;
  char               *data = NULL;

  if (copy_from_user(&hdr, buf, sizeof(hdr)) != 0) {
    res = -EFAULT;
    goto end;
  }
  buf += sizeof(hdr);

  if (hdr.data_size) {
    data = (char *)kmalloc(hdr.data_size, GFP_KERNEL);
    if (!data) {
      res = -ENOMEM;
      goto end;
    }

    if (copy_from_user(data, buf, hdr.data_size) != 0) {
      res = -EFAULT;
      goto end;
    }
  }

  switch (hdr.id) {
  case MTLK_LOG_CTRL_ID_VERINFO:
    if (hdr.data_size == sizeof(mtlk_log_ctrl_ver_info_data_t)) {
      mtlk_log_ctrl_ver_info_data_t *ver_info = (mtlk_log_ctrl_ver_info_data_t *)data;

      ver_info->major = RTLOGGER_VER_MAJOR;
      ver_info->minor = RTLOGGER_VER_MINOR;
      res = 0;
    }
    break;
  default:
    res = -ENOTSUPP;
    break;
  }

  if (hdr.data_size &&
      copy_to_user(buf, data, hdr.data_size) != 0) {
      res = -EFAULT;
      goto end;
  }

end:
  if (data) {
    kfree(data);
  }

  return res;
}

static long 
cdev_on_ioctl (struct file  *filp,
               unsigned int  cmd, 
               unsigned long arg)
{
  int          res = -ENOTSUPP;
  char __user *buf = (char __user *)arg;

  switch (cmd) {
  case SIOCDEVPRIVATE:
    res = cdev_on_logserv_ctrl(buf);
    break;
  default:
    break;
  }

  return res;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
static long 
cdev_unlocked_ioctl (struct file  *filp, 
                     unsigned int  cmd, 
                     unsigned long arg)
{
  return cdev_on_ioctl(filp, cmd, arg);
}
#else
static int 
cdev_ioctl (struct inode *inode, 
            struct file  *filp, 
            unsigned int  cmd, 
            unsigned long arg)
{
  return (int)cdev_on_ioctl(filp, cmd, arg);
}
#endif

// TODO: make a pool of free usp_entries
static mtlk_usp_entry_t *
alloc_usp_entry(void)
{
  return mtlk_osal_mem_alloc(sizeof(mtlk_usp_entry_t), MTLK_MEM_TAG_LOGGER);
}

static void
release_usp_entry(mtlk_usp_entry_t *usp_entry)
{
  ASSERT(usp_entry);

  switch (usp_entry->type) {
  case USP_ENTRY_CDATA:
    // Pointer to constant data, do not free
    break;
  case USP_ENTRY_READY_BUF:
    mtlk_log_put_free_buf(usp_entry->u.pbuf);
    break;
  default:
    ASSERT(0);
    ELOG_V("Unknown data queue item type");
    break;
  }
  mtlk_osal_mem_free(usp_entry);
}

static void
usp_queue_release_entries(void)
{
  mtlk_usp_entry_t *usp_entry;
  while ((usp_entry = (mtlk_usp_entry_t *)
        mtlk_ldlist_pop_front(&log_data_osdep.usp_queue)) != NULL) {
    release_usp_entry(usp_entry);
  }
}

static int logger_debug_write(struct file *file, const char *buffer,
  size_t count, loff_t *data)
{
  char *debug_conf = NULL;
  int result = 0;

  debug_conf = kmalloc(count + 1, GFP_KERNEL);
  if (!debug_conf) {
    ELOG_D("Unable to allocate %lu bytes", count);
    result = -EFAULT;
    goto FINISH;
  }
  if (copy_from_user(debug_conf, buffer, count)) {
    result = -EFAULT;
    goto FINISH;
  }
  debug_conf[count] = '\0';
  mtlk_log_set_conf(debug_conf);
  result = count;
FINISH:
  if (debug_conf)
    kfree(debug_conf);
  return result;
}
static int logger_debug_read(struct file *filp,char *page,size_t count,loff_t *offp )
{
    char *p = page;

    p += mtlk_log_get_conf(p, count);
	*offp =1;
    return p - page;
}

static int logger_stat_read (struct file *filp,char *page,size_t count,loff_t *offp )
{
  int bytes_written = 0;
  
  mtlk_osal_lock_acquire(&log_data.data_lock);

  bytes_written += snprintf(page + bytes_written, count - bytes_written,
    "%10u\tLog packets processed successfully\n", log_data.log_pkt_reservations_succeeded);
  bytes_written += snprintf(page + bytes_written, count - bytes_written,
    "%10u\tLog packets failed to process\n", log_data.log_pkt_reservations_failed);
  bytes_written += snprintf(page + bytes_written, count - bytes_written,
    "%10u\tLog buffer allocations succeeded\n", log_data.log_buff_allocations_succeeded);
  bytes_written += snprintf(page + bytes_written, count - bytes_written,
    "%10u\tLog buffer allocations failed\n", log_data.log_buff_allocations_failed);

  mtlk_osal_lock_release(&log_data.data_lock);

  *offp =1;
  return bytes_written;
}

