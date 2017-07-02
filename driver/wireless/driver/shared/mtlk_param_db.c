#include "mtlkinc.h"
#include "mtlk_param_db.h"

#define LOG_LOCAL_GID  GID_PDB
#define LOG_LOCAL_FID   1

struct _mtlk_pdb_t
{
  mtlk_pdb_value_t storage[PARAM_DB_LAST_VALUE_ID];

  /* sync objects */
  mtlk_osal_spinlock_t db_lock;
  mtlk_atomic_t usage_ref_cnt;

  MTLK_DECLARE_INIT_STATUS;
};


MTLK_INIT_STEPS_LIST_BEGIN(param_db)
  MTLK_INIT_STEPS_LIST_ENTRY(param_db, PARAM_DB_PREPARE_STRUCTURE)
  MTLK_INIT_STEPS_LIST_ENTRY(param_db, PARAM_DB_SET_UNINITIALIZED)
  MTLK_INIT_STEPS_LIST_ENTRY(param_db, PARAM_DB_PREPARE_SYNCHRONIZATION)
MTLK_INIT_INNER_STEPS_BEGIN(param_db)
MTLK_INIT_STEPS_LIST_END(param_db);


static void _mtlk_pdb_init_param(mtlk_pdb_value_t * obj, uint32 flags, mtlk_pdb_t* parent) {
  obj->flags = flags;
  obj->parent = parent;
}

static int _mtlk_pdb_prepare_structure (mtlk_pdb_t* obj) {

  MTLK_UNREFERENCED_PARAM(obj);
  /* Allocate data strucutre */

    return MTLK_ERR_OK;
}

static __INLINE void _mtlk_pdb_incref(mtlk_pdb_t* obj) {
  mtlk_osal_atomic_inc(&obj->usage_ref_cnt);
}

static __INLINE void _mtlk_pdb_decref(mtlk_pdb_t* obj) {
  MTLK_ASSERT(mtlk_osal_atomic_get(&obj->usage_ref_cnt) != 0);

  mtlk_osal_atomic_dec(&obj->usage_ref_cnt);
}

static int _mtlk_pdb_destroy_stored_params (mtlk_pdb_t * obj) {
  int i;

  for(i = 0; i < PARAM_DB_LAST_VALUE_ID; i++) {

    if((PARAM_DB_TYPE_STRING == obj->storage[i].type ||
        PARAM_DB_TYPE_BINARY == obj->storage[i].type ||
        PARAM_DB_TYPE_MAC == obj->storage[i].type) ) {

      if (NULL != obj->storage[i].value.value_ptr) {
        mtlk_osal_mem_free(obj->storage[i].value.value_ptr);
        obj->storage[i].value.value_ptr = NULL;
      }

      mtlk_osal_lock_cleanup(&obj->storage[i].param_lock);
    }

    obj->storage[i].flags = PARAM_DB_VALUE_FLAG_UNINITIALIZED;
  }

  return MTLK_ERR_OK;
}

