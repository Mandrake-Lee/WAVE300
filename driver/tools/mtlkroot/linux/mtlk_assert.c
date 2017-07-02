#include "mtlkinc.h"
#include "mtlk_assert.h"

void __MTLK_IFUNC
__mtlk_assert (mtlk_slid_t slid)
{
  printk("Assertion failed " MTLK_SLID_FMT "\n",
         MTLK_SLID_ARGS(slid));
  BUG();
}

EXPORT_SYMBOL(__mtlk_assert);
