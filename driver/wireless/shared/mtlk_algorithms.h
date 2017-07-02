#ifndef _MTLK_ALGORITHMS_H_
#define _MTLK_ALGORITHMS_H_

typedef void* (*mtlk_get_next_t)(void* item);
typedef void  (*mtlk_set_next_t)(void* item, void* next);
typedef int   (*mtlk_is_less_t)(void* item1, void* item2);

/*! 
  \fn      void* __MTLK_IFUNC mtlk_sort_slist(...)
  \brief   Sort abstract singly linked list.

  \param   list          Pointer to the first element of the list to be sorted
  \param   func_get_next Pointer to function that returns next element of the given element
  \param   func_set_next Pointer to function that sets next element for the given element
  \param   func_is_less  Pointer to compare function that returns non-zero if first given element less then second, 0 otherwise.

  \return  Pointer to new first element of the list

  \warning This routine assumes that given list is non-cyclic singly linked list, next of its last element is NULL
 */

void __MTLK_IFUNC
mtlk_sort_slist(void** list, mtlk_get_next_t func_get_next,
                mtlk_set_next_t func_set_next, mtlk_is_less_t func_is_less);

#endif //_MTLK_ALGORITHMS_H_
