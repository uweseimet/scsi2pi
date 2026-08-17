##---------------------------------------------------------------------------
##
## SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
##
## Copyright (C) 2023-2026 Uwe Seimet
##
##---------------------------------------------------------------------------

.SUFFIXES:

BUILD_DIR ?= ../build

##   CROSS_COMPILE= Specifies the compiler toolchain, e.g.
##                  arm-linux-gnueabihf-
CROSS_COMPILE ?=

CXX := $(CROSS_COMPILE)g++
AR := $(CROSS_COMPILE)gcc-ar
STRIP := $(CROSS_COMPILE)strip

ifeq ($(strip $(CROSS_COMPILE)),)
ifeq ($(shell pkg-config --exists spdlog 2>/dev/null && echo yes),yes)
SPDLOG_PKGCONFIG_CFLAGS := $(shell pkg-config --cflags spdlog 2>/dev/null)
SPDLOG_PKGCONFIG_LIBS := $(shell pkg-config --libs spdlog 2>/dev/null)
endif
endif

SPDLOG_FLAGS ?= -DSPDLOG_COMPILED_LIB -DSPDLOG_SHARED_LIB $(SPDLOG_PKGCONFIG_CFLAGS)
SPDLOG_LIBS ?= $(if $(strip $(SPDLOG_PKGCONFIG_LIBS)),$(SPDLOG_PKGCONFIG_LIBS),-lspdlog -lfmt)

DEBUG_FLAGS ?= -g -O0 -DDEBUG
RELEASE_FLAGS ?= -O2 -DNDEBUG
WARNING_FLAGS ?= -Wall -Wextra

CXXFLAGS := -std=c++20 -I. -I$(BUILD_DIR) -D_FILE_OFFSET_BITS=64 $(SPDLOG_FLAGS) $(WARNING_FLAGS) -MD -MP -fvisibility=hidden -fvisibility-inlines-hidden

##   DEBUG=1       A Debug build includes the debugger symbols and
##                 disables compiler optimization. This is usually
##                 only used by developers.
DEBUG ?= 0
ifeq ($(DEBUG), 1)
# Debug compile flags
	CXXFLAGS += $(DEBUG_FLAGS)
else
# Release compile flags
	CXXFLAGS += $(RELEASE_FLAGS)
endif

UNAME := $(shell uname -s)

ifeq ($(UNAME),Linux)
# -Wno-psabi might not work on non-Linux platforms
	CXXFLAGS += -Wno-psabi
	IS_LINUX := 1
endif

ifeq ($(UNAME),FreeBSD)
	AR := $(CROSS_COMPILE)ar
	CXXFLAGS += -fexperimental-library -I/usr/local/include -Wno-unused-parameter -Wno-macro-redefined
endif

ifeq ($(UNAME),NetBSD)
	AR := $(CROSS_COMPILE)ar
	CXXFLAGS += -I/usr/pkg/include -Wno-unused-parameter -Wno-macro-redefined
endif

ifeq ($(UNAME),OpenBSD)
	AR := $(CROSS_COMPILE)ar
	CXXFLAGS += -I/usr/local/include -Wno-unused-parameter
	LDFLAGS += -L/usr/local/lib
endif

ifeq ($(UNAME),Darwin)
	AR := $(CROSS_COMPILE)ar
	CXXFLAGS += -Wno-unused-parameter
endif

## EXTRA_FLAGS Can be used to pass special purpose compiler flags
CXXFLAGS += $(EXTRA_FLAGS)

PROTOC ?= protoc
ifeq ($(shell command -v $(PROTOC) 2>/dev/null),)
  $(error "$(PROTOC)" not found — install protobuf-compiler or set PROTOC=/path/to/protoc)
endif

## Additional libraries are required since protobuf version 22
PROTOC_VERSION := $(shell $(PROTOC) --version)
PROTOC_MAJOR := $(shell echo "$(PROTOC_VERSION)" | grep -oE '[0-9]+' | head -1)
ifeq ($(PROTOC_MAJOR),)
  $(error Could not determine protoc major version from "$(PROTOC_VERSION)")
endif
ifeq ($(shell test $(PROTOC_MAJOR) -ge 22 && echo yes),yes)
	ABSEIL_LIBS = -labsl_log_internal_check_op -labsl_log_internal_message -labsl_status -labsl_log_internal_nullguard -labsl_hash
