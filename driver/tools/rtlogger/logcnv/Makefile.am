# $Id: Makefile.am 11012 2011-04-12 14:59:24Z dmitryf $

include $(top_srcdir)/Makefile.am.target_apps

rtlogger_PROGRAMS = logcnv
AM_CPPFLAGS += -I$(tools_shared_srcdir)/linux \
               -I$(tools_shared_srcdir) \
               -I$(wireless_shared_srcdir) \
               -I$(srcdir)/../shared

logcnv_SOURCES = LogEvt.cpp LogEvt.h \
                 LogFmtDB.cpp LogFmtDB.h \
                 logcnv.cpp LogInfo.h pcapdefs.h

logcnv_LDADD = @ld_group_start@ \
               $(wireless_shared_builddir)/libmtlkwls.a \
               $(tools_shared_builddir)/linux/libmtlkc.a \
               @ld_group_end@ \
               $(tools_shared_builddir)/linux/libmtlkcpp.a \
               @cxx_ldadd@
