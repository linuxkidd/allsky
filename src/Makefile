platform = $(shell uname -m)
prefix = 

sysconfdir = ${prefix}/etc
exec_prefix = /usr
bindir = ${exec_prefix}/bin
libexecdir = ${exec_prefix}/libexec/allsky
sharedir = ${exec_prefix}/share/allsky

PDIR = $(shell dirname "$$PWD")
MYDIR = $(shell basename "$$PWD")
NLU = $(shell basename "$(PDIR)")
RIGHTPATH = 1

ifneq (src,$(MYDIR))
  RIGHTPATH = 0
endif
ifneq (allsky,$(NLU))
  RIGHTPATH = 0
endif

ifneq (1,$(RIGHTPATH))
  @echo Unexpected directory structure.
  @echo This should be in allsky/src directory.
else

.DEFAULT_GOAL := all

ROOTCHECK=$(shell id -u)
ifneq ($(ROOTCHECK),0)
  ifeq ($(PKGBUILD),1)
    ROOTCHECK=0
  endif
endif

deps:
ifneq ($(shell id -u), 0)
	@echo This must be ran with root permissions.
	@echo Please run \'sudo make deps\'
else
	@echo `date +%F\ %R:%S` Installing build dependencies...
	@apt update && apt -y install libopencv-dev libusb-dev libusb-1.0-0-dev cmake ffmpeg gawk lftp jq imagemagick
endif

USB=$(shell pkg-config --cflags --libs libusb-1.0)
ifeq (,$(USB))
  $(error Did not find USB Libraries, try 'sudo make deps')
endif

DEFS = -D_LIN -D_DEBUG -DGLIBC_20
CFLAGS = -Wall -Wno-psabi -g -O2 -lpthread
OPENCV = $(shell pkg-config --exists opencv && pkg-config --cflags --libs opencv || (pkg-config --exists opencv4 && pkg-config --cflags --libs opencv4))

ifeq (,$(OPENCV))
  $(error Did not find any OpenCV Libraries, try 'sudo make deps')
endif

ZWOSDK = -I./include

ifeq ($(platform), armv6l)
  CC = arm-linux-gnueabihf-g++
  AR= arm-linux-gnueabihf-ar
  CFLAGS += -march=armv6
  CFLAGS += -lrt
  ZWOSDK += -Llib/armv6
endif

ifeq ($(platform), armv7l)
  CC = arm-linux-gnueabihf-g++
  AR= arm-linux-gnueabihf-ar
  CFLAGS += -march=armv7 -mthumb
  ZWOSDK = -Llib/armv7
endif

#Ubuntu 20.04 added by Jos Wennmacker
ifeq ($(platform), aarch64)
  CC = g++
  AR= ar
  ZWOSDK = -Llib/armv8
endif

ifeq ($(platform), x86_64)
  CC = g++
  AR= ar
  ZWOSDK = -Llib/x64
endif

ifeq ($(platform), i386)
  CC = g++
  AR= ar
  ZWOSDK = -Llib/x86
endif

RANLIB = ranlib

ifeq (,$(CC))
  $(warning Could not identify the proper compiler for your platform.)
  $(error Unknown platform $(platform))
endif

CFLAGS += $(DEFS) $(ZWOSDK)

all:libsunwait.a capture capture_RPiHQ startrails keogram

LIBSUN = -I./libsunwait-src -L./ -lsunwait

#libsunwait.o:libsunwait-src/libsunwait.cpp
#sun.o:libsunwait-src/sun.cpp
#sunarc.o:libsunwait-src/sunarc.cpp
%.o:libsunwait-src/%.cpp
	@echo `date +%F\ %R:%S` Building $@
	@$(CC) -c $? -o $@ $(CFLAGS) -UDEFAULT_LONGITUDE -UDEFAULT_LATITUDE -UNOT_SET -std=gnu++11 -I./libsunwait-src
	@echo `date +%F\ %R:%S` complete.

#.PHONY: build_sunwait

libsunwait.a:sun.o sunarc.o libsunwait.o 
	@$(AR) qc $@ $?
	@$(RANLIB) libsunwait.a

capture:capture.cpp libsunwait.a
	@echo `date +%F\ %R:%S` Building capture program...
	@$(CC) $? -o $@ $(CFLAGS) $(OPENCV) $(LIBSUN) -lASICamera2 $(USB)
	@echo `date +%F\ %R:%S` Done.

capture_RPiHQ:capture_RPiHQ.cpp
	@echo `date +%F\ %R:%S` Building capture_RPiHQ program...
	@$(CC) $? -o $@ $(CFLAGS) $(OPENCV) $(LIBSUN)
	@echo `date +%F\ %R:%S` Done.

startrails:startrails.cpp
	@echo `date +%F\ %R:%S` Building startrails program...
	@$(CC) $? -o $@ $(CFLAGS) $(OPENCV)
	@echo `date +%F\ %R:%S` Done.

keogram:keogram.cpp
	@echo `date +%F\ %R:%S` Building keogram program...
	@$(CC) $? -o $@ $(CFLAGS) $(OPENCV)
	@echo `date +%F\ %R:%S` Done.

install:
ifneq ($(ROOTCHECK), 0)
	@echo This must be ran with root permissions.
	@echo Please run \'sudo make install\'
else
	@echo `date +%F\ %R:%S` Copying binaries...
	@if [ $(PKGBUILD) -eq 1 ]; then \
	  [ ! -e $(DESTDIR)$(bindir) ] && mkdir -p $(DESTDIR)$(bindir); \
	  install capture $(DESTDIR)$(bindir); \
	  install capture_RPiHQ $(DESTDIR)$(bindir); \
	  install keogram $(DESTDIR)$(bindir); \
	  install startrails $(DESTDIR)$(bindir); \
	else \
	  install -o $(SUDO_USER) -g $(SUDO_USER) capture ../; \
	  install -o $(SUDO_USER) -g $(SUDO_USER) capture_RPiHQ ../; \
	  install -o $(SUDO_USER) -g $(SUDO_USER) keogram ../; \
	  install -o $(SUDO_USER) -g $(SUDO_USER) startrails ../; \
	fi
endif # sudo / root check

clean:
	rm -f capture capture_RPiHQ startrails keogram *.o *.a

endif # Correct directory structure check