endif

## Installation path
##   The path to install binaries and manpages in, default is /opt/scsi2pi
PREFIX ?= /opt/scsi2pi

##   BOARD=FULLSPEC
##     Specifies the board type that you are using. Typically this is
##     STANDARD or FULLSPEC. The default is FULLSPEC.
BOARD ?= FULLSPEC

VALID_BOARDS := STANDARD FULLSPEC
ifeq ($(filter $(VALID_BOARDS),$(BOARD)),)
  $(error Invalid BOARD "$(BOARD)" — must be one of: $(VALID_BOARDS))
endif

INITIATOR_SUPPORT := $(and $(filter FULLSPEC,$(BOARD)),$(IS_LINUX))

CXXFLAGS += -DBOARD_$(BOARD)

##   ENABLE_*=1
##   DISABLE_*=1
##     Specifies the device types to build or not to build, default is to build all. Supported types:
##       SCHD SCSI Non-removable Hard Drive
##       SCRM Removable Media Drive
##       SCCD SCSI CD-Rom Drive
##       SCTP SCSI Tape Drive
##       SCMO SCSI Optical Memory
##       SCDP DaynaPort Network Adapter
##       SCLP SCSI Printer
##       SCSG Linux Generic SCSI (e.g. for SCSI-to-USB bridge)
##       SCHS Host Services (e.g. for realtime clock and shutdown support)
##       SAHD SASI Hard Drive
ifdef ENABLE_SCRM
	ENABLE_SCHD = 1
endif
ifdef DISABLE_SCRM
	DISABLE_SCHD = 1
endif
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

$(foreach var, SCHD SCMO SCCD SCTP SCDP SCLP SCSG SCHS SAHD, $(eval $(call add_cxx_flag,$(var))))

ifneq ($(strip $(ENABLE_SCHD)$(ENABLE_SCMO)$(ENABLE_SCCD)$(ENABLE_SAHD)$(ENABLE_SCTP)),)
	CXXFLAGS += -DBUILD_STORAGE_DEVICE
endif

ifneq ($(strip $(ENABLE_SCHD)$(ENABLE_SCMO)$(ENABLE_SCCD)$(ENABLE_SAHD)),)
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

INSTALL_BIN := $(PREFIX)/bin

OBJDIR := $(BUILD_DIR)/obj
LIBDIR := $(BUILD_DIR)/lib
BINDIR := $(BUILD_DIR)/bin
GENERATED_DIR := $(BUILD_DIR)/generated

BIN_ALL := \
	$(BINDIR)/$(S2P) \
	$(BINDIR)/$(S2PCTL) \
	$(BINDIR)/$(S2PTOOL)
# s2pdump, s2pexec, s2pproto and s2pformat require initiator support
ifeq ($(INITIATOR_SUPPORT),1)
	BIN_ALL += $(BINDIR)/$(S2PDUMP)
	BIN_ALL += $(BINDIR)/$(S2PEXEC)
	BIN_ALL += $(BINDIR)/$(S2PPROTO)
	BIN_ALL += $(BINDIR)/$(S2PFORMAT)
endif

# s2psimh only makes sense with streamer support
ifeq ($(ENABLE_SCTP),1)
	BIN_ALL += $(BINDIR)/$(S2PSIMH)
endif

DIR_SHARED := shared
DIR_SHARED_PROTOBUF := protobuf
DIR_SHARED_COMMAND := command
DIR_SHARED_INITIATOR := initiator
DIR_BASE := base
DIR_BUSES := buses
DIR_CONTROLLERS := controllers
DIR_DEVICES := devices

PROTO_API := ../api/s2p_interface.proto

SRC_API := $(GENERATED_DIR)/s2p_interface.pb.cpp

SRC_SHARED := $(DIR_SHARED)/command_meta_data.cpp \
	$(DIR_SHARED)/memory_util.cpp \
	$(DIR_SHARED)/network_util.cpp \
	$(DIR_SHARED)/s2p_formatter.cpp \
	$(DIR_SHARED)/s2p_util.cpp \
	$(DIR_SHARED)/s2p_version.cpp \
	$(DIR_SHARED)/sg_util.cpp \
	$(DIR_SHARED)/simh_util.cpp

ifdef IS_LINUX
SRC_SHARED += $(DIR_SHARED)/sg_adapter.cpp
endif

