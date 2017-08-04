/*
 * $Id: mtlk_df_proc_impl.c 11320 2011-06-29 12:47:41Z fleytman $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * DF UI API implementation for working with Linux Proc_FS
 *
 */


/********************************************************************
 * Includes
 ********************************************************************/
#include "mtlkinc.h"

#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "mtlk_df.h"

#include <linux/uidgid.h>

/********************************************************************
 * Local definitions
 ********************************************************************/
#define LOG_LOCAL_GID   GID_PROCAUX
#define LOG_LOCAL_FID   1


#define MTLK_PROCFS_UID         0
#define MTLK_PROCFS_GID         0

/***
 * Linux OS related definitions
 ***/
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#define PROC_NET init_net.proc_net
static inline struct proc_dir_entry *PDE(const struct inode *inode)
{
  return inode->u.generic_ip;
}
#else
#define PROC_NET init_net.proc_net

#endif

/********************************************************************
 * Local types definitions
 ********************************************************************/

/* structure to hold seq_ops near mtlk_core_t pointer
 * so we can get it from seq_ functions
 */
struct mtlk_seq_ops {
  mtlk_dlist_entry_t      list_entry;
  void                    *df;
  struct semaphore        sem;
  struct seq_operations   seq_ops;
};

struct _mtlk_df_proc_fs_node_t {
  /* parent node */
  struct _mtlk_df_proc_fs_node_t *parent;

  /* our dir */
  struct proc_dir_entry *dir;

  /* our name */
  const uint8* name;

  /* list of structs with seq_ops needed for some proc entries.*/
  mtlk_dlist_t seq_ops_list;

  uint16 num_entries;
};

/********************************************************************
 * Functions implementation
 *********************************************************************/

mtlk_df_proc_fs_node_t* __MTLK_IFUNC
mtlk_df_proc_node_create(const uint8 *name, mtlk_df_proc_fs_node_t* parent)
{
  mtlk_df_proc_fs_node_t *proc_node;

  MTLK_ASSERT(NULL != name);

  /* allocate node */
  proc_node = mtlk_osal_mem_alloc(sizeof(*proc_node), MTLK_MEM_TAG_PROC);
  if (NULL == proc_node){
    ELOG_V("Cannot allocate memory for mtlk_df_proc_fs_node_t structure.");
    goto alloc_err;
  }

  memset(proc_node, 0, sizeof(*proc_node));

  proc_node->name = name;

  /* if parent node is NULL */
  if (NULL == parent) {
    /*- create root node (usually /proc/net/ */
    proc_node->dir = PROC_NET;
  } else {
    /*- create new node (usually /proc/net/... */
    proc_node->parent = parent;
    proc_node->dir = proc_mkdir(name, proc_node->parent->dir);
    if (NULL == proc_node->dir) {
      goto create_err;
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,00)
    proc_node->dir->uid.val = MTLK_PROCFS_UID;
    proc_node->dir->gid.val = MTLK_PROCFS_GID;
#else
	proc_set_user(proc_node->dir,KUIDT_INIT(MTLK_PROCFS_UID),KGIDT_INIT(MTLK_PROCFS_GID));
#endif
  }

  /* initialize list of seq_ops structures */
  mtlk_dlist_init(&proc_node->seq_ops_list);

  if (NULL == parent) {
    ILOG1_S("Proc node %s/ created", proc_node->name);
  } else {
    ILOG1_SS("Proc node %s/%s created", proc_node->parent->name, proc_node->name);
  }

  return proc_node;

create_err:
  /* free node */
  mtlk_osal_mem_free(proc_node);
alloc_err:
  ELOG_S("Failed to create proc node %s", name);
  return NULL;
}

void __MTLK_IFUNC
mtlk_df_proc_node_delete(mtlk_df_proc_fs_node_t* proc_node)
{
  mtlk_dlist_entry_t* entry;
  struct mtlk_seq_ops* seq_ops;

  /*check node entries - all entries should be removed */
  if (0 != proc_node->num_entries) {
    WLOG_D("Some (%u) Proc entries are not deleted", proc_node->num_entries);
    MTLK_ASSERT(0);
    return;
  }

  if (NULL != proc_node->parent) {
    /* it's not root node - remove Proc_Fs entry */
    remove_proc_entry(proc_node->name, proc_node->parent->dir);
  }

  if (NULL == proc_node->parent) {
    ILOG1_S("Proc node %s/ deleted", proc_node->name);
  } else {
    ILOG1_SS("Proc node %s/%s deleted", proc_node->parent->name, proc_node->name);
  }

  /* freeing all dynamically allocated mtlk_seq_ops
   * structures needed for some proc entries.
   */
  while( NULL != (entry = mtlk_dlist_pop_back(&proc_node->seq_ops_list)) )
  {
    seq_ops = MTLK_LIST_GET_CONTAINING_RECORD(
                  entry, struct mtlk_seq_ops, list_entry);

    mtlk_osal_mem_free(seq_ops);
  }

  mtlk_dlist_cleanup(&proc_node->seq_ops_list);

  /* free node */
  mtlk_osal_mem_free(proc_node);
}

