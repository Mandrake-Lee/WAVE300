# $Id: Makefile.am 12321 2011-12-29 12:41:19Z nayshtut $

include $(top_srcdir)/Makefile.am.target_apps

if BUILD_WIRELESS_SHARED

logger_origin_name = libmtlkwls
logger_origin_id = 5

AM_CPPFLAGS += -I$(srcdir)/../ \
               -I$(fw_shared_srcdir)/ \
               -I$(tools_shared_srcdir)/ \
               -I$(tools_shared_srcdir)/linux/ \
               -I$(driver_shared_srcdir)/ \
               -I$(tools_srcdir)/mtidl

noinst_LIBRARIES = libmtlkwls.a

nodist_libmtlkwls_a_SOURCES = mem_leak.c \
                              mtlk_algorithms.c \
                              mtlkmetrics.c mtlkmetrics.h \
                              mtlk_objpool.c \
                              mtlkstartup.c  \
                              mtlkerr.c \
                              mtlkstrtok.c mtlkstrtok.h \
                              $(tools_shared_srcdir)/mtlkirbhash.c  \
                              pattern.c mtlkwssa.c \
                              $(rtlogger_shared_srcdir)/logmacro_database.c

WIRELESS_SHARED_EXTRA_DIST = $(nodist_libmtlkwls_a_SOURCES)

endif

noinst_HEADERS = memtags.h mtlkerr.h \
                 mtlkhash.h mtlklist.h mtlklist_dbg.h \
                 pattern.h mtlkbfield.h mtlkwssa.h mtlkwssairb.h \
                 mtlkstartup.h mem_leak.h \
                 dataex.h \
                 mtlkguid.h \
                 mtlk_objpool.h \
                 mtlk_algorithms.h \
                 mtlkaselirb.h \
                 mtlk_assert.h \
                 mtlk_slid.h \
                 mtlkwssa_drvinfo.h \
                 mtlkwlanirbdefs.h \
                 mtlk_reflim.h \
                 nl.h

EXTRA_DIST = mtlkhash_ieee_addr.c mtlkhash_ieee_addr.h mtlkstartup.c mtlkerr.c \
             mtlk_objpool.c pattern.c mtlkhash_ieee_addr.c \
             mtlk_algorithms.c mem_leak.c \
             $(WIRELESS_SHARED_EXTRA_DIST)