SRC_SHARED_PROTOBUF := $(DIR_SHARED_PROTOBUF)/protobuf_util.cpp \
	$(DIR_SHARED_PROTOBUF)/s2p_interface_util.cpp

SRC_SHARED_COMMAND := $(DIR_SHARED_COMMAND)/command_context.cpp \
	$(DIR_SHARED_COMMAND)/command_dispatcher.cpp \
	$(DIR_SHARED_COMMAND)/command_executor.cpp \
	$(DIR_SHARED_COMMAND)/command_image_support.cpp \
	$(DIR_SHARED_COMMAND)/command_localizer.cpp \
	$(DIR_SHARED_COMMAND)/command_response.cpp

SRC_SHARED_INITIATOR := $(DIR_SHARED_INITIATOR)/initiator_executor.cpp \
	$(DIR_SHARED_INITIATOR)/initiator_util.cpp

SRC_BASE := $(DIR_BASE)/device.cpp \
	$(DIR_BASE)/device_factory.cpp \
	$(DIR_BASE)/primary_device.cpp \
	$(DIR_BASE)/property_handler.cpp

SRC_BUSES := $(DIR_BUSES)/bus.cpp \
	$(DIR_BUSES)/bus_factory.cpp \
	$(DIR_BUSES)/in_process_bus.cpp \
	$(DIR_BUSES)/rpi_bus.cpp

SRC_CONTROLLERS := $(DIR_CONTROLLERS)/abstract_controller.cpp \
	$(DIR_CONTROLLERS)/controller.cpp \
	$(DIR_CONTROLLERS)/controller_factory.cpp \
	$(DIR_CONTROLLERS)/phase_handler.cpp \
	$(DIR_CONTROLLERS)/script_generator.cpp

SRC_STORAGE_COMMON := \
	$(DIR_DEVICES)/storage_device.cpp \
	$(DIR_DEVICES)/page_handler.cpp

SRC_DISK := \
	$(DIR_DEVICES)/disk.cpp \
	$(DIR_DEVICES)/linux_cache.cpp \
	$(DIR_DEVICES)/disk_cache.cpp \
	$(DIR_DEVICES)/disk_track.cpp \
	$(SRC_STORAGE_COMMON)

ifeq ($(ENABLE_SCHD),1)
SRC_SCHD := \
	$(DIR_DEVICES)/scsi_hd.cpp \
	$(SRC_DISK)
endif

ifeq ($(ENABLE_SCMO),1)
SRC_SCMO := \
	$(DIR_DEVICES)/optical_memory.cpp \
	$(SRC_DISK)
endif

ifeq ($(ENABLE_SCCD),1)
SRC_SCCD := \
	$(DIR_DEVICES)/scsi_cd.cpp \
	$(SRC_DISK)
endif

ifeq ($(ENABLE_SCTP),1)
SRC_SCTP := \
	$(DIR_DEVICES)/tape.cpp \
	$(SRC_STORAGE_COMMON)
endif

ifeq ($(ENABLE_SCDP),1)
SRC_SCDP := \
	$(DIR_DEVICES)/daynaport.cpp \
	$(DIR_DEVICES)/tap_driver.cpp
endif

ifeq ($(ENABLE_SCLP),1)
SRC_SCLP := \
	$(DIR_DEVICES)/printer.cpp
endif

ifeq ($(ENABLE_SCSG),1)
SRC_SCSG := \
	$(DIR_DEVICES)/scsi_generic.cpp
endif

ifeq ($(ENABLE_SCHS),1)
SRC_SCHS := \
	$(DIR_DEVICES)/host_services.cpp \
	$(DIR_DEVICES)/page_handler.cpp
endif

ifeq ($(ENABLE_SAHD),1)
SRC_SAHD := \
	$(DIR_DEVICES)/sasi_hd.cpp \
	$(SRC_DISK)
endif

SRC_S2P_CORE := s2p/s2p_core.cpp \
	s2p/s2p_parser.cpp \
	s2p/s2p_server.cpp \
	s2p/s2p_thread.cpp
SRC_S2P := s2p/s2p.cpp

SRC_S2PCTL_CORE := s2pctl/s2pctl_commands.cpp \
	s2pctl/s2pctl_core.cpp \
	s2pctl/s2pctl_display.cpp