static int _mtlk_pdb_fill_stored_params (mtlk_pdb_t * obj) {
  int i;
  mtlk_pdb_initial_value *initial_values;
  int result = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != obj);

  for(i = 0; i < PARAM_DB_LAST_VALUE_ID; i++) {
    _mtlk_pdb_init_param(&obj->storage[i], PARAM_DB_VALUE_FLAG_UNINITIALIZED, obj);
  }

  i = 0;

  while((initial_values = mtlk_pdb_initial_values[i++]) != NULL) {
    do {
      MTLK_ASSERT(initial_values->id < PARAM_DB_LAST_VALUE_ID);

      obj->storage[initial_values->id].type = initial_values->type;
      obj->storage[initial_values->id].flags = initial_values->flag;
      obj->storage[initial_values->id].size = initial_values->size;

      ILOG6_DDDDP("id %d, type 0x%x, flag 0x%x, size %d, ptr %p", 
                  initial_values->id,
                  initial_values->type,
                  initial_values->flag,
                  initial_values->size,
                  initial_values->value);


      switch(initial_values->type) {
        case PARAM_DB_TYPE_INT:
            mtlk_osal_atomic_set(&obj->storage[initial_values->id].value.value_int,
                                 *((uint32 *) (initial_values->value)));

            ILOG6_D("value %d", *((uint32 *) (initial_values->value)));
          break;

        case PARAM_DB_TYPE_STRING:
            /*It's recommended to initialize STRING parameters
             * using maximum possible string length value + 1 (for zero)*/
            obj->storage[initial_values->id].value.value_ptr =
                mtlk_osal_mem_alloc(initial_values->size, MTLK_MEM_TAG_PARAM_DB);

            if(!obj->storage[initial_values->id].value.value_ptr) {
              result = MTLK_ERR_NO_MEM;
              goto end;
            }

            ILOG6_S("value %s", (char*)initial_values->value);

            strncpy(obj->storage[initial_values->id].value.value_ptr,
                    (char*)initial_values->value,
                    (initial_values->size - 1));

            mtlk_osal_lock_init(&obj->storage[initial_values->id].param_lock);
          break;

        case PARAM_DB_TYPE_BINARY:
        case PARAM_DB_TYPE_MAC:
            obj->storage[initial_values->id].value.value_ptr = mtlk_osal_mem_alloc(initial_values->size, MTLK_MEM_TAG_PARAM_DB);

            if(!obj->storage[initial_values->id].value.value_ptr) {
              result = MTLK_ERR_NO_MEM;
              goto end;
            }

            memcpy(obj->storage[initial_values->id].value.value_ptr,
                   initial_values->value,
                   initial_values->size);

            mtlk_osal_lock_init(&obj->storage[initial_values->id].param_lock);
          break;

        default:
          MTLK_ASSERT(!"Invalid type");
          break;
      }
    }
    while(++initial_values && initial_values->id != PARAM_DB_LAST_VALUE_ID);
  }

end:
  if(MTLK_ERR_OK != result) {
    _mtlk_pdb_destroy_stored_params(obj);
  }

  return result;
}

static int _mtlk_pdb_prepare_sync (mtlk_pdb_t * obj) {

  mtlk_osal_atomic_set(&obj->usage_ref_cnt, 0);
  mtlk_osal_lock_init(&obj->db_lock);

  return MTLK_ERR_OK;
}

static int _mtlk_pdb_destroy_structure (mtlk_pdb_t * obj) {
  MTLK_UNREFERENCED_PARAM(obj);

  return MTLK_ERR_OK;
}

static int _mtlk_pdb_free_sync (mtlk_pdb_t * obj) {
  /* For debugging purposes we can check if there are non 0 reference counters */
  MTLK_ASSERT(mtlk_osal_atomic_get(&obj->usage_ref_cnt) == 0);

  mtlk_osal_lock_cleanup(&obj->db_lock);

  return MTLK_ERR_OK;
}

static void __MTLK_IFUNC 
_mtlk_pdb_cleanup(mtlk_pdb_t* obj) {
  ILOG4_V(">>");
  MTLK_ASSERT(NULL != obj);

  MTLK_CLEANUP_BEGIN(param_db, MTLK_OBJ_PTR(obj))
    MTLK_CLEANUP_STEP(param_db, PARAM_DB_PREPARE_SYNCHRONIZATION, MTLK_OBJ_PTR(obj), 
                        _mtlk_pdb_free_sync, (obj));
    MTLK_CLEANUP_STEP(param_db, PARAM_DB_SET_UNINITIALIZED, MTLK_OBJ_PTR(obj), 
                        _mtlk_pdb_destroy_stored_params, (obj));
    MTLK_CLEANUP_STEP(param_db, PARAM_DB_PREPARE_STRUCTURE, MTLK_OBJ_PTR(obj), 
                        _mtlk_pdb_destroy_structure, (obj));
  MTLK_CLEANUP_END(param_db, MTLK_OBJ_PTR(obj));
}

