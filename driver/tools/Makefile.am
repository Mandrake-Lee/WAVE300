# $Id: Makefile.am 11049 2011-04-24 09:29:12Z dmitryf $

if BUILD_DUT
DUTSERVER_DIRS = dutserver
else
DUTSERVER_DIRS =
endif

if BUILD_MTTOOLS
BCL_DIRS = BCLSockServer
else
BCL_DIRS =
endif

#Reminder: Subdir shared added via root Makefile
#          Do not put it here

SUBDIRS = mttools mtlkroot rtlogger $(DUTSERVER_DIRS) $(BCL_DIRS)
DIST_SUBDIRS = $(SUBDIRS)

oom_killer_hook_ko_src_dir=oom_killer_hook

if BUILD_HELPER_TOOLS

include $(top_srcdir)/Makefile.am.common

module_canonical_name = oom_killer_hook_ko
oom_killer_hook_ko_binary_name = oom_killer_hook.ko

hlprtools_PROGRAMS = oom_killer_hook.ko

nodist_hlprtools_DATA = $(srcdir)/$(oom_killer_hook_ko_src_dir)/rnotes.txt \
                        $(builddir)/$(oom_killer_hook_ko_src_dir)/drv_toolchain_info \
                        $(top_builddir)/.config

.PHONY : $(oom_killer_hook_ko_binary_name)
oom_killer_hook_ko_SOURCES =

include $(top_srcdir)/Makefile.am.module_build

oom_killer_hook_ko_LINK = @$(oom_killer_hook_ko_BUILD_CMD)

clean-local:
	$(oom_killer_hook_ko_CLEAN_CMD)

EXTRA_DIST = oom_killer_hook/oom_killer_hook.c \
             oom_killer_hook/Kbuild \
             $(srcdir)/$(oom_killer_hook_ko_src_dir)/rnotes.txt

endif

