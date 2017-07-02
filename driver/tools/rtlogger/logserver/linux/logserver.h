/*
 * Copyright (c) 2007-2008 Metalink Broadband (Israel)
 *
 * Log server
 *
 * Written by: Andrey Fidrya
 *
 */

#ifndef __LOGSERVER_H__
#define __LOGSERVER_H__

extern int log_to_console;
extern int log_to_syslog;
extern int text_protocol;
extern int syslog_pri;

void send_to_all(char *data, size_t len);

#endif // !__LOGSERVER_H__

