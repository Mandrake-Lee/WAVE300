dnl $Id: configure.ac 11631 2011-09-18 14:29:22Z fleytman $

AC_PREREQ(2.63)
m4_include([branch_version.m4])
AC_INIT([mtlk], [MTLK_BRANCH_VERSION], [])
AC_CONFIG_SRCDIR([mtlk_version])
AC_CONFIG_HEADER([config.h])

AC_CANONICAL_HOST

AC_MSG_CHECKING([for toolchain to use])
AC_ARG_WITH(app-toolchain,
            AS_HELP_STRING([--with-app-toolchain],
                           [use specified aplication toolchain instead of default one]),
            [override_app_toolchain=$withval],
            [unset override_app_toolchain])
AS_IF([test "x$override_app_toolchain" = x],
      [AC_MSG_RESULT(default)],
      [AC_MSG_RESULT($override_app_toolchain)])


AC_MSG_CHECKING([for kernel to use])
AC_ARG_WITH(kernel,
            AS_HELP_STRING([--with-kernel],
                           [use specified kernel instead of default one]),
            [override_kernel=$withval],
            [unset override_kernel])
AS_IF([test "x$override_kernel" = x],
      [AC_MSG_RESULT(default)],
      [AC_MSG_RESULT($override_kernel)])

AC_MSG_CHECKING([for kernel compiler prefix to use])
AC_ARG_WITH(kernel-cross-compile,
            AS_HELP_STRING([--with-kernel-cross-compile],
                           [use specified kernel compiler prefix instead of default one]),
            [override_kernel_cross_compile=$withval],
            [unset override_kernel_cross_compile])
AS_IF([test "x$override_kernel_cross_compile" = x],
      [AC_MSG_RESULT(default)],
      [AC_MSG_RESULT($override_kernel_cross_compile)])

AC_MSG_CHECKING([for prebuilt rflib to use])
AC_ARG_WITH(prebuilt-rflib,
            AS_HELP_STRING([--with-prebuilt-rflib],
                           [use specified prebuilt rflib]),
            [prebuilt_rflib=$withval],
            [unset prebuilt_rflib])
AS_IF([test "x$prebuilt_rflib" = x],
      [AC_MSG_RESULT(none)],
      [AC_MSG_RESULT($prebuilt_rflib)])

AM_CONDITIONAL([USE_PREBUILT_RFLIB],
               [test "x$prebuilt_rflib" != x])

AC_SUBST(prebuilt_rflib)

. $(pwd)/.config

AC_MSG_CHECKING([for environment configuration file])
AC_MSG_RESULT([${CONFIG_ENVIRONMENT_NAME}])

get_toolchain_path()
{
  echo ${override_app_toolchain:-$DEFAULT_TOOLCHAIN_PATH}
}

get_kernel_dir()
{
  echo ${override_kernel:-$DEFAULT_KERNEL_DIR}
}

get_kernel_cross_compile()
{
  echo ${override_kernel_cross_compile:-$DEFAULT_KERNEL_CROSS_COMPILE}
}

. ${srcdir}/support/${CONFIG_ENVIRONMENT_NAME}.env

AC_SUBST(CUSTOM_APP_ENV_INFO)
AC_SUBST([APP_TOOLCHAIN_PATH], [$(get_toolchain_path)])

kernel_dir=$(get_kernel_dir)

# MTLK_TOOLCHAIN_COMPONENT_PATH(COMPONENT-NAME, VAR-NAME)
# --------------------------------------
AC_DEFUN([MTLK_TOOLCHAIN_COMPONENT_PATH],
         [{ AC_MSG_CHECKING([for toolchain [$1]])
            AS_IF([test x"${[$2]}" = x""],
                  [AS_IF([test x"`echo $CC | grep 'gcc$'`" != x""],
                          [[$2]=`echo $CC | sed -e's/gcc$/[$1]/g'`], [])
                   AS_IF([test -x "${[$2]}"],
                         [], [[$2]=""])],
                  [])
            AC_MSG_RESULT([${[$2]}]);
          }])

MTLK_TOOLCHAIN_COMPONENT_PATH(strip, STRIP)
MTLK_TOOLCHAIN_COMPONENT_PATH(ranlib, RANLIB)

AC_SUBST([CONFIG_STATUS_DEPENDENCIES], [`readlink -f ${srcdir}/support/${CONFIG_ENVIRONMENT_NAME}.env`])

