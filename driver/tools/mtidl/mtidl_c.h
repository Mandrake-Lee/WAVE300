#ifndef __MTLK_MTIDL_C_H__
#define __MTLK_MTIDL_C_H__

/* MTIDL-wrapped enums definition */
#define _MTIDL_WRAPPED_ENUM_START            enum {
#define _MTIDL_WRAPPED_ENUM_ENTRY(id, value) id=value,
#define _MTIDL_WRAPPED_ENUM_END              };

/* Macros for ID lists */
#define MTIDL_ID_LIST_START _MTIDL_WRAPPED_ENUM_START
#define MTIDL_ID(id, value) _MTIDL_WRAPPED_ENUM_ENTRY(id, value)
#define MTIDL_ID_LIST_END   _MTIDL_WRAPPED_ENUM_END

/* Macros for types lists */
#define MTIDL_TYPE_LIST_START _MTIDL_WRAPPED_ENUM_START
#define MTIDL_TYPE(id, value) _MTIDL_WRAPPED_ENUM_ENTRY(id, value)
#define MTIDL_TYPE_LIST_END   _MTIDL_WRAPPED_ENUM_END

/* Macros for levels lists */
#define MTIDL_LEVEL_LIST_START _MTIDL_WRAPPED_ENUM_START
#define MTIDL_LEVEL(id, value) _MTIDL_WRAPPED_ENUM_ENTRY(id, value)
#define MTIDL_LEVEL_LIST_END   _MTIDL_WRAPPED_ENUM_END

/* Macros for sources lists */
#define MTIDL_SOURCE_LIST_START _MTIDL_WRAPPED_ENUM_START
#define MTIDL_SOURCE(id, value) _MTIDL_WRAPPED_ENUM_ENTRY(id, value)
#define MTIDL_SOURCE_LIST_END   _MTIDL_WRAPPED_ENUM_END

/* Macros for constants lists */
#define MTIDL_CONST_LIST_START _MTIDL_WRAPPED_ENUM_START
#define MTIDL_CONST(id, value) _MTIDL_WRAPPED_ENUM_ENTRY(id, value)
#define MTIDL_CONST_LIST_END   _MTIDL_WRAPPED_ENUM_END

/* Macros for MTIDL enum fields */
#define MTIDL_ENUM_START                           typedef enum {
#define MTIDL_ENUM_ENTRY(id, value, name)          id = (value),
#define MTIDL_ENUM_END(type)                       } type;

/* Macros for MTIDL bitfield values */
#define MTIDL_BITFIELD_START                       typedef enum {
#define MTIDL_BITFIELD_ENTRY(id, bit, name)        id= bit,
#define MTIDL_BITFIELD_END(type)                   } type;

/* Types */
typedef uint32    mtidl_long_t;
typedef int32     mtidl_slong_t;
typedef uint32    mtidl_enum_t;
typedef uint32    mtidl_bitfield_t;
typedef uint64    mtidl_huge_t;
typedef int64     mtidl_shuge_t;
typedef uint32    mtidl_timestamp_ms_t;
typedef uint32    mtidl_flag_t;
typedef uint64    mtidl_macaddr_t;

/* Macros for structures definitions */
#define MTIDL_ITEM_START(friendly_name, type, level, source, id, description) \
  typedef struct { 

#define MTIDL_ITEM_END(type_name) \
  } __MTLK_PACKED type_name;

#define MTIDL_LONGVAL(name, description) \
  mtidl_long_t name;

#define MTIDL_SLONGVAL(name, description) \
  mtidl_slong_t name;

#define MTIDL_HUGEVAL(name, description) \
  mtidl_huge_t name;

#define MTIDL_SHUGEVAL(name, description) \
  mtidl_shuge_t name;

#define MTIDL_TIMESTAMP(name, description) \
  mtidl_timestamp_ms_t name;

#define MTIDL_FLAG(name, description) \
  mtidl_flag_t name;

#define MTIDL_ENUM(name, binary_type, description) \
  mtidl_enum_t name;

#define MTIDL_BITFIELD(name, binary_type, description) \
  mtidl_bitfield_t name;

#define MTIDL_ITEM(item_type_name, name, description) \
  item_type_name name;

#define MTIDL_MACADDR(name, description) \
  mtidl_macaddr_t name;

#define MTIDL_LONGVAL_ARRAY(name, size, description) \
  mtidl_long_t name[size];

#define MTIDL_HUGEVAL_ARRAY(name, size, description) \
  mtidl_huge_t name[size];

#define MTIDL_TIMESTAMP_ARRAY(name, size, description) \
  mtidl_timestamp_ms_t name[size];

#define MTIDL_FLAG_ARRAY(name, size, description) \
  mtidl_flag_t name[size];

#define MTIDL_ITEM_ARRAY(item_type_name, name, size, description) \
  item_type_name name[size];

#define MTIDL_ENUM_ARRAY(name, size, binary_type, description) \
  mtidl_enum_t name[size];

#define MTIDL_BITFIELD_ARRAY(name, size, binary_type, description) \
  mtidl_bitfield_t name[size];

#define MTIDL_MACADDR_ARRAY(name, size, description) \
  mtidl_macaddr_t name[size];

#define MTIDL_LONGFRACT(name, fract_size, description) \
  mtidl_long_t name;

#define MTIDL_SLONGFRACT(name, fract_size, description) \
  mtidl_slong_t name;

#define MTIDL_LONGFRACT_ARRAY(name, size, fract_size, description) \
  mtidl_long_t name[size];

#define MTIDL_SLONGFRACT_ARRAY(name, size, fract_size, description) \
  mtidl_slong_t name[size];

#endif //__MTLK_MTIDL_C_H_
