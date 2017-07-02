# $Id: Makefile.am 11589 2011-08-31 15:30:12Z fleytman $

include $(top_srcdir)/Makefile.am.common

module_canonical_name = mtlkroot_ko
mtlkroot_ko_src_dir = linux
mtlkroot_ko_binary_name = mtlkroot.ko

driver_PROGRAMS = $(mtlkroot_ko_binary_name)
nodist_driver_DATA = $(builddir)/$(mtlkroot_ko_src_dir)/drv_toolchain_info \
                     $(builddir)/$(mtlkroot_ko_src_dir)/mtlkroot.scd

.PHONY : $(mtlkroot_ko_binary_name)
mtlkroot_ko_SOURCES =

include $(top_srcdir)/Makefile.am.module_build

mtlkroot_ko_LINK = @$(mtlkroot_ko_BUILD_CMD)
clean-local:
	$(mtlkroot_ko_CLEAN_CMD)

EXTRA_DIST = log_drv.h log_drv.c \
             linux/drvver.h linux/logdrv_osdep.c \
             linux/mtlkroot.c linux/mtlk_assert.c \
             linux/mtlkirbd_root.c shared/mtlkirbd.h \
             linux/nlmsgs.c linux/nlmsgs.h \
             linux/mtlkcdev.c linux/mtlkcdev.h \
             linux/Kbuild
