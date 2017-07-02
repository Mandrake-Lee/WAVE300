# $Id: Makefile.am 11014 2011-04-12 16:13:52Z andriit $

include $(top_srcdir)/Makefile.am.target_apps

mttools_PROGRAMS = mtlk_cli

mtlk_cli_SOURCES = main.c

nodist_mtlk_cli_SOURCES = app_info.c \
						$(rtlogger_shared_srcdir)/logmacro_database.c

mtlk_cli_LDADD = $(builddir)/../shared/libmttools.a \
                 @ld_group_start@ \
                 $(tools_shared_builddir)/linux/libmtlkc.a \
                 $(wireless_shared_builddir)/libmtlkwls.a \
                 @ld_group_end@

AM_CPPFLAGS += -I$(srcdir)/../shared/ \
               -I$(srcdir)/../shared/linux \
               -I$(wireless_shared_srcdir) \
               -I$(tools_shared_srcdir)/linux \
               -I$(tools_shared_srcdir)
