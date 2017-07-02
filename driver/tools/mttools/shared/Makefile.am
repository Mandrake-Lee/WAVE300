# $Id: Makefile.am 11872 2011-11-01 09:13:39Z fleytman $

include $(top_srcdir)/Makefile.am.target_apps

logger_origin_name = libmttools
logger_origin_id = 6

noinst_LIBRARIES = libmttools.a

libmttools_a_SOURCES = driver_api.c driver_api.h \
                       mtlk_cli_client.c mtlk_cli_client.h \
                       mtlk_cli_server.c mtlk_cli_server.h \
                       linux/mtlk_cli_port.c linux/mtlk_cli_port.h \
                       mtlk_cli_defs.h \
                       compat.h

nodist_libmttools_a_SOURCES = $(rtlogger_shared_srcdir)/logmacro_database.c

AM_CPPFLAGS += -I$(wireless_shared_srcdir) \
               -I$(tools_shared_srcdir) \
               -I$(fw_shared_srcdir) \
               -I$(tools_shared_srcdir)/linux