ENV_CFLAGS="$CFLAGS"
ENV_CPPFLAGS="$CPPFLAGS"
ENV_CXXFLAGS="$CXXFLAGS"
ENV_LDFLAGS="$LDFLAGS"

# MTLK_CONDITIONAL_COMPONENT(CONDITIONAL-VAR, CONFIG-VAR, COMPONENT-NAME)
# --------------------------------------
AC_DEFUN([MTLK_CONDITIONAL_COMPONENT],
         [{ AM_CONDITIONAL([$1], [test "x${[$2]}" = xy ])
            AC_MSG_CHECKING([whether to build [$3]])
            AS_IF([test x${[$2]} = xy], [AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no)])
          }])

MTLK_CONDITIONAL_COMPONENT([BUILD_DRIVER], [CONFIG_LIN_DRV], [the wireless driver])

AC_MSG_CHECKING([for supported hardware set])
supported_hw_list=`set | grep CONFIG_LINDRV_HW_.\*=y | \
                         sed -e's/CONFIG_LINDRV_HW_//g' -e's/=y//g' | \
                         awk '{printf $0 " "}'`
AM_CONDITIONAL([AT_LEAST_ONE_HW_SUPPORTED],
               [test "x$supported_hw_list" != x])
AM_COND_IF([BUILD_DRIVER],
           [AM_COND_IF([AT_LEAST_ONE_HW_SUPPORTED],
                       [AC_MSG_RESULT([$supported_hw_list])],
                       [AC_MSG_ERROR([At least one hardware type must be supported by Linux driver])]
                      )
           ]
          )

MTLK_CONDITIONAL_COMPONENT([BUILD_MTTOOLS], [CONFIG_LIN_MTTOOLS], [the MTTools])

MTLK_CONDITIONAL_COMPONENT([BUILD_RTLOGGER], [CONFIG_LIN_RTLOG_COMPONENTS],
                           [remote logging tools])

MTLK_CONDITIONAL_COMPONENT([BUILD_DUT], [CONFIG_LIN_DUT],
                           [the DUT components])

MTLK_CONDITIONAL_COMPONENT([BUILD_HELPER_TOOLS], [CONFIG_LIN_HELPER_TOOLS],
                           [helper tools])

AM_CONDITIONAL([BUILD_WIRELESS_SHARED],
               [test "x${CONFIG_LIN_RTLOG_COMPONENTS}" = xy || \
                test "x${CONFIG_LIN_MTTOOLS}" = xy \
              ])

AM_CONDITIONAL([BUILD_WIRELESS_DATA],
               [test "x${CONFIG_LIN_DRV}" = xy || \
                test "x${CONFIG_LIN_MTTOOLS}" = xy \
              ])

AC_SUBST(cxx_ldadd)

# MTLK_CHECK_BUILDER_COMPONENT(COMPONENT-PATH, COMPONENT-NAME, [ACTION-IF-EMPTY=AC_MSG_ERROR(...)])
# --------------------------------------
AC_DEFUN([MTLK_CHECK_BUILDER_COMPONENT],
         [{ AC_MSG_CHECKING([for $2])
            AS_IF([test x[$1] = x],
                  [
                      AC_MSG_RESULT([not specified])
                      m4_default([$3], [AC_MSG_ERROR([Path to $2 not specified])])
                  ],
                  [
                    AS_IF([test -d [$1]],
                          [
                            _mtlk_processed_path=$1
                            AC_MSG_RESULT([${_mtlk_processed_path}])
                          ],
                          [
                            AC_MSG_RESULT([no])
                            _mtlk_processed_path=$1
                            AC_MSG_ERROR([$2 (${_mtlk_processed_path}) does not exist])
                          ])
                  ])
         }])

MTLK_CHECK_BUILDER_COMPONENT(${kernel_dir}, [target kernel package])

AC_MSG_CHECKING([for target kernel compiler])

AS_IF([which $(get_kernel_cross_compile)gcc >/dev/null],
      [ 
         target_kernel_gcc=`which $(get_kernel_cross_compile)gcc`
         AC_MSG_RESULT([${target_kernel_gcc}])
      ], 
      [AC_MSG_ERROR([Target kernel compiler not found])])

AC_SUBST(kernel_dir)
AC_SUBST(kernel_cross_compile, $(get_kernel_cross_compile))
AC_SUBST(kernel_cflags)

AS_IF([test "x$kernel_arch" != "x"],
      [kernel_arch_var="ARCH=$kernel_arch"], 
      [kernel_arch_var=""])