/***
 * Simple Proc_Fs files API implementation
 *
 ***/
static int
_mtlk_df_proc_node_add_entry(char *name,
                            mtlk_df_proc_fs_node_t *parent_node,
                            void *df,
                            mode_t mode,
                            mtlk_df_proc_entry_read_f  read_func,
                            mtlk_df_proc_entry_write_f write_func)
{
  struct proc_dir_entry *pde;
  struct file_operations *mtlk_file_ops;

  MTLK_ASSERT(NULL != parent_node);
  MTLK_ASSERT(NULL != name);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,00)
  pde = create_proc_entry(name, mode, parent_node->dir);
#else
  mtlk_file_ops = kmalloc (sizeof(struct file_operations),GFP_KERNEL);
  mtlk_file_ops->read = read_func;
  mtlk_file_ops->write = write_func;

  pde = proc_create_data(name, mode, parent_node->dir,mtlk_file_ops,df);

#endif


  if (NULL == pde) {
    goto pde_err;
  };
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,00)
  pde->uid.val = MTLK_PROCFS_UID;
  pde->gid.val = MTLK_PROCFS_GID;
  pde->read_proc = read_func;
  pde->write_proc = write_func;
  pde->data = df;
  /* set the .owner field, so that reference count of the
   * module increment when this file is opened
   */
  pde->owner = THIS_MODULE;
#else
  proc_set_user(pde,KUIDT_INIT(MTLK_PROCFS_UID),KGIDT_INIT(MTLK_PROCFS_GID));
#endif

  parent_node->num_entries++;

  ILOG1_SS("Proc entry %s/%s created", parent_node->name, name);

  return MTLK_ERR_OK;

pde_err:
  ELOG_S("Failed to create proc entry for %s", name);
  return MTLK_ERR_UNKNOWN;
}

int __MTLK_IFUNC
mtlk_df_proc_node_add_ro_entry(char *name,
                               mtlk_df_proc_fs_node_t *parent_node,
                               void *df,
                               mtlk_df_proc_entry_read_f  read_func)
{
  return _mtlk_df_proc_node_add_entry(
            name, parent_node, df,
            S_IFREG|S_IRUSR|S_IRGRP,
            read_func, NULL);
}

int  __MTLK_IFUNC
mtlk_df_proc_node_add_wo_entry(char *name,
                               mtlk_df_proc_fs_node_t *parent_node,
                               void *df,
                               mtlk_df_proc_entry_write_f write_func)
{
  return _mtlk_df_proc_node_add_entry(
            name, parent_node, df,
            S_IFREG|S_IWUSR,
            NULL, write_func);
}

int  __MTLK_IFUNC
mtlk_df_proc_node_add_rw_entry(char *name,
                               mtlk_df_proc_fs_node_t *parent_node,
                               void *df,
                               mtlk_df_proc_entry_read_f  read_func,
                               mtlk_df_proc_entry_write_f write_func)
{
  return _mtlk_df_proc_node_add_entry(
            name, parent_node, df,
            S_IFREG|S_IRUGO|S_IWUSR, /*Read all, change only root*/
            read_func, write_func);
}

/***
 * SEQ Proc_Fs files API implementation
 ***/

/* these three functions are used to "emulate" simplified
 * seq_file methods (single_*). we can't use it because
 * we have older kernels (e.g. 2.4.20 on cavium) which
 * lack this functionality.
 */
void*
_mtlk_df_proc_seq_entry_start_ops(struct seq_file *s, loff_t *pos)
{
  /* this function must return non-NULL pointer
   * to start new sequence, so the address of this
   * variable is used. just don't want to return
   * pointer to "something".
   */
  static unsigned long counter = 0;

  /* beginning a new sequence ? */
  if ( *pos == 0 )
    return &counter;
  else {
    /* no => it's the end of the sequence, return NULL to stop reading */
    *pos = 0;
    return NULL;
  }
}

void*
_mtlk_df_proc_seq_entry_next_ops(struct seq_file *s, void *v, loff_t *pos)
{
  /* increase position, so the next invokation of
   * .seq_start will return NULL.
   */
  (*pos)++;

  /* return NULL to stop the current sequence */
  return NULL;
}

void
_mtlk_df_proc_seq_entry_stop_ops(struct seq_file *s, void *v)
{
  /* do nothing here */
  return;
}

