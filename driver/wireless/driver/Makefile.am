# $Id: Makefile.am 12054 2011-11-29 10:46:54Z strashko $

SUBDIRS=rflib

mtlk_ko_src_dir = linux

if BUILD_DRIVER

include $(top_srcdir)/Makefile.am.common

module_canonical_name = mtlk_ko
mtlk_ko_binary_name = mtlk.ko

driver_PROGRAMS = mtlk.ko

nodist_driver_DATA = $(srcdir)/$(mtlk_ko_src_dir)/LICENSE.txt \
                     $(builddir)/$(mtlk_ko_src_dir)/driver.scd

nodist_wls_DATA = $(builddir)/$(mtlk_ko_src_dir)/drv_toolchain_info

.PHONY : $(mtlk_ko_binary_name)
mtlk_ko_SOURCES =

include $(top_srcdir)/Makefile.am.module_build

# Modules.symvers of mtlkroot.ko copied to the build dir of mtlk.ko to give linker a clue
# about mtlkroot.ko dependencies. See "6.3 Symbols From Another External Module"
# in kernel's kbuild documentation (http://lxr.linux.no/#linux+v2.6.38/Documentation/kbuild/modules.txt)
mtlk_ko_LINK = mkdir -p $(abs_builddir)/$(mtlk_ko_src_dir)/ && \
               { test "x`find $(abs_root_driver_builddir) -name Module*.symvers`" = x || \
                 cp -v $(abs_root_driver_builddir)/Module*.symvers $(abs_builddir)/$(mtlk_ko_src_dir)/; } && \
               $(mtlk_ko_BUILD_CMD)

clean-local:
	$(mtlk_ko_CLEAN_CMD)

endif

