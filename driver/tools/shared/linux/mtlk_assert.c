#include "mtlkinc.h"
#include <assert.h>

void __MTLK_IFUNC
__mtlk_assert (mtlk_slid_t slid)
{
  printf("Assertion failed " MTLK_SLID_FMT "\n",
         MTLK_SLID_ARGS(slid));
  assert(0);
}