SRC_S2PCTL := s2pctl/s2pctl.cpp

ifeq ($(INITIATOR_SUPPORT),1)
SRC_S2PDUMP_CORE := s2pdump/board_executor.cpp \
	s2pdump/s2pdump_core.cpp \
	s2pdump/s2pdump_executor.cpp
ifdef IS_LINUX
SRC_S2PDUMP_CORE += s2pdump/sg_executor.cpp
endif
SRC_S2PDUMP := $(SRC_S2PDUMP_CORE) s2pdump/s2pdump.cpp
SRC_S2PEXEC_CORE := s2pexec/s2pexec_core.cpp \
	s2pexec/s2pexec_executor.cpp
SRC_S2PEXEC := $(SRC_S2PEXEC_CORE) s2pexec/s2pexec.cpp
SRC_S2PPROTO_CORE := s2pproto/s2pproto_core.cpp \
	s2pproto/s2pproto_executor.cpp
SRC_S2PPROTO := $(SRC_S2PPROTO_CORE) s2pproto/s2pproto.cpp
SRC_S2PFORMAT := s2pformat/s2pformat_core.cpp \
	s2pformat/s2pformat.cpp
endif

SRC_S2PSIMH := s2psimh/s2psimh_core.cpp \
	s2psimh/s2psimh.cpp

SRC_S2P_TEST := test/abstract_controller_test.cpp \
	test/bus_factory_test.cpp \
	test/bus_test.cpp \
	test/command_context_test.cpp \
	test/command_dispatcher_test.cpp \
	test/command_executor_test.cpp \
	test/command_image_support_test.cpp \
	test/command_localizer_test.cpp \
	test/command_meta_data_test.cpp \
	test/command_response_test.cpp \
	test/controller_factory_test.cpp \
	test/controller_test.cpp \
	test/daynaport_test.cpp \
	test/device_factory_test.cpp \
	test/device_test.cpp \
	test/disk_cache_test.cpp \
	test/disk_test.cpp \
	test/host_services_test.cpp \
	test/initiator_util_test.cpp \
	test/in_process_bus_test.cpp \
	test/linux_cache_test.cpp \
	test/memory_util_test.cpp \
	test/network_util_test.cpp \
	test/optical_memory_test.cpp \
	test/page_handler_test.cpp \
	test/phase_handler_test.cpp \
	test/primary_device_test.cpp \
	test/printer_test.cpp \
	test/property_handler_test.cpp \
	test/protobuf_util_test.cpp \
	test/rpi_bus_test.cpp \
	test/s2pctl_commands_test.cpp \
	test/s2pctl_display_test.cpp \
	test/s2p_exceptions_test.cpp \
	test/s2p_formatter_test.cpp \
	test/s2p_interface_util_test.cpp \
	test/s2p_parser_test.cpp \
	test/s2p_thread_test.cpp \
	test/s2p_util_test.cpp \
	test/sasi_hd_test.cpp \
	test/script_generator_test.cpp \
	test/scsi_cd_test.cpp \
	test/scsi_hd_test.cpp \
	test/sg_util_test.cpp \
	test/simh_util_test.cpp \
	test/storage_device_test.cpp \
	test/tap_driver_test.cpp \
	test/tape_test.cpp \
	test/test_setup.cpp \
	test/test_shared.cpp
ifdef IS_LINUX
SRC_S2P_TEST += test/scsi_generic_test.cpp
endif

SRC_S2PTOOL := s2ptool/s2ptool.cpp \
	$(SRC_S2PDUMP_CORE) $(SRC_S2PEXEC_CORE) $(SRC_S2PPROTO_CORE)

LIB_SHARED := $(LIBDIR)/libshared.a
LIB_SHARED_PROTOBUF := $(LIBDIR)/libprotobuf.a
LIB_SHARED_COMMAND := $(LIBDIR)/libcommand.a
LIB_SHARED_INITIATOR := $(LIBDIR)/libinitiator.a
LIB_BUS := $(LIBDIR)/libbus.a
LIB_CONTROLLER := $(LIBDIR)/libcontroller.a
LIB_DEVICE := $(LIBDIR)/libdevice.a

define obj_files
$(addprefix $(OBJDIR)/,$(1:%.cpp=%.o))
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

