/* $Id: mtlk_card_types.h 10358 2011-01-15 09:59:33Z dmytrof $ */

#ifndef __HW_MTLK_CARD_TYPES_H__
#define __HW_MTLK_CARD_TYPES_H__

typedef enum
{
  MTLK_CARD_UNKNOWN = 0x8888, /* Just magics to avoid    */
  MTLK_CARD_FIRST = 0xABCD,   /* accidental coincidences */

#ifdef MTCFG_LINDRV_HW_PCIE
  MTLK_CARD_PCIE,
#endif
#ifdef MTCFG_LINDRV_HW_PCIG3
  MTLK_CARD_PCIG3,
#endif
#ifdef MTCFG_LINDRV_HW_AHBG35
  MTLK_CARD_AHBG35,
#endif

  MTLK_CARD_LAST
} mtlk_card_type_t;


#endif /* __HW_MTLK_CARD_TYPES_H__ */
