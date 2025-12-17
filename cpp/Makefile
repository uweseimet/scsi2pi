##---------------------------------------------------------------------------
##
## SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
##
## Copyright (C) 2023-2025 Uwe Seimet
##
##---------------------------------------------------------------------------

.DEFAULT_GOAL: all

## Optional build flags:
##   CROSS_COMPILE= Specifies the compiler toolchain, e.g.
##                  arm-linux-gnueabihf-
CROSS_COMPILE =

CXX = $(CROSS_COMPILE)g++
AR = $(CROSS_COMPILE)gcc-ar
STRIP = $(CROSS_COMPILE)strip

##   DEBUG=1       A Debug build includes the debugger symbols and
##                 disables compiler optimization. This is usually
##                 only by developers.
DEBUG ?= 0
ifeq ($(DEBUG), 1)
# Debug compiler flags
	CXXFLAGS += -O0 -g -DDEBUG
else
# Release compiler flags
	CXXFLAGS += -O3 -Wall -DNDEBUG
endif

CXXFLAGS += -std=c++20 -I. -D_FILE_OFFSET_BITS=64 -DFMT_HEADER_ONLY -DSPDLOG_FMT_EXTERNAL -MD -MP -Wall -Wextra

ifeq ("$(shell uname -s)", "Linux")
# -Wno-psabi might not work on non-Linux platforms
	CXXFLAGS += -Wno-psabi
	IS_LINUX = 1
endif

ifeq ("$(shell uname -s)", "FreeBSD")
	AR = $(CROSS_COMPILE)ar
	CXXFLAGS += -fexperimental-library -I/usr/local/include -Wno-unused-parameter -Wno-macro-redefined
endif

ifeq ("$(shell uname -s)", "NetBSD")
	AR = $(CROSS_COMPILE)ar
	CXXFLAGS += -I/usr/pkg/include -Wno-unused-parameter -Wno-macro-redefined
endif

ifeq ("$(shell uname -s)", "Darwin")
	AR = $(CROSS_COMPILE)ar
	CXXFLAGS += -Wno-unused-parameter
endif

## EXTRA_FLAGS Can be used to pass special purpose flag
CXXFLAGS += $(EXTRA_FLAGS)

## Additional libraries are required since protobuf version 22
ifeq ($(shell expr `protoc --version | sed -e 's/3\.//g' | cut -d ' ' -f 2` \>= 22), 1)
	ABSEIL_LIBS = -labsl_log_internal_check_op -labsl_log_internal_message -labsl_status -labsl_log_internal_nullguard -labsl_hash
endif

## Installation path
##   The path to install binaries and manpages in, default is /opt/scsi2pi
TARGET := /opt/scsi2pi

##   BOARD=FULLSPEC
##     Specifies the board type that you are using. Typically this is 
##     STANDARD or FULLSPEC. The default is FULLSPEC.
BOARD ?= FULLSPEC

CXXFLAGS += -DBOARD_$(BOARD)

##   ENABLE_*=1
##   DISABLE_*=1
##     Specifies the device types to build or not to build, default is to build all. Supported types:
##       SCHD SCSI Removable and Non-removable Hard Drive
##       SCCD SCSI CD-Rom Drive
##       SCTP SCSI Tape Drive
##       SCMO SCSI Optical Memory
##       SCDP DaynaPort Network Adapter
##       SCLP SCSI Printer
##       SCSG Linux Generic SCSI (e.g. for SCSI-to-USB bridge)
##       SCHS Host Services (e.g. for realtime clock and shutdown)
##       SAHD SASI Hard Drive
ifeq ($(or $(ENABLE_SCHD),$(ENABLE_SCMO),$(ENABLE_SCCD),$(ENABLE_SCTP),$(ENABLE_SCDP),$(ENABLE_SCLP),$(ENABLE_SCSG),$(ENABLE_SCHS),$(ENABLE_SAHD)),)
	ENABLE_SCHD = 1
	ENABLE_SCMO = 1
	ENABLE_SCCD = 1
	ENABLE_SCTP = 1
	ENABLE_SCDP = 1
	ENABLE_SCLP = 1
	ENABLE_SCSG = 1
	ENABLE_SCHS = 1
	ENABLE_SAHD = 1
