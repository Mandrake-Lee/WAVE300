/*
 * $Id: utils.h 11187 2011-05-27 10:08:55Z Strashko $
 *
 * Copyright (c) 2006-2007 Metalink Broadband (Israel)
 *
 * Utilities.
 *
 * Originally written by Andrey Fidrya
 *
 */

#ifndef _UTILS_H_
#define _UTILS_H_

void mtlk_dump(uint8 level, const void *buf, uint32 len, char *str);

uint32 mtlk_shexdump (char *buffer, uint8 *data, size_t size);

char * __MTLK_IFUNC mtlk_get_token (char *str, char *buf, size_t len, char delim);

#endif // _UTILS_H_

