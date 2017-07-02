#include "mtlkinc.h"
#include "mtlk_param_db.h"
#include "core_pdb_def.h"
#include "dfs_pdb_def.h"

/* include section for the parameter definitions from each module */
/* #include "mtlk_module1_params.h"*/
/* #include "mtlk_module2_params.h"*/

#ifdef MTLK_PDB_UNIT_TEST
/* sample section - those arrays should be define in each module */
const char    test_module_a_hello_str[] = "Hello world";
const uint32  test_module_a_param1      = 1;
const uint32  test_module_a_param2      = 2;
const uint8   test_module_a_binary[]    = {7,8,9,3,4,5,6};

const uint32  test_module_b_param1      = 42;
const char    test_module_b_signature[] = "Signature";

const mtlk_pdb_initial_value sample_module_params1[] = 
    { 
      /* ID,                          TYPE,                 FLAGS,                        SIZE,                            POINTER TO CONST */
      {PARAM_DB_MODULE_A_TEST_STRING, PARAM_DB_TYPE_STRING, PARAM_DB_VALUE_FLAG_READONLY, sizeof(test_module_a_hello_str), test_module_a_hello_str},
      {PARAM_DB_MODULE_A_TEST_INT1,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(test_module_a_param1),    &test_module_a_param1},
      {PARAM_DB_MODULE_A_TEST_INT2,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_READONLY, sizeof(test_module_a_param2),    &test_module_a_param2},
      {PARAM_DB_MODULE_A_TEST_BINARY, PARAM_DB_TYPE_BINARY, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(test_module_a_binary),    test_module_a_binary},
      {PARAM_DB_LAST_VALUE_ID,        0,                    0,                            0,                               NULL},
    };

const mtlk_pdb_initial_value sample_module_params2[] = 
    { 
      /* ID,                          TYPE,                 FLAGS,                        SIZE,                            POINTER TO CONST */
      {PARAM_DB_MODULE_B_TEST_STRING, PARAM_DB_TYPE_STRING, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(test_module_b_signature), test_module_b_signature},
      {PARAM_DB_MODULE_B_TEST_INT1,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(test_module_b_param1),    &test_module_b_param1},
      {PARAM_DB_LAST_VALUE_ID,        0,                    0,                            0,                               NULL},
    };
#endif /* MTLK_PDB_UNIT_TEST */

/* end of sample section */

const mtlk_pdb_initial_value *mtlk_pdb_initial_values[] = {
    mtlk_core_parameters,             /* coremodule parameters */
    mtlk_dfs_parameters,              /* DFS module parameters */
#ifdef MTLK_PDB_UNIT_TEST
  sample_module_params1,              /* tets parameters */
  sample_module_params2,              /* tets parameters */
#endif /* MTLK_PDB_UNIT_TEST */
  NULL,
};

