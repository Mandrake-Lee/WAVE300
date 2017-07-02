# $Id: Makefile.am 11905 2011-11-06 14:36:46Z fleytman $

#This list must specify correct "high-level" build order
#i.e. shared components to be built before before components
#that may use them, helper applications to be built first

include $(top_srcdir)/Makefile.am.common

EXTRA_DIST = mtlk_version \
             MTLKConfig \
             branch_version.m4.in \
             support/cfghlpr.sh \
             support/mtlkfront.sh \
             support/svn_info.sh \
             support/mtlk_ver.pl \
             support/make_cfg_header.awk \
             Makefile.am.module_helper Makefile.am.kbuild \
             $(shell find $(top_srcdir)/tools/kconfig -name '*.h' -o \
                                                      -name '*.c' -o \
                                                      -name Makefile -o \
                                                      -name '*.moc') \
             $(shell find $(top_srcdir)/support -name '*.env' -o \
                                                -name '*.env.common' -o \
                                                -name '*.cfg') \
             $(shell find $(top_srcdir)/support/release/configs -name '*.complist' -o \
                                                                -name '*.config')

SUBDIRS = tools/shared \
          wireless/shared \
          tools \
          wireless

DIST_SUBDIRS = $(SUBDIRS)

dist-hook:
	cp -fp $(abs_top_srcdir)/Makefile $(distdir)
	rm -f $(distdir)/.config $(abs_top_builddir)/.prebuilt_location
	test x$(GPL_DISTRIBUTION) != xy || ls -1 $(distdir)/wireless/driver/rflib | grep -v '^Makefile.am$$' | xargs -I {} rm -rf $(distdir)/wireless/driver/rflib/{}
	test x$(GPL_DISTRIBUTION) != xy || echo $(PREBUILT_TARBALL) > $(abs_top_builddir)/.prebuilt_location
	$(abs_top_srcdir)/support/cfghlpr.sh $(top_srcdir)/.config complete_makefiles_am $(abs_top_srcdir) $(distdir)
	$(abs_top_srcdir)/support/svn_info.sh $(abs_top_srcdir) > $(distdir)/svn_info
	cd $(distdir) && autoreconf --install

distcheck-hook:
	chmod -R +w $(distdir)
	make -C $(distdir) defconfig
	rm -rf $(distdir)/builds
	cp -fp $(abs_top_builddir)/.config $(distdir)/
	cp -fp $(abs_top_builddir)/.config $(distdir)/_build
	awk -f $(distdir)/support/make_cfg_header.awk \
                             < $(distdir)/_build/.config \
                             > $(distdir)/_build/.config.h
	test ! -e $(abs_top_builddir)/.prebuilt_location || \
            ( cd $(distdir) && tar jxf `cat $(abs_top_builddir)/.prebuilt_location` )
	test -e $(distdir)/svn_info
	awk '/^[^[:space:]]+[[:space:]][[:digit:]]+$$/{exit 0} //{exit 1}' \
                            < $(distdir)/svn_info
	test x"`wc -l $(distdir)/svn_info | awk '{print $$1}'`" = x"1"

DISTCHECK_CONFIGURE_FLAGS = --host "`cat ../.config | grep CONFIG_HOST_TYPE | sed -e "s/.*CONFIG_HOST_TYPE=//" -e"s/\\\"//g"`" \
                            --build="`../config.guess`" \
                            `test ! -e $(abs_top_builddir)/.prebuilt_location || echo "--with-prebuilt-rflib=\`cat $(abs_top_builddir)/.prebuilt_location\`"`

DISTCLEANFILES = .config .config.h `find -name Makefile` \
                 $(distdir)/svn_info \
                 $(abs_top_builddir)/.prebuilt_location \
                 $(distdir)/wireless/driver/rflib/prebuilt

clean-local:
	rm -rf ./.logprep_ts
	rm -f logmacros.c logmacros.h loggroups.h \
        UNKNOWN_ORIGIN.scd 

