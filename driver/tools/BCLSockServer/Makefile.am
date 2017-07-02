# $Id: Makefile.am 10708 2011-02-24 16:53:13Z dmytrof $

include $(top_srcdir)/Makefile.am.target_apps

AM_CPPFLAGS += -I$(srcdir)/BCL/
mttools_PROGRAMS = BclSockServer
dist_mttools_DATA = $(srcdir)/scripts/bcl_util.tcl

BclSockServer_SOURCES = BCLSockServer.c BCLSockServer.h \
                        mt_tftp.c \
                        BCL/mt_bcl.c BCL/mt_bcl.h BCL/mt_bcl_defs.h \
                        BCL/mt_util.c BCL/mt_util.h\
                        BCL/mt_bcl_funcs.c BCL/mt_bcl_funcs.h \
                        BCL/mt_lchacc.c \
                        BCL/mt_wapi.c BCL/mt_wapi.h \
                        BCL/mt_bcl_wrap.h BCL/mt_chacc.h \
                        BCL/mt_cnfg.h BCL/mt_help.h BCL/mt_types.h

nodist_BclSockServer_SOURCES = $(abs_builddir)/app_info.c

BclSockServer_LDADD = -lpthread

AM_CPPFLAGS += -I$(tools_shared_srcdir)/linux \
               -I$(tools_shared_srcdir) \
               -I$(wireless_shared_srcdir)