AC_SUBST(kernel_arch_var)
AC_SUBST(CUSTOM_DRV_ENV_INFO)

env_lib_path="$LD_LIBRARY_PATH"
AC_SUBST(env_lib_path)
export LD_LIBRARY_PATH

AM_INIT_AUTOMAKE([dist-bzip2 foreign])

dnl For some reason configure script uses $ECHO variable
dnl which is empty. Probably bug in autoconf
AC_PATH_PROG([ECHO], [echo], [echo])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB

AC_CHECK_PROG(HAVE_READLINK, [readlink], [yes], [no])
AS_IF([test x${HAVE_READLINK} != xyes],
      [AC_MSG_ERROR([readlink tool must be present and accessible via PATH variable])],
      [])

CC="`cd $srcdir; pwd`/support/mtlkfront.sh $CC"
CXX="`cd $srcdir; pwd`/support/mtlkfront.sh $CXX"

#These variables will be used by mtlk_front.sh in following tests
export abs_builddir=$PWD
export abs_top_srcdir=`cd $srcdir; pwd`
export abs_tools_srcdir=$abs_top_srcdir/tools

AC_MSG_CHECKING([whether compiler frontend works for C programs])
AC_TRY_LINK(, [], [AC_MSG_RESULT([yes])], [AC_MSG_ERROR([no])])
AC_LANG_PUSH([C++])
AC_MSG_CHECKING([whether compiler frontend works for C++ programs])
AC_TRY_LINK(, [], [AC_MSG_RESULT([yes])], [AC_MSG_ERROR([no])])
AC_LANG_POP()

BUILDER_CC="`cd $srcdir; pwd`/support/mtlkfront.sh gcc"
BUILDER_CXX="`cd $srcdir; pwd`/support/mtlkfront.sh g++"
AC_SUBST(BUILDER_CC)
AC_SUBST(BUILDER_CXX)

cc_pic=-fPIC
AC_SUBST(cc_pic)
cc_shared=-shared
AC_SUBST(cc_shared)

ld_group_start=-Wl,--start-group
AC_SUBST(ld_group_start)
ld_group_end=-Wl,--end-group
AC_SUBST(ld_group_end)


dnl Does this compiler have built-in functions for atomic memory access?
AC_MSG_CHECKING([whether platform supports built-in atomics])
AC_TRY_LINK(,
 [
   int variable = 1;
   return (__sync_add_and_fetch(&variable, 1)
           && __sync_sub_and_fetch(&variable, 1)
             && __sync_lock_test_and_set(&variable, 1)) ? 1 : 0;
 ],
 [
   AC_MSG_RESULT([yes])
   AC_DEFINE(HAVE_BUILTIN_ATOMIC, 1, [Define to 1 if gcc supports builin atomics])
 ],
 [
   AC_MSG_RESULT([no])
 ])

AC_DEFINE(LINUX, 1, [Define as 1 to tell that we are building on Linux])
AC_DEFINE(MTLK_COPYRIGHT, ["Copyright(c) 2006-2010 Metalink Broadband"],
                          [Standard copyright string])

CFLAGS="$ENV_CFLAGS"
CPPFLAGS="$ENV_CPPFLAGS"
CXXFLAGS="$ENV_CXXFLAGS"
LDFLAGS="$ENV_LDFLAGS"

AC_CONFIG_FILES([Makefile
		 tools/Makefile
		 tools/dutserver/Makefile
		 tools/dutserver/linux/Makefile
		 tools/shared/Makefile
		 tools/shared/3rd_party/Makefile
		 tools/shared/3rd_party/iniparser/Makefile
		 tools/shared/linux/Makefile
		 wireless/Makefile
		 wireless/shared/Makefile
		 wireless/driver/Makefile
		 wireless/driver/rflib/Makefile
		 tools/rtlogger/Makefile
		 tools/rtlogger/shared/Makefile
		 tools/rtlogger/logserver/Makefile
		 tools/rtlogger/logserver/linux/Makefile
		 tools/rtlogger/logcnv/Makefile
		 tools/rtlogger/prebuilt/Makefile
		 tools/mtlkroot/Makefile
		 tools/mttools/Makefile
		 tools/mttools/shared/Makefile
		 tools/mttools/drvhlpr/Makefile
		 tools/mttools/mtdump/Makefile
		 tools/mttools/mtlk_cli/Makefile
		 tools/mttools/mtlk_cli_dbg_srv/Makefile
		 tools/BCLSockServer/Makefile
                 ])

AC_OUTPUT