static int __MTLK_IFUNC
_mtlk_pdb_init(mtlk_pdb_t* obj) {
  ILOG4_V(">>");

  MTLK_ASSERT(NULL != obj);

  MTLK_INIT_TRY(param_db, MTLK_OBJ_PTR(obj))
    MTLK_INIT_STEP(param_db, PARAM_DB_PREPARE_STRUCTURE, MTLK_OBJ_PTR(obj),
                   _mtlk_pdb_prepare_structure, (obj));
    MTLK_INIT_STEP(param_db, PARAM_DB_SET_UNINITIALIZED, MTLK_OBJ_PTR(obj),
                   _mtlk_pdb_fill_stored_params, (obj));
    MTLK_INIT_STEP(param_db, PARAM_DB_PREPARE_SYNCHRONIZATION, MTLK_OBJ_PTR(obj),
                   _mtlk_pdb_prepare_sync, (obj));

  MTLK_INIT_FINALLY(param_db, MTLK_OBJ_PTR(obj))
  MTLK_INIT_RETURN(param_db, MTLK_OBJ_PTR(obj), _mtlk_pdb_cleanup, (obj))
}

void __MTLK_IFUNC
mtlk_pdb_delete (mtlk_pdb_t *obj) 
{
  ILOG4_V(">>");
  _mtlk_pdb_cleanup(obj);
  mtlk_osal_mem_free(obj);
}

mtlk_pdb_t * __MTLK_IFUNC 
mtlk_pdb_create() {
  mtlk_pdb_t *param_db;
  ILOG4_V(">>");

  param_db = (mtlk_pdb_t *)mtlk_osal_mem_alloc(sizeof(mtlk_pdb_t), MTLK_MEM_TAG_PARAM_DB);
  if(NULL == param_db) {
    return NULL;
  }

  memset(param_db, 0, sizeof(mtlk_pdb_t));

  if (MTLK_ERR_OK != _mtlk_pdb_init(param_db)) {
    mtlk_osal_mem_free(param_db);
    return NULL;
  }

  return param_db;
}

mtlk_pdb_handle_t __MTLK_IFUNC 
mtlk_pdb_open(mtlk_pdb_t* obj, mtlk_pdb_id_t id) {
  
  mtlk_pdb_handle_t handle = NULL;

  ILOG4_V(">>");

  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(id < PARAM_DB_LAST_VALUE_ID);


  mtlk_osal_lock_acquire(&obj->db_lock);
  _mtlk_pdb_incref(obj);
  handle = &obj->storage[id];
  mtlk_osal_lock_release(&obj->db_lock);
  ILOG3_D("<< open fast handle id(%u) ", id);

  return handle;
}

void __MTLK_IFUNC 
mtlk_pdb_close(mtlk_pdb_handle_t handle) {
  ILOG4_V(">>");
  /* Perform debug\statistics - for example:
     if we keep per value reference counts, check them here */
  mtlk_osal_lock_acquire(&handle->parent->db_lock);
  _mtlk_pdb_decref(handle->parent);
  mtlk_osal_lock_release(&handle->parent->db_lock);

  ILOG3_V("<< close fast handle");
}

int __MTLK_IFUNC 
mtlk_pdb_get_int(mtlk_pdb_t* obj, mtlk_pdb_id_t id)
{
  int value;
  mtlk_pdb_handle_t handle;

  ILOG4_V(">>");

  handle = mtlk_pdb_open(obj, id);
  value = mtlk_pdb_fast_get_int(handle);
  mtlk_pdb_close(handle);

  ILOG3_DD("<< get param id(%u) value(%d)", id, value);
  return value;
}

int __MTLK_IFUNC 
mtlk_pdb_get_string(mtlk_pdb_t* obj, mtlk_pdb_id_t id, char * value, mtlk_pdb_size_t * size) {

  int result = MTLK_ERR_OK;
  mtlk_pdb_handle_t handle;

  ILOG4_V(">>");

  handle = mtlk_pdb_open(obj, id);
  result = mtlk_pdb_fast_get_string(handle, value, size);
  mtlk_pdb_close(handle);

  ILOG3_D("<< exit code - %d", result);
  return result;
}

int __MTLK_IFUNC 
mtlk_pdb_get_binary(mtlk_pdb_t* obj, mtlk_pdb_id_t id, void * buffer, mtlk_pdb_size_t * size) {

  int result = MTLK_ERR_OK;
  mtlk_pdb_handle_t handle;

  ILOG4_V(">>");

  handle = mtlk_pdb_open(obj, id);
  if(handle) {
    result = mtlk_pdb_fast_get_binary(handle, buffer, size);
    mtlk_pdb_close(handle);
  }

  ILOG4_D("<< exit code - %d", result);
  return result;
}

