#include "iniparser.h"
#include "mtlkerr.h"

#define INIPARSER_KEY_DELIM         ":"
#define INIPARSER_FULL_KEY_MAX_SIZE 128

/* Key creation as required by iniparser. 
 * NOTE: gkey means global key (i.e. out of sections key)
 */
#define iniparser_aux_key_const(section, key) (section INIPARSER_KEY_DELIM key)
#define iniparser_aux_gkey_const(key)         iniparser_aux_key_const("", key)

static __INLINE int
iniparser_aux_key (const char *section, 
                   const char *key, 
                   char       *buf,
                   uint32      buf_size)
{
  return snprintf(buf, buf_size, "%s" INIPARSER_KEY_DELIM "%s", section?section:"", key);
}

static __INLINE int
iniparser_aux_gkey (const char *key, 
                    char       *buf,
                    uint32      buf_size)
{
  return iniparser_aux_key(NULL, key, buf, buf_size);
}

static __INLINE int
iniparser_aux_fillstr (dictionary *dict,
                       const char *key,
                       char       *buf,
                       uint32      buf_size)
{
  int   res = MTLK_ERR_UNKNOWN;
  char *tmp = iniparser_getstr(dict, key);

  if (!tmp) {
    res = MTLK_ERR_NOT_IN_USE;
  }
  else if (strlen(tmp) >= buf_size) {
    strncpy(buf, tmp, buf_size - 1);
    buf[buf_size - 1] = 0;
    return MTLK_ERR_BUF_TOO_SMALL;
  }
  else {
    strcpy(buf, tmp);
    res = MTLK_ERR_OK;
  }

  return res;
}

static __INLINE char *
iniparser_aux_getstring (dictionary *dict,
                         const char *section,
                         const char *key,
                         char       *def)
{
  char full_key[INIPARSER_FULL_KEY_MAX_SIZE];
  int  n = iniparser_aux_key(section, key, full_key, sizeof(full_key));

#ifndef DEBUG
  MTLK_UNREFERENCED_PARAM(n);
#else
  MTLK_ASSERT(n < sizeof(full_key));
#endif

  return iniparser_getstring(dict, full_key, def);
}

static __INLINE char *
iniparser_aux_getstr (dictionary *dict,
                      const char *section,
                      const char *key)
{
  return iniparser_aux_getstring(dict, section, key, NULL);
}

static __INLINE int
iniparser_aux_getint (dictionary *dict,
                      const char *section,
                      const char *key,
                      int         notfound)
{
  char full_key[INIPARSER_FULL_KEY_MAX_SIZE];
  int  n = iniparser_aux_key(section, key, full_key, sizeof(full_key));

#ifndef DEBUG
  MTLK_UNREFERENCED_PARAM(n);
#else
  MTLK_ASSERT(n < sizeof(full_key));
#endif

  return iniparser_getint(dict, full_key, notfound);
}