/* these two functions are registered as .open and .release
 * methods of the file_operations structure used by all
 * "one-function" seq files.
 */
static int
_mtlk_df_proc_seq_entry_open_ops(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
  struct mtlk_seq_ops *ops = (PDE(inode))->data;
#else
  struct mtlk_seq_ops *ops = PDE_DATA(inode);
#endif
  /* try to get semaphore without sleep first */
  if(down_trylock(&ops->sem)) {
    if(file->f_flags & O_NONBLOCK)
      return -EAGAIN;
    /* ok, we'll have to sleep if it's still not free */
    if(down_interruptible(&ops->sem))
      return -EINTR;
  };

  /* we've got semaphore, so proceed */
  return seq_open(file, &ops->seq_ops);
}

static int
_mtlk_df_proc_seq_entry_release_ops(struct inode *inode, struct file *file)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
  struct mtlk_seq_ops *ops = (PDE(inode))->data;
#else
  struct mtlk_seq_ops *ops = PDE_DATA(inode);
#endif
  /* release semaphore acquired in the .open function */
  up(&ops->sem);
  return seq_release(inode, file);
}

/* this is the common structure used for all one
 * function seq_file files. open method gets per
 * file semaphore and calls seq_open, release
 * method releases per-file semaphore and calls
 * seq_release.
 */
static struct file_operations _mtlk_df_proc_seq_entry_fops = {
  .owner   = THIS_MODULE,
  .open    = _mtlk_df_proc_seq_entry_open_ops,
  .read    = seq_read,
  .llseek  = seq_lseek,
  .release = _mtlk_df_proc_seq_entry_release_ops
};


int __MTLK_IFUNC
mtlk_df_proc_node_add_seq_entry(char *name,
                                 mtlk_df_proc_fs_node_t *parent_node,
                                 void *df,
                                 struct seq_operations *seq_ops)
{
  struct mtlk_seq_ops *tmp_seq_ops;
  struct proc_dir_entry *pde;

  MTLK_ASSERT(NULL != parent_node);
  MTLK_ASSERT(NULL != name);

  tmp_seq_ops = mtlk_osal_mem_alloc(sizeof(*tmp_seq_ops), MTLK_MEM_TAG_PROC);
  if (NULL == tmp_seq_ops){
    ELOG_V("Cannot allocate memory for mtlk_seq_ops structure.");
    goto alloc_err;
  }
  memset(tmp_seq_ops, 0, sizeof(*tmp_seq_ops));

  /* initialize newly allocated mtlk_seq_ops structure */
  sema_init(&tmp_seq_ops->sem, 1);
  tmp_seq_ops->df = df;
  tmp_seq_ops->seq_ops = *seq_ops;


  /* create and init proc entry */
  pde = proc_create_data(name, S_IFREG|S_IRUSR|S_IRGRP|S_IROTH, parent_node->dir,&_mtlk_df_proc_seq_entry_fops,tmp_seq_ops);
  if (NULL == pde) {
    goto pde_err;
  };
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,00)
  pde->uid.val = MTLK_PROCFS_UID;
  pde->gid.val = MTLK_PROCFS_GID;
  pde->data = tmp_seq_ops;
#else
	proc_set_user(pde,KUIDT_INIT(MTLK_PROCFS_UID),KGIDT_INIT(MTLK_PROCFS_GID));
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
  /* set the .owner field, so that reference count of the
   * module increment when this file is opened
   */
  pde->owner = THIS_MODULE;
#endif

  /* adding allocated structure to the list, so it
   * can be easily tracked and freed on cleanup
   */
  mtlk_dlist_push_back(&parent_node->seq_ops_list, &tmp_seq_ops->list_entry);

  parent_node->num_entries++;

  ILOG1_SS("Proc entry %s/%s created", parent_node->name, name);

  return MTLK_ERR_OK;

pde_err:
  mtlk_osal_mem_free(tmp_seq_ops);

alloc_err:
  ELOG_S("Failed to create proc entry for %s", name);
  return MTLK_ERR_UNKNOWN;
}

void __MTLK_IFUNC
mtlk_df_proc_node_remove_entry(char *name,
                               mtlk_df_proc_fs_node_t *parent_node)
{
  remove_proc_entry(name, parent_node->dir);

  if (0 < parent_node->num_entries) {
    parent_node->num_entries--;
  } else {
    WLOG_S("Some entries in %s have been deleted more then once", parent_node->name);
  }

  ILOG1_SS("Proc entry %s/%s deleted", parent_node->name, name);
}

void* __MTLK_IFUNC
mtlk_df_proc_seq_entry_get_df(mtlk_seq_entry_t *seq_ctx)
{
  return (container_of(seq_ctx->op, struct mtlk_seq_ops, seq_ops))->df;
}
