#include "mtlkinc.h"
#include "mtlkhash_ieee_addr.h"

#define LOG_LOCAL_GID   GID_MTLKHASH_IEEE_ADDR
#define LOG_LOCAL_FID   1

static __INLINE uint32
_mtlk_hash_ieee_addr_hashval (const IEEE_ADDR *key, uint32 nof_buckets)
{
  return ((key->au8Addr[5]) & (nof_buckets - 1));
}

static __INLINE int
_mtlk_hash_ieee_addr_keycmp (const IEEE_ADDR *key1, const IEEE_ADDR *key2)
{
  return mtlk_osal_compare_eth_addresses(key1->au8Addr, key2->au8Addr);
}

MTLK_HASH_DEFINE_EXTERN(ieee_addr, IEEE_ADDR, 
                        _mtlk_hash_ieee_addr_hashval, 
                        _mtlk_hash_ieee_addr_keycmp);