EXTRA_DIST =  $(srcdir)/$(mtlk_ko_src_dir)/LICENSE.txt \
              linux/Kbuild \
              linux/compat.c linux/compat.h \
              linux/core.c \
              linux/core.h \
              linux/core_ab.c \
              linux/core_priv.h \
              linux/core_pdb.c \
              linux/core_pdb_def.h \
              linux/dfs_pdb_def.h \
              linux/mtlk_df_nbuf_impl.h \
              linux/l2nat.c linux/l2nat.h \
              linux/log_osdep.c linux/log_osdep.h \
              linux/mcast.c linux/mcast.h \
              linux/memdefs.h \
              linux/mib_osdep.c linux/mib_osdep.h \
              linux/mmb_ops.h \
              linux/mtlk_ahb_drv.c linux/mtlk_ahb_drv_decls.h linux/mtlk_ahb_drv_defs.h \
              linux/mtlk_pci_drv.c linux/mtlk_pci_drv_decls.h linux/mtlk_pci_drv_defs.h \
              linux/mtlk_dbg_osdep.c linux/mtlk_dbg_osdep.h \
              linux/mtlkdfdefs.h \
              linux/mtlk_df_priv.h \
              linux/mtlk_df_fw.c \
              linux/mtlk_dfg_linux.c linux/mtlk_df_linux.c \
              linux/mtlk_df_user.c \
              linux/mtlk_df_proc_impl.c \
              linux/mtlk_df_user_priv.h \
              linux/mtlk_df_user_tbl.c \
              linux/mtlk_fast_mem.c \
              linux/mtlklist_osdep_decls.h linux/mtlklist_osdep_defs.h \
              linux/mtlkpack.h \
              linux/mtlk_serializer_osdep.c linux/mtlk_serializer_osdep.h \
              linux/mtlk_sq_osdep.c linux/mtlk_sq_osdep.h \
              linux/osal_defs.c linux/osal_osdep_decls.h linux/osal_osdep_defs.h \
              linux/osal_atomic_decls.h linux/osal_atomic_defs.h \
              linux/osal_atomic64_decls.h linux/osal_atomic64_defs.h \
              linux/progmodel.c linux/progmodel.h \
              linux/rod_osdep.h \
              linux/sq.h \
              linux/utils.c linux/utils.h \
              linux/wpa.c linux/wpa.h \
              linux/mtlk_vap_manager.c linux/mtlk_vap_manager.h \
              shared/addba.c shared/addba.h \
              shared/aocs.h shared/aocshistory.c shared/aocshistory.h \
              shared/bitrate.c shared/bitrate.h \
              shared/cache.c shared/cache.h \
              shared/channels.h \
              shared/cpu_stat.c shared/cpu_stat.h \
              shared/dfs.h \
              shared/drvver.h \
              shared/eeprom.h \
              shared/frame.c shared/frame.h \
              shared/hw_mmb.c shared/hw_mmb.h \
              shared/mtlk_wss_id.h \
              shared/ieee80211defs.h \
              shared/iperf_debug.c shared/iperf_debug.h \
              shared/mtlk_ab_manager.c shared/mtlk_ab_manager.h \
              shared/mtlkaux.c shared/mtlkaux.h \
              shared/mtlk_card_selector.h \
              shared/mtlk_card_types.h \
              shared/mtlk_ccr_decls.h shared/mtlk_ccr_defs.h \
              shared/mtlk_clipboard.c shared/mtlk_clipboard.h \
              shared/mtlk_coc.c shared/mtlk_coc.h \
              shared/mtlk_core_iface.h \
              shared/mtlk_coreui.h \
              shared/mtlk_dbg.h \
              shared/mtlk_df.h \
              shared/mtlk_fast_mem.h \
              shared/mtlkflctrl.c shared/mtlkflctrl.h \
              shared/mtlk_g35_ccr_decls.h shared/mtlk_g35_ccr_defs.h shared/mtlk_g3_ccr_defs.h \
              shared/mtlk_param_db.c shared/mtlk_param_db.h shared/mtlk_param_db_def.c  \
              shared/mtlk_pcie_ccr.c shared/mtlk_pcie_ccr_decls.h shared/mtlk_pcie_ccr_defs.h \
              shared/mtlk_pcig3_ccr_decls.h shared/mtlk_pcig3_ccr_defs.h \
              shared/pcishram_ex.h shared/g3shram_ex.h \
              shared/mtlk_gpl_helper.h \
              shared/mtlkhal.h \
              shared/mtlk_irb_pinger.c shared/mtlk_irb_pinger.h \
              shared/mtlkmib.c shared/mtlkmib.h \
              shared/mtlk_mmb_msg.h \
              shared/mtlkmsg.h \
              shared/mtlknbufpriv.h \
              shared/mtlknbufstats.h shared/mtlknbufstats.c \
              shared/mtlk_osal_dbg.h \
              shared/mtlk_osal.h \
              shared/mtlk_packets.h \
              shared/mtlkparams.h \
              shared/mtlkqos.c shared/mtlkqos.h \
              shared/mtlk_rfmgmt.h \
              shared/mtlkrmlock.h \
              shared/mtlk_serializer.c shared/mtlk_serializer.h \
              shared/mtlk_snprintf.c shared/mtlk_snprintf.h \
              shared/mtlk_sq.c shared/mtlk_sq.h \
              shared/cbsmgr.h shared/coex20_40.h \
              shared/coexfrgen.h shared/coexlve.h \
              shared/scexempt.h shared/coex20_40priv.h \
              shared/rod.c shared/rod.h \
              shared/scan.c shared/scan.h \
              shared/shram_ex.h \
              shared/stadb.c shared/stadb.h \
              shared/txmm.c shared/txmm.h \
              shared/mtlk_analyzer.c shared/mtlk_analyzer.h \
              shared/MT_mac_host_adapt.h \
              $(tools_shared_srcdir)/mtlkirb_osdep.h \
              $(tools_srcdir)/mtidl/mtidl_c.h \
              $(wireless_shared_srcdir)/mtlk_assert.h \
              $(wireless_shared_srcdir)/mtlkwlanirbdefs.h \
              $(wireless_shared_srcdir)/mtlk_wss.h $(wireless_shared_srcdir)/mtlk_wss.c \
              shared/mtlk_wssd.h shared/mtlk_wssd.c \
              $(wireless_shared_srcdir)/mtlk_slid.h \
              linux/mtlkinc.h \
              linux/mtlkidefs.h