OBJ_DEVICES := $(sort \
	$(OBJ_SCHD) \
	$(OBJ_SCMO) \
	$(OBJ_SCCD) \
	$(OBJ_SCTP) \
	$(OBJ_SCDP) \
	$(OBJ_SCLP) \
	$(OBJ_SCSG) \
	$(OBJ_SCHS) \
	$(OBJ_SAHD))

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
OBJ_S2P_TEST := $(call obj_files,$(SRC_S2P_TEST))
OBJ_API := $(OBJDIR)/generated/s2p_interface.pb.o

BINARIES = $(INSTALL_BIN)/$(S2PCTL) \
	$(INSTALL_BIN)/$(S2PTOOL) \
	$(INSTALL_BIN)/$(S2P)
ifeq ($(INITIATOR_SUPPORT),1)
	BINARIES += $(INSTALL_BIN)/$(S2PDUMP)
	BINARIES += $(INSTALL_BIN)/$(S2PEXEC)
	BINARIES += $(INSTALL_BIN)/$(S2PPROTO)
	BINARIES += $(INSTALL_BIN)/$(S2PFORMAT)
endif
ifeq ($(ENABLE_SCTP),1)
	BINARIES += $(INSTALL_BIN)/$(S2PSIMH)
endif

ALL_OBJ := $(OBJ_API) \
	$(OBJ_S2P_CORE) \
	$(OBJ_S2PCTL_CORE) \
	$(OBJ_S2P) \
	$(OBJ_S2PCTL) \
	$(OBJ_S2PDUMP) \
	$(OBJ_S2PEXEC) \
	$(OBJ_S2PPROTO) \
	$(OBJ_S2PSIMH) \
	$(OBJ_S2PFORMAT) \
	$(OBJ_S2PTOOL) \
	$(OBJ_SHARED) \
	$(OBJ_SHARED_PROTOBUF) \
	$(OBJ_SHARED_INITIATOR) \
	$(OBJ_SHARED_COMMAND) \
	$(OBJ_BASE) \
	$(OBJ_BUSES) \
	$(OBJ_CONTROLLERS) \
	$(OBJ_DEVICES) \
	$(OBJ_S2P_TEST)
ALL_DEPS := $(patsubst %.o,%.d,$(ALL_OBJ))
# The following will include all of the auto-generated dependency files (*.d)
# if they exist. This will trigger a rebuild of a source file if a header changes
-include $(ALL_DEPS)

$(LIB_SHARED): $(OBJ_SHARED)
$(LIB_SHARED_PROTOBUF): $(OBJ_API) $(OBJ_SHARED_PROTOBUF) $(OBJ_SHARED)
$(LIB_SHARED_COMMAND): $(OBJ_SHARED_COMMAND) $(OBJ_API) $(OBJ_SHARED_PROTOBUF) $(OBJ_SHARED)
$(LIB_SHARED_INITIATOR): $(OBJ_SHARED_INITIATOR)
$(LIB_BUS): $(OBJ_BUSES)
$(LIB_CONTROLLER): $(OBJ_CONTROLLERS)
$(LIB_DEVICE): $(OBJ_DEVICES) $(OBJ_BASE)

$(LIBDIR) $(BINDIR) $(GENERATED_DIR):
	@mkdir -p $@

$(LIB_SHARED) $(LIB_SHARED_PROTOBUF) $(LIB_SHARED_INITIATOR) $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE): | $(LIBDIR)
	rm -f $@
	$(AR) rcs $@ $^

$(OBJDIR)/%.o: %.cpp | $(SRC_API)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_API): $(SRC_API)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRC_API): $(PROTO_API) | $(GENERATED_DIR)
	$(PROTOC) -I ../api --cpp_out=$(GENERATED_DIR) $(PROTO_API)
	mv $(GENERATED_DIR)/s2p_interface.pb.cc $@

## Build targets:
##   all             Build all executable files (default target)
##   test            Build and run unit tests
##   coverage        Build and run unit tests and create coverage files
##                   Run 'make clean' between coverage and non-coverage builds.
.DEFAULT_GOAL := all
.PHONY: all cpp test coverage

all: cpp

cpp: $(BIN_ALL)

test: $(BINDIR)/$(S2P_TEST)
	$(BINDIR)/$(S2P_TEST)

coverage: CXXFLAGS += --coverage
coverage: test

$(SRC_S2P_CORE) $(SRC_S2PCTL_CORE) $(SRC_S2PPROTO) $(SRC_S2PTOOL): $(OBJ_API)

