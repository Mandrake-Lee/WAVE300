#include "mtlkinc.h"
#include "mtlk_algorithms.h"

void __MTLK_IFUNC
mtlk_sort_slist(void** list, mtlk_get_next_t func_get_next,
                mtlk_set_next_t func_set_next, mtlk_is_less_t func_is_less)
{
  /* This function implements generic insertion sort algorithm */
  /* for single-linked list abstraction.                       */

  void  *front_iterator, *prev_front_iterator, *tmp = NULL;

  if (!*list) return;

  prev_front_iterator = *list;
  front_iterator = func_get_next(prev_front_iterator);

  while(front_iterator) {
    void *rear_iterator = *list,
         *prev_rear_iterator = NULL;

    while(rear_iterator != front_iterator) {

      if( func_is_less(front_iterator, rear_iterator) ) {
        /* Put item pointed by front iterator before item */
        /* pointed by rear iterator                       */

        /* 1. Remove front_iterator from list             */
        func_set_next(prev_front_iterator,
                      func_get_next(front_iterator));
        /* 2. Put it to the new location                  */
        if(prev_rear_iterator) {
          tmp = func_get_next(prev_rear_iterator);
          func_set_next(prev_rear_iterator, front_iterator);
        }
        else {
          tmp = *list;
          *list = front_iterator;
        }
        func_set_next(front_iterator, tmp);

        break;
      }
      /* Advance rear iterator */
      prev_rear_iterator = rear_iterator;
      rear_iterator = func_get_next(rear_iterator);
    }
    /* Advance front iterator */
    prev_front_iterator = front_iterator;
    front_iterator = func_get_next(front_iterator);
  }
}