endif

ifndef IS_LINUX
	ENABLE_SCSG =
endif

define disable_type
	ifdef DISABLE_$1
		ENABLE_$1 =
	endif
endef

define add_cxx_flag
	ifdef ENABLE_$1
		CXXFLAGS += -DBUILD_$1
	endif
endef

$(foreach var, SCHD SCMO SCCD SCTP SCDP SCLP SCSG SCHS SAHD, $(eval $(call disable_type,$(var))))

$(foreach var, SCHD SCRM SCMO SCCD SCTP SCDP SCLP SCSG SCHS SAHD, $(eval $(call add_cxx_flag,$(var))))

ifeq ($(or $(ENABLE_SCHD),$(ENABLE_SCRM),$(ENABLE_SCMO),$(ENABLE_SCCD),$(ENABLE_SAHD),$(ENABLE_SCTP)),1)
	CXXFLAGS += -DBUILD_STORAGE_DEVICE
endif

ifeq ($(or $(ENABLE_SCHD),$(ENABLE_SCRM),$(ENABLE_SCMO),$(ENABLE_SCCD),$(ENABLE_SAHD)),1)
	CXXFLAGS += -DBUILD_DISK
endif

S2P := s2p
S2PCTL := s2pctl
S2PDUMP := s2pdump
S2PEXEC := s2pexec
S2PPROTO := s2pproto
S2PSIMH := s2psimh
S2PFORMAT := s2pformat
S2PTOOL := s2ptool
S2P_TEST := s2p_test

INSTALL_BIN := $(TARGET)/bin

OBJDIR := obj
LIBDIR := lib
BINDIR := bin

BIN_ALL = \
	$(BINDIR)/$(S2P) \
	$(BINDIR)/$(S2PCTL) \
	$(BINDIR)/$(S2PTOOL)
# s2pdump, s2pexec and s2pproto require initiator support
ifeq ($(BOARD), FULLSPEC)
	BIN_ALL += $(BINDIR)/$(S2PDUMP)
	BIN_ALL += $(BINDIR)/$(S2PEXEC)
	BIN_ALL += $(BINDIR)/$(S2PPROTO)
endif
# s2psimh only makes sense with SCTP support
ifeq ($(ENABLE_SCTP),1)
	BIN_ALL += $(BINDIR)/$(S2PSIMH)
endif
# s2pformat requires the Linux SG 3 driver
ifeq ($(IS_LINUX),1)
	BIN_ALL += $(BINDIR)/$(S2PFORMAT)
endif

DIR_SHARED := shared
DIR_SHARED_PROTOBUF := protobuf
DIR_SHARED_COMMAND := command
DIR_SHARED_INITIATOR := initiator
DIR_BASE := base
DIR_BUSES := buses
DIR_CONTROLLERS := controllers
DIR_DEVICES := devices
DIR_PI := pi

PROTO_API := ../api/s2p_interface.proto

SRC_API = $(GENERATED_DIR)/s2p_interface.pb.cpp