$(BINDIR)/$(S2P): $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(LIB_SHARED) $(OBJ_S2P_CORE) $(OBJ_S2P) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2P_CORE) $(OBJ_S2P) $(LIB_SHARED_COMMAND) $(LIB_BUS) $(LIB_CONTROLLER) \
	$(LIB_DEVICE) $(LIB_SHARED) $(SPDLOG_LIBS) $(ABSEIL_LIBS) -lpthread -lprotobuf

$(BINDIR)/$(S2PCTL): $(LIB_SHARED_PROTOBUF) $(OBJ_S2PCTL_CORE) $(OBJ_S2PCTL) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PCTL_CORE) $(OBJ_S2PCTL) $(LIB_SHARED_PROTOBUF) $(SPDLOG_LIBS) $(ABSEIL_LIBS) -lprotobuf

$(BINDIR)/$(S2PDUMP): $(OBJ_S2PDUMP) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PDUMP) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) $(SPDLOG_LIBS)

$(BINDIR)/$(S2PEXEC): $(OBJ_S2PEXEC) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PEXEC) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) $(SPDLOG_LIBS)

$(BINDIR)/$(S2PPROTO): $(OBJ_S2PPROTO) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) $(OBJ_API) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PPROTO) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) \
	$(OBJ_API)  $(SPDLOG_LIBS) $(ABSEIL_LIBS) -lprotobuf

$(BINDIR)/$(S2PSIMH): $(OBJ_S2PSIMH) $(LIB_SHARED) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PSIMH) $(LIB_SHARED) $(SPDLOG_LIBS)

$(BINDIR)/$(S2PFORMAT): $(OBJ_S2PFORMAT) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2PFORMAT) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_SHARED) $(SPDLOG_LIBS)

$(BINDIR)/$(S2PTOOL): $(LIB_SHARED_COMMAND) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(LIB_SHARED) \
	$(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2PTOOL) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2PTOOL) $(LIB_SHARED_COMMAND) \
	$(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(LIB_SHARED) $(SPDLOG_LIBS) $(ABSEIL_LIBS) -lpthread -lprotobuf

$(BINDIR)/$(S2P_TEST): $(LIB_SHARED_COMMAND) $(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(LIB_SHARED) \
	$(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2P_TEST) | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJ_S2P_CORE) $(OBJ_S2PCTL_CORE) $(OBJ_S2P_TEST) $(LIB_SHARED_COMMAND) \
	$(LIB_SHARED_INITIATOR) $(LIB_BUS) $(LIB_CONTROLLER) $(LIB_DEVICE) $(LIB_SHARED) $(SPDLOG_LIBS) $(ABSEIL_LIBS) -lpthread -lprotobuf -lgmock -lgtest

# Rules for building individual binaries
.PHONY: $(S2P) $(S2PCTL) $(S2PDUMP) $(S2PEXEC) $(S2PPROTO) $(S2PSIMH) $(S2PFORMAT) $(S2PTOOL) $(S2P_TEST)

$(S2P): $(BINDIR)/$(S2P)
$(S2PCTL): $(BINDIR)/$(S2PCTL)
$(S2PDUMP): $(BINDIR)/$(S2PDUMP)
$(S2PEXEC): $(BINDIR)/$(S2PEXEC)
$(S2PPROTO): $(BINDIR)/$(S2PPROTO)
$(S2PSIMH): $(BINDIR)/$(S2PSIMH)
$(S2PFORMAT): $(BINDIR)/$(S2PFORMAT)
$(S2PTOOL): $(BINDIR)/$(S2PTOOL)
$(S2P_TEST): $(BINDIR)/$(S2P_TEST)

##   clean           Remove all of the object files, intermediate
##                   compiler files and executable files
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

##   install         Installs the stripped binaries, usually to /opt/scsi2pi/bin
.PHONY: install
install: $(BINARIES)

$(INSTALL_BIN)/%: $(BINDIR)/%
	@mkdir -p $(INSTALL_BIN)
	cp $< $@
	$(STRIP) $@


##   help            Lists information about how to use the Makefile
# The help rule is based upon the approach from:
#   https://swcarpentry.github.io/make-novice/08-self-doc/index.html
.PHONY: help
help: Makefile
	@sed -n 's/^##//p' $<
