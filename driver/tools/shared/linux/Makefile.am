# $Id: Makefile.am 12664 2012-02-15 15:55:46Z nayshtut $

include $(top_srcdir)/Makefile.am.target_apps

logger_origin_name = libmtlk
logger_origin_id = 4

AM_CPPFLAGS += -I$(srcdir)/../ \
               -I$(wireless_shared_srcdir) \
               -I$(wireless_shared_srcdir)/linux \
               -I$(tools_shared_srcdir)/3rd_party/iniparser \
               -I$(tools_srcdir)/mttools/shared

noinst_LIBRARIES = libmtlkc.a libmtlkcpp.a

libmtlkc_a_SOURCES = mtlkirba.c mtlk_assert.c \
                     mtlknlink.c mtlknlink.h \
                     osal_osdep.c osal_osdep_decls.h osal_osdep_defs.h \
                     utils.c utils.h \
                     mtlksighandler.c mtlksighandler.h \
                     $(tools_shared_srcdir)/mtlkirbhash.c \
                     $(srcdir)/../mtlkcontainer.c \
                     $(srcdir)/../mtlk_socket.c \
                     $(srcdir)/../argv_parser.c \
                     mtlklist_osdep_defs.h mtlklist_osdep_decls.h \
                     log_osdep.c log_osdep.h \
                     $(tools_srcdir)/mtidl/mtidl_ini_parser.c $(tools_srcdir)/mtidl/mtidl_ini_parser.h

nodist_libmtlkc_a_SOURCES = $(rtlogger_shared_srcdir)/logmacro_database.c

libmtlkcpp_a_SOURCES = $(srcdir)/../aux_utils.cpp  \
                       $(srcdir)/../CmdLine.cpp  \
                       $(srcdir)/../Debug.cpp  \
                       $(srcdir)/../ParamInfo.cpp \
                       LinException.h

noinst_HEADERS = mtlkidefs.h mtlkinc.h mtlkpack.h
dist_noinst_SCRIPTS = $(tools_srcdir)/mtidl/mtidl_ini.pl
