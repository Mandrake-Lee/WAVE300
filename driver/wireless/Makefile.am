# $Id: Makefile.am 11571 2011-08-28 10:22:01Z fleytman $

include $(top_srcdir)/Makefile.am.common

#Reminder: Subdir shared added via root Makefile
#          Do not put it here

SUBDIRS = driver

EXTRA_DIST = shared_mbss_mac/msgid.h shared_mbss_mac/mhi_rsn_caps.h \
             shared_mbss_mac/mhi_frame.h shared_mbss_mac/mtlkbfield.h \
             shared_mbss_mac/mhi_umi_propr.h shared_mbss_mac/rsn.h \
             shared_mbss_mac/mhi_statistics.h shared_mbss_mac/umi_rsn.h \
             shared_mbss_mac/mhi_ieee_address.h shared_mbss_mac/mhi_umi.h \
             shared_mbss_mac/shram.h shared_mbss_mac/host_if.h \
             shared_mbss_mac/mhi_mib_id.h shared_mbss_mac/mhi_mac_event.h

if BUILD_WIRELESS_DATA

nodist_wls_DATA= $(top_builddir)/.config \
                 $(srcdir)/rnotes.txt

EXTRA_DIST += $(srcdir)/rnotes.txt

endif
