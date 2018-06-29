# $Id: Makefile.am 11049 2011-04-24 09:29:12Z dmitryf $
include $(top_srcdir)/Makefile.am.common

if BUILD_RTLOGGER
SUBDIRS = logserver logcnv shared
else
SUBDIRS = shared
endif

DIST_SUBDIRS = $(SUBDIRS)

EXTRA_DIST = logprep/logprep.pl

