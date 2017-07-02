
RM= rm -rf
CC= gcc
CXX= g++

CFLAGS= -O2 -c -MD -Wall -Wstrict-prototypes -fomit-frame-pointer
CXXFLAGS= -O2 -c -MD -Wall -fomit-frame-pointer

COMMON_SRC_DIR = .
COMMON_BIN_DIR = .

CONF_SRC_DIR = .
CONF_BIN_DIR = .

MCONF_SRC_DIR = .
MCONF_BIN_DIR = .

QCONF_SRC_DIR = .
QCONF_BIN_DIR = .

LXDIALOG_SRC_DIR = lxdialog
LXDIALOG_BIN_DIR = lxdialog

CONF_ELF = $(CONF_BIN_DIR)/conf
MCONF_ELF = $(MCONF_BIN_DIR)/mconf
QCONF_ELF = $(QCONF_BIN_DIR)/qconf

#########################
all: config menuconfig xconfig

menuconfig: $(MCONF_ELF) 

config: $(CONF_ELF)

xconfig: $(QCONF_ELF) 

clean:
	$(RM) $(MCONF_ELF) $(CONF_ELF) $(QCONF_ELF) \
	$(COMMON_OBJFILES) $(COMMON_DEPFILES) \
	$(CONF_OBJFILES) $(CONF_DEPFILES) \
	$(LXDIALOG_OBJFILES) $(LXDIALOG_DEPFILES)\
	$(MCONF_OBJFILES) $(MCONF_DEPFILES) \
	$(QCONF_OBJFILES) $(QCONF_DEPFILES) \
	$(QCONF_CXXOBJFILES) $(QCONF_CXXDEPFILES) \
	$(QCONF_BIN_DIR)/.tmp_qtcheck 

############################
# commonn source files
COMMON_SRCFILES = $(COMMON_SRC_DIR)/zconf.tab.c

COMMON_OBJFILES := $(COMMON_SRCFILES)
COMMON_OBJFILES := $(patsubst %.c,$(COMMON_BIN_DIR)/%.o,$(COMMON_OBJFILES))

COMMON_DEPFILES := $(COMMON_SRCFILES)
COMMON_DEPFILES := $(patsubst %.c,$(COMMON_BIN_DIR)/%.d,$(COMMON_DEPFILES))

############################
# conf
CONF_SRCFILES = $(CONF_SRC_DIR)/conf.c
#				zconf.tab.c

CONF_OBJFILES := $(CONF_SRCFILES)
CONF_OBJFILES := $(patsubst %.c,$(CONF_BIN_DIR)/%.o,$(CONF_OBJFILES))

CONF_DEPFILES := $(CONF_SRCFILES)
CONF_DEPFILES := $(patsubst %.c,$(CONF_BIN_DIR)/%.d,$(CONF_DEPFILES))


#########################
# mconf
MCONF_SRCFILES = $(MCONF_SRC_DIR)/mconf.c
#				zconf.tab.c

MCONF_OBJFILES := $(MCONF_SRCFILES)
MCONF_OBJFILES := $(patsubst %.c, %.o, $(MCONF_OBJFILES))

MCONF_DEPFILES := $(MCONF_SRCFILES)
MCONF_DEPFILES := $(patsubst %.c, %.d, $(MCONF_DEPFILES))

MCONF_INCLUDES = -I/usr/include/ncurses
MCONF_DEFINES = -DLOCALE -DCURSES_LOC="<ncurses.h>"
MCONF_LIBS = -lncurses -ldl

############################
# lxdialog
LXDIALOG_SRCFILES = $(LXDIALOG_SRC_DIR)/checklist.c \
					$(LXDIALOG_SRC_DIR)/inputbox.c \
					$(LXDIALOG_SRC_DIR)/menubox.c \
					$(LXDIALOG_SRC_DIR)/textbox.c \
					$(LXDIALOG_SRC_DIR)/util.c \
					$(LXDIALOG_SRC_DIR)/yesno.c

LXDIALOG_OBJFILES := $(LXDIALOG_SRCFILES)
LXDIALOG_OBJFILES := $(patsubst %.c, %.o, $(LXDIALOG_OBJFILES))

LXDIALOG_DEPFILES := $(LXDIALOG_SRCFILES)
LXDIALOG_DEPFILES := $(patsubst %.c, %.d, $(LXDIALOG_SRCFILES))


############################
# qconf
QCONF_SRCFILES = $(QCONF_SRC_DIR)/kconfig_load.c 
#                 zconf.tab.c
				

QCONF_OBJFILES := $(QCONF_SRCFILES)
QCONF_OBJFILES := $(patsubst %.c, %.o, $(QCONF_OBJFILES))

QCONF_DEPFILES := $(QCONF_SRCFILES)
QCONF_DEPFILES := $(patsubst %.c, %.d, $(QCONF_SRCFILES))

QCONF_CXXSRCFILES = qconf.cc

QCONF_CXXOBJFILES := $(QCONF_CXXSRCFILES)
QCONF_CXXOBJFILES := $(patsubst %.cc, %.o, $(QCONF_CXXOBJFILES))

QCONF_CXXDEPFILES := $(QCONF_CXXSRCFILES)
QCONF_CXXDEPFILES := $(patsubst %.cc, %.d, $(QCONF_CXXSRCFILES))

QDEFINES= -DLKC_DIRECT_LINK -DQT_SHARED -DQT_TABLET_SUPPORT -DQT_NO_DEBUG -DQT_THREAD_SUPPORT -D_REENTRANT

