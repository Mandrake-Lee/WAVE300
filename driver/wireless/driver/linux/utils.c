/*
 * $Id: utils.c 11187 2011-05-27 10:08:55Z Strashko $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Utilities.
 *
 * Originally written by Andrey Fidrya
 *
 */
#include "mtlkinc.h"

#include "utils.h"

#define LOG_LOCAL_GID   GID_UTILS
#define LOG_LOCAL_FID   1

/*
 * Function outputs buffer in hex format
 */
#ifdef MTCFG_SILENT
void __MTLK_IFUNC
mtlk_aux_print_hex (const void *buf, unsigned int l)
{
}
#else

#ifdef MTCFG_SILENT
#define LOG_BUFFER
#else
#define LOG_BUFFER             printk
#endif

void __MTLK_IFUNC
mtlk_aux_print_hex (const void *buf, unsigned int l)
{
  unsigned int i,j;
  unsigned char *cp = (unsigned char*)buf;
  LOG_BUFFER("cp= 0x%p l=%d\n", cp, l);
  for (i = 0; i < l/16; i++) {
    LOG_BUFFER("%04x:  ", 16*i);
    for (j = 0; j < 16; j++)
      LOG_BUFFER("%02x %s", *cp++, j== 7 ? " " : "");
    LOG_BUFFER("\n");
  }
  if (l & 0x0f) {
    LOG_BUFFER("%04x:  ", 16*i);
    for (j = 0; j < (l&0x0f); j++)
      LOG_BUFFER("%02x %s", *cp++, j== 7 ? " " : "");
    LOG_BUFFER("\n");
  }
}
#endif

void mtlk_dump(uint8 level, const void *buf, uint32 len, char *str)
{
  if(level > RTLOG_MAX_DLEVEL) {
    return;
  }

  ILOG0_S("%s",str);
  mtlk_aux_print_hex(buf, len);
}

uint32
mtlk_shexdump (char *buffer, uint8 *data, size_t size)
{
  uint8 line, i;
  uint32 counter = 0;

  for (line = 0; size; line++) {
    counter += sprintf(buffer+counter, "%04x: ", line * 0x10);
    for (i = 0x10; i && size; size--,i--,data++) {
      counter +=sprintf(buffer+counter, " %02x", *data);
    }
    counter += sprintf(buffer+counter, "\n");
  }
  return counter;
}

char * __MTLK_IFUNC
mtlk_get_token (char *str, char *buf, size_t len, char delim)
{
  char *dlm;

  if (!str) {
    buf[0] = 0;
    return NULL;
  }
  dlm = strchr(str, delim);
  if (dlm && ((size_t)(dlm - str) < len)) {
    memcpy(buf, str, dlm - str);
    buf[dlm - str] = 0;
  } else {
    memcpy(buf, str, len - 1);
    buf[len - 1] = 0;
  }
  ILOG4_S("Get token: '%s'", buf);
  if (dlm)
    return dlm + 1;
  return dlm;
}
