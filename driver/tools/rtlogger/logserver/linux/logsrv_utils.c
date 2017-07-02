/*
 * Copyright (c) 2007-2008 Metalink Broadband (Israel)
 *
 * Utilities
 *
 * Written by: Andrey Fidrya
 *
 */

#include "mtlkinc.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include "logsrv_utils.h"

#define LOG_LOCAL_GID   GID_LOGSRV_UTILS
#define LOG_LOCAL_FID   1

int
get_line(const char *filename, char *buf, size_t buf_size, FILE *fl,
  int trim_crlf, int *peof)
{
  int rslt = 0;
  int num_read;

  *peof = 0;

  if (!fgets(buf, buf_size, fl)) {
    if (feof(fl)) {
      *peof = 1;
      goto cleanup;
    }
    ELOG_SS("%s: cannot read: %s", filename, strerror(errno));
    rslt = -1;
    goto cleanup;
  }

  num_read = strlen(buf);
  if (num_read >= 1 && buf[num_read - 1] != '\n') {
    ELOG_SS("%s: line too long: %s", filename, buf);
    rslt = -1;
    goto cleanup;
  }

  if (trim_crlf) {
    while (num_read >= 1 &&
        (buf[num_read - 1] == '\n' || buf[num_read - 1] == '\r')) {
      buf[--num_read] = '\0';
    }
  }

cleanup:
  return rslt;
}

int
get_word(char **pp, char *buf, size_t buf_size)
{
  int rslt = 0;
  int at = 0;
  char c;

  skip_spcrlf(pp);

  for (;;) {
    c = **pp;
    if (!c || is_spcrlf(c))
      break;
    if (at >= buf_size - 1) {
      rslt = -1;
      goto cleanup;
    }
    buf[at] = c;
    ++at;
    ++(*pp);
  }
  buf[at] = '\0';
    
cleanup:
  return rslt;
}

int
is_spcrlf(char c)
{
  switch (c) {
  case ' ':
  case '\t':
  case '\r':
  case '\n':
    return 1;
  default:
    break;
  }
  return 0;
}

void
skip_spcrlf(char **pp)
{
  for (;;) {
    switch (**pp) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
      ++(*pp);
      continue;
    case '\0':
    default:
      return;
    }
  }
}

int
str_decode(char *str)
{
  char *p = str;
  char *to = str;
  char c;
  int rslt = 0;

  skip_spcrlf(&p);

  if (*p != '\"') {
    rslt = -1;
    goto cleanup;
  }
  ++p;

  while ((c = *p) != '\0') {
    if (c == '\"')
      break;
    ++p;

    if (c == '\\') {
      switch (*p) {
      case 'r':
        *to++ = '\r';
        break;
      case 't':
        *to++ = '\t';
        break;
      case 'n':
        *to++ = '\n';
        break;
      case '\\':
        *to++ = '\\';
        break;
      case '\'':
        *to++ = '\'';
        break;
      case '\"':
        *to++ = '\"';
        break;
      default:
        rslt = -1;
        goto cleanup;
      }
      ++p;
    } else {
      *to++ = c;
    }
  }

  if (c == '\0') {
    // Unterminated string
    rslt = -1;
    goto cleanup;
  }

  *to = '\0';

cleanup:
  return rslt;
}