void __MTLK_IFUNC
mtlk_pdb_get_mac(mtlk_pdb_t* obj, mtlk_pdb_id_t id, void * mac)
{
  mtlk_pdb_handle_t handle;

  ILOG4_V(">>");

  handle = mtlk_pdb_open(obj, id);
  mtlk_pdb_fast_get_mac(handle, mac);
  mtlk_pdb_close(handle);

  ILOG3_DY("<< get MAC param: id(%u) MAC(%Y) ", id, mac);
}

void __MTLK_IFUNC
mtlk_pdb_set_int(mtlk_pdb_t* obj, mtlk_pdb_id_t id, uint32 value) {

  mtlk_pdb_handle_t handle;

  ILOG3_V(">>");

  handle = mtlk_pdb_open(obj, id);
  mtlk_pdb_fast_set_int(handle, value);
  mtlk_pdb_close(handle);

  ILOG1_DD("<< set param: id(%u) value(%d)", id, value);
}

int __MTLK_IFUNC 
mtlk_pdb_set_string(mtlk_pdb_t* obj, mtlk_pdb_id_t id, const char * value)
{
  int result = MTLK_ERR_OK;
  mtlk_pdb_handle_t handle;

  ILOG4_V(">>");

  handle = mtlk_pdb_open(obj, id);
  result = mtlk_pdb_fast_set_string(handle, value);
  mtlk_pdb_close(handle);

  ILOG1_DDS("<< set string param: res(%d) id(%u) val(%s)", result, id, value);
  return result;
}

int __MTLK_IFUNC 
mtlk_pdb_set_binary(mtlk_pdb_t* obj, mtlk_pdb_id_t id, const void * buffer, mtlk_pdb_size_t size) {
  int result = MTLK_ERR_OK;
  mtlk_pdb_handle_t handle;

  ILOG4_V(">>");

  handle = mtlk_pdb_open(obj, id);
  if(handle) {
    result = mtlk_pdb_fast_set_binary(handle, buffer, size);
    mtlk_pdb_close(handle);
  }

  ILOG1_DD("<< set binary param: res(%d) id(%u) ", result, id);

  return result;
}

void __MTLK_IFUNC
mtlk_pdb_set_mac(mtlk_pdb_t* obj, mtlk_pdb_id_t id, const void *mac)
{
  mtlk_pdb_handle_t handle;

  ILOG4_V(">>");

  handle = mtlk_pdb_open(obj, id);
  mtlk_pdb_fast_set_mac(handle, mac);
  mtlk_pdb_close(handle);

  ILOG1_DY("<< set MAC param: id(%u) MAC(%Y) ", id, mac);
}

#ifndef MTLK_PDB_UNIT_TEST

int __MTLK_IFUNC 
mtlk_pdb_unit_test(mtlk_pdb_t* obj) {
  return MTLK_ERR_OK;
}

#else

