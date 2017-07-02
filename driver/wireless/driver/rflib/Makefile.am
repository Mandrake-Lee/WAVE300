# $Id: Makefile.am 11821 2011-10-25 17:05:25Z nayshtut $

if BUILD_DRIVER

include $(top_srcdir)/Makefile.am.common

module_canonical_name = mtlk_rflib_a
mtlk_rflib_a_src_dir = linux
mtlk_rflib_a_binary_name = mtlk_rflib.a

nodist_driver_DATA = $(builddir)/$(mtlk_rflib_a_src_dir)/rflib.scd
nodist_rflib_DATA = $(builddir)/$(mtlk_rflib_a_src_dir)/rflib.scd
nodist_wlsprebult_DATA = $(builddir)/prebuilt.tar.bz2

$(builddir)/prebuilt.tar.bz2: mtlk_rflib.a $(builddir)/$(mtlk_rflib_a_src_dir)/rflib.scd
	rm -rf $(builddir)/_prebuilt_package
	mkdir -p $(builddir)/_prebuilt_package/wireless/driver/rflib/prebuilt/$(CONFIG_ENVIRONMENT_NAME)
	cp -fp $^ $(builddir)/_prebuilt_package/wireless/driver/rflib/prebuilt/$(CONFIG_ENVIRONMENT_NAME)
	cd $(builddir)/_prebuilt_package/ && tar -cjf $(abs_builddir)/prebuilt.tar.bz2 wireless

$(builddir)/$(mtlk_rflib_a_src_dir)/rflib.scd: mtlk_rflib.a
	@:

rflib_PROGRAMS = mtlk_rflib.a

.PHONY : $(mtlk_rflib_a_binary_name)
mtlk_rflib_a_SOURCES =

mtlk_build_kernel_library=y
include $(top_srcdir)/Makefile.am.module_build

mtlk_rflib_a_LINK = @{ \
                        $(rebase_copy_cmd) $(abs_srcdir)/prebuilt/$(CONFIG_ENVIRONMENT_NAME)/mtlk_rflib.a $(abs_builddir) 2>/dev/null && \
                        mkdir -p $(abs_builddir)/$(mtlk_rflib_a_src_dir) && \
                        $(rebase_copy_cmd) $(abs_srcdir)/prebuilt/$(CONFIG_ENVIRONMENT_NAME)/rflib.scd $(abs_builddir)/$(mtlk_rflib_a_src_dir) 2>/dev/null; \
                      } || $(mtlk_rflib_a_BUILD_CMD)
clean-local:
	$(mtlk_rflib_a_CLEAN_CMD)
	rm -rf $(builddir)/_prebuilt_package $(builddir)/prebuilt.tar.bz2

endif

if !USE_PREBUILT_RFLIB

EXTRA_DIST = linux/Kbuild \
             shared/mtlk_aocs.c shared/mtlk_aocs_propr.h \
             shared/mtlk_channels.c shared/mtlk_channels_propr.h \
             shared/mtlk_dfs.c shared/mtlk_dfs.h \
             shared/mtlk_eeprom.c shared/mtlk_eeprom.h \
             shared/mtlk_gpl_helper.c \
             shared/mtlk_rfmgmt.c \
             shared/mtlk_scan.c shared/mtlk_scan_propr.h \
             shared/mtlk_tpcv4.c shared/mtlk_tpcv4.h \
             shared/rdlim.c shared/rdlim.h \
	     shared/coex20_40.c shared/coexfrgen.c \
	     shared/coexlve.c shared/cbsmgr.c \
	     shared/scexempt.c

endif