SRC_SHARED = $(shell ls -1 $(DIR_SHARED)/*.cpp | grep -v sg_adapter)
ifdef IS_LINUX
SRC_SHARED += $(wildcard $(DIR_SHARED)/sg_adapter.cpp)
endif
SRC_PI := $(wildcard $(DIR_PI)/*.cpp)
SRC_SHARED_PROTOBUF := $(wildcard $(DIR_SHARED_PROTOBUF)/*.cpp)
SRC_SHARED_COMMAND := $(wildcard $(DIR_SHARED_COMMAND)/*.cpp)
SRC_SHARED_INITIATOR := $(wildcard $(DIR_SHARED_INITIATOR)/*.cpp)
SRC_BASE = $(wildcard $(DIR_BASE)/*.cpp)
SRC_BUSES = $(wildcard $(DIR_BUSES)/*.cpp)
SRC_CONTROLLERS = $(wildcard $(DIR_CONTROLLERS)/*.cpp)

SRC_DISK = \
	$(DIR_DEVICES)/disk.cpp \
	$(DIR_DEVICES)/linux_cache.cpp \
	$(DIR_DEVICES)/disk_cache.cpp \
	$(DIR_DEVICES)/disk_track.cpp \
	$(DIR_DEVICES)/storage_device.cpp \
	$(DIR_DEVICES)/page_handler.cpp

ifeq ($(ENABLE_SCHD),1)
SRC_SCHD = \
	$(DIR_DEVICES)/scsi_hd.cpp \
	$(DIR_DEVICES)/storage_device.cpp \
	$(DIR_DEVICES)/page_handler.cpp
SRC_SCHD += $(SRC_DISK)
endif

ifeq ($(ENABLE_SCMO),1)
SRC_SCMO = \
	$(DIR_DEVICES)/optical_memory.cpp \
	$(DIR_DEVICES)/storage_device.cpp \
	$(DIR_DEVICES)/page_handler.cpp
SRC_SCMO += $(SRC_DISK)
endif

ifeq ($(ENABLE_SCCD),1)
SRC_SCCD = \
	$(DIR_DEVICES)/scsi_cd.cpp \
	$(DIR_DEVICES)/storage_device.cpp \
	$(DIR_DEVICES)/page_handler.cpp
SRC_SCCD += $(SRC_DISK)
endif

ifeq ($(ENABLE_SCTP),1)
SRC_SCTP = \
	$(DIR_DEVICES)/tape.cpp \
	$(DIR_DEVICES)/storage_device.cpp \
	$(DIR_DEVICES)/page_handler.cpp
endif

ifeq ($(ENABLE_SCDP),1)
SRC_SCDP = \
	$(DIR_DEVICES)/daynaport.cpp \
	$(DIR_DEVICES)/tap_driver.cpp
endif

ifeq ($(ENABLE_SCLP),1)
SRC_SCLP = \
	$(DIR_DEVICES)/printer.cpp
endif

ifeq ($(ENABLE_SCSG),1)
SRC_SCSG = \
	$(DIR_DEVICES)/scsi_generic.cpp
endif

ifeq ($(ENABLE_SCHS),1)
SRC_SCHS = \
	$(DIR_DEVICES)/host_services.cpp \
	$(DIR_DEVICES)/page_handler.cpp
endif

ifeq ($(ENABLE_SAHD),1)
SRC_SAHD = \
	$(DIR_DEVICES)/sasi_hd.cpp \
	$(DIR_DEVICES)/storage_device.cpp \
	$(DIR_DEVICES)/page_handler.cpp
SRC_SAHD += $(SRC_DISK)
endif

SRC_S2P_CORE = $(shell ls -1 s2p/*.cpp | grep -v s2p.cpp)
SRC_S2P = s2p/s2p.cpp

SRC_S2PCTL_CORE = $(shell ls -1 s2pctl/*.cpp | grep -v s2pctl.cpp)
SRC_S2PCTL = s2pctl/s2pctl.cpp

SRC_S2PDUMP = s2pdump/s2pdump.cpp
SRC_S2PDUMP += $(shell ls -1 s2pdump/*.cpp | grep -v sg_ | grep -v s2pdump.cpp)
ifdef IS_LINUX
SRC_S2PDUMP += $(wildcard s2pdump/sg_*.cpp)
endif

SRC_S2PEXEC := $(wildcard s2pexec/*.cpp)

SRC_S2PPROTO := $(wildcard s2pproto/*.cpp)

SRC_S2PSIMH := $(wildcard s2psimh/*.cpp)

SRC_S2PFORMAT := $(wildcard s2pformat/*.cpp)

SRC_S2P_TEST = $(shell ls -1 test/*.cpp | grep -v scsi_generic_test.cpp)
ifdef IS_LINUX
SRC_S2P_TEST += test/scsi_generic_test.cpp
endif

SRC_S2PTOOL = s2ptool/s2ptool.cpp
SRC_S2PTOOL += $(shell ls -1 s2pdump/*.cpp | grep -v sg_ | grep -v s2pdump.cpp)
SRC_S2PTOOL += $(shell ls -1 s2pexec/*.cpp | grep -v s2pexec.cpp)
SRC_S2PTOOL += $(shell ls -1 s2pproto/*.cpp | grep -v s2pproto.cpp)
ifdef IS_LINUX
SRC_S2PTOOL += $(wildcard s2pdump/sg_*.cpp)
endif

VPATH := $(DIR_SHARED) $(DIR_SHARED_PROTOBUF) $(DIR_SHARED_INITIATOR) $(DIR_SHARED_COMMAND) $(DIR_BASE) $(DIR_BUSES) \
	$(DIR_PI) $(DIR_CONTROLLERS) $(DIR_DEVICES) ./s2p ./s2pctl ./s2pdump ./s2pexec ./s2pproto ./s2psimh ./s2ptool ./s2pformat

vpath %.h $(VPATH)
vpath %.cpp $(VPATH) test
vpath %.o $(OBJDIR)

LIB_SHARED := $(LIBDIR)/libshared.a
LIB_SHARED_PROTOBUF := $(LIBDIR)/libprotobuf.a
LIB_SHARED_COMMAND := $(LIBDIR)/libcommand.a
LIB_SHARED_INITIATOR := $(LIBDIR)/libinitiator.a
LIB_BUS := $(LIBDIR)/libbus.a
LIB_CONTROLLER := $(LIBDIR)/libcontroller.a
LIB_DEVICE := $(LIBDIR)/libdevice.a

define obj_files
$(addprefix $(OBJDIR)/,$(notdir $(1:%.cpp=%.o)))
endef

OBJ_SCHD := $(call obj_files,$(SRC_SCHD))
OBJ_SCMO := $(call obj_files,$(SRC_SCMO))
OBJ_SCCD := $(call obj_files,$(SRC_SCCD))
OBJ_SCTP := $(call obj_files,$(SRC_SCTP))
OBJ_SCDP := $(call obj_files,$(SRC_SCDP))
OBJ_SCLP := $(call obj_files,$(SRC_SCLP))
OBJ_SCSG := $(call obj_files,$(SRC_SCSG))
OBJ_SCHS := $(call obj_files,$(SRC_SCHS))
OBJ_SAHD := $(call obj_files,$(SRC_SAHD))
OBJ_SHARED := $(call obj_files,$(SRC_SHARED))
OBJ_SHARED_PROTOBUF := $(call obj_files,$(SRC_SHARED_PROTOBUF))
OBJ_SHARED_COMMAND := $(call obj_files,$(SRC_SHARED_COMMAND))
OBJ_SHARED_INITIATOR := $(call obj_files,$(SRC_SHARED_INITIATOR))
OBJ_BASE := $(call obj_files,$(SRC_BASE))
OBJ_BUSES := $(call obj_files,$(SRC_BUSES))
OBJ_CONTROLLERS := $(call obj_files,$(SRC_CONTROLLERS))
OBJ_PI := $(call obj_files,$(SRC_PI))

OBJ_DEVICES := $(shell echo $(OBJ_SCHD) $(OBJ_SCMO) $(OBJ_SCCD) $(OBJ_SCTP) $(OBJ_SCDP) $(OBJ_SCLP) $(OBJ_SCSG) $(OBJ_SCHS) $(OBJ_SAHD) | xargs -n1 | sort -u | xargs)

OBJ_S2P_CORE := $(call obj_files,$(SRC_S2P_CORE))
OBJ_S2P := $(call obj_files,$(SRC_S2P))
OBJ_S2PCTL_CORE := $(call obj_files,$(SRC_S2PCTL_CORE))
OBJ_S2PCTL := $(call obj_files,$(SRC_S2PCTL))
OBJ_S2PDUMP := $(call obj_files,$(SRC_S2PDUMP))
OBJ_S2PEXEC := $(call obj_files,$(SRC_S2PEXEC))
OBJ_S2PPROTO := $(call obj_files,$(SRC_S2PPROTO))
OBJ_S2PSIMH := $(call obj_files,$(SRC_S2PSIMH))
OBJ_S2PFORMAT := $(call obj_files,$(SRC_S2PFORMAT))
OBJ_S2PTOOL := $(call obj_files,$(SRC_S2PTOOL))
OBJ_API := $(call obj_files,$(SRC_API))
OBJ_S2P_TEST := $(call obj_files,$(SRC_S2P_TEST))

BINARIES = $(INSTALL_BIN)/$(S2PCTL) \
	$(INSTALL_BIN)/$(S2PTOOL) \
	$(INSTALL_BIN)/$(S2P)
ifeq ($(BOARD), FULLSPEC)
	BINARIES += $(INSTALL_BIN)/$(S2PDUMP)
	BINARIES += $(INSTALL_BIN)/$(S2PEXEC)
	BINARIES += $(INSTALL_BIN)/$(S2PPROTO)
	BINARIES += $(INSTALL_BIN)/$(S2PFORMAT)
endif
ifeq ($(ENABLE_SCTP),1)
	BINARIES += $(INSTALL_BIN)/$(S2PSIMH)
endif

GENERATED_DIR := generated

# The following will include all of the auto-generated dependency files (*.d)
# if they exist. This will trigger a rebuild of a source file if a header changes
ALL_DEPS := $(patsubst %.o,%.d,$(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2P) $(OBJ_S2PCTL) $(OBJ_S2PDUMP) \
	$(OBJ_S2PEXEC) $(OBJ_S2PPROTO) $(OBJ_S2PSIMH) $(OBJ_S2PFORMAT) $(OBJ_S2PTOOL) $(OBJ_SHARED) $(OBJ_SHARED_PROTOBUF) \
	$(OBJ_SHARED_INITIATOR) $(OBJ_SHARED_COMMAND) $(OBJ_BASE) $(OBJ_BUSES) $(OBJ_CONTROLLERS) $(OBJ_DEVICES) $(OBJ_S2P_TEST))
-include $(ALL_DEPS)

$(OBJ_API): $(SRC_API)
$(LIB_SHARED): $(OBJ_SHARED)
$(LIB_SHARED_PROTOBUF): $(OBJ_API) $(OBJ_SHARED_PROTOBUF) $(OBJ_SHARED)
$(LIB_SHARED_COMMAND): $(OBJ_SHARED_COMMAND) $(OBJ_API) $(OBJ_SHARED_PROTOBUF) $(OBJ_SHARED)
$(LIB_SHARED_INITIATOR): $(OBJ_SHARED_INITIATOR)
$(LIB_BUS): $(OBJ_BUSES) $(OBJ_PI)
$(LIB_CONTROLLER): $(OBJ_CONTROLLERS)
$(LIB_DEVICE): $(OBJ_DEVICES) $(OBJ_BASE)

$(OBJDIR) $(LIBDIR) $(BINDIR) $(GENERATED_DIR):
	mkdir -p $@

$(LIB_SHARED) $(LIB_SHARED_PROTOBUF) $(LIB_SHARED_INITIATOR) $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE): | $(LIBDIR)
	$(AR) rcs $@ $^

$(OBJDIR)/%.o: %.cpp | $(OBJDIR) $(SRC_API)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_API): | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC_API): $(PROTO_API) | $(GENERATED_DIR)
	protoc -I ../api --cpp_out=$(GENERATED_DIR) $(PROTO_API)
	mv $(GENERATED_DIR)/s2p_interface.pb.cc $@

## Build targets:
##   all             Buidl all executable files (default target)
##   test            Build and run unit tests
##   coverage        Build and run unit tests and create coverage files
##                   Run 'make clean' between coverage and non-coverage builds.
.DEFAULT_GOAL := all
.PHONY: all test coverage

all: cpp

cpp: $(BIN_ALL)

test: $(BINDIR)/$(S2P_TEST)
	$(BINDIR)/$(S2P_TEST)

coverage: CXXFLAGS += --coverage
coverage: test

$(SRC_S2P_CORE) $(SRC_S2PCTL_CORE) $(SRC_S2PPROTO) $(SRC_S2PTOOL): $(OBJ_API)

$(BINDIR)/$(S2P): $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(LIB_SHARED) $(OBJ_S2P_CORE) $(OBJ_S2P) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2P_CORE) $(OBJ_S2P) $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) \
	$(LIB_DEVICE) $(LIB_SHARED) $(ABSEIL_LIBS) -lpthread -lprotobuf

$(BINDIR)/$(S2PCTL): $(LIB_SHARED_PROTOBUF) $(OBJ_S2PCTL_CORE) $(OBJ_S2PCTL) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PCTL_CORE) $(OBJ_S2PCTL) $(LIB_SHARED_PROTOBUF) $(ABSEIL_LIBS) -lprotobuf

$(BINDIR)/$(S2PDUMP): $(OBJ_S2PDUMP) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PDUMP) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED)

$(BINDIR)/$(S2PEXEC): $(OBJ_S2PEXEC) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PEXEC) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED)

$(BINDIR)/$(S2PPROTO): $(OBJ_S2PPROTO) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) $(OBJ_API) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PPROTO) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) \
	$(OBJ_API) $(ABSEIL_LIBS) -lprotobuf

$(BINDIR)/$(S2PSIMH): $(OBJ_S2PSIMH) $(LIB_SHARED) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PSIMH) $(LIB_SHARED)

$(BINDIR)/$(S2PFORMAT): $(OBJ_S2PFORMAT) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PFORMAT) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED)

$(BINDIR)/$(S2PTOOL): $(LIB_SHARED_COMMAND) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(LIB_SHARED) \
	$(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2PTOOL) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2PTOOL) $(LIB_SHARED_COMMAND) \
	$(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(LIB_SHARED) $(ABSEIL_LIBS) -lpthread -lprotobuf

$(BINDIR)/$(S2P_TEST): $(LIB_SHARED_COMMAND) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(LIB_SHARED) \
	$(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2P_TEST) $(OBJ_S2PCTL_TEST) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2P_TEST) $(LIB_SHARED_COMMAND) \
	$(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(LIB_SHARED) $(ABSEIL_LIBS) -lpthread -lprotobuf -lgmock -lgtest

# Rules for building individual binaries
.PHONY: $(S2P) $(S2PCTL) $(S2PDUMP) $(S2PEXEC) $(S2PPROTO) $(S2PSIMH) $(S2PFORMAT) $(S2PTOOL) $(S2P_TEST)

$(S2P): $(BINDIR)/$(S2P) 
$(S2PCTL): $(BINDIR)/$(S2PCTL) 
$(S2PDUMP): $(BINDIR)/$(S2PDUMP) 
$(S2PEXEC): $(BINDIR)/$(S2PEXEC) 
$(S2PPROTO): $(BINDIR)/$(S2PPROTO) 
$(S2PSIMH): $(BINDIR)/$(S2PSIMH) 
$(S2FORMAT): $(BINDIR)/$(S2PFORMAT) 
$(S2PTOOL): $(BINDIR)/$(S2PTOOL)
$(S2P_TEST): $(BINDIR)/$(S2P_TEST)

##   clean           Remove all of the object files, intermediate 
##                   compiler files and executable files 
.PHONY: clean
clean:
	rm -rf $(OBJDIR) $(LIBDIR) $(BINDIR) $(GENERATED_DIR)

##   install         Installs the stripped binaries, usually to /opt/s2p/bin
.PHONY: install
install: $(BINARIES)
	$(STRIP) $^
	chown root:root $^
	chmod +s $^

$(INSTALL_BIN)%: $(BINDIR)/%
	mkdir -p $(INSTALL_BIN)
	cp $< $@


##   help            Lists information about how to use the Makefile
# The help rule is based upon the approach from:
#   https://swcarpentry.github.io/make-novice/08-self-doc/index.html 
.PHONY: help
help: Makefile
	@sed -n 's/^##//p' $<