int __MTLK_IFUNC 
mtlk_pdb_unit_test(mtlk_pdb_t* obj) {
  int value;
  char buffer[256];
  int test_size = 256;
  int return_code;

  /* testing mtlk_pdb_get_int */
  return_code = mtlk_pdb_get_int(obj, PARAM_DB_MODULE_A_TEST_INT1, &value);
  ILOG0_DDD("ID %d, value %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_INT1, value, return_code);

  return_code = mtlk_pdb_get_int(obj, PARAM_DB_MODULE_A_TEST_INT2, &value);
  ILOG0_DDD("ID %d, value %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_INT1, value, return_code);

  return_code = mtlk_pdb_get_int(obj, PARAM_DB_MODULE_B_TEST_INT1, &value);
  ILOG0_DDD("ID %d, value %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_INT1, value, return_code);



  /* error cases - should return error codes or cause assertions*/
  /* Tested
  return_code = mtlk_pdb_get_int(obj, PARAM_DB_LAST_VALUE_ID + 666, &value);
  ILOG0_DDD("ID %d, value %d, ret_code = %d", PARAM_DB_LAST_VALUE_ID + 666, value, return_code);

  return_code = mtlk_pdb_get_int(NULL, PARAM_DB_MODULE_A_TEST_INT1, &value);
  ILOG0_DDD("ID %d, value %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_INT1, value, return_code);

  return_code = mtlk_pdb_get_int(obj, PARAM_DB_MODULE_A_TEST_INT1, NULL);
  ILOG0_DDD("ID %d, value %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_INT1, value, return_code);
  
  return_code = mtlk_pdb_get_int(obj, PARAM_DB_MODULE_A_TEST_BINARY, &value);
  ILOG0_DDD("ID %d, value %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_INT1, value, return_code);

  return_code = mtlk_pdb_get_int(obj, PARAM_DB_MODULE_A_TEST_STRING, &value);
  ILOG0_DDD("ID %d, value %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_INT1, value, return_code);
  */
  /* end testing mtlk_pdb_get_int */


  /* testing mtlk_pdb_get_string */
  return_code = mtlk_pdb_get_string(obj, PARAM_DB_MODULE_A_TEST_STRING, buffer, &test_size);
  ILOG0_DDSD("ID %d, size %d, value %s, ret_code = %d", PARAM_DB_MODULE_A_TEST_STRING, test_size, buffer, return_code);

  return_code = mtlk_pdb_get_string(obj, PARAM_DB_MODULE_B_TEST_STRING, buffer, &test_size);
  ILOG0_DDSD("ID %d, size %d, value %s, ret_code = %d", PARAM_DB_MODULE_B_TEST_STRING, test_size, buffer, return_code);

  test_size = 4;
  return_code = mtlk_pdb_get_string(obj, PARAM_DB_MODULE_B_TEST_STRING, buffer, &test_size);
  ILOG0_DDSD("ID %d, size %d, value %s, ret_code = %d", PARAM_DB_MODULE_B_TEST_STRING, test_size, buffer, return_code);
  
  test_size = 256;
  return_code = mtlk_pdb_get_string(obj, PARAM_DB_MODULE_B_TEST_STRING, buffer, &test_size);
  ILOG0_DDSD("ID %d, size %d, value %s, ret_code = %d", PARAM_DB_MODULE_B_TEST_STRING, test_size, buffer, return_code);

      /* error cases - should return error codes or cause assertions*/
  /* end testing mtlk_pdb_get_string */


  /* testing mtlk_pdb_get_binary */
  test_size = 256;
  return_code = mtlk_pdb_get_binary(obj, PARAM_DB_MODULE_A_TEST_BINARY, buffer, &test_size);
  ILOG0_DDD("ID %d, size %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_BINARY, test_size, return_code);
  for(value = 0; value < test_size; value++) {
      ILOG0_DD("binary[%d] = %d", value, buffer[value]);
  }
      /* error cases - should return error codes or cause assertions*/
  /* end testing mtlk_pdb_get_binary */

  /* testing mtlk_pdb_set_int */
  return_code = mtlk_pdb_set_int(obj, PARAM_DB_MODULE_A_TEST_INT1, 1234);
  ILOG0_DD("ID %d, ret_code after set = %d", PARAM_DB_MODULE_A_TEST_INT1, return_code);
  return_code = mtlk_pdb_get_int(obj, PARAM_DB_MODULE_A_TEST_INT1, &value);
  ILOG0_DDD("ID %d, value %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_INT1, value, return_code);

  return_code = mtlk_pdb_set_int(obj, PARAM_DB_MODULE_A_TEST_INT1, 0x1000);
  ILOG0_DD("ID %d, ret_code after set = %d", PARAM_DB_MODULE_A_TEST_INT1, return_code);
  return_code = mtlk_pdb_get_int(obj, PARAM_DB_MODULE_A_TEST_INT1, &value);
  ILOG0_DDD("ID %d, value %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_INT1, value, return_code);
      /* error cases - should return error codes or cause assertions*/
  /* end testing mtlk_pdb_set_int */

  /* testing mtlk_pdb_set_string */
  return_code = mtlk_pdb_set_string(obj, PARAM_DB_MODULE_B_TEST_STRING, "Good bye12!");
  ILOG0_DD("ID %d, ret_code after set = %d", PARAM_DB_MODULE_B_TEST_STRING, return_code);
  test_size = 256;
  return_code = mtlk_pdb_get_string(obj, PARAM_DB_MODULE_B_TEST_STRING, buffer, &test_size);
  ILOG0_DDSD("ID %d, size %d, value %s, ret_code = %d", PARAM_DB_MODULE_B_TEST_STRING, test_size, buffer, return_code);

  return_code = mtlk_pdb_set_string(obj, PARAM_DB_MODULE_B_TEST_STRING, "Good 12345!");
  ILOG0_DD("ID %d, ret_code after set = %d", PARAM_DB_MODULE_B_TEST_STRING, return_code);
  test_size = 256;
  return_code = mtlk_pdb_get_string(obj, PARAM_DB_MODULE_B_TEST_STRING, buffer, &test_size);
  ILOG0_DDSD("ID %d, size %d, value %s, ret_code = %d", PARAM_DB_MODULE_B_TEST_STRING, test_size, buffer, return_code);
      /* error cases - should return error codes or cause assertions*/
  /* end testing mtlk_pdb_set_string */

  /* testing mtlk_pdb_set_binary */
  buffer[0] = 4;
  buffer[1] = 2;
  return_code = mtlk_pdb_set_binary(obj, PARAM_DB_MODULE_A_TEST_BINARY, buffer, 2);
  ILOG0_DD("ID %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_BINARY, return_code);

  test_size = 256;
  return_code = mtlk_pdb_get_binary(obj, PARAM_DB_MODULE_A_TEST_BINARY, buffer, &test_size);
  ILOG0_DDD("ID %d, size %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_BINARY, test_size, return_code);
  for(value = 0; value < test_size; value++) {
      ILOG0_DD("binary[%d] = %d", value, buffer[value]);
  }

  buffer[0] = 2;
  buffer[1] = 4;
  return_code = mtlk_pdb_set_binary(obj, PARAM_DB_MODULE_A_TEST_BINARY, buffer, 2);
  ILOG0_DD("ID %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_BINARY, return_code);

  test_size = 256;
  return_code = mtlk_pdb_get_binary(obj, PARAM_DB_MODULE_A_TEST_BINARY, buffer, &test_size);
  ILOG0_DDD("ID %d, size %d, ret_code = %d", PARAM_DB_MODULE_A_TEST_BINARY, test_size, return_code);
  for(value = 0; value < test_size; value++) {
      ILOG0_DD("binary[%d] = %d", value, buffer[value]);
  }
      /* error cases - should return error codes or cause assertions*/
  /* end testing mtlk_pdb_set_binary */

  /* Test open ref counters - use rmmod to get assert message*/
  
  {
    mtlk_pdb_handle_t handle ;
    int result;
    handle = mtlk_pdb_open(obj, PARAM_DB_MODULE_A_TEST_BINARY);

    if(handle) {
      buffer[0] = 2;
      buffer[1] = 4;

      result = mtlk_pdb_fast_cmp_binary(handle, buffer, 2);
      ILOG0_D("Binary comparison result 1 %d", result);

      result = mtlk_pdb_fast_cmp_binary(handle, buffer, 1);
      ILOG0_D("Binary comparison result 2 %d", result);

      result = mtlk_pdb_fast_cmp_binary(handle, buffer, 122);
      ILOG0_D("Binary comparison result 3 %d", result);


      buffer[0] = 4;
      buffer[1] = 2;

      result = mtlk_pdb_fast_cmp_binary(handle, buffer, 2);
      ILOG0_D("Binary comparison result 4 %d", result);

      mtlk_pdb_close(handle);
    }
  /*  handle = mtlk_pdb_open(obj, PARAM_DB_MODULE_A_TEST_BINARY);
    handle = mtlk_pdb_open(obj, PARAM_DB_MODULE_B_TEST_STRING);*/
  }
  

  return MTLK_ERR_OK;
}
#endif /* MTLK_PDB_UNIT_TEST */
