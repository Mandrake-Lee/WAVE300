# $Id: Makefile.am 11832 2011-10-26 13:39:48Z fleytman $

include $(top_srcdir)/Makefile.am.common

SUBDIRS = 3rd_party linux

EXTRA_DIST = argv_parser.c aux_utils.cpp CmdLine.cpp Debug.cpp \
             mtlkcontainer.c mtlk_socket.c ParamInfo.cpp \
             mtlkirbhash.c

noinst_HEADERS = iniparseraux.h mtlkirba.h mtlkirb_osdep.h \
                 mtlk_osal.h mtlk_socket.h ParamInfo.h \
                 argv_parser.h aux_utils.h CmdLine.h Debug.h \
                 memdefs.h mtlkcontainer.h mtlk_osal_dbg.h \
                 mtlkirbhash.h \
                 $(tools_srcdir)/mtidl/mtidl_c.h

