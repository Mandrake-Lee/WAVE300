# $Id: Makefile.am 11018 2011-04-13 08:02:30Z andriit $

include $(top_srcdir)/Makefile.am.target_apps

dist_rtlogger_DATA=app_toolchain_info

rtlogger_PROGRAMS = logserver
AM_CPPFLAGS += -I$(tools_shared_srcdir)/linux \
               -I$(tools_shared_srcdir) \
               -I$(wireless_shared_srcdir) \
               -I$(srcdir)/../../shared

nodist_logserver_SOURCES = $(rtlogger_shared_srcdir)/logmacro_database.c
                         
logserver_SOURCES = logserver.c logserver.h \
                    db.c db.h \
                    net.c net.h \
                    cqueue.c cqueue.h \
                    proto_drv.c proto_drv.h \
                    proto_lg.c proto_lg.h \
                    logsrv_utils.c logsrv_utils.h \
                    compat.h

logserver_LDADD = @ld_group_start@                              \
                      $(tools_shared_builddir)/linux/libmtlkc.a \
                      $(wireless_shared_builddir)/libmtlkwls.a  \
                  @ld_group_end@                                \
                  -lpthread
