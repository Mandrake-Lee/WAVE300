/* $Id: mtlk_card_selector.h 10358 2011-01-15 09:59:33Z dmytrof $ */

#ifndef __HW_MTLK_CARD_SELECTOR_H__
#define __HW_MTLK_CARD_SELECTOR_H__

#ifdef MTLK_DEBUG

static __INLINE BOOL
_known_card_type(mtlk_card_type_t x)
{
  return (x > MTLK_CARD_FIRST) && (x < MTLK_CARD_LAST);
}

#endif // MTLK_DEBUG

#define CARD_SELECTOR_START(hw_type) \
  for(;;) { \
    MTLK_ASSERT(_known_card_type(hw_type)); \
    switch(hw_type) \
    { \
    default: \
      MTLK_ASSERT(!"Actions for all known hw types to be explicitly provided in selector");

#define CARD_SELECTOR_END() \
    } \
    break; \
  }

#ifdef MTCFG_LINDRV_HW_PCIE
#   define IF_CARD_PCIE(op) \
      case MTLK_CARD_PCIE: {op;} break
#else
#   define IF_CARD_PCIE(op)
#endif

#ifdef MTCFG_LINDRV_HW_PCIG3
#   define IF_CARD_PCIG3(op) \
      case MTLK_CARD_PCIG3: {op;} break
#else
#   define IF_CARD_PCIG3(op)
#endif

#ifdef MTCFG_LINDRV_HW_AHBG35
#   define IF_CARD_AHBG35(op) \
      case MTLK_CARD_AHBG35: {op;} break
#else
#   define IF_CARD_AHBG35(op) 
#endif


#if defined(MTCFG_LINDRV_HW_PCIE) || defined (MTCFG_LINDRV_HW_PCIG3)
#  ifdef MTCFG_LINDRV_HW_PCIG3
#   define __CASE_PCIG3 case MTLK_CARD_PCIG3:
#  else
#   define __CASE_PCIG3
#  endif

#  ifdef MTCFG_LINDRV_HW_PCIE
#   define __CASE_PCIE case MTLK_CARD_PCIE:
#  else
#   define __CASE_PCIE
#  endif

#   define IF_CARD_G3(op)   \
      __CASE_PCIG3          \
      __CASE_PCIE           \
          {op;} break
#else
#   define IF_CARD_G3(op)
#endif

#endif /* __HW_MTLK_CARD_SELECTOR_H__ */