ifeq ($(MAKECMDGOALS),xconfig)
-include $(QCONF_BIN_DIR)/.tmp_qtcheck
endif

ifeq ($(MAKECMDGOALS),all)
-include $(QCONF_BIN_DIR)/.tmp_qtcheck
endif

ifeq ($(MAKECMDGOALS),)
-include $(QCONF_BIN_DIR)/.tmp_qtcheck
endif

$(QCONF_BIN_DIR)/.tmp_qtcheck:
	@set -e; echo "  CHECK   qt"; dir=""; pkg=""; \
	pkg-config --exists qt 2> /dev/null && pkg=qt; \
	pkg-config --exists qt-mt 2> /dev/null && pkg=qt-mt; \
	if [ -n "$$pkg" ]; then \
	  cflags="\$$(shell pkg-config $$pkg --cflags)"; \
	  libs="\$$(shell pkg-config $$pkg --libs)"; \
	  dir="$$(pkg-config $$pkg --variable=prefix)"; \
	else \
	  for d in $$QTDIR /usr/share/qt* /usr/lib/qt*; do \
	    if [ -f $$d/include/qconfig.h ]; then dir=$$d; break; fi; \
	  done; \
	  if [ -z "$$dir" ]; then \
	    echo "*"; \
	    echo "* Unable to find the QT3 installation. Please make sure that"; \
	    echo "* the QT3 development package is correctly installed and"; \
	    echo "* either install pkg-config or set the QTDIR environment"; \
	    echo "* variable to the correct location."; \
	    echo "*"; \
	    false; \
	  fi; \
	  libpath=$$dir/lib; lib=qt; osdir=""; \
	  $(CXX) -print-multi-os-directory > /dev/null 2>&1 && \
	    osdir=x$$($(CXX) -print-multi-os-directory); \
	  test -d $$libpath/$$osdir && libpath=$$libpath/$$osdir; \
	  test -f $$libpath/libqt-mt.so && lib=qt-mt; \
	  cflags="-I$$dir/include"; \
	  libs="-L$$libpath -Wl,-rpath,$$libpath -l$$lib"; \
	fi; \
	echo "KC_QT_CFLAGS=$$cflags" > $@; \
	echo "KC_QT_LIBS=$$libs" >> $@;

############################
# compile common files (zconf.tab.c)
$(COMMON_OBJFILES): %.o: %.c
	@echo '[$<]'
	$(CC) $(CFLAGS) -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o $@ $<

# compile "conf 'c'" files
$(CONF_OBJFILES): %.o: %.c
	@echo '[$<]'
	$(CC) $(CFLAGS) -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o $@ $<

# compile "mconf 'c'" files
$(MCONF_OBJFILES): %.o: %.c
	@echo '[$<]'
	$(CC) $(MCONF_INCLUDES) $(MCONF_DEFINES) $(CFLAGS) -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o $@ $<

# compile "lxdialog 'c'" files
$(LXDIALOG_OBJFILES): %.o: %.c
	@echo '[$<]'
	$(CC) $(MCONF_INCLUDES) $(MCONF_DEFINES) $(CFLAGS) -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o $@ $<

# compile "qconf 'c'" files
$(QCONF_OBJFILES): %.o: %.c
	@echo '[$<]'
	$(CC) $(CFLAGS) $(KC_QT_CFLAGS) -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o $@ $<

# compile "qt 'c++'" files
$(QCONF_CXXOBJFILES): %.o: %.cc
	@echo '[$<]'
	$(CXX) $(QDEFINES) $(CXXFLAGS) $(KC_QT_CFLAGS) -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o $@ $<

############################
# link .conf
$(CONF_ELF): $(COMMON_OBJFILES) $(CONF_OBJFILES)
	@echo 'Linking $(CONF_ELF)'
	$(CC) $(COMMON_OBJFILES) $(CONF_OBJFILES) -o $(CONF_ELF) 
	@echo

# link .mconf
$(MCONF_ELF): $(COMMON_OBJFILES) $(LXDIALOG_OBJFILES) $(MCONF_OBJFILES)
	@echo 'Linking $(MCONF_ELF)'
	$(CC) $(COMMON_OBJFILES) $(MCONF_OBJFILES) $(LXDIALOG_OBJFILES) $(MCONF_LIBS) -o $(MCONF_ELF) 
	@echo

# link .qconf
$(QCONF_ELF): $(COMMON_OBJFILES) $(QCONF_OBJFILES) $(QCONF_CXXOBJFILES)
	@echo 'Linking $(QCONF_ELF)'
	$(CC) $(COMMON_OBJFILES) $(QCONF_OBJFILES) $(QCONF_CXXOBJFILES) $(KC_QT_LIBS) -o $(QCONF_ELF) -lstdc++
	@echo

############################
# dependencies
ifneq ($(strip $(COMMON_DEPFILES)),)
-include $(COMMON_DEPFILES)
endif

ifneq ($(strip $(CONF_DEPFILES)),)
-include $(CONF_DEPFILES)
endif

ifneq ($(strip $(MCONF_DEPFILES)),)
-include $(MCONF_DEPFILES)
endif

ifneq ($(strip $(QCONF_DEPFILES)),)
-include $(QCONF_DEPFILES)
endif

ifneq ($(strip $(LXDIALOG_DEPFILES)),)
-include $(LXDIALOG_DEPFILES)
endif
