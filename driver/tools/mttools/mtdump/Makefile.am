# $Id: Makefile.am 11983 2011-11-22 15:32:28Z fleytman $

include $(top_srcdir)/Makefile.am.target_apps

mttools_PROGRAMS = mtdump
nodist_mttools_DATA = mtdump.mtidlc

mtdump_SOURCES = mtdump.c \
                 mtidl_reader.c mtidl_reader.h

nodist_mtdump_SOURCES = app_info.c \
                        $(rtlogger_shared_srcdir)/logmacro_database.c

mtdump_LDADD = $(builddir)/../shared/libmttools.a \
               @ld_group_start@ \
               $(tools_shared_builddir)/linux/libmtlkc.a \
               $(wireless_shared_builddir)/libmtlkwls.a \
               @ld_group_end@ \
               $(tools_shared_builddir)/3rd_party/iniparser/libiniparser.a \
               -lpthread -lnl

AM_CPPFLAGS += -I$(srcdir)/../shared/ \
               -I$(wireless_shared_srcdir) \
               -I$(tools_shared_srcdir)/linux \
               -I$(tools_shared_srcdir) \
               -I$(tools_srcdir)/mtidl \
               -I$(fw_shared_srcdir)

mtdump.mtidlc: $(tools_srcdir)/mtidl/mtidl_ini.pl $(fw_shared_srcdir)/mhi_statistics.h $(wireless_shared_srcdir)/mtlkwssa_drvinfo.h
	perl $(tools_srcdir)/mtidl/mtidl_ini.pl mtdump $(fw_shared_srcdir)/mhi_statistics.h $(wireless_shared_srcdir)/mtlkwssa_drvinfo.h

clean_local_hook = rm -f mtdump.mtidlc
