# $Id: Makefile.am 11014 2011-04-12 16:13:52Z andriit $

include $(top_srcdir)/Makefile.am.target_apps

tests_PROGRAMS = mtlk_cli_dbg_srv

mtlk_cli_dbg_srv_SOURCES = main_dbg_server.c

nodist_mtlk_cli_dbg_srv_SOURCES = app_info.c \
						$(rtlogger_shared_srcdir)/logmacro_database.c
						
mtlk_cli_dbg_srv_LDADD = $(builddir)/../shared/libmttools.a \
                         @ld_group_start@ \
                         $(tools_shared_builddir)/linux/libmtlkc.a \
                         $(wireless_shared_builddir)/libmtlkwls.a \
                         @ld_group_end@ \
                         -lpthread

AM_CPPFLAGS += -I$(srcdir)/../shared/ \
               -I$(srcdir)/../shared/linux \
               -I$(wireless_shared_srcdir) \
               -I$(tools_shared_srcdir)/linux \
               -I$(tools_shared_srcdir)
