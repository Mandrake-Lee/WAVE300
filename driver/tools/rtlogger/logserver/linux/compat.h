/*
 * Copyright (c) 2007-2008 Metalink Broadband (Israel)
 *
 * Compatibility macros
 *
 * Written by: Andrey Fidrya
 *
 */

#ifndef __COMPAT_H__
#define __COMPAT_H__

#define __PACKED __attribute__((aligned(1), packed))

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#endif // !__COMPAT_H__

